"""Real temporary Windows files; fake manager, no emulator operations.

SPDX-License-Identifier: GPL-3.0-or-later
"""
import contextlib
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from mumu_resukisu.binding import (LocalBinding, create_binding, digest, exclusive,
                                   hashfile, load_binding, safe_path, vdi_identity, write_binding)
from mumu_resukisu.controller import Candidate, Controller, DiskWriteError, ProfileRequirements


def fake_disk(fill=0):
    data = bytearray(1024)
    for offset, value in ((64, 0xBEDA107F), (68, 0x10001), (72, 400), (76, 1),
                          (340, 512), (344, 1024), (376, 1048576), (384, 1)):
        struct.pack_into('<I', data, offset, value)
    struct.pack_into('<Q', data, 368, 1048576)
    data[392:408] = b'c'*16
    data[424:440] = b'p'*16
    data[800] = fill
    return bytes(data)


class FakeManager:
    def __init__(self, identity):
        self.identity = identity.copy()
        self.running = False
        self.stops = 0

    def info(self):
        return {**self.identity, 'is_process_started': self.running, 'is_android_started': self.running}

    def shutdown(self):
        self.stops += 1
        self.running = False


@unittest.skipUnless(os.name == 'nt', 'real Windows exclusive-handle fixture tests')
class ControllerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.manager_path = self.root/'engine'/'nx_main'/'MuMuManager.exe'
        self.manager_path.parent.mkdir(parents=True)
        self.manager_path.write_bytes(b'fake manager never executed')
        self.instance = self.root/'engine'/'vms'/'MuMuPlayer-15.0-9'
        (self.instance/'configs').mkdir(parents=True)
        (self.instance/'configs'/'vm_config.json').write_text('{"vm":{"root":"false"}}')
        self.disk = self.instance/'system-diff.vdi'
        self.original = fake_disk()
        self.disk.write_bytes(self.original)
        self.base = self.root/'base.vdi'
        self.base.write_bytes(b'base fixture')
        self.identity = {'index':'9','name':'temporary fixture','created_timestamp':123,'android_version':'15.0'}
        self.binding = LocalBinding('test-profile', self.manager_path, self.instance, self.disk, self.base,
                                    self.identity, digest(self.original), vdi_identity(self.original))
        self.manager = FakeManager(self.identity)
        self.profile = ProfileRequirements(hashfile(self.base), digest(self.original), ((0, 1048576),),
                                           1048576, 'test-profile', '15.0')
        self.candidate_path = self.root/'candidate.vdi'
        self.candidate_data = fake_disk(1)
        self.candidate_path.write_bytes(self.candidate_data)
        self.audit_path = self.root/'candidate.json'
        self.audit = {'schemaVersion':1,'verified':True,'outsideAllowedUnchanged':True,'profileId':'test-profile',
                      'baseSha256':hashfile(self.base),'sourceDiskSha256':digest(self.original),
                      'candidateSha256':digest(self.candidate_data),'allowedPartitions':[{'offset':0,'bytes':1048576}],
                      'comparedVirtualBytes':1048576}
        self.audit_path.write_text(json.dumps(self.audit))
        self.candidate = Candidate(self.candidate_path,digest(self.candidate_data),self.audit_path,hashfile(self.audit_path))
        self.verifications = 0
        def verify(*args):
            self.verifications += 1
        self.control = Controller(self.binding,self.profile,self.candidate,self.root/'state',
                                  verify_candidate=verify,manager=self.manager,sleep=lambda _:None)

    def tearDown(self):
        self.temp.cleanup()

    def assert_original(self):
        self.assertEqual(self.disk.read_bytes(), self.original)

    def test_install_restore_roundtrip(self):
        result=self.control.install()
        self.assertEqual(self.disk.read_bytes(),self.candidate_data)
        self.assertEqual(self.verifications,1)
        self.manager.running=True
        restored=self.control.restore(result['transaction'])
        self.assertEqual(restored['status'],'restored')
        self.assertFalse(restored['userdataRestored'])
        self.assertEqual(self.manager.stops,1)
        self.assertIsNone(self.control.status()['activeTransaction'])
        self.assert_original()

    def test_install_refuses_running_without_shutdown(self):
        self.manager.running=True
        with self.assertRaisesRegex(RuntimeError,'running'):self.control.install()
        self.assertEqual(self.manager.stops,0)
        self.assert_original()

    def test_replaced_instance_refused(self):
        self.manager.identity['created_timestamp']=124
        with self.assertRaisesRegex(RuntimeError,'replaced'):self.control.install()
        self.assert_original()

    def test_builtin_root_change_refused(self):
        (self.instance/'configs'/'vm_config.json').write_text('{"vm":{"root":"true"}}')
        with self.assertRaisesRegex(RuntimeError,'Root'):self.control.install()
        self.assert_original()

    def test_shared_base_change_refused(self):
        self.base.write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError,'shared base'):self.control.install()
        self.assert_original()

    def test_candidate_audit_pin_and_full_callback(self):
        self.audit_path.write_text('{}')
        with self.assertRaisesRegex(RuntimeError,'audit changed'):self.control.install()
        self.audit_path.write_text(json.dumps(self.audit))
        self.control.verify_candidate=lambda *args: (_ for _ in ()).throw(RuntimeError('full audit failed'))
        with self.assertRaisesRegex(RuntimeError,'full audit failed'):self.control.install()
        self.assert_original()

    def test_candidate_mutated_during_full_audit(self):
        def mutate(*args):self.candidate_path.write_bytes(fake_disk(7))
        self.control.verify_candidate=mutate
        with self.assertRaisesRegex(RuntimeError,'changed during verification'):self.control.install()
        self.assert_original()

    def test_prewrite_external_edit_is_never_rolled_back(self):
        real=self.control._replace
        changed=fake_disk(9)
        def mutate(*args,**kwargs):
            self.disk.write_bytes(changed)
            return real(*args,**kwargs)
        with patch.object(self.control,'_replace',mutate):
            with self.assertRaisesRegex(RuntimeError,'changed since snapshot'):self.control.install()
        self.assertEqual(self.disk.read_bytes(),changed)
        self.assertIsNone(self.control.status()['activeTransaction'])

    def test_partial_write_rolls_back_on_same_handle(self):
        real=self.control._write_and_check
        calls=[]
        def fail_once(stream,data):
            calls.append(stream.fileno())
            if len(calls)==1:
                stream.seek(0);stream.write(b'bad');stream.flush()
                raise OSError('injected write error')
            real(stream,data)
        with patch.object(self.control,'_write_and_check',fail_once):
            with self.assertRaises(DiskWriteError):self.control.install()
        self.assertEqual(calls[0],calls[1])
        self.assert_original()
        self.assertIsNone(self.control.status()['activeTransaction'])

    def test_failed_inplace_rollback_retains_active_evidence(self):
        with patch.object(self.control,'_write_and_check',side_effect=OSError('injected')):
            with self.assertRaises(DiskWriteError):self.control.install()
        status=self.control.status()
        self.assertEqual(status['journal']['status'],'write_failed_rollback_failed')
        self.assertIsNotNone(status['activeTransaction'])
        self.control.restore(status['activeTransaction'])
        self.assert_original()

    def test_postwrite_journal_failure_restores_only_own_candidate(self):
        real=self.control._save
        failed=[]
        def fail_once(path,obj):
            if obj.get('status')=='installed' and not failed:
                failed.append(True);raise OSError('journal write failed')
            real(path,obj)
        with patch.object(self.control,'_save',fail_once):
            with self.assertRaisesRegex(OSError,'journal'):self.control.install()
        self.assert_original()
        self.assertIsNone(self.control.status()['activeTransaction'])

    def test_postwrite_external_edit_preserved_after_journal_failure(self):
        real=self.control._save
        changed=fake_disk(8)
        def fail(path,obj):
            if obj.get('status')=='installed':
                self.disk.write_bytes(changed);raise OSError('journal write failed')
            real(path,obj)
        with patch.object(self.control,'_save',fail):
            with self.assertRaisesRegex(RuntimeError,'ambiguous'):self.control.install()
        self.assertEqual(self.disk.read_bytes(),changed)
        self.assertIsNotNone(self.control.status()['activeTransaction'])

    def test_corrupt_backup_refused(self):
        tx=self.control.install()['transaction']
        (self.control.root/'transactions'/tx/'rollback.vdi').write_bytes(b'bad')
        with self.assertRaisesRegex(RuntimeError,'backup damaged'):self.control.restore(tx)
        self.assertEqual(self.disk.read_bytes(),self.candidate_data)

    def test_unknown_interrupted_bytes_preserved(self):
        tx=self.control.install()['transaction']
        path,journal=self.control._journal(tx)
        journal['status']='applying';self.control._save(path,journal)
        changed=fake_disk(6);self.disk.write_bytes(changed)
        with self.assertRaisesRegex(RuntimeError,'ambiguous'):self.control.restore(tx)
        self.assertEqual(self.disk.read_bytes(),changed)

    def test_changed_disk_identity_refused_on_restore(self):
        tx=self.control.install()['transaction']
        changed=bytearray(self.candidate_data);changed[392]=0
        self.disk.write_bytes(changed)
        with self.assertRaisesRegex(RuntimeError,'identity changed'):self.control.restore(tx)
        self.assertEqual(self.disk.read_bytes(),changed)

    def test_terminal_journal_crash_gap_reconciles_without_write(self):
        tx=self.control.install()['transaction'];self.control.restore(tx)
        self.control._pointer(tx)
        with patch.object(self.control,'_replace',side_effect=AssertionError('must not write')):
            result=self.control.restore(tx)
        self.assertTrue(result['activeClearedWithoutDiskWrite'])
        self.assertIsNone(self.control.status()['activeTransaction'])

    def test_terminal_journal_changed_disk_refused(self):
        tx=self.control.install()['transaction'];self.control.restore(tx);self.control._pointer(tx)
        self.disk.write_bytes(fake_disk(4))
        with self.assertRaisesRegex(RuntimeError,'terminal transaction'):self.control.restore(tx)

    def test_aborted_terminal_preserves_unknown_external_change(self):
        tx=self.control.install()['transaction']
        path,journal=self.control._journal(tx)
        journal['status']='aborted_before_disk_write';self.control._save(path,journal)
        changed=fake_disk(4);self.disk.write_bytes(changed)
        with patch.object(self.control,'_replace',side_effect=AssertionError('must not write')):
            self.control.restore(tx)
        self.assertEqual(self.disk.read_bytes(),changed)

    def test_invalid_transaction_and_cross_binding_state_refused(self):
        with self.assertRaisesRegex(RuntimeError,'transaction identifier'):self.control.restore('../other')
        tx=self.control.install()['transaction']
        pointer=self.control.root/'active.json'
        obj=json.loads(pointer.read_text());obj['bindingSha256']='bad';pointer.write_text(json.dumps(obj))
        with self.assertRaisesRegex(RuntimeError,'another binding'):self.control.restore(tx)

    def test_exclusive_handle_and_hardlink_refused(self):
        with self.disk.open('rb'):
            with self.assertRaises(OSError):
                with exclusive(self.disk):pass
        alias=self.root/'alias.vdi';os.link(self.disk,alias)
        with self.assertRaisesRegex(RuntimeError,'links/type'):
            with exclusive(self.disk):pass

    def test_nested_reparse_path_refused(self):
        link=self.root/'linked'
        # Junction creation needs no symlink privilege. Remove only the junction itself.
        command=['cmd.exe','/d','/c','mklink','/J',str(link),str(self.instance)]
        result=subprocess.run(command,capture_output=True,timeout=10)
        self.assertEqual(result.returncode,0,'temporary junction creation failed')
        try:
            with self.assertRaisesRegex(RuntimeError,'reparse|symlink'):safe_path(link/'new'/'file')
        finally:
            os.rmdir(link)

    def test_local_binding_captures_baseline_and_rejects_overwrite(self):
        bound=create_binding(profile_id='test-profile',manager_path=self.manager_path,instance_directory=self.instance,
            private_disk_path=self.disk,base_disk_path=self.base,identity=self.identity,manager=self.manager,sleep=lambda _:None)
        self.assertEqual(bound.baseline_sha256,digest(self.original))
        output=self.root/'binding.json';write_binding(output,bound)
        self.assertEqual(load_binding(output),bound)
        with self.assertRaises(FileExistsError):write_binding(output,bound)

    def test_binding_unknown_fields_duplicate_keys_and_wrong_path(self):
        obj=self.binding.to_dict();obj['unexpected']=True
        output=self.root/'binding.json';output.write_text(json.dumps(obj))
        with self.assertRaisesRegex(RuntimeError,'schema'):load_binding(output)
        output.write_text('{"schemaVersion":1,"schemaVersion":1}')
        with self.assertRaisesRegex(RuntimeError,'duplicate'):load_binding(output)
        obj=self.binding.to_dict();obj['identity']['index']='3';output.write_text(json.dumps(obj))
        with self.assertRaisesRegex(RuntimeError,'instance path'):load_binding(output)

    def test_multiple_instances_not_touched(self):
        other=self.root/'engine'/'vms'/'MuMuPlayer-12.0-1';other.mkdir()
        disk=other/'system-diff.vdi';disk.write_bytes(b'other untouched')
        tx=self.control.install()['transaction'];self.control.restore(tx)
        self.assertEqual(disk.read_bytes(),b'other untouched')

    def test_nonblocking_lock_refuses_concurrent_operation(self):
        with self.control._lock():
            with self.assertRaisesRegex(RuntimeError,'owns the lock'):self.control.install()
        self.assert_original()

    def test_interrupted_known_candidate_restores(self):
        tx=self.control.install()['transaction']
        path,journal=self.control._journal(tx)
        journal['status']='applying';self.control._save(path,journal)
        self.control.restore(tx)
        self.assert_original()

    def test_already_restored_inplace_terminal_reconciles(self):
        tx=self.control.install()['transaction']
        path,journal=self.control._journal(tx)
        journal['status']='write_failed_restored_in_place';self.control._save(path,journal)
        self.disk.write_bytes(self.original)
        with patch.object(self.control,'_replace',side_effect=AssertionError('must not write')):
            self.control.restore(tx)
        self.assertIsNone(self.control.status()['activeTransaction'])

    def test_instance_starts_between_snapshot_and_write(self):
        real=self.control._replace
        def started(*args,**kwargs):
            self.manager.running=True
            return real(*args,**kwargs)
        with patch.object(self.control,'_replace',started):
            with self.assertRaisesRegex(RuntimeError,'instance started'):self.control.install()
        self.assert_original()
        self.assertEqual(self.manager.stops,0)

    def test_restore_without_candidate_files_or_manifest(self):
        tx=self.control.install()['transaction']
        self.candidate_path.unlink();self.audit_path.unlink()
        recover=Controller(self.binding,self.profile,None,self.control.root,
            verify_candidate=lambda *args: (_ for _ in ()).throw(AssertionError('must not audit')),
            manager=self.manager,sleep=lambda _:None)
        self.assertEqual(recover.status()['activeTransaction'],tx)
        recover.restore(tx)
        self.assert_original()
        with self.assertRaisesRegex(RuntimeError,'candidate is required'):recover.install()


if __name__=='__main__':
    unittest.main()
