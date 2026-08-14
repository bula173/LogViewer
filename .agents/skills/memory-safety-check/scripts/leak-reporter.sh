#!/bin/bash
# leak-reporter.sh
# Extract and summarize memory leaks from ASan output
# Groups leaks by allocation site and function
#
# Usage: ./leak-reporter.sh <input-file> [output-file]

set -euo pipefail

INPUT_FILE="${1:--}"
OUTPUT_FILE="${2:-}"

TEMP_FILE=$(mktemp)
trap "rm -f $TEMP_FILE" EXIT

# Extract leak information
{
    echo "# Memory Leak Report"
    echo ""
    echo "**Generated**: $(date)"
    echo ""
    
    # Leak summary
    LEAK_SUMMARY=$(grep -E "SUMMARY: AddressSanitizer.*leaked" "$INPUT_FILE" 2>/dev/null || echo "No leaks found")
    echo "## Leak Summary"
    echo ""
    echo "**$LEAK_SUMMARY**"
    echo ""
    
    # Leaks by allocation site
    echo "## Leaks by Allocation Site"
    echo ""
    
    # Extract leak detail sections
    grep -B 5 "LeakSanitizer: Directly lost" "$INPUT_FILE" 2>/dev/null | grep -E "(LeakSanitizer|^ +#|in [a-zA-Z_])" > "$TEMP_FILE" || true
    
    if [[ -s "$TEMP_FILE" ]]; then
        echo '```'
        cat "$TEMP_FILE"
        echo '```'
    else
        echo "No detailed leak information available."
    fi
    
    echo ""
    echo "## How to Investigate"
    echo ""
    echo "1. **Identify the allocation site**: Look for the \`#0\` frame showing where memory was allocated"
    echo "2. **Check for missing cleanup**: Look for a missing \`delete\` or \`free\` in the corresponding cleanup path"
    echo "3. **Use smart pointers**: Replace raw \`new\`/\`delete\` with \`std::unique_ptr\` or \`std::shared_ptr\`"
    echo "4. **Verify error paths**: Ensure error handling doesn't skip cleanup"
    echo "5. **Use RAII**: Let destructors manage cleanup automatically"
    echo ""
    
    echo "## Suppression (if leak is acceptable)"
    echo ""
    echo "If a leak is acceptable (e.g., global initialization), suppress it:"
    echo ""
    echo '```'
    echo 'leak:MyFunction'
    echo '```'
    echo ""
    echo "Then run with: \`LSAN_OPTIONS=suppressions=suppressions.txt ctest ...\`"
    
} > "$TEMP_FILE"

if [[ -n "$OUTPUT_FILE" ]]; then
    cat "$TEMP_FILE" > "$OUTPUT_FILE"
    echo "✅ Report written to: $OUTPUT_FILE" >&2
else
    cat "$TEMP_FILE"
fi
