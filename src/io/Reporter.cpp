// src/io/Reporter.cpp
// Smart City Delivery & Traffic Management System

#include "Reporter.h"
#include "../dataprocessor/MergeSort.h"
#include "../graph/Dijkstra.h"
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static void separator(std::ostream& os, char c = '-', int n = 52) {
    os << std::string(n, c) << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fleet Routes
// ─────────────────────────────────────────────────────────────────────────────
void Reporter::printFleetRoutes(const std::vector<VehicleRoute>& routes,
                                 std::ostream& os)
{
    separator(os, '=');
    os << "  OPTIMISED ROUTES FOR ALL VEHICLES\n";
    separator(os, '=');

    if (routes.empty()) {
        os << "  No routes generated.\n";
        return;
    }

    for (const auto& r : routes) {
        separator(os);
        os << "  Vehicle : " << r.vehicle->getName()
           << " [" << vehicleTypeToString(r.vehicle->getType()) << "]\n";
        os << "  Packages: " << r.assignedPackages.size()
           << "  Total cost: " << std::fixed << std::setprecision(2)
           << r.totalCost << "\n";
        separator(os);

        for (std::size_t i = 0; i < r.stops.size(); ++i) {
            const auto& s = r.stops[i];
            os << "  [" << i << "] " << std::left << std::setw(12) << s.action
               << " location=" << s.locationId;
            if (s.package)
                os << "  pkg=" << s.package->getTrackingNumber();
            if (i > 0)
                os << "  leg=" << std::fixed << std::setprecision(2) << s.legCost;
            os << "\n";

            // Print path nodes
            if (i > 0 && !s.pathFromPrev.empty()) {
                os << "     path: ";
                for (int n : s.pathFromPrev) os << n << " ";
                os << "\n";
            }
        }
        os << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Delivery Schedule
// ─────────────────────────────────────────────────────────────────────────────
void Reporter::printDeliverySchedule(
    std::vector<std::shared_ptr<Package>> packages,
    std::ostream& os)
{
    separator(os, '=');
    os << "  DELIVERY SCHEDULE (sorted by deadline)\n";
    separator(os, '=');

    // Merge sort by deadline ascending
    Sorting::sortByDeadline(packages);

    os << std::left
       << std::setw(6)  << "ID"
       << std::setw(12) << "Tracking"
       << std::setw(10) << "Priority"
       << std::setw(10) << "Weight"
       << std::setw(12) << "Status"
       << "Deadline\n";
    separator(os);

    for (const auto& p : packages) {
        std::time_t dl = p->getDeadline();
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&dl));

        os << std::left
           << std::setw(6)  << p->getId()
           << std::setw(12) << p->getTrackingNumber()
           << std::setw(10) << priorityToString(p->getPriority())
           << std::setw(10) << (std::to_string(p->getWeightKg()) + "kg")
           << std::setw(12) << deliveryStatusToString(p->getStatus())
           << buf << "\n";
    }
    os << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Metrics
// ─────────────────────────────────────────────────────────────────────────────
void Reporter::printMetrics(const std::vector<VehicleRoute>& routes,
                             std::ostream& os)
{
    separator(os, '=');
    os << "  FLEET METRICS\n";
    separator(os, '=');

    double totalCost     = 0;
    int    totalPackages = 0;
    int    totalStops    = 0;

    for (const auto& r : routes) {
        totalCost     += r.totalCost;
        totalPackages += static_cast<int>(r.assignedPackages.size());
        totalStops    += r.stopCount();

        os << "  " << std::left << std::setw(12) << r.vehicle->getName()
           << " pkgs=" << std::setw(4) << r.assignedPackages.size()
           << " stops=" << std::setw(4) << r.stopCount()
           << " cost=" << std::fixed << std::setprecision(2) << r.totalCost
           << "\n";
    }

    separator(os);
    os << "  Vehicles active  : " << routes.size()    << "\n";
    os << "  Total packages   : " << totalPackages     << "\n";
    os << "  Total stops      : " << totalStops        << "\n";
    os << "  Total route cost : " << std::fixed
       << std::setprecision(2) << totalCost           << "\n";
    if (!routes.empty())
        os << "  Avg cost/vehicle : " << std::fixed
           << std::setprecision(2) << totalCost / routes.size() << "\n";
    os << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Unserved deliveries
// ─────────────────────────────────────────────────────────────────────────────
void Reporter::printUnserved(
    const std::vector<std::shared_ptr<Package>>& allPackages,
    const std::vector<VehicleRoute>&             routes,
    std::ostream& os)
{
    // Collect served package IDs
    std::unordered_set<int> served;
    for (const auto& r : routes)
        for (const auto& p : r.assignedPackages)
            served.insert(p->getId());

    std::vector<std::shared_ptr<Package>> unserved;
    for (const auto& p : allPackages)
        if (!served.count(p->getId()) && p->isPending())
            unserved.push_back(p);

    separator(os, '=');
    os << "  UNSERVED DELIVERIES (" << unserved.size() << ")\n";
    separator(os, '=');

    if (unserved.empty()) {
        os << "  All pending deliveries have been assigned.\n\n";
        return;
    }
    for (const auto& p : unserved)
        os << "  " << *p << "\n";
    os << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Traffic bottleneck analysis
// ─────────────────────────────────────────────────────────────────────────────
void Reporter::printBottleneckAnalysis(
    const RoadNetwork& graph,
    const TrafficMonitor& monitor,
    int topK, std::ostream& os)
{
    separator(os, '=');
    os << "  TRAFFIC BOTTLENECK ANALYSIS (top " << topK << ")\n";
    separator(os, '=');

    auto congested = monitor.getMostCongestedRoads(topK);
    if (congested.empty()) {
        os << "  No congestion events recorded.\n\n";
        return;
    }
    for (const auto& ev : congested) {
        os << "  Road " << ev.fromId << " -> " << ev.toId
           << "  weight=" << std::fixed << std::setprecision(2) << ev.newWeight
           << "  event=" << trafficEventTypeToString(ev.type)
           << "  \"" << ev.description << "\"\n";
    }
    os << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Busiest intersections
// ─────────────────────────────────────────────────────────────────────────────
void Reporter::printBusiestIntersections(
    const std::vector<VehicleRoute>& routes,
    const RoadNetwork& graph,
    int topK, std::ostream& os)
{
    separator(os, '=');
    os << "  TOP-" << topK << " BUSIEST INTERSECTIONS\n";
    separator(os, '=');

    // Count how many times each node appears across all route paths
    std::unordered_map<int, int> nodeCount;
    for (const auto& route : routes)
        for (const auto& stop : route.stops)
            for (int n : stop.pathFromPrev)
                nodeCount[n]++;

    // Sort by count descending using a vector of pairs
    std::vector<std::pair<int,int>> sorted(nodeCount.begin(), nodeCount.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const std::pair<int,int>& a, const std::pair<int,int>& b) {
            return a.second > b.second;
        });

    if (sorted.empty()) {
        os << "  No path data available.\n\n";
        return;
    }

    int count = 0;
    for (const auto& [nodeId, freq] : sorted) {
        if (count++ >= topK) break;
        auto loc = graph.getLocation(nodeId);
        os << "  #" << count << "  node=" << nodeId;
        if (loc) os << " (" << loc->getName() << ")";
        os << "  traversals=" << freq << "\n";
    }
    os << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Full report
// ─────────────────────────────────────────────────────────────────────────────
void Reporter::printFullReport(
    const std::vector<VehicleRoute>&             routes,
    const std::vector<std::shared_ptr<Package>>& allPackages,
    const RoadNetwork&                           graph,
    const TrafficMonitor&                        monitor,
    std::ostream&                                os)
{
    os << "\n";
    separator(os, '#');
    os << "#    SMART CITY DELIVERY SYSTEM — FULL REPORT\n";
    separator(os, '#');
    os << "\n";

    printFleetRoutes(routes, os);
    printDeliverySchedule(allPackages, os);
    printMetrics(routes, os);
    printUnserved(allPackages, routes, os);
    printBottleneckAnalysis(graph, monitor, 5, os);
    printBusiestIntersections(routes, graph, 5, os);
}