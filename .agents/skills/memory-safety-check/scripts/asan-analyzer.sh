#!/bin/bash
# asan-analyzer.sh
# Parse ASan output and generate a structured report of memory issues
#
# Usage: ./asan-analyzer.sh <input-file> [output-file]
# Example: ctest --preset macos-asan-test-qt 2>&1 | tee asan.log
#          ./asan-analyzer.sh asan.log report.md

set -euo pipefail

INPUT_FILE="${1:--}"  # Default to stdin
OUTPUT_FILE="${2:-}"

# Temp files
ERRORS_FILE=$(mktemp)
LEAKS_FILE=$(mktemp)
SUMMARY_FILE=$(mktemp)

trap "rm -f $ERRORS_FILE $LEAKS_FILE $SUMMARY_FILE" EXIT

# Extract all ASan errors
echo "=== Analyzing ASan Output ===" >&2

# Parse errors
grep -E "ERROR: AddressSanitizer:" "$INPUT_FILE" 2>/dev/null | sort | uniq -c | sort -rn > "$ERRORS_FILE" || true

# Parse leaks
grep -E "SUMMARY: AddressSanitizer.*leaked" "$INPUT_FILE" 2>/dev/null > "$LEAKS_FILE" || true

# Count totals
ERROR_COUNT=$(wc -l < "$ERRORS_FILE")
LEAK_COUNT=$(wc -l < "$LEAKS_FILE")

# Generate report
{
    echo "# ASan Analysis Report"
    echo ""
    echo "**Generated**: $(date)"
    echo ""
    
    if [[ $ERROR_COUNT -gt 0 ]]; then
        echo "## 🔴 Memory Errors Detected: $ERROR_COUNT"
        echo ""
        echo "### Error Summary"
        echo ""
        echo '| Count | Error Type |'
        echo '|-------|------------|'
        while read -r line; do
            count=$(echo "$line" | awk '{print $1}')
            error=$(echo "$line" | sed 's/^[[:space:]]*[0-9]*[[:space:]]*//' | sed 's/ERROR: AddressSanitizer: //')
            printf '| %d | `%s` |\n' "$count" "$error"
        done < "$ERRORS_FILE"
        echo ""
    else
        echo "## ✅ No Memory Errors"
        echo ""
    fi
    
    if [[ $LEAK_COUNT -gt 0 ]]; then
        echo "## 💧 Memory Leaks"
        echo ""
        while read -r line; do
            echo "- $line"
        done < "$LEAKS_FILE"
        echo ""
    else
        echo "## ✅ No Memory Leaks"
        echo ""
    fi
    
    echo "## Detailed Analysis"
    echo ""
    echo "For detailed stack traces and allocation sites, check the full test output."
    echo ""
    
    # Extract unique error types with first occurrence stack trace
    echo "### Sample Error Locations"
    echo ""
    grep -A 3 "ERROR: AddressSanitizer:" "$INPUT_FILE" 2>/dev/null | head -30 || echo "No errors to display"
    
} > "$SUMMARY_FILE"

# Output report
if [[ -n "$OUTPUT_FILE" ]]; then
    cat "$SUMMARY_FILE" > "$OUTPUT_FILE"
    echo "✅ Report written to: $OUTPUT_FILE" >&2
else
    cat "$SUMMARY_FILE"
fi
