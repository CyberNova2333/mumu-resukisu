# Staged Validation & Architectural Decisions

::: warning AI Translation Notice
This documentation page is an English summary synced from the canonical Chinese validation record. See [Chinese Validation & Decisions](/docs/validation) for complete staged details.
:::

## Verified Checkpoints (Strictly Scoped)

1. **2026-11-01 — Project Extraction & Offline Rebuild**:
   - Editable package install and CLI operational; initial 75 unit tests passed (including 29 Windows handle/fault-injection cases and 21 release-boundary tests).
   - Clean-directory native build verified `kernelsu.ko` (`325384` bytes, SHA256 `39469a8c30b4c2c01475eda91900c195d830a8f9aad68515c423b6157eb9e769`) with exact `235` undefined symbols, `driver-init` (`70920` bytes, SHA256 `306eb1b527f1c03b8fd9d91281f69f235b66f3683850b560c23779ba18cef74b`, exits `64` outside PID 1), kernel dual-decompression/CRC parity, `e2fsck`-checked partitions, and full `2,465,270,784`-byte VDI COW assembly.
   - Userspace build verified 8 helpers plus 6 ASAN/UBSAN models and 11 32-bit direct interface cases; `fetch-release` tested against `v4.2.0-rc3` prerelease while stable channel failed as expected.
2. **2026-11-01 — Stock Migration & Pre-Grant Gating**:
   - Host unit tests expanded to 77 (plus 18 ASAN/UBSAN `stock-unit` real-file fault-injection cases).
   - Formal `install` / `restore` cycle and cold boot on fingerprinted Android 15 x86_64 instance verified `235` symbol bindings, `migrated_stock` backup verification (`stockBackupVerified=true`, `complete=true`, `errno=0`), `versionCode: 35253`, `uapiVersion: 6`, and `expect-denied` unauthorized rejection.
3. **2026-11-02 — Post-Grant Cold Start, Dual `su32`, & Restricted Firefox Attach**:
   - After Manager grant, cold boot confirmed `preserved_supported` daemon state, `uid=0` / `u:r:ksu:s0`, restricted filter subprocess (`NoNewPrivs` preserved), and both ELF32 `execve` & `execveat` `su` call paths.
   - Local restricted orchestration attached once to an existing Firefox PID to toggle `GeckoRuntime` remote debugging and verified clean detach, supervisor teardown, forward cleanup, and unchanged parent policy/SELinux/mount state.

## Uncovered Scope & Non-Negotiable Boundaries

- `refactoredDeviceValidation` and offline `deviceValidated` flags remain **`false`** (`daemonArchitectureMeasured=false`, `lifecycleVerified=false`).
- **Not verified**: 3-phase script lifecycle (`post-fs-data` / `service` / `boot-completed`), 32-bit daemon binaries, Enforcing SELinux, Android 12, non-fingerprinted kernels/engines, arbitrary Root modules/apps, or general Firefox orchestration.

## Key Architectural Decisions

- **Separate `/data` Migration State from System-VDI Rollback**: Keeps stock `dusk` backups and Manager grants intact across private system disk rollbacks.
- **Whole-Disk 1 MiB COW Verification**: Re-audits all virtual bytes on every build rather than trusting partial partition checks.
- **Decoupled Release Channels**: `fetch-release` and `register-release.py` never rewrite `supported-daemons.json` or kernel patches automatically.
