import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
from mumu_resukisu.images import Vdi, assemble, audit, hashfile, FREE, ZERO

B=1<<20
def make(path, blocks, parent=None):
    header=bytearray(512); mapped=[x for x in blocks if isinstance(x,bytes)]
    for off,val in {64:0xbeda107f,68:0x10001,72:400,76:1,340:512,344:1024,
                    376:B,384:len(blocks),388:len(mapped)}.items():struct.pack_into('<I',header,off,val)
    struct.pack_into('<Q',header,368,len(blocks)*B)
    header[392:408]=b'S'*16 if parent else b'B'*16
    header[408:424]=b'M'*16
    if parent:header[424:440]=b'B'*16
    mapping=[]; index=0
    for x in blocks:
        mapping.append(index if isinstance(x,bytes) else x)
        index+=isinstance(x,bytes)
    path.write_bytes(header+struct.pack(f'<{len(mapping)}I',*mapping)+bytes(512-len(mapping)*4)+b''.join(mapped))

class ImagesTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.base=self.root/'base.vdi';self.source=self.root/'source.vdi'
        make(self.base,[b'a'*B,b'b'*B,b'c'*B]);make(self.source,[FREE,ZERO,FREE],True)
        self.profile={'id':'fixture','virtualBytes':B*3,'baseSha256':hashfile(self.base),
                      'partitions':[{'name':'part','offset':B-10,'bytes':30,
                         'sourceSha256':hashlib.sha256(b'a'*10+bytes(20)).hexdigest()}]}
        self.part=self.root/'part.img';self.part.write_bytes(b'X'*30)
    def tearDown(self):self.tmp.cleanup()
    def build(self):return assemble(self.base,self.source,{'part':self.part},self.profile,self.root/'out')
    def test_cross_block_cow_and_zero_semantics(self):
        original=hashfile(self.source);self.build()
        with Vdi(self.base) as base,Vdi(self.root/'out/system-diff.vdi',base) as v:
            self.assertEqual(v.read(B-11,32),b'a'+b'X'*30+b'\0')
            self.assertEqual(v.mapping[2],FREE)
        self.assertEqual(hashfile(self.source),original)
    def test_reject_outside_change(self):
        self.build();p=self.root/'out/system-diff.vdi'
        with p.open('r+b') as f:f.seek(1024);f.write(b'z')
        with self.assertRaisesRegex(ValueError,'outside'):audit(self.base,self.source,p,self.profile)
    def test_reject_wrong_parent(self):
        with self.source.open('r+b') as f:f.seek(424);f.write(b'Z'*16)
        with Vdi(self.base) as b:
            with self.assertRaisesRegex(ValueError,'parent'):Vdi(self.source,b)
    def test_reject_alias_and_bounds(self):
        make(self.source,[b'x'*B,b'y'*B,FREE],True)
        with self.source.open('r+b') as f:f.seek(516);f.write(struct.pack('<I',0))
        with Vdi(self.base) as b:
            with self.assertRaisesRegex(ValueError,'alias'):Vdi(self.source,b)
        with Vdi(self.base) as b:
            with self.assertRaisesRegex(ValueError,'range'):b.read(-1,4)
    def test_no_overwrite(self):
        self.build()
        with self.assertRaises(FileExistsError):self.build()
    def test_source_fingerprint_required(self):
        self.profile['partitions'][0]['sourceSha256']='0'*64
        with self.assertRaisesRegex(ValueError,'Source partition'):self.build()
    def test_overlap_rejected(self):
        self.profile['partitions']*=2
        with self.assertRaisesRegex(ValueError,'overlapping'):self.build()
    def test_traversal_partition_name_rejected(self):
        self.profile['partitions'][0]['name']='../escape'
        with self.assertRaisesRegex(ValueError,'Unsafe partition'):self.build()

if __name__=='__main__':unittest.main()
