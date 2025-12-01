#include <gtest/gtest.h>
#include "db_pool.hpp"
#include "db_config.hpp"
#include <pqxx/pqxx>

using namespace asciimmo::db;

class AuthDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = Config::from_env();
        try {
            pool_ = std::make_unique<ConnectionPool>(config_);
        } catch (const std::exception& e) {
            GTEST_SKIP() << "Database not available: " << e.what();
        }
    }
    
    void TearDown() override {
        // Cleanup test data
        if (pool_) {
            auto conn = pool_->acquire();
            pqxx::work txn(conn.get());
            txn.exec("DELETE FROM users WHERE username LIKE 'test_user_%'");
            txn.commit();
        }
        pool_.reset();
    }
    
    Config config_;
    std::unique_ptr<ConnectionPool> pool_;
};

TEST_F(AuthDatabaseTest, CreateUser) {
    auto conn = pool_->acquire();
    pqxx::work txn(conn.get());
    
    std::string username = "test_user_create";
    std::string password_hash = "hashed_password_12345";
    
    auto result = txn.exec(
        "INSERT INTO users (username, password_hash) VALUES ($1, $2) RETURNING id",
        pqxx::params{username, password_hash}
    );
    
    ASSERT_EQ(result.size(), 1);
    int user_id = result[0][0].as<int>();
    EXPECT_GT(user_id, 0);
    
    txn.commit();
}

TEST_F(AuthDatabaseTest, FindUserByUsername) {
    auto conn = pool_->acquire();
    
    // Insert test user
    {
        pqxx::work txn(conn.get());
        txn.exec(
            "INSERT INTO users (username, password_hash) VALUES ($1, $2)",
            pqxx::params{"test_user_find", "hashed_pass"}
        );
        txn.commit();
    }
    
    // Find user
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "SELECT id, username, password_hash FROM users WHERE username = $1",
            pqxx::params{"test_user_find"}
        );
        
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0]["username"].as<std::string>(), "test_user_find");
        EXPECT_EQ(result[0]["password_hash"].as<std::string>(), "hashed_pass");
    }
}

TEST_F(AuthDatabaseTest, DeleteUser) {
    auto conn = pool_->acquire();
    
    // Insert test user
    int user_id;
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "INSERT INTO users (username, password_hash) VALUES ($1, $2) RETURNING id",
            pqxx::params{"test_user_delete", "hash"}
        );
        user_id = result[0][0].as<int>();
        txn.commit();
    }
    
    // Delete user
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "DELETE FROM users WHERE id = $1",
            pqxx::params{user_id}
        );
        txn.commit();
        EXPECT_EQ(result.affected_rows(), 1);
    }
    
    // Verify deletion
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "SELECT COUNT(*) FROM users WHERE id = $1",
            pqxx::params{user_id}
        );
        EXPECT_EQ(result[0][0].as<int>(), 0);
    }
}

TEST_F(AuthDatabaseTest, UniqueUsernameConstraint) {
    auto conn = pool_->acquire();
    
    std::string username = "test_user_unique";
    
    // Insert first user
    {
        pqxx::work txn(conn.get());
        txn.exec(
            "INSERT INTO users (username, password_hash) VALUES ($1, $2)",
            pqxx::params{username, "hash1"}
        );
        txn.commit();
    }
    
    // Try to insert duplicate username
    EXPECT_THROW({
        pqxx::work txn(conn.get());
        txn.exec(
            "INSERT INTO users (username, password_hash) VALUES ($1, $2)",
            pqxx::params{username, "hash2"}
        );
        txn.commit();
    }, pqxx::unique_violation);
}

TEST_F(AuthDatabaseTest, UpdateLastLogin) {
    auto conn = pool_->acquire();
    
    // Insert test user
    int user_id;
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "INSERT INTO users (username, password_hash) VALUES ($1, $2) RETURNING id",
            pqxx::params{"test_user_login", "hash"}
        );
        user_id = result[0][0].as<int>();
        txn.commit();
    }
    
    // Update last_login
    {
        pqxx::work txn(conn.get());
        txn.exec(
            "UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = $1",
            pqxx::params{user_id}
        );
        txn.commit();
    }
    
    // Verify update
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "SELECT last_login FROM users WHERE id = $1",
            pqxx::params{user_id}
        );
        EXPECT_FALSE(result[0][0].is_null()) << "last_login should be set";
    }
}
