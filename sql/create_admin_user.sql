-- Create admin user: LucasTheLost
-- Date: 2025-12-01

INSERT INTO users (username, password_hash, email, admin, email_confirmed, is_active) 
VALUES (
    'LucasTheLost', 
    'temp_hash_change_on_first_login', 
    'mstorer3772@gmail.com', 
    true, 
    true, 
    true
)
ON CONFLICT (username) DO UPDATE 
SET 
    email = EXCLUDED.email,
    admin = EXCLUDED.admin,
    email_confirmed = EXCLUDED.email_confirmed
RETURNING id, username, email, admin, email_confirmed, created_at;
