# Security policy

CellWright loads native code from disk at runtime. The plugin loader — not the
spreadsheet engine — is the part of this project where a bug is a security bug.

## Reporting a vulnerability

**Do not open a public issue.** Use GitHub's private vulnerability reporting
(**Security → Report a vulnerability** on this repo), which opens a private advisory
with the maintainer. Include a description, the affected commit, and a minimal
reproduction.

You will get an acknowledgement within a few days. Please allow a reasonable window
for a fix before any public disclosure.

## In scope

- **The plugin trust gate.** `PluginManager::check_trust` verifies a SHA-256
  allowlist *before* any `dlopen`. Any path that loads a library before verifying it,
  or that lets an untrusted path reach `dlopen`, is a vulnerability rather than a
  refactor. This is a named invariant in `CLAUDE.md` §2 and is covered by
  `tests/test_plugin_allowlist.cpp`.
- **The three plugin ABIs** — C++ `IFunction`, the C ABI, and Python via pybind11 —
  including hot reload, which re-loads a library whose bytes may have changed since
  it was trusted.
- **Untrusted file parsing**: `.magic` (JSON) and CSV import, and the Arrow C Data
  Interface import path, which reads externally-produced structures.
- **`=SQL(...)`**, which executes user-supplied SQL in an embedded DuckDB instance.

## Out of scope

- The security of the host OS, and any attacker with root, kernel, hypervisor, or
  physical-memory access. CellWright assumes a non-compromised single-user machine.
- A plugin the user has *explicitly* trusted. Trust is the whole decision: once a
  library's hash is on the allowlist it runs in-process with full privileges, by
  design. We defend the gate, not what you choose to let through it.
- Denial of service from a pathological sheet (a formula that is merely slow, or an
  import large enough to exhaust memory).

## Supported versions

Pre-1.0: only `main` is supported, and fixes land there. There are no backports.

## The trust model, and why there is no signature check

The gate is a **SHA-256 allowlist**: nothing is `dlopen`ed until a human explicitly trusts
that exact artifact, and any byte change revokes that trust automatically. Trust is rooted
in user consent rather than a certificate authority.

That is a choice, not an omission:

- **Code signing answers "who published this?"** — which matters for untrusted
  distribution. CellWright has no marketplace; plugins are self-built or shared inside an
  organisation. In the self-built case the author and the operator are the same person.
- **There is no portable mechanism.** macOS has Mach-O signatures, Windows has
  Authenticode, and **Linux has no OS-level code signing for shared libraries at all**. A
  cross-platform publisher check would need detached signatures (e.g. Ed25519) verified
  against a key we distribute — a different design, not an extension of `codesign`.
- **A plain `codesign -v` check would have been theatre.** It accepts *ad-hoc* signatures,
  which anyone can produce with `codesign -f -s -` in one command. It verifies integrity,
  which the SHA-256 hash already does more directly, and establishes no provenance. A
  `verify_codesign` helper with exactly that behaviour existed, was never called from the
  load path, and was removed rather than wired in.

**If a marketplace ever exists**, the mechanism would be detached signatures verified
against a published key, with the allowlist retained for self-built plugins. That is a
future design question, not a defect in the current model.

## Known gaps

Current, deliberately-accepted weaknesses are tracked in [OPEN.md](OPEN.md) with a
re-open trigger each. One is relevant to anyone assessing this project:

- Only macOS is covered by CI (`OPEN.md` #8), so Linux and Windows builds — including
  the plugin loader's platform-specific paths — are verified manually rather than on
  every change.
