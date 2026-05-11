#pragma once
// src/dataprocessor/QuickSort.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <functional>
#include <algorithm>
#include <random>
#include <memory>
#include "../core/Vehicle.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: Sorting  (extends MergeSort.h)
//
//  Quick Sort — average O(N log N), in-place, cache-friendly.
//  Used for large vehicle/package datasets where stability is not required.
//
//  Features:
//    - Randomised pivot (avoids worst-case O(N²) on sorted input)
//    - 3-way partition (handles duplicate keys efficiently)
//    - Falls back to insertion sort for small subarrays (< 16 elements)
// ─────────────────────────────────────────────────────────────────────────────
namespace Sorting {

namespace detail {

// ── Insertion sort for small subarrays ────────────────────────────────────────
template<typename T>
void insertionSort(std::vector<T>& arr, int lo, int hi,
                   const std::function<bool(const T&, const T&)>& cmp)
{
    for (int i = lo + 1; i <= hi; ++i) {
        T key = arr[i];
        int j = i - 1;
        while (j >= lo && cmp(key, arr[j])) {
            arr[j + 1] = std::move(arr[j]);
            --j;
        }
        arr[j + 1] = std::move(key);
    }
}

// ── 3-way partition (Dutch National Flag) ─────────────────────────────────────
// Partitions arr[lo..hi] into: < pivot | == pivot | > pivot
// Returns {lt, gt} — equal-element range [lt, gt]
template<typename T>
std::pair<int,int> partition3(std::vector<T>& arr, int lo, int hi,
                               const std::function<bool(const T&, const T&)>& cmp)
{
    // Randomised pivot
    static std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(lo, hi);
    std::swap(arr[dist(rng)], arr[hi]);

    T& pivot = arr[hi];
    int lt = lo, gt = hi, i = lo;

    while (i <= gt) {
        if      (cmp(arr[i], pivot))  std::swap(arr[lt++], arr[i++]);
        else if (cmp(pivot,  arr[i])) std::swap(arr[i],    arr[gt--]);
        else                          ++i;
    }
    return {lt, gt};
}

template<typename T>
void quickSortRec(std::vector<T>& arr, int lo, int hi,
                  const std::function<bool(const T&, const T&)>& cmp)
{
    if (lo >= hi) return;

    // Insertion sort for small subarrays
    if (hi - lo < 16) {
        insertionSort(arr, lo, hi, cmp);
        return;
    }

    auto [lt, gt] = partition3(arr, lo, hi, cmp);
    quickSortRec(arr, lo,    lt - 1, cmp);
    quickSortRec(arr, gt + 1, hi,   cmp);
}

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
//  quickSort<T>  — public API
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
void quickSort(std::vector<T>& arr,
               std::function<bool(const T&, const T&)> cmp
                   = [](const T& a, const T& b){ return a < b; })
{
    if (arr.size() < 2) return;
    detail::quickSortRec(arr, 0, static_cast<int>(arr.size()) - 1, cmp);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Convenience: sort vehicles by remaining capacity (descending)
// ─────────────────────────────────────────────────────────────────────────────

inline void sortVehiclesByCapacity(std::vector<std::shared_ptr<Vehicle>>& vehicles) {
    quickSort<std::shared_ptr<Vehicle>>(vehicles,
        [](const std::shared_ptr<Vehicle>& a,
           const std::shared_ptr<Vehicle>& b) {
            return a->getRemainingCapacity() > b->getRemainingCapacity();
        });
}

inline void sortVehiclesByDistance(
    std::vector<std::shared_ptr<Vehicle>>& vehicles,
    double qx, double qy,
    const std::function<std::pair<double,double>(int)>& getCoords)
{
    quickSort<std::shared_ptr<Vehicle>>(vehicles,
        [&](const std::shared_ptr<Vehicle>& a,
            const std::shared_ptr<Vehicle>& b) {
            auto [ax, ay] = getCoords(a->getCurrentLocationId());
            auto [bx, by] = getCoords(b->getCurrentLocationId());
            double da = (ax-qx)*(ax-qx) + (ay-qy)*(ay-qy);
            double db = (bx-qx)*(bx-qx) + (by-qy)*(by-qy);
            return da < db;
        });
}

} // namespace Sorting