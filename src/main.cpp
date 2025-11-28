#include "worldgen.hpp"
#include <iostream>
#include <string>

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--seed N] [--width W] [--height H] [--http-port P]\n";
}

int main(int argc, char** argv) {
    uint64_t seed = 12345;
    int width = 80;
    int height = 24;
    int http_port = 0;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--seed" && i + 1 < argc) {
            seed = std::stoull(argv[++i]);
        } else if (a == "--width" && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (a == "--height" && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        } else if (a == "--http-port" && i + 1 < argc) {
            http_port = std::stoi(argv[++i]);
        } else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    asciimmo::WorldGen gen(seed, width, height);
    std::string map = gen.generate();

    if (http_port != 0) {
        std::cout << "HTTP server is not implemented in this scaffold yet.\n";
        std::cout << "Requested port: " << http_port << "\n";
        std::cout << "TODO: integrate an HTTP library (cpp-httplib / Boost.Beast) to serve /world?seed=<seed>" << std::endl;
        // TODO: start an HTTP server and serve `map` at /world?seed=<seed>
        return 0;
    }

    std::cout << map << std::endl;
    return 0;
}
