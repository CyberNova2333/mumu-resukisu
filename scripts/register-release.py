#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify an official release offline and propose a catalog entry; no APK execution."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import re
import struct
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[1]
SIGNER='d3469712b6214462764a1d8d3e5cbe1d6819a0b629791b9f4101867821f1df64'
PACKAGE='com.resukisu.resukisu'
def sha(data):return hashlib.sha256(data).hexdigest()
def read_elf(data):
    return 64<=len(data)<=64*1024*1024 and data[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<HHI',data,16)==(3,62,1) and struct.unpack_from('<H',data,52)[0]==64
def verify_signature_output(text):
    certs=re.findall(r'^Signer #\d+ certificate SHA-256 digest: ([0-9a-fA-F]{64})$',text,re.M)
    return 'Verifies' in text.splitlines() and 'Verified using v2 scheme (APK Signature Scheme v2): true' in text and certs==[SIGNER] and 'Number of signers: 1' in text
def source_uapi(text):
    found=re.findall(r'\bstatic\s+const\s+__u32\s+KERNEL_SU_UAPI_VERSION\s*=\s*(\d+)\s*;',text)
    if found!=['6']:raise ValueError('Source must define exactly UAPI6')
    return 6
def run(args):
    p=subprocess.run(list(map(str,args)),capture_output=True,text=True,encoding='utf-8',timeout=60)
    if p.returncode:raise RuntimeError(f'{Path(str(args[0])).name} failed with exit {p.returncode}')
    return p.stdout
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--apk',type=Path,required=True);p.add_argument('--expected-apk-sha256',required=True)
    p.add_argument('--source-root',type=Path,required=True);p.add_argument('--source-revision',required=True)
    p.add_argument('--reviewed-release-association',action='store_true',required=True)
    p.add_argument('--apksigner-jar',type=Path,required=True);p.add_argument('--aapt',type=Path,required=True)
    p.add_argument('--java',default='java');p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if not re.fullmatch('[0-9a-f]{40}',a.source_revision) or not re.fullmatch('[0-9a-f]{64}',a.expected_apk_sha256):raise ValueError('Explicit reviewed revision and hash required')
    spec=importlib.util.spec_from_file_location('build_support',ROOT/'scripts/build-userspace.py')
    support=importlib.util.module_from_spec(spec);spec.loader.exec_module(support)
    output=support.safe_output(a.output)
    apk=a.apk.resolve();source=a.source_root.resolve()
    if not apk.is_file() or apk.stat().st_size>512*1024*1024:raise ValueError('APK size/type invalid')
    before=support.digest(apk)
    if before!=a.expected_apk_sha256:raise ValueError('APK hash mismatch')
    if run(['git','-C',source,'rev-parse','HEAD']).strip()!=a.source_revision:raise ValueError('Source revision mismatch')
    run(['git','-C',source,'diff','--exit-code','HEAD','--','uapi/supercall.h'])
    header=source/'uapi/supercall.h';content=header.read_bytes();uapi=source_uapi(content.decode())
    signed=run([a.java,'--enable-native-access=ALL-UNNAMED','-jar',a.apksigner_jar,'verify','--verbose','--print-certs',apk])
    if not verify_signature_output(signed):raise ValueError('Full signature verification or official signer mismatch')
    badging=run([a.aapt,'dump','badging',apk])
    m=re.search(r"^package: name='([^']+)' versionCode='([^']+)' versionName='([^']+)'",badging,re.M)
    if not m or m[1]!=PACKAGE or not m[2].isdigit() or not re.fullmatch(r'[A-Za-z0-9._+\-]+',m[3]):raise ValueError('Manager package metadata mismatch')
    with zipfile.ZipFile(apk) as z:
        entries=[v for v in z.infolist() if v.filename=='lib/x86_64/libksud.so']
        if len(entries)!=1 or not 64<=entries[0].file_size<=64*1024*1024:raise ValueError('Unique bounded x64 daemon required')
        daemon=z.read(entries[0])
    if not read_elf(daemon):raise ValueError('Unsupported daemon ELF')
    if support.digest(apk)!=before or header.read_bytes()!=content:raise ValueError('Inputs changed during verification')
    entry={'releaseName':m[3],'versionCode':int(m[2]),'package':PACKAGE,'apkSha256':before,'signerSha256':SIGNER,
        'daemonSha256':sha(daemon),'daemonBytes':len(daemon),'elfClass':64,'elfMachine':62,'elfType':3,'uapiVersion':uapi,
        'sourceRevision':a.source_revision,'sourceUapiHeaderSha256':sha(content),'sourceApkAssociationMaintainerReviewed':True,
        'sourceBinaryReproducibilityProven':False,'seedBaseline':False,'deviceUpgradeValidated':False}
    proof={'schemaVersion':1,'apkSignatureFullyVerified':True,'entry':entry,'signatureToolSha256':support.digest(a.apksigner_jar),
        'packageToolSha256':support.digest(a.aapt),'verificationOutput':signed,'catalogUpdated':False,
        'boundary':'Verified source and APK identity. Release association is an explicit maintainer assertion, not reproducible-build or device upgrade proof.'}
    output.mkdir(parents=True)
    (output/'entry.json').write_text(json.dumps(entry,indent=2)+'\n',encoding='utf-8')
    (output/'proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'verified':True,'catalogUpdated':False,'requiresCodeReviewAndRebuild':True,'daemonSha256':sha(daemon)}))
if __name__=='__main__':main()
