#include "shared/http_server.hpp"
#include <iostream>
#include <string>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--port P] [--cert FILE] [--key FILE]\n";
}

int main(int argc, char** argv) {
    int port = 8081;
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

    boost::asio::io_context ioc;
    asciimmo::http::Server svr(ioc, port, cert_file, key_file);

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

    svr.post("/shutdown", [&ioc](const asciimmo::http::Request&, asciimmo::http::Response& res, const std::smatch&) {
        res.result(boost::beast::http::status::ok);
        res.body() = R"({"status":"ok","message":"shutting down"})";
        res.prepare_payload();
        ioc.stop();
    });

    std::cout << "[auth-service] listening on port " << port << "\n";
    
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){ ioc.stop(); });
    
    svr.run();
    ioc.run();
    
    return 0;
}
