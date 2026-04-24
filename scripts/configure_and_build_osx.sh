#!/usr/bin/env bash
set -euo pipefail

# Configure and build LogViewer on macOS using CMake presets.
# Usage: ./scripts/configure_and_build_osx.sh [debug|release|asan]
#
# Presets map:
#   debug   -> macos-debug-qt   + macos-debug-build-qt
#   release -> macos-release-qt + macos-release-build-qt
#   asan    -> macos-asan-qt    + macos-asan-build-qt

PRESET="${1:-debug}"

case "$PRESET" in
    debug)
        CONFIGURE_PRESET="macos-debug-qt"
        BUILD_PRESET="macos-debug-build-qt"
        ;;
    release)
        CONFIGURE_PRESET="macos-release-qt"
        BUILD_PRESET="macos-release-build-qt"
        ;;
    asan)
        CONFIGURE_PRESET="macos-asan-qt"
        BUILD_PRESET="macos-asan-build-qt"
        ;;
    *)
        echo "Unknown preset: $PRESET"
        echo "Usage: $0 [debug|release|asan]"
        exit 1
        ;;
esac

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

echo "==> Configuring: $CONFIGURE_PRESET"
cmake --preset "$CONFIGURE_PRESET"

echo "==> Building: $BUILD_PRESET"
cmake --build --preset "$BUILD_PRESET"

echo "==> Done. Binary: build/$CONFIGURE_PRESET/src/main/LogViewer.app/Contents/MacOS/LogViewer"
