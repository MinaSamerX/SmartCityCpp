#pragma once
// src/scheduler/DeliveryScheduler.h
// Smart City Delivery & Traffic Management System
//
// ── Responsibilities ──────────────────────────────────────────────────────────
// DeliveryScheduler is the central hub for all delivery queue operations.
// It owns three complementary data structures that work together:
//
//   1. PriorityQueue (binary heap) — O(log N) extract-max by priorityScore()
//      Used for: pulling the next most-urgent delivery to assign
//
//   2. BST (deadline-ordered) — O(log N) range queries by deadline
//      Used for: "which deliveries are due in the next 30 minutes?"
//
//   3. HashTable (id → Package) — O(1) lookup for status updates
//      Used for: updating a delivery's status or priority mid-queue
//
// DeliveryScheduler also implements ITrafficObserver:
// When a TrafficEvent fires, it re-evaluates overdue packages and
// reorders the priority queue accordingly.

#include <memory>
#include <vector>
#include <ostream>
#include "../core/Package.h"
#include "../core/Vehicle.h"
#include "../scheduler/PriorityQueue.h"
#include "../spatial/BST.h"
#include "../hashtable/HashTable.h"
#include "../hashtable/HashFunctions.h"
#include "../system/TrafficMonitor.h"

class DeliveryScheduler : public ITrafficObserver {
public:
    // ── Construction ─────────────────────────────────────────────────────────
    DeliveryScheduler();
    ~DeliveryScheduler() override = default;

    // Non-copyable
    DeliveryScheduler(const DeliveryScheduler&)            = delete;
    DeliveryScheduler& operator=(const DeliveryScheduler&) = delete;

    // ── Queue operations ──────────────────────────────────────────────────────

    // Add a new delivery to all three structures
    void addDelivery(std::shared_ptr<Package> pkg);

    // Remove and return the highest-priority pending delivery
    std::shared_ptr<Package> popNextDelivery();

    // Peek at highest-priority without removing
    std::shared_ptr<Package> peekNextDelivery() const;

    // Update priority of an existing package (e.g. customer escalation)
    void updatePriority(int packageId, DeliveryPriority newPriority);

    // Mark delivery as assigned to a vehicle — removes from pending queue
    void assignDelivery(int packageId, int vehicleId);

    // Mark delivery as completed / failed — updates status
    void completeDelivery(int packageId, bool success = true);

    // Cancel a delivery and remove it from all structures
    bool cancelDelivery(int packageId);

    // ── Query operations ──────────────────────────────────────────────────────

    // All deliveries due within the next N seconds
    std::vector<std::shared_ptr<Package>> getDueWithin(int seconds) const;

    // All overdue deliveries (deadline already passed)
    std::vector<std::shared_ptr<Package>> getOverdue() const;

    // Lookup by ID — O(1)
    std::shared_ptr<Package> getPackage(int id) const;

    // All pending deliveries sorted by priority (snapshot copy)
    std::vector<std::shared_ptr<Package>> getAllPending() const;

    // ── Metrics ───────────────────────────────────────────────────────────────
    int  pendingCount()   const { return static_cast<int>(m_queue.size()); }
    int  totalCount()     const { return static_cast<int>(m_allPackages.size()); }
    bool isEmpty()        const { return m_queue.empty(); }

    void printQueue(std::ostream& os = std::cout) const;

    // ── ITrafficObserver ──────────────────────────────────────────────────────
    // Called by TrafficMonitor when a traffic event fires.
    // Reorders the queue: packages on affected roads get priority boost.
    void onTrafficEvent(const TrafficEvent& event) override;
    const char* observerName() const override { return "DeliveryScheduler"; }

private:
    // Max-heap by priorityScore() — highest urgency served first
    DeliveryQueue                                      m_queue;

    // BST sorted by deadline — range queries for upcoming deadlines
    DeadlineBST                                        m_deadlineBST;

    // O(1) lookup for any package by ID (all statuses)
    HashTable<int, std::shared_ptr<Package>, HashFn::IntHash> m_allPackages;

    // Rebuild priority queue from current pending packages
    // Called after batch priority changes
    void rebuildQueue();
};