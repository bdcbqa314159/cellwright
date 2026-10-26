#!/usr/bin/env bash
# Local equivalent of the CI pipeline. Run this BEFORE pushing — a CI round trip
# is ~15 minutes and a lost runner tells you nothing.
#
#   tools/verify.sh           reuse build/ (fast, incremental)
#   tools/verify.sh --clean   configure from scratch, as CI does
#
# Mirrors .github/workflows/ci.yml step for step. Keep the two in sync: if a step
# is added there, add it here, or "verified locally" stops meaning anything.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD=build
VENV=.venv
CLEAN=0
[[ "${1:-}" == "--clean" ]] && CLEAN=1

step() { printf '\n\033[1m── %s\033[0m\n' "$1"; }
fail() { printf '\033[31mFAIL: %s\033[0m\n' "$1" >&2; exit 1; }

step "Layer conformance"
python3 tools/check_layers.py || fail "layering"

if (( CLEAN )); then
    step "Clean configure (CI-equivalent)"
    rm -rf "$BUILD"
fi

step "Configure"
cmake -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release > /tmp/cw_cfg.log 2>&1 \
    || { tail -30 /tmp/cw_cfg.log; fail "configure"; }

step "Build"
cmake --build "$BUILD" --parallel > /tmp/cw_build.log 2>&1 \
    || { grep -E "error|FAILED" /tmp/cw_build.log | head -20; fail "build"; }

# -Wall/-Wextra are on our targets and OUR sources are warning-clean, so any warning
# from them is a regression. Dependencies compile with their own flags and warn in
# their own code (googletest's gtest-printers.h emits -Wcharacter-conversion and
# predates our flags entirely), and the macOS SDK warns too — neither is ours to fix.
# Narrow this filter and the gate fails on other people's code; widen it and the gate
# goes blind. Negative-tested in both directions.
#
# Written to a file with `|| true` rather than counted in a pipeline: under
# `set -euo pipefail` a grep that matches nothing exits 1 and kills the script, so the
# pipeline form failed precisely when the tree was clean — the expected steady state.
grep -E 'warning:' /tmp/cw_build.log 2>/dev/null \
    | grep -vE '_deps/|\.sdk/|^ld: ' > /tmp/cw_warn.log || true
WARN=$(wc -l < /tmp/cw_warn.log | tr -d ' ')
if (( WARN > 0 )); then
    head -20 /tmp/cw_warn.log
    fail "$WARN warning(s) in our sources — the tree is meant to be warning-clean"
fi
echo "0 warnings in our sources"

step "C++ tests"
# --no-tests=error: a run that collected nothing must not report green.
ctest --test-dir "$BUILD" --build-config Release --output-on-failure --no-tests=error \
    || fail "ctest"

step "Python tests"
if [[ ! -x "$VENV/bin/python" ]]; then
    echo "creating $VENV"
    python3.12 -m venv "$VENV"
    "$VENV/bin/pip" install -q -r requirements.txt
fi
"$VENV/bin/python" -m pytest tests/python -q || fail "pytest"

printf '\n\033[32mALL GREEN — safe to push\033[0m\n'
