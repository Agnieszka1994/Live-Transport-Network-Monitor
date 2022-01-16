#include "network-monitor/TransportNetwork.h"

#include <nlohmann/json.hpp>

#include <boost/container_hash/hash.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <queue>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <vector>

using NetworkMonitor::Id;
using NetworkMonitor::Line;
using NetworkMonitor::PassengerEvent;
using NetworkMonitor::Route;
using NetworkMonitor::Station;
using NetworkMonitor::TransportNetwork;
using NetworkMonitor::TravelRoute;

// Station — Public methods

bool Station::operator==(const Station& other) const
{
    return id == other.id;
}

// Route — Public methods

bool Route::operator==(const Route& other) const
{
    return id == other.id;
}

// Line — Public methods

bool Line::operator==(const Line& other) const
{
    return id == other.id;
}

// TravelRoute — Public methods

bool TravelRoute::Step::operator==(const TravelRoute::Step& other) const
{
    return travelTime == other.travelTime &&
        startStationId == other.startStationId &&
        endStationId == other.endStationId &&
        lineId == other.lineId &&
        routeId == other.routeId;
}

bool TravelRoute::operator==(const TravelRoute& other) const
{
    return totalTravelTime == other.totalTravelTime &&
        startStationId == other.startStationId &&
        endStationId == other.endStationId &&
        steps == other.steps;
}

// Free functions

void NetworkMonitor::from_json(
    const nlohmann::json& src,
    PassengerEvent& dst
)
{
    dst.stationId = src.at("station_id").get<std::string>();
    dst.type = src.at("passenger_event").get<std::string>() == "in" ?
        PassengerEvent::Type::In : PassengerEvent::Type::Out;

    // We exclude the final 'Z' when parsing the datetime string.
    auto datetimeZ {src.at("datetime").get<std::string>()};
    auto datetime {datetimeZ.substr(0, datetimeZ.size() - 1)};
    dst.timestamp = boost::posix_time::from_iso_extended_string(datetime);
}

std::ostream& NetworkMonitor::operator<<(
    std::ostream& os,
    const TravelRoute& r
)
{
    os << nlohmann::json(r);
    return os;
}

void NetworkMonitor::to_json(
    nlohmann::json& dst,
    const TravelRoute::Step& src
)
{
    dst["start_station_id"] = src.startStationId;
    dst["end_station_id"] = src.endStationId;
    dst["line_id"] = src.lineId;
    dst["route_id"] = src.routeId;
    dst["travel_time"] = src.travelTime;
}

void NetworkMonitor::from_json(
    const nlohmann::json& src,
    TravelRoute::Step& dst
)
{
    dst.startStationId = src.at("start_station_id").get<Id>();
    dst.endStationId = src.at("end_station_id").get<Id>();
    dst.lineId= src.at("line_id").get<Id>();
    dst.routeId = src.at("route_id").get<Id>();
    dst.travelTime = src.at("travel_time").get<unsigned int>();
}

void NetworkMonitor::to_json(
    nlohmann::json& dst,
    const TravelRoute& src
)
{
    dst["start_station_id"] = src.startStationId;
    dst["end_station_id"] = src.endStationId;
    dst["total_travel_time"] = src.totalTravelTime;
    dst["steps"] = src.steps;
}

void NetworkMonitor::from_json(
    const nlohmann::json& src,
    TravelRoute& dst
)
{
    dst.startStationId = src.at("start_station_id").get<Id>();
    dst.endStationId = src.at("end_station_id").get<Id>();
    dst.totalTravelTime = src.at("total_travel_time").get<unsigned int>();
    dst.steps = src.at("steps").get<std::vector<TravelRoute::Step>>();
}

// TransportNetwork — Public methods

TransportNetwork::TransportNetwork() = default;

TransportNetwork::~TransportNetwork() = default;

TransportNetwork::TransportNetwork(
    const TransportNetwork& copied
) = default;

TransportNetwork::TransportNetwork(
    TransportNetwork&& moved
) = default;

TransportNetwork& TransportNetwork::operator=(
    const TransportNetwork& copied
) = default;

TransportNetwork& TransportNetwork::operator=(
    TransportNetwork&& moved
) = default;

bool TransportNetwork::FromJson(
    nlohmann::json&& src
)
{
    bool ok {true};

    // First, add all the stations.
    for (auto&& stationJson: src.at("stations")) {
        Station station {
            std::move(stationJson.at("station_id").get<std::string>()),
            std::move(stationJson.at("name").get<std::string>()),
        };
        ok &= AddStation(station);
        if (!ok) {
            throw std::runtime_error("Could not add station " + station.id);
        }
    }

    // Then, add the lines.
    for (auto&& lineJson: src.at("lines")) {
        Line line {
            std::move(lineJson.at("line_id").get<std::string>()),
            std::move(lineJson.at("name").get<std::string>()),
            {}, // We will add the routes shortly.
        };
        line.routes.reserve(lineJson.at("routes").size());
        for (auto&& routeJson: lineJson.at("routes")) {
            line.routes.emplace_back(Route {
                std::move(routeJson.at("route_id").get<std::string>()),
                std::move(routeJson.at("direction").get<std::string>()),
                std::move(routeJson.at("line_id").get<std::string>()),
                std::move(routeJson.at("start_station_id").get<std::string>()),
                std::move(routeJson.at("end_station_id").get<std::string>()),
                std::move(
                    routeJson.at("route_stops").get<std::vector<std::string>>()
                ),
            });
        }
        ok &= AddLine(line);
        if (!ok) {
            throw std::runtime_error("Could not add line " + line.id);
        }
    }

    // Finally, set the travel times.
    for (auto&& travelTimeJson: src.at("travel_times")) {
        ok &= SetTravelTime(
            std::move(travelTimeJson.at("start_station_id").get<std::string>()),
            std::move(travelTimeJson.at("end_station_id").get<std::string>()),
            std::move(travelTimeJson.at("travel_time").get<unsigned int>())
        );
    }

    return ok;
}

bool TransportNetwork::AddStation(
    const Station& station
)
{
    // Cannot add a station that is already in the network.
    if (GetStation(station.id) != nullptr) {
        return false;
    }

    // Create a new station node and add it to the map.
    auto node {std::make_shared<GraphNode>(GraphNode {
        station.id,
        station.name,
        0, // We start with no passengers.
        {} // We start with no edges.
    })};
    stations_.emplace(station.id, std::move(node));

    return true;
}

bool TransportNetwork::AddLine(
    const Line& line
)
{
    // Cannot add a line that is already in the network.
    if (GetLine(line.id) != nullptr) {
        return false;
    }

    // Create the internal version of the line.
    auto lineInternal {std::make_shared<LineInternal>(LineInternal {
        line.id,
        line.name,
        {} // We will add routes shortly.
    })};

    // Add the routes to the line.
    for (const auto& route: line.routes) {
        bool ok {AddRouteToLine(route, lineInternal)};
        if (!ok) {
            return false;
        }
    }

    // Only add the line to the map when we are sure that there were no errors.
    lines_.emplace(line.id, std::move(lineInternal));

    return true;
}

bool TransportNetwork::RecordPassengerEvent(
    const PassengerEvent& event
)
{
    // Find the station.
    const auto stationNode {GetStation(event.stationId)};
    if (stationNode == nullptr) {
        return false;
    }

    // Increase or decrease the passenger count at the station.
    switch (event.type) {
        case PassengerEvent::Type::In:
            ++stationNode->passengerCount;
            return true;
        case PassengerEvent::Type::Out:
            --stationNode->passengerCount;
            return true;
        default:
            return false;
    }
}

long long int TransportNetwork::GetPassengerCount(
    const Id& station
) const
{
    // Find the station.
    const auto stationNode {GetStation(station)};
    if (stationNode == nullptr) {
        throw std::runtime_error("Could not find station in the network: " +
                                 station);
    }
    return stationNode->passengerCount;
}

std::vector<Id> TransportNetwork::GetRoutesServingStation(
    const Id& station
) const
{
    // Find the station.
    const auto stationNode {GetStation(station)};
    if (stationNode == nullptr) {
        return {};
    }

    std::vector<Id> routes {};

    // Iterate over all edges departing from the node. Each edge corresponds to
    // one route serving the station.
    const auto& edges {stationNode->edges};
    for (const auto& edge: edges) {
        routes.push_back(edge->route->id);
    }

    // The previous loop misses a corner case: The end station of a route does
    // not have any edge containing that route, because we only track the routes
    // that *leave from*, not *arrive to* a certain station.
    // We need to loop over all line routes to check if our station is the end
    // stop of any route.
    // FIXME: In the worst case, we are iterating over all routes for all
    //        lines in the network. We may want to optimize this.
    for (const auto& [_, line]: lines_) {
        for (const auto& [_, route]: line->routes) {
            const auto& endStop {route->stops[route->stops.size() - 1]};
            if (stationNode == endStop) {
                routes.push_back(route->id);
            }
        }
    }

    return routes;
}

bool TransportNetwork::SetTravelTime(
    const Id& stationA,
    const Id& stationB,
    const unsigned int travelTime
)
{
    // Find the stations.
    const auto stationANode {GetStation(stationA)};
    const auto stationBNode {GetStation(stationB)};
    if (stationANode == nullptr || stationBNode == nullptr) {
        return false;
    }

    // Search all edges connecting A -> B and B -> A.
    // We use a lambda to avoid code duplication.
    bool foundAnyEdge {false};
    auto setTravelTime {[&foundAnyEdge, &travelTime](auto from, auto to) {
        for (auto& edge: from->edges) {
            if (edge->nextStop == to) {
                edge->travelTime = travelTime;
                foundAnyEdge = true;
            }
        }
    }};
    setTravelTime(stationANode, stationBNode);
    setTravelTime(stationBNode, stationANode);

    return foundAnyEdge;
}

unsigned int TransportNetwork::GetTravelTime(
    const Id& stationA,
    const Id& stationB
) const
{
    // Find the stations.
    const auto stationANode {GetStation(stationA)};
    const auto stationBNode {GetStation(stationB)};
    if (stationANode == nullptr || stationBNode == nullptr) {
        return 0;
    }

    // Check if there is an edge A -> B, then B -> A.
    // We can return early as soon as we find a match: Wwe know that the travel
    // time from A to B is the same as the travel time from B to A, across all
    // routes.
    for (const auto& edge: stationANode->edges) {
        if (edge->nextStop == stationBNode) {
            return edge->travelTime;
        }
    }
    for (const auto& edge: stationBNode->edges) {
        if (edge->nextStop == stationANode) {
            return edge->travelTime;
        }
    }
    return 0;
}

unsigned int TransportNetwork::GetTravelTime(
    const Id& line,
    const Id& route,
    const Id& stationA,
    const Id& stationB
) const
{
    // Find the route.
    const auto routeInternal {GetRoute(line, route)};
    if (routeInternal == nullptr) {
        return 0;
    }

    // Find the stations.
    const auto stationANode {GetStation(stationA)};
    const auto stationBNode {GetStation(stationB)};
    if (stationANode == nullptr || stationBNode == nullptr) {
        return 0;
    }

    // Walk the route looking for station A.
    unsigned int travelTime {0};
    bool foundA {false};
    for (const auto& stop: routeInternal->stops) {
        // If we found station A, we start counting.
        if (stop == stationANode) {
            foundA = true;
        }

        // If we found station B, we return the cumulative travel time so far.
        if (stop == stationBNode) {
            return travelTime;
        }

        // Accummulate the travel time since we found station A..
        if (foundA) {
            auto edgeIt {stop->FindEdgeForRoute(routeInternal)};
            if (edgeIt == stop->edges.end()) {
                // Unexpected: The station should definitely have an edge for
                //             this route.
                return 0;
            }
            travelTime += (*edgeIt)->travelTime;
        }
    }

    // If we got here, we didn't find station A, B, or both.
    return 0;
}

TravelRoute TransportNetwork::GetFastestTravelRoute(
    const Id& stationAId,
    const Id& stationBId
) const
{
    // Find the stations.
    const auto stationA {GetStation(stationAId)};
    const auto stationB {GetStation(stationBId)};
    if (stationA == nullptr || stationB == nullptr) {
        return TravelRoute {};
    }
    spdlog::info("GetFastestTravelRoute: {} -> {}", stationA->id, stationB->id);

    // Corner case: A and B are the same station.
    if (stationA == stationB) {
        return TravelRoute {
            stationAId,
            stationAId,
            0,
            {TravelRoute::Step {
                stationAId,
                stationAId,
                {},
                {},
                0
            }},
        };
    }

    // Get the fastest path from A to B.
    const auto path {GetFastestTravelRoute(
        {{stationA, nullptr}, 0},
        stationB
    )};

    // Corner case: There is no valid path between A and B.
    if (path.empty()) {
        return TravelRoute {
            stationAId,
            stationBId,
            0,
            {},
        };
    }

    // Assemble the path.
    // Note: We go in reverse order, from B to A, because this is how the
    //       previousStop map is structured.
    const auto& totalTravelTime {path.back().second};
    TravelRoute travelRoute {
        stationAId,
        stationBId,
        totalTravelTime,
        {},
    };
    travelRoute.steps.reserve(path.size());
    for (size_t idx {1}; idx < path.size(); ++idx) {
        const auto& prevStop {path[idx - 1].first};
        const auto& currStop {path[idx].first};
        travelRoute.steps.push_back(TravelRoute::Step {
            prevStop.node->id,
            currStop.node->id,
            currStop.edge->route->line->id,
            currStop.edge->route->id,
            currStop.edge->travelTime,
        });
    }
    return travelRoute;
}

TravelRoute TransportNetwork::GetQuietTravelRoute(
    const Id& stationAId,
    const Id& stationBId,
    const double maxSlowdownPc,
    const double minQuietnessPc,
    const size_t maxNPaths
) const
{
    // Find the stations.
    const auto stationA {GetStation(stationAId)};
    const auto stationB {GetStation(stationBId)};
    if (stationA == nullptr || stationB == nullptr) {
        return TravelRoute {};
    }
    spdlog::info("GetQuietTravelRoute: {} -> {}", stationA->id, stationB->id);

    // Corner case: A and B are the same station.
    if (stationA == stationB) {
        return TravelRoute {
            stationAId,
            stationAId,
            0,
            {TravelRoute::Step {
                stationAId,
                stationAId,
                {},
                {},
                0
            }},
        };
    }

    // Get all the paths within a certain travel time threshold.
    // These are all valid candidates for the most quiet route.
    auto paths {GetFastestTravelRoutes(
        stationA,
        stationB,
        maxSlowdownPc,
        maxNPaths
    )};

    // Corner case: There is no valid path between A and B.
    if (paths.empty()) {
        return TravelRoute {
            stationAId,
            stationBId,
            0,
            {},
        };
    }

    // Select the most quiet route among the fastest paths.
    // By definition, we only selected paths that have an acceptable travel
    // time, so here we can simply select the path with the lowest passenger
    // count. If the path is not quiet "enough", we just go with the fastest
    // route.
    spdlog::info("Found {} paths", paths.size());
    auto& mostQuietPath {paths.front()}; // Fastest path
    unsigned int minCrowding {GetPathCrowding(paths.front())};
    spdlog::info("Fastest path: {} travel time, {} crowding",
                 mostQuietPath.back().second, minCrowding);
    auto maxCrowding {static_cast<unsigned int>(
        minCrowding * (1 - minQuietnessPc)
    )};
    for (size_t idx {1}; idx < paths.size(); ++idx) {
        auto crowding {GetPathCrowding(paths[idx])};
        if (crowding > maxCrowding) {
            continue;
        }
        if (crowding < minCrowding) {
            minCrowding = crowding;
            mostQuietPath = paths[idx];
        }
    }
    spdlog::info("Most quiet path: {} travel time, {} crowding",
                 mostQuietPath.back().second, minCrowding);

    // Assemble the path.
    // Note: We go in reverse order, from B to A, because this is how the
    //       previousStop map is structured.
    const auto& totalTravelTime {mostQuietPath.back().second};
    TravelRoute travelRoute {
        stationAId,
        stationBId,
        totalTravelTime,
        {},
    };
    travelRoute.steps.reserve(mostQuietPath.size());
    for (size_t idx {1}; idx < mostQuietPath.size(); ++idx) {
        const auto& prevStop {mostQuietPath[idx - 1].first};
        const auto& currStop {mostQuietPath[idx].first};
        travelRoute.steps.push_back(TravelRoute::Step {
            prevStop.node->id,
            currStop.node->id,
            currStop.edge->route->line->id,
            currStop.edge->route->id,
            currStop.edge->travelTime,
        });
    }
    return travelRoute;
}

// TransportNetwork — Private methods

std::vector<
    std::shared_ptr<TransportNetwork::GraphEdge>
>::const_iterator TransportNetwork::GraphNode::FindEdgeForRoute(
    const std::shared_ptr<RouteInternal>& route
) const
{
    return std::find_if(
        edges.begin(),
        edges.end(),
        [&route](const auto& edge) {
            return edge->route == route;
        }
    );
}

bool TransportNetwork::PathStop::operator==(
    const TransportNetwork::PathStop& other
) const
{
    return node == other.node && edge == other.edge;
}

size_t TransportNetwork::PathStopHash::operator()(
    const TransportNetwork::PathStop& stop
) const
{
    size_t seed {0};
    boost::hash_combine(seed, stop.node ? stop.node->id : "");
    boost::hash_combine(seed, stop.edge ? stop.edge->route->id : "");
    return seed;
}

bool TransportNetwork::PathStopDistCmp::operator()(
    const TransportNetwork::PathStopDist& a,
    const TransportNetwork::PathStopDist& b
) const
{
    return a.second > b.second;
}

bool TransportNetwork::PathCmp::operator()(
    const TransportNetwork::Path& a,
    const TransportNetwork::Path& b
) const
{
    return a.back().second > b.back().second;
}

std::shared_ptr<TransportNetwork::GraphNode> TransportNetwork::GetStation(
    const Id& stationId
) const
{
    auto stationIt {stations_.find(stationId)};
    if (stationIt == stations_.end()) {
        return nullptr;
    }
    return stationIt->second;
}

std::shared_ptr<TransportNetwork::LineInternal> TransportNetwork::GetLine(
    const Id& lineId
) const
{
    auto lineIt {lines_.find(lineId)};
    if (lineIt == lines_.end()) {
        return nullptr;
    }
    return lineIt->second;
}

std::shared_ptr<TransportNetwork::RouteInternal> TransportNetwork::GetRoute(
    const Id& lineId,
    const Id& routeId
) const
{
    auto line {GetLine(lineId)};
    if (line == nullptr) {
        return nullptr;
    }
    const auto& routes {line->routes};
    auto routeIt {routes.find(routeId)};
    if (routeIt == routes.end()) {
        return nullptr;
    }
    return routeIt->second;
}

bool TransportNetwork::AddRouteToLine(
    const Route& route,
    const std::shared_ptr<LineInternal>& lineInternal
)
{
    // Cannot add a line route that is already in the network.
    if (lineInternal->routes.find(route.id) != lineInternal->routes.end()) {
        return false;
    }

    // We first gather the list of stations.
    // All stations must already be in the network.
    std::vector<std::shared_ptr<GraphNode>> stops {};
    stops.reserve(route.stops.size());
    for (const auto& stopId: route.stops) {
        const auto station {GetStation(stopId)};
        if (station == nullptr) {
            return false;
        }
        stops.push_back(station);
    }

    // Create the route.
    auto routeInternal {std::make_shared<RouteInternal>(RouteInternal {
        route.id,
        lineInternal,
        std::move(stops)
    })};

    // Walk the station nodes to add an edge for the route.
    for (size_t idx {0}; idx < routeInternal->stops.size() - 1; ++idx) {
        const auto& thisStop {routeInternal->stops[idx]};
        const auto& nextStop {routeInternal->stops[idx + 1]};
        thisStop->edges.emplace_back(std::make_shared<GraphEdge>(GraphEdge {
            routeInternal,
            nextStop,
            0,
        }));
    }

    // Finally, add the route to the line.
    lineInternal->routes[route.id] = std::move(routeInternal);

    return true;
}

TransportNetwork::Path TransportNetwork::GetFastestTravelRoute(
    const TransportNetwork::PathStopDist& stopA,
    const std::shared_ptr<TransportNetwork::GraphNode>& stationB,
    const std::unordered_set<
        TransportNetwork::PathStop, TransportNetwork::PathStopHash
    >& excludedStops
) const
{
    const auto& stationA {stopA.first.node};

    // Corner case: A and B are the same station.
    if (stationA == stationB) {
        return {{{stationA, nullptr}, 0}};
    }

    // Supporting data structures for Dijkstra's algorithm.
    // - Distance of any station from A, through a specific route.
    std::unordered_map<PathStop, unsigned int, PathStopHash> distFromA {};
    distFromA[stopA.first] = stopA.second;
    // - The previous stop in the shortest path.
    std::unordered_map<PathStop, PathStop, PathStopHash> previousStop {};
    // - The priority queue of stops to visit.
    std::priority_queue<
        PathStopDist,
        std::vector<PathStopDist>,
        PathStopDistCmp
    > nodesToVisit;
    nodesToVisit.push(stopA);

    // Dijkstra's algorithm
    while (!nodesToVisit.empty()) {
        // Remove the node from the priority queue.
        auto [currStop, currentDistFromA] = nodesToVisit.top();
        const auto& currStation {currStop.node};
        const auto& edgeToCurrStation {currStop.edge};
        nodesToVisit.pop();

        // Check if we found station B.
        if (currStation == stationB) {
            // We do not want to break here! We may still have some nodes in
            // the queue that may lead to a better path to station B.
            continue;
        }

        // Explore the neighborhood.
        for (const auto& neighborEdge: currStation->edges) {
            PathStop neighbor {neighborEdge->nextStop, neighborEdge};
            if (excludedStops.find(neighbor) != excludedStops.end()) {
                continue;
            }

            // Calculate the distance of the neighbor from station A.
            auto neighborDistFromA {currentDistFromA + neighborEdge->travelTime};
            if (edgeToCurrStation != nullptr &&
                edgeToCurrStation->route != neighborEdge->route
            ) {
                // We add a penalty of 5 minutes if we need to change route to
                // get to our neighbor.
                neighborDistFromA += 5;
            }

            // Update our records of the fastest way to get to the neighbor.
            const auto neighborDistFromAIt {distFromA.find(neighbor)};
            if (neighborDistFromAIt == distFromA.end()) {
                // First time we see this neighbor.
                distFromA[neighbor] = neighborDistFromA;
                previousStop[neighbor] = currStop;
                nodesToVisit.push({neighbor, neighborDistFromA});
            } else {
                // We already saw this neighbor, and only update our records if
                // it's worth it.
                if (neighborDistFromA < neighborDistFromAIt->second) {
                    distFromA[neighbor] = neighborDistFromA;
                    previousStop[neighbor] = currStop;

                    // Note: Because there may have been a change of routes in
                    //       the path to this neighbor, we need to re-walk the
                    //       path from here onwards.
                    nodesToVisit.push({neighbor, neighborDistFromA});
                }
            }
        }
    }

    // Valid paths to station B.
    std::vector<PathStopDist> pathsToB {};
    for (const auto& [pathStop, distance]: distFromA) {
        if (pathStop.node == stationB) {
            pathsToB.push_back({pathStop, distance});
        }
    }

    // Check if we found no valid path between A and B.
    if (pathsToB.empty()) {
        return {};
    }

    // Sort the pathsToB vector to get the fastest path from A to B.
    std::sort(pathsToB.begin(), pathsToB.end(), PathStopDistCmp {});
    auto& fastestPathToB {pathsToB.back()};

    // Assemble the path.
    // Note: We go in reverse order, from B to A, because this is how the
    //       previousStop map is structured.
    Path path {fastestPathToB};
    auto& stop {fastestPathToB.first};
    while (stop.node != stationA) {
        stop = previousStop.at(stop);
        const auto& distance {distFromA.at(stop)};
        path.push_back({stop, distance});
    }
    std::reverse(path.begin(), path.end());

    return path;
}

std::vector<TransportNetwork::Path> TransportNetwork::GetFastestTravelRoutes(
    const std::shared_ptr<TransportNetwork::GraphNode>& stationA,
    const std::shared_ptr<TransportNetwork::GraphNode>& stationB,
    const double maxSlowdownPc,
    const size_t maxNPaths
) const
{
    // Start by finding the fastest path in the network.
    const auto fastestPath {GetFastestTravelRoute(
        {{stationA, nullptr}, 0},
        stationB
    )};
    if (fastestPath.empty()) {
        return {};
    }
    const auto& minTravelTime {fastestPath.back().second};

    // Supporting data structures for Yen's algorithm
    // - List of fastest paths
    std::vector<Path> fastestPaths {fastestPath};
    // - Set of potential k-th shortest paths
    //   We use a priority queue because at the k-th iteration we want to
    //   extract the k-th fastest path among all options found so far.
    std::priority_queue<Path, std::vector<Path>, PathCmp> potentialPaths;

    // Differently from Yen's algorithm, we do not calculate a fixed number of
    // paths (k). Instead, we calculate all paths within a certain travel time.
    // To avoid an excessive amount of calculations, we also limit the total
    // number of paths we find.
    const auto maxTravelTime {static_cast<unsigned int>(
        minTravelTime * (1 + maxSlowdownPc)
    )};
    while (fastestPaths.size() < maxNPaths) {
        const auto& lastFastestPath {fastestPaths.back()};

        // Find all potential paths for the k-th fastest path.
        for (size_t idx {0}; idx < lastFastestPath.size() - 1; ++idx) {
            const auto& rootPathStart {lastFastestPath.begin()};
            const auto& rootPathEnd {lastFastestPath.begin() + idx};
            const auto& spurNode {lastFastestPath[idx]};

            // Remove the links shared between this path and the previous one.
            std::unordered_set<PathStop, PathStopHash> removedStops;
            for (const auto& path: fastestPaths) {
                if (idx < path.size() - 1 &&
                    std::equal(
                        path.begin(), path.begin() + idx,
                        rootPathStart, rootPathEnd
                    )) {
                    removedStops.insert(path[idx + 1].first);
                }
            }

            // Find the shortest path from the spur stop to station B.
            const auto spurPath {GetFastestTravelRoute(
                spurNode,
                stationB,
                removedStops
            )};

            // Assemble the new potential path.
            // newPath = rootPath + spurPath;
            if (!spurPath.empty()) {
                Path newPath {};
                newPath.reserve(idx + 1 + spurPath.size());
                newPath.insert(newPath.end(),
                               rootPathStart, rootPathEnd);
                newPath.insert(newPath.end(),
                               spurPath.begin(), spurPath.end());
                potentialPaths.emplace(std::move(newPath));
            }
        }

        // Select the k-th fastest path from the queue.
        // The priority queue is sorted so that we always process the fastest
        // paths first. We may already have found some of these paths, though.
        bool kthPathFound {false};
        while (potentialPaths.size() > 0) {
            auto kthPath {potentialPaths.top()};
            potentialPaths.pop();
            if (kthPath.back().second > maxTravelTime) {
                // Since the queue is sorted, if we got here it means there is
                // nothing else left to explore that would meet our travel time
                // requirements.
                break;
            }
            if (std::find(fastestPaths.begin(), fastestPaths.end(), kthPath)
                == fastestPaths.end()) {
                // We have found the kth fastest path.
                fastestPaths.emplace_back(std::move(kthPath));
                kthPathFound = true;
                break;
            }
        }
        if (!kthPathFound) {
            break;
        }
    }

    return fastestPaths;
}

unsigned int TransportNetwork::GetPathCrowding(
    const Path& path
) const
{
    unsigned int totPassengerCount {0};
    for (const auto& [stop, _]: path) {
        totPassengerCount += stop.node->passengerCount;
    }
    return totPassengerCount;
}