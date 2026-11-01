import contextlib
import gzip
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
from mumu_resukisu import cli
from mumu_resukisu.bootfs import initramfs, serialize, unpack

FIELDS=[1,0o100750,0,0,1,0,0,0,0,0,0,0,0]
def archive(entries):
    return b''.join(serialize(n,FIELDS,b) for n,b in entries)+serialize('TRAILER!!!',FIELDS,b'')

class BootAndCliTests(unittest.TestCase):
    def test_init_failure_guard_precedes_android_and_original_is_retained(self):
        script=b'#!/bin/sh\nexport ANDROID_ROOT=/system\ninsmod /system/lib64/vaddress.ko\nexec switch_root /android /init\n'
        raw=initramfs(gzip.compress(archive([('init',script),('untouched',b'keep')])),b'loader',b'module',b'prestage')
        files={n:b for n,_,b in unpack(raw)}
        self.assertEqual(files['init'],b'loader');self.assertEqual(files['untouched'],b'keep')
        self.assertLess(files['init.real'].index(b'if ! /resukisu-migrate-prestage'),files['init.real'].index(b'insmod'))
    def test_duplicate_or_traversal_archive_rejected(self):
        for entries in [[('../escape',b'')],[('same',b'a'),('same',b'b')]]:
            with self.assertRaisesRegex(ValueError,'Unsafe'):unpack(archive(entries))
    def test_truncated_archive_rejected(self):
        with self.assertRaises(ValueError):unpack(archive([('init',b'data')])[:70])
    def test_restore_does_not_require_candidate(self):
        controller=mock.Mock();controller.restore.return_value={'status':'restored'}
        with mock.patch.object(cli,'build_controller',return_value=controller) as factory,contextlib.redirect_stdout(io.StringIO()):
            cli.main(['restore','--profile','profile.json','--binding','binding.json','--transaction','a'*32])
        self.assertIsNone(factory.call_args.args[0].candidate)
        controller.restore.assert_called_once_with('a'*32)

if __name__=='__main__':unittest.main()
