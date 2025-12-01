#!/bin/bash

# Comprehensive test of token broadcasting and validation

echo "=== Token Broadcasting and Validation Test ==="
echo ""

# Ensure logs directory exists
mkdir -p logs

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

echo "✓ Services started"
echo ""

# Create a session (which should broadcast to other services)
echo "Step 1: Creating session token..."
RESPONSE=$(curl -sk -X POST https://localhost:8082/session \
  -H "Content-Type: application/json" \
  -d '{"user":"testuser","role":"player"}' 2>/dev/null)

echo "Response: $RESPONSE"

# Extract token using basic text processing
TOKEN=$(echo "$RESPONSE" | grep -oP '(?<="token":")[^"]+')
echo "✓ Token created: $TOKEN"
echo ""

# Wait for broadcast to complete
echo "Step 2: Waiting for token broadcast..."
sleep 2

# Check if world service received the token
echo "Step 3: Checking world service logs..."
if grep -q "Registered token: $TOKEN" logs/world-test.log; then
    echo "✓ World service received and cached token"
else
    echo "✗ World service did NOT receive token"
    echo "  World log:"
    tail -3 logs/world-test.log
fi
echo ""

# Check if social service received the token
echo "Step 4: Checking social service logs..."
if grep -q "Registered token: $TOKEN" logs/social-test.log; then
    echo "✓ Social service received and cached token"
else
    echo "✗ Social service did NOT receive token"
    echo "  Social log:"
    tail -3 logs/social-test.log
fi
echo ""

# Summary
echo "=== Test Summary ==="
echo "Token: $TOKEN"
echo "TTL: 15 minutes (900 seconds)"
echo ""
echo "Services that cached the token:"
grep -l "Registered token: $TOKEN" logs/*-test.log 2>/dev/null | sed 's/logs\//  - /' | sed 's/-test.log//'
echo ""

# Cleanup
echo "Stopping services..."
kill $WORLD_PID $SESSION_PID $SOCIAL_PID 2>/dev/null
wait $WORLD_PID $SESSION_PID $SOCIAL_PID 2>/dev/null

echo "✓ Test complete!"
