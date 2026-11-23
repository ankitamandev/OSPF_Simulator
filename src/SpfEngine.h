#pragma once

#include "LSDB.h"
#include "Dijkstra.h"
#include <string>

// Maintains one router's shortest path tree across topology changes,
// either by full recomputation or incrementally.
//
// Incremental strategy (dynamic Dijkstra, in the spirit of Narváez et
// al.'s dynamic SPT framework):
//
//   Link down / cost up on a TREE edge (p, c) where parent[c] == p:
//     only c's subtree can change. Invalidate that subtree, reseed each
//     of its nodes from neighbours outside it, then run Dijkstra over
//     the affected region only. Everything outside keeps its distance.
//   Link down on a NON-tree edge: no shortest path used it — no work.
//   Link up / cost down on (u, v): if it shortens the path to either
//     endpoint, relax outward from there; only improving nodes are touched.
//
// Always call the handler AFTER the LSDB has been mutated.
class SpfEngine {
public:
    SpfEngine(const LSDB& lsdb, int root);

    void fullRecompute();
    void onLinkDown(int u, int v);           // removed, or cost increased
    void onLinkUp(int u, int v, int cost);   // added, or cost decreased

    const SPT& tree() const { return spt_; }
    int root() const { return root_; }

    // Nodes whose distance was reset or updated by the last operation —
    // a hardware-independent work measure, reported alongside time.
    int lastTouched() const { return touched_; }

private:
    const LSDB& lsdb_;
    int root_;
    SPT spt_;
    int touched_ = 0;

    bool inRange(int id) const {
        return id >= 0 && id < static_cast<int>(spt_.dist.size());
    }
};

// Correctness oracle. Checks that `candidate` is a valid shortest path
// tree for the LSDB rooted at `root`, using a fresh full recompute as
// ground truth:
//   1. every distance matches the reference exactly, and
//   2. every parent pointer is a real link with dist[p] + cost == dist[v],
//      with unreachable routers having no parent.
// Parents are NOT required to match the reference — with equal-cost
// paths, several valid trees exist and either algorithm may pick any.
bool verifySpt(const LSDB& lsdb, int root, const SPT& candidate, std::string* why = nullptr);
