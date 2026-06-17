set(SOMNI_KERNEL_SOURCES
    src/core/EventBus.cpp
    src/core/WorldState.cpp
    src/core/TickEngine.cpp
    src/core/SimulationLOD.cpp
    src/core/SimulationKernel.cpp
    src/world/Geography.cpp
    src/world/Resources.cpp
    src/world/WorldMap.cpp
    src/agents/Needs.cpp
    src/agents/Memory.cpp
    src/agents/StateMachine.cpp
    src/agents/BehaviorTree.cpp
    src/agents/NPC.cpp
    src/societies/Faction.cpp
    src/societies/Economy.cpp
    src/societies/Conflict.cpp
)

add_library(somni_kernel STATIC ${SOMNI_KERNEL_SOURCES})
add_library(SOMNI::kernel ALIAS somni_kernel)

target_include_directories(somni_kernel
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(somni_kernel
    PUBLIC
        EnTT::EnTT
        behaviortree_cpp
        FastNoise2
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

target_compile_options(somni_kernel PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O3 -march=native -DNDEBUG>
        $<$<CONFIG:Debug>:-g3>
    >
)
# NOTE: sanitizers (ASan/UBSan) are opt-in via SOMNI_SANITIZE in the root
# CMakeLists and applied globally to BOTH compile and link of every target.
# Applying -fsanitize to only this library's compile step left the test and
# binding executables unable to resolve the __asan_*/__ubsan_* runtime symbols.

target_compile_definitions(somni_kernel PUBLIC
    SOMNI_VERSION_MAJOR=0
    SOMNI_VERSION_MINOR=1
    SOMNI_VERSION_PATCH=0
)
