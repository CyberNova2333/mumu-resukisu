/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
static const char *status_fixture,*mount_fixture;
static FILE *review_fopen(const char *path,const char *mode);
#define fopen review_fopen
#define main reviewed_supervisor_main
#include "supervisor.c"
#undef main
#undef fopen
static FILE *review_fopen(const char *path,const char *mode){
 (void)mode;
 if(!strcmp(path,"/proc/self/mountinfo")&&mount_fixture)return fmemopen((void*)mount_fixture,strlen(mount_fixture),"r");
 if(strstr(path,"/proc/")==path&&strstr(path,"/status")&&status_fixture)return fmemopen((void*)status_fixture,strlen(status_fixture),"r");
 return fopen(path,mode);
}
static void cap_fixture(char *out,size_t size,int selected,unsigned long long bit,int nnp){
 snprintf(out,size,"CapEff:\t%016llx\nCapPrm:\t%016llx\nCapInh:\t%016llx\nCapBnd:\t%016llx\nCapAmb:\t%016llx\nNoNewPrivs:\t%d\n",selected==0?bit:0ULL,selected==1?bit:0ULL,selected==2?bit:0ULL,selected==3?bit:0ULL,selected==4?bit:0ULL,nnp);
}
int main(void){
 assert(platform()==0);
 const char *nonce="0123456789abcdef0123456789abcdef";
 assert(nonce_valid(nonce));assert(!nonce_valid("0123"));assert(!nonce_valid("0123456789abcdef0123456789abcdeg"));
 char *args[]={"host-supervisor","--task-run",(char*)nonce,NULL};assert(reviewed_supervisor_main(3,args)==64);
 char *stop_args[]={"host-supervisor","--stop-owned","target","100","1",(char*)nonce,NULL};assert(reviewed_supervisor_main(6,stop_args)==64);
 stop_args[2]="browser";assert(stop_owned(stop_args)==64);stop_args[2]="target";stop_args[3]="1";assert(stop_owned(stop_args)==64);
 stop_args[3]="4194304";assert(stop_owned(stop_args)==64);stop_args[3]="100";stop_args[4]="0";assert(stop_owned(stop_args)==64);
 stop_args[4]="1";stop_args[5]="bad";assert(stop_owned(stop_args)==64);
 char buffer[512];status_fixture=buffer;cap_fixture(buffer,sizeof(buffer),-1,0,1);assert(post_exec_caps(getpid()));
 unsigned cases=1;
 for(int cap=0;cap<2;cap++)for(int set=0;set<5;set++){
  cap_fixture(buffer,sizeof(buffer),set,1ULL<<(cap?CAP_MAC_ADMIN:CAP_SYS_ADMIN),1);assert(!post_exec_caps(getpid()));cases++;
 }
 cap_fixture(buffer,sizeof(buffer),-1,0,0);assert(!post_exec_caps(getpid()));cases++;
 status_fixture="CapEff:\t0000000000000000\nNoNewPrivs:\t1\n";assert(!post_exec_caps(getpid()));cases++;
 status_fixture="CapEff:\tnot-hex\nCapPrm:\t0\nCapInh:\t0\nCapBnd:\t0\nCapAmb:\t0\nNoNewPrivs:\t1\n";assert(!post_exec_caps(getpid()));cases++;
 mount_fixture="1 0 0:1 / /sys/fs/selinux rw - selinuxfs selinuxfs rw\n";assert(aliases_only_expected());
 mount_fixture="1 0 0:1 / /sys/fs/selinux rw - selinuxfs selinuxfs rw\n2 1 0:1 / /tmp/alias rw - selinuxfs selinuxfs rw\n";assert(!aliases_only_expected());
 mount_fixture="1 0 0:1 / /sys/fs/selinux rw - selinuxfs selinuxfs rw\n2 1 0:1 / /sys/fs/selinux ro - selinuxfs selinuxfs rw\n";assert(aliases_only_expected());
 mount_fixture="1 0 0:1 / / rw - ext4 /dev/fixture rw\n";assert(!aliases_only_expected());
 mount_fixture="bad - selinuxfs broken\n";assert(!aliases_only_expected());
 int p[2];char data[4]={0};assert(!pipe(p));assert(!write_all(p[1],"abc",3));close(p[1]);assert(!read_all(p[0],data,3)&&!memcmp(data,"abc",3));assert(read_all(p[0],data,1)<0);close(p[0]);
 assert(!pipe(p));stopping=1;assert(read_all(p[0],data,1)<0);close(p[0]);close(p[1]);stopping=0;
 printf("PASS: host gate; nonce; %u cap-status cases; 5 mount-alias cases; bounded pipe EOF/stop\n",cases);
 return 0;
}
