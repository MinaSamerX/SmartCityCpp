# Smart City Delivery & Traffic Management System

## Build
```bash
mkdir build && cd build
cmake ..
make
```

## Run
```bash
./SmartCitySystem
```

## Run Tests
```bash
cd build && ctest --output-on-failure
```

## Project Structure
| Folder | Purpose |
|--------|---------|
| `src/core` | Shared data structs (Location, Vehicle, Package, Customer) |
| `src/graph` | Road network + all graph algorithms |
| `src/hashtable` | Custom hash tables (chaining + open addressing) |
| `src/spatial` | QuadTree, KD-Tree, BST, Segment Tree |
| `src/scheduler` | Binary heap priority queue + delivery scheduler |
| `src/optimizer` | Greedy route optimizer, knapsack, activity selector |
| `src/dataprocessor` | Merge/quick sort, binary search, D&C algorithms |
| `src/system` | 6 system components + design patterns |
| `src/io` | File parser + analytics reporter |
| `data/` | Input text files |
| `tests/` | Unit & integration tests |
| `docs/` | Design document + UML diagram |

See `docs/design.md` for design pattern mapping and UML.
