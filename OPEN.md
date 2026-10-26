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

### #3b — Windows: DuckDB v1.1.3's vendored `fmt` does not compile with current MSVC

| | |
|---|---|
| **What** | `_deps/duckdb-src/third_party/fmt/include/fmt/format.h:306` references `stdext::checked_array_iterator`, which Microsoft **removed** from its STL. Every DuckDB target fails with `C2653: 'stdext': is not a class or namespace name`. No file in `src/` is involved. |
| **Evidence** | CI run 40254262245, job `Windows (MSVC)` — failed at 12m20s during `Build`. |
| **Fix** | Bump the DuckDB pin (`cmake/Dependencies.cmake:99`) from `v1.1.3` to a current `1.4.x` release, which carries a modern `fmt`. Our DuckDB API surface is four stable types — `DuckDB`, `Connection`, `Value`, `Appender` — so migration risk is low, but the bump needs a cold local build to validate. |
| **Re-open trigger** | Closes when Windows CI reports green. Re-open on any future DuckDB bump that reintroduces a vendored-dependency/toolchain conflict. |

### #3c — macOS CI job hung to the 6-hour ceiling; no job timeout configured

| | |
|---|---|
| **What** | In CI run 40254262245 the `macOS (Apple Clang)` job ran **6h5m** and was killed by GitHub's hard per-job limit, while Linux failed at 4m44s and Windows at 12m20s. `ci.yml` sets no `timeout-minutes` on any job, so a hang costs the full 6 hours of runner time before anyone learns anything. |
| **Not yet diagnosed** | Whether the hang is the same Python/dyld fault (#3), a slow cold DuckDB build, or a genuinely hanging step. The 6h kill destroyed the useful signal. |
| **Fix** | Add `timeout-minutes` to every job in `ci.yml` (a cold DuckDB build is the long pole; budget generously, e.g. 90) so a hang fails fast and leaves a readable log. Then re-characterise the macOS job. |
| **Re-open trigger** | Closes when macOS CI reports a terminal state inside its timeout. |

### #3d — Linux CI job cancelled with no cause in its own log

| | |
|---|---|
| **What** | `Linux (GCC)` build received SIGTERM across ~25 translation units at a single timestamp (01:31:40.40) followed by `##[error]The operation was canceled.` `strategy.fail-fast` is `false`, so a failing sibling should not have cancelled it, and the log contains **no** OOM, disk-pressure, or timeout evidence. |
| **Honest status** | Cause unknown. An earlier OOM hypothesis was checked and **rejected** — an OOM kill appears as SIGKILL (`Killed signal terminated program cc1plus`), not the graceful SIGTERM seen here. |
| **Fix** | Re-run Linux alone once #3 and #3b land and characterise it against a clean log. Do not guess at a fix before the cause is known. |
| **Re-open trigger** | Closes when Linux CI reports green, or converts to a specific entry once the cause is identified. |

### #4 — `main` branch protection has no required status checks

| | |
|---|---|
| **What** | Protection is live (no direct push, no force-push, no deletion, admins included, conversation resolution required) but `required_status_checks` is `null`, so a PR can be merged with no CI evidence at all. |
| **Why it exists** | Required contexts must match real, reporting check names. Setting them before CI has ever run leaves every PR blocked on a check that never arrives. |
| **Fix** | After #3 goes green, `PUT` the protection rule with the three contexts and `strict: true`. |
| **Re-open trigger** | Closes with #3. |
