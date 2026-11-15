# Image & Recovery Boundaries

::: warning AI Translation Notice
This page is a concise English summary of the canonical Chinese reference in [镜像变化与恢复边界](/docs/image-boundaries).
:::

## System Diff VDI vs `/data` Partition Scope
- **System Partition (`system-diff.vdi`)**: Controlled via `src/mumu_resukisu/controller.py` using atomic transaction journals and `rollback.vdi` in `<binding_dir>/.state/<sha256>/`.
- **Data Partition (`/data`)**: Stores Manager APK, authorizations, and durable `/system/xbin/dusk` migration backups created on first cold start. `mumu-resukisu restore` **never wipes or reverts `/data`**.

## Pre-Write vs In-Flight Failure Rules
| Scenario | Controller Behavior |
| --- | --- |
| External modification detected before write begins | Refuses to overwrite third-party changes; aborts cleanly without erasing external bytes. |
| Failure mid-write inside `install` | Attempts in-handle byte-level restoration from `rollback.vdi`; preserves evidence and refuses blind guesswork on unknown partial state. |
| Candidate build files deleted after install | `restore` still succeeds because recovery depends solely on `.state/<sha256>/rollback.vdi` and transaction metadata. |

## Early-Boot Stock Daemon Migration (`stock-migration.h`)
`resukisu-migrate-prestage` compares `/system/xbin/dusk` against `native/prestage/stock-daemon.json` (`3753880` bytes) and `supported-daemons.json` (`6001344` bytes `ksud`):
- Exact stock match: durably backs up stock binary to `/data`, verifies backup, then stages supported `ksud`.
- Unknown binary or backup mismatch: preserves artifacts and aborts boot transition rather than guessing or overwriting.
