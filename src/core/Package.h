#pragma once
// src/core/Package.h
// Smart City Delivery & Traffic Management System

#include <string>
#include <ctime>
#include <ostream>

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: DeliveryStatus
// ─────────────────────────────────────────────────────────────────────────────
enum class DeliveryStatus {
    Pending,     // in queue, not yet assigned
    Assigned,    // assigned to a vehicle
    InTransit,   // vehicle is on its way
    Delivered,   // successfully delivered
    Failed,      // missed deadline or could not be served
    Cancelled    // cancelled by customer
};

inline std::string deliveryStatusToString(DeliveryStatus s) {
    switch (s) {
        case DeliveryStatus::Pending:   return "Pending";
        case DeliveryStatus::Assigned:  return "Assigned";
        case DeliveryStatus::InTransit: return "InTransit";
        case DeliveryStatus::Delivered: return "Delivered";
        case DeliveryStatus::Failed:    return "Failed";
        case DeliveryStatus::Cancelled: return "Cancelled";
        default:                        return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: DeliveryPriority
//  Higher numeric value = more urgent.
// ─────────────────────────────────────────────────────────────────────────────
enum class DeliveryPriority {
    Low      = 1,
    Normal   = 2,
    High     = 3,
    Urgent   = 4,
    Critical = 5
};

inline std::string priorityToString(DeliveryPriority p) {
    switch (p) {
        case DeliveryPriority::Low:      return "Low";
        case DeliveryPriority::Normal:   return "Normal";
        case DeliveryPriority::High:     return "High";
        case DeliveryPriority::Urgent:   return "Urgent";
        case DeliveryPriority::Critical: return "Critical";
        default:                         return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Class: Package
//
//  Represents one delivery request in the system.
//
//  Responsibilities:
//    - Store all delivery metadata: source, destination, deadline,
//      priority, weight, assigned vehicle
//    - Track status transitions throughout the delivery lifecycle
//    - Provide a composite priority score used by the PriorityQueue
//
//  OOP:
//    - Encapsulated: all data private, accessed via getters / setters
//    - Status transitions enforced through explicit setter
//    - Comparable: operator< on priority score → used in binary heap
//    - Rule of Zero: no raw resources
// ─────────────────────────────────────────────────────────────────────────────
class Package {
public:
    // ── Construction ─────────────────────────────────────────────────────────
    Package() = default;

    Package(int            id,
            std::string    trackingNumber,
            int            sourceLocationId,
            int            destLocationId,
            int            customerId,
            double         weightKg,
            DeliveryPriority priority,
            std::time_t    deadlineEpoch)
        : m_id(id)
        , m_trackingNumber(std::move(trackingNumber))
        , m_sourceLocationId(sourceLocationId)
        , m_destLocationId(destLocationId)
        , m_customerId(customerId)
        , m_weightKg(weightKg)
        , m_priority(priority)
        , m_deadlineEpoch(deadlineEpoch)
        , m_status(DeliveryStatus::Pending)
        , m_assignedVehicleId(-1)
    {}

    // ── Identity Getters ──────────────────────────────────────────────────────
    int                getId()             const { return m_id;             }
    const std::string& getTrackingNumber() const { return m_trackingNumber; }
    int                getCustomerId()     const { return m_customerId;     }

    // ── Route Getters ─────────────────────────────────────────────────────────
    int getSourceLocationId() const { return m_sourceLocationId; }
    int getDestLocationId()   const { return m_destLocationId;   }

    // ── Delivery Spec Getters ─────────────────────────────────────────────────
    double           getWeightKg()      const { return m_weightKg;      }
    DeliveryPriority getPriority()      const { return m_priority;      }
    std::time_t      getDeadline()      const { return m_deadlineEpoch; }
    DeliveryStatus   getStatus()        const { return m_status;        }
    int              getAssignedVehicle() const { return m_assignedVehicleId; }

    // ── Setters ───────────────────────────────────────────────────────────────
    void setStatus(DeliveryStatus s)     { m_status = s;             }
    void setPriority(DeliveryPriority p) { m_priority = p;           }
    void assignVehicle(int vehicleId)    {
        m_assignedVehicleId = vehicleId;
        m_status = DeliveryStatus::Assigned;
    }

    // ── Utility ───────────────────────────────────────────────────────────────

    // Composite priority score used by PriorityQueue:
    //   - Higher DeliveryPriority → higher score
    //   - Closer deadline → higher score
    //   Urgency = priority_level * 1000 + (1 / seconds_until_deadline)
    double priorityScore() const {
        auto now       = std::time(nullptr);
        double remaining = static_cast<double>(m_deadlineEpoch - now);
        double urgency   = (remaining > 0.0) ? (1.0 / remaining) : 1e9; // past deadline = max urgency
        return static_cast<int>(m_priority) * 1000.0 + urgency;
    }

    bool isOverdue() const {
        return std::time(nullptr) > m_deadlineEpoch;
    }

    bool isPending() const {
        return m_status == DeliveryStatus::Pending;
    }

    // ── Operators ─────────────────────────────────────────────────────────────
    // Lower priority score → "less important" (min-heap friendly)
    bool operator< (const Package& o) const { return priorityScore() < o.priorityScore(); }
    bool operator> (const Package& o) const { return o < *this;                           }
    bool operator==(const Package& o) const { return m_id == o.m_id;                      }
    bool operator!=(const Package& o) const { return !(*this == o);                       }

    friend std::ostream& operator<<(std::ostream& os, const Package& p) {
        return os << "[Package id="      << p.m_id
                  << " track="           << p.m_trackingNumber
                  << " "                 << p.m_sourceLocationId
                  << "->"                << p.m_destLocationId
                  << " weight="          << p.m_weightKg << "kg"
                  << " priority="        << priorityToString(p.m_priority)
                  << " status="          << deliveryStatusToString(p.m_status)
                  << " vehicle="         << p.m_assignedVehicleId << "]";
    }

private:
    int              m_id              {-1};
    std::string      m_trackingNumber;
    int              m_sourceLocationId{-1};
    int              m_destLocationId  {-1};
    int              m_customerId      {-1};
    double           m_weightKg        {0.0};
    DeliveryPriority m_priority        {DeliveryPriority::Normal};
    std::time_t      m_deadlineEpoch   {0};
    DeliveryStatus   m_status          {DeliveryStatus::Pending};
    int              m_assignedVehicleId{-1};
};
