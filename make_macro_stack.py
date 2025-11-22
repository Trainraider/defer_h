#!/usr/bin/env python3
"""Generate macro stack for defer2.h scope management.

Usage:
    make_macro_stack.py <length> [fail|fallback] > <output_file>

"""

import sys
from typing import TextIO


def print_usage(stream: TextIO = sys.stderr) -> None:
    stream.write("Usage: {} <length> [fail|fallback] > <output_file>\n".format(sys.argv[0]))
    stream.write("\n")
    stream.write("  length     Number of top-level defer scopes supported (each consumes 2 stack slots)\n")
    stream.write("  fail       When stack depleted, cause compile error (default)\n")
    stream.write("  fallback   When stack depleted, fall back to global keyword redefinition\n")


def generate_macro_stack(length: int, mode: str, stream: TextIO = sys.stdout) -> None:
    if mode not in {"fail", "fallback"}:
        print(f"Error: Mode must be 'fail' or 'fallback'", file=sys.stderr)
        sys.exit(1)

    # Initial definition determines behavior when stack is depleted
    if mode == "fail":
        # Start with unsupported value to trigger compile error when depleted
        stream.write("#define IN_SCOPE ERROR_DEFER_SCOPE_STACK_DEPLETED\n")
    else:
        # Start with 1 to allow fallback to global keyword redefinition
        stream.write("#define IN_SCOPE 1\n")

    # Generate the push and define pairs
    for i in range(1, 2 * length + 2):  # inclusive of 2*length+1
        stream.write('#pragma push_macro("IN_SCOPE")\n')
        if i % 2 == 1:
            stream.write("#define IN_SCOPE 0\n")
        else:
            stream.write("#define IN_SCOPE 1\n")

    stream.write("#define SCOPE_MACRO_STACK_AVAILABLE\n")


def main(argv: list[str]) -> int:
    if len(argv) < 2 or len(argv) > 3:
        print_usage()
        return 1

    try:
        length = int(argv[1])
    except ValueError:
        print("Error: length must be an integer", file=sys.stderr)
        return 1

    mode = argv[2] if len(argv) == 3 else "fail"
    generate_macro_stack(length, mode)
    return 0


if __name__ == "__main__":  # pragma: no cover - CLI entry point
    raise SystemExit(main(sys.argv))
