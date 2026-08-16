# OSPF Routing Engine — Simulator

A C++17 simulation of the OSPF (Open Shortest Path First) routing protocol — the link-state protocol standardised in RFC 2328 that runs inside enterprise networks and Cisco IOS devices. Implements Dijkstra's Shortest Path First algorithm from scratch, a Link State Database for topology flooding and reconvergence, a thread-safe REST API, and a live React + D3.js topology visualiser.

## Project Structure
```
ospf-router/
├── CMakeLists.txt                     build config — FetchContent pulls cpp-httplib, json, Catch2
├── Dockerfile                         backend container (builds + runs ospf_router)
├── docker-compose.yml                 one-command full-stack deploy (backend + frontend)
├── .dockerignore                      excludes build/ and node_modules/ — required or Docker build fails
├── README.md                          project docs, API reference, build instructions
│
├── src/
│   ├── Graph.h / Graph.cpp            weighted adjacency list — the network topology
│   ├── Dijkstra.h / Dijkstra.cpp      SPF algorithm from scratch, min-heap based
│   ├── LSDB.h / LSDB.cpp              Link State Database — reconvergence, seq-number freshness, timing
│   ├── DefaultTopology.h / .cpp       20-router enterprise campus network, loaded at startup and on reset
│   ├── Server.h / Server.cpp          REST API, 7 endpoints, shared_mutex reader-writer thread safety
│   ├── main.cpp                       entry point — loads default topology, starts server on port 8080
│   └── benchmark.cpp                  standalone executable — prints min/avg/p50/p99/max reconvergence µs
│
├── tests/
│   ├── test_graph.cpp                 7 tests — addEdge, removeEdge, neighbours, node counting
│   ├── test_dijkstra.cpp              8 tests — shortest path, path reconstruction, unreachable nodes
│   └── test_lsdb.cpp                  7 tests — LSA freshness, failLink, restoreLink, reconvergence
│
├── frontend/
│   ├── index.html                     Vite entry HTML — Inter + JetBrains Mono, global reset
│   ├── package.json                   React + D3 dependencies
│   ├── vite.config.js                 dev server config (port 5173)
│   ├── Dockerfile                     frontend container (nginx serving the build)
│   └── src/
│       ├── main.jsx                   React root render
│       └── App.jsx                    D3 force graph with tier colouring, route / fail / restore / reset controls
│
└── scripts/
    ├── seed_topology.sh               optional — reloads the 20-router topology via curl
    └── run_benchmark.sh               runs benchmark.cpp and saves timestamped results

```

## Architecture

| Module           | Responsibility                                    | Key DSA / technique               |
|------------------|---------------------------------------------------|-----------------------------------|
| Graph            | Weighted adjacency list representing the network  | `unordered_map<int, vector<Edge>>`|
| Dijkstra         | Shortest path computation from any source         | Min-heap, O((V+E) log V)          |
| LSDB             | Link State Database — topology + reconvergence    | Sequence-numbered map updates     |
| DefaultTopology  | 20-router enterprise campus network definition    | Loaded at startup and on reset    |
| Server           | REST API exposing the engine over HTTP            | `std::shared_mutex` (reader-writer)|
| Frontend         | Live topology visualisation                       | React + D3 force graph            |

## Default topology

A 20-router, 30-link enterprise campus network loads automatically at startup — no seeding required.

| Tier         | Routers | Role                                |
|--------------|---------|-------------------------------------|
| Core         | 1–3     | Fully meshed backbone, cost 4 links |
| Distribution | 4–8     | Connect core to access layer        |
| Edge/Access  | 9–20    | End-point routers, redundant paths  |

## Prerequisites

- C++17 compiler (GCC 9+ or Clang 10+)
- CMake 3.20+
- Node.js 18+ (for the frontend)
- Docker (optional, for one-command deployment)

## Build and run (local)

```bash
# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run unit tests
cd build && ctest --output-on-failure && cd ..

# Run the reconvergence benchmark
./build/benchmark 50 1000

# Start the server — topology loads automatically
./build/ospf_router
```

In a separate terminal, try the API:

```bash
curl "http://localhost:8080/route?src=1&dst=20"
curl "http://localhost:8080/table/1"
curl -X POST http://localhost:8080/fail    -H "Content-Type: application/json" -d '{"u":8,"v":17}'
curl -X POST http://localhost:8080/restore -H "Content-Type: application/json" -d '{"u":8,"v":17,"cost":15}'
curl -X POST http://localhost:8080/reset
```

## Run the frontend

```bash
cd frontend
npm install
npm run dev
```

Open `http://localhost:5173`.

## Run everything with Docker

```bash
docker compose up --build
```

Backend on `localhost:8080`, frontend on `localhost:3000`.  
The topology loads automatically nothing else to run.

> **Note:** a `.dockerignore` excluding `build/` and `frontend/node_modules/` is required. Without it, a locally generated `CMakeCache.txt` is copied into the image and CMake fails with a path-mismatch error.

## API reference

| Method | Endpoint              | Body / Params           | Description                             |
|--------|-----------------------|-------------------------|-----------------------------------------|
| GET    | `/topology`           | —                       | Returns current graph `{nodes, edges}`  |
| POST   | `/topology`           | `{nodes, edges}`        | Loads a custom topology                 |
| GET    | `/route`              | `?src=X&dst=Y`          | Shortest path and cost between routers  |
| GET    | `/table/:id`          | —                       | Full routing table for one router       |
| POST   | `/fail`               | `{u, v}`                | Simulates a link failure                |
| POST   | `/restore`            | `{u, v, cost}`          | Restores a failed link (cost default 10)|
| POST   | `/reset`              | —                       | Reloads the default 20-router topology  |

All read endpoints acquire a `shared_lock`; all write endpoints acquire a `unique_lock` on the same `std::shared_mutex`, allowing concurrent reads while serialising topology mutations.

## Algorithms and complexity

| Operation                    | Complexity      |
|------------------------------|-----------------|
| Dijkstra's SPF               | O((V+E) log V)  |
| Graph rebuild from LSDB      | O(V+E)          |
| Adjacency list space         | O(V+E)          |
| LSA update (sequence check)  | O(log V)        |

## Why Dijkstra over Bellman-Ford

Dijkstra is O((V+E) log V) versus Bellman-Ford's O(VE). Real networks are sparse (E ≈ 2–4V), and link costs are always positive, so Bellman-Ford's only advantage — handling negative weights — is irrelevant here. This is exactly why OSPF standardised on Dijkstra.

## Why adjacency list over adjacency matrix

A router typically has 4–8 physical neighbours regardless of network size. An adjacency matrix costs O(V²) space — mostly wasted on non-existent links. The adjacency list costs O(V+E), which at 500 routers is roughly 2,000 entries versus 250,000 for a matrix.

## Benchmark results

Reconvergence latency over 1,000 randomised link-failure events on a 50-node topology, measured with `std::chrono::high_resolution_clock`:

```
min : 14 µs
avg : 42 µs
p50 : 39 µs
p99 : 91 µs
max : 109 µs
```

Reproduce with `./build/benchmark 50 1000`.

