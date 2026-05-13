// src/main.cpp
// Smart City Delivery & Traffic Management System

#include <iostream>
#include <string>
#include <ctime>
#include <fstream>
#include <vector>
#include "system/SmartCitySystem.h"
#include "system/CityMapManager.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static void banner(const std::string& title) {
    std::cout << "\n" << std::string(54,'=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(54,'=') << "\n";
}
static void section(const std::string& title) {
    std::cout << "\n── " << title << " ──\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario 1 — New Delivery Request  (PDF scenario, Section "Sample Operations")
//  Flow: hash lookup → spatial tree → priority queue → Dijkstra → greedy
// ─────────────────────────────────────────────────────────────────────────────
static void demoNewDelivery(SmartCitySystem& sys) {
    section("Scenario 1: New Delivery Request");

    // Add two new urgent deliveries using node IDs from our city_map.txt
    // Warehouse_A(1) → Hospital(3)  — High priority
    bool ok1 = sys.processNewDelivery(
        2001, 1, 3, 0,             // id=2001, src=1, dest=3, cust=0
        12.5, DeliveryPriority::High,
        static_cast<std::time_t>(1770000000));
    std::cout << "  Delivery 2001 (Warehouse→Hospital, High):   "
              << (ok1 ? "✓ Queued" : "✗ Failed") << "\n";

    // Downtown(2) → University(5)  — Critical
    bool ok2 = sys.processNewDelivery(
        2002, 2, 5, 0,
        8.0, DeliveryPriority::Critical,
        static_cast<std::time_t>(1770003600));
    std::cout << "  Delivery 2002 (Downtown→University, Crit):  "
              << (ok2 ? "✓ Queued" : "✗ Failed") << "\n";

    // Show what is next in queue
    auto pending = sys.getPendingDeliveries();
    std::cout << "  Total pending : " << pending.size() << "\n";
    if (!pending.empty()) {
        auto& top = pending.front();
        std::cout << "  Highest priority: id=" << top->getId()
                  << "  priority=" << static_cast<int>(top->getPriority())
                  << "  route=" << top->getSourceLocationId()
                  << " -> "   << top->getDestLocationId() << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario 2 — Rush Hour Traffic Update  (PDF scenario)
//  Flow: graph update → observer broadcast → scheduler reorder → reroute
// ─────────────────────────────────────────────────────────────────────────────
static void demoRushHour(SmartCitySystem& sys) {
    section("Scenario 2: Rush Hour Traffic Update");

    auto& mgr = CityMapManager::getInstance();

    auto before = mgr.shortestPath(1, 6);   // Warehouse → Airport
    std::cout << "  Path 1(Warehouse)→6(Airport) BEFORE rush:\n";
    std::cout << "    cost=" << before.totalCost << "  via:";
    for (int n : before.path) std::cout << " " << n;
    std::cout << "\n";

    // Simulate congestion on main arteries
    // Apply heavy congestion on top of current weights
    sys.handleTrafficUpdate(1, 2, 50.0, "Rush hour: Warehouse→Downtown blocked");
    sys.handleTrafficUpdate(2, 3, 45.0, "Rush hour: Downtown→Hospital congested");
    sys.handleTrafficUpdate(5, 6, 80.0, "Rush hour: University→Airport gridlock");

    auto after = mgr.shortestPath(1, 6);
    std::cout << "  Path 1(Warehouse)→6(Airport) AFTER  rush:\n";
    std::cout << "    cost=" << after.totalCost << "  via:";
    for (int n : after.path) std::cout << " " << n;
    std::cout << "\n";
    std::cout << "  Algorithm: " << mgr.currentAlgorithm() << "\n";
    std::cout << "  Cost delta: +" << (after.totalCost - before.totalCost) << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario 3 — Road Closure & Accident
// ─────────────────────────────────────────────────────────────────────────────
static void demoIncidents(SmartCitySystem& sys) {
    section("Scenario 3: Road Closure & Accident");

    auto& mgr = CityMapManager::getInstance();

    // Close road Industrial_Zone→Train_Station (7→8)
    sys.handleRoadClosure(7, 8, "Water main burst");
    std::cout << "  Road 7(Industrial)→8(Train Station) CLOSED\n";

    auto pathAlt = mgr.shortestPath(7, 9);  // find alt route to Residential
    std::cout << "  Alt path 7→9: ";
    if (pathAlt.reachable) {
        std::cout << "cost=" << pathAlt.totalCost << "  via:";
        for (int n : pathAlt.path) std::cout << " " << n;
    } else {
        std::cout << "UNREACHABLE";
    }
    std::cout << "\n";

    // Accident on Hospital→University (3→5)
    sys.handleAccident(3, 5, "Multi-vehicle collision on Hospital road");
    std::cout << "  Accident on 3(Hospital)→5(University) — road closed + penalised\n";

    // Show affected paths
    auto pathAcc = mgr.shortestPath(1, 5);
    std::cout << "  Path 1→5 after accident: cost=" << pathAcc.totalCost
              << "  via:";
    for (int n : pathAcc.path) std::cout << " " << n;
    std::cout << "\n";

    // Overdue check
    auto overdue = sys.getOverdueDeliveries();
    std::cout << "  Overdue deliveries: " << overdue.size() << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario 4 — Spatial Queries
// ─────────────────────────────────────────────────────────────────────────────
static void demoSpatial(SmartCitySystem& sys) {
    section("Spatial Queries (QuadTree + KDTree)");

    // Nearest location to Downtown(25,30)
    auto nearLoc = sys.findNearestLocation(25.0, 30.0);
    if (nearLoc)
        std::cout << "  Nearest location to (25,30): "
                  << nearLoc->getName() << "\n";

    // Nearest available vehicle to Warehouse(10,20)
    auto nearVeh = sys.findNearestVehicle(10.0, 20.0);
    if (nearVeh)
        std::cout << "  Nearest vehicle to (10,20):  "
                  << nearVeh->getName()
                  << "  (at location " << nearVeh->getCurrentLocationId() << ")\n";

    // Several path queries to show graph algorithms
    std::vector<std::pair<int,int>> queries = {{1,10},{2,9},{7,6},{8,5}};
    for (auto [s,d] : queries) {
        auto p = sys.shortestPath(s, d);
        std::cout << "  " << s << " → " << d << ": ";
        if (p.reachable) {
            std::cout << "cost=" << p.totalCost << "  hops:";
            for (int n : p.path) std::cout << " " << n;
        } else {
            std::cout << "unreachable";
        }
        std::cout << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario 5 — Fleet Dispatch + Full Report
// ─────────────────────────────────────────────────────────────────────────────
static void demoDispatch(SmartCitySystem& sys) {
    section("Scenario 5: Fleet Dispatch");

    auto routes = sys.dispatchFleet();
    std::cout << "  Routes generated: " << routes.size() << "\n\n";

    if (!routes.empty()) {
        banner("Full System Report");
        sys.printReport(routes, std::cout);
    } else {
        // Still print status/schedule even if no routes dispatched
        sys.printStatus(std::cout);
        std::cout << "\n  (No routes dispatched — all vehicles may be at "
                     "capacity or no compatible packages)\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    banner("Smart City Delivery & Traffic Management System");

    // ── Resolve data directory ────────────────────────────────────────────────
    // Try: (1) command-line arg, (2) ./data, (3) ../../data (when run from build/bin)
    std::string dataDir = "data";
    if (argc > 1) {
        dataDir = argv[1];
    } else {
        // Auto-detect: walk up from exe location to find data/
        std::vector<std::string> candidates = {
            "data",
            "../data",
            "../../data",
            "../../../data"
        };
        for (auto& c : candidates) {
            std::ifstream test(c + "/city_map.txt");
            if (test.good()) { dataDir = c; break; }
        }
    }
    std::cout << "Data directory : " << dataDir << "\n";
    std::cout << "Data files     : city_map.txt, locations.txt, vehicles.txt,\n"
              << "                 deliveries.txt, traffic_updates.txt\n";

    // ── Initialise system (loads all data files) ──────────────────────────────
    SmartCitySystem sys;
    if (!sys.loadFromDirectory(dataDir)) {
        std::cerr << "[FATAL] Could not load data from: " << dataDir << "\n";
        std::cerr << "        Run from project root or pass data path as argument:\n";
        std::cerr << "        SmartCityApp.exe ../../data\n";
        return 1;
    }

    sys.printStatus(std::cout);

    // ── Run all 5 scenarios ───────────────────────────────────────────────────
    demoSpatial(sys);
    demoDispatch(sys);      // dispatch loaded packages BEFORE manual demos
    demoNewDelivery(sys);
    demoRushHour(sys);
    demoIncidents(sys);

    banner("System Shutdown — All Done");
    return 0;
}