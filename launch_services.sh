#!/bin/bash
# Launch all ASCIIMMO microservices
# Services read configuration from config/services.yaml

# Config file path (can be overridden)
CONFIG_FILE="${CONFIG_FILE:-config/services.yaml}"

# Create logs directory if it doesn't exist
mkdir -p logs

echo "Starting ASCIIMMO services..."
echo "Using config file: $CONFIG_FILE"
echo ""

# Launch services (they read ports and certs from config)
build/world-service --config "$CONFIG_FILE" >> logs/world.log 2>&1 &
build/auth-service --config "$CONFIG_FILE" >> logs/auth.log 2>&1 &
build/session-service --config "$CONFIG_FILE" >> logs/session.log 2>&1 &
build/social-service --config "$CONFIG_FILE" >> logs/social.log 2>&1 &

echo "Services launched with HTTPS:"
echo "  world-service   -> https://localhost:8080 (logs/world.log)"
echo "  auth-service    -> https://localhost:8081 (logs/auth.log)"
echo "  session-service -> https://localhost:8082 (logs/session.log)"
echo "  social-service  -> https://localhost:8083 (logs/social.log)"
echo ""
echo "Configuration in: $CONFIG_FILE"
echo "To change ports or settings, edit the config file or use --port --cert --key flags"
