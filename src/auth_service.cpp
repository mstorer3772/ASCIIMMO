#include "shared/http_server.hpp"
#include "shared/logger.hpp"
#include "shared/service_config.hpp"
#include <iostream>
#include <string>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--config FILE] [--port P] [--cert FILE] [--key FILE]\n";
    std::cerr << "  Config file defaults to config/services.yaml\n";
    std::cerr << "  Command line options override config file values\n";
}

int main(int argc, char** argv) {
    // Load configuration
    auto& config = asciimmo::config::ServiceConfig::instance();
    std::string config_file = "config/services.yaml";
    
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            config_file = argv[++i];
            break;
        }
    }
    
    if (!config.load(config_file)) {
        std::cerr << "Warning: Could not load config file: " << config_file << std::endl;
    }
    
    int port = config.get_int("auth_service.port", 8081);
    std::string cert_file = config.get_string("global.cert_file", "certs/server.crt");
    std::string key_file = config.get_string("global.key_file", "certs/server.key");

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--config") {
            ++i;
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

    asciimmo::log::Logger logger("auth-service");
    
    boost::asio::io_context ioc;
    asciimmo::http::Server svr(ioc, port, cert_file, key_file);

    logger.info("Starting auth-service on port " + std::to_string(port));

    // Stub: POST /auth/login  (expects {"username":"...", "password":"..."})
    svr.post("/auth/login", [](const asciimmo::http::Request&, asciimmo::http::Response& res, const std::smatch&) {
        // TODO: validate credentials, generate session token
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","token":"stub-token-12345"})";
        res.prepare_payload();
    });

    // Stub: POST /auth/register
    svr.post("/auth/register", [](const asciimmo::http::Request&, asciimmo::http::Response& res, const std::smatch&) {
        // TODO: create user account
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","message":"user registered"})";
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
