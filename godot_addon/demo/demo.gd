##
## SomniDemo — minimal scene showing how to wire up the SOMNI client.
##
## Attach this script to a Node in your scene.
## Make sure the "SOMNI Client" plugin is enabled in Project Settings > Plugins.
##
## The autoload "Somni" is then available everywhere in your project.
##

extends Node

# References to UI nodes (adjust to your scene tree).
@onready var label_status   : Label  = $UI/LabelStatus
@onready var label_tick     : Label  = $UI/LabelTick
@onready var label_factions : Label  = $UI/LabelFactions
@onready var btn_war        : Button = $UI/BtnDeclareWar
@onready var btn_peace      : Button = $UI/BtnMakePeace
@onready var btn_army       : Button = $UI/BtnRecruitArmy
@onready var btn_food       : Button = $UI/BtnSendFood
@onready var btn_disaster   : Button = $UI/BtnDisaster
@onready var btn_pause      : Button = $UI/BtnPause

var _paused := false

# ---------------------------------------------------------------------------
# Lifecycle
# ---------------------------------------------------------------------------

func _ready() -> void:
	# Wire up Somni signals.
	Somni.connection_changed.connect(_on_connection_changed)
	Somni.world_loaded.connect(_on_world_loaded)
	Somni.tick_received.connect(_on_tick)
	Somni.faction_war.connect(_on_faction_war)
	Somni.command_result.connect(_on_command_result)
	Somni.error_occurred.connect(_on_error)

	# Wire up buttons.
	btn_war.pressed.connect(_on_declare_war)
	btn_peace.pressed.connect(_on_make_peace)
	btn_army.pressed.connect(_on_recruit_army)
	btn_food.pressed.connect(_on_send_food)
	btn_disaster.pressed.connect(_on_disaster)
	btn_pause.pressed.connect(_on_toggle_pause)

	# Connect and create a world.
	Somni.connect_to_server()

	label_status.text = "Connecting to SOMNI..."

	# Create a world after a short delay to let the WebSocket handshake complete.
	await get_tree().create_timer(1.0).timeout
	_create_world()

# ---------------------------------------------------------------------------
# World creation
# ---------------------------------------------------------------------------

func _create_world() -> void:
	label_status.text = "Creating world..."
	Somni.create_world({
		"scenario": "A medieval kingdom torn by rival factions",
		"scale": "medium",
		"factions": 3,
		"tech_level": "medieval",
		"ticks_per_second": 20,
		"headless": true,
	}, func(d):
		if d.is_empty():
			label_status.text = "World creation failed — is the server running?"
		else:
			label_status.text = "World: %s  |  Backend: %s" % [
				d.get("world_name", "?"),
				d.get("backend", "?"),
			]
	)

# ---------------------------------------------------------------------------
# Signal handlers
# ---------------------------------------------------------------------------

func _on_connection_changed(connected: bool) -> void:
	if connected:
		label_status.text = "Connected to SOMNI"
	else:
		label_status.text = "Disconnected — retrying..."

func _on_world_loaded(world: Dictionary) -> void:
	var factions = world.get("factions", [])
	var lines := PackedStringArray()
	for f in factions:
		lines.append("%s  pop=%.0f  army=%.0f" % [
			f.get("name", "?"),
			float(f.get("population", 0)),
			float(f.get("army", 0)),
		])
	label_factions.text = "\n".join(lines)

func _on_tick(tick: int, _sim_time: float) -> void:
	label_tick.text = "Tick: %d" % tick
	# Refresh faction data every ~5 seconds (100 ticks at 20 tps).
	if tick % 100 == 0:
		Somni.get_world_json()

func _on_faction_war(attacker: int, defender: int, region: int) -> void:
	print("[SOMNI] WAR: faction %d attacked faction %d in region %d" % [attacker, defender, region])
	label_status.text = "WAR: faction %d vs %d (region %d)" % [attacker, defender, region]

func _on_command_result(result: Dictionary) -> void:
	print("[SOMNI] Command result: ", result)

func _on_error(message: String) -> void:
	push_warning("[SOMNI] Error: " + message)

# ---------------------------------------------------------------------------
# Button handlers
# ---------------------------------------------------------------------------

func _on_declare_war() -> void:
	Somni.declare_war(0, 1)

func _on_make_peace() -> void:
	Somni.make_peace(0, 1)

func _on_recruit_army() -> void:
	Somni.recruit_army(0, 50.0)

func _on_send_food() -> void:
	Somni.send_food(0, 200.0)

func _on_disaster() -> void:
	Somni.trigger_disaster(0, 0.7)

func _on_toggle_pause() -> void:
	_paused = not _paused
	if _paused:
		Somni.pause_sim()
		btn_pause.text = "Resume"
	else:
		Somni.resume_sim()
		btn_pause.text = "Pause"
