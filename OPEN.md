# OPEN.md — debt ledger

Every suppression, skipped test, layering allowance, and load-bearing shortcut lives
here with a rationale and a **re-open trigger**, or it does not exist (`CLAUDE.md` §3).

Invariant: `tools/check_layers.py` allowances − layering rows below = 0. Currently **4 − 4 = 0**.

---

## Layering allowances

### #1 — `core/` reaches up into `formula/` (3 files, 6 includes)

| | |
|---|---|
| **Allowances** | `core/CellInputService.cpp` → `formula` · `core/Clipboard.cpp` → `formula` · `core/FormulaAdjust.cpp` → `formula` |
| **Edge** | L1 → L2, upward |
| **Why it exists** | These three are not core data structures — they are *services* that orchestrate core **and** formula. `CellInputService` tokenizes, parses, evaluates, and updates the dependency graph; `FormulaAdjust` rewrites references on row/column shift; `Clipboard` rewrites formulas on paste. They were filed under `core/` because they operate on `Sheet`, but they sit above both layers. |
| **Fix** | Move all three into a new `src/services/` layer at L3 (above `formula`, below `ui`). No behaviour change — a pure move plus include/CMake updates. |
| **Re-open trigger** | Immediately — this is the next scheduled PR after the scaffold lands. Also re-open if a 4th file in `core/` needs `formula/`: that is the signal the service layer is overdue, and it must **not** be added to the allowlist. |
| **Risk if left** | `core` cannot be tested or reused without the formula engine, and the dependency direction that makes the engine substitutable is already broken. |

### #2 — `ui/MainWindow.cpp` reaches up into `app/` (2 includes)

| | |
|---|---|
| **Allowances** | `ui/MainWindow.cpp` → `app` (`app/AppState.hpp`, `app/AutoSave.hpp`) |
| **Edge** | L4 → L5, upward |
| **Why it exists** | `AppState` is the shared session/document state and `AutoSave` the recovery timer; both were filed under `app/` next to the shell (`App`, `Settings`). `MainWindow` legitimately needs the state it renders — the misplacement is `AppState`'s, not `MainWindow`'s. |
| **Fix** | Move `AppState` and `AutoSave` **down** out of `app/`, leaving `app/` as the shell only (`App`, `Settings`). Candidate home: the same `src/services/` layer from #1, or `src/state/` at L3. Decide when #1 lands so the two moves share one layer. |
| **Re-open trigger** | The PR immediately after #1. Also re-open if any other `ui/` file needs `app/`. |
| **Risk if left** | The UI layer is welded to the application shell, so no panel can be tested or hosted outside the full app. |

---

## Verification gaps

### #3 — macOS build fails: pybind11 links Xcode's Python 3.9 framework

| | |
|---|---|
| **What** | `cmake/Dependencies.cmake:83` calls `find_package(Python3 ...)` with CMake's default `Python3_FIND_FRAMEWORK=FIRST`, so a macOS **framework** Python outranks anything on `PATH` — it selects Xcode's 3.9. Independently, pybind11 runs its deprecated `FindPythonLibsNew.cmake`, which finds `/usr/bin/python3` (3.9.6) and links `Python3.framework/Versions/3.9/lib/libpython3.9.dylib`. That dylib's install name is `@rpath/Python3.framework/Versions/3.9/Python3`, but the emitted rpath is `.../Versions/3.9/lib`, so resolution yields `.../Versions/3.9/lib/Python3.framework/Versions/3.9/Python3` — which does not exist. |
| **Symptom** | `cellwright_tests` links but **aborts on exec** with `dyld: Library not loaded`. `gtest_discover_tests` runs the binary *at build time*, so the whole build fails at 519/521, and `ctest` then sees only the `cellwright_tests_NOT_BUILT` sentinel. Net effect: **the full 254-test suite ran zero tests.** |
| **Why it existed** | `97d78af` disabled CI auto-trigger "until builds are validated locally" — the validation never happened, so the break sat undetected from 2026-03-10 to 2026-10-26. |
| **Fix** | Set `Python3_FIND_FRAMEWORK LAST` and `PYBIND11_FINDPYTHON ON` before the `find_package`/`FetchContent_MakeAvailable(pybind11)` calls, so both resolvers agree on one non-framework interpreter. **Verified locally 2026-10-26:** build exit 0, `ctest` **254/254 passed** in 5.98s, `bin/cellwright` produced. |
| **Re-open trigger** | Closes when the fix lands and `ci.yml` push/PR triggers are re-enabled. Re-open if any platform picks a framework Python again, or if pybind11 is upgraded (the `PYBIND11_FINDPYTHON` option could change semantics). |

### #4 — `main` branch protection has no required status checks

| | |
|---|---|
| **What** | Protection is live (no direct push, no force-push, no deletion, admins included, conversation resolution required) but `required_status_checks` is `null`, so a PR can be merged with no CI evidence at all. |
| **Why it exists** | Required contexts must match real, reporting check names. Setting them before CI has ever run leaves every PR blocked on a check that never arrives. |
| **Fix** | After #3 goes green, `PUT` the protection rule with the three contexts and `strict: true`. |
| **Re-open trigger** | Closes with #3. |
