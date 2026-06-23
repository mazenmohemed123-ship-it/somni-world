"""
SOMNI API Server — FastAPI REST + WebSocket interface for player interaction.
This is the ONLY layer that communicates with external clients.
The simulation kernel runs independently and is queried here.
"""
from __future__ import annotations

import asyncio
import json
import logging
from pathlib import Path
from typing import List, Optional

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

VIEWER_DIR = Path(__file__).resolve().parents[2] / "viewer"

from ..simulation.runner import SimulationRunner
from ..bootstrap.world_schema import WorldSpec

log = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Request / response models
# ---------------------------------------------------------------------------

class CreateWorldRequest(BaseModel):
    scenario: str
    scale: Optional[str] = "medium"
    factions: Optional[int] = 2
    faction_names: Optional[List[str]] = None
    tech_level: Optional[str] = "medieval"
    ticks_per_second: Optional[int] = 20
    headless: Optional[bool] = True


class PlayerCommandRequest(BaseModel):
    command: str
    # region-level commands
    region_id: Optional[int] = None
    resource_type: Optional[int] = None
    amount: Optional[float] = None
    magnitude: Optional[float] = 0.5
    ticks: Optional[int] = 100
    # faction-level commands
    faction_a: Optional[int] = None    # declare_war / make_peace
    faction_b: Optional[int] = None    # declare_war / make_peace
    faction_id: Optional[int] = None   # recruit_army / send_food / spawn_settlers


class WorldResponse(BaseModel):
    tick: int
    year: int
    season: str
    is_running: bool


# ---------------------------------------------------------------------------
# WebSocket connection manager
# ---------------------------------------------------------------------------

class ConnectionManager:
    def __init__(self):
        self.active: List[WebSocket] = []

    async def connect(self, ws: WebSocket) -> None:
        await ws.accept()
        self.active.append(ws)

    def disconnect(self, ws: WebSocket) -> None:
        self.active.remove(ws)

    async def broadcast(self, message: dict) -> None:
        dead = []
        for ws in self.active:
            try:
                await ws.send_json(message)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.active.remove(ws)


# ---------------------------------------------------------------------------
# App factory
# ---------------------------------------------------------------------------

def create_app(runner: Optional[SimulationRunner] = None) -> FastAPI:
    """
    Create the FastAPI app, optionally binding to an existing SimulationRunner.
    If runner is None, the /world/create endpoint must be used first.
    """
    app = FastAPI(
        title="SOMNI API",
        description="Living Simulation Kernel — Player Interaction Layer",
        version="0.1.0",
    )

    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_methods=["*"],
        allow_headers=["*"],
    )

    manager = ConnectionManager()
    state: dict = {"runner": runner}

    def get_runner() -> SimulationRunner:
        r = state.get("runner")
        if r is None:
            raise HTTPException(503, "No simulation running. POST /world/create first.")
        return r

    # ------------------------------------------------------------------
    # Preset library (powers the launcher search + suggestions page)
    # ------------------------------------------------------------------

    @app.get("/worlds/presets", summary="List ready-to-run preset worlds")
    async def list_presets():
        from ..bootstrap.presets import get_presets
        return {"presets": get_presets()}

    @app.get("/health", summary="Liveness probe + active backend")
    async def health():
        r = state.get("runner")
        return {
            "status": "ok",
            "world_running": r is not None and r.is_running(),
            "backend": (r.backend if r else None),
        }

    # ------------------------------------------------------------------
    # World management endpoints
    # ------------------------------------------------------------------

    @app.post("/world/create", summary="Bootstrap and start a new simulation world")
    async def create_world(req: CreateWorldRequest):
        from ..bootstrap.questioner import WorldQuestioner

        answers = {
            "input":        req.scenario,
            "time_period":  req.tech_level or "medieval",
            "scale":        req.scale or "medium",
            "factions":     str(req.factions or 2),
            "faction_names": ",".join(req.faction_names or []),
            "conflict":     req.scenario,
        }

        spec = WorldQuestioner(headless_answers=answers).run()
        spec.ticks_per_second = req.ticks_per_second or 20

        r = SimulationRunner(
            spec=spec,
            ticks_per_second=spec.ticks_per_second,
            headless=req.headless if req.headless is not None else True,
        )

        # Kernel callbacks fire from the simulation's background thread, not the
        # asyncio loop — so schedule the broadcast onto the loop thread-safely.
        loop = asyncio.get_running_loop()

        def relay(message: dict) -> None:
            if not manager.active:
                return
            try:
                asyncio.run_coroutine_threadsafe(manager.broadcast(message), loop)
            except RuntimeError:
                pass  # loop shutting down

        @r.on("tick")
        def relay_tick(tick, sim_time):
            # Broadcast at ~2 Hz, not every tick, to avoid flooding clients.
            if tick % max(1, spec.ticks_per_second // 2) == 0:
                relay({"event": "tick", "tick": tick, "sim_time": sim_time})

        @r.on("faction_war")
        def relay_war(attacker, defender, region):
            relay({"event": "faction_war", "attacker": attacker,
                   "defender": defender, "region": region})

        r.setup().start()
        state["runner"] = r

        return {
            "status": "started",
            "world_name": spec.name,
            "seed": spec.seed,
            "dimensions": spec.dimensions(),
            "factions": len(spec.factions),
            "backend": r.backend,
        }

    @app.delete("/world", summary="Stop and clear the current simulation")
    async def destroy_world():
        r = state.get("runner")
        if r:
            r.stop()
            state["runner"] = None
        return {"status": "stopped"}

    @app.get("/world", response_model=WorldResponse, summary="Current world state summary")
    async def get_world():
        r = get_runner()
        info = r.clock_info()
        return WorldResponse(
            tick=r.current_tick(),
            year=info.get("year", 1),
            season=info.get("season", "spring"),
            is_running=r.is_running(),
        )

    @app.get("/world/json", summary="Full world state as JSON")
    async def get_world_json():
        r = get_runner()
        return json.loads(r.world_json())

    @app.get("/world/region/{region_id}", summary="Region details")
    async def get_region(region_id: int):
        r = get_runner()
        return r.region_info(region_id)

    @app.get("/world/events", summary="Recent simulation events")
    async def get_events(since_tick: int = 0):
        r = get_runner()
        return r.sync_bridge.get_events_since(since_tick)

    @app.get("/world/snapshot", summary="Get simulation metrics snapshot")
    async def get_snapshot():
        r = get_runner()
        snap = r.sync_bridge.get_snapshot()
        return {
            "tick": snap.tick,
            "active_wars": snap.active_wars,
            "recent_events": snap.recent_events[-20:],
        }

    # ------------------------------------------------------------------
    # Simulation control
    # ------------------------------------------------------------------

    @app.post("/sim/pause")
    async def pause():
        get_runner().pause()
        return {"status": "paused"}

    @app.post("/sim/resume")
    async def resume():
        get_runner().resume()
        return {"status": "resumed"}

    @app.post("/sim/save")
    async def save(path: Optional[str] = None):
        r = get_runner()
        saved_path = r.save_snapshot(path)
        return {"saved_to": saved_path}

    # ------------------------------------------------------------------
    # Player commands (STEP 6)
    # ------------------------------------------------------------------

    @app.get("/world/factions", summary="All faction data")
    async def get_factions():
        r = get_runner()
        return {"factions": r.factions_info()}

    @app.post("/player/command", summary="Issue a player command to the simulation")
    async def player_command(req: PlayerCommandRequest):
        r = get_runner()

        if req.command == "add_resource":
            if req.region_id is None or req.resource_type is None:
                raise HTTPException(400, "region_id and resource_type required")
            r.add_resource(req.region_id, req.resource_type, req.amount or 100.0)
            return {"status": "ok", "command": "add_resource"}

        elif req.command == "disaster":
            if req.region_id is None:
                raise HTTPException(400, "region_id required")
            r.trigger_disaster(req.region_id, req.magnitude or 0.5)
            return {"status": "ok", "command": "disaster"}

        elif req.command == "declare_war":
            if req.faction_a is None or req.faction_b is None:
                raise HTTPException(400, "faction_a and faction_b required")
            r.declare_war(req.faction_a, req.faction_b)
            return {"status": "ok", "command": "declare_war",
                    "attacker": req.faction_a, "defender": req.faction_b}

        elif req.command == "make_peace":
            if req.faction_a is None or req.faction_b is None:
                raise HTTPException(400, "faction_a and faction_b required")
            r.make_peace(req.faction_a, req.faction_b)
            return {"status": "ok", "command": "make_peace",
                    "faction_a": req.faction_a, "faction_b": req.faction_b}

        elif req.command == "recruit_army":
            r.recruit_army(req.faction_id or 0, req.amount or 50.0)
            return {"status": "ok", "command": "recruit_army",
                    "faction_id": req.faction_id or 0, "amount": req.amount or 50.0}

        elif req.command == "send_food":
            r.send_food(req.faction_id or 0, req.amount or 200.0)
            return {"status": "ok", "command": "send_food",
                    "faction_id": req.faction_id or 0, "amount": req.amount or 200.0}

        elif req.command == "spawn_settlers":
            r.spawn_settlers(req.faction_id or 0, int(req.amount or 100))
            return {"status": "ok", "command": "spawn_settlers",
                    "faction_id": req.faction_id or 0, "amount": req.amount or 100}

        else:
            raise HTTPException(400, f"Unknown command: {req.command}")

    # ------------------------------------------------------------------
    # WebSocket stream (real-time events)
    # ------------------------------------------------------------------

    @app.websocket("/ws")
    async def websocket_endpoint(ws: WebSocket):
        await manager.connect(ws)
        try:
            # Send current state on connect
            r = state.get("runner")
            if r:
                await ws.send_json({
                    "event": "connected",
                    "tick": r.current_tick(),
                    "world": json.loads(r.world_json()) if r else {},
                })
            while True:
                data = await ws.receive_text()
                try:
                    msg = json.loads(data)
                    if msg.get("command") == "ping":
                        await ws.send_json({"event": "pong",
                                            "tick": r.current_tick() if r else 0})
                except json.JSONDecodeError:
                    pass
        except WebSocketDisconnect:
            manager.disconnect(ws)

    # ------------------------------------------------------------------
    # Serve the viewer UI from the same origin (no file:// CORS issues).
    #   /            -> launcher hub (search + preset worlds)
    #   /view        -> 3D world viewer
    #   /viewer/...  -> raw static assets
    # ------------------------------------------------------------------
    if VIEWER_DIR.is_dir():
        @app.get("/", include_in_schema=False)
        async def launcher_page():
            launcher = VIEWER_DIR / "launcher.html"
            target = launcher if launcher.exists() else VIEWER_DIR / "index.html"
            return FileResponse(target)

        @app.get("/view", include_in_schema=False)
        async def world_page():
            return FileResponse(VIEWER_DIR / "index.html")

        app.mount("/viewer", StaticFiles(directory=str(VIEWER_DIR)), name="viewer")

    return app


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    import uvicorn
    app = create_app()
    uvicorn.run(app, host="0.0.0.0", port=8080, log_level="info")


if __name__ == "__main__":
    main()
