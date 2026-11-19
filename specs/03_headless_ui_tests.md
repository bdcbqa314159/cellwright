# Artifact #3 — headless-ui-tests (ratified)

**Status:** ratified 2026-11-18 (Bernardo Cohen)
**Governing purpose:** Give `ui/` automated coverage, so UI changes are backed by observed
behaviour instead of hash-identity, and spec 01's A7 manual pass stops being load-bearing.
**Inherits:** THE BAR (`CLAUDE.md` §0)

## Reframe

The UI is not broken. 28 PRs of `ui/` changes landed without a reported regression, and the
structural work (`MainWindow` split, `SessionState` retype) was verified by byte-identical
method bodies — strong evidence that *nothing changed*. What it cannot show is that the app
*renders correctly*, which is why A7 exists and why every UI claim this session carried a
caveat. This slice closes that gap. It is not a rescue.

## Proven by spike before writing this

| Question | Answer |
|---|---|
| Can ImGui run with no platform and no renderer backend? | **Yes.** 4 core sources, no GLFW/OpenGL/Objective-C. `NewFrame`/`Render` produce valid draw data. |
| Is widget state observable? | **Yes.** Modal open/close observed; vertex counts confirm real layout. |
| Can input be injected? | **Yes.** `io.AddMousePosEvent` + `AddMouseButtonEvent` on a discovered widget rect registered a click. So *dispatch* is testable, not just layout. |

## Obstacles the spike found

1. **No `ui/` source is in a library** — all live only in the `cellwright` executable, so
   nothing can link them.
2. **The imgui target bundles backends with core** (`imgui_impl_glfw`, `imgui_impl_opengl3`)
   and links `glfw`, so imgui core cannot be had without the window system.
3. **nfd is `#include`d by all 6 `MainWindow*.cpp` but *called* by only one** — a
   consequence of PR #22 duplicating the full include preamble into each TU.
   `MainWindowChrome` (sheet tabs, status bar) has no reason to see a file dialog.
4. **Linking, not compiling, is the binding constraint.** A test linking the ui library
   pulls `MainWindowActions`' symbols and therefore nfd regardless of include hygiene.

## MUST — in scope, full discipline

1. `imgui_core` (portable: `imgui.cpp`, `_draw`, `_tables`, `_widgets`) separated from
   `imgui_backends` (glfw + opengl3). `imgui_core` is available even when
   `CELLWRIGHT_BUILD_APP=OFF`.
2. A `cellwright_ui` library containing `src/ui/`, linked by both the `cellwright`
   executable and the test target.
3. Per-TU include preambles trimmed so each `MainWindow*.cpp` includes only what it uses.
4. A headless harness fixture: context creation, `DisplaySize`, default font atlas with a
   fake texture id, frame pump, and mouse/key injection helpers.
5. **Two injected platform ports**, both implemented in the application shell so
   `cellwright_ui` carries no platform library at all — same idiom as spec 02's
   `FormulaRewriter`:
   - **`FileDialogs`** — `open` and `save`, each returning an optional path (5 call sites).
   - **`FramebufferReader`** — one operation, read a pixel rect (1 call site:
     `ChartPanel::export_png`'s `glReadPixels`).
6. `cellwright_ui` links neither nfd nor ImPlot, and therefore builds under the `gcc`
   preset.
7. At least four **behavioural** tests, each asserting an observable outcome rather than
   "it rendered": a context-menu action dispatching the right command, a modal flag
   opening and closing, grid selection arithmetic under a click, and `do_open` loading a
   file via a **fake** dialog that returns a canned path.

## NOT-NOW — real, but deferred

- **Testing `ChartPanel`'s output.** It now lives in `cellwright_ui` (see Drift #1) and
  therefore compiles headlessly, but no test asserts what a chart *looks* like. Its export
  path is covered only to the extent that the injected ports are called correctly.
- **Verifying anything is drawn correctly.** Deferred indefinitely: see C7 and `NEVER`.
  Headless tests assert state and dispatch, never appearance.
- **Decomposing `MainWindow` into collaborators** (issue #17 follow-on). This slice is its
  prerequisite, not its companion.

## NEVER — out of scope

- Pixel/screenshot comparison. Brittle against font rendering and ImGui version drift, and
  it answers "did pixels change", not "is the behaviour right".
- Driving the real GLFW window in tests. That is an integration test needing a display
  server; it would not run in CI and would reintroduce the platform coupling being removed.
- Dear ImGui Test Engine as a dependency. The spike shows the needed capability is ~30
  lines of fixture; a third-party engine is not yet earned (rule of two).

## Drift

| # | Spec claimed | Reality | Change |
|---|---|---|---|
| 1 | `ChartPanel` stays out of `cellwright_ui`, since its body is ImPlot rendering | **`MainWindow` holds `ChartPanel chart_panel_` by value.** Excluding ChartPanel excludes MainWindow — the primary test target — so the exclusion defeated the slice. Measurement also showed it was unneeded: ChartPanel's entire platform surface is **1 GL call and 3 nfd calls, all inside `export_png`**, and ImPlot itself is platform-free (`<immintrin.h>`, `<stdlib.h>` only) | `ChartPanel` **joins** `cellwright_ui`. A second port, `FramebufferReader`, injects the one `glReadPixels`. ImPlot moves out of the `CELLWRIGHT_BUILD_APP` guard alongside `imgui_core`, being portable. What stays deferred is asserting what a chart *looks* like — the genuinely visual part |

## Acceptance

| ID | Criterion | Oracle | Rung |
|----|-----------|--------|------|
| C1 | The UI test target builds **and its tests run** under `cmake --preset gcc` | `ctest --preset gcc` includes the new UI tests and passes | mechanical |
| C1b | `cellwright_ui` depends on no platform library | `grep -rn 'nfd\|GLFW\|OpenGL/gl\|GL/gl' src/ui/` → empty (ImPlot is allowed: it is portable). C1 passing proves it by construction | mechanical |
| C2 | WHEN the Insert Row context action is dispatched THE undo stack SHALL contain one `InsertRowCommand` | new test; assert on `UndoManager` depth and `description()` | 3 |
| C3 | WHEN the plugin-trust modal is triggered THE modal flag SHALL be set, and cleared on dismiss | new test; assert the observable flag across frames | 3 |
| C4 | WHEN a click is injected at a known cell rect THE grid SHALL report that cell selected | new test; assert `GridState::selection` | 3 |
| C4b | WHEN `do_open` runs with a fake dialog returning a known path THE session SHALL load that file | new test injecting a stub `FileDialogs` | 3 |
| C4c | WHEN chart export runs with stub ports THE readback and save ports SHALL each be called once with the computed rect | new test injecting stub `FramebufferReader` + `FileDialogs`; reuses `framebuffer_rect` from PR #3 | 3 |
| C5 | No `src/ui/*.cpp` includes a header it does not use | per-TU: strip each candidate include, recompile, confirm failure ⇒ it was needed | mechanical |
| C6 | Existing suites unaffected | `tools/verify.sh` green; no existing test edited | 3 |
| C7 | A7's manual pass is reduced, not eliminated | **UNCHECKABLE** — headless tests cannot show the app *draws* correctly. Partial evidence accepted: C2-C4 cover dispatch/state; the residual manual check shrinks to "does it visibly render", recorded in spec 01 | — |

## Decisions

| # | Decision | Alternatives | Why this | Revisit when |
|---|----------|--------------|----------|--------------|
| D1 | Split `imgui_core` from `imgui_backends` | keep one target; compile imgui sources directly into the test | The split is honest about what imgui actually is — portable core plus platform glue — and makes the core available under `CELLWRIGHT_BUILD_APP=OFF`, so UI logic is reachable from the GCC preset too | ImGui restructures its own sources |
| D2 | `cellwright_ui` is a library; the executable keeps only `main.cpp` + `app/` | move ui sources into `cellwright_core` | `core` is L0-L3 and GUI-free by design; folding L4 into it would undo the layering work of specs 01/02 and break `check_layers.py`'s meaning | — |
| D3 | **Inject a `FileDialogs` port so `cellwright_ui` is platform-free**, and UI tests run on **both** toolchains | accept Clang-only tests (the original D3); keep nfd in the ui library | Amended at ratification. The surface is 5 call sites and 2 operations, so the port costs almost nothing — and it removes the compromise rather than documenting it. It also makes `do_open`/`do_save` testable with a fake, which the Clang-only route never would. Same idiom as spec 02's injected rewriter, already established in this codebase | A dialog needs more than open/save — then the port grows a method rather than leaking nfd back in |
| D4 | Hand-rolled fixture, not Dear ImGui Test Engine | add the engine as a dependency | The spike needed ~30 lines. A dependency for that fails the rule of two | a second consumer needs scripted multi-step interaction |

## Ratification record (2026-11-18)

1. **D3 amended, not accepted as drafted.** The original proposed Clang-only UI tests. The
   owner asked for the best achievable code and to defer anything *visual*, which resolves
   the tension the other way: inject a `FileDialogs` port (5 call sites, 2 operations) so
   `cellwright_ui` is platform-free and the tests run on both toolchains. What gets deferred
   is **rendering**, not dependency hygiene.
2. The dialog abstraction moves from NOT-NOW into MUST. `ChartPanel`'s ImPlot rendering
   stays deferred — that is the genuinely visual part.
3. Non-blocking, settled — `cellwright_ui` is STATIC, matching `cellwright_core`.

## Progress

| ID | Status | Oracle result | Note |
|----|--------|---------------|------|
| C1 | todo | | |
| C1b | todo | | |
| C2 | todo | | |
| C3 | todo | | |
| C4 | todo | | |
| C4b | todo | | |
| C4c | todo | | |
| C5 | todo | | |
| C6 | todo | | |
| C7 | todo | | residual manual check, see spec 01 A7 |
