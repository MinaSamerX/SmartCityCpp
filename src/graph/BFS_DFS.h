#pragma once
// src/graph/BFS_DFS.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include "RoadNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: GraphSearch
//
//  Stateless free-functions for BFS and DFS on a RoadNetwork.
//  All functions take const RoadNetwork& — they never mutate the graph.
//
//  Modern C++11 notes:
//    - Lambdas used for optional per-node visitor callbacks
//    - Structured bindings (C++17) used internally for clarity
//    - Returns by value (NRVO — zero-cost in practice)
// ─────────────────────────────────────────────────────────────────────────────
namespace GraphSearch {

// ─────────────────────────────────────────────────────────────────────────────
//  BFS — Breadth-First Search
//
//  Explores nodes level by level from startId.
//  Skips closed roads.
//
//  Parameters:
//    graph     — city road network
//    startId   — source node
//    visitor   — optional lambda called on each discovered node
//                signature: void(int nodeId)
//
//  Returns: vector of node IDs in BFS discovery order
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<int> bfs(
    const RoadNetwork& graph,
    int startId,
    std::function<void(int)> visitor = nullptr)
{
    std::vector<int>        visited;
    std::unordered_set<int> seen;
    std::queue<int>         q;

    if (!graph.hasNode(startId)) return visited;

    q.push(startId);
    seen.insert(startId);

    while (!q.empty()) {
        int curr = q.front();
        q.pop();

        visited.push_back(curr);
        if (visitor) visitor(curr);

        for (const auto& edge : graph.getNeighbours(curr)) {
            if (edge.isClosed)              continue; // respect road closures
            if (seen.count(edge.to))        continue; // already visited
            seen.insert(edge.to);
            q.push(edge.to);
        }
    }
    return visited;
}

// ─────────────────────────────────────────────────────────────────────────────
//  DFS — Depth-First Search (iterative, avoids stack overflow on large graphs)
//
//  Parameters / returns: same contract as bfs()
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<int> dfs(
    const RoadNetwork& graph,
    int startId,
    std::function<void(int)> visitor = nullptr)
{
    std::vector<int>        visited;
    std::unordered_set<int> seen;
    std::stack<int>         st;

    if (!graph.hasNode(startId)) return visited;

    st.push(startId);

    while (!st.empty()) {
        int curr = st.top();
        st.pop();

        if (seen.count(curr)) continue;
        seen.insert(curr);

        visited.push_back(curr);
        if (visitor) visitor(curr);

        // Push neighbours in reverse so left-to-right order is preserved
        const auto& nbrs = graph.getNeighbours(curr);
        for (int i = static_cast<int>(nbrs.size()) - 1; i >= 0; --i) {
            if (!nbrs[i].isClosed && !seen.count(nbrs[i].to))
                st.push(nbrs[i].to);
        }
    }
    return visited;
}

// ─────────────────────────────────────────────────────────────────────────────
//  isReachable
//  Returns true if dest is reachable from src via open roads.
//  Uses BFS (stops early on find — more cache-friendly than DFS for this use).
// ─────────────────────────────────────────────────────────────────────────────
inline bool isReachable(const RoadNetwork& graph, int srcId, int destId) {
    if (srcId == destId)         return graph.hasNode(srcId);
    if (!graph.hasNode(srcId))   return false;
    if (!graph.hasNode(destId))  return false;

    std::unordered_set<int> seen;
    std::queue<int>         q;

    q.push(srcId);
    seen.insert(srcId);

    while (!q.empty()) {
        int curr = q.front();
        q.pop();
        if (curr == destId) return true;

        for (const auto& edge : graph.getNeighbours(curr)) {
            if (!edge.isClosed && !seen.count(edge.to)) {
                seen.insert(edge.to);
                q.push(edge.to);
            }
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  connectedComponent
//  Returns all node IDs reachable from startId (BFS-based).
//  Useful for detecting isolated subgraphs after road closures.
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<int> connectedComponent(const RoadNetwork& graph,
                                           int startId) {
    return bfs(graph, startId);
}

// ─────────────────────────────────────────────────────────────────────────────
//  allConnectedComponents
//  Partitions all nodes into weakly-connected groups.
//  Returns a vector of groups (each group is a vector of nodeIds).
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<std::vector<int>> allConnectedComponents(
    const RoadNetwork& graph)
{
    std::unordered_set<int>          visited;
    std::vector<std::vector<int>>    components;

    for (int nodeId : graph.getAllNodeIds()) {
        if (visited.count(nodeId)) continue;

        auto comp = bfs(graph, nodeId);
        for (int id : comp) visited.insert(id);
        components.push_back(std::move(comp));
    }
    return components;
}

// ─────────────────────────────────────────────────────────────────────────────
//  findAlternatePaths
//  Given a closed road (from→to), finds all nodes still reachable from src.
//  Returns nodes that lost reachability after the closure vs before.
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<int> findAffectedNodes(const RoadNetwork& graph,
                                          int closedFrom, int closedTo) {
    // All currently reachable from closedFrom (closed roads are skipped by BFS)
    auto reachableNow  = bfs(graph, closedFrom);

    // What was reachable before (simulate: temporarily re-open the edge)
    // We do this by using a raw BFS that ignores the isClosed flag for that one edge
    // Simple approach: BFS on graph but treat closedFrom->closedTo as open
    std::vector<int>        affectedNodes;
    std::unordered_set<int> nowSet(reachableNow.begin(), reachableNow.end());

    // BFS ignoring closure for this specific edge
    std::unordered_set<int> seen;
    std::queue<int>         q;
    q.push(closedFrom);
    seen.insert(closedFrom);

    while (!q.empty()) {
        int curr = q.front();
        q.pop();
        for (const auto& edge : graph.getNeighbours(curr)) {
            bool isTheClosedEdge = (curr == closedFrom && edge.to == closedTo);
            if ((edge.isClosed && !isTheClosedEdge) || seen.count(edge.to))
                continue;
            seen.insert(edge.to);
            q.push(edge.to);
        }
    }

    // Nodes that were reachable before but not now
    for (int id : seen) {
        if (!nowSet.count(id))
            affectedNodes.push_back(id);
    }
    return affectedNodes;
}

} // namespace GraphSearch