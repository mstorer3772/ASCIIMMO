#include "server_http.hpp"
#include "worldgen.hpp"
#include <iostream>

#include "httplib.h"

namespace asciimmo {

void run_http_server(int port, uint64_t seed, int width, int height) {
    httplib::Server svr;

    svr.Get("/world", [&seed, &width, &height](const httplib::Request& req, httplib::Response& res) {
        try {
            if (req.has_param("seed")) {
                seed = std::stoull(req.get_param_value("seed"));
            }
            if (req.has_param("width")) {
                width = std::stoi(req.get_param_value("width"));
            }
            if (req.has_param("height")) {
                height = std::stoi(req.get_param_value("height"));
            }
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
