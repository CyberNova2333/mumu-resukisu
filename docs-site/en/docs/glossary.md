# Glossary

::: warning AI Translation Notice
This page is a concise English summary of [核心术语表](/docs/glossary).
:::

| Term | Meaning in `mumu-resukisu` | Further Reading |
| --- | --- | --- |
| Target Profile | Allowed engine disk fingerprint, capacity, and partition offsets (`targets/mumu-android15-x86_64/profile.json`), not auto-detection. | [Inputs & Contracts](/en/docs/configuration) |
| Instance Binding | Verified tuple of local MuMu instance index, explicit directory paths, and pre-write disk digest (`bindings/*.json`). | [Overview & Build Workflow](/en/docs/) |
| Candidate | Locally built and whole-disk audited differential VDI before device cold-boot verification. | [One Reversible Run Walkthrough](/en/docs/walkthrough) |
| Transaction | Recorded pre-write backup (`rollback.vdi`), journal metadata, and ownership lock inside `.state/<sha256>/`. | [Image & Recovery Boundaries](/en/docs/image-boundaries) |
| Latest Release | Upstream release selected by `fetch-release` at query time; isolated from pinned build commits. | [Release Lifecycle & Compatibility](/en/docs/release-lifecycle) |
| Pinned Build Version | Specific upstream Git commit and native target configuration verified for compilation. | [Release Lifecycle & Compatibility](/en/docs/release-lifecycle) |
| Registration | Artifact verification generating pending maintainer review manifests (`catalogUpdated: false`). | [Release Lifecycle & Compatibility](/en/docs/release-lifecycle) |
