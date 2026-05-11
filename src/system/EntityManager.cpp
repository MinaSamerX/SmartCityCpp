// src/system/EntityManager.cpp
// Smart City Delivery & Traffic Management System

#include "EntityManager.h"
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
//  Locations
// ─────────────────────────────────────────────────────────────────────────────
void EntityManager::registerLocation(std::shared_ptr<Location> loc) {
    if (!loc) return;
    m_locations.insert(loc->getId(), loc);
}

std::shared_ptr<Location> EntityManager::getLocation(int id) const {
    const auto* ptr = m_locations.search(id);
    return ptr ? *ptr : nullptr;
}

bool EntityManager::hasLocation(int id)  const { return m_locations.contains(id); }
bool EntityManager::removeLocation(int id)      { return m_locations.remove(id);  }

std::vector<std::shared_ptr<Location>> EntityManager::getAllLocations() const {
    std::vector<std::shared_ptr<Location>> result;
    m_locations.forEach([&result](const int&, const std::shared_ptr<Location>& l) {
        result.push_back(l);
    });
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Vehicles
// ─────────────────────────────────────────────────────────────────────────────
void EntityManager::registerVehicle(std::shared_ptr<Vehicle> v) {
    if (!v) return;
    m_vehicles.insert(v->getId(), v);
}

std::shared_ptr<Vehicle> EntityManager::getVehicle(int id) const {
    const auto* ptr = m_vehicles.search(id);
    return ptr ? *ptr : nullptr;
}

bool EntityManager::hasVehicle(int id)  const { return m_vehicles.contains(id); }
bool EntityManager::removeVehicle(int id)      { return m_vehicles.remove(id);  }

std::vector<std::shared_ptr<Vehicle>> EntityManager::getAllVehicles() const {
    std::vector<std::shared_ptr<Vehicle>> result;
    m_vehicles.forEach([&result](const int&, const std::shared_ptr<Vehicle>& v) {
        result.push_back(v);
    });
    return result;
}

std::vector<std::shared_ptr<Vehicle>> EntityManager::getAvailableVehicles() const {
    std::vector<std::shared_ptr<Vehicle>> result;
    m_vehicles.forEach([&result](const int&, const std::shared_ptr<Vehicle>& v) {
        if (v->isAvailable()) result.push_back(v);
    });
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Packages
// ─────────────────────────────────────────────────────────────────────────────
void EntityManager::registerPackage(std::shared_ptr<Package> pkg) {
    if (!pkg) return;
    m_packages.insert(pkg->getId(), pkg);
    m_trackingIndex.insert(pkg->getTrackingNumber(), pkg->getId());
}

std::shared_ptr<Package> EntityManager::getPackage(int id) const {
    const auto* ptr = m_packages.search(id);
    return ptr ? *ptr : nullptr;
}

std::shared_ptr<Package> EntityManager::getPackageByTracking(
    const std::string& trackingNo) const {
    const auto* idPtr = m_trackingIndex.search(trackingNo);
    if (!idPtr) return nullptr;
    return getPackage(*idPtr);
}

bool EntityManager::hasPackage(int id)  const { return m_packages.contains(id); }

bool EntityManager::removePackage(int id) {
    auto pkg = getPackage(id);
    if (!pkg) return false;
    m_trackingIndex.remove(pkg->getTrackingNumber());
    return m_packages.remove(id);
}

std::vector<std::shared_ptr<Package>> EntityManager::getAllPackages() const {
    std::vector<std::shared_ptr<Package>> result;
    m_packages.forEach([&result](const int&, const std::shared_ptr<Package>& p) {
        result.push_back(p);
    });
    return result;
}

std::vector<std::shared_ptr<Package>> EntityManager::getPendingPackages() const {
    std::vector<std::shared_ptr<Package>> result;
    m_packages.forEach([&result](const int&, const std::shared_ptr<Package>& p) {
        if (p->isPending()) result.push_back(p);
    });
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Customers
// ─────────────────────────────────────────────────────────────────────────────
void EntityManager::registerCustomer(std::shared_ptr<Customer> c) {
    if (!c) return;
    m_customers.insert(c->getId(), c);
}

std::shared_ptr<Customer> EntityManager::getCustomer(int id) const {
    const auto* ptr = m_customers.search(id);
    return ptr ? *ptr : nullptr;
}

bool EntityManager::hasCustomer(int id)  const { return m_customers.contains(id); }
bool EntityManager::removeCustomer(int id)      { return m_customers.remove(id);  }

std::vector<std::shared_ptr<Customer>> EntityManager::getAllCustomers() const {
    std::vector<std::shared_ptr<Customer>> result;
    m_customers.forEach([&result](const int&, const std::shared_ptr<Customer>& c) {
        result.push_back(c);
    });
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bulk registration
// ─────────────────────────────────────────────────────────────────────────────
void EntityManager::registerAll(
    const std::vector<std::shared_ptr<Location>>& locs,
    const std::vector<std::shared_ptr<Vehicle>>&  vehs,
    const std::vector<std::shared_ptr<Package>>&  pkgs)
{
    for (auto& l : locs) registerLocation(l);
    for (auto& v : vehs) registerVehicle(v);
    for (auto& p : pkgs) registerPackage(p);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Metrics
// ─────────────────────────────────────────────────────────────────────────────
int EntityManager::locationCount() const {
    return static_cast<int>(m_locations.size());
}
int EntityManager::vehicleCount()  const {
    return static_cast<int>(m_vehicles.size());
}
int EntityManager::packageCount()  const {
    return static_cast<int>(m_packages.size());
}
int EntityManager::customerCount() const {
    return static_cast<int>(m_customers.size());
}

void EntityManager::printStats(std::ostream& os) const {
    os << "EntityManager stats:\n";
    os << "  Locations : " << locationCount() << "\n";
    os << "  Vehicles  : " << vehicleCount()  << "\n";
    os << "  Packages  : " << packageCount()  << "\n";
    os << "  Customers : " << customerCount() << "\n";
    m_locations.printStats(os);
    m_packages.printStats(os);
}

void EntityManager::clear() {
    m_locations.clear();
    m_vehicles.clear();
    m_packages.clear();
    m_customers.clear();
    m_trackingIndex.clear();
}