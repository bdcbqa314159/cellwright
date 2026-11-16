# OPEN.md — debt ledger

Every suppression, skipped test, layering allowance, and load-bearing shortcut lives
here with a rationale and a **re-open trigger**, or it does not exist (`CLAUDE.md` §3).

**Scope (set 2026-11-14):** this file holds debt that must be visible *beside the code* —
suppressions, allowlist rows, and the §3 invariant that `check_layers.py` allowances minus
the layering rows below equals zero. **Schedulable work lives in GitHub Issues**, and each
entry here links to its issue. Two trackers, one job each: Issues decide *when*, this file
records *what is currently untrue about the code*.

| Entry | Issue |
|---|---|
| #1 layering — `Sheet` rewrites formula text *(closed)* | [#12](../../issues/12) |
| #8 CI matrix reduced to macOS | [#13](../../issues/13) |
| #10 `-Werror` not enabled | [#14](../../issues/14) |
| #14 tree not formatted to `.clang-format` | [#16](../../issues/16) |
| #15 `verify_codesign` never invoked *(closed — removed)* | [#15](../../issues/15) |
| audit B2 — `MainWindow` TU split | [#17](../../issues/17) |
| — async shutdown race | [#11](../../issues/11) *(fixed)* |

Invariant: `tools/check_layers.py` allowances − open layering rows below = 0. Currently **0 − 0 = 0** — the allowlist is **empty**, which is the end state `CLAUDE.md` §1's shrink-only rule exists to reach.

---

## Layering allowances

### #1 — `core/Sheet` rewrote formula text *(closed 2026-11-15)*

| | |
|---|---|
| **Resolved by** | `specs/02_sheet_formula_rewriting.md`. |
| **What it was** | `Sheet::insert_row`/`delete_row`/`insert_column`/`delete_column` each did two jobs: index arithmetic (L1) and formula-text rewriting via `FormulaAdjust` → `Tokenizer` (L2). The last upward edge in the tree. |
| **Why the obvious fix failed twice** | Moving `FormulaAdjust` up to L3 would have made `core/Sheet` depend on L3 — worse than the edge removed. And the only production callers were the row/column commands in `core/Command.cpp`, also L1, so deleting the rewrite from `Sheet` merely relocated the same edge. |
| **How it was fixed** | The rewriter is **injected**: `Sheet`'s four methods take a `FormulaRewriter` callable whose type lives at L1, so `core/` never includes `formula/`. `FormulaAdjust` moved to `src/services/` (L3) and the concrete rewriters are supplied at the `ui/` construction site. Chosen over narrowing `Sheet` or moving the commands because a required parameter makes it **impossible** to shift formula keys and forget the text — which would silently corrupt every formula. |
| **Evidence** | `check_layers.py`: **0 declared allowances** — the allowlist is empty. `grep -rn '#include "formula/' src/core/` → empty. |

### #2 — `ui/ ↔ app/` dependency cycle *(closed 2026-11-15)*

| | |
|---|---|
| **Resolved by** | `specs/01_appstate_decomposition.md`, landed as PRs #10, #19, #20. |
| **What it was** | `ui/MainWindow.cpp` included `app/AppState.hpp` (L4 → L5, the violation), while `app/AppState.hpp` included `ui/MainWindow.hpp` because `AppState` owned `MainWindow` and `ToastManager` **by value**. The dependency was mutual, so no file move could fix it. |
| **How it was fixed** | `AppState` was two things fused. Document/session state became `SessionState` at L3 (`src/session/`), which `ui/` now depends on downward. The UI-owning members went to their natural owners: `MainWindow` owns the `ToastManager` it draws, and the font handshake became an explicit interface instead of two shared mutable fields. `AppState` itself was **deleted** — `App` owns `SessionState` and `MainWindow` directly. |
| **Evidence** | `grep -rn '#include "app/' src/ui/` → empty. `check_layers.py` allowances 2 → 1. No `AppState` symbol remains anywhere in `src/` or `tests/`. |
| **Residual** | `ui/` has no automated coverage, so behaviour preservation across this change rests on the unchanged C++/Python suites plus a manual pass — recorded as criterion A7 in the spec, not claimed as proven here. |

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

### #14 — tree formatted to `.clang-format` *(closed 2026-11-15)*

| | |
|---|---|
| **Resolved** | 136 of 146 tracked C++ files reformatted; 4065 changed lines. Sequenced deliberately last, after the layering decompositions (specs 01/02) and the `MainWindow` split, so it did not bury those diffs. |
| **Proof it is formatting-only** | All **37 `cellwright_core` object files are byte-identical** to the pre-format build. The compiler emitted the same machine code, so no token changed. 279/279 tests pass and the build is warning-clean. |
| **Version pin matters** | Formatted with the pinned `clang-format==20.1.8` from `requirements-dev.txt`, not the system binary (Homebrew ships 23.1.1 here, which formats differently). Using the wrong version reintroduces a whole-tree diff. |
| **Two passes from cold** | `IncludeBlocks: Regroup` settles include order on pass 1 and reflows around it on pass 2; pass 3 is a no-op. Noted in `requirements-dev.txt` so nobody reports a phantom diff. |
| **Vendored exception** | `src/util/stb_image_write.h` carries an in-file `// clang-format off` guard. `.clang-format-ignore` was tried first and is **not honoured** by the pinned build — no pattern form worked — so the file-level guard is the mechanism that actually holds, and it survives editor-on-save too. |

### #15 — `verify_codesign` dead security control *(closed 2026-11-16 — removed)*

| | |
|---|---|
| **Resolved** | `verify_codesign` and its test deleted. The SHA-256 allowlist is the sole, documented gate. `SECURITY.md` now states the trust model positively rather than listing the absence of signing as a gap. |
| **Why removed rather than wired in** | Three defects made it unfixable as-is, and the threat model made fixing it unwarranted. (1) The `#else` branch returned `false`, so as a gate it would have rejected **every** plugin on Linux and Windows. (2) On macOS it accepted **ad-hoc** signatures — measured: `Signature=adhoc`, `TeamIdentifier=not set`, and `codesign -f -s -` forges one in a single command — so it proved integrity, never provenance. (3) `codesign -v -R '=anchor apple generic'` correctly rejects our own ad-hoc-signed plugins (exit 3), confirming plain `-v` discriminates nothing. |
| **Why not replace it** | Code signing answers "who published this?", which matters for untrusted distribution. There is no marketplace; plugins are self-built or shared within an organisation, where author and operator coincide. And there is **no portable mechanism** — Linux has no OS-level code signing for shared libraries, so "correct on all platforms" would require detached signatures with a distributed key: a different design, not an extension. |
| **What the allowlist already provides** | Explicit per-artifact human consent; automatic revocation on any byte change, which catches the realistic failure (a stale or overwritten build, not an adversary); identical behaviour on all three platforms; no secrets or CA infrastructure. |
| **Future path, recorded not lost** | If a marketplace ever exists: detached Ed25519 signatures verified against a published key, with the allowlist retained for self-built plugins. Documented in `SECURITY.md`. |

### #16 — GCC build is toolchain coverage only, not Linux coverage

| | |
|---|---|
| **What** | The `gcc` CMake preset (GCC 16.2.0 / libstdc++) builds `cellwright_core` + the full test suite and passes **279/279**. It needs `CELLWRIGHT_BUILD_APP=OFF` because GLFW and the native file-dialog library carry Objective-C sources GCC cannot compile on macOS. |
| **What it genuinely covers** | **libstdc++ instead of libc++** — the exact toolchain difference that produced the missing-includes bug (#7, fixed in PR #2), which only failed on Linux. Plus GCC-only diagnostics: it immediately found 7 warnings Clang reports zero of (issue #26). Covers all of `core`/`formula`/`services`/`session`/`io`/`plugin`. |
| **What it does NOT cover** | `ui/` and `app/` (skipped entirely). And nothing Linux-*platform*: `dlopen` semantics, ELF plugin loading, GTK file dialogs, X11/Wayland, or the runner memory limits that required `--parallel 2`. **A green `gcc` preset is not evidence that Linux works** — issue #13 stays open. |
| **macOS-only workaround inside it** | The preset sets `-D_DARWIN_C_SOURCE`. DuckDB's vendored mbedtls (`platform_util.cpp:14`) defines `_POSIX_C_SOURCE 200112L`, which on macOS hides `quick_exit`/`at_quick_exit` from `stdlib.h` while GCC's header wrapper expects them — 8 errors in a dependency, none of our code. Verified: the flag takes it to 0. **Not needed on real Linux**, where glibc provides those under `_POSIX_C_SOURCE`, so a failure here would be a false alarm about Linux. |
| **Re-open trigger** | When a real Linux environment exists (#13), compare its findings against this preset's. If Linux finds things GCC-on-macOS did not, record what the gap was so the preset's claimed scope stays honest. |

### #17 — `-Wno-attributes` suppressed for GCC builds

| | |
|---|---|
| **What** | `cellwright_set_warnings()` adds `-Wno-attributes` when the compiler is GNU. Clang never emits this diagnostic, so Clang stays maximally strict — verified: 0/56 Clang TUs carry the flag, 37/37 GCC TUs do. |
| **The diagnostic** | `'magic::PyFunctionPlugin' declared with greater visibility than the type of its field 'scope_'`. `PyFunctionPlugin` has default visibility while its pybind11 members are hidden (pybind11 marks its types hidden). GCC is right that a visibility mismatch is an ODR hazard **if the type crossed a shared-library boundary**. |
| **Why it is safe here** | It does not cross one. `PyFunctionPlugin` is referenced only by `PluginManager` (same library) and the tests (statically linked). Plugins implement `IFunction`, never this adapter. |
| **Why not fix it with an attribute** | Tried and reverted — **it is viral.** Marking `PyFunctionPlugin` hidden moved the warning to `PluginManager`, which holds it; marking that hidden moved it to `SessionState`, which `App` owns and every `ui/` signature takes. Measured each step. Spreading a visibility attribute through the whole application to silence one GCC diagnostic is worse code than one scoped suppression. |
| **The real fixes, and their costs** | (a) **Pimpl** — hide the pybind11 members behind an opaque pointer so no header exposes a hidden type. Contains the problem at its boundary; a genuine refactor deserving its own slice. (b) **Global `-fvisibility=hidden`**, which is what pybind11 recommends — but our plugins are separate dylibs implementing `IFunction`, so hiding the host's RTTI could break cross-boundary `dynamic_cast`. Needs testing against the real plugins, not a guess. |
| **Re-open trigger** | If `PyFunctionPlugin` or `PluginManager` is ever referenced from a plugin dylib — then the hazard becomes real and (a) is required. Also re-open when attempting #14 (`-Werror`), to reconfirm this is still the right trade. |
