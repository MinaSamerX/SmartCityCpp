#pragma once
// src/dataprocessor/ClosestPair.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include "../core/Location.h"
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: DivideConquer
//
//  Closest Pair of Points — O(N log N) divide and conquer.
//
//  Purpose in this system:
//    - Find two delivery locations that are closest together
//    - Batch nearby deliveries into a single vehicle run
//    - Identify delivery clusters for zone partitioning
//
//  Algorithm:
//    1. Sort points by X coordinate
//    2. Recursively find closest pair in left/right halves
//    3. Check strip of width 2*delta around midline
//    4. Strip check is O(N) because at most 7 points per delta×2delta box
//
//  Complexity: O(N log N)
// ─────────────────────────────────────────────────────────────────────────────
namespace DivideConquer {

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: ClosestPairResult
// ─────────────────────────────────────────────────────────────────────────────
struct ClosestPairResult {
    std::shared_ptr<Location> locA;
    std::shared_ptr<Location> locB;
    double                    distance{std::numeric_limits<double>::max()};

    bool valid() const { return locA != nullptr && locB != nullptr; }
};

namespace detail {

inline double dist(const std::shared_ptr<Location>& a,
                   const std::shared_ptr<Location>& b) {
    double dx = a->getX() - b->getX();
    double dy = a->getY() - b->getY();
    return std::sqrt(dx*dx + dy*dy);
}

// Brute force for small sets (≤ 3 points)
inline ClosestPairResult bruteForce(
    const std::vector<std::shared_ptr<Location>>& pts, int lo, int hi)
{
    ClosestPairResult best;
    for (int i = lo; i <= hi; ++i)
        for (int j = i + 1; j <= hi; ++j) {
            double d = dist(pts[i], pts[j]);
            if (d < best.distance)
                best = {pts[i], pts[j], d};
        }
    return best;
}

// Strip check — O(N), at most 7 comparisons per point
inline ClosestPairResult stripCheck(
    std::vector<std::shared_ptr<Location>> strip, double delta)
{
    // Sort strip by Y
    std::sort(strip.begin(), strip.end(),
        [](const std::shared_ptr<Location>& a,
           const std::shared_ptr<Location>& b){ return a->getY() < b->getY(); });

    ClosestPairResult best;
    best.distance = delta;
    int n = static_cast<int>(strip.size());

    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n && (strip[j]->getY() - strip[i]->getY()) < best.distance; ++j) {
            double d = dist(strip[i], strip[j]);
            if (d < best.distance)
                best = {strip[i], strip[j], d};
        }
    return best;
}

inline ClosestPairResult closestRec(
    std::vector<std::shared_ptr<Location>>& pts, int lo, int hi)
{
    if (hi - lo < 3) return bruteForce(pts, lo, hi);

    int mid     = lo + (hi - lo) / 2;
    double midX = pts[mid]->getX();

    auto left  = closestRec(pts, lo,    mid);
    auto right = closestRec(pts, mid+1, hi);

    // Best of two halves
    ClosestPairResult best = (left.distance < right.distance) ? left : right;
    double delta = best.distance;

    // Build strip around midline
    std::vector<std::shared_ptr<Location>> strip;
    for (int i = lo; i <= hi; ++i)
        if (std::abs(pts[i]->getX() - midX) < delta)
            strip.push_back(pts[i]);

    auto stripBest = stripCheck(std::move(strip), delta);
    return (stripBest.valid() && stripBest.distance < best.distance)
           ? stripBest : best;
}

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
//  closestPair  — public API
// ─────────────────────────────────────────────────────────────────────────────
inline ClosestPairResult closestPair(
    std::vector<std::shared_ptr<Location>> locs)
{
    if (locs.size() < 2) return {};

    // Sort by X
    std::sort(locs.begin(), locs.end(),
        [](const std::shared_ptr<Location>& a,
           const std::shared_ptr<Location>& b){ return a->getX() < b->getX(); });

    return detail::closestRec(locs, 0, static_cast<int>(locs.size()) - 1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  allClosePairs
//  Returns all pairs of locations closer than threshold distance.
//  Used to identify delivery batching opportunities.
// ─────────────────────────────────────────────────────────────────────────────
inline std::vector<std::pair<std::shared_ptr<Location>, std::shared_ptr<Location>>>
allClosePairs(const std::vector<std::shared_ptr<Location>>& locs, double threshold)
{
    std::vector<std::pair<std::shared_ptr<Location>,
                          std::shared_ptr<Location>>> result;
    int n = static_cast<int>(locs.size());
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (locs[i]->distanceTo(*locs[j]) <= threshold)
                result.emplace_back(locs[i], locs[j]);
    return result;
}

} // namespace DivideConquer