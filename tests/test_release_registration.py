# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
from pathlib import Path
import struct
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location('release_registration',ROOT/'scripts/register-release.py')
registration=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(registration)

class RegistrationTests(unittest.TestCase):
    def test_import_does_not_run_tools(self):
        with patch('subprocess.run',side_effect=AssertionError('import executed tool')):
            SPEC.loader.exec_module(importlib.util.module_from_spec(SPEC))

    def test_signature_full_verification_and_unique_signer_required(self):
        lines=['Verifies','Verified using v2 scheme (APK Signature Scheme v2): true',
               'Number of signers: 1',f'Signer #1 certificate SHA-256 digest: {registration.SIGNER}']
        text='\n'.join(lines)
        self.assertTrue(registration.verify_signature_output(text))
        for index in range(len(lines)):
            self.assertFalse(registration.verify_signature_output('\n'.join(lines[:index]+lines[index+1:])))
        self.assertFalse(registration.verify_signature_output(text.replace(registration.SIGNER,'0'*64)))
        self.assertFalse(registration.verify_signature_output(text+'\n'+lines[-1]))
        self.assertFalse(registration.verify_signature_output(text.replace('Number of signers: 1','Number of signers: 2')))

    def test_only_one_uapi_definition(self):
        line='static const __u32 KERNEL_SU_UAPI_VERSION = 6;'
        self.assertEqual(registration.source_uapi(line),6)
        for value in ('',line+line,line.replace('6','7')):
            with self.assertRaises(ValueError):registration.source_uapi(value)

    def test_elf_architecture_and_truncation(self):
        data=bytearray(64);data[:6]=b'\x7fELF\x02\x01'
        struct.pack_into('<HHI',data,16,3,62,1);struct.pack_into('<H',data,52,64)
        self.assertTrue(registration.read_elf(data))
        self.assertFalse(registration.read_elf(data[:63]))
        for offset in (0,4,5,16,18,20,52):
            changed=data.copy();changed[offset]^=1
            self.assertFalse(registration.read_elf(changed))

if __name__=='__main__':unittest.main()
