#include "worldgen.hpp"
#include <iostream>
#include <string>
#include "httplib.h"

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--port P] [--default-seed N] [--default-width W] [--default-height H]\n";
}

int main(int argc, char** argv) {
    int port = 8080;
    unsigned long long default_seed = 12345;
    int default_width = 80;
    int default_height = 24;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (a == "--default-seed" && i + 1 < argc) {
            default_seed = std::stoull(argv[++i]);
        } else if (a == "--default-width" && i + 1 < argc) {
            default_width = std::stoi(argv[++i]);
        } else if (a == "--default-height" && i + 1 < argc) {
            default_height = std::stoi(argv[++i]);
        } else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    httplib::Server svr;

    svr.Get("/world", [default_seed, default_width, default_height](const httplib::Request& req, httplib::Response& res) {
        unsigned long long seed = default_seed;
        int width = default_width;
        int height = default_height;
        try {
            if (req.has_param("seed")) seed = std::stoull(req.get_param_value("seed"));
            if (req.has_param("width")) width = std::stoi(req.get_param_value("width"));
            if (req.has_param("height")) height = std::stoi(req.get_param_value("height"));
        } catch (...) {
            // ignore parse errors and fall back to defaults
        }

        asciimmo::WorldGen gen(seed, width, height);
        std::string map = gen.generate();
        res.set_content(map, "text/plain; charset=utf-8");
    });

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\",\"service\":\"world\"}", "application/json");
    });

    svr.Post("/shutdown", [&svr](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\",\"message\":\"shutting down\"}", "application/json");
        svr.stop();
    });

    std::cout << "[world-service] listening on port " << port << "\n";
    svr.listen("0.0.0.0", port);
    return 0;
}
