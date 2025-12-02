#include <gtest/gtest.h>
#include "db_pool.hpp"
#include "db_config.hpp"
#include <pqxx/pqxx>

using namespace asciimmo::db;

class SocialDatabaseTest : public ::testing::Test
    {
    protected:
        void SetUp() override
            {
            config_ = Config::from_env();
            try
                {
                pool_ = std::make_unique<ConnectionPool>(config_);
                }
                catch (const std::exception& e)
                    {
                    GTEST_SKIP() << "Database not available: " << e.what();
                    }

                // Create test users
                auto conn = pool_->acquire();
                pqxx::work txn(conn.get());
                auto result1 = txn.exec(
                    "INSERT INTO users (username, password_hash, salt) VALUES ($1, $2, $3) RETURNING id",
                    pqxx::params{ "test_social_user1", int64_t(111222333444), int64_t(444333222111) }
                );
                test_user1_id_ = result1[0][0].as<int>();

                auto result2 = txn.exec(
                    "INSERT INTO users (username, password_hash, salt) VALUES ($1, $2, $3) RETURNING id",
                    pqxx::params{ "test_social_user2", int64_t(555666777888), int64_t(888777666555) }
                );
                test_user2_id_ = result2[0][0].as<int>();
                txn.commit();
            }

        void TearDown() override
            {
            if (pool_)
                {
                auto conn = pool_->acquire();
                pqxx::work txn(conn.get());
                // Delete parties and guilds first (they reference users)
                txn.exec("DELETE FROM parties WHERE leader_id IN (" + txn.quote(test_user1_id_) + ", " + txn.quote(test_user2_id_) + ")");
                txn.exec("DELETE FROM guilds WHERE leader_id IN (" + txn.quote(test_user1_id_) + ", " + txn.quote(test_user2_id_) + ")");
                txn.exec("DELETE FROM users WHERE id IN ($1, $2)",
                    pqxx::params{ test_user1_id_, test_user2_id_ });
                txn.commit();
                }
            pool_.reset();
            }

        Config config_;
        std::unique_ptr<ConnectionPool> pool_;
        int test_user1_id_;
        int test_user2_id_;
    };

// Chat Tests
TEST_F(SocialDatabaseTest, CreateChatMessage)
    {
    auto conn = pool_->acquire();
    pqxx::work txn(conn.get());

    auto result = txn.exec(
        "INSERT INTO chat_messages (user_id, username, message, channel) "
        "VALUES ($1, $2, $3, $4) RETURNING id",
        pqxx::params{ test_user1_id_, "test_social_user1", "Hello, world!", "global" }
    );

    ASSERT_EQ(result.size(), 1);
    int msg_id = result[0][0].as<int>();
    EXPECT_GT(msg_id, 0);

    txn.commit();
    }

TEST_F(SocialDatabaseTest, RetrieveChatMessages)
    {
    auto conn = pool_->acquire();

    // Insert messages
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO chat_messages (user_id, username, message, channel) VALUES "
        "($1, $2, 'Message 1', 'global'), "
        "($1, $2, 'Message 2', 'global')",
        pqxx::params{ test_user1_id_, "test_social_user1" }
    );
    txn.commit();
    }

    // Retrieve messages
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT message FROM chat_messages WHERE user_id = $1 ORDER BY created_at DESC LIMIT 10",
        pqxx::params{ test_user1_id_ }
    );

    EXPECT_GE(result.size(), 2);
    }
    }

TEST_F(SocialDatabaseTest, DeleteChatMessage)
    {
    auto conn = pool_->acquire();

    int msg_id;
    // Insert message
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "INSERT INTO chat_messages (user_id, username, message) "
        "VALUES ($1, $2, $3) RETURNING id",
        pqxx::params{ test_user1_id_, "test_social_user1", "Delete me" }
    );
    msg_id = result[0][0].as<int>();
    txn.commit();
    }

    // Delete message
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "DELETE FROM chat_messages WHERE id = $1",
        pqxx::params{ msg_id }
    );
    txn.commit();
    EXPECT_EQ(result.affected_rows(), 1);
    }
    }

// Friendship Tests
TEST_F(SocialDatabaseTest, CreateFriendship)
    {
    auto conn = pool_->acquire();
    pqxx::work txn(conn.get());

    auto result = txn.exec(
        "INSERT INTO friendships (user_id, friend_id) VALUES ($1, $2) RETURNING id",
        pqxx::params{ test_user1_id_, test_user2_id_ }
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_GT(result[0][0].as<int>(), 0);

    txn.commit();
    }

TEST_F(SocialDatabaseTest, GetFriendsList)
    {
    auto conn = pool_->acquire();

    // Create friendship
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO friendships (user_id, friend_id) VALUES ($1, $2)",
        pqxx::params{ test_user1_id_, test_user2_id_ }
    );
    txn.commit();
    }

    // Get friends
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT friend_id FROM friendships WHERE user_id = $1",
        pqxx::params{ test_user1_id_ }
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0][0].as<int>(), test_user2_id_);
    }
    }

TEST_F(SocialDatabaseTest, RemoveFriendship)
    {
    auto conn = pool_->acquire();

    // Create friendship
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO friendships (user_id, friend_id) VALUES ($1, $2)",
        pqxx::params{ test_user1_id_, test_user2_id_ }
    );
    txn.commit();
    }

    // Remove friendship
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "DELETE FROM friendships WHERE user_id = $1 AND friend_id = $2",
        pqxx::params{ test_user1_id_, test_user2_id_ }
    );
    txn.commit();
    EXPECT_EQ(result.affected_rows(), 1);
    }

    // Verify removal
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT COUNT(*) FROM friendships WHERE user_id = $1",
        pqxx::params{ test_user1_id_ }
    );
    EXPECT_EQ(result[0][0].as<int>(), 0);
    }
    }

// Party Tests
TEST_F(SocialDatabaseTest, CreateParty)
    {
    auto conn = pool_->acquire();
    pqxx::work txn(conn.get());

    std::string party_id = "party_test_123";
    auto result = txn.exec(
        "INSERT INTO parties (party_id, leader_id) VALUES ($1, $2) RETURNING id",
        pqxx::params{ party_id, test_user1_id_ }
    );

    ASSERT_EQ(result.size(), 1);
    int id = result[0][0].as<int>();
    EXPECT_GT(id, 0);

    txn.commit();
    }

TEST_F(SocialDatabaseTest, AddPartyMember)
    {
    auto conn = pool_->acquire();

    int party_pk;
    // Create party
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "INSERT INTO parties (party_id, leader_id) VALUES ($1, $2) RETURNING id",
        pqxx::params{ "party_members_test", test_user1_id_ }
    );
    party_pk = result[0][0].as<int>();
    txn.commit();
    }

    // Add member
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO party_members (party_id, user_id) VALUES ($1, $2)",
        pqxx::params{ party_pk, test_user2_id_ }
    );
    txn.commit();
    }

    // Verify member
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT COUNT(*) FROM party_members WHERE party_id = $1",
        pqxx::params{ party_pk }
    );
    EXPECT_EQ(result[0][0].as<int>(), 1);
    }
    }

TEST_F(SocialDatabaseTest, RemoveParty)
    {
    auto conn = pool_->acquire();

    int party_pk;
    // Create party
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "INSERT INTO parties (party_id, leader_id) VALUES ($1, $2) RETURNING id",
        pqxx::params{ "party_delete_test", test_user1_id_ }
    );
    party_pk = result[0][0].as<int>();
    txn.commit();
    }

    // Delete party (cascades to members)
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "DELETE FROM parties WHERE id = $1",
        pqxx::params{ party_pk }
    );
    txn.commit();
    EXPECT_EQ(result.affected_rows(), 1);
    }
    }

// Guild Tests
TEST_F(SocialDatabaseTest, CreateGuild)
    {
    auto conn = pool_->acquire();
    pqxx::work txn(conn.get());

    std::string guild_id = "guild_test_123";
    auto result = txn.exec(
        "INSERT INTO guilds (guild_id, name, leader_id) VALUES ($1, $2, $3) RETURNING id",
        pqxx::params{ guild_id, "Test Guild", test_user1_id_ }
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_GT(result[0][0].as<int>(), 0);

    txn.commit();
    }

TEST_F(SocialDatabaseTest, AddGuildMember)
    {
    auto conn = pool_->acquire();

    int guild_pk;
    // Create guild
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "INSERT INTO guilds (guild_id, name, leader_id) VALUES ($1, $2, $3) RETURNING id",
        pqxx::params{ "guild_members_test", "Members Guild", test_user1_id_ }
    );
    guild_pk = result[0][0].as<int>();
    txn.commit();
    }

    // Add member
    {
    pqxx::work txn(conn.get());
    txn.exec(
        "INSERT INTO guild_members (guild_id, user_id, role) VALUES ($1, $2, $3)",
        pqxx::params{ guild_pk, test_user2_id_, "member" }
    );
    txn.commit();
    }

    // Verify member
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "SELECT user_id, role FROM guild_members WHERE guild_id = $1",
        pqxx::params{ guild_pk }
    );
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["user_id"].as<int>(), test_user2_id_);
    EXPECT_EQ(result[0]["role"].as<std::string>(), "member");
    }
    }

TEST_F(SocialDatabaseTest, RemoveGuild)
    {
    auto conn = pool_->acquire();

    int guild_pk;
    // Create guild
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "INSERT INTO guilds (guild_id, name, leader_id) VALUES ($1, $2, $3) RETURNING id",
        pqxx::params{ "guild_delete_test", "Delete Guild", test_user1_id_ }
    );
    guild_pk = result[0][0].as<int>();
    txn.commit();
    }

    // Delete guild (cascades to members)
    {
    pqxx::work txn(conn.get());
    auto result = txn.exec(
        "DELETE FROM guilds WHERE id = $1",
        pqxx::params{ guild_pk }
    );
    txn.commit();
    EXPECT_EQ(result.affected_rows(), 1);
    }
    }
