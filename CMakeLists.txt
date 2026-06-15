cmake_minimum_required(VERSION 3.20)
project(SOMNI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SOMNI_BUILD_BINDINGS "Build Python pybind11 bindings" ON)
option(SOMNI_BUILD_TESTS    "Build Catch2 test suite"        ON)

# ---------------------------------------------------------------------------
# Dependencies — pulled at configure time via FetchContent
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# EnTT — entity component system (header-only)
FetchContent_Declare(entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.0
    GIT_SHALLOW    TRUE)

# BehaviorTree.CPP v4 — behavior tree engine
FetchContent_Declare(behaviortree_cpp
    GIT_REPOSITORY https://github.com/BehaviorTree/BehaviorTree.CPP.git
    GIT_TAG        4.6.2
    GIT_SHALLOW    TRUE)

# FastNoise2 — SIMD procedural noise
FetchContent_Declare(fastnoise2
    GIT_REPOSITORY https://github.com/Auburn/FastNoise2.git
    GIT_TAG        v0.10.0-alpha
    GIT_SHALLOW    TRUE)

# nlohmann/json — world state serialization
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE)

# spdlog — structured logging
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE)

# Catch2 — test framework
if(SOMNI_BUILD_TESTS)
    FetchContent_Declare(catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.6.0
        GIT_SHALLOW    TRUE)
endif()

# Disable sub-project options that add noise / conflict
set(BTCPP_BUILD_TOOLS        OFF CACHE BOOL "" FORCE)
set(BTCPP_GROOT_INTERFACE    OFF CACHE BOOL "" FORCE)
set(BTCPP_SQLITE_LOGGING     OFF CACHE BOOL "" FORCE)
set(BTCPP_MANUAL_SELECTOR    OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING            OFF CACHE BOOL "" FORCE)
set(FASTNOISE2_NOISETOOL     OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(entt behaviortree_cpp fastnoise2 nlohmann_json spdlog)
if(SOMNI_BUILD_TESTS)
    FetchContent_MakeAvailable(catch2)
endif()

# ---------------------------------------------------------------------------
# Kernel library
# ---------------------------------------------------------------------------
add_subdirectory(kernel)

# ---------------------------------------------------------------------------
# Python bindings
# ---------------------------------------------------------------------------
if(SOMNI_BUILD_BINDINGS)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    FetchContent_Declare(pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.12.0
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(pybind11)
    add_subdirectory(kernel/bindings)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(SOMNI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()
