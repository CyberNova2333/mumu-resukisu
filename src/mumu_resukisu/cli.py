"""Explicit offline and instance-bound commands; importing has no side effects."""
import argparse
import json
import re
from pathlib import Path
import subprocess
from . import __version__
from .images import Vdi, assemble, audit, hashfile, partitions, safe_path, write_json

def read(path):
    return json.loads(safe_path(path).read_text(encoding='utf-8'))

def load_profile(path):
    p=read(path)
    if p.get('schemaVersion')!=1 or p.get('androidVersion')!='15.0':raise ValueError('Unsupported target profile')
    if not isinstance(p.get('id'),str) or not re.fullmatch('[a-z][a-z0-9_-]{0,63}',p['id']):raise ValueError('Invalid profile ID')
    if type(p.get('virtualBytes')) is not int or not 0<p['virtualBytes']<=1<<40:raise ValueError('Invalid virtual capacity')
    for value in [p.get('baseSha256'),*(x.get('sourceSha256') for x in p['partitions'])]:
        if not isinstance(value,str) or not re.fullmatch('[0-9a-f]{64}',value):raise ValueError('Invalid profile digest')
    partitions(p)
    return p

def candidate(path):
    from .controller import Candidate
    m=read(path);root=Path(path).absolute().parent
    def local(key):
        s=m[key]
        if Path(s).name!=s:raise ValueError('Manifest artifact must be an adjacent basename')
        return safe_path(root/s)
    return m,Candidate(local('diskFile'),m['diskSha256'],local('auditFile'),m['auditSha256'])

def build_controller(a):
    from .binding import load_binding
    from .controller import Controller,ProfileRequirements
    b=load_binding(a.binding.absolute());p=load_profile(a.profile)
    c=None
    if a.candidate:
        m,c=candidate(a.candidate)
        if m['profileId']!=p['id'] or m['sourceDiskSha256']!=b.baseline_sha256 or m['baseSha256']!=p['baseSha256']:
            raise ValueError('Candidate does not match binding/profile')
    r=ProfileRequirements(base_sha=p['baseSha256'],source_sha=b.baseline_sha256,
        allowed_partitions=tuple((x['offset'],x['bytes']) for x in p['partitions']),
        virtual_bytes=p['virtualBytes'],profile_id=p['id'],android_version=p['androidVersion'])
    def verify(binding,requirements,cand):
        result=audit(binding.base_disk_path,binding.private_disk_path,cand.file,p)
        if result['sourceDiskSha256']!=binding.baseline_sha256 or result['candidateSha256']!=cand.sha:
            raise ValueError('Live baseline/candidate changed')
        return result
    import hashlib
    identity=json.dumps({'disk':str(b.private_disk_path).casefold(),'identity':b.identity},sort_keys=True).encode()
    state=Path(a.binding).absolute().parent/'.state'/hashlib.sha256(identity).hexdigest()
    return Controller(b,r,c,state,verify_candidate=verify)

def main(argv=None):
    ap=argparse.ArgumentParser(description='MuMu Android15 target-bound ReSukiSU tools')
    ap.add_argument('--version',action='version',version=__version__)
    subs=ap.add_subparsers(dest='command',required=True)
    p=subs.add_parser('profile',help='Inspect supported fingerprints without touching a device')
    p.add_argument('--profile',type=Path,required=True)
    p=subs.add_parser('extract',help='Extract partitions from read-only caller-supplied snapshots')
    for key in ('profile','base','source','output'):p.add_argument('--'+key,type=Path,required=True)
    p=subs.add_parser('assemble',help='Build a local candidate; never install')
    for key in ('profile','base','source','output'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--partition',action='append',required=True,metavar='NAME=FILE')
    p=subs.add_parser('audit',help='Recompare every virtual byte against local snapshots')
    for key in ('profile','base','source','candidate'):p.add_argument('--'+key,type=Path,required=True)
    p=subs.add_parser('bind',help='Read and bind one explicitly selected stopped instance')
    for key in ('profile','manager','instance-directory','private-disk','base','output'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--index',required=True)
    for name in ('inspect','status','install','restore'):
        p=subs.add_parser(name)
        for key in ('binding','profile'):p.add_argument('--'+key,type=Path,required=True)
        p.add_argument('--candidate',type=Path,required=name in ('install','inspect'))
        if name=='restore':p.add_argument('--transaction',required=True)
    a=ap.parse_args(argv)
    if a.command=='profile':result=load_profile(a.profile)
    elif a.command=='extract':
        p=load_profile(a.profile)
        if hashfile(a.base)!=p['baseSha256']:raise ValueError('Wrong base fingerprint')
        out=safe_path(a.output);out.mkdir(parents=True,exist_ok=False)
        records=[]
        before=hashfile(a.source)
        with Vdi(a.base) as base,Vdi(a.source,base) as src:
            if src.disk_bytes!=p['virtualBytes']:raise ValueError('Wrong virtual capacity')
            for part in p['partitions']:
                if src.range_hash(part['offset'],part['bytes'])!=part['sourceSha256']:raise ValueError('Unknown source partition')
                path=out/(part['name']+'.img')
                with path.open('xb') as f:
                    for at in range(0,part['bytes'],8<<20):f.write(src.read(part['offset']+at,min(8<<20,part['bytes']-at)))
                records.append({'file':path.name,'sha256':hashfile(path)})
        if hashfile(a.source)!=before or hashfile(a.base)!=p['baseSha256']:raise ValueError('Input changed')
        result={'profileId':p['id'],'sourceSha256':before,'partitions':records}
        write_json(out/'extraction.json',result)
    elif a.command=='assemble':
        replacements={}
        for item in a.partition:
            name,sep,path=item.partition('=')
            if not sep or name in replacements:raise ValueError('Invalid or duplicate partition')
            replacements[name]=Path(path)
        result=assemble(a.base,a.source,replacements,load_profile(a.profile),a.output)
    elif a.command=='audit':result=audit(a.base,a.source,a.candidate,load_profile(a.profile))
    elif a.command=='bind':
        from .binding import create_binding,write_binding
        p=load_profile(a.profile)
        if hashfile(a.base)!=p['baseSha256']:raise ValueError('Wrong base fingerprint')
        q=subprocess.run([str(safe_path(a.manager)),'info','-v',a.index],capture_output=True,check=True,timeout=10)
        info=json.loads(q.stdout)
        identity={k:info[k] for k in ('index','name','created_timestamp','android_version')}
        if str(identity['index'])!=a.index or identity['android_version']!=p['androidVersion']:raise ValueError('Wrong instance')
        b=create_binding(profile_id=p['id'],manager_path=a.manager.absolute(),instance_directory=a.instance_directory.absolute(),
            private_disk_path=a.private_disk.absolute(),base_disk_path=a.base.absolute(),identity=identity)
        write_binding(a.output.absolute(),b)
        result={'status':'bound','bindingFile':str(a.output),'deviceWritten':False}
    else:
        c=build_controller(a)
        result=c.restore(a.transaction) if a.command=='restore' else getattr(c,a.command)()
    print(json.dumps(result,indent=2,ensure_ascii=False))

if __name__=='__main__':main()
