#include "LSDB.h"
#include <algorithm>

void LSDB::update(const LSA& lsa) {
    auto it = db.find(lsa.routerID);
    if (it == db.end() || lsa.seqNum > it->second.seqNum) {
        db[lsa.routerID] = lsa;
    }
}

// Removes neighbour from one LSA. Returns true if anything was removed.
static bool eraseNeighbour(LSA& lsa, int neighbourID) {
    auto& n = lsa.neighbours;
    auto newEnd = std::remove_if(n.begin(), n.end(),
        [neighbourID](const std::pair<int, int>& p) { return p.first == neighbourID; });
    bool changed = newEnd != n.end();
    n.erase(newEnd, n.end());
    return changed;
}

// Sets neighbour's cost in one LSA, adding it if absent.
// Returns true if anything changed.
static bool upsertNeighbour(LSA& lsa, int neighbourID, int cost) {
    for (auto& [id, c] : lsa.neighbours) {
        if (id == neighbourID) {
            if (c == cost) return false;
            c = cost;
            return true;
        }
    }
    lsa.neighbours.push_back({neighbourID, cost});
    return true;
}

bool LSDB::removeLink(int u, int v) {
    auto iu = db.find(u), iv = db.find(v);
    if (iu == db.end() || iv == db.end()) return false;

    LSA a = iu->second, b = iv->second;
    bool changedA = eraseNeighbour(a, v);
    bool changedB = eraseNeighbour(b, u);
    if (!changedA && !changedB) return false;

    // Re-originate each changed LSA with a higher sequence number.
    if (changedA) { a.seqNum++; update(a); }
    if (changedB) { b.seqNum++; update(b); }
    return true;
}

bool LSDB::setLink(int u, int v, int cost) {
    if (u == v || cost <= 0) return false;
    auto iu = db.find(u), iv = db.find(v);
    if (iu == db.end() || iv == db.end()) return false;

    LSA a = iu->second, b = iv->second;
    bool changedA = upsertNeighbour(a, v, cost);
    bool changedB = upsertNeighbour(b, u, cost);
    if (!changedA && !changedB) return false;

    if (changedA) { a.seqNum++; update(a); }
    if (changedB) { b.seqNum++; update(b); }
    return true;
}

std::optional<int> LSDB::linkCost(int u, int v) const {
    auto it = db.find(u);
    if (it == db.end()) return std::nullopt;
    for (const auto& [id, c] : it->second.neighbours)
        if (id == v) return c;
    return std::nullopt;
}

const std::vector<std::pair<int, int>>& LSDB::neighbours(int id) const {
    static const std::vector<std::pair<int, int>> empty;
    auto it = db.find(id);
    return it != db.end() ? it->second.neighbours : empty;
}

int LSDB::maxRouterId() const {
    return db.empty() ? 0 : db.rbegin()->first;
}

void LSDB::clear() {
    db.clear();
}
