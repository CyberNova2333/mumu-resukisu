#!/usr/bin/env python3
"""Offline Linux/WSL build of the pinned target module and PID1-only loader."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]
PROFILE = json.loads((ROOT/'native/driver/target-profile.json').read_text())

def require(ok, message):
    if not ok:
        raise RuntimeError(message)

def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def verify_module(path, target_names):
    from elftools.elf.elffile import ELFFile
    expected = json.loads((ROOT/'native/driver/undefined-symbols.json').read_text())
    with path.open('rb') as f:
        e = ELFFile(f)
        require(e.elfclass==64 and e['e_machine']=='EM_X86_64' and e['e_type']=='ET_REL', 'wrong module ELF')
        symbols = e.get_section_by_name('.symtab')
        names=sorted(s.name for s in symbols.iter_symbols() if s['st_shndx']=='SHN_UNDEF' and s.name)
        require(names==expected and len(names)==235, 'undefined symbol set changed; target re-review required')
        require(set(names)<=target_names, 'module imports absent from supplied core symbol evidence')
        sec=e.get_section_by_name('.gnu.linkonce.this_module')
        require(sec and sec['sh_size']==PROFILE['moduleStructBytes'], 'module ABI size mismatch')
        for n in ('__versions','.kcfi_traps'):
            s=e.get_section_by_name(n)
            require(s is None or s['sh_size']==0, 'unsupported version/CFI section')
        rel=e.get_section_by_name('.rela.gnu.linkonce.this_module')
        actual=[(r['r_offset'],symbols.get_symbol(r['r_info_sym']).name,r['r_addend']) for r in rel.iter_relocations()]
        require(actual==[(368,'init_module',0),(848,'cleanup_module',0)], 'module init/exit ABI mismatch')
        require(symbols.get_symbol_by_name('resukisu_mumu_init') and not symbols.get_symbol_by_name('kernelsu_init'), 'old init blacklist name conflict')
        info=e.get_section_by_name('.modinfo').data()
        require(b'vermagic=6.1.90-perf+ ' in info, 'wrong kernel release')
    data=path.read_bytes()
    require(b'/data/adb/resukisu-stage' not in data, 'diagnostic observer unexpectedly present')
    for phase,count in (('post-fs-data',1),('services',2),('boot-completed',1)):
        require(data.count(('exec u:r:ksu:s0 root -- /data/adb/ksud '+phase+'\n').encode())==count, 'normal RC wiring differs')
    return names

def parse_symbols(path):
    names=set()
    for line in path.read_text().splitlines():
        fields=line.split()
        if len(fields)==3:
            try: address=int(fields[0],16)
            except ValueError: continue
            if address and fields[1] not in ('U','u'): names.add(fields[2])
    require(len(names)>=235, 'expected nonzero core kallsyms lines; module-owned entries excluded')
    return names

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--resukisu-source',type=Path,required=True,help='Git repository containing the pinned commit; working edits are ignored')
    ap.add_argument('--kernel-archive',type=Path,required=True,help='Exact pinned ACK tar.gz; checked before extraction')
    ap.add_argument('--kernel-config',type=Path,required=True,help='Exact original target config/config.gz or prepared config')
    ap.add_argument('--clang',type=Path,required=True,help='Toolchain directory containing bin/clang')
    ap.add_argument('--target-symbols',type=Path,required=True,help='Local core kallsyms evidence, not embedded addresses')
    ap.add_argument('--output',type=Path,default=ROOT/'build/native')
    ap.add_argument('--work-dir',type=Path,help='New Linux filesystem directory for source/Kbuild; avoids slow cross-filesystem extraction')
    ap.add_argument('--jobs',type=int,default=4)
    ap.add_argument('--check-inputs',action='store_true')
    args=ap.parse_args()
    require(sys.platform.startswith('linux'),'Use Linux/WSL; no emulator commands are invoked')
    require(1<=args.jobs<=32,'jobs must be 1..32')
    args.output=args.output.resolve()
    require(args.output.is_relative_to((ROOT/'build').resolve()) and args.output!=(ROOT/'build').resolve(),'output must be a new subdirectory of project build/')
    require(not args.output.exists(),'preserve previous build; choose a new output directory')
    for name in ('resukisu_source','kernel_archive','kernel_config','clang','target_symbols'):
        setattr(args,name,getattr(args,name).resolve(strict=True))
    require(sha(args.kernel_archive)==PROFILE['kernelArchiveSha256'],'ACK archive mismatch')
    config=args.kernel_config.read_bytes()
    if config.startswith(b'\x1f\x8b'): config=gzip.decompress(config)
    config_sha=hashlib.sha256(config).hexdigest()
    require(config_sha in (PROFILE['preparedConfigSha256'],'6a60ae3570151e23df0344c628afeea58b4b3cad5ad6e204839141a737291680'),'target config mismatch')
    rev=subprocess.check_output(['git','-C',str(args.resukisu_source),'rev-parse',PROFILE['resukisuCommit']+'^{commit}'],text=True).strip()
    require(rev==PROFILE['resukisuCommit'],'source commit mismatch')
    compiler=subprocess.check_output([str(args.clang/'bin/clang'),'--version'],text=True)
    require(all(v in compiler for v in PROFILE['compilerVersionContains']),'compiler family mismatch')
    names=parse_symbols(args.target_symbols)
    require(set(json.loads((ROOT/'native/driver/undefined-symbols.json').read_text()))<=names,'missing target symbols')
    if args.check_inputs:
        print(json.dumps({'inputsVerified':True,'deviceTouched':False,'sourceCommit':rev})); return
    args.output.mkdir(parents=True,exist_ok=False)
    out=args.output
    work=args.work_dir.resolve() if args.work_dir else out/'work'
    require(not work.exists(),'work directory must be new; previous work is preserved')
    require(not any(p.is_relative_to(work) for p in (ROOT,args.resukisu_source,args.kernel_archive,args.kernel_config,args.clang,args.target_symbols)), 'work directory cannot contain an input')
    work.mkdir(parents=True,exist_ok=False)
    env=os.environ.copy()
    env.update(PATH=str(args.clang/'bin')+':'+env['PATH'],ARCH='x86_64',LLVM='1',LLVM_IAS='1',LOCALVERSION='+',KBUILD_BUILD_USER='builder',KBUILD_BUILD_HOST='builder')
    def run(label,command,expected=0,timeout=300):
        q=subprocess.run([str(v) for v in command],env=env,capture_output=True,timeout=timeout)
        (out/(label+'.log')).write_bytes(q.stdout+q.stderr)
        require(q.returncode==expected,label+' failed; see build log')
        print(label+': passed',flush=True)
        return q.stdout
    source=work/'resukisu'; source.mkdir()
    archive=work/'resukisu.tar'
    with archive.open('xb') as f:
        subprocess.run(['git','-C',str(args.resukisu_source),'archive',rev],stdout=f,check=True,timeout=60)
    with tarfile.open(archive) as t: t.extractall(source,filter='data')
    run('apply-source-check',['git','-C',source,'apply','--check',ROOT/'native/driver/resukisu.patch'])
    run('apply-source',['git','-C',source,'apply',ROOT/'native/driver/resukisu.patch'])
    run('apply-metadata-check',['git','-C',source,'apply','--check',ROOT/'native/driver/build-metadata.patch'])
    run('apply-metadata',['git','-C',source,'apply',ROOT/'native/driver/build-metadata.patch'])
    kernel=work/'ack'; kernel.mkdir()
    with tarfile.open(args.kernel_archive) as t: t.extractall(kernel,filter='data')
    include=work/'include/linux'; include.mkdir(parents=True)
    shutil.copy2(kernel/'include/linux/sched.h',include/'sched.h')
    run('apply-abi-check',['git','-C',work,'apply','--check',ROOT/'native/driver/ack-task-layout.patch'])
    run('apply-abi',['git','-C',work,'apply',ROOT/'native/driver/ack-task-layout.patch'])
    kbuild=work/'kbuild'; kbuild.mkdir(); (kbuild/'.config').write_bytes(config)
    if config_sha!=PROFILE['preparedConfigSha256']:
        run('apply-config-check',['git','-C',kbuild,'apply','--check',ROOT/'native/driver/prepare-config.patch'])
        run('apply-config',['git','-C',kbuild,'apply',ROOT/'native/driver/prepare-config.patch'])
    require(sha(kbuild/'.config')==PROFILE['preparedConfigSha256'],'config preparation mismatch')
    make=['make','-C',kernel,'O='+str(kbuild),'-j'+str(args.jobs)]
    run('olddefconfig',make+['olddefconfig'])
    require(sha(kbuild/'.config')==PROFILE['preparedConfigSha256'],'prepared config changed; no implicit ABI drift')
    run('modules-prepare',make+['modules_prepare'])
    run('headers-install',make+['headers_install','INSTALL_HDR_PATH='+str(kbuild/'usr')])
    run('selinux-headers',make+['security/selinux/built-in.a'])
    run('module',make+['M='+str(source/'kernel'),'NOSTDINC_FLAGS=-nostdinc -I'+str(work/'include'),
        'KCFLAGS=-DCONFIG_KSU_TARGET_MUMU_A15_ABI=1','CONFIG_KSU=m','CONFIG_KSU_MULTI_MANAGER_SUPPORT=y',
        'CONFIG_KSU_TRACEPOINT_HOOK=y','CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER=',
        'KSU_UAPI_INC='+str(kbuild/'usr/include'),'KBUILD_MODPOST_WARN=1','modules'])
    module=out/'kernelsu.ko';shutil.copy2(source/'kernel/kernelsu.ko',module)
    verify_module(module,names)
    loader=out/'loader';loader.mkdir()
    for name in ('loader.c','test-loader.c'): shutil.copy2(ROOT/'native/loader'/name,loader/name)
    (loader/'expected_probe.h').write_text('#define EXPECTED_PROBE_BYTES '+str(module.stat().st_size)+'\nstatic const unsigned char expected_probe_sha256[32]={'+','.join('0x'+sha(module)[i:i+2] for i in range(0,64,2))+'};\n')
    run('loader-build',['musl-gcc','-static','-Os','-Wall','-Wextra','-Werror','-fstack-protector-strong','-fPIE','-Wl,-z,noexecstack','-s',loader/'loader.c','-o',out/'driver-init'])
    run('loader-tests-build',['musl-gcc','-static','-O1','-Wall','-Wextra','-Werror','-include','stddef.h',loader/'test-loader.c','-o',out/'test-loader'])
    run('loader-tests',[out/'test-loader',module])
    run('loader-host-gate',[out/'driver-init'],64)
    manifest={'schemaVersion':1,'profileId':PROFILE['id'],'sourceCommit':rev,'deviceValidated':False,
        'module':{'path':'kernelsu.ko','sha256':sha(module),'bytes':module.stat().st_size},
        'loader':{'path':'driver-init','sha256':sha(out/'driver-init'),'bytes':(out/'driver-init').stat().st_size},
        'fixedParameters':PROFILE['runtimeParameters'],'symbolCount':235,'staticModuleChecksPassed':True,
        'loaderTestsPassed':True,'hostGatePassed':True,'sourcePatchSha256':sha(ROOT/'native/driver/resukisu.patch'),
        'abiPatchSha256':sha(ROOT/'native/driver/ack-task-layout.patch')}
    manifest['buildMetadataPatchSha256']=sha(ROOT/'native/driver/build-metadata.patch')
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))

if __name__=='__main__': main()
