#!/usr/bin/env bash
# SOMNI - one-click launcher for macOS / Linux.
# Opens the standalone SOMNI.html in your default browser.
# No Python, no server, no install needed.
cd "$(dirname "$0")"
echo
echo "  Opening SOMNI in your browser..."
echo
if command -v xdg-open >/dev/null 2>&1; then
  xdg-open "SOMNI.html"
elif command -v open >/dev/null 2>&1; then
  open "SOMNI.html"
else
  echo "  Could not auto-open. Please open SOMNI.html manually in your browser."
fi
