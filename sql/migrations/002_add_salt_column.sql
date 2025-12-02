-- Migration: Add salt column to users table
-- Date: 2025-12-01

-- Add salt column if it doesn't exist
DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM information_schema.columns 
        WHERE table_name = 'users' AND column_name = 'salt'
    ) THEN
        ALTER TABLE users ADD COLUMN salt VARCHAR(255);
        RAISE NOTICE 'Added salt column to users table';
        
        -- Update existing users with a placeholder salt
        -- In production, you would need to regenerate password hashes with proper salts
        UPDATE users SET salt = 'MIGRATION_PLACEHOLDER_SALT_' || id::text WHERE salt IS NULL;
        
        -- Make salt NOT NULL after setting values
        ALTER TABLE users ALTER COLUMN salt SET NOT NULL;
        RAISE NOTICE 'Set salt as NOT NULL';
    ELSE
        RAISE NOTICE 'Column salt already exists';
    END IF;
END $$;

-- Show result
SELECT column_name, data_type, is_nullable 
FROM information_schema.columns 
WHERE table_name = 'users' 
AND column_name IN ('password_hash', 'salt');
