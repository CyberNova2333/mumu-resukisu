"""Target-bound ext2 data-slot editing and newc initramfs assembly."""
import gzip
import hashlib
from pathlib import PurePosixPath
import struct
from .images import require

SOURCE_BOOT='c00d2193e7d733a7d7df7cf3ba6d261f111901b1aed184e3934c44fc38a8753c'
SOURCE_KERNEL='48e7b624b3652ccbc215e25b24432bab052081b55aa806e77c28fe4c8bd9a685'
PATCHED_KERNEL='7c549726126ffff501c88302e76d7c0706804eb6f709eb30ff6802eecbff8698'
def sha(b):return hashlib.sha256(b).hexdigest()

class BootFs:
    def __init__(self, data):
        require(sha(data)==SOURCE_BOOT,'Unknown boot filesystem')
        self.data=bytearray(data)
        require(self.u16(1080)==0xef53 and self.u32(1100)==0,'Unsupported ext2 revision')
        self.bs=1024<<self.u32(1048);require(self.bs==1024,'Unsupported ext2 block size')
        self.ipg=self.u32(1064);require(self.ipg>0,'Invalid inode geometry')
        directory,_=self.file(2);self.names={};at=0
        while at<len(directory):
            require(at+8<=len(directory),'Truncated directory')
            ino,rec,n=struct.unpack_from('<IHH',directory,at)
            require(rec>=8 and rec%4==0 and at+rec<=len(directory) and n<=rec-8,'Invalid directory entry')
            if ino:
                name=directory[at+8:at+8+n].decode('ascii')
                require(name not in self.names,'Duplicate directory name');self.names[name]=ino
            at+=rec
    def u16(self,o):return struct.unpack_from('<H',self.data,o)[0]
    def u32(self,o):return struct.unpack_from('<I',self.data,o)[0]
    def file(self,n):
        g,s=divmod(n-1,self.ipg);table=self.u32(2*self.bs+32*g+8);off=table*self.bs+128*s
        require(0<=off and off+128<=len(self.data),'Inode outside image')
        size=self.u32(off+4);count=(size+self.bs-1)//self.bs
        require(count<=len(self.data)//self.bs,'File too large')
        ptr=struct.unpack_from('<15I',self.data,off+40);blocks=[];seen=set()
        def take(p,depth):
            require(0<p<len(self.data)//self.bs and p not in seen,'Invalid/aliased block')
            seen.add(p)
            if depth==0:blocks.append(p);return
            for q in struct.unpack_from('<256I',self.data,p*self.bs):
                if len(blocks)>=count:break
                take(q,depth-1)
        for i,p in enumerate(ptr):
            if len(blocks)>=count:break
            take(p,max(0,i-11))
        require(len(blocks)==count,'File block count mismatch')
        return b''.join(self.data[b*self.bs:(b+1)*self.bs] for b in blocks)[:size],blocks
    def replace(self,name,value):
        old,blocks=self.file(self.names[name]);require(len(old)==len(value),'Data-slot size must stay unchanged')
        for i,b in enumerate(blocks):
            chunk=value[i*self.bs:(i+1)*self.bs];self.data[b*self.bs:b*self.bs+len(chunk)]=chunk
        require(self.file(self.names[name])[0]==value,'Readback mismatch')

def unpack(raw):
    entries=[];at=0;names=set()
    while at+110<=len(raw):
        require(raw[at:at+6]==b'070701','Only unchecksummed newc supported')
        fields=[int(raw[at+6+i*8:at+14+i*8],16) for i in range(13)]
        length=fields[11];require(length>0 and at+110+length<=len(raw),'Invalid cpio name')
        encoded=raw[at+110:at+110+length];require(encoded[-1:]==b'\0','Missing cpio terminator')
        name=encoded[:-1].decode();start=(at+110+length+3)&~3
        data=raw[start:start+fields[6]];require(len(data)==fields[6],'Truncated cpio data')
        at=(start+fields[6]+3)&~3
        if name=='TRAILER!!!':return entries
        require(name and not name.startswith('/') and '..' not in PurePosixPath(name).parts and name not in names,'Unsafe/duplicate cpio name')
        names.add(name);entries.append((name,fields,data))
    raise ValueError('No cpio terminator')

def serialize(name,fields,data):
    fields=list(fields);fields[6]=len(data);fields[11]=len(name.encode())+1
    b=b'070701'+b''.join(f'{v:08x}'.encode() for v in fields)+name.encode()+b'\0'
    b+=bytes(-len(b)%4);b+=data;b+=bytes(-len(b)%4);return b

def initramfs(original,loader,module,prestage):
    entries=unpack(gzip.decompress(original));names={n for n,_,_ in entries}
    additions={'init':(loader,0o100750),'kernelsu.ko':(module,0o100600),'resukisu-migrate-prestage':(prestage,0o100750)}
    require('init' in names and not (names & {'init.real','kernelsu.ko','resukisu-migrate-prestage'}),'Unexpected existing boot entries')
    packed=[];expected={}
    for name,fields,data in entries:
        if name=='init':
            marker=b'export ANDROID_ROOT=/system\n'
            guard=b'\n# Refuse Android startup when fixed input validation fails.\nif ! /resukisu-migrate-prestage; then\n    /bin/busybox sync\n    /bin/busybox poweroff -f\n    while :; do /bin/busybox sleep 1; done\nfi\n'
            require(data.count(marker)==1,'Unexpected init script')
            data=data.replace(marker,marker+guard);name='init.real'
            require(data.index(guard)<data.index(b'insmod /system/lib64/vaddress.ko') and data.index(guard)<data.index(b'exec switch_root /android /init'),'Guard installed too late')
        expected[name]=(fields[1],data);packed.append(serialize(name,fields,data))
    ino=max(f[0] for _,f,_ in entries)+1
    for i,(name,(data,mode)) in enumerate(additions.items()):
        expected[name]=(mode,data);packed.append(serialize(name,[ino+i,mode,0,0,1,0,0,0,0,0,0,0,0],data))
    packed.append(serialize('TRAILER!!!',[0,0,0,0,1,0,0,0,0,0,0,0,0],b''))
    raw=b''.join(packed);raw+=bytes(-len(raw)%512)
    require({n:(f[1],d) for n,f,d in unpack(raw)}==expected,'CPIO reparse mismatch')
    return raw
