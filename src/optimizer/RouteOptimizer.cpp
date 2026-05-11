// src/optimizer/RouteOptimizer.cpp
// Smart City Delivery & Traffic Management System

#include "RouteOptimizer.h"
#include "Knapsack.h"
#include "ActivitySelector.h"
#include "../dataprocessor/ZonePartitioner.h"
#include "../system/CityMapManager.h"
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <limits>

// ─────────────────────────────────────────────────────────────────────────────
//  VehicleRoute::print
// ─────────────────────────────────────────────────────────────────────────────
void VehicleRoute::print(std::ostream& os) const {
    os << "VehicleRoute[vehicle=" << vehicle->getName()
       << " stops=" << stops.size()
       << " pkgs=" << assignedPackages.size()
       << " cost=" << totalCost << "]\n";
    for (const auto& s : stops) {
        os << "  " << s.action << " @ loc=" << s.locationId
           << " legCost=" << s.legCost;
        if (s.package) os << " pkg=" << s.package->getId();
        os << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  TSP nearest-neighbour greedy
// ─────────────────────────────────────────────────────────────────────────────
std::vector<int> RouteOptimizer::tspNearestNeighbour(
    int startLocId,
    const std::vector<int>& destLocIds,
    std::function<std::pair<double,double>(int)> getCoords) const
{
    std::vector<int>         ordered;
    std::unordered_set<int>  unvisited(destLocIds.begin(), destLocIds.end());
    int current = startLocId;

    while (!unvisited.empty()) {
        double bestDist = std::numeric_limits<double>::max();
        int    bestLoc  = -1;

        auto [cx, cy] = getCoords(current);

        for (int loc : unvisited) {
            auto [lx, ly] = getCoords(loc);
            double d = (lx-cx)*(lx-cx) + (ly-cy)*(ly-cy);
            if (d < bestDist) { bestDist = d; bestLoc = loc; }
        }
        if (bestLoc == -1) break;
        ordered.push_back(bestLoc);
        unvisited.erase(bestLoc);
        current = bestLoc;
    }
    return ordered;
}

// ─────────────────────────────────────────────────────────────────────────────
//  buildStops — chain Dijkstra between ordered stops
// ─────────────────────────────────────────────────────────────────────────────
std::vector<RouteStop> RouteOptimizer::buildStops(
    const std::vector<int>& orderedLocs,
    const std::vector<std::shared_ptr<Package>>& packages) const
{
    auto& mgr = CityMapManager::getInstance();

    // Build destLoc → package map for action labelling
    std::unordered_map<int, std::shared_ptr<Package>> destMap;
    for (const auto& pkg : packages)
        destMap[pkg->getDestLocationId()] = pkg;

    std::vector<RouteStop> stops;
    stops.reserve(orderedLocs.size());

    for (std::size_t i = 0; i < orderedLocs.size(); ++i) {
        RouteStop s;
        s.locationId = orderedLocs[i];

        // Find package for this stop
        auto it = destMap.find(s.locationId);
        if (it != destMap.end()) {
            s.package = it->second;
            s.action  = (i == 0) ? "start+pickup" : "deliver";
        } else {
            s.action = (i == 0) ? "start" : "waypoint";
        }

        // Compute leg from previous stop
        if (i > 0) {
            auto path = mgr.shortestPath(orderedLocs[i-1], orderedLocs[i]);
            s.pathFromPrev = path.reachable ? path.path : std::vector<int>{};
            s.legCost      = path.reachable ? path.totalCost : 0.0;
        } else {
            s.legCost = 0.0;
        }
        stops.push_back(std::move(s));
    }
    return stops;
}

// ─────────────────────────────────────────────────────────────────────────────
//  optimiseRoute — single vehicle
// ─────────────────────────────────────────────────────────────────────────────
VehicleRoute RouteOptimizer::optimiseRoute(
    std::shared_ptr<Vehicle>                     vehicle,
    const std::vector<std::shared_ptr<Package>>& candidates,
    std::function<std::pair<double,double>(int)> getCoords) const
{
    VehicleRoute route;
    route.vehicle = vehicle;

    if (!vehicle || !vehicle->isAvailable() || candidates.empty()) return route;

    // ── Step 1: Activity selection (time-feasible deliveries) ────────────────
    auto activities = Greedy::buildActivities(candidates,
        [&](int /*pkgId*/) -> double { return 0.0; }); // simplified: no travel pre-estimate

    auto selected = Greedy::selectActivities(std::move(activities));

    std::vector<std::shared_ptr<Package>> feasible;
    feasible.reserve(selected.size());
    for (const auto& act : selected) feasible.push_back(act.pkg);

    // ── Step 2: Knapsack — select by weight/priority ──────────────────────────
    auto loaded = Greedy::fractionalKnapsack(
        feasible,
        vehicle->getRemainingCapacity(),
        m_maxPkgPerVehicle);

    if (loaded.empty()) return route;

    // ── Step 3: TSP nearest-neighbour ordering ────────────────────────────────
    std::vector<int> destLocs;
    destLocs.reserve(loaded.size());
    for (const auto& pkg : loaded)
        destLocs.push_back(pkg->getDestLocationId());

    auto ordered = tspNearestNeighbour(
        vehicle->getCurrentLocationId(), destLocs, getCoords);

    // Prepend vehicle start location
    std::vector<int> fullRoute;
    fullRoute.push_back(vehicle->getCurrentLocationId());
    fullRoute.insert(fullRoute.end(), ordered.begin(), ordered.end());

    // ── Step 4: Build stops with Dijkstra legs ────────────────────────────────
    route.stops            = buildStops(fullRoute, loaded);
    route.assignedPackages = loaded;

    for (const auto& s : route.stops)
        route.totalCost += s.legCost;

    route.totalDistance = route.totalCost; // distance == cost in simple model
    route.valid         = true;
    return route;
}

// ─────────────────────────────────────────────────────────────────────────────
//  optimiseFleet — all vehicles
// ─────────────────────────────────────────────────────────────────────────────
std::vector<VehicleRoute> RouteOptimizer::optimiseFleet(
    const std::vector<std::shared_ptr<Vehicle>>&  vehicles,
    const std::vector<std::shared_ptr<Package>>&  packages,
    std::function<std::pair<double,double>(int)>  getCoords) const
{
    std::vector<VehicleRoute> routes;
    if (vehicles.empty() || packages.empty()) return routes;

    // ── Collect destination locations ─────────────────────────────────────────
    std::vector<std::shared_ptr<Location>> destLocs;
    auto& mgr = CityMapManager::getInstance();
    for (const auto& pkg : packages) {
        auto loc = mgr.getLocation(pkg->getDestLocationId());
        if (loc) destLocs.push_back(loc);
    }

    // ── Zone partition: one zone per available vehicle ────────────────────────
    auto availVehicles = std::vector<std::shared_ptr<Vehicle>>{};
    for (const auto& v : vehicles)
        if (v->isAvailable()) availVehicles.push_back(v);

    if (availVehicles.empty()) return routes;

    int numZones = static_cast<int>(availVehicles.size());
    auto zones   = DivideConquer::partitionZones(destLocs, numZones);
    DivideConquer::assignPackagesToZones(zones, packages);

    // ── Assign each zone to nearest vehicle, build route ─────────────────────
    std::unordered_set<int> usedVehicles;

    for (auto& zone : zones) {
        if (zone.packages.empty()) continue;

        // Find nearest available vehicle to zone centroid
        auto [zx, zy] = zone.centroid();
        std::shared_ptr<Vehicle> bestVeh;
        double bestDist = std::numeric_limits<double>::max();

        for (const auto& v : availVehicles) {
            if (usedVehicles.count(v->getId())) continue;
            auto [vx, vy] = getCoords(v->getCurrentLocationId());
            double d = (vx-zx)*(vx-zx) + (vy-zy)*(vy-zy);
            if (d < bestDist) { bestDist = d; bestVeh = v; }
        }
        if (!bestVeh) continue;
        usedVehicles.insert(bestVeh->getId());

        auto route = optimiseRoute(bestVeh, zone.packages, getCoords);
        if (route.valid) routes.push_back(std::move(route));
    }
    return routes;
}