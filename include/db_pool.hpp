#pragma once
#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include "db_config.hpp"

namespace asciimmo {
namespace db {

// RAII wrapper for database connections from the pool
class PooledConnection {
public:
    PooledConnection(std::unique_ptr<pqxx::connection> conn, class ConnectionPool* pool);
    ~PooledConnection();
    
    // Disable copy, allow move
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;
    PooledConnection(PooledConnection&&) noexcept;
    PooledConnection& operator=(PooledConnection&&) noexcept;
    
    pqxx::connection& get();
    pqxx::connection* operator->() { return conn_.get(); }
    
private:
    std::unique_ptr<pqxx::connection> conn_;
    ConnectionPool* pool_;
};

// Thread-safe connection pool for PostgreSQL
class ConnectionPool {
public:
    explicit ConnectionPool(const Config& config);
    ~ConnectionPool();
    
    // Get a connection from the pool (blocks if none available)
    PooledConnection acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    // Return a connection to the pool
    void release(std::unique_ptr<pqxx::connection> conn);
    
    // Pool statistics
    size_t size() const;
    size_t available() const;
    
private:
    void create_connections();
    std::unique_ptr<pqxx::connection> create_connection();
    
    Config config_;
    std::queue<std::unique_ptr<pqxx::connection>> pool_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool shutdown_ = false;
    
    friend class PooledConnection;
};

} // namespace db
} // namespace asciimmo
