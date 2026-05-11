#pragma once
// src/graph/RoadNetwork.h
// Smart City Delivery & Traffic Management System

#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <limits>
#include <memory>
#include <ostream>
#include <iostream>
#include "../core/Location.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────────────────────────
constexpr double INF_WEIGHT = std::numeric_limits<double>::infinity();

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: Edge
//  Represents a directed road between two location-nodes.
//
//  Fields:
//    to       — destination node ID
//    weight   — base cost (distance / time)
//    roadId   — unique road identifier (used by traffic updates)
//    isClosed — dynamically toggled by TrafficMonitor
// ─────────────────────────────────────────────────────────────────────────────
struct Edge {
    int    to       {-1};
    double weight   {1.0};
    int    roadId   {-1};
    bool   isClosed {false};

    Edge() = default;
    Edge(int to, double weight, int roadId = -1)
        : to(to), weight(weight), roadId(roadId) {}

    bool operator==(const Edge& o) const {
        return to == o.to && roadId == o.roadId;
    }

    friend std::ostream& operator<<(std::ostream& os, const Edge& e) {
        return os << "->(" << e.to << " w=" << e.weight
                  << (e.isClosed ? " CLOSED" : "") << ")";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: PathResult
//  Returned by shortest-path algorithms.
// ─────────────────────────────────────────────────────────────────────────────
struct PathResult {
    std::vector<int> path;       // sequence of node IDs (src → dest)
    double           totalCost{INF_WEIGHT};
    bool             reachable{false};

    bool isValid() const { return reachable && !path.empty(); }

    friend std::ostream& operator<<(std::ostream& os, const PathResult& r) {
        if (!r.reachable) return os << "[PathResult: unreachable]";
        os << "[PathResult cost=" << r.totalCost << " path=";
        for (size_t i = 0; i < r.path.size(); ++i) {
            if (i) os << "->";
            os << r.path[i];
        }
        return os << "]";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Class: RoadNetwork
//
//  Weighted directed graph representing the city road map.
//
//  Responsibilities:
//    - Maintain adjacency list: nodeId → vector<Edge>
//    - Store Location objects indexed by node ID
//    - Provide graph mutation: addNode, addEdge, removeEdge, updateWeight
//    - Provide graph queries: neighbours, hasEdge, nodeCount, edgeCount
//    - Provide road closure toggling (used by TrafficMonitor)
//    - Serve as data source for all graph algorithms (Dijkstra, BFS, etc.)
//
//  OOP:
//    - Locations stored as shared_ptr — shared safely with EntityManager
//    - All mutation methods validate inputs (no silent corruption)
//    - Non-copyable (graph is a singleton-managed resource); movable
//    - Const-correct: all read-only queries are const
//
//  Algorithms (implemented in separate headers, take RoadNetwork const&):
//    Dijkstra, BellmanFord, BFS, DFS, MST, TopologicalSort
// ─────────────────────────────────────────────────────────────────────────────
class RoadNetwork {
public:
    // ── Construction ─────────────────────────────────────────────────────────
    RoadNetwork()  = default;
    ~RoadNetwork() = default;

    // Non-copyable — graph owns unique topology state
    RoadNetwork(const RoadNetwork&)            = delete;
    RoadNetwork& operator=(const RoadNetwork&) = delete;

    // Movable
    RoadNetwork(RoadNetwork&&)            = default;
    RoadNetwork& operator=(RoadNetwork&&) = default;

    // ── Node Operations ───────────────────────────────────────────────────────

    // Add a node using a Location object (shared ownership with EntityManager)
    bool addNode(std::shared_ptr<Location> loc);

    // Convenience: construct Location in-place
    bool addNode(int id, const std::string& name,
                 double x, double y,
                 LocationType type = LocationType::Intersection);

    // Remove a node and all edges incident to it
    bool removeNode(int nodeId);

    bool hasNode(int nodeId)  const;
    int  nodeCount()          const;

    // Returns nullptr if not found
    std::shared_ptr<Location> getLocation(int nodeId) const;

    // All node IDs
    std::vector<int> getAllNodeIds() const;

    // ── Edge Operations ───────────────────────────────────────────────────────

    // Add a directed edge from → to with given weight
    // roadId is optional; pass -1 to auto-assign
    bool addEdge(int from, int to, double weight, int roadId = -1);

    // Add bidirectional road (calls addEdge twice)
    bool addBidirectionalEdge(int from, int to, double weight, int roadId = -1);

    // Remove a directed edge
    bool removeEdge(int from, int to);

    // Update weight of existing edge (used by TrafficMonitor for congestion)
    bool updateEdgeWeight(int from, int to, double newWeight);

    // Toggle road closure — closed edges are skipped by algorithms
    bool setRoadClosed(int from, int to, bool closed);
    bool isRoadClosed(int from, int to) const;

    bool   hasEdge(int from, int to) const;
    double getEdgeWeight(int from, int to) const; // returns INF_WEIGHT if missing
    int    edgeCount() const;

    // ── Adjacency Access (used by algorithm headers) ──────────────────────────

    // Returns reference to neighbour list — algorithms iterate this
    const std::vector<Edge>& getNeighbours(int nodeId) const;

    // ── Utility ───────────────────────────────────────────────────────────────
    void clear();

    void print(std::ostream& os = std::cout) const;

    friend std::ostream& operator<<(std::ostream& os, const RoadNetwork& g) {
        g.print(os);
        return os;
    }

private:
    // node ID → outgoing edges
    std::unordered_map<int, std::vector<Edge>> m_adjList;

    // node ID → Location (shared with EntityManager)
    std::unordered_map<int, std::shared_ptr<Location>> m_locations;

    // monotonic road ID counter (used when caller passes -1)
    int m_nextRoadId{1};

    // empty sentinel returned for unknown nodes
    static const std::vector<Edge> s_emptyEdges;

    // internal helpers
    Edge*       findEdge(int from, int to);
    const Edge* findEdge(int from, int to) const;
};