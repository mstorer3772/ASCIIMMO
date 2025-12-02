-- Add email confirmation tokens table
CREATE TABLE IF NOT EXISTS email_confirmation_tokens (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    token VARCHAR(255) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP NOT NULL,
    used BOOLEAN DEFAULT FALSE
);

CREATE INDEX idx_email_tokens_token ON email_confirmation_tokens(token);
CREATE INDEX idx_email_tokens_user ON email_confirmation_tokens(user_id);
CREATE INDEX idx_email_tokens_expires ON email_confirmation_tokens(expires_at);
