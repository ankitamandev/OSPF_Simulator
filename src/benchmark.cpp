// Benchmarks reconvergence latency: how long it takes the routing
// engine to rebuild its graph and re-run Dijkstra after a link
// failure. Run this and use the printed avg/p99 numbers as your
// resume metric — never invent a number, always measure it here.
//
// Usage: ./build/benchmark [num_nodes] [num_trials]
//   e.g. ./build/benchmark 50 1000

#include "LSDB.h"
#include "Dijkstra.h"
#include <iostream>
#include <random>
#include <algorithm>
#include <vector>

// Builds a random connected topology with `numNodes` routers.
// Each node connects to 2-4 random earlier nodes (keeps it connected
// and sparse, like a real network) plus some random extra edges.
static LSDB buildRandomTopology(int numNodes, std::mt19937& rng) {
    LSDB lsdb;
    std::uniform_int_distribution<int> costDist(1, 100);

    for (int i = 1; i <= numNodes; i++) {
        lsdb.update(LSA{i, {}, 1});
    }

    // Connect each node to 1-2 earlier nodes to guarantee connectivity.
    for (int i = 2; i <= numNodes; i++) {
        std::uniform_int_distribution<int> earlierDist(1, i - 1);
        int numLinks = std::min(i - 1, 1 + (int)(rng() % 2));
        for (int k = 0; k < numLinks; k++) {
            int j = earlierDist(rng);
            int cost = costDist(rng);

            LSA a = lsdb.all().at(i);
            a.neighbours.push_back({j, cost});
            a.seqNum++;
            lsdb.update(a);

            LSA b = lsdb.all().at(j);
            b.neighbours.push_back({i, cost});
            b.seqNum++;
            lsdb.update(b);
        }
    }

    // Add extra random edges so average degree is ~2.5-4 (realistic sparsity).
    int extraEdges = numNodes / 2;
    std::uniform_int_distribution<int> nodeDist(1, numNodes);
    for (int k = 0; k < extraEdges; k++) {
        int u = nodeDist(rng);
        int v = nodeDist(rng);
        if (u == v) continue;
        int cost = costDist(rng);

        LSA a = lsdb.all().at(u);
        a.neighbours.push_back({v, cost});
        a.seqNum++;
        lsdb.update(a);

        LSA b = lsdb.all().at(v);
        b.neighbours.push_back({u, cost});
        b.seqNum++;
        lsdb.update(b);
    }

    return lsdb;
}

int main(int argc, char* argv[]) {
    int numNodes = argc > 1 ? std::stoi(argv[1]) : 50;
    int numTrials = argc > 2 ? std::stoi(argv[2]) : 1000;

    std::mt19937 rng(42); // fixed seed for reproducibility
    LSDB lsdb = buildRandomTopology(numNodes, rng);

    Graph initial = lsdb.buildGraph();
    std::cout << "Topology: " << numNodes << " nodes, "
              << "approx " << (numNodes * 2) << " directed edge entries\n";
    std::cout << "Running " << numTrials << " random link-failure trials...\n\n";

    std::uniform_int_distribution<int> nodeDist(1, numNodes);
    std::vector<long long> times;
    times.reserve(numTrials);

    int src = 1;
    int successfulTrials = 0;

    for (int t = 0; t < numTrials; t++) {
        int u = nodeDist(rng);
        int v = nodeDist(rng);
        if (u == v) continue;

        long long us = lsdb.failLink(u, v, src);
        times.push_back(us);

        // Restore so the topology doesn't degrade across trials.
        std::uniform_int_distribution<int> costDist(1, 100);
        lsdb.restoreLink(u, v, costDist(rng), src);

        successfulTrials++;
    }

    if (times.empty()) {
        std::cerr << "No successful trials — try a larger topology.\n";
        return 1;
    }

    std::sort(times.begin(), times.end());
    long long sum = 0;
    for (auto t : times) sum += t;
    long long avg = sum / (long long)times.size();
    long long p50 = times[times.size() * 50 / 100];
    long long p99 = times[std::min(times.size() - 1, times.size() * 99 / 100)];
    long long minV = times.front();
    long long maxV = times.back();

    std::cout << "Results over " << successfulTrials << " trials:\n";
    std::cout << "  min : " << minV << " us\n";
    std::cout << "  avg : " << avg  << " us\n";
    std::cout << "  p50 : " << p50  << " us\n";
    std::cout << "  p99 : " << p99  << " us\n";
    std::cout << "  max : " << maxV << " us\n\n";

    return 0;
}