"""Explicit local instance bindings and guarded Windows disk access.

SPDX-License-Identifier: GPL-3.0-or-later
"""
from __future__ import annotations

import contextlib
import ctypes
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import time


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def hashfile(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_path(path, root=None):
    """Reject links before resolve, including existing ancestors of new paths."""
    path = Path(path)
    require(path.is_absolute(), 'absolute local path required')
    require(not str(path).startswith(('\\\\', '//')), 'network paths are unsupported')
    for item in (path, *path.parents):
        require(not item.is_symlink(), 'symlink path refused')
        if item.exists():
            meta = item.stat(follow_symlinks=False)
            require(not getattr(meta, 'st_file_attributes', 0) & 0x400, 'reparse path refused')
    resolved = path.resolve()
    if root is not None:
        safe_path(root)
        require(resolved.is_relative_to(Path(root).resolve()), 'path outside bound directory')
    return path


def read_json(path):
    safe_path(path)
    def unique(items):
        result = {}
        for key, value in items:
            require(key not in result, 'duplicate JSON key')
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def vdi_identity(data, virtual_bytes=None):
    require(len(data) >= 512, 'short VDI')
    u32 = lambda at: struct.unpack_from('<I', data, at)[0]
    require(tuple(u32(at) for at in (64, 68, 72, 76, 80)) ==
            (0xBEDA107F, 0x10001, 400, 1, 0), 'unsupported VDI header')
    capacity = struct.unpack_from('<Q', data, 368)[0]
    require(capacity > 0 and (virtual_bytes is None or capacity == virtual_bytes), 'wrong virtual disk capacity')
    require(u32(376) == 1048576 and u32(380) == 0, 'unsupported VDI block geometry')
    require(u32(384) == (capacity + 1048575) // 1048576, 'VDI block count mismatch')
    return {'creation': data[392:408].hex(), 'parent': data[424:440].hex(),
            'parentModification': data[440:456].hex(), 'geometry': data[340:388].hex()}


@contextlib.contextmanager
def exclusive(path, write=False):
    """Open an existing exact local file with Windows share mode zero."""
    require(os.name == 'nt', 'disk access requires Windows')
    import msvcrt
    path = safe_path(path)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32,
                                  ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    kernel.CloseHandle.restype = ctypes.c_int
    handle = kernel.CreateFileW(str(path), 0xC0000000 if write else 0x80000000, 0, None, 3, 0, None)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    class FileInformation(ctypes.Structure):
        _fields_ = [('attributes', ctypes.c_uint32), ('times', ctypes.c_uint32 * 6),
                    ('volume', ctypes.c_uint32), ('sizeHigh', ctypes.c_uint32),
                    ('sizeLow', ctypes.c_uint32), ('links', ctypes.c_uint32),
                    ('indexHigh', ctypes.c_uint32), ('indexLow', ctypes.c_uint32)]
    kernel.GetFileInformationByHandle.argtypes = [ctypes.c_void_p, ctypes.POINTER(FileInformation)]
    kernel.GetFileInformationByHandle.restype = ctypes.c_int
    kernel.GetFinalPathNameByHandleW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32]
    kernel.GetFinalPathNameByHandleW.restype = ctypes.c_uint32
    try:
        meta = FileInformation()
        require(kernel.GetFileInformationByHandle(handle, ctypes.byref(meta)), 'cannot inspect disk handle')
        require(meta.links == 1 and not meta.attributes & (0x400 | 0x10), 'disk handle links/type invalid')
        buffer = ctypes.create_unicode_buffer(32768)
        length = kernel.GetFinalPathNameByHandleW(handle, buffer, len(buffer), 0)
        require(0 < length < len(buffer), 'cannot resolve disk handle')
        final = buffer.value
        if final.startswith('\\\\?\\'):
            final = final[4:]
        require(os.path.normcase(final) == os.path.normcase(str(path.resolve())), 'opened disk target changed')
        safe_path(path)
        fd = msvcrt.open_osfhandle(handle, (os.O_RDWR if write else os.O_RDONLY) | os.O_BINARY)
    except BaseException:
        kernel.CloseHandle(handle)
        raise
    with os.fdopen(fd, 'r+b' if write else 'rb') as stream:
        yield stream


IDENTITY_KEYS = {'index', 'name', 'created_timestamp', 'android_version'}
BINDING_KEYS = {'schemaVersion', 'profileId', 'managerPath', 'instanceDirectory',
                'privateDiskPath', 'baseDiskPath', 'identity', 'baselineSha256', 'baselineIdentity'}


@dataclass(frozen=True)
class LocalBinding:
    profile_id: str
    manager_path: Path
    instance_directory: Path
    private_disk_path: Path
    base_disk_path: Path
    identity: dict
    baseline_sha256: str
    baseline_identity: dict

    def validate(self):
        require(isinstance(self.profile_id, str) and re.fullmatch(r'[a-z0-9][a-z0-9._-]*', self.profile_id), 'invalid profile id')
        for path in (self.manager_path, self.instance_directory, self.private_disk_path, self.base_disk_path):
            safe_path(path)
        require(Path(self.private_disk_path).parent.resolve() == Path(self.instance_directory).resolve(), 'private disk must be directly in bound instance')
        require(Path(self.private_disk_path).name == 'system-diff.vdi', 'unexpected private disk filename')
        require(Path(self.base_disk_path).resolve() != Path(self.private_disk_path).resolve(), 'base/private alias')
        require(set(self.identity) == IDENTITY_KEYS, 'invalid instance identity fields')
        require(isinstance(self.identity['index'], str) and self.identity['index'].isdigit(), 'invalid instance index')
        require(isinstance(self.identity['name'], str) and bool(self.identity['name']), 'invalid instance name')
        require(type(self.identity['created_timestamp']) is int and self.identity['created_timestamp'] > 0, 'invalid creation timestamp')
        require(isinstance(self.identity['android_version'], str) and bool(self.identity['android_version']), 'invalid Android version')
        require(re.fullmatch(r'[0-9]+\.[0-9]+', self.identity['android_version']), 'unsupported engine version format')
        require(self.manager_path.name.lower() == 'mumumanager.exe' and self.manager_path.parent.name.lower() == 'nx_main',
                'unsupported manager layout')
        expected = self.manager_path.parent.parent / 'vms' / ('MuMuPlayer-'+self.identity['android_version']+'-'+self.identity['index'])
        require(self.instance_directory.resolve() == expected.resolve(), 'instance path does not match manager identity')
        require(isinstance(self.baseline_sha256, str) and re.fullmatch('[0-9a-f]{64}', self.baseline_sha256), 'invalid baseline digest')
        require(set(self.baseline_identity) == {'creation', 'parent', 'parentModification', 'geometry'}, 'invalid disk identity fields')
        for key, length in [('creation', 32), ('parent', 32), ('parentModification', 32), ('geometry', 96)]:
            require(isinstance(self.baseline_identity[key], str) and re.fullmatch('[0-9a-f]{'+str(length)+'}', self.baseline_identity[key]), 'invalid disk identity value')
        return self

    def to_dict(self):
        return {'schemaVersion': 1, 'profileId': self.profile_id, 'managerPath': str(self.manager_path),
                'instanceDirectory': str(self.instance_directory), 'privateDiskPath': str(self.private_disk_path),
                'baseDiskPath': str(self.base_disk_path), 'identity': dict(self.identity),
                'baselineSha256': self.baseline_sha256, 'baselineIdentity': dict(self.baseline_identity)}


def load_binding(path):
    obj = read_json(Path(path))
    require(set(obj) == BINDING_KEYS and type(obj['schemaVersion']) is int and obj['schemaVersion'] == 1, 'unknown binding schema')
    return LocalBinding(obj['profileId'], Path(obj['managerPath']), Path(obj['instanceDirectory']),
                        Path(obj['privateDiskPath']), Path(obj['baseDiskPath']), obj['identity'],
                        obj['baselineSha256'], obj['baselineIdentity']).validate()


def write_binding(path, binding):
    binding.validate()
    path = safe_path(Path(path))
    require(not path.resolve().is_relative_to(binding.instance_directory.resolve()), 'binding cannot be stored inside instance')
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(binding.to_dict(), stream, indent=2)
        stream.flush()
        os.fsync(stream.fileno())


class MuMuManager:
    def __init__(self, binding):
        self.binding = binding

    def _run(self, args):
        safe_path(self.binding.manager_path)
        result = subprocess.run([str(self.binding.manager_path), *args], capture_output=True, timeout=10)
        require(result.returncode == 0, 'emulator manager command failed')
        return result.stdout

    def info(self):
        actual = json.loads(self._run(['info', '-v', self.binding.identity['index']]))
        check_identity(actual, self.binding.identity)
        return actual

    def shutdown(self):
        self.info()
        self._run(['control', '-v', self.binding.identity['index'], 'shutdown'])


def check_identity(actual, expected):
    require(all(actual.get(key) == value for key, value in expected.items()), 'bound instance was changed or replaced')
    require(type(actual.get('is_process_started')) is bool and type(actual.get('is_android_started')) is bool,
            'manager readiness fields missing')


def require_root_disabled(binding):
    config = safe_path(binding.instance_directory / 'configs' / 'vm_config.json', binding.instance_directory)
    require(read_json(config)['vm']['root'] == 'false', 'built-in Root setting changed')


def wait_stopped(manager, expected, *, stop=False, sleep=time.sleep):
    current = manager.info()
    check_identity(current, expected)
    if current['is_process_started']:
        require(stop, 'bound instance is running; no disk write allowed')
        manager.shutdown()
    count = 0
    for _ in range(30):
        current = manager.info()
        check_identity(current, expected)
        count = count + 1 if not current['is_process_started'] and not current['is_android_started'] else 0
        if count == 3:
            return
        sleep(1)
    raise RuntimeError('bound instance did not stop stably')


def create_binding(*, profile_id, manager_path, instance_directory, private_disk_path,
                   base_disk_path, identity, manager=None, sleep=time.sleep):
    """Capture the actual stopped disk baseline; never takes a caller-supplied hash."""
    provisional = LocalBinding(profile_id, Path(manager_path), Path(instance_directory), Path(private_disk_path),
                               Path(base_disk_path), dict(identity), '0'*64,
                               {'creation':'0'*32,'parent':'0'*32,'parentModification':'0'*32,'geometry':'0'*96}).validate()
    manager = manager or MuMuManager(provisional)
    require_root_disabled(provisional)
    wait_stopped(manager, provisional.identity, sleep=sleep)
    with exclusive(provisional.private_disk_path) as stream:
        before = manager.info()
        check_identity(before, provisional.identity)
        require(not before['is_process_started'] and not before['is_android_started'], 'instance started during binding')
        data = stream.read()
    return LocalBinding(provisional.profile_id, provisional.manager_path, provisional.instance_directory,
                        provisional.private_disk_path, provisional.base_disk_path, provisional.identity,
                        digest(data), vdi_identity(data)).validate()
