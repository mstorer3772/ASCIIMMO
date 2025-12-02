-- Migration 004: Convert password hashes, salts, and tokens from VARCHAR to BIGINT
-- This provides better performance and storage efficiency for numeric hash values

BEGIN;

-- Update users table to use BIGINT for password_hash and salt
ALTER TABLE users 
    ALTER COLUMN password_hash TYPE BIGINT USING 0,
    ALTER COLUMN salt TYPE BIGINT USING 0;

-- Update sessions table to use BIGINT for token
ALTER TABLE sessions
    ALTER COLUMN token TYPE BIGINT USING 0;

-- Update email_confirmation_tokens to use BIGINT for token
ALTER TABLE email_confirmation_tokens
    ALTER COLUMN token TYPE BIGINT USING 0;

-- Drop and recreate indexes with new type
DROP INDEX IF EXISTS idx_sessions_token;
CREATE INDEX idx_sessions_token ON sessions(token);

COMMIT;
