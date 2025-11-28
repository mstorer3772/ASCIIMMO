#include "worldgen.hpp"
#include <random>
#include <vector>
#include <sstream>

namespace asciimmo {

WorldGen::WorldGen(uint64_t seed, int width, int height)
    : seed_(seed), width_(width), height_(height) {}

static inline int idx(int x, int y, int w) { return y * w + x; }

std::string WorldGen::generate() {
    std::mt19937_64 rng(seed_);
    std::uniform_real_distribution<double> d(0.0, 1.0);

    int w = width_;
    int h = height_;
    std::vector<double> map(w * h);

    // Layer 1: base noise
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double nx = double(x) / double(w) - 0.5;
            double ny = double(y) / double(h) - 0.5;
            // bias toward center for land
            double dist = std::sqrt(nx * nx + ny * ny);
            double val = d(rng);
            val = val * (1.0 - dist * 0.8);
            map[idx(x, y, w)] = val;
        }
    }

    // Smooth the map a few times to create blobs
    for (int iter = 0; iter < 3; ++iter) {
        std::vector<double> tmp = map;
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                double sum = 0.0;
                for (int yy = -1; yy <= 1; ++yy)
                    for (int xx = -1; xx <= 1; ++xx)
                        sum += map[idx(x + xx, y + yy, w)];
                tmp[idx(x, y, w)] = sum / 9.0;
            }
        }
        map.swap(tmp);
    }

    // Convert to ASCII using thresholds
    std::ostringstream out;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double v = map[idx(x, y, w)];
            char ch = '.'; // plains
            if (v < 0.18) ch = '~';            // water
            else if (v < 0.30) ch = ',';       // marsh/shore
            else if (v < 0.55) ch = '.';       // grass
            else if (v < 0.75) ch = 'T';       // forest
            else ch = '^';                     // mountain
            out << ch;
        }
        if (y < h - 1) out << '\n';
    }

    return out.str();
}

} // namespace asciimmo
