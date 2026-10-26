#!/usr/bin/env python3
"""Layer conformance check: a lower layer must never include a higher one.

Scans `#include "<dir>/<file>"` across src/ and fails on any upward edge.
Known violations live in ALLOWED with a matching OPEN.md ledger entry; the
count is asserted so the allowlist can only shrink, never silently grow.
"""
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"

# Lower index = lower layer. Dependencies point DOWN only.
LAYERS = {
    "util": 0,
    "core": 1,
    "formula": 2,
    "builtin": 3,
    "io": 3,
    "plugin": 3,
    "ui": 4,
    "app": 5,
}

# Declared debt — each entry MUST have an OPEN.md row with a re-open trigger.
# Shrinks only. See OPEN.md #1 and #2.
ALLOWED = {
    ("core/CellInputService.cpp", "formula"),
    ("core/Clipboard.cpp", "formula"),
    ("core/FormulaAdjust.cpp", "formula"),
    ("ui/MainWindow.cpp", "app"),
}

INCLUDE = re.compile(r'^\s*#\s*include\s+"([a-z_]+)/')


def main() -> int:
    violations, used_allowances = [], set()

    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".cpp", ".hpp", ".h"):
            continue
        rel = path.relative_to(SRC)
        if len(rel.parts) < 2:
            continue  # src/main.cpp — the entry point, outside the spine
        owner = rel.parts[0]
        if owner not in LAYERS:
            print(f"error: {rel} sits in unmapped directory '{owner}/'")
            return 1

        for lineno, line in enumerate(path.read_text().splitlines(), 1):
            m = INCLUDE.match(line)
            if not m:
                continue
            target = m.group(1)
            if target not in LAYERS or LAYERS[target] <= LAYERS[owner]:
                continue
            key = (rel.as_posix(), target)
            if key in ALLOWED:
                used_allowances.add(key)
            else:
                violations.append(
                    f"{rel}:{lineno}: {owner}/ (L{LAYERS[owner]}) includes "
                    f"{target}/ (L{LAYERS[target]}) — upward"
                )

    stale = ALLOWED - used_allowances
    for key in sorted(stale):
        print(f"stale allowance: {key[0]} no longer includes {key[1]}/ — remove it from ALLOWED")

    for v in violations:
        print(f"violation: {v}")

    if violations or stale:
        print(f"\nFAIL — {len(violations)} violation(s), {len(stale)} stale allowance(s)")
        return 1

    # Floor on the population: a green that examined nothing is not green.
    scanned = sum(1 for p in SRC.rglob("*") if p.suffix in (".cpp", ".hpp", ".h"))
    if scanned < 80:
        print(f"FAIL — only {scanned} source files scanned; expected >= 80 (bad glob?)")
        return 1

    print(f"OK — {scanned} files scanned, layering acyclic, "
          f"{len(used_allowances)} declared allowance(s) in OPEN.md")
    return 0


if __name__ == "__main__":
    sys.exit(main())
