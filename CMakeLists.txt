cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(network-monitor)

# Dependencies
# If dependencies are managed with conan, set the CMake module path (where
# CMake looks for dependencies) to the current build folder.
if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/conaninfo.txt)
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_BINARY_DIR})
endif()

find_package(Boost 1.74 REQUIRED COMPONENTS system)

set(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_executable(network-monitor ${SOURCES})

target_compile_features(network-monitor
    PRIVATE
        cxx_std_17
)
target_link_libraries(network-monitor
    PRIVATE
        Boost::Boost
)