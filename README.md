# Required dependencies 

* CMake
```
$ sudo snap install cmake
```
* Conan 
```
$ pip install conan
```
* Spdlog
```
$ sudo apt-get install libspdlog-dev
```

# Get started
```
$ git clone --recurse-submodules https://github.com/Agnieszka1994/Live-Transport-Network-Monitor
    && cd ./Live-Transport-Network-Monitor/ \
    && ./bin/00-installDependencies.sh \
    && ./bin/11-build.sh
```


# Live Transport Network Monitor
The London Transport Company wants to optimize the passenger load on their Underground lines at peak times.

They want ot test a new itineary option for users called `quiet route`. The company can show this option to the users of services like Google Maps, wchich connect to the LTC infrastructure to get travel recommendations.

`A quiet route is a route that may not be the fastest but is noticeably less crowded than other options.`

The London Transport Company hopes that passengers will follow these quiet route suggestions and spread out more evenly acress different lines, especially at peak times when the network is under heavier load.

## Quiet route

The `quiet-route` service provides on-demand itinerary recommendations for the quiet route option. This is the main goal of this project. Given a request to travel from A to B, where A and B are Underground stations, the `quiet-route` service provides a single itinerary recommendation that avoids busy lines and stations.

## Project structure
The application contains the following components:

- A `Websocket/STOMP client` that connects to the network-events service. The client supports TLS connections and needs to authenticate with the server.
- `A Websocket/STOMP server` that provides the quiet-route service. The server supports TLS connections but no authentication.
- A `C++ object` representing the `network layout` and the current level of crowding on the Underground network, as informed by the live passenger events. This component also offers APIs to produce `itineraries from A to B`.
A `C++ component that produces quiet route recommendations` based on a request to go from A to B.

## Project Requirements
- The solution supports `up to 300 events per second`.
- The code is thoroughly `unit-tested`. 
- `Integration tests` and `stress tests` implemented.
- Solid `build system` and an `automated deployment pipeline`.

## London Transport Company - Infrastructure overview
The LTC infrastructure uses Websockets extensively to communicate acress services.
They offer a service that sends live network events, called `network-events`.

**Real-time network events**
The `network-event` data feed provides real time network events for the London Underground lines. It provides one type of events: `Passenger events`, which represent passenger action, like `entering` and `exiting` a station.

**Protocol**
The `network-events` data feed follows the `STOMP 1.2 protocol` and is served over secure Websockets.

**Underground network layout**
The London Underground networl layout is described by a static JSON file listing all lines and stations.

// table with documentation 

**Events**

The network-events API is provided as part of this Project to mimick realistic data from a typical business day on the London Underground. In particular:

- The `/passengers` subscription produces messages only from 5am to midnight UTC. The data is produced every day, even on days that are not business days in the "real" world.
- The passenger events follow the crowding pattern of a typical business day on the London Underground, with peak hours between `7am` to `9.30am` UTC and `4.30pm` to `6pm` UTC.



