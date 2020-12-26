cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(LiveTransportNetworkMonitor)

set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_executable(network-monitor ${SOURCES})