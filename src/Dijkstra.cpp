#include "Dijkstra.h"
#include <algorithm>

std::vector<int> reconstructPath(const std::vector<int>& parent, int src, int dst) {
    const int n = static_cast<int>(parent.size());
    if (src < 0 || src >= n || dst < 0 || dst >= n) return {};
    if (src == dst) return {src};

    std::vector<int> path;
    int current = dst;
    while (current != src) {
        path.push_back(current);
        current = parent[current];
        if (current == NO_PARENT) return {};            // unreachable
        if (static_cast<int>(path.size()) > n) return {}; // defensive: cycle
    }
    path.push_back(src);
    std::reverse(path.begin(), path.end());
    return path;
}
