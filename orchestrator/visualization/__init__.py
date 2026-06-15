"""
SOMNI Visualization Layer — optional, never part of simulation core.
Uses free open-source tools to render world state.

Available backends:
  - rerun   : real-time 3D timeline visualization (pip install rerun-sdk)
  - pyvista : 3D terrain mesh rendering         (pip install pyvista)
  - ascii   : terminal heatmap, zero dependencies (always available)
"""
from .world_renderer import WorldRenderer

__all__ = ["WorldRenderer"]
