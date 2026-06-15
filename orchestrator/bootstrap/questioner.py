"""
World Questioner — structured CLI interview to produce a WorldSpec.
NO LLM involved. Pure rule-based interpretation of user responses.
"""
from __future__ import annotations

import hashlib
import math
import re
from typing import Any, Dict, List, Optional, Tuple

from .world_schema import (
    FactionSpec, Government, Ideology, ScriptedEvent,
    TechLevel, WorldScale, WorldSpec, WorldType,
)


# ---------------------------------------------------------------------------
# Keyword → enum mapping tables (no NLP, just token matching)
# ---------------------------------------------------------------------------

_WORLD_TYPE_KEYWORDS: Dict[str, WorldType] = {
    "historical": WorldType.ALTERNATE,
    "alternate": WorldType.ALTERNATE,
    "what if": WorldType.ALTERNATE,
    "fantasy": WorldType.FANTASY,
    "magic": WorldType.FANTASY,
    "myth": WorldType.MYTHIC,
    "mythical": WorldType.MYTHIC,
    "god": WorldType.MYTHIC,
    "sci-fi": WorldType.SCI_FI,
    "science fiction": WorldType.SCI_FI,
    "space": WorldType.SCI_FI,
    "realistic": WorldType.REALISTIC,
    "real": WorldType.REALISTIC,
    "modern": WorldType.REALISTIC,
}

_TECH_KEYWORDS: Dict[str, TechLevel] = {
    "stone": TechLevel.STONE_AGE,
    "prehistoric": TechLevel.STONE_AGE,
    "bronze": TechLevel.BRONZE_AGE,
    "iron": TechLevel.IRON_AGE,
    "roman": TechLevel.IRON_AGE,
    "ancient": TechLevel.IRON_AGE,
    "medieval": TechLevel.MEDIEVAL,
    "middle age": TechLevel.MEDIEVAL,
    "dark age": TechLevel.MEDIEVAL,
    "renaissance": TechLevel.RENAISSANCE,
    "early modern": TechLevel.RENAISSANCE,
    "industrial": TechLevel.INDUSTRIAL,
    "victorian": TechLevel.INDUSTRIAL,
    "modern": TechLevel.MODERN,
    "contemporary": TechLevel.MODERN,
    "future": TechLevel.FUTURE,
    "futuristic": TechLevel.FUTURE,
    "sci-fi": TechLevel.FUTURE,
}

_IDEOLOGY_KEYWORDS: Dict[str, Ideology] = {
    "tribe": Ideology.TRIBAL,
    "tribal": Ideology.TRIBAL,
    "clan": Ideology.TRIBAL,
    "farm": Ideology.AGRARIAN,
    "agricultural": Ideology.AGRARIAN,
    "agrarian": Ideology.AGRARIAN,
    "war": Ideology.MILITARIST,
    "military": Ideology.MILITARIST,
    "warrior": Ideology.MILITARIST,
    "conquer": Ideology.MILITARIST,
    "trade": Ideology.MERCANTILE,
    "merchant": Ideology.MERCANTILE,
    "commerce": Ideology.MERCANTILE,
    "church": Ideology.THEOCRATIC,
    "religion": Ideology.THEOCRATIC,
    "god": Ideology.THEOCRATIC,
    "nomad": Ideology.NOMADIC,
    "roam": Ideology.NOMADIC,
    "horde": Ideology.NOMADIC,
}

_SCALE_KEYWORDS: Dict[str, WorldScale] = {
    "small": WorldScale.SMALL,
    "settlement": WorldScale.MICRO,
    "village": WorldScale.MICRO,
    "city": WorldScale.SMALL,
    "city-state": WorldScale.SMALL,
    "region": WorldScale.MEDIUM,
    "province": WorldScale.MEDIUM,
    "continent": WorldScale.LARGE,
    "world": WorldScale.LARGE,
    "planet": WorldScale.MASSIVE,
    "huge": WorldScale.MASSIVE,
    "large": WorldScale.LARGE,
    "medium": WorldScale.MEDIUM,
    "massive": WorldScale.MASSIVE,
}


def _match_keyword(text: str, table: Dict[str, Any]) -> Optional[Any]:
    text = text.lower()
    # Try multi-word matches first (longer keys first)
    for key in sorted(table.keys(), key=len, reverse=True):
        if key in text:
            return table[key]
    return None


def _extract_number(text: str, default: int = 2) -> int:
    m = re.search(r'\b(\d+)\b', text)
    return int(m.group(1)) if m else default


# ---------------------------------------------------------------------------
# Questioner
# ---------------------------------------------------------------------------

class WorldQuestioner:
    """
    Conducts a structured Q&A to produce a WorldSpec.
    Can be used in CLI mode (interactive) or headless mode (pre-supplied answers).
    """

    QUESTIONS: List[Tuple[str, str]] = [
        ("input",        "Describe your world scenario in one sentence:"),
        ("time_period",  "Time period or tech level? (e.g. medieval, bronze age, futuristic):"),
        ("scale",        "World scale? (micro/small/medium/large/massive):"),
        ("factions",     "How many factions/civilizations? (1-8):"),
        ("faction_names","Name each faction (comma-separated):"),
        ("conflict",     "What is the core conflict? (optional):"),
    ]

    def __init__(self, headless_answers: Optional[Dict[str, str]] = None):
        self._answers: Dict[str, str] = headless_answers or {}
        self._interactive = headless_answers is None

    def ask(self, key: str, prompt: str) -> str:
        if not self._interactive and key in self._answers:
            return self._answers[key]
        if self._interactive:
            return input(f"\n{prompt} ").strip()
        return ""

    def run(self) -> WorldSpec:
        spec = WorldSpec()

        # Q1: Scenario input
        raw_input = self.ask("input", "Describe your world scenario in one sentence:")
        spec.core_conflict = raw_input

        # Derive world type from input
        wt = _match_keyword(raw_input, _WORLD_TYPE_KEYWORDS)
        spec.world_type = wt if wt is not None else WorldType.REALISTIC

        # Q2: Time period
        time_ans = self.ask("time_period", "Time period / tech level?")
        tl = _match_keyword(time_ans, _TECH_KEYWORDS)
        spec.tech_level = tl if tl is not None else TechLevel.MEDIEVAL

        # Enable fantasy resources for fantasy/mythic worlds
        spec.enable_fantasy_resources = spec.world_type in (WorldType.FANTASY, WorldType.MYTHIC)

        # Q3: Scale
        scale_ans = self.ask("scale", "World scale? (micro/small/medium/large/massive):")
        sc = _match_keyword(scale_ans, _SCALE_KEYWORDS)
        spec.scale = sc if sc is not None else WorldScale.MEDIUM

        # Q4: Faction count
        fcount_ans = self.ask("factions", "Number of factions? (1-8):")
        n_factions = max(1, min(8, _extract_number(fcount_ans, 2)))

        # Q5: Faction names
        names_ans = self.ask("faction_names", f"Name the {n_factions} factions (comma-separated):")
        faction_names = [n.strip() for n in names_ans.split(",") if n.strip()]
        while len(faction_names) < n_factions:
            faction_names.append(f"Faction_{len(faction_names) + 1}")
        faction_names = faction_names[:n_factions]

        # Q6: Core conflict (optional)
        conflict_ans = self.ask("conflict", "Core conflict (optional):")
        spec.core_conflict = conflict_ans or raw_input

        # Derive seed from name
        spec.name = re.sub(r'[^a-zA-Z0-9_-]', '_', raw_input[:40]).strip('_') or "world"
        spec.seed = int.from_bytes(
            hashlib.sha256(spec.name.encode()).digest()[:8], "big"
        )

        # Build faction specs
        w, h = spec.dimensions()
        spec.factions = self._build_factions(faction_names, raw_input, w, h)

        # Validate
        errors = spec.validate()
        if errors:
            raise ValueError(f"Invalid world spec: {errors}")

        return spec

    def _build_factions(self, names: List[str], scenario_text: str,
                        world_w: int, world_h: int) -> List[FactionSpec]:
        factions = []
        n = len(names)
        for i, name in enumerate(names):
            # Derive ideology from name/scenario keywords
            ideology = _match_keyword(name.lower(), _IDEOLOGY_KEYWORDS) or \
                       _match_keyword(scenario_text.lower(), _IDEOLOGY_KEYWORDS) or \
                       Ideology.TRIBAL

            # Spread factions across the world
            angle = (i / n) * 2 * math.pi
            spawn_x = int(world_w / 2 + (world_w * 0.35) * math.cos(angle))
            spawn_y = int(world_h / 2 + (world_h * 0.35) * math.sin(angle))
            spawn_x = max(8, min(world_w - 8, spawn_x))
            spawn_y = max(8, min(world_h - 8, spawn_y))

            gov = self._ideology_to_government(ideology)

            factions.append(FactionSpec(
                name=name,
                ideology=ideology,
                government=gov,
                spawn_x=spawn_x,
                spawn_y=spawn_y,
                initial_population=100,
                initial_treasury=200.0,
            ))

        return factions

    @staticmethod
    def _ideology_to_government(ideology: Ideology) -> Government:
        mapping = {
            Ideology.TRIBAL:     Government.CHIEFTAINCY,
            Ideology.AGRARIAN:   Government.MONARCHY,
            Ideology.MILITARIST: Government.MONARCHY,
            Ideology.MERCANTILE: Government.OLIGARCHY,
            Ideology.THEOCRATIC: Government.THEOCRACY,
            Ideology.NOMADIC:    Government.CHIEFTAINCY,
        }
        return mapping.get(ideology, Government.CHIEFTAINCY)
