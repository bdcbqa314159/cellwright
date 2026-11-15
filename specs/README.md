# specs/ — the rationale tier

`CLAUDE.md` is the **enforceable** tier: guardrails, machine-checked, loaded every session.
This directory is the **rationale** tier: why each decision was made, read on demand.

Every artifact carries a status — `draft` · `ratified <date>` · `current` ·
`superseded by NN`. An artifact with no status is presumed stale.

## ► REFERENCE WHILE BUILDING

| # | Artifact | Status | Governs |
|---|----------|--------|---------|
| 01 | [appstate-decomposition](01_appstate_decomposition.md) | **ratified 2026-10-30** — A1-A6 pass, A7 awaiting manual pass | Breaking the `ui/ ↔ app/` cycle (`OPEN.md` #2, closed) |
| 02 | [sheet-formula-rewriting](02_sheet_formula_rewriting.md) | **ratified 2026-11-15** | Removing the last allowlist entry (`OPEN.md` #1, issue #12) |

## Conventions

- Numbered by ratification order — the order is information.
- Over ~150 lines means more than one slice; split it.
- Debt discovered while implementing a spec goes in [`../OPEN.md`](../OPEN.md), not here.
