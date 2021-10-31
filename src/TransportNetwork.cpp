#include "network-monitor/TransportNetwork.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using NetworkMonitor::Id;
using NetworkMonitor::Line;
using NetworkMonitor::PassengerEvent;
using NetworkMonitor::Route;
using NetworkMonitor::Station;
using NetworkMonitor::TransportNetwork;

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

// PassengerEvent — Free functions

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