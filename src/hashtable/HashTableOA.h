#pragma once
// src/hashtable/HashTableOA.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <functional>
#include <ostream>
#include <cstdint>
#include "HashFunctions.h"

// ─────────────────────────────────────────────────────────────────────────────
//  SlotState — moved OUTSIDE the template class to avoid MinGW enum-class
//  parsing bug with private sections of template classes.
//  Plain enum wrapped in a namespace for scoping.
// ─────────────────────────────────────────────────────────────────────────────
namespace OAInternal {
    enum SlotState : uint8_t {
        SLOT_EMPTY    = 0,
        SLOT_OCCUPIED = 1,
        SLOT_DELETED  = 2
    };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Class: HashTableOA<K, V, Hasher>
//
//  Custom hash table using OPEN ADDRESSING with LINEAR PROBING.
//
//  Design:
//    - Flat array of slots — better cache locality than chaining
//    - Each slot has a state: EMPTY, OCCUPIED, DELETED (tombstone)
//    - Deletion uses tombstones so probe sequences stay intact
//    - Rehash triggered when load factor exceeds threshold (default 0.60)
//
//  Complexity:
//    insert / search / remove:  O(1) average
//    Worst case (high load):    O(n) — avoided by rehash at 0.6
// ─────────────────────────────────────────────────────────────────────────────
template<typename K,
         typename V,
         typename Hasher = HashFn::IntHash>
class HashTableOA {
public:
    // ── Construction ─────────────────────────────────────────────────────────
    explicit HashTableOA(std::size_t initialCapacity = 17,
                         double      maxLoadFactor   = 0.60)
        : m_size(0)
        , m_maxLoadFactor(maxLoadFactor)
    {
        m_slots.resize(initialCapacity);
        for (auto& s : m_slots) s.state = OAInternal::SLOT_EMPTY;
    }

    HashTableOA(const HashTableOA&)             = default;
    HashTableOA& operator=(const HashTableOA&)  = default;
    HashTableOA(HashTableOA&&)                  = default;
    HashTableOA& operator=(HashTableOA&&)       = default;

    // ── Core Operations ───────────────────────────────────────────────────────

    void insert(const K& key, const V& value) {
        rehashIfNeeded();

        std::size_t idx      = startIndex(key);
        std::size_t cap      = m_slots.size();
        std::size_t tombstone = cap;

        for (std::size_t i = 0; i < cap; ++i) {
            std::size_t probe = (idx + i) % cap;
            Slot& slot = m_slots[probe];

            if (slot.state == OAInternal::SLOT_OCCUPIED && slot.key == key) {
                slot.value = value;
                return;
            }
            if (slot.state == OAInternal::SLOT_DELETED && tombstone == cap) {
                tombstone = probe;
            }
            if (slot.state == OAInternal::SLOT_EMPTY) {
                std::size_t insertAt = (tombstone < cap) ? tombstone : probe;
                m_slots[insertAt].key   = key;
                m_slots[insertAt].value = value;
                m_slots[insertAt].state = OAInternal::SLOT_OCCUPIED;
                ++m_size;
                return;
            }
        }
        if (tombstone < cap) {
            m_slots[tombstone].key   = key;
            m_slots[tombstone].value = value;
            m_slots[tombstone].state = OAInternal::SLOT_OCCUPIED;
            ++m_size;
        }
    }

    V* search(const K& key) {
        std::size_t idx = startIndex(key);
        std::size_t cap = m_slots.size();
        for (std::size_t i = 0; i < cap; ++i) {
            std::size_t probe = (idx + i) % cap;
            Slot& slot = m_slots[probe];
            if (slot.state == OAInternal::SLOT_EMPTY)   return nullptr;
            if (slot.state == OAInternal::SLOT_OCCUPIED && slot.key == key)
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
            if (slot.state == OAInternal::SLOT_EMPTY)   return nullptr;
            if (slot.state == OAInternal::SLOT_OCCUPIED && slot.key == key)
                return &slot.value;
        }
        return nullptr;
    }

    bool contains(const K& key) const { return search(key) != nullptr; }

    bool remove(const K& key) {
        std::size_t idx = startIndex(key);
        std::size_t cap = m_slots.size();
        for (std::size_t i = 0; i < cap; ++i) {
            std::size_t probe = (idx + i) % cap;
            Slot& slot = m_slots[probe];
            if (slot.state == OAInternal::SLOT_EMPTY)  return false;
            if (slot.state == OAInternal::SLOT_OCCUPIED && slot.key == key) {
                slot.state = OAInternal::SLOT_DELETED;
                --m_size;
                return true;
            }
        }
        return false;
    }

    V& operator[](const K& key) {
        V* found = search(key);
        if (found) return *found;
        insert(key, V{});
        return *search(key);
    }

    // ── Iteration ─────────────────────────────────────────────────────────────
    void forEach(std::function<void(const K&, V&)> visitor) {
        for (auto& slot : m_slots)
            if (slot.state == OAInternal::SLOT_OCCUPIED)
                visitor(slot.key, slot.value);
    }

    void forEach(std::function<void(const K&, const V&)> visitor) const {
        for (const auto& slot : m_slots)
            if (slot.state == OAInternal::SLOT_OCCUPIED)
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
        for (auto& s : m_slots) s.state = OAInternal::SLOT_EMPTY;
        m_size = 0;
    }

    void printStats(std::ostream& os) const {
        std::size_t del = 0;
        for (const auto& s : m_slots)
            if (s.state == OAInternal::SLOT_DELETED) ++del;
        os << "HashTableOA[open-addr] size=" << m_size
           << " capacity=" << capacity()
           << " loadFactor=" << loadFactor()
           << " tombstones=" << del << "\n";
    }

private:
    // ── Slot struct ───────────────────────────────────────────────────────────
    // Explicit default constructor + assignment — avoids brace-init issues on MinGW
    struct Slot {
        K                     key;
        V                     value;
        OAInternal::SlotState state;

        Slot() : key(), value(), state(OAInternal::SLOT_EMPTY) {}
    };

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
        std::vector<Slot> newSlots(newCap);  // all default-constructed → SLOT_EMPTY

        for (const auto& slot : m_slots) {
            if (slot.state != OAInternal::SLOT_OCCUPIED) continue;
            std::size_t idx = m_hasher(slot.key) % newCap;
            for (std::size_t i = 0; i < newCap; ++i) {
                std::size_t probe = (idx + i) % newCap;
                if (newSlots[probe].state == OAInternal::SLOT_EMPTY) {
                    newSlots[probe].key   = slot.key;
                    newSlots[probe].value = slot.value;
                    newSlots[probe].state = OAInternal::SLOT_OCCUPIED;
                    break;
                }
            }
        }
        m_slots = std::move(newSlots);
    }
};