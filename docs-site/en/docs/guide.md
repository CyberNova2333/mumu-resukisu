# Engineering Guide Index

::: warning AI Translation Notice
This page is an English entry point synced from the Chinese engineering guide in [工程指南入口](/docs/guide).
:::

`mumu-resukisu` solves one explicit problem: building a fingerprinted Root candidate for a fixed MuMu Android 15 `x86_64` engine and installing it onto a single explicitly bound Windows emulator instance with byte-level rollback. It does not auto-adapt arbitrary kernels, Android 12 images, or third-party simulators.

## Quick Navigation
- **Walkthrough**: [One Reversible Run Walkthrough](/en/docs/walkthrough)
- **Build & CLI Operations**: [Overview & Build Workflow](/en/docs/) · [Inputs, Contracts & CLI Matrix](/en/docs/configuration)
- **Boundaries & Release Policy**: [Image & Recovery Boundaries](/en/docs/image-boundaries) · [Release Lifecycle & Compatibility](/en/docs/release-lifecycle)
- **Code & Evidence**: [Code Map](/en/docs/code-map) · [Source Evidence](/en/docs/source-evidence) · [Staged Validation & Decisions](/en/docs/validation)
- **Review, Glossary & History**: [Quality Review](/en/docs/quality-review) · [Glossary](/en/docs/glossary) · [Sync Changelog](/en/docs/change-log)

Independent tools are licensed under **AGPLv3** with Additional Terms (`ADDITIONAL_TERMS.zh-CN.md` / `ADDITIONAL_TERMS.en.md`; assets in `ASSET_LICENSES.md`); inherited kernel patches and `native/images/system-edit.c` retain **GPLv2-only**. See root `THIRD_PARTY.md` for full attributions.
