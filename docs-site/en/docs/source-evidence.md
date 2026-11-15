# Source Evidence

::: warning AI Translation Notice
This page is a concise English summary of the canonical Chinese ledger in [源码证据台账](/docs/source-evidence).
:::

## Key Verified Claims
- **Fixed partition & COW boundary enforcement**: Confirmed in `targets/mumu-android15-x86_64/profile.json`, `src/mumu_resukisu/images.py`, and `tests/test_images.py` (any 1-byte out-of-range write must be rejected by `audit`).
- **Exclusive lock & rollback isolation**: Confirmed in `src/mumu_resukisu/controller.py` (separates pre-write external changes from in-flight write recovery; `/data` excluded from rollback).
- **Pinned native build & 235-symbol gate**: Confirmed in `scripts/build-native.py` and `native/driver/target-profile.json`.
- **Durable stock daemon migration**: Confirmed in `native/prestage/stock-migration.h`, `stock-daemon.json`, and 18 ASAN/UBSAN host test scenarios (`stock-unit`).
- **Staged runtime validation & decoupled release channels**: Confirmed in `src/mumu_resukisu/releases.py`, `scripts/register-release.py`, and [Staged Validation & Decisions](/en/docs/validation).
- **Single-source guide integration**: Confirmed in `docs-site/.vitepress/config.mts` and `guide-links.mjs`.
