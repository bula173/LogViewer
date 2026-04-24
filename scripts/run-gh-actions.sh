#!/usr/bin/env bash
set -euo pipefail
# run-gh-actions.sh
#
# Simulate GitHub Actions workflows locally.
#
# On Linux  : uses `act` (nektos/act) with Docker to run the exact workflow YAML.
# On macOS  : Docker cannot run macOS runners, so we execute the equivalent cmake
#             presets directly (no Docker required).
#
# Prerequisites:
#   Linux  – Docker daemon running, `act` installed (brew install act / cargo install act)
#   macOS  – cmake, clang, Qt 6 in PATH (same as normal build)
#
# Usage:
#   ./scripts/run-gh-actions.sh [workflow] [options]
#
# Workflows:
#   build          Build debug + release (default)
#   quality        clang-tidy + cppcheck + format check
#   asan           Build and test with AddressSanitizer
#   tsan           Build and test with ThreadSanitizer
#   test           Run CTest on the debug build
#   all            Run all of the above in sequence
#
# Options:
#   --list         List available workflows and exit
#   --dry-run      Print commands without executing them
#   -h, --help     Show this help

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

PLATFORM="$(uname -s)"
DRY_RUN=false
WORKFLOW="${1:-build}"

# ── Helpers ────────────────────────────────────────────────────────────────────

run() {
    if $DRY_RUN; then
        echo "[dry-run] $*"
    else
        echo "==> $*"
        "$@"
    fi
}

header() { echo; echo "━━━ $* ━━━"; }

require_cmd() {
    if ! command -v "$1" &>/dev/null; then
        echo "ERROR: '$1' not found. $2"
        exit 1
    fi
}

# ── Argument parsing ───────────────────────────────────────────────────────────

for arg in "$@"; do
    case "$arg" in
        --dry-run) DRY_RUN=true ;;
        --list)
            echo "Available workflows: build quality asan tsan test all"
            exit 0
            ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
    esac
done

# First positional arg is workflow; strip option flags
WORKFLOW="build"
for arg in "$@"; do
    case "$arg" in
        --*|-h) ;;
        *) WORKFLOW="$arg"; break ;;
    esac
done

# ── macOS native execution ─────────────────────────────────────────────────────

run_macos_build() {
    header "macOS – build (debug + release)"
    run cmake --preset macos-debug-qt
    run cmake --build --preset macos-debug-build-qt
    run cmake --preset macos-release-qt
    run cmake --build --preset macos-release-build-qt
}

run_macos_quality() {
    header "macOS – quality (checks preset: clang-tidy + cppcheck)"
    run cmake --preset macos-checks-qt
    run cmake --build --preset macos-checks-build-qt
    header "macOS – format check"
    run cmake --build --preset macos-format-check || {
        echo "NOTICE: Format check found issues. Run 'cmake --build --preset macos-format-apply' to fix."
        return 1
    }
}

run_macos_asan() {
    header "macOS – ASan build + test"
    run cmake --preset macos-asan-qt
    run cmake --build --preset macos-asan-build-qt
    ASAN_OPTIONS="detect_leaks=1:halt_on_error=1" \
    UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
    run ctest --preset macos-asan-test-qt --output-on-failure
}

run_macos_tsan() {
    header "macOS – TSan build + test"
    run cmake --preset macos-tsan-qt
    run cmake --build --preset macos-tsan-build-qt
    TSAN_OPTIONS="halt_on_error=1" \
    run ctest --preset macos-tsan-test-qt --output-on-failure
}

run_macos_test() {
    header "macOS – CTest (debug)"
    run cmake --preset macos-debug-qt
    run cmake --build --preset macos-debug-build-qt
    run ctest --preset macos-debug-test-qt --output-on-failure
}

run_macos_workflow() {
    case "$WORKFLOW" in
        build)   run_macos_build ;;
        quality) run_macos_quality ;;
        asan)    run_macos_asan ;;
        tsan)    run_macos_tsan ;;
        test)    run_macos_test ;;
        all)
            run_macos_build
            run_macos_quality
            run_macos_asan
            run_macos_test
            ;;
        *)
            echo "Unknown workflow: $WORKFLOW"
            echo "Available: build quality asan tsan test all"
            exit 1
            ;;
    esac
}

# ── Linux via `act` ────────────────────────────────────────────────────────────

ACT_IMAGE="catthehacker/ubuntu:act-22.04"

# Map our logical workflow names to .github/workflows/*.yml job IDs
act_workflow_for() {
    case "$1" in
        build)   echo ".github/workflows/build.yml" ;;
        quality) echo ".github/workflows/quality.yml" ;;
        asan)    echo ".github/workflows/sanitizers.yml" ;;
        tsan)    echo ".github/workflows/sanitizers.yml" ;;
        test)    echo ".github/workflows/build.yml" ;;
        all)     echo "all" ;;
        *)       echo "" ;;
    esac
}

run_linux_workflow() {
    require_cmd act "Install via: brew install act  OR  curl -s https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash"
    require_cmd docker "Install Docker Desktop: https://docs.docker.com/desktop/"

    if [[ "$WORKFLOW" == "all" ]]; then
        for wf in build quality asan; do
            WF_FILE="$(act_workflow_for "$wf")"
            header "act – $WF_FILE"
            run act --platform "ubuntu-latest=$ACT_IMAGE" \
                    --workflows "$WF_FILE" \
                    --artifact-server-path /tmp/act-artifacts \
                    push
        done
    else
        WF_FILE="$(act_workflow_for "$WORKFLOW")"
        if [[ -z "$WF_FILE" ]]; then
            echo "Unknown workflow: $WORKFLOW"
            echo "Available: build quality asan tsan test all"
            exit 1
        fi

        EXTRA_ARGS=()
        # For tsan, pass job filter if workflow supports it
        if [[ "$WORKFLOW" == "tsan" ]]; then
            EXTRA_ARGS+=(--job tsan)
        fi

        header "act – $WF_FILE"
        run act --platform "ubuntu-latest=$ACT_IMAGE" \
                --workflows "$WF_FILE" \
                --artifact-server-path /tmp/act-artifacts \
                "${EXTRA_ARGS[@]}" \
                push
    fi
}

# ── Main ───────────────────────────────────────────────────────────────────────

echo "Platform : $PLATFORM"
echo "Workflow : $WORKFLOW"
$DRY_RUN && echo "Mode     : dry-run"
echo

case "$PLATFORM" in
    Darwin)
        echo "macOS detected – running cmake presets directly (no Docker needed)"
        run_macos_workflow
        ;;
    Linux)
        echo "Linux detected – delegating to 'act' (Docker-based)"
        run_linux_workflow
        ;;
    *)
        echo "Unsupported platform: $PLATFORM"
        echo "Please run cmake presets manually. See README for details."
        exit 1
        ;;
esac

echo
echo "Done."
