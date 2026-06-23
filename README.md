# SOMNI — Living Simulation Kernel

A deterministic, real-time engine for simulating living worlds: procedural
terrain, factions, economies, conflicts, and autonomous NPCs — **with no LLM in
the simulation loop**. Same seed always produces the same world.

---

## Quick start (zero install — just open a file)

The fastest way to use SOMNI needs **no Python, no server, no build** — just a
web browser.

### Open it
- **Windows** — double-click **`SOMNI.html`** (or `start.bat`)
- **macOS / Linux** — double-click **`SOMNI.html`** (or run `./start.sh`)

That's it. The file opens in your browser, you pick a world, and it simulates
live in 3D. Then you **command the world**: type things like *declare war on
the second faction*, *send food to my people*, *raise an army*, *unleash a
disaster* — in English or Arabic — and the world reacts in real time.

Everything (world generation, the simulation loop, factions, wars, economy,
and 3D rendering) runs **inside the browser**. It is fully deterministic — the
same world always plays out the same way.

> The only thing it loads from the internet is the Three.js 3D library from a
> CDN. For the richer per-region backend, use the optional Python server below.

---

## Advanced: the Python server (optional, richer backend)

There is also a Python backend with a per-region statistical simulation and a
REST + WebSocket API, serving `viewer/launcher.html` + `viewer/index.html`. No
C++ build needed — if `somni_core` isn't present it uses the pure-Python kernel.

```bash
pip install fastapi uvicorn pydantic websockets
python somni.py
```

Then open <http://localhost:8080/>.

---

## What you can do

- **Pick a preset world** — Viking Age, Bronze Age Collapse, Silk Road,
  Dragonreach, Martian City-States, and more (search + tag filters).
- **Create a custom world** — describe any scenario in plain words.
- **Watch it live in 3D** — terrain, biomes, factions, day/night, population.
- **Influence it** — trigger disasters, add resources (player commands).

---

## Two backends, one API

| | Pure-Python backend (default) | C++ kernel (optional) |
|---|---|---|
| Build needed | **None** | CMake + compiler |
| Simulation | Region-level statistical | Per-NPC behavior trees |
| Scale | Thousands | Millions |
| Determinism | ✅ | ✅ |
| Use when | You just want it to run | You need full fidelity / scale |

The orchestrator picks the C++ kernel automatically **if** `somni_core` is
importable; otherwise it falls back to Python. Same REST/WebSocket API either
way, so the 3D viewer doesn't care which one is running.

---

## Building the C++ kernel (optional, for full fidelity)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# copy the produced somni_core.* next to the orchestrator, then run as above
```

Run the C++ tests:
```bash
cmake -B build -DSOMNI_BUILD_TESTS=ON && cmake --build build && ./build/tests/cpp/somni_tests
```

---

## Project layout

```
kernel/         C++ simulation core (deterministic math, LOD, event buses,
                behavior trees, factions, economy, conflict)
orchestrator/   Python orchestration
  pysim/        pure-Python fallback kernel  (zero-build backend)
  bootstrap/    world schema, presets, world builder
  simulation/   runner + state sync
  api/          FastAPI REST + WebSocket server (also serves the viewer)
  visualization/analytics/   rendering + DuckDB/Polars analytics
viewer/         launcher.html (search/presets hub) + index.html (3D viewer)
tests/          C++ (Catch2) + Python (pytest)
```

## Run the tests
```bash
PYTHONPATH=. python -m pytest tests/python -q     # 46 tests
```
