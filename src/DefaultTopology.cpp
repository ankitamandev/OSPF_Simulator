#include "DefaultTopology.h"
#include <vector>
#include <tuple>

void loadDefaultTopology(LSDB& lsdb) {
    lsdb.clear();   // update() rejects stale seqNums, so start from empty

    for (int i = 1; i <= 20; i++) {
        lsdb.update(LSA{i, {}, 1});
    }

    // {u, v, cost} — mirrors scripts/seed_topology.sh exactly
    const std::vector<std::tuple<int, int, int>> edges = {
        {1,  2,  4}, {2,  3,  4}, {1,  3,  4},                       // core mesh
        {1,  4, 10}, {1,  5, 10}, {2,  6, 10},                       // core -> dist
        {2,  7, 10}, {3,  8, 10}, {3,  5, 12},
        {4,  5,  8}, {6,  7,  8},                                    // dist cross-links
        {4,  9, 15}, {4, 10, 15}, {5, 11, 15}, {5, 12, 18},          // dist -> edge
        {6, 13, 15}, {6, 14, 20}, {7, 15, 15}, {7, 16, 18},
        {8, 17, 15}, {8, 18, 15}, {8, 19, 20},
        {9, 10,  5}, {11, 12, 5}, {13, 14, 5}, {15, 16, 5}, {17, 18, 5},  // edge cross-links
        {10, 20, 25}, {16, 20, 22}, {19, 20, 18},                    // paths to router 20
    };

    for (const auto& [u, v, cost] : edges) {
        lsdb.setLink(u, v, cost);
    }
}
