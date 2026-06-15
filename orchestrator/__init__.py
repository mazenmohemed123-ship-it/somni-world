"""
SOMNI Orchestrator — Python layer for world bootstrap, API, and state sync.
The C++ kernel handles all simulation logic; this layer handles:
  - World Bootstrap (questioning + world specification)
  - API server (player interaction)
  - State synchronization (kernel → external consumers)
"""
from .bootstrap.world_builder import WorldBuilder
from .simulation.runner import SimulationRunner

__version__ = "0.1.0"
__all__ = ["WorldBuilder", "SimulationRunner"]
