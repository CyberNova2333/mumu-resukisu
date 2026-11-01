#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build isolated Frida validation helpers only; never starts a device/server."""
import argparse
import datetime
import importlib.util
import json
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'native/tools/frida'
spec=importlib.util.spec_from_file_location('userspace_builder',ROOT/'scripts/build-userspace.py')
support=importlib.util.module_from_spec(spec);spec.loader.exec_module(support)
SERVER_SHA='e6f7e35901db8e83d40088d51692afa77bd23c8ae69e521c3e5955f741a7b9d6'
SERVER_BYTES=116223160

def pin(path,prefix,size,digest):
    path.write_text(f'#define {prefix.upper()}_BYTES {size}\nstatic const unsigned char {prefix}_hash[32]={{'+','.join('0x'+digest[i:i+2] for i in range(0,64,2))+'};\n',encoding='utf-8')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ndk-bin',type=Path,required=True,help='NDK LLVM prebuilt bin for the host platform')
    p.add_argument('--server',type=Path,required=True,help='Previously authenticated official x64 server')
    p.add_argument('--port',type=int,required=True,help='Explicit unused loopback port for later controlled tests')
    p.add_argument('--output',type=Path,default=ROOT/'build/frida')
    p.add_argument('--wsl-distro');p.add_argument('--wsl-user')
    a=p.parse_args()
    support.require(1024<=a.port<=65535,'Port outside allowed range')
    support.require(a.server.is_file() and a.server.stat().st_size==SERVER_BYTES and support.digest(a.server)==SERVER_SHA,'Unsupported official server content')
    suffix='.exe' if os.name=='nt' else ''
    cc=a.ndk_bin/('clang'+suffix);readelf=a.ndk_bin/('llvm-readelf'+suffix)
    support.require(cc.is_file() and readelf.is_file(),'NDK LLVM tools missing')
    out=support.safe_output(a.output);r=support.Runner(a)
    support.require(r.run(['id','-u']).strip()!='0','Host tests must use an ordinary user')
    out.mkdir(parents=True)
    report={'schemaVersion':1,'status':'building','deviceValidated':False,'serverSha256':SERVER_SHA,
            'startedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'binaries':{},'loopbackPort':a.port}
    log=[]
    def run(args):
        q=subprocess.run(list(map(str,args)),capture_output=True,text=True,encoding='utf-8',timeout=60)
        log.append({'program':Path(str(args[0])).name,'exitCode':q.returncode,'stdout':q.stdout,'stderr':q.stderr})
        if q.returncode:raise RuntimeError(q.stderr[-2000:])
        return q.stdout
    try:
        flags=['--target=x86_64-linux-android35','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fPIE','-pie','-Wl,-z,relro,-z,now']
        target=out/'resukisu-frida-probe'
        run([cc,*flags,'-Wl,--export-dynamic','-fno-omit-frame-pointer',SOURCE/'target.c','-o',target])
        pin(out/'target-pin.h','target',target.stat().st_size,support.digest(target))
        pin(out/'server-pin.h','server',SERVER_BYTES,SERVER_SHA)
        (out/'runtime-config.h').write_text('#define FRIDA_LISTEN "127.0.0.1:'+str(a.port)+'"\n',encoding='utf-8')
        supervisor=out/'resukisu-frida-supervisor'
        run([cc,*flags,'-I'+str(out),SOURCE/'supervisor.c','-o',supervisor])
        for binary in (target,supervisor):
            details=run([readelf,'-hl','--dyn-syms',binary])
            support.require('ELF64' in details and '/system/bin/linker64' in details,'Expected Bionic x64 executable')
            if binary==target:support.require('resukisu_probe_value' in details and 'resukisu_probe_nonce' in details,'Missing target exports')
            report['binaries'][binary.name]={'file':binary.name,'bytes':binary.stat().st_size,'sha256':support.digest(binary)}
        host=out/'supervisor-unit'
        r.run(['gcc','-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function',
            '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+r.path(out),r.path(SOURCE/'unit.c'),'-o',r.path(host)])
        result=r.run([r.path(host)])
        report.update(status='host_tests_passed_not_device_validated',hostTests={'passed':True,'output':result.strip()},
            agentSha256=support.digest(SOURCE/'agent.js'),finishedAt=datetime.datetime.now(datetime.timezone.utc).isoformat())
    except BaseException as e:
        report.update(status='failed',failureType=type(e).__name__,failureReason=str(e));raise
    finally:
        (out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        (out/'build-log.json').write_text(json.dumps(log+r.events,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
