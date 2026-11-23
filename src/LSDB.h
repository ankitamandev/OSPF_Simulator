#pragma once

#include <map>
#include <vector>
#include <utility>
#include <optional>

// A Link State Advertisement — what one router announces about its own
// directly attached links. Mirrors a simplified OSPF Router LSA (Type 1).
struct LSA {
    int routerID = 0;
    std::vector<std::pair<int, int>> neighbours;   // {neighbourID, cost}
    int seqNum = 0;
};

// The Link State Database: one LSA per router. In real OSPF every router
// holds an identical copy after flooding; here a single shared instance
// models that converged state.
//
// The LSDB is now the topology SPF runs on directly — there is no
// intermediate Graph. It only mutates state; running SPF and timing it
// is the caller's job (see SpfEngine and benchmark.cpp).
class LSDB {
public:
    // Accepts an LSA only if its sequence number is strictly greater than
    // the stored one, so stale or duplicate LSAs can never overwrite
    // fresher topology — the OSPF freshness rule.
    void update(const LSA& lsa);

    // Removes the link u–v from both routers' LSAs and bumps both
    // sequence numbers. Returns false (and changes nothing) if the link
    // didn't exist — a router only re-originates when its links change.
    bool removeLink(int u, int v);

    // Adds link u–v with the given cost, or updates its cost if it
    // already exists. Idempotent: never creates duplicate entries.
    // Returns false if either router is unknown or nothing changed.
    bool setLink(int u, int v, int cost);

    // Cost of link u–v as advertised by u, if it exists.
    std::optional<int> linkCost(int u, int v) const;

    // What SPF reads: a router's advertised links. Same shape as
    // Graph::neighbours so the templated dijkstra() accepts either.
    const std::vector<std::pair<int, int>>& neighbours(int id) const;

    // Largest router ID — O(1), since std::map keeps keys sorted.
    int maxRouterId() const;

    // Removes all LSAs. Required before reloading a topology: update()
    // rejects LSAs whose sequence number isn't higher than the stored
    // one, so a plain reload after failures would be silently ignored.
    void clear();

    size_t routerCount() const { return db.size(); }
    const std::map<int, LSA>& all() const { return db; }

private:
    std::map<int, LSA> db;   // routerID -> its LSA
};
