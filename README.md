# OSPF Routing Engine — Simulator

A C++17 simulation of the OSPF (Open Shortest Path First) routing protocol — the link-state protocol standardised in RFC 2328 that runs inside enterprise networks and Cisco IOS devices. Implements Dijkstra's Shortest Path First algorithm from scratch, a Link State Database for topology flooding and reconvergence, a thread-safe REST API, and a live React + D3.js topology visualiser.

## Architecture

| Module    | Responsibility                                      | Key DSA / technique              |
|-----------|------------------------------------------------------|-----------------------------------|
| Graph     | Weighted adjacency list representing the network     | `unordered_map<int, vector<Edge>>`|
| Dijkstra  | Shortest path computation from any source             | Min-heap, O((V+E) log V)          |
| LSDB      | Link State Database — topology + reconvergence        | Sequence-numbered map updates     |
| Server    | REST API exposing the engine over HTTP                 | `std::shared_mutex` (reader-writer)|
| Frontend  | Live topology visualisation                            | React + D3 force graph            |

## Prerequisites

- C++17 compiler (GCC 9+ or Clang 10+)
- CMake 3.20+
- Node.js 18+ (for the frontend)
- Docker (optional, for one-command deployment)

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run unit tests
cd build && ctest --output-on-failure && cd ..

# Run the reconvergence benchmark
./build/benchmark 50 1000

# Start the server
./build/ospf_router
\`\`\`

In a separate terminal, seed a test topology and try the API:

\`\`\`bash
chmod +x scripts/seed_topology.sh
./scripts/seed_topology.sh

curl "http://localhost:8080/route?src=1&dst=10"
curl "http://localhost:8080/table/1"
curl -X POST http://localhost:8080/fail -H "Content-Type: application/json" -d '{"u":1,"v":2}'
\`\`\`

## Run the frontend

\`\`\`bash
cd frontend
npm install
npm run dev
\`\`\`

Open `http://localhost:5173`.

## Run everything with Docker

\`\`\`bash
docker compose up --build
\`\`\`

Backend on `localhost:8080`, frontend on `localhost:3000`.

## API reference

| Method | Endpoint              | Description                          |
|--------|------------------------|---------------------------------------|
| GET    | `/topology`            | Returns current graph `{nodes, edges}`|
| POST   | `/topology`             | Loads a topology `{nodes, edges}`     |
| GET    | `/route?src=X&dst=Y`   | Shortest path between two routers     |
| POST   | `/fail`                 | Simulates link failure `{u, v}`       |
| GET    | `/table/:id`            | Full routing table for a router       |

## Algorithms and complexity

| Operation                  | Complexity         |
|-----------------------------|---------------------|
| Dijkstra's SPF               | O((V+E) log V)      |
| Graph rebuild from LSDB      | O(V+E)               |
| Adjacency list space          | O(V+E)               |
| LSA update (sequence check)  | O(log V)             |

## Why Dijkstra over Bellman-Ford

Dijkstra is O((V+E) log V) versus Bellman-Ford's O(VE). Real networks are sparse (E ≈ 2-4V), and link costs are always positive, so Bellman-Ford's only advantage — handling negative weights — is irrelevant here. This is exactly why OSPF standardised on Dijkstra.

## Why adjacency list over adjacency matrix

A router typically has 4-8 physical neighbours regardless of network size. An adjacency matrix costs O(V²) space — mostly wasted on non-existent links. The adjacency list costs O(V+E), which at 500 routers is roughly 2,000 entries versus 250,000 for a matrix.

## Benchmark results

Run `./build/benchmark 50 1000` to see results 
for example :-
\`\`\`
min : 14 us
avg : 42 us
p50 : 39 us
p99 : 91 us
max : 109 us

\`\`\`

