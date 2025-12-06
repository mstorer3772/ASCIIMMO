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
build/world-service --config "$CONFIG_FILE" > logs/world.log 2>&1 &
WORLD_PID=$!
#build/world-service --config "$CONFIG_FILE" > logs/social.log 2>&1 &
#SOCIAL_PID=$!

# make sure cloudflare tunnel is up
cloudflared tunnel run asciimmo > logs/tunnel.log 2>&1 &
TUNNEL_PID=$!

# Give services a moment to start
sleep 1

echo "Service Status:"
ps aux | grep $WORLD_PID | grep -v grep
ps aux | grep $TUNNEL_PID | grep -v grep
#ps aux | grep $SOCIAL_PID | grep -v grep

echo "Configuration: $CONFIG_FILE"
echo "Logs directory: logs/"
echo ""
echo "To stop all services: './shutdown_services.sh'"
