#pragma once
// src/scheduler/PriorityQueue.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <ostream>
#include <memory>
#include <algorithm>
#include "../core/Package.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Class: PriorityQueue<T, Comparator>
//
//  Generic binary heap with O(log N) insert, extract, and updatePriority.
//
//  Template params:
//    T          — element type (must be copyable)
//    KeyFn      — callable: T → int (extracts unique ID for decrease-key)
//    Comparator — callable: (T,T) → bool  (default: max-heap by operator<)
//
//  Operations:
//    push(item)              O(log N)
//    top()                   O(1)
//    pop()                   O(log N)
//    updatePriority(id, val) O(log N)  ← decrease/increase key
//    contains(id)            O(1)
//
//  OOP / Modern C++:
//    - Comparator is a template policy (no virtual overhead)
//    - KeyFn extracts stable ID from element (for position tracking)
//    - Position map (id → heap index) enables O(log N) updatePriority
//    - Lambda comparators fully supported
// ─────────────────────────────────────────────────────────────────────────────
template<
    typename T,
    typename KeyFn     = std::function<int(const T&)>,
    typename Comparator = std::function<bool(const T&, const T&)>
>
class PriorityQueue {
public:
    // ── Construction ─────────────────────────────────────────────────────────
    explicit PriorityQueue(
        KeyFn     keyFn  = [](const T& t) -> int { return static_cast<int>(t); },
        Comparator cmp   = [](const T& a, const T& b) { return a < b; })
        : m_keyFn(std::move(keyFn))
        , m_cmp(std::move(cmp))
    {}

    // ── Core Operations ───────────────────────────────────────────────────────

    void push(const T& item) {
        int id  = m_keyFn(item);
        // If already present, update in place
        auto it = m_pos.find(id);
        if (it != m_pos.end()) {
            m_heap[it->second] = item;
            heapifyUp(it->second);
            heapifyDown(it->second);
            return;
        }
        m_heap.push_back(item);
        int idx = static_cast<int>(m_heap.size()) - 1;
        m_pos[id] = idx;
        heapifyUp(idx);
    }

    const T& top() const {
        if (empty()) throw std::out_of_range("PriorityQueue is empty");
        return m_heap[0];
    }

    void pop() {
        if (empty()) throw std::out_of_range("PriorityQueue is empty");
        int topId = m_keyFn(m_heap[0]);
        m_pos.erase(topId);

        if (m_heap.size() == 1) { m_heap.pop_back(); return; }

        // Move last element to root
        m_heap[0] = std::move(m_heap.back());
        m_heap.pop_back();
        m_pos[m_keyFn(m_heap[0])] = 0;
        heapifyDown(0);
    }

    // Update an existing element's priority by replacing with newItem.
    // newItem must have the same key (ID) as the existing one.
    void updatePriority(int id, const T& newItem) {
        auto it = m_pos.find(id);
        if (it == m_pos.end()) { push(newItem); return; }

        int idx       = it->second;
        m_heap[idx]   = newItem;
        heapifyUp(idx);
        heapifyDown(idx);
    }

    // Remove element by ID
    bool remove(int id) {
        auto it = m_pos.find(id);
        if (it == m_pos.end()) return false;

        int idx      = it->second;
        int lastIdx  = static_cast<int>(m_heap.size()) - 1;
        m_pos.erase(it);

        if (idx == lastIdx) { m_heap.pop_back(); return true; }

        m_heap[idx] = std::move(m_heap.back());
        m_heap.pop_back();
        m_pos[m_keyFn(m_heap[idx])] = idx;
        heapifyUp(idx);
        heapifyDown(idx);
        return true;
    }

    bool        contains(int id) const { return m_pos.count(id) > 0; }
    bool        empty()          const { return m_heap.empty();       }
    std::size_t size()           const { return m_heap.size();        }

    // Non-destructive iteration — copy of all elements (unsorted)
    std::vector<T> toVector() const { return m_heap; }

    void print(std::ostream& os) const {
        os << "PriorityQueue[size=" << m_heap.size() << "]: ";
        for (const auto& t : m_heap) os << t << " ";
        os << "\n";
    }

private:
    std::vector<T>              m_heap;
    std::unordered_map<int,int> m_pos;   // id → heap index
    KeyFn                       m_keyFn;
    Comparator                  m_cmp;

    // ── Heap helpers ──────────────────────────────────────────────────────────
    int parent(int i)  const { return (i - 1) / 2; }
    int leftChild(int i)  const { return 2 * i + 1; }
    int rightChild(int i) const { return 2 * i + 2; }

    void heapifyUp(int idx) {
        while (idx > 0) {
            int p = parent(idx);
            if (m_cmp(m_heap[p], m_heap[idx])) {
                swapNodes(idx, p);
                idx = p;
            } else break;
        }
    }

    void heapifyDown(int idx) {
        int n = static_cast<int>(m_heap.size());
        while (true) {
            int best  = idx;
            int left  = leftChild(idx);
            int right = rightChild(idx);

            if (left  < n && m_cmp(m_heap[best], m_heap[left]))  best = left;
            if (right < n && m_cmp(m_heap[best], m_heap[right])) best = right;

            if (best == idx) break;
            swapNodes(idx, best);
            idx = best;
        }
    }

    void swapNodes(int i, int j) {
        m_pos[m_keyFn(m_heap[i])] = j;
        m_pos[m_keyFn(m_heap[j])] = i;
        std::swap(m_heap[i], m_heap[j]);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Convenience aliases used throughout the system
// ─────────────────────────────────────────────────────────────────────────────

// Max-heap: higher priorityScore() = served first
using DeliveryQueue = PriorityQueue<
    std::shared_ptr<Package>,
    std::function<int(const std::shared_ptr<Package>&)>,
    std::function<bool(const std::shared_ptr<Package>&,
                       const std::shared_ptr<Package>&)>
>;

inline DeliveryQueue makeDeliveryQueue() {
    return DeliveryQueue(
        [](const std::shared_ptr<Package>& p) { return p->getId(); },
        [](const std::shared_ptr<Package>& a,
           const std::shared_ptr<Package>& b) {
            return a->priorityScore() < b->priorityScore(); // max-heap
        }
    );
}