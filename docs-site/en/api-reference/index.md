---
layout: page
sidebar: false
title: API & Component Reference
description: Reference for MuMu ReSukiSU's 10 CLI subcommands, 8 Linux/WSL build & partition scripts, and JSON/VDI safety contracts.
---

<script setup lang="ts">
import { apiSpecEn } from '../../.vitepress/data/api-spec'
</script>

<ApiExplorer :spec="apiSpecEn">

### Target Fingerprint Verification & Partition Extract (`profile` / `extract`) {#profile-and-extract}

Validates `virtualBytes: 2465270784` and exact base/partition SHA256 digests before extracting read-only `boot`, `system`, and `vendor` images.

### Native Kernel Module & Kernel Prep (`build-native.py` / `prepare-kernel.py`) {#build-native-and-kernel}

Builds `kernelsu.ko` and `driver-init` on Linux/WSL from a pinned upstream Git commit, checking all 235 undefined external symbols and loader assertions.

### Userspace Prestage & Partition Prep (`build-userspace.py` / `prepare-*.py`) {#build-userspace-and-partitions}

Requires the 3-key `bindings/userspace.json` manifest, fixed 64-bit `ksud`, and stock `/system/xbin/dusk` (`3,753,880` B); emits `deviceValidated: false` manifests.

### VDI Candidate Assembly & Whole-Disk COW Byte Audit (`assemble` / `audit`) {#assemble-and-audit}

Re-reads every `2,465,270,784` logical byte across parent/diff/zero/unallocated blocks and proves zero modifications outside declared partition offsets.

### Single-Instance Bind, Preflight Inspect & Transactional Install (`bind` / `inspect` / `install`) {#bind-inspect-install}

Acquires Win32 `CreateFileW(share=0)` exclusive locks after three shutdown polls, persists `rollback.vdi`, flashes diff bytes, and verifies read-back without rebooting.

### Transaction Status & Atomic Disk Rollback (`status` / `restore`) {#status-and-restore}

Restores the private diff VDI byte-for-byte from `.state/<sha256>/rollback.vdi` even without candidate build outputs, leaving Android `/data` untouched.

### Upstream Release Query & Isolated Review Registration (`fetch-release` / `register-release.py`) {#fetch-and-register-release}

Queries pinned upstream repositories without altering target fingerprints or installed daemons (`catalogUpdated: false`).

</ApiExplorer>
