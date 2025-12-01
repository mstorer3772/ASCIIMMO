#include "shared/token_cache.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace asciimmo::auth;

class TokenCacheTest : public ::testing::Test {
protected:
    TokenCache cache;
};

TEST_F(TokenCacheTest, AddAndValidateToken) {
    std::string token = "test-token-123";
    std::string user_data = "user:alice";
    
    cache.add_token(token);
    
    bool valid = cache.validate_token(token);
    
    EXPECT_TRUE(valid);
}

TEST_F(TokenCacheTest, ValidateNonExistentToken) {
    std::string retrieved_data;
    bool valid = cache.validate_token("non-existent-token");
    
    EXPECT_FALSE(valid);
}

TEST_F(TokenCacheTest, UpdateExistingToken) {
    std::string token = "test-token-456";
    
    cache.add_token(token, 15 );
    cache.add_token(token, 15);  // Update
    
    bool valid = cache.validate_token(token);
    
    EXPECT_TRUE(valid);
}

TEST_F(TokenCacheTest, MultipleTokens) {
    cache.add_token("token1", 15);
    cache.add_token("token2", 15);
    cache.add_token("token3", 15);
    
    EXPECT_TRUE(cache.validate_token("token1"));
    EXPECT_TRUE(cache.validate_token("token2"));
    EXPECT_TRUE(cache.validate_token("token3"));
}

TEST_F(TokenCacheTest, CleanupExpiredTokens) {
    // Note: This test uses the actual 15-minute expiration from TokenCache
    // We'll add tokens and verify cleanup doesn't remove valid tokens
    cache.add_token("valid-token", 15);
    
    // Cleanup should not remove recently added tokens
    cache.cleanup_expired();
    
    std::string retrieved_data;
    bool valid = cache.validate_token("valid-token");
    
    EXPECT_TRUE(valid);
    EXPECT_EQ(retrieved_data, "user:alice");
}

TEST_F(TokenCacheTest, ConcurrentAccess) {
    // Test thread-safety by having multiple threads add/validate tokens
    const int num_threads = 10;
    const int tokens_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, tokens_per_thread]() {
            for (int i = 0; i < tokens_per_thread; ++i) {
                std::string token = "token-" + std::to_string(t) + "-" + std::to_string(i);
                std::string user_data = "user-" + std::to_string(t) + "-" + std::to_string(i);
                
                cache.add_token(token, user_data);
                
                std::string retrieved_data;
                bool valid = cache.validate_token(token, retrieved_data);
                
                EXPECT_TRUE(valid);
                EXPECT_EQ(retrieved_data, user_data);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all tokens are still accessible
    for (int t = 0; t < num_threads; ++t) {
        for (int i = 0; i < tokens_per_thread; ++i) {
            std::string token = "token-" + std::to_string(t) + "-" + std::to_string(i);
            std::string user_data = "user-" + std::to_string(t) + "-" + std::to_string(i);
            
            std::string retrieved_data;
            bool valid = cache.validate_token(token, retrieved_data);
            
            EXPECT_TRUE(valid);
            EXPECT_EQ(retrieved_data, user_data);
        }
    }
}

TEST_F(TokenCacheTest, ConcurrentCleanup) {
    // Test that cleanup can safely run concurrently with other operations
    cache.add_token("persistent-token", "user:persistent");
    
    std::atomic<bool> stop{false};
    
    // Thread that continuously adds tokens
    std::thread adder([this, &stop]() {
        int counter = 0;
        while (!stop) {
            cache.add_token("temp-" + std::to_string(counter++), "data");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Thread that continuously validates tokens
    std::thread validator([this, &stop]() {
        while (!stop) {
            std::string data;
            cache.validate_token("persistent-token", data);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Thread that runs cleanup
    std::thread cleaner([this, &stop]() {
        while (!stop) {
            cache.cleanup_expired();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // Let threads run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;
    
    adder.join();
    validator.join();
    cleaner.join();
    
    // Verify the persistent token is still valid
    std::string data;
    EXPECT_TRUE(cache.validate_token("persistent-token", data));
    EXPECT_EQ(data, "user:persistent");
}

TEST_F(TokenCacheTest, EmptyStringToken) {
    cache.add_token("", "empty-key-data");
    
    std::string retrieved_data;
    bool valid = cache.validate_token("", retrieved_data);
    
    EXPECT_TRUE(valid);
    EXPECT_EQ(retrieved_data, "empty-key-data");
}

TEST_F(TokenCacheTest, EmptyStringUserData) {
    cache.add_token("test-token", "");
    
    std::string retrieved_data = "should-be-cleared";
    bool valid = cache.validate_token("test-token", retrieved_data);
    
    EXPECT_TRUE(valid);
    EXPECT_EQ(retrieved_data, "");
}

TEST_F(TokenCacheTest, LargeUserData) {
    std::string large_data(10000, 'x');  // 10KB of 'x' characters
    cache.add_token("large-token", large_data);
    
    std::string retrieved_data;
    bool valid = cache.validate_token("large-token", retrieved_data);
    
    EXPECT_TRUE(valid);
    EXPECT_EQ(retrieved_data, large_data);
}

TEST_F(TokenCacheTest, SpecialCharactersInData) {
    std::string special_data = R"({"user":"alice","roles":["admin","user"],"settings":{"theme":"dark"}})";
    cache.add_token("json-token", special_data);
    
    std::string retrieved_data;
    bool valid = cache.validate_token("json-token", retrieved_data);
    
    EXPECT_TRUE(valid);
    EXPECT_EQ(retrieved_data, special_data);
}
