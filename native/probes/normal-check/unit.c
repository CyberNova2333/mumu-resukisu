/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main check_main
#include "check.c"
#undef main
#include <assert.h>
int main(int argc,char **argv){
    assert(argc==2&&getuid()!=0);assert(valid_boot("12345678-1234-1234-1234-123456789abc"));assert(!valid_boot("12345678-1234-1234-1234-123456789abG"));
    char line[160];
    snprintf(line,sizeof(line),"com.resukisu.resukisu %u 0 ignored\n",EXPECTED_UID);assert(package_line(line)==1);
    snprintf(line,sizeof(line),"other.package %u 0 ignored\n",EXPECTED_UID);assert(package_line(line)==0);
    snprintf(line,sizeof(line),"com.resukisu.resukisu.extra %u 0 ignored\n",EXPECTED_UID);assert(package_line(line)==0);
    snprintf(line,sizeof(line),"com.resukisu.resukisu %u 0 ignored\n",EXPECTED_UID+1);assert(package_line(line)==-1);
    snprintf(line,sizeof(line),"com.resukisu.resukisu %ux 0 ignored\n",EXPECTED_UID);assert(package_line(line)==-1);
    assert(package_line("com.resukisu.resukisu -1 ignored\n")==-1);

    struct managers m={.count=32,.total_count=1,.entries={{EXPECTED_UID,0}}};assert(manager_list_match(&m)==1);m.entries[0].signature_index=1;assert(manager_list_match(&m)==0);m.entries[0].signature_index=0;m.entries[0].uid=EXPECTED_UID+1;assert(manager_list_match(&m)==0);m.entries[0].uid=EXPECTED_UID;m.total_count=2;m.entries[1]=m.entries[0];assert(manager_list_match(&m)==0);m.total_count=33;assert(manager_list_match(&m)==-1);m.count=0;m.total_count=0;assert(manager_list_match(&m)==-1);
    FILE *f=fopen(argv[1],"rb");assert(f);unsigned char *p=malloc(EXPECTED_BYTES);assert(p);assert(fread(p,1,EXPECTED_BYTES,f)==EXPECTED_BYTES);fclose(f);assert(image_match(p,EXPECTED_BYTES));assert(!image_match(p,EXPECTED_BYTES-1));p[100]^=1;assert(!image_match(p,EXPECTED_BYTES));free(p);
    struct stat s={.st_dev=1,.st_ino=2,.st_size=3,.st_uid=0,.st_gid=0,.st_mode=S_IFREG|0755,.st_nlink=1},t=s;assert(stable(&s,&t));t.st_ctim.tv_nsec=1;assert(!stable(&s,&t));
    char temp[]="/tmp/resukisu-normal-check-unit-XXXXXX";assert(mkdtemp(temp));int dir=open(temp,O_RDONLY|O_DIRECTORY|O_CLOEXEC);assert(dir>=0);assert(absent_at(dir,"marker")==1);assert(marker_absent(dir,"missing-directory")==1);assert(!symlinkat("/missing",dir,"marker"));assert(absent_at(dir,"marker")==0);assert(marker_absent(dir,"marker")==-1);assert(!unlinkat(dir,"marker",0));close(dir);assert(!rmdir(temp));
    struct result r={.observer_absent={1,1,1,1}};assert(all_absent(&r));r.observer_absent[2]=0;assert(!all_absent(&r));r.observer_absent[2]=-1;assert(!all_absent(&r));
    puts("PASS: fixed package parser, manager signature/UID bounds and duplicates, full daemon hash/ELF with mutation and truncation, stable metadata, actual dangling-symlink presence and unknown-parent rejection; no device or KSU requests");return 0;
}
