#pragma once
// src/io/FileParser.h
// Smart City Delivery & Traffic Management System

#include <string>
#include <vector>
#include <memory>
#include <ctime>
#include "../core/Location.h"
#include "../core/Vehicle.h"
#include "../core/Package.h"
#include "../core/Customer.h"
#include "../graph/RoadNetwork.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: TrafficUpdate
//  Parsed entry from traffic_updates.txt
//  Format: <from_id> <to_id> <new_weight> <timestamp_epoch>
// ─────────────────────────────────────────────────────────────────────────────
struct TrafficUpdate {
    int         fromId;
    int         toId;
    double      newWeight;
    std::time_t timestamp;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Struct: ParseResult
// ─────────────────────────────────────────────────────────────────────────────
struct ParseResult {
    std::vector<std::shared_ptr<Location>> locations;
    std::vector<std::shared_ptr<Vehicle>>  vehicles;
    std::vector<std::shared_ptr<Package>>  packages;
    std::vector<std::shared_ptr<Customer>> customers;
    std::vector<TrafficUpdate>             trafficUpdates;

    // Graph edges stored until RoadNetwork is populated
    struct EdgeData {
        int    from, to, roadId;
        double weight;
    };
    std::vector<EdgeData> edges;

    std::vector<std::string> errors;
    bool hasErrors() const { return !errors.empty(); }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Class: FileParser  (static utility)
//
//  Actual file formats used in this project:
//
//  city_map.txt:
//    <num_nodes> <num_edges>
//    <node_id> <name> <x> <y>          ← NO type column
//    <from_id> <to_id> <weight>         ← NO road_id column
//
//  locations.txt:
//    <id> <name> <x> <y> <type>
//
//  vehicles.txt:
//    <id> <capacity> <speed> <battery> <start_location_id>
//    ← NO name, NO vehicle type
//
//  deliveries.txt:
//    <id> <src_id> <dest_id> <deadline_epoch> <priority_str> <weight_kg>
//    ← NO tracking number, NO customer ID
//    priority_str: Critical | High | Medium | Normal | Low
//
//  traffic_updates.txt:
//    <from_id> <to_id> <new_weight> <timestamp_epoch>
// ─────────────────────────────────────────────────────────────────────────────
class FileParser {
public:
    FileParser() = delete;  // static-only

    // ── Top-level parsers ─────────────────────────────────────────────────────
    static bool parseCityMap(const std::string& filepath,
                             RoadNetwork&        graph,
                             std::vector<std::string>& errors);

    static std::vector<std::shared_ptr<Location>>
    parseLocations(const std::string& filepath,
                   std::vector<std::string>& errors);

    static std::vector<std::shared_ptr<Vehicle>>
    parseVehicles(const std::string& filepath,
                  std::vector<std::string>& errors);

    static std::vector<std::shared_ptr<Package>>
    parseDeliveries(const std::string& filepath,
                    std::vector<std::string>& errors);

    static std::vector<TrafficUpdate>
    parseTrafficUpdates(const std::string& filepath,
                        std::vector<std::string>& errors);

    // Convenience: parse all files at once
    static ParseResult parseAll(const std::string& dataDir);

private:
    static LocationType      stringToLocationType(const std::string& s);
    static DeliveryPriority  stringToPriority(const std::string& s);
    static std::string       trim(const std::string& s);
    static bool              skipLine(const std::string& line);
};