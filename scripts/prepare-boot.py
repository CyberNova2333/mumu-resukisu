"""Build a boot filesystem from caller-supplied pinned inputs; run on Linux/WSL."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'src'))
from mumu_resukisu.bootfs import BootFs, initramfs, sha, SOURCE_KERNEL, PATCHED_KERNEL
from mumu_resukisu.images import safe_path, write_json

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('source','kernel','loader','module','prestage','output'):ap.add_argument('--'+name,required=True,type=Path)
    for name in ('native-manifest','userspace-manifest'):ap.add_argument('--'+name,required=True,type=Path)
    a=ap.parse_args()
    inputs={k:safe_path(getattr(a,k)).read_bytes() for k in ('source','kernel','loader','module','prestage')}
    native=json.loads(safe_path(a.native_manifest).read_text())
    userspace=json.loads(safe_path(a.userspace_manifest).read_text())
    if not all(native.get(k) is True for k in ('staticModuleChecksPassed','loaderTestsPassed','hostGatePassed')):
        raise ValueError('Native build verification incomplete')
    if userspace.get('status')!='host_tests_passed_not_device_validated':raise ValueError('Userspace build verification incomplete')
    for k in ('loader','module'):
        if sha(inputs[k])!=native[k]['sha256']:raise ValueError('Mismatched native build artifact: '+k)
    if sha(inputs['prestage'])!=userspace['binaries']['resukisu-migrate-prestage']['sha256']:
        raise ValueError('Prestage differs from userspace build manifest')
    fs=BootFs(inputs['source'])
    if sha(fs.file(fs.names['kernel'])[0])!=SOURCE_KERNEL or sha(inputs['kernel'])!=PATCHED_KERNEL:
        raise ValueError('Kernel fingerprint mismatch; this is not a generic boot patcher')
    # These are local build inputs; bundle hashes record their exact identity.
    for k in ('loader','module','prestage'):
        if not inputs[k].startswith(b'\x7fELF'):raise ValueError('Expected ELF: '+k)
    out=safe_path(a.output);out.mkdir(parents=True,exist_ok=False)
    original=fs.file(fs.names['initrd'])[0]
    raw=initramfs(original,inputs['loader'],inputs['module'],inputs['prestage'])
    (out/'initrd.cpio').write_bytes(raw)
    subprocess.run(['zstd','-19','--no-progress','-T2',str(out/'initrd.cpio'),'-o',str(out/'initrd.zst')],check=True,timeout=120)
    compressed=(out/'initrd.zst').read_bytes()
    if len(compressed)>len(original):raise ValueError('Compressed initramfs exceeds fixed data slot')
    fs.replace('kernel',inputs['kernel']);fs.replace('initrd',compressed+bytes(len(original)-len(compressed)))
    # Unrelated files and their metadata must survive the data-slot edits.
    baseline=BootFs(inputs['source'])
    allowed=set()
    for name in ('kernel','initrd'):allowed.update(baseline.file(baseline.names[name])[1])
    if any(x!=y and i//1024 not in allowed for i,(x,y) in enumerate(zip(inputs['source'],fs.data))):
        raise ValueError('Changed boot filesystem metadata')
    (out/'boot.img').write_bytes(fs.data)
    write_json(out/'boot-build.json',{'schemaVersion':1,'sourceSha256':sha(inputs['source']),
        'outputSha256':sha(fs.data),'inputHashes':{k:sha(v) for k,v in inputs.items()},
        'filesystemMetadataUnchanged':True,'preparationFailureStopsAndroid':True,'deviceValidated':False})

if __name__=='__main__':main()
