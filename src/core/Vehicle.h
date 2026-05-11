#pragma once
// src/core/Vehicle.h
// Smart City Delivery & Traffic Management System

#include <string>
#include <vector>
#include <algorithm>
#include <ostream>
#include "Location.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: VehicleStatus
// ─────────────────────────────────────────────────────────────────────────────
enum class VehicleStatus {
    Idle,         // available for assignment
    EnRoute,      // travelling to pickup / delivery
    Delivering,   // actively on a delivery run
    Charging,     // at a charging station
    Maintenance   // out of service
};

inline std::string vehicleStatusToString(VehicleStatus s) {
    switch (s) {
        case VehicleStatus::Idle:        return "Idle";
        case VehicleStatus::EnRoute:     return "EnRoute";
        case VehicleStatus::Delivering:  return "Delivering";
        case VehicleStatus::Charging:    return "Charging";
        case VehicleStatus::Maintenance: return "Maintenance";
        default:                         return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: VehicleType
// ─────────────────────────────────────────────────────────────────────────────
enum class VehicleType {
    Bike,    // small, fast, low capacity
    Van,     // medium capacity
    Truck    // heavy load
};

inline std::string vehicleTypeToString(VehicleType t) {
    switch (t) {
        case VehicleType::Bike:  return "Bike";
        case VehicleType::Van:   return "Van";
        case VehicleType::Truck: return "Truck";
        default:                 return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Class: Vehicle
//
//  Represents a delivery vehicle in the fleet.
//
//  Responsibilities:
//    - Store identity, physical specs (capacity, speed, battery)
//    - Track real-time location and current status
//    - Maintain the ordered list of assigned delivery IDs
//    - Provide load management (canLoad, loadPackage, unloadPackage)
//
//  OOP:
//    - Private data, public interface (encapsulation)
//    - Status & location are mutable (runtime updates)
//    - Rule of Zero — copyable & movable by default
//    - operator<< for easy reporting
// ─────────────────────────────────────────────────────────────────────────────
class Vehicle {
public:
    // ── Construction ─────────────────────────────────────────────────────────
    Vehicle() = default;

    Vehicle(int id,
            std::string  name,
            VehicleType  type,
            double       capacityKg,
            double       speedKmh,
            double       batteryKwh,
            int          startLocationId)
        : m_id(id)
        , m_name(std::move(name))
        , m_type(type)
        , m_capacityKg(capacityKg)
        , m_speedKmh(speedKmh)
        , m_batteryKwh(batteryKwh)
        , m_currentBattery(batteryKwh)   // starts full
        , m_currentLocationId(startLocationId)
        , m_status(VehicleStatus::Idle)
        , m_currentLoadKg(0.0)
    {}

    // ── Identity Getters ──────────────────────────────────────────────────────
    int                getId()        const { return m_id;          }
    const std::string& getName()      const { return m_name;        }
    VehicleType        getType()      const { return m_type;        }

    // ── Spec Getters ──────────────────────────────────────────────────────────
    double getCapacityKg()     const { return m_capacityKg;        }
    double getSpeedKmh()       const { return m_speedKmh;          }
    double getBatteryKwh()     const { return m_batteryKwh;        }
    double getCurrentBattery() const { return m_currentBattery;    }
    double getCurrentLoadKg()  const { return m_currentLoadKg;     }
    double getRemainingCapacity() const {
        return m_capacityKg - m_currentLoadKg;
    }

    // ── Runtime State Getters ─────────────────────────────────────────────────
    VehicleStatus getStatus()          const { return m_status;           }
    int           getCurrentLocationId() const { return m_currentLocationId; }

    const std::vector<int>& getAssignedDeliveries() const {
        return m_assignedDeliveryIds;
    }

    // ── Runtime State Setters ─────────────────────────────────────────────────
    void setStatus(VehicleStatus status)      { m_status = status;          }
    void setCurrentLocation(int locationId)   { m_currentLocationId = locationId; }
    void setCurrentBattery(double kwh)        { m_currentBattery = kwh;     }

    // ── Load Management ───────────────────────────────────────────────────────

    // Returns true if vehicle can take an additional weightKg
    bool canLoad(double weightKg) const {
        return (m_currentLoadKg + weightKg) <= m_capacityKg;
    }

    // Add a package to this vehicle
    // Returns false if over capacity
    bool loadPackage(int deliveryId, double weightKg) {
        if (!canLoad(weightKg)) return false;
        m_assignedDeliveryIds.push_back(deliveryId);
        m_currentLoadKg += weightKg;
        return true;
    }

    // Remove a delivered package
    void unloadPackage(int deliveryId, double weightKg) {
        auto& v = m_assignedDeliveryIds;
        v.erase(std::remove(v.begin(), v.end(), deliveryId), v.end());
        m_currentLoadKg = std::max(0.0, m_currentLoadKg - weightKg);
    }

    // Clear all assignments (e.g. after a full run)
    void clearDeliveries() {
        m_assignedDeliveryIds.clear();
        m_currentLoadKg = 0.0;
    }

    // ── Utility ───────────────────────────────────────────────────────────────
    bool isAvailable() const { return m_status == VehicleStatus::Idle; }

    // Estimated travel time (hours) for a given distance
    double estimatedTravelTime(double distanceKm) const {
        return (m_speedKmh > 0.0) ? distanceKm / m_speedKmh : 0.0;
    }

    // ── Operators ─────────────────────────────────────────────────────────────
    bool operator==(const Vehicle& o) const { return m_id == o.m_id; }
    bool operator!=(const Vehicle& o) const { return !(*this == o);  }
    bool operator< (const Vehicle& o) const { return m_id  < o.m_id; }

    friend std::ostream& operator<<(std::ostream& os, const Vehicle& v) {
        return os << "[Vehicle id="       << v.m_id
                  << " name=\""           << v.m_name          << "\""
                  << " type="             << vehicleTypeToString(v.m_type)
                  << " status="           << vehicleStatusToString(v.m_status)
                  << " load="             << v.m_currentLoadKg << "/" << v.m_capacityKg << "kg"
                  << " battery="          << v.m_currentBattery << "/" << v.m_batteryKwh << "kwh"
                  << " location="         << v.m_currentLocationId << "]";
    }

private:
    // identity
    int           m_id   {-1};
    std::string   m_name;
    VehicleType   m_type {VehicleType::Van};

    // specs (immutable after construction)
    double        m_capacityKg   {0.0};
    double        m_speedKmh     {0.0};
    double        m_batteryKwh   {0.0};

    // runtime state (mutable)
    double        m_currentBattery   {0.0};
    int           m_currentLocationId{-1};
    VehicleStatus m_status           {VehicleStatus::Idle};
    double        m_currentLoadKg    {0.0};

    // ordered list of delivery IDs currently assigned
    std::vector<int> m_assignedDeliveryIds;
};
