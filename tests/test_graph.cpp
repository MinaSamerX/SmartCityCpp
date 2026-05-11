// tests/test_graph.cpp
// Smart City Delivery & Traffic Management System
// ── RoadNetwork + BFS/DFS + Dijkstra + BellmanFord + MST + TopoSort Tests ────

#include "graph/RoadNetwork.h"
#include "graph/BFS_DFS.h"
#include "graph/Dijkstra.h"
#include "graph/BellmanFord.h"
#include "graph/MST.h"
#include "graph/TopologicalSort.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <unordered_map>

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

// ─────────────────────────────────────────────────────────────────────────────
//  Shared test graph  (mirrors city_map.txt structure)
//
//    1(Warehouse) --5--> 2(JunctionA) --4--> 3(JunctionB) --3--> 4(CustomerX)
//         |                   |                                        ^
//         +--------8-------> 5(Charging) ----------6------------------+
//
// ─────────────────────────────────────────────────────────────────────────────
static RoadNetwork buildGraph() {
    RoadNetwork g;
    g.addNode(1, "Warehouse",  0,  0, LocationType::Warehouse);
    g.addNode(2, "JunctionA",  5,  0, LocationType::Intersection);
    g.addNode(3, "JunctionB",  5,  5, LocationType::Intersection);
    g.addNode(4, "CustomerX", 10,  5, LocationType::Customer);
    g.addNode(5, "Charging",  10,  0, LocationType::ChargingStation);
    g.addEdge(1, 2, 5.0);
    g.addEdge(2, 3, 4.0);
    g.addEdge(3, 4, 3.0);
    g.addEdge(2, 5, 5.0);
    g.addEdge(5, 4, 6.0);
    g.addEdge(1, 5, 8.0);
    return g;
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. RoadNetwork
// ─────────────────────────────────────────────────────────────────────────────
static void test_roadnetwork() {
    std::cout << "\n── RoadNetwork ──\n";
    auto g = buildGraph();

    CHECK(g.nodeCount() == 5,          "nodeCount == 5");
    CHECK(g.edgeCount() == 6,          "edgeCount == 6");
    CHECK(g.hasNode(1),                "hasNode(1)");
    CHECK(!g.hasNode(99),              "!hasNode(99)");
    CHECK(g.hasEdge(1, 2),             "hasEdge(1,2)");
    CHECK(!g.hasEdge(4, 1),            "!hasEdge(4,1) — directed");
    CHECK(APPROX(g.getEdgeWeight(1,2), 5.0), "getEdgeWeight(1,2)==5");
    CHECK(g.getEdgeWeight(1,99) == INF_WEIGHT, "missing edge → INF");

    // updateEdgeWeight
    g.updateEdgeWeight(1, 2, 9.0);
    CHECK(APPROX(g.getEdgeWeight(1,2), 9.0), "updateEdgeWeight");
    g.updateEdgeWeight(1, 2, 5.0);            // restore

    // addBidirectionalEdge
    g.addNode(6, "Extra", 15, 5);
    g.addBidirectionalEdge(4, 6, 2.0);
    CHECK(g.hasEdge(4, 6) && g.hasEdge(6, 4), "bidirectional edge both ways");
    g.removeNode(6);
    CHECK(!g.hasNode(6),               "removeNode cleans node");
    CHECK(!g.hasEdge(4, 6),            "removeNode cleans outgoing edges");

    // Road closure
    g.setRoadClosed(2, 3, true);
    CHECK(g.isRoadClosed(2, 3),        "setRoadClosed true");
    g.setRoadClosed(2, 3, false);
    CHECK(!g.isRoadClosed(2, 3),       "setRoadClosed false");

    // getAllNodeIds
    auto ids = g.getAllNodeIds();
    CHECK(ids.size() == 5,             "getAllNodeIds returns 5 ids");

    // Location retrieval
    auto loc = g.getLocation(1);
    CHECK(loc && loc->getName() == "Warehouse", "getLocation(1) == Warehouse");
    CHECK(!g.getLocation(99),          "getLocation(missing) == nullptr");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. BFS
// ─────────────────────────────────────────────────────────────────────────────
static void test_bfs() {
    std::cout << "\n── BFS ──\n";
    auto g = buildGraph();

    auto order = GraphSearch::bfs(g, 1);
    CHECK(order.size() == 5,           "BFS visits all 5 nodes");
    CHECK(order[0] == 1,               "BFS starts at source");

    CHECK(GraphSearch::isReachable(g, 1, 4),  "1 → 4 reachable");
    CHECK(!GraphSearch::isReachable(g, 4, 1), "4 → 1 unreachable (directed)");
    CHECK(GraphSearch::isReachable(g, 1, 1),  "node reachable from itself");

    // Closure blocks path
    g.setRoadClosed(2, 3, true);
    // Still reachable via 1→5→4
    CHECK(GraphSearch::isReachable(g, 1, 4),  "reachable via alt after closure");
    g.setRoadClosed(1, 5, true);
    // Now 4 still reachable via 1→2→5→4
    g.setRoadClosed(2, 5, true);
    // 1→4 now needs 1→2→3→4 but 2→3 is closed, and 1→5 closed
    CHECK(!GraphSearch::isReachable(g, 1, 4), "unreachable after all closures");
    // Restore
    g.setRoadClosed(2, 3, false);
    g.setRoadClosed(1, 5, false);
    g.setRoadClosed(2, 5, false);

    // Lambda visitor
    int visitCount = 0;
    GraphSearch::bfs(g, 1, [&visitCount](int){ ++visitCount; });
    CHECK(visitCount == 5,             "BFS lambda visitor called for each node");

    // Connected components
    auto comps = GraphSearch::allConnectedComponents(g);
    CHECK(!comps.empty(),              "connectedComponents non-empty");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. DFS
// ─────────────────────────────────────────────────────────────────────────────
static void test_dfs() {
    std::cout << "\n── DFS ──\n";
    auto g = buildGraph();

    auto order = GraphSearch::dfs(g, 1);
    CHECK(order.size() == 5,           "DFS visits all 5 nodes");
    CHECK(order[0] == 1,               "DFS starts at source");

    // Visitor
    std::vector<int> visited;
    GraphSearch::dfs(g, 1, [&visited](int n){ visited.push_back(n); });
    CHECK(visited.size() == 5,         "DFS lambda visitor fires 5 times");

    // Unreachable from isolated node
    RoadNetwork g2;
    g2.addNode(10, "A", 0, 0);
    g2.addNode(11, "B", 5, 5);  // no edges
    CHECK(!GraphSearch::isReachable(g2, 10, 11), "isolated nodes unreachable");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Topological Sort
// ─────────────────────────────────────────────────────────────────────────────
static void test_topo() {
    std::cout << "\n── Topological Sort ──\n";
    auto g = buildGraph();

    auto res = TopoSort::topologicalSort(g);
    CHECK(!res.hasCycle,               "no cycle in test graph");
    CHECK(res.order.size() == 5,       "topo order has all 5 nodes");

    // Verify ordering: for every edge u→v, u appears before v
    std::unordered_map<int,int> pos;
    for (int i = 0; i < (int)res.order.size(); ++i) pos[res.order[i]] = i;
    bool orderOk = true;
    for (int u : g.getAllNodeIds())
        for (const auto& e : g.getNeighbours(u))
            if (!e.isClosed && pos[u] >= pos[e.to]) { orderOk = false; break; }
    CHECK(orderOk,                     "topo order satisfies all edge constraints");

    CHECK(TopoSort::isDAG(g),          "isDAG returns true");

    // Cycle detection
    RoadNetwork cyc;
    cyc.addNode(1,"A",0,0); cyc.addNode(2,"B",1,0); cyc.addNode(3,"C",2,0);
    cyc.addEdge(1,2,1); cyc.addEdge(2,3,1); cyc.addEdge(3,1,1);
    auto cycRes = TopoSort::topologicalSort(cyc);
    CHECK(cycRes.hasCycle,             "cycle detected");
    CHECK(!cycRes.cycleNodes.empty(),  "cycleNodes populated");
    CHECK(!TopoSort::isDAG(cyc),       "isDAG returns false for cyclic graph");

    // getDeliveryOrder
    auto ordered = TopoSort::getDeliveryOrder(g, {4, 2, 1});
    CHECK(!ordered.empty(),            "getDeliveryOrder non-empty");
    CHECK(ordered[0] == 1,             "node 1 first in order (source)");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Dijkstra
// ─────────────────────────────────────────────────────────────────────────────
static void test_dijkstra() {
    std::cout << "\n── Dijkstra ──\n";
    auto g = buildGraph();

    // 1→4: optimal is 1→2→3→4 = 5+4+3 = 12
    auto r = Dijkstra::shortestPath(g, 1, 4);
    CHECK(r.reachable,                 "1→4 reachable");
    CHECK(APPROX(r.totalCost, 12.0),   "1→4 cost == 12");
    CHECK(r.path.front() == 1,         "path starts at 1");
    CHECK(r.path.back()  == 4,         "path ends at 4");

    // 1→1 (same node)
    auto self = Dijkstra::shortestPath(g, 1, 1);
    CHECK(self.reachable && APPROX(self.totalCost, 0.0), "1→1 cost == 0");

    // Unreachable (directed graph, no return edges)
    auto unr = Dijkstra::shortestPath(g, 4, 1);
    CHECK(!unr.reachable,              "4→1 unreachable");

    // Traffic update changes shortest path
    g.updateEdgeWeight(2, 3, 100.0);  // 1→2→3→4 now costs 108
    auto r2 = Dijkstra::shortestPath(g, 1, 4);
    CHECK(r2.reachable,                "still reachable after weight update");
    CHECK(r2.totalCost < 108.0,        "takes cheaper route after congestion");
    g.updateEdgeWeight(2, 3, 4.0);    // restore

    // Road closure forces detour
    g.setRoadClosed(2, 3, true);
    auto r3 = Dijkstra::shortestPath(g, 1, 4);
    CHECK(r3.reachable,                "reachable via detour after closure");
    CHECK(r3.totalCost > 12.0,         "detour costs more than direct path");
    g.setRoadClosed(2, 3, false);

    // Multi-stop
    auto ms = Dijkstra::shortestPathMultiStop(g, {1, 2, 4});
    CHECK(ms.reachable,                "multi-stop 1→2→4 reachable");
    CHECK(ms.path.front() == 1 && ms.path.back() == 4, "multi-stop endpoints");

    // All distances from node 1
    auto all = Dijkstra::allShortestPaths(g, 1);
    CHECK(APPROX(all[1], 0.0),         "allDistances: dist(1,1)==0");
    CHECK(APPROX(all[4], 12.0),        "allDistances: dist(1,4)==12");
    CHECK(all[5] < INF_WEIGHT,         "allDistances: node 5 reachable");

    // Invalid nodes
    auto inv = Dijkstra::shortestPath(g, 99, 1);
    CHECK(!inv.reachable,              "invalid src returns unreachable");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Bellman-Ford
// ─────────────────────────────────────────────────────────────────────────────
static void test_bellmanford() {
    std::cout << "\n── Bellman-Ford ──\n";
    auto g = buildGraph();

    // Matches Dijkstra on positive weights
    auto bf = BellmanFord::shortestPath(g, 1, 4);
    CHECK(bf.reachable && !bf.hasNegativeCycle, "1→4 reachable, no neg cycle");
    CHECK(APPROX(bf.totalCost, 12.0),  "BF cost matches Dijkstra (12)");

    // Negative toll discount
    g.addEdge(2, 4, -1.0, 999);        // shortcut with discount
    auto bfNeg = BellmanFord::shortestPath(g, 1, 4);
    CHECK(bfNeg.reachable && bfNeg.totalCost < 12.0, "neg edge gives cheaper path");
    g.removeEdge(2, 4);

    // Negative cycle detection
    RoadNetwork negG;
    negG.addNode(1,"A",0,0); negG.addNode(2,"B",1,0); negG.addNode(3,"C",2,0);
    negG.addEdge(1,2, 1.0);
    negG.addEdge(2,3,-2.0);
    negG.addEdge(3,1,-2.0);
    auto bfCyc = BellmanFord::shortestPath(negG, 1, 3);
    CHECK(bfCyc.hasNegativeCycle,      "negative cycle detected");

    // Unreachable
    auto unr = BellmanFord::shortestPath(g, 4, 1);
    CHECK(!unr.reachable,              "4→1 unreachable");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. MST
// ─────────────────────────────────────────────────────────────────────────────
static void test_mst() {
    std::cout << "\n── MST (Kruskal + Prim) ──\n";
    auto g = buildGraph();

    // Kruskal
    auto k = MST::kruskal(g);
    CHECK(k.isComplete,                "Kruskal: spanning tree complete");
    CHECK(k.edges.size() == 4,         "Kruskal: V-1 = 4 edges");
    CHECK(k.totalWeight > 0,           "Kruskal: positive total weight");

    // Prim — same total weight
    auto p = MST::prim(g, 1);
    CHECK(p.isComplete,                "Prim: spanning tree complete");
    CHECK(p.edges.size() == 4,         "Prim: V-1 = 4 edges");
    CHECK(APPROX(p.totalWeight, k.totalWeight), "Prim weight == Kruskal weight");

    // Disconnected graph
    RoadNetwork disc;
    disc.addNode(1,"A",0,0);
    disc.addNode(2,"B",10,10);  // no edges
    auto dk = MST::kruskal(disc);
    CHECK(!dk.isComplete,              "disconnected graph: MST not complete");

    // Union-Find via single connected component
    RoadNetwork line;
    line.addNode(1,"A",0,0); line.addNode(2,"B",1,0);
    line.addNode(3,"C",2,0); line.addNode(4,"D",3,0);
    line.addEdge(1,2,1); line.addEdge(2,3,2);
    line.addEdge(3,4,3); line.addEdge(1,4,10); // 10 should NOT be in MST
    auto lk = MST::kruskal(line);
    CHECK(lk.isComplete,               "line graph: MST complete");
    bool noExpensiveEdge = true;
    for (auto& e : lk.edges) if (e.weight >= 10.0) { noExpensiveEdge = false; }
    CHECK(noExpensiveEdge,             "MST excludes expensive edge (weight=10)");
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║       Graph Algorithm Test Suite     ║\n";
    std::cout << "╚══════════════════════════════════════╝\n";

    test_roadnetwork();
    test_bfs();
    test_dfs();
    test_topo();
    test_dijkstra();
    test_bellmanford();
    test_mst();

    std::cout << "\n══════════════════════════════════════\n";
    std::cout << "  Passed : " << s_passed << "\n";
    std::cout << "  Failed : " << s_failed << "\n";
    std::cout << "══════════════════════════════════════\n";
    return s_failed == 0 ? 0 : 1;
}