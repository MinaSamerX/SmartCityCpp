#pragma once
// src/system/SmartCitySystem.h
// Smart City Delivery & Traffic Management System
//
// ── DESIGN PATTERN: Facade ───────────────────────────────────────────────────
// SmartCitySystem is the single entry point for main.cpp.
// It hides all six sub-components behind one clean API:
//
//   CityMapManager    — graph / path finding (Singleton)
//   EntityManager     — O(1) entity lookup
//   SpatialQueryEngine— nearest-neighbour / radius search
//   TrafficMonitor    — event broadcasting (Observer)
//   DeliveryScheduler — priority queue management
//   RouteOptimizer    — greedy route generation
//   Reporter          — formatted output
//
// The two main scenarios from the PDF are exposed as single method calls:
//   processNewDelivery()  — new delivery request end-to-end
//   handleRushHour()      — traffic update → reroute → reorder

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <ostream>
#include "CityMapManager.h"
#include "EntityManager.h"
#include "SpatialQueryEngine.h"
#include "TrafficMonitor.h"
#include "EntityFactory.h"
#include "../scheduler/DeliveryScheduler.h"
#include "../optimizer/RouteOptimizer.h"
#include "../io/Reporter.h"
#include "../io/FileParser.h"

class SmartCitySystem {
public:
    // ── Construction ─────────────────────────────────────────────────────────
    SmartCitySystem();
    ~SmartCitySystem() = default;

    SmartCitySystem(const SmartCitySystem&)            = delete;
    SmartCitySystem& operator=(const SmartCitySystem&) = delete;

    // ── Initialisation ────────────────────────────────────────────────────────

    // Load everything from data directory — returns false on critical failure
    bool loadFromDirectory(const std::string& dataDir);

    // ── Scenario 1: New Delivery Request ──────────────────────────────────────
    // 1. Hash lookup: customer + destination
    // 2. Spatial tree: nearest available vehicle
    // 3. Priority queue: insert with deadline-based priority
    // 4. Graph: Dijkstra from vehicle → pickup → destination
    // 5. Greedy: if multi-delivery, optimise order
    bool processNewDelivery(
        int              packageId,
        int              srcLocationId,
        int              destLocationId,
        int              customerId,
        double           weightKg,
        DeliveryPriority priority,
        std::time_t      deadline);

    // ── Scenario 2: Rush Hour Traffic Update ──────────────────────────────────
    // 1. Graph: update edge weights for congested roads
    // 2. TrafficMonitor: broadcast event to all observers
    // 3. DeliveryScheduler (as observer): reorders at-risk packages
    // 4. RouteOptimizer: regenerate affected vehicle routes
    void handleTrafficUpdate(int fromId, int toId,
                             double newWeight,
                             const std::string& description = "");

    void handleRoadClosure(int fromId, int toId,
                           const std::string& description = "");

    void handleAccident(int fromId, int toId,
                        const std::string& description = "");

    // ── Fleet dispatch ────────────────────────────────────────────────────────
    // Assign all pending deliveries to vehicles and generate routes
    std::vector<VehicleRoute> dispatchFleet();

    // ── Queries (delegated to sub-components) ─────────────────────────────────
    std::shared_ptr<Location> findNearestLocation(double x, double y) const;
    std::shared_ptr<Vehicle>  findNearestVehicle(double x, double y)  const;
    PathResult                shortestPath(int src, int dest)          const;
    std::vector<std::shared_ptr<Package>> getPendingDeliveries()      const;
    std::vector<std::shared_ptr<Package>> getOverdueDeliveries()      const;

    // ── Reporting ─────────────────────────────────────────────────────────────
    void printReport(const std::vector<VehicleRoute>& routes,
                     std::ostream& os = std::cout) const;

    void printStatus(std::ostream& os = std::cout) const;

    // ── Sub-component accessors (for advanced use) ────────────────────────────
    EntityManager&      entities()   { return m_entities;   }
    TrafficMonitor&     traffic()    { return m_traffic;    }
    DeliveryScheduler&  scheduler()  { return m_scheduler;  }
    SpatialQueryEngine& spatial()    { return m_spatial;    }
    RouteOptimizer&     optimizer()  { return m_optimizer;  }

private:
    // ── Sub-components ────────────────────────────────────────────────────────
    EntityManager       m_entities;
    SpatialQueryEngine  m_spatial;
    TrafficMonitor      m_traffic;
    DeliveryScheduler   m_scheduler;
    RouteOptimizer      m_optimizer;

    // Last dispatched routes (for reporting)
    std::vector<VehicleRoute> m_lastRoutes;

    // ── Helpers ───────────────────────────────────────────────────────────────

    // Lambda injected into spatial/optimizer queries
    std::function<std::pair<double,double>(int)> makeCoordsFn() const;

    void rebuildSpatialIndex();
    void connectObservers();
};