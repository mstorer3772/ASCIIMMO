#!/bin/bash

# Check if auth service is running
echo "Checking if auth service is running on port 8081..."
if ! curl -k -s --max-time 2 https://localhost:8081/auth/register -X POST \
  -H "Content-Type: application/json" \
  -d '{"username":"test"}' 2>/dev/null | grep -q "status"; then
  echo "ERROR: Auth service is not running or not responding on port 8081"
  echo "Please start the auth service first: ./build/auth-service"
  exit 1
fi

echo "Auth service is running ✓"
echo ""

# Test registration endpoint
echo "Testing registration..."
RESPONSE=$(curl -k -s -X POST https://localhost:8081/auth/register \
  -H "Content-Type: application/json" \
  -d '{"username":"testuser","password":"testpass123","email":"test@example.com"}' 2>/dev/null)

echo "$RESPONSE" | jq '.'

if echo "$RESPONSE" | jq -e '.status == "ok"' >/dev/null 2>&1; then
  echo ""
  echo "✓ Registration successful!"
  echo "Check the auth-service logs for the confirmation email (it's in debug mode)"
elif echo "$RESPONSE" | jq -e '.message' | grep -q "already exists"; then
  echo ""
  echo "⚠ User already exists. Try a different username or clean the database."
else
  echo ""
  echo "✗ Registration failed"
fi
