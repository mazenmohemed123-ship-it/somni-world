"""
WorldBuilder — converts a WorldSpec into a C++ kernel WorldBootstrap,
then hands it off to SimulationKernel.
"""
from __future__ import annotations

import logging
from typing import Optional

from .world_schema import (
    FactionSpec, Ideology, Government, ScriptedEvent, WorldSpec, WorldType,
)

log = logging.getLogger(__name__)


class WorldBuilder:
    """
    STEP 3 + 4 of the pipeline:
      WorldSpec → WorldBootstrap → kernel.bootstrap()
    """

    def __init__(self, spec: WorldSpec):
        self.spec = spec

    def build_and_bootstrap(self, kernel) -> None:
        """
        Build the WorldBootstrap descriptor and hand it to the C++ kernel.
        kernel is a somni_core.SimulationKernel instance.
        """
        try:
            import somni_core
        except ImportError as e:
            raise RuntimeError(
                "somni_core not found. Build the C++ kernel with CMake first.\n"
                "Run: mkdir build && cd build && cmake .. && make"
            ) from e

        errors = self.spec.validate()
        if errors:
            raise ValueError(f"WorldSpec validation failed: {errors}")

        bootstrap = self._build_bootstrap(somni_core)
        log.info("Bootstrapping kernel: world=%s seed=%d", self.spec.name, self.spec.seed)
        kernel.bootstrap(bootstrap)
        log.info("Bootstrap complete")

    def _build_bootstrap(self, sc) -> object:
        """Build somni_core.WorldBootstrap from self.spec."""
        bootstrap = sc.WorldBootstrap()

        # World config
        w, h = self.spec.dimensions()
        bootstrap.world_config.seed             = self.spec.seed
        bootstrap.world_config.name             = self.spec.name
        bootstrap.world_config.width            = w
        bootstrap.world_config.height           = h
        bootstrap.world_config.region_size      = 16
        bootstrap.world_config.ticks_per_second = self.spec.ticks_per_second
        bootstrap.world_config.time_scale       = self.spec.time_scale

        bootstrap.initial_npc_per_faction  = self.spec.initial_npc_per_faction
        bootstrap.enable_fantasy_resources = self.spec.enable_fantasy_resources

        # Factions
        for fs in self.spec.factions:
            faction_spec = sc.FactionSpec()
            faction_spec.name               = fs.name
            faction_spec.ideology           = sc.FactionIdeology(int(fs.ideology))
            faction_spec.government         = sc.GovernmentType(int(fs.government))
            faction_spec.spawn_x            = fs.spawn_x
            faction_spec.spawn_y            = fs.spawn_y
            faction_spec.initial_population = fs.initial_population
            faction_spec.initial_treasury   = fs.initial_treasury
            bootstrap.factions.append(faction_spec)

        # Scripted events
        for ev in self.spec.scripted_events:
            event_spec = sc.EventSpec()
            event_spec.trigger_tick = ev.trigger_tick
            event_spec.type         = ev.event_type
            event_spec.region_id    = ev.region_id
            event_spec.magnitude    = ev.magnitude
            bootstrap.scripted_events.append(event_spec)

        return bootstrap

    @classmethod
    def from_user_input(cls, scenario: str,
                        additional_answers: Optional[dict] = None) -> "WorldBuilder":
        """
        Convenience factory: run the questioner with a pre-supplied scenario
        and optional extra answers, then return a builder.
        """
        from .questioner import WorldQuestioner
        answers = {"input": scenario}
        if additional_answers:
            answers.update(additional_answers)
        spec = WorldQuestioner(headless_answers=answers).run()
        return cls(spec)

    @classmethod
    def from_spec_file(cls, path: str) -> "WorldBuilder":
        with open(path) as f:
            spec = WorldSpec.from_json(f.read())
        return cls(spec)
