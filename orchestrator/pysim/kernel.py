"""
PyKernel — a deterministic, pure-Python simulation kernel.

Drives a region-level statistical simulation:
  * procedural terrain (deterministic value-noise) -> biomes
  * factions claim territory around their spawn point
  * per-region population grows/shrinks from food + safety (logistic model)
  * resources deplete and regenerate
  * adjacent rival factions occasionally go to war; survivors trade

Exposes the same query/control surface the SimulationRunner expects, so it is a
drop-in replacement for the C++ somni_core.SimulationKernel binding. Runs its
tick loop on a background thread, exactly like the C++ kernel's real-time mode.
"""
from __future__ import annotations

import json
import math
import threading
import time
from typing import Callable, Dict, List, Optional

# ---------------------------------------------------------------------------
# Constants mirrored from the C++ WorldClock so the two backends agree.
# ---------------------------------------------------------------------------
TICKS_PER_HOUR = 60
TICKS_PER_DAY = TICKS_PER_HOUR * 24
TICKS_PER_MONTH = TICKS_PER_DAY * 30
TICKS_PER_SEASON = TICKS_PER_MONTH * 3
TICKS_PER_YEAR = TICKS_PER_SEASON * 4
SEASONS = ["spring", "summer", "autumn", "winter"]

# Biome ids match the viewer's BIOME table (0..10).
BIOME_OCEAN = 0
BIOME_BEACH = 1
BIOME_GRASSLAND = 2
BIOME_FOREST = 3
BIOME_JUNGLE = 4
BIOME_DESERT = 5
BIOME_SAVANNA = 6
BIOME_TAIGA = 7
BIOME_TUNDRA = 8
BIOME_MOUNTAIN = 9
BIOME_SNOW = 10

REGION_SIZE = 16


def is_available() -> bool:
    """The Python backend is always available (no native deps)."""
    return True


# ---------------------------------------------------------------------------
# Deterministic value noise (no numpy dependency)
# ---------------------------------------------------------------------------
def _hash2(x: int, y: int, seed: int) -> float:
    """Deterministic hash -> float in [0,1). Stable across platforms."""
    h = (x * 374761393 + y * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
    h = (h ^ (h >> 16)) & 0xFFFFFFFF
    return h / 0xFFFFFFFF


def _smooth(t: float) -> float:
    return t * t * (3.0 - 2.0 * t)


def _value_noise(x: float, y: float, seed: int) -> float:
    xi, yi = int(math.floor(x)), int(math.floor(y))
    xf, yf = x - xi, y - yi
    v00 = _hash2(xi, yi, seed)
    v10 = _hash2(xi + 1, yi, seed)
    v01 = _hash2(xi, yi + 1, seed)
    v11 = _hash2(xi + 1, yi + 1, seed)
    u, v = _smooth(xf), _smooth(yf)
    return (v00 * (1 - u) + v10 * u) * (1 - v) + (v01 * (1 - u) + v11 * u) * v


def _fbm(x: float, y: float, seed: int, octaves: int = 4) -> float:
    total, amp, freq, norm = 0.0, 1.0, 1.0, 0.0
    for _ in range(octaves):
        total += amp * _value_noise(x * freq, y * freq, seed)
        norm += amp
        amp *= 0.5
        freq *= 2.0
    return total / norm


# ---------------------------------------------------------------------------
# Region
# ---------------------------------------------------------------------------
class _Region:
    __slots__ = ("id", "grid_x", "grid_y", "biome", "elevation",
                 "npc_count", "controlling_faction", "resources",
                 "resource_cap", "resource_regen")

    def __init__(self, rid: int, gx: int, gy: int):
        self.id = rid
        self.grid_x = gx
        self.grid_y = gy
        self.biome = BIOME_GRASSLAND
        self.elevation = 0.0
        self.npc_count = 0
        self.controlling_faction = -1
        self.resources = [0.0] * 16
        self.resource_cap = [0.0] * 16
        self.resource_regen = [0.0] * 16


# ---------------------------------------------------------------------------
# PyKernel
# ---------------------------------------------------------------------------
class PyKernel:
    def __init__(self, spec, ticks_per_second: int = 20, headless: bool = True):
        self.spec = spec
        self.tps = max(1, ticks_per_second)
        self.headless = headless
        self.seed = int(getattr(spec, "seed", 42))

        w, h = spec.dimensions()
        self.width = w
        self.height = h
        self.region_size = REGION_SIZE
        self.grid_w = max(1, w // REGION_SIZE)
        self.grid_h = max(1, h // REGION_SIZE)

        self._tick = 0
        self._running = False
        self._paused = False
        self._thread: Optional[threading.Thread] = None
        self._lock = threading.Lock()

        # Callbacks (match the C++ binding's registration methods)
        self._cb_tick: Optional[Callable] = None
        self._cb_npc_died: Optional[Callable] = None
        self._cb_faction_war: Optional[Callable] = None
        self._cb_trade: Optional[Callable] = None

        self._faction_names: Dict[int, str] = {}
        self._faction_treasury: Dict[int, float] = {}
        self._faction_army: Dict[int, float] = {}
        self._active_wars: List[tuple] = []

        self.regions: List[_Region] = []
        self._generate_world()

    # ------------------------------------------------------------------
    # World generation (deterministic)
    # ------------------------------------------------------------------
    def _generate_world(self) -> None:
        gw, gh = self.grid_w, self.grid_h
        cx, cy = (gw - 1) / 2.0, (gh - 1) / 2.0
        max_d = math.sqrt(cx * cx + cy * cy) or 1.0

        for gy in range(gh):
            for gx in range(gw):
                r = _Region(gy * gw + gx, gx, gy)
                # Island-ish elevation: noise minus distance falloff.
                n = _fbm(gx * 0.18, gy * 0.18, self.seed, 5)
                d = math.sqrt((gx - cx) ** 2 + (gy - cy) ** 2) / max_d
                elev = n - d * 0.55
                moist = _fbm(gx * 0.22 + 100, gy * 0.22 + 100, self.seed + 7, 4)
                temp = 1.0 - (gy / max(1, gh - 1))  # north colder
                r.elevation = max(0.0, elev)
                r.biome = self._biome_for(elev, moist, temp)
                self._seed_resources(r)
                self.regions.append(r)

        self._place_factions()

    @staticmethod
    def _biome_for(elev: float, moist: float, temp: float) -> int:
        if elev < 0.02:
            return BIOME_OCEAN
        if elev < 0.06:
            return BIOME_BEACH
        if elev > 0.55:
            return BIOME_SNOW if temp < 0.35 else BIOME_MOUNTAIN
        if temp < 0.25:
            return BIOME_TUNDRA if moist < 0.5 else BIOME_TAIGA
        if temp > 0.7 and moist < 0.3:
            return BIOME_DESERT
        if temp > 0.6 and moist < 0.5:
            return BIOME_SAVANNA
        if moist > 0.6:
            return BIOME_JUNGLE if temp > 0.55 else BIOME_FOREST
        return BIOME_GRASSLAND

    def _seed_resources(self, r: "_Region") -> None:
        # Food + water capacity by biome; deterministic initial fill.
        food = {
            BIOME_GRASSLAND: 120, BIOME_FOREST: 90, BIOME_JUNGLE: 100,
            BIOME_SAVANNA: 80, BIOME_TAIGA: 60, BIOME_BEACH: 50,
            BIOME_DESERT: 15, BIOME_TUNDRA: 20, BIOME_MOUNTAIN: 25,
            BIOME_SNOW: 5, BIOME_OCEAN: 70,
        }.get(r.biome, 60)
        water = 150 if r.biome in (BIOME_OCEAN, BIOME_JUNGLE, BIOME_BEACH) else 70
        jitter = 0.6 + 0.8 * _hash2(r.grid_x, r.grid_y, self.seed + 31)
        r.resource_cap[0] = food * jitter        # FOOD
        r.resource_cap[1] = water * jitter       # WATER
        r.resources[0] = r.resource_cap[0] * 0.8
        r.resources[1] = r.resource_cap[1] * 0.8
        r.resource_regen[0] = r.resource_cap[0] * 0.002
        r.resource_regen[1] = r.resource_cap[1] * 0.003

    def _place_factions(self) -> None:
        land = [r for r in self.regions if r.biome != BIOME_OCEAN]
        if not land:
            land = list(self.regions)
        nf = len(self.spec.factions)
        if nf == 0:
            return
        # Fair share of land per faction (leave some wilderness unclaimed).
        share = max(1, int(len(land) * 0.8) // nf)

        for fid, fs in enumerate(self.spec.factions):
            self._faction_names[fid] = fs.name
            self._faction_treasury[fid] = float(getattr(fs, "initial_treasury", 200.0))
            self._faction_army[fid] = float(getattr(fs, "initial_army", 50.0))

            # Spawn near requested coords (clamped to the region grid), else
            # spread factions evenly across the map.
            sx = int(getattr(fs, "spawn_x", 0)) // self.region_size
            sy = int(getattr(fs, "spawn_y", 0)) // self.region_size
            if sx == 0 and sy == 0:
                sx = int((fid + 0.5) / nf * self.grid_w)
                sy = self.grid_h // 2
            sx = max(0, min(self.grid_w - 1, sx))
            sy = max(0, min(self.grid_h - 1, sy))

            # Claim the nearest unclaimed land regions, up to the fair share.
            candidates = sorted(
                (r for r in land if r.controlling_faction == -1),
                key=lambda r: (r.grid_x - sx) ** 2 + (r.grid_y - sy) ** 2,
            )
            claimed = candidates[:share] or candidates[:1]
            pop = int(getattr(fs, "initial_population", 100))
            per = max(1, pop // max(1, len(claimed)))
            for r in claimed:
                r.controlling_faction = fid
                r.npc_count = per

    # ------------------------------------------------------------------
    # Tick loop
    # ------------------------------------------------------------------
    def _step(self) -> None:
        with self._lock:
            self._tick += 1
            t = self._tick

            # Resource regen + population dynamics every few ticks (cheap).
            if t % 5 == 0:
                for r in self.regions:
                    for i in (0, 1):
                        r.resources[i] = min(
                            r.resource_cap[i], r.resources[i] + r.resource_regen[i] * 5)
                    if r.npc_count <= 0:
                        continue
                    food = r.resources[0]
                    # Logistic-ish growth limited by food carrying capacity.
                    cap = max(1.0, r.resource_cap[0] * 0.5)
                    growth = 0.01 * r.npc_count * (1.0 - r.npc_count / cap)
                    consume = r.npc_count * 0.05
                    r.resources[0] = max(0.0, food - consume)
                    starving = food < consume
                    if starving:
                        deaths = max(1, int(r.npc_count * 0.02))
                        r.npc_count = max(0, r.npc_count - deaths)
                        if self._cb_npc_died:
                            self._cb_npc_died(r.id, 0)  # cause 0 = starvation
                    else:
                        r.npc_count = max(0, int(r.npc_count + growth))

            # Occasional war between adjacent rival factions.
            if t % 600 == 0 and len(self.spec.factions) >= 2:
                self._maybe_war(t)

            # Occasional trade between peaceful neighbors.
            if t % 400 == 0 and len(self.spec.factions) >= 2:
                self._maybe_trade(t)

        # Fire tick callback outside the lock.
        if self._cb_tick:
            self._cb_tick(t, float(t))

    def _maybe_war(self, t: int) -> None:
        roll = _hash2(t, 1, self.seed)
        if roll > 0.5:
            return
        nf = len(self.spec.factions)
        a = int(_hash2(t, 2, self.seed) * nf) % nf
        b = (a + 1 + int(_hash2(t, 3, self.seed) * (nf - 1))) % nf
        if a == b:
            return
        region = int(_hash2(t, 4, self.seed) * len(self.regions)) % len(self.regions)
        self._active_wars.append((a, b))
        if self._cb_faction_war:
            self._cb_faction_war(a, b, region)

    def _maybe_trade(self, t: int) -> None:
        nf = len(self.spec.factions)
        a = int(_hash2(t, 5, self.seed) * nf) % nf
        b = (a + 1) % nf
        if a == b:
            return
        amount = 10.0 + 40.0 * _hash2(t, 6, self.seed)
        self._faction_treasury[a] = self._faction_treasury.get(a, 0) + amount * 0.5
        self._faction_treasury[b] = self._faction_treasury.get(b, 0) + amount * 0.5
        if self._cb_trade:
            self._cb_trade(a, b, 0, amount)

    def _loop(self) -> None:
        interval = 1.0 / self.tps
        while self._running:
            if not self._paused:
                self._step()
            time.sleep(interval)

    # ------------------------------------------------------------------
    # Control surface (mirrors somni_core.SimulationKernel)
    # ------------------------------------------------------------------
    def on_tick(self, fn): self._cb_tick = fn
    def on_npc_died(self, fn): self._cb_npc_died = fn
    def on_faction_war(self, fn): self._cb_faction_war = fn
    def on_trade_completed(self, fn): self._cb_trade = fn

    def start(self) -> None:
        if self._running:
            return
        self._running = True
        self._paused = False
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._running = False
        if self._thread:
            self._thread.join(timeout=1.0)
            self._thread = None

    def pause(self) -> None: self._paused = True
    def resume(self) -> None: self._paused = False
    def current_tick(self) -> int: return self._tick
    def is_running(self) -> bool: return self._running and not self._paused

    def save_snapshot(self, path: str) -> None:
        with open(path, "w") as f:
            f.write(self.world_json())

    # ------------------------------------------------------------------
    # Queries
    # ------------------------------------------------------------------
    def clock(self) -> "_PyClock":
        return _PyClock(self._tick)

    def region(self, region_id: int) -> "_RegionView":
        return _RegionView(self.regions[region_id])

    def _faction_summary(self) -> list:
        """Aggregate per-region stats into per-faction totals for the JSON output."""
        pop: Dict[int, float] = {fid: 0.0 for fid in self._faction_names}
        food: Dict[int, float] = {fid: 0.0 for fid in self._faction_names}
        territory: Dict[int, int] = {fid: 0 for fid in self._faction_names}
        for r in self.regions:
            fid = r.controlling_faction
            if fid not in self._faction_names:
                continue
            pop[fid] += r.npc_count
            food[fid] += r.resources[0] if r.resources else 0.0
            territory[fid] += 1
        at_war: Dict[int, list] = {fid: [] for fid in self._faction_names}
        for a, b in self._active_wars:
            if a in at_war: at_war[a].append(b)
            if b in at_war: at_war[b].append(a)
        return [
            {
                "id": fid,
                "name": name,
                "population": round(pop.get(fid, 0.0), 1),
                "food": round(food.get(fid, 0.0), 1),
                "gold": round(self._faction_treasury.get(fid, 0.0), 1),
                "army": round(self._faction_army.get(fid, 0.0), 1),
                "territory": territory.get(fid, 0),
                "at_war_with": at_war.get(fid, []),
            }
            for fid, name in self._faction_names.items()
        ]

    def world_json(self) -> str:
        c = self.clock()
        with self._lock:
            regions = [
                {
                    "id": r.id, "grid_x": r.grid_x, "grid_y": r.grid_y,
                    "biome": r.biome, "npc_count": r.npc_count,
                    "controlling_faction": r.controlling_faction,
                    "resources": list(r.resources),
                }
                for r in self.regions
            ]
        return json.dumps({
            "config": {
                "name": self.spec.name, "width": self.width,
                "height": self.height, "region_size": self.region_size,
                "seed": self.seed,
            },
            "clock": {
                "tick": c.tick, "year": c.year, "season": c.season,
                "month": c.month, "day": c.day, "hour": c.hour,
            },
            "factions": self._faction_summary(),
            "regions": regions,
        })

    # Player commands -------------------------------------------------
    def add_resource(self, region_id: int, resource_type: int, amount: float) -> None:
        with self._lock:
            r = self.regions[region_id]
            ri = max(0, min(15, resource_type))
            r.resources[ri] = min(r.resource_cap[ri] or 1e9, r.resources[ri] + amount)

    def trigger_disaster(self, region_id: int, magnitude: float = 0.5) -> None:
        with self._lock:
            r = self.regions[region_id]
            deaths = int(r.npc_count * magnitude)
            r.npc_count = max(0, r.npc_count - deaths)

    def declare_war(self, faction_a: int, faction_b: int) -> None:
        nf = len(self.spec.factions)
        if not (0 <= faction_a < nf and 0 <= faction_b < nf and faction_a != faction_b):
            return
        with self._lock:
            already = any((a == faction_a and b == faction_b) or
                          (a == faction_b and b == faction_a)
                          for a, b in self._active_wars)
            if not already:
                self._active_wars.append((faction_a, faction_b))
        region = next((r.id for r in self.regions
                       if r.controlling_faction == faction_b), 0)
        if self._cb_faction_war:
            self._cb_faction_war(faction_a, faction_b, region)

    def make_peace(self, faction_a: int, faction_b: int) -> None:
        with self._lock:
            self._active_wars = [
                (a, b) for a, b in self._active_wars
                if not ((a == faction_a and b == faction_b) or
                        (a == faction_b and b == faction_a))
            ]

    def recruit_army(self, faction_id: int, amount: float = 50.0) -> None:
        with self._lock:
            self._faction_army[faction_id] = \
                self._faction_army.get(faction_id, 0.0) + amount

    def send_food(self, faction_id: int, amount: float = 200.0) -> None:
        with self._lock:
            for r in self.regions:
                if r.controlling_faction == faction_id:
                    r.resources[0] = min(
                        r.resource_cap[0] or 1e9, r.resources[0] + amount)

    def spawn_settlers(self, faction_id: int, amount: int = 100) -> None:
        with self._lock:
            owned = [r for r in self.regions
                     if r.controlling_faction == faction_id]
            if not owned:
                return
            per = max(1, amount // len(owned))
            for r in owned:
                r.npc_count += per


class _PyClock:
    """season is an int index (0..3) to match the C++ WorldClock JSON shape."""
    def __init__(self, tick: int):
        self.tick = tick
        self.hour = (tick // TICKS_PER_HOUR) % 24
        self.day = (tick // TICKS_PER_DAY) % 30 + 1
        self.month = (tick // TICKS_PER_MONTH) % 12 + 1
        self.season = (tick // TICKS_PER_SEASON) % 4
        self.year = tick // TICKS_PER_YEAR + 1

    @property
    def season_name(self) -> str:
        return SEASONS[self.season]

    def is_night(self) -> bool:
        return self.hour < 6 or self.hour >= 20


class _RegionView:
    def __init__(self, r: _Region):
        self._r = r
        self.id = r.id
        self.npc_count = r.npc_count
        self.controlling_faction = r.controlling_faction

    def biome(self) -> int:
        return self._r.biome

    def resource_amount(self, i: int) -> float:
        return self._r.resources[i] if 0 <= i < 16 else 0.0
