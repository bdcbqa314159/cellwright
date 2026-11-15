# CLAUDE.md — CellWright guardrails

These are **guardrails, not suggestions.** When a change conflicts with this file,
this file wins. Rationale belongs in `specs/`; this is the enforceable subset only —
every line below can be violated, and most are machine-checked.

---

## 0. THE BAR

A change lands only when all four hold:

1. **`tools/check_layers.py` is green** — layering conformance.
2. **`ctest` is green** — the whole suite, not the subset you touched.
3. **New behaviour ships with a test that fails without it.** A test that passes
   against the unchanged code proves nothing.
4. **Any suppression, skipped test, or shortcut is in `OPEN.md`** — see §3.

"Builds and looks right in the UI" is not evidence. Neither is a green suite that
never exercised the new path.

---

## 1. The spine (layers — dependencies point DOWN only, never up)

```
L5  app        application shell — owns the window, the loop, and composition
L4  ui         ImGui panels, grid, formula bar, charts — draws, never computes
L3  builtin · io · plugin · services · session
               functions · serialization · plugin loading ·
               orchestration across core+formula · session-scoped state
L2  formula    tokenizer · parser · AST · evaluator · dependency graph
L1  core       sheet · workbook · column · cell value · command · SIMD
L0  util       hashing, config paths, third-party shims — CellWright-free
```

**The law:** a lower layer must never include a higher one. Enforced per-commit and
in CI by `tools/check_layers.py` (Tarjan-free: a direct upward-edge scan over
`#include "<dir>/..."`). If a change needs an upward include, the design is wrong —
fix the layering, never the check, and never the allowlist.

`src/main.cpp` sits outside the spine (entry point only).

**The allowlist in `check_layers.py` shrinks and never grows.** Each entry requires a
matching `OPEN.md` row with a re-open trigger. Adding an entry to buy a green check is
the one thing this file exists to forbid.

**Layer membership is semantic, not just acyclic.** The check proves dependency
*direction*; it cannot see a file sitting in the wrong layer. Notably: **L4 `ui/` draws
and dispatches — it does not compute.** A formula evaluated, a pattern inferred, or a
range mutated inside `ui/` is a layer violation the check is blind to. Review it at
every PR.

---

## 2. Invariants

- **No `dlopen` before trust.** `PluginManager` verifies the SHA-256 allowlist *before*
  loading any library (`PluginManager.cpp` "Phase 1: security gate"). A code path that
  loads first and checks after is a security bug, not a refactor. Covered by
  `tests/test_plugin_allowlist.cpp`.
- **Arena-allocated AST never outlives its arena.** Parser AST nodes are arena pointers;
  a cached or stored `ASTNode*` must keep its `Arena` alive. Dangling-AST bugs are not
  visible in a green suite — they are visible in a crash three commits later.
- **Empty cell is NaN, and aggregation is NaN-aware.** `sum`/`min`/`max`/`count` skip
  NaN rather than propagating it. A new aggregate that propagates NaN silently changes
  every sheet that has a gap.
- **Recalculation is topological, with cycle detection.** A new dependency edge goes
  through `DependencyGraph`; nothing recalculates by walking cells in index order.

---

## 3. Debt rule

**No debt is silenced silently.** Every suppressed warning, skipped test, layering
allowance, `catch(...)` that swallows, or load-bearing TODO goes into `OPEN.md` with a
rationale and a **re-open trigger** — or it does not exist.

- Debt may **never** buy a green check. A change that can't go green honestly is not done.
- Debt is allowed for *deferred scope*, never for *hidden wrongness*.
- Invariant: `check_layers.py` allowances − `OPEN.md` layering rows = 0.

---

## 4. PR flow

`main` is protected: no direct pushes, no force-push, no deletion — admins included.

- **One PR per concern.** A layering fix and a feature are two PRs.
- CI (Linux/macOS/Windows) green before merge.
- Commit messages: short, single line, no co-author trailer.
- A PR that both moves files and changes behaviour is unreviewable — move in one, change
  in the next.
