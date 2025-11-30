#include <gtest/gtest.h>
#include "db_pool.hpp"
#include "db_config.hpp"
#include <pqxx/pqxx>

using namespace asciimmo::db;

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure environment variables are set or use defaults
        config_ = Config::from_env();
        
        // Verify database is accessible
        try {
            pool_ = std::make_unique<ConnectionPool>(config_);
        } catch (const std::exception& e) {
            GTEST_SKIP() << "Database not available: " << e.what();
        }
    }
    
    void TearDown() override {
        pool_.reset();
    }
    
    Config config_;
    std::unique_ptr<ConnectionPool> pool_;
};

// Connection Pool Tests
TEST_F(DatabaseTest, PoolInitialization) {
    EXPECT_GT(pool_->size(), 0) << "Pool should have connections";
    EXPECT_EQ(pool_->available(), pool_->size()) << "All connections should be available initially";
}

TEST_F(DatabaseTest, AcquireAndReleaseConnection) {
    size_t initial_available = pool_->available();
    
    {
        auto conn = pool_->acquire();
        EXPECT_TRUE(conn.get().is_open()) << "Connection should be open";
        EXPECT_EQ(pool_->available(), initial_available - 1) << "Available count should decrease";
    }
    
    // Connection auto-released when going out of scope
    EXPECT_EQ(pool_->available(), initial_available) << "Connection should be returned to pool";
}

TEST_F(DatabaseTest, MultipleConnections) {
    std::vector<PooledConnection> connections;
    size_t acquire_count = std::min(3UL, pool_->size());
    
    for (size_t i = 0; i < acquire_count; ++i) {
        connections.push_back(pool_->acquire());
    }
    
    EXPECT_EQ(pool_->available(), pool_->size() - acquire_count);
    
    connections.clear();
    EXPECT_EQ(pool_->available(), pool_->size()) << "All connections returned";
}

TEST_F(DatabaseTest, ConnectionTimeout) {
    // Acquire all connections
    std::vector<PooledConnection> connections;
    for (size_t i = 0; i < pool_->size(); ++i) {
        connections.push_back(pool_->acquire());
    }
    
    // Try to acquire one more with short timeout
    EXPECT_THROW(
        pool_->acquire(std::chrono::milliseconds(100)),
        std::runtime_error
    ) << "Should timeout when pool is exhausted";
}

// Basic Database Operations
TEST_F(DatabaseTest, SimpleQuery) {
    auto conn = pool_->acquire();
    pqxx::work txn(conn.get());
    
    auto result = txn.exec("SELECT 1 AS test_value");
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0][0].as<int>(), 1);
}

TEST_F(DatabaseTest, TableExists) {
    auto conn = pool_->acquire();
    pqxx::work txn(conn.get());
    
    auto result = txn.exec(
        "SELECT table_name FROM information_schema.tables "
        "WHERE table_schema = 'public' AND table_name IN "
        "('users', 'sessions', 'chat_messages', 'friendships', 'parties', 'guilds')"
    );
    
    EXPECT_GE(result.size(), 6) << "All expected tables should exist";
}
