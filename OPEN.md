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

### #3c — macOS CI 6-hour hang *(resolved — it was the pre-fix build, not a hang)*

| | |
|---|---|
| **Original symptom** | In run 40254262245 the `macOS (Apple Clang)` job ran **6h5m** and was killed by GitHub's per-job ceiling, while Linux failed at 4m44s and Windows at 12m20s. Recorded as a possible hang of unknown cause. |
| **Resolution** | Not a hang. After the Python/DuckDB/include fixes, macOS completed its **Build step successfully in 17m43s** (run 40354648434). The 6-hour figure was the broken pre-fix build with no timeout to stop it. |
| **Retained guard** | `timeout-minutes: 120` stays — it is what converts any future hang into a readable failure instead of six silent hours. |

### #3d — Linux CI OOM: uncapped `--parallel` kills cc1plus *(fixed)*

| | |
|---|---|
| **What** | `cmake --build build --parallel` passed no job limit, so it fanned out to all 4 runner vCPUs. DuckDB's unity-build translation units (`ub_*.cpp`) take gigabytes of RAM each, exceeding the runner's memory: `c++: fatal error: Killed signal terminated program cc1plus`, followed by `The runner has received a shutdown signal` — the OOM took the runner agent down with the compiler, which is why the job reported `cancelled` rather than `failure`. |
| **Diagnosis history** | Worth recording, because the first failure was misdiagnosed twice. The initial log (run 40254262245) contained **zero** occurrences of `cc1plus` or `Killed signal` — that runner died before flushing the message — so the OOM hypothesis was raised and then **wrongly rejected** on the grounds that an OOM is SIGKILL while the log showed only graceful SIGTERM. The second failure (run 40352430336) carried the signature explicitly. Lesson: a *missing* failure signature is not evidence of its absence when the process that would have written it was itself killed. |
| **Fix** | `--parallel 2` in `ci.yml`, with a comment naming the failure mode so it is not "optimised" back. `timeout-minutes` raised 90 → 120 so the cap trades neither an OOM for a timeout. |
| **Re-open trigger** | Re-open if any platform OOMs at `-j2`, or if the build exceeds 120 minutes — in which case the answer is caching or trimming DuckDB's build, never raising the job count. |

### #6 — MSVC `__cplusplus` non-conformance defeated DuckDB's feature guard *(fixed)*

| | |
|---|---|
| **What** | Windows failed with `error C2039: 'uncaught_exception': is not a member of 'std'` in DuckDB's `src/common/exception.cpp:55`. DuckDB guards correctly with `#if __cplusplus >= 201703L`, but **MSVC reports `__cplusplus` as `199711L`** unless `/Zc:__cplusplus` is passed, so the guard took the pre-C++17 branch and called `std::uncaught_exception()` — removed in C++20. It is compiled as C++20 because `CMakeLists.txt`'s `CMAKE_CXX_STANDARD 20` is inherited by every dependency subproject (DuckDB's own `set(... "11" CACHE ...)` cannot win: a parent non-cache variable shadows the cache). |
| **Fix** | `add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>)` in `CMakeLists.txt`, set **before** `include(cmake/Dependencies.cmake)` so dependencies see it. This repairs the root cause (a lying `__cplusplus`) rather than suppressing the symptom. |
| **Latent sibling, not fixed** | Imposing our C++20 on dependencies is itself the deeper issue — a dependency that expects an older standard may break in other ways. The architecturally correct fix is a per-target standard (`target_compile_features(... cxx_std_20)`) instead of the global variable. Deferred: it touches every target and is not needed while `/Zc:__cplusplus` holds. |
| **Re-open trigger** | Any further dependency failure traceable to the inherited C++20 standard — then do the per-target conversion. |

### #7 — Missing standard-library includes across `src/` *(fixed)*

| | |
|---|---|
| **What** | 18 standard headers were used without being included across 14 files, compiling only by transitive luck on Apple's libc++. GCC's libstdc++ is stricter and broke the Linux build: `ChartPanel.cpp:335: error: 'memcpy' is not a member of 'std'`. This was the first time this code had ever been compiled by GCC. |
| **Fix** | Added the 18 headers (`<cstring>`, `<cstdio>`, `<memory>`, `<type_traits>`, `<optional>`, `<variant>`, `<mutex>`, `<limits>`, `<new>`). Found by scanning every `src/` file for `std::` symbols against its include set, so the remaining siblings were fixed in one pass rather than one per 40-minute CI cycle. Subsumes the audit's C1 finding. |
| **Residual risk** | The scan covers a curated symbol→header table, not the whole standard library — it is a heuristic, not a proof. MSVC may still surface a case neither libc++ nor libstdc++ needed. |
| **Re-open trigger** | Any further missing-include error on any platform; extend the scan table rather than fixing only the reported file. |

### #8 — CI matrix reduced to macOS only; Linux and Windows verified manually

| | |
|---|---|
| **What** | `ci.yml`'s matrix is commented down to `macos-latest`. Linux and Windows no longer run on push/PR. |
| **Why** | A cold three-platform DuckDB build at `-j2` costs 40–80 minutes per push, and the dependency cache cannot warm until a job goes fully green — so every iteration paid full price. That is too slow to debug against. macOS is the development platform and the only one validated end-to-end (build + 254/254 locally and in CI). |
| **What this costs** | The README advertises Linux and Windows support and **CI no longer proves either.** Both were genuinely broken until this PR (`#3d` OOM, `#6` MSVC `__cplusplus`), so the risk of silent regression is real, not theoretical. Owner verifies both manually before they return. |
| **Known state at time of removal** | Linux: OOM fixed by `-j2`, then surfaced `#7` (missing includes, now fixed) — **never observed fully green.** Windows: `#3b` fixed by the DuckDB bump, then `#6` fixed by `/Zc:__cplusplus` — **never observed past the build step.** Neither platform has ever run the test suite in CI. |
| **Fix** | Uncomment the two matrix entries once each passes a manual local build + `ctest`. Consider `ccache`/`sccache` or a prebuilt DuckDB first, so a three-platform run is minutes rather than an hour. |
| **Re-open trigger** | Any report of a Linux or Windows build failure, or the first dependency bump after this point — a bump is exactly when the untested platforms break. |

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
