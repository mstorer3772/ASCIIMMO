#include "server_http.hpp"
#include "worldgen.hpp"
#include <iostream>

#include "httplib.h"

namespace asciimmo {

void run_http_server(int port, unsigned long long default_seed, int default_width, int default_height) {
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

    std::cout << "ASCIIMMO HTTP server listening on port " << port << "\n";
    svr.listen("0.0.0.0", port);
}

} // namespace asciimmo
