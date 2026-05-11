#pragma once
// src/optimizer/RouteOptimizer.h
// Smart City Delivery & Traffic Management System
//
// ── Responsibilities ──────────────────────────────────────────────────────────
// RouteOptimizer produces optimised delivery routes for each vehicle.
//
// Pipeline for one vehicle:
//   1. ActivitySelector → select non-overlapping deliveries in time window
//   2. Knapsack         → select best packages within weight capacity
//   3. TSP greedy       → nearest-neighbour ordering of selected stops
//   4. CityMapManager   → Dijkstra/BellmanFord legs between each stop
//
// Output: VehicleRoute — ordered list of stops with full path + cost.

#include <vector>
#include <memory>
#include <ostream>
#include "../core/Vehicle.h"
#include "../core/Package.h"
#include "../core/Location.h"
#include "../graph/RoadNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: RouteStop — one delivery stop in a vehicle's route
// ─────────────────────────────────────────────────────────────────────────────
struct RouteStop {
    int                      locationId;
    std::shared_ptr<Package> package;       // nullptr for depot / charging stops
    std::vector<int>         pathFromPrev;  // node IDs from previous stop
    double                   legCost{0.0};  // cost of this leg
    std::string              action;        // "pickup", "deliver", "start"
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: VehicleRoute — complete route for one vehicle
// ─────────────────────────────────────────────────────────────────────────────
struct VehicleRoute {
    std::shared_ptr<Vehicle>       vehicle;
    std::vector<RouteStop>         stops;
    std::vector<std::shared_ptr<Package>> assignedPackages;
    double                         totalCost{0.0};
    double                         totalDistance{0.0};
    bool                           valid{false};

    int stopCount() const { return static_cast<int>(stops.size()); }

    void print(std::ostream& os) const;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Class: RouteOptimizer
// ─────────────────────────────────────────────────────────────────────────────
class RouteOptimizer {
public:
    RouteOptimizer() = default;

    // ── Single vehicle route ──────────────────────────────────────────────────

    // Build optimised route for one vehicle given candidate packages
    VehicleRoute optimiseRoute(
        std::shared_ptr<Vehicle>                      vehicle,
        const std::vector<std::shared_ptr<Package>>&  candidates,
        std::function<std::pair<double,double>(int)>  getCoords) const;

    // ── Fleet route ───────────────────────────────────────────────────────────

    // Assign packages to vehicles and generate routes for the whole fleet.
    // Uses ZonePartitioner + Knapsack + ActivitySelector.
    std::vector<VehicleRoute> optimiseFleet(
        const std::vector<std::shared_ptr<Vehicle>>&  vehicles,
        const std::vector<std::shared_ptr<Package>>&  packages,
        std::function<std::pair<double,double>(int)>  getCoords) const;

    // ── Config ────────────────────────────────────────────────────────────────
    void setMaxPackagesPerVehicle(int n)   { m_maxPkgPerVehicle = n; }
    void setTimeWindowSeconds(int secs)    { m_timeWindowSecs = secs; }

private:
    int m_maxPkgPerVehicle{10};
    int m_timeWindowSecs  {28800};  // 8-hour window default

    // TSP nearest-neighbour greedy ordering of delivery stops
    // Returns ordered location IDs starting from vehicleCurrentLoc
    std::vector<int> tspNearestNeighbour(
        int startLocId,
        const std::vector<int>& destLocIds,
        std::function<std::pair<double,double>(int)> getCoords) const;

    // Build RouteStops by chaining Dijkstra between ordered stops
    std::vector<RouteStop> buildStops(
        const std::vector<int>&                      orderedLocs,
        const std::vector<std::shared_ptr<Package>>& packages) const;
};