"""
SOMNI API Server — FastAPI REST + WebSocket interface for player interaction.
This is the ONLY layer that communicates with external clients.
The simulation kernel runs independently and is queried here.
"""
from __future__ import annotations

import asyncio
import json
import logging
from typing import List, Optional

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

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
    command: str           # "add_resource" | "disaster" | "spawn_npc" | "advance"
    region_id: Optional[int] = None
    resource_type: Optional[int] = None
    amount: Optional[float] = None
    magnitude: Optional[float] = 0.5
    ticks: Optional[int] = 100


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

        # Relay events to WebSocket clients
        @r.on("tick")
        def relay_tick(tick, sim_time):
            asyncio.create_task(manager.broadcast({
                "event": "tick",
                "tick": tick,
                "sim_time": sim_time,
            }))

        @r.on("faction_war")
        def relay_war(attacker, defender, region):
            asyncio.create_task(manager.broadcast({
                "event": "faction_war",
                "attacker": attacker,
                "defender": defender,
                "region": region,
            }))

        r.setup().start()
        state["runner"] = r

        return {
            "status": "started",
            "world_name": spec.name,
            "seed": spec.seed,
            "dimensions": spec.dimensions(),
            "factions": len(spec.factions),
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
