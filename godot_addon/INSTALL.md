# SOMNI Client — Godot 4 Plugin

Connects your Godot 4 game to the SOMNI living-world simulation backend via
WebSocket (real-time events) and REST (commands + world state queries).

---

## Requirements

| Requirement | Version |
|---|---|
| Godot | 4.1 or later |
| Python | 3.10+ (for the SOMNI backend) |
| OS | Windows / macOS / Linux |

---

## Step 1 — Start the SOMNI Python server

Open a terminal in the project root and run:

```bash
pip install fastapi uvicorn pydantic websockets
python somni.py
```

The server starts on `http://localhost:8080`. Keep it running while you work in Godot.

---

## Step 2 — Copy the addon into your Godot project

Copy the `addons/somni/` folder into your project's `addons/` directory:

```
your_godot_project/
  addons/
    somni/
      plugin.cfg
      plugin.gd
      somni_client.gd
      somni_types.gd
```

---

## Step 3 — Enable the plugin

1. Open your Godot project.
2. Go to **Project > Project Settings > Plugins**.
3. Find **SOMNI Client** and click **Enable**.

This registers an autoload singleton named `Somni` that is available in every
script in your project — no `preload` needed.

---

## Step 4 — Connect and create a world

From any script:

```gdscript
func _ready() -> void:
    # Wire up signals first.
    Somni.tick_received.connect(_on_tick)
    Somni.faction_war.connect(_on_war)
    Somni.world_loaded.connect(_on_world_loaded)

    # Open WebSocket connection.
    Somni.connect_to_server()

    # Create (or load) a world.
    Somni.create_world({
        "scenario": "Bronze Age city-states competing for the Nile delta",
        "scale":    "medium",
        "factions": 4,
        "tech_level": "bronze_age",
    })

func _on_tick(tick: int, sim_time: float) -> void:
    print("tick ", tick)

func _on_war(attacker: int, defender: int, region: int) -> void:
    print("WAR: %d attacks %d in region %d" % [attacker, defender, region])

func _on_world_loaded(world: Dictionary) -> void:
    print("World loaded with %d factions" % world.get("factions", []).size())
```

---

## Step 5 — Issue player commands

```gdscript
# Diplomacy
Somni.declare_war(0, 1)
Somni.make_peace(0, 1)

# Military
Somni.recruit_army(0, 100.0)   # faction_id, amount

# Economy
Somni.send_food(0, 500.0)       # faction_id, amount
Somni.spawn_settlers(1, 200)    # faction_id, count

# Resources & disasters
Somni.add_resource(3, 0, 300.0)      # region_id, resource_type(0=food), amount
Somni.trigger_disaster(3, 0.8)       # region_id, magnitude 0..1

# Simulation control
Somni.pause_sim()
Somni.resume_sim()
Somni.save_sim()                     # saves a snapshot to worlds/snap_<tick>.somni
```

---

## Step 6 — Use typed helpers (optional)

Import `SomniTypes` for cleaner access:

```gdscript
const T = preload("res://addons/somni/somni_types.gd")

func _on_world_loaded(raw: Dictionary) -> void:
    var world := T.SomniWorld.from_dict(raw)
    for faction in world.factions:
        print(faction.name, "  pop=", faction.population)
    var region_0 := world.get_region(0)
    print("Region 0 food: ", region_0.resource(0))
```

---

## Full signal reference

| Signal | Arguments | When fired |
|---|---|---|
| `connection_changed` | `connected: bool` | WebSocket opens / closes |
| `world_loaded` | `world: Dictionary` | Full world JSON received |
| `tick_received` | `tick: int, sim_time: float` | ~2× per real-time second |
| `faction_war` | `attacker, defender, region: int` | War starts between factions |
| `trade_completed` | `faction_a, faction_b, resource: int, amount: float` | Trade route completes |
| `command_result` | `result: Dictionary` | Any player command succeeds |
| `error_occurred` | `message: String` | HTTP error or timeout |

---

## Demo scene

Open `demo/demo.tscn` in your Godot project for a working minimal example with
buttons for every command and live faction/tick display.

---

## Changing the server address

If the SOMNI server runs on a different machine:

```gdscript
# In _ready(), before connect_to_server():
Somni.host = "192.168.1.42"
Somni.port = 8080
Somni.connect_to_server()
```

---

## Architecture overview

```
Godot 4 game
  |
  |  WebSocket  ws://localhost:8080/ws   (real-time: ticks, wars, trade)
  |  HTTP REST  http://localhost:8080/   (commands, world JSON, factions)
  |
SOMNI Python backend  (somni.py / orchestrator/)
  |
  |-- PyKernel  (pure-Python deterministic simulation)
  |   factions, economy, wars, population, resources
  |
  \-- C++ kernel (optional, build with CMake for NPC behavior trees + scale)
```

The Godot plugin is purely a network bridge — it never simulates anything
itself. All logic lives in the SOMNI kernel running as a local server.
