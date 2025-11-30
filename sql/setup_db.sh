#!/bin/bash
# Setup ASCIIMMO PostgreSQL database

set -e

DB_NAME="asciimmo"
DB_USER="asciimmo_user"
DB_PASSWORD="asciimmo_dev_password"

echo "Creating PostgreSQL database and user for ASCIIMMO..."

# Run as postgres user
sudo -u postgres psql <<EOF
-- Create user if doesn't exist
DO \$\$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_user WHERE usename = '$DB_USER') THEN
        CREATE USER $DB_USER WITH PASSWORD '$DB_PASSWORD';
    END IF;
END
\$\$;

-- Create database if doesn't exist
SELECT 'CREATE DATABASE $DB_NAME OWNER $DB_USER'
WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = '$DB_NAME')\gexec

-- Grant privileges
GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO $DB_USER;
EOF

echo "Database created. Loading schema..."

# Load schema
PGPASSWORD=$DB_PASSWORD psql -U $DB_USER -d $DB_NAME -f "$(dirname "$0")/schema.sql"

echo "Setup complete!"
echo ""
echo "Connection details:"
echo "  Database: $DB_NAME"
echo "  User: $DB_USER"
echo "  Password: $DB_PASSWORD"
echo ""
echo "Set environment variables:"
echo "  export ASCIIMMO_DB_NAME=$DB_NAME"
echo "  export ASCIIMMO_DB_USER=$DB_USER"
echo "  export ASCIIMMO_DB_PASSWORD=$DB_PASSWORD"
