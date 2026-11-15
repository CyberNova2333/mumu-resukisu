# Code Map

::: warning AI Translation Notice
This page is a concise English summary of the canonical Chinese code map in [代码地图](/docs/code-map).
:::

## Repository Layout Overview

| Path | Responsibility | Key Entrypoints / Verification |
| --- | --- | --- |
| `src/mumu_resukisu/` | Host CLI, VDI COW parsing/auditing, release retrieval, instance binding, atomic install/rollback | `cli.py`, `images.py`, `bootfs.py`, `releases.py`, `binding.py`, `controller.py` |
| `scripts/` | Linux/WSL native build, kernel prep, userspace ASAN models, partition patching, release registration, optional Frida build | `build-native.py`, `prepare-kernel.py`, `build-userspace.py`, `prepare-{boot,system,vendor}.py`, `register-release.py`, `build-frida.py` |
| `native/` | Early-boot `driver-init` loader, kernel driver profile/patches, `resukisu-migrate-prestage` (`stock-migration.h`), ext4 `system-edit.c` | `native/driver/target-profile.json`, `native/prestage/*.json`, `native/loader/loader.c` |
| `targets/mumu-android15-x86_64/` | Pinned target capacity, base/partition hashes, and validation status flags | `profile.json` |
| `tests/` | Cross-platform and Windows host unit tests (48 cross-platform + 29 Win32-handle tests) | `python -m unittest discover -s tests -v` |
| `repo-docs/` | Standard-mode Chinese engineering guide & evidence ledger (`validate_repo_docs.py`) | `repo-docs/README.md` |
| `docs-site/` | Bilingual VitePress documentation site reusing core `repo-docs/` pages via `guide-links.mjs` | `pnpm --dir docs-site test`, `pnpm --dir docs-site run docs:build` |
