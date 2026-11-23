#include <catch2/catch_test_macros.hpp>
#include "LSDB.h"
#include "Dijkstra.h"
#include "DefaultTopology.h"

static LSDB threeRouters() {
    // 1-2 cost 5 (direct), 1-3 cost 10, 2-3 cost 10
    LSDB db;
    for (int i = 1; i <= 3; i++) db.update(LSA{i, {}, 1});
    db.setLink(1, 2, 5);
    db.setLink(1, 3, 10);
    db.setLink(2, 3, 10);
    return db;
}

TEST_CASE("stale LSA with lower sequence number is rejected", "[lsdb]") {
    LSDB db;
    db.update(LSA{1, {{2, 10}}, 5});
    db.update(LSA{1, {{2, 99}}, 2});
    REQUIRE(db.linkCost(1, 2) == 10);
}

TEST_CASE("fresher LSA overwrites older one", "[lsdb]") {
    LSDB db;
    db.update(LSA{1, {{2, 10}}, 1});
    db.update(LSA{1, {{2, 25}}, 2});
    REQUIRE(db.linkCost(1, 2) == 25);
}

TEST_CASE("setLink creates the link on both routers", "[lsdb]") {
    LSDB db = threeRouters();
    REQUIRE(db.linkCost(1, 2) == 5);
    REQUIRE(db.linkCost(2, 1) == 5);
    REQUIRE(db.neighbours(2).size() == 2);
}

TEST_CASE("removeLink removes both directions and bumps sequence numbers", "[lsdb]") {
    LSDB db = threeRouters();
    int seq1 = db.all().at(1).seqNum, seq2 = db.all().at(2).seqNum;

    REQUIRE(db.removeLink(1, 2));
    REQUIRE_FALSE(db.linkCost(1, 2));
    REQUIRE_FALSE(db.linkCost(2, 1));
    REQUIRE(db.all().at(1).seqNum == seq1 + 1);
    REQUIRE(db.all().at(2).seqNum == seq2 + 1);
}

TEST_CASE("removing a link that doesn't exist changes nothing", "[lsdb]") {
    LSDB db = threeRouters();
    REQUIRE(db.removeLink(1, 2));
    int seq1 = db.all().at(1).seqNum;

    REQUIRE_FALSE(db.removeLink(1, 2));           // already gone
    REQUIRE(db.all().at(1).seqNum == seq1);       // no re-origination
}

TEST_CASE("setLink is idempotent and updates cost without duplicating", "[lsdb]") {
    LSDB db = threeRouters();
    REQUIRE_FALSE(db.setLink(1, 2, 5));           // same cost: no change
    REQUIRE(db.setLink(1, 2, 7));                 // new cost: updated
    REQUIRE(db.linkCost(1, 2) == 7);
    REQUIRE(db.neighbours(1).size() == 2);        // still 2 neighbours, no duplicate
}

TEST_CASE("reconvergence reroutes around a failed link", "[lsdb]") {
    LSDB db = threeRouters();
    REQUIRE(dijkstra(db, 1).dist[2] == 5);        // direct

    db.removeLink(1, 2);
    auto after = dijkstra(db, 1);
    REQUIRE(after.dist[2] == 20);                 // via router 3
    REQUIRE(reconstructPath(after.parent, 1, 2) == std::vector<int>{1, 3, 2});
}

TEST_CASE("default topology reloads cleanly after failures", "[lsdb]") {
    LSDB db;
    loadDefaultTopology(db);
    REQUIRE(db.removeLink(8, 17));
    REQUIRE(db.removeLink(17, 18));               // router 17 now isolated

    loadDefaultTopology(db);                      // must not be rejected as stale
    REQUIRE(db.routerCount() == 20);
    REQUIRE(db.linkCost(8, 17) == 15);
    REQUIRE(dijkstra(db, 1).dist[17] == 29);
}
