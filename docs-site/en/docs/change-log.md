# Sync Changelog

::: warning AI Translation Notice
This page is a concise English summary of [文档同步记录](/docs/change-log).
:::

## Historical Milestones
- **2026-11-01 (Initial Extraction)**: Established `repo-docs/` Standard guide, 75 initial host tests, and clean-directory offline kernel/userspace/VDI rebuild checks (`Synced through f08c278`).
- **2026-11-01 (Stock Migration & Pre-Grant Gating)**: Added durable `/system/xbin/dusk` backup migration on `/data`, 18-case ASAN/UBSAN `stock-unit` models, 77 unit tests, and pre-grant denial checks (`Synced through ac2e0e9`).
- **2026-11-02 (Post-Grant Cold Start & Restricted Firefox Attach)**: Recorded Shell grant cold boot (`uid=0`), 235 symbol bindings, dual ELF32 `su` paths, and single restricted Firefox remote-debug attach/detach (`Synced through 987e138`).
- **2026-11-15 (Full Documentation Reset)**: Replaced unrelated template boilerplate with verified project docs, reconciled `docs-site/.vitepress/config.mts` with `guide-links.mjs`, migrated root `docs/*.md` into the bilingual 3-stage `docs-site/`, aligned `README.md`, and re-verified `repo-docs/` and `tests/guide-links.test.mjs`.

For exact command records and verification SHA anchors, read [文档同步记录](/docs/change-log).
