#pragma once

#include "Graph.h"
#include <map>
#include <vector>
#include <utility>

// A Link State Advertisement — what a router broadcasts to describe
// its own connectivity. Mirrors the real OSPF LSA structure (simplified).
struct LSA {
    int routerID = 0;
    std::vector<std::pair<int, int>> neighbours; // {neighbourID, cost}
    int seqNum = 0;
};

// The Link State Database. In real OSPF, every router floods its LSA
// to the whole network so that every router ends up with an identical
// copy of the LSDB — a complete map of the topology. This class
// simulates that shared, converged state in a single process.
class LSDB {
public:
    // Inserts or updates an LSA. Only accepted if its sequence number
    // is strictly greater than the currently stored one for that
    // router — this prevents stale/out-of-order updates from
    // overwriting fresher topology data.
    void update(const LSA& lsa);

    // Reconstructs the full network Graph from all stored LSAs.
    // O(V+E) — iterates every LSA and every neighbour pair once.
    Graph buildGraph() const;

    // Simulates a link failure between routers u and v: removes the
    // edge from both routers' LSAs (bumping their sequence numbers)
    // and updates the LSDB. Returns the reconvergence time in
    // microseconds (rebuild graph + re-run Dijkstra from `src`).
    long long failLink(int u, int v, int src);

    // Restores a previously failed link with the given cost.
    long long restoreLink(int u, int v, int cost, int src);

    // Times a full reconvergence cycle (rebuild graph + run Dijkstra
    // from src) without changing any topology. Useful for benchmarking
    // a "steady state" computation cost.
    long long reconverge(int src) const;

    size_t routerCount() const { return db.size(); }

    const std::map<int, LSA>& all() const { return db; }

private:
    std::map<int, LSA> db; // routerID -> its LSA
};