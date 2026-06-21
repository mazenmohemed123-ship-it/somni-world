"""
Preset world library — ready-to-run scenarios for the launcher page.

Each preset is a plain dict that maps directly onto CreateWorldRequest, so the
launcher can POST it to /world/create unchanged. Tags power the search box.
No LLM involved: these are curated, structured world definitions.
"""
from __future__ import annotations

from typing import Dict, List

PRESETS: List[Dict] = [
    # ---- Realistic / historical -----------------------------------------
    {
        "id": "viking_age",
        "name": "Viking Age",
        "icon": "🛶",
        "description": "Norse clans raid, trade and settle cold coastal fjords.",
        "scenario": "viking norse clans raiding and trading across cold fjords",
        "scale": "medium", "tech_level": "iron_age", "factions": 4,
        "faction_names": ["Northmen", "Danelaw", "Rus Traders", "Jarls of Vestland"],
        "tags": ["realistic", "historical", "war", "trade", "cold", "sea"],
    },
    {
        "id": "bronze_collapse",
        "name": "Bronze Age Collapse",
        "icon": "🏺",
        "description": "Mediterranean kingdoms strain under drought and sea raiders.",
        "scenario": "bronze age mediterranean kingdoms collapsing under drought and invasion",
        "scale": "large", "tech_level": "bronze_age", "factions": 5,
        "faction_names": ["Mycenae", "Hatti", "Egypt", "Sea Peoples", "Ugarit"],
        "tags": ["realistic", "historical", "collapse", "drought", "war"],
    },
    {
        "id": "silk_road",
        "name": "Silk Road Empires",
        "icon": "🐫",
        "description": "Caravan empires compete over desert trade routes and oases.",
        "scenario": "silk road desert caravan empires competing over trade routes",
        "scale": "large", "tech_level": "medieval", "factions": 4,
        "faction_names": ["Samarkand", "Tang Frontier", "Persian Satraps", "Steppe Khanate"],
        "tags": ["realistic", "trade", "desert", "economy", "historical"],
    },
    {
        "id": "feudal_realms",
        "name": "Feudal Realms",
        "icon": "🏰",
        "description": "Medieval kingdoms vie for farmland, faith and crowns.",
        "scenario": "medieval feudal kingdoms warring over farmland and succession",
        "scale": "medium", "tech_level": "medieval", "factions": 3,
        "faction_names": ["House Aldoran", "Kingdom of Veil", "The Free Cities"],
        "tags": ["realistic", "medieval", "war", "politics"],
    },
    {
        "id": "island_settlers",
        "name": "Island Settlers",
        "icon": "🏝️",
        "description": "Small tribes colonize a fresh archipelago — survival first.",
        "scenario": "small tribes settling and surviving on a remote tropical archipelago",
        "scale": "small", "tech_level": "stone_age", "factions": 2,
        "faction_names": ["Tide Clan", "Palm Folk"],
        "tags": ["realistic", "survival", "tropical", "small", "peaceful"],
    },
    {
        "id": "industrial_dawn",
        "name": "Industrial Dawn",
        "icon": "🏭",
        "description": "Rival nations race to industrialize coal and rail.",
        "scenario": "industrial revolution rival nations racing over coal steel and railways",
        "scale": "large", "tech_level": "industrial", "factions": 4,
        "faction_names": ["Albion", "Ruhr Union", "Gallia", "New Columbia"],
        "tags": ["realistic", "economy", "industrial", "modern"],
    },

    # ---- Fantasy ---------------------------------------------------------
    {
        "id": "dragon_reach",
        "name": "Dragonreach",
        "icon": "🐉",
        "description": "Kingdoms shelter beneath mountains where dragons nest.",
        "scenario": "fantasy kingdoms surviving near mountains inhabited by dragons",
        "scale": "medium", "tech_level": "medieval", "factions": 3,
        "faction_names": ["Emberhold", "Frostpeak", "Ashen Order"],
        "tags": ["fantasy", "dragons", "magic", "war", "mountains"],
    },
    {
        "id": "elven_wars",
        "name": "Elven Succession Wars",
        "icon": "🏹",
        "description": "Ancient forest houses fracture over a vacant throne.",
        "scenario": "fantasy elven forest houses at war over a vacant high throne",
        "scale": "medium", "tech_level": "medieval", "factions": 4,
        "faction_names": ["House Sylvaris", "Moonvale", "Thornwood", "The Exiles"],
        "tags": ["fantasy", "elves", "forest", "politics", "war"],
    },
    {
        "id": "undead_tide",
        "name": "The Undead Tide",
        "icon": "💀",
        "description": "The living unite as a necropolis spreads across the land.",
        "scenario": "fantasy living kingdoms uniting against a spreading undead necropolis",
        "scale": "large", "tech_level": "medieval", "factions": 4,
        "faction_names": ["The Living Pact", "Holy Conclave", "Free Mercenaries", "Necropolis"],
        "tags": ["fantasy", "undead", "survival", "war", "dark"],
    },

    # ---- Sci-fi ----------------------------------------------------------
    {
        "id": "colony_drift",
        "name": "Colony Drift",
        "icon": "🚀",
        "description": "Crash-landed colonists splinter into rival settlements.",
        "scenario": "sci-fi crash landed colonists splintering into rival settlements on an alien world",
        "scale": "medium", "tech_level": "future", "factions": 3,
        "faction_names": ["Command Remnant", "The Drifters", "Hab-Collective"],
        "tags": ["sci-fi", "survival", "future", "alien"],
    },
    {
        "id": "mars_states",
        "name": "Martian City-States",
        "icon": "🔴",
        "description": "Dome cities fight over water ice and atmosphere rights.",
        "scenario": "sci-fi martian dome city-states competing over water ice and air",
        "scale": "large", "tech_level": "future", "factions": 5,
        "faction_names": ["Olympus", "Valles Union", "Red Syndicate", "Polaris", "Free Domes"],
        "tags": ["sci-fi", "mars", "economy", "future", "war"],
    },
    {
        "id": "ai_uprising",
        "name": "Machine Frontier",
        "icon": "🤖",
        "description": "Human enclaves and autonomous machine clusters share a ruined world.",
        "scenario": "sci-fi human enclaves and autonomous machine clusters dividing a ruined earth",
        "scale": "large", "tech_level": "future", "factions": 4,
        "faction_names": ["Human Remnant", "The Foundry", "Wild Networks", "Scavenger Guild"],
        "tags": ["sci-fi", "ai", "post-apocalyptic", "future"],
    },
]


def get_presets() -> List[Dict]:
    return PRESETS


def get_preset(preset_id: str) -> Dict:
    for p in PRESETS:
        if p["id"] == preset_id:
            return p
    raise KeyError(preset_id)
