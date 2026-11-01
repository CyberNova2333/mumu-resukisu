/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included after the shared file helpers. The only recognized predecessor is
 * the pinned stock daemon. No content is executed and no unknown file is removed. */
#ifndef STOCK_FAULT_POINT
#define STOCK_FAULT_POINT(name) 0
#endif
#define STOCK_STATE_DIR "resukisu-stock-migration"
#define STOCK_BACKUP "stock-ksud"
#define STOCK_RETIRED "retired-ksud"

static int stock_renamed_identity(const struct stat *a,const struct stat *b){
    struct stat adjusted=*b;
    /* rename changes ctime, but must preserve all other pinned identity fields. */
    adjusted.st_ctim=a->st_ctim;return same_identity(a,&adjusted);
}
static int stock_load(int dir,const char *name,struct image *im,struct stat *identity){
    struct stat a,b;if(fstatat(dir,name,&a,AT_SYMLINK_NOFOLLOW))return errno==ENOENT?0:-1;
    if(!exact_file_meta(&a)){errno=EPERM;return -1;}
    if(load_at(dir,name,STOCK_DAEMON_BYTES,STOCK_DAEMON_BYTES,stock_daemon_sha,0,im))return -1;
    if(!stock_image(im)||fstatat(dir,name,&b,AT_SYMLINK_NOFOLLOW)||!same_identity(&a,&b)){
        free(im->data);memset(im,0,sizeof(*im));errno=ESTALE;return -1;
    }
    *identity=b;return 1;
}
static int stock_validate(int dir,const char *name,struct stat *identity){
    struct image im={0};int rc=stock_load(dir,name,&im,identity);free(im.data);return rc;
}
static int stock_dir(int adb,int create){
    int created=0;struct stat before,after;
    if(fstatat(adb,STOCK_STATE_DIR,&before,AT_SYMLINK_NOFOLLOW)){
        if(errno!=ENOENT)return -1;if(!create)return -2;
        if(mkdirat(adb,STOCK_STATE_DIR,0700))return -1;created=1;
        if(fsync(adb)||STOCK_FAULT_POINT("directory_durable"))return -1;
        if(fstatat(adb,STOCK_STATE_DIR,&before,AT_SYMLINK_NOFOLLOW))return -1;
    }
    if(!protected_dir_meta(&before,1)||before.st_gid!=0){errno=EPERM;return -1;}
    int dir=openat(adb,STOCK_STATE_DIR,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(dir<0)return -1;
    if(fstat(dir,&after)||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino){close(dir);errno=ESTALE;return -1;}
    if(fsync(adb)||(created&&fsync(dir))){int e=errno;close(dir);errno=e;return -1;}return dir;
}
static int stock_publish(int dir,const char *name,const struct image *im,int *published){
    *published=0;unsigned char random[16];size_t at=0;
    while(at<sizeof(random)){ssize_t n=getrandom(random+at,sizeof(random)-at,GRND_NONBLOCK);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;at+=(size_t)n;}
    char temporary[64]=".publish-";for(int i=0;i<16;i++)snprintf(temporary+9+i*2,3,"%02x",random[i]);
    int fd=openat(dir,temporary,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return -1;
    int ok=!write_all(fd,im->data,im->size)&&!fchmod(fd,0755)&&!fsync(fd);int error=errno;
    struct stat held,path;
    if(ok&&(fstat(fd,&held)||fstatat(dir,temporary,&path,AT_SYMLINK_NOFOLLOW)||!same_identity(&held,&path))){ok=0;error=ESTALE;}
    if(close(fd)&&ok){ok=0;error=errno;}
    if(ok&&syscall(SYS_renameat2,dir,temporary,dir,name,1U)){ok=0;error=errno;}
    if(ok){*published=1;if(STOCK_FAULT_POINT(!strcmp(name,STOCK_BACKUP)?"backup_published":"seed_published"))return -1;return fsync(dir);}
    /* Leave a failed/partial staging file intact. A fresh random name makes
     * retries independent; it is never mistaken for the durable backup. */
    errno=error;return -1;
}
static int stock_backup_durable(int dir){
    struct stat expected,held;if(stock_validate(dir,STOCK_BACKUP,&expected)!=1){errno=EKEYREJECTED;return -1;}
    int fd=openat(dir,STOCK_BACKUP,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;
    int rc=fstat(fd,&held);if(!rc&&!same_identity(&expected,&held)){errno=ESTALE;rc=-1;}
    if(!rc)rc=fsync(fd);int error=errno;close(fd);if(rc){errno=error;return -1;}return fsync(dir);
}
static int stock_remove_retired(int dir,const struct stat *expected){
    if(STOCK_FAULT_POINT("before_retired_cleanup"))return -1;
    if(stock_backup_durable(dir))return -1;
    struct stat now;if(stock_validate(dir,STOCK_RETIRED,&now)!=1||!same_identity(expected,&now)){errno=ESTALE;return -1;}
    if(unlinkat(dir,STOCK_RETIRED,0))return -1;return fsync(dir);
}
static int stock_retire(int adb,int dir,const struct stat *expected,struct result *r){
    if(STOCK_FAULT_POINT("before_retire"))return -1;
    if(stock_backup_durable(dir))return -1;
    struct stat now;if(stock_validate(adb,"ksud",&now)!=1||!same_identity(expected,&now)){errno=ESTALE;return -1;}
    /* Pin the source through the rename and persist its data before moving it. */
    int fd=openat(adb,"ksud",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)return -1;
    struct stat held;int bad=fstat(fd,&held);if(!bad&&!same_identity(expected,&held)){errno=ESTALE;bad=1;}if(!bad)bad=fsync(fd);int e=errno;if(bad){close(fd);errno=e;return -1;}
    if(syscall(SYS_renameat2,adb,"ksud",dir,STOCK_RETIRED,1U)){e=errno;close(fd);errno=e;return -1;}r->stock_retired=1;
    int valid=stock_validate(dir,STOCK_RETIRED,&now);
    e=errno;close(fd);errno=e;
    if(valid!=1||!stock_renamed_identity(expected,&now)){
        /* A competing writer must not cause publication over an unknown
         * file. Restore the moved entry only if its original name is vacant. */
        int cause=ESTALE;
        if(!syscall(SYS_renameat2,dir,STOCK_RETIRED,adb,"ksud",1U)){fsync(adb);fsync(dir);r->stock_retired=0;}
        errno=cause;return -1;
    }
    if(fsync(adb)||fsync(dir)||STOCK_FAULT_POINT("retired_durable"))return -1;return 0;
}

/* Called only after seed identity and every observer have been validated.
 * The immutable stock backup is durable before retiring any live file.
 * Recovery is derived from exact file identities, not a mutable JSON claim. */
static int migrate_stock(int adb,const struct image *seed,int initial_present,int initial_index,struct result *r){
    int dir=stock_dir(adb,initial_present&&initial_index==-2);if(dir==-2)return 0;if(dir<0)return -1;
    struct image old={0};struct stat backup,retired,live;
    int backup_present=stock_validate(dir,STOCK_BACKUP,&backup);
    int retired_present=stock_validate(dir,STOCK_RETIRED,&retired);
    int rc=-1;if(backup_present<0||retired_present<0)goto out;
    if(retired_present&&!backup_present){errno=EKEYREJECTED;goto out;}
    if(initial_present&&initial_index==-2){
        if(stock_load(adb,"ksud",&old,&live)!=1||!same_identity(&r->before,&live)){errno=ESTALE;goto out;}
        if(!backup_present){
            int published=0;if(stock_publish(dir,STOCK_BACKUP,&old,&published))goto out;
            if(STOCK_FAULT_POINT("backup_durable"))goto out;
            if(stock_validate(dir,STOCK_BACKUP,&backup)!=1){errno=EKEYREJECTED;goto out;}
        }
        if(stock_backup_durable(dir))goto out;r->stock_backup=1;
        /* A stock boot may have recopied its daemon after a failed candidate.
         * Discard only the exact redundant retired copy, after backup proof. */
        if(retired_present&&stock_remove_retired(dir,&retired))goto out;
        if(stock_retire(adb,dir,&live,r))goto out;
        if(stock_validate(dir,STOCK_RETIRED,&retired)!=1){errno=EKEYREJECTED;goto out;}
        retired_present=1;
    }else{
        if(!backup_present){if(retired_present){errno=EKEYREJECTED;goto out;}rc=0;goto out;}
        if(stock_backup_durable(dir))goto out;r->stock_backup=1;r->stock_resumed=!initial_present||retired_present;
    }
    int present,index;struct stat current;
    if(daemon_existing(adb,&present,&index,&current))goto out;
    if(!present){
        int published=0;if(!seed->data||catalog_match(seed)<0){errno=EKEYREJECTED;goto out;}
        if(STOCK_FAULT_POINT("before_seed"))goto out;
        if(stock_publish(adb,"ksud",seed,&published)){r->seeded+=published;goto out;}r->seeded+=published;
        if(STOCK_FAULT_POINT("seed_durable"))goto out;
        if(daemon_existing(adb,&present,&index,&current))goto out;
    }
    if(!present||index<0){errno=EKEYREJECTED;goto out;}
    if(retired_present&&stock_remove_retired(dir,&retired))goto out;
    r->action=initial_present&&initial_index==-2?"migrated_stock":r->stock_resumed?"resumed_stock_migration":"preserved_supported";
    if(STOCK_FAULT_POINT("migration_completed"))goto out;rc=1;
out:
    {int e=errno;free(old.data);close(dir);errno=e;}return rc;
}
