/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Read-only fixed root acceptance after the user-authorized normal su path. */
#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "sha256.h"
#define EXPECTED_BYTES 6001344
#define GET_INFO 0x80104b02UL
#define GET_MANAGERS 0xc0004b69UL
#include "target.h"
#define EXPECTED_UID TARGET_MANAGER_UID
#include "expected-daemon.h"
struct ksu_info{uint32_t version,flags,features,uapi_version;};
struct manager_entry{uint32_t uid;uint8_t signature_index;}__attribute__((packed));
struct managers{uint16_t count,total_count;struct manager_entry entries[32];}__attribute__((packed));
_Static_assert(sizeof(struct ksu_info)==16,"UAPI info");
_Static_assert(sizeof(struct manager_entry)==5,"UAPI manager");
struct result{const char *status;int error,executed,passed,manager_rc,package_match,manager_match,daemon_match,observer_absent[4];struct ksu_info info;char boot[37];};
static int valid_boot(const char *s){for(int i=0;i<36;i++){if(i==8||i==13||i==18||i==23){if(s[i]!='-')return 0;}else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;}return s[36]==0;}
static int read_boot(char out[37]){char b[38]={0};int fd=open("/proc/sys/kernel/random/boot_id",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;ssize_t n=read(fd,b,sizeof(b));close(fd);if(n!=37||b[36]!='\n'){errno=EINVAL;return -1;}memcpy(out,b,36);out[36]=0;if(!valid_boot(out)){errno=EINVAL;return -1;}return 0;}
static int stable(const struct stat *a,const struct stat *b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&a->st_mode==b->st_mode&&a->st_uid==b->st_uid&&a->st_gid==b->st_gid&&a->st_nlink==b->st_nlink&&a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;}
static int dir_at(int parent,const char *name,int exact_mode){
    int fd=openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;struct stat st;
    if(fstat(fd,&st)||!S_ISDIR(st.st_mode)||(exact_mode&&(st.st_uid!=0||st.st_gid!=0||(st.st_mode&07777)!=(mode_t)exact_mode))){close(fd);errno=EPERM;return -1;}return fd;
}
static int driver(struct ksu_info *info){
    int fd=-1;syscall(SYS_reboot,0xdeadbeefUL,0xcafebabeUL,0UL,&fd);if(fd<0){errno=ENODEV;return -1;}
    char path[64],name[64];snprintf(path,sizeof(path),"/proc/self/fd/%d",fd);ssize_t n=readlink(path,name,sizeof(name)-1);
    if(n<0){close(fd);return -1;}name[n]=0;if(strcmp(name,"anon_inode:[ksu_driver]")){close(fd);errno=EPROTO;return -1;}
    if(fcntl(fd,F_SETFD,FD_CLOEXEC)||ioctl(fd,GET_INFO,info)||info->version!=35253||info->uapi_version!=6||info->flags!=1||info->features!=7){close(fd);errno=EPROTO;return -1;}return fd;
}
static int manager_list_match(const struct managers *m){
    if(m->count!=32||m->total_count>32)return -1;int count=0;
    for(unsigned i=0;i<m->total_count;i++)if(m->entries[i].uid==EXPECTED_UID&&m->entries[i].signature_index==0)count++;
    return count==1;
}
static int package_line(const char *line){
    static const char prefix[]="com.resukisu.resukisu ";if(strncmp(line,prefix,sizeof(prefix)-1))return 0;
    const char *p=line+sizeof(prefix)-1;unsigned value=0;int digits=0;
    while(*p>='0'&&*p<='9'){if(++digits>5)return -1;value=value*10U+(unsigned)(*p++-'0');}
    return digits&&*p==' '&&value==EXPECTED_UID?1:-1;
}
static int package_match(int data){
    int system=dir_at(data,"system",0);if(system<0)return -1;int fd=openat(system,"packages.list",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);close(system);if(fd<0)return -1;
    struct stat a,b;if(fstat(fd,&a)||!S_ISREG(a.st_mode)||a.st_nlink!=1||a.st_size<=0||a.st_size>4*1024*1024||(a.st_mode&0022)||(a.st_uid!=0&&a.st_uid!=1000)){close(fd);errno=EPERM;return -1;}
    FILE *f=fdopen(fd,"r");if(!f){close(fd);return -1;}char line[4096];int seen=0,ok=1;unsigned bytes=0;
    while(fgets(line,sizeof(line),f)){
        bytes+=(unsigned)strlen(line);if(bytes>4*1024*1024){ok=0;break;}
        if(!strchr(line,'\n')){ok=0;break;}int value=package_line(line);if(value<0){ok=0;break;}if(value==1)seen++;
    }
    if(ferror(f)||fstat(fd,&b)||!stable(&a,&b))ok=0;fclose(f);return ok&&seen==1;
}
static int image_match(const unsigned char *buf,size_t n){
    if(n!=EXPECTED_BYTES)return 0;unsigned char hash[32];sha256(buf,n,hash);if(memcmp(hash,expected_hash,32))return 0;
    Elf64_Ehdr h;memcpy(&h,buf,sizeof(h));return !memcmp(h.e_ident,ELFMAG,4)&&h.e_ident[EI_CLASS]==ELFCLASS64&&h.e_ident[EI_DATA]==ELFDATA2LSB&&h.e_machine==EM_X86_64&&h.e_type==ET_DYN;
}
static int daemon_match(int adb){
    int fd=openat(adb,"ksud",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;struct stat a,b;
    if(fstat(fd,&a)||!S_ISREG(a.st_mode)||a.st_uid!=0||a.st_gid!=0||a.st_nlink!=1||(a.st_mode&07777)!=0755||a.st_size!=EXPECTED_BYTES){close(fd);errno=EKEYREJECTED;return -1;}
    unsigned char *buf=malloc(EXPECTED_BYTES);if(!buf){close(fd);return -1;}size_t at=0;
    while(at<EXPECTED_BYTES){ssize_t n=read(fd,buf+at,EXPECTED_BYTES-at);if(n<0&&errno==EINTR)continue;if(n<=0)break;at+=(size_t)n;}
    int ok=image_match(buf,at)&&!fstat(fd,&b)&&stable(&a,&b);free(buf);close(fd);if(!ok){errno=EKEYREJECTED;return -1;}return 1;
}
static int absent_at(int dir,const char *name){struct stat st;if(!fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW))return 0;return errno==ENOENT?1:-1;}
static int marker_absent(int adb,const char *name){
    int absent=absent_at(adb,name);if(absent!=0)return absent;int dir=dir_at(adb,name,0755);if(dir<0)return -1;absent=absent_at(dir,"00-resukisu-validation.sh");close(dir);return absent;
}
static int all_absent(const struct result *r){for(int i=0;i<4;i++)if(r->observer_absent[i]!=1)return 0;return 1;}
static int root_context(void){char b[64]={0};int fd=open("/proc/self/attr/current",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return 0;ssize_t n=read(fd,b,sizeof(b)-1);close(fd);if(n<=0||n>=(ssize_t)sizeof(b)-1)return 0;while(n&&(b[n-1]=='\n'||!b[n-1]))b[--n]=0;return !strcmp(b,"u:r:ksu:s0");}
static int report(const struct result *r){
    char buffer[1536];int n=snprintf(buffer,sizeof(buffer),"{\"schemaVersion\":1,\"executed\":%s,\"passed\":%s,\"status\":\"%s\",\"error\":%d,\"bootId\":\"%s\",\"uid\":%u,\"euid\":%u,\"version\":%u,\"uapiVersion\":%u,\"managerRecognized\":%s,\"expectedManagerUid\":%u,\"managerPackageUidMatched\":%s,\"managerQueryRc\":%d,\"daemonHashMatched\":%s,\"daemonElf64\":%s,\"observerAbsent\":%s,\"observerAbsence\":{\"supervisor\":%s,\"postMountMarker\":%s,\"serviceMarker\":%s,\"bootCompletedMarker\":%s},\"lifecycleVerified\":false,\"naturalLifecycleSignalAvailable\":false,\"filesWritten\":false,\"allowlistModified\":false,\"scope\":\"normal_root_manager_daemon_and_exact_observer_absence_not_three_stage_completion\"}\n",
      r->executed?"true":"false",r->passed?"true":"false",r->status,r->error,r->boot,(unsigned)getuid(),(unsigned)geteuid(),r->info.version,r->info.uapi_version,r->manager_match&&r->package_match?"true":"false",(unsigned)EXPECTED_UID,r->package_match?"true":"false",r->manager_rc,r->daemon_match?"true":"false",r->daemon_match?"true":"false",all_absent(r)?"true":"false",r->observer_absent[0]==1?"true":"false",r->observer_absent[1]==1?"true":"false",r->observer_absent[2]==1?"true":"false",r->observer_absent[3]==1?"true":"false");
    if(n<0||(size_t)n>=sizeof(buffer))return -1;size_t at=0;while(at<(size_t)n){ssize_t count=write(1,buffer+at,(size_t)n-at);if(count<0&&errno==EINTR)continue;if(count<=0)return -1;at+=(size_t)count;}return 0;
}
int main(int argc,char **argv){
    (void)argv;struct result r={.status="root_gate_denied",.manager_rc=-ENODATA,.observer_absent={-1,-1,-1,-1}};
    if(argc!=1||getuid()!=0||geteuid()!=0){report(&r);return 64;}
    struct sigaction sa={.sa_handler=SIG_DFL};sigemptyset(&sa.sa_mask);sigset_t unblocked;sigemptyset(&unblocked);sigaddset(&unblocked,SIGALRM);
    if(sigaction(SIGALRM,&sa,NULL)||sigprocmask(SIG_UNBLOCK,&unblocked,NULL)){r.error=errno;report(&r);return 65;}alarm(4);
    r.status="root_context_mismatch";if(!root_context()){report(&r);return 65;}r.executed=1;
    int top=-1,data=-1,adb=-1,fd=-1;int rc=68;
    r.status="boot_id_failed";if(read_boot(r.boot))goto out;
    r.status="driver_mismatch";fd=driver(&r.info);if(fd<0)goto out;
    r.status="directory_validation_failed";top=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(top<0)goto out;data=dir_at(top,"data",0);if(data<0)goto out;adb=dir_at(data,"adb",0700);if(adb<0)goto out;
    r.status="manager_package_mismatch";int pkg=package_match(data);if(pkg!=1){if(!pkg)errno=ENODATA;goto out;}r.package_match=1;
    r.status="manager_registry_mismatch";struct managers m={.count=32};r.manager_rc=ioctl(fd,GET_MANAGERS,&m);if(r.manager_rc){r.manager_rc=-errno;goto out;}int match=manager_list_match(&m);if(match!=1){errno=match<0?EOVERFLOW:ENODATA;goto out;}r.manager_match=1;
    r.status="daemon_identity_mismatch";if(daemon_match(adb)!=1)goto out;r.daemon_match=1;
    r.status="observer_presence_or_path_error";r.observer_absent[0]=absent_at(adb,"resukisu-stage");const char *dirs[]={"post-mount.d","service.d","boot-completed.d"};for(int i=0;i<3;i++)r.observer_absent[i+1]=marker_absent(adb,dirs[i]);if(!all_absent(&r)){errno=EEXIST;goto out;}
    r.passed=1;r.status="normal_root_manager_daemon_and_observer_absence_verified";rc=0;
out:
    r.error=rc?errno:0;if(fd>=0)close(fd);if(adb>=0)close(adb);if(data>=0)close(data);if(top>=0)close(top);alarm(0);if(report(&r))return 69;return rc;
}
