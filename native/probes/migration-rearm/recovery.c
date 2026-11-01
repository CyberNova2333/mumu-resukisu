/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Task-only interrupted-fixture rearm. Fixed paths; no external commands. */
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
static int file_verified(int dir,const char *name,size_t size,const unsigned char expected[32],mode_t mode,struct stat *identity){
    int fd=openat(dir,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;struct stat a,b;
    if(fstat(fd,&a)||!S_ISREG(a.st_mode)||a.st_uid!=0||a.st_gid!=0||a.st_nlink!=1||(a.st_mode&07777)!=mode||a.st_size!=(off_t)size){close(fd);errno=EKEYREJECTED;return -1;}
    unsigned char *data=malloc(size);if(!data){close(fd);return -1;}size_t at=0;while(at<size){ssize_t n=read(fd,data+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0)break;at+=(size_t)n;}unsigned char hash[32];sha256(data,at,hash);free(data);
    int ok=at==size&&!memcmp(hash,expected,32)&&!fstat(fd,&b)&&same(&a,&b);close(fd);if(!ok){errno=EKEYREJECTED;return -1;}*identity=a;return 0;
}
static int identity_still(int dir,const char *name,const struct stat *before){struct stat now;if(fstatat(dir,name,&now,AT_SYMLINK_NOFOLLOW)||!same(before,&now)){errno=ESTALE;return -1;}return 0;}
static int remove_exact(int dir,const char *name,const struct stat *identity,int *removed){if(identity_still(dir,name,identity))return -1;if(unlinkat(dir,name,0))return -1;*removed=1;return fsync(dir);}
struct result {
    const char *stage;
    int error, passed, daemon_verified, backup_verified, script_verified;
    int marker_present, marker_validated, marker_removed, marker_removed_or_absent;
    int daemon_removed, backup_retained, script_retained;
    char boot[37], prior_marker_boot[37];
};
struct layout { int data, adb, service, task; struct stat adb_id, service_id, task_id; };

static int same_directory(const struct stat *a,const struct stat *b) {
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino && a->st_uid==b->st_uid &&
        a->st_gid==b->st_gid && a->st_mode==b->st_mode && a->st_nlink==b->st_nlink;
}
static int directory_still(int dir,const char *name,const struct stat *before) {
    struct stat now;
    if(fstatat(dir,name,&now,AT_SYMLINK_NOFOLLOW)||!same_directory(before,&now)) { errno=ESTALE; return -1; }
    return 0;
}
static int layout_still(const struct layout *l) {
    return directory_still(l->data,"adb",&l->adb_id)||
        directory_still(l->adb,"service.d",&l->service_id)||
        directory_still(l->adb,TASK,&l->task_id) ? -1:0;
}
static int task_entries(int dir,int expect_marker) {
    int copy=openat(dir,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(copy<0)return -1;
    DIR *d=fdopendir(copy); if(!d){close(copy);return -1;}
    struct dirent *e; int backup=0,marker=0,rc=0; errno=0;
    while((e=readdir(d))) {
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        if(!strcmp(e->d_name,BACKUP))backup++;
        else if(!strcmp(e->d_name,MARKER))marker++;
        else {errno=EEXIST;rc=-1;break;}
        errno=0;
    }
    if(!rc&&errno)rc=-1;
    if(!rc&&(backup!=1||marker!=expect_marker)){errno=ESTALE;rc=-1;}
    int err=errno;closedir(d);errno=err;return rc;
}
static int read_prior_marker(int dir,int *present,char boot[37],struct stat *identity) {
    *present=0;
    int fd=openat(dir,MARKER,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return errno==ENOENT?0:-1;
    struct stat a,b;
    if(fstat(fd,&a)||!S_ISREG(a.st_mode)||a.st_uid!=0||a.st_gid!=0||
        a.st_nlink!=1||(a.st_mode&07777)!=0600||a.st_size!=37) {
        close(fd);errno=EKEYREJECTED;return -1;
    }
    char bytes[38]={0};size_t at=0;
    while(at<37){ssize_t n=read(fd,bytes+at,37-at);if(n<0&&errno==EINTR)continue;if(n<=0)break;at+=(size_t)n;}
    int ok=at==37&&bytes[36]=='\n'&&!fstat(fd,&b)&&same(&a,&b);
    close(fd);bytes[36]=0;
    if(!ok||!valid_boot(bytes)){errno=EKEYREJECTED;return -1;}
    memcpy(boot,bytes,37);*identity=a;*present=1;return 0;
}
static int rearm(const struct layout *l,struct result *r) {
    struct stat daemon_id,backup_id,script_id,marker_id;
    r->stage="validate_daemon_backup_and_script";
    if(layout_still(l))return -1;
    if(file_verified(l->adb,"ksud",DAEMON_BYTES,daemon_hash,0755,&daemon_id))return -1;
    r->daemon_verified=1;
    if(file_verified(l->task,BACKUP,DAEMON_BYTES,daemon_hash,0755,&backup_id))return -1;
    r->backup_verified=1;
    if(file_verified(l->service,SCRIPT,SCRIPT_BYTES,script_hash,0755,&script_id))return -1;
    r->script_verified=1;
    if(daemon_id.st_dev!=backup_id.st_dev||daemon_id.st_ino==backup_id.st_ino){errno=EXDEV;return -1;}
    r->stage="validate_prior_marker_and_task_entries";
    if(read_prior_marker(l->task,&r->marker_present,r->prior_marker_boot,&marker_id)||
        task_entries(l->task,r->marker_present))return -1;
    r->marker_validated=1;
    r->stage="revalidate_complete_set_before_mutation";
    if(layout_still(l)||identity_still(l->adb,"ksud",&daemon_id)||
        identity_still(l->task,BACKUP,&backup_id)||identity_still(l->service,SCRIPT,&script_id)||
        (r->marker_present?identity_still(l->task,MARKER,&marker_id):absent(l->task,MARKER))||
        task_entries(l->task,r->marker_present))return -1;
    r->backup_retained=1;r->script_retained=1;
    r->stage="remove_verified_marker_first";
    if(r->marker_present&&remove_exact(l->task,MARKER,&marker_id,&r->marker_removed))return -1;
    if(absent(l->task,MARKER))return -1;
    r->marker_removed_or_absent=1;
    r->stage="revalidate_retained_files_before_daemon_removal";
    if(layout_still(l)||identity_still(l->task,BACKUP,&backup_id)||
        identity_still(l->service,SCRIPT,&script_id)||task_entries(l->task,0))return -1;
    r->stage="remove_same_version_daemon_last";
    if(remove_exact(l->adb,"ksud",&daemon_id,&r->daemon_removed))return -1;
    r->stage="verify_rearmed_state";
    if(layout_still(l)||absent(l->adb,"ksud")||absent(l->task,MARKER)||
        identity_still(l->task,BACKUP,&backup_id)||identity_still(l->service,SCRIPT,&script_id)||
        task_entries(l->task,0))return -1;
    r->stage="rearmed_for_next_boot";r->passed=1;return 0;
}
static int report(const struct result *r) {
    char b[2048];
    int n=snprintf(b,sizeof(b),"{\"schemaVersion\":1,\"mode\":\"rearm\",\"stage\":\"%s\",\"passed\":%s,\"errno\":%d,\"bootId\":\"%s\",\"priorMarkerBootId\":\"%s\",\"daemonHashMatched\":%s,\"backupHashMatched\":%s,\"scriptHashMatched\":%s,\"priorMarkerPresent\":%s,\"priorMarkerValidatedOrAbsent\":%s,\"markerRemoved\":%s,\"markerRemovedOrAbsent\":%s,\"daemonRemoved\":%s,\"backupRetained\":%s,\"scriptRetained\":%s,\"otherUserFilesModified\":false,\"externalCommandsExecuted\":false}\n",
        r->stage,r->passed?"true":"false",r->error,r->boot,r->prior_marker_boot,
        r->daemon_verified?"true":"false",r->backup_verified?"true":"false",r->script_verified?"true":"false",
        r->marker_present?"true":"false",r->marker_validated?"true":"false",r->marker_removed?"true":"false",
        r->marker_removed_or_absent?"true":"false",r->daemon_removed?"true":"false",r->backup_retained?"true":"false",r->script_retained?"true":"false");
    if(n<0||(size_t)n>=sizeof(b))return -1;return write_all(1,b,(size_t)n);
}
int main(int argc,char **argv) {
    struct result r={.stage="identity_gate"};
    if(argc!=2||strcmp(argv[1],"rearm")||getuid()!=0||geteuid()!=0||getgid()!=0||getegid()!=0){report(&r);return 64;}
    struct sigaction sa={.sa_handler=SIG_DFL};sigemptyset(&sa.sa_mask);
    sigset_t unblocked;sigemptyset(&unblocked);sigaddset(&unblocked,SIGALRM);
    if(sigaction(SIGALRM,&sa,NULL)||sigprocmask(SIG_UNBLOCK,&unblocked,NULL))return 65;
    alarm(6);
    if(!root_context()){report(&r);return 65;}
    if(get_boot(r.boot)){r.error=errno;report(&r);return 65;}
    struct layout l={.data=-1,.adb=-1,.service=-1,.task=-1};int top=-1,rc=68;
    r.stage="directory_validation";top=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(top<0)goto out;
    l.data=directory(top,"data",0,0,0);if(l.data<0)goto out;
    l.adb=directory(l.data,"adb",0,0700,1);if(l.adb<0)goto out;
    l.service=directory(l.adb,"service.d",0,0755,0);if(l.service<0)goto out;
    l.task=directory(l.adb,TASK,0,0700,1);if(l.task<0)goto out;
    if(fstat(l.adb,&l.adb_id)||fstat(l.service,&l.service_id)||fstat(l.task,&l.task_id)||
        l.adb_id.st_dev!=l.service_id.st_dev||l.adb_id.st_dev!=l.task_id.st_dev){errno=EXDEV;goto out;}
    if(!rearm(&l,&r))rc=0;
out:
    r.error=rc?errno:0;
    if(l.task>=0)close(l.task);if(l.service>=0)close(l.service);if(l.adb>=0)close(l.adb);if(l.data>=0)close(l.data);if(top>=0)close(top);
    alarm(0);if(report(&r))return 69;return rc;
}
