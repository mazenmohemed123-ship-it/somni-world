#!/usr/bin/env bash
# SOMNI - one-command launcher for macOS / Linux.
# No C++ build required: the pure-Python backend is used automatically
# when the compiled kernel (somni_core) isn't present.
set -e
cd "$(dirname "$0")"
echo
echo "  Starting SOMNI..."
echo
exec python3 somni.py
