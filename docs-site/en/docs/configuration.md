# Inputs, Contracts & CLI Matrix

::: warning AI Translation Notice
This documentation page is an English summary synced from the canonical Chinese configuration reference. See the [Chinese Reference](/docs/configuration) for full details.
:::

## Target & Native Manifest Contracts

| File | Verified Pinned Values | Purpose |
| --- | --- | --- |
| `targets/mumu-android15-x86_64/profile.json` | `virtualBytes: 2465270784`, pinned `base` & `boot`/`system`/`vendor` SHA256s, `refactoredDeviceValidation: false` | Rejects unknown Android 12/15 images or resized VDI disks during `extract`, `assemble`, and `audit`. |
| `native/driver/target-profile.json` | Fixed Git commit, `vermagic=6.1.90-perf+ `, `versionCode: 35253`, `uapiVersion: 6`, `moduleStructBytes: 960`, `norc=1 preinit=0`, `235` undefined symbols | Gates `scripts/build-native.py` compilation and module ABI validation. |
| `native/prestage/supported-daemons.json` | Paired 64-bit `ksud` size `6001344` bytes (`d93436...`) | Prevents booting unsupported daemon binaries. |
| `native/prestage/stock-daemon.json` | Stock `/system/xbin/dusk` size `3753880` bytes (`c87a46...`) | Allows safe backup-before-replace migration on `/data`; aborts on unknown executables. |

## Userspace Binding Schema (`bindings/userspace.json`)

Strictly 3 required keys (`schemaVersion`, `expectedManagerUid`, `daemonFile`):

```json
{
  "schemaVersion": 1,
  "expectedManagerUid": 10123,
  "daemonFile": "inputs/ksud"
}
```

- `schemaVersion`: must equal `1`.
- `expectedManagerUid`: integer in `10000..99999` verified on the target emulator instance.
- `daemonFile`: path to the local `ksud` binary matching `supported-daemons.json`.

## `mumu-resukisu` CLI Subcommands (`src/mumu_resukisu/cli.py`)

| Subcommand | Role |
| --- | --- |
| `fetch-release` | Query or download official stable/prerelease APK & source tarball to a new local directory (`--channel`, `--check`, `--output`). |
| `profile` | Inspect pinned partition offsets and target fingerprint metadata. |
| `extract` | Verify `base.vdi`/`source.vdi` fingerprints and export raw `boot`, `system`, and `vendor` images. |
| `assemble` | Build differential candidate VDI from prepared partitions and run whole-disk byte audit. |
| `audit` | Re-run whole-disk COW byte integrity audit on an existing candidate. |
| `bind` | Query `MuMuManager.exe` live state, enforce stopped + built-in-root-off checks, and save single-instance binding JSON. |
| `inspect` | Dry-run preflight checking candidate digests, target disk state, parent identity, and lock preconditions. |
| `install` | Perform 3 consecutive shutdown checks, acquire Win32 exclusive handle (`share=0` + `msvcrt.locking`), save `rollback.vdi`, write diff blocks, and read back. |
| `status` | Read active or recorded transaction state from adjacent `.state/<sha256>/`. |
| `restore` | Verify stopped instance + backup integrity and restore the private system diff disk byte-for-byte. |

## Build Helper Scripts (`scripts/*.py`)

- `scripts/build-native.py` — Exports pinned ReSukiSU commit, compiles `kernelsu.ko` & `driver-init`, checks 235 undefined symbols, runs loader tests.
- `scripts/prepare-kernel.py` — Validates kernel/ELF fingerprints, dual-decompresses payload, checks slot bounds & CRC.
- `scripts/build-userspace.py` — Builds prestage & helper tools, runs 18-case ASAN/UBSAN `stock-unit` migration & 32-bit interface host models.
- `scripts/prepare-boot.py` / `prepare-system.py` / `prepare-vendor.py` — Partition-scoped editing within fixed BootFs slots, `e2fsck`-verified ext4 allocation, and vendor overwrite removal.
- `scripts/register-release.py` — Generates pending maintainer review artifacts (`entry.json`, `proof.json`) with `catalogUpdated: false`.
- `scripts/build-frida.py` — Optional standalone build targeting Frida 17.19.0 server (`116223160` bytes) using `--ndk-clang`.
