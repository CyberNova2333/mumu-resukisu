"""Stopped-instance install/restore transactions, with explicit local ownership.

SPDX-License-Identifier: GPL-3.0-or-later
"""
from __future__ import annotations

import contextlib
from dataclasses import dataclass
import datetime
import json
import os
from pathlib import Path
import re
import time
import uuid

from .binding import (LocalBinding, MuMuManager, check_identity, digest, exclusive,
                      hashfile, read_json, require, require_root_disabled, safe_path,
                      vdi_identity, wait_stopped)


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


@dataclass(frozen=True)
class ProfileRequirements:
    base_sha: str
    source_sha: str
    allowed_partitions: tuple
    virtual_bytes: int
    profile_id: str
    android_version: str


@dataclass(frozen=True)
class Candidate:
    file: Path
    sha: str
    audit: Path
    audit_sha: str


class DiskWriteError(RuntimeError):
    def __init__(self, error, rollback_error=None):
        super().__init__(str(error))
        self.rollback_error = rollback_error


class Controller:
    def __init__(self, binding: LocalBinding, profile: ProfileRequirements, candidate: Candidate | None,
                 state_dir: Path, *, verify_candidate, manager=None, sleep=time.sleep):
        self.binding = binding.validate()
        self.profile = profile
        self.candidate = candidate
        self.root = safe_path(Path(state_dir))
        self.manager = manager or MuMuManager(binding)
        self.sleep = sleep
        require(callable(verify_candidate), 'full candidate verification callback is mandatory')
        self.verify_candidate = verify_candidate
        require(profile.profile_id == binding.profile_id, 'binding/profile mismatch')
        require(profile.android_version == binding.identity['android_version'], 'Android target mismatch')
        require(profile.source_sha == binding.baseline_sha256, 'baseline/profile mismatch')
        for value in (profile.base_sha, profile.source_sha, *((candidate.sha, candidate.audit_sha) if candidate else ())):
            require(isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value), 'invalid pinned digest')
        require(type(profile.virtual_bytes) is int and profile.virtual_bytes > 0, 'invalid virtual capacity')
        last = 0
        require(bool(profile.allowed_partitions), 'no allowed partitions')
        for offset, size in profile.allowed_partitions:
            require(type(offset) is int and type(size) is int and offset >= last and size > 0 and offset+size <= profile.virtual_bytes,
                    'invalid or overlapping allowed partitions')
            last = offset+size
        for path in ((candidate.file, candidate.audit) if candidate else ()):
            safe_path(path)
            require(not path.resolve().is_relative_to(binding.instance_directory.resolve()), 'candidate must be outside installed instance')
        if candidate:
            require(candidate.file.resolve() != candidate.audit.resolve(), 'candidate/audit alias')
        require(not self.root.resolve().is_relative_to(binding.instance_directory.resolve()), 'state directory inside installed instance')
        require(not binding.instance_directory.resolve().is_relative_to(self.root.resolve()), 'state directory contains installed instance')
        require(not binding.base_disk_path.resolve().is_relative_to(self.root.resolve()), 'state directory contains shared base')
        require(all(not path.resolve().is_relative_to(self.root.resolve()) for path in ((candidate.file, candidate.audit) if candidate else ())), 'state directory contains candidate inputs')
        self.binding_key = digest(json.dumps(binding.to_dict(), sort_keys=True, separators=(',', ':')).encode())

    def _identity(self, data):
        return vdi_identity(data, self.profile.virtual_bytes)

    def _info(self):
        actual = self.manager.info()
        check_identity(actual, self.binding.identity)
        return actual

    def _configuration(self):
        self.binding.validate()
        require_root_disabled(self.binding)
        require(hashfile(self.binding.base_disk_path) == self.profile.base_sha, 'shared base changed; no disk write allowed')

    def _stopped(self, stop=False):
        wait_stopped(self.manager, self.binding.identity, stop=stop, sleep=self.sleep)

    @contextlib.contextmanager
    def _lock(self):
        require(os.name == 'nt', 'controller requires Windows')
        import msvcrt
        safe_path(self.root)
        self.root.mkdir(parents=True, exist_ok=True)
        path = safe_path(self.root/'control.lock', self.root)
        with path.open('a+b') as stream:
            require(stream.fileno() >= 0 and os.fstat(stream.fileno()).st_nlink == 1, 'lock hard link refused')
            if stream.tell() == 0:
                stream.write(b'0')
                stream.flush()
            stream.seek(0)
            try:
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            except OSError as error:
                raise RuntimeError('another install/restore operation owns the lock') from error
            try:
                yield
            finally:
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)

    def _save(self, path, value):
        path = safe_path(path, self.root)
        temporary = safe_path(path.with_name('.'+path.name+'.'+uuid.uuid4().hex+'.tmp'), self.root)
        with temporary.open('x', encoding='utf-8', newline='\n') as stream:
            json.dump(value, stream, indent=2)
            stream.flush()
            os.fsync(stream.fileno())
        safe_path(path, self.root)
        os.replace(temporary, path)

    def _active(self):
        path = safe_path(self.root/'active.json', self.root)
        if not path.exists():
            return None
        value = read_json(path)
        require(value.get('bindingSha256') == self.binding_key, 'state belongs to another binding')
        transaction = value.get('transaction')
        require(transaction is None or isinstance(transaction, str) and re.fullmatch('[0-9a-f]{32}', transaction), 'invalid transaction pointer')
        return transaction

    def _pointer(self, transaction, **extra):
        self._save(self.root/'active.json', {'bindingSha256': self.binding_key, 'transaction': transaction, **extra})

    def _journal(self, transaction):
        require(isinstance(transaction, str) and re.fullmatch('[0-9a-f]{32}', transaction), 'invalid transaction identifier')
        path = safe_path(self.root/'transactions'/transaction/'journal.json', self.root)
        value = read_json(path)
        require(value.get('transaction') == transaction and value.get('bindingSha256') == self.binding_key and
                value.get('baseSha256') == self.profile.base_sha and value.get('originalSha256') == self.profile.source_sha and
                value.get('identity') == self.binding.baseline_identity, 'invalid transaction ownership or baseline')
        return path, value

    def _candidate(self):
        candidate = self.candidate
        require(candidate is not None, 'candidate is required for inspect/install')
        safe_path(candidate.file)
        safe_path(candidate.audit)
        require(hashfile(candidate.audit) == candidate.audit_sha, 'candidate audit changed')
        audit = read_json(candidate.audit)
        require(audit.get('schemaVersion') == 1 and audit.get('verified') is True and audit.get('outsideAllowedUnchanged') is True,
                'candidate audit incomplete')
        require(audit.get('profileId') == self.profile.profile_id and audit.get('baseSha256') == self.profile.base_sha and
                audit.get('sourceDiskSha256') == self.profile.source_sha and audit.get('candidateSha256') == candidate.sha,
                'candidate audit baseline mismatch')
        require(tuple((p['offset'],p['bytes']) for p in audit.get('allowedPartitions', [])) == self.profile.allowed_partitions and
                audit.get('comparedVirtualBytes') == self.profile.virtual_bytes, 'candidate audit ranges mismatch')
        require(hashfile(candidate.file) == candidate.sha, 'candidate changed')
        # A JSON claim alone is never sufficient: the caller must re-read the effective disk.
        self.verify_candidate(self.binding, self.profile, candidate)
        data = candidate.file.read_bytes()
        require(digest(data) == candidate.sha and hashfile(candidate.audit) == candidate.audit_sha, 'candidate changed during verification')
        require(self._identity(data) == self.binding.baseline_identity, 'candidate changes private identity or geometry')
        return data, audit

    def _replace(self, data, *, expected=None, identity=None):
        self._configuration()
        actual = self._info()
        require(not actual['is_process_started'] and not actual['is_android_started'], 'instance started before disk write')
        with exclusive(self.binding.private_disk_path, True) as stream:
            before = stream.read()
            if expected:
                require(digest(before) == expected, 'disk changed since snapshot')
            if identity:
                require(self._identity(before) == identity, 'private disk identity changed; preserve external replacement')
            try:
                self._write_and_check(stream, data)
            except BaseException as error:
                try:
                    self._write_and_check(stream, before)
                except BaseException as rollback_error:
                    raise DiskWriteError(error, repr(rollback_error)) from error
                raise DiskWriteError(error) from error

    @staticmethod
    def _write_and_check(stream, data):
        stream.seek(0)
        require(stream.write(data) == len(data), 'short disk write')
        stream.truncate()
        stream.flush()
        os.fsync(stream.fileno())
        stream.seek(0)
        require(stream.read() == data, 'disk readback mismatch')

    def inspect(self):
        with self._lock():
            self._configuration()
            data, _ = self._candidate()
            return {'at': now(), 'info': self._info(), 'activeTransaction': self._active(),
                    'candidateSha256': digest(data), 'expectedSourceSha256': self.profile.source_sha,
                    'currentPrivateSha256': hashfile(self.binding.private_disk_path),
                    'baseSha256': self.profile.base_sha, 'candidateIdentity': self._identity(data), 'installedFilesWritten': False}

    def status(self):
        with self._lock():
            transaction = self._active()
            return {'at': now(), 'activeTransaction': transaction,
                    'journal': self._journal(transaction)[1] if transaction else None, 'info': self._info()}

    def install(self):
        with self._lock():
            require(self._active() is None, 'restore active transaction first')
            self._configuration()
            self._stopped()
            candidate, _ = self._candidate()
            with exclusive(self.binding.private_disk_path) as stream:
                original = stream.read()
            require(digest(original) == self.profile.source_sha, 'private disk differs from bound baseline')
            identity = self._identity(original)
            require(identity == self.binding.baseline_identity == self._identity(candidate), 'private disk identity changed')
            transaction = uuid.uuid4().hex
            folder = safe_path(self.root/'transactions'/transaction, self.root)
            folder.mkdir(parents=True, exist_ok=False)
            with (folder/'rollback.vdi').open('xb') as stream:
                stream.write(original)
                stream.flush()
                os.fsync(stream.fileno())
            journal = {'schemaVersion': 1, 'transaction': transaction, 'bindingSha256': self.binding_key,
                       'startedAt': now(), 'status': 'prepared', 'originalSha256': digest(original),
                       'candidateSha256': digest(candidate), 'candidateAuditSha256': self.candidate.audit_sha,
                       'baseSha256': self.profile.base_sha, 'identity': identity, 'userdataRestored': False}
            path = folder/'journal.json'
            self._save(path, journal)
            self._pointer(transaction)
            write_completed = False
            try:
                journal['status'] = 'applying'
                self._save(path, journal)
                self._replace(candidate, expected=self.profile.source_sha, identity=identity)
                write_completed = True
                journal.update(status='installed', installedAt=now(), installedReadbackSha256=hashfile(self.binding.private_disk_path))
                self._save(path, journal)
            except BaseException as error:
                if write_completed:
                    self._restore(transaction, False, expected_current=digest(candidate))
                elif isinstance(error, DiskWriteError):
                    journal.update(status='write_failed_rollback_failed' if error.rollback_error else 'write_failed_restored_in_place',
                                   error=repr(error), rollbackError=error.rollback_error, failedAt=now())
                    self._save(path, journal)
                    if not error.rollback_error:
                        self._pointer(None, lastAborted=transaction)
                else:
                    journal.update(status='aborted_before_disk_write', error=repr(error), failedAt=now())
                    self._save(path, journal)
                    self._pointer(None, lastAborted=transaction)
                raise
            return journal

    def _restore(self, transaction, stop, expected_current=None):
        path, journal = self._journal(transaction)
        require(self._active() == transaction, 'transaction does not own this installation')
        if journal['status'] in ('restored', 'write_failed_restored_in_place', 'aborted_before_disk_write'):
            if journal['status'] != 'aborted_before_disk_write':
                self._configuration()
                self._stopped()
                with exclusive(self.binding.private_disk_path) as stream:
                    current = stream.read()
                require(digest(current) == journal['originalSha256'] and self._identity(current) == journal['identity'],
                        'terminal transaction disk changed; no automatic overwrite')
            journal.update(reconciledAt=now(), activeClearedWithoutDiskWrite=True)
            self._save(path, journal)
            self._pointer(None, lastReconciled=transaction)
            return journal
        require(journal['status'] in ('installed', 'restoring', 'rollback_failed', 'prepared', 'applying', 'write_failed_rollback_failed'),
                'transaction never installed candidate')
        self._configuration()
        self._stopped(stop)
        rollback = safe_path(path.parent/'rollback.vdi', self.root)
        data = rollback.read_bytes()
        require(digest(data) == journal['originalSha256'] and self._identity(data) == journal['identity'], 'rollback backup damaged')
        if journal['status'] in ('prepared', 'applying'):
            current = hashfile(self.binding.private_disk_path)
            require(current in (journal['originalSha256'], journal['candidateSha256']), 'ambiguous interrupted install; preserve unknown bytes')
            expected_current = current
        journal.update(status='restoring', restoreStartedAt=now())
        self._save(path, journal)
        try:
            self._replace(data, expected=expected_current, identity=journal['identity'])
            require(hashfile(self.binding.private_disk_path) == journal['originalSha256'] and
                    hashfile(self.binding.base_disk_path) == self.profile.base_sha, 'restore verification failed')
            journal.update(status='restored', restoredAt=now(), restoredPrivateSha256=hashfile(self.binding.private_disk_path),
                           testInstanceStopped=True, userdataRestored=False)
            self._save(path, journal)
            self._pointer(None, lastRestored=transaction)
        except BaseException as error:
            journal.update(status='rollback_failed', error=repr(error))
            self._save(path, journal)
            raise
        return journal

    def restore(self, transaction, stop=True):
        with self._lock():
            return self._restore(transaction, stop)
