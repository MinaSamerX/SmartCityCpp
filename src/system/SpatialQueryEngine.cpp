// src/system/SpatialQueryEngine.cpp
// Smart City Delivery & Traffic Management System

#include "SpatialQueryEngine.h"
#include <algorithm>
#include <limits>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
//  Index management
// ─────────────────────────────────────────────────────────────────────────────
void SpatialQueryEngine::build(
    const std::vector<std::shared_ptr<Location>>& locations)
{
    m_locations = locations;

    // Build QuadTree from scratch
    m_quadTree = QuadTree::build(locations);

    // Build balanced KDTree
    m_kdTree.build(locations);

    m_built = true;
}

void SpatialQueryEngine::addLocation(std::shared_ptr<Location> loc) {
    if (!loc) return;
    m_locations.push_back(loc);
    // Lazy rebuild: just rebuild both indexes from scratch
    // (acceptable for incremental additions during setup)
    build(m_locations);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Nearest-neighbor
// ─────────────────────────────────────────────────────────────────────────────
std::shared_ptr<Location>
SpatialQueryEngine::nearestLocation(double x, double y) const {
    if (!m_built) return nullptr;
    // KDTree gives guaranteed O(log N) balanced nearest-neighbor
    return m_kdTree.nearestNeighbor(x, y);
}

std::shared_ptr<Location>
SpatialQueryEngine::nearestOfType(double x, double y,
                                   LocationType type) const {
    if (!m_built) return nullptr;

    // Use KDTree radius search expanding outward, filter by type
    // Start with a small radius, double until we find one or exhaust locations
    double r = 10.0;
    constexpr double MAX_R = 1e9;

    while (r < MAX_R) {
        auto candidates = m_kdTree.queryRadius(x, y, r);
        std::shared_ptr<Location> best;
        double bestDist2 = std::numeric_limits<double>::max();

        for (const auto& loc : candidates) {
            if (loc->getType() != type) continue;
            double dx = loc->getX() - x, dy = loc->getY() - y;
            double d2 = dx*dx + dy*dy;
            if (d2 < bestDist2) { bestDist2 = d2; best = loc; }
        }
        if (best) return best;
        r *= 2.0;
    }
    return nullptr;
}

std::shared_ptr<Vehicle>
SpatialQueryEngine::nearestAvailableVehicle(
    double x, double y,
    const std::vector<std::shared_ptr<Vehicle>>& vehicles,
    std::function<std::pair<double,double>(int)> getCoords) const
{
    std::shared_ptr<Vehicle> best;
    double bestDist2 = std::numeric_limits<double>::max();

    for (const auto& v : vehicles) {
        if (!v->isAvailable()) continue;
        auto [vx, vy] = getCoords(v->getCurrentLocationId());
        double dx = vx - x, dy = vy - y;
        double d2 = dx*dx + dy*dy;
        if (d2 < bestDist2) { bestDist2 = d2; best = v; }
    }
    return best;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Radius queries
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::shared_ptr<Location>>
SpatialQueryEngine::locationsInRadius(double x, double y, double radius) const {
    if (!m_built) return {};
    // Use QuadTree for radius — intersectsCircle pruning is very effective
    return m_quadTree.queryRadius(x, y, radius);
}

std::vector<std::shared_ptr<Location>>
SpatialQueryEngine::locationsInRadiusOfType(double x, double y,
                                             double radius,
                                             LocationType type) const {
    auto all = locationsInRadius(x, y, radius);
    std::vector<std::shared_ptr<Location>> result;
    for (auto& loc : all)
        if (loc->getType() == type) result.push_back(loc);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  K-nearest
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::shared_ptr<Location>>
SpatialQueryEngine::kNearest(double x, double y, int k) const {
    if (!m_built) return {};
    return m_kdTree.kNearest(x, y, k);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Range / bounding box
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::shared_ptr<Location>>
SpatialQueryEngine::locationsInBox(double minX, double minY,
                                    double maxX, double maxY) const {
    if (!m_built) return {};
    double cx = (minX + maxX) / 2.0;
    double cy = (minY + maxY) / 2.0;
    double hw = (maxX - minX) / 2.0;
    double hh = (maxY - minY) / 2.0;
    return m_quadTree.queryRange(BoundingBox{cx, cy, hw, hh});
}

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostics
// ─────────────────────────────────────────────────────────────────────────────
void SpatialQueryEngine::print(std::ostream& os) const {
    os << "SpatialQueryEngine[built=" << m_built
       << " locations=" << m_locations.size() << "]\n";
    m_quadTree.print(os);
}