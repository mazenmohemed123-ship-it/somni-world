#!/usr/bin/env bash
# SOMNI environment bootstrap — installs system deps + Python env
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== SOMNI Environment Bootstrap ==="

# System packages (Debian/Ubuntu)
if command -v apt-get &>/dev/null; then
    echo "Installing system packages..."
    sudo apt-get update -qq
    sudo apt-get install -y \
        cmake build-essential ninja-build git \
        python3 python3-pip python3-venv \
        libssl-dev libfmt-dev
fi

# Python virtual environment
VENV="$ROOT/.venv"
if [ ! -d "$VENV" ]; then
    echo "Creating Python venv at $VENV..."
    python3 -m venv "$VENV"
fi

echo "Installing Python dependencies..."
"$VENV/bin/pip" install -q --upgrade pip
"$VENV/bin/pip" install -q -e "$ROOT[dev]"

echo ""
echo "=== Bootstrap complete ==="
echo ""
echo "Next steps:"
echo "  1. Activate venv:  source $VENV/bin/activate"
echo "  2. Build kernel:   ./scripts/build.sh"
echo "  3. Run tests:      cd build && ctest --output-on-failure"
echo "  4. Start API:      somni-api"
echo "  5. Create world:   curl -X POST http://localhost:8080/world/create \\"
echo "                       -H 'Content-Type: application/json' \\"
echo "                       -d '{\"scenario\": \"What if Rome never fell?\", \"scale\": \"medium\"}'"
