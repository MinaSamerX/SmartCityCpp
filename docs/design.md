# Design Document — Smart City Delivery & Traffic Management System

## Design Patterns

| Pattern | Class | Purpose |
|---------|-------|---------|
| Singleton | `CityMapManager` | One global road network instance |
| Strategy | `IPathFinder` | Swap Dijkstra / Bellman-Ford at runtime |
| Observer | `TrafficMonitor` → `Vehicle` | Broadcast traffic events |
| Factory | `EntityFactory` | Create Location / Vehicle / Package |
| Facade | `SmartCitySystem` | Single entry point hiding all subsystems |
| Template Method | Sorter base class | Sort skeleton, override comparator |

## UML Class Diagram
![UML](UML_ClassDiagram.png)

## Component Responsibilities

### CityMapManager (Singleton)
Load graph, update traffic weights, shortest path, detect road closures.

### EntityManager
Register and O(1)-lookup locations, vehicles, customers via HashTable.

### SpatialQueryEngine
QuadTree/KD-Tree for nearest-vehicle search and radius delivery queries.

### DeliveryScheduler
Binary heap priority queue — urgent deliveries first, dynamic reordering.

### RouteOptimizer
Greedy route selection, fractional knapsack loading, TSP nearest-neighbor.

### DataProcessor
Merge/quick sort, binary search, closest-pair, D&C zone partitioning.
