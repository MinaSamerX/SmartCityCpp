#pragma once
// src/graph/TopologicalSort.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <queue>
#include <unordered_map>
#include "RoadNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: TopoSort
//
//  Topological ordering of the city directed graph.
//
//  Purpose in this system:
//    - Validate one-way street networks (no circular dependencies)
//    - Order mandatory delivery checkpoints where sequencing is enforced
//    - Detect cycles — indicates contradictory one-way rules
//
//  Algorithm: Kahn's BFS-based topological sort  O(V + E)
//    1. Compute in-degree for every node
//    2. Enqueue all zero-in-degree nodes
//    3. Process queue: emit node, decrement neighbours' in-degrees
//    4. Emitted count < nodeCount  →  cycle detected
// ─────────────────────────────────────────────────────────────────────────────
namespace TopoSort {

struct TopoResult {
    std::vector<int> order;          // topological order (empty if cycle)
    bool             hasCycle{false};
    std::vector<int> cycleNodes;     // nodes still with nonzero in-degree
};

// ── topologicalSort ───────────────────────────────────────────────────────────
inline TopoResult topologicalSort(const RoadNetwork& graph) {
    TopoResult result;

    auto nodeIds = graph.getAllNodeIds();
    if (nodeIds.empty()) return result;

    // Step 1 — compute in-degrees
    std::unordered_map<int, int> inDegree;
    for (int id : nodeIds) inDegree[id] = 0;

    for (int from : nodeIds)
        for (const auto& edge : graph.getNeighbours(from))
            if (!edge.isClosed)
                inDegree[edge.to]++;

    // Step 2 — seed queue
    std::queue<int> q;
    for (const auto& [id, deg] : inDegree)
        if (deg == 0) q.push(id);

    // Step 3 — Kahn's loop
    while (!q.empty()) {
        int curr = q.front(); q.pop();
        result.order.push_back(curr);

        for (const auto& edge : graph.getNeighbours(curr)) {
            if (edge.isClosed) continue;
            if (--inDegree[edge.to] == 0)
                q.push(edge.to);
        }
    }

    // Step 4 — cycle detection
    if (static_cast<int>(result.order.size()) < static_cast<int>(nodeIds.size())) {
        result.hasCycle = true;
        result.order.clear();
        for (const auto& [id, deg] : inDegree)
            if (deg > 0) result.cycleNodes.push_back(id);
    }
    return result;
}

// ── isDAG ─────────────────────────────────────────────────────────────────────
inline bool isDAG(const RoadNetwork& graph) {
    return !topologicalSort(graph).hasCycle;
}

// ── getDeliveryOrder ──────────────────────────────────────────────────────────
// Filter full topo order to only the requested checkpoint IDs,
// preserving their relative topological ordering.
inline std::vector<int> getDeliveryOrder(const RoadNetwork& graph,
                                         const std::vector<int>& checkpoints) {
    auto result = topologicalSort(graph);
    if (result.hasCycle) return {};

    std::unordered_map<int, bool> wanted;
    for (int id : checkpoints) wanted[id] = true;

    std::vector<int> ordered;
    ordered.reserve(checkpoints.size());
    for (int id : result.order)
        if (wanted.count(id)) ordered.push_back(id);

    return ordered;
}

} // namespace TopoSort