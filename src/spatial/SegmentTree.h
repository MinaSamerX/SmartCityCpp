#pragma once
#include <limits>
// src/spatial/SegmentTree.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <limits>
#include <functional>
#include <stdexcept>
#include <ostream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Class: SegmentTree<T>
//
//  Generic segment tree for range queries and point updates.
//
//  Purpose in this system:
//    - Store traffic density / congestion values per road segment index
//    - Query: total / max / min congestion in any road index range [l, r]
//    - Update: set new congestion value when TrafficMonitor fires an event
//
//  Complexity:
//    build:          O(N)
//    pointUpdate:    O(log N)
//    rangeQuery:     O(log N)
//
//  OOP / Modern C++:
//    - T is the value type (double for congestion weight)
//    - Combine function is a template policy (sum, max, min — all supported)
//    - Identity value configurable (0 for sum, -INF for max)
//    - Internal flat array representation (2N nodes)
// ─────────────────────────────────────────────────────────────────────────────
template<typename T = double>
class SegmentTree {
public:
    using CombineFn = std::function<T(T, T)>;

    // ── Construction ─────────────────────────────────────────────────────────
    SegmentTree() = default;

    // Build from initial values with custom combine and identity
    SegmentTree(std::vector<T>  values,
                CombineFn       combine  = [](T a, T b){ return a + b; },
                T               identity = T{})
        : m_n(static_cast<int>(values.size()))
        , m_combine(std::move(combine))
        , m_identity(identity)
        , m_tree(4 * m_n, identity)
    {
        if (m_n > 0) buildRec(values, 1, 0, m_n - 1);
    }

    // Named constructors for common use cases
    static SegmentTree<T> makeSumTree(const std::vector<T>& v) {
        return SegmentTree<T>(v,
            [](T a, T b){ return a + b; }, T{0});
    }

    static SegmentTree<T> makeMaxTree(const std::vector<T>& v) {
        return SegmentTree<T>(v,
            [](T a, T b){ return std::max(a, b); },
            std::numeric_limits<T>::lowest());
    }

    static SegmentTree<T> makeMinTree(const std::vector<T>& v) {
        return SegmentTree<T>(v,
            [](T a, T b){ return std::min(a, b); },
            std::numeric_limits<T>::max());
    }

    // ── Operations ────────────────────────────────────────────────────────────

    // Update value at index i to newVal — O(log N)
    void update(int i, T newVal) {
        if (i < 0 || i >= m_n)
            throw std::out_of_range("SegmentTree::update index out of range");
        updateRec(1, 0, m_n - 1, i, newVal);
    }

    // Query combined value over range [l, r] — O(log N)
    T query(int l, int r) const {
        if (l < 0 || r >= m_n || l > r)
            throw std::out_of_range("SegmentTree::query range invalid");
        return queryRec(1, 0, m_n - 1, l, r);
    }

    // Point query
    T at(int i) const { return query(i, i); }

    int  size()  const { return m_n;      }
    bool empty() const { return m_n == 0; }

    // Rebuild from new values (O(N))
    void rebuild(const std::vector<T>& values) {
        m_n    = static_cast<int>(values.size());
        m_tree.assign(4 * m_n, m_identity);
        if (m_n > 0) buildRec(values, 1, 0, m_n - 1);
    }

    void print(std::ostream& os) const {
        os << "SegmentTree[n=" << m_n << "] values: ";
        for (int i = 0; i < m_n; ++i) os << at(i) << " ";
        os << "\n";
    }

private:
    int        m_n{0};
    CombineFn  m_combine;
    T          m_identity{};
    std::vector<T> m_tree;

    void buildRec(const std::vector<T>& vals, int node, int lo, int hi) {
        if (lo == hi) { m_tree[node] = vals[lo]; return; }
        int mid = lo + (hi - lo) / 2;
        buildRec(vals, 2*node,   lo,    mid);
        buildRec(vals, 2*node+1, mid+1, hi);
        m_tree[node] = m_combine(m_tree[2*node], m_tree[2*node+1]);
    }

    void updateRec(int node, int lo, int hi, int idx, T val) {
        if (lo == hi) { m_tree[node] = val; return; }
        int mid = lo + (hi - lo) / 2;
        if (idx <= mid) updateRec(2*node,   lo,    mid, idx, val);
        else            updateRec(2*node+1, mid+1, hi,  idx, val);
        m_tree[node] = m_combine(m_tree[2*node], m_tree[2*node+1]);
    }

    T queryRec(int node, int lo, int hi, int l, int r) const {
        if (r < lo || hi < l) return m_identity;
        if (l <= lo && hi <= r) return m_tree[node];
        int mid = lo + (hi - lo) / 2;
        return m_combine(queryRec(2*node,   lo,    mid, l, r),
                         queryRec(2*node+1, mid+1, hi,  l, r));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Alias: TrafficDensityTree
//  Stores congestion level per road-segment index.
//  Max query → find most congested segment in range.
// ─────────────────────────────────────────────────────────────────────────────
using TrafficDensityTree = SegmentTree<double>;