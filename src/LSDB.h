#pragma once

#include "Graph.h"
#include <map>
#include <vector>
#include <utility>

// A Link State Advertisement used by a router broadcasts to describe its own connectivity.
// Simple version of the real OSPF LSA
struct LSA {
    int routerID = 0;
    std::vector<std::pair<int, int>> neighbours; // {neighbourID, cost}
    int seqNum = 0;
};

// Link State Database
class LSDB {
public:
    // Inserts or updates an LSA.
    void update(const LSA& lsa);

    // Reconstruct the full network Graph from all stored LSAs.
    // O(V+E) iterates every LSA and every neighbour pair once.
    Graph buildGraph() const;

    // Simulates a link failure between routers u and v
    long long failLink(int u, int v, int src);

    // Restores a previously failed link with the given cost
    long long restoreLink(int u, int v, int cost, int src);

    // reconvergence cycle rebuild graph and run Dijkstrafrom src
    long long reconverge(int src) const;

    // Remove all LSAs and used by POST /reset before reloading the default topology
    void clear();

    // counts the number of routers
    size_t routerCount() const { return db.size(); }

    const std::map<int, LSA>& all() const { return db; }

private:
    std::map<int, LSA> db; 
};