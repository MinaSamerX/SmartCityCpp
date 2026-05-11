// src/spatial/QuadTree.cpp
// Smart City Delivery & Traffic Management System

#include "QuadTree.h"
#include <algorithm>
#include <limits>
#include <queue>

// ─────────────────────────────────────────────────────────────────────────────
//  Factory
// ─────────────────────────────────────────────────────────────────────────────
QuadTree QuadTree::build(const std::vector<std::shared_ptr<Location>>& locs,
                         int capacity)
{
    if (locs.empty()) return QuadTree({0,0,1000,1000}, capacity);

    // Compute tight bounding box from actual coordinates
    double minX = locs[0]->getX(), maxX = minX;
    double minY = locs[0]->getY(), maxY = minY;
    for (const auto& l : locs) {
        minX = std::min(minX, l->getX()); maxX = std::max(maxX, l->getX());
        minY = std::min(minY, l->getY()); maxY = std::max(maxY, l->getY());
    }

    double cx   = (minX + maxX) / 2.0;
    double cy   = (minY + maxY) / 2.0;
    double hw   = (maxX - minX) / 2.0 + 1.0; // +1 margin
    double hh   = (maxY - minY) / 2.0 + 1.0;

    QuadTree qt({cx, cy, hw, hh}, capacity);
    for (const auto& l : locs) qt.insert(l);
    return qt;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Insert
// ─────────────────────────────────────────────────────────────────────────────
bool QuadTree::insert(std::shared_ptr<Location> loc) {
    if (!m_boundary.contains(loc->getX(), loc->getY())) return false;

    if (!m_divided) {
        if (static_cast<int>(m_points.size()) < m_capacity
            || m_depth >= MAX_DEPTH) {
            m_points.push_back(std::move(loc));
            return true;
        }
        subdivide();
    }

    // Try children
    if (m_nw->insert(loc)) return true;
    if (m_ne->insert(loc)) return true;
    if (m_sw->insert(loc)) return true;
    if (m_se->insert(loc)) return true;

    // Fallback (on boundary edge)
    m_points.push_back(std::move(loc));
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Subdivide
// ─────────────────────────────────────────────────────────────────────────────
void QuadTree::subdivide() {
    double hw = m_boundary.halfW / 2.0;
    double hh = m_boundary.halfH / 2.0;
    double cx = m_boundary.cx, cy = m_boundary.cy;

    m_nw = std::make_unique<QuadTree>(BoundingBox{cx-hw, cy+hh, hw, hh}, m_capacity, m_depth+1);
    m_ne = std::make_unique<QuadTree>(BoundingBox{cx+hw, cy+hh, hw, hh}, m_capacity, m_depth+1);
    m_sw = std::make_unique<QuadTree>(BoundingBox{cx-hw, cy-hh, hw, hh}, m_capacity, m_depth+1);
    m_se = std::make_unique<QuadTree>(BoundingBox{cx+hw, cy-hh, hw, hh}, m_capacity, m_depth+1);

    m_divided = true;

    // Re-insert existing points into children
    for (auto& p : m_points) {
        if (!m_nw->insert(p) && !m_ne->insert(p) &&
            !m_sw->insert(p) && !m_se->insert(p)) {
            // Stays at this level (boundary point)
        }
    }
    m_points.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
//  queryRadius
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::shared_ptr<Location>>
QuadTree::queryRadius(double qx, double qy, double r) const {
    std::vector<std::shared_ptr<Location>> result;

    if (!m_boundary.intersectsCircle(qx, qy, r)) return result;

    for (const auto& p : m_points) {
        double dx = p->getX() - qx, dy = p->getY() - qy;
        if (dx*dx + dy*dy <= r*r)
            result.push_back(p);
    }

    if (m_divided) {
        for (auto* child : {m_nw.get(), m_ne.get(), m_sw.get(), m_se.get()}) {
            auto sub = child->queryRadius(qx, qy, r);
            result.insert(result.end(), sub.begin(), sub.end());
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  queryRange
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::shared_ptr<Location>>
QuadTree::queryRange(const BoundingBox& range) const {
    std::vector<std::shared_ptr<Location>> result;
    if (!m_boundary.intersects(range)) return result;

    for (const auto& p : m_points)
        if (range.contains(p->getX(), p->getY()))
            result.push_back(p);

    if (m_divided) {
        for (auto* child : {m_nw.get(), m_ne.get(), m_sw.get(), m_se.get()}) {
            auto sub = child->queryRange(range);
            result.insert(result.end(), sub.begin(), sub.end());
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  nearestNeighbor
// ─────────────────────────────────────────────────────────────────────────────
std::shared_ptr<Location>
QuadTree::nearestNeighbor(double qx, double qy) const {
    double bestDist2 = std::numeric_limits<double>::max();
    std::shared_ptr<Location> bestLoc;
    nearestRec(qx, qy, bestDist2, bestLoc);
    return bestLoc;
}

void QuadTree::nearestRec(double qx, double qy,
                           double& bestDist2,
                           std::shared_ptr<Location>& bestLoc) const
{
    // Prune: closest possible point in this bounding box is farther than best
    double dx = std::max(std::abs(qx - m_boundary.cx) - m_boundary.halfW, 0.0);
    double dy = std::max(std::abs(qy - m_boundary.cy) - m_boundary.halfH, 0.0);
    if (dx*dx + dy*dy >= bestDist2) return;

    for (const auto& p : m_points) {
        double pdx = p->getX()-qx, pdy = p->getY()-qy;
        double d2  = pdx*pdx + pdy*pdy;
        if (d2 < bestDist2) { bestDist2 = d2; bestLoc = p; }
    }

    if (m_divided) {
        for (auto* child : {m_nw.get(), m_ne.get(), m_sw.get(), m_se.get()})
            child->nearestRec(qx, qy, bestDist2, bestLoc);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  kNearest — BFS expanding radius until k results collected
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::shared_ptr<Location>>
QuadTree::kNearest(double qx, double qy, int k) const {
    // Collect all, sort by distance, return top-k
    auto all = queryRange(m_boundary);  // all points
    std::sort(all.begin(), all.end(),
        [qx, qy](const std::shared_ptr<Location>& a,
                 const std::shared_ptr<Location>& b) {
            double da = (a->getX()-qx)*(a->getX()-qx)+(a->getY()-qy)*(a->getY()-qy);
            double db = (b->getX()-qx)*(b->getX()-qx)+(b->getY()-qy)*(b->getY()-qy);
            return da < db;
        });
    if (static_cast<int>(all.size()) > k) all.resize(k);
    return all;
}

// ─────────────────────────────────────────────────────────────────────────────
//  size / print
// ─────────────────────────────────────────────────────────────────────────────
int QuadTree::size() const {
    int total = static_cast<int>(m_points.size());
    if (m_divided)
        for (auto* c : {m_nw.get(), m_ne.get(), m_sw.get(), m_se.get()})
            total += c->size();
    return total;
}

void QuadTree::print(std::ostream& os, int indent) const {
    std::string pad(indent * 2, ' ');
    os << pad << "QuadTree[depth=" << m_depth
       << " points=" << m_points.size()
       << " divided=" << m_divided << "]\n";
    if (m_divided)
        for (auto* c : {m_nw.get(), m_ne.get(), m_sw.get(), m_se.get()})
            c->print(os, indent + 1);
}