#pragma once
// src/optimizer/Knapsack.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <algorithm>
#include <memory>
#include "../core/Package.h"
#include "../core/Vehicle.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: Greedy
//
//  Fractional Knapsack — O(N log N)
//
//  Purpose: given a vehicle with remaining capacity, select the best subset
//  of packages to load, maximising value/weight ratio (profit per kg).
//
//  "Fractional" means if a package doesn't fully fit we can take a fraction
//  of its profit — useful for splitting large batches across vehicles.
//  In practice for discrete packages we use 0-1 selection (take or skip).
// ─────────────────────────────────────────────────────────────────────────────
namespace Greedy {

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: KnapsackItem
//  Wraps a Package with a computed value score for sorting.
// ─────────────────────────────────────────────────────────────────────────────
struct KnapsackItem {
    std::shared_ptr<Package> pkg;
    double                   valuePerKg;  // priority score / weight

    bool operator>(const KnapsackItem& o) const {
        return valuePerKg > o.valuePerKg;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  fractionalKnapsack
//
//  Selects packages for a vehicle greedily by value/weight ratio.
//  Returns the selected packages in loading order.
//  Modifies nothing — purely functional.
//
//  Parameters:
//    packages       — candidates (all pending)
//    capacityKg     — remaining vehicle capacity
//    maxCount       — optional limit on number of packages (-1 = unlimited)
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<std::shared_ptr<Package>> fractionalKnapsack(
    const std::vector<std::shared_ptr<Package>>& packages,
    double capacityKg,
    int    maxCount = -1)
{
    // Build items with value/weight ratio
    // Value = priorityScore() (higher priority + closer deadline = more value)
    std::vector<KnapsackItem> items;
    items.reserve(packages.size());

    for (const auto& pkg : packages) {
        if (!pkg->isPending()) continue;
        double w = pkg->getWeightKg();
        if (w <= 0.0) w = 0.001;   // guard against zero weight
        items.push_back({pkg, pkg->priorityScore() / w});
    }

    // Sort by value/weight descending — greedy criterion
    std::sort(items.begin(), items.end(),
        [](const KnapsackItem& a, const KnapsackItem& b) {
            return a.valuePerKg > b.valuePerKg;
        });

    // Greedily fill vehicle
    std::vector<std::shared_ptr<Package>> selected;
    double remaining = capacityKg;
    int    count     = 0;

    for (const auto& item : items) {
        if (maxCount >= 0 && count >= maxCount) break;
        if (item.pkg->getWeightKg() <= remaining) {
            selected.push_back(item.pkg);
            remaining -= item.pkg->getWeightKg();
            ++count;
        }
        // 0-1 knapsack: skip if doesn't fit (no fractional packages)
    }
    return selected;
}

// ─────────────────────────────────────────────────────────────────────────────
//  loadVehicle
//  Convenience wrapper: selects packages for a vehicle and loads them.
//  Returns the list of packages actually loaded.
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<std::shared_ptr<Package>> loadVehicle(
    Vehicle& vehicle,
    const std::vector<std::shared_ptr<Package>>& candidates)
{
    auto selected = fractionalKnapsack(candidates,
                                       vehicle.getRemainingCapacity());
    for (auto& pkg : selected)
        vehicle.loadPackage(pkg->getId(), pkg->getWeightKg());
    return selected;
}

} // namespace Greedy