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
WORLD_PID=$!
build/auth-service --config "$CONFIG_FILE" >> logs/auth.log 2>&1 &
AUTH_PID=$!
build/session-service --config "$CONFIG_FILE" >> logs/session.log 2>&1 &
SESSION_PID=$!
build/social-service --config "$CONFIG_FILE" >> logs/social.log 2>&1 &
SOCIAL_PID=$!

# Launch web server for client files
cd client
sudo pm run serve >> logs/web.log 2>&1 &
WEB_PID=$!

# Give services a moment to start
sleep 1

# Check if each service is running
check_service() {
    local name=$1
    local pid=$2
    local port=$3
    
    if ps -p $pid > /dev/null 2>&1; then
        echo "  ✓ $name (PID: $pid) -> https://localhost:$port"
        return 0
    else
        echo "  ✗ $name failed to start (check logs/$name.log)"
        return 1
    fi
}

echo "Service Status:"
check_service "world-service  " $WORLD_PID 8080
check_service "auth-service   " $AUTH_PID 8081
check_service "session-service" $SESSION_PID 8082
check_service "social-service " $SOCIAL_PID 8083
check_service "npm run serve" $WEB_PID 80
echo ""
echo "Configuration: $CONFIG_FILE"
echo "Logs directory: logs/"
echo ""
echo "To stop all services: './shutdown_services.sh'"
