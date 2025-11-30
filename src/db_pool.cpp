#include "db_pool.hpp"
#include <stdexcept>
#include <iostream>

namespace asciimmo {
namespace db {

// PooledConnection implementation
PooledConnection::PooledConnection(std::unique_ptr<pqxx::connection> conn, ConnectionPool* pool)
    : conn_(std::move(conn)), pool_(pool) {}

PooledConnection::~PooledConnection() {
    if (conn_ && pool_) {
        pool_->release(std::move(conn_));
    }
}

PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : conn_(std::move(other.conn_)), pool_(other.pool_) {
    other.pool_ = nullptr;
}

PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept {
    if (this != &other) {
        if (conn_ && pool_) {
            pool_->release(std::move(conn_));
        }
        conn_ = std::move(other.conn_);
        pool_ = other.pool_;
        other.pool_ = nullptr;
    }
    return *this;
}

pqxx::connection& PooledConnection::get() {
    if (!conn_) {
        throw std::runtime_error("PooledConnection: connection is null");
    }
    return *conn_;
}

// ConnectionPool implementation
ConnectionPool::ConnectionPool(const Config& config)
    : config_(config) {
    create_connections();
}

ConnectionPool::~ConnectionPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    cv_.notify_all();
}

void ConnectionPool::create_connections() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (int i = 0; i < config_.pool_size; ++i) {
        pool_.push(create_connection());
    }
}

std::unique_ptr<pqxx::connection> ConnectionPool::create_connection() {
    try {
        auto conn = std::make_unique<pqxx::connection>(config_.connection_string());
        if (!conn->is_open()) {
            throw std::runtime_error("Failed to open database connection");
        }
        return conn;
    } catch (const std::exception& e) {
        std::cerr << "Database connection error: " << e.what() << std::endl;
        throw;
    }
}

PooledConnection ConnectionPool::acquire(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    auto deadline = std::chrono::steady_clock::now() + timeout;
    
    while (pool_.empty() && !shutdown_) {
        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            throw std::runtime_error("Connection pool timeout: no connections available");
        }
    }
    
    if (shutdown_) {
        throw std::runtime_error("Connection pool is shutting down");
    }
    
    auto conn = std::move(pool_.front());
    pool_.pop();
    
    // Verify connection is still alive
    if (!conn->is_open()) {
        conn = create_connection();
    }
    
    return PooledConnection(std::move(conn), this);
}

void ConnectionPool::release(std::unique_ptr<pqxx::connection> conn) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!shutdown_ && conn && conn->is_open()) {
            pool_.push(std::move(conn));
        }
    }
    cv_.notify_one();
}

size_t ConnectionPool::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.pool_size;
}

size_t ConnectionPool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

} // namespace db
} // namespace asciimmo
