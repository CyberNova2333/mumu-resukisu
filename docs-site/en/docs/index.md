# Overview & Build Workflow

::: warning AI Translation Notice
This documentation page is an English summary synced from the canonical Chinese build guide. Please refer to the [Chinese Build Guide](/docs/) for full phrasing.
:::

`mumu-resukisu` targets a single fingerprinted **MuMu Android 15 `x86_64`** emulator engine. It enforces offline verification of native kernel modules, early-boot prestage binaries, and whole-disk VDI COW images before permitting exclusive-handle single-instance flashing on Windows. No original kernels, VM disks, manager APKs, or external toolchains are bundled in this repository.

## Platform Separation & Prerequisites

| Stage | Platform | Required Toolchain & Notes |
| --- | --- | --- |
| Offline host tests & release checks | Linux / Windows | Python 3.11+ (`python -m pip install -e .`, `unittest`; 29 Win32-handle tests skip on Linux). |
| Native module, kernel, prestage & partitions | Linux / WSL | Clang/LLVM, `make`, `musl-gcc`, `gcc`, `binutils`, `pyelftools`, Unicorn, `e2fsprogs`, `libext2fs` dev headers, `zstd`, `liblz4`, optional Android NDK Clang (`--ndk-clang`). |
| VDI candidate assembly, bind, install & restore | Windows (`assemble`/`audit` also run on Linux) | `MuMuManager.exe`, Windows Win32 exclusive file handle (`CreateFileW` share=0). |

## End-to-End Pipeline

### 1. Prepare Inputs & Extract Partitions

Place your stopped snapshot (`inputs/base.vdi`, `inputs/source.vdi`), original `kernel`, `kernel.elf`, `core-kallsyms.txt`, fixed ReSukiSU Git checkout (`inputs/resukisu`), ACK kernel archive (`inputs/ack.tar.gz`), kernel config (`inputs/prepared.config`), paired `ksud`, and extracted `/system/xbin/dusk` (`inputs/stock-dusk`) inside gitignored `inputs/`.

Optional release lookup (never alters `target-profile.json` or `supported-daemons.json`):

```powershell
mumu-resukisu fetch-release --check
mumu-resukisu fetch-release --channel prerelease --output inputs/latest-release
```

Extract and verify the original partitions against `targets/mumu-android15-x86_64/profile.json`:

```powershell
mumu-resukisu profile --profile targets/mumu-android15-x86_64/profile.json
mumu-resukisu extract --profile targets/mumu-android15-x86_64/profile.json --base inputs/base.vdi --source inputs/source.vdi --output build/extracted
```

### 2. Build Native Driver, Loader & Kernel

In Linux or WSL:

```sh
python3 scripts/build-native.py \
  --resukisu-source inputs/resukisu \
  --kernel-archive inputs/ack.tar.gz \
  --kernel-config inputs/prepared.config \
  --clang inputs/clang \
  --target-symbols inputs/core-kallsyms.txt \
  --output build/native

python3 scripts/prepare-kernel.py --kernel inputs/kernel --elf inputs/kernel.elf --output build/kernel
```

`build-native.py` exports the pinned Git commit without dirty local changes, enforces `vermagic=6.1.90-perf+ `, checks all `235` undefined external symbols, and runs loader host assertions (non-PID 1 invocation exits `64`).

### 3. Build Userspace Prestage & Migration Unit Tests

Create the 3-key manifest `bindings/userspace.json` (`schemaVersion: 1`, `expectedManagerUid`, `daemonFile`) and build:

```sh
python3 scripts/build-userspace.py --config bindings/userspace.json --stock-daemon inputs/stock-dusk --output build/userspace
```

This step compiles `resukisu-migrate-prestage` and runs ASAN/UBSAN host failure-injection models (`stock-unit`) against the supplied stock `/system/xbin/dusk` input (`3753880` bytes). Output manifests retain `deviceValidated: false`.

### 4. Prepare Partitions & Audit Whole-Disk VDI Candidate

```sh
python3 scripts/prepare-boot.py --source build/extracted/boot.img \
  --kernel build/kernel/kernel --loader build/native/driver-init \
  --module build/native/kernelsu.ko \
  --native-manifest build/native/manifest.json \
  --userspace-manifest build/userspace/manifest.json \
  --prestage build/userspace/resukisu-migrate-prestage --output build/boot

python3 scripts/prepare-system.py --source build/extracted/system.img --daemon inputs/ksud --output build/system
python3 scripts/prepare-vendor.py --source build/extracted/vendor.img --output build/vendor

mumu-resukisu assemble --profile targets/mumu-android15-x86_64/profile.json --base inputs/base.vdi --source inputs/source.vdi --partition boot=build/boot/boot.img --partition system=build/system/system-candidate.img --partition vendor=build/vendor/vendor.img --output build/candidate
```

`assemble` writes `candidate.json` and `audit.json` after re-reading every `2,465,270,784` logical byte via VDI 1 MiB COW semantics.

### 5. Bind Instance, Install & Rollback (Windows)

```powershell
mumu-resukisu bind --profile targets/mumu-android15-x86_64/profile.json --manager '<MuMuDir>/nx_main/MuMuManager.exe' --index '<Index>' --instance-directory '<InstanceDir>' --private-disk '<InstanceDir>/system-diff.vdi' --base '<BaseVDI>' --output bindings/test.json
mumu-resukisu inspect --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json --candidate build/candidate/candidate.json
mumu-resukisu install --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json --candidate build/candidate/candidate.json
mumu-resukisu status --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json
mumu-resukisu restore --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json --transaction '<TxId>'
```

Keep `bindings/test.json` paired with its adjacent `.state/<sha256>/` journal and `rollback.vdi`. `restore` does not require candidate build outputs and never modifies Android `/data`.
