#include <catch2/catch_test_macros.hpp>
#include "SpfEngine.h"
#include "DefaultTopology.h"
#include <random>
#include <string>

TEST_CASE("tree-edge failure matches full recompute", "[incremental]") {
    LSDB db;
    loadDefaultTopology(db);
    SpfEngine eng(db, 1);

    // 1-4 is on router 1's tree (path to 9 and 10 goes through it).
    REQUIRE(eng.tree().parent[4] == 1);
    db.removeLink(1, 4);
    eng.onLinkDown(1, 4);

    std::string why;
    REQUIRE(verifySpt(db, 1, eng.tree(), &why));
    REQUIRE(eng.lastTouched() > 0);
    REQUIRE(eng.lastTouched() < 20);              // did not recompute everything
}

TEST_CASE("non-tree edge failure does no work", "[incremental]") {
    LSDB db;
    loadDefaultTopology(db);
    SpfEngine eng(db, 1);

    // 17-18 (cost 5) is not on router 1's tree: both hang off router 8.
    REQUIRE(eng.tree().parent[17] != 18);
    REQUIRE(eng.tree().parent[18] != 17);
    db.removeLink(17, 18);
    eng.onLinkDown(17, 18);

    REQUIRE(eng.lastTouched() == 0);
    REQUIRE(verifySpt(db, 1, eng.tree()));
}

TEST_CASE("partition makes a subtree unreachable, and restore heals it", "[incremental]") {
    LSDB db;
    loadDefaultTopology(db);
    SpfEngine eng(db, 1);

    // Router 17 has exactly two links; cut both.
    db.removeLink(8, 17);  eng.onLinkDown(8, 17);
    db.removeLink(17, 18); eng.onLinkDown(17, 18);
    REQUIRE(eng.tree().dist[17] == UNREACHABLE);
    REQUIRE(verifySpt(db, 1, eng.tree()));

    db.setLink(8, 17, 15); eng.onLinkUp(8, 17, 15);
    REQUIRE(eng.tree().dist[17] == 29);
    REQUIRE(verifySpt(db, 1, eng.tree()));
}

TEST_CASE("cheaper new link reroutes via link-up", "[incremental]") {
    LSDB db;
    loadDefaultTopology(db);
    SpfEngine eng(db, 1);
    REQUIRE(eng.tree().dist[20] == 50);

    db.setLink(1, 20, 3);
    eng.onLinkUp(1, 20, 3);
    REQUIRE(eng.tree().dist[20] == 3);
    REQUIRE(verifySpt(db, 1, eng.tree()));
}

TEST_CASE("oracle rejects a wrong tree", "[incremental]") {
    LSDB db;
    loadDefaultTopology(db);
    SPT bad = dijkstra(db, 1);
    bad.dist[20] += 1;
    REQUIRE_FALSE(verifySpt(db, 1, bad));

    SPT badParent = dijkstra(db, 1);
    badParent.parent[20] = 2;                     // not even a link
    REQUIRE_FALSE(verifySpt(db, 1, badParent));
}

// The key test: thousands of random failures and restorations, on random
// topologies, with the incremental tree checked against a full recompute
// after every single change.
TEST_CASE("randomised: incremental always equals full recompute", "[incremental]") {
    for (unsigned seed = 1; seed <= 20; seed++) {
        std::mt19937 rng(seed);
        LSDB db;
        const int n = 30;
        for (int i = 1; i <= n; i++) db.update(LSA{i, {}, 1});
        std::uniform_int_distribution<int> cost(1, 20);     // small range => many ties
        for (int i = 2; i <= n; i++) {
            std::uniform_int_distribution<int> earlier(1, i - 1);
            db.setLink(i, earlier(rng), cost(rng));
        }
        std::uniform_int_distribution<int> any(1, n);
        for (int k = 0; k < n; k++) {
            int u = any(rng), v = any(rng);
            if (u != v) db.setLink(u, v, cost(rng));
        }

        SpfEngine eng(db, 1);
        for (int step = 0; step < 150; step++) {
            int u = any(rng), v = any(rng);
            if (u == v) continue;
            if (auto c = db.linkCost(u, v); c && (rng() % 2)) {
                db.removeLink(u, v);
                eng.onLinkDown(u, v);
            } else if (!c) {
                int w = cost(rng);
                db.setLink(u, v, w);
                eng.onLinkUp(u, v, w);
            } else {
                int w = cost(rng);                  // cost change on existing link
                db.setLink(u, v, w);
                if (w > *c) eng.onLinkDown(u, v);
                else        eng.onLinkUp(u, v, w);
            }
            std::string why;
            bool ok = verifySpt(db, 1, eng.tree(), &why);
            INFO("seed " << seed << " step " << step << ": " << why);
            REQUIRE(ok);
        }
    }
}
