# PostgreSQL Integration

## Overview

ASCIIMMO now uses PostgreSQL for persistent data storage with connection pooling for high throughput and responsiveness.

## Features

- **Connection Pool**: Thread-safe connection pooling with configurable size
- **Auto-reconnect**: Automatically handles stale connections
- **Environment Config**: Configure via environment variables
- **Schema Management**: SQL scripts for database initialization

## Database Schema

- **users**: User accounts and authentication
- **sessions**: User sessions with expiration
- **chat_messages**: Global and channel-based chat history
- **friendships**: Friend relationships
- **parties**: Temporary player groups
- **guilds**: Persistent player organizations

## Setup

### 1. Initialize Database

The database, user, and schema have already been created. To verify:

```bash
sudo -u postgres psql -d asciimmo -c "\dt"
```

### 2. Set Environment Variables

```bash
export ASCIIMMO_DB_NAME=asciimmo
export ASCIIMMO_DB_USER=asciimmo_user
export ASCIIMMO_DB_PASSWORD=asciimmo_dev_password
export ASCIIMMO_DB_HOST=localhost
export ASCIIMMO_DB_PORT=5432
export ASCIIMMO_DB_POOL_SIZE=10
```

Or add to `.bashrc`/`.zshrc` for persistence.

### 3. Test Connection

```bash
PGPASSWORD=asciimmo_dev_password psql -U asciimmo_user -d asciimmo -c "SELECT COUNT(*) FROM users;"
```

## Usage in Services

### Basic Connection Pool

```cpp
#include "db_pool.hpp"
#include "db_config.hpp"

// Initialize pool (once per service)
auto config = asciimmo::db::Config::from_env();
asciimmo::db::ConnectionPool pool(config);

// Acquire connection (RAII - auto-returns to pool)
auto conn = pool.acquire();

// Use connection
pqxx::work txn(conn.get());
auto result = txn.exec("SELECT * FROM users WHERE username = 'player1'");
txn.commit();
```

### With Timeout

```cpp
// Acquire with 10 second timeout
auto conn = pool.acquire(std::chrono::seconds(10));
```

## Connection Pool Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `ASCIIMMO_DB_HOST` | localhost | PostgreSQL host |
| `ASCIIMMO_DB_PORT` | 5432 | PostgreSQL port |
| `ASCIIMMO_DB_NAME` | asciimmo | Database name |
| `ASCIIMMO_DB_USER` | asciimmo_user | Database user |
| `ASCIIMMO_DB_PASSWORD` | (empty) | Database password |
| `ASCIIMMO_DB_POOL_SIZE` | 10 | Max connections in pool |

## Maintenance

### Cleanup Expired Sessions

```sql
SELECT cleanup_expired_sessions();
```

Run periodically (e.g., via cron or service timer).

### View Pool Status

```cpp
std::cout << "Pool size: " << pool.size() << "\n";
std::cout << "Available: " << pool.available() << "\n";
```

## Next Steps

Services (auth, session, social) can now be updated to use PostgreSQL instead of in-memory storage. The `db_utils` library is linked and ready to use.

## Security Notes

⚠️ **Production**: Change the default password and use SSL connections:
- Update password in database
- Set `ASCIIMMO_DB_PASSWORD` environment variable
- Add `sslmode=require` to connection string in `db_config.cpp`
