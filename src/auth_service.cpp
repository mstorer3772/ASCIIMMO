#include <iostream>
#include <string>
#include "httplib.h"

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--port P]\n";
}

int main(int argc, char** argv) {
    int port = 8081;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    httplib::Server svr;

    // Stub: POST /auth/login  (expects {"username":"...", "password":"..."})
    svr.Post("/auth/login", [](const httplib::Request& req, httplib::Response& res) {
        // TODO: validate credentials, generate session token
        res.set_content("{\"status\":\"ok\",\"token\":\"stub-token-12345\"}", "application/json");
    });

    // Stub: POST /auth/register
    svr.Post("/auth/register", [](const httplib::Request& req, httplib::Response& res) {
        // TODO: create user account
        res.set_content("{\"status\":\"ok\",\"message\":\"user registered\"}", "application/json");
    });

    svr.Post("/shutdown", [&svr](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\",\"message\":\"shutting down\"}", "application/json");
        svr.stop();
    });

    std::cout << "[auth-service] listening on port " << port << "\n";
    svr.listen("0.0.0.0", port);
    return 0;
}
