#include <gtest/gtest.h>
#include "worldgen.hpp"

// Note: Testing HTTP services requires mocking or integration tests
// For now, testing the underlying worldgen functionality used by the service

TEST(WorldServiceTest, WorldGenIntegration) {
    // Test that worldgen produces valid output for service endpoints
    asciimmo::WorldGen gen(12345, 80, 24);
    std::string map = gen.generate();
    
    EXPECT_FALSE(map.empty()) << "World generation should produce output";
}

TEST(WorldServiceTest, ParameterRanges) {
    // Test various parameter combinations that service might receive
    EXPECT_NO_THROW({
        asciimmo::WorldGen gen(0, 10, 10);
        gen.generate();
    }) << "Should handle minimum seed";
    
    EXPECT_NO_THROW({
        asciimmo::WorldGen gen(UINT64_MAX, 200, 50);
        gen.generate();
    }) << "Should handle maximum seed and larger dimensions";
}

TEST(WorldServiceTest, DefaultParameters) {
    // Test default parameters as used by service
    asciimmo::WorldGen gen(12345, 80, 24);
    std::string map = gen.generate();
    
    EXPECT_GT(map.size(), 0) << "Default parameters should work";
}

// TODO: Add HTTP endpoint tests with mock server
// TEST(WorldServiceTest, HealthEndpoint) { ... }
// TEST(WorldServiceTest, WorldEndpoint) { ... }
