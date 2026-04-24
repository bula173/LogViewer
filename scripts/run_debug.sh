#!/usr/bin/env bash
set -euo pipefail

# Run the debug build of LogViewer on macOS.
# Usage: ./scripts/run_debug.sh

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$REPO_ROOT/build/macos-debug-qt/src/main/LogViewer.app/Contents/MacOS/LogViewer"

if [[ ! -f "$APP" ]]; then
    echo "ERROR: Debug binary not found: $APP"
    echo "Build first with: cmake --build --preset macos-debug-build-qt"
    exit 1
fi

exec "$APP" "$@"
