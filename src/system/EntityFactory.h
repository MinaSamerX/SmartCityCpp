#pragma once
// src/system/EntityFactory.h
// Smart City Delivery & Traffic Management System
//
// ── DESIGN PATTERN: Factory ───────────────────────────────────────────────────
// EntityFactory centralises object construction.
// All shared_ptr<T> domain objects are created here — callers never
// call make_shared directly, so construction logic lives in one place.
// This makes it trivial to add validation, ID generation, or logging
// without touching every call site.

#include <memory>
#include <string>
#include <atomic>
#include "../core/Location.h"
#include "../core/Vehicle.h"
#include "../core/Package.h"
#include "../core/Customer.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Class: EntityFactory
//  Purely static factory — no instances needed.
//  Auto-generates unique IDs when caller passes id = -1.
// ─────────────────────────────────────────────────────────────────────────────
class EntityFactory {
public:
    EntityFactory()  = delete;   // static-only

    // ── Location ─────────────────────────────────────────────────────────────
    static std::shared_ptr<Location> createLocation(
        int         id,
        std::string name,
        double      x,
        double      y,
        LocationType type = LocationType::Intersection)
    {
        if (id < 0) id = nextLocationId();
        return std::make_shared<Location>(id, std::move(name), x, y, type);
    }

    // ── Vehicle ──────────────────────────────────────────────────────────────
    static std::shared_ptr<Vehicle> createVehicle(
        int         id,
        std::string name,
        VehicleType type,
        double      capacityKg,
        double      speedKmh,
        double      batteryKwh,
        int         startLocationId)
    {
        if (id < 0) id = nextVehicleId();
        return std::make_shared<Vehicle>(
            id, std::move(name), type,
            capacityKg, speedKmh, batteryKwh, startLocationId);
    }

    // ── Package ───────────────────────────────────────────────────────────────
    static std::shared_ptr<Package> createPackage(
        int              id,
        std::string      trackingNumber,
        int              srcLocationId,
        int              destLocationId,
        int              customerId,
        double           weightKg,
        DeliveryPriority priority,
        std::time_t      deadlineEpoch)
    {
        if (id < 0) id = nextPackageId();
        if (trackingNumber.empty())
            trackingNumber = "TRK-" + std::to_string(id);
        return std::make_shared<Package>(
            id, std::move(trackingNumber),
            srcLocationId, destLocationId,
            customerId, weightKg, priority, deadlineEpoch);
    }

    // ── Customer ──────────────────────────────────────────────────────────────
    static std::shared_ptr<Customer> createCustomer(
        int          id,
        std::string  name,
        std::string  phone,
        std::string  email,
        int          defaultLocationId,
        CustomerTier tier = CustomerTier::Standard)
    {
        if (id < 0) id = nextCustomerId();
        return std::make_shared<Customer>(
            id, std::move(name), std::move(phone),
            std::move(email), defaultLocationId, tier);
    }

    // ── ID counters (thread-safe with atomic) ─────────────────────────────────
    static int nextLocationId() { return ++s_locId;  }
    static int nextVehicleId()  { return ++s_vehId;  }
    static int nextPackageId()  { return ++s_pkgId;  }
    static int nextCustomerId() { return ++s_custId; }

    // Reset counters (useful for unit tests)
    static void resetCounters() {
        s_locId  = 0;
        s_vehId  = 0;
        s_pkgId  = 0;
        s_custId = 0;
    }

private:
    static std::atomic<int> s_locId;
    static std::atomic<int> s_vehId;
    static std::atomic<int> s_pkgId;
    static std::atomic<int> s_custId;
};

// ── Static member definitions (in header — ODR-safe with inline in C++17) ────
inline std::atomic<int> EntityFactory::s_locId {0};
inline std::atomic<int> EntityFactory::s_vehId {0};
inline std::atomic<int> EntityFactory::s_pkgId {0};
inline std::atomic<int> EntityFactory::s_custId{0};