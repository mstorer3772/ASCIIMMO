#!/bin/bash
# Launch all ASCIIMMO microservices with HTTPS support

# Default certificate paths
CERT_FILE="${CERT_FILE:-certs/server.crt}"
KEY_FILE="${KEY_FILE:-certs/server.key}"

# Create logs directory if it doesn't exist
mkdir -p logs

# Launch services with SSL certificates
build/world-service --cert "$CERT_FILE" --key "$KEY_FILE" >> logs/world.log 2>&1 &
build/auth-service --cert "$CERT_FILE" --key "$KEY_FILE" >> logs/auth.log 2>&1 &
build/session-service --cert "$CERT_FILE" --key "$KEY_FILE" >> logs/session.log 2>&1 &
build/social-service --cert "$CERT_FILE" --key "$KEY_FILE" >> logs/social.log 2>&1 &

echo "Services launched with HTTPS:"
echo "  world-service   -> https://localhost:8080 (logs/world.log)"
echo "  auth-service    -> https://localhost:8081 (logs/auth.log)"
echo "  session-service -> https://localhost:8082 (logs/session.log)"
echo "  social-service  -> https://localhost:8083 (logs/social.log)"
