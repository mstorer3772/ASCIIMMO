#!/bin/bash

# Test token broadcasting between services

echo "=== Token Broadcasting Test ==="
echo ""

# Start services in background
echo "Starting services..."
./build/world-service --port 8080 > logs/world-test.log 2>&1 &
WORLD_PID=$!
./build/session-service --port 8082 > logs/session-test.log 2>&1 &
SESSION_PID=$!
./build/social-service --port 8083 > logs/social-test.log 2>&1 &
SOCIAL_PID=$!

# Wait for services to start
sleep 2

echo "Services started (PIDs: world=$WORLD_PID, session=$SESSION_PID, social=$SOCIAL_PID)"
echo ""

# Create a session (which should broadcast to other services)
echo "Creating session token..."
RESPONSE=$(curl -sk -X POST https://localhost:8082/session \
  -H "Content-Type: application/json" \
  -d '{"user":"testuser","data":"test data"}')

echo "Session response: $RESPONSE"
echo ""

# Extract token (simple grep, would use jq in production)
TOKEN=$(echo $RESPONSE | grep -oP '(?<="token":")[^"]+')
echo "Token created: $TOKEN"
echo ""

# Wait a moment for broadcast to complete
sleep 2

# Check logs to see if token was received by other services
echo "=== World Service Log ==="
tail -5 logs/world-test.log
echo ""

echo "=== Session Service Log ==="
tail -5 logs/session-test.log
echo ""

echo "=== Social Service Log ==="
tail -5 logs/social-test.log
echo ""

# Cleanup
echo "Stopping services..."
kill $WORLD_PID $SESSION_PID $SOCIAL_PID 2>/dev/null
wait $WORLD_PID $SESSION_PID $SOCIAL_PID 2>/dev/null

echo "Test complete!"
