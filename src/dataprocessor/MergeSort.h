#pragma once
// src/dataprocessor/MergeSort.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <functional>
#include <algorithm>
#include <memory>
#include "../core/Package.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: Sorting
//
//  Merge Sort — stable O(N log N) sort used for:
//    - Sorting deliveries by deadline / priority / weight
//    - Sorting analytics output by completion time
//    - Merging sorted vehicle route results
//
//  Modern C++:
//    - Comparator passed as std::function or lambda
//    - Works on any std::vector<T>
//    - Stable sort preserves relative order of equal elements
// ─────────────────────────────────────────────────────────────────────────────
namespace Sorting {

// ─────────────────────────────────────────────────────────────────────────────
//  mergeSort<T>
//  In-place stable merge sort on a vector slice [lo, hi].
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
void mergeSortRec(std::vector<T>& arr, int lo, int hi,
                  const std::function<bool(const T&, const T&)>& cmp)
{
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    mergeSortRec(arr, lo,    mid, cmp);
    mergeSortRec(arr, mid+1, hi,  cmp);

    // Merge two sorted halves
    std::vector<T> temp;
    temp.reserve(hi - lo + 1);
    int i = lo, j = mid + 1;
    while (i <= mid && j <= hi) {
        // Stable: prefer left on equal (hence !cmp(right, left))
        if (!cmp(arr[j], arr[i])) temp.push_back(arr[i++]);
        else                       temp.push_back(arr[j++]);
    }
    while (i <= mid) temp.push_back(arr[i++]);
    while (j <= hi)  temp.push_back(arr[j++]);

    for (int k = lo; k <= hi; ++k) arr[k] = std::move(temp[k - lo]);
}

template<typename T>
void mergeSort(std::vector<T>& arr,
               std::function<bool(const T&, const T&)> cmp
                   = [](const T& a, const T& b){ return a < b; })
{
    if (arr.size() < 2) return;
    mergeSortRec(arr, 0, static_cast<int>(arr.size()) - 1, cmp);
}

// ─────────────────────────────────────────────────────────────────────────────
//  mergeSorted
//  Merge two already-sorted vectors into one sorted vector — O(N + M).
//  Used when merging route results from different vehicles.
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
std::vector<T> mergeSorted(
    const std::vector<T>& a,
    const std::vector<T>& b,
    std::function<bool(const T&, const T&)> cmp
        = [](const T& x, const T& y){ return x < y; })
{
    std::vector<T> result;
    result.reserve(a.size() + b.size());
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (!cmp(b[j], a[i])) result.push_back(a[i++]);
        else                   result.push_back(b[j++]);
    }
    while (i < a.size()) result.push_back(a[i++]);
    while (j < b.size()) result.push_back(b[j++]);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Convenience: sort packages by deadline (ascending)
// ─────────────────────────────────────────────────────────────────────────────

inline void sortByDeadline(std::vector<std::shared_ptr<Package>>& pkgs) {
    mergeSort<std::shared_ptr<Package>>(pkgs,
        [](const std::shared_ptr<Package>& a,
           const std::shared_ptr<Package>& b) {
            return a->getDeadline() < b->getDeadline();
        });
}

inline void sortByPriority(std::vector<std::shared_ptr<Package>>& pkgs) {
    mergeSort<std::shared_ptr<Package>>(pkgs,
        [](const std::shared_ptr<Package>& a,
           const std::shared_ptr<Package>& b) {
            return a->priorityScore() > b->priorityScore(); // descending
        });
}

inline void sortByWeight(std::vector<std::shared_ptr<Package>>& pkgs) {
    mergeSort<std::shared_ptr<Package>>(pkgs,
        [](const std::shared_ptr<Package>& a,
           const std::shared_ptr<Package>& b) {
            return a->getWeightKg() < b->getWeightKg();
        });
}

} // namespace Sorting