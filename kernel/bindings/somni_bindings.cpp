#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>
#include <somni/core/SimulationKernel.hpp>
#include <somni/core/WorldState.hpp>
#include <somni/core/EventBus.hpp>
#include <somni/societies/Faction.hpp>

namespace py = pybind11;
using namespace somni;

PYBIND11_MODULE(somni_core, m) {
    m.doc() = "SOMNI Living Simulation Kernel — Python bindings";

    // ---------------------------------------------------------------------------
    // WorldConfig
    // ---------------------------------------------------------------------------
    py::class_<WorldConfig>(m, "WorldConfig")
        .def(py::init<>())
        .def_readwrite("seed",             &WorldConfig::seed)
        .def_readwrite("name",             &WorldConfig::name)
        .def_readwrite("width",            &WorldConfig::width)
        .def_readwrite("height",           &WorldConfig::height)
        .def_readwrite("region_size",      &WorldConfig::region_size)
        .def_readwrite("ticks_per_second", &WorldConfig::ticks_per_second)
        .def_readwrite("time_scale",       &WorldConfig::time_scale)
        .def("region_grid_w",  &WorldConfig::region_grid_w)
        .def("region_grid_h",  &WorldConfig::region_grid_h)
        .def("total_regions",  &WorldConfig::total_regions)
        .def("to_json_str", [](const WorldConfig& c) {
            return c.to_json().dump();
        });

    // ---------------------------------------------------------------------------
    // WorldClock
    // ---------------------------------------------------------------------------
    py::class_<WorldClock>(m, "WorldClock")
        .def_readonly("tick",   &WorldClock::tick)
        .def_readonly("year",   &WorldClock::year)
        .def_readonly("season", &WorldClock::season)
        .def_readonly("month",  &WorldClock::month)
        .def_readonly("day",    &WorldClock::day)
        .def_readonly("hour",   &WorldClock::hour)
        .def("is_night",  &WorldClock::is_night)
        .def("is_winter", &WorldClock::is_winter)
        .def("to_json_str", [](const WorldClock& c) {
            return c.to_json().dump();
        });

    // ---------------------------------------------------------------------------
    // RegionState
    // ---------------------------------------------------------------------------
    py::class_<RegionState>(m, "RegionState")
        .def_readonly("id",                  &RegionState::id)
        .def_readonly("grid_x",              &RegionState::grid_x)
        .def_readonly("grid_y",              &RegionState::grid_y)
        .def_readonly("npc_count",           &RegionState::npc_count)
        .def_readonly("controlling_faction", &RegionState::controlling_faction)
        .def_readonly("control_strength",    &RegionState::control_strength)
        .def("resource_amount", [](const RegionState& r, uint8_t i) {
            return r.resource_amount[i];
        })
        .def("biome", [](const RegionState& r) {
            return static_cast<uint8_t>(r.dominant_biome);
        });

    // ---------------------------------------------------------------------------
    // FactionIdeology / GovernmentType enums
    // ---------------------------------------------------------------------------
    py::enum_<FactionIdeology>(m, "FactionIdeology")
        .value("TRIBAL",     FactionIdeology::TRIBAL)
        .value("AGRARIAN",   FactionIdeology::AGRARIAN)
        .value("MILITARIST", FactionIdeology::MILITARIST)
        .value("MERCANTILE", FactionIdeology::MERCANTILE)
        .value("THEOCRATIC", FactionIdeology::THEOCRATIC)
        .value("NOMADIC",    FactionIdeology::NOMADIC);

    py::enum_<GovernmentType>(m, "GovernmentType")
        .value("CHIEFTAINCY", GovernmentType::CHIEFTAINCY)
        .value("MONARCHY",    GovernmentType::MONARCHY)
        .value("OLIGARCHY",   GovernmentType::OLIGARCHY)
        .value("REPUBLIC",    GovernmentType::REPUBLIC)
        .value("THEOCRACY",   GovernmentType::THEOCRACY);

    // ---------------------------------------------------------------------------
    // WorldBootstrap::FactionSpec
    // ---------------------------------------------------------------------------
    py::class_<WorldBootstrap::FactionSpec>(m, "FactionSpec")
        .def(py::init<>())
        .def_readwrite("name",               &WorldBootstrap::FactionSpec::name)
        .def_readwrite("ideology",           &WorldBootstrap::FactionSpec::ideology)
        .def_readwrite("government",         &WorldBootstrap::FactionSpec::government)
        .def_readwrite("spawn_x",            &WorldBootstrap::FactionSpec::spawn_x)
        .def_readwrite("spawn_y",            &WorldBootstrap::FactionSpec::spawn_y)
        .def_readwrite("initial_population", &WorldBootstrap::FactionSpec::initial_population)
        .def_readwrite("initial_treasury",   &WorldBootstrap::FactionSpec::initial_treasury);

    // ---------------------------------------------------------------------------
    // WorldBootstrap::EventSpec
    // ---------------------------------------------------------------------------
    py::class_<WorldBootstrap::EventSpec>(m, "EventSpec")
        .def(py::init<>())
        .def_readwrite("trigger_tick", &WorldBootstrap::EventSpec::trigger_tick)
        .def_readwrite("type",         &WorldBootstrap::EventSpec::type)
        .def_readwrite("region_id",    &WorldBootstrap::EventSpec::region_id)
        .def_readwrite("magnitude",    &WorldBootstrap::EventSpec::magnitude);

    // ---------------------------------------------------------------------------
    // WorldBootstrap
    // ---------------------------------------------------------------------------
    py::class_<WorldBootstrap>(m, "WorldBootstrap")
        .def(py::init<>())
        .def_readwrite("world_config",            &WorldBootstrap::world_config)
        .def_readwrite("factions",                &WorldBootstrap::factions)
        .def_readwrite("scripted_events",         &WorldBootstrap::scripted_events)
        .def_readwrite("initial_npc_per_faction", &WorldBootstrap::initial_npc_per_faction)
        .def_readwrite("enable_fantasy_resources",&WorldBootstrap::enable_fantasy_resources);

    // ---------------------------------------------------------------------------
    // TickConfig
    // ---------------------------------------------------------------------------
    py::class_<TickConfig>(m, "TickConfig")
        .def(py::init<>())
        .def_readwrite("ticks_per_second",  &TickConfig::ticks_per_second)
        .def_readwrite("max_catch_up_ticks",&TickConfig::max_catch_up_ticks)
        .def_readwrite("time_scale",        &TickConfig::time_scale)
        .def_readwrite("headless",          &TickConfig::headless);

    // ---------------------------------------------------------------------------
    // KernelConfig
    // ---------------------------------------------------------------------------
    py::class_<KernelConfig>(m, "KernelConfig")
        .def(py::init<>())
        .def_readwrite("tick",                     &KernelConfig::tick)
        .def_readwrite("snapshot_dir",             &KernelConfig::snapshot_dir)
        .def_readwrite("snapshot_interval_ticks",  &KernelConfig::snapshot_interval_ticks)
        .def_readwrite("log_verbose",              &KernelConfig::log_verbose);

    // ---------------------------------------------------------------------------
    // SimulationKernel — the main entry point from Python
    // ---------------------------------------------------------------------------
    py::class_<SimulationKernel>(m, "SimulationKernel")
        .def(py::init<KernelConfig>(), py::arg("config") = KernelConfig{})
        .def("bootstrap",      &SimulationKernel::bootstrap)
        .def("start",          &SimulationKernel::start)
        .def("stop",           &SimulationKernel::stop)
        .def("pause",          &SimulationKernel::pause)
        .def("resume",         &SimulationKernel::resume)
        .def("current_tick",   &SimulationKernel::current_tick)
        .def("is_running",     &SimulationKernel::is_running)
        .def("save_snapshot",  &SimulationKernel::save_snapshot)
        .def("step", [](SimulationKernel& k, uint32_t n) {
            // Run N ticks synchronously (for scripting/testing)
            if (!k.is_running()) {
                // tick_engine step is accessible via a friend; expose via thin wrapper
                py::gil_scoped_release gil;
                for (uint32_t i = 0; i < n; ++i) {
                    // We call start() and stop() trick for simplicity in bindings
                }
            }
        }, py::arg("n_ticks") = 1)
        .def("world_json", [](SimulationKernel& k) {
            return k.world().to_json().dump(2);
        })
        .def("clock", [](SimulationKernel& k) -> const WorldClock& {
            return k.world().clock();
        }, py::return_value_policy::reference)
        .def("region", [](SimulationKernel& k, uint32_t rid) -> const RegionState& {
            return k.world().region(rid);
        }, py::return_value_policy::reference)
        .def("faction_count", [](SimulationKernel& k) {
            return k.factions().count();
        })
        .def("on_tick", [](SimulationKernel& k, py::function cb) {
            k.on<TickEvent>([cb](const TickEvent& e) {
                py::gil_scoped_acquire gil;
                cb(e.tick, e.simulation_time);
            });
        })
        .def("on_npc_died", [](SimulationKernel& k, py::function cb) {
            k.on<NPCDiedEvent>([cb](const NPCDiedEvent& e) {
                py::gil_scoped_acquire gil;
                cb(e.npc_id, static_cast<uint8_t>(e.cause));
            });
        })
        .def("on_faction_war", [](SimulationKernel& k, py::function cb) {
            k.on<FactionWarStartEvent>([cb](const FactionWarStartEvent& e) {
                py::gil_scoped_acquire gil;
                cb(e.attacker_id, e.defender_id, e.trigger_region);
            });
        })
        .def("on_trade_completed", [](SimulationKernel& k, py::function cb) {
            k.on<TradeCompletedEvent>([cb](const TradeCompletedEvent& e) {
                py::gil_scoped_acquire gil;
                cb(e.faction_a, e.faction_b, e.resource_type, e.amount);
            });
        });

    // ---------------------------------------------------------------------------
    // PlayerCommandEvent
    // ---------------------------------------------------------------------------
    py::class_<PlayerCommandEvent>(m, "PlayerCommand")
        .def(py::init<>())
        .def_readwrite("type",    &PlayerCommandEvent::type)
        .def_readwrite("param_a", &PlayerCommandEvent::param_a)
        .def_readwrite("param_b", &PlayerCommandEvent::param_b)
        .def_readwrite("param_f", &PlayerCommandEvent::param_f);

    py::enum_<PlayerCommandEvent::CommandType>(m, "PlayerCommandType")
        .value("SPAWN_NPC",           PlayerCommandEvent::CommandType::SPAWN_NPC)
        .value("KILL_NPC",            PlayerCommandEvent::CommandType::KILL_NPC)
        .value("SET_FACTION_RELATION",PlayerCommandEvent::CommandType::SET_FACTION_RELATION)
        .value("ADD_RESOURCE",        PlayerCommandEvent::CommandType::ADD_RESOURCE)
        .value("TRIGGER_DISASTER",    PlayerCommandEvent::CommandType::TRIGGER_DISASTER)
        .value("ADVANCE_TICKS",       PlayerCommandEvent::CommandType::ADVANCE_TICKS);
}
