"""Offline VDI COW assembly. Never writes the source or shared parent image.

Format reference: VirtualBox src/VBox/Storage/VDICore.h and VDI.cpp.
Only the audited dynamic VDI variant and plain 1 MiB blocks are accepted.
"""
from __future__ import annotations
import hashlib
import json
import math
import os
import re
from pathlib import Path
import shutil
import struct
import uuid

FREE, ZERO = 0xffffffff, 0xfffffffe

def require(value, message):
    if not value:
        raise ValueError(message)

def hashfile(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def safe_path(path):
    p = Path(path).absolute()
    for item in (p, *p.parents):
        if item.is_symlink():
            raise ValueError('Symbolic link path rejected')
        if item.exists():
            s = item.stat(follow_symlinks=False)
            require(not getattr(s, 'st_file_attributes', 0) & 0x400, 'Reparse point rejected')
    if p.is_file():
        require(p.stat().st_nlink == 1, 'Hard-linked file rejected')
    return p

def write_json(path, value):
    path = safe_path(path)
    with path.open('x', encoding='utf-8') as f:
        json.dump(value, f, indent=2, ensure_ascii=False)
        f.write('\n')

class Vdi:
    def __init__(self, path, parent=None):
        self.path, self.parent = safe_path(path), parent
        self.f = self.path.open('rb')
        try:
            self.h = self.f.read(512)
            require(len(self.h) == 512, 'Short VDI header')
            u32 = lambda o: struct.unpack_from('<I', self.h, o)[0]
            require(u32(64)==0xbeda107f and u32(68)==0x10001, 'Unsupported VDI signature/version')
            require((u32(72),u32(76),u32(80))==(400,1,0), 'Unsupported VDI header/type/flags')
            self.map_offset, self.data_offset = u32(340),u32(344)
            self.disk_bytes = struct.unpack_from('<Q', self.h, 368)[0]
            self.block_bytes, extra, self.blocks, self.allocated = (u32(o) for o in (376,380,384,388))
            require(self.block_bytes==1048576 and extra==0, 'Only plain 1 MiB blocks supported')
            require(0 < self.disk_bytes <= 1 << 40, 'Unsupported capacity')
            require(self.blocks==math.ceil(self.disk_bytes/self.block_bytes), 'Capacity mismatch')
            require(self.map_offset>=512 and self.map_offset+self.blocks*4<=self.data_offset, 'Map bounds')
            require(self.path.stat().st_size==self.data_offset+self.allocated*self.block_bytes, 'Physical size mismatch')
            self.f.seek(self.map_offset)
            self.mapping = list(struct.unpack(f'<{self.blocks}I',self.f.read(self.blocks*4)))
            allocated = [n for n in self.mapping if n < ZERO]
            require(len(set(allocated))==self.allocated==len(allocated), 'Block aliases/count mismatch')
            require(sorted(allocated)==list(range(self.allocated)), 'Invalid block mapping')
            self.create,self.modify,self.link,self.parent_modify = (self.h[o:o+16] for o in (392,408,424,440))
            if parent:
                require(self.link==parent.create and self.parent_modify in (bytes(16),parent.modify), 'Wrong parent identity')
                require(self.disk_bytes==parent.disk_bytes and self.blocks==parent.blocks, 'Parent geometry mismatch')
            else:
                require(self.link==bytes(16), 'Missing parent image')
        except BaseException:
            self.f.close()
            raise

    def close(self): self.f.close()
    def __enter__(self): return self
    def __exit__(self,*_): self.close()

    def block(self,index):
        require(0<=index<self.blocks,'Block out of range')
        p=self.mapping[index]
        if p==FREE: return self.parent.block(index) if self.parent else bytes(self.block_bytes)
        if p==ZERO: return bytes(self.block_bytes)
        self.f.seek(self.data_offset+p*self.block_bytes)
        b=self.f.read(self.block_bytes)
        require(len(b)==self.block_bytes,'Short physical block')
        return b

    def read(self,offset,length):
        require(offset>=0 and length>=0 and offset+length<=self.disk_bytes,'Read out of range')
        out=bytearray()
        while length:
            i,within=divmod(offset,self.block_bytes)
            n=min(length,self.block_bytes-within)
            out+=self.block(i)[within:within+n]
            offset+=n; length-=n
        return bytes(out)

    def range_hash(self,offset,length):
        h=hashlib.sha256()
        for at in range(0,length,8<<20): h.update(self.read(offset+at,min(8<<20,length-at)))
        return h.hexdigest()

def partitions(profile):
    ranges=profile['partitions']
    require(bool(ranges),'No allowed partitions')
    end=0; names=set()
    for p in ranges:
        require(type(p['offset']) is int and type(p['bytes']) is int,'Invalid range type')
        require(isinstance(p['name'],str) and re.fullmatch('[a-z][a-z0-9_-]{0,47}',p['name']),'Unsafe partition name')
        require(p['offset']>=end and p['bytes']>0,'Unsorted/overlapping range')
        end=p['offset']+p['bytes']
        require(end<=profile['virtualBytes'] and p['name'] not in names,'Range/name mismatch')
        names.add(p['name'])
    return ranges

def audit(base_path,source_path,candidate_path,profile):
    ranges=partitions(profile)
    base_hash=hashfile(base_path); source_hash=hashfile(source_path); candidate_hash=hashfile(candidate_path)
    require(base_hash==profile['baseSha256'],'Unrecognized engine base')
    with Vdi(base_path) as base, Vdi(source_path,base) as src, Vdi(candidate_path,base) as dst:
        require(src.disk_bytes==dst.disk_bytes==profile['virtualBytes'],'Target capacity mismatch')
        require((src.create,src.link,src.parent_modify)==(dst.create,dst.link,dst.parent_modify),'Private disk identity changed')
        changed=[]
        for i in range(src.blocks):
            offset=i*src.block_bytes; valid=min(src.block_bytes,src.disk_bytes-offset)
            before,after=src.block(i)[:valid],dst.block(i)[:valid]
            if before==after:
                require(src.mapping[i]==dst.mapping[i],'Unchanged block mapping changed')
                continue
            changed.append(i); cursor=0
            for p in ranges:
                a,b=max(0,p['offset']-offset),min(valid,p['offset']+p['bytes']-offset)
                if a>=b: continue
                require(before[cursor:a]==after[cursor:a],'Change outside allowed partitions')
                cursor=b
            require(before[cursor:]==after[cursor:],'Change outside allowed partitions')
        require(bool(changed),'No effective image changes')
        allowed=set(range(388,392))|set(range(408,424))
        for i in changed:
            if src.mapping[i]>=ZERO:
                allowed.update(range(src.map_offset+4*i,src.map_offset+4*i+4))
            else: require(src.mapping[i]==dst.mapping[i],'Existing physical mapping changed')
        src.f.seek(0);dst.f.seek(0)
        a,b=src.f.read(src.data_offset),dst.f.read(dst.data_offset)
        require(len(a)==len(b) and all(x==y or i in allowed for i,(x,y) in enumerate(zip(a,b))), 'Unexpected VDI metadata change')
        result=[]
        for p in ranges:
            require(src.range_hash(p['offset'],p['bytes'])==p['sourceSha256'],'Source partition mismatch')
            result.append({'name':p['name'],'offset':p['offset'],'bytes':p['bytes'],
                           'sha256':dst.range_hash(p['offset'],p['bytes'])})
    require(hashfile(base_path)==base_hash and hashfile(source_path)==source_hash and hashfile(candidate_path)==candidate_hash,'Inputs changed during audit')
    return {'schemaVersion':1,'verified':True,'profileId':profile['id'],'baseSha256':base_hash,
            'sourceDiskSha256':source_hash,'candidateSha256':candidate_hash,
            'allowedPartitions':[{'offset':p['offset'],'bytes':p['bytes']} for p in ranges],
            'partitionResults':result,'comparedVirtualBytes':profile['virtualBytes'],
            'outsideAllowedUnchanged':True,'changedVirtualBlocks':changed,'deviceValidated':False}

def assemble(base_path,source_path,replacements,profile,output_dir):
    ranges=partitions(profile)
    require(set(replacements)=={p['name'] for p in ranges},'Supply exactly all target partitions')
    out=safe_path(output_dir);out.mkdir(parents=True,exist_ok=False)
    output=out/'system-diff.vdi'
    base_hash=hashfile(base_path); source_hash=hashfile(source_path)
    require(base_hash==profile['baseSha256'],'Unrecognized engine base')
    replacement_hashes={name:hashfile(path) for name,path in replacements.items()}
    with Vdi(base_path) as base,Vdi(source_path,base) as src:
        require(src.disk_bytes==profile['virtualBytes'],'Target capacity mismatch')
        for p in ranges:
            require(safe_path(replacements[p['name']]).stat().st_size==p['bytes'],'Partition size mismatch')
            require(src.range_hash(p['offset'],p['bytes'])==p['sourceSha256'],'Source partition mismatch')
        with Path(source_path).open('rb') as r,output.open('xb') as w: shutil.copyfileobj(r,w)
        mapping=src.mapping.copy(); allocated=src.allocated
        with output.open('r+b') as f:
            for i in range(src.blocks):
                off=i*src.block_bytes
                relevant=[p for p in ranges if p['offset']<off+src.block_bytes and p['offset']+p['bytes']>off]
                if not relevant: continue
                before=src.block(i); after=bytearray(before)
                for p in relevant:
                    a,b=max(off,p['offset']),min(off+src.block_bytes,p['offset']+p['bytes'])
                    with Path(replacements[p['name']]).open('rb') as part:
                        part.seek(a-p['offset']); data=part.read(b-a)
                    require(len(data)==b-a,'Short replacement')
                    after[a-off:b-off]=data
                if before==after:continue
                physical=mapping[i]
                if physical>=ZERO:physical=allocated;allocated+=1;mapping[i]=physical
                f.seek(src.data_offset+physical*src.block_bytes);f.write(after)
            f.seek(src.map_offset);f.write(struct.pack(f'<{src.blocks}I',*mapping))
            f.seek(388);f.write(struct.pack('<I',allocated))
            f.seek(408);f.write(uuid.uuid4().bytes_le)
            f.flush();os.fsync(f.fileno())
    evidence=audit(base_path,source_path,output,profile)
    require(hashfile(source_path)==source_hash,'Source changed')
    for p in evidence['partitionResults']:
        require(p['sha256']==replacement_hashes[p['name']]==hashfile(replacements[p['name']]),'Replacement changed/readback mismatch')
    write_json(out/'audit.json',evidence)
    manifest={'schemaVersion':1,'profileId':profile['id'],'diskFile':'system-diff.vdi',
              'diskSha256':evidence['candidateSha256'],'sourceDiskSha256':source_hash,
              'baseSha256':base_hash,'auditFile':'audit.json','auditSha256':hashfile(out/'audit.json')}
    write_json(out/'candidate.json',manifest)
    return manifest
