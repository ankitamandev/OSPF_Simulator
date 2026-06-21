#pragma once

#include "Graph.h"
#include <map>
#include <vector>
#include <utility>
#include <climits>

// Sentinel for "unreachable" — mirrors INT_MAX semantics used throughout.
constexpr int UNREACHABLE = INT_MAX;

// Runs Dijkstra's Shortest Path First (the same algorithm OSPF uses
// per RFC 2328 §16.1) from `src` to every other node in the graph.
//
// Returns a pair:
//   dist   — dist[v] = shortest cost from src to v (UNREACHABLE if no path)
//   parent — parent[v] = previous node on the shortest path to v
//
// Complexity: O((V+E) log V) using a binary min-heap.
std::pair<std::map<int, int>, std::map<int, int>>
dijkstra(const Graph& g, int src);

// Reconstructs the full hop-by-hop path from src to dst using the
// parent map returned by dijkstra(). Returns an empty vector if dst
// is unreachable from src.
std::vector<int> reconstructPath(const std::map<int, int>& parent,
                                  int src, int dst);