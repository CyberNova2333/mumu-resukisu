/* SPDX-License-Identifier: GPL-2.0-only */
#include <ext2fs/ext2fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

static void ck(errcode_t e, const char *s) { if(e) { fprintf(stderr,"%s: %ld\n",s,(long)e); exit(2); } }
static unsigned *owners; static unsigned collision, target, current;
static blk64_t *all; static size_t nall, capall;
static int blocks(ext2_filsys fs, blk64_t *b, e2_blkcnt_t bc, blk64_t rb, int ro, void *x) {
 (void)fs;(void)bc;(void)rb;(void)ro;(void)x;
 if(!*b) return 0;
 if(*b>=421544) {fprintf(stderr,"block out of bounds\n");exit(3);}
 if(owners) { if(owners[*b] && owners[*b]!=current && (current==target || owners[*b]==target)) collision++; if(!owners[*b] || current==target) owners[*b]=current; }
 if(nall==capall) {capall=capall?capall*2:2048;all=realloc(all,capall*sizeof(*all));if(!all)exit(4);}
 all[nall++]=*b; return 0;
}
static void geometry(ext2_filsys fs) {
 if(fs->blocksize!=4096 || fs->super->s_inodes_count!=4160 || ext2fs_blocks_count(fs->super)!=421544 || EXT2_INODE_SIZE(fs->super)!=256) {fprintf(stderr,"unexpected FS geometry\n");exit(5);}
 ck(ext2fs_namei(fs,EXT2_ROOT_INO,EXT2_ROOT_INO,"/system/xbin/dusk",&target),"find dusk");
 if(target!=3980)exit(6);
}
static void scan(ext2_filsys fs,int emit) {
 ext2_inode_scan s; ext2_ino_t ino; unsigned char raw[256]; unsigned allocated=0;
 owners=calloc(421544,sizeof(*owners));if(!owners)exit(7);
 ck(ext2fs_open_inode_scan(fs,0,&s),"scan open");
 if(emit)puts("{\"inodes\":[");
 for(;;) {
  ck(ext2fs_get_next_inode_full(s,&ino,(struct ext2_inode *)raw,sizeof(raw)),"next inode");if(!ino)break;
  if(!ext2fs_test_inode_bitmap2(fs->inode_map,ino))continue;
  struct ext2_inode *i=(void *)raw; current=ino;nall=0;
  if(ext2fs_inode_has_valid_blocks2(fs,i)) ck(ext2fs_block_iterate3(fs,ino,BLOCK_FLAG_READ_ONLY,NULL,blocks,NULL),"walk blocks");
  if(i->i_file_acl) {blk64_t b=i->i_file_acl;blocks(fs,&b,-9,0,0,NULL);}
  if(emit){
   printf("%s{\"inode\":%u,\"raw\":\"",allocated?",\n":"",ino);for(unsigned z=0;z<sizeof(raw);z++)printf("%02x",raw[z]);
   printf("\",\"blocks\":[");for(size_t z=0;z<nall;z++)printf("%s%llu",z?",":"",(unsigned long long)all[z]);puts("]}");
  }
  allocated++;
 }
 ext2fs_close_inode_scan(s);
 if(emit)printf("],\"allocatedInodes\":%u,\"targetBlockCollisions\":%u}\n",allocated,collision);
 free(owners);owners=NULL;
 if(collision){fprintf(stderr,"dusk block shared with another inode\n");exit(8);}
}
int main(int argc,char **argv) {
 if(argc<3 || (strcmp(argv[1],"scan") && strcmp(argv[1],"replace")) || (strcmp(argv[1],"replace")==0 && argc!=4))return 1;
 int write=!strcmp(argv[1],"replace");ext2_filsys fs;
 struct stat destination;
 if(write && (lstat(argv[2], &destination) || !S_ISREG(destination.st_mode) || destination.st_nlink != 1 || destination.st_size != 1754411008)){fprintf(stderr,"write file rejected\n");return 9;}
 ck(ext2fs_open(argv[2],write?EXT2_FLAG_RW:0,0,0,unix_io_manager,&fs),"open"); geometry(fs);ck(ext2fs_read_bitmaps(fs),"bitmaps");
 scan(fs,!write);
 if(write){
  struct ext2_inode in; ck(ext2fs_read_inode(fs,target,&in),"read dusk inode");
  if(!LINUX_S_ISREG(in.i_mode) || in.i_links_count!=1 || EXT2_I_SIZE(&in)!=3753880 || in.i_flags!=EXT4_EXTENTS_FL){fprintf(stderr,"dusk inode mismatch\n");return 10;}
  FILE *src=fopen(argv[3],"rb");if(!src)return 11;
  struct stat st;if(fstat(fileno(src),&st) || st.st_size!=6001344)return 12;
  ext2_file_t f;ck(ext2fs_file_open(fs,target,EXT2_FILE_WRITE,&f),"file open");
  unsigned char buf[65536];size_t n;unsigned long long total=0;
  while((n=fread(buf,1,sizeof(buf),src))){unsigned done=0;ck(ext2fs_file_write(f,buf,n,&done),"write");if(done!=n)return 13;total+=n;}
  if(ferror(src) || total!=6001344)return 14;
  fclose(src);
  ck(ext2fs_file_set_size2(f,total),"file size");ck(ext2fs_file_close(f),"file close");
 }
 ck(ext2fs_close(fs),"close");return 0;
}
