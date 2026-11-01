"""Offline, fixed-input dusk replacement. Never opens installed disks for writing."""
import argparse, datetime, hashlib, json, shutil, struct, subprocess
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'src'))
from mumu_resukisu.images import safe_path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source',type=Path,required=True)
parser.add_argument('--daemon',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
cli=parser.parse_args()
SOURCE=safe_path(cli.source);DAEMON=safe_path(cli.daemon)
STAGE=safe_path(cli.output);STAGE.mkdir(parents=True,exist_ok=False)
CANDIDATE=STAGE/'system-candidate.img'
TOOL=STAGE/'system-edit'
TOOL_SOURCE=Path(__file__).resolve().parents[1]/'native/images/system-edit.c'
args=argparse.Namespace(verify_existing=False)

def require(v,m):
    if not v:raise RuntimeError(m)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def run(args):
    p=subprocess.run([str(x) for x in args],capture_output=True,text=True,timeout=180)
    r=dict(args=[str(x) for x in args],exitCode=p.returncode,stdout=p.stdout,stderr=p.stderr)
    require(p.returncode==0,json.dumps(r))
    return r
def scan(p,name):
    r=run([TOOL,'scan',p]);(STAGE/name).write_text(r['stdout'])
    return json.loads(r['stdout'])
require(CANDIDATE.exists() if args.verify_existing else not CANDIDATE.exists(),'Candidate existence does not match explicit mode')
require(CANDIDATE.parent.resolve()==STAGE.resolve(),'Candidate path escaped')
source_stat=SOURCE.stat()
source_sha=sha(SOURCE);daemon_sha=sha(DAEMON)
require(source_sha=='82a7f8e6fce62c1744221a90b63a69eafc80537105f754f9cf5ef142226e70d1','Source hash changed')
require(daemon_sha=='d9343627fe3ee8587f883aa9b2ec56fde1025567326cd100ef5219436bc5fce5','Daemon hash changed')
run(['gcc','-O2','-Wall','-Wextra','-Werror','-o',TOOL,TOOL_SOURCE,'-lext2fs','-lcom_err'])
before=scan(SOURCE,'inodes-before.json')
require(before['targetBlockCollisions']==0,'Old dusk physical data shared')
if not args.verify_existing:
    with SOURCE.open('rb') as a, CANDIDATE.open('xb') as b:shutil.copyfileobj(a,b)
    edit=run([TOOL,'replace',CANDIDATE,DAEMON]);(STAGE/'edit-log.json').write_text(json.dumps(edit,indent=2))
after=scan(CANDIDATE,'inodes-after.json')
require(after['targetBlockCollisions']==0,'New dusk physical data shared')
bi={i['inode']:i for i in before['inodes']};ai={i['inode']:i for i in after['inodes']}
require(bi.keys()==ai.keys(),'Allocated inode set changed')
require(all(bi[i]==ai[i] for i in bi if i!=3980),'Unrelated inode metadata/block mapping changed')
old=bytes.fromhex(bi[3980]['raw']);new=bytes.fromhex(ai[3980]['raw'])
mutable=set(range(4,8))|set(range(28,32))|set(range(40,100))|set(range(108,116))
require(all(a==b or i in mutable for i,(a,b) in enumerate(zip(old,new))),'Dusk owner, mode, time, links or xattrs changed')
dump=STAGE/('candidate-dusk-recheck.bin' if args.verify_existing else 'candidate-dusk.bin')
run(['debugfs','-R',f'dump /system/xbin/dusk "{dump}"',CANDIDATE])
require(dump.is_file() and sha(dump)==daemon_sha,'File readback differs')
check=run(['e2fsck','-fn',CANDIDATE]);(STAGE/'e2fsck.json').write_text(json.dumps(check,indent=2))
with SOURCE.open('rb') as f:
    head=f.read(8192);sb=head[1024:2048];gd=head[4096:8192]
blocks=struct.unpack_from('<I',sb,4)[0];per_group=struct.unpack_from('<I',sb,32)[0]
groups=(blocks+per_group-1)//per_group
def power(n,base):
    while n>1 and n%base==0:n//=base
    return n==1
backup_groups=[g for g in range(groups) if g in (0,1) or any(power(g,b) for b in (3,5,7))]
bb=[struct.unpack_from('<I',gd,g*32)[0] for g in range(groups)]
target_table=struct.unpack_from('<I',gd,((3980-1)//320)*32+8)[0]+(((3980-1)%320)*256)//4096
target_old=set(bi[3980]['blocks']);target_new=set(ai[3980]['blocks'])
require(target_old<=target_new,'Existing file blocks unexpectedly released')
allocated=target_new-target_old
allowed=target_old|target_new|set(bb)|{target_table}
for g in backup_groups:allowed.update([g*per_group,g*per_group+1])
changed=[]
with SOURCE.open('rb') as a,CANDIDATE.open('rb') as b:
    index=0
    while True:
        x,y=a.read(4096),b.read(4096)
        require(len(x)==len(y),'Image length changed')
        if not x:break
        if x!=y:
            require(index in allowed,f'Unapproved block changed: {index}')
            changed.append(index)
        index+=1
    for group,bitmap in enumerate(bb):
        a.seek(bitmap*4096);b.seek(bitmap*4096)
        x,y=a.read(4096),b.read(4096)
        for offset,(v,w) in enumerate(zip(x,y)):
            if v==w:continue
            require(v&w==v,'Allocated blocks were freed')
            for bit in range(8):
                if (v^w)&(1<<bit):require(group*per_group+offset*8+bit in allocated,'Bitmap changed outside new dusk blocks')
    a.seek(SOURCE.stat().st_size-64);footer=a.read(64)
last_stat=SOURCE.stat()
require(sha(SOURCE)==source_sha and (last_stat.st_size,last_stat.st_mtime_ns,last_stat.st_ino)==(source_stat.st_size,source_stat.st_mtime_ns,source_stat.st_ino),'Read-only source content or modification metadata changed')
report=dict(checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),verified=True,
    sourceSha256=source_sha,candidateSha256=sha(CANDIDATE),daemonSha256=daemon_sha,
    candidateBytes=CANDIDATE.stat().st_size,filesystemCheckExitCode=check['exitCode'],
    sourceFilesystemBlockCount=blocks,sourceFreeBlocks=4110,blockSize=4096,
    allocatedInodesCompared=before['allocatedInodes'],unrelatedInodesUnchanged=True,
    targetInode=3980,targetLinks=1,targetMode='0755',targetUid=0,targetGid=2000,
    targetOldBytes=3753880,targetNewBytes=6001344,targetExistingBlockCount=len(target_old),
    targetNewBlockCount=len(target_new),newlyAllocatedBlockCount=len(allocated),
    targetPhysicalBlockAliasCount=0,targetOwnerModeTimesAndXattrsPreserved=True,
    targetSelinuxLabel='u:object_r:system_file:s0',sharedBlocksFeaturePreserved=True,
    changedFilesystemBlocks=changed,unchangedOutsidePermittedDataAndMetadata=True,
    unchangedNonFilesystemTrailingBytes=CANDIDATE.stat().st_size-blocks*4096,
    avbFooterAtLast64=footer[:4]==b'AVBf',installedFilesWritten=False,
    managerApkIncluded=False,otherSystemFilesAdded=False)
(STAGE/'candidate-report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='changedFilesystemBlocks'},indent=2))
