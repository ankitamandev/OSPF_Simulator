#include <catch2/catch_test_macros.hpp>
#include "LSDB.h"
#include "Dijkstra.h"

TEST_CASE("stale LSA with lower sequence number is rejected", "[lsdb]") {
    LSDB db;
    LSA fresh{1, {{2, 10}}, 5};
    LSA stale{1, {{2, 99}}, 2}; // lower seqNum — should be ignored

    db.update(fresh);
    db.update(stale);

    Graph g = db.buildGraph();
    REQUIRE(g.neighbours(1)[0].cost == 10); // fresh value retained
}

TEST_CASE("fresher LSA overwrites older one", "[lsdb]") {
    LSDB db;
    LSA v1{1, {{2, 10}}, 1};
    LSA v2{1, {{2, 25}}, 2}; // higher seqNum — should win

    db.update(v1);
    db.update(v2);

    Graph g = db.buildGraph();
    REQUIRE(g.neighbours(1)[0].cost == 25);
}

TEST_CASE("buildGraph reconstructs correct adjacency from LSAs", "[lsdb]") {
    LSDB db;
    db.update(LSA{1, {{2, 10}}, 1});
    db.update(LSA{2, {{1, 10}, {3, 20}}, 1});
    db.update(LSA{3, {{2, 20}}, 1});

    Graph g = db.buildGraph();

    REQUIRE(g.nodeCount() == 3);
    REQUIRE(g.neighbours(2).size() == 2);
}

TEST_CASE("failLink removes the edge from the rebuilt graph", "[lsdb]") {
    LSDB db;
    db.update(LSA{1, {{2, 10}}, 1});
    db.update(LSA{2, {{1, 10}, {3, 20}}, 1});
    db.update(LSA{3, {{2, 20}}, 1});

    db.failLink(1, 2, 3);

    Graph g = db.buildGraph();
    REQUIRE(g.neighbours(1).empty());
    REQUIRE(g.neighbours(2).size() == 1);
    REQUIRE(g.neighbours(2)[0].to == 3);
}

TEST_CASE("failLink returns a measurable, non-negative reconvergence time", "[lsdb]") {
    LSDB db;
    db.update(LSA{1, {{2, 10}}, 1});
    db.update(LSA{2, {{1, 10}}, 1});

    long long us = db.failLink(1, 2, 1);

    REQUIRE(us >= 0);
}

TEST_CASE("restoreLink adds the edge back", "[lsdb]") {
    LSDB db;
    db.update(LSA{1, {{2, 10}}, 1});
    db.update(LSA{2, {{1, 10}}, 1});

    db.failLink(1, 2, 1);
    db.restoreLink(1, 2, 15, 1);

    Graph g = db.buildGraph();
    REQUIRE(g.neighbours(1).size() == 1);
    REQUIRE(g.neighbours(1)[0].cost == 15);
}

TEST_CASE("reconvergence updates shortest path after failure", "[lsdb]") {
    // 1-2 (cost 5) is the cheap direct path.
    // 1-3-2 (cost 10+10=20) is the fallback.
    LSDB db;
    db.update(LSA{1, {{2, 5}, {3, 10}}, 1});
    db.update(LSA{2, {{1, 5}, {3, 10}}, 1});
    db.update(LSA{3, {{1, 10}, {2, 10}}, 1});

    Graph before = db.buildGraph();
    auto [distBefore, _] = dijkstra(before, 1);
    REQUIRE(distBefore[2] == 5); // direct path

    db.failLink(1, 2, 1);

    Graph after = db.buildGraph();
    auto [distAfter, parentAfter] = dijkstra(after, 1);
    REQUIRE(distAfter[2] == 20); // re-routed via node 3
}