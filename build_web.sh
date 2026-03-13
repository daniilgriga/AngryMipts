#!/usr/bin/env bash
# build_web.sh — Build AngryMipts as WebAssembly via Emscripten + Raylib
set -euo pipefail

EMSDK_PATH="${EMSDK:-$HOME/Programs/emsdk}"
BUILD_DIR="build_web"

on_interrupt() {
    echo ""
    echo "Interrupted. If the next run appears stuck, force-clean deps:"
    echo "  CLEAN_WEB_DEPS=1 bash build_web.sh"
    exit 130
}
trap on_interrupt INT

clean_fetchcontent_dep_if_broken() {
    local dep="$1"
    local dep_src="$BUILD_DIR/_deps/${dep}-src"
    local dep_build="$BUILD_DIR/_deps/${dep}-build"
    local dep_subbuild="$BUILD_DIR/_deps/${dep}-subbuild"

    # Typical broken state after Ctrl+C:
    # subbuild exists, but src was never fully cloned.
    if [ -d "$dep_subbuild" ] && [ ! -d "$dep_src" ]; then
        echo "Detected stale FetchContent state for '$dep' (subbuild without src), cleaning..."
        rm -rf "$dep_src" "$dep_build" "$dep_subbuild"
        return
    fi

    # Partially cloned repo (missing CMakeLists) also needs cleanup.
    if [ -d "$dep_src" ] && [ ! -f "$dep_src/CMakeLists.txt" ]; then
        echo "Detected broken source cache for '$dep', cleaning..."
        rm -rf "$dep_src" "$dep_build" "$dep_subbuild"
    fi
}

if [ ! -f "$EMSDK_PATH/emsdk_env.sh" ]; then
    echo "ERROR: emsdk not found at $EMSDK_PATH"
    echo "Set EMSDK env var or install to ~/Programs/emsdk"
    exit 1
fi

# Activate emsdk in this bash session
source "$EMSDK_PATH/emsdk_env.sh"

mkdir -p "$BUILD_DIR"

if [ "${CLEAN_WEB_DEPS:-0}" = "1" ]; then
    echo "CLEAN_WEB_DEPS=1 -> removing $BUILD_DIR/_deps"
    rm -rf "$BUILD_DIR/_deps"
fi

clean_fetchcontent_dep_if_broken "raylib"
clean_fetchcontent_dep_if_broken "box2d"
clean_fetchcontent_dep_if_broken "nlohmann_json"

echo "=== Configuring (Emscripten) ==="
emcmake cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DFETCHCONTENT_QUIET=OFF \
    --log-level=STATUS

echo "=== Building ==="
cmake --build "$BUILD_DIR" -- -j"$(nproc)"

echo ""
echo "=== Done ==="
echo "Output: $BUILD_DIR/AngryMipts.html"
echo ""
echo "To test locally:"
echo "  cd $BUILD_DIR && python3 -m http.server 8000"
echo "  Open http://localhost:8000/AngryMipts.html"
