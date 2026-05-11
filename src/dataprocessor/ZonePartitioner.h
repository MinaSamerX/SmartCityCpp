#pragma once
// src/dataprocessor/ZonePartitioner.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <memory>
#include <algorithm>
#include <ostream>
#include <unordered_map>
#include "../core/Location.h"
#include "../core/Package.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: DivideConquer
//
//  Zone Partitioner — O(N log N)
//
//  Splits the city's delivery locations into balanced geographic zones.
//  Each zone is assigned to one vehicle.
//
//  Algorithm (Divide & Conquer):
//    1. Find bounding box of all delivery points
//    2. Split along the longer axis at the median coordinate
//    3. Recurse on each half until zones ≤ maxZoneSize or depth limit
//
//  This gives balanced, spatially coherent zones — minimising
//  cross-zone travel and empty-vehicle distance.
// ─────────────────────────────────────────────────────────────────────────────
namespace DivideConquer {

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: Zone — one geographic delivery zone
// ─────────────────────────────────────────────────────────────────────────────
struct Zone {
    int                                    zoneId;
    std::vector<std::shared_ptr<Location>> locations;
    std::vector<std::shared_ptr<Package>>  packages;

    // Centroid of all locations in this zone
    std::pair<double,double> centroid() const {
        if (locations.empty()) return {0.0, 0.0};
        double sx = 0, sy = 0;
        for (const auto& l : locations) { sx += l->getX(); sy += l->getY(); }
        return {sx / locations.size(), sy / locations.size()};
    }

    double totalWeight() const {
        double w = 0;
        for (const auto& p : packages) w += p->getWeightKg();
        return w;
    }

    void print(std::ostream& os) const {
        auto [cx, cy] = centroid();
        os << "Zone[" << zoneId << "] locs=" << locations.size()
           << " pkgs=" << packages.size()
           << " weight=" << totalWeight() << "kg"
           << " centroid=(" << cx << "," << cy << ")\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Internal recursive helper
// ─────────────────────────────────────────────────────────────────────────────
namespace detail {

inline void partitionRec(
    std::vector<std::shared_ptr<Location>>& locs,
    int lo, int hi,
    int maxPerZone,
    int maxDepth,
    int depth,
    int& nextZoneId,
    std::vector<Zone>& zones)
{
    int count = hi - lo + 1;
    if (count <= 0) return;

    // Base case: small enough or max depth → create a zone
    if (count <= maxPerZone || depth >= maxDepth) {
        Zone z;
        z.zoneId = nextZoneId++;
        for (int i = lo; i <= hi; ++i)
            z.locations.push_back(locs[i]);
        zones.push_back(std::move(z));
        return;
    }

    // Find bounding box of locs[lo..hi]
    double minX = locs[lo]->getX(), maxX = minX;
    double minY = locs[lo]->getY(), maxY = minY;
    for (int i = lo; i <= hi; ++i) {
        minX = std::min(minX, locs[i]->getX());
        maxX = std::max(maxX, locs[i]->getX());
        minY = std::min(minY, locs[i]->getY());
        maxY = std::max(maxY, locs[i]->getY());
    }

    int mid = lo + (count - 1) / 2;

    // Split along the longer axis
    if ((maxX - minX) >= (maxY - minY)) {
        // Split on X — nth_element puts median at mid, left < median, right >= median
        std::nth_element(locs.begin() + lo, locs.begin() + mid,
                         locs.begin() + hi + 1,
            [](const std::shared_ptr<Location>& a,
               const std::shared_ptr<Location>& b) {
                return a->getX() < b->getX();
            });
    } else {
        // Split on Y
        std::nth_element(locs.begin() + lo, locs.begin() + mid,
                         locs.begin() + hi + 1,
            [](const std::shared_ptr<Location>& a,
               const std::shared_ptr<Location>& b) {
                return a->getY() < b->getY();
            });
    }

    partitionRec(locs, lo,    mid, maxPerZone, maxDepth, depth+1, nextZoneId, zones);
    partitionRec(locs, mid+1, hi,  maxPerZone, maxDepth, depth+1, nextZoneId, zones);
}

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
//  partitionZones  — public API
//
//  Splits locations into at most numZones balanced geographic zones.
//  Returns vector of Zone (each zone gets its location subset).
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<Zone> partitionZones(
    std::vector<std::shared_ptr<Location>> locations,
    int numZones,
    int maxDepth = 16)
{
    if (locations.empty() || numZones <= 0) return {};

    // maxPerZone: target size per zone
    int maxPerZone = std::max(1,
        static_cast<int>(locations.size()) / numZones);

    std::vector<Zone> zones;
    int nextId = 0;
    detail::partitionRec(locations, 0,
                         static_cast<int>(locations.size()) - 1,
                         maxPerZone, maxDepth, 0,
                         nextId, zones);
    return zones;
}

// ─────────────────────────────────────────────────────────────────────────────
//  assignPackagesToZones
//  After partitioning, match each Package to its zone by destination location.
// ─────────────────────────────────────────────────────────────────────────────
inline void assignPackagesToZones(
    std::vector<Zone>& zones,
    const std::vector<std::shared_ptr<Package>>& packages)
{
    // Build location-id → zone-index map
    std::unordered_map<int, int> locToZone;
    for (int zi = 0; zi < static_cast<int>(zones.size()); ++zi)
        for (const auto& loc : zones[zi].locations)
            locToZone[loc->getId()] = zi;

    for (const auto& pkg : packages) {
        auto it = locToZone.find(pkg->getDestLocationId());
        if (it != locToZone.end())
            zones[it->second].packages.push_back(pkg);
    }
}

} // namespace DivideConquer