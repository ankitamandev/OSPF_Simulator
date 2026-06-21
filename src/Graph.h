#pragma once

#include <unordered_map>
#include <vector>
#include <algorithm>

// A single edge: destination node and link cost.
struct Edge {
    int to;
    int cost;
};

// Weighted undirected graph representing the network topology.
// Routers are integer node IDs. Physical links are bidirectional
// weighted edges. Stored as an adjacency list — O(V+E) space,
// O(degree) neighbour traversal — the right choice for sparse
// networks where each router has only a handful of neighbours.
class Graph {
public:
    std::unordered_map<int, std::vector<Edge>> adj;

    // Inserts u→v and v→u so the link is undirected.
    void addEdge(int u, int v, int cost) {
        adj[u].push_back({v, cost});
        adj[v].push_back({u, cost});
        // Ensure both nodes exist as keys even with no other edges.
        if (adj.find(u) == adj.end()) adj[u] = {};
        if (adj.find(v) == adj.end()) adj[v] = {};
    }

    // Removes both directions of the link between u and v.
    void removeEdge(int u, int v) {
        auto removeFrom = [](std::vector<Edge>& edges, int target) {
            edges.erase(
                std::remove_if(edges.begin(), edges.end(),
                    [target](const Edge& e) { return e.to == target; }),
                edges.end());
        };
        if (adj.count(u)) removeFrom(adj[u], v);
        if (adj.count(v)) removeFrom(adj[v], u);
    }

    // Returns the neighbour list for a node, or an empty list if absent.
    const std::vector<Edge>& neighbours(int u) const {
        static const std::vector<Edge> empty;
        auto it = adj.find(u);
        return it != adj.end() ? it->second : empty;
    }

    // Ensures a node exists in the graph even with no edges yet.
    void addNode(int id) {
        if (adj.find(id) == adj.end()) adj[id] = {};
    }

    size_t nodeCount() const { return adj.size(); }
};