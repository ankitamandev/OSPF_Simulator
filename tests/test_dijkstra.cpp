#include <catch2/catch_test_macros.hpp>
#include "Dijkstra.h"

TEST_CASE("shortest path on a simple 5-node graph", "[dijkstra]") {
    // R1 -10- R2 -15- R4 -5- R5
    // R1 -50- R3 -20- R4
    // Shortest R1->R5 should go via R2 and R4: 10+15+5 = 30
    Graph g;
    g.addEdge(1, 2, 10);
    g.addEdge(1, 3, 50);
    g.addEdge(2, 4, 15);
    g.addEdge(3, 4, 20);
    g.addEdge(4, 5, 5);

    auto [dist, parent] = dijkstra(g, 1);

    REQUIRE(dist[5] == 30);
    REQUIRE(dist[2] == 10);
    REQUIRE(dist[3] == 45);
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
    auto path = reconstructPath(parent, 1, 5);

    REQUIRE(path == std::vector<int>{1, 2, 4, 5});
}

TEST_CASE("disconnected node is unreachable", "[dijkstra]") {
    Graph g;
    g.addEdge(1, 2, 5);
    g.addNode(99); // isolated, no edges

    auto [dist, parent] = dijkstra(g, 1);

    REQUIRE(dist[99] == UNREACHABLE);
}

TEST_CASE("source node has distance zero to itself", "[dijkstra]") {
    Graph g;
    g.addEdge(1, 2, 5);

    auto [dist, parent] = dijkstra(g, 1);

    REQUIRE(dist[1] == 0);
}

TEST_CASE("single-node graph with no edges", "[dijkstra]") {
    Graph g;
    g.addNode(1);

    auto [dist, parent] = dijkstra(g, 1);

    REQUIRE(dist[1] == 0);
    REQUIRE(dist.size() == 1);
}

TEST_CASE("reconstructPath returns single-element path when src equals dst", "[dijkstra]") {
    Graph g;
    g.addEdge(1, 2, 5);

    auto [dist, parent] = dijkstra(g, 1);
    auto path = reconstructPath(parent, 1, 1);

    REQUIRE(path == std::vector<int>{1});
}

TEST_CASE("reconstructPath returns empty vector for unreachable destination", "[dijkstra]") {
    Graph g;
    g.addEdge(1, 2, 5);
    g.addNode(99);

    auto [dist, parent] = dijkstra(g, 1);
    auto path = reconstructPath(parent, 1, 99);

    REQUIRE(path.empty());
}

TEST_CASE("Dijkstra picks the cheaper of two alternate paths", "[dijkstra]") {
    // Two paths from 1 to 4: via 2 (cost 3) or via 3 (cost 100)
    Graph g;
    g.addEdge(1, 2, 1);
    g.addEdge(2, 4, 2);
    g.addEdge(1, 3, 50);
    g.addEdge(3, 4, 50);

    auto [dist, parent] = dijkstra(g, 1);

    REQUIRE(dist[4] == 3);
    auto path = reconstructPath(parent, 1, 4);
    REQUIRE(path == std::vector<int>{1, 2, 4});
}