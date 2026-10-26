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

### #4 — `main` branch protection required status checks *(closed)*

| | |
|---|---|
| **Resolved** | 2026-10-26. `main` now requires `macOS (Apple Clang)` and `Layer conformance` to pass, with `strict: true` (branch must be up to date), alongside the existing no-direct-push / no-force-push / no-deletion / `enforce_admins: true` / conversation-resolution rules and 0 required approvals. |
| **Note** | Only macOS is a required context because only macOS is in the matrix (#8). When Linux and Windows return, add them as required contexts too, or protection will pass on evidence from one platform while advertising three. |

### #5 — Bumping a `GIT_SHALLOW` dependency re-fetches its entire history

| | |
|---|---|
| **What** | Every `FetchContent_Declare` in `cmake/Dependencies.cmake` uses `GIT_SHALLOW TRUE`. Changing a pinned tag in an existing build tree makes FetchContent run `git fetch --tags --force origin`, which cannot be satisfied from a depth-1 clone and pulls the dependency's full history — observed as `git index-pack` on **1,038,874 objects** while moving DuckDB `v1.1.3` → `v1.5.6`. |
| **Impact** | Local only, and it does not affect CI: the `actions/cache` key hashes `cmake/Dependencies.cmake`, so a pin change misses the cache and does a clean shallow clone. But a developer bumping a pin in place appears to hang indefinitely. |
| **Workaround** | `rm -rf build/_deps/<dep>-src build/_deps/<dep>-subbuild build/_deps/<dep>-build` before reconfiguring, which forces a fresh shallow clone of the new tag. |
| **Re-open trigger** | Revisit if dependency bumps become frequent enough to justify documenting this in the README, or if a `FETCHCONTENT_UPDATES_DISCONNECTED` / unshallow strategy is adopted. |
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

### #9 — OpenGL is deprecated on macOS; `GL_SILENCE_DEPRECATION` acknowledges it

| | |
|---|---|
| **What** | `glReadPixels` (and the GL surface generally) is deprecated on macOS 10.14+. `CMakeLists.txt` defines `GL_SILENCE_DEPRECATION` on the `cellwright` target, which is Apple's documented acknowledgement, not a fix. |
| **Why deferred** | The real fix is a Metal or MoltenVK backend — a rendering rewrite far beyond any warning cleanup. |
| **Why it is a suppression and declared** | It silences a true diagnostic: Apple may remove the OpenGL surface in a future macOS, which would break rendering entirely. |
| **Re-open trigger** | Apple announcing removal (not just deprecation) of OpenGL, or any macOS release where the GL path stops working. |

### #10 — `-Werror` not enabled

| | |
|---|---|
| **What** | Our targets build with `-Wall -Wextra` (`/W4` on MSVC) and are warning-clean, but warnings do not fail the build. |
| **Why deferred** | Only macOS is in the CI matrix (#8). GCC and MSVC emit warnings Apple Clang does not — the missing-includes bug (#7) is exactly that shape — so enabling `-Werror` now would hard-fail the two platforms nobody can currently observe, on diagnostics nobody can read. |
| **Fix** | Enable `-Werror` / `/WX` once Linux and Windows are back in CI and all three are warning-clean. Until then the invariant is upheld by review, not by the compiler. |
| **Re-open trigger** | Closes together with #8. |

### #11 — Vendored `stb_image_write.h` diagnostics suppressed at its include sites

| | |
|---|---|
| **What** | `src/util/stb_image_write.h` is vendored third-party code producing 8 `-Wmissing-field-initializers` and 1 `-Wdeprecated-declarations` (`sprintf`). Both include sites (`ui/ChartPanel.cpp`, `util/stb_image_write_impl.cpp`) wrap it in `#pragma GCC diagnostic push/ignored` (`#pragma warning(push, 0)` on MSVC). |
| **Why this shape** | The alternative was dropping those warnings for our own code, which would have hidden the 6 real `date_input_hint` cases this PR fixed. Scoped suppression keeps our sources strict. |
| **Note** | `stb_image_write.h` uses `sprintf` in its HDR writer. We only call `stbi_write_png`, so that path is unreachable for us — but the suppression means a future vendored-code issue would also be silent. |
| **Re-open trigger** | Updating the vendored stb copy, or needing any stb writer beyond PNG. |

### #12 — `gtest_discover_tests` coupled build success to machine load *(fixed)*

| | |
|---|---|
| **What** | `tests/CMakeLists.txt` used `gtest_discover_tests(cellwright_tests)` in its default `POST_BUILD` mode, which **executes the test binary during the build** to enumerate test names, with a 5-second default timeout. `cellwright_tests` loads `libpython3.12` and a 38MB `libduckdb.dylib`, so it is slow to start; on a machine busy with its own parallel build it was killed mid-listing and **failed the build**. |
| **Evidence** | Clean local build: `Timeout: '5'` / `Process terminated due to timeout`, with partial output showing it had reached `Arena.BasicAllocate`. Nothing in `src/` was involved. |
| **Second symptom it explains** | It is also why the original macOS Python/dyld fault (#3) failed the **build** at 519/521 rather than failing the tests — a discovery-time crash is a build error in `POST_BUILD` mode. The build was reporting test-execution problems as compilation problems. |
| **Fix** | `DISCOVERY_MODE PRE_TEST` with `DISCOVERY_TIMEOUT 60`. Enumeration moves to `ctest` time, so building no longer requires successfully running the binary, and a genuinely slow start no longer looks like a build failure. |
| **Re-open trigger** | Any discovery timeout at 60s (that would be a real startup regression, not load), or test collection failing to appear under `ctest`. |

### #13 — No local equivalent of the CI pipeline *(fixed)*

| | |
|---|---|
| **What** | Changes were validated ad-hoc — build and `ctest` by hand — while CI additionally runs `pytest tests/python`. That step had **never been run locally**, so "verified locally" did not mean what it sounded like, and CI round trips (~15 min cold) were being used to discover things a local run would have caught. |
| **Fix** | `tools/verify.sh`, mirroring `ci.yml` step for step: layer check → configure → build → **assert 0 warnings** → `ctest --no-tests=error` → `pytest`. `--clean` wipes `build/` for a from-scratch run. Creates the venv from `requirements.txt` if absent. |
| **Standing obligation** | The two must be kept in sync by hand. If a step is added to `ci.yml` and not to `verify.sh`, local green stops being evidence. There is no check enforcing this — it is a convention, which is why it is recorded here. |
| **Warning filter is load-bearing** | The warning assertion counts only our sources, excluding `_deps/`, the macOS SDK, and `ld:` lines. Dependencies compile with their own flags and warn in their own code — googletest's `gtest-printers.h` emits `-Wcharacter-conversion` and predates our flags entirely. Without the exclusion the gate fails on code we cannot fix; widen it carelessly and the gate goes blind. |
| **Verified in both directions** | A gate is only evidence if it has been seen to fail. Clean tree → exit 0, `ALL GREEN`. One unused variable injected into `src/core/Sheet.cpp` → exit 1, naming both resulting warnings. Both checked before this landed. |
| **Shell footgun it already hit** | The first version counted warnings in a pipeline. Under `set -euo pipefail` a `grep` matching nothing exits 1, killing the script with no message — so the gate failed **precisely when the tree was clean**, the expected steady state. Warnings are now written to a file with `|| true` and counted with `wc -l`. Any future pipeline added to this script needs the same care. |
| **Re-open trigger** | Any CI failure that `verify.sh` should have caught — that means the two have drifted. |
