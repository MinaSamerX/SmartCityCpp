#pragma once
// src/spatial/KDTree.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <limits>
#include <ostream>
#include "../core/Location.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Class: KDTree
//
//  2-D K-Dimensional Tree — alternative spatial index to QuadTree.
//
//  Advantages over QuadTree for this system:
//    - Guaranteed O(log N) nearest-neighbor with balanced tree
//    - Alternates split axis (X at even depth, Y at odd depth)
//    - Better for uniformly distributed or clustered data
//
//  Operations:
//    build(locations)          O(N log N)
//    nearestNeighbor(x, y)     O(log N) average
//    queryRadius(x, y, r)      O(log N + k)
//    kNearest(x, y, k)         O(k log N)
//
//  OOP / Modern C++:
//    - Nodes owned by unique_ptr (Rule of Zero)
//    - Shared_ptr<Location> storage (shared with EntityManager)
//    - Build uses median-of-sorted approach for balance
// ─────────────────────────────────────────────────────────────────────────────
class KDTree {
public:
    KDTree()  = default;

    // Build balanced KDTree from a set of locations — O(N log N)
    void build(std::vector<std::shared_ptr<Location>> locs);

    bool empty() const { return m_root == nullptr; }
    int  size()  const { return m_size; }

    // ── Queries ───────────────────────────────────────────────────────────────

    // Single nearest location to query point
    std::shared_ptr<Location> nearestNeighbor(double qx, double qy) const;

    // All locations within radius r
    std::vector<std::shared_ptr<Location>>
    queryRadius(double qx, double qy, double r) const;

    // K closest locations sorted by distance
    std::vector<std::shared_ptr<Location>>
    kNearest(double qx, double qy, int k) const;

    void print(std::ostream& os) const;

private:
    struct Node {
        std::shared_ptr<Location> loc;
        std::unique_ptr<Node>     left;
        std::unique_ptr<Node>     right;
        int                       axis; // 0 = split on X, 1 = split on Y

        explicit Node(std::shared_ptr<Location> l, int ax)
            : loc(std::move(l)), axis(ax) {}
    };

    std::unique_ptr<Node> m_root;
    int                   m_size{0};

    // ── Internal helpers ──────────────────────────────────────────────────────
    std::unique_ptr<Node> buildRec(
        std::vector<std::shared_ptr<Location>>& locs,
        int lo, int hi, int depth);

    void nearestRec(const Node* node,
                    double qx, double qy,
                    double& bestDist2,
                    std::shared_ptr<Location>& bestLoc) const;

    void radiusRec(const Node* node,
                   double qx, double qy, double r2,
                   std::vector<std::shared_ptr<Location>>& result) const;

    // Max-heap entry for kNearest: (dist², location)
    using HeapEntry = std::pair<double, std::shared_ptr<Location>>;

    void kNearestRec(const Node* node,
                     double qx, double qy, int k,
                     std::vector<HeapEntry>& maxHeap) const;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Implementation (header-only for templates; inline for this class)
// ─────────────────────────────────────────────────────────────────────────────

inline void KDTree::build(std::vector<std::shared_ptr<Location>> locs) {
    m_size = static_cast<int>(locs.size());
    m_root = buildRec(locs, 0, static_cast<int>(locs.size()) - 1, 0);
}

inline std::unique_ptr<KDTree::Node>
KDTree::buildRec(std::vector<std::shared_ptr<Location>>& locs,
                 int lo, int hi, int depth)
{
    if (lo > hi) return nullptr;
    int axis = depth % 2; // 0=X, 1=Y

    // Partial sort to find median
    int mid = lo + (hi - lo) / 2;
    std::nth_element(locs.begin() + lo, locs.begin() + mid, locs.begin() + hi + 1,
        [axis](const std::shared_ptr<Location>& a,
               const std::shared_ptr<Location>& b) {
            return axis == 0 ? a->getX() < b->getX()
                             : a->getY() < b->getY();
        });

    auto node = std::make_unique<Node>(locs[mid], axis);
    node->left  = buildRec(locs, lo,    mid - 1, depth + 1);
    node->right = buildRec(locs, mid + 1, hi,    depth + 1);
    return node;
}

inline std::shared_ptr<Location>
KDTree::nearestNeighbor(double qx, double qy) const {
    double bestDist2 = std::numeric_limits<double>::max();
    std::shared_ptr<Location> bestLoc;
    nearestRec(m_root.get(), qx, qy, bestDist2, bestLoc);
    return bestLoc;
}

inline void KDTree::nearestRec(const Node* node,
                                double qx, double qy,
                                double& bestDist2,
                                std::shared_ptr<Location>& bestLoc) const
{
    if (!node) return;

    double dx = node->loc->getX() - qx;
    double dy = node->loc->getY() - qy;
    double d2 = dx*dx + dy*dy;
    if (d2 < bestDist2) { bestDist2 = d2; bestLoc = node->loc; }

    // Decide which side to search first (side containing query point)
    double diff  = (node->axis == 0) ? (qx - node->loc->getX())
                                     : (qy - node->loc->getY());
    auto* first  = (diff <= 0) ? node->left.get()  : node->right.get();
    auto* second = (diff <= 0) ? node->right.get() : node->left.get();

    nearestRec(first, qx, qy, bestDist2, bestLoc);

    // Only visit other side if splitting hyperplane could contain closer point
    if (diff * diff < bestDist2)
        nearestRec(second, qx, qy, bestDist2, bestLoc);
}

inline std::vector<std::shared_ptr<Location>>
KDTree::queryRadius(double qx, double qy, double r) const {
    std::vector<std::shared_ptr<Location>> result;
    radiusRec(m_root.get(), qx, qy, r*r, result);
    return result;
}

inline void KDTree::radiusRec(const Node* node,
                               double qx, double qy, double r2,
                               std::vector<std::shared_ptr<Location>>& result) const
{
    if (!node) return;
    double dx = node->loc->getX()-qx, dy = node->loc->getY()-qy;
    if (dx*dx + dy*dy <= r2) result.push_back(node->loc);

    double diff = (node->axis == 0) ? (qx - node->loc->getX())
                                    : (qy - node->loc->getY());
    auto* first  = (diff <= 0) ? node->left.get()  : node->right.get();
    auto* second = (diff <= 0) ? node->right.get() : node->left.get();

    radiusRec(first, qx, qy, r2, result);
    if (diff * diff <= r2)
        radiusRec(second, qx, qy, r2, result);
}

inline std::vector<std::shared_ptr<Location>>
KDTree::kNearest(double qx, double qy, int k) const {
    std::vector<HeapEntry> maxHeap; // max-heap by distance
    kNearestRec(m_root.get(), qx, qy, k, maxHeap);

    std::vector<std::shared_ptr<Location>> result;
    result.reserve(maxHeap.size());
    // Sort ascending by distance
    std::sort(maxHeap.begin(), maxHeap.end(),
        [](const HeapEntry& a, const HeapEntry& b){ return a.first < b.first; });
    for (auto& e : maxHeap) result.push_back(e.second);
    return result;
}

inline void KDTree::kNearestRec(const Node* node,
                                 double qx, double qy, int k,
                                 std::vector<HeapEntry>& maxHeap) const
{
    if (!node) return;
    double dx = node->loc->getX()-qx, dy = node->loc->getY()-qy;
    double d2 = dx*dx + dy*dy;

    auto cmp = [](const HeapEntry& a, const HeapEntry& b){ return a.first < b.first; };

    if (static_cast<int>(maxHeap.size()) < k) {
        maxHeap.push_back({d2, node->loc});
        std::push_heap(maxHeap.begin(), maxHeap.end(), cmp);
    } else if (d2 < maxHeap.front().first) {
        std::pop_heap(maxHeap.begin(), maxHeap.end(), cmp);
        maxHeap.back() = {d2, node->loc};
        std::push_heap(maxHeap.begin(), maxHeap.end(), cmp);
    }

    double diff  = (node->axis == 0) ? (qx - node->loc->getX())
                                     : (qy - node->loc->getY());
    auto* first  = (diff <= 0) ? node->left.get()  : node->right.get();
    auto* second = (diff <= 0) ? node->right.get() : node->left.get();

    kNearestRec(first, qx, qy, k, maxHeap);

    double worstBest = maxHeap.empty() ? std::numeric_limits<double>::max()
                                       : maxHeap.front().first;
    if (static_cast<int>(maxHeap.size()) < k || diff*diff < worstBest)
        kNearestRec(second, qx, qy, k, maxHeap);
}

inline void KDTree::print(std::ostream& os) const {
    os << "KDTree[size=" << m_size << "]\n";
}