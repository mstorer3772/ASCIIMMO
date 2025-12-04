#!/bin/bash
# Shutdown all ASCIIMMO microservices using their /shutdown endpoints

echo "Shutting down services via HTTPS endpoints..."

# Shutdown each service via its /shutdown endpoint
curl -sk -X POST https://localhost:8080/shutdown > /dev/null 2>&1 && echo "  ✓ world-service (8080) shutdown" || echo "  ✗ world-service (8080) not responding"
curl -sk -X POST https://localhost:8081/shutdown > /dev/null 2>&1 && echo "  ✓ auth-service (8081) shutdown" || echo "  ✗ auth-service (8081) not responding"
curl -sk -X POST https://localhost:8082/shutdown > /dev/null 2>&1 && echo "  ✓ session-service (8082) shutdown" || echo "  ✗ session-service (8082) not responding"
curl -sk -X POST https://localhost:8083/shutdown > /dev/null 2>&1 && echo "  ✓ social-service (8083) shutdown" || echo "  ✗ social-service (8083) not responding"

pkill -f "node run server.js"

echo "Shutdown complete"
