#include "worldgen.hpp"
#include "server_http.hpp"
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

    if (http_port != 0) {
        // Start HTTP server that serves /world?seed=&width=&height=
        asciimmo::run_http_server(http_port, seed, width, height);
        return 0;
    }

    asciimmo::WorldGen gen(seed, width, height);
    std::string map = gen.generate();
    std::cout << map << std::endl;
    return 0;
}
