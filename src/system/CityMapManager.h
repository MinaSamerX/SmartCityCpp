#pragma once
// src/system/CityMapManager.h
// Smart City Delivery & Traffic Management System
//
// ── DESIGN PATTERN: Singleton ────────────────────────────────────────────────
// CityMapManager is the single source-of-truth for the city road network.
// Every component (EntityManager, SpatialQueryEngine, RouteOptimizer…)
// obtains the same instance via CityMapManager::getInstance().
//
// ── Responsibilities ──────────────────────────────────────────────────────────
//  - Load the city graph from file (delegates to FileParser)
//  - Expose the RoadNetwork for read-only algorithm queries
//  - Apply real-time traffic updates (edge weight changes, road closures)
//  - Find shortest paths using the plugged-in IPathFinder strategy
//  - Detect road closures and report affected nodes

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "../graph/RoadNetwork.h"
#include "../graph/BFS_DFS.h"
#include "../graph/MST.h"
#include "../graph/TopologicalSort.h"
#include "IPathFinder.h"

struct TrafficUpdate; // forward-declared; defined in FileParser.h

// ─────────────────────────────────────────────────────────────────────────────
//  Class: CityMapManager  (Singleton)
// ─────────────────────────────────────────────────────────────────────────────
class CityMapManager {
public:
    // ── Singleton access ──────────────────────────────────────────────────────
    static CityMapManager& getInstance() {
        static CityMapManager instance;  // guaranteed init once (C++11)
        return instance;
    }

    // Non-copyable, non-movable (Singleton contract)
    CityMapManager(const CityMapManager&)            = delete;
    CityMapManager& operator=(const CityMapManager&) = delete;
    CityMapManager(CityMapManager&&)                 = delete;
    CityMapManager& operator=(CityMapManager&&)      = delete;

    // ── Initialisation ────────────────────────────────────────────────────────

    // Load graph from city_map.txt; returns false on failure
    bool loadFromFile(const std::string& cityMapPath);

    // Manually add nodes/edges (useful for tests)
    bool addNode(int id, const std::string& name,
                 double x, double y, LocationType type = LocationType::Intersection);
    bool addEdge(int from, int to, double weight, int roadId = -1);
    bool addBidirectionalEdge(int from, int to, double weight, int roadId = -1);

    // ── Strategy: swap path-finding algorithm at runtime ──────────────────────
    void setPathFinder(std::unique_ptr<IPathFinder> finder) {
        m_pathFinder = std::move(finder);
    }
    const char* currentAlgorithm() const {
        return m_pathFinder ? m_pathFinder->name() : "none";
    }

    // ── Path queries ──────────────────────────────────────────────────────────
    PathResult shortestPath(int srcId, int destId) const;
    PathResult multiStopPath(const std::vector<int>& stops) const;

    // All shortest distances from srcId (Dijkstra SSSP)
    std::unordered_map<int, double> allDistancesFrom(int srcId) const;

    // ── Traffic management ────────────────────────────────────────────────────

    // Update edge weight (congestion / construction / toll change)
    bool updateTraffic(int fromId, int toId, double newWeight);

    // Apply a batch of traffic updates (from traffic_updates.txt)
    void applyTrafficUpdates(const std::vector<TrafficUpdate>& updates);

    // Close / re-open a road
    bool closeRoad(int fromId, int toId);
    bool openRoad(int fromId, int toId);

    // Returns nodes whose reachability is affected by closing fromId→toId
    std::vector<int> getAffectedNodes(int closedFrom, int closedTo) const;

    // ── Connectivity queries ──────────────────────────────────────────────────
    bool isReachable(int srcId, int destId) const;
    std::vector<std::vector<int>> connectedComponents() const;
    bool isFullyConnected() const;

    // ── MST (infrastructure planning) ────────────────────────────────────────
    MST::MSTResult buildMST(bool usePrim = false) const;

    // ── Topological ordering (one-way streets) ────────────────────────────────
    TopoSort::TopoResult topologicalOrder() const;
    std::vector<int> orderedCheckpoints(const std::vector<int>& stops) const;

    // ── Graph accessors (read-only) ───────────────────────────────────────────
    const RoadNetwork& getGraph()             const { return m_graph; }
    int                nodeCount()            const { return m_graph.nodeCount(); }
    int                edgeCount()            const { return m_graph.edgeCount(); }
    bool               hasNode(int id)        const { return m_graph.hasNode(id); }
    std::shared_ptr<Location> getLocation(int id) const {
        return m_graph.getLocation(id);
    }
    std::vector<int>   getAllNodeIds()         const { return m_graph.getAllNodeIds(); }

    // ── Diagnostics ───────────────────────────────────────────────────────────
    void printGraph(std::ostream& os = std::cout) const { m_graph.print(os); }
    void reset(); // clears graph + resets strategy to Dijkstra

private:
    CityMapManager();   // private — Singleton

    RoadNetwork                  m_graph;
    std::unique_ptr<IPathFinder> m_pathFinder;
    bool                         m_hasNegativeEdges{false};

    // Auto-switch strategy when a negative edge is detected
    void checkAndSwitchStrategy(double weight);
};