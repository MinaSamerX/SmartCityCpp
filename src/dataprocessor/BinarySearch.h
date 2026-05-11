#pragma once
// src/dataprocessor/BinarySearch.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include <ctime>
#include "../core/Package.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: Search
//
//  Binary search variants for sorted containers.
//
//  Use-cases in this system:
//    - Search sorted delivery schedules by time slot
//    - Find insertion point for new delivery in sorted queue
//    - Locate packages with deadline in a range
//    - Find first/last occurrence for range queries
//
//  All functions assume the input vector is sorted according to `cmp`.
//  Complexity: O(log N)
// ─────────────────────────────────────────────────────────────────────────────
namespace Search {

// ─────────────────────────────────────────────────────────────────────────────
//  binarySearch
//  Returns index of target if found, -1 otherwise.
// ─────────────────────────────────────────────────────────────────────────────
template<typename T, typename Key>
int binarySearch(const std::vector<T>& arr,
                 const Key& target,
                 std::function<Key(const T&)> keyFn,
                 std::function<bool(const Key&, const Key&)> cmp
                     = [](const Key& a, const Key& b){ return a < b; })
{
    int lo = 0, hi = static_cast<int>(arr.size()) - 1;
    while (lo <= hi) {
        int  mid = lo + (hi - lo) / 2;
        Key  mk  = keyFn(arr[mid]);
        if      (!cmp(mk, target) && !cmp(target, mk)) return mid; // equal
        else if (cmp(mk, target))                       lo = mid + 1;
        else                                            hi = mid - 1;
    }
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
//  lowerBound
//  Returns index of first element where key >= target.
//  If all elements < target, returns arr.size().
// ─────────────────────────────────────────────────────────────────────────────
template<typename T, typename Key>
int lowerBound(const std::vector<T>& arr,
               const Key& target,
               std::function<Key(const T&)> keyFn)
{
    int lo = 0, hi = static_cast<int>(arr.size());
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (keyFn(arr[mid]) < target) lo = mid + 1;
        else                          hi = mid;
    }
    return lo;
}

// ─────────────────────────────────────────────────────────────────────────────
//  upperBound
//  Returns index of first element where key > target.
// ─────────────────────────────────────────────────────────────────────────────
template<typename T, typename Key>
int upperBound(const std::vector<T>& arr,
               const Key& target,
               std::function<Key(const T&)> keyFn)
{
    int lo = 0, hi = static_cast<int>(arr.size());
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (!(target < keyFn(arr[mid]))) lo = mid + 1;
        else                             hi = mid;
    }
    return lo;
}

// ─────────────────────────────────────────────────────────────────────────────
//  rangeSearch
//  Returns all elements with key in [lo, hi] from a sorted array.
//  Uses lowerBound + upperBound — O(log N + k).
// ─────────────────────────────────────────────────────────────────────────────
template<typename T, typename Key>
std::vector<T> rangeSearch(const std::vector<T>& arr,
                            const Key& lo, const Key& hi,
                            std::function<Key(const T&)> keyFn)
{
    int left  = lowerBound<T, Key>(arr, lo, keyFn);
    int right = upperBound<T, Key>(arr, hi, keyFn);
    return std::vector<T>(arr.begin() + left, arr.begin() + right);
}

// ─────────────────────────────────────────────────────────────────────────────
//  findOptimalTimeSlot
//  Given a sorted list of occupied time slots (as time_t pairs start,end)
//  and a required duration, returns the earliest available start time.
//  Returns -1 if no slot found within the horizon.
// ─────────────────────────────────────────────────────────────────────────────
inline long long findOptimalTimeSlot(
    const std::vector<std::pair<long long, long long>>& occupiedSlots,
    long long  duration,
    long long  earliest,
    long long  horizon)
{
    long long candidate = earliest;

    // Slots assumed sorted by start time
    for (const auto& [start, end] : occupiedSlots) {
        if (candidate + duration <= start) return candidate; // gap fits
        if (end > candidate) candidate = end;               // advance past slot
    }
    return (candidate + duration <= horizon) ? candidate : -1LL;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Convenience: search packages sorted by deadline
// ─────────────────────────────────────────────────────────────────────────────

inline int findPackageByDeadline(
    const std::vector<std::shared_ptr<Package>>& sorted,
    std::time_t deadline)
{
    return binarySearch<std::shared_ptr<Package>, std::time_t>(
        sorted,
        deadline,
        std::function<std::time_t(const std::shared_ptr<Package>&)>(
            [](const std::shared_ptr<Package>& p){ return p->getDeadline(); }
        )
    );
}

} // namespace Search