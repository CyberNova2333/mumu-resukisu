# Troubleshooting & FAQ

::: warning AI Translation Notice
This documentation page is an English summary synced from the canonical Chinese FAQ. See [Chinese FAQ](/docs/faq) for complete details.
:::

## 1. Why do `extract` or `assemble` reject my VDI snapshots?
- **Cause**: `images.py` enforces exact `2465270784` virtual bytes and matching SHA256s for `base` plus `boot`/`system`/`vendor`. Images from Android 12, other MuMu builds, previously modified systems, or running VMs fail immediately.
- **Action**: Capture snapshots only from a completely stopped fingerprinted Android 15 x86_64 instance and retry using a fresh non-existent `--output` directory.

## 2. Why does `build-native.py` fail the 235-symbol gate (e.g., missing `shmem_file_setup`)?
- **Cause**: Missing UAPI headers during build can silently disable upstream pre-init features and alter exported/undefined symbol counts.
- **Action**: Supply clean ACK kernel archives and original config (`--kernel-archive`, `--kernel-config`), and re-run from a fresh empty `--output` directory without relaxing symbol gates.

## 3. Why does `bind` or `install` report unstable shutdown or built-in root enabled?
- **Cause**: `binding.py` and `controller.py` query `MuMuManager.exe` to require built-in root off and three consecutive stopped observations before allowing exclusive disk writes.
- **Action**: Power off the instance completely (disable background keep-alive and MuMu built-in root) and wait a few seconds before re-running.

## 4. How do I handle Windows `CreateFileW` / `msvcrt.locking` failures?
- **Cause**: Another emulator process, terminal holding the same `bindings/<name>.json`, or antivirus/sync software holds an open handle to `system-diff.vdi` or `.state/`.
- **Action**: Exit all MuMu processes and close competing handles; never duplicate binding files to run parallel transactions on the same instance.

## 5. What happens if `install` fails mid-write vs external disk modification?
- **Cause**: `controller.py` isolates pre-write external changes (preserves external edits without blind rollback) from in-flight controller write failures (attempts atomic restoration inside the same exclusive handle, or stops safely on unknown partial states).
- **Action**: Check `mumu-resukisu status`, then run `mumu-resukisu restore --transaction <TxId>`. Never delete `.state/` files or edit digests manually.

## 6. Why did `fetch-release` download a new APK that still cannot build?
- **Cause**: Release retrieval (`releases.py`) and offline registration (`register-release.py`) only collect verifiable metadata (`catalogUpdated: false`). They do not modify `supported-daemons.json` or kernel patches until audited and tested on target. See [Release Lifecycle & Compatibility](/en/docs/release-lifecycle).

## 7. Why are 29 tests skipped when running `unittest` on Linux?
- **Cause**: `tests/test_controller.py` contains 29 Windows Win32 exclusive-handle and locking fault-injection tests that run only on Windows; the remaining 48 tests execute on Linux.
