# System Architecture

::: warning AI Translation Notice
This documentation page is an English summary synced from the canonical Chinese architecture guide. See [Chinese Architecture](/docs/architecture) for full prose.
:::

MuMu ReSukiSU isolates untrusted offline image preparation from privileged host disk writes and Android early-boot execution across three explicit domains.

## Interactive Architecture Diagram

<DiagramFrame
  src="/diagrams/resukisu-architecture.html"
  title="MuMu-ResuKSU Interactive Topology (pan, zoom, inspect)"
  aspect="950 / 308"
  new-tab-label="Open Fullscreen ↗"
/>

## Three-Domain Isolation

1. **Offline Build & Whole-Disk Audit (Linux / Cross-Platform Python)**
   - Validates `2,465,270,784`-byte VDI topology and partition hashes via `src/mumu_resukisu/images.py`.
   - Builds `kernelsu.ko` (235 undefined symbols, `vermagic=6.1.90-perf+ `) and `driver-init` from pinned upstream Git commits, patches fixed `BootFs` slots (`norc=1 preinit=0`), edits ext4 system daemon metadata (`e2fsck`-verified), removes the vendor overwrite step, and audits every VDI 1 MiB COW block before emitting `candidate.json`.
2. **Windows Single-Instance Controller (`binding.py` & `controller.py`)**
   - Binds explicit instance directory, `MuMuManager.exe`, and `system-diff.vdi` paths after verifying shut-down state and built-in root disablement.
   - Requires three consecutive shutdown confirmations, locks the private diff disk exclusively (`CreateFileW` share=0 + non-blocking `msvcrt.locking`), saves `rollback.vdi` + atomic journal under `.state/<sha256>/`, writes candidate blocks, and verifies read-back.
3. **Android 15 Early-Boot Runtime (`native/loader/` & `native/prestage/`)**
   - `driver-init` runs only in early PID-1 context (exits `64` otherwise) to load `kernelsu.ko` and execute `/resukisu-migrate-prestage` before `switch_root`.
   - `stock-migration.h` separates `/data` stock daemon preservation (`3753880`-byte `/system/xbin/dusk`) from system-VDI rollback: exact stock binaries are backed up durably on `/data` and verified before exposing `ksud`; unknown executables halt cleanly.
