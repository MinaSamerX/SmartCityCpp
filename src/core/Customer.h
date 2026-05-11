#pragma once
// src/core/Customer.h
// Smart City Delivery & Traffic Management System

#include <string>
#include <vector>
#include <ostream>

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: CustomerTier
//  Tier affects default delivery priority boost.
// ─────────────────────────────────────────────────────────────────────────────
enum class CustomerTier {
    Standard,  // no boost
    Premium,   // +1 priority level
    VIP        // +2 priority levels (or always Urgent)
};

inline std::string customerTierToString(CustomerTier t) {
    switch (t) {
        case CustomerTier::Standard: return "Standard";
        case CustomerTier::Premium:  return "Premium";
        case CustomerTier::VIP:      return "VIP";
        default:                     return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Class: Customer
//
//  Represents a registered customer in the system.
//
//  Responsibilities:
//    - Store contact info and service tier
//    - Track the customer's default delivery location
//    - Maintain history of delivery IDs (for analytics & reporting)
//
//  OOP:
//    - Encapsulated: private data / public interface
//    - Delivery history is append-only from outside (no direct vector access)
//    - Rule of Zero — copyable & movable by default
// ─────────────────────────────────────────────────────────────────────────────
class Customer {
public:
    // ── Construction ─────────────────────────────────────────────────────────
    Customer() = default;

    Customer(int           id,
             std::string   name,
             std::string   phone,
             std::string   email,
             int           defaultLocationId,
             CustomerTier  tier = CustomerTier::Standard)
        : m_id(id)
        , m_name(std::move(name))
        , m_phone(std::move(phone))
        , m_email(std::move(email))
        , m_defaultLocationId(defaultLocationId)
        , m_tier(tier)
    {}

    // ── Getters ───────────────────────────────────────────────────────────────
    int                getId()               const { return m_id;               }
    const std::string& getName()             const { return m_name;             }
    const std::string& getPhone()            const { return m_phone;            }
    const std::string& getEmail()            const { return m_email;            }
    int                getDefaultLocation()  const { return m_defaultLocationId; }
    CustomerTier       getTier()             const { return m_tier;             }

    const std::vector<int>& getDeliveryHistory() const {
        return m_deliveryHistory;
    }

    int getTotalDeliveries() const {
        return static_cast<int>(m_deliveryHistory.size());
    }

    // ── Setters ───────────────────────────────────────────────────────────────
    void setPhone(const std::string& phone)  { m_phone = phone;              }
    void setEmail(const std::string& email)  { m_email = email;              }
    void setDefaultLocation(int locationId)  { m_defaultLocationId = locationId; }
    void setTier(CustomerTier tier)          { m_tier  = tier;               }

    // ── History ───────────────────────────────────────────────────────────────
    void addDelivery(int deliveryId) {
        m_deliveryHistory.push_back(deliveryId);
    }

    bool hasDelivery(int deliveryId) const {
        for (int id : m_deliveryHistory)
            if (id == deliveryId) return true;
        return false;
    }

    // ── Operators ─────────────────────────────────────────────────────────────
    bool operator==(const Customer& o) const { return m_id == o.m_id; }
    bool operator!=(const Customer& o) const { return !(*this == o);  }
    bool operator< (const Customer& o) const { return m_id  < o.m_id; }

    friend std::ostream& operator<<(std::ostream& os, const Customer& c) {
        return os << "[Customer id="  << c.m_id
                  << " name=\""       << c.m_name    << "\""
                  << " tier="         << customerTierToString(c.m_tier)
                  << " location="     << c.m_defaultLocationId
                  << " deliveries="   << c.m_deliveryHistory.size() << "]";
    }

private:
    int           m_id                {-1};
    std::string   m_name;
    std::string   m_phone;
    std::string   m_email;
    int           m_defaultLocationId {-1};
    CustomerTier  m_tier              {CustomerTier::Standard};

    std::vector<int> m_deliveryHistory; // IDs of all past / active deliveries
};
