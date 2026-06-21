#include <catch2/catch_test_macros.hpp>
#include "Graph.h"

TEST_CASE("addEdge creates bidirectional connectivity", "[graph]") {
    Graph g;
    g.addEdge(1, 2, 10);

    REQUIRE(g.neighbours(1).size() == 1);
    REQUIRE(g.neighbours(2).size() == 1);
    REQUIRE(g.neighbours(1)[0].to == 2);
    REQUIRE(g.neighbours(1)[0].cost == 10);
    REQUIRE(g.neighbours(2)[0].to == 1);
    REQUIRE(g.neighbours(2)[0].cost == 10);
}

TEST_CASE("removeEdge clears both directions", "[graph]") {
    Graph g;
    g.addEdge(1, 2, 5);
    g.removeEdge(1, 2);

    REQUIRE(g.neighbours(1).empty());
    REQUIRE(g.neighbours(2).empty());
}

TEST_CASE("removeEdge only removes the specified link", "[graph]") {
    Graph g;
    g.addEdge(1, 2, 5);
    g.addEdge(1, 3, 7);
    g.removeEdge(1, 2);

    REQUIRE(g.neighbours(1).size() == 1);
    REQUIRE(g.neighbours(1)[0].to == 3);
}

TEST_CASE("neighbours of unknown node returns empty list", "[graph]") {
    Graph g;
    g.addEdge(1, 2, 5);

    REQUIRE(g.neighbours(999).empty());
}

TEST_CASE("a node can have multiple neighbours", "[graph]") {
    Graph g;
    g.addEdge(1, 2, 10);
    g.addEdge(1, 3, 20);
    g.addEdge(1, 4, 30);

    REQUIRE(g.neighbours(1).size() == 3);
}

TEST_CASE("addNode creates an isolated node with no edges", "[graph]") {
    Graph g;
    g.addNode(5);

    REQUIRE(g.nodeCount() == 1);
    REQUIRE(g.neighbours(5).empty());
}

TEST_CASE("nodeCount reflects number of distinct routers", "[graph]") {
    Graph g;
    g.addEdge(1, 2, 5);
    g.addEdge(2, 3, 5);

    REQUIRE(g.nodeCount() == 3);
}