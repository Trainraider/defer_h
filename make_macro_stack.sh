#!/bin/bash
# Generate macro stack for defer2.h scope management
# Usage: $0 <length> [fail|fallback] > <output_file>

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 <length> [fail|fallback] > <output_file>" >&2
    echo "" >&2
    echo "  length     Number of top-level defer scopes supported (each consumes 2 stack slots)" >&2
    echo "  fail       When stack depleted, cause compile error (default)" >&2
    echo "  fallback   When stack depleted, fall back to global keyword redefinition" >&2
    exit 1
fi

length=$1
mode="${2:-fail}"

if [ "$mode" != "fail" ] && [ "$mode" != "fallback" ]; then
    echo "Error: Mode must be 'fail' or 'fallback'" >&2
    exit 1
fi

# Initial definition determines behavior when stack is depleted
if [ "$mode" = "fail" ]; then
    # Start with unsupported value to trigger compile error when depleted
    echo "#define IN_SCOPE ERROR_DEFER_SCOPE_STACK_DEPLETED"
else
    # Start with 1 to allow fallback to global keyword redefinition
    echo "#define IN_SCOPE 1"
fi

# Generate the push and define pairs
for ((i = 1; i <= 2 * length + 1; i++)); do
    echo '#pragma push_macro("IN_SCOPE")'
    if [ $((i % 2)) -eq 1 ]; then
        echo "#define IN_SCOPE 0"
    else
        echo "#define IN_SCOPE 1"
    fi
done

echo "#define SCOPE_MACRO_STACK_AVAILABLE"