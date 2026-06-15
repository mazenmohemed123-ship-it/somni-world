#pragma once
#include <cstdint>
#include <functional>
#include <chrono>
#include <atomic>
#include <string>

namespace somni {

// ---------------------------------------------------------------------------
// Fixed-timestep tick engine
// Guarantees deterministic ordering and reproducible simulation
// ---------------------------------------------------------------------------
struct TickConfig {
    uint32_t ticks_per_second{20};
    uint32_t max_catch_up_ticks{5};   // limit spiral-of-death
    float    time_scale{1.0f};        // simulation speed multiplier
    bool     headless{true};          // true = run as fast as possible
};

// ---------------------------------------------------------------------------
// System registration — each system runs in a fixed order each tick
// ---------------------------------------------------------------------------
struct SimulationSystem {
    std::string name;
    uint32_t    priority{100};   // lower = runs first
    std::function<void(uint64_t tick)> update;
};

// ---------------------------------------------------------------------------
// TickEngine: drives the simulation loop
//
// Guaranteed ordering per tick:
//  1. world state (resources, environment, clock)
//  2. NPC needs decay
//  3. NPC behavior trees + FSMs
//  4. Society systems (economy, conflict, diplomacy)
//  5. Event bus flush
//  6. Snapshot/checkpoint (if scheduled)
// ---------------------------------------------------------------------------
class TickEngine {
public:
    explicit TickEngine(TickConfig cfg = {});

    // Register a system — must be called before run()
    void register_system(SimulationSystem sys);

    // Run the simulation loop
    // blocking=true: runs until stop() is called or tick_limit is reached
    // blocking=false: call step() manually
    void run(uint64_t tick_limit = 0);
    void step(uint32_t n_ticks = 1);
    void stop();
    void pause();
    void resume();

    // Current simulation state
    uint64_t current_tick()    const { return tick_; }
    bool     is_running()      const { return running_.load(); }
    bool     is_paused()       const { return paused_.load(); }
    double   ticks_per_second_actual() const { return actual_tps_; }

    // Snapshot scheduling
    void schedule_snapshot(uint64_t at_tick, std::function<void(uint64_t)> fn);

private:
    void sort_systems();
    void execute_tick(uint64_t tick);
    void run_scheduled_snapshots(uint64_t tick);

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::nanoseconds;

    TickConfig  cfg_;
    Duration    tick_duration_;    // nanoseconds per tick
    uint64_t    tick_{0};
    double      actual_tps_{0.0};

    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};

    std::vector<SimulationSystem>                        systems_;
    std::vector<std::pair<uint64_t, std::function<void(uint64_t)>>> snapshots_;
};

}  // namespace somni
