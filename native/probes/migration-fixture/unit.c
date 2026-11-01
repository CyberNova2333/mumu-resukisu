/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
static struct {dev_t dev;ino_t ino;} fixtures[16];static int fixture_count;
static void model_owner(struct stat *s){for(int i=0;i<fixture_count;i++)if(s->st_dev==fixtures[i].dev&&s->st_ino==fixtures[i].ino){s->st_uid=0;s->st_gid=0;}}
static int fixture_fstat(int fd,struct stat *s){int rc=fstat(fd,s);if(!rc)model_owner(s);return rc;}
static int fixture_fstatat(int fd,const char *n,struct stat *s,int flags){int rc=fstatat(fd,n,s,flags);if(!rc)model_owner(s);return rc;}
#define fstat fixture_fstat
#define fstatat fixture_fstatat
#define main fixture_main
#include "fixture.c"
#undef main
#undef fstat
#undef fstatat
static void register_fixture(int dir,const char *name){assert(fixture_count<16);struct stat s;assert(!fstatat(dir,name,&s,AT_SYMLINK_NOFOLLOW));fixtures[fixture_count].dev=s.st_dev;fixtures[fixture_count++].ino=s.st_ino;}
static void save_file(int dir,const char *name,const void *body,size_t n,mode_t mode){int fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL,mode);assert(fd>=0);assert(!write_all(fd,body,n)&&!fchmod(fd,mode)&&!close(fd));register_fixture(dir,name);}
int main(int argc,char **argv){
    assert(argc==2&&getuid()!=0);assert(valid_boot("12345678-1234-1234-1234-123456789abc"));assert(!valid_boot("12345678-1234-1234-1234-123456789abX"));
    FILE *f=fopen(argv[1],"rb");assert(f);unsigned char *daemon=malloc(DAEMON_BYTES);assert(daemon&&fread(daemon,1,DAEMON_BYTES,f)==DAEMON_BYTES);fclose(f);
    char tmp[]="/tmp/resukisu-migration-fixture-unit-XXXXXX";assert(mkdtemp(tmp));int adb=open(tmp,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(adb>=0);register_fixture(adb,".");
    assert(!mkdirat(adb,"service.d",0755));register_fixture(adb,"service.d");assert(!mkdirat(adb,TASK,0700));register_fixture(adb,TASK);int service=directory(adb,"service.d",0,0755,0),task=directory(adb,TASK,0,0700,1);assert(service>=0&&task>=0);
    assert(!check_task_entries(task,0));save_file(adb,"ksud",daemon,DAEMON_BYTES,0755);struct stat original,script_id,backup_id,marker_id,current_id;assert(!file_verified(adb,"ksud",DAEMON_BYTES,daemon_hash,0755,&original));
    int installed=0;assert(!install_script(service,&installed)&&installed==1);register_fixture(service,SCRIPT);assert(!file_verified(service,SCRIPT,SCRIPT_BYTES,script_hash,0755,&script_id));installed=0;assert(install_script(service,&installed)<0&&errno==EEXIST&&installed==0);assert(!identity_still(service,SCRIPT,&script_id));
    assert(!syscall(SYS_renameat2,adb,"ksud",task,BACKUP,1U));assert(!absent(adb,"ksud"));assert(!file_verified(task,BACKUP,DAEMON_BYTES,daemon_hash,0755,&backup_id)&&backup_id.st_ino==original.st_ino);
    save_file(adb,"ksud",daemon,DAEMON_BYTES,0755);assert(!file_verified(adb,"ksud",DAEMON_BYTES,daemon_hash,0755,&current_id));
    const char boot[]="12345678-1234-1234-1234-123456789abc";char marker[38];snprintf(marker,sizeof(marker),"%s\n",boot);save_file(task,MARKER,marker,37,0600);assert(!marker_verified(task,boot,&marker_id));assert(marker_verified(task,"aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",&marker_id)<0);assert(!marker_verified(task,boot,&marker_id));assert(!check_task_entries(task,1));
    save_file(task,"foreign","keep",4,0600);assert(check_task_entries(task,1)<0&&errno==EEXIST);assert(!identity_still(task,BACKUP,&backup_id));assert(!unlinkat(task,"foreign",0));
    save_file(service,"real-user-script.sh","do not remove\n",14,0755);struct stat user_script;assert(!fstatat(service,"real-user-script.sh",&user_script,AT_SYMLINK_NOFOLLOW));
    struct stat mismatch=backup_id;mismatch.st_ino++;int removed=0;assert(remove_exact(task,BACKUP,&mismatch,&removed)<0&&removed==0);assert(!identity_still(task,BACKUP,&backup_id));
    assert(!remove_exact(service,SCRIPT,&script_id,&removed)&&removed);removed=0;assert(!remove_exact(task,MARKER,&marker_id,&removed)&&removed);removed=0;assert(!remove_exact(task,BACKUP,&backup_id,&removed)&&removed);assert(!check_task_entries(task,0));assert(!identity_still(adb,"ksud",&current_id));
    struct stat user_after;assert(!fstatat(service,"real-user-script.sh",&user_after,AT_SYMLINK_NOFOLLOW)&&same(&user_script,&user_after));close(task);assert(!unlinkat(adb,TASK,AT_REMOVEDIR));
    assert(!unlinkat(service,"real-user-script.sh",0));close(service);assert(!unlinkat(adb,"service.d",AT_REMOVEDIR));assert(!unlinkat(adb,"ksud",0));close(adb);assert(!rmdir(tmp));free(daemon);
    puts("PASS: modeled root metadata only; actual host script NO_REPLACE collision, daemon atomic backup inode, current-boot marker match/stale rejection, unknown task entry preserves backup, exact-identity cleanup, actual user script and current daemon retained; no device/script/daemon execution");return 0;
}
