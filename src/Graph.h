#pragma once

#include <unordered_map>
#include <vector>
#include <algorithm>

// A single edge: destination node and link cost.
struct Edge {
    int to;
    int cost;
};

// Weighted undirected graph. No longer used on the reconvergence path —
// SPF now runs directly on the LSDB. Kept as a general-purpose topology
// type for unit tests and for any caller that wants a standalone graph.
class Graph {
public:
    std::unordered_map<int, std::vector<Edge>> adj;

    // Inserts u→v and v→u so the link is undirected.
    // operator[] default-constructs a missing key, so both nodes exist
    // after this call without any extra guard.
    void addEdge(int u, int v, int cost) {
        adj[u].push_back({v, cost});
        adj[v].push_back({u, cost});
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

    // Largest router ID present — SPF sizes its arrays from this.
    int maxRouterId() const {
        int m = 0;
        for (const auto& [id, _] : adj) m = std::max(m, id);
        return m;
    }

    size_t nodeCount() const { return adj.size(); }
};
