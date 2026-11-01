/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Fixtures stay owned by the ordinary host user. Only their returned uid/gid
 * metadata is modeled as root for exercising the privileged branch offline. */
#define _GNU_SOURCE
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
static struct {dev_t dev;ino_t ino;} fixtures[8];static int fixture_count;
static void model_owner(struct stat *s){for(int i=0;i<fixture_count;i++)if(s->st_dev==fixtures[i].dev&&s->st_ino==fixtures[i].ino){s->st_uid=0;s->st_gid=0;}}
static int fixture_fstat(int fd,struct stat *s){int rc=fstat(fd,s);if(!rc)model_owner(s);return rc;}
static int fixture_fstatat(int fd,const char *n,struct stat *s,int flags){int rc=fstatat(fd,n,s,flags);if(!rc)model_owner(s);return rc;}
#define fstat fixture_fstat
#define fstatat fixture_fstatat
#define main migration_main
#include "prepare.c"
#undef main
#undef fstat
#undef fstatat
static void register_fixture(int dir,const char *name){assert(fixture_count<8);struct stat s;assert(!fstatat(dir,name,&s,AT_SYMLINK_NOFOLLOW));fixtures[fixture_count].dev=s.st_dev;fixtures[fixture_count++].ino=s.st_ino;}
static void save_file(int dir,const char *name,const void *body,size_t n){int fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL,0755);assert(fd>=0);assert(!write_all(fd,body,n));assert(!fchmod(fd,0755));assert(!close(fd));}
static void file_same(int dir,const char *name,const struct stat *before,const void *contents,size_t len){struct stat after;assert(!fstatat(dir,name,&after,AT_SYMLINK_NOFOLLOW)&&same_identity(before,&after));int fd=openat(dir,name,O_RDONLY|O_NOFOLLOW);assert(fd>=0);unsigned char *p=malloc(len);assert(p&&read(fd,p,len)==(ssize_t)len&&!memcmp(p,contents,len));free(p);close(fd);}
int main(int argc,char **argv){
    assert(argc==2&&getuid()!=0);assert(migration_main(1,argv)==64);assert(CATALOG_COUNT>=1);
    FILE *f=fopen(argv[1],"rb");assert(f);struct image official={malloc(DAEMON_BYTES),DAEMON_BYTES};assert(official.data&&fread(official.data,1,official.size,f)==official.size);fclose(f);int baseline_index=catalog_match(&official);assert(elf64_image(&official)&&baseline_index>=0&&catalog[baseline_index].uapi==6);official.data[300]^=1;assert(elf64_image(&official)&&catalog_match(&official)==-1);official.data[300]^=1;
    char temp[]="/tmp/resukisu-migration-unit-XXXXXX";assert(mkdtemp(temp));int dir=open(temp,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(dir>=0);
    int present,index;struct stat identity;assert(!daemon_existing(dir,&present,&index,&identity)&&!present&&index==-1);
    int published=99;assert(!seed_missing(dir,&official,&published)&&published==1);register_fixture(dir,"ksud");assert(!daemon_existing(dir,&present,&index,&identity)&&present&&index==baseline_index);
    struct stat before;assert(!fstatat(dir,"ksud",&before,AT_SYMLINK_NOFOLLOW));assert(!daemon_existing(dir,&present,&index,&identity));file_same(dir,"ksud",&before,official.data,official.size);
    assert(seed_missing(dir,&official,&published)<0&&errno==EEXIST&&published==0);file_same(dir,"ksud",&before,official.data,official.size);assert(!unlinkat(dir,"ksud",0));
    official.data[300]^=1;save_file(dir,"ksud",official.data,official.size);register_fixture(dir,"ksud");assert(!fstatat(dir,"ksud",&before,AT_SYMLINK_NOFOLLOW));assert(daemon_existing(dir,&present,&index,&identity)<0&&present&&index==-1&&errno==EKEYREJECTED);file_same(dir,"ksud",&before,official.data,official.size);
    assert(seed_missing(dir,&official,&published)<0&&errno==EEXIST&&published==0);file_same(dir,"ksud",&before,official.data,official.size);assert(!unlinkat(dir,"ksud",0));official.data[300]^=1;
    assert(!mkdirat(dir,"service.d",0755));register_fixture(dir,"service.d");int hooks=-1;assert(!hook_directory(dir,"service.d",&hooks)&&hooks>=0);save_file(hooks,"user-owned-script.sh","#!/system/bin/sh\nexit 0\n",24);assert(!fstatat(hooks,"user-owned-script.sh",&before,AT_SYMLINK_NOFOLLOW));
    struct removal rm[4]={{.dir=dir,.pin=0,.name="resukisu-stage"},{.dir=-1,.pin=1,.name="00-resukisu-validation.sh"},{.dir=hooks,.pin=2,.name="00-resukisu-validation.sh"},{.dir=-1,.pin=3,.name="00-resukisu-validation.sh"}};assert(!validate_removals(rm,0)&&!validate_removals(rm,1));for(int i=0;i<4;i++){int did=9;assert(!remove_own(&rm[i],&did)&&did==0);}file_same(hooks,"user-owned-script.sh",&before,"#!/system/bin/sh\nexit 0\n",24);
    save_file(hooks,"00-resukisu-validation.sh","foreign",7);register_fixture(hooks,"00-resukisu-validation.sh");assert(validate_removals(rm,0)<0);struct stat foreign;assert(!fstatat(hooks,"00-resukisu-validation.sh",&foreign,AT_SYMLINK_NOFOLLOW));file_same(hooks,"00-resukisu-validation.sh",&foreign,"foreign",7);
    assert(!unlinkat(hooks,"00-resukisu-validation.sh",0));assert(!unlinkat(hooks,"user-owned-script.sh",0));close(hooks);assert(!unlinkat(dir,"service.d",AT_REMOVEDIR));close(dir);assert(!rmdir(temp));free(official.data);
    puts("PASS: synthetic root metadata only; actual host files verify absent-only seed, supported inode/mtime/content preserved, unknown valid ELF rejected unchanged, RENAME_NOREPLACE collision preserves existing file, unrelated user script retained without execution, exact-name collision retained; no device or root execution");return 0;
}
