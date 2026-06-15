#!/usr/bin/env bash
# SOMNI kernel build script
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc)}"

echo "=== SOMNI Build ==="
echo "  Root:       $ROOT"
echo "  Build dir:  $BUILD_DIR"
echo "  Build type: $BUILD_TYPE"
echo "  Jobs:       $JOBS"

cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DSOMNI_BUILD_BINDINGS=ON \
    -DSOMNI_BUILD_TESTS=ON \
    "$ROOT"

cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo ""
echo "=== Build complete ==="
echo "  Kernel library: $BUILD_DIR/kernel/libsomni_kernel.a"
echo "  Python module:  $ROOT/orchestrator/somni_core*.so"
echo ""
echo "Run tests with:  cd $BUILD_DIR && ctest --output-on-failure"
echo "Start API with:  somni-api  (or: python -m orchestrator.api.server)"
