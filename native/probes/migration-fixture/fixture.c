/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Task-only two-boot fixture. Fixed paths and modes; no external commands. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "sha256.h"
#include "pins.h"
static const char *TASK="resukisu-migration-test",*SCRIPT="90-resukisu-migration-test.sh",*BACKUP="daemon-before-seed",*MARKER="service-ran";
struct result{const char *mode,*stage;int error,passed,script_installed,daemon_moved,marker_verified,seed_verified,script_removed,marker_removed,backup_removed,task_removed;char boot[37];};
static int write_all(int fd,const void *p,size_t n){const unsigned char *b=p;while(n){ssize_t k=write(fd,b,n);if(k<0&&errno==EINTR)continue;if(k<=0)return -1;b+=k;n-=(size_t)k;}return 0;}
static int same(const struct stat *a,const struct stat *b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&a->st_uid==b->st_uid&&a->st_gid==b->st_gid&&a->st_mode==b->st_mode&&a->st_nlink==b->st_nlink&&a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;}
static int valid_boot(const char *b){for(int i=0;i<36;i++){if(i==8||i==13||i==18||i==23){if(b[i]!='-')return 0;}else if(!((b[i]>='0'&&b[i]<='9')||(b[i]>='a'&&b[i]<='f')))return 0;}return !b[36];}
static int get_boot(char b[37]){char tmp[38]={0};int fd=open("/proc/sys/kernel/random/boot_id",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;ssize_t n=read(fd,tmp,sizeof(tmp));close(fd);if(n!=37||tmp[36]!='\n'){errno=EINVAL;return -1;}memcpy(b,tmp,36);b[36]=0;if(!valid_boot(b)){errno=EINVAL;return -1;}return 0;}
static int root_context(void){char b[64]={0};int fd=open("/proc/self/attr/current",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return 0;ssize_t n=read(fd,b,sizeof(b)-1);close(fd);if(n<=0||n>=(ssize_t)sizeof(b)-1)return 0;while(n&&(b[n-1]=='\n'||!b[n-1]))b[--n]=0;return !strcmp(b,"u:r:ksu:s0");}
static int directory(int parent,const char *name,int create,mode_t mode,int exact){
    int created=0;if(create){if(!mkdirat(parent,name,mode))created=1;else if(errno!=EEXIST)return -1;}int fd=openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;struct stat s;
    if(fstat(fd,&s)||!S_ISDIR(s.st_mode)||(mode&&(s.st_uid!=0||s.st_gid!=0||(s.st_mode&0022)||(exact&&(s.st_mode&07777)!=mode)))){close(fd);errno=EPERM;return -1;}if(created&&fchmod(fd,mode)){int e=errno;close(fd);errno=e;return -1;}return fd;
}
static int absent(int dir,const char *name){struct stat s;if(!fstatat(dir,name,&s,AT_SYMLINK_NOFOLLOW)){errno=EEXIST;return -1;}return errno==ENOENT?0:-1;}
static int check_task_entries(int dir,int armed){
    int copy=openat(dir,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(copy<0)return -1;DIR *d=fdopendir(copy);if(!d){close(copy);return -1;}struct dirent *e;int count=0,rc=0;errno=0;
    while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        if(!armed||(strcmp(e->d_name,BACKUP)&&strcmp(e->d_name,MARKER))){errno=EEXIST;rc=-1;break;}count++;errno=0;}
    if(!rc&&errno)rc=-1;if(!rc&&armed&&count!=2){errno=ENODATA;rc=-1;}int err=errno;closedir(d);errno=err;return rc;
}
static int file_verified(int dir,const char *name,size_t size,const unsigned char expected[32],mode_t mode,struct stat *identity){
    int fd=openat(dir,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;struct stat a,b;
    if(fstat(fd,&a)||!S_ISREG(a.st_mode)||a.st_uid!=0||a.st_gid!=0||a.st_nlink!=1||(a.st_mode&07777)!=mode||a.st_size!=(off_t)size){close(fd);errno=EKEYREJECTED;return -1;}
    unsigned char *data=malloc(size);if(!data){close(fd);return -1;}size_t at=0;while(at<size){ssize_t n=read(fd,data+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0)break;at+=(size_t)n;}unsigned char hash[32];sha256(data,at,hash);free(data);
    int ok=at==size&&!memcmp(hash,expected,32)&&!fstat(fd,&b)&&same(&a,&b);close(fd);if(!ok){errno=EKEYREJECTED;return -1;}*identity=a;return 0;
}
static int marker_verified(int dir,const char *boot,struct stat *identity){
    unsigned char hash[32];char expected[38];snprintf(expected,sizeof(expected),"%s\n",boot);sha256((const unsigned char*)expected,37,hash);return file_verified(dir,MARKER,37,hash,0600,identity);
}
static int identity_still(int dir,const char *name,const struct stat *before){struct stat now;if(fstatat(dir,name,&now,AT_SYMLINK_NOFOLLOW)||!same(before,&now)){errno=ESTALE;return -1;}return 0;}
static int install_script(int service,int *installed){
    char temp[96];snprintf(temp,sizeof(temp),".resukisu-migration-script.tmp.%ld",(long)getpid());int fd=openat(service,temp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return -1;
    int ok=!write_all(fd,script_bytes,SCRIPT_BYTES)&&!fchmod(fd,0755)&&!fsync(fd);int err=errno;if(close(fd)&&ok){ok=0;err=errno;}
    if(ok&&syscall(SYS_renameat2,service,temp,service,SCRIPT,1U)){ok=0;err=errno;}if(!ok){unlinkat(service,temp,0);errno=err;return -1;}*installed=1;return fsync(service);
}
static int remove_exact(int dir,const char *name,const struct stat *identity,int *removed){if(identity_still(dir,name,identity))return -1;if(unlinkat(dir,name,0))return -1;*removed=1;return fsync(dir);}
static int report(const struct result *r){char b[1536];int n=snprintf(b,sizeof(b),"{\"schemaVersion\":1,\"mode\":\"%s\",\"stage\":\"%s\",\"passed\":%s,\"errno\":%d,\"bootId\":\"%s\",\"scriptInstalled\":%s,\"daemonMovedToBackup\":%s,\"markerMatchesCurrentBoot\":%s,\"seededDaemonHashMatched\":%s,\"scriptRemoved\":%s,\"markerRemoved\":%s,\"backupRemoved\":%s,\"taskDirectoryRemoved\":%s,\"otherUserFilesModified\":false,\"externalCommandsExecuted\":false}\n",r->mode,r->stage,r->passed?"true":"false",r->error,r->boot,r->script_installed?"true":"false",r->daemon_moved?"true":"false",r->marker_verified?"true":"false",r->seed_verified?"true":"false",r->script_removed?"true":"false",r->marker_removed?"true":"false",r->backup_removed?"true":"false",r->task_removed?"true":"false");if(n<0||(size_t)n>=sizeof(b))return -1;return write_all(1,b,(size_t)n);}
int main(int argc,char **argv){
    int arm=argc==2&&!strcmp(argv[1],"arm"),check=argc==2&&!strcmp(argv[1],"check-cleanup");struct result r={.mode=arm?"arm":check?"check-cleanup":"invalid",.stage="identity_gate"};
    if((!arm&&!check)||getuid()!=0||geteuid()!=0||getgid()!=0||getegid()!=0){report(&r);return 64;}
    struct sigaction sa={.sa_handler=SIG_DFL};sigemptyset(&sa.sa_mask);sigset_t unblocked;sigemptyset(&unblocked);sigaddset(&unblocked,SIGALRM);if(sigaction(SIGALRM,&sa,NULL)||sigprocmask(SIG_UNBLOCK,&unblocked,NULL))return 65;alarm(6);
    if(!root_context()){report(&r);return 65;}umask(0077);if(get_boot(r.boot)){r.error=errno;report(&r);return 65;}
    int top=-1,data=-1,adb=-1,service=-1,task=-1,rc=68;struct stat daemon_id,backup_id,script_id,marker_id,task_id,adb_id,service_id;
    r.stage="directory_validation";top=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(top<0)goto out;data=directory(top,"data",0,0,0);if(data<0)goto out;adb=directory(data,"adb",0,0700,1);if(adb<0)goto out;
    service=directory(adb,"service.d",arm,0755,0);if(service<0)goto out;task=directory(adb,TASK,arm,0700,1);if(task<0)goto out;
    if(fstat(adb,&adb_id)||fstat(service,&service_id)||fstat(task,&task_id)||adb_id.st_dev!=service_id.st_dev||adb_id.st_dev!=task_id.st_dev){errno=EXDEV;goto out;}
    r.stage="fixture_collision_check";if(check_task_entries(task,check))goto out;
    r.stage="daemon_validation";if(file_verified(adb,"ksud",DAEMON_BYTES,daemon_hash,0755,&daemon_id))goto out;r.seed_verified=check;
    if(arm){
        r.stage="script_destination_validation";if(absent(service,SCRIPT)||absent(task,BACKUP)||absent(task,MARKER))goto out;
        r.stage="install_script";if(install_script(service,&r.script_installed))goto out;if(file_verified(service,SCRIPT,SCRIPT_BYTES,script_hash,0755,&script_id))goto out;
        r.stage="move_daemon_last";if(identity_still(adb,"ksud",&daemon_id)||syscall(SYS_renameat2,adb,"ksud",task,BACKUP,1U))goto out;r.daemon_moved=1;if(fsync(adb)||fsync(task))goto out;
        r.stage="verify_armed";if(file_verified(task,BACKUP,DAEMON_BYTES,daemon_hash,0755,&backup_id)||backup_id.st_ino!=daemon_id.st_ino||absent(adb,"ksud")){errno=EKEYREJECTED;goto out;}
        r.stage="armed_for_next_boot";r.passed=1;rc=0;
    }else{
        r.stage="validate_current_boot_marker";if(marker_verified(task,r.boot,&marker_id))goto out;r.marker_verified=1;
        r.stage="validate_script_and_backup";if(file_verified(service,SCRIPT,SCRIPT_BYTES,script_hash,0755,&script_id)||file_verified(task,BACKUP,DAEMON_BYTES,daemon_hash,0755,&backup_id))goto out;
        r.stage="revalidate_cleanup_set";if(identity_still(adb,"ksud",&daemon_id)||identity_still(task,MARKER,&marker_id)||identity_still(task,BACKUP,&backup_id)||identity_still(service,SCRIPT,&script_id)||check_task_entries(task,1))goto out;
        r.stage="cleanup_verified_fixture";if(remove_exact(service,SCRIPT,&script_id,&r.script_removed)||remove_exact(task,MARKER,&marker_id,&r.marker_removed)||remove_exact(task,BACKUP,&backup_id,&r.backup_removed))goto out;
        if(check_task_entries(task,0))goto out;struct stat now;if(fstatat(adb,TASK,&now,AT_SYMLINK_NOFOLLOW)||now.st_dev!=task_id.st_dev||now.st_ino!=task_id.st_ino){errno=ESTALE;goto out;}if(unlinkat(adb,TASK,AT_REMOVEDIR))goto out;r.task_removed=1;if(fsync(adb))goto out;
        r.stage="fixture_verified_and_cleaned";r.passed=1;rc=0;
    }
out:
    r.error=rc?errno:0;if(task>=0)close(task);if(service>=0)close(service);if(adb>=0)close(adb);if(data>=0)close(data);if(top>=0)close(top);alarm(0);if(report(&r))return 69;return rc;
}
