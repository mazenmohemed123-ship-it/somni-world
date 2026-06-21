"""
Tests for the pure-Python simulation backend and the preset library.
These exercise the ZERO-BUILD path (no C++ kernel required).
"""
import json
import time

from orchestrator.bootstrap.world_schema import WorldSpec, FactionSpec, WorldScale
from orchestrator.bootstrap.presets import get_presets, get_preset
from orchestrator.pysim import PyKernel, is_available


def _spec(scale=WorldScale.SMALL, seed=12345):
    s = WorldSpec(name="UnitWorld", seed=seed, scale=scale)
    s.factions = [
        FactionSpec(name="Alpha", initial_population=300),
        FactionSpec(name="Beta", initial_population=300),
        FactionSpec(name="Gamma", initial_population=200),
    ]
    return s


class TestPyKernelWorldGen:
    def test_available(self):
        assert is_available() is True

    def test_world_json_shape(self):
        k = PyKernel(_spec())
        w = json.loads(k.world_json())
        assert set(w) >= {"config", "clock", "regions", "factions"}
        assert w["config"]["width"] == 128
        assert len(w["regions"]) == 64  # 128/16 = 8x8
        r0 = w["regions"][0]
        assert set(r0) >= {"id", "grid_x", "grid_y", "biome",
                           "npc_count", "controlling_faction", "resources"}
        assert len(r0["resources"]) == 16

    def test_factions_claim_territory(self):
        k = PyKernel(_spec())
        w = json.loads(k.world_json())
        claimed = [r for r in w["regions"] if r["controlling_faction"] >= 0]
        assert len(claimed) >= 3                      # each faction got land
        owners = {r["controlling_faction"] for r in claimed}
        assert owners == {0, 1, 2}                    # all three present

    def test_population_seeded(self):
        k = PyKernel(_spec())
        w = json.loads(k.world_json())
        total = sum(r["npc_count"] for r in w["regions"])
        assert total > 0

    def test_biomes_varied(self):
        k = PyKernel(_spec(scale=WorldScale.MEDIUM))
        w = json.loads(k.world_json())
        biomes = {r["biome"] for r in w["regions"]}
        assert len(biomes) >= 3                        # not a flat single-biome map


class TestDeterminism:
    def test_same_seed_identical_world(self):
        a = PyKernel(_spec(seed=777)).world_json()
        b = PyKernel(_spec(seed=777)).world_json()
        assert a == b

    def test_different_seed_differs(self):
        a = PyKernel(_spec(seed=1)).world_json()
        b = PyKernel(_spec(seed=2)).world_json()
        assert a != b


class TestTickLoop:
    def test_clock_advances(self):
        k = PyKernel(_spec(), ticks_per_second=500)
        assert k.current_tick() == 0
        k.start()
        time.sleep(0.4)
        k.stop()
        assert k.current_tick() > 0

    def test_events_fire(self):
        k = PyKernel(_spec(), ticks_per_second=2000)
        seen = {"tick": 0}
        k.on_tick(lambda t, s: seen.__setitem__("tick", t))
        k.start()
        time.sleep(0.4)
        k.stop()
        assert seen["tick"] > 0

    def test_player_disaster_reduces_population(self):
        k = PyKernel(_spec())
        # find a populated region
        rid = next(r.id for r in k.regions if r.npc_count > 0)
        before = k.regions[rid].npc_count
        k.trigger_disaster(rid, 0.5)
        assert k.regions[rid].npc_count < before


class TestClock:
    def test_clock_fields(self):
        k = PyKernel(_spec())
        c = k.clock()
        assert c.tick == 0
        assert isinstance(c.season, int) and 0 <= c.season <= 3
        assert c.season_name in ("spring", "summer", "autumn", "winter")


class TestPresets:
    def test_presets_nonempty(self):
        presets = get_presets()
        assert len(presets) >= 10

    def test_preset_fields(self):
        for p in get_presets():
            assert {"id", "name", "scenario", "scale", "factions", "tags"} <= set(p)
            assert p["factions"] >= 1

    def test_get_preset_by_id(self):
        p = get_preset("viking_age")
        assert p["name"] == "Viking Age"

    def test_preset_ids_unique(self):
        ids = [p["id"] for p in get_presets()]
        assert len(ids) == len(set(ids))
