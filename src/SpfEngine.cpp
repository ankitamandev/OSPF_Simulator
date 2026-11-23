#include "SpfEngine.h"
#include <queue>
#include <sstream>

SpfEngine::SpfEngine(const LSDB& lsdb, int root) : lsdb_(lsdb), root_(root) {
    fullRecompute();
}

void SpfEngine::fullRecompute() {
    spt_ = dijkstra(lsdb_, root_);
    touched_ = static_cast<int>(lsdb_.routerCount());
}

void SpfEngine::onLinkDown(int u, int v) {
    touched_ = 0;
    if (!inRange(u) || !inRange(v) ||
        static_cast<int>(spt_.dist.size()) != lsdb_.maxRouterId() + 1) {
        fullRecompute();                     // topology resized — start over
        return;
    }

    // Which endpoint hangs below the edge in the tree?
    int child;
    if      (spt_.parent[v] == u) child = v;
    else if (spt_.parent[u] == v) child = u;
    else return;                             // non-tree edge: nothing changes

    auto& dist   = spt_.dist;
    auto& parent = spt_.parent;
    const int n  = static_cast<int>(dist.size());

    //  Collect child's subtree. Children of x are exactly the neighbours
    //    y with parent[y] == x — tree edges are real links, so no separate
    //    child list needs to be maintained.
    std::vector<char> affected(n, 0);
    std::vector<int>  subtree{child};
    affected[child] = 1;
    for (size_t i = 0; i < subtree.size(); ++i) {
        int x = subtree[i];
        for (const auto& [y, w] : lsdb_.neighbours(x)) {
            if (y >= 0 && y < n && !affected[y] && parent[y] == x) {
                affected[y] = 1;
                subtree.push_back(y);
            }
        }
    }
    touched_ = static_cast<int>(subtree.size());

    // Invalidate the subtree.
    for (int x : subtree) { dist[x] = UNREACHABLE; parent[x] = NO_PARENT; }

    // Reseed each subtree node from its best neighbour outside the subtree.
    // Nodes outside are still optimal: weights only went up, and their
    // tree paths never used the changed edge.
    using Entry = std::pair<int, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> pq;
    for (int x : subtree) {
        for (const auto& [y, w] : lsdb_.neighbours(x)) {
            if (y < 0 || y >= n || affected[y] || dist[y] == UNREACHABLE) continue;
            if (dist[y] + w < dist[x]) { dist[x] = dist[y] + w; parent[x] = y; }
        }
        if (dist[x] != UNREACHABLE) pq.push({dist[x], x});
    }

    // Dijkstra over the affected region. Only subtree nodes can still
    // improve, so the relaxation test naturally stops at its boundary.
    while (!pq.empty()) {
        auto [d, x] = pq.top();
        pq.pop();
        if (d > dist[x]) continue;
        for (const auto& [y, w] : lsdb_.neighbours(x)) {
            if (y < 0 || y >= n) continue;
            if (d + w < dist[y]) { dist[y] = d + w; parent[y] = x; pq.push({dist[y], y}); }
        }
    }
    // Subtree nodes still at UNREACHABLE are genuinely partitioned off.
}

void SpfEngine::onLinkUp(int u, int v, int cost) {
    touched_ = 0;
    if (!inRange(u) || !inRange(v) ||
        static_cast<int>(spt_.dist.size()) != lsdb_.maxRouterId() + 1) {
        fullRecompute();
        return;
    }

    auto& dist   = spt_.dist;
    auto& parent = spt_.parent;
    const int n  = static_cast<int>(dist.size());

    using Entry = std::pair<int, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> pq;

    // Seed: does the new or cheaper link shorten the path to either end?
    auto trySeed = [&](int from, int to) {
        if (dist[from] != UNREACHABLE && dist[from] + cost < dist[to]) {
            dist[to] = dist[from] + cost;
            parent[to] = from;
            pq.push({dist[to], to});
            ++touched_;
        }
    };
    trySeed(u, v);
    trySeed(v, u);

    // Propagate improvements outward. 
    // Weights only went down, so a node is touched only if its distance strictly decreases.
    while (!pq.empty()) {
        auto [d, x] = pq.top();
        pq.pop();
        if (d > dist[x]) continue;
        for (const auto& [y, w] : lsdb_.neighbours(x)) {
            if (y < 0 || y >= n) continue;
            if (d + w < dist[y]) {
                dist[y] = d + w;
                parent[y] = x;
                pq.push({dist[y], y});
                ++touched_;
            }
        }
    }
}

bool verifySpt(const LSDB& lsdb, int root, const SPT& cand, std::string* why) {
    SPT ref = dijkstra(lsdb, root);
    auto fail = [&](const std::string& msg) { if (why) *why = msg; return false; };

    if (cand.dist.size() != ref.dist.size() || cand.parent.size() != ref.dist.size())
        return fail("size mismatch");

    const int n = static_cast<int>(ref.dist.size());
    for (int v = 0; v < n; ++v) {
        if (cand.dist[v] != ref.dist[v]) {
            std::ostringstream s;
            s << "dist[" << v << "] = " << cand.dist[v] << ", expected " << ref.dist[v];
            return fail(s.str());
        }
        if (v == root) {
            if (cand.parent[v] != NO_PARENT) return fail("root has a parent");
            continue;
        }
        if (cand.dist[v] == UNREACHABLE) {
            if (cand.parent[v] != NO_PARENT) return fail("unreachable node has a parent");
            continue;
        }
        int p = cand.parent[v];
        if (p < 0 || p >= n) {
            std::ostringstream s; s << "node " << v << " has invalid parent " << p;
            return fail(s.str());
        }
        auto c = lsdb.linkCost(p, v);
        if (!c || cand.dist[p] == UNREACHABLE || cand.dist[p] + *c != cand.dist[v]) {
            std::ostringstream s;
            s << "parent " << p << " of " << v << " is not on a shortest path";
            return fail(s.str());
        }
    }
    // Distances strictly increase along parent pointers (all costs > 0),
    // so matching distances + valid parents also rules out cycles.
    return true;
}
