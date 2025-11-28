#pragma once
#include <string>
#include <cstdint>

namespace asciimmo {

class WorldGen {
public:
    WorldGen(uint64_t seed, int width = 80, int height = 24);
    std::string generate();

private:
    uint64_t seed_;
    int width_;
    int height_;
};

} // namespace asciimmo
