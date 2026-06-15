"""
WorldRenderer — converts SOMNI world JSON into visual output.

Backends (all optional, graceful fallback to ASCII):
  rerun   → real-time 3D scatter + heatmaps with timeline scrubbing
  pyvista → static 3D terrain mesh (elevation surface)
  ascii   → terminal biome/resource heatmap (no extra deps)
"""
from __future__ import annotations

import json
import math
from typing import Any, Dict, List, Optional


BIOME_COLORS = {
    0: (0.1, 0.3, 0.8),   # OCEAN       — blue
    1: (0.85, 0.9, 0.95), # TUNDRA      — ice white
    2: (0.2, 0.5, 0.3),   # BOREAL      — dark green
    3: (0.3, 0.7, 0.3),   # TEMP_FOREST — green
    4: (0.8, 0.85, 0.3),  # GRASSLAND   — yellow-green
    5: (0.85, 0.75, 0.3), # SAVANNA     — tan
    6: (0.95, 0.85, 0.5), # DESERT      — sand
    7: (0.1, 0.6, 0.1),   # JUNGLE      — deep green
    8: (0.4, 0.55, 0.4),  # SWAMP       — murky green
    9: (0.6, 0.6, 0.65),  # MOUNTAINS   — grey
    10:(0.95, 0.98, 1.0), # ICE         — white
}

BIOME_ASCII = {
    0: '~', 1: '.', 2: 'f', 3: 'F', 4: ',',
    5: 's', 6: 'd', 7: 'J', 8: 'w', 9: '^', 10: '*',
}

BIOME_NAMES = {
    0: "Ocean", 1: "Tundra", 2: "Boreal Forest", 3: "Temperate Forest",
    4: "Grassland", 5: "Savanna", 6: "Desert", 7: "Jungle",
    8: "Swamp", 9: "Mountains", 10: "Ice Sheet",
}


class WorldRenderer:
    """
    Render SOMNI world state using the best available backend.
    """

    def __init__(self, backend: str = "auto"):
        """
        backend: "auto" | "rerun" | "pyvista" | "ascii"
        """
        self._backend = self._resolve_backend(backend)

    def _resolve_backend(self, requested: str) -> str:
        if requested != "auto":
            return requested
        # Try backends in preference order
        for b in ("rerun", "pyvista", "ascii"):
            if self._can_use(b):
                return b
        return "ascii"

    def _can_use(self, backend: str) -> bool:
        try:
            if backend == "rerun":
                import rerun  # noqa: F401
            elif backend == "pyvista":
                import pyvista  # noqa: F401
            return True
        except ImportError:
            return False

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def render_terrain(self, world_json: str, output_path: Optional[str] = None) -> None:
        """Render the world terrain to the active backend."""
        data = json.loads(world_json)
        cfg  = data.get("config", {})
        regs = data.get("regions", [])
        w    = cfg.get("width", 256) // cfg.get("region_size", 16)
        h    = cfg.get("height", 256) // cfg.get("region_size", 16)

        if self._backend == "rerun":
            self._render_rerun(regs, w, h, cfg, data.get("clock", {}))
        elif self._backend == "pyvista":
            self._render_pyvista(regs, w, h, output_path)
        else:
            self._render_ascii(regs, w, h, cfg)

    def render_factions(self, world_json: str) -> None:
        """Print faction status summary."""
        data = json.loads(world_json)
        regs = data.get("regions", [])
        faction_regions: Dict[int, int] = {}
        for r in regs:
            fid = r.get("controlling_faction", 0xFFFFFFFF)
            if fid != 0xFFFFFFFF:
                faction_regions[fid] = faction_regions.get(fid, 0) + 1
        print("\n=== Faction Territory ===")
        for fid, count in sorted(faction_regions.items(), key=lambda x: -x[1]):
            print(f"  Faction {fid:>4}: {count:>4} regions")

    # ------------------------------------------------------------------
    # Rerun backend — real-time 3D + timeline
    # ------------------------------------------------------------------

    def _render_rerun(self, regions: List[Dict], gw: int, gh: int,
                      cfg: Dict, clock: Dict) -> None:
        import rerun as rr
        import numpy as np

        rr.init("SOMNI World", spawn=True)

        tick = clock.get("tick", 0)
        rr.set_time_sequence("tick", tick)

        # 3D scatter: one point per region colored by biome
        positions = []
        colors    = []
        labels    = []

        for r in regions:
            gx  = r.get("grid_x", 0)
            gy  = r.get("grid_y", 0)
            bio = r.get("biome", 4)
            npc = r.get("npc_count", 0)
            elev = 0.0  # could pull from terrain if stored

            positions.append([gx, gy, elev])
            col = BIOME_COLORS.get(bio, (0.5, 0.5, 0.5))
            colors.append([int(c * 255) for c in col] + [200])
            labels.append(f"{BIOME_NAMES.get(bio, '?')} | NPCs:{npc}")

        positions_arr = np.array(positions, dtype=np.float32)
        colors_arr    = np.array(colors, dtype=np.uint8)

        rr.log("world/regions", rr.Points3D(
            positions=positions_arr,
            colors=colors_arr,
            radii=0.4,
            labels=labels,
        ))

        # NPC heatmap overlay
        npc_counts = np.zeros((gh, gw), dtype=np.float32)
        for r in regions:
            npc_counts[r.get("grid_y", 0), r.get("grid_x", 0)] = r.get("npc_count", 0)

        rr.log("world/npc_density", rr.DepthImage(npc_counts))

        print(f"[Rerun] Logged tick={tick} — open http://localhost:9090 in browser")

    # ------------------------------------------------------------------
    # PyVista backend — 3D terrain mesh
    # ------------------------------------------------------------------

    def _render_pyvista(self, regions: List[Dict], gw: int, gh: int,
                        output_path: Optional[str]) -> None:
        import pyvista as pv
        import numpy as np

        # Build elevation grid from biome (approximate)
        BIOME_ELEV = {0: 0.0, 1: 0.3, 2: 0.4, 3: 0.5, 4: 0.3,
                      5: 0.25, 6: 0.2, 7: 0.45, 8: 0.1, 9: 0.8, 10: 0.9}

        elev = np.zeros((gh, gw), dtype=np.float32)
        color_rgb = np.zeros((gh, gw, 3), dtype=np.float32)

        for r in regions:
            gx  = r.get("grid_x", 0)
            gy  = r.get("grid_y", 0)
            bio = r.get("biome", 4)
            elev[gy, gx]    = BIOME_ELEV.get(bio, 0.3)
            col = BIOME_COLORS.get(bio, (0.5, 0.5, 0.5))
            color_rgb[gy, gx] = col

        grid = pv.ImageData(dimensions=(gw, gh, 1), spacing=(1, 1, 1))
        grid.point_data["elevation"] = elev.flatten(order="F")

        # Warp the surface by elevation
        surface = grid.extract_surface()
        warped  = surface.warp_by_scalar("elevation", factor=8.0)
        warped.point_data["biome_color"] = color_rgb.reshape(-1, 3)

        pl = pv.Plotter(window_size=(1280, 720), off_screen=output_path is not None)
        pl.add_mesh(warped, scalars="biome_color", rgb=True, smooth_shading=True)
        pl.add_title("SOMNI World — Terrain")
        pl.camera_position = "iso"

        if output_path:
            pl.screenshot(output_path)
            print(f"[PyVista] Saved terrain render to {output_path}")
        else:
            pl.show()

    # ------------------------------------------------------------------
    # ASCII backend — terminal heatmap (zero deps)
    # ------------------------------------------------------------------

    def _render_ascii(self, regions: List[Dict], gw: int, gh: int,
                      cfg: Dict) -> None:
        grid = [[' '] * gw for _ in range(gh)]
        pop_grid = [[0] * gw for _ in range(gh)]

        for r in regions:
            gx  = r.get("grid_x", 0)
            gy  = r.get("grid_y", 0)
            bio = r.get("biome", 4)
            npc = r.get("npc_count", 0)
            if 0 <= gy < gh and 0 <= gx < gw:
                grid[gy][gx]    = BIOME_ASCII.get(bio, '?')
                pop_grid[gy][gx] = npc

        print(f"\n=== SOMNI World '{cfg.get('name', '?')}' "
              f"({gw}×{gh} regions) ===")
        print("Biome map:")
        for row in grid:
            print("  " + "".join(row))

        print("\nPopulation density (. = 0, + = 1-10, # = 10+):")
        for row in pop_grid:
            line = ""
            for v in row:
                line += '.' if v == 0 else ('+' if v < 10 else '#')
            print("  " + line)

        print(f"\nLegend: {' | '.join(f'{v}={k}' for k, v in BIOME_ASCII.items())}")


# ---------------------------------------------------------------------------
# Quick-start helper
# ---------------------------------------------------------------------------

def render_from_api(api_url: str = "http://localhost:8080",
                    backend: str = "auto") -> None:
    """Fetch world state from the running API and render it."""
    try:
        import httpx
    except ImportError:
        print("pip install httpx  to use render_from_api()")
        return

    resp = httpx.get(f"{api_url}/world/json", timeout=5.0)
    resp.raise_for_status()
    renderer = WorldRenderer(backend=backend)
    renderer.render_terrain(resp.text)
    renderer.render_factions(resp.text)
