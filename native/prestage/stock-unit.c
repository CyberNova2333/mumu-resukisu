/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Real temporary files, ordinary host uid; only stat ownership returned for
 * this exact fixture tree is modeled as root. No Android or root execution. */
#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
static char fixture_root[PATH_MAX];
static int fixture_fd(int fd){
    char proc[64],path[PATH_MAX];snprintf(proc,sizeof(proc),"/proc/self/fd/%d",fd);
    ssize_t n=readlink(proc,path,sizeof(path)-1);if(n<0)return 0;path[n]=0;
    size_t length=strlen(fixture_root);return length&&(!strcmp(path,fixture_root)||(!strncmp(path,fixture_root,length)&&path[length]=='/'));
}
static void model_owner(int fd,struct stat *s){if(fixture_fd(fd)&&s->st_uid==getuid()&&s->st_gid==getgid()){s->st_uid=0;s->st_gid=0;}}
static int fixture_fstat(int fd,struct stat *s){int rc=fstat(fd,s);if(!rc)model_owner(fd,s);return rc;}
static int fixture_fstatat(int fd,const char *name,struct stat *s,int flags){int rc=fstatat(fd,name,s,flags);if(!rc)model_owner(fd,s);return rc;}
static const char *fail_point;static int fault_mode,fault_dir;
static int fault(const char *point);
#define STOCK_FAULT_POINT(name) fault(name)
#define fstat fixture_fstat
#define fstatat fixture_fstatat
#define main prepare_main
#include "prepare.c"
#undef main
#undef fstat
#undef fstatat

static int fault(const char *point){
    if(!fail_point||strcmp(point,fail_point))return 0;fail_point=NULL;
    if(fault_mode==2){int f=openat(fault_dir,"ksud",O_WRONLY|O_NOFOLLOW);assert(f>=0);unsigned char byte=0x41;assert(pwrite(f,&byte,1,300)==1);close(f);return 0;}
    if(fault_mode==3){int f=openat(fault_dir,"ksud",O_WRONLY|O_CREAT|O_EXCL,0755);assert(f>=0);assert(write(f,"foreign",7)==7);close(f);return 0;}
    errno=EIO;return -1;
}
static struct image input(const char *path){
    FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>0);rewind(f);
    struct image im={malloc((size_t)n),(size_t)n};assert(im.data&&fread(im.data,1,im.size,f)==im.size);fclose(f);return im;
}
static void save(int dir,const char *name,const struct image *im){
    int f=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0755);assert(f>=0);assert(!write_all(f,im->data,im->size));assert(!fchmod(f,0755));assert(!fsync(f));close(f);
}
static void cleanup_tree(int dir){
    DIR *d=fdopendir(dup(dir));assert(d);struct dirent *e;
    while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;struct stat s;assert(!fstatat(dir,e->d_name,&s,AT_SYMLINK_NOFOLLOW));
        if(S_ISDIR(s.st_mode)){int sub=openat(dir,e->d_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);assert(sub>=0);cleanup_tree(sub);close(sub);assert(!unlinkat(dir,e->d_name,AT_REMOVEDIR));}
        else assert(!unlinkat(dir,e->d_name,0));}
    closedir(d);
}
static int begin(const struct image *stock){
    strcpy(fixture_root,"/tmp/resukisu-stock-unit-XXXXXX");assert(mkdtemp(fixture_root));
    int dir=open(fixture_root,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);assert(dir>=0);save(dir,"ksud",stock);fault_dir=dir;fail_point=NULL;return dir;
}
static void end(int dir){cleanup_tree(dir);close(dir);assert(!rmdir(fixture_root));fixture_root[0]=0;}
static struct result last_result;
static int run(int dir,const struct image *seed){
    struct result r={.index=-1};int present,index;if(daemon_existing(dir,&present,&index,&r.before))return -1;
    int rc=migrate_stock(dir,seed,present,index,&r);last_result=r;return rc;
}
static void complete(int dir,const struct image *seed){
    int present,index;struct stat a,b;assert(!daemon_existing(dir,&present,&index,&a)&&present&&index==catalog_match(seed));
    int state=stock_dir(dir,0);assert(state>=0&&stock_validate(state,STOCK_BACKUP,&b)==1);
    assert(stock_validate(state,STOCK_RETIRED,&b)==0);close(state);
    assert(run(dir,seed)==1&&last_result.stock_resumed==0&&!strcmp(last_result.action,"preserved_supported"));assert(!daemon_existing(dir,&present,&index,&b)&&same_identity(&a,&b));
}
int main(int argc,char **argv){
    assert(argc==3&&getuid()!=0);struct image seed=input(argv[1]),stock=input(argv[2]);assert(catalog_match(&seed)>=0&&stock_image(&stock)&&catalog_match(&stock)==-1);
    const char *points[]={"directory_durable","backup_published","backup_durable","before_retire","retired_durable","before_seed","seed_published","seed_durable","before_retired_cleanup","migration_completed"};
    for(size_t i=0;i<sizeof(points)/sizeof(*points);i++){
        int dir=begin(&stock);fail_point=points[i];fault_mode=1;assert(run(dir,&seed)<0&&fail_point==NULL);
        assert(run(dir,&seed)==1);complete(dir,&seed);end(dir);
    }
    /* Existing backup remains byte-identical; a restored stock system may
     * have recreated live ksud while a prior retired copy still exists. */
    int dir=begin(&stock);fail_point="retired_durable";fault_mode=1;assert(run(dir,&seed)<0);save(dir,"ksud",&stock);assert(run(dir,&seed)==1);complete(dir,&seed);end(dir);
    /* Unknown backup, symlink and hardlink collisions are never overwritten. */
    for(int mode=0;mode<3;mode++){
        dir=begin(&stock);int state=stock_dir(dir,1);assert(state>=0);struct image foreign={(unsigned char *)"foreign",7};
        if(mode==0)save(state,STOCK_BACKUP,&foreign);
        if(mode==1)assert(!symlinkat("../ksud",state,STOCK_BACKUP));
        if(mode==2)assert(!linkat(dir,"ksud",state,STOCK_BACKUP,0));
        struct stat before,after;assert(!fstatat(state,STOCK_BACKUP,&before,AT_SYMLINK_NOFOLLOW));
        assert(run(dir,&seed)<0);assert(!fstatat(state,STOCK_BACKUP,&after,AT_SYMLINK_NOFOLLOW)&&same_identity(&before,&after));
        struct image live={0};int f=openat(dir,"ksud",O_RDONLY);assert(f>=0);live.data=malloc(stock.size);live.size=stock.size;assert(read(f,live.data,live.size)==(ssize_t)live.size&&!memcmp(live.data,stock.data,stock.size));free(live.data);close(f);close(state);end(dir);
    }
    /* Unknown retired file refuses migration before live is moved. */
    dir=begin(&stock);int state=stock_dir(dir,1);assert(state>=0);save(state,STOCK_BACKUP,&stock);struct image foreign={(unsigned char *)"foreign",7};save(state,STOCK_RETIRED,&foreign);assert(run(dir,&seed)<0);struct stat identity;assert(stock_validate(dir,"ksud",&identity)==1);close(state);end(dir);
    /* Revalidation detects source modification and does not replace it. */
    dir=begin(&stock);fail_point="before_retire";fault_mode=2;assert(run(dir,&seed)<0);int present,index;assert(daemon_existing(dir,&present,&index,&identity)<0&&present&&observed_daemon.sha[0]);end(dir);
    /* A new live pathname after retirement is preserved by NOREPLACE. */
    dir=begin(&stock);fail_point="before_seed";fault_mode=3;assert(run(dir,&seed)<0&&errno==EEXIST);int f=openat(dir,"ksud",O_RDONLY);char text[8]={0};assert(f>=0&&read(f,text,7)==7&&!strcmp(text,"foreign"));close(f);state=stock_dir(dir,0);assert(stock_validate(state,STOCK_BACKUP,&identity)==1&&stock_validate(state,STOCK_RETIRED,&identity)==1);close(state);end(dir);
    /* A stale interrupted temporary file is retained and cannot block retry. */
    dir=begin(&stock);state=stock_dir(dir,1);save(state,".publish-stale",&foreign);assert(run(dir,&seed)==1);complete(dir,&seed);assert(!fstatat(state,".publish-stale",&identity,AT_SYMLINK_NOFOLLOW)&&identity.st_size==7);close(state);end(dir);
    free(seed.data);free(stock.data);puts("PASS: 18 stock migration real-file cases; durable-boundary retry, preserved backup, exact stock only, symlink/hardlink/unknown collisions, source race, live collision and stale temporary preservation; no device execution");return 0;
}
