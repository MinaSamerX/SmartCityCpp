#pragma once
// src/system/EntityManager.h
// Smart City Delivery & Traffic Management System
//
// ── Responsibilities ──────────────────────────────────────────────────────────
// EntityManager is the O(1) lookup registry for every domain object.
// It wraps our custom HashTable<K,V> for Locations, Vehicles, Packages,
// and Customers — exactly as specified in the PDF ("Fast Lookup System").
//
// Design decisions:
//  - Separate HashTable per entity type (different key types + hash fns)
//  - Locations:  HashTableOA<int, shared_ptr> — int IDs, cache-friendly OA
//  - Vehicles:   HashTableOA<int, shared_ptr>
//  - Packages:   HashTable<int, shared_ptr>   — chaining (more entries)
//  - Customers:  HashTable<int, shared_ptr>
//  - Tracking#:  HashTable<string, int>       — string key → package ID
//
// EntityManager is NOT a Singleton — SmartCitySystem owns one instance.

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "../core/Location.h"
#include "../core/Vehicle.h"
#include "../core/Package.h"
#include "../core/Customer.h"
#include "../hashtable/HashTable.h"
#include "../hashtable/HashTableOA.h"
#include "../hashtable/HashFunctions.h"
#include <iostream>

class EntityManager {
public:
    EntityManager()  = default;
    ~EntityManager() = default;

    // Non-copyable (owns shared_ptrs — copies would be ambiguous)
    EntityManager(const EntityManager&)            = delete;
    EntityManager& operator=(const EntityManager&) = delete;
    EntityManager(EntityManager&&)                 = default;
    EntityManager& operator=(EntityManager&&)      = default;

    // ── Locations ─────────────────────────────────────────────────────────────
    void registerLocation(std::shared_ptr<Location> loc);
    std::shared_ptr<Location>              getLocation(int id)  const;
    bool                                   hasLocation(int id)  const;
    bool                                   removeLocation(int id);
    std::vector<std::shared_ptr<Location>> getAllLocations()    const;

    // ── Vehicles ──────────────────────────────────────────────────────────────
    void registerVehicle(std::shared_ptr<Vehicle> v);
    std::shared_ptr<Vehicle>              getVehicle(int id)   const;
    bool                                  hasVehicle(int id)   const;
    bool                                  removeVehicle(int id);
    std::vector<std::shared_ptr<Vehicle>> getAllVehicles()      const;
    std::vector<std::shared_ptr<Vehicle>> getAvailableVehicles() const;

    // ── Packages ──────────────────────────────────────────────────────────────
    void registerPackage(std::shared_ptr<Package> pkg);
    std::shared_ptr<Package>              getPackage(int id)           const;
    std::shared_ptr<Package>              getPackageByTracking(
                                              const std::string& trackingNo) const;
    bool                                  hasPackage(int id)           const;
    bool                                  removePackage(int id);
    std::vector<std::shared_ptr<Package>> getAllPackages()              const;
    std::vector<std::shared_ptr<Package>> getPendingPackages()          const;

    // ── Customers ─────────────────────────────────────────────────────────────
    void registerCustomer(std::shared_ptr<Customer> c);
    std::shared_ptr<Customer>              getCustomer(int id) const;
    bool                                   hasCustomer(int id) const;
    bool                                   removeCustomer(int id);
    std::vector<std::shared_ptr<Customer>> getAllCustomers()   const;

    // ── Bulk registration (from FileParser results) ────────────────────────────
    void registerAll(const std::vector<std::shared_ptr<Location>>& locs,
                     const std::vector<std::shared_ptr<Vehicle>>&  vehs,
                     const std::vector<std::shared_ptr<Package>>&  pkgs);

    // ── Metrics ───────────────────────────────────────────────────────────────
    int locationCount() const;
    int vehicleCount()  const;
    int packageCount()  const;
    int customerCount() const;

    void printStats(std::ostream& os = std::cout) const;
    void clear();

private:
    // Open-addressing for int-keyed, frequently-hit tables
    HashTableOA<int, std::shared_ptr<Location>, HashFn::IntHash> m_locations;
    HashTableOA<int, std::shared_ptr<Vehicle>,  HashFn::IntHash> m_vehicles;

    // Chaining for packages (many entries, string secondary key)
    HashTable<int, std::shared_ptr<Package>,  HashFn::IntHash>   m_packages;
    HashTable<int, std::shared_ptr<Customer>, HashFn::IntHash>   m_customers;

    // Secondary index: tracking number → package ID  (O(1) lookup by TRK-xxx)
    HashTable<std::string, int, HashFn::StringHash>              m_trackingIndex;
};