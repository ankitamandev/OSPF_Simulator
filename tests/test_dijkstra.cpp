#include <catch2/catch_test_macros.hpp>
#include "Dijkstra.h"
#include "Graph.h"
#include "LSDB.h"

TEST_CASE("shortest path on a simple 5-node graph", "[dijkstra]") {
    // R1 -10- R2 -15- R4 -5- R5
    // R1 -50- R3 -20- R4
    Graph g;
    g.addEdge(1, 2, 10);
    g.addEdge(1, 3, 50);
    g.addEdge(2, 4, 15);
    g.addEdge(3, 4, 20);
    g.addEdge(4, 5, 5);

    auto [dist, parent] = dijkstra(g, 1);

    REQUIRE(dist[5] == 30);   // 1 -> 2 -> 4 -> 5
    REQUIRE(dist[2] == 10);
    REQUIRE(dist[3] == 45);   // 1 -> 2 -> 4 -> 3 (10+15+20) beats the direct 50
    REQUIRE(dist[4] == 25);
}

TEST_CASE("path reconstruction gives correct hop sequence", "[dijkstra]") {
    Graph g;
    g.addEdge(1, 2, 10);
    g.addEdge(2, 4, 15);
    g.addEdge(4, 5, 5);
    g.addEdge(1, 3, 50);
    g.addEdge(3, 4, 20);

    auto [dist, parent] = dijkstra(g, 1);
    REQUIRE(reconstructPath(parent, 1, 5) == std::vector<int>{1, 2, 4, 5});
}

TEST_CASE("disconnected node is unreachable", "[dijkstra]") {
    Graph g;
    g.addEdge(1, 2, 5);
    g.addNode(99);

    auto [dist, parent] = dijkstra(g, 1);
    REQUIRE(dist[99] == UNREACHABLE);
    REQUIRE(parent[99] == NO_PARENT);
}

TEST_CASE("source node has distance zero to itself", "[dijkstra]") {
    Graph g;
    g.addEdge(1, 2, 5);

    auto [dist, parent] = dijkstra(g, 1);
    REQUIRE(dist[1] == 0);
    REQUIRE(parent[1] == NO_PARENT);
}

TEST_CASE("single-node graph with no edges", "[dijkstra]") {
    Graph g;
    g.addNode(1);

    auto [dist, parent] = dijkstra(g, 1);
    REQUIRE(dist[1] == 0);
    int reachable = 0;
    for (int d : dist) reachable += (d != UNREACHABLE);
    REQUIRE(reachable == 1);
}

TEST_CASE("reconstructPath returns single-element path when src equals dst", "[dijkstra]") {
    Graph g;
    g.addEdge(1, 2, 5);
    auto [dist, parent] = dijkstra(g, 1);
    REQUIRE(reconstructPath(parent, 1, 1) == std::vector<int>{1});
}

TEST_CASE("reconstructPath returns empty vector for unreachable destination", "[dijkstra]") {
    Graph g;
    g.addEdge(1, 2, 5);
    g.addNode(99);
    auto [dist, parent] = dijkstra(g, 1);
    REQUIRE(reconstructPath(parent, 1, 99).empty());
    REQUIRE(reconstructPath(parent, 1, 500).empty());   // out of range
}

TEST_CASE("Dijkstra picks the cheaper of two alternate paths", "[dijkstra]") {
    Graph g;
    g.addEdge(1, 2, 1);
    g.addEdge(2, 4, 2);
    g.addEdge(1, 3, 50);
    g.addEdge(3, 4, 50);

    auto [dist, parent] = dijkstra(g, 1);
    REQUIRE(dist[4] == 3);
    REQUIRE(reconstructPath(parent, 1, 4) == std::vector<int>{1, 2, 4});
}

TEST_CASE("SPF on the LSDB matches SPF on an equivalent Graph", "[dijkstra]") {
    Graph g;
    LSDB db;
    for (int i = 1; i <= 5; i++) db.update(LSA{i, {}, 1});
    const int edges[][3] = {{1,2,10}, {1,3,50}, {2,4,15}, {3,4,20}, {4,5,5}};
    for (auto& e : edges) {
        g.addEdge(e[0], e[1], e[2]);
        db.setLink(e[0], e[1], e[2]);
    }

    auto fromGraph = dijkstra(g, 1);
    auto fromLsdb  = dijkstra(db, 1);
    REQUIRE(fromGraph.dist == fromLsdb.dist);
}
