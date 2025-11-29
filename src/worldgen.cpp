#include "worldgen.hpp"
#include <random>
#include <sstream>
#include <vector>

namespace asciimmo {

WorldGen::WorldGen(uint64_t seed, int width, int height)
    : seed_(seed), width_(width), height_(height) {}

static inline int idx(int x, int y, int w) { return y * w + x; }

std::string WorldGen::generate() {
    std::mt19937_64 rng(seed_);
    std::uniform_real_distribution<double> d(-0.5, 1.5);

    std::vector<double> map(width_ * height_);

    // Layer 1: base noise
    double dWidth = double(width_);
    double dHeight = double(height_);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            double val = d(rng);
            map[idx(x, y, width_)] = val;
        }
    }

    // Smooth the map a few times to create blobs
    for (int iter = 0; iter < 2; ++iter) {
        std::vector<double> tmp = map;
        for (int y = 1; y < height_ - 1; ++y) {
            for (int x = 1; x < width_ - 1; ++x) {
                double sum = 0.0;
                double count = 0;
                for (int yy = -1; yy <= 1; ++yy){
                    for (int xx = -1; xx <= 1; ++xx) {
                        if (xx + x < 0 || 
                            xx + x >= width_ ||
                            yy + y < 0 ||
                            yy + y >= height_) {
                            continue;  // skip out-of-bounds
                        }
                        sum += map[idx(x + xx, y + yy, width_)];
                        ++count;
                    }
                }
                tmp[idx(x, y, width_)] = sum / count;
            }
        }
        map.swap(tmp);
    }

    // Convert to ASCII using thresholds
    std::ostringstream out;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            double v = map[idx(x, y, width_)];
            char ch = '.'; // plains
            if (v < 0.18) ch = '~';            // water
            else if (v < 0.30) ch = ',';       // marsh/shore
            else if (v < 0.55) ch = '.';       // grass
            else if (v < 0.75) ch = 'T';       // forest
            else ch = '^';                     // mountain
            out << ch;
        }
        if (y < height_ - 1) out << '\n';
    }

    return out.str();
}

} // namespace asciimmo
