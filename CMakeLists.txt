cmake_minimum_required(VERSION 3.20)
project(ospf_router CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# cpp-httplib — single header HTTP server library
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.15.3
)

# nlohmann/json — single header JSON library
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

# Catch2 — unit testing framework
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.3
)

FetchContent_MakeAvailable(httplib json Catch2)

# ---- Core library (shared between main app, benchmark, and tests) ----
add_library(ospf_core
  src/Graph.cpp
  src/Dijkstra.cpp
  src/LSDB.cpp
)
target_include_directories(ospf_core PUBLIC src)
target_link_libraries(ospf_core PUBLIC nlohmann_json::nlohmann_json)

# ---- Main server executable ----
add_executable(ospf_router
  src/main.cpp
  src/Server.cpp
)
target_link_libraries(ospf_router PRIVATE ospf_core httplib::httplib)

# ---- Benchmark executable ----
add_executable(benchmark
  src/benchmark.cpp
)
target_link_libraries(benchmark PRIVATE ospf_core)

# ---- Tests ----
enable_testing()
add_executable(ospf_tests
  tests/test_graph.cpp
  tests/test_dijkstra.cpp
  tests/test_lsdb.cpp
)
target_link_libraries(ospf_tests PRIVATE ospf_core Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(ospf_tests)