#pragma once
// src/io/Reporter.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <memory>
#include <string>
#include <ostream>
#include "../core/Package.h"
#include "../core/Vehicle.h"
#include "../optimizer/RouteOptimizer.h"
#include "../system/TrafficMonitor.h"
#include "../graph/RoadNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Class: Reporter
//  Generates all system output reports as specified in the PDF.
//  Purely static — no instance needed.
// ─────────────────────────────────────────────────────────────────────────────
class Reporter {
public:
    Reporter() = delete;

    // ── Route output ──────────────────────────────────────────────────────────
    static void printFleetRoutes(
        const std::vector<VehicleRoute>& routes,
        std::ostream& os = std::cout);

    // ── Delivery schedule sorted by completion time ────────────────────────────
    static void printDeliverySchedule(
        std::vector<std::shared_ptr<Package>> packages,
        std::ostream& os = std::cout);

    // ── Metrics ───────────────────────────────────────────────────────────────
    static void printMetrics(
        const std::vector<VehicleRoute>& routes,
        std::ostream& os = std::cout);

    // ── Unserved deliveries ────────────────────────────────────────────────────
    static void printUnserved(
        const std::vector<std::shared_ptr<Package>>& allPackages,
        const std::vector<VehicleRoute>&             routes,
        std::ostream& os = std::cout);

    // ── Traffic bottleneck analysis ────────────────────────────────────────────
    static void printBottleneckAnalysis(
        const RoadNetwork&    graph,
        const TrafficMonitor& monitor,
        int                   topK = 5,
        std::ostream&         os   = std::cout);

    // ── Top-K busiest intersections ────────────────────────────────────────────
    static void printBusiestIntersections(
        const std::vector<VehicleRoute>& routes,
        const RoadNetwork&               graph,
        int                              topK = 5,
        std::ostream&                    os   = std::cout);

    // ── Full report (calls all above) ─────────────────────────────────────────
    static void printFullReport(
        const std::vector<VehicleRoute>&             routes,
        const std::vector<std::shared_ptr<Package>>& allPackages,
        const RoadNetwork&                           graph,
        const TrafficMonitor&                        monitor,
        std::ostream&                                os = std::cout);
};