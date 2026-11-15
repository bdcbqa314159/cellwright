# Artifact #2 — sheet-formula-rewriting (ratified)

**Status:** ratified 2026-11-15 (Bernardo Cohen)
**Governing purpose:** Stop `core/Sheet` rewriting formula text, so `core/` no longer
depends on `formula/` and the `check_layers.py` allowlist becomes **empty**.
**Inherits:** THE BAR (`CLAUDE.md` §0)

## Reframe

The reference-adjustment behaviour is correct and well covered — `tests/test_row_col_ops.cpp`
pins `=A3 → =A4` on insert, `=A4 → #REF!` on delete, multi-reference rewriting, and the
undo paths. **None of that is in question.** This is about *where* the rewriting happens,
not whether it is right. Scope is one dependency edge.

## The edge, precisely

```
core/Sheet.cpp  --includes-->  core/FormulaAdjust.hpp  --includes-->  formula/Tokenizer.hpp
   L1                                 L1                                      L2
```

`Sheet::insert_row` / `delete_row` / `insert_column` / `delete_column` each do **two**
things: shift column data and formula *keys* (pure index arithmetic, L1), and rewrite
formula *text* (needs the tokenizer, L2).

**Why the obvious fix fails.** `OPEN.md` #1 first proposed moving `FormulaAdjust` up to
L3, as was done for `CellInputService` and `Clipboard` in #8. That cannot work here:
`core/Sheet.cpp` consumes it, so moving it up makes `core/Sheet` — the most-depended-on
type in the tree — depend on L3. Strictly worse.

**And the callers are also at L1.** The only production callers of the four `Sheet`
methods are `InsertRowCommand`, `DeleteRowCommand`, `InsertColumnCommand`,
`DeleteColumnCommand` in `core/Command.cpp`. So simply deleting the rewrite from `Sheet`
relocates the same L1 → L2 edge into `Command.cpp` rather than removing it. The rewriting
has to leave L1 **entirely**.

**What makes this tractable:** `Sheet` already exposes `all_formulas()` and
`set_all_formulas()` (`Sheet.hpp:34,37`) — the delete commands already use them to
snapshot and restore. Reading and replacing the whole formula map from outside `Sheet` is
already a supported operation. Note also that `DeleteRowCommand::undo` already *overrides*
`insert_row`'s text adjustment with its snapshot, so that rewrite is redundant on that path.

## MUST — in scope, full discipline

1. `core/Sheet`'s four row/column methods do **only** the L1-pure work: shift column data,
   row/column counts, and formula **keys**. They no longer rewrite formula text, and
   `core/Sheet.cpp` no longer includes `FormulaAdjust.hpp`.
2. Formula-text rewriting is supplied **from above L2** and reaches `Sheet` by injection,
   so a caller cannot perform the structural half and forget the textual half (D1).
3. `FormulaAdjust` relocates out of `core/` to L3.
4. The `("core/FormulaAdjust.cpp", "formula")` allowlist entry is **removed** and the
   allowlist is **empty**. `OPEN.md` #1 closes; issue #12 closes.
5. `Sheet`'s index behaviour becomes testable without the formula engine, and ships a test
   proving keys shift while text is left untouched — impossible to write today.

## NOT-NOW — real, but deferred

- **Tree-wide `clang-format`** (issue #16). Unblocked by this, and deliberately last.
- **Making `UndoManager`/`Command` generic over the target.** Not needed for this edge.

## NEVER — out of scope

- Moving `Tokenizer` to L1. It is the formula lexer; L1 membership would be semantic nonsense.
- Reimplementing reference-finding inside `core/` without the tokenizer. Two
  implementations of one piece of knowledge, guaranteed to diverge.
- Changing the reference-adjustment *semantics* (`#REF!` behaviour, absolute refs,
  multi-reference handling). This slice moves code; it does not touch behaviour.

## Acceptance

Structural criteria are decided by a **mechanical check**; the oracle ladder's rungs
describe numerical agreement and do not apply to a dependency property.

| ID | Criterion | Oracle | Rung |
|----|-----------|--------|------|
| B1 | `check_layers.py` reports **0** declared allowances | `python3 tools/check_layers.py` | mechanical |
| B2 | No file under `src/core/` includes `formula/` | `grep -rn '#include "formula/' src/core/` → empty | mechanical |
| B3 | Reference-adjustment behaviour is unchanged | `tests/test_row_col_ops.cpp` passes with **no assertion edited** — only call sites updated for the new signature | 3 (existing suite as reference) |
| B4 | WHEN a row is inserted with a no-op rewriter THE sheet SHALL shift formula keys and leave formula text byte-identical | new test; fails to compile today because `Sheet` always rewrites | — |
| B5 | `OPEN.md` layering rows = allowlist entries = 0 | count both → 0 and 0 (`CLAUDE.md` §3) | mechanical |
| B6 | Full suite green | `tools/verify.sh` | 3 |

**B3 is weaker than spec 01's A5 and the reason is recorded:** the four `Sheet` signatures
change, so `test_row_col_ops.cpp`'s *call sites* must change. The oracle is therefore "no
**assertion** edited", verified by diffing only `EXPECT_`/`ASSERT_` lines — not the
stronger "no test file touched" that spec 01 could claim.

## Decisions

| # | Decision | Alternatives | Why this | Revisit when |
|---|----------|--------------|----------|--------------|
| D1 | **Inject the rewriter into `Sheet`'s four methods** — they take a rewriter callable defined at L1; `core/` never includes `formula/` | (a) narrow `Sheet` and let callers do both steps; (b) move the four commands to L3 | (a) leaves a trap: shifting keys without rewriting text silently corrupts every formula, and nothing in the signature prevents it — "errors are part of the interface". (b) is cleaner on paper but moves four classes and still needs `Sheet`'s contract narrowed, so it carries (a)'s hazard too. Injection makes the requirement **unforgettable by type** | A second structural operation needs the same treatment — then promote the callable to a named interface |
| D2 | The four commands stay at L1 and hold the injected rewriter | move them to `src/services/` | They need no formula *knowledge*, only to pass a rewriter through. Keeping them put is the smaller change and leaves `UndoManager` untouched | A command needs to *interpret* formulas rather than forward a rewriter |
| D3 | `FormulaAdjust` relocates to `src/services/` | `src/session/` | `session/` is state (spec 01 D2); this is behaviour over strings | — |

## Ratification record (2026-11-15)

1. **D1 accepted — inject the rewriter.** Chosen over narrowing `Sheet` and over moving the
   four commands to L3. The deciding argument: both alternatives leave a caller able to
   shift formula keys without rewriting the text, which silently corrupts every formula in
   the sheet and which no signature prevents. Injection makes the requirement
   unforgettable by type.
2. **Callable alias, not an abstract interface.** One implementation exists; the rule of two
   says do not abstract yet. Promote if a second shape appears.
3. Non-blocking, settled — **one PR**. The diff is small enough.

## Drift

| # | Spec claimed | Reality | Change |
|---|---|---|---|
| 1 | D2: the four commands "hold the injected rewriter" (singular) | Each runs one operation on `execute` and its **inverse** on `undo` — `InsertRowCommand::undo` calls `delete_row` — so each needs **two** rewriters, not one | Introduced `RowColumnRewriters`, a four-field bundle of the callables, built once at the call site and held by each command. Same decision (inject a callable), applied to four operations instead of one |

## Progress

| ID | Status | Oracle result | Note |
|----|--------|---------------|------|
| B1 | **pass** | `check_layers.py`: "110 files scanned, layering acyclic, **0** declared allowance(s)" | allowlist **empty** |
| B2 | **pass** | `grep -rn '#include "formula/' src/core/` → empty | |
| B3 | **pass** | `diff` of all `EXPECT_`/`ASSERT_` lines in `test_row_col_ops.cpp` before vs after → **identical**; only call sites changed | the weaker oracle the spec predicted |
| B4 | **pass** | 4 new `SheetIndexOps` tests; the oracle failed to **compile** against the old `Sheet` (`error: unknown type name 'FormulaRewriter'`) | red-before-green shown |
| B5 | **pass** | allowlist 0 − open `OPEN.md` layering rows 0 = **0** | `CLAUDE.md` §3 |
| B6 | **pass** | `tools/verify.sh` ALL GREEN | see PR |

## Outcome

**The allowlist is empty.** Every layering allowance this project ever declared is closed:
`core → formula` via the services extraction (#8) and this slice, and `ui → app` via
spec 01. `CLAUDE.md` §1's shrink-only rule has reached its end state.

`Sheet` is now independently testable without the formula engine — the `SheetIndexOps`
tests exercise the index arithmetic with a no-op rewriter, which was structurally
impossible before. Unblocks issue #16 (tree-wide `clang-format`).
