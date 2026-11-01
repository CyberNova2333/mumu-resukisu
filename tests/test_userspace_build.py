# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('userspace_build', ROOT/'scripts/build-userspace.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)

class UserspaceBuildTests(unittest.TestCase):
    def test_import_has_no_commands(self):
        with patch('subprocess.run', side_effect=AssertionError('command on import')):
            other=importlib.util.module_from_spec(spec)
            spec.loader.exec_module(other)

    def test_catalog_matches_compiled_single_release(self):
        value=build.verify_catalog()
        self.assertEqual(len(value['releases']), 1)

    def test_manager_uid_must_be_explicit_integer(self):
        with tempfile.TemporaryDirectory() as folder:
            file=Path(folder)/'config.json'
            for value in (None, True, '12345', 0, 9999, 100000):
                file.write_text(json.dumps({'schemaVersion':1,'expectedManagerUid':value,'daemonFile':'absent'}))
                with self.assertRaisesRegex(ValueError, 'UID'):
                    build.load_config(file)

    def test_unknown_fields_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            file=Path(folder)/'config.json'
            file.write_text(json.dumps({'schemaVersion':1,'expectedManagerUid':12345,'daemonFile':'absent','skipHash':True}))
            with self.assertRaisesRegex(ValueError, 'config keys'):
                build.load_config(file)

    def test_wrong_daemon_refused_without_executing_it(self):
        with tempfile.TemporaryDirectory() as folder, patch('subprocess.run', side_effect=AssertionError('executed input')):
            file=Path(folder)/'config.json'; daemon=Path(folder)/'daemon'
            daemon.write_bytes(b'not executable')
            file.write_text(json.dumps({'schemaVersion':1,'expectedManagerUid':12345,'daemonFile':'daemon'}))
            with self.assertRaisesRegex(ValueError, 'Unsupported daemon'):
                build.load_config(file)

    def test_output_must_be_new_and_within_build(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(build, 'ROOT', Path(folder)):
            root=Path(folder)
            self.assertEqual(build.safe_output(root/'build'/'new'),root/'build'/'new')
            for value in (root/'elsewhere', root/'build'/'..'/'escape', root/'build'):
                with self.assertRaises(ValueError):build.safe_output(value)
            occupied=root/'build'/'old';occupied.mkdir(parents=True)
            with self.assertRaisesRegex(ValueError, 'Preserve previous'):
                build.safe_output(occupied)

if __name__=='__main__':unittest.main()
