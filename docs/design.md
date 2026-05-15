# Design Document
## Smart City Delivery & Traffic Management System
---

## 1. System Overview

The Smart City Delivery & Traffic Management System is a C++17 simulation of a real-world logistics platform. It manages package deliveries across a directed weighted city road graph, optimises vehicle routes, handles real-time traffic events, and coordinates multiple vehicles using advanced algorithms and object-oriented design patterns.

The system is divided into six core components, each encapsulating one responsibility, connected through a Facade entry point:

```
┌─────────────────────────────────────────────────────────┐
│                   SmartCitySystem (Facade)               │
├──────────┬──────────┬──────────┬──────────┬─────────────┤
│CityMap   │Entity    │Spatial   │Delivery  │Route        │
│Manager   │Manager   │Query     │Scheduler │Optimizer    │
│(Singleton│(HashTable│Engine    │(PQ + BST │(Greedy +    │
│+ Strategy│ lookup)  │(QuadTree │+ HashTbl)│ D&C)        │
│ pattern) │          │+ KDTree) │          │             │
└──────────┴──────────┴──────────┴──────────┴─────────────┘
                           ↓
                    TrafficMonitor
                    (Observer pattern)
```

---

## 2. Design Patterns

### 2.1 Singleton — `CityMapManager`

**File:** `src/system/CityMapManager.h/.cpp`

`CityMapManager` is the single source-of-truth for the city road graph. Every component accesses it via `CityMapManager::getInstance()`. This guarantees:
- One consistent graph state across the entire system
- Thread-safe first-use initialisation (C++11 Meyers Singleton)
- No possibility of two components working on different graph copies

```cpp
// Access from anywhere — always same instance
auto& mgr = CityMapManager::getInstance();
auto path = mgr.shortestPath(srcId, destId);
```

**UML:**
```
┌──────────────────────────────┐
│       CityMapManager         │
│──────────────────────────────│
│ - m_graph : RoadNetwork      │
│ - m_pathFinder : IPathFinder │
│──────────────────────────────│
│ + getInstance() : CityMapMgr&│  ← static, returns singleton
│ + loadFromFile()             │
│ + shortestPath()             │
│ + updateTraffic()            │
│ + buildMST()                 │
└──────────────────────────────┘
```

---

### 2.2 Strategy — `IPathFinder`

**File:** `src/system/IPathFinder.h`

The path-finding algorithm is a pluggable strategy. `CityMapManager` holds a `unique_ptr<IPathFinder>` and swaps it at runtime:
- `DijkstraFinder` — default, O(E log V), for non-negative edge weights
- `BellmanFordFinder` — activated automatically when a negative edge (toll discount) is detected

Callers never change — they only call `findPath()`.

```cpp
// Strategy swaps transparently when negative edge detected
mgr.addEdge(2, 4, -1.0);  // toll discount
// CityMapManager auto-switches to BellmanFordFinder
auto path = mgr.shortestPath(1, 4);  // same call, different algorithm
```

**UML:**
```
        «interface»
      IPathFinder
      ┌────────────────────┐
      │ + findPath()       │
      │ + findMultiStop()  │
      │ + name()           │
      └────────────────────┘
              △
      ┌───────┴───────┐
      │               │
DijkstraFinder  BellmanFordFinder
  O(E log V)        O(VE)
```

---

### 2.3 Observer — `TrafficMonitor`

**File:** `src/system/TrafficMonitor.h/.cpp`

`TrafficMonitor` is the Observable (Subject). Any class implementing `ITrafficObserver` can subscribe and be notified of traffic events.

Key implementation detail: observers are stored as `weak_ptr<ITrafficObserver>` — expired observers are silently swept during notification, preventing dangling references and memory leaks.

```
TrafficMonitor (Subject)
        │
        │ fireEvent(TrafficEvent)
        ▼
┌───────────────────────────┐
│  ITrafficObserver         │  «interface»
│  + onTrafficEvent(event)  │
└───────────────────────────┘
        △
        ├──────────────────────┐
DeliveryScheduler         LambdaObserver
(reorders PQ on           (lambda-based,
 high-severity events)     zero-boilerplate)
```

**Event types by severity:**

| Severity | Event | Action |
|----------|-------|--------|
| 5 | Accident | Close road + triple weight + boost affected pkg priority |
| 4 | RoadClosed | Close road in graph |
| 3 | Construction | Partial closure |
| 2 | Congestion | Update edge weight |
| 1 | Cleared / Opened | Restore weight |

---

### 2.4 Factory — `EntityFactory`

**File:** `src/system/EntityFactory.h`

All domain objects are created through `EntityFactory`. This centralises construction, auto-generates unique IDs (using `std::atomic<int>`), and ensures consistent initialisation across the system.

```cpp
// Callers never use make_shared directly
auto loc  = EntityFactory::createLocation(-1, "Hub", 10, 20, LocationType::Warehouse);
auto veh  = EntityFactory::createVehicle(-1, "Van-01", VehicleType::Van, 500, 80, 100, 1);
auto pkg  = EntityFactory::createPackage(-1, "", 1, 4, 0, 12.5, DeliveryPriority::High, dl);
auto cust = EntityFactory::createCustomer(-1, "Alice", "010", "a@b.com", 4);
```

---

### 2.5 Facade — `SmartCitySystem`

**File:** `src/system/SmartCitySystem.h/.cpp`

`SmartCitySystem` is the single entry point. It hides all six sub-components behind one clean API. `main.cpp` only interacts with `SmartCitySystem` — never with individual components directly.

```
main.cpp
   │
   ▼
SmartCitySystem (Facade)
   ├── loadFromDirectory()
   ├── processNewDelivery()   → CityMapManager + SpatialQueryEngine + DeliveryScheduler
   ├── dispatchFleet()        → RouteOptimizer + all components
   ├── handleTrafficUpdate()  → TrafficMonitor → CityMapManager + DeliveryScheduler
   ├── handleRoadClosure()    → TrafficMonitor → CityMapManager
   ├── shortestPath()         → CityMapManager
   ├── findNearestLocation()  → SpatialQueryEngine
   ├── findNearestVehicle()   → SpatialQueryEngine + EntityManager
   └── printReport()          → Reporter
```

---

## 3. Component Responsibilities

### Component 1 — CityMapManager (City Map)
- Load city graph from `city_map.txt` via `FileParser`
- Maintain `RoadNetwork` (adjacency list, edge weights, closure flags)
- Run shortest-path queries through plugged-in `IPathFinder` strategy
- Apply real-time traffic weight updates from `TrafficMonitor`
- Build MST (Kruskal or Prim) for infrastructure analysis
- Perform topological sort for one-way street ordering

### Component 2 — EntityManager (Hash-Based Lookup)
- Register and O(1)-lookup all domain entities by ID
- `HashTableOA<int, Location>` — open addressing for location IDs (cache-friendly)
- `HashTableOA<int, Vehicle>` — open addressing for vehicle IDs
- `HashTable<int, Package>` — separate chaining for packages (more entries)
- `HashTable<string, int>` — secondary index: tracking number → package ID
- `getAvailableVehicles()` — filter by `VehicleStatus::Idle`
- `getPendingPackages()` — filter by `DeliveryStatus::Pending`

### Component 3 — SpatialQueryEngine (Tree-Based)
- Build `QuadTree` and `KDTree` from all registered locations
- `nearestLocation(x, y)` — KDTree O(log N) nearest-neighbor
- `nearestOfType(x, y, type)` — expanding radius search by location type
- `nearestAvailableVehicle(x, y, vehicles)` — find closest idle vehicle
- `locationsInRadius(x, y, r)` — QuadTree circle-rectangle pruning
- `kNearest(x, y, k)` — K closest locations sorted by distance
- `locationsInBox(minX, minY, maxX, maxY)` — zone boundary queries

### Component 4 — DeliveryScheduler (Priority Queue)
Three data structures working together:
- `PriorityQueue` (binary max-heap) — O(log N) extract most-urgent package
- `BST` (deadline-ordered) — O(log N) range query: "due in next N seconds"
- `HashTable<int, Package>` — O(1) lookup for any package by ID

Implements `ITrafficObserver` — when a high-severity traffic event fires, packages on affected roads are boosted to `Urgent` priority and the heap is rebuilt.

### Component 5 — RouteOptimizer (Greedy)
- `ActivitySelector` (greedy activity selection) — select most profitable packages per time window
- `Knapsack` (fractional knapsack) — load packages by weight/volume ratio
- TSP nearest-neighbor heuristic — order stops for minimum travel
- `ZonePartitioner` (D&C) — geographically split delivery zones, assign one vehicle per zone
- `optimiseFleet()` — coordinates all above for full fleet dispatch

### Component 6 — DataProcessor (Divide & Conquer)
- `MergeSort` — stable O(N log N) sort by deadline / priority / weight
- `QuickSort` — in-place O(N log N) for large datasets (vehicles/packages)
- `BinarySearch` — lower/upper bound, range search in sorted schedules
- `ClosestPair` — O(N log N) find two nearest delivery locations for batching
- `ZonePartitioner` — recursive geographic zone splitting

---

## 4. Data Flow — New Delivery Request

As described in the PDF "Sample Operations Flow":

```
1. EntityManager (Hash)    → verify customer + location IDs exist   O(1)
2. SpatialQueryEngine      → find nearest available vehicle          O(log N)
3. DeliveryScheduler (PQ)  → insert delivery with priority score     O(log N)
4. CityMapManager (Dijkstra)→ calculate optimal route src → dest     O(E log V)
5. RouteOptimizer (Greedy) → if vehicle has multiple deliveries,
                              order stops by nearest-neighbor TSP     O(N²)
6. DataProcessor           → binary search for optimal time slot     O(log N)
```

---

## 5. Data Flow — Rush Hour Traffic Update

```
1. TrafficMonitor          → reportCongestion(from, to, newWeight)
2. CityMapManager          → updateEdgeWeight(from, to, newWeight)
3. TrafficMonitor          → notifyAll(TrafficEvent)
4. DeliveryScheduler       → onTrafficEvent → boost affected packages → rebuild PQ
5. CityMapManager (Dijkstra)→ next shortestPath() call uses updated weights automatically
6. RouteOptimizer          → reassign packages to less-busy vehicles if severity >= 3
```

---

## 6. Class Relationships (Simplified UML)

```
SmartCitySystem
    │ uses
    ├──► CityMapManager ──► RoadNetwork ──► Dijkstra / BellmanFord / MST / BFS / Topo
    │         └──► IPathFinder (Strategy)
    │
    ├──► EntityManager ──► HashTable<int, Location>
    │                  ──► HashTableOA<int, Vehicle>
    │                  ──► HashTable<int, Package>
    │                  ──► HashTable<string, int>   (tracking index)
    │
    ├──► SpatialQueryEngine ──► QuadTree
    │                       ──► KDTree
    │
    ├──► DeliveryScheduler ──► PriorityQueue (binary heap)
    │    (ITrafficObserver)──► BST (deadline tree)
    │                       ──► HashTable<int, Package>
    │
    ├──► RouteOptimizer ──► ActivitySelector (greedy)
    │                   ──► Knapsack (fractional)
    │                   ──► ZonePartitioner (D&C)
    │                   ──► ClosestPair (D&C)
    │
    ├──► TrafficMonitor ──► vector<weak_ptr<ITrafficObserver>>
    │    (Observable)        └──► DeliveryScheduler
    │                            └──► LambdaObserver
    │
    └──► Reporter ──► MergeSort / QuickSort / BinarySearch

EntityFactory ──creates──► Location, Vehicle, Package, Customer
FileParser    ──parses──►   city_map.txt → RoadNetwork
                            locations.txt → Location[]
                            vehicles.txt  → Vehicle[]
                            deliveries.txt → Package[]
                            traffic_updates.txt → TrafficUpdate[]
```

---

## 7. Complexity Summary

| Operation | Data Structure | Complexity |
|-----------|---------------|-----------|
| Register entity | HashTable / HashTableOA | O(1) avg |
| Lookup entity by ID | HashTable / HashTableOA | O(1) avg |
| Lookup package by tracking no. | HashTable<string,int> | O(1) avg |
| Shortest path | Dijkstra (binary heap) | O(E log V) |
| Negative-weight path | Bellman-Ford | O(VE) |
| Connectivity check | BFS | O(V+E) |
| MST construction | Kruskal (Union-Find) | O(E log E) |
| Topo sort | Kahn's BFS | O(V+E) |
| Nearest location | KDTree | O(log N) |
| Radius search | QuadTree | O(log N + k) |
| Insert delivery | Binary heap PQ | O(log N) |
| Extract most urgent | Binary heap PQ | O(log N) |
| Deadline range query | BST | O(log N + k) |
| Traffic density query | Segment Tree | O(log N) |
| Sort deliveries | Merge Sort | O(N log N) |
| Find closest pair | D&C Closest Pair | O(N log N) |
| Zone partitioning | D&C Recursive | O(N log N) |
| Package selection | Activity Selection | O(N log N) |

---

## 8. Modern C++11 Features Used

| Feature | Where |
|---------|-------|
| `shared_ptr` / `unique_ptr` | All domain objects, tree nodes, observer storage |
| `weak_ptr` | `TrafficMonitor` observer list — prevents ownership cycles |
| `std::move` / move semantics | All constructor parameters (strings, vectors) |
| `std::make_shared` / `make_unique` | All heap allocations |
| Lambda functions | `forEach`, visitor callbacks, comparators |
| `std::function` | PriorityQueue comparator, HashTable visitor, path-finder callbacks |
| `enum class` | `LocationType`, `VehicleStatus`, `DeliveryPriority`, `TrafficEventType` |
| `auto` + structured bindings | Dijkstra heap entries, Prim priority queue |
| Template policies | `HashTable<K,V,Hasher>`, `PriorityQueue<T,KeyFn,Cmp>`, `BST<T,Key,KeyFn,Cmp>` |
| `std::atomic<int>` | `EntityFactory` ID counters — thread-safe auto-increment |
| `std::numeric_limits` | INF_WEIGHT sentinel, SegmentTree identity values |
| In-class member initialisation | All struct/class default values |
| `= default` / `= delete` | Rule of Zero, non-copyable Singleton |

---