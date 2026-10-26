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

### #3c — macOS CI job hung to the 6-hour ceiling; no job timeout configured

| | |
|---|---|
| **What** | In CI run 40254262245 the `macOS (Apple Clang)` job ran **6h5m** and was killed by GitHub's hard per-job limit, while Linux failed at 4m44s and Windows at 12m20s. `ci.yml` sets no `timeout-minutes` on any job, so a hang costs the full 6 hours of runner time before anyone learns anything. |
| **Not yet diagnosed** | Whether the hang is the same Python/dyld fault (#3), a slow cold DuckDB build, or a genuinely hanging step. The 6h kill destroyed the useful signal. |
| **Mitigated, not fixed** | `timeout-minutes: 90` (build matrix) and `10` (layers) added to the workflows. A hang now fails inside 90 minutes with a readable log instead of consuming the 6-hour ceiling. The underlying cause is still unknown — the timeout buys a diagnosis, it is not one. |
| **Re-open trigger** | Closes when macOS CI reports a terminal state inside its timeout. If macOS now hits the 90-minute timeout, this becomes a real hang to diagnose, not a slow build. |

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

### #5 — Bumping a `GIT_SHALLOW` dependency re-fetches its entire history

| | |
|---|---|
| **What** | Every `FetchContent_Declare` in `cmake/Dependencies.cmake` uses `GIT_SHALLOW TRUE`. Changing a pinned tag in an existing build tree makes FetchContent run `git fetch --tags --force origin`, which cannot be satisfied from a depth-1 clone and pulls the dependency's full history — observed as `git index-pack` on **1,038,874 objects** while moving DuckDB `v1.1.3` → `v1.5.6`. |
| **Impact** | Local only, and it does not affect CI: the `actions/cache` key hashes `cmake/Dependencies.cmake`, so a pin change misses the cache and does a clean shallow clone. But a developer bumping a pin in place appears to hang indefinitely. |
| **Workaround** | `rm -rf build/_deps/<dep>-src build/_deps/<dep>-subbuild build/_deps/<dep>-build` before reconfiguring, which forces a fresh shallow clone of the new tag. |
| **Re-open trigger** | Revisit if dependency bumps become frequent enough to justify documenting this in the README, or if a `FETCHCONTENT_UPDATES_DISCONNECTED` / unshallow strategy is adopted. |
