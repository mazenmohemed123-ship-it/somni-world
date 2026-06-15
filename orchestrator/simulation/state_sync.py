"""
StateSyncBridge — collects simulation events and exposes
aggregated state snapshots for the API layer and external clients.

No LLMs. Pure data aggregation.
"""
from __future__ import annotations

import threading
from collections import deque
from dataclasses import dataclass, field
from typing import Deque, Dict, List, Optional


@dataclass
class TickStats:
    tick: int = 0
    sim_time: float = 0.0
    npc_deaths_this_tick: int = 0
    trades_this_tick: int = 0


@dataclass
class StateSnapshot:
    """Read-consistent snapshot of simulation state."""
    tick: int = 0
    year: int = 1
    season: str = "spring"

    # Aggregated counts
    total_npcs: int = 0
    total_factions: int = 0
    active_wars: int = 0

    # Recent events log (last N events, ring buffer)
    recent_events: List[Dict] = field(default_factory=list)

    # Per-region summary [region_id → summary]
    region_summaries: Dict[int, Dict] = field(default_factory=dict)


class StateSyncBridge:
    """
    Thread-safe bridge between C++ event callbacks and Python API consumers.
    Accumulates events and produces periodic snapshots.
    """

    MAX_EVENTS = 500   # rolling event log

    def __init__(self):
        self._lock         = threading.RLock()
        self._events: Deque[Dict] = deque(maxlen=self.MAX_EVENTS)
        self._stats        = TickStats()
        self._snapshot     = StateSnapshot()
        self._war_count    = 0

    # ------------------------------------------------------------------
    # Called from C++ event callbacks (may be on background thread)
    # ------------------------------------------------------------------

    def on_tick(self, tick: int, sim_time: float) -> None:
        with self._lock:
            self._stats.tick     = tick
            self._stats.sim_time = sim_time
            # Reset per-tick counters
            self._stats.npc_deaths_this_tick = 0
            self._stats.trades_this_tick     = 0

    def on_npc_died(self, npc_id: int, cause: int) -> None:
        cause_names = {0: "starvation", 1: "dehydration", 2: "combat",
                       3: "illness", 4: "old_age"}
        with self._lock:
            self._stats.npc_deaths_this_tick += 1
            self._events.append({
                "type": "npc_died",
                "tick": self._stats.tick,
                "npc_id": npc_id,
                "cause": cause_names.get(cause, "unknown"),
            })

    def on_faction_war(self, attacker: int, defender: int, region: int) -> None:
        with self._lock:
            self._war_count += 1
            self._events.append({
                "type": "faction_war_start",
                "tick": self._stats.tick,
                "attacker": attacker,
                "defender": defender,
                "region": region,
            })

    def on_battle_resolved(self, attacker: int, defender: int,
                           region: int, attacker_won: bool) -> None:
        with self._lock:
            if attacker_won: self._war_count = max(0, self._war_count - 1)
            self._events.append({
                "type": "battle_resolved",
                "tick": self._stats.tick,
                "attacker": attacker,
                "defender": defender,
                "region": region,
                "attacker_won": attacker_won,
            })

    def on_trade_completed(self, fa: int, fb: int, res: int, amount: float) -> None:
        with self._lock:
            self._stats.trades_this_tick += 1

    # ------------------------------------------------------------------
    # Read-side: safe snapshot for API consumers
    # ------------------------------------------------------------------

    def get_snapshot(self) -> StateSnapshot:
        with self._lock:
            snap = StateSnapshot()
            snap.tick         = self._stats.tick
            snap.active_wars  = self._war_count
            snap.recent_events = list(self._events)[-50:]  # last 50
            return snap

    def get_events_since(self, tick: int) -> List[Dict]:
        with self._lock:
            return [e for e in self._events if e.get("tick", 0) >= tick]

    def current_tick(self) -> int:
        return self._stats.tick
