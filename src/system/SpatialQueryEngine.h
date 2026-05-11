#pragma once
// src/system/SpatialQueryEngine.h
// Smart City Delivery & Traffic Management System
//
// ── Responsibilities ──────────────────────────────────────────────────────────
// SpatialQueryEngine wraps both QuadTree and KDTree behind one clean API.
// All spatial operations in the system go through here:
//   - Find nearest available vehicle to a pickup point
//   - Find all delivery locations within a radius
//   - Range queries for zone partitioning
//   - K-nearest neighbours for batching deliveries
//
// The engine rebuilds its indexes lazily when the location set changes.
// It holds shared_ptr<Location> so it shares ownership with EntityManager.

#include <memory>
#include <vector>
#include <functional>
#include <ostream>
#include "../core/Location.h"
#include "../core/Vehicle.h"
#include "../spatial/QuadTree.h"
#include "../spatial/KDTree.h"
#include <iostream>

class SpatialQueryEngine {
public:
    // ── Construction ─────────────────────────────────────────────────────────
    SpatialQueryEngine()  = default;
    ~SpatialQueryEngine() = default;

    // Non-copyable (owns tree structures)
    SpatialQueryEngine(const SpatialQueryEngine&)            = delete;
    SpatialQueryEngine& operator=(const SpatialQueryEngine&) = delete;
    SpatialQueryEngine(SpatialQueryEngine&&)                 = default;
    SpatialQueryEngine& operator=(SpatialQueryEngine&&)      = default;

    // ── Index management ──────────────────────────────────────────────────────

    // Build / rebuild both indexes from a location list
    void build(const std::vector<std::shared_ptr<Location>>& locations);

    // Add a single location to both indexes
    void addLocation(std::shared_ptr<Location> loc);

    bool isBuilt() const { return m_built; }
    int  size()    const { return m_kdTree.size(); }

    // ── Nearest-neighbor queries ───────────────────────────────────────────────

    // Single nearest location to (x, y)
    std::shared_ptr<Location> nearestLocation(double x, double y) const;

    // Nearest location of a specific type (e.g. ChargingStation)
    std::shared_ptr<Location> nearestOfType(double x, double y,
                                             LocationType type) const;

    // Nearest AVAILABLE vehicle to pickup point (x, y)
    // getCoords: lambda mapping locationId → (x, y)  [injected dependency]
    std::shared_ptr<Vehicle> nearestAvailableVehicle(
        double x, double y,
        const std::vector<std::shared_ptr<Vehicle>>& vehicles,
        std::function<std::pair<double,double>(int locationId)> getCoords) const;

    // ── Radius queries ────────────────────────────────────────────────────────

    // All locations within radius r of (x, y)
    std::vector<std::shared_ptr<Location>>
    locationsInRadius(double x, double y, double radius) const;

    // All locations within radius filtered by type
    std::vector<std::shared_ptr<Location>>
    locationsInRadiusOfType(double x, double y, double radius,
                             LocationType type) const;

    // ── K-nearest queries ─────────────────────────────────────────────────────

    // K nearest locations to (x, y) — sorted closest first
    std::vector<std::shared_ptr<Location>>
    kNearest(double x, double y, int k) const;

    // ── Range / bounding-box queries ──────────────────────────────────────────
    std::vector<std::shared_ptr<Location>>
    locationsInBox(double minX, double minY,
                   double maxX, double maxY) const;

    // ── Diagnostics ───────────────────────────────────────────────────────────
    void print(std::ostream& os = std::cout) const;

private:
    QuadTree m_quadTree{BoundingBox{0,0,1,1}};  // rebuilt on build()
    KDTree   m_kdTree;
    bool     m_built{false};

    // Stored locations for addLocation incremental inserts
    std::vector<std::shared_ptr<Location>> m_locations;
};