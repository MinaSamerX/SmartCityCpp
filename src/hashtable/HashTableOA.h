#pragma once
// src/hashtable/HashTableOA.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <functional>
#include <ostream>
#include <stdexcept>
#include "HashFunctions.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Class: HashTableOA<K, V, Hasher>
//
//  Custom hash table using OPEN ADDRESSING with LINEAR PROBING.
//
//  Design:
//    - Flat array of slots — better cache locality than chaining
//    - Each slot has a State: EMPTY, OCCUPIED, DELETED (tombstone)
//    - Deletion uses tombstones so probe sequences stay intact
//    - Rehash triggered when load factor exceeds threshold
//    - Primary hash + step-1 probing: index = (h + i) % cap
//
//  Complexity:
//    - insert / search / remove:  O(1) average
//    - Worst case (high load):    O(n) — avoided by rehash at 0.6
//
//  When to use vs HashTable (chaining):
//    - HashTableOA: small fixed-type keys (int IDs), cache-critical lookups
//    - HashTable:   variable-length keys (strings), low-collision tolerance
// ─────────────────────────────────────────────────────────────────────────────
template<typename K,
         typename V,
         typename Hasher = HashFn::IntHash>
class HashTableOA {
private:
    // ── Slot state ────────────────────────────────────────────────────────────
    enum class State : uint8_t { EMPTY, OCCUPIED, DELETED };

    struct Slot {
        K     key{};
        V     value{};
        State state{State::EMPTY};
    };

public:
    // ── Construction ─────────────────────────────────────────────────────────
    explicit HashTableOA(std::size_t initialCapacity = 17,
                         double      maxLoadFactor   = 0.60)
        : m_slots(initialCapacity)
        , m_size(0)
        , m_maxLoadFactor(maxLoadFactor)
    {}

    HashTableOA(const HashTableOA&)             = default;
    HashTableOA& operator=(const HashTableOA&)  = default;
    HashTableOA(HashTableOA&&)                  = default;
    HashTableOA& operator=(HashTableOA&&)       = default;

    // ── Core Operations ───────────────────────────────────────────────────────

    // Insert or overwrite key → value
    void insert(const K& key, const V& value) {
        rehashIfNeeded();

        std::size_t idx      = startIndex(key);
        std::size_t cap      = m_slots.size();
        std::size_t tombstone = cap;   // first available tombstone slot

        for (std::size_t i = 0; i < cap; ++i) {
            std::size_t probe = (idx + i) % cap;
            Slot& slot = m_slots[probe];

            if (slot.state == State::OCCUPIED && slot.key == key) {
                slot.value = value;   // overwrite
                return;
            }
            if (slot.state == State::DELETED && tombstone == cap) {
                tombstone = probe;    // remember first tombstone
            }
            if (slot.state == State::EMPTY) {
                // Insert at tombstone if found, else here
                std::size_t insertAt = (tombstone < cap) ? tombstone : probe;
                m_slots[insertAt] = {key, value, State::OCCUPIED};
                ++m_size;
                return;
            }
        }
        // Table full — fallback insert at tombstone
        if (tombstone < cap) {
            m_slots[tombstone] = {key, value, State::OCCUPIED};
            ++m_size;
        }
    }

    // Returns pointer to value, nullptr if not found
    V* search(const K& key) {
        std::size_t idx = startIndex(key);
        std::size_t cap = m_slots.size();
        for (std::size_t i = 0; i < cap; ++i) {
            std::size_t probe = (idx + i) % cap;
            Slot& slot = m_slots[probe];
            if (slot.state == State::EMPTY)    return nullptr;
            if (slot.state == State::OCCUPIED && slot.key == key)
                return &slot.value;
        }
        return nullptr;
    }

    const V* search(const K& key) const {
        std::size_t idx = startIndex(key);
        std::size_t cap = m_slots.size();
        for (std::size_t i = 0; i < cap; ++i) {
            std::size_t probe = (idx + i) % cap;
            const Slot& slot = m_slots[probe];
            if (slot.state == State::EMPTY)    return nullptr;
            if (slot.state == State::OCCUPIED && slot.key == key)
                return &slot.value;
        }
        return nullptr;
    }

    bool contains(const K& key) const { return search(key) != nullptr; }

    // Tombstone deletion — O(1) amortised
    bool remove(const K& key) {
        std::size_t idx = startIndex(key);
        std::size_t cap = m_slots.size();
        for (std::size_t i = 0; i < cap; ++i) {
            std::size_t probe = (idx + i) % cap;
            Slot& slot = m_slots[probe];
            if (slot.state == State::EMPTY)  return false;
            if (slot.state == State::OCCUPIED && slot.key == key) {
                slot.state = State::DELETED;  // tombstone
                --m_size;
                return true;
            }
        }
        return false;
    }

    // operator[] — inserts default if missing
    V& operator[](const K& key) {
        V* found = search(key);
        if (found) return *found;
        insert(key, V{});
        return *search(key);
    }

    // ── Iteration ─────────────────────────────────────────────────────────────
    void forEach(std::function<void(const K&, V&)> visitor) {
        for (auto& slot : m_slots)
            if (slot.state == State::OCCUPIED)
                visitor(slot.key, slot.value);
    }

    void forEach(std::function<void(const K&, const V&)> visitor) const {
        for (const auto& slot : m_slots)
            if (slot.state == State::OCCUPIED)
                visitor(slot.key, slot.value);
    }

    std::vector<K> keys() const {
        std::vector<K> result;
        result.reserve(m_size);
        forEach([&result](const K& k, const V&){ result.push_back(k); });
        return result;
    }

    // ── Metrics ───────────────────────────────────────────────────────────────
    std::size_t size()       const { return m_size;                }
    bool        empty()      const { return m_size == 0;           }
    std::size_t capacity()   const { return m_slots.size();        }
    double      loadFactor() const {
        return m_slots.empty() ? 0.0
             : static_cast<double>(m_size) / m_slots.size();
    }

    void clear() {
        for (auto& s : m_slots) s.state = State::EMPTY;
        m_size = 0;
    }

    void printStats(std::ostream& os) const {
        std::size_t del = 0;
        for (const auto& s : m_slots)
            if (s.state == State::DELETED) ++del;
        os << "HashTableOA[open-addr] size=" << m_size
           << " capacity=" << capacity()
           << " loadFactor=" << loadFactor()
           << " tombstones=" << del << "\n";
    }

private:
    std::vector<Slot> m_slots;
    std::size_t       m_size;
    double            m_maxLoadFactor;
    Hasher            m_hasher;

    std::size_t startIndex(const K& key) const {
        return m_hasher(key) % m_slots.size();
    }

    void rehashIfNeeded() {
        if (loadFactor() < m_maxLoadFactor) return;

        std::size_t newCap = m_slots.size() * 2 + 1;
        std::vector<Slot> newSlots(newCap);

        for (const auto& slot : m_slots) {
            if (slot.state != State::OCCUPIED) continue;
            // Re-insert into new table
            std::size_t idx = m_hasher(slot.key) % newCap;
            for (std::size_t i = 0; i < newCap; ++i) {
                std::size_t probe = (idx + i) % newCap;
                if (newSlots[probe].state == State::EMPTY) {
                    newSlots[probe] = {slot.key, slot.value, State::OCCUPIED};
                    break;
                }
            }
        }
        m_slots = std::move(newSlots);
    }
};