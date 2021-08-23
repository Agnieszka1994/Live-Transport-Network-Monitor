cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(network-monitor)

# Dependencies
# If dependencies are managed with conan, we set the CMake module path (where
# CMake looks for dependencies) to the current build folder.
if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/conaninfo.txt)
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_BINARY_DIR})
endif()
find_package(Boost 1.74 REQUIRED COMPONENTS system unit_test_framework)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)      

enable_testing()

# Static library
set(LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/FileDownloader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TransportNetwork.cpp"
)

add_library(network-monitor STATIC ${LIB_SOURCES})

target_compile_features(network-monitor
    PUBLIC
        cxx_std_17
)
target_include_directories(network-monitor
    PUBLIC
        inc
)
target_link_libraries(network-monitor
    PUBLIC
        Boost::Boost
        nlohmann_json::nlohmann_json
        OpenSSL::OpenSSL
    PRIVATE
        CURL::CURL
)

# Tests
set(TESTS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/FileDownloader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/WebsocketClient.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/TransportNetwork.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp"
)
add_executable(network-monitor-tests ${TESTS_SOURCES})
target_compile_features(network-monitor-tests
    PRIVATE
        cxx_std_14
)
target_compile_definitions(network-monitor-tests
    PRIVATE 
        TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/test-data"
        TESTS_CACERT_PEM="${CMAKE_CURRENT_SOURCE_DIR}/tests/cacert.pem"
        TESTS_NETWORK_LAYOUT_JSON="${CMAKE_CURRENT_SOURCE_DIR}/tests/test-data/network-layout.json"
)
target_link_libraries(network-monitor-tests
    PRIVATE
        network-monitor
        Boost::Boost
        OpenSSL::OpenSSL
)

add_test(
    NAME network-monitor-tests
    COMMAND $<TARGET_FILE:network-monitor-tests>
)

# This tells CMake to check for a specific output to verify the test outcome.
# When all unit tests pass, Boost.Test prints "No errors detected".
set_tests_properties(network-monitor-tests PROPERTIES
    PASS_REGULAR_EXPRESSION ".*No errors detected"
)