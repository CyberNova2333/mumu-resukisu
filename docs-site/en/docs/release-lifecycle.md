# Release Lifecycle & Compatibility

::: warning AI Translation Notice
This page is a concise English summary of the canonical Chinese reference in [发行版获取与兼容性边界](/docs/release-lifecycle).
:::

## Separation of Release Channels from Supported Targets
| Operation | What Changes | What Does NOT Change |
| --- | --- | --- |
| `mumu-resukisu fetch-release` | Queries official repository (GitHub ID `1119492074`), downloads APK/source tarball & checksum manifest to `inputs/`. | Does not verify APK signature, alter `target-profile.json`, or update `supported-daemons.json`. |
| `scripts/register-release.py` | Verifies APK signature, UAPI, and Git tree relation; emits `entry.json` and `proof.json` (`catalogUpdated: false`). | Does not auto-enable new versions in boot prestage. |
| Maintainer review + code update | Updates pinned commits, kernel patches, and `supported-daemons.json` after device validation. | Does not extrapolate support to Android 12 or other kernels. |

Note: `fetch-release` defaults to stable releases and exits cleanly when none exist unless `--channel prerelease` is explicitly passed. Extracted source tarballs lack Git metadata; `--resukisu-source` requires an explicit Git checkout at the reviewed commit.
