#!/bin/bash
# Loads a sample 10-router topology into the running OSPF server so
# you have something to query immediately via curl or the frontend.
#
# Usage: ./scripts/seed_topology.sh [host:port]
#   e.g. ./scripts/seed_topology.sh localhost:8080

HOST="${1:-localhost:8080}"

echo "Seeding topology into http://${HOST} ..."

curl -s -X POST "http://${HOST}/topology" \
  -H "Content-Type: application/json" \
  -d '{
    "nodes": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
    "edges": [
      {"u": 1, "v": 2, "cost": 10},
      {"u": 1, "v": 3, "cost": 50},
      {"u": 2, "v": 4, "cost": 15},
      {"u": 3, "v": 4, "cost": 20},
      {"u": 4, "v": 5, "cost": 5},
      {"u": 5, "v": 6, "cost": 12},
      {"u": 6, "v": 7, "cost": 8},
      {"u": 7, "v": 8, "cost": 18},
      {"u": 8, "v": 9, "cost": 6},
      {"u": 9, "v": 10, "cost": 14},
      {"u": 3, "v": 6, "cost": 25},
      {"u": 5, "v": 8, "cost": 30}
    ]
  }'

echo ""
echo "Done. Try:"
echo "  curl \"http://${HOST}/route?src=1&dst=10\""
echo "  curl \"http://${HOST}/table/1\""