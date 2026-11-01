# SPDX-License-Identifier: GPL-3.0-or-later
"""Execute the actual ELF32 instructions with mocked kernel interrupts; no device."""
import argparse,hashlib,json,pathlib,struct
from elftools.elf.elffile import ELFFile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_INTR
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary',type=pathlib.Path,required=True)
parser.add_argument('--output',type=pathlib.Path,required=True)
args=parser.parse_args()
P=args.binary.resolve()
def case(name,uid=2000,cap=0,seccomp=0,fd=3,fdname=b'anon_inode:[ksu_driver]',ioctl=0,version=35253):
    em=Uc(UC_ARCH_X86,UC_MODE_32)
    with P.open('rb') as f:
        elf=ELFFile(f);entry=elf.header['e_entry']
        mapped=set()
        for s in elf.iter_segments():
            if s['p_type']!='PT_LOAD':continue
            lo=s['p_vaddr']&~4095;hi=(s['p_vaddr']+s['p_memsz']+4095)&~4095
            for a in range(lo,hi,4096):
                if a not in mapped:em.mem_map(a,4096);mapped.add(a)
            em.mem_write(s['p_vaddr'],s.data())
    em.mem_map(0x70000000,0x10000);em.reg_write(UC_X86_REG_ESP,0x7000f000)
    calls=[];output=bytearray();status=[]
    def cstring(a):
        b=bytes(em.mem_read(a,64));return b.split(b'\0',1)[0]
    def interrupt(uc,number,user):
        assert number==0x80
        nr=uc.reg_read(UC_X86_REG_EAX);a=uc.reg_read(UC_X86_REG_EBX);b=uc.reg_read(UC_X86_REG_ECX);c=uc.reg_read(UC_X86_REG_EDX);d=uc.reg_read(UC_X86_REG_ESI)
        calls.append(nr);ret=0
        if nr in (199,201):ret=uid
        elif nr==184:
            assert bytes(uc.mem_read(a,8))==struct.pack('<II',0x20080522,0)
            uc.mem_write(b,struct.pack('<6I',cap,0,0,0,0,0))
        elif nr==172:assert a==21;ret=seccomp
        elif nr==88:
            assert (a,b,c)==(0xdeadbeef,0xcafebabe,0)
            assert struct.unpack('<i',bytes(uc.mem_read(d,4)))[0]==-1
            uc.mem_write(d,struct.pack('<i',fd));ret=-1  # Native syscall EPERM even when FD transport succeeds.
        elif nr==85:
            assert cstring(a)==('/proc/self/fd/'+str(fd)).encode()
            uc.mem_write(b,fdname);ret=len(fdname)
        elif nr==54:
            assert a==fd and b==0x80104b02
            if not ioctl:uc.mem_write(c,struct.pack('<4I',version,1,7,6))
            ret=ioctl
        elif nr==6:assert a==fd and fd>=0
        elif nr==4:assert a==1;output.extend(uc.mem_read(b,c));ret=c
        elif nr==1:status.append(a);uc.emu_stop()
        else:raise AssertionError(('unexpected syscall',nr))
        uc.reg_write(UC_X86_REG_EAX,ret&0xffffffff)
    em.hook_add(UC_HOOK_INTR,interrupt)
    em.emu_start(entry,0,count=5000)
    assert len(status)==1
    result={'name':name,'exitCode':status[0],'syscalls':calls,'report':json.loads(output) if output else None}
    return result
results=[]
for fd in [0,3,123456,2147483647]:
    r=case('valid_fd_'+str(fd),fd=fd);assert r['exitCode']==0 and r['report']['status']=='direct_fd_get_info_verified';results.append(r)
r=case('absent_fd',fd=-1);assert r['exitCode']==71 and r['report']['status']=='direct_fd_absent' and 6 not in r['syscalls'];results.append(r)
r=case('root_identity',uid=0);assert r['exitCode']==64 and r['syscalls']==[199,1];results.append(r)
for name,kw in [('sys_boot_cap',{'cap':1<<22}),('filter_enabled',{'seccomp':2})]:
    r=case(name,**kw);assert r['exitCode']==72 and 88 not in r['syscalls'];results.append(r)
r=case('wrong_fd',fdname=b'anon_inode:[ksu_driver_su]');assert r['exitCode']==65 and 54 not in r['syscalls'];results.append(r)
r=case('ioctl_error',ioctl=-25);assert r['exitCode']==66;results.append(r)
r=case('wrong_version',version=32525);assert r['exitCode']==67;results.append(r)
out={'purpose':'actual_ELF32_machine_code_with_mock_kernel_interrupts_not_device_evidence','binarySha256':hashlib.sha256(P.read_bytes()).hexdigest(),'testsPassed':len(results),'syscallErrnoIgnoredWhenReturnedFdValid':True,'grantRootOrWriteIoctlAbsent':True,'results':results}
args.output.write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in out.items() if k!='results'},indent=2))
