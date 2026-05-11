#pragma once
// src/core/Location.h
// Smart City Delivery & Traffic Management System

#include <string>
#include <cmath>
#include <ostream>

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: LocationType
//  Categorises every node in the city graph.
// ─────────────────────────────────────────────────────────────────────────────
enum class LocationType {
    Intersection,    // plain road junction
    Warehouse,       // package origin / depot
    Customer,        // delivery destination
    ChargingStation, // vehicle recharge point
    TrafficHub       // high-throughput junction (monitored)
};

inline std::string locationTypeToString(LocationType t) {
    switch (t) {
        case LocationType::Intersection:    return "Intersection";
        case LocationType::Warehouse:       return "Warehouse";
        case LocationType::Customer:        return "Customer";
        case LocationType::ChargingStation: return "ChargingStation";
        case LocationType::TrafficHub:      return "TrafficHub";
        default:                            return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: Coordinates
//  2-D geographic position (city-grid units or GPS).
// ─────────────────────────────────────────────────────────────────────────────
struct Coordinates {
    double x{0.0};
    double y{0.0};

    Coordinates() = default;
    Coordinates(double x, double y) : x(x), y(y) {}

    double distanceTo(const Coordinates& other) const {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    bool operator==(const Coordinates& o) const { return x == o.x && y == o.y; }

    friend std::ostream& operator<<(std::ostream& os, const Coordinates& c) {
        return os << "(" << c.x << ", " << c.y << ")";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Class: Location
//
//  Represents a node in the city road graph.
//
//  Responsibilities:
//    - Hold identity (id, name) and spatial data (coordinates, type)
//    - Provide distance helpers used by spatial trees & route optimizer
//    - Support equality / ordering so it works in STL containers
//
//  OOP:
//    - Private data, public interface (encapsulation)
//    - Immutable core (id, coords) — set once at construction
//    - Rule of Zero: no raw resources → copyable & movable by default
//    - operator<< for easy debugging / reporting
// ─────────────────────────────────────────────────────────────────────────────
class Location {
public:
    // ── Construction ─────────────────────────────────────────────────────────
    Location() = default;

    Location(int id,
             std::string name,
             double x, double y,
             LocationType type = LocationType::Intersection)
        : m_id(id)
        , m_name(std::move(name))   // C++11 move — avoids extra copy
        , m_coords(x, y)
        , m_type(type)
    {}

    // ── Getters ───────────────────────────────────────────────────────────────
    int                getId()     const { return m_id;       }
    const std::string& getName()   const { return m_name;     }
    const Coordinates& getCoords() const { return m_coords;   }
    LocationType       getType()   const { return m_type;     }
    double             getX()      const { return m_coords.x; }
    double             getY()      const { return m_coords.y; }

    // ── Setters (only truly mutable fields) ───────────────────────────────────
    void setName(const std::string& name) { m_name = name; }
    void setType(LocationType type)       { m_type = type; }

    // ── Utility ───────────────────────────────────────────────────────────────
    // Euclidean distance to another Location
    double distanceTo(const Location& other) const {
        return m_coords.distanceTo(other.m_coords);
    }

    // Euclidean distance to raw coordinates (used by QuadTree / KDTree)
    double distanceTo(double x, double y) const {
        return m_coords.distanceTo({x, y});
    }

    // ── Operators ─────────────────────────────────────────────────────────────
    bool operator==(const Location& o) const { return m_id == o.m_id;  }
    bool operator!=(const Location& o) const { return !(*this == o);   }
    bool operator< (const Location& o) const { return m_id  < o.m_id;  } // for std::set / std::map

    friend std::ostream& operator<<(std::ostream& os, const Location& l) {
        return os << "[Location id=" << l.m_id
                  << " name=\""      << l.m_name   << "\""
                  << " coords="      << l.m_coords
                  << " type="        << locationTypeToString(l.m_type) << "]";
    }

private:
    int          m_id    {-1};
    std::string  m_name;
    Coordinates  m_coords;
    LocationType m_type  {LocationType::Intersection};
};
