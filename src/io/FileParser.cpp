// src/io/FileParser.cpp
// Smart City Delivery & Traffic Management System

#include "FileParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
std::string FileParser::trim(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(),
                    [](unsigned char c){ return std::isspace(c); });
    auto end   = std::find_if_not(s.rbegin(), s.rend(),
                    [](unsigned char c){ return std::isspace(c); }).base();
    return (start < end) ? std::string(start, end) : "";
}

bool FileParser::skipLine(const std::string& line) {
    auto t = trim(line);
    return t.empty() || t[0] == '#';
}

// ── Location type from string ──────────────────────────────────────────────────
// Handles both our enum names AND the types used in the actual data files
LocationType FileParser::stringToLocationType(const std::string& s) {
    if (s == "Warehouse"  || s == "Storage")     return LocationType::Warehouse;
    if (s == "Customer"   || s == "Residential"
                          || s == "Residential_Area")
                                                  return LocationType::Customer;
    if (s == "ChargingStation")                   return LocationType::ChargingStation;
    if (s == "TrafficHub" || s == "Transport"
                          || s == "Commercial"
                          || s == "Medical"
                          || s == "Shopping"
                          || s == "Education"
                          || s == "Industrial"
                          || s == "Logistics")    return LocationType::TrafficHub;
    return LocationType::Intersection;            // default
}

// ── Priority from string ───────────────────────────────────────────────────────
DeliveryPriority FileParser::stringToPriority(const std::string& s) {
    if (s == "Critical")               return DeliveryPriority::Critical;
    if (s == "High"   || s == "high")  return DeliveryPriority::High;
    if (s == "Normal" || s == "Medium" || s == "medium")
                                       return DeliveryPriority::Normal;
    if (s == "Low"    || s == "low")   return DeliveryPriority::Low;
    return DeliveryPriority::Normal;
}

// ─────────────────────────────────────────────────────────────────────────────
//  parseCityMap
//
//  Format:
//    <num_nodes> <num_edges>
//    <node_id> <name> <x> <y>        ← 4 columns (NO type)
//    <from_id> <to_id> <weight>       ← 3 columns (NO road_id)
//
//  Uses RoadNetwork::addNode(int nodeId, const string& name)
//  and separately stores coordinates in a Location registered
//  via addNode(shared_ptr<Location>).
//  If your RoadNetwork::addNode only takes (int, string), we fall back to that.
// ─────────────────────────────────────────────────────────────────────────────
bool FileParser::parseCityMap(const std::string& filepath,
                               RoadNetwork& graph,
                               std::vector<std::string>& errors)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        errors.push_back("Cannot open: " + filepath);
        return false;
    }

    std::string line;
    int numNodes = 0, numEdges = 0;

    // ── Read header ───────────────────────────────────────────────────────────
    while (std::getline(file, line)) {
        if (skipLine(line)) continue;
        std::istringstream ss(line);
        ss >> numNodes >> numEdges;
        break;
    }

    // ── Read nodes ────────────────────────────────────────────────────────────
    for (int i = 0; i < numNodes; ++i) {
        while (std::getline(file, line) && skipLine(line)) {}
        std::istringstream ss(line);
        int id; double x, y; std::string name;
        if (!(ss >> id >> name >> x >> y)) {
            errors.push_back("Bad node line: " + line);
            continue;
        }
        // Use the 5-arg overload from OUR RoadNetwork (id, name, x, y, type)
        // If the project's RoadNetwork only has (id, name), this will fail —
        // in that case replace with: graph.addNode(id, name);
        graph.addNode(id, name, x, y, LocationType::Intersection);
    }

    // ── Read edges ────────────────────────────────────────────────────────────
    int roadId = 1;
    for (int i = 0; i < numEdges; ++i) {
        while (std::getline(file, line) && skipLine(line)) {}
        std::istringstream ss(line);
        int from, to; double weight;
        if (!(ss >> from >> to >> weight)) {
            errors.push_back("Bad edge line: " + line);
            continue;
        }
        graph.addEdge(from, to, weight, roadId++);
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  parseLocations
//  Format: <id> <name> <x> <y> <type>
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::shared_ptr<Location>>
FileParser::parseLocations(const std::string& filepath,
                            std::vector<std::string>& errors)
{
    std::vector<std::shared_ptr<Location>> result;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        errors.push_back("Cannot open: " + filepath);
        return result;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (skipLine(line)) continue;
        std::istringstream ss(line);
        int id; double x, y; std::string name, typeStr;
        if (!(ss >> id >> name >> x >> y >> typeStr)) {
            errors.push_back("Bad location line: " + line);
            continue;
        }
        result.push_back(std::make_shared<Location>(
            id, name, x, y, stringToLocationType(typeStr)));
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  parseVehicles
//  Format: <id> <capacity> <speed> <battery> <start_location_id>
//  NO name, NO type — auto-generate name as "Vehicle-<id>", type = Van
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::shared_ptr<Vehicle>>
FileParser::parseVehicles(const std::string& filepath,
                           std::vector<std::string>& errors)
{
    std::vector<std::shared_ptr<Vehicle>> result;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        errors.push_back("Cannot open: " + filepath);
        return result;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (skipLine(line)) continue;
        std::istringstream ss(line);
        int id, startLoc; double cap, speed, battery;
        if (!(ss >> id >> cap >> speed >> battery >> startLoc)) {
            errors.push_back("Bad vehicle line: " + line);
            continue;
        }
        // Auto-generate name; pick type by capacity
        std::string name = "Vehicle-" + std::to_string(id);
        VehicleType type = (cap >= 80.0) ? VehicleType::Truck
                         : (cap >= 40.0) ? VehicleType::Van
                                         : VehicleType::Bike;
        result.push_back(std::make_shared<Vehicle>(
            id, name, type, cap, speed, battery, startLoc));
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  parseDeliveries
//  Format: <id> <src_id> <dest_id> <deadline_epoch> <priority_str> <weight_kg>
//  NO tracking number → auto-generate "TRK-<id>"
//  NO customer ID     → default to 0
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::shared_ptr<Package>>
FileParser::parseDeliveries(const std::string& filepath,
                             std::vector<std::string>& errors)
{
    std::vector<std::shared_ptr<Package>> result;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        errors.push_back("Cannot open: " + filepath);
        return result;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (skipLine(line)) continue;
        std::istringstream ss(line);
        int id, src, dest; long long deadline;
        double weight; std::string priorityStr;

        if (!(ss >> id >> src >> dest >> deadline >> priorityStr >> weight)) {
            errors.push_back("Bad delivery line: " + line);
            continue;
        }
        std::string tracking = "TRK-" + std::to_string(id);
        result.push_back(std::make_shared<Package>(
            id, tracking, src, dest,
            0,                                          // customerId = 0
            weight,
            stringToPriority(priorityStr),
            static_cast<std::time_t>(deadline)));
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  parseTrafficUpdates
//  Format: <from_id> <to_id> <new_weight> <timestamp_epoch>
// ─────────────────────────────────────────────────────────────────────────────
std::vector<TrafficUpdate>
FileParser::parseTrafficUpdates(const std::string& filepath,
                                 std::vector<std::string>& errors)
{
    std::vector<TrafficUpdate> result;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        errors.push_back("Cannot open: " + filepath);
        return result;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (skipLine(line)) continue;
        std::istringstream ss(line);
        TrafficUpdate upd; long long ts;
        if (!(ss >> upd.fromId >> upd.toId >> upd.newWeight >> ts)) {
            errors.push_back("Bad traffic_update line: " + line);
            continue;
        }
        upd.timestamp = static_cast<std::time_t>(ts);
        result.push_back(upd);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  parseAll — convenience wrapper
// ─────────────────────────────────────────────────────────────────────────────
ParseResult FileParser::parseAll(const std::string& dataDir) {
    ParseResult result;
    std::string sep = (dataDir.empty() || dataDir.back() == '/') ? "" : "/";

    result.locations = parseLocations(
        dataDir + sep + "locations.txt", result.errors);

    // Parse city map into a temporary graph just to extract edge list
    RoadNetwork tmpGraph;
    parseCityMap(dataDir + sep + "city_map.txt", tmpGraph, result.errors);

    // Copy edge data out of tmp graph for caller
    for (int from : tmpGraph.getAllNodeIds()) {
        for (const auto& e : tmpGraph.getNeighbours(from)) {
            ParseResult::EdgeData ed;
            ed.from   = from;
            ed.to     = e.to;
            ed.roadId = e.roadId;
            ed.weight = e.weight;
            result.edges.push_back(ed);
        }
    }

    result.vehicles = parseVehicles(
        dataDir + sep + "vehicles.txt", result.errors);

    result.packages = parseDeliveries(
        dataDir + sep + "deliveries.txt", result.errors);

    result.trafficUpdates = parseTrafficUpdates(
        dataDir + sep + "traffic_updates.txt", result.errors);

    return result;
}