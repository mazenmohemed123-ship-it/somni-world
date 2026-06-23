##
## SomniClient — autoload singleton for Godot 4.
##
## Connects to the SOMNI Python backend via:
##   WebSocket  ws://host:port/ws        (real-time event stream)
##   HTTP REST  http://host:port/...     (commands + world state)
##
## Usage (from any node or script):
##   Somni.connect_to_server()
##   Somni.create_world({ "scenario": "Viking age", "factions": 3 })
##   Somni.declare_war(0, 1)
##   Somni.faction_war.connect(_on_war)
##

extends Node

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

## Server host — change if your Python server is on a different machine.
@export var host: String = "localhost"
@export var port: int = 8080

## How often (seconds) to attempt reconnect when disconnected.
@export var reconnect_interval: float = 3.0

# ---------------------------------------------------------------------------
# Signals
# ---------------------------------------------------------------------------

## Emitted when the WebSocket connection state changes.
signal connection_changed(connected: bool)

## Full world JSON delivered on first WebSocket connect and via REST.
signal world_loaded(world: Dictionary)

## Simulation clock tick — emitted ~2 times per real-time second (throttled by server).
signal tick_received(tick: int, sim_time: float)

## A faction-vs-faction war just started.
signal faction_war(attacker: int, defender: int, region: int)

## A trade route completed between two factions.
signal trade_completed(faction_a: int, faction_b: int, resource: int, amount: float)

## Fired after every HTTP command call with {"status": "ok", "command": ...}.
signal command_result(result: Dictionary)

## Fired when any HTTP error or timeout occurs.
signal error_occurred(message: String)

# ---------------------------------------------------------------------------
# Internal state
# ---------------------------------------------------------------------------

var _ws: WebSocketPeer = null
var _connected: bool = false
var _reconnect_timer: float = 0.0

# HTTP request queue — one active request at a time keeps things simple.
var _http_queue: Array[Dictionary] = []   # { method, url, body, callback }
var _http_busy: bool = false
var _http: HTTPRequest = null

# ---------------------------------------------------------------------------
# Lifecycle
# ---------------------------------------------------------------------------

func _ready() -> void:
	_http = HTTPRequest.new()
	_http.timeout = 10.0
	add_child(_http)
	_http.request_completed.connect(_on_http_completed)

func _process(delta: float) -> void:
	_ws_poll(delta)

# ---------------------------------------------------------------------------
# Connection
# ---------------------------------------------------------------------------

## Open the WebSocket connection to the simulation backend.
func connect_to_server() -> void:
	if _ws != null and _ws.get_ready_state() in [
			WebSocketPeer.STATE_OPEN, WebSocketPeer.STATE_CONNECTING]:
		return
	_ws = WebSocketPeer.new()
	var url := "ws://%s:%d/ws" % [host, port]
	var err := _ws.connect_to_url(url)
	if err != OK:
		push_warning("SomniClient: WebSocket connect failed (%d)" % err)
		_ws = null

## Close the WebSocket connection.
func disconnect_from_server() -> void:
	if _ws != null:
		_ws.close()
		_ws = null
	_set_connected(false)

func is_connected_to_server() -> bool:
	return _connected

# ---------------------------------------------------------------------------
# WebSocket polling (called every _process)
# ---------------------------------------------------------------------------

func _ws_poll(delta: float) -> void:
	if _ws == null:
		_reconnect_timer += delta
		if _reconnect_timer >= reconnect_interval and _connected == false:
			_reconnect_timer = 0.0
		return

	_ws.poll()
	var state := _ws.get_ready_state()

	match state:
		WebSocketPeer.STATE_OPEN:
			if not _connected:
				_set_connected(true)
			# Drain all pending packets
			while _ws.get_available_packet_count() > 0:
				var raw := _ws.get_packet()
				var text := raw.get_string_from_utf8()
				_handle_ws_message(text)

		WebSocketPeer.STATE_CLOSED:
			if _connected:
				_set_connected(false)
			_ws = null
			_reconnect_timer = 0.0

func _set_connected(value: bool) -> void:
	if _connected != value:
		_connected = value
		connection_changed.emit(value)

func _handle_ws_message(text: String) -> void:
	var json := JSON.new()
	if json.parse(text) != OK:
		return
	var msg: Dictionary = json.get_data()
	var event: String = msg.get("event", "")

	match event:
		"connected":
			var world = msg.get("world", {})
			if not world.is_empty():
				world_loaded.emit(world)

		"tick":
			tick_received.emit(
				int(msg.get("tick", 0)),
				float(msg.get("sim_time", 0.0))
			)

		"faction_war":
			faction_war.emit(
				int(msg.get("attacker", 0)),
				int(msg.get("defender", 0)),
				int(msg.get("region", 0))
			)

		"trade_completed":
			trade_completed.emit(
				int(msg.get("faction_a", 0)),
				int(msg.get("faction_b", 0)),
				int(msg.get("resource", 0)),
				float(msg.get("amount", 0.0))
			)

## Send a raw JSON message over the WebSocket (ping, custom commands, etc.)
func ws_send(data: Dictionary) -> void:
	if _ws == null or _ws.get_ready_state() != WebSocketPeer.STATE_OPEN:
		push_warning("SomniClient: WebSocket not open")
		return
	_ws.send_text(JSON.stringify(data))

# ---------------------------------------------------------------------------
# REST helpers
# ---------------------------------------------------------------------------

func _base_url() -> String:
	return "http://%s:%d" % [host, port]

func _get(path: String, callback: Callable) -> void:
	_enqueue("GET", _base_url() + path, "", callback)

func _post(path: String, body: Dictionary, callback: Callable) -> void:
	_enqueue("POST", _base_url() + path, JSON.stringify(body), callback)

func _delete(path: String, callback: Callable) -> void:
	_enqueue("DELETE", _base_url() + path, "", callback)

func _enqueue(method: String, url: String, body: String, callback: Callable) -> void:
	_http_queue.append({ "method": method, "url": url, "body": body, "callback": callback })
	_flush_queue()

func _flush_queue() -> void:
	if _http_busy or _http_queue.is_empty():
		return
	_http_busy = true
	var req: Dictionary = _http_queue.pop_front()
	var headers := PackedStringArray(["Content-Type: application/json"])
	var method_int: int
	match req.method:
		"POST":   method_int = HTTPClient.METHOD_POST
		"DELETE": method_int = HTTPClient.METHOD_DELETE
		_:        method_int = HTTPClient.METHOD_GET
	var err := _http.request(req.url, headers, method_int, req.body)
	if err != OK:
		_http_busy = false
		push_warning("SomniClient: HTTP request failed (%d) — %s" % [err, req.url])
		_flush_queue()
	else:
		# Store callback so _on_http_completed can call it.
		_http.set_meta("_callback", req.callback)

func _on_http_completed(result: int, response_code: int,
		_headers: PackedStringArray, body: PackedByteArray) -> void:
	var cb: Callable = _http.get_meta("_callback", Callable())
	_http_busy = false

	if result != HTTPRequest.RESULT_SUCCESS or response_code < 200 or response_code >= 300:
		var msg := "HTTP error %d (result=%d)" % [response_code, result]
		error_occurred.emit(msg)
		push_warning("SomniClient: " + msg)
		_flush_queue()
		if cb.is_valid():
			cb.call({})
		return

	var text := body.get_string_from_utf8()
	var json := JSON.new()
	var data: Dictionary = {}
	if json.parse(text) == OK:
		data = json.get_data()

	if cb.is_valid():
		cb.call(data)

	_flush_queue()

# ---------------------------------------------------------------------------
# World management REST calls
# ---------------------------------------------------------------------------

## Fetch available preset worlds from the server.
## Callback receives { "presets": [...] }
func get_presets(callback: Callable = Callable()) -> void:
	_get("/worlds/presets", func(d):
		if callback.is_valid(): callback.call(d))

## Check server health.
## Callback receives { "status": "ok", "world_running": bool, "backend": "python"|"cpp" }
func health(callback: Callable = Callable()) -> void:
	_get("/health", func(d):
		if callback.is_valid(): callback.call(d))

##
## Create (and start) a new simulation world.
##
## params keys (all optional except scenario):
##   scenario      String  — plain-text world description
##   scale         String  — "small" | "medium" | "large"
##   factions      int     — number of factions (2..8)
##   faction_names Array   — list of faction name strings
##   tech_level    String  — "stone_age" | "medieval" | "renaissance" | ...
##   ticks_per_second int  — simulation speed
##   headless      bool    — true = no local window
##
## Callback receives the create-world response dictionary.
##
func create_world(params: Dictionary, callback: Callable = Callable()) -> void:
	_post("/world/create", params, func(d):
		if not d.is_empty():
			world_loaded.emit(d)
		if callback.is_valid(): callback.call(d))

## Stop and discard the running world.
func destroy_world(callback: Callable = Callable()) -> void:
	_delete("/world", func(d):
		if callback.is_valid(): callback.call(d))

## Get the current clock/status summary.
func get_world(callback: Callable = Callable()) -> void:
	_get("/world", func(d):
		if callback.is_valid(): callback.call(d))

## Get the full world JSON (regions, factions, economy, wars, etc.)
func get_world_json(callback: Callable = Callable()) -> void:
	_get("/world/json", func(d):
		if not d.is_empty():
			world_loaded.emit(d)
		if callback.is_valid(): callback.call(d))

## Get details for one region by id.
func get_region(region_id: int, callback: Callable = Callable()) -> void:
	_get("/world/region/%d" % region_id, func(d):
		if callback.is_valid(): callback.call(d))

## Get all faction data.
func get_factions(callback: Callable = Callable()) -> void:
	_get("/world/factions", func(d):
		if callback.is_valid(): callback.call(d))

## Get recent events since a given tick.
func get_events(since_tick: int = 0, callback: Callable = Callable()) -> void:
	_get("/world/events?since_tick=%d" % since_tick, func(d):
		if callback.is_valid(): callback.call(d))

## Get a snapshot of simulation metrics.
func get_snapshot(callback: Callable = Callable()) -> void:
	_get("/world/snapshot", func(d):
		if callback.is_valid(): callback.call(d))

# ---------------------------------------------------------------------------
# Simulation control
# ---------------------------------------------------------------------------

func pause_sim(callback: Callable = Callable()) -> void:
	_post("/sim/pause", {}, func(d):
		if callback.is_valid(): callback.call(d))

func resume_sim(callback: Callable = Callable()) -> void:
	_post("/sim/resume", {}, func(d):
		if callback.is_valid(): callback.call(d))

func save_sim(path: String = "", callback: Callable = Callable()) -> void:
	var url := "/sim/save" + ("?path=" + path.uri_encode() if path != "" else "")
	_post(url, {}, func(d):
		if callback.is_valid(): callback.call(d))

# ---------------------------------------------------------------------------
# Player commands
# ---------------------------------------------------------------------------

func _command(body: Dictionary, callback: Callable) -> void:
	_post("/player/command", body, func(d):
		if not d.is_empty():
			command_result.emit(d)
		if callback.is_valid(): callback.call(d))

## Add resources to a region.
## resource_type: 0=food 1=wood 2=stone 3=iron 4=gold
func add_resource(region_id: int, resource_type: int, amount: float,
		callback: Callable = Callable()) -> void:
	_command({
		"command": "add_resource",
		"region_id": region_id,
		"resource_type": resource_type,
		"amount": amount,
	}, callback)

## Trigger a natural disaster in a region.
## magnitude: 0.0 (light) … 1.0 (devastating)
func trigger_disaster(region_id: int, magnitude: float = 0.5,
		callback: Callable = Callable()) -> void:
	_command({
		"command": "disaster",
		"region_id": region_id,
		"magnitude": magnitude,
	}, callback)

## Force two factions to declare war on each other.
func declare_war(faction_a: int, faction_b: int,
		callback: Callable = Callable()) -> void:
	_command({
		"command": "declare_war",
		"faction_a": faction_a,
		"faction_b": faction_b,
	}, callback)

## Force two factions to make peace.
func make_peace(faction_a: int, faction_b: int,
		callback: Callable = Callable()) -> void:
	_command({
		"command": "make_peace",
		"faction_a": faction_a,
		"faction_b": faction_b,
	}, callback)

## Recruit additional army units for a faction.
func recruit_army(faction_id: int, amount: float = 50.0,
		callback: Callable = Callable()) -> void:
	_command({
		"command": "recruit_army",
		"faction_id": faction_id,
		"amount": amount,
	}, callback)

## Send food aid to a faction.
func send_food(faction_id: int, amount: float = 200.0,
		callback: Callable = Callable()) -> void:
	_command({
		"command": "send_food",
		"faction_id": faction_id,
		"amount": amount,
	}, callback)

## Spawn settler population for a faction.
func spawn_settlers(faction_id: int, amount: int = 100,
		callback: Callable = Callable()) -> void:
	_command({
		"command": "spawn_settlers",
		"faction_id": faction_id,
		"amount": float(amount),
	}, callback)
