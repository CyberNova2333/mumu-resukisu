/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Boot-only supported-daemon preservation, absent-only seed, and exact observer migration. */
#define _GNU_SOURCE
#include <errno.h>
#include <elf.h>
#include <inttypes.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include "sha256.h"
#include "pins.h"
#include "catalog.h"

struct image { unsigned char *data; size_t size; };
struct result { const char *stage,*action; int error,pinned,cleanup_verified,removed,absent,seeded,supported,preserved,index; struct stat before,after; char boot[37]; };
static int write_all(int fd,const void *p,size_t n) {
    const unsigned char *b=p;
    while(n){ssize_t k=write(fd,b,n);if(k<0&&errno==EINTR)continue;if(k<=0)return -1;b+=k;n-=(size_t)k;}return 0;
}
static int valid_uuid(const char *s) {
    for(int i=0;i<36;i++)if(i==8||i==13||i==18||i==23){if(s[i]!='-')return 0;}
        else if(!((s[i]>='a'&&s[i]<='f')||(s[i]>='0'&&s[i]<='9')))return 0;
    return s[36]==0;
}
static int get_boot(char out[37]) {
    int f=open("/proc/sys/kernel/random/boot_id",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(f<0)return -1;
    char b[38]={0};ssize_t n=read(f,b,sizeof(b));int e=errno;close(f);errno=e;
    if(n!=37||b[36]!='\n'){errno=EINVAL;return -1;}memcpy(out,b,36);out[36]=0;
    if(!valid_uuid(out)){errno=EINVAL;return -1;}return 0;
}
static int protected_dir_meta(const struct stat *s,int exact0700) {
    return S_ISDIR(s->st_mode)&&s->st_uid==0&&!(s->st_mode&0022)&&(!exact0700||((s->st_mode&07777)==0700));
}
static int protected_file_meta(const struct stat *s) {
    return S_ISREG(s->st_mode)&&s->st_uid==0&&s->st_nlink==1&&!(s->st_mode&0022);
}
static int open_dir_at(int fd,const char *name,int create,int protect,int exact0700,mode_t mode) {
    int created=0;
    if(create){if(!mkdirat(fd,name,mode))created=1;else if(errno!=EEXIST)return -1;}
    int n=openat(fd,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(n<0)return -1;
    struct stat s;
    if(fstat(n,&s)||!S_ISDIR(s.st_mode)||(protect&&!protected_dir_meta(&s,exact0700))){close(n);errno=EPERM;return -1;}
    if(created&&fchmod(n,mode)){int e=errno;close(n);errno=e;return -1;}
    return n;
}
static int existing_ok(int dir,const char *name) {
    struct stat s;if(fstatat(dir,name,&s,AT_SYMLINK_NOFOLLOW))return errno==ENOENT?0:-1;
    if(!protected_file_meta(&s)){errno=EPERM;return -1;}return 0;
}
static int atomic_write(int dir,const char *name,const void *data,size_t n,mode_t mode) {
    if(existing_ok(dir,name))return -1;
    char temp[96];int z=snprintf(temp,sizeof(temp),".%s.tmp.%ld",name,(long)getpid());
    if(z<0||(size_t)z>=sizeof(temp)){errno=EINVAL;return -1;}
    int fd=openat(dir,temp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return -1;
    int good=write_all(fd,data,n)==0&&fchmod(fd,mode)==0&&fsync(fd)==0;int e=errno;
    if(close(fd)&&good){good=0;e=errno;}
    if(good&&existing_ok(dir,name)){good=0;e=errno;}
    if(good&&renameat(dir,temp,dir,name)){good=0;e=errno;}
    if(!good){unlinkat(dir,temp,0);errno=e;return -1;}
    return fsync(dir);
}
static int image_hash(const struct image *im,size_t size,const unsigned char hash[32]) {
    unsigned char h[32];if(im->size!=size)return 0;sha256(im->data,im->size,h);return !memcmp(h,hash,32);
}
static int load_at(int dir,const char *name,size_t exact,size_t max,const unsigned char *hash,int readonly,struct image *im) {
    int f=openat(dir,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(f<0)return -1;
    struct stat a,b;struct statvfs v;
    if(fstat(f,&a)||!protected_file_meta(&a)||a.st_size<=0||(uint64_t)a.st_size>max||(exact&&(uint64_t)a.st_size!=exact))goto bad;
    if(readonly&&(fstatvfs(f,&v)||!(v.f_flag&ST_RDONLY)))goto bad;
    im->data=calloc((size_t)a.st_size+1,1);if(!im->data){close(f);return -1;}im->size=(size_t)a.st_size;
    size_t at=0;while(at<im->size){ssize_t n=read(f,im->data+at,im->size-at);if(n<0&&errno==EINTR)continue;if(n<=0)goto fail;at+=(size_t)n;}
    if(fstat(f,&b)||a.st_dev!=b.st_dev||a.st_ino!=b.st_ino||a.st_size!=b.st_size||a.st_mtim.tv_sec!=b.st_mtim.tv_sec||a.st_mtim.tv_nsec!=b.st_mtim.tv_nsec)goto fail;
    if(hash&&!image_hash(im,exact,hash))goto fail;
    close(f);return 0;
fail: free(im->data);im->data=NULL;im->size=0;
bad: close(f);errno=EKEYREJECTED;return -1;
}
static int same_device(const struct stat *data,const struct stat *root,const struct stat *block) {
    return S_ISDIR(data->st_mode)&&S_ISDIR(root->st_mode)&&S_ISBLK(block->st_mode)&&data->st_dev==block->st_rdev&&data->st_dev!=root->st_dev;
}
static int data_dir(int *root_out) {
    int top=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(top<0)return -1;
    int android=open_dir_at(top,"android",0,1,0,0);
    int dev=open_dir_at(top,"dev",0,1,0,0);close(top);
    if(android<0||dev<0){if(android>=0)close(android);if(dev>=0)close(dev);return -1;}
    int blocks=open_dir_at(dev,"block",0,1,0,0);close(dev);
    if(blocks<0){close(android);return -1;}
    struct stat d,r,b;int data=open_dir_at(android,"data",0,0,0,0);
    int good=data>=0&&!fstat(data,&d)&&!fstat(android,&r)&&!fstatat(blocks,"sdc3",&b,AT_SYMLINK_NOFOLLOW)&&same_device(&d,&r,&b);
    close(blocks);if(!good){if(data>=0)close(data);close(android);errno=EXDEV;return -1;}
    *root_out=android;return data;
}
static int report_dir(int data,const char *boot) {
    int local=open_dir_at(data,"local",0,0,0,0);if(local<0)return -1;
    int tmp=open_dir_at(local,"tmp",0,0,0,0);close(local);if(tmp<0)return -1;
    int own=open_dir_at(tmp,"resukisu-migration-prestage",1,1,0,0755);close(tmp);if(own<0)return -1;
    int run=open_dir_at(own,boot,1,1,0,0755);close(own);return run;
}
static int loader_report_ok(const struct image *im,const char *boot) {
    if(im->size>2048||im->size<200||memchr(im->data,0,im->size))return 0;
    /* This is fixed loader output, never arbitrary app logs. Check its schema and boot. */
    const char *b=(const char *)im->data;char marker[64];snprintf(marker,sizeof(marker),"\"bootId\":\"%s\"",boot);
    const char prefix[]="{\"schemaVersion\":1,\"stage\":";
    if(strncmp(b,prefix,sizeof(prefix)-1)||!strstr(b,marker)||!strstr(b,"\"pid\":1,")||!strstr(b,"\"symbolCount\":")||im->data[im->size-1]!='\n')return 0;
    for(size_t i=0;i<im->size;i++)if(im->data[i]<32&&im->data[i]!='\n')return 0;
    return 1;
}

struct removal {int dir,pin,present;const char *name;struct stat identity;};
static int exact_file_meta(const struct stat *s){return protected_file_meta(s)&&s->st_gid==0&&(s->st_mode&07777)==0755;}
static int same_identity(const struct stat *a,const struct stat *b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&a->st_uid==b->st_uid&&a->st_gid==b->st_gid&&a->st_mode==b->st_mode&&a->st_nlink==b->st_nlink&&a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;}
static int elf64_image(const struct image *im){
    if(im->size<sizeof(Elf64_Ehdr)||im->size>64U*1024U*1024U)return 0;Elf64_Ehdr h;memcpy(&h,im->data,sizeof(h));
    return !memcmp(h.e_ident,ELFMAG,SELFMAG)&&h.e_ident[EI_CLASS]==ELFCLASS64&&h.e_ident[EI_DATA]==ELFDATA2LSB&&h.e_machine==EM_X86_64&&h.e_type==ET_DYN&&h.e_version==EV_CURRENT&&h.e_ehsize==sizeof(Elf64_Ehdr);
}
static int catalog_match(const struct image *im){
    if(!elf64_image(im))return -1;unsigned char hash[32];sha256(im->data,im->size,hash);
    for(unsigned i=0;i<CATALOG_COUNT;i++)if(catalog[i].uapi==6&&catalog[i].size==im->size&&!memcmp(catalog[i].hash,hash,32))return (int)i;return -1;
}
static int daemon_existing(int dir,int *present,int *index,struct stat *identity){
    *present=0;*index=-1;struct stat a,b;if(fstatat(dir,"ksud",&a,AT_SYMLINK_NOFOLLOW))return errno==ENOENT?0:-1;*present=1;
    if(!exact_file_meta(&a)){errno=EPERM;return -1;}struct image im={0};
    if(load_at(dir,"ksud",0,64U*1024U*1024U,NULL,0,&im))return -1;int found=catalog_match(&im);free(im.data);
    if(fstatat(dir,"ksud",&b,AT_SYMLINK_NOFOLLOW)||!same_identity(&a,&b)){errno=ESTALE;return -1;}
    *identity=a;if(found<0){errno=EKEYREJECTED;return -1;}*index=found;return 0;
}
static int target_marker(int dir,const char *name,int pin,int *present,struct stat *identity){
    *present=0;if(dir<0)return 0;struct stat a,b;if(fstatat(dir,name,&a,AT_SYMLINK_NOFOLLOW))return errno==ENOENT?0:-1;
    if(!exact_file_meta(&a)){errno=EPERM;return -1;}struct image im={0};if(load_at(dir,name,pins[pin].size,pins[pin].size,pins[pin].hash,0,&im))return -1;free(im.data);
    if(fstatat(dir,name,&b,AT_SYMLINK_NOFOLLOW)||!same_identity(&a,&b)){errno=ESTALE;return -1;}*present=1;*identity=a;return 0;
}
static int hook_directory(int adb,const char *name,int *out){
    *out=-1;struct stat a,b;if(fstatat(adb,name,&a,AT_SYMLINK_NOFOLLOW))return errno==ENOENT?0:-1;
    if(!protected_dir_meta(&a,0)||a.st_gid!=0){errno=EPERM;return -1;}int fd=open_dir_at(adb,name,0,1,0,0);if(fd<0)return -1;
    if(fstat(fd,&b)||a.st_dev!=b.st_dev||a.st_ino!=b.st_ino){close(fd);errno=ESTALE;return -1;}*out=fd;return 0;
}
static int validate_removals(struct removal list[4],int again){
    for(int i=0;i<4;i++){int present;struct stat now;if(target_marker(list[i].dir,list[i].name,list[i].pin,&present,&now))return -1;
        if(again){if(present!=list[i].present||(present&&!same_identity(&now,&list[i].identity))){errno=ESTALE;return -1;}}
        else{list[i].present=present;if(present)list[i].identity=now;}}
    return 0;
}
static int remove_own(struct removal *r,int *did){
    *did=0;if(!r->present)return 0;struct stat now;if(fstatat(r->dir,r->name,&now,AT_SYMLINK_NOFOLLOW)||!same_identity(&r->identity,&now)||!exact_file_meta(&now)){errno=ESTALE;return -1;}
    if(unlinkat(r->dir,r->name,0))return -1;*did=1;return fsync(r->dir);
}
static int seed_missing(int adb,const struct image *im,int *published){
    *published=0;char name[96];int n=snprintf(name,sizeof(name),".ksud.seed.tmp.%ld",(long)getpid());if(n<0||(size_t)n>=sizeof(name)){errno=EINVAL;return -1;}
    int fd=openat(adb,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return -1;
    int ok=!write_all(fd,im->data,im->size)&&!fchmod(fd,0755)&&!fsync(fd);int err=errno;if(close(fd)&&ok){ok=0;err=errno;}
    if(ok&&syscall(SYS_renameat2,adb,name,adb,"ksud",1U)){ok=0;err=errno;}
    if(!ok){unlinkat(adb,name,0);errno=err;return -1;}*published=1;return fsync(adb);
}
static int write_report(int dir,const struct result *r){
    const char *hash=r->index>=0&&r->index<(int)CATALOG_COUNT?catalog[r->index].hex:"";char b[2048];
    int n=snprintf(b,sizeof(b),"{\"schemaVersion\":1,\"bootId\":\"%s\",\"stage\":\"%s\",\"errno\":%d,\"complete\":%s,\"dataMountVerified\":true,\"inputsPinned\":%s,\"cleanupVerified\":%s,\"daemonAction\":\"%s\",\"daemonSupported\":%s,\"daemonCatalogIndex\":%d,\"daemonUapiVersion\":%d,\"daemonSha256\":\"%s\",\"seededFiles\":%d,\"daemonPreservedUnchanged\":%s,\"daemonInodeBefore\":%" PRIuMAX ",\"daemonInodeAfter\":%" PRIuMAX ",\"daemonMtimeBefore\":%" PRIdMAX ",\"daemonMtimeAfter\":%" PRIdMAX ",\"removedObserverFiles\":%d,\"alreadyAbsentObserverFiles\":%d,\"unrelatedScriptsEnumerated\":false,\"unrelatedFilesDeleted\":false,\"daemonExecutedByHelper\":false,\"normalRcVerifiedByThisHelper\":false}\n",
        r->boot,r->stage,r->error,!strcmp(r->stage,"completed")?"true":"false",r->pinned?"true":"false",r->cleanup_verified?"true":"false",r->action,r->supported?"true":"false",r->index,r->supported?6:0,hash,r->seeded,r->preserved?"true":"false",(uintmax_t)r->before.st_ino,(uintmax_t)r->after.st_ino,(intmax_t)r->before.st_mtim.tv_sec,(intmax_t)r->after.st_mtim.tv_sec,r->removed,r->absent);
    if(n<0||(size_t)n>=sizeof(b)){errno=EOVERFLOW;return -1;}return atomic_write(dir,"prestage.json",b,(size_t)n,0644);
}
int main(int argc,char **argv){
    (void)argv;if(argc!=1||getuid()!=0||geteuid()!=0||getgid()!=0||getegid()!=0||getppid()!=1)return 64;umask(0077);
    struct result r={.stage="mount_validation",.action="not_checked",.index=-1};if(get_boot(r.boot))return 65;int android=-1,data=data_dir(&android);if(data<0)return 66;
    int report=report_dir(data,r.boot);if(report<0){close(data);close(android);return 67;}
    int top=-1,system=-1,xbin=-1,adb=-1,dirs[3]={-1,-1,-1},rc=1,present=0;struct image seed={0},loader={0};
    struct removal removals[4]={{.dir=-1,.pin=0,.name="resukisu-stage"},{.dir=-1,.pin=1,.name="00-resukisu-validation.sh"},{.dir=-1,.pin=2,.name="00-resukisu-validation.sh"},{.dir=-1,.pin=3,.name="00-resukisu-validation.sh"}};
    top=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(top<0)goto out;r.stage="loader_report_validation";
    if(load_at(top,"userspace-driver-boot.json",0,2048,NULL,0,&loader))goto out;if(!loader_report_ok(&loader,r.boot)){errno=EINVAL;goto out;}if(atomic_write(report,"userspace-driver-boot.json",loader.data,loader.size,0644))goto out;
    r.stage="destination_validation";adb=open_dir_at(data,"adb",1,1,1,0700);if(adb<0)goto out;struct stat adb_meta;if(fstat(adb,&adb_meta)||adb_meta.st_gid!=0){errno=EPERM;goto out;}
    r.stage="existing_daemon_validation";if(daemon_existing(adb,&present,&r.index,&r.before)){r.action="rejected_unknown";goto out;}
    if(present){r.action="preserved_supported";r.supported=1;r.pinned=1;}
    else{
        r.stage="seed_source_validation";system=open_dir_at(android,"system",0,1,0,0);if(system<0)goto out;xbin=open_dir_at(system,"xbin",0,1,0,0);if(xbin<0)goto out;
        if(load_at(xbin,"dusk",DAEMON_BYTES,DAEMON_BYTES,daemon_sha,1,&seed))goto out;if(!elf64_image(&seed)){errno=ENOEXEC;goto out;}r.index=catalog_match(&seed);if(r.index<0){errno=EKEYREJECTED;goto out;}r.pinned=1;
    }
    r.stage="observer_validation";const char *paths[3]={"post-mount.d","service.d","boot-completed.d"};for(int i=0;i<3;i++){if(hook_directory(adb,paths[i],&dirs[i]))goto out;removals[i+1].dir=dirs[i];}removals[0].dir=adb;
    if(validate_removals(removals,0))goto out;r.cleanup_verified=1;for(int i=0;i<4;i++)if(!removals[i].present)r.absent++;
    if(!present){r.stage="seed_missing_daemon";int seed_rc=seed_missing(adb,&seed,&r.seeded);if(seed_rc)goto out;r.action="seeded_missing";}
    r.stage="daemon_revalidation";int now_present,now_index;if(daemon_existing(adb,&now_present,&now_index,&r.after)||!now_present||now_index!=r.index){errno=EKEYREJECTED;goto out;}r.supported=1;
    if(present){if(!same_identity(&r.before,&r.after)){errno=ESTALE;goto out;}r.preserved=1;}
    r.stage="observer_revalidation";if(validate_removals(removals,1))goto out;
    r.stage="observer_cleanup";for(int i=0;i<4;i++){int did;int remove_rc=remove_own(&removals[i],&did);r.removed+=did;if(remove_rc)goto out;}
    r.stage="completed";rc=0;
out:
    r.error=rc?errno:0;if(write_report(report,&r))rc=68;free(seed.data);free(loader.data);for(int i=0;i<3;i++)if(dirs[i]>=0)close(dirs[i]);if(adb>=0)close(adb);if(xbin>=0)close(xbin);if(system>=0)close(system);if(top>=0)close(top);close(report);close(data);close(android);return rc;
}
