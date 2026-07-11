

#!/bin/bash
# Loads a realistic 20-router enterprise campus topology into the running
# OSPF server. Routers 1-3 are core, 4-8 distribution, 9-20 edge/access.
#
# Usage: ./scripts/seed_topology.sh [host:port]
HOST="${1:-localhost:8080}"

echo "Seeding 20-router enterprise topology into http://${HOST} ..."

curl -s -X POST "http://${HOST}/topology" \
  -H "Content-Type: application/json" \
  -d '{
    "nodes": [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20],
    "edges": [
      {"u":1,"v":2,"cost":4},
      {"u":2,"v":3,"cost":4},
      {"u":1,"v":3,"cost":4},
      {"u":1,"v":4,"cost":10},
      {"u":1,"v":5,"cost":10},
      {"u":2,"v":6,"cost":10},
      {"u":2,"v":7,"cost":10},
      {"u":3,"v":8,"cost":10},
      {"u":3,"v":5,"cost":12},
      {"u":4,"v":5,"cost":8},
      {"u":6,"v":7,"cost":8},
      {"u":4,"v":9,"cost":15},
      {"u":4,"v":10,"cost":15},
      {"u":5,"v":11,"cost":15},
      {"u":5,"v":12,"cost":18},
      {"u":6,"v":13,"cost":15},
      {"u":6,"v":14,"cost":20},
      {"u":7,"v":15,"cost":15},
      {"u":7,"v":16,"cost":18},
      {"u":8,"v":17,"cost":15},
      {"u":8,"v":18,"cost":15},
      {"u":8,"v":19,"cost":20},
      {"u":9,"v":10,"cost":5},
      {"u":11,"v":12,"cost":5},
      {"u":13,"v":14,"cost":5},
      {"u":15,"v":16,"cost":5},
      {"u":17,"v":18,"cost":5},
      {"u":10,"v":20,"cost":25},
      {"u":16,"v":20,"cost":22},
      {"u":19,"v":20,"cost":18}
    ]
  }'

echo ""
echo "Done. 20 routers, 30 links loaded."
echo ""
echo "Try:"
echo "  curl \"http://${HOST}/route?src=1&dst=20\""
echo "  curl \"http://${HOST}/route?src=9&dst=17\""
echo "  curl \"http://${HOST}/table/1\""