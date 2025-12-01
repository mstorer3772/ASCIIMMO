#include "shared/http_server.hpp"
#include "shared/logger.hpp"
#include "shared/http_client.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--port P] [--cert FILE] [--key FILE]\n";
}

// In-memory session store (ephemeral; replace with Redis/DB for production)
static std::unordered_map<std::string, std::string> sessions;
static std::mutex sessions_mtx;

// Broadcast token to all other services
static void broadcast_token(const std::string& token, const std::string& user_data, asciimmo::log::Logger& logger) {
    // Service endpoints (adjust ports as needed)
    std::vector<std::pair<std::string, int>> services = {
        {"localhost", 8080}, // world-service
        {"localhost", 8083}  // social-service
    };
    
    // Properly construct JSON string
    std::string body = R"({"token":")" + token + R"(","user_data":")" + user_data + R"(","ttl":900})";
    
    // Broadcast in background thread to avoid blocking
    std::thread([services, body, &logger]() {
        for (const auto& [host, port] : services) {
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
    int port = 8082;
    std::string cert_file = "certs/server.crt";
    std::string key_file = "certs/server.key";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) {
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

    // GET /session/:token
    svr.get(R"(/session/(\w+))", [](const asciimmo::http::Request&, asciimmo::http::Response& res, const std::smatch& matches) {
        std::string token = matches[1].str();
        std::lock_guard<std::mutex> lock(sessions_mtx);
        auto it = sessions.find(token);
        if (it != sessions.end()) {
            res.result(boost::beast::http::status::ok);
            res.body() = R"({"status":"ok","data":")" + it->second + R"("})";
        } else {
            res.result(boost::beast::http::status::not_found);
            res.body() = R"({"status":"error","message":"session not found"})";
        }
        res.prepare_payload();
    });

    // POST /session (create new session; expects {"token":"...", "data":"..."})
    svr.post("/session", [&logger](const asciimmo::http::Request& req, asciimmo::http::Response& res, const std::smatch&) {
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

    svr.get("/health", [](const asciimmo::http::Request&, asciimmo::http::Response& res, const std::smatch&) {
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
