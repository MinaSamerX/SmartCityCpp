#pragma once
// src/graph/MST.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <algorithm>
#include <unordered_map>
#include <queue>
#include "RoadNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: MST
//
//  Minimum Spanning Tree algorithms for infrastructure planning.
//
//  Use-cases:
//    - Find cheapest set of roads connecting all city locations
//    - Identify redundant roads (edges not in MST are redundant)
//    - Estimate minimum infrastructure cost for new districts
//
//  Provides both Kruskal (edge-sorted, Union-Find) and Prim (node-greedy).
// ─────────────────────────────────────────────────────────────────────────────
namespace MST {

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: MSTEdge — an edge in the result MST
// ─────────────────────────────────────────────────────────────────────────────
struct MSTEdge {
    int    from, to;
    double weight;
    bool operator<(const MSTEdge& o) const { return weight < o.weight; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: MSTResult
// ─────────────────────────────────────────────────────────────────────────────
struct MSTResult {
    std::vector<MSTEdge> edges;
    double               totalWeight{0.0};
    bool                 isComplete{false}; // true if spans all nodes

    void print(std::ostream& os) const {
        os << "MST [edges=" << edges.size()
           << " totalWeight=" << totalWeight
           << (isComplete ? " COMPLETE" : " PARTIAL") << "]\n";
        for (const auto& e : edges)
            os << "  " << e.from << " -> " << e.to
               << "  w=" << e.weight << "\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Union-Find (Disjoint Set Union) — internal helper for Kruskal
// ─────────────────────────────────────────────────────────────────────────────
class UnionFind {
public:
    explicit UnionFind(const std::vector<int>& nodes) {
        for (int n : nodes) { parent[n] = n; rank[n] = 0; }
    }

    // Path-compressed find
    int find(int x) {
        if (parent[x] != x)
            parent[x] = find(parent[x]); // path compression
        return parent[x];
    }

    // Union by rank — returns false if already in same set (cycle)
    bool unite(int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra == rb) return false;
        if (rank[ra] < rank[rb]) std::swap(ra, rb);
        parent[rb] = ra;
        if (rank[ra] == rank[rb]) ++rank[ra];
        return true;
    }

    bool connected(int a, int b) { return find(a) == find(b); }

private:
    std::unordered_map<int, int> parent;
    std::unordered_map<int, int> rank;
};

// ─────────────────────────────────────────────────────────────────────────────
//  kruskal  —  O(E log E)
//  Sort all edges by weight, greedily add if it doesn't form a cycle.
// ─────────────────────────────────────────────────────────────────────────────
inline MSTResult kruskal(const RoadNetwork& graph) {
    MSTResult result;
    auto nodeIds = graph.getAllNodeIds();
    if (nodeIds.empty()) return result;

    // Collect all undirected edges (treat directed graph as undirected for MST)
    std::vector<MSTEdge> allEdges;
    for (int from : nodeIds) {
        for (const auto& e : graph.getNeighbours(from)) {
            if (!e.isClosed)
                allEdges.push_back({from, e.to, e.weight});
        }
    }

    // Sort by weight ascending
    std::sort(allEdges.begin(), allEdges.end());

    UnionFind uf(nodeIds);
    int needed = static_cast<int>(nodeIds.size()) - 1;

    for (const auto& e : allEdges) {
        if (result.edges.size() == static_cast<size_t>(needed)) break;
        if (uf.unite(e.from, e.to)) {
            result.edges.push_back(e);
            result.totalWeight += e.weight;
        }
    }

    result.isComplete =
        (static_cast<int>(result.edges.size()) == needed);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  prim  —  O((V + E) log V)
//  Grow MST from startNode using a min-heap of crossing edges.
// ─────────────────────────────────────────────────────────────────────────────
inline MSTResult prim(const RoadNetwork& graph, int startNode = -1) {
    MSTResult result;
    auto nodeIds = graph.getAllNodeIds();
    if (nodeIds.empty()) return result;

    if (startNode == -1) startNode = nodeIds.front();
    if (!graph.hasNode(startNode)) return result;

    std::unordered_map<int, bool> inMST;
    for (int id : nodeIds) inMST[id] = false;

    // Min-heap: (weight, from, to)
    using Entry = std::tuple<double, int, int>;
    std::priority_queue<Entry,
                        std::vector<Entry>,
                        std::greater<Entry>> pq;

    auto addEdges = [&](int node) {
        for (const auto& e : graph.getNeighbours(node)) {
            if (!e.isClosed && !inMST[e.to])
                pq.push({e.weight, node, e.to});
        }
    };

    inMST[startNode] = true;
    addEdges(startNode);

    while (!pq.empty()) {
        auto [w, from, to] = pq.top(); pq.pop();
        if (inMST[to]) continue;

        inMST[to] = true;
        result.edges.push_back({from, to, w});
        result.totalWeight += w;
        addEdges(to);
    }

    int needed = static_cast<int>(nodeIds.size()) - 1;
    result.isComplete =
        (static_cast<int>(result.edges.size()) == needed);
    return result;
}

} // namespace MST