"""
Preset world library — ready-to-run scenarios for the launcher page.

Each preset is a plain dict that maps directly onto CreateWorldRequest, so the
launcher can POST it to /world/create unchanged. Tags power the search box.
No LLM involved: these are curated, structured world definitions.
"""
from __future__ import annotations

from typing import Dict, List

PRESETS: List[Dict] = [

    # =========================================================
    # HISTORICAL — ANCIENT
    # =========================================================
    {
        "id": "mesopotamia",
        "name": "Mesopotamia",
        "description": "City-states of Babylon, Assyria and Sumer fight over the Fertile Crescent.",
        "scenario": "ancient mesopotamian city-states competing for fertile farmland and rivers",
        "scale": "large", "tech_level": "bronze_age", "factions": 5,
        "faction_names": ["Babylon", "Assyria", "Sumer", "Akkad", "Elam"],
        "tags": ["historical", "ancient", "war", "economy", "rivers"],
    },
    {
        "id": "greek_city_states",
        "name": "Greek City-States",
        "description": "Athens and Sparta drag all of Greece into the Peloponnesian War.",
        "scenario": "ancient greek city-states at war athens sparta peloponnesian conflict",
        "scale": "medium", "tech_level": "iron_age", "factions": 4,
        "faction_names": ["Athens", "Sparta", "Corinth", "Thebes"],
        "tags": ["historical", "ancient", "war", "politics", "sea"],
    },
    {
        "id": "roman_civil_war",
        "name": "Roman Civil Wars",
        "description": "Roman generals march on the capital as the Republic tears itself apart.",
        "scenario": "roman republic civil war generals marching on rome caesarian conflict",
        "scale": "large", "tech_level": "iron_age", "factions": 4,
        "faction_names": ["Caesarians", "Pompeians", "Senate Guard", "Eastern Legions"],
        "tags": ["historical", "ancient", "war", "politics", "rome"],
    },
    {
        "id": "bronze_collapse",
        "name": "Bronze Age Collapse",
        "description": "Mediterranean kingdoms strain under drought, famine and sea raiders.",
        "scenario": "bronze age mediterranean kingdoms collapsing under drought and invasion",
        "scale": "large", "tech_level": "bronze_age", "factions": 5,
        "faction_names": ["Mycenae", "Hatti", "Egypt", "Sea Peoples", "Ugarit"],
        "tags": ["historical", "ancient", "collapse", "drought", "war"],
    },
    {
        "id": "three_kingdoms",
        "name": "Three Kingdoms of China",
        "description": "Wei, Shu and Wu battle for the soul of a fractured Han dynasty.",
        "scenario": "three kingdoms china wei shu wu battling for reunification after han collapse",
        "scale": "large", "tech_level": "iron_age", "factions": 3,
        "faction_names": ["Wei", "Shu Han", "Wu"],
        "tags": ["historical", "ancient", "war", "politics", "china"],
    },
    {
        "id": "aztec_rivalry",
        "name": "Aztec Triple Alliance",
        "description": "City-states of Mesoamerica war for tribute, land and captives.",
        "scenario": "aztec mesoamerican city-states competing for tribute land and ritual sacrifices",
        "scale": "medium", "tech_level": "stone_age", "factions": 4,
        "faction_names": ["Tenochtitlan", "Texcoco", "Tlaxcala", "Tlatelolco"],
        "tags": ["historical", "ancient", "war", "jungle", "trade"],
    },

    # =========================================================
    # HISTORICAL — MEDIEVAL
    # =========================================================
    {
        "id": "viking_age",
        "name": "Viking Age",
        "description": "Norse clans raid, trade and settle cold coastal fjords.",
        "scenario": "viking norse clans raiding and trading across cold fjords",
        "scale": "medium", "tech_level": "iron_age", "factions": 4,
        "faction_names": ["Northmen", "Danelaw", "Rus Traders", "Jarls of Vestland"],
        "tags": ["historical", "medieval", "war", "trade", "cold", "sea"],
    },
    {
        "id": "mongol_conquest",
        "name": "Mongol Conquest",
        "description": "Steppe horsemen sweep across Eurasia, reshaping every kingdom in their path.",
        "scenario": "mongol cavalry empire sweeping across eurasian steppe kingdoms and cities",
        "scale": "large", "tech_level": "medieval", "factions": 5,
        "faction_names": ["Golden Horde", "Kievan Rus", "Song Dynasty", "Persian Sultanate", "Hungarian Kingdom"],
        "tags": ["historical", "medieval", "war", "conquest", "steppe"],
    },
    {
        "id": "crusader_states",
        "name": "Crusader States",
        "description": "Christian kingdoms hold Jerusalem as Islamic powers encircle the Holy Land.",
        "scenario": "crusader kingdoms holding jerusalem as islamic forces besiege the holy land",
        "scale": "medium", "tech_level": "medieval", "factions": 4,
        "faction_names": ["Jerusalem", "Antioch", "Saladin's Caliphate", "Byzantine Empire"],
        "tags": ["historical", "medieval", "war", "religion", "desert"],
    },
    {
        "id": "feudal_realms",
        "name": "Feudal Realms",
        "description": "Medieval kingdoms vie for farmland, faith and crowns.",
        "scenario": "medieval feudal kingdoms warring over farmland and royal succession",
        "scale": "medium", "tech_level": "medieval", "factions": 3,
        "faction_names": ["House Aldoran", "Kingdom of Veil", "The Free Cities"],
        "tags": ["historical", "medieval", "war", "politics", "farming"],
    },
    {
        "id": "hundred_years_war",
        "name": "Hundred Years War",
        "description": "France and England bleed each other dry over a contested throne.",
        "scenario": "hundred years war england france battling over throne territory and trade",
        "scale": "medium", "tech_level": "medieval", "factions": 3,
        "faction_names": ["England", "France", "Burgundy"],
        "tags": ["historical", "medieval", "war", "politics", "france"],
    },
    {
        "id": "sengoku_japan",
        "name": "Sengoku Japan",
        "description": "Warring clans of feudal Japan fight to unify the islands under one banner.",
        "scenario": "sengoku era japanese clans fighting to unify the islands under a shogunate",
        "scale": "medium", "tech_level": "medieval", "factions": 5,
        "faction_names": ["Oda", "Takeda", "Uesugi", "Mori", "Shimazu"],
        "tags": ["historical", "medieval", "war", "japan", "islands"],
    },
    {
        "id": "silk_road",
        "name": "Silk Road Empires",
        "description": "Caravan empires compete over desert trade routes, oases and caravanserais.",
        "scenario": "silk road desert caravan empires competing over trade routes and oases",
        "scale": "large", "tech_level": "medieval", "factions": 4,
        "faction_names": ["Samarkand", "Tang Frontier", "Persian Satraps", "Steppe Khanate"],
        "tags": ["historical", "medieval", "trade", "desert", "economy"],
    },
    {
        "id": "ottoman_expansion",
        "name": "Ottoman Expansion",
        "description": "The rising Ottoman state absorbs Anatolia and pushes into Europe and Persia.",
        "scenario": "ottoman empire expanding across anatolia balkans and middle east",
        "scale": "large", "tech_level": "medieval", "factions": 4,
        "faction_names": ["Ottomans", "Byzantine Remnant", "Safavid Persia", "Venice"],
        "tags": ["historical", "medieval", "conquest", "trade", "sea"],
    },

    # =========================================================
    # HISTORICAL — MODERN
    # =========================================================
    {
        "id": "industrial_dawn",
        "name": "Industrial Dawn",
        "description": "Rival nations race to industrialize coal, steel and railways.",
        "scenario": "industrial revolution rival nations racing over coal steel and railways",
        "scale": "large", "tech_level": "industrial", "factions": 4,
        "faction_names": ["Albion", "Ruhr Union", "Gallia", "New Columbia"],
        "tags": ["historical", "industrial", "economy", "war", "modern"],
    },
    {
        "id": "colonial_africa",
        "name": "Colonial Scramble",
        "description": "European powers carve up a continent, ignoring kingdoms that stood for centuries.",
        "scenario": "european colonial powers dividing africa while indigenous kingdoms resist",
        "scale": "large", "tech_level": "industrial", "factions": 5,
        "faction_names": ["Britannia", "La France", "Ashanti", "Zulu Nation", "Ethiopian Empire"],
        "tags": ["historical", "industrial", "war", "colonial", "africa"],
    },
    {
        "id": "cold_war_proxy",
        "name": "Cold War Proxy",
        "description": "Two superpowers fight through client states across a contested region.",
        "scenario": "cold war era proxy conflict superpower client states fighting for regional dominance",
        "scale": "medium", "tech_level": "modern", "factions": 4,
        "faction_names": ["Western Bloc", "Eastern Bloc", "Non-Aligned Movement", "Regional Insurgents"],
        "tags": ["historical", "modern", "war", "politics", "cold-war"],
    },

    # =========================================================
    # SURVIVAL & ECOLOGY
    # =========================================================
    {
        "id": "island_settlers",
        "name": "Island Settlers",
        "description": "Small tribes colonize a fresh archipelago — survival before politics.",
        "scenario": "small tribes settling and surviving on a remote tropical archipelago",
        "scale": "small", "tech_level": "stone_age", "factions": 2,
        "faction_names": ["Tide Clan", "Palm Folk"],
        "tags": ["survival", "tropical", "peaceful", "small", "ecology"],
    },
    {
        "id": "ice_age",
        "name": "Ice Age Migration",
        "description": "Hunter-gatherer bands follow megafauna herds across a freezing continent.",
        "scenario": "ice age hunter-gatherer bands migrating and competing for megafauna herds",
        "scale": "large", "tech_level": "stone_age", "factions": 4,
        "faction_names": ["Mammoth Hunters", "Cave Bear Clan", "River Folk", "Coast Wanderers"],
        "tags": ["survival", "ancient", "cold", "ecology", "prehistoric"],
    },
    {
        "id": "volcanic_island",
        "name": "Volcanic Archipelago",
        "description": "Settlers race to claim fertile slopes before the volcanoes erupt again.",
        "scenario": "settlers colonizing volcanic islands racing to farm fertile slopes before eruptions",
        "scale": "small", "tech_level": "stone_age", "factions": 3,
        "faction_names": ["Highland Kin", "Shore Runners", "Deep Miners"],
        "tags": ["survival", "islands", "ecology", "small", "disaster"],
    },
    {
        "id": "pandemic_survivors",
        "name": "After the Plague",
        "description": "Pandemic survivors rebuild city networks from abandoned ruins.",
        "scenario": "pandemic survivors rebuilding civilization from ruins of a collapsed modern city",
        "scale": "medium", "tech_level": "modern", "factions": 3,
        "faction_names": ["City Builders", "Nomadic Scavengers", "Quarantine Communes"],
        "tags": ["survival", "post-apocalyptic", "modern", "ecology", "collapse"],
    },

    # =========================================================
    # FANTASY
    # =========================================================
    {
        "id": "dragon_reach",
        "name": "Dragonreach",
        "description": "Kingdoms shelter beneath mountains where dragons nest and demand tribute.",
        "scenario": "fantasy kingdoms surviving near mountains inhabited by dragons demanding tribute",
        "scale": "medium", "tech_level": "medieval", "factions": 3,
        "faction_names": ["Emberhold", "Frostpeak", "Ashen Order"],
        "tags": ["fantasy", "dragons", "magic", "war", "mountains"],
    },
    {
        "id": "elven_wars",
        "name": "Elven Succession Wars",
        "description": "Ancient forest houses fracture over a vacant high throne.",
        "scenario": "fantasy elven forest houses at war over a vacant high throne",
        "scale": "medium", "tech_level": "medieval", "factions": 4,
        "faction_names": ["House Sylvaris", "Moonvale", "Thornwood", "The Exiles"],
        "tags": ["fantasy", "elves", "forest", "politics", "war"],
    },
    {
        "id": "undead_tide",
        "name": "The Undead Tide",
        "description": "The living unite in desperate alliance as a necropolis spreads across the land.",
        "scenario": "fantasy living kingdoms uniting against a spreading undead necropolis",
        "scale": "large", "tech_level": "medieval", "factions": 4,
        "faction_names": ["The Living Pact", "Holy Conclave", "Free Mercenaries", "Necropolis"],
        "tags": ["fantasy", "undead", "survival", "war", "dark"],
    },
    {
        "id": "pirate_islands",
        "name": "Pirate Archipelago",
        "description": "Pirate fleets and merchant guilds wrestle over island trade lanes.",
        "scenario": "pirate fleets and merchant guilds competing over island sea trade routes",
        "scale": "medium", "tech_level": "medieval", "factions": 4,
        "faction_names": ["The Crimson Fleet", "Merchant Guild", "Royal Navy", "Hidden Coves"],
        "tags": ["fantasy", "sea", "trade", "war", "islands"],
    },
    {
        "id": "dwarven_holds",
        "name": "Dwarven Hold Wars",
        "description": "Underground clans fight over precious ore veins in a vast cave network.",
        "scenario": "dwarven underground clans fighting over ore veins and cavern fortresses",
        "scale": "medium", "tech_level": "medieval", "factions": 4,
        "faction_names": ["Ironmaw Hold", "Deepvein Clan", "Stonebrow", "Exile Tunnelers"],
        "tags": ["fantasy", "underground", "economy", "war", "mining"],
    },
    {
        "id": "desert_nomads",
        "name": "Desert Warlords",
        "description": "Nomad war-bands fight for oases while a dying empire crumbles around them.",
        "scenario": "desert nomad warlords battling over oases as a once-great empire falls apart",
        "scale": "medium", "tech_level": "iron_age", "factions": 4,
        "faction_names": ["Sand Riders", "Oasis Wardens", "The Fallen Empire", "Merchant Caravans"],
        "tags": ["fantasy", "desert", "survival", "war", "trade"],
    },
    {
        "id": "demon_breach",
        "name": "The Demon Breach",
        "description": "A rift tears open, and mortal kingdoms must ally or be consumed.",
        "scenario": "fantasy demon portal opens forcing mortal kingdoms to fight together or perish",
        "scale": "large", "tech_level": "medieval", "factions": 4,
        "faction_names": ["Mortal Alliance", "Demon Vanguard", "Rift Cultists", "Planar Wanderers"],
        "tags": ["fantasy", "dark", "survival", "war", "magic"],
    },

    # =========================================================
    # SCI-FI
    # =========================================================
    {
        "id": "colony_drift",
        "name": "Colony Drift",
        "description": "Crash-landed colonists splinter into rival settlements on an alien world.",
        "scenario": "sci-fi crash landed colonists splintering into rival settlements on alien world",
        "scale": "medium", "tech_level": "future", "factions": 3,
        "faction_names": ["Command Remnant", "The Drifters", "Hab-Collective"],
        "tags": ["sci-fi", "survival", "future", "alien", "colony"],
    },
    {
        "id": "mars_states",
        "name": "Martian City-States",
        "description": "Dome cities fight over water ice, atmosphere rights and rare minerals.",
        "scenario": "sci-fi martian dome city-states competing over water ice and air rights",
        "scale": "large", "tech_level": "future", "factions": 5,
        "faction_names": ["Olympus", "Valles Union", "Red Syndicate", "Polaris", "Free Domes"],
        "tags": ["sci-fi", "mars", "economy", "future", "war"],
    },
    {
        "id": "machine_frontier",
        "name": "Machine Frontier",
        "description": "Human enclaves and autonomous machine clusters share a ruined world.",
        "scenario": "sci-fi human enclaves and autonomous machine clusters dividing a ruined earth",
        "scale": "large", "tech_level": "future", "factions": 4,
        "faction_names": ["Human Remnant", "The Foundry", "Wild Networks", "Scavenger Guild"],
        "tags": ["sci-fi", "ai", "post-apocalyptic", "future", "survival"],
    },
    {
        "id": "deep_sea_colony",
        "name": "Deep Sea Colonies",
        "description": "Underwater pressure-domes compete for hydrothermal vent resources.",
        "scenario": "sci-fi underwater dome colonies competing for hydrothermal vents and deep sea resources",
        "scale": "medium", "tech_level": "future", "factions": 3,
        "faction_names": ["Abyssal Corp", "Vent Commune", "Surface Control"],
        "tags": ["sci-fi", "ocean", "economy", "future", "survival"],
    },
    {
        "id": "generation_ship",
        "name": "Generation Ship",
        "description": "Factions inside a 500-year voyage argue over destination, resources and power.",
        "scenario": "sci-fi generation ship factions arguing over direction resources and governance after centuries of travel",
        "scale": "small", "tech_level": "future", "factions": 4,
        "faction_names": ["Navigation Council", "Labor Decks", "Tech Priests", "The Sleepers"],
        "tags": ["sci-fi", "future", "politics", "survival", "small"],
    },
    {
        "id": "corporate_megacities",
        "name": "Corporate Megacities",
        "description": "Megacorporations govern city-states as governments collapse.",
        "scenario": "corporate megacities replacing governments as corporations wage economic and physical war",
        "scale": "large", "tech_level": "future", "factions": 4,
        "faction_names": ["OmniCorp", "Bio-Helix", "The Syndicate", "Free Net"],
        "tags": ["sci-fi", "modern", "economy", "war", "dystopia"],
    },
    {
        "id": "nuclear_wasteland",
        "name": "Nuclear Wasteland",
        "description": "Vault survivors emerge to rebuild in a radioactive world contested by raiders.",
        "scenario": "post nuclear war vault survivors rebuilding while raider factions contest the wasteland",
        "scale": "large", "tech_level": "future", "factions": 4,
        "faction_names": ["Vault Keepers", "Desert Raiders", "Mutant Tribes", "Tech Salvagers"],
        "tags": ["sci-fi", "post-apocalyptic", "survival", "war", "desert"],
    },
    {
        "id": "alien_contact",
        "name": "First Contact",
        "description": "An alien signal reaches Earth. Nations race to respond — or prepare for war.",
        "scenario": "first alien contact nations racing to communicate or prepare defenses against unknown species",
        "scale": "large", "tech_level": "future", "factions": 4,
        "faction_names": ["Contact Initiative", "Military Coalition", "Independent States", "The Signal"],
        "tags": ["sci-fi", "alien", "politics", "future", "war"],
    },
]


def get_presets() -> List[Dict]:
    return PRESETS


def get_preset(preset_id: str) -> Dict:
    for p in PRESETS:
        if p["id"] == preset_id:
            return p
    raise KeyError(preset_id)
