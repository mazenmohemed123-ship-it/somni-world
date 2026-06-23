"""
SimulationRunner — orchestrates the full pipeline:
  1. World bootstrap
  2. Kernel init
  3. Simulation loop management
  4. Event relay to connected clients
"""
from __future__ import annotations

import logging
import os
import threading
import time
from pathlib import Path
from typing import Callable, Dict, List, Optional

from ..bootstrap.world_builder import WorldBuilder
from ..bootstrap.world_schema import WorldSpec
from .state_sync import StateSyncBridge

log = logging.getLogger(__name__)


class SimulationRunner:
    """
    High-level controller for the SOMNI simulation kernel.
    """

    def __init__(self, spec: WorldSpec, snapshot_dir: str = "worlds/",
                 ticks_per_second: int = 20, headless: bool = True,
                 verbose: bool = False):
        self.spec          = spec
        self.snapshot_dir  = snapshot_dir
        self.tps           = ticks_per_second
        self.headless      = headless
        self.verbose       = verbose
        self.kernel        = None
        self.backend       = None   # "cpp" | "python", set in setup()
        self.sync_bridge   = StateSyncBridge()
        self._event_handlers: Dict[str, List[Callable]] = {}

    # ------------------------------------------------------------------
    # Pipeline execution
    # ------------------------------------------------------------------

    def setup(self) -> "SimulationRunner":
        """
        STEPS 3–4: Generate world and initialize agents.
        Must be called before start().

        Uses the compiled C++ kernel (somni_core) when available; otherwise
        transparently falls back to the pure-Python backend so the project
        runs with ZERO build steps (no CMake, no .pyd, no DLLs).
        """
        os.makedirs(self.snapshot_dir, exist_ok=True)

        try:
            import somni_core
            self.backend = "cpp"
        except ImportError:
            somni_core = None
            self.backend = "python"

        if somni_core is not None:
            cfg = somni_core.KernelConfig()
            cfg.tick.ticks_per_second = self.tps
            cfg.tick.headless         = self.headless
            cfg.tick.time_scale       = self.spec.time_scale
            cfg.snapshot_dir          = self.snapshot_dir
            cfg.snapshot_interval_ticks = 5000
            cfg.log_verbose           = self.verbose
            self.kernel = somni_core.SimulationKernel(cfg)
            builder = WorldBuilder(self.spec)
            builder.build_and_bootstrap(self.kernel)
        else:
            from ..pysim import PyKernel
            log.warning(
                "somni_core (C++ kernel) not found — using the pure-Python "
                "backend. The world still simulates deterministically; build "
                "the C++ kernel later for per-NPC behavior trees and scale.")
            self.kernel = PyKernel(
                self.spec, ticks_per_second=self.tps, headless=self.headless)

        # Register event relay (same surface on both backends).
        self.kernel.on_tick(self._on_tick)
        self.kernel.on_npc_died(self._on_npc_died)
        self.kernel.on_faction_war(self._on_faction_war)
        self.kernel.on_trade_completed(self._on_trade_completed)

        log.info("SimulationRunner ready — world=%s backend=%s",
                 self.spec.name, self.backend)
        return self

    def start(self) -> "SimulationRunner":
        """STEP 5: Start the simulation loop (non-blocking)."""
        if self.kernel is None:
            raise RuntimeError("Call setup() before start()")
        self.kernel.start()
        log.info("Simulation started")
        return self

    def stop(self) -> None:
        if self.kernel:
            self.kernel.stop()
            log.info("Simulation stopped at tick=%d", self.current_tick())

    def pause(self) -> None:
        if self.kernel: self.kernel.pause()

    def resume(self) -> None:
        if self.kernel: self.kernel.resume()

    def run_for_ticks(self, n: int, poll_interval: float = 0.05) -> None:
        """
        Start the simulation and wait until n ticks have elapsed.
        Useful for testing and scripted scenarios.
        """
        start_tick = self.current_tick()
        self.start()
        while (self.current_tick() - start_tick) < n:
            time.sleep(poll_interval)
        self.stop()

    def save_snapshot(self, path: Optional[str] = None) -> str:
        path = path or os.path.join(
            self.snapshot_dir, f"snap_{self.current_tick()}.somni")
        if self.kernel:
            self.kernel.save_snapshot(path)
        return path

    # ------------------------------------------------------------------
    # State queries (STEP 6 player interaction layer)
    # ------------------------------------------------------------------

    def current_tick(self) -> int:
        return self.kernel.current_tick() if self.kernel else 0

    def is_running(self) -> bool:
        return self.kernel.is_running() if self.kernel else False

    def world_json(self) -> str:
        if not self.kernel: return "{}"
        return self.kernel.world_json()

    def region_info(self, region_id: int) -> dict:
        if not self.kernel: return {}
        r = self.kernel.region(region_id)
        return {
            "id": r.id,
            "biome": r.biome(),
            "npc_count": r.npc_count,
            "controlling_faction": r.controlling_faction,
            "resources": [r.resource_amount(i) for i in range(16)],
        }

    def clock_info(self) -> dict:
        if not self.kernel: return {}
        c = self.kernel.clock()
        return {
            "tick": c.tick,
            "year": c.year,
            "season": ["spring", "summer", "autumn", "winter"][c.season],
            "month": c.month,
            "day": c.day,
            "hour": c.hour,
            "is_night": c.is_night(),
        }

    # ------------------------------------------------------------------
    # Player command interface (STEP 6)
    # ------------------------------------------------------------------

    def add_resource(self, region_id: int, resource_type: int, amount: float) -> None:
        if self.backend == "python":
            self.kernel.add_resource(region_id, resource_type, amount)
            return
        import somni_core
        cmd = somni_core.PlayerCommand()
        cmd.type    = somni_core.PlayerCommandType.ADD_RESOURCE
        cmd.param_a = region_id
        cmd.param_b = resource_type
        cmd.param_f = amount
        self.kernel.player_command(cmd)

    def trigger_disaster(self, region_id: int, magnitude: float = 0.5) -> None:
        if self.backend == "python":
            self.kernel.trigger_disaster(region_id, magnitude)
            return
        import somni_core
        cmd = somni_core.PlayerCommand()
        cmd.type    = somni_core.PlayerCommandType.TRIGGER_DISASTER
        cmd.param_a = region_id
        cmd.param_f = magnitude
        self.kernel.player_command(cmd)

    def declare_war(self, faction_a: int, faction_b: int) -> None:
        if self.backend == "python":
            self.kernel.declare_war(faction_a, faction_b)

    def make_peace(self, faction_a: int, faction_b: int) -> None:
        if self.backend == "python":
            self.kernel.make_peace(faction_a, faction_b)

    def recruit_army(self, faction_id: int, amount: float = 50.0) -> None:
        if self.backend == "python":
            self.kernel.recruit_army(faction_id, amount)

    def send_food(self, faction_id: int, amount: float = 200.0) -> None:
        if self.backend == "python":
            self.kernel.send_food(faction_id, amount)

    def spawn_settlers(self, faction_id: int, amount: int = 100) -> None:
        if self.backend == "python":
            self.kernel.spawn_settlers(faction_id, amount)

    def factions_info(self) -> list:
        """Return all faction data from the running world."""
        if not self.kernel:
            return []
        import json
        return json.loads(self.kernel.world_json()).get("factions", [])

    # ------------------------------------------------------------------
    # Event handlers
    # ------------------------------------------------------------------

    def on(self, event_type: str) -> Callable:
        """Decorator to register a Python callback for a simulation event."""
        def decorator(fn: Callable) -> Callable:
            self._event_handlers.setdefault(event_type, []).append(fn)
            return fn
        return decorator

    def _dispatch(self, event_type: str, *args) -> None:
        for handler in self._event_handlers.get(event_type, []):
            try:
                handler(*args)
            except Exception as e:
                log.warning("Event handler error [%s]: %s", event_type, e)

    def _on_tick(self, tick: int, sim_time: float) -> None:
        self.sync_bridge.on_tick(tick, sim_time)
        self._dispatch("tick", tick, sim_time)

    def _on_npc_died(self, npc_id: int, cause: int) -> None:
        self.sync_bridge.on_npc_died(npc_id, cause)
        self._dispatch("npc_died", npc_id, cause)

    def _on_faction_war(self, attacker: int, defender: int, region: int) -> None:
        log.info("WAR: faction %d attacks faction %d in region %d",
                 attacker, defender, region)
        self._dispatch("faction_war", attacker, defender, region)

    def _on_trade_completed(self, fa: int, fb: int, res: int, amount: float) -> None:
        self._dispatch("trade_completed", fa, fb, res, amount)
