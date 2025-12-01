#include "shared/http_server.hpp"
#include "shared/logger.hpp"
#include "shared/http_client.hpp"
#include "shared/service_config.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--config FILE] [--port P] [--cert FILE] [--key FILE]\n";
    std::cerr << "  Config file defaults to config/services.yaml\n";
    std::cerr << "  Command line options override config file values\n";
}

// In-memory session store (ephemeral; replace with Redis/DB for production)
static std::unordered_map<std::string, std::string> sessions;
static std::mutex sessions_mtx;

// Broadcast token to services that need session validation
static void broadcast_token(const std::string& token, const std::string& user_data, asciimmo::log::Logger& logger) {
    auto& config = asciimmo::config::ServiceConfig::instance();
    
    // List of all services to check
    std::vector<std::string> service_names = {
        "world_service",
        "auth_service", 
        "social_service"
    };
    
    // Build list of services that need session tokens
    std::vector<std::pair<std::string, int>> target_services;
    for (const auto& service_name : service_names) {
        bool needs_session = config.get_bool(service_name + ".needs_session", false);
        if (needs_session) {
            int service_port = config.get_int(service_name + ".port", 0);
            if (service_port > 0) {
                target_services.push_back({"localhost", service_port});
                logger.debug("Will broadcast to " + service_name + " on port " + std::to_string(service_port));
            }
        }
    }
    
    // Properly construct JSON string
    std::string body = R"({"token":")" + token + R"(","user_data":")" + user_data + R"(","ttl":900})";
    
    // Broadcast in background thread to avoid blocking
    std::thread([target_services, body, &logger]() {
        for (const auto& [host, port] : target_services) {
            try {
                bool success = asciimmo::http::Client::post(host, port, "/token/register", body);
                if (success) {
                    logger.debug("Broadcasted token to " + host + ":" + std::to_string(port));
                } else {
                    logger.warning("Failed to broadcast token to " + host + ":" + std::to_string(port));
                }
            } catch (const std::exception& e) {
                logger.error("Exception broadcasting to " + host + ":" + std::to_string(port) + " - " + e.what());
            }
        }
    }).detach();
}

int main(int argc, char** argv) {
    // Load configuration
    auto& config = asciimmo::config::ServiceConfig::instance();
    std::string config_file = "config/services.yaml";
    
    // Check for config file argument first
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            config_file = argv[++i];
            break;
        }
    }
    
    if (!config.load(config_file)) {
        std::cerr << "Warning: Could not load config file: " << config_file << std::endl;
        std::cerr << "Using default values and command line options only." << std::endl;
    }
    
    // Get values from config with defaults
    int port = config.get_int("session_service.port", 8082);
    std::string cert_file = config.get_string("global.cert_file", "certs/server.crt");
    std::string key_file = config.get_string("global.key_file", "certs/server.key");

    // Command line arguments override config file
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--config") {
            ++i; // Skip, already processed
        } else if (a == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (a == "--cert" && i + 1 < argc) {
            cert_file = argv[++i];
        } else if (a == "--key" && i + 1 < argc) {
            key_file = argv[++i];
        } else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    asciimmo::log::Logger logger("session-service");
    
    boost::asio::io_context ioc;
    asciimmo::http::Server svr(ioc, port, cert_file, key_file);

    logger.info("Starting session-service on port " + std::to_string(port));

    // GET /session/:token?session_token=xxx
    svr.get(R"(/session/(\w+))", [](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch& matches) {
        std::string token = matches[1].str();
        // Session token is optional for session service GET operations
        std::lock_guard<std::mutex> lock(sessions_mtx);
        auto it = sessions.find(token);
        if (it != sessions.end()) {
            res.result(boost::beast::http::status::ok);
            res.body() = R"({"status":"ok","data":"" + it->second + R"("})";
        } else {
            res.result(boost::beast::http::status::not_found);
            res.body() = R"({"status":"error","message":"session not found"})";
        }
        res.prepare_payload();
    });

    // POST /session?session_token=xxx (create new session; expects {"token":"...", "data":"..."})
    svr.post("/session", [&logger](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&) {
        // Session token is optional for creating new sessions (this is the auth point)
        // TODO: parse JSON properly; stub: just store body as data
        std::string token = "session-" + std::to_string(std::hash<std::string>{}(req.body()));
        std::string user_data = req.body();
        {
            std::lock_guard<std::mutex> lock(sessions_mtx);
            sessions[token] = user_data;
        }
        
        // Broadcast token to other services
        logger.info("Created session token: " + token + ", broadcasting to services");
        broadcast_token(token, user_data, logger);
        
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","token":")" + token + R"("})";
        res.prepare_payload();
    });

    svr.get("/health", [](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&) {
        // Health endpoint doesn't require session token
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","service":"session"})";
        res.prepare_payload();
    });

    svr.post("/shutdown", [&ioc, &logger](const asciimmo::http::Request&, asciimmo::http::Response& res, const std::smatch&) {
        logger.info("Shutdown requested via /shutdown endpoint");
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","message":"shutting down"})";
        res.prepare_payload();
        ioc.stop();
    });

    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){
        logger.info("Shutdown signal received");
        ioc.stop();
    });
    
    svr.run();
    ioc.run();
    
    logger.info("Service stopped");
    return 0;
}
