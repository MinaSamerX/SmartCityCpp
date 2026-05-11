// src/system/SmartCitySystem.cpp
// Smart City Delivery & Traffic Management System
//
// ── DESIGN PATTERN: Facade ───────────────────────────────────────────────────
// SmartCitySystem wires all six sub-components together and exposes
// the two main PDF scenarios as single method calls.

#include "SmartCitySystem.h"
#include <iostream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────
SmartCitySystem::SmartCitySystem() {
    connectObservers();
}

// Wire DeliveryScheduler as a traffic observer so it auto-reorders
// when TrafficMonitor fires events.
void SmartCitySystem::connectObservers() {
    // DeliveryScheduler implements ITrafficObserver — share it safely
    // We keep a shared_ptr wrapper that does NOT own the object
    // (custom no-op deleter) so TrafficMonitor's weak_ptr stays valid.
    auto schedObs = std::shared_ptr<ITrafficObserver>(
        &m_scheduler, [](ITrafficObserver*){} // no-op deleter
    );
    m_traffic.subscribe(schedObs);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Initialisation
// ─────────────────────────────────────────────────────────────────────────────
bool SmartCitySystem::loadFromDirectory(const std::string& dataDir) {
    std::string sep = dataDir.empty() ? "" : "/";
    std::vector<std::string> errors;

    // 1. Load city graph into CityMapManager (Singleton)
    auto& mapMgr = CityMapManager::getInstance();
    mapMgr.reset();
    bool ok = mapMgr.loadFromFile(dataDir + sep + "city_map.txt");
    if (!ok) {
        std::cerr << "[SmartCitySystem] Failed to load city_map.txt\n";
        return false;
    }
    std::cout << "[SmartCitySystem] Graph loaded: "
              << mapMgr.nodeCount() << " nodes, "
              << mapMgr.edgeCount() << " edges\n";

    // 2. Parse and register all entities
    auto locs = FileParser::parseLocations(dataDir + sep + "locations.txt",  errors);
    auto vehs = FileParser::parseVehicles( dataDir + sep + "vehicles.txt",   errors);
    auto pkgs = FileParser::parseDeliveries(dataDir + sep + "deliveries.txt",errors);

    m_entities.registerAll(locs, vehs, pkgs);
    std::cout << "[SmartCitySystem] Entities: "
              << m_entities.locationCount() << " locations, "
              << m_entities.vehicleCount()  << " vehicles, "
              << m_entities.packageCount()  << " packages\n";

    // 3. Build spatial index from loaded locations
    rebuildSpatialIndex();

    // 4. Populate scheduler with all pending packages
    for (auto& p : m_entities.getPendingPackages())
        m_scheduler.addDelivery(p);
    std::cout << "[SmartCitySystem] Scheduler: "
              << m_scheduler.pendingCount() << " pending deliveries\n";

    // 5. Apply any initial traffic updates
    auto updates = FileParser::parseTrafficUpdates(
        dataDir + sep + "traffic_updates.txt", errors);
    if (!updates.empty()) {
        mapMgr.applyTrafficUpdates(updates);
        std::cout << "[SmartCitySystem] Applied "
                  << updates.size() << " traffic updates\n";
    }

    if (!errors.empty()) {
        std::cerr << "[SmartCitySystem] Parse warnings:\n";
        for (auto& e : errors) std::cerr << "  " << e << "\n";
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario 1 — New Delivery Request
//  PDF flow: Hash → Spatial → PriorityQueue → Graph → Greedy
// ─────────────────────────────────────────────────────────────────────────────
bool SmartCitySystem::processNewDelivery(
    int              packageId,
    int              srcLocationId,
    int              destLocationId,
    int              customerId,
    double           weightKg,
    DeliveryPriority priority,
    std::time_t      deadline)
{
    std::cout << "\n[Scenario 1] Processing delivery id=" << packageId << "\n";

    // Step 1 — Hash lookup: verify customer + locations exist
    // customerId == 0 means anonymous delivery (no customer record required)
    if (customerId > 0 && !m_entities.hasCustomer(customerId)) {
        std::cerr << "  [ERR] Customer " << customerId << " not found\n";
        return false;
    }
    auto& mapMgr = CityMapManager::getInstance();
    if (!mapMgr.hasNode(srcLocationId) || !mapMgr.hasNode(destLocationId)) {
        std::cerr << "  [ERR] Source or destination location not in graph\n";
        return false;
    }

    // Step 2 — Factory: create package and register
    auto pkg = EntityFactory::createPackage(
        packageId, "", srcLocationId, destLocationId,
        customerId, weightKg, priority, deadline);
    m_entities.registerPackage(pkg);

    // Step 3 — PriorityQueue: insert delivery
    m_scheduler.addDelivery(pkg);
    std::cout << "  [+] Package " << packageId
              << " queued (priority=" << static_cast<int>(priority) << ")\n";

    // Step 4 — Spatial: find nearest available vehicle
    auto srcLoc = mapMgr.getLocation(srcLocationId);
    double qx = srcLoc ? srcLoc->getX() : 0.0;
    double qy = srcLoc ? srcLoc->getY() : 0.0;

    auto vehicles = m_entities.getAvailableVehicles();
    auto coordsFn = makeCoordsFn();
    auto vehicle  = m_spatial.nearestAvailableVehicle(qx, qy, vehicles, coordsFn);

    if (!vehicle) {
        std::cout << "  [!] No available vehicle — delivery queued\n";
        return true;   // delivery is queued; will be assigned at dispatchFleet
    }
    std::cout << "  [+] Nearest vehicle: " << vehicle->getName() << "\n";

    // Step 5 — Graph: Dijkstra vehicle_loc → pickup → destination
    int vLocId    = vehicle->getCurrentLocationId();
    auto toPickup = mapMgr.shortestPath(vLocId, srcLocationId);
    auto toDestP  = mapMgr.shortestPath(srcLocationId, destLocationId);

    if (!toPickup.reachable || !toDestP.reachable) {
        std::cerr << "  [ERR] Route unreachable\n";
        return false;
    }
    std::cout << "  [+] Route: vehicle_loc->" << srcLocationId
              << " cost=" << toPickup.totalCost
              << " | " << srcLocationId << "->" << destLocationId
              << " cost=" << toDestP.totalCost << "\n";

    // Step 6 — Assign delivery
    if (vehicle->canLoad(weightKg)) {
        m_scheduler.assignDelivery(packageId, vehicle->getId());
        vehicle->loadPackage(packageId, weightKg);
        vehicle->setStatus(VehicleStatus::EnRoute);
        std::cout << "  [+] Assigned to " << vehicle->getName() << "\n";
    } else {
        std::cout << "  [!] Vehicle at capacity — delivery queued\n";
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scenario 2 — Traffic Updates
//  PDF flow: Graph → TrafficMonitor → Observer(Scheduler) → Reroute
// ─────────────────────────────────────────────────────────────────────────────
void SmartCitySystem::handleTrafficUpdate(int fromId, int toId,
                                           double newWeight,
                                           const std::string& desc) {
    std::cout << "\n[Scenario 2] Traffic update: "
              << fromId << "->" << toId
              << " new_weight=" << newWeight << "\n";
    // TrafficMonitor updates graph + notifies DeliveryScheduler (Observer)
    m_traffic.reportCongestion(fromId, toId, newWeight, desc);
}

void SmartCitySystem::handleRoadClosure(int fromId, int toId,
                                         const std::string& desc) {
    std::cout << "\n[Scenario 2] Road closure: "
              << fromId << "->" << toId << "\n";
    m_traffic.reportRoadClosed(fromId, toId, desc);
}

void SmartCitySystem::handleAccident(int fromId, int toId,
                                      const std::string& desc) {
    std::cout << "\n[Scenario 2] Accident: "
              << fromId << "->" << toId << "\n";
    m_traffic.reportAccident(fromId, toId, desc);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Fleet dispatch
//  Assign all pending deliveries to vehicles and generate optimised routes.
// ─────────────────────────────────────────────────────────────────────────────
std::vector<VehicleRoute> SmartCitySystem::dispatchFleet() {
    std::cout << "\n[Fleet Dispatch] pending="
              << m_scheduler.pendingCount() << "\n";

    auto vehicles = m_entities.getAllVehicles();
    auto packages = m_scheduler.getAllPending();

    // RouteOptimizer: D&C zone partition → greedy assignment → TSP ordering
    auto coordsFn = makeCoordsFn();
    m_lastRoutes  = m_optimizer.optimiseFleet(vehicles, packages, coordsFn);

    // Update entity states from routes
    for (auto& route : m_lastRoutes) {
        if (!route.vehicle) continue;
        for (auto& pkg : route.assignedPackages) {
            m_scheduler.assignDelivery(pkg->getId(), route.vehicle->getId());
            route.vehicle->loadPackage(pkg->getId(), pkg->getWeightKg());
        }
        if (!route.assignedPackages.empty())
            route.vehicle->setStatus(VehicleStatus::Delivering);
    }

    std::cout << "[Fleet Dispatch] Generated "
              << m_lastRoutes.size() << " routes\n";
    return m_lastRoutes;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Queries
// ─────────────────────────────────────────────────────────────────────────────
std::shared_ptr<Location>
SmartCitySystem::findNearestLocation(double x, double y) const {
    return m_spatial.nearestLocation(x, y);
}

std::shared_ptr<Vehicle>
SmartCitySystem::findNearestVehicle(double x, double y) const {
    auto vehicles = m_entities.getAvailableVehicles();
    return m_spatial.nearestAvailableVehicle(x, y, vehicles, makeCoordsFn());
}

PathResult SmartCitySystem::shortestPath(int src, int dest) const {
    return CityMapManager::getInstance().shortestPath(src, dest);
}

std::vector<std::shared_ptr<Package>>
SmartCitySystem::getPendingDeliveries() const {
    return m_scheduler.getAllPending();
}

std::vector<std::shared_ptr<Package>>
SmartCitySystem::getOverdueDeliveries() const {
    return m_scheduler.getOverdue();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Reporting
// ─────────────────────────────────────────────────────────────────────────────
void SmartCitySystem::printReport(const std::vector<VehicleRoute>& routes,
                                   std::ostream& os) const {
    auto& mapMgr = CityMapManager::getInstance();
    Reporter::printFleetRoutes(routes, os);
    Reporter::printMetrics(routes, os);

    auto allPkgs = m_entities.getAllPackages();
    Reporter::printDeliverySchedule(allPkgs, os);
    Reporter::printBottleneckAnalysis(mapMgr.getGraph(), m_traffic, 5, os);

    auto unserved = m_scheduler.getAllPending();
    Reporter::printUnserved(unserved, routes, os);
}

void SmartCitySystem::printStatus(std::ostream& os) const {
    auto& mapMgr = CityMapManager::getInstance();
    os << "\n========================================\n";
    os << "  Smart City System — Status\n";
    os << "========================================\n";
    os << "  Graph       : " << mapMgr.nodeCount() << " nodes, "
                             << mapMgr.edgeCount() << " edges\n";
    os << "  Algorithm   : " << mapMgr.currentAlgorithm() << "\n";
    m_entities.printStats(os);
    os << "  Pending jobs: " << m_scheduler.pendingCount() << "\n";
    os << "  Traffic evts: " << m_traffic.getHistory().size() << "\n";
    os << "  Observers   : " << m_traffic.observerCount() << "\n";
    os << "========================================\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────────────────────────────────────
std::function<std::pair<double,double>(int)>
SmartCitySystem::makeCoordsFn() const {
    // Capture a pointer to the map manager graph for location lookup.
    // Lambda used to inject spatial dependency without coupling types.
    return [this](int locationId) -> std::pair<double,double> {
        auto loc = CityMapManager::getInstance().getLocation(locationId);
        if (!loc) {
            // Fallback: try EntityManager
            loc = m_entities.getLocation(locationId);
        }
        if (loc) return {loc->getX(), loc->getY()};
        return {0.0, 0.0};
    };
}

void SmartCitySystem::rebuildSpatialIndex() {
    // Merge locations from both graph and EntityManager
    std::vector<std::shared_ptr<Location>> allLocs;

    auto& mapMgr = CityMapManager::getInstance();
    for (int id : mapMgr.getAllNodeIds()) {
        auto l = mapMgr.getLocation(id);
        if (l) allLocs.push_back(l);
    }
    // Add any registered locations not in graph
    for (auto& l : m_entities.getAllLocations()) {
        if (!mapMgr.hasNode(l->getId()))
            allLocs.push_back(l);
    }

    m_spatial.build(allLocs);
    std::cout << "[SmartCitySystem] Spatial index built: "
              << allLocs.size() << " locations\n";
}