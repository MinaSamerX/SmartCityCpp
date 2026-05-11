// tests/test_priorityqueue.cpp
// Smart City Delivery & Traffic Management System
// ── PriorityQueue + BST + SegmentTree Tests ──────────────────────────────────

#include "scheduler/PriorityQueue.h"
#include "spatial/BST.h"
#include "spatial/SegmentTree.h"
#include "core/Package.h"
#include "system/EntityFactory.h"
#include <iostream>
#include <cassert>
#include <ctime>
#include <memory>
#include <vector>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Test framework
// ─────────────────────────────────────────────────────────────────────────────
static int s_passed = 0, s_failed = 0;

static void pass(const char* name) {
    std::cout << "  [PASS] " << name << "\n";
    ++s_passed;
}
static void fail(const char* name, const char* detail = "") {
    std::cerr << "  [FAIL] " << name << "  " << detail << "\n";
    ++s_failed;
}
#define CHECK(cond, name) do { if(cond) pass(name); else fail(name, #cond); } while(0)
#define APPROX(a,b) (std::fabs((a)-(b)) < 1e-9)

// ── Helper: create a package with deadline secsFromNow ─────────────────────────
static std::shared_ptr<Package> mkPkg(int id, DeliveryPriority p, int secsFromNow) {
    std::time_t dl = std::time(nullptr) + secsFromNow;
    return std::make_shared<Package>(
        id, "TRK-"+std::to_string(id), 1, 4, 0, 5.0, p, dl);
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. PriorityQueue — max-heap by priorityScore
// ─────────────────────────────────────────────────────────────────────────────
static void test_pq_basic() {
    std::cout << "\n── PriorityQueue basic (max-heap) ──\n";

    auto pq = makeDeliveryQueue();
    CHECK(pq.empty(),                  "empty on construction");
    CHECK(pq.size() == 0,              "size == 0 initially");

    auto p1 = mkPkg(1, DeliveryPriority::Normal,   3600);
    auto p2 = mkPkg(2, DeliveryPriority::Critical, 3600);
    auto p3 = mkPkg(3, DeliveryPriority::Low,      3600);
    auto p4 = mkPkg(4, DeliveryPriority::Urgent,   3600);

    pq.push(p1); pq.push(p2); pq.push(p3); pq.push(p4);
    CHECK(pq.size() == 4,              "size == 4 after 4 pushes");
    CHECK(!pq.empty(),                 "not empty");

    // Critical (5) must be on top
    CHECK(pq.top()->getId() == 2,      "Critical package on top");

    // Pop order: Critical → Urgent → Normal → Low
    pq.pop();
    CHECK(pq.top()->getId() == 4,      "Urgent after Critical popped");
    pq.pop();
    CHECK(pq.top()->getId() == 1,      "Normal after Urgent popped");
    pq.pop();
    CHECK(pq.top()->getId() == 3,      "Low last remaining");
    pq.pop();
    CHECK(pq.empty(),                  "empty after all pops");
}

static void test_pq_updatePriority() {
    std::cout << "\n── PriorityQueue updatePriority ──\n";

    auto pq = makeDeliveryQueue();
    auto p1 = mkPkg(1, DeliveryPriority::Low,    7200);
    auto p2 = mkPkg(2, DeliveryPriority::Normal, 7200);
    auto p3 = mkPkg(3, DeliveryPriority::High,   7200);
    pq.push(p1); pq.push(p2); pq.push(p3);

    CHECK(pq.top()->getId() == 3,      "High on top initially");

    // Promote p1 (Low) to Critical
    auto p1_up = mkPkg(1, DeliveryPriority::Critical, 100);
    pq.updatePriority(1, p1_up);
    CHECK(pq.top()->getId() == 1,      "p1 promoted to top after updatePriority");

    // Demote p3 (High) to Low
    auto p3_dn = mkPkg(3, DeliveryPriority::Low, 7200);
    pq.updatePriority(3, p3_dn);
    pq.pop(); // pop p1 (Critical)
    CHECK(pq.top()->getId() == 2,      "Normal on top after p3 demoted");
}

static void test_pq_remove() {
    std::cout << "\n── PriorityQueue remove by ID ──\n";

    auto pq = makeDeliveryQueue();
    pq.push(mkPkg(10, DeliveryPriority::High,   3600));
    pq.push(mkPkg(20, DeliveryPriority::Normal, 3600));
    pq.push(mkPkg(30, DeliveryPriority::Low,    3600));

    CHECK(pq.contains(20),             "contains(20) before remove");
    CHECK(pq.remove(20),               "remove(20) returns true");
    CHECK(!pq.contains(20),            "!contains(20) after remove");
    CHECK(pq.size() == 2,              "size decremented");
    CHECK(!pq.remove(99),              "remove missing returns false");
}

static void test_pq_duplicate() {
    std::cout << "\n── PriorityQueue duplicate push (overwrite) ──\n";

    auto pq = makeDeliveryQueue();
    pq.push(mkPkg(5, DeliveryPriority::Low, 3600));
    pq.push(mkPkg(5, DeliveryPriority::Critical, 1800)); // same ID
    CHECK(pq.size() == 1,              "size == 1 after duplicate push");
    CHECK(pq.top()->getPriority() == DeliveryPriority::Critical, "overwritten to Critical");
}

static void test_pq_deadline_tiebreak() {
    std::cout << "\n── PriorityQueue: closer deadline wins same priority ──\n";

    auto pq = makeDeliveryQueue();
    auto pFar  = mkPkg(1, DeliveryPriority::High, 7200); // 2 hours
    auto pNear = mkPkg(2, DeliveryPriority::High, 900);  // 15 minutes — more urgent
    pq.push(pFar);
    pq.push(pNear);
    // pNear should score higher because deadline is closer
    CHECK(pq.top()->getId() == 2,      "closer deadline wins tie on priority");
}

static void test_pq_custom_int() {
    std::cout << "\n── PriorityQueue: custom int max-heap ──\n";

    PriorityQueue<int,
        std::function<int(const int&)>,
        std::function<bool(const int&, const int&)>>
    pq([](const int& x){ return x; },
       [](const int& a, const int& b){ return a < b; });

    for (int v : {3,1,4,1,5,9,2,6}) pq.push(v);
    CHECK(pq.top() == 9,               "max element on top");
    pq.pop();
    CHECK(pq.top() == 6,               "next max after pop");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. BST — deadline-ordered
// ─────────────────────────────────────────────────────────────────────────────
static void test_bst_inorder() {
    std::cout << "\n── BST inorder (deadline sorted) ──\n";

    auto bst = makeDeadlineBST();
    std::time_t now = std::time(nullptr);
    auto p1 = mkPkg(1, DeliveryPriority::Normal, 3600);  // 1h
    auto p2 = mkPkg(2, DeliveryPriority::Normal, 1800);  // 30m — earliest
    auto p3 = mkPkg(3, DeliveryPriority::Normal, 7200);  // 2h — latest

    bst.insert(p1); bst.insert(p2); bst.insert(p3);
    CHECK(bst.size() == 3,             "BST size == 3");

    auto sorted = bst.inorder();
    CHECK(sorted.size() == 3,          "inorder returns 3 items");
    CHECK(sorted[0]->getId() == 2,     "earliest deadline first");
    CHECK(sorted[2]->getId() == 3,     "latest deadline last");
    CHECK(sorted[0]->getDeadline() <= sorted[1]->getDeadline(), "inorder ascending[0,1]");
    CHECK(sorted[1]->getDeadline() <= sorted[2]->getDeadline(), "inorder ascending[1,2]");
}

static void test_bst_range() {
    std::cout << "\n── BST range query ──\n";

    auto bst = makeDeadlineBST();
    std::time_t now = std::time(nullptr);
    bst.insert(mkPkg(1, DeliveryPriority::Normal, 500));   // in 500s
    bst.insert(mkPkg(2, DeliveryPriority::Normal, 1000));  // in 1000s
    bst.insert(mkPkg(3, DeliveryPriority::Normal, 5000));  // in 5000s

    // Query: deadlines in next 1500s
    auto range = bst.rangeQuery(now, now + 1500);
    CHECK(range.size() == 2,           "range [now, now+1500] hits 2 packages");

    // Query: only far ones
    auto far = bst.rangeQuery(now + 2000, now + 9999);
    CHECK(far.size() == 1,             "range [now+2000, ...] hits 1 package");
}

static void test_bst_minmax() {
    std::cout << "\n── BST min/max ──\n";

    auto bst = makeDeadlineBST();
    bst.insert(mkPkg(1, DeliveryPriority::Normal, 3600));
    bst.insert(mkPkg(2, DeliveryPriority::Normal, 900));
    bst.insert(mkPkg(3, DeliveryPriority::Normal, 7200));

    CHECK(bst.minimum() != nullptr,    "minimum() non-null");
    CHECK(bst.maximum() != nullptr,    "maximum() non-null");
    CHECK((*bst.minimum())->getId() == 2, "minimum is earliest deadline (id=2)");
    CHECK((*bst.maximum())->getId() == 3, "maximum is latest deadline (id=3)");
}

static void test_bst_remove() {
    std::cout << "\n── BST remove ──\n";

    auto bst = makeDeadlineBST();
    auto p1 = mkPkg(1, DeliveryPriority::Normal, 1000);
    auto p2 = mkPkg(2, DeliveryPriority::Normal, 2000);
    auto p3 = mkPkg(3, DeliveryPriority::Normal, 3000);
    bst.insert(p1); bst.insert(p2); bst.insert(p3);

    CHECK(bst.remove(p2->getDeadline()), "remove middle key returns true");
    CHECK(bst.size() == 2,             "size decremented");
    auto remaining = bst.inorder();
    CHECK(remaining.size() == 2,       "2 items remain after remove");
    CHECK(!bst.remove(p2->getDeadline()), "remove missing key returns false");
}

static void test_bst_generic_int() {
    std::cout << "\n── BST generic int ──\n";

    BST<int, int, std::function<int(const int&)>> intBST(
        [](const int& x){ return x; });

    for (int v : {5,3,7,1,4,6,8}) intBST.insert(v);
    CHECK(intBST.size() == 7,          "7 elements inserted");
    CHECK(intBST.contains(4),          "contains(4)");
    CHECK(!intBST.contains(99),        "!contains(99)");

    auto sorted = intBST.inorder();
    bool asc = true;
    for (int i = 0; i+1 < (int)sorted.size(); ++i)
        if (sorted[i] >= sorted[i+1]) { asc = false; break; }
    CHECK(asc,                         "inorder ascending");
    CHECK(*intBST.minimum() == 1,      "minimum == 1");
    CHECK(*intBST.maximum() == 8,      "maximum == 8");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. SegmentTree
// ─────────────────────────────────────────────────────────────────────────────
static void test_segtree_sum() {
    std::cout << "\n── SegmentTree sum ──\n";

    std::vector<double> v = {1,3,2,5,4,6,2,8};
    auto st = SegmentTree<double>::makeSumTree(v);

    CHECK(APPROX(st.query(0,7), 31.0), "sum[0..7] == 31");
    CHECK(APPROX(st.query(0,0),  1.0), "sum[0..0] ==  1");
    CHECK(APPROX(st.query(2,5), 17.0), "sum[2..5] == 17");

    st.update(3, 10.0);
    CHECK(APPROX(st.query(0,7), 36.0), "sum after update(3, 10)");
    CHECK(APPROX(st.at(3),     10.0),  "at(3) == 10 after update");
}

static void test_segtree_max() {
    std::cout << "\n── SegmentTree max ──\n";

    std::vector<double> v = {2,4,1,8,3,7,5,6};
    auto st = SegmentTree<double>::makeMaxTree(v);

    CHECK(APPROX(st.query(0,7), 8.0),  "max[0..7] == 8");
    CHECK(APPROX(st.query(0,2), 4.0),  "max[0..2] == 4");
    CHECK(APPROX(st.query(4,7), 7.0),  "max[4..7] == 7");

    st.update(5, 20.0);
    CHECK(APPROX(st.query(0,7), 20.0), "max after update(5,20)");
    CHECK(APPROX(st.query(0,4),  8.0), "max[0..4] unaffected");
}

static void test_segtree_min() {
    std::cout << "\n── SegmentTree min ──\n";

    std::vector<double> v = {5,3,8,1,6,2,7,4};
    auto st = SegmentTree<double>::makeMinTree(v);

    CHECK(APPROX(st.query(0,7), 1.0),  "min[0..7] == 1");
    CHECK(APPROX(st.query(0,2), 3.0),  "min[0..2] == 3");
    CHECK(APPROX(st.query(5,7), 2.0),  "min[5..7] == 2");

    st.update(3, 99.0);                // change min element
    CHECK(APPROX(st.query(0,7), 2.0),  "new min after changing index 3");
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║  PriorityQueue / BST / SegTree Tests ║\n";
    std::cout << "╚══════════════════════════════════════╝\n";

    test_pq_basic();
    test_pq_updatePriority();
    test_pq_remove();
    test_pq_duplicate();
    test_pq_deadline_tiebreak();
    test_pq_custom_int();

    test_bst_inorder();
    test_bst_range();
    test_bst_minmax();
    test_bst_remove();
    test_bst_generic_int();

    test_segtree_sum();
    test_segtree_max();
    test_segtree_min();

    std::cout << "\n══════════════════════════════════════\n";
    std::cout << "  Passed : " << s_passed << "\n";
    std::cout << "  Failed : " << s_failed << "\n";
    std::cout << "══════════════════════════════════════\n";
    return s_failed == 0 ? 0 : 1;
}