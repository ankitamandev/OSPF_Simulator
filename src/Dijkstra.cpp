#include "Dijkstra.h"
#include <queue>

// Implements OSPF's Shortest Path First computation — RFC 2328 §16.1.
//
// Uses std::priority_queue as a min-heap over {distance, node} pairs.
// Lazy deletion handles stale heap entries: when a node is popped whose
// recorded distance no longer matches the best known distance, it means
// a better path was already found and processed, so we simply skip it
// rather than maintaining a decrease-key operation (which std::priority_queue
// doesn't support natively).
std::pair<std::map<int, int>, std::map<int, int>>
dijkstra(const Graph &g, int src)
{
    std::map<int, int> dist;
    std::map<int, int> parent;

    for (const auto &[node, _] : g.adj)
    {
        dist[node] = UNREACHABLE;
    }
    dist[src] = 0;

    // Min-heap: smallest distance is popped first via std::greater<>.
    std::priority_queue<
        std::pair<int, int>,
        std::vector<std::pair<int, int>>,
        std::greater < >>
            pq;

    pq.push({0, src});

    while (!pq.empty())
    {
        auto [d, u] = pq.top();
        pq.pop();

        // Stale entry, a shorter path to u was already processed.
        if (d > dist[u])
            continue;

        for (const auto &edge : g.neighbours(u))
        {
            int v = edge.to;
            int newDist = dist[u] + edge.cost;
            if (dist.find(v) == dist.end() || newDist < dist[v])
            {
                dist[v] = newDist;
                parent[v] = u;
                pq.push({newDist, v});
            }
        }
    }

    return {dist, parent};
}

std::vector<int> reconstructPath(const std::map<int, int> &parent,
                                 int src, int dst)
{
    if (src == dst)
        return {src};

    std::vector<int> path;
    int current = dst;

    while (current != src)
    {
        path.push_back(current);
        auto it = parent.find(current);
        if (it == parent.end())
        {
            // No path exists from src to dst.
            return {};
        }
        current = it->second;
    }
    path.push_back(src);

    std::reverse(path.begin(), path.end());
    return path;
}