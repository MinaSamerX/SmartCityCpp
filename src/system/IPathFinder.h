#pragma once
// src/system/IPathFinder.h
// Smart City Delivery & Traffic Management System
//
// ── DESIGN PATTERN: Strategy ─────────────────────────────────────────────────
// IPathFinder is the abstract strategy interface.
// CityMapManager holds a unique_ptr<IPathFinder> and swaps it at runtime:
//   - DijkstraFinder    → default, all non-negative weights  O(E log V)
//   - BellmanFordFinder → when toll-discount (negative) edges exist O(VE)
// Callers never care which algorithm runs — they only call findPath().

#include <vector>
#include <memory>
#include "../graph/RoadNetwork.h"
#include "../graph/Dijkstra.h"
#include "../graph/BellmanFord.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Abstract Strategy
// ─────────────────────────────────────────────────────────────────────────────
class IPathFinder {
public:
    virtual ~IPathFinder() = default;

    // Single src → dest shortest path
    virtual PathResult findPath(const RoadNetwork& graph,
                                int srcId,
                                int destId) const = 0;

    // Chained waypoints: stop[0] → stop[1] → … → stop[N-1]
    virtual PathResult findMultiStopPath(const RoadNetwork&      graph,
                                         const std::vector<int>& stops) const = 0;

    virtual const char* name() const = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Concrete Strategy A — Dijkstra (non-negative weights, default)
// ─────────────────────────────────────────────────────────────────────────────
class DijkstraFinder final : public IPathFinder {
public:
    PathResult findPath(const RoadNetwork& graph,
                        int srcId, int destId) const override {
        return Dijkstra::shortestPath(graph, srcId, destId);
    }

    PathResult findMultiStopPath(const RoadNetwork&      graph,
                                  const std::vector<int>& stops) const override {
        return Dijkstra::shortestPathMultiStop(graph, stops);
    }

    const char* name() const override { return "Dijkstra"; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Concrete Strategy B — Bellman-Ford (supports negative/toll discount edges)
// ─────────────────────────────────────────────────────────────────────────────
class BellmanFordFinder final : public IPathFinder {
public:
    PathResult findPath(const RoadNetwork& graph,
                        int srcId, int destId) const override {
        auto res = BellmanFord::shortestPath(graph, srcId, destId);
        if (res.hasNegativeCycle) return {};        // guard: empty = error
        return static_cast<PathResult>(res);
    }

    PathResult findMultiStopPath(const RoadNetwork&      graph,
                                  const std::vector<int>& stops) const override {
        PathResult full;
        if (stops.size() < 2) return full;
        full.reachable = true;
        for (std::size_t i = 0; i + 1 < stops.size(); ++i) {
            auto leg = findPath(graph, stops[i], stops[i + 1]);
            if (!leg.reachable) { full.reachable = false; return full; }
            full.totalCost += leg.totalCost;
            if (full.path.empty()) full.path = leg.path;
            else full.path.insert(full.path.end(),
                                  leg.path.begin() + 1, leg.path.end());
        }
        return full;
    }

    const char* name() const override { return "BellmanFord"; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Factory helper — CityMapManager uses this to pick the right strategy
// ─────────────────────────────────────────────────────────────────────────────
inline std::unique_ptr<IPathFinder> makePathFinder(bool hasNegativeWeights = false) {
    if (hasNegativeWeights) return std::make_unique<BellmanFordFinder>();
    return std::make_unique<DijkstraFinder>();
}