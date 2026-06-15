#include <somni/core/TickEngine.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <thread>

namespace somni {

TickEngine::TickEngine(TickConfig cfg) : cfg_(cfg) {
    uint64_t ns_per_tick = static_cast<uint64_t>(
        1e9 / (cfg_.ticks_per_second * cfg_.time_scale));
    tick_duration_ = Duration(ns_per_tick);
}

void TickEngine::register_system(SimulationSystem sys) {
    systems_.push_back(std::move(sys));
    sort_systems();
}

void TickEngine::sort_systems() {
    std::sort(systems_.begin(), systems_.end(),
              [](const SimulationSystem& a, const SimulationSystem& b) {
                  return a.priority < b.priority;
              });
}

void TickEngine::schedule_snapshot(uint64_t at_tick, std::function<void(uint64_t)> fn) {
    snapshots_.push_back({at_tick, std::move(fn)});
}

void TickEngine::execute_tick(uint64_t tick) {
    for (auto& sys : systems_) {
        sys.update(tick);
    }
    run_scheduled_snapshots(tick);
}

void TickEngine::run_scheduled_snapshots(uint64_t tick) {
    for (auto& [at, fn] : snapshots_) {
        if (at == tick) fn(tick);
    }
}

void TickEngine::step(uint32_t n_ticks) {
    for (uint32_t i = 0; i < n_ticks; ++i) {
        execute_tick(tick_++);
    }
}

void TickEngine::run(uint64_t tick_limit) {
    running_.store(true);
    paused_.store(false);

    if (cfg_.headless) {
        // Run as fast as possible — no wall-clock pacing
        auto t_start = Clock::now();
        uint64_t ticks_since_measure = 0;

        while (running_.load()) {
            if (paused_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            execute_tick(tick_++);
            ++ticks_since_measure;

            if (tick_limit > 0 && tick_ >= tick_limit) break;

            // Update actual TPS measurement every 1000 ticks
            if (ticks_since_measure >= 1000) {
                auto now     = Clock::now();
                double elapsed = std::chrono::duration<double>(now - t_start).count();
                actual_tps_   = ticks_since_measure / elapsed;
                ticks_since_measure = 0;
                t_start = now;
                spdlog::debug("SOMNI tick={} TPS={:.1f}", tick_, actual_tps_);
            }
        }
    } else {
        // Real-time pacing
        auto next_tick_time = Clock::now();

        while (running_.load()) {
            if (paused_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                next_tick_time = Clock::now();
                continue;
            }
            auto now = Clock::now();
            if (now >= next_tick_time) {
                uint32_t catch_up = 0;
                while (now >= next_tick_time && catch_up < cfg_.max_catch_up_ticks) {
                    execute_tick(tick_++);
                    next_tick_time += tick_duration_;
                    ++catch_up;
                    if (tick_limit > 0 && tick_ >= tick_limit) { stop(); break; }
                }
                if (catch_up > 1) {
                    spdlog::warn("SOMNI falling behind: caught up {} ticks", catch_up);
                }
            } else {
                std::this_thread::sleep_for(next_tick_time - now);
            }
        }
    }

    running_.store(false);
}

void TickEngine::stop()   { running_.store(false); }
void TickEngine::pause()  { paused_.store(true); }
void TickEngine::resume() { paused_.store(false); }

}  // namespace somni
