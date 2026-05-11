// src/system/TrafficMonitor.cpp
// Smart City Delivery & Traffic Management System

#include "TrafficMonitor.h"
#include "CityMapManager.h"
#include <iostream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Observer management
// ─────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::subscribe(std::shared_ptr<ITrafficObserver> observer) {
    if (!observer) return;
    // Avoid duplicates by name
    for (auto& wp : m_observers) {
        auto sp = wp.lock();
        if (sp && std::string(sp->observerName()) == observer->observerName())
            return;
    }
    m_observers.push_back(observer);
}

void TrafficMonitor::unsubscribe(const std::string& observerName) {
    m_observers.erase(
        std::remove_if(m_observers.begin(), m_observers.end(),
            [&observerName](const std::weak_ptr<ITrafficObserver>& wp) {
                auto sp = wp.lock();
                return !sp || std::string(sp->observerName()) == observerName;
            }),
        m_observers.end());
}

int TrafficMonitor::observerCount() const {
    int count = 0;
    for (auto& wp : m_observers)
        if (!wp.expired()) ++count;
    return count;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Event firing
// ─────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::reportCongestion(int fromId, int toId,
                                       double newWeight,
                                       const std::string& desc)
{
    auto& mgr     = CityMapManager::getInstance();
    double oldW   = mgr.getGraph().getEdgeWeight(fromId, toId);

    TrafficEvent ev;
    ev.type        = TrafficEventType::Congestion;
    ev.fromId      = fromId;
    ev.toId        = toId;
    ev.oldWeight   = oldW;
    ev.newWeight   = newWeight;
    ev.description = desc.empty() ? "Traffic congestion reported" : desc;
    ev.timestamp   = std::time(nullptr);

    mgr.updateTraffic(fromId, toId, newWeight);
    fireEvent(std::move(ev));
}

void TrafficMonitor::reportRoadClosed(int fromId, int toId,
                                       const std::string& desc)
{
    auto& mgr   = CityMapManager::getInstance();
    double oldW = mgr.getGraph().getEdgeWeight(fromId, toId);

    TrafficEvent ev;
    ev.type        = TrafficEventType::RoadClosed;
    ev.fromId      = fromId;
    ev.toId        = toId;
    ev.oldWeight   = oldW;
    ev.newWeight   = INF_WEIGHT;
    ev.description = desc.empty() ? "Road closed" : desc;
    ev.timestamp   = std::time(nullptr);

    mgr.closeRoad(fromId, toId);
    fireEvent(std::move(ev));
}

void TrafficMonitor::reportRoadOpened(int fromId, int toId,
                                       double restoredWeight,
                                       const std::string& desc)
{
    auto& mgr = CityMapManager::getInstance();

    TrafficEvent ev;
    ev.type        = TrafficEventType::RoadOpened;
    ev.fromId      = fromId;
    ev.toId        = toId;
    ev.oldWeight   = INF_WEIGHT;
    ev.newWeight   = restoredWeight;
    ev.description = desc.empty() ? "Road reopened" : desc;
    ev.timestamp   = std::time(nullptr);

    mgr.openRoad(fromId, toId);
    mgr.updateTraffic(fromId, toId, restoredWeight);
    fireEvent(std::move(ev));
}

void TrafficMonitor::reportAccident(int fromId, int toId,
                                     const std::string& desc)
{
    auto& mgr   = CityMapManager::getInstance();
    double oldW = mgr.getGraph().getEdgeWeight(fromId, toId);

    TrafficEvent ev;
    ev.type        = TrafficEventType::Accident;
    ev.fromId      = fromId;
    ev.toId        = toId;
    ev.oldWeight   = oldW;
    ev.newWeight   = oldW * 3.0;   // triple travel time during accident
    ev.description = desc.empty() ? "Accident reported" : desc;
    ev.timestamp   = std::time(nullptr);

    mgr.updateTraffic(fromId, toId, ev.newWeight);
    mgr.closeRoad(fromId, toId);   // close for safety
    fireEvent(std::move(ev));
}

void TrafficMonitor::fireEvent(TrafficEvent event) {
    m_history.push_back(event);
    notifyAll(event);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Notification  (removes expired weak_ptrs automatically)
// ─────────────────────────────────────────────────────────────────────────────
void TrafficMonitor::notifyAll(const TrafficEvent& event) {
    // Sweep-and-notify: remove expired, call live ones
    std::vector<std::weak_ptr<ITrafficObserver>> alive;
    for (auto& wp : m_observers) {
        if (auto sp = wp.lock()) {
            sp->onTrafficEvent(event);
            alive.push_back(wp);
        }
        // expired weak_ptrs are simply not kept
    }
    m_observers = std::move(alive);
}

// ─────────────────────────────────────────────────────────────────────────────
//  History queries
// ─────────────────────────────────────────────────────────────────────────────
std::vector<TrafficEvent>
TrafficMonitor::getEventsByType(TrafficEventType t) const {
    std::vector<TrafficEvent> result;
    for (const auto& ev : m_history)
        if (ev.type == t) result.push_back(ev);
    return result;
}

std::vector<TrafficEvent>
TrafficMonitor::getRecentEvents(int maxCount) const {
    int n = static_cast<int>(m_history.size());
    int start = std::max(0, n - maxCount);
    return std::vector<TrafficEvent>(m_history.begin() + start,
                                     m_history.end());
}

std::vector<TrafficEvent>
TrafficMonitor::getMostCongestedRoads(int topK) const {
    std::vector<TrafficEvent> congestion;
    for (const auto& ev : m_history)
        if (ev.type == TrafficEventType::Congestion ||
            ev.type == TrafficEventType::Accident)
            congestion.push_back(ev);

    std::sort(congestion.begin(), congestion.end(),
        [](const TrafficEvent& a, const TrafficEvent& b) {
            return a.newWeight > b.newWeight;   // highest weight first
        });

    if (static_cast<int>(congestion.size()) > topK)
        congestion.resize(topK);
    return congestion;
}

void TrafficMonitor::print(std::ostream& os) const {
    os << "TrafficMonitor[observers=" << observerCount()
       << " events=" << m_history.size() << "]\n";
    for (const auto& ev : m_history)
        os << "  " << ev << "\n";
}