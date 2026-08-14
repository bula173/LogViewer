#!/bin/bash
# run-all-sanitizers.sh
# Build and run tests under all available sanitizers (ASan, MSan, UBSan)
# Generates a summary report
#
# Usage: ./run-all-sanitizers.sh [output-dir]

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
OUTPUT_DIR="${1:-.}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_DIR="$OUTPUT_DIR/sanitizer-reports-$TIMESTAMP"

mkdir -p "$REPORT_DIR"

echo "🔬 Running All Sanitizer Tests"
echo "📁 Reports will be saved to: $REPORT_DIR"
echo ""

# Function to run sanitizer tests
run_sanitizer() {
    local name=$1
    local preset_build=$2
    local preset_test=$3
    
    echo "=================================================="
    echo "🧪 Running $name..."
    echo "=================================================="
    
    local log_file="$REPORT_DIR/$name.log"
    local report_file="$REPORT_DIR/$name-report.md"
    
    cd "$PROJECT_ROOT"
    
    # Build
    echo "   Building with $name..."
    if ! cmake --build --preset "$preset_build" 2>&1 | tee -a "$log_file"; then
        echo "   ❌ Build failed for $name"
        return 1
    fi
    
    # Test
    echo "   Running tests with $name..."
    if ctest --preset "$preset_test" --output-on-failure 2>&1 | tee -a "$log_file"; then
        echo "   ✅ $name: All tests passed"
    else
        echo "   ⚠️  $name: Some tests failed (check log)"
    fi
    
    # Generate report if analyzer script exists
    if [[ -f "$PROJECT_ROOT/.agents/skills/memory-safety-check/scripts/asan-analyzer.sh" ]]; then
        bash "$PROJECT_ROOT/.agents/skills/memory-safety-check/scripts/asan-analyzer.sh" "$log_file" "$report_file" || true
    fi
    
    echo ""
}

# Track results
RESULTS=()

# ASan (Address Sanitizer)
if run_sanitizer "ASan" "macos-asan-build-qt" "macos-asan-test-qt"; then
    RESULTS+=("✅ ASan")
else
    RESULTS+=("❌ ASan")
fi

# MSan (Memory Sanitizer) - may not be available on macOS
if run_sanitizer "MSan" "macos-msan-build-qt" "macos-msan-test-qt" 2>/dev/null || true; then
    RESULTS+=("✅ MSan")
else
    RESULTS+=("⏭️  MSan (not available on this platform)")
fi

# UBSan (Undefined Behavior Sanitizer) - may not be in separate preset
if run_sanitizer "UBSan" "macos-ubsan-build-qt" "macos-ubsan-test-qt" 2>/dev/null || true; then
    RESULTS+=("✅ UBSan")
else
    RESULTS+=("⏭️  UBSan (not available)")
fi

# Summary report
echo ""
echo "=================================================="
echo "📊 SANITIZER TEST SUMMARY"
echo "=================================================="
echo ""

for result in "${RESULTS[@]}"; do
    echo "$result"
done

echo ""
echo "📁 Detailed reports: $REPORT_DIR"
echo "   - ASan details: $REPORT_DIR/ASan-report.md"
echo "   - Full logs: $REPORT_DIR/*.log"
echo ""

# Generate index report
{
    echo "# Sanitizer Test Results"
    echo ""
    echo "**Date**: $(date)"
    echo "**Project**: $PROJECT_ROOT"
    echo ""
    echo "## Summary"
    echo ""
    for result in "${RESULTS[@]}"; do
        echo "- $result"
    done
    echo ""
    echo "## Reports"
    echo ""
    ls -lh "$REPORT_DIR"/*.log 2>/dev/null | awk '{print "- `" $9 "` (" $5 ")"}' || echo "No logs found"
    echo ""
    echo "## Next Steps"
    echo ""
    echo "1. Review the detailed reports in this directory"
    echo "2. For each error, check the allocation site and call stack"
    echo "3. Implement fixes using RAII and smart pointers"
    echo "4. Re-run tests to verify fixes"
    
} > "$REPORT_DIR/INDEX.md"

echo "✅ Summary: $REPORT_DIR/INDEX.md"
