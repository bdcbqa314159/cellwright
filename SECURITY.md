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

## Known gaps

Current, deliberately-accepted weaknesses are tracked in [OPEN.md](OPEN.md) with a
re-open trigger each. Two are relevant to anyone assessing this project:

- Only macOS is covered by CI (`OPEN.md` #8), so Linux and Windows builds — including
  the plugin loader's platform-specific paths — are verified manually rather than on
  every change.
- **Code-signature verification is implemented but not enforced** (`OPEN.md` #15).
  `PluginAllowlist::verify_codesign` exists and shells out to `/usr/bin/codesign -v`,
  but nothing in the load path calls it — its only callers are in the test suite. The
  SHA-256 allowlist is therefore the **sole** gate on every platform, including macOS.
  A library whose bytes match an explicitly-trusted hash loads; signature state is
  not consulted either way.
