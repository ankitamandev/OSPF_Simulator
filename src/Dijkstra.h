#pragma once

#include <vector>
#include <queue>
#include <climits>
#include <utility>

constexpr int UNREACHABLE = INT_MAX;
constexpr int NO_PARENT   = -1;

// Shortest path tree rooted at one router (RFC 2328 calls this the SPT).
// Indexed directly by router ID: dist[v] is the cost from the root to v,
// parent[v] is v's predecessor on that path. Router IDs are small dense
// integers, so plain vectors replace the old std::map — one allocation
// each instead of one heap node per router, and contiguous in memory.
struct SPT {
    std::vector<int> dist;
    std::vector<int> parent;
};

// Dijkstra's Shortest Path First — RFC 2328 §16.1.
//
// Templated on the topology type so it runs directly on the LSDB (the
// production path) as well as on a Graph (unit tests). Any type works
// if it provides:
//     int  maxRouterId() const;
//     neighbours(int u) const  -> iterable of {neighbourId, cost} pairs
//
// Uses a binary min-heap with lazy deletion: std::priority_queue has no
// decrease-key, so a better path pushes a fresh entry and the outdated
// one is skipped when it surfaces. O((V+E) log V).
template <typename Topology>
SPT dijkstra(const Topology& topo, int src) {
    const int n = topo.maxRouterId() + 1;
    SPT t{std::vector<int>(n, UNREACHABLE), std::vector<int>(n, NO_PARENT)};
    if (src < 0 || src >= n) return t;

    t.dist[src] = 0;
    using Entry = std::pair<int, int>;                  // {distance, router}
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> pq;
    pq.push({0, src});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d > t.dist[u]) continue;                    // stale entry

        for (const auto& [v, w] : topo.neighbours(u)) {
            if (v < 0 || v >= n) continue;
            if (d + w < t.dist[v]) {
                t.dist[v]   = d + w;
                t.parent[v] = u;
                pq.push({t.dist[v], v});
            }
        }
    }
    return t;
}

// this walks parent pointers back from dst to src and Returns an empty vector
// if dst is unreachable or out of range. O(path length).
std::vector<int> reconstructPath(const std::vector<int>& parent, int src, int dst);
