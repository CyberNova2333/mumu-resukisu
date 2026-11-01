/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
static struct {dev_t dev;ino_t ino;} fixtures[32];static int fixture_count;
static int reject_daemon_remove,fail_marker_fsync,marker_unlinked,daemon_unlinked,order_valid=1;
static void model_owner(struct stat *s){for(int i=0;i<fixture_count;i++)if(s->st_dev==fixtures[i].dev&&s->st_ino==fixtures[i].ino){s->st_uid=0;s->st_gid=0;}}
static int fixture_fstat(int fd,struct stat *s){int rc=fstat(fd,s);if(!rc)model_owner(s);return rc;}
static int fixture_fstatat(int fd,const char *n,struct stat *s,int flags){int rc=fstatat(fd,n,s,flags);if(!rc)model_owner(s);return rc;}
static int fixture_unlinkat(int fd,const char *name,int flags){
    if(!strcmp(name,"ksud")&&reject_daemon_remove){errno=EIO;return -1;}
    int rc=unlinkat(fd,name,flags);
    if(!rc&&!strcmp(name,"service-ran")){if(daemon_unlinked)order_valid=0;marker_unlinked=1;}
    if(!rc&&!strcmp(name,"ksud"))daemon_unlinked=1;
    return rc;
}
static int fixture_fsync(int fd){if(fail_marker_fsync&&marker_unlinked){errno=EIO;return -1;}return fsync(fd);}
#define fstat fixture_fstat
#define fstatat fixture_fstatat
#define unlinkat fixture_unlinkat
#define fsync fixture_fsync
#define main recovery_main
#include "recovery.c"
#undef main
#undef fstat
#undef fstatat
#undef unlinkat
#undef fsync
static void register_fixture(int dir,const char *name){assert(fixture_count<32);struct stat s;assert(!fstatat(dir,name,&s,AT_SYMLINK_NOFOLLOW));fixtures[fixture_count].dev=s.st_dev;fixtures[fixture_count++].ino=s.st_ino;}
static void save_file(int dir,const char *name,const void *body,size_t n,mode_t mode){int fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL,mode);assert(fd>=0);assert(!write_all(fd,body,n)&&!fchmod(fd,mode)&&!close(fd));register_fixture(dir,name);}
static const char prior[]="12345678-1234-1234-1234-123456789abc";
static const char current[]="87654321-1234-1234-1234-123456789abc";
static void run_case(int test,const unsigned char *daemon) {
    fixture_count=0;reject_daemon_remove=0;fail_marker_fsync=0;marker_unlinked=0;daemon_unlinked=0;order_valid=1;
    char tmp[]="/tmp/resukisu-migration-rearm-unit-XXXXXX";assert(mkdtemp(tmp));
    struct layout l={0};l.data=open(tmp,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(l.data>=0);
    assert(!mkdirat(l.data,"adb",0700));register_fixture(l.data,"adb");l.adb=directory(l.data,"adb",0,0700,1);assert(l.adb>=0);
    assert(!mkdirat(l.adb,"service.d",0755));register_fixture(l.adb,"service.d");l.service=directory(l.adb,"service.d",0,0755,0);assert(l.service>=0);
    assert(!mkdirat(l.adb,TASK,0700));register_fixture(l.adb,TASK);l.task=directory(l.adb,TASK,0,0700,1);assert(l.task>=0);
    assert(!fixture_fstat(l.adb,&l.adb_id)&&!fixture_fstat(l.service,&l.service_id)&&!fixture_fstat(l.task,&l.task_id));
    save_file(l.adb,"ksud",daemon,DAEMON_BYTES,0755);save_file(l.task,BACKUP,daemon,DAEMON_BYTES,0755);
    save_file(l.service,SCRIPT,script_bytes,SCRIPT_BYTES,0755);save_file(l.service,"user-script.sh","keep\n",5,0755);
    struct stat backup_id,script_id,user_id;assert(!file_verified(l.task,BACKUP,DAEMON_BYTES,daemon_hash,0755,&backup_id));
    assert(!file_verified(l.service,SCRIPT,SCRIPT_BYTES,script_hash,0755,&script_id));assert(!fixture_fstatat(l.service,"user-script.sh",&user_id,AT_SYMLINK_NOFOLLOW));
    char marker[38];snprintf(marker,sizeof(marker),"%s\n",prior);
    if(test!=1)save_file(l.task,MARKER,marker,37,0600);
    if(test==2)save_file(l.task,"foreign","keep",4,0600);
    if(test==3){int fd=openat(l.task,MARKER,O_WRONLY);assert(fd>=0&&pwrite(fd,"X",1,0)==1&&!close(fd));}
    if(test==4){assert(!unlinkat(l.task,MARKER,0)&&!symlinkat(BACKUP,l.task,MARKER));}
    if(test==5){int fd=openat(l.adb,"ksud",O_WRONLY);assert(fd>=0&&pwrite(fd,"X",1,0)==1&&!close(fd));}
    if(test==6)assert(!fchmodat(l.adb,"ksud",0775,0));
    if(test==7)assert(!linkat(l.task,BACKUP,l.task,"backup-hardlink",0));
    if(test==8){int fd=openat(l.service,SCRIPT,O_WRONLY);assert(fd>=0&&pwrite(fd,"X",1,0)==1&&!close(fd));}
    if(test==9)assert(!fchmodat(l.task,MARKER,0644,0));
    if(test==10)reject_daemon_remove=1;
    if(test==11)fail_marker_fsync=1;
    if(test==12)assert(!unlinkat(l.adb,"ksud",0));
    if(test==13){assert(!unlinkat(l.task,MARKER,0));save_file(l.task,MARKER,"invalid\n",8,0600);}
    if(test==14){struct stat s;assert(!fstatat(l.task,MARKER,&s,AT_SYMLINK_NOFOLLOW));for(int i=0;i<fixture_count;i++)if(fixtures[i].dev==s.st_dev&&fixtures[i].ino==s.st_ino)fixtures[i].ino=0;}
    if(test==15){assert(!unlinkat(l.task,BACKUP,0)&&!symlinkat("../ksud",l.task,BACKUP));}
    if(test==16){assert(!unlinkat(l.service,SCRIPT,0)&&!symlinkat("user-script.sh",l.service,SCRIPT));}
    if(test==17)assert(!fchmod(l.task,0755));
    struct result r={.stage="unit"};memcpy(r.boot,current,sizeof(current));int rc=rearm(&l,&r);
    if(test<=1){assert(rc==0&&r.passed&&r.daemon_removed&&r.marker_removed_or_absent&&r.backup_retained&&r.script_retained);assert(!absent(l.adb,"ksud")&&!absent(l.task,MARKER));assert(order_valid);assert(!identity_still(l.task,BACKUP,&backup_id)&&!identity_still(l.service,SCRIPT,&script_id));if(test==0)assert(r.marker_removed&&!strcmp(r.prior_marker_boot,prior));else assert(!r.marker_present&&!r.marker_removed&&!r.prior_marker_boot[0]);}
    else {assert(rc<0&&!r.passed&&!r.daemon_removed);assert(!daemon_unlinked);if(test==10||test==11){assert(r.marker_removed&&marker_unlinked);assert(!identity_still(l.task,BACKUP,&backup_id)&&!identity_still(l.service,SCRIPT,&script_id));}else assert(!marker_unlinked);}
    struct stat after_user;assert(!fixture_fstatat(l.service,"user-script.sh",&after_user,AT_SYMLINK_NOFOLLOW)&&same(&user_id,&after_user));
    /* Host cleanup lists only this model's own exact fixture paths. */
    const char *task_names[]={MARKER,BACKUP,"foreign","backup-hardlink"};for(unsigned i=0;i<4;i++){int c=unlinkat(l.task,task_names[i],0);assert(!c||errno==ENOENT);}
    assert(!unlinkat(l.service,SCRIPT,0)&&!unlinkat(l.service,"user-script.sh",0));int c=unlinkat(l.adb,"ksud",0);assert(!c||errno==ENOENT);
    close(l.task);close(l.service);assert(!unlinkat(l.adb,TASK,AT_REMOVEDIR)&&!unlinkat(l.adb,"service.d",AT_REMOVEDIR));close(l.adb);assert(!unlinkat(l.data,"adb",AT_REMOVEDIR));close(l.data);assert(!rmdir(tmp));
}
int main(int argc,char **argv){
    assert(argc==2&&getuid()!=0);assert(valid_boot(prior)&&!valid_boot("12345678-1234-1234-1234-123456789abX"));
    FILE *f=fopen(argv[1],"rb");assert(f);unsigned char *daemon=malloc(DAEMON_BYTES);assert(daemon&&fread(daemon,1,DAEMON_BYTES,f)==DAEMON_BYTES);fclose(f);
    for(int i=0;i<18;i++)run_case(i,daemon);free(daemon);
    puts("PASS: 18 actual host-filesystem cases with modeled root ownership; present/absent marker, old UUID, unknown entry/content, symlink, hardlink, metadata/owner rejection, all validation before mutation, marker-first daemon-last, partial fsync/unlink failure, backup/script/user script preserved; no device or script execution");return 0;
}
