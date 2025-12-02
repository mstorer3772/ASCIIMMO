#!/bin/bash
# Set admin and email_confirmed flags for a user

USERNAME="${1:-$ASCIIMMO_USER}"

if [ -z "$USERNAME" ]; then
    echo "Usage: $0 <username>"
    echo "Or set ASCIIMMO_USER environment variable"
    exit 1
fi

echo "Setting admin=true and email_confirmed=true for user: $USERNAME"

sudo -u postgres psql -d asciimmo <<EOF
UPDATE users 
SET admin = true, email_confirmed = true 
WHERE username = '$USERNAME';

SELECT id, username, email, admin, email_confirmed 
FROM users 
WHERE username = '$USERNAME';
EOF

if [ $? -eq 0 ]; then
    echo "Successfully updated user: $USERNAME"
else
    echo "Error updating user: $USERNAME"
    exit 1
fi
