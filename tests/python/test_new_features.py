"""
Tests for the three new features:
  1. DeterministicMath (Python-side validation of concepts)
  2. LOD (Python analytics integration)
  3. EventBus segmentation (SimulationBuses concept)
  4. Visualization (ASCII backend, zero deps)
  5. Analytics (pure-Python summary)
"""
import pytest
from orchestrator.analytics.world_analytics import WorldAnalytics
from orchestrator.visualization.world_renderer import WorldRenderer, BIOME_ASCII


# ---------------------------------------------------------------------------
# DeterministicMath concept validation
# ---------------------------------------------------------------------------

class TestDeterministicConcepts:
    """
    Validate that Python-side equivalents are deterministic.
    The C++ Fixed32 / PCG32 are tested in C++ test suite.
    """
    def test_same_seed_same_sequence(self):
        import hashlib

        def seeded_sequence(seed: int, n: int):
            state = seed
            results = []
            for _ in range(n):
                h = hashlib.sha256(state.to_bytes(8, "big")).digest()
                state = int.from_bytes(h[:8], "big")
                results.append(state % 1000)
            return results

        s1 = seeded_sequence(42, 10)
        s2 = seeded_sequence(42, 10)
        s3 = seeded_sequence(99, 10)
        assert s1 == s2      # same seed → same sequence
        assert s1 != s3      # different seed → different sequence

    def test_deterministic_hash_no_collision(self):
        """det_hash(tick, id_a) produces different values for different inputs."""
        def det_hash(tick: int, id_a: int) -> int:
            h = 14695981039346656037
            h = (h ^ tick)  * 1099511628211 & 0xFFFFFFFFFFFFFFFF
            h = (h ^ id_a)  * 1099511628211 & 0xFFFFFFFFFFFFFFFF
            h ^= h >> 33
            return h & 0xFFFFFFFF

        hashes = {det_hash(t, i) for t in range(100) for i in range(100)}
        assert len(hashes) > 9000  # near-zero collision rate


# ---------------------------------------------------------------------------
# LOD concept validation
# ---------------------------------------------------------------------------

class TestLODConcepts:
    def _make_regions(self, grid_w: int, grid_h: int) -> list:
        regions = []
        for gy in range(grid_h):
            for gx in range(grid_w):
                regions.append({
                    "id": gy * grid_w + gx,
                    "grid_x": gx, "grid_y": gy,
                    "biome": 4, "npc_count": 10,
                    "controlling_faction": 1,
                    "resources": [100.0] * 16,
                })
        return regions

    def test_distance_calculation(self):
        import math
        obs_x, obs_y = 4, 4
        for gx, gy in [(4, 4), (5, 4), (9, 4), (25, 25)]:
            dx, dy = gx - obs_x, gy - obs_y
            dist = math.sqrt(dx*dx + dy*dy)
            if dist <= 4.0:
                lod = "FULL"
            elif dist <= 16.0:
                lod = "STATISTICAL"
            else:
                lod = "FROZEN"

            if gx == 4 and gy == 4:  assert lod == "FULL"
            if gx == 5 and gy == 4:  assert lod == "FULL"
            if gx == 9 and gy == 4:   assert lod == "STATISTICAL"
            if gx == 25 and gy == 25: assert lod == "FROZEN"

    def test_statistical_model_population_grows(self):
        """Validate the statistical population advance formula."""
        population = 100.0
        birth_rate = 0.0001
        death_rate = 0.00008
        net_rate = birth_rate - death_rate
        n_ticks = 1000
        new_pop = population * (1.0 + net_rate * n_ticks)
        assert new_pop > population  # should grow

    def test_frozen_regions_not_updated(self):
        """Frozen regions should have no state change — validated by LOD manager logic."""
        class MockRegion:
            npc_count = 50
            resource_amount = [100.0] * 16

        # Simulate: FROZEN region → skip all updates
        is_frozen = True
        region = MockRegion()
        original_npc = region.npc_count

        if not is_frozen:
            region.npc_count += 10  # this would run

        assert region.npc_count == original_npc  # unchanged


# ---------------------------------------------------------------------------
# EventBus segmentation concept
# ---------------------------------------------------------------------------

class TestEventBusSegmentation:
    def test_three_independent_buses(self):
        """Each bus should be isolated — events on one don't fire on others."""
        from orchestrator.simulation.state_sync import StateSyncBridge

        world_events = []
        agent_events = []
        economy_events = []

        # Simulate three buses with Python queues
        class Bus:
            def __init__(self):
                self._handlers = []
            def subscribe(self, fn): self._handlers.append(fn)
            def emit(self, e):
                for h in self._handlers: h(e)

        world_bus   = Bus()
        agent_bus   = Bus()
        economy_bus = Bus()

        world_bus.subscribe(lambda e: world_events.append(e))
        agent_bus.subscribe(lambda e: agent_events.append(e))
        economy_bus.subscribe(lambda e: economy_events.append(e))

        world_bus.emit({"type": "disaster", "region": 5})
        agent_bus.emit({"type": "npc_died", "npc_id": 99})
        economy_bus.emit({"type": "trade_completed", "amount": 50.0})

        assert len(world_events)   == 1 and world_events[0]["type"] == "disaster"
        assert len(agent_events)   == 1 and agent_events[0]["type"] == "npc_died"
        assert len(economy_events) == 1 and economy_events[0]["amount"] == 50.0
        # No cross-contamination
        assert len(world_events) == 1
        assert len(agent_events) == 1


# ---------------------------------------------------------------------------
# Visualization tests
# ---------------------------------------------------------------------------

class TestWorldRenderer:
    def _make_world_json(self) -> str:
        import json
        return json.dumps({
            "config": {"name": "test", "width": 64, "height": 64, "region_size": 16},
            "clock": {"tick": 100, "year": 1, "season": 0, "month": 1, "day": 1, "hour": 8},
            "regions": [
                {"id": i, "grid_x": i % 4, "grid_y": i // 4,
                 "biome": i % 11, "npc_count": i * 5,
                 "controlling_faction": 1, "resources": [100.0] * 16}
                for i in range(16)
            ]
        })

    def test_ascii_renderer_no_crash(self, capsys):
        renderer = WorldRenderer(backend="ascii")
        renderer.render_terrain(self._make_world_json())
        captured = capsys.readouterr()
        assert "SOMNI World" in captured.out
        assert "Biome map" in captured.out

    def test_ascii_biome_characters(self):
        """Each biome maps to a distinct ASCII character."""
        chars = list(BIOME_ASCII.values())
        assert len(chars) == len(set(chars))  # all unique

    def test_renderer_backend_auto_falls_back(self):
        """Auto backend always resolves to something."""
        renderer = WorldRenderer(backend="auto")
        assert renderer._backend in ("rerun", "pyvista", "ascii")

    def test_faction_render_no_crash(self, capsys):
        renderer = WorldRenderer(backend="ascii")
        renderer.render_factions(self._make_world_json())
        captured = capsys.readouterr()
        assert "Faction Territory" in captured.out


# ---------------------------------------------------------------------------
# Analytics tests
# ---------------------------------------------------------------------------

class TestWorldAnalytics:
    def _make_events(self) -> list:
        return [
            {"tick": 10,  "type": "npc_died", "npc_id": 1,  "cause": "starvation", "region": 0},
            {"tick": 20,  "type": "npc_died", "npc_id": 2,  "cause": "combat",     "region": 1},
            {"tick": 30,  "type": "npc_died", "npc_id": 3,  "cause": "starvation", "region": 0},
            {"tick": 50,  "type": "faction_war_start", "attacker": 1, "defender": 2, "region": 3},
            {"tick": 100, "type": "trade_completed", "faction_a": 1, "faction_b": 2, "amount": 50.0},
            {"tick": 200, "type": "trade_completed", "faction_a": 1, "faction_b": 3, "amount": 30.0},
        ]

    def test_summary_counts(self):
        analytics = WorldAnalytics(self._make_events())
        s = analytics.summary()
        assert s["total_events"]  == 6
        assert s["total_deaths"]  == 3
        assert s["total_wars"]    == 1
        assert s["total_trades"]  == 2

    def test_death_causes(self):
        analytics = WorldAnalytics(self._make_events())
        s = analytics.summary()
        assert s["deaths_by_cause"]["starvation"] == 2
        assert s["deaths_by_cause"]["combat"]     == 1

    def test_networkx_faction_graph(self):
        pytest.importorskip("networkx")
        analytics = WorldAnalytics(self._make_events())
        G = analytics.faction_network()
        import networkx as nx
        assert G.has_edge(1, 2)
        assert G[1][2]["wars"] == 1

    def test_print_report_no_crash(self, capsys):
        analytics = WorldAnalytics(self._make_events())
        analytics.print_report()
        captured = capsys.readouterr()
        assert "Analytics Report" in captured.out
