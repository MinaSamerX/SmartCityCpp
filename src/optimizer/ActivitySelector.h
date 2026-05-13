#pragma once
// src/optimizer/ActivitySelector.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <algorithm>
#include <memory>
#include <functional>
#include "../core/Package.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: Greedy
//
//  Activity Selection — O(N log N)
//
//  Classic greedy: given N deliveries each with a start and finish time,
//  select the maximum number of non-overlapping deliveries a single
//  vehicle can complete in a time window.
//
//  In this system each delivery's "interval" is:
//    start  = earliest pickup time (now + travel time to src)
//    finish = deadline
//
//  Greedy rule: always pick the activity with the earliest finish time
//  that is compatible with the last selected activity.
// ─────────────────────────────────────────────────────────────────────────────
namespace Greedy {

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: Activity
// ─────────────────────────────────────────────────────────────────────────────
struct Activity {
    std::shared_ptr<Package> pkg;
    double                   start;     // seconds from now
    double                   finish;    // deadline in seconds from now
    double                   profit;    // optional profit value

    bool operator<(const Activity& o) const { return finish < o.finish; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  selectActivities
//  Returns maximum non-overlapping subset sorted by finish time.
//  O(N log N) — sort + single linear scan.
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<Activity> selectActivities(
    std::vector<Activity> activities)
{
    if (activities.empty()) return {};

    // Sort by finish time (earliest deadline first)
    std::sort(activities.begin(), activities.end());

    std::vector<Activity> selected;
    double lastFinish = -1.0;

    for (const auto& act : activities) {
        if (act.start >= lastFinish) {   // compatible with last selection
            selected.push_back(act);
            lastFinish = act.finish;
        }
    }
    return selected;
}

// ─────────────────────────────────────────────────────────────────────────────
//  selectMaxProfit
//  Greedy variant: sort by profit/duration ratio, pick non-overlapping.
//  Maximises total profit instead of count.
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<Activity> selectMaxProfit(
    std::vector<Activity> activities)
{
    if (activities.empty()) return {};

    // Sort by profit per time unit descending
    std::sort(activities.begin(), activities.end(),
        [](const Activity& a, const Activity& b) {
            double durA = std::max(a.finish - a.start, 0.001);
            double durB = std::max(b.finish - b.start, 0.001);
            return (a.profit / durA) > (b.profit / durB);
        });

    std::vector<Activity> selected;
    // Check compatibility after greedy pick (interval scheduling)
    for (const auto& act : activities) {
        bool compatible = true;
        for (const auto& sel : selected) {
            // Overlap if not (act ends before sel starts OR act starts after sel ends)
            if (!(act.finish <= sel.start || act.start >= sel.finish)) {
                compatible = false;
                break;
            }
        }
        if (compatible) selected.push_back(act);
    }
    return selected;
}

// ─────────────────────────────────────────────────────────────────────────────
//  buildActivities
//  Helper: convert packages to Activity list.
//  travelTimeFn: packageId → estimated seconds to reach pickup
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<Activity> buildActivities(
    const std::vector<std::shared_ptr<Package>>& packages,
    std::function<double(int pkgId)>             travelTimeFn,
    double                                        profitPerPriorityLevel = 100.0)
{
    std::time_t now = std::time(nullptr);
    std::vector<Activity> acts;
    acts.reserve(packages.size());

    for (const auto& pkg : packages) {
        if (!pkg->isPending()) continue;
        double travelSecs  = travelTimeFn(pkg->getId());
        double start       = travelSecs;
        double finish      = static_cast<double>(pkg->getDeadline() - now);

        // Overdue packages: keep them but boost profit so they are served first.
        // finish = 0 means "serve immediately" — set a small positive window.
        if (finish <= 0.0) {
            // Overdue — treat as most urgent: start=0, finish=large, max profit
            start  = 0.0;
            finish = 1e9;  // serve immediately, no deadline constraint
        }

        double profit = static_cast<int>(pkg->getPriority())
                        * profitPerPriorityLevel;
        // Extra profit boost for overdue deliveries
        if (pkg->isOverdue()) profit *= 2.0;

        acts.push_back({pkg, start, finish, profit});
    }
    return acts;
}

} // namespace Greedy