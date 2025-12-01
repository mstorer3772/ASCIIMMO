#include <gtest/gtest.h>
#include "db_pool.hpp"
#include "db_config.hpp"
#include <pqxx/pqxx>

using namespace asciimmo::db;

class SessionDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = Config::from_env();
        try {
            pool_ = std::make_unique<ConnectionPool>(config_);
        } catch (const std::exception& e) {
            GTEST_SKIP() << "Database not available: " << e.what();
        }
        
        // Create test user
        auto conn = pool_->acquire();
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "INSERT INTO users (username, password_hash) VALUES ($1, $2) RETURNING id",
            pqxx::params{"test_session_user", "hash"}
        );
        test_user_id_ = result[0][0].as<int>();
        txn.commit();
    }
    
    void TearDown() override {
        if (pool_) {
            auto conn = pool_->acquire();
            pqxx::work txn(conn.get());
            txn.exec("DELETE FROM users WHERE id = $1", pqxx::params{test_user_id_});
            txn.commit();
        }
        pool_.reset();
    }
    
    Config config_;
    std::unique_ptr<ConnectionPool> pool_;
    int test_user_id_;
};

TEST_F(SessionDatabaseTest, CreateSession) {
    auto conn = pool_->acquire();
    pqxx::work txn(conn.get());
    
    std::string token = "test_token_12345";
    std::string data = R"({"user":"test","role":"player"})";
    
    auto result = txn.exec(
        "INSERT INTO sessions (token, user_id, data, expires_at) "
        "VALUES ($1, $2, $3::jsonb, CURRENT_TIMESTAMP + INTERVAL '1 hour') "
        "RETURNING id",
        pqxx::params{token, test_user_id_, data}
    );
    
    ASSERT_EQ(result.size(), 1);
    int session_id = result[0][0].as<int>();
    EXPECT_GT(session_id, 0);
    
    txn.commit();
}

TEST_F(SessionDatabaseTest, FindSessionByToken) {
    auto conn = pool_->acquire();
    
    std::string token = "test_token_find";
    
    // Create session
    {
        pqxx::work txn(conn.get());
        txn.exec(
            "INSERT INTO sessions (token, user_id, data, expires_at) "
            "VALUES ($1, $2, '{}'::jsonb, CURRENT_TIMESTAMP + INTERVAL '1 hour')",
            pqxx::params{token, test_user_id_}
        );
        txn.commit();
    }
    
    // Find session
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "SELECT token, user_id FROM sessions WHERE token = $1",
            pqxx::params{token}
        );
        
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0]["token"].as<std::string>(), token);
        EXPECT_EQ(result[0]["user_id"].as<int>(), test_user_id_);
    }
}

TEST_F(SessionDatabaseTest, DeleteSession) {
    auto conn = pool_->acquire();
    
    std::string token = "test_token_delete";
    
    // Create session
    {
        pqxx::work txn(conn.get());
        txn.exec(
            "INSERT INTO sessions (token, user_id, data, expires_at) "
            "VALUES ($1, $2, '{}'::jsonb, CURRENT_TIMESTAMP + INTERVAL '1 hour')",
            pqxx::params{token, test_user_id_}
        );
        txn.commit();
    }
    
    // Delete session
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "DELETE FROM sessions WHERE token = $1",
            pqxx::params{token}
        );
        txn.commit();
        EXPECT_EQ(result.affected_rows(), 1);
    }
    
    // Verify deletion
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "SELECT COUNT(*) FROM sessions WHERE token = $1",
            pqxx::params{token}
        );
        EXPECT_EQ(result[0][0].as<int>(), 0);
    }
}

TEST_F(SessionDatabaseTest, SessionExpiration) {
    auto conn = pool_->acquire();
    
    std::string token = "test_token_expired";
    
    // Create expired session
    {
        pqxx::work txn(conn.get());
        txn.exec(
            "INSERT INTO sessions (token, user_id, data, expires_at) "
            "VALUES ($1, $2, '{}'::jsonb, CURRENT_TIMESTAMP - INTERVAL '1 hour')",
            pqxx::params{token, test_user_id_}
        );
        txn.commit();
    }
    
    // Check if session is expired
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "SELECT COUNT(*) FROM sessions WHERE token = $1 AND expires_at > CURRENT_TIMESTAMP",
            pqxx::params{token}
        );
        EXPECT_EQ(result[0][0].as<int>(), 0) << "Expired session should not be found";
    }
}

TEST_F(SessionDatabaseTest, CleanupExpiredSessions) {
    auto conn = pool_->acquire();
    
    // Create mix of valid and expired sessions
    {
        pqxx::work txn(conn.get());
        txn.exec(
            "INSERT INTO sessions (token, user_id, data, expires_at) VALUES "
            "($1, $2, '{}'::jsonb, CURRENT_TIMESTAMP - INTERVAL '1 hour'), "
            "($3, $2, '{}'::jsonb, CURRENT_TIMESTAMP + INTERVAL '1 hour')",
            pqxx::params{"expired_token", test_user_id_, "valid_token"}
        );
        txn.commit();
    }
    
    // Run cleanup
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec("SELECT cleanup_expired_sessions()");
        int deleted = result[0][0].as<int>();
        txn.commit();
        EXPECT_GE(deleted, 1) << "Should delete at least one expired session";
    }
    
    // Verify only valid session remains
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "SELECT COUNT(*) FROM sessions WHERE user_id = $1",
            pqxx::params{test_user_id_}
        );
        EXPECT_EQ(result[0][0].as<int>(), 1);
    }
}

TEST_F(SessionDatabaseTest, UpdateSessionActivity) {
    auto conn = pool_->acquire();
    
    std::string token = "test_token_activity";
    
    // Create session
    {
        pqxx::work txn(conn.get());
        txn.exec(
            "INSERT INTO sessions (token, user_id, data, expires_at) "
            "VALUES ($1, $2, '{}'::jsonb, CURRENT_TIMESTAMP + INTERVAL '1 hour')",
            pqxx::params{token, test_user_id_}
        );
        txn.commit();
    }
    
    // Update last activity
    {
        pqxx::work txn(conn.get());
        txn.exec(
            "UPDATE sessions SET last_activity = CURRENT_TIMESTAMP WHERE token = $1",
            pqxx::params{token}
        );
        txn.commit();
    }
    
    // Verify update
    {
        pqxx::work txn(conn.get());
        auto result = txn.exec(
            "SELECT last_activity FROM sessions WHERE token = $1",
            pqxx::params{token}
        );
        EXPECT_FALSE(result[0][0].is_null());
    }
}
