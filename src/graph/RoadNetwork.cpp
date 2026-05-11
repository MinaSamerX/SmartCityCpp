// src/graph/RoadNetwork.cpp
// Smart City Delivery & Traffic Management System

#include "RoadNetwork.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
//  Static sentinel
// ─────────────────────────────────────────────────────────────────────────────
const std::vector<Edge> RoadNetwork::s_emptyEdges{};

// ─────────────────────────────────────────────────────────────────────────────
//  Node Operations
// ─────────────────────────────────────────────────────────────────────────────

bool RoadNetwork::addNode(std::shared_ptr<Location> loc) {
    if (!loc) return false;
    int id = loc->getId();
    if (hasNode(id)) return false;  // duplicate — silent no-op

    m_locations[id] = std::move(loc);
    m_adjList[id];  // default-construct empty edge list
    return true;
}

bool RoadNetwork::addNode(int id, const std::string& name,
                          double x, double y, LocationType type) {
    // Construct Location on the heap and forward to the shared_ptr overload
    auto loc = std::make_shared<Location>(id, name, x, y, type);
    return addNode(std::move(loc));
}

bool RoadNetwork::removeNode(int nodeId) {
    if (!hasNode(nodeId)) return false;

    // Remove outgoing edges
    m_adjList.erase(nodeId);
    m_locations.erase(nodeId);

    // Remove all incoming edges pointing to nodeId
    for (auto& [from, edges] : m_adjList) {
        edges.erase(
            std::remove_if(edges.begin(), edges.end(),
                [nodeId](const Edge& e) { return e.to == nodeId; }),
            edges.end()
        );
    }
    return true;
}

bool RoadNetwork::hasNode(int nodeId) const {
    return m_adjList.count(nodeId) > 0;
}

int RoadNetwork::nodeCount() const {
    return static_cast<int>(m_adjList.size());
}

std::shared_ptr<Location> RoadNetwork::getLocation(int nodeId) const {
    auto it = m_locations.find(nodeId);
    return (it != m_locations.end()) ? it->second : nullptr;
}

std::vector<int> RoadNetwork::getAllNodeIds() const {
    std::vector<int> ids;
    ids.reserve(m_adjList.size());
    for (const auto& [id, _] : m_adjList)
        ids.push_back(id);
    return ids;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Edge Operations
// ─────────────────────────────────────────────────────────────────────────────

bool RoadNetwork::addEdge(int from, int to, double weight, int roadId) {
    if (!hasNode(from) || !hasNode(to)) return false;
    if (from == to)                      return false; // no self-loops
    if (weight < 0.0 && weight != -1.0)  {}            // allow negative (Bellman-Ford)
    if (hasEdge(from, to))               return false; // no duplicate edges

    if (roadId == -1) roadId = m_nextRoadId++;

    m_adjList[from].emplace_back(to, weight, roadId);
    return true;
}

bool RoadNetwork::addBidirectionalEdge(int from, int to,
                                       double weight, int roadId) {
    // Both directions get the same roadId so traffic updates affect both
    if (roadId == -1) roadId = m_nextRoadId++;
    bool a = addEdge(from, to, weight, roadId);
    bool b = addEdge(to, from, weight, roadId);
    return a && b;
}

bool RoadNetwork::removeEdge(int from, int to) {
    if (!hasNode(from)) return false;
    auto& edges = m_adjList[from];
    auto  it    = std::find_if(edges.begin(), edges.end(),
                    [to](const Edge& e) { return e.to == to; });
    if (it == edges.end()) return false;
    edges.erase(it);
    return true;
}

bool RoadNetwork::updateEdgeWeight(int from, int to, double newWeight) {
    Edge* e = findEdge(from, to);
    if (!e) return false;
    e->weight = newWeight;
    return true;
}

bool RoadNetwork::setRoadClosed(int from, int to, bool closed) {
    Edge* e = findEdge(from, to);
    if (!e) return false;
    e->isClosed = closed;
    return true;
}

bool RoadNetwork::isRoadClosed(int from, int to) const {
    const Edge* e = findEdge(from, to);
    return e ? e->isClosed : false;
}

bool RoadNetwork::hasEdge(int from, int to) const {
    return findEdge(from, to) != nullptr;
}

double RoadNetwork::getEdgeWeight(int from, int to) const {
    const Edge* e = findEdge(from, to);
    return e ? e->weight : INF_WEIGHT;
}

int RoadNetwork::edgeCount() const {
    int total = 0;
    for (const auto& [_, edges] : m_adjList)
        total += static_cast<int>(edges.size());
    return total;
}

const std::vector<Edge>& RoadNetwork::getNeighbours(int nodeId) const {
    auto it = m_adjList.find(nodeId);
    return (it != m_adjList.end()) ? it->second : s_emptyEdges;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Utility
// ─────────────────────────────────────────────────────────────────────────────

void RoadNetwork::clear() {
    m_adjList.clear();
    m_locations.clear();
    m_nextRoadId = 1;
}

void RoadNetwork::print(std::ostream& os) const {
    os << "RoadNetwork [nodes=" << nodeCount()
       << " edges=" << edgeCount() << "]\n";

    // Sort node IDs for deterministic output
    auto ids = getAllNodeIds();
    std::sort(ids.begin(), ids.end());

    for (int id : ids) {
        auto loc = getLocation(id);
        os << "  [" << id << "] ";
        if (loc) os << loc->getName();
        os << " -> ";
        for (const auto& e : getNeighbours(id))
            os << e << " ";
        os << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────────────────────────────────────

Edge* RoadNetwork::findEdge(int from, int to) {
    auto it = m_adjList.find(from);
    if (it == m_adjList.end()) return nullptr;
    for (auto& e : it->second)
        if (e.to == to) return &e;
    return nullptr;
}

const Edge* RoadNetwork::findEdge(int from, int to) const {
    auto it = m_adjList.find(from);
    if (it == m_adjList.end()) return nullptr;
    for (const auto& e : it->second)
        if (e.to == to) return &e;
    return nullptr;
}