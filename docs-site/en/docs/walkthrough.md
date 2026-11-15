# One Reversible Run Walkthrough

::: warning AI Translation Notice
This page is a concise English summary of the canonical Chinese walkthrough in [一次可回退安装全流程](/docs/walkthrough).
:::

## Step 1: Confirm Target Snapshot Identity
`extract` validates `base.vdi` SHA256, `2465270784` virtual bytes, and raw `boot`/`system`/`vendor` partition hashes against `targets/mumu-android15-x86_64/profile.json`. Native builds independently pin Git commit, compiler, kernel config, and 235 symbol expectations.

## Step 2: Generate Local Replacements
`build-native.py`, `build-userspace.py`, and partition preparation scripts emit `kernelsu.ko`, `driver-init`, `/resukisu-migrate-prestage`, and modified partition images inside isolated `build/` directories without overwriting original inputs.

## Step 3: Audit the Whole VDI
`assemble` constructs the candidate differential disk across VDI 1 MiB parent/diff/zero/unallocated blocks and verifies that zero bytes changed outside the three allowed partitions (`tests/test_images.py` enforces cross-block, alias, and out-of-range refusal).

## Step 4: Bind Instance & Flash Atomically
`bind` checks live `MuMuManager.exe` status, instance identity, shutdown state, and built-in root off. `install` requires three consecutive shutdown checks, acquires a Windows exclusive handle (`CreateFileW` share=0 + `msvcrt.locking`), persists `rollback.vdi` in `.state/<sha256>/`, writes candidate blocks, and reads back.

## Step 5: Verify Boot Evidence or Roll Back
`restore` returns the private system diff disk to pre-transaction bytes using the recorded transaction ID without altering `/data` stock backups or Manager grants. See [Staged Validation & Decisions](/en/docs/validation) for verified vs uncovered runtime checks.
