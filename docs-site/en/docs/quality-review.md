# Quality Review

::: warning AI Translation Notice
This page is a concise English summary of [质量复核](/docs/quality-review).
:::

## Reader Comprehension & Falsifiability Checks
- **Main flow comprehension**: Build/input separation, COW block verification, exclusive Win32 write locking, and `/data` vs system rollback boundaries remain clearly separated across walkthrough, architecture, and build docs.
- **Refusal gates**: Unmatched VDI fingerprints, unstable or running instances, external disk edits, corrupted rollback archives, and unverified upstream releases are documented as strict hard stops.
- **Falsification rule**: If any 1-byte modification outside allowed partition offsets passes `audit`, or if `fetch-release` / `register-release` alters `supported-daemons.json`, documentation claims must be treated as broken and fixed in code/tests.
- **Scope note**: Host unit tests and documentation builds never substitute for device cold-boot validation. See [Staged Validation & Decisions](/en/docs/validation).
