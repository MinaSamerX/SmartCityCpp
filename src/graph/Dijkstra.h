#pragma once
// src/graph/Dijkstra.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include "RoadNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: Dijkstra
//
//  Shortest-path algorithm on weighted directed graphs.
//  Skips closed roads — uses only open edges.
//
//  Complexity: O((V + E) log V)  using a min-heap priority queue
//
//  Modern C++ notes:
//    - Min-heap via std::priority_queue with a lambda comparator
//    - Path reconstruction using parent map (unordered_map)
//    - Returns PathResult (defined in RoadNetwork.h)
// ─────────────────────────────────────────────────────────────────────────────
namespace Dijkstra {

// ─────────────────────────────────────────────────────────────────────────────
//  shortestPath
//  Returns PathResult with the optimal path and total cost from src to dest.
//  If dest is unreachable, result.reachable == false.
// ─────────────────────────────────────────────────────────────────────────────
inline PathResult shortestPath(const RoadNetwork& graph,
                                int srcId, int destId)
{
    PathResult result;
    if (!graph.hasNode(srcId) || !graph.hasNode(destId)) return result;

    // dist[node] = best known distance from src
    std::unordered_map<int, double> dist;
    std::unordered_map<int, int>    parent;

    for (int id : graph.getAllNodeIds()) {
        dist[id]   = INF_WEIGHT;
        parent[id] = -1;
    }
    dist[srcId] = 0.0;

    // Min-heap: (cost, nodeId)
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry,
                        std::vector<Entry>,
                        std::greater<Entry>> pq;
    pq.push({0.0, srcId});

    while (!pq.empty()) {
        auto [cost, curr] = pq.top();
        pq.pop();

        // Stale entry — skip
        if (cost > dist[curr]) continue;
        // Early exit once destination is settled
        if (curr == destId)    break;

        for (const auto& edge : graph.getNeighbours(curr)) {
            if (edge.isClosed) continue;

            double newDist = dist[curr] + edge.weight;
            if (newDist < dist[edge.to]) {
                dist[edge.to]   = newDist;
                parent[edge.to] = curr;
                pq.push({newDist, edge.to});
            }
        }
    }

    if (dist[destId] == INF_WEIGHT) return result; // unreachable

    // Reconstruct path
    result.reachable  = true;
    result.totalCost  = dist[destId];
    for (int v = destId; v != -1; v = parent[v])
        result.path.push_back(v);
    std::reverse(result.path.begin(), result.path.end());

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  allShortestPaths (Single-Source)
//  Returns dist map from src to every reachable node.
//  Used by analytics (top-K busiest intersections).
// ─────────────────────────────────────────────────────────────────────────────
inline std::unordered_map<int, double>
allShortestPaths(const RoadNetwork& graph, int srcId)
{
    std::unordered_map<int, double> dist;
    for (int id : graph.getAllNodeIds()) dist[id] = INF_WEIGHT;
    dist[srcId] = 0.0;

    using Entry = std::pair<double, int>;
    std::priority_queue<Entry,
                        std::vector<Entry>,
                        std::greater<Entry>> pq;
    pq.push({0.0, srcId});

    while (!pq.empty()) {
        auto [cost, curr] = pq.top(); pq.pop();
        if (cost > dist[curr]) continue;

        for (const auto& edge : graph.getNeighbours(curr)) {
            if (edge.isClosed) continue;
            double nd = dist[curr] + edge.weight;
            if (nd < dist[edge.to]) {
                dist[edge.to] = nd;
                pq.push({nd, edge.to});
            }
        }
    }
    return dist;
}

// ─────────────────────────────────────────────────────────────────────────────
//  shortestPathMultiStop
//  Visit a sequence of waypoints in order, chaining Dijkstra between each.
//  Returns total PathResult covering all stops.
// ─────────────────────────────────────────────────────────────────────────────
inline PathResult shortestPathMultiStop(const RoadNetwork&      graph,
                                         const std::vector<int>& stops)
{
    PathResult full;
    if (stops.size() < 2) return full;

    full.reachable  = true;
    full.totalCost  = 0.0;

    for (std::size_t i = 0; i + 1 < stops.size(); ++i) {
        auto leg = shortestPath(graph, stops[i], stops[i + 1]);
        if (!leg.reachable) { full.reachable = false; return full; }

        full.totalCost += leg.totalCost;

        // Append leg (avoid duplicating junction nodes)
        if (full.path.empty()) {
            full.path = leg.path;
        } else {
            full.path.insert(full.path.end(),
                             leg.path.begin() + 1,
                             leg.path.end());
        }
    }
    return full;
}

} // namespace Dijkstra