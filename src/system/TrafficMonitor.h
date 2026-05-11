#pragma once
// src/system/TrafficMonitor.h
// Smart City Delivery & Traffic Management System
//
// ── DESIGN PATTERN: Observer ─────────────────────────────────────────────────
//
//  Subject  → TrafficMonitor
//  Observer → ITrafficObserver  (interface)
//  Concrete → VehicleObserver, SchedulerObserver  (registered listeners)
//
//  Flow:
//    1. Traffic event fires (accident, congestion, road closure)
//    2. TrafficMonitor updates CityMapManager edge weights
//    3. TrafficMonitor notifies ALL registered ITrafficObserver instances
//    4. Each observer reacts independently:
//         VehicleObserver   → marks vehicle route as stale → triggers reroute
//         SchedulerObserver → reorders the delivery priority queue
//
//  Modern C++:
//    - Observers stored as weak_ptr to avoid ownership cycles
//    - Event struct carries all data observers need (no raw queries)
//    - Lambda-based observer supported via LambdaObserver adapter

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <ostream>
#include <algorithm>
#include <ctime>
#include "../graph/RoadNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Enum: TrafficEventType
// ─────────────────────────────────────────────────────────────────────────────
enum class TrafficEventType {
    Congestion,     // edge weight increased (slow traffic)
    Cleared,        // edge weight decreased (traffic cleared)
    RoadClosed,     // edge removed from routing
    RoadOpened,     // edge re-enabled
    Accident,       // severe — triggers emergency reroute
    Construction    // long-term partial closure
};

inline std::string trafficEventTypeToString(TrafficEventType t) {
    switch (t) {
        case TrafficEventType::Congestion:   return "Congestion";
        case TrafficEventType::Cleared:      return "Cleared";
        case TrafficEventType::RoadClosed:   return "RoadClosed";
        case TrafficEventType::RoadOpened:   return "RoadOpened";
        case TrafficEventType::Accident:     return "Accident";
        case TrafficEventType::Construction: return "Construction";
        default:                             return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: TrafficEvent  — payload delivered to every observer
// ─────────────────────────────────────────────────────────────────────────────
struct TrafficEvent {
    TrafficEventType type;
    int              fromId;
    int              toId;
    double           oldWeight;
    double           newWeight;
    std::string      description;
    std::time_t      timestamp;

    // Severity score used by PriorityQueue for event ordering
    int severity() const {
        switch (type) {
            case TrafficEventType::Accident:     return 5;
            case TrafficEventType::RoadClosed:   return 4;
            case TrafficEventType::Construction: return 3;
            case TrafficEventType::Congestion:   return 2;
            case TrafficEventType::Cleared:      return 1;
            case TrafficEventType::RoadOpened:   return 1;
            default:                             return 0;
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const TrafficEvent& e) {
        return os << "[TrafficEvent " << trafficEventTypeToString(e.type)
                  << " road=" << e.fromId << "->" << e.toId
                  << " w:" << e.oldWeight << "->" << e.newWeight
                  << " \"" << e.description << "\"]";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Interface: ITrafficObserver
// ─────────────────────────────────────────────────────────────────────────────
class ITrafficObserver {
public:
    virtual ~ITrafficObserver() = default;
    virtual void onTrafficEvent(const TrafficEvent& event) = 0;
    virtual const char* observerName() const = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Concrete Observer: LambdaObserver
//  Lets callers register a lambda instead of subclassing ITrafficObserver.
//  Usage:
//    monitor.subscribe(std::make_shared<LambdaObserver>("MyObs",
//        [](const TrafficEvent& e){ std::cout << e; }));
// ─────────────────────────────────────────────────────────────────────────────
class LambdaObserver final : public ITrafficObserver {
public:
    LambdaObserver(std::string name,
                   std::function<void(const TrafficEvent&)> fn)
        : m_name(std::move(name)), m_fn(std::move(fn)) {}

    void onTrafficEvent(const TrafficEvent& event) override {
        if (m_fn) m_fn(event);
    }
    const char* observerName() const override { return m_name.c_str(); }

private:
    std::string                              m_name;
    std::function<void(const TrafficEvent&)> m_fn;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Class: TrafficMonitor  (Observable / Subject)
//
//  Responsibilities:
//    - Accept real-time traffic events
//    - Update CityMapManager edge weights / closure flags
//    - Notify all registered observers
//    - Maintain event history for analytics
// ─────────────────────────────────────────────────────────────────────────────
class TrafficMonitor {
public:
    TrafficMonitor()  = default;
    ~TrafficMonitor() = default;

    // Non-copyable
    TrafficMonitor(const TrafficMonitor&)            = delete;
    TrafficMonitor& operator=(const TrafficMonitor&) = delete;

    // ── Observer management ───────────────────────────────────────────────────

    // Subscribe an observer (stored as weak_ptr — monitor doesn't own it)
    void subscribe(std::shared_ptr<ITrafficObserver> observer);

    // Unsubscribe by name
    void unsubscribe(const std::string& observerName);

    int observerCount() const;

    // ── Event firing ──────────────────────────────────────────────────────────

    // Report a congestion change on a road segment
    // Updates CityMapManager and notifies all observers
    void reportCongestion(int fromId, int toId,
                          double newWeight,
                          const std::string& description = "");

    // Report road closure — closes edge in CityMapManager
    void reportRoadClosed(int fromId, int toId,
                          const std::string& description = "");

    // Report road re-opened
    void reportRoadOpened(int fromId, int toId,
                          double restoredWeight,
                          const std::string& description = "");

    // Report accident (highest severity)
    void reportAccident(int fromId, int toId,
                        const std::string& description = "");

    // Fire a fully constructed TrafficEvent directly
    void fireEvent(TrafficEvent event);

    // ── Event history ─────────────────────────────────────────────────────────
    const std::vector<TrafficEvent>& getHistory()      const { return m_history; }
    std::vector<TrafficEvent>        getEventsByType(TrafficEventType t) const;
    std::vector<TrafficEvent>        getRecentEvents(int maxCount)       const;
    void                             clearHistory()          { m_history.clear(); }

    // Most congested roads (sorted by newWeight descending)
    std::vector<TrafficEvent> getMostCongestedRoads(int topK = 5) const;

    void print(std::ostream& os = std::cout) const;

private:
    // weak_ptr prevents ownership cycles — expired observers auto-cleaned
    std::vector<std::weak_ptr<ITrafficObserver>> m_observers;
    std::vector<TrafficEvent>                    m_history;

    // Notify all live observers, remove expired weak_ptrs
    void notifyAll(const TrafficEvent& event);
};