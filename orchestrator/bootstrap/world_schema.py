"""
World specification schema — structured data, no LLMs.
All world properties are numeric/enum; strings are only for names/labels.
"""
from __future__ import annotations

import json
import hashlib
from dataclasses import dataclass, field
from enum import IntEnum
from typing import List, Optional


class WorldScale(IntEnum):
    MICRO   = 0   # 64×64   — small settlement
    SMALL   = 1   # 128×128 — city-state
    MEDIUM  = 2   # 256×256 — region
    LARGE   = 3   # 512×512 — continent
    MASSIVE = 4   # 1024×1024 — world


class TechLevel(IntEnum):
    STONE_AGE    = 0
    BRONZE_AGE   = 1
    IRON_AGE     = 2
    MEDIEVAL     = 3
    RENAISSANCE  = 4
    INDUSTRIAL   = 5
    MODERN       = 6
    FUTURE       = 7


class WorldType(IntEnum):
    REALISTIC  = 0
    FANTASY    = 1
    SCI_FI     = 2
    ALTERNATE  = 3   # historical "what if"
    MYTHIC     = 4


class Ideology(IntEnum):
    TRIBAL     = 0
    AGRARIAN   = 1
    MILITARIST = 2
    MERCANTILE = 3
    THEOCRATIC = 4
    NOMADIC    = 5


class Government(IntEnum):
    CHIEFTAINCY = 0
    MONARCHY    = 1
    OLIGARCHY   = 2
    REPUBLIC    = 3
    THEOCRACY   = 4


WORLD_DIMENSIONS = {
    WorldScale.MICRO:   (64,   64),
    WorldScale.SMALL:   (128,  128),
    WorldScale.MEDIUM:  (256,  256),
    WorldScale.LARGE:   (512,  512),
    WorldScale.MASSIVE: (1024, 1024),
}


@dataclass
class FactionSpec:
    name: str
    ideology: Ideology = Ideology.TRIBAL
    government: Government = Government.CHIEFTAINCY
    spawn_x: int = 0
    spawn_y: int = 0
    initial_population: int = 100
    initial_treasury: float = 200.0
    initial_resources: List[float] = field(default_factory=lambda: [0.0] * 16)

    def validate(self) -> bool:
        return bool(self.name) and self.initial_population > 0

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "ideology": int(self.ideology),
            "government": int(self.government),
            "spawn_x": self.spawn_x,
            "spawn_y": self.spawn_y,
            "initial_population": self.initial_population,
            "initial_treasury": self.initial_treasury,
            "initial_resources": self.initial_resources,
        }


@dataclass
class ScriptedEvent:
    trigger_tick: int
    event_type: str   # "disaster" | "plague" | "trade_boom" | "invasion"
    region_id: int
    magnitude: float = 0.5


@dataclass
class WorldSpec:
    """
    Complete world specification — produced by WorldQuestioner,
    consumed by WorldBuilder to create a WorldBootstrap for the C++ kernel.
    """
    # Identity
    name: str = "unnamed_world"
    seed: int = 42

    # Scale and type
    scale: WorldScale = WorldScale.MEDIUM
    world_type: WorldType = WorldType.REALISTIC
    tech_level: TechLevel = TechLevel.MEDIEVAL

    # Factions
    factions: List[FactionSpec] = field(default_factory=list)

    # Simulation parameters
    ticks_per_second: int = 20
    time_scale: float = 1.0
    initial_npc_per_faction: int = 50
    enable_fantasy_resources: bool = False

    # Scripted events (optional — for historical/scenario worlds)
    scripted_events: List[ScriptedEvent] = field(default_factory=list)

    # Internal metadata
    core_conflict: str = ""     # text description, stored but NOT used in simulation
    narrative_tag: str = ""     # "viking-age", "roman-alternate", etc.

    def dimensions(self) -> tuple:
        return WORLD_DIMENSIONS[self.scale]

    def derive_seed(self, extra: str = "") -> int:
        """Derive a deterministic seed from name + extra text."""
        h = hashlib.sha256(f"{self.name}{extra}".encode()).digest()
        return int.from_bytes(h[:8], "big")

    def validate(self) -> List[str]:
        errors = []
        if not self.name:
            errors.append("World name is required")
        if not self.factions:
            errors.append("At least one faction is required")
        if len(self.factions) > 64:
            errors.append("Maximum 64 factions supported")
        for i, f in enumerate(self.factions):
            if not f.validate():
                errors.append(f"Faction {i} is invalid")
        return errors

    def to_json(self) -> str:
        d = {
            "name": self.name,
            "seed": self.seed,
            "scale": int(self.scale),
            "world_type": int(self.world_type),
            "tech_level": int(self.tech_level),
            "ticks_per_second": self.ticks_per_second,
            "time_scale": self.time_scale,
            "initial_npc_per_faction": self.initial_npc_per_faction,
            "enable_fantasy_resources": self.enable_fantasy_resources,
            "factions": [f.to_dict() for f in self.factions],
            "scripted_events": [
                {
                    "trigger_tick": e.trigger_tick,
                    "type": e.event_type,
                    "region_id": e.region_id,
                    "magnitude": e.magnitude,
                }
                for e in self.scripted_events
            ],
        }
        return json.dumps(d, indent=2)

    @classmethod
    def from_json(cls, s: str) -> "WorldSpec":
        d = json.loads(s)
        spec = cls()
        spec.name         = d.get("name", "unnamed")
        spec.seed         = d.get("seed", 42)
        spec.scale        = WorldScale(d.get("scale", 2))
        spec.world_type   = WorldType(d.get("world_type", 0))
        spec.tech_level   = TechLevel(d.get("tech_level", 3))
        spec.factions     = [
            FactionSpec(
                name=f["name"],
                ideology=Ideology(f.get("ideology", 0)),
                government=Government(f.get("government", 0)),
                spawn_x=f.get("spawn_x", 0),
                spawn_y=f.get("spawn_y", 0),
                initial_population=f.get("initial_population", 100),
                initial_treasury=f.get("initial_treasury", 200.0),
                initial_resources=f.get("initial_resources", [0.0] * 16),
            )
            for f in d.get("factions", [])
        ]
        return spec
