#pragma once
// src/hashtable/HashTable.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <list>
#include <utility>
#include <functional>
#include <stdexcept>
#include <ostream>
#include <algorithm>
#include "HashFunctions.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Class: HashTable<K, V, Hasher>
//
//  Custom generic hash table using SEPARATE CHAINING for collision resolution.
//
//  Design:
//    - Each bucket holds a std::list<pair<K,V>> (chain)
//    - Automatic rehash when load factor exceeds threshold
//    - Template Hasher parameter — defaults to HashFn::IntHash
//      but callers pass HashFn::StringHash, HashFn::TrackingHash, etc.
//
//  Complexity:
//    - insert / search / remove:  O(1) average,  O(n) worst
//    - rehash:                    O(n) amortised
//
//  OOP:
//    - Rule of Five: custom destructor not needed (no raw resources)
//      → Rule of Zero applies — STL handles memory
//    - Iterator support via forEach lambda (C++11 style)
//    - Template specialisation friendly (Hasher is a policy)
// ─────────────────────────────────────────────────────────────────────────────
template<typename K,
         typename V,
         typename Hasher = HashFn::IntHash>
class HashTable {
public:
    // ── Types ─────────────────────────────────────────────────────────────────
    using Bucket   = std::list<std::pair<K, V>>;
    using BucketVec = std::vector<Bucket>;

    // ── Construction ─────────────────────────────────────────────────────────
    explicit HashTable(std::size_t initialCapacity = 16,
                       double      maxLoadFactor   = 0.75)
        : m_buckets(initialCapacity)
        , m_size(0)
        , m_maxLoadFactor(maxLoadFactor)
    {}

    HashTable(const HashTable&)             = default;
    HashTable& operator=(const HashTable&)  = default;
    HashTable(HashTable&&)                  = default;
    HashTable& operator=(HashTable&&)       = default;

    // ── Core Operations ───────────────────────────────────────────────────────

    // Insert or overwrite key → value
    void insert(const K& key, const V& value) {
        rehashIfNeeded();
        std::size_t idx = bucketIndex(key);
        for (auto& kv : m_buckets[idx]) {
            if (kv.first == key) {
                kv.second = value;   // overwrite existing
                return;
            }
        }
        m_buckets[idx].emplace_back(key, value);
        ++m_size;
    }

    // Returns pointer to value, or nullptr if not found — O(1) avg
    V* search(const K& key) {
        std::size_t idx = bucketIndex(key);
        for (auto& kv : m_buckets[idx])
            if (kv.first == key) return &kv.second;
        return nullptr;
    }

    const V* search(const K& key) const {
        std::size_t idx = bucketIndex(key);
        for (const auto& kv : m_buckets[idx])
            if (kv.first == key) return &kv.second;
        return nullptr;
    }

    // Returns true if key exists
    bool contains(const K& key) const {
        return search(key) != nullptr;
    }

    // Remove key — returns true if removed, false if not found
    bool remove(const K& key) {
        std::size_t idx = bucketIndex(key);
        auto& bucket = m_buckets[idx];
        auto  it = std::find_if(bucket.begin(), bucket.end(),
                    [&key](const std::pair<K,V>& kv){ return kv.first == key; });
        if (it == bucket.end()) return false;
        bucket.erase(it);
        --m_size;
        return true;
    }

    // operator[] — inserts default value if key missing (mirrors std::map)
    V& operator[](const K& key) {
        rehashIfNeeded();
        std::size_t idx = bucketIndex(key);
        for (auto& kv : m_buckets[idx])
            if (kv.first == key) return kv.second;
        m_buckets[idx].emplace_back(key, V{});
        ++m_size;
        return m_buckets[idx].back().second;
    }

    // ── Iteration ─────────────────────────────────────────────────────────────

    // Call visitor(key, value) for every entry — C++11 lambda friendly
    void forEach(std::function<void(const K&, V&)> visitor) {
        for (auto& bucket : m_buckets)
            for (auto& kv : bucket)
                visitor(kv.first, kv.second);
    }

    void forEach(std::function<void(const K&, const V&)> visitor) const {
        for (const auto& bucket : m_buckets)
            for (const auto& kv : bucket)
                visitor(kv.first, kv.second);
    }

    // Collect all keys into a vector
    std::vector<K> keys() const {
        std::vector<K> result;
        result.reserve(m_size);
        forEach([&result](const K& k, const V&){ result.push_back(k); });
        return result;
    }

    // Collect all values into a vector
    std::vector<V> values() const {
        std::vector<V> result;
        result.reserve(m_size);
        forEach([&result](const K&, const V& v){ result.push_back(v); });
        return result;
    }

    // ── Metrics ───────────────────────────────────────────────────────────────
    std::size_t size()       const { return m_size;                              }
    bool        empty()      const { return m_size == 0;                         }
    std::size_t capacity()   const { return m_buckets.size();                    }
    double      loadFactor() const {
        return m_buckets.empty() ? 0.0
             : static_cast<double>(m_size) / m_buckets.size();
    }

    void clear() {
        for (auto& b : m_buckets) b.clear();
        m_size = 0;
    }

    // Diagnostic: longest chain (worst-case lookup cost)
    std::size_t maxChainLength() const {
        std::size_t mx = 0;
        for (const auto& b : m_buckets)
            if (b.size() > mx) mx = b.size();
        return mx;
    }

    void printStats(std::ostream& os) const {
        os << "HashTable[chaining] size=" << m_size
           << " capacity=" << capacity()
           << " loadFactor=" << loadFactor()
           << " maxChain=" << maxChainLength() << "\n";
    }

private:
    BucketVec   m_buckets;
    std::size_t m_size;
    double      m_maxLoadFactor;
    Hasher      m_hasher;

    std::size_t bucketIndex(const K& key) const {
        return m_hasher(key) % m_buckets.size();
    }

    void rehashIfNeeded() {
        if (loadFactor() < m_maxLoadFactor) return;

        std::size_t newCap = m_buckets.size() * 2 + 1;
        BucketVec   newBuckets(newCap);

        for (auto& bucket : m_buckets) {
            for (auto& kv : bucket) {
                std::size_t idx = m_hasher(kv.first) % newCap;
                newBuckets[idx].push_back(std::move(kv));
            }
        }
        m_buckets = std::move(newBuckets);
    }
};