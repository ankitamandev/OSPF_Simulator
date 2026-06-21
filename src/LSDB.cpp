#include "LSDB.h"
#include "Dijkstra.h"
#include <chrono>
#include <algorithm>

void LSDB::update(const LSA& lsa) {
    auto it = db.find(lsa.routerID);
    if (it == db.end() || lsa.seqNum > it->second.seqNum) {
        db[lsa.routerID] = lsa;
    }
    // Otherwise: stale update, silently ignored — mirrors real OSPF
    // behaviour of rejecting LSAs with an old sequence number.
}

Graph LSDB::buildGraph() const {
    Graph g;
    for (const auto& [routerID, lsa] : db) {
        g.addNode(routerID);
        for (const auto& [neighbourID, cost] : lsa.neighbours) {
            // addEdge is called once per direction naturally, since
            // both routers in a link carry the other in their LSA.
            // To avoid duplicate edges we only add when routerID < neighbourID,
            // then addEdge itself inserts both directions.
            if (routerID < neighbourID) {
                g.addEdge(routerID, neighbourID, cost);
            } else {
                g.addNode(routerID);
            }
        }
    }
    return g;
}

static void removeNeighbourFromLSA(LSA& lsa, int neighbourID) {
    lsa.neighbours.erase(
        std::remove_if(lsa.neighbours.begin(), lsa.neighbours.end(),
            [neighbourID](const std::pair<int, int>& p) {
                return p.first == neighbourID;
            }),
        lsa.neighbours.end());
}

long long LSDB::failLink(int u, int v, int src) {
    auto t0 = std::chrono::high_resolution_clock::now();

    if (db.count(u)) {
        LSA updated = db[u];
        removeNeighbourFromLSA(updated, v);
        updated.seqNum++;
        update(updated);
    }
    if (db.count(v)) {
        LSA updated = db[v];
        removeNeighbourFromLSA(updated, u);
        updated.seqNum++;
        update(updated);
    }

    Graph g = buildGraph();
    auto [dist, parent] = dijkstra(g, src);

    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
}

long long LSDB::restoreLink(int u, int v, int cost, int src) {
    auto t0 = std::chrono::high_resolution_clock::now();

    if (db.count(u)) {
        LSA updated = db[u];
        updated.neighbours.push_back({v, cost});
        updated.seqNum++;
        update(updated);
    }
    if (db.count(v)) {
        LSA updated = db[v];
        updated.neighbours.push_back({u, cost});
        updated.seqNum++;
        update(updated);
    }

    Graph g = buildGraph();
    auto [dist, parent] = dijkstra(g, src);

    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
}

long long LSDB::reconverge(int src) const {
    auto t0 = std::chrono::high_resolution_clock::now();

    Graph g = buildGraph();
    auto [dist, parent] = dijkstra(g, src);

    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
}