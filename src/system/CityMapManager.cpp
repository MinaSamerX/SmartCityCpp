// src/system/CityMapManager.cpp
// Smart City Delivery & Traffic Management System

#include "CityMapManager.h"
#include <algorithm>
#include "../io/FileParser.h"
#include "../graph/Dijkstra.h"
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor — private (Singleton)
// ─────────────────────────────────────────────────────────────────────────────
CityMapManager::CityMapManager()
    : m_pathFinder(std::make_unique<DijkstraFinder>())  // default strategy
{}

// ─────────────────────────────────────────────────────────────────────────────
//  Initialisation
// ─────────────────────────────────────────────────────────────────────────────
bool CityMapManager::loadFromFile(const std::string& cityMapPath) {
    std::vector<std::string> errors;
    bool ok = FileParser::parseCityMap(cityMapPath, m_graph, errors);
    for (auto& e : errors) std::cerr << "[CityMapManager] " << e << "\n";

    // Scan for negative edges → auto-switch strategy
    for (int id : m_graph.getAllNodeIds())
        for (const auto& edge : m_graph.getNeighbours(id))
            if (edge.weight < 0.0) { m_hasNegativeEdges = true; break; }

    if (m_hasNegativeEdges)
        m_pathFinder = std::make_unique<BellmanFordFinder>();

    return ok;
}

bool CityMapManager::addNode(int id, const std::string& name,
                              double x, double y, LocationType type) {
    return m_graph.addNode(id, name, x, y, type);
}

bool CityMapManager::addEdge(int from, int to, double weight, int roadId) {
    checkAndSwitchStrategy(weight);
    return m_graph.addEdge(from, to, weight, roadId);
}

bool CityMapManager::addBidirectionalEdge(int from, int to,
                                           double weight, int roadId) {
    checkAndSwitchStrategy(weight);
    return m_graph.addBidirectionalEdge(from, to, weight, roadId);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Path queries  — delegated to current IPathFinder strategy
// ─────────────────────────────────────────────────────────────────────────────
PathResult CityMapManager::shortestPath(int srcId, int destId) const {
    if (!m_pathFinder) return {};
    return m_pathFinder->findPath(m_graph, srcId, destId);
}

PathResult CityMapManager::multiStopPath(const std::vector<int>& stops) const {
    if (!m_pathFinder) return {};
    return m_pathFinder->findMultiStopPath(m_graph, stops);
}

std::unordered_map<int, double>
CityMapManager::allDistancesFrom(int srcId) const {
    return Dijkstra::allShortestPaths(m_graph, srcId);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Traffic management
// ─────────────────────────────────────────────────────────────────────────────
bool CityMapManager::updateTraffic(int fromId, int toId, double newWeight) {
    bool ok = m_graph.updateEdgeWeight(fromId, toId, newWeight);
    if (ok) checkAndSwitchStrategy(newWeight);
    return ok;
}

void CityMapManager::applyTrafficUpdates(const std::vector<TrafficUpdate>& updates) {
    for (const auto& upd : updates) {
        if (!m_graph.updateEdgeWeight(upd.fromId, upd.toId, upd.newWeight)) {
            std::cerr << "[CityMapManager] TrafficUpdate: edge "
                      << upd.fromId << "->" << upd.toId << " not found\n";
        } else {
            checkAndSwitchStrategy(upd.newWeight);
        }
    }
}

bool CityMapManager::closeRoad(int fromId, int toId) {
    return m_graph.setRoadClosed(fromId, toId, true);
}

bool CityMapManager::openRoad(int fromId, int toId) {
    return m_graph.setRoadClosed(fromId, toId, false);
}

std::vector<int> CityMapManager::getAffectedNodes(int closedFrom,
                                                    int closedTo) const {
    return GraphSearch::findAffectedNodes(m_graph, closedFrom, closedTo);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Connectivity
// ─────────────────────────────────────────────────────────────────────────────
bool CityMapManager::isReachable(int srcId, int destId) const {
    return GraphSearch::isReachable(m_graph, srcId, destId);
}

std::vector<std::vector<int>> CityMapManager::connectedComponents() const {
    return GraphSearch::allConnectedComponents(m_graph);
}

bool CityMapManager::isFullyConnected() const {
    auto comps = connectedComponents();
    return comps.size() == 1;
}

// ─────────────────────────────────────────────────────────────────────────────
//  MST
// ─────────────────────────────────────────────────────────────────────────────
MST::MSTResult CityMapManager::buildMST(bool usePrim) const {
    // Use first node from sorted list as Prim start (not front() of unordered_map)
    auto ids = m_graph.getAllNodeIds();
    int startNode = ids.empty() ? -1 : *std::min_element(ids.begin(), ids.end());
    return usePrim ? MST::prim(m_graph, startNode) : MST::kruskal(m_graph);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Topological ordering
// ─────────────────────────────────────────────────────────────────────────────
TopoSort::TopoResult CityMapManager::topologicalOrder() const {
    return TopoSort::topologicalSort(m_graph);
}

std::vector<int> CityMapManager::orderedCheckpoints(
    const std::vector<int>& stops) const {
    return TopoSort::getDeliveryOrder(m_graph, stops);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostics
// ─────────────────────────────────────────────────────────────────────────────
void CityMapManager::reset() {
    m_graph.clear();
    m_hasNegativeEdges = false;
    m_pathFinder = std::make_unique<DijkstraFinder>();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Private helpers
// ─────────────────────────────────────────────────────────────────────────────
void CityMapManager::checkAndSwitchStrategy(double weight) {
    if (!m_hasNegativeEdges && weight < 0.0) {
        m_hasNegativeEdges = true;
        m_pathFinder = std::make_unique<BellmanFordFinder>();
        std::cout << "[CityMapManager] Negative edge detected — "
                     "switched to BellmanFord strategy.\n";
    }
}