#!/usr/bin/env bash
set -euo pipefail

ROOT="${INSTALL_SMOKE_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
BUILD_DIR="${INSTALL_SMOKE_BUILD_DIR:-$ROOT/build}"
INSTALL_PREFIX="${INSTALL_SMOKE_PREFIX:-$ROOT/build-cmake/install-prefix}"
CONSUMER_BUILD_DIR="${INSTALL_SMOKE_CONSUMER_BUILD_DIR:-$ROOT/build-cmake/install-smoke}"

usage() {
    cat <<EOF
Usage: $(basename "$0") [extra cmake configure args for consumer build...]

Installs prebyte-engine into a prefix and verifies find_package consumption.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    printf 'Build directory not configured: %s\n' "$BUILD_DIR" >&2
    exit 1
fi

rm -rf "$INSTALL_PREFIX" "$CONSUMER_BUILD_DIR"
cmake --install "$BUILD_DIR" --prefix "$INSTALL_PREFIX"

CMAKE_ARGS=(
    -S "$ROOT/tests/cmake-consumer"
    -B "$CONSUMER_BUILD_DIR"
    -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX"
)

if [[ -n "${CMAKE_TOOLCHAIN_FILE:-}" ]]; then
    CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE")
fi

if [[ -n "${VCPKG_TARGET_TRIPLET:-}" ]]; then
    CMAKE_ARGS+=(-DVCPKG_TARGET_TRIPLET="$VCPKG_TARGET_TRIPLET")
fi

cmake "${CMAKE_ARGS[@]}" "$@"
cmake --build "$CONSUMER_BUILD_DIR" --parallel

if [[ -d "$INSTALL_PREFIX/lib64" ]]; then
    export LD_LIBRARY_PATH="$INSTALL_PREFIX/lib64:${LD_LIBRARY_PATH:-}"
elif [[ -d "$INSTALL_PREFIX/lib" ]]; then
    export LD_LIBRARY_PATH="$INSTALL_PREFIX/lib:${LD_LIBRARY_PATH:-}"
fi

"$CONSUMER_BUILD_DIR/prebyte_engine_cpp_smoke"
"$CONSUMER_BUILD_DIR/prebyte_engine_c_smoke"

printf 'Install smoke passed (prefix: %s)\n' "$INSTALL_PREFIX"
