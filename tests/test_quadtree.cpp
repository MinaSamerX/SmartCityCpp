// tests/test_quadtree.cpp
// Smart City Delivery & Traffic Management System
// ── QuadTree + KDTree + ClosestPair + Sort/Search Tests ──────────────────────

#include "spatial/QuadTree.h"
#include "spatial/KDTree.h"
#include "dataprocessor/ClosestPair.h"
#include "dataprocessor/MergeSort.h"
#include "dataprocessor/QuickSort.h"
#include "dataprocessor/BinarySearch.h"
#include "dataprocessor/ZonePartitioner.h"
#include "core/Location.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <memory>
#include <vector>

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
#define APPROX(a,b) (std::fabs((a)-(b)) < 1e-6)

// ─────────────────────────────────────────────────────────────────────────────
//  Shared test locations  (mirrors city data)
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<std::shared_ptr<Location>> buildLocs() {
    return {
        std::make_shared<Location>(1,  "Warehouse",  10, 20, LocationType::Warehouse),
        std::make_shared<Location>(2,  "Downtown",   25, 30, LocationType::Intersection),
        std::make_shared<Location>(3,  "Hospital",   40, 35, LocationType::TrafficHub),
        std::make_shared<Location>(4,  "Mall",       55, 20, LocationType::Customer),
        std::make_shared<Location>(5,  "University", 70, 40, LocationType::Intersection),
        std::make_shared<Location>(6,  "Airport",    90, 50, LocationType::ChargingStation),
        std::make_shared<Location>(7,  "Industrial", 20, 70, LocationType::Warehouse),
        std::make_shared<Location>(8,  "Train",      45, 75, LocationType::TrafficHub),
        std::make_shared<Location>(9,  "Residential",65, 85, LocationType::Customer),
        std::make_shared<Location>(10, "Port",       95, 90, LocationType::TrafficHub),
    };
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. QuadTree
// ─────────────────────────────────────────────────────────────────────────────
static void test_quadtree_build() {
    std::cout << "\n── QuadTree build ──\n";

    auto locs = buildLocs();
    auto qt = QuadTree::build(locs);
    CHECK(qt.size() == 10,             "QuadTree contains all 10 locations");
}

static void test_quadtree_nearest() {
    std::cout << "\n── QuadTree nearestNeighbor ──\n";

    auto locs = buildLocs();
    auto qt = QuadTree::build(locs);

    // Nearest to Downtown (25,30) → should be Downtown itself
    auto near = qt.nearestNeighbor(25.0, 30.0);
    CHECK(near != nullptr,             "nearestNeighbor non-null");
    CHECK(near->getId() == 2,          "nearest to (25,30) is Downtown(id=2)");

    // Nearest to (0,0) → Warehouse(10,20) is closest
    auto near2 = qt.nearestNeighbor(0.0, 0.0);
    CHECK(near2 != nullptr,            "nearestNeighbor(0,0) non-null");
    CHECK(near2->getId() == 1,         "nearest to (0,0) is Warehouse(id=1)");

    // Nearest to (90,50) → Airport itself
    auto near3 = qt.nearestNeighbor(91.0, 51.0);
    CHECK(near3 != nullptr,            "nearestNeighbor(91,51) non-null");
    CHECK(near3->getId() == 6,         "nearest to (91,51) is Airport(id=6)");
}

static void test_quadtree_radius() {
    std::cout << "\n── QuadTree radius query ──\n";

    auto locs = buildLocs();
    auto qt = QuadTree::build(locs);

    // Large radius from center — should find many
    auto r1 = qt.queryRadius(50.0, 50.0, 40.0);
    CHECK(!r1.empty(),                 "radius query returns results");
    CHECK(r1.size() >= 3,              "radius(50,50,r=40) finds >= 3 locations");

    // Tiny radius — should find none or one
    auto r2 = qt.queryRadius(0.0, 0.0, 5.0);
    CHECK(r2.empty(),                  "tiny radius from (0,0) finds nothing");

    // Exact match radius
    auto r3 = qt.queryRadius(10.0, 20.0, 1.0);
    CHECK(!r3.empty(),                 "exact point radius finds Warehouse");
    CHECK(r3[0]->getId() == 1,         "found Warehouse at (10,20)");
}

static void test_quadtree_range() {
    std::cout << "\n── QuadTree range (bounding box) ──\n";

    auto locs = buildLocs();
    auto qt = QuadTree::build(locs);

    // Box covering left side of city
    BoundingBox box{17.5, 45.0, 17.5, 45.0}; // center(17.5,45) half(17.5,45) → x[0,35] y[0,90]
    auto rangeRes = qt.queryRange(box);
    CHECK(!rangeRes.empty(),           "range query returns results");
}

static void test_quadtree_knearest() {
    std::cout << "\n── QuadTree kNearest ──\n";

    auto locs = buildLocs();
    auto qt = QuadTree::build(locs);

    auto k3 = qt.kNearest(50.0, 50.0, 3);
    CHECK(k3.size() == 3,              "kNearest(3) returns 3 results");

    // Verify sorted by distance
    auto dist = [](const std::shared_ptr<Location>& l, double x, double y) {
        double dx = l->getX()-x, dy = l->getY()-y;
        return std::sqrt(dx*dx+dy*dy);
    };
    double d0 = dist(k3[0],50,50);
    double d1 = dist(k3[1],50,50);
    double d2 = dist(k3[2],50,50);
    CHECK(d0 <= d1 && d1 <= d2,        "kNearest results sorted by distance");

    auto k1 = qt.kNearest(10.0, 20.0, 1);
    CHECK(k1.size() == 1,              "kNearest(1) returns 1 result");
    CHECK(k1[0]->getId() == 1,         "kNearest(1) from Warehouse is Warehouse");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. KDTree
// ─────────────────────────────────────────────────────────────────────────────
static void test_kdtree_build() {
    std::cout << "\n── KDTree build ──\n";

    KDTree kdt;
    CHECK(kdt.empty(),                 "KDTree empty on construction");
    kdt.build(buildLocs());
    CHECK(!kdt.empty(),                "KDTree not empty after build");
    CHECK(kdt.size() == 10,            "KDTree size == 10");
}

static void test_kdtree_nearest() {
    std::cout << "\n── KDTree nearestNeighbor ──\n";

    KDTree kdt;
    kdt.build(buildLocs());

    auto nn = kdt.nearestNeighbor(25.0, 30.0);
    CHECK(nn != nullptr,               "nearestNeighbor non-null");
    CHECK(nn->getId() == 2,            "nearest to (25,30) is Downtown(id=2)");

    auto nn2 = kdt.nearestNeighbor(0.0, 0.0);
    CHECK(nn2->getId() == 1,           "nearest to (0,0) is Warehouse(id=1)");

    // KDTree and QuadTree agree on nearest
    auto qt = QuadTree::build(buildLocs());
    auto qtNear = qt.nearestNeighbor(50.0, 55.0);
    auto kdNear = kdt.nearestNeighbor(50.0, 55.0);
    CHECK(qtNear && kdNear &&
          qtNear->getId() == kdNear->getId(), "QuadTree and KDTree agree on nearest");
}

static void test_kdtree_radius() {
    std::cout << "\n── KDTree queryRadius ──\n";

    KDTree kdt;
    kdt.build(buildLocs());

    auto r = kdt.queryRadius(50.0, 50.0, 30.0);
    CHECK(!r.empty(),                  "KDTree radius query non-empty");

    auto qt = QuadTree::build(buildLocs());
    auto qtR = qt.queryRadius(50.0, 50.0, 30.0);
    CHECK(r.size() == qtR.size(),      "KDTree and QuadTree radius agree");
}

static void test_kdtree_knearest() {
    std::cout << "\n── KDTree kNearest ──\n";

    KDTree kdt;
    kdt.build(buildLocs());

    auto k5 = kdt.kNearest(50.0, 50.0, 5);
    CHECK(k5.size() == 5,              "kNearest(5) returns 5 results");

    // Sorted by distance
    auto d = [](const std::shared_ptr<Location>& l){
        double dx=l->getX()-50, dy=l->getY()-50;
        return dx*dx+dy*dy;
    };
    bool sorted = true;
    for (int i = 0; i+1 < (int)k5.size(); ++i)
        if (d(k5[i]) > d(k5[i+1])) { sorted = false; break; }
    CHECK(sorted,                      "kNearest sorted by distance");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. ClosestPair
// ─────────────────────────────────────────────────────────────────────────────
static void test_closest_pair() {
    std::cout << "\n── ClosestPair (Divide & Conquer) ──\n";

    auto locs = buildLocs();
    auto cp = DivideConquer::closestPair(locs);
    CHECK(cp.valid(),                  "closestPair result is valid");
    CHECK(cp.distance > 0,             "distance > 0");

    // Brute-force verification: find actual minimum distance
    double minDist = std::numeric_limits<double>::max();
    for (int i = 0; i < (int)locs.size(); ++i)
        for (int j = i+1; j < (int)locs.size(); ++j) {
            double d = locs[i]->distanceTo(*locs[j]);
            if (d < minDist) minDist = d;
        }
    CHECK(APPROX(cp.distance, minDist),"D&C matches brute force min distance");

    // allClosePairs
    auto pairs = DivideConquer::allClosePairs(locs, 20.0);
    CHECK(!pairs.empty(),              "allClosePairs within threshold non-empty");
    for (auto& [a,b] : pairs)
        CHECK(a->distanceTo(*b) <= 20.0, "all pairs within threshold distance");

    // Edge case: 2 points
    std::vector<std::shared_ptr<Location>> two = {
        std::make_shared<Location>(1,"A",0,0),
        std::make_shared<Location>(2,"B",3,4)
    };
    auto cp2 = DivideConquer::closestPair(two);
    CHECK(APPROX(cp2.distance, 5.0),   "closestPair 2 points: dist(0,0)-(3,4) == 5");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. MergeSort + QuickSort + BinarySearch
// ─────────────────────────────────────────────────────────────────────────────
static void test_mergesort() {
    std::cout << "\n── MergeSort ──\n";

    // Integers
    std::vector<int> v = {5,3,8,1,9,2,7,4,6};
    Sorting::mergeSort<int>(v, [](int a, int b){ return a < b; });
    bool asc = true;
    for (int i = 0; i+1 < (int)v.size(); ++i) if (v[i]>v[i+1]) asc=false;
    CHECK(asc,                         "MergeSort integers ascending");

    // Stable: equal elements keep relative order
    using P = std::pair<int,int>; // {value, original_index}
    std::vector<P> ps = {{3,0},{1,1},{3,2},{2,3},{1,4}};
    Sorting::mergeSort<P>(ps, [](const P& a, const P& b){ return a.first < b.first; });
    CHECK(ps[0].second < ps[1].second, "MergeSort stable: equal keys preserve order");

    // Packages by deadline
    auto p1 = std::make_shared<Package>(1,"T1",1,4,0,5.0,DeliveryPriority::Normal,
                static_cast<std::time_t>(3000));
    auto p2 = std::make_shared<Package>(2,"T2",1,4,0,5.0,DeliveryPriority::Normal,
                static_cast<std::time_t>(1000));
    auto p3 = std::make_shared<Package>(3,"T3",1,4,0,5.0,DeliveryPriority::Normal,
                static_cast<std::time_t>(2000));
    std::vector<std::shared_ptr<Package>> pkgs = {p1,p2,p3};
    Sorting::sortByDeadline(pkgs);
    CHECK(pkgs[0]->getId()==2 && pkgs[2]->getId()==1, "sortByDeadline correct order");

    // mergeSorted
    std::vector<int> a={1,3,5,7}, b={2,4,6,8};
    auto merged = Sorting::mergeSorted<int>(a, b,
        std::function<bool(const int&,const int&)>([](int x, int y){ return x<y; }));
    bool ok = true;
    for (int i=0;i+1<(int)merged.size();++i) if(merged[i]>merged[i+1]) ok=false;
    CHECK(ok,                          "mergeSorted produces sorted result");
    CHECK(merged.size()==8,            "mergeSorted has all 8 elements");
}

static void test_quicksort() {
    std::cout << "\n── QuickSort ──\n";

    std::vector<int> v = {9,3,7,1,5,8,2,6,4};
    Sorting::quickSort<int>(v, [](int a, int b){ return a < b; });
    bool asc = true;
    for (int i=0;i+1<(int)v.size();++i) if(v[i]>v[i+1]) asc=false;
    CHECK(asc,                         "QuickSort integers ascending");

    // Already sorted (edge case for randomised pivot)
    std::vector<int> sorted={1,2,3,4,5,6,7,8,9,10};
    Sorting::quickSort<int>(sorted, [](int a, int b){ return a<b; });
    bool stillSorted = true;
    for (int i=0;i+1<(int)sorted.size();++i) if(sorted[i]>sorted[i+1]) stillSorted=false;
    CHECK(stillSorted,                 "QuickSort handles already-sorted input");

    // Large dataset
    std::vector<int> large(1000);
    for (int i=0;i<1000;++i) large[i] = 1000 - i;
    Sorting::quickSort<int>(large, [](int a, int b){ return a<b; });
    CHECK(large[0]==1 && large[999]==1000, "QuickSort large dataset correct");
}

static void test_binarysearch() {
    std::cout << "\n── BinarySearch ──\n";

    std::vector<int> v = {1,3,5,7,9,11,13,15};

    // lowerBound
    int lb5 = Search::lowerBound<int,int>(v, 5, [](const int& x){ return x; });
    CHECK(lb5 == 2,                    "lowerBound(5) == index 2");

    int lb6 = Search::lowerBound<int,int>(v, 6, [](const int& x){ return x; });
    CHECK(lb6 == 3,                    "lowerBound(6) == index 3 (between 5 and 7)");

    // upperBound
    int ub7 = Search::upperBound<int,int>(v, 7, [](const int& x){ return x; });
    CHECK(ub7 == 4,                    "upperBound(7) == index 4");

    // binarySearch
    int idx = Search::binarySearch<int,int>(v, 9,
        std::function<int(const int&)>([](const int& x){ return x; }));
    CHECK(idx == 4,                    "binarySearch finds 9 at index 4");

    int miss = Search::binarySearch<int,int>(v, 6,
        std::function<int(const int&)>([](const int& x){ return x; }));
    CHECK(miss == -1,                  "binarySearch returns -1 for missing key");

    // rangeSearch
    auto range = Search::rangeSearch<int,int>(v, 5, 11,
        std::function<int(const int&)>([](const int& x){ return x; }));
    CHECK(range.size() == 4,           "rangeSearch [5,11] returns {5,7,9,11}");

    // findOptimalTimeSlot
    std::vector<std::pair<long long,long long>> slots = {{100,200},{300,400},{500,600}};
    long long slot = Search::findOptimalTimeSlot(slots, 50, 0, 1000);
    CHECK(slot == 0,                   "optimal slot at t=0 (before first slot)");

    long long slot2 = Search::findOptimalTimeSlot(slots, 50, 150, 1000);
    CHECK(slot2 == 200,                "optimal slot at t=200 (after first slot ends)");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. ZonePartitioner
// ─────────────────────────────────────────────────────────────────────────────
static void test_zone_partitioner() {
    std::cout << "\n── ZonePartitioner (D&C) ──\n";

    auto locs = buildLocs();
    int numZones = 3;
    auto zones = DivideConquer::partitionZones(locs, numZones);

    CHECK((int)zones.size() >= 1, "partition creates at least 1 zone");

    // All locations should be in exactly one zone
    int total = 0;
    for (auto& z : zones) total += static_cast<int>(z.locations.size());
    CHECK(total == (int)locs.size(),   "all locations assigned to a zone");

    // Each zone non-empty
    bool allNonEmpty = true;
    for (auto& z : zones) if (z.locations.empty()) allNonEmpty = false;
    CHECK(allNonEmpty,                 "all zones non-empty");

    // Assign vehicles to zones
    std::vector<std::shared_ptr<Location>> depots = {
        locs[0], locs[4], locs[9]  // 3 depots for 3 zones
    };
    // assignVehiclesToZones not in this API - check assignPackagesToZones
    CHECK(zones.size() > 0, "zones created"); // assignment covered by dispatch test

}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║   Spatial / Sort / Search Tests      ║\n";
    std::cout << "╚══════════════════════════════════════╝\n";

    test_quadtree_build();
    test_quadtree_nearest();
    test_quadtree_radius();
    test_quadtree_range();
    test_quadtree_knearest();

    test_kdtree_build();
    test_kdtree_nearest();
    test_kdtree_radius();
    test_kdtree_knearest();

    test_closest_pair();

    test_mergesort();
    test_quicksort();
    test_binarysearch();

    test_zone_partitioner();

    std::cout << "\n══════════════════════════════════════\n";
    std::cout << "  Passed : " << s_passed << "\n";
    std::cout << "  Failed : " << s_failed << "\n";
    std::cout << "══════════════════════════════════════\n";
    return s_failed == 0 ? 0 : 1;
}