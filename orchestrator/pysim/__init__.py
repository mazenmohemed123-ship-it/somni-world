"""
Pure-Python simulation backend.

This is the ZERO-BUILD fallback for the SOMNI orchestrator. When the compiled
C++ kernel (somni_core.pyd / .so) is not available — which is the common case
on Windows where building pybind11 + CMake + the DLL dependencies is painful —
the orchestrator transparently uses this deterministic Python kernel instead.

Same philosophy as the C++ core:
  * fully deterministic (seeded RNG, same seed => same world)
  * NO LLM in the simulation loop
  * agents/regions driven by numeric rules, not language

It is intentionally lighter than the C++ kernel (region-level statistical
simulation rather than per-NPC behavior trees), which keeps it fast in pure
Python and good enough to drive the 3D viewer and the analytics layer.
"""
from .kernel import PyKernel, is_available

__all__ = ["PyKernel", "is_available"]
