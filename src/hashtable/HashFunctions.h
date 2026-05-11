#pragma once
// src/hashtable/HashFunctions.h
// Smart City Delivery & Traffic Management System

#include <string>
#include <cstddef>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
//  Namespace: HashFn
//
//  Custom hash functions for every key type used in this system.
//
//  Design goals:
//    - Low collision rate across real-world inputs (IDs, names, codes)
//    - Consistent: same key always returns same hash
//    - Callable as function objects — compatible with HashTable<K,V>
//
//  Each hasher is a struct with operator()(const K&) const
//  so it can be passed as a template parameter.
// ─────────────────────────────────────────────────────────────────────────────
namespace HashFn {

// ─── Integer hasher ───────────────────────────────────────────────────────────
// Uses Wang hash — excellent avalanche effect for sequential IDs
struct IntHash {
    std::size_t operator()(int key) const noexcept {
        std::size_t x = static_cast<std::size_t>(key);
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        return x;
    }
};

// ─── String hasher ────────────────────────────────────────────────────────────
// FNV-1a — fast, good distribution for short strings (names, codes)
struct StringHash {
    std::size_t operator()(const std::string& key) const noexcept {
        constexpr std::size_t FNV_PRIME  = 1099511628211ULL;
        constexpr std::size_t FNV_OFFSET = 14695981039346656037ULL;
        std::size_t hash = FNV_OFFSET;
        for (unsigned char c : key) {
            hash ^= c;
            hash *= FNV_PRIME;
        }
        return hash;
    }
};

// ─── Tracking-number hasher ───────────────────────────────────────────────────
// Tracking numbers are alphanumeric strings like "TRK-00123".
// We strip the prefix and hash the numeric suffix with polynomial rolling.
struct TrackingHash {
    std::size_t operator()(const std::string& key) const noexcept {
        constexpr std::size_t BASE   = 31;
        constexpr std::size_t MOD    = 1000000007ULL;
        std::size_t hash = 0;
        for (unsigned char c : key) {
            hash = (hash * BASE + c) % MOD;
        }
        return hash;
    }
};

// ─── Coordinate-pair hasher ───────────────────────────────────────────────────
// Used by spatial structures for grid-cell keys.
struct CoordHash {
    std::size_t operator()(std::pair<int,int> key) const noexcept {
        IntHash h;
        std::size_t h1 = h(key.first);
        std::size_t h2 = h(key.second);
        // Combine with Boost-style hash_combine
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

// ─── Double hasher (for weighted keys) ────────────────────────────────────────
struct DoubleHash {
    std::size_t operator()(double key) const noexcept {
        // Reinterpret bit pattern — avoids float equality issues
        std::size_t bits;
        static_assert(sizeof(double) == sizeof(std::size_t), "size mismatch");
        __builtin_memcpy(&bits, &key, sizeof(double));
        IntHash h;
        return h(static_cast<int>(bits ^ (bits >> 32)));
    }
};

// ─── Universal combine helper ─────────────────────────────────────────────────
// Combine two hash values (used internally when building composite keys).
inline std::size_t hashCombine(std::size_t h1, std::size_t h2) noexcept {
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
}

} // namespace HashFn