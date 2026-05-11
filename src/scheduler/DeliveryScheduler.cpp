// src/scheduler/DeliveryScheduler.cpp
// Smart City Delivery & Traffic Management System

#include "DeliveryScheduler.h"
#include <iostream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────
DeliveryScheduler::DeliveryScheduler()
    : m_queue(makeDeliveryQueue())
    , m_deadlineBST(makeDeadlineBST())
{}

// ─────────────────────────────────────────────────────────────────────────────
//  Queue operations
// ─────────────────────────────────────────────────────────────────────────────
void DeliveryScheduler::addDelivery(std::shared_ptr<Package> pkg) {
    if (!pkg) return;
    int id = pkg->getId();

    // Register in O(1) lookup table
    m_allPackages.insert(id, pkg);

    // Push to max-heap (only if pending)
    if (pkg->isPending()) {
        m_queue.push(pkg);
        m_deadlineBST.insert(pkg);
    }
}

std::shared_ptr<Package> DeliveryScheduler::popNextDelivery() {
    if (m_queue.empty()) return nullptr;
    auto pkg = m_queue.top();
    m_queue.pop();
    m_deadlineBST.remove(pkg->getDeadline());
    return pkg;
}

std::shared_ptr<Package> DeliveryScheduler::peekNextDelivery() const {
    if (m_queue.empty()) return nullptr;
    return m_queue.top();
}

void DeliveryScheduler::updatePriority(int packageId,
                                        DeliveryPriority newPriority) {
    auto* ptr = m_allPackages.search(packageId);
    if (!ptr) return;
    auto& pkg = *ptr;

    // Update on the Package object
    pkg->setPriority(newPriority);

    // Re-key in the heap (updatePriority triggers heapifyUp/Down)
    if (m_queue.contains(packageId))
        m_queue.updatePriority(packageId, pkg);
}

void DeliveryScheduler::assignDelivery(int packageId, int vehicleId) {
    auto* ptr = m_allPackages.search(packageId);
    if (!ptr) return;
    auto& pkg = *ptr;

    pkg->assignVehicle(vehicleId);         // sets status → Assigned

    // Remove from pending structures
    m_queue.remove(packageId);
    m_deadlineBST.remove(pkg->getDeadline());
}

void DeliveryScheduler::completeDelivery(int packageId, bool success) {
    auto* ptr = m_allPackages.search(packageId);
    if (!ptr) return;
    auto& pkg = *ptr;

    pkg->setStatus(success ? DeliveryStatus::Delivered
                           : DeliveryStatus::Failed);

    // Ensure removed from pending
    m_queue.remove(packageId);
}

bool DeliveryScheduler::cancelDelivery(int packageId) {
    auto* ptr = m_allPackages.search(packageId);
    if (!ptr) return false;
    auto& pkg = *ptr;

    pkg->setStatus(DeliveryStatus::Cancelled);
    m_queue.remove(packageId);
    m_deadlineBST.remove(pkg->getDeadline());
    m_allPackages.remove(packageId);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Query operations
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::shared_ptr<Package>>
DeliveryScheduler::getDueWithin(int seconds) const {
    std::time_t now  = std::time(nullptr);
    std::time_t hi   = now + static_cast<std::time_t>(seconds);
    // BST range query: deadlines in [now, now+seconds]
    return m_deadlineBST.rangeQuery(now, hi);
}

std::vector<std::shared_ptr<Package>>
DeliveryScheduler::getOverdue() const {
    std::time_t now = std::time(nullptr);
    // BST range query: deadlines in [0, now]
    return m_deadlineBST.rangeQuery(static_cast<std::time_t>(0), now);
}

std::shared_ptr<Package> DeliveryScheduler::getPackage(int id) const {
    const auto* ptr = m_allPackages.search(id);
    return ptr ? *ptr : nullptr;
}

std::vector<std::shared_ptr<Package>>
DeliveryScheduler::getAllPending() const {
    // Return snapshot of heap contents (unsorted copy)
    auto items = m_queue.toVector();
    // Sort by priorityScore descending for human-readable output
    std::sort(items.begin(), items.end(),
        [](const std::shared_ptr<Package>& a,
           const std::shared_ptr<Package>& b) {
            return a->priorityScore() > b->priorityScore();
        });
    return items;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ITrafficObserver — called by TrafficMonitor on every event
// ─────────────────────────────────────────────────────────────────────────────
void DeliveryScheduler::onTrafficEvent(const TrafficEvent& event) {
    // For high-severity events (accident, road closed):
    // scan all pending packages and boost priority for those
    // whose SOURCE or DESTINATION is on the affected road.
    if (event.severity() < 3) return;  // ignore minor congestion updates

    std::cout << "[DeliveryScheduler] Traffic event received: "
              << trafficEventTypeToString(event.type)
              << " on road " << event.fromId << "->" << event.toId << "\n";

    bool anyChanged = false;
    m_allPackages.forEach([&](const int&, std::shared_ptr<Package>& pkg) {
        if (!pkg->isPending()) return;

        bool affected = (pkg->getSourceLocationId() == event.fromId ||
                         pkg->getSourceLocationId() == event.toId   ||
                         pkg->getDestLocationId()   == event.fromId ||
                         pkg->getDestLocationId()   == event.toId);

        if (affected && pkg->getPriority() < DeliveryPriority::Urgent) {
            pkg->setPriority(DeliveryPriority::Urgent);
            std::cout << "  [DeliveryScheduler] Boosted package "
                      << pkg->getId() << " to Urgent\n";
            anyChanged = true;
        }
    });

    if (anyChanged) rebuildQueue();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────────────────────────────────────
void DeliveryScheduler::rebuildQueue() {
    // Collect all pending packages
    std::vector<std::shared_ptr<Package>> pending;
    m_allPackages.forEach([&](const int&, const std::shared_ptr<Package>& p) {
        if (p->isPending()) pending.push_back(p);
    });

    // Rebuild heap from scratch
    m_queue = makeDeliveryQueue();
    for (auto& p : pending) m_queue.push(p);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostics
// ─────────────────────────────────────────────────────────────────────────────
void DeliveryScheduler::printQueue(std::ostream& os) const {
    auto pending = getAllPending();
    os << "DeliveryScheduler[pending=" << pending.size()
       << " total=" << totalCount() << "]\n";
    for (const auto& p : pending)
        os << "  " << *p << "\n";
}