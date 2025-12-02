-- Migration: Add admin and email_confirmed flags to users table
-- Date: 2025-12-01

-- Add admin column if it doesn't exist
DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns 
        WHERE table_name = 'users' AND column_name = 'admin'
    ) THEN
        ALTER TABLE users ADD COLUMN admin BOOLEAN DEFAULT FALSE;
        RAISE NOTICE 'Added admin column to users table';
    ELSE
        RAISE NOTICE 'Column admin already exists';
    END IF;
END $$;

-- Add email_confirmed column if it doesn't exist
DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns 
        WHERE table_name = 'users' AND column_name = 'email_confirmed'
    ) THEN
        ALTER TABLE users ADD COLUMN email_confirmed BOOLEAN DEFAULT FALSE;
        RAISE NOTICE 'Added email_confirmed column to users table';
    ELSE
        RAISE NOTICE 'Column email_confirmed already exists';
    END IF;
END $$;

-- Create index for admin queries (optional but recommended)
CREATE INDEX IF NOT EXISTS idx_users_admin ON users(admin) WHERE admin = TRUE;

-- Show result
SELECT column_name, data_type, column_default 
FROM information_schema.columns 
WHERE table_name = 'users' 
AND column_name IN ('admin', 'email_confirmed');
