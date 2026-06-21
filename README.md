# SOMNI — Living Simulation Kernel

A deterministic, real-time engine for simulating living worlds: procedural
terrain, factions, economies, conflicts, and autonomous NPCs — **with no LLM in
the simulation loop**. Same seed always produces the same world.

---

## 🚀 Quick start (no build required)

You do **not** need to compile any C++ to run SOMNI. If the compiled kernel
isn't present, SOMNI automatically uses a pure-Python simulation backend.

### Windows
Double-click **`start.bat`** — that's it.

### macOS / Linux
```bash
./start.sh
```

### Any platform (manual)
```bash
pip install fastapi uvicorn pydantic websockets
python somni.py
```

Your browser opens the **World Launcher** at <http://localhost:8080/>.
Search for a world (e.g. *viking*, *dragons*, *mars*), click it, and watch it
simulate live in 3D.

> No `somni_core.pyd`, no DLLs, no CMake, no PATH headaches. The Python backend
> runs everywhere Python runs.

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
