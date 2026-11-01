"""Remove only the fixed vendor daemon overwrite line from a local snapshot."""
import argparse
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'src'))
from mumu_resukisu.images import safe_path, write_json
from mumu_resukisu.bootfs import sha

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source',required=True,type=Path);ap.add_argument('--output',required=True,type=Path)
    a=ap.parse_args();data=safe_path(a.source).read_bytes()
    if sha(data)!='68b76874bf917edd33e7e8c7b8e1550f1df5da41ee8c7f830b120da43a04cb69':raise ValueError('Unknown vendor image')
    offset=4002638
    old=b'    copy /system/xbin/dusk /data/adb/ksud\n'
    new=b'    # preserve the managed daemon        \n'
    if len(old)!=len(new) or data[offset:offset+len(old)]!=old:raise ValueError('Vendor line mismatch')
    changed=data[:offset]+new+data[offset+len(old):]
    if sha(changed)!='26e7fc49693f8b93a61c27ffccc9a5100096701821d60960803a1d901402ebab':raise ValueError('Candidate mismatch')
    out=safe_path(a.output);out.mkdir(parents=True,exist_ok=False)
    (out/'vendor.img').write_bytes(changed)
    write_json(out/'vendor-build.json',{'schemaVersion':1,'inputSha256':sha(data),'outputSha256':sha(changed),
        'changedRange':{'offset':offset,'bytes':len(old)},'outsideRangeUnchanged':True,
        'avbTailPreserved':True,'avbVerified':False,'deviceValidated':False})

if __name__=='__main__':main()
