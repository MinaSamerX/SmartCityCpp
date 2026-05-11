#pragma once
// src/graph/BellmanFord.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <unordered_map>
#include <algorithm>
#include "RoadNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: BellmanFord
//
//  Shortest-path algorithm that handles negative edge weights.
//  Use-case in this system: toll discount roads (negative weight).
//
//  Complexity: O(V × E)
//
//  Also detects negative-weight cycles — critical for correctness.
//  A negative cycle means the "discount" creates an infinite loop —
//  reported back to the caller instead of silently looping.
// ─────────────────────────────────────────────────────────────────────────────
namespace BellmanFord {

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: BFResult
//  Extends PathResult with negative-cycle detection flag.
// ─────────────────────────────────────────────────────────────────────────────
struct BFResult : PathResult {
    bool hasNegativeCycle{false};
};

// ─────────────────────────────────────────────────────────────────────────────
//  shortestPath
// ─────────────────────────────────────────────────────────────────────────────
inline BFResult shortestPath(const RoadNetwork& graph,
                              int srcId, int destId)
{
    BFResult result;
    if (!graph.hasNode(srcId) || !graph.hasNode(destId)) return result;

    auto nodeIds = graph.getAllNodeIds();
    int  V       = static_cast<int>(nodeIds.size());

    // Map node IDs to contiguous indices for array access
    std::unordered_map<int,int> idx;
    for (int i = 0; i < V; ++i) idx[nodeIds[i]] = i;

    std::vector<double> dist(V, INF_WEIGHT);
    std::vector<int>    parent(V, -1);
    dist[idx[srcId]] = 0.0;

    // Collect all edges once (avoid re-iterating neighbours every pass)
    struct FlatEdge { int from, to; double w; };
    std::vector<FlatEdge> edges;
    for (int id : nodeIds)
        for (const auto& e : graph.getNeighbours(id))
            if (!e.isClosed)
                edges.push_back({id, e.to, e.weight});

    // ── V-1 relaxation passes ─────────────────────────────────────────────────
    for (int pass = 0; pass < V - 1; ++pass) {
        bool updated = false;
        for (const auto& e : edges) {
            int fi = idx[e.from], ti = idx[e.to];
            if (dist[fi] == INF_WEIGHT) continue;
            double nd = dist[fi] + e.w;
            if (nd < dist[ti]) {
                dist[ti]   = nd;
                parent[ti] = e.from;
                updated    = true;
            }
        }
        if (!updated) break; // early termination if stable
    }

    // ── Negative cycle detection (V-th pass) ─────────────────────────────────
    for (const auto& e : edges) {
        int fi = idx[e.from], ti = idx[e.to];
        if (dist[fi] != INF_WEIGHT && dist[fi] + e.w < dist[ti]) {
            result.hasNegativeCycle = true;
            return result;
        }
    }

    int di = idx[destId];
    if (dist[di] == INF_WEIGHT) return result; // unreachable

    // Reconstruct path
    result.reachable = true;
    result.totalCost = dist[di];

    for (int v = destId; v != -1; ) {
        result.path.push_back(v);
        int vi = idx[v];
        v = parent[vi];
        if (v == -1) break;
    }
    std::reverse(result.path.begin(), result.path.end());

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  hasNegativeCycle
//  Quick check — does graph contain any negative cycle from srcId?
// ─────────────────────────────────────────────────────────────────────────────
inline bool hasNegativeCycle(const RoadNetwork& graph, int srcId) {
    return BellmanFord::shortestPath(graph, srcId, srcId).hasNegativeCycle;
}

} // namespace BellmanFord