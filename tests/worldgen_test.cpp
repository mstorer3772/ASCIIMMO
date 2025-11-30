#include "worldgen.hpp"
#include <gtest/gtest.h>

using namespace asciimmo;

TEST(WorldGenTest, CreateInstance) {
    WorldGen gen(12345, 80, 24);
    EXPECT_NO_THROW(gen.generate());
}

TEST(WorldGenTest, DeterministicOutput) {
    WorldGen gen1(12345, 80, 24);
    WorldGen gen2(12345, 80, 24);
    
    std::string map1 = gen1.generate();
    std::string map2 = gen2.generate();
    
    EXPECT_EQ(map1, map2) << "Same seed should produce identical maps";
}

TEST(WorldGenTest, DifferentSeeds) {
    WorldGen gen1(12345, 80, 24);
    WorldGen gen2(54321, 80, 24);
    
    std::string map1 = gen1.generate();
    std::string map2 = gen2.generate();
    
    EXPECT_NE(map1, map2) << "Different seeds should produce different maps";
}

TEST(WorldGenTest, OutputSize) {
    int width = 80;
    int height = 24;
    WorldGen gen(12345, width, height);
    
    std::string map = gen.generate();
    
    // Count newlines (should be height - 1, no newline after last line)
    int lines = 0;
    for (char c : map) {
        if (c == '\n') ++lines;
    }
    EXPECT_EQ(lines, height - 1) << "Output should have height-1 newlines";
}

TEST(WorldGenTest, CustomDimensions) {
    WorldGen gen(12345, 40, 12);
    std::string map = gen.generate();
    
    EXPECT_FALSE(map.empty()) << "Map should not be empty";
    
    // Count newlines (should be height - 1)
    int lines = 0;
    for (char c : map) {
        if (c == '\n') ++lines;
    }
    EXPECT_EQ(lines, 11) << "Custom height should produce height-1 newlines";
}

TEST(WorldGenTest, ContainsExpectedChars) {
    WorldGen gen(12345, 80, 24);
    std::string map = gen.generate();
    
    // Should contain various terrain characters
    bool hasWater = map.find('~') != std::string::npos || map.find('≈') != std::string::npos;
    bool hasLand = map.find('.') != std::string::npos || map.find('^') != std::string::npos;
    
    EXPECT_TRUE(hasWater || hasLand) << "Map should contain terrain characters";
}
