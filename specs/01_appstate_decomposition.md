# Artifact #1 — appstate-decomposition (ratified)

**Status:** ratified 2026-10-30 (Bernardo Cohen)
**Governing purpose:** Break the `ui/ ↔ app/` dependency cycle so `ui/` depends only
downward, clearing the last-but-one allowlist entry in `tools/check_layers.py`.
**Inherits:** THE BAR (`CLAUDE.md` §0)

## Reframe

Nothing is functionally broken. The suite is 273/273, the app builds and runs, and no
defect has been traced to this structure. This is a **layering and testability** fix:
today no `ui/` panel can be compiled, tested, or hosted without the whole application
shell, and the `MainWindow` translation-unit split (audit B2) is blocked behind it.
Scope is the dependency direction — not a UI rewrite.

## The cycle, precisely

```
ui/MainWindow.cpp  --includes-->  app/AppState.hpp     L4 → L5   VIOLATION (OPEN.md #2)
app/AppState.hpp   --includes-->  ui/MainWindow.hpp    L5 → L4   legal, but closes the loop
```

`AppState` is two things fused: **document/session state** and a **UI composition root**
(it owns `MainWindow main_window` and `ToastManager toasts` by value, which is why its
header needs `ui/`).

Measured: `ui/` reads `workbook` 43×, `toasts` 13×, `clipboard` 12×, `active_state()`
11×, `cell_input` 10×, `pending_plugin_path` 8×, `current_file` 6× — but
**`main_window` 0×** and `auto_save` 0×. `ui/` needs the state, never the composition.

Three members block a naive move, and each resolves differently:

| Member | Currently | Finding | Resolution |
|---|---|---|---|
| `ToastManager toasts` | `ui/` (L4) | depends only on `<deque>`, `<string>` — **no `ui/` deps at all** | move down to L3; it is a message queue, not a widget |
| `Settings`, `AutoSave` | `app/` (L5) | depend only on `util/` (L0) and `io/` (L3) | move down to L3 |
| `mono_font`, `font_rebuild_needed` | `app/` (L5) | a real **L5↔L4 handshake**: `App` writes the font, `ui/` requests rebuilds, `App` clears | not relocatable — replace the shared fields with an explicit `MainWindow` interface |

## MUST — in scope, full discipline

1. A `SessionState` aggregate at **L3** holding the document/session members, with
   `active_state()`, `ensure_sheet_states()`, `remove_sheet_state()` unchanged.
2. `ToastManager`, `Settings`, `AutoSave` relocated to L3; `tools/check_layers.py` layer
   map updated.
3. Every `ui/` signature currently taking `AppState&` takes `SessionState&` instead.
4. The font handshake replaced by `MainWindow::set_mono_font()` /
   `font_rebuild_requested()` / `clear_font_rebuild_request()`; both shared fields deleted.
5. `app/AppState` reduced to the composition root (`SessionState` + `MainWindow`), owned
   by `App` as today — or deleted if nothing remains to justify it.
6. The `("ui/MainWindow.cpp", "app")` allowlist entry **removed**, and `OPEN.md` #2 closed.

## NOT-NOW — real, but deferred

- **`MainWindow` TU split** (audit B2). Unblocked by this, done after it, not with it —
  `CLAUDE.md` §4 forbids one PR that both moves files and changes behaviour.
- **`OPEN.md` #1** (`Sheet` rewriting formula text). Independent cycle, its own spec.
- **Tree-wide `clang-format`** (`OPEN.md` #14). Must not land in the same window as this.

## NEVER — out of scope

- Extracting `MenuBar` / `InputRouter` / `ModalHost` collaborators. Different concern
  (responsibility count, not dependency direction); ruled out 2026-10-27 in favour of the
  TU split.
- Making `SessionState` an abstract interface. One implementation, one owner — no second
  consumer exists to justify it (rule of two).
- Moving ImGui types out of `ui/`. `ImFont*` is third-party and layer-neutral.

## Acceptance

Structural criteria are decided by a **mechanical check**, not a numeric oracle rung; the
ladder's rungs describe numerical agreement and do not apply to a dependency property.
Where that leaves a gap, it is declared rather than papered over.

| ID | Criterion | Oracle | Rung |
|----|-----------|--------|------|
| A1 | `tools/check_layers.py` reports exactly **1** declared allowance (`core/FormulaAdjust.cpp`) | `python3 tools/check_layers.py` | mechanical |
| A2 | No file under `src/ui/` includes `app/` | `grep -rn '#include "app/' src/ui/` → empty | mechanical |
| A3 | No file under `src/services/` or `src/session/` includes `ui/` or `app/` | `grep -rn '#include "\(ui\|app\)/' src/services src/session` → empty | mechanical |
| A4 | WHEN the font size changes THE UI SHALL request a rebuild without touching shared state | `grep` shows no `font_rebuild_needed` member; `MainWindow::font_rebuild_requested()` exists and is called by `App` | mechanical |
| A5 | Behaviour is unchanged: the full suite passes with **no test edited** except include paths | `tools/verify.sh` → 273/273, and `git diff` on `tests/` touches include lines only | 3 (existing suite as reference) |
| A6 | `OPEN.md` layering rows equal `check_layers.py` allowances | count both → difference 0 (`CLAUDE.md` §3) | mechanical |
| A7 | Rendering behaviour is unchanged | **UNCHECKABLE** — `ui/` has no automated coverage. Partial evidence accepted: (a) every moved method body byte-identical by SHA-256, as done for the abandoned B2 split; (b) manual launch, open a `.magic` file, edit a cell, export a chart, trigger the plugin-trust modal | — |

## Decisions

| # | Decision | Alternatives | Why this | Revisit when |
|---|----------|--------------|----------|--------------|
| D1 | `SessionState` is a plain aggregate, like `AppState` today | class with accessors; interface | It is a bag of owned subsystems; encapsulation buys nothing and would churn 100+ call sites | A second owner appears, or an invariant across members needs enforcing |
| D2 | New `src/session/` directory at L3 | reuse `src/services/` | `services/` holds behaviour (`CellInputService`, `Clipboard`); this is state. Keeping them distinct keeps the layer map readable | `session/` stays a single header after the next two slices — then fold it into `services/` |
| D3 | Font handshake becomes a `MainWindow` method pair | keep the fields in `SessionState` | `ImFont*` is layer-neutral so the fields *could* move, but font runtime is not document state — `CLAUDE.md` §1 says membership is semantic, and that is the same mistake `AppState` already made | A second consumer needs the font-rebuild signal |
| D4 | `ToastManager` moves to L3 rather than being passed separately | extra `ToastManager&` parameter on 25 methods | It has zero `ui/` dependencies, so it was misfiled; moving it is smaller than widening every signature | It acquires an ImGui dependency — then it is genuinely L4 and must be passed |

## Ratification record (2026-10-30)

1. **Three PRs**, each independently green: **A** relocate `ToastManager`/`Settings`/
   `AutoSave` to L3 · **B** introduce `SessionState` and rewrite the ~25 `ui/` signatures ·
   **C** font handshake and allowlist removal. Rejected the atomic variant: a ~30-file
   diff is not reviewable, and `CLAUDE.md` §4 already forbids mixing moves with behaviour.
2. **`AppState` is deleted.** `App` owns `SessionState` and `MainWindow` directly. The type
   existed only to fuse the two concerns this spec separates, so retaining it would
   preserve the shape being removed.
3. Non-blocking, settled — named `SessionState`: it covers more than one document
   (settings, plugins, SQL engine).

**Directory:** the three relocated classes land in a new `src/session/` (L3) alongside
the future `SessionState`, per D2. `services/` stays behaviour-only.

## Progress

| ID | Status | Oracle result | Note |
|----|--------|---------------|------|
| A1 | todo | | |
| A2 | todo | | |
| A3 | todo | | |
| A4 | todo | | |
| A5 | todo | | |
| A6 | todo | | |
| A7 | todo | | manual pass required before close |
