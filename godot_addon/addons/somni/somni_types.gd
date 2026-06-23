##
## SomniTypes — lightweight typed wrappers around the raw Dictionary data
## returned by the SOMNI REST API.
##
## Usage:
##   var world := SomniWorld.from_dict(Somni.get_world_json())
##   print(world.factions[0].name)
##

extends RefCounted

# ---------------------------------------------------------------------------
# SomniRegion
# ---------------------------------------------------------------------------

class SomniRegion:
	var id: int = 0
	var biome: int = 0
	var npc_count: int = 0
	var controlling_faction: int = -1
	var resources: Array = []   # Array[float], index = resource type

	static func from_dict(d: Dictionary) -> SomniRegion:
		var r := SomniRegion.new()
		r.id                  = int(d.get("id", 0))
		r.biome               = int(d.get("biome", 0))
		r.npc_count           = int(d.get("npc_count", 0))
		r.controlling_faction = int(d.get("controlling_faction", -1))
		r.resources           = d.get("resources", [])
		return r

	func resource(type: int) -> float:
		if type < 0 or type >= resources.size():
			return 0.0
		return float(resources[type])

	func to_dict() -> Dictionary:
		return {
			"id": id, "biome": biome, "npc_count": npc_count,
			"controlling_faction": controlling_faction, "resources": resources,
		}

# ---------------------------------------------------------------------------
# SomniFaction
# ---------------------------------------------------------------------------

class SomniFaction:
	var id: int = 0
	var name: String = ""
	var population: float = 0.0
	var food: float = 0.0
	var gold: float = 0.0
	var army: float = 0.0
	var territory: int = 0     # number of regions controlled
	var at_war_with: Array = []   # Array[int] — faction ids

	static func from_dict(d: Dictionary) -> SomniFaction:
		var f := SomniFaction.new()
		f.id          = int(d.get("id", 0))
		f.name        = str(d.get("name", ""))
		f.population  = float(d.get("population", 0.0))
		f.food        = float(d.get("food", 0.0))
		f.gold        = float(d.get("gold", 0.0))
		f.army        = float(d.get("army", 0.0))
		f.territory   = int(d.get("territory", 0))
		f.at_war_with = d.get("at_war_with", [])
		return f

	func is_at_war() -> bool:
		return not at_war_with.is_empty()

	func to_dict() -> Dictionary:
		return {
			"id": id, "name": name, "population": population,
			"food": food, "gold": gold, "army": army,
			"territory": territory, "at_war_with": at_war_with,
		}

# ---------------------------------------------------------------------------
# SomniWorld
# ---------------------------------------------------------------------------

class SomniWorld:
	var tick: int = 0
	var year: int = 1
	var season: String = "spring"
	var regions: Array = []     # Array[SomniRegion]
	var factions: Array = []    # Array[SomniFaction]
	var active_wars: Array = [] # Array[Dictionary] { attacker, defender, region }

	static func from_dict(d: Dictionary) -> SomniWorld:
		var w := SomniWorld.new()
		w.tick        = int(d.get("tick", 0))
		w.year        = int(d.get("year", 1))
		w.season      = str(d.get("season", "spring"))
		w.active_wars = d.get("active_wars", [])

		for rd in d.get("regions", []):
			w.regions.append(SomniRegion.from_dict(rd))

		for fd in d.get("factions", []):
			w.factions.append(SomniFaction.from_dict(fd))

		return w

	func get_region(id: int) -> SomniRegion:
		for r in regions:
			if r.id == id:
				return r
		return null

	func get_faction(id: int) -> SomniFaction:
		for f in factions:
			if f.id == id:
				return f
		return null

	func total_population() -> float:
		var total := 0.0
		for f in factions:
			total += f.population
		return total
