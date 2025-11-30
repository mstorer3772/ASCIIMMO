-- ASCIIMMO Database Schema

-- Users and Authentication
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    email VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP,
    is_active BOOLEAN DEFAULT TRUE
);

CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_email ON users(email);

-- Sessions
CREATE TABLE IF NOT EXISTS sessions (
    id SERIAL PRIMARY KEY,
    token VARCHAR(255) UNIQUE NOT NULL,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    data JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_sessions_token ON sessions(token);
CREATE INDEX idx_sessions_user_id ON sessions(user_id);
CREATE INDEX idx_sessions_expires_at ON sessions(expires_at);

-- Chat messages
CREATE TABLE IF NOT EXISTS chat_messages (
    id BIGSERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    username VARCHAR(50) NOT NULL,
    message TEXT NOT NULL,
    channel VARCHAR(50) DEFAULT 'global',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_chat_channel_time ON chat_messages(channel, created_at DESC);

-- Friends
CREATE TABLE IF NOT EXISTS friendships (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    friend_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_id, friend_id),
    CHECK (user_id != friend_id)
);

CREATE INDEX idx_friendships_user ON friendships(user_id);
CREATE INDEX idx_friendships_friend ON friendships(friend_id);

-- Parties
CREATE TABLE IF NOT EXISTS parties (
    id SERIAL PRIMARY KEY,
    party_id VARCHAR(100) UNIQUE NOT NULL,
    leader_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS party_members (
    id SERIAL PRIMARY KEY,
    party_id INTEGER REFERENCES parties(id) ON DELETE CASCADE,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(party_id, user_id)
);

CREATE INDEX idx_party_members_party ON party_members(party_id);
CREATE INDEX idx_party_members_user ON party_members(user_id);

-- Guilds
CREATE TABLE IF NOT EXISTS guilds (
    id SERIAL PRIMARY KEY,
    guild_id VARCHAR(100) UNIQUE NOT NULL,
    name VARCHAR(100) NOT NULL,
    leader_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS guild_members (
    id SERIAL PRIMARY KEY,
    guild_id INTEGER REFERENCES guilds(id) ON DELETE CASCADE,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    role VARCHAR(50) DEFAULT 'member',
    UNIQUE(guild_id, user_id)
);

CREATE INDEX idx_guild_members_guild ON guild_members(guild_id);
CREATE INDEX idx_guild_members_user ON guild_members(user_id);

-- Session cleanup function (call periodically)
CREATE OR REPLACE FUNCTION cleanup_expired_sessions()
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM sessions WHERE expires_at < CURRENT_TIMESTAMP;
    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql;

-- Grant permissions (adjust user as needed)
-- GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO asciimmo_user;
-- GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO asciimmo_user;
