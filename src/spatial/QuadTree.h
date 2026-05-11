#pragma once
// src/spatial/QuadTree.h
// Smart City Delivery & Traffic Management System

#include <vector>
#include <memory>
#include <cmath>
#include <functional>
#include <algorithm>
#include <ostream>
#include "../core/Location.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: BoundingBox
//  Axis-aligned rectangle used to partition 2-D city space.
// ─────────────────────────────────────────────────────────────────────────────
struct BoundingBox {
    double cx, cy;      // center
    double halfW, halfH;// half-extents

    BoundingBox(double cx, double cy, double hw, double hh)
        : cx(cx), cy(cy), halfW(hw), halfH(hh) {}

    bool contains(double x, double y) const {
        return x >= cx - halfW && x <= cx + halfW
            && y >= cy - halfH && y <= cy + halfH;
    }

    bool intersects(const BoundingBox& o) const {
        return !(o.cx - o.halfW > cx + halfW
              || o.cx + o.halfW < cx - halfW
              || o.cy - o.halfH > cy + halfH
              || o.cy + o.halfH < cy - halfH);
    }

    // Circle-rectangle intersection (for radius queries)
    bool intersectsCircle(double qx, double qy, double r) const {
        double dx = std::max(std::abs(qx - cx) - halfW, 0.0);
        double dy = std::max(std::abs(qy - cy) - halfH, 0.0);
        return (dx * dx + dy * dy) <= (r * r);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Class: QuadTree
//
//  2-D spatial index that partitions city locations into quadrants.
//
//  Responsibilities:
//    - insert(Location)          O(log N) average
//    - queryRadius(x,y,r)        O(log N + k)   k = results
//    - nearestNeighbor(x,y)      O(log N)
//    - queryRange(BoundingBox)   O(log N + k)
//
//  OOP / Modern C++:
//    - Children owned by unique_ptr → automatic cleanup
//    - Locations stored as shared_ptr (shared with RoadNetwork)
//    - Subdivision on capacity threshold (configurable)
//    - Const-correct query methods
// ─────────────────────────────────────────────────────────────────────────────
class QuadTree {
public:
    static constexpr int DEFAULT_CAPACITY = 4;   // points per node before split
    static constexpr int MAX_DEPTH        = 12;  // prevent infinite subdivision

    // ── Construction ─────────────────────────────────────────────────────────
    QuadTree(BoundingBox boundary, int capacity = DEFAULT_CAPACITY, int depth = 0)
        : m_boundary(boundary)
        , m_capacity(capacity)
        , m_depth(depth)
        , m_divided(false)
    {}

    // Factory: build QuadTree from a list of locations
    static QuadTree build(const std::vector<std::shared_ptr<Location>>& locs,
                          int capacity = DEFAULT_CAPACITY);

    // ── Mutation ──────────────────────────────────────────────────────────────
    bool insert(std::shared_ptr<Location> loc);

    // ── Queries ───────────────────────────────────────────────────────────────

    // All locations within radius r of (qx, qy)
    std::vector<std::shared_ptr<Location>>
    queryRadius(double qx, double qy, double r) const;

    // All locations within a bounding box
    std::vector<std::shared_ptr<Location>>
    queryRange(const BoundingBox& range) const;

    // Single nearest location to (qx, qy) — nullptr if empty
    std::shared_ptr<Location>
    nearestNeighbor(double qx, double qy) const;

    // K nearest locations
    std::vector<std::shared_ptr<Location>>
    kNearest(double qx, double qy, int k) const;

    // ── Metrics ───────────────────────────────────────────────────────────────
    int  size()   const;
    void print(std::ostream& os, int indent = 0) const;

private:
    BoundingBox m_boundary;
    int         m_capacity;
    int         m_depth;
    bool        m_divided;

    std::vector<std::shared_ptr<Location>> m_points;

    // NW, NE, SW, SE
    std::unique_ptr<QuadTree> m_nw, m_ne, m_sw, m_se;

    void subdivide();

    // Internal nearest-neighbor helper: updates best (dist², location)
    void nearestRec(double qx, double qy,
                    double& bestDist2,
                    std::shared_ptr<Location>& bestLoc) const;
};