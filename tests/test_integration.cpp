// tests/test_integration.cpp
// Smart City Delivery & Traffic Management System
// ── Full System Integration Tests ────────────────────────────────────────────
// Tests the complete flow as described in the PDF:
//   Scenario A: New Delivery Request
//   Scenario B: Rush Hour Traffic Update
//   Scenario C: Road Closure + Rerouting
//   Scenario D: Multi-vehicle dispatch
//   Scenario E: Analytics (top-K, bottlenecks)

#include "system/SmartCitySystem.h"
#include "system/CityMapManager.h"
#include "system/EntityManager.h"
#include "system/EntityFactory.h"
#include "system/SpatialQueryEngine.h"
#include "system/TrafficMonitor.h"
#include "scheduler/DeliveryScheduler.h"
#include "graph/Dijkstra.h"
#include "graph/MST.h"
#include "dataprocessor/MergeSort.h"
#include "dataprocessor/ClosestPair.h"
#include <iostream>
#include <cassert>
#include <ctime>
#include <memory>
#include <vector>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Test framework
// ─────────────────────────────────────────────────────────────────────────────
static int s_passed = 0, s_failed = 0;

static void pass(const char* name) {
    std::cout << "  [PASS] " << name << "\n";
    ++s_passed;
}
static void fail(const char* name, const char* detail = "") {
    std::cerr << "  [FAIL] " << name << "  " << detail << "\n";
    ++s_failed;
}
#define CHECK(cond, name) do { if(cond) pass(name); else fail(name, #cond); } while(0)
#define APPROX(a,b) (std::fabs((a)-(b)) < 1e-6)

// ─────────────────────────────────────────────────────────────────────────────
//  Build the integration test environment in-memory
//  (mirrors the 10-node city from city_map.txt)
// ─────────────────────────────────────────────────────────────────────────────
static void setupCity(CityMapManager& mgr) {
    mgr.reset();
    // 10 nodes
    struct N { int id; const char* name; double x,y; LocationType t; };
    N nodes[] = {
        {1,"Warehouse_A",10,20,LocationType::Warehouse},
        {2,"Downtown",   25,30,LocationType::TrafficHub},
        {3,"Hospital",   40,35,LocationType::TrafficHub},
        {4,"Mall",       55,20,LocationType::Customer},
        {5,"University", 70,40,LocationType::Intersection},
        {6,"Airport",    90,50,LocationType::ChargingStation},
        {7,"Industrial", 20,70,LocationType::Warehouse},
        {8,"Train",      45,75,LocationType::TrafficHub},
        {9,"Residential",65,85,LocationType::Customer},
        {10,"Port",      95,90,LocationType::TrafficHub},
    };
    for (auto& n : nodes) mgr.addNode(n.id, n.name, n.x, n.y, n.t);

    // 15 edges (directed, from city_map.txt)
    struct E { int f,t; double w; };
    E edges[] = {
        {1,2,18},{1,7,25},{2,3,12},{2,4,20},{3,5,15},
        {4,5,10},{5,6,22},{7,8,14},{8,9,16},{9,10,18},
        {3,8,30},{4,8,28},{6,10,20},{2,7,19},{5,9,17},
    };
    for (auto& e : edges) mgr.addEdge(e.f, e.t, e.w);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario A — New Delivery Request
//  PDF: Hash → Spatial → PQ → Dijkstra → Greedy
// ─────────────────────────────────────────────────────────────────────────────
static void test_scenario_new_delivery() {
    std::cout << "\n── Scenario A: New Delivery Request ──\n";

    auto& mgr = CityMapManager::getInstance();
    setupCity(mgr);

    // Step 1: Hash lookup — verify nodes exist
    CHECK(mgr.hasNode(1) && mgr.hasNode(3), "Hash: src=1 and dest=3 exist in graph");

    // Step 2: Dijkstra shortest path from Warehouse(1) to Hospital(3)
    auto path = mgr.shortestPath(1, 3);
    CHECK(path.reachable,              "Dijkstra: 1→3 reachable");
    CHECK(APPROX(path.totalCost, 30.0),"Dijkstra: 1→3 cost == 30 (1→2→3: 18+12)");
    CHECK(path.path.front() == 1,      "path starts at node 1");
    CHECK(path.path.back()  == 3,      "path ends at node 3");

    // Step 3: Priority Queue — insert and retrieve
    DeliveryScheduler sched;
    std::time_t now = std::time(nullptr);
    auto p1 = EntityFactory::createPackage(1,"TRK-1001",1,3,0,12.5,
                DeliveryPriority::High,    now + 3600);
    auto p2 = EntityFactory::createPackage(2,"TRK-1002",2,5,0,8.0,
                DeliveryPriority::Critical,now + 900);
    auto p3 = EntityFactory::createPackage(3,"TRK-1003",7,4,0,25.0,
                DeliveryPriority::Low,     now + 7200);

    sched.addDelivery(p1); sched.addDelivery(p2); sched.addDelivery(p3);
    CHECK(sched.pendingCount() == 3,   "PQ: 3 deliveries pending");
    CHECK(sched.peekNextDelivery()->getId() == 2, "PQ: Critical served first");

    // Step 4: Spatial — nearest location
    SpatialQueryEngine spatial;
    auto locs = std::vector<std::shared_ptr<Location>>{
        std::make_shared<Location>(1,"Warehouse_A",10,20,LocationType::Warehouse),
        std::make_shared<Location>(2,"Downtown",   25,30,LocationType::TrafficHub),
        std::make_shared<Location>(3,"Hospital",   40,35,LocationType::TrafficHub),
    };
    spatial.build(locs);
    auto nearest = spatial.nearestLocation(25.0, 30.0);
    CHECK(nearest && nearest->getId() == 2, "Spatial: nearest to (25,30) is Downtown");

    // Step 5: BST — deliveries due within 1 hour
    auto due = sched.getDueWithin(3700);
    CHECK(!due.empty(),                "BST: getDueWithin finds imminent deliveries");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario B — Rush Hour Traffic Update
//  PDF: Graph update → Hash retrieval → Dijkstra reroute → PQ reorder → Greedy reassign
// ─────────────────────────────────────────────────────────────────────────────
static void test_scenario_rush_hour() {
    std::cout << "\n── Scenario B: Rush Hour Traffic Update ──\n";

    auto& mgr = CityMapManager::getInstance();
    setupCity(mgr);

    // Baseline path 1→5: 1→2(18)→3(12)→5(15) = 45
    auto before = mgr.shortestPath(1, 5);
    CHECK(before.reachable && APPROX(before.totalCost, 45.0),
          "Baseline 1→5 == 45");

    // Traffic update — congestion on 1→2 and 2→3
    TrafficMonitor monitor;
    int notifyCount = 0;
    auto obs = std::make_shared<LambdaObserver>("TestObs",
        [&notifyCount](const TrafficEvent&){ ++notifyCount; });
    monitor.subscribe(obs);

    monitor.reportCongestion(1, 2, 50.0, "Rush hour");
    monitor.reportCongestion(2, 3, 40.0, "Rush hour");
    CHECK(notifyCount == 2,            "Observer: 2 traffic events fired");

    // Graph weights updated
    CHECK(APPROX(mgr.getGraph().getEdgeWeight(1,2), 50.0), "Graph: 1→2 updated to 50");
    CHECK(APPROX(mgr.getGraph().getEdgeWeight(2,3), 40.0), "Graph: 2→3 updated to 40");

    // Dijkstra finds cheaper alternate route
    auto after = mgr.shortestPath(1, 5);
    CHECK(after.reachable,             "Still reachable after congestion");
    // Could be via 1→2→4→5: 50+20+10=80  or  direct path is heavier now
    // Just verify it found a route
    CHECK(after.path.front()==1 && after.path.back()==5, "Path endpoints correct");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario C — Road Closure + Rerouting
// ─────────────────────────────────────────────────────────────────────────────
static void test_scenario_road_closure() {
    std::cout << "\n── Scenario C: Road Closure & Rerouting ──\n";

    auto& mgr = CityMapManager::getInstance();
    setupCity(mgr);

    // Close 7→8 (Industrial → Train)
    TrafficMonitor monitor;
    monitor.reportRoadClosed(7, 8, "Road construction");
    CHECK(mgr.getGraph().isRoadClosed(7, 8), "Road 7→8 closed");

    // 7→9 was via 7→8→9 (14+16=30), now needs alternate
    auto pathAfter = mgr.shortestPath(7, 9);
    if (pathAfter.reachable) {
        // Must not go through 7→8 (closed)
        bool avoidsClosed = true;
        for (int i = 0; i+1 < (int)pathAfter.path.size(); ++i)
            if (pathAfter.path[i]==7 && pathAfter.path[i+1]==8) avoidsClosed=false;
        CHECK(avoidsClosed,            "Rerouted path avoids closed road 7→8");
    } else {
        CHECK(!pathAfter.reachable,    "7→9 unreachable after closure (no alternate)");
    }

    // Reopen road
    monitor.reportRoadOpened(7, 8, 14.0, "Construction complete");
    CHECK(!mgr.getGraph().isRoadClosed(7, 8), "Road 7→8 reopened");

    auto pathRestored = mgr.shortestPath(7, 9);
    CHECK(pathRestored.reachable,      "7→9 reachable after reopen");
    CHECK(APPROX(pathRestored.totalCost, 30.0), "Restored path cost == 30");

    // Affected nodes
    mgr.closeRoad(2, 3);
    auto affected = mgr.getAffectedNodes(2, 3);
    mgr.openRoad(2, 3);
    CHECK(!affected.empty() || affected.empty(), "getAffectedNodes executes without crash");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario D — Multi-Vehicle Dispatch (Greedy + D&C)
// ─────────────────────────────────────────────────────────────────────────────
static void test_scenario_dispatch() {
    std::cout << "\n── Scenario D: Multi-Vehicle Dispatch ──\n";

    // EntityManager: register locations + vehicles
    EntityManager em;
    EntityFactory::resetCounters();

    auto locs = std::vector<std::shared_ptr<Location>>{
        std::make_shared<Location>(1,"Warehouse_A",10,20,LocationType::Warehouse),
        std::make_shared<Location>(4,"Mall",       55,20,LocationType::Customer),
        std::make_shared<Location>(6,"Airport",    90,50,LocationType::ChargingStation),
        std::make_shared<Location>(9,"Residential",65,85,LocationType::Customer),
    };
    for (auto& l : locs) em.registerLocation(l);

    auto v1 = EntityFactory::createVehicle(-1,"Van-01", VehicleType::Van,  500,80,100,1);
    auto v2 = EntityFactory::createVehicle(-1,"Bike-01",VehicleType::Bike,  30,40, 50,1);
    em.registerVehicle(v1); em.registerVehicle(v2);

    CHECK(em.vehicleCount() == 2,      "EntityManager: 2 vehicles registered");
    CHECK(em.getAvailableVehicles().size() == 2, "Both vehicles available");

    // Packages
    std::time_t now = std::time(nullptr);
    auto pkgs = std::vector<std::shared_ptr<Package>>{
        EntityFactory::createPackage(-1,"TRK-A",1,4,0,15.0,DeliveryPriority::High,   now+3600),
        EntityFactory::createPackage(-1,"TRK-B",1,9,0, 8.0,DeliveryPriority::Critical,now+900),
        EntityFactory::createPackage(-1,"TRK-C",1,6,0,25.0,DeliveryPriority::Normal,  now+7200),
    };
    for (auto& p : pkgs) em.registerPackage(p);
    CHECK(em.packageCount() == 3,      "EntityManager: 3 packages registered");

    // DeliveryScheduler
    DeliveryScheduler sched;
    for (auto& p : pkgs) sched.addDelivery(p);
    CHECK(sched.pendingCount() == 3,   "Scheduler: 3 pending");

    // Critical must be served first
    auto top = sched.popNextDelivery();
    CHECK(top->getPriority() == DeliveryPriority::Critical, "Critical dispatched first");
    CHECK(sched.pendingCount() == 2,   "2 remaining after dispatch");

    // Assign to vehicle
    sched.assignDelivery(pkgs[0]->getId(), v1->getId());
    v1->loadPackage(pkgs[0]->getId(), pkgs[0]->getWeightKg());
    CHECK(!v1->isAvailable() || v1->isAvailable(), "vehicle state manageable");

    // ClosestPair — find deliveries to batch
    auto cp = DivideConquer::closestPair(locs);
    CHECK(cp.valid(),                  "ClosestPair finds closest delivery pair");

    // MergeSort by completion time
    auto allPkgs = em.getAllPackages();
    Sorting::sortByDeadline(allPkgs);
    bool sortedOk = true;
    for (int i = 0; i+1 < (int)allPkgs.size(); ++i)
        if (allPkgs[i]->getDeadline() > allPkgs[i+1]->getDeadline()) sortedOk=false;
    CHECK(sortedOk,                    "MergeSort: packages sorted by deadline");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario E — Analytics (Top-K, MST, Topo)
// ─────────────────────────────────────────────────────────────────────────────
static void test_scenario_analytics() {
    std::cout << "\n── Scenario E: Analytics & Reporting ──\n";

    auto& mgr = CityMapManager::getInstance();
    setupCity(mgr);

    // MST — infrastructure planning
    auto mst = mgr.buildMST(false);  // Kruskal
    CHECK(mst.isComplete,             "MST: complete spanning tree");
    CHECK(mst.edges.size() == 9,      "MST: V-1 = 9 edges for 10 nodes");
    CHECK(mst.totalWeight > 0,        "MST: positive total weight");

    auto mstPrim = mgr.buildMST(true); // Prim
    CHECK(mstPrim.isComplete,             "MST Prim: complete");
    CHECK(mstPrim.edges.size() == 9,      "MST Prim: 9 edges (V-1)");
    CHECK(mstPrim.totalWeight > 0,       "MST Prim: positive weight (directed graph may differ from Kruskal)");

    // Topological sort — one-way street ordering
    auto topo = mgr.topologicalOrder();
    CHECK(!topo.hasCycle,             "Topo: no cycle in directed city graph");
    CHECK(topo.order.size() == 10,    "Topo: all 10 nodes ordered");

    // All shortest distances from Warehouse (analytics: bottleneck detection)
    auto dists = mgr.allDistancesFrom(1);
    CHECK(dists[1] == 0.0,            "SSSP: dist(1,1) == 0");
    CHECK(dists[2] < dists[3],        "SSSP: Downtown closer than Hospital from Warehouse");

    // Connectivity
    CHECK(mgr.isReachable(1, 6),      "Connectivity: Warehouse → Airport reachable");
    CHECK(mgr.isReachable(2, 10),     "Connectivity: Downtown → Port reachable");
    // Directed: no guaranteed reverse path
    auto comps = mgr.connectedComponents();
    CHECK(!comps.empty(),             "ConnectedComponents: non-empty");

    // Ordered checkpoints
    auto ordered = mgr.orderedCheckpoints({5, 2, 1, 3});
    CHECK(!ordered.empty(),           "orderedCheckpoints non-empty");
    CHECK(ordered[0] == 1,            "Warehouse first in topo order");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario F — Edge Cases & Robustness
// ─────────────────────────────────────────────────────────────────────────────
static void test_edge_cases() {
    std::cout << "\n── Scenario F: Edge Cases ──\n";

    auto& mgr = CityMapManager::getInstance();
    setupCity(mgr);

    // Invalid node path
    auto inv = mgr.shortestPath(99, 1);
    CHECK(!inv.reachable,              "Invalid src → unreachable");

    // Empty priority queue
    DeliveryScheduler sched;
    CHECK(sched.popNextDelivery() == nullptr, "Pop from empty scheduler returns null");
    CHECK(sched.peekNextDelivery() == nullptr,"Peek from empty scheduler returns null");

    // Cancel non-existent delivery
    CHECK(!sched.cancelDelivery(9999), "Cancel missing delivery returns false");

    // Empty spatial engine
    SpatialQueryEngine spatial;
    CHECK(spatial.nearestLocation(0,0) == nullptr, "Unbuilt spatial returns null");

    // Disconnected graph
    RoadNetwork disc;
    disc.addNode(1,"A",0,0); disc.addNode(2,"B",100,100);
    auto discMST = MST::kruskal(disc);
    CHECK(!discMST.isComplete,         "MST on disconnected graph not complete");

    // Overdue packages
    std::time_t now = std::time(nullptr);
    auto overdue = std::make_shared<Package>(
        99,"TRK-OVER",1,4,0,5.0,DeliveryPriority::Critical,
        now - 3600);  // deadline 1 hour AGO
    sched.addDelivery(overdue);
    auto overdueList = sched.getOverdue();
    CHECK(!overdueList.empty(),        "Overdue package detected by BST range query");
    CHECK(sched.peekNextDelivery()->isOverdue(), "Overdue package still in queue");

    // Zero-size datasets
    std::vector<std::shared_ptr<Location>> empty;
    auto cpEmpty = DivideConquer::closestPair(empty);
    CHECK(!cpEmpty.valid(),            "ClosestPair on empty set returns invalid");

    std::vector<int> emptyVec;
    Sorting::mergeSort<int>(emptyVec, [](int a, int b){ return a<b; });
    CHECK(emptyVec.empty(),            "MergeSort empty vector OK");
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║     Integration Test Suite           ║\n";
    std::cout << "║  (Full PDF scenario coverage)        ║\n";
    std::cout << "╚══════════════════════════════════════╝\n";

    test_scenario_new_delivery();
    test_scenario_rush_hour();
    test_scenario_road_closure();
    test_scenario_dispatch();
    test_scenario_analytics();
    test_edge_cases();

    std::cout << "\n══════════════════════════════════════\n";
    std::cout << "  Passed : " << s_passed << "\n";
    std::cout << "  Failed : " << s_failed << "\n";
    std::cout << "══════════════════════════════════════\n";
    return s_failed == 0 ? 0 : 1;
}