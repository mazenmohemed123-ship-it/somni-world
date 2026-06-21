#!/usr/bin/env python3
"""
SOMNI — one-command launcher.

    python somni.py

What it does:
  1. Checks that the Python web dependencies are installed (offers to install
     them for you if they're missing).
  2. Starts the SOMNI server.
  3. Opens the World Launcher in your browser.

NO C++ build is required. If the compiled kernel (somni_core) is not present,
SOMNI automatically uses its pure-Python simulation backend — so this works
out of the box on Windows, macOS and Linux with just Python installed.
"""
from __future__ import annotations

import importlib.util
import subprocess
import sys

REQUIRED = ["fastapi", "uvicorn", "pydantic"]


def ensure_deps() -> None:
    missing = [m for m in REQUIRED if importlib.util.find_spec(m) is None]
    if not missing:
        return
    print(f"Missing Python packages: {', '.join(missing)}")
    try:
        ans = input("Install them now with pip? [Y/n] ").strip().lower()
    except EOFError:
        ans = "y"
    if ans in ("", "y", "yes"):
        subprocess.check_call(
            [sys.executable, "-m", "pip", "install", *missing, "websockets"])
    else:
        print("Cannot start without these packages. Exiting.")
        sys.exit(1)


def main() -> None:
    ensure_deps()
    # Defer import until deps are guaranteed present.
    from orchestrator.__main__ import main as run_server
    sys.argv = ["somni", "--open"]
    run_server()


if __name__ == "__main__":
    main()
