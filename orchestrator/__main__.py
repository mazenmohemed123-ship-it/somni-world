"""
Run the SOMNI orchestrator API + viewer with:

    python -m orchestrator

Then open http://localhost:8080/ in your browser. No C++ build required —
if the compiled kernel (somni_core) isn't present, the pure-Python backend is
used automatically.
"""
from __future__ import annotations

import argparse
import sys


def main() -> None:
    ap = argparse.ArgumentParser(description="SOMNI orchestrator server")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--open", action="store_true", help="open the launcher in a browser")
    args = ap.parse_args()

    try:
        import uvicorn
    except ImportError:
        print("ERROR: uvicorn/fastapi not installed.\n"
              "Install with:  pip install fastapi uvicorn pydantic websockets",
              file=sys.stderr)
        sys.exit(1)

    from .api.server import create_app

    if args.open:
        import threading, webbrowser
        url = f"http://{args.host}:{args.port}/"
        threading.Timer(1.2, lambda: webbrowser.open(url)).start()

    print(f"\n  SOMNI is starting on http://{args.host}:{args.port}/")
    print("  Open that URL to pick a world and watch it simulate in 3D.\n")
    uvicorn.run(create_app(), host=args.host, port=args.port, log_level="info")


if __name__ == "__main__":
    main()
