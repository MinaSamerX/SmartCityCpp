// tests/test_hashtable.cpp
// Smart City Delivery & Traffic Management System
// ── HashTable (Chaining) + HashTableOA (Open Addressing) Tests ───────────────

#include "hashtable/HashTable.h"
#include "hashtable/HashTableOA.h"
#include "hashtable/HashFunctions.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Test framework helpers
// ─────────────────────────────────────────────────────────────────────────────
static int s_passed = 0, s_failed = 0;

static void pass(const char* name) {
    std::cout << "  [PASS] " << name << "\n";
    ++s_passed;
}
static void fail(const char* name, const char* msg = "") {
    std::cerr << "  [FAIL] " << name << "  " << msg << "\n";
    ++s_failed;
}
#define CHECK(cond, name) do { if(cond) pass(name); else fail(name, #cond); } while(0)

// ─────────────────────────────────────────────────────────────────────────────
//  1. HashTable — Separate Chaining
// ─────────────────────────────────────────────────────────────────────────────
static void test_chaining_basic() {
    std::cout << "\n── HashTable<int,string> chaining ──\n";

    HashTable<int, std::string, HashFn::IntHash> ht;

    // Insert
    ht.insert(1, "Warehouse");
    ht.insert(2, "Junction");
    ht.insert(3, "Customer");
    CHECK(ht.size()     == 3,          "size after 3 inserts");
    CHECK(!ht.empty(),                 "not empty");
    CHECK(ht.contains(1),              "contains key 1");
    CHECK(!ht.contains(99),            "does not contain key 99");

    // Search
    CHECK(*ht.search(1) == "Warehouse","search returns correct value");
    CHECK(ht.search(99) == nullptr,    "search missing key returns nullptr");

    // Update (insert same key)
    ht.insert(1, "UPDATED");
    CHECK(*ht.search(1) == "UPDATED",  "overwrite existing key");
    CHECK(ht.size() == 3,              "size unchanged after overwrite");

    // Remove
    CHECK(ht.remove(2),                "remove existing key returns true");
    CHECK(!ht.contains(2),             "key removed");
    CHECK(ht.size() == 2,              "size decremented after remove");
    CHECK(!ht.remove(99),              "remove missing key returns false");
}

static void test_chaining_string_key() {
    std::cout << "\n── HashTable<string,int> StringHash ──\n";

    HashTable<std::string, int, HashFn::StringHash> ht;
    ht.insert("TRK-001", 1);
    ht.insert("TRK-002", 2);
    ht.insert("TRK-003", 3);

    CHECK(ht.size() == 3,              "size == 3");
    CHECK(*ht.search("TRK-001") == 1,  "TRK-001 → 1");
    CHECK(*ht.search("TRK-002") == 2,  "TRK-002 → 2");
    CHECK(ht.search("TRK-999") == nullptr, "missing tracking returns nullptr");

    ht.remove("TRK-002");
    CHECK(!ht.contains("TRK-002"),     "TRK-002 removed");
    CHECK(ht.size() == 2,              "size after remove");
}

static void test_chaining_rehash() {
    std::cout << "\n── HashTable rehash under load ──\n";

    HashTable<int, int, HashFn::IntHash> ht(4, 0.75); // tiny capacity
    for (int i = 0; i < 100; ++i) ht.insert(i, i * 10);

    CHECK(ht.size() == 100,            "100 elements stored");
    bool allCorrect = true;
    for (int i = 0; i < 100; ++i) {
        auto* p = ht.search(i);
        if (!p || *p != i * 10) { allCorrect = false; break; }
    }
    CHECK(allCorrect,                  "all values correct after rehash");
    CHECK(ht.loadFactor() <= 0.75,     "load factor within threshold");
}

static void test_chaining_operator_bracket() {
    std::cout << "\n── HashTable operator[] ──\n";

    HashTable<std::string, int, HashFn::StringHash> ht;
    ht["alpha"] = 10;
    ht["beta"]  = 20;
    CHECK(ht["alpha"] == 10,           "operator[] read");
    ht["alpha"] += 5;
    CHECK(ht["alpha"] == 15,           "operator[] write increment");
    CHECK(ht.size() == 2,              "size via operator[]");
}

static void test_chaining_forEach() {
    std::cout << "\n── HashTable forEach iteration ──\n";

    HashTable<int, int, HashFn::IntHash> ht;
    for (int i = 1; i <= 5; ++i) ht.insert(i, i * i);

    int sum = 0;
    ht.forEach([&sum](const int&, const int& v){ sum += v; });
    CHECK(sum == 1+4+9+16+25,          "forEach sums all values (1²+…+5²=55)");

    auto ks = ht.keys();
    CHECK(ks.size() == 5,              "keys() returns all 5 keys");
}

static void test_chaining_collision() {
    std::cout << "\n── HashTable collision handling ──\n";

    // Force collisions by using capacity=1
    HashTable<int, std::string, HashFn::IntHash> ht(1);
    ht.insert(10, "A");
    ht.insert(20, "B");
    ht.insert(30, "C");
    CHECK(*ht.search(10) == "A",       "collision: key 10 still findable");
    CHECK(*ht.search(20) == "B",       "collision: key 20 still findable");
    CHECK(*ht.search(30) == "C",       "collision: key 30 still findable");
    CHECK(ht.maxChainLength() >= 1,    "chain exists");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. HashTableOA — Open Addressing (Linear Probing)
// ─────────────────────────────────────────────────────────────────────────────
static void test_oa_basic() {
    std::cout << "\n── HashTableOA<int,string> open addressing ──\n";

    HashTableOA<int, std::string, HashFn::IntHash> ht;
    for (int i = 0; i < 30; ++i)
        ht.insert(i, "val-" + std::to_string(i));

    CHECK(ht.size() == 30,             "30 elements stored");
    CHECK(*ht.search(0)  == "val-0",   "search first element");
    CHECK(*ht.search(29) == "val-29",  "search last element");
    CHECK(!ht.search(99),              "search missing key");
}

static void test_oa_tombstone() {
    std::cout << "\n── HashTableOA tombstone deletion ──\n";

    HashTableOA<int, std::string, HashFn::IntHash> ht;
    ht.insert(1, "A");
    ht.insert(2, "B");
    ht.insert(3, "C");

    // Delete middle element (creates tombstone)
    CHECK(ht.remove(2),                "remove returns true");
    CHECK(!ht.contains(2),             "removed element not found");
    CHECK(ht.size() == 2,              "size decremented");

    // Insert after tombstone — must land correctly
    ht.insert(2, "B_NEW");
    CHECK(*ht.search(2) == "B_NEW",    "re-insert after tombstone");
    CHECK(ht.size() == 3,              "size restored");

    // Keys 1 and 3 unaffected
    CHECK(*ht.search(1) == "A",        "key 1 unchanged");
    CHECK(*ht.search(3) == "C",        "key 3 unchanged");
}

static void test_oa_rehash() {
    std::cout << "\n── HashTableOA rehash ──\n";

    HashTableOA<int, int, HashFn::IntHash> ht(4, 0.60);
    for (int i = 0; i < 50; ++i) ht.insert(i, i * 3);

    CHECK(ht.size() == 50,             "50 elements after rehash");
    bool allOk = true;
    for (int i = 0; i < 50; ++i) {
        auto* p = ht.search(i);
        if (!p || *p != i * 3) { allOk = false; break; }
    }
    CHECK(allOk,                       "all values correct after rehash");
    CHECK(ht.loadFactor() <= 0.60,     "load factor within threshold");
}

static void test_oa_operator_bracket() {
    std::cout << "\n── HashTableOA operator[] ──\n";

    HashTableOA<int, int, HashFn::IntHash> ht;
    ht[100] = 999;
    CHECK(ht[100] == 999,              "operator[] set and read");
    ht[100] = 42;
    CHECK(ht[100] == 42,               "operator[] overwrite");
}

static void test_oa_forEach() {
    std::cout << "\n── HashTableOA forEach ──\n";

    HashTableOA<int, int, HashFn::IntHash> ht;
    for (int i = 1; i <= 4; ++i) ht.insert(i, i);

    int product = 1;
    ht.forEach([&product](const int&, const int& v){ product *= v; });
    CHECK(product == 24,               "forEach: 1*2*3*4 = 24");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Hash functions
// ─────────────────────────────────────────────────────────────────────────────
static void test_hash_functions() {
    std::cout << "\n── Hash Functions ──\n";

    HashFn::IntHash    ih;
    HashFn::StringHash sh;
    HashFn::TrackingHash th;
    HashFn::CoordHash  ch;

    // Deterministic — same input → same hash
    CHECK(ih(42)        == ih(42),         "IntHash deterministic");
    CHECK(sh("hello")   == sh("hello"),    "StringHash deterministic");
    CHECK(th("TRK-001") == th("TRK-001"), "TrackingHash deterministic");
    CHECK(ch({3,7})     == ch({3,7}),     "CoordHash deterministic");

    // Different inputs → different hashes (not guaranteed but expected for common cases)
    CHECK(ih(1) != ih(2),                  "IntHash: 1 != 2");
    CHECK(sh("abc") != sh("xyz"),          "StringHash: abc != xyz");
    CHECK(ch({1,2}) != ch({2,1}),          "CoordHash: {1,2} != {2,1}");

    // hashCombine
    auto h1 = HashFn::hashCombine(ih(10), ih(20));
    auto h2 = HashFn::hashCombine(ih(10), ih(20));
    CHECK(h1 == h2,                        "hashCombine deterministic");
    CHECK(HashFn::hashCombine(ih(1),ih(2)) != HashFn::hashCombine(ih(2),ih(1)),
                                           "hashCombine order-sensitive");
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║      HashTable Test Suite            ║\n";
    std::cout << "╚══════════════════════════════════════╝\n";

    // Chaining
    test_chaining_basic();
    test_chaining_string_key();
    test_chaining_rehash();
    test_chaining_operator_bracket();
    test_chaining_forEach();
    test_chaining_collision();

    // Open addressing
    test_oa_basic();
    test_oa_tombstone();
    test_oa_rehash();
    test_oa_operator_bracket();
    test_oa_forEach();

    // Hash functions
    test_hash_functions();

    std::cout << "\n══════════════════════════════════════\n";
    std::cout << "  Passed : " << s_passed << "\n";
    std::cout << "  Failed : " << s_failed << "\n";
    std::cout << "══════════════════════════════════════\n";
    return s_failed == 0 ? 0 : 1;
}