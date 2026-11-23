cmake_minimum_required(VERSION 3.20)
project(ospf_router CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Release by default: benchmarks on a Debug (-O0) build run ~7x slower
# and measure the wrong thing.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# CMake 4.x refuses dependencies that declare cmake_minimum_required < 3.5
# (nlohmann/json 3.11.3 declares 3.1). Harmless on CMake 3.x.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)

FetchContent_Declare(httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.15.3)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3)

FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.3)

FetchContent_MakeAvailable(httplib json Catch2)

find_package(Threads REQUIRED)

#Core library: shared by the server, the benchmark, and the tests 
add_library(ospf_core
  src/Graph.cpp
  src/Dijkstra.cpp
  src/LSDB.cpp
  src/SpfEngine.cpp
  src/DefaultTopology.cpp
)
target_include_directories(ospf_core PUBLIC src)

# Server 
add_executable(ospf_router src/main.cpp src/Server.cpp)
target_link_libraries(ospf_router PRIVATE ospf_core httplib::httplib nlohmann_json::nlohmann_json)

#  Experiment harness (no external dependencies beyond threads
add_executable(benchmark src/benchmark.cpp)
target_link_libraries(benchmark PRIVATE ospf_core Threads::Threads)

# Tests 
enable_testing()
add_executable(ospf_tests
  tests/test_graph.cpp
  tests/test_dijkstra.cpp
  tests/test_lsdb.cpp
  tests/test_incremental.cpp
)
target_link_libraries(ospf_tests PRIVATE ospf_core Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(ospf_tests)

# Warnings on our own code only — not on fetched dependencies.
foreach(t ospf_core ospf_router benchmark ospf_tests)
  target_compile_options(${t} PRIVATE -Wall -Wextra)
endforeach()
