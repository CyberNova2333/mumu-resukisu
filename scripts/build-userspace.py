#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build fixed-target helpers and run host models. Never connects to Android."""
from __future__ import annotations
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
NATIVE = ROOT / 'native'
DAEMON_SHA = 'd9343627fe3ee8587f883aa9b2ec56fde1025567326cd100ef5219436bc5fce5'
DAEMON_BYTES = 6001344

def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def require(value, message):
    if not value:
        raise ValueError(message)

def load_config(path):
    config = json.loads(Path(path).read_text(encoding='utf-8'))
    require(set(config) == {'schemaVersion', 'expectedManagerUid', 'daemonFile'}, 'Invalid config keys')
    require(config['schemaVersion'] == 1, 'Unsupported config schema')
    uid = config['expectedManagerUid']
    require(type(uid) is int and 10000 <= uid <= 99999, 'An explicitly measured manager UID is required')
    daemon = Path(config['daemonFile'])
    if not daemon.is_absolute():
        daemon = Path(path).resolve().parent / daemon
    require(daemon.is_file() and not daemon.is_symlink(), 'Daemon must be a regular local file')
    require(daemon.stat().st_size == DAEMON_BYTES and digest(daemon) == DAEMON_SHA, 'Unsupported daemon content')
    with daemon.open('rb') as f:
        header = f.read(64)
    require(header[:6] == b'\x7fELF\x02\x01' and struct.unpack_from('<HH', header, 16) == (3, 62), 'Expected x64 ELF shared executable')
    return uid, daemon.resolve()

def safe_output(path):
    raw = Path(path).absolute()
    for node in (raw, *raw.parents):
        if node.exists():
            require(not node.is_symlink() and not getattr(node.stat(follow_symlinks=False), 'st_file_attributes', 0) & 0x400, 'Reparse output path refused')
    output = raw.resolve()
    require(output.is_relative_to(ROOT / 'build') and output != ROOT / 'build', 'Output must be a new directory below build/')
    require(not output.exists(), 'Preserve previous output; choose a new directory')
    return output

class Runner:
    def __init__(self, args):
        self.prefix = []
        if os.name == 'nt':
            self.prefix = ['wsl.exe']
            if args.wsl_distro:
                self.prefix += ['-d', args.wsl_distro]
            if args.wsl_user:
                self.prefix += ['-u', args.wsl_user]
            self.prefix += ['--']
        elif args.wsl_distro or args.wsl_user:
            raise ValueError('WSL options require Windows')
        self.events = []

    def path(self, path):
        if os.name != 'nt':
            return str(Path(path).resolve())
        result = subprocess.run([*self.prefix, 'wslpath', '-a', '-u', Path(path).resolve().as_posix()], capture_output=True, text=True, timeout=10, check=True)
        return result.stdout.strip()

    def run(self, args, *, expected=0, timeout=90):
        p = subprocess.run([*self.prefix, *map(str, args)], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=timeout)
        self.events.append({'program': Path(str(args[0])).name, 'exitCode': p.returncode, 'stdout': p.stdout, 'stderr': p.stderr})
        if p.returncode != expected:
            raise RuntimeError(f'{Path(str(args[0])).name} exited {p.returncode}, expected {expected}: {p.stderr[-2000:]}')
        return p.stdout

def verify_catalog():
    catalog = json.loads((NATIVE / 'prestage/supported-daemons.json').read_text())
    require(catalog['schemaVersion'] == 1 and catalog['supportedUapi'] == 6, 'Unsupported catalog')
    entries = catalog['releases']
    require(len(entries) == 1 and entries[0]['daemonSha256'] == DAEMON_SHA and entries[0]['daemonBytes'] == DAEMON_BYTES, 'New official releases require reviewed source/catalog/probe changes')
    r = entries[0]
    require(r['versionCode'] == 35253 and r['uapiVersion'] == 6 and r['signerSha256'] == catalog['fixedOfficialSigner'], 'Catalog identity changed')
    require(r['elfClass'] == 64 and r['elfMachine'] == 62 and r['elfType'] == 3, 'Catalog architecture changed')
    h = DAEMON_SHA
    body = '/* SPDX-License-Identifier: GPL-3.0-or-later */\nstruct supported_daemon {size_t size;unsigned uapi;unsigned char hash[32];const char *hex;};\nstatic const struct supported_daemon catalog[]={\n'
    body += '{'+str(DAEMON_BYTES)+',6,{'+','.join('0x'+h[i:i+2] for i in range(0,64,2))+'},"'+h+'"},\n};\n#define CATALOG_COUNT 1U\n'
    require((NATIVE / 'prestage/catalog.h').read_text() == body, 'Frozen compiled catalog differs from JSON')
    return catalog

def build(args):
    uid, daemon = load_config(args.config)
    catalog = verify_catalog()
    out = safe_output(args.output)
    runner = Runner(args)
    require(runner.run(['id','-u']).strip() != '0', 'Host tests must run as an ordinary user')
    runner.run([args.model_python, '-c', 'import unicorn; import elftools'])
    if args.check_inputs:
        return {'inputsVerified': True, 'deviceValidated': False, 'catalogEntries': len(catalog['releases'])}
    out.mkdir(parents=True)
    report = {'schemaVersion': 1, 'startedAt': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'status': 'building', 'deviceValidated': False, 'catalogEntries': 1, 'binaries': {}, 'hostTests': {}}
    def save():
        (out / 'manifest.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        (out / 'build-log.json').write_text(json.dumps(runner.events, indent=2)+'\n', encoding='utf-8')
    try:
        include = out / 'generated'; include.mkdir()
        (include / 'target.h').write_text(f'#define TARGET_MANAGER_UID {uid}U\n', encoding='utf-8')
        common = ['-I'+runner.path(NATIVE / 'common'), '-I'+runner.path(include)]
        static = ['-static','-Os','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-fstack-protector-strong','-fPIE','-Wl,-z,noexecstack','-s']
        models = ['-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        specs = [('prestage','prepare.c','resukisu-migrate-prestage',True),
                 ('probes/grant','probe.c','resukisu-grant-probe',False),
                 ('probes/filtered-grant','probe.c','resukisu-filtered-grant-probe',False),
                 ('probes/normal-check','check.c','resukisu-normal-check',True),
                 ('probes/migration-fixture','fixture.c','resukisu-migration-fixture',True),
                 ('probes/migration-rearm','recovery.c','resukisu-migration-rearm',True)]
        for folder, source, name, takes_daemon in specs:
            src = NATIVE / folder
            binary, unit = out / name, out / (name+'-unit')
            runner.run(['musl-gcc',*static,*common,runner.path(src / source),'-o',runner.path(binary)])
            runner.run(['gcc',*models,*common,runner.path(src / 'unit.c'),'-o',runner.path(unit)])
            unit_text = runner.run([runner.path(unit), *([runner.path(daemon)] if takes_daemon else [])])
            report['hostTests'][name] = {'passed': True, 'output': unit_text.strip()}
            report['binaries'][name] = {'file': name, 'sha256': digest(binary), 'bytes': binary.stat().st_size, 'sourceSha256': digest(src / source), 'elfClass':64}
            # Invoking production tools on an ordinary host must stop at identity gates.
            modes = {'resukisu-migration-fixture':['arm'], 'resukisu-migration-rearm':['rearm']}.get(name, [])
            gate = runner.run([runner.path(binary), *modes], expected=64)
            if name == 'resukisu-normal-check':
                require(json.loads(gate)['expectedManagerUid'] == uid, 'Manager UID report did not use configuration')
            save()
        flags32=['-m32','-std=c11','-Os','-ffreestanding','-fno-builtin','-fno-pie','-fno-stack-protector','-fno-asynchronous-unwind-tables','-nostdlib','-static','-no-pie','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-Wl,--build-id=none,-z,noexecstack']
        binary=out/'resukisu-su32'; unit=out/'resukisu-su32-unit'
        for source,target in [('probe.c',binary),('unit32.c',unit)]:
            runner.run(['gcc',*flags32,runner.path(NATIVE/'probes/su32'/source),'-o',runner.path(target)])
        unit_text=runner.run([runner.path(unit)])
        unit_lines=[json.loads(line) for line in unit_text.splitlines()]
        require(unit_lines[-1]['hostTestsPassed']>=40 and unit_lines[-1]['kernelSuTested'] is False,'IA32 model failed')
        report['hostTests']['resukisu-su32']={'passed':True,'results':unit_lines}
        for mode in ('execve','execveat'):
            gate=json.loads(runner.run([runner.path(binary),mode],expected=64))
            require(gate['executed'] is False,'IA32 host gate executed')
        report['binaries']['resukisu-su32']={'file':binary.name,'sha256':digest(binary),'bytes':binary.stat().st_size,'elfClass':32}
        binary=out/'resukisu-direct32'; obj=out/'direct32.o'
        runner.run(['as','--32',runner.path(NATIVE/'probes/direct32/direct32.S'),'-o',runner.path(obj)])
        runner.run(['ld','-m','elf_i386','-static','--build-id=none','-z','noexecstack',runner.path(obj),'-o',runner.path(binary)])
        model_output=out/'direct32-model.json'
        runner.run([args.model_python,runner.path(NATIVE/'probes/direct32/model-test.py'),'--binary',runner.path(binary),'--output',runner.path(model_output)])
        model=json.loads(model_output.read_text())
        require(model['testsPassed']>=11,'Direct IA32 model incomplete')
        report['hostTests']['resukisu-direct32']={'passed':True,'testsPassed':model['testsPassed']}
        report['binaries']['resukisu-direct32']={'file':binary.name,'sha256':digest(binary),'bytes':binary.stat().st_size,'elfClass':32}
        for name, artifact in report['binaries'].items():
            head=(out/name).read_bytes()[:64]
            require(head[:4]==b'\x7fELF' and head[4]==(1 if artifact['elfClass']==32 else 2),'Wrong compiled ELF class')
            elf=runner.run(['readelf','-l',runner.path(out/name)])
            require('INTERP' not in elf,'Unexpected dynamic interpreter')
        report.update(status='host_tests_passed_not_device_validated', finishedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                      expectedManagerUid=uid, daemonSha256=DAEMON_SHA, catalogSha256=digest(NATIVE/'prestage/supported-daemons.json'))
    except BaseException as e:
        report.update(status='failed', failureType=type(e).__name__, failureReason=str(e))
        raise
    finally:
        save()
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'build/userspace')
    p.add_argument('--wsl-distro');p.add_argument('--wsl-user')
    p.add_argument('--model-python',default='python3',help='Linux Python with unicorn and pyelftools')
    p.add_argument('--check-inputs',action='store_true')
    args=p.parse_args()
    result=build(args)
    print(json.dumps({k:v for k,v in result.items() if k not in ('hostTests','binaries')},indent=2))

if __name__=='__main__':
    main()
