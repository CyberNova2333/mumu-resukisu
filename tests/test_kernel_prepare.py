import importlib.util
from pathlib import Path
import struct
import unittest

SCRIPT=Path(__file__).resolve().parents[1]/'scripts/prepare-kernel.py'
spec=importlib.util.spec_from_file_location('kernel_prepare',SCRIPT)
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)

class KernelDecoderTests(unittest.TestCase):
    def test_literals_and_overlapping_match(self):
        self.assertEqual(mod.decode_block_independently(b'\x30abc'),b'abc')
        self.assertEqual(mod.decode_block_independently(b'\x14a\x01\x00'),b'a'*9)

    def test_truncated_and_invalid_distance(self):
        for bad in (b'\xf0', b'\x30ab', b'\x00\x00\x00', b'\x00\x01\x00'):
            with self.assertRaises(AssertionError):mod.decode_block_independently(bad)

    def test_stream_requires_exact_length_and_zero_tail(self):
        literal=b'\x30abc'
        stream=struct.pack('<II',mod.MAGIC,len(literal))+literal+bytes(4)+struct.pack('<I',3)
        self.assertEqual(mod.split_stream(stream,permit_zero_padding=True)[1],b'abc')
        with self.assertRaises(AssertionError):mod.split_stream(stream,permit_zero_padding=False)
        with self.assertRaises(AssertionError):mod.split_stream(stream[:-4]+struct.pack('<I',4),permit_zero_padding=True)

if __name__=='__main__':unittest.main()
