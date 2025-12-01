#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include "httplib.h"

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--port P]\n";
}

// In-memory session store (ephemeral; replace with Redis/DB for production)
static std::unordered_map<std::string, std::string> sessions;
static std::mutex sessions_mtx;

int main(int argc, char** argv) {
    int port = 8082;

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

    // GET /session/:token
    svr.Get(R"(/session/(\w+))", [](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.matches[1];
        std::lock_guard<std::mutex> lock(sessions_mtx);
        auto it = sessions.find(token);
        if (it != sessions.end()) {
            res.set_content("{\"status\":\"ok\",\"data\":\"" + it->second + "\"}", "application/json");
        } else {
            res.status = 404;
            res.set_content("{\"status\":\"error\",\"message\":\"session not found\"}", "application/json");
        }
    });

    // POST /session (create new session; expects {"token":"...", "data":"..."})
    svr.Post("/session", [](const httplib::Request& req, httplib::Response& res) {
        // TODO: parse JSON properly; stub: just store body as data
        std::string token = "session-" + std::to_string(std::hash<std::string>{}(req.body));
        {
            std::lock_guard<std::mutex> lock(sessions_mtx);
            sessions[token] = req.body;
        }
        res.set_content("{\"status\":\"ok\",\"token\":\"" + token + "\"}", "application/json");
    });

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\",\"service\":\"session\"}", "application/json");
    });

    svr.Post("/shutdown", [&svr](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\",\"message\":\"shutting down\"}", "application/json");
        svr.stop();
    });

    std::cout << "[session-service] listening on port " << port << "\n";
    svr.listen("0.0.0.0", port);
    return 0;
}
