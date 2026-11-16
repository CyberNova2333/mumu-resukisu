---
layout: home

hero:
  name: MuMu ReSukiSU
  text: Fingerprinted Root Image Builder & Reversible Installer
  tagline: Offline ReSukiSU kernel/userspace builder, whole-disk VDI COW byte auditor, and Windows exclusive-handle single-instance installer for fingerprinted MuMu Android 15 x86_64.
  actions:
    - theme: brand
      text: Build & Install
      link: /en/docs/
    - theme: alt
      text: API & CLI Reference
      link: /en/api-reference/
    - theme: alt
      text: Architecture & Boundaries
      link: /en/docs/architecture

features:
  - title: Strict Target Fingerprint Guard
    details: Verifies base VDI SHA256, 2,465,270,784 virtual bytes, boot/system/vendor hashes, and 235 kernel symbols before emitting or writing any candidate.
  - title: Whole-Disk VDI COW Audit
    details: Re-reads every logical byte across parent, diff, zero, and unallocated VDI blocks to guarantee zero out-of-range modifications outside allowed partitions.
  - title: Exclusive Write & Atomic Rollback
    details: Confirms three consecutive instance shutdowns and built-in root disablement, opens the private diff disk with Win32 share=0 locks, persists rollback.vdi, and restores by transaction ID.
---

::: info Delivered Capabilities (How Far It Goes)
- Builds `kernelsu.ko`, `driver-init` (`vermagic=6.1.90-perf+ `, `235` kernel symbols), `resukisu-migrate-prestage` (ASAN/UBSAN host suite), and patched `boot`/`system`/`vendor` images offline on Linux/WSL.
- Audits every `2,465,270,784` logical byte across VDI 1 MiB COW semantics and executes Win32 `CreateFileW(share=0)` transactional flashing (`rollback.vdi` + atomic `restore`) on Windows.
- Browse every subcommand, build script, and schema contract interactively in [API & CLI Reference](/en/api-reference/).
:::

::: warning Explicit Non-Goals & Hard Boundaries
- **Strictly locked to fingerprinted MuMu Android 15 `x86_64`**: Rejects Android 12 or un-fingerprinted images; never tamper with digest checks or accept unverified 32-bit daemons.
- **No bundled binaries & platform-split checks**: `inputs/` must contain user-supplied snapshots/`ksud`/`dusk`; Linux `unittest` skips `29` Win32 handle tests, and `refactoredDeviceValidation` remains `false`.
- **No auto-reboot & `/data`-safe restore**: `fetch-release` and `register-release.py` (`catalogUpdated: false`) never mutate pinned build targets; `scripts/build-frida.py` remains optional/standalone.
:::

::: warning AI Translation Notice
This documentation page is an English summary synced from the canonical Chinese technical docs. If you notice terminology or validation nuances that require verification, please check the [Chinese Homepage](/).
:::

## Interactive Architecture Diagram

<div class="archify-diagram-card" style="margin: 1.5rem 0; border: 1px solid var(--vp-c-border); border-radius: 12px; overflow: hidden; background: var(--vp-c-bg-soft); box-shadow: 0 4px 16px rgba(0,0,0,0.06);">
  <div style="display: flex; justify-content: space-between; align-items: center; padding: 10px 18px; background: var(--vp-c-bg-elv); border-bottom: 1px solid var(--vp-c-border); font-size: 13px;">
    <span style="font-weight: 600; color: var(--vp-c-text-1); display: flex; align-items: center; gap: 8px;">
      <span>📐</span> MuMu-ResuKSU Interactive Topology (pan, zoom, and inspect nodes)
    </span>
    <a href="/diagrams/resukisu-architecture.html" target="_blank" rel="noreferrer" style="font-size: 12px; color: var(--vp-c-brand-1); text-decoration: none; font-weight: 500;">
      Open Fullscreen ↗
    </a>
  </div>
  <iframe src="/diagrams/resukisu-architecture.html" width="100%" height="520px" frameborder="0" style="display: block; border: none;" allowfullscreen loading="lazy"></iframe>
</div>

## Quick Start Verification

Install the Python package in editable mode (Python 3.11+) and run the offline host test suite without touching any emulator disk:

```sh
python -m pip install -e .
python -m unittest discover -s tests -v
```

## Documentation Guide

::: tip Quick Navigation by Goal
- **Build offline & flash a test instance**: Start with [Overview & Build Workflow](/en/docs/) for Linux/WSL native build, VDI partition assembly, and Windows bind/install/restore commands;
- **Inspect JSON schemas & diagnose errors**: Read [Inputs, Contracts & CLI Matrix](/en/docs/configuration) and [Troubleshooting & FAQ](/en/docs/faq) for all 10 `mumu-resukisu` subcommands and failure diagnostics;
- **Understand 3-domain trust & recovery boundaries**: See [System Architecture](/en/docs/architecture), [One Reversible Run Walkthrough](/en/docs/walkthrough), and [Image & Recovery Boundaries](/en/docs/image-boundaries);
- **Check staged validation & source files**: Explore [Staged Validation & Decisions](/en/docs/validation) and [Code Map](/en/docs/code-map).
:::
