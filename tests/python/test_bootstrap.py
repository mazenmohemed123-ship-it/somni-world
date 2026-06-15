"""
Python orchestrator tests — no C++ kernel required.
Tests WorldSpec, WorldQuestioner, and StateSyncBridge.
"""
import pytest
from orchestrator.bootstrap.world_schema import (
    FactionSpec, Ideology, WorldScale, WorldSpec, WorldType, TechLevel
)
from orchestrator.bootstrap.questioner import WorldQuestioner, _match_keyword, _TECH_KEYWORDS
from orchestrator.simulation.state_sync import StateSyncBridge


# ---------------------------------------------------------------------------
# WorldSpec tests
# ---------------------------------------------------------------------------

class TestWorldSpec:
    def test_default_validates_without_factions(self):
        spec = WorldSpec()
        errors = spec.validate()
        assert any("faction" in e.lower() for e in errors)

    def test_valid_spec(self):
        spec = WorldSpec(name="test", factions=[
            FactionSpec(name="Romans", spawn_x=100, spawn_y=100)
        ])
        assert spec.validate() == []

    def test_dimensions_match_scale(self):
        spec = WorldSpec(scale=WorldScale.SMALL)
        assert spec.dimensions() == (128, 128)
        spec.scale = WorldScale.LARGE
        assert spec.dimensions() == (512, 512)

    def test_seed_derivation_is_deterministic(self):
        spec = WorldSpec(name="hello")
        s1 = spec.derive_seed()
        s2 = spec.derive_seed()
        assert s1 == s2

    def test_json_roundtrip(self):
        spec = WorldSpec(
            name="my_world", seed=999,
            factions=[FactionSpec(name="Mongols", ideology=Ideology.NOMADIC)]
        )
        j = spec.to_json()
        spec2 = WorldSpec.from_json(j)
        assert spec2.name == "my_world"
        assert spec2.seed == 999
        assert len(spec2.factions) == 1
        assert spec2.factions[0].name == "Mongols"
        assert spec2.factions[0].ideology == Ideology.NOMADIC


# ---------------------------------------------------------------------------
# WorldQuestioner tests
# ---------------------------------------------------------------------------

class TestWorldQuestioner:
    def _make_questioner(self, scenario: str, **extra) -> WorldQuestioner:
        answers = {
            "input":        scenario,
            "time_period":  "medieval",
            "scale":        "medium",
            "factions":     "2",
            "faction_names": "Romans,Mongols",
            "conflict":     "clash of empires",
        }
        answers.update(extra)
        return WorldQuestioner(headless_answers=answers)

    def test_produces_valid_spec(self):
        spec = self._make_questioner("What if Rome never fell?").run()
        errors = spec.validate()
        assert errors == [], f"Validation errors: {errors}"

    def test_fantasy_keyword_detection(self):
        spec = self._make_questioner("A fantasy world with magic").run()
        assert spec.world_type == WorldType.FANTASY
        assert spec.enable_fantasy_resources is True

    def test_faction_count(self):
        spec = self._make_questioner(
            "medieval kingdom wars",
            factions="3",
            faction_names="England,France,Spain",
        ).run()
        assert len(spec.factions) == 3

    def test_tech_keyword_medieval(self):
        result = _match_keyword("medieval", _TECH_KEYWORDS)
        assert result == TechLevel.MEDIEVAL

    def test_tech_keyword_roman(self):
        result = _match_keyword("roman empire", _TECH_KEYWORDS)
        assert result == TechLevel.IRON_AGE

    def test_factions_spread_across_map(self):
        spec = self._make_questioner(
            "two kingdoms",
            factions="2",
            faction_names="North,South",
        ).run()
        f0, f1 = spec.factions[0], spec.factions[1]
        # They should not be at the same position
        assert (f0.spawn_x != f1.spawn_x) or (f0.spawn_y != f1.spawn_y)

    def test_seed_is_deterministic_for_same_input(self):
        q1 = self._make_questioner("rome vs carthage")
        q2 = self._make_questioner("rome vs carthage")
        assert q1.run().seed == q2.run().seed


# ---------------------------------------------------------------------------
# StateSyncBridge tests
# ---------------------------------------------------------------------------

class TestStateSyncBridge:
    def test_initial_state(self):
        b = StateSyncBridge()
        snap = b.get_snapshot()
        assert snap.tick == 0
        assert snap.active_wars == 0
        assert snap.recent_events == []

    def test_tick_advances(self):
        b = StateSyncBridge()
        b.on_tick(42, 42.0)
        assert b.current_tick() == 42

    def test_npc_died_event_recorded(self):
        b = StateSyncBridge()
        b.on_tick(10, 10.0)
        b.on_npc_died(99, 0)  # 0 = starvation
        events = b.get_events_since(10)
        assert len(events) == 1
        assert events[0]["type"] == "npc_died"
        assert events[0]["cause"] == "starvation"
        assert events[0]["npc_id"] == 99

    def test_war_increments_counter(self):
        b = StateSyncBridge()
        b.on_faction_war(1, 2, 5)
        snap = b.get_snapshot()
        assert snap.active_wars == 1

    def test_events_since_filters_by_tick(self):
        b = StateSyncBridge()
        b.on_tick(5,  5.0);  b.on_npc_died(1, 0)
        b.on_tick(10, 10.0); b.on_npc_died(2, 0)
        b.on_tick(15, 15.0); b.on_npc_died(3, 0)
        events = b.get_events_since(10)
        ticks = [e["tick"] for e in events]
        assert all(t >= 10 for t in ticks)
