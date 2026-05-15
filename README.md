# Smart City Delivery & Traffic Management System

> **Architecture & Implementation:** Mina  
> **Code comments, refinement & error resolution:** Claude Sonnet 4.6 

A comprehensive C++17 simulation of a real-world logistics and traffic coordination platform.  
Implements advanced data structures, graph algorithms, design patterns, and greedy / divide-and-conquer strategies — covering every requirement of the EDGES Advanced Software Development project brief.

---

## How to Build

> **Requirements:** MinGW (g++ 13.1.0+), CMake 3.16+

### ✅ Recommended — use `build.bat`

From the **project root** folder:

```bat
.\build.bat
```

What it does:
1. Deletes the old `build/` directory
2. Runs `cmake ..` to configure
3. Runs `mingw32-make` to compile all targets

All binaries are placed in `build/bin/`:

| Binary | Description |
|--------|-------------|
| `SmartCityApp.exe` | Main application — full system demo |
| `test_graph.exe` | Graph algorithm tests |
| `test_hashtable.exe` | Hash table tests |
| `test_integration.exe` | End-to-end scenario tests |
| `test_priorityqueue.exe` | PriorityQueue / BST / SegmentTree tests |
| `test_quadtree.exe` | Spatial / Sort / Search tests |

## How to Run

### ✅ Recommended — use `run.bat`

From the **project root** folder:

```bat
.\run.bat
```

Runs all executables in sequence

> The app **auto-detects** the `data/` folder whether you run from the project root
> or from `build/bin/` — no manual path configuration needed.

### Run main app only

```bat
.\build\bin\SmartCityApp.exe
```

### Run a single test

```bat
.\build\bin\test_graph.exe
.\build\bin\test_hashtable.exe
.\build\bin\test_priorityqueue.exe
.\build\bin\test_quadtree.exe
.\build\bin\test_integration.exe
```
---

## Expected Output

### Main App

```
======================================================
  Smart City Delivery & Traffic Management System
======================================================
Data directory : ../../data
[SmartCitySystem] Graph loaded: 10 nodes, 15 edges
[SmartCitySystem] Entities: 10 locations, 8 vehicles, 10 packages
[SmartCitySystem] Spatial index built: 10 locations
[SmartCitySystem] Scheduler: 10 pending deliveries
[SmartCitySystem] Applied 10 traffic updates

── Spatial Queries (QuadTree + KDTree) ──
  Nearest location to (25,30): Downtown
  Nearest vehicle to (10,20):  Vehicle-201  (at location 1)
  1 → 10: cost=90  hops: 1 7 8 9 10
  2 → 9:  cost=53  hops: 2 3 5 9

── Scenario 5: Fleet Dispatch ──
[Fleet Dispatch] Generated 8 routes

── Scenario 2: Rush Hour Traffic Update ──
  Path BEFORE rush: cost=93   via: 1 2 3 5 6
  Path AFTER  rush: cost=166  via: 1 2 4 5 6
  Cost delta: +73

── Scenario 3: Road Closure & Accident ──
  Road 7→8 CLOSED
  Accident 3→5: road closed + penalised
  Path 1→5 rerouted: cost=86  via: 1 2 4 5
```

### Test Results

```
test_graph              Passed: 69    Failed: 0
test_hashtable          Passed: 56    Failed: 0
test_integration        Passed: 55    Failed: 0
test_priorityqueue      Passed: 58    Failed: 0
test_quadtree           Passed: 55    Failed: 0
─────────────────────────────────────────────────
TOTAL                   Passed: 293   Failed: 0
```

---

## Project Structure

```
SmartCitySystem/
├── build.bat                        ← Build all targets
├── run.bat                          ← Run app + all tests
├── CMakeLists.txt
├── data/
│   ├── city_map.txt                 ← 10 nodes, 15 directed edges
│   ├── locations.txt                ← Coordinates and types
│   ├── vehicles.txt                 ← 8 vehicles (Van, Bike, Truck)
│   ├── deliveries.txt               ← 10 delivery packages
│   └── traffic_updates.txt          ← Real-time weight changes
├── src/
│   ├── core/                        ← Location, Vehicle, Package, Customer
│   ├── graph/                       ← RoadNetwork, Dijkstra, BellmanFord,
│   │                                   BFS/DFS, MST, TopologicalSort
│   ├── hashtable/                   ← HashTable (chaining), HashTableOA (OA)
│   ├── spatial/                     ← QuadTree, KDTree, BST, SegmentTree
│   ├── scheduler/                   ← PriorityQueue (binary heap), DeliveryScheduler
│   ├── dataprocessor/               ← MergeSort, QuickSort, BinarySearch,
│   │                                   ClosestPair, ZonePartitioner
│   ├── optimizer/                   ← RouteOptimizer, Knapsack, ActivitySelector
│   ├── io/                          ← FileParser, Reporter
│   ├── system/                      ← SmartCitySystem (Facade)
│   │                                   CityMapManager (Singleton)
│   │                                   EntityManager, SpatialQueryEngine
│   │                                   TrafficMonitor (Observer)
│   │                                   IPathFinder (Strategy)
│   │                                   EntityFactory (Factory)
│   └── main.cpp
├── tests/
│   ├── test_graph.cpp               ← 69 tests
│   ├── test_hashtable.cpp           ← 56 tests
│   ├── test_priorityqueue.cpp       ← 58 tests
│   ├── test_quadtree.cpp            ← 55 tests
│   └── test_integration.cpp         ← 55 tests (all PDF scenarios)
└── docs/
    └── design.md                    ← Architecture, UML, design patterns
```

---

## Algorithms Implemented

| Algorithm | File | Complexity | Purpose |
|-----------|------|-----------|---------|
| Dijkstra | `graph/Dijkstra.h` | O(E log V) | Shortest delivery route |
| Bellman-Ford | `graph/BellmanFord.h` | O(VE) | Negative-weight edges (toll discounts) |
| BFS | `graph/BFS_DFS.h` | O(V+E) | Reachability, connected components |
| DFS | `graph/BFS_DFS.h` | O(V+E) | Road closure detection |
| Kruskal | `graph/MST.h` | O(E log E) | MST infrastructure planning |
| Prim | `graph/MST.h` | O((V+E) log V) | MST alternative |
| Topological Sort | `graph/TopologicalSort.h` | O(V+E) | One-way street ordering |
| QuadTree | `spatial/QuadTree.h` | O(log N) | Nearest vehicle, radius search |
| KD-Tree | `spatial/KDTree.h` | O(log N) | Balanced spatial search |
| Segment Tree | `spatial/SegmentTree.h` | O(log N) | Traffic density range queries |
| BST | `spatial/BST.h` | O(log N) | Deadline-sorted delivery queue |
| Binary Heap PQ | `scheduler/PriorityQueue.h` | O(log N) | Urgent delivery ordering |
| Merge Sort | `dataprocessor/MergeSort.h` | O(N log N) | Sort by deadline / priority |
| Quick Sort | `dataprocessor/QuickSort.h` | O(N log N) | Large dataset sorting |
| Binary Search | `dataprocessor/BinarySearch.h` | O(log N) | Schedule search |
| Closest Pair D&C | `dataprocessor/ClosestPair.h` | O(N log N) | Batch nearby deliveries |
| Fractional Knapsack | `optimizer/Knapsack.h` | O(N log N) | Package loading by weight |
| Activity Selection | `optimizer/ActivitySelector.h` | O(N log N) | Maximise deliveries per window |
| Zone Partitioning | `dataprocessor/ZonePartitioner.h` | O(N log N) | Split city into vehicle zones |

---

## Design Patterns

| Pattern | Class | Purpose |
|---------|-------|---------|
| **Singleton** | `CityMapManager` | One global road network instance — all components share it |
| **Strategy** | `IPathFinder` → `DijkstraFinder` / `BellmanFordFinder` | Swap path algorithm at runtime based on edge weights |
| **Observer** | `TrafficMonitor` → `ITrafficObserver` | Broadcast traffic events; observers auto-cleaned via `weak_ptr` |
| **Factory** | `EntityFactory` | Centralised construction of Location / Vehicle / Package / Customer |
| **Facade** | `SmartCitySystem` | Single public API hiding all 6 subsystems |
| **Template Method** | `Sorter` base | Sorting skeleton; subclasses override comparator |

---

## Data File Formats

### city_map.txt
```
<num_nodes> <num_edges>
<node_id> <name> <x> <y>
<from_id> <to_id> <weight>
```

### locations.txt
```
<id> <name> <x> <y> <type>
```
Types: `Warehouse`, `Commercial`, `Medical`, `Transport`, `Industrial`, `Residential`, `Logistics`

### vehicles.txt
```
<id> <capacity_kg> <speed_kmh> <battery_kwh> <start_location_id>
```

### deliveries.txt
```
<id> <src_id> <dest_id> <deadline_epoch> <priority> <weight_kg>
```
Priorities: `Critical` `High` `Medium` `Low`

### traffic_updates.txt
```
<from_id> <to_id> <new_weight> <timestamp_epoch>
```
---

See `docs/design.md` for design pattern mapping and UML.
