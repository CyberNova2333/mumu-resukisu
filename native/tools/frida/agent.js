// SPDX-License-Identifier: GPL-3.0-or-later
'use strict';
const main = Process.mainModule;
if (Process.arch !== 'x64' || main.path !== '/data/local/tmp/resukisu-frida-probe')
  throw new Error('task_identity_mismatch');
const uid = new NativeFunction(Module.getGlobalExportByName('getuid'), 'uint', [])();
const euid = new NativeFunction(Module.getGlobalExportByName('geteuid'), 'uint', [])();
const nonce = new NativeFunction(main.getExportByName('resukisu_probe_nonce'), 'pointer', [])().readUtf8String(32);
if (uid !== 2000 || euid !== 2000 || nonce !== EXPECTED_NONCE || Process.id !== EXPECTED_PID)
  throw new Error('task_identity_mismatch');
const value = new NativeFunction(main.getExportByName('resukisu_probe_value'), 'int', [])();
send({kind: 'fixed_task_observation', pid: Process.id, uid, euid, value,
      nonce, arch: Process.arch, fridaVersion: Frida.version});
