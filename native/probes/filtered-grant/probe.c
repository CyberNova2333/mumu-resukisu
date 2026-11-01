/* SPDX-License-Identifier: GPL-3.0-or-later */
/* V2 test correction: upstream clears seccomp while Linux NNP remains sticky. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define QUERY_MAGIC1 0xdeadbeefUL
#define QUERY_MAGIC2 0xcafebabeUL
#define GET_INFO 0x80104b02UL
#define GRANT_ROOT 0x4b01UL
#define GROUP_LIMIT 256
#define CAP_SYS_BOOT 22
/* Linux capget V3 fixed UAPI layout; avoid glibc headers in musl builds. */
#define CAP_VERSION3 0x20080522U
struct cap_header {uint32_t version;int32_t pid;};
struct cap_data {uint32_t effective,permitted,inheritable;};
_Static_assert(QUERY_MAGIC1!=0xfee1deadUL,"Not a Linux reboot request");
struct info {uint32_t version,flags,features,uapi;};
struct snapshot {
    uint32_t uid,euid,suid,gid,egid,sgid;
    uint64_t cap_effective,cap_permitted,cap_inheritable;
    int nnp,seccomp,group_count;
    gid_t groups[GROUP_LIMIT];
    char context[128];
};
struct packet {uint32_t magic;int before_ok,attempted,rc,error,after_ok;struct snapshot before,after;};
_Static_assert(sizeof(struct packet)<4096,"One bounded pipe packet");
static int install_child_filter(void) {
    /* Linux classic-BPF fixed UAPI: one RET ALLOW instruction. */
    struct filter_instruction {uint16_t code;uint8_t jt,jf;uint32_t k;};
    struct filter_program {unsigned short len;struct filter_instruction *filter;};
    struct filter_instruction code={.code=0x06,.jt=0,.jf=0,.k=0x7fff0000};
    struct filter_program p={.len=1,.filter=&code};
    _Static_assert(sizeof(code)==8,"classic BPF instruction size");
    if(prctl(PR_SET_NO_NEW_PRIVS,1UL,0UL,0UL,0UL))return -1;
    return syscall(SYS_seccomp,1UL,0UL,&p)==0?0:-1;
}
static int caps(struct snapshot *s) {
    struct cap_header h={.version=CAP_VERSION3,.pid=0};
    struct cap_data c[2]={{0}};if(syscall(SYS_capget,&h,c))return -1;
    s->cap_effective=((uint64_t)c[1].effective<<32)|c[0].effective;
    s->cap_permitted=((uint64_t)c[1].permitted<<32)|c[0].permitted;
    s->cap_inheritable=((uint64_t)c[1].inheritable<<32)|c[0].inheritable;return 0;
}
static int read_context(char *out,size_t size) {
    int fd=open("/proc/self/attr/current",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return -1;
    ssize_t n=read(fd,out,size);int e=errno;close(fd);errno=e;
    if(n<=0||(size_t)n>=size){errno=EPROTO;return -1;}
    while(n>0&&(out[n-1]=='\n'||out[n-1]==0))n--;
    if(!n){errno=EPROTO;return -1;}out[n]=0;
    for(ssize_t i=0;i<n;i++)if((unsigned char)out[i]<32||(unsigned char)out[i]>126){errno=EPROTO;return -1;}
    return 0;
}
static int take_snapshot(struct snapshot *s) {
    memset(s,0,sizeof(*s));uid_t u,e,su;gid_t g,eg,sg;
    if(getresuid(&u,&e,&su)||getresgid(&g,&eg,&sg))return -1;
    s->uid=u;s->euid=e;s->suid=su;s->gid=g;s->egid=eg;s->sgid=sg;
    if(caps(s))return -1;s->nnp=prctl(PR_GET_NO_NEW_PRIVS,0UL,0UL,0UL,0UL);
    s->seccomp=prctl(PR_GET_SECCOMP,0UL,0UL,0UL,0UL);if(s->nnp<0||s->seccomp<0)return -1;
    s->group_count=getgroups(GROUP_LIMIT,s->groups);if(s->group_count<0)return -1;
    return read_context(s->context,sizeof(s->context));
}
static int same_snapshot(const struct snapshot *a,const struct snapshot *b) {return !memcmp(a,b,sizeof(*a));}
static int same_groups(const struct snapshot *a,const struct snapshot *b) {return a->group_count==b->group_count&&!memcmp(a->groups,b->groups,(size_t)a->group_count*sizeof(gid_t));}
static const char *context_label(const char *s) {
    if(!strcmp(s,"u:r:shell:s0"))return "u:r:shell:s0";
    if(!strcmp(s,"u:r:ksu:s0"))return "u:r:ksu:s0";
    if(!strcmp(s,"u:r:su:s0"))return "u:r:su:s0";
    return "other";
}
static int get_boot_id(char out[37]) {
    int fd=open("/proc/sys/kernel/random/boot_id",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return -1;
    char buf[38]={0};ssize_t n=read(fd,buf,sizeof(buf));close(fd);if(n!=37||buf[36]!='\n'){errno=EPROTO;return -1;}
    for(int i=0;i<36;i++){if(i==8||i==13||i==18||i==23){if(buf[i]!='-'){errno=EPROTO;return -1;}}else if(!((buf[i]>='0'&&buf[i]<='9')||(buf[i]>='a'&&buf[i]<='f'))){errno=EPROTO;return -1;}}
    memcpy(out,buf,36);out[36]=0;return 0;
}
static int driver(struct info *info) {
    int fd=-1;syscall(SYS_reboot,QUERY_MAGIC1,QUERY_MAGIC2,0UL,&fd);if(fd<0){errno=ENODEV;return -1;}
    char path[64],target[64];snprintf(path,sizeof(path),"/proc/self/fd/%d",fd);ssize_t n=readlink(path,target,sizeof(target)-1);
    if(n<0){int e=errno;close(fd);errno=e;return -1;}target[n]=0;
    /* Reject su-session FDs: this tests the normal permission path. */
    if(strcmp(target,"anon_inode:[ksu_driver]")){close(fd);errno=EPROTO;return -1;}
    fcntl(fd,F_SETFD,FD_CLOEXEC);
    if(ioctl(fd,GET_INFO,info)||info->version!=35253||info->uapi!=6){close(fd);errno=EPROTO;return -1;}
    return fd;
}
static long long now_ms(void) {struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return (long long)t.tv_sec*1000+t.tv_nsec/1000000;}
static void pause_ms(void) {struct timespec t={0,10000000};while(nanosleep(&t,&t)&&errno==EINTR){}}
static int child_alarm(unsigned seconds) {
    /* A root child may no longer be signalable by its UID2000 parent. */
    struct sigaction action={.sa_handler=SIG_DFL};sigemptyset(&action.sa_mask);
    sigset_t unblocked;sigemptyset(&unblocked);sigaddset(&unblocked,SIGALRM);
    if(sigaction(SIGALRM,&action,NULL)||sigprocmask(SIG_UNBLOCK,&unblocked,NULL))return -1;
    alarm(seconds);return 0;
}
static int bounded_wait(pid_t pid,int milliseconds,int *status,int *timed_out) {
    long long start=now_ms();if(start<0)return -1;
    for(;;){pid_t r=waitpid(pid,status,WNOHANG);if(r==pid)return 0;if(r<0){if(errno==EINTR)continue;return -1;}
        long long now=now_ms();if(now<0||now-start>=milliseconds){*timed_out=1;
            if(kill(pid,SIGKILL)&&errno!=ESRCH)return -1;
            long long deadline=now_ms()+1000;
            for(;;){r=waitpid(pid,status,WNOHANG);if(r==pid)return 0;if(r<0&&errno!=EINTR)return -1;if(now_ms()>=deadline){errno=ETIMEDOUT;return -1;}pause_ms();}
        }pause_ms();
    }
}
static const char *classify(int expect_granted,int complete,int child_ok,int parent_same,const struct packet *p) {
    if(!complete||!child_ok||!p->before_ok||!p->after_ok||p->magic!=0x47525036||!p->attempted)return "probe_incomplete";
    if(!parent_same)return "parent_identity_changed";
    if(p->before.nnp!=1||p->before.seccomp!=2)return "child_filter_not_installed";
    if(p->rc==-1&&p->error==EPERM&&same_snapshot(&p->before,&p->after))return expect_granted?"denied_after_expected_grant":"denied_and_child_identity_unchanged";
    if(p->rc==0&&p->error==0&&p->after.uid==0&&p->after.euid==0&&p->after.gid==0&&p->after.egid==0&&p->after.nnp==1&&p->after.seccomp==0)return expect_granted?"granted_filter_removed_nnp_preserved":"unexpected_grant_filter_removed_nnp_preserved";
    return "unexpected_authorization_result";
}
static void print_snapshot(const char *name,const struct snapshot *s) {
    printf("\"%s\":{\"uid\":%u,\"euid\":%u,\"savedUid\":%u,\"gid\":%u,\"egid\":%u,\"savedGid\":%u,\"capEffective\":\"%016llx\",\"capPermitted\":\"%016llx\",\"capInheritable\":\"%016llx\",\"noNewPrivs\":%d,\"seccompMode\":%d,\"groupCount\":%d,\"selinuxContext\":\"%s\"}",name,s->uid,s->euid,s->suid,s->gid,s->egid,s->sgid,(unsigned long long)s->cap_effective,(unsigned long long)s->cap_permitted,(unsigned long long)s->cap_inheritable,s->nnp,s->seccomp,s->group_count,context_label(s->context));
}

#ifndef GRANT_PROBE_UNIT_TEST
int main(int argc,char **argv) {
    /* UID and capget gates before any file access or driver syscall. */
    struct snapshot gate={0};
    int expect_granted=argc==2&&!strcmp(argv[1],"expect-granted");
    int valid_mode=argc==2&&(expect_granted||!strcmp(argv[1],"expect-denied"));
    if(!valid_mode||getuid()!=2000||geteuid()!=2000||caps(&gate)||(gate.cap_effective&(1ULL<<CAP_SYS_BOOT))){
        static const char msg[]="{\"schemaVersion\":1,\"executed\":false,\"status\":\"requires_uid2000_without_cap_sys_boot_and_fixed_mode\"}\n";
        (void)write(STDOUT_FILENO,msg,sizeof(msg)-1);return 64;
    }
    struct snapshot parent_before={0},parent_after={0};char boot[37];struct info info={0};
    if(take_snapshot(&parent_before)||get_boot_id(boot)){printf("{\"schemaVersion\":1,\"executed\":false,\"status\":\"snapshot_failed\",\"errno\":%d}\n",errno);return 65;}
    int fd=driver(&info);if(fd<0){printf("{\"schemaVersion\":1,\"executed\":false,\"status\":\"driver_mismatch\",\"errno\":%d}\n",errno);return 66;}
    int channel[2];if(pipe2(channel,O_CLOEXEC|O_NONBLOCK)){close(fd);return 67;}
    pid_t pid=fork();if(pid<0){close(fd);close(channel[0]);close(channel[1]);return 67;}
    if(!pid){
        close(channel[0]);struct packet p={.magic=0x47525036,.rc=-2};
        if(child_alarm(4)){close(channel[1]);close(fd);_exit(73);}
        p.before_ok=install_child_filter()==0 && take_snapshot(&p.before)==0;
        if(p.before_ok&&p.before.nnp==1&&p.before.seccomp==2&&p.before.uid==2000&&p.before.euid==2000&&!(p.before.cap_effective&(1ULL<<CAP_SYS_BOOT))){
            p.attempted=1;errno=0;p.rc=ioctl(fd,GRANT_ROOT,NULL);p.error=errno;
        }else p.error=EPERM;
        p.after_ok=take_snapshot(&p.after)==0;
        ssize_t n;do{n=write(channel[1],&p,sizeof(p));}while(n<0&&errno==EINTR);
        close(channel[1]);close(fd);_exit(n==(ssize_t)sizeof(p)?0:72);
    }
    close(channel[1]);close(fd);int status=0,timed_out=0;
    int wait_ok=bounded_wait(pid,5000,&status,&timed_out)==0;
    struct packet packet={0};ssize_t received=read(channel[0],&packet,sizeof(packet));close(channel[0]);
    int after_ok=take_snapshot(&parent_after)==0;
    int parent_same=after_ok&&same_snapshot(&parent_before,&parent_after);
    int packet_complete=received==(ssize_t)sizeof(packet)&&!timed_out;
    int complete=packet_complete&&after_ok;
    int child_ok=wait_ok&&WIFEXITED(status)&&WEXITSTATUS(status)==0;
    const char *result=classify(expect_granted,complete,child_ok,parent_same,&packet);
    int passed=!strcmp(result,expect_granted?"granted_filter_removed_nnp_preserved":"denied_and_child_identity_unchanged");
    printf("{\"schemaVersion\":1,\"executed\":true,\"bootId\":\"%s\",\"expectation\":\"%s\",\"status\":\"%s\",\"passed\":%s,\"version\":%u,\"uapiVersion\":%u,\"requestCount\":%d,\"ioctlRc\":%d,\"ioctlErrno\":%d,\"timedOut\":%s,\"childExited\":%s,\"childExitCode\":%d,\"childSignal\":%d,\"packetComplete\":%s,\"parentAfterSnapshotOk\":%s,\"childBeforeSnapshotOk\":%s,\"childAfterSnapshotOk\":%s,\"parentUnchanged\":%s,\"childGroupsUnchanged\":%s,",boot,expect_granted?"granted":"denied",result,passed?"true":"false",info.version,info.uapi,packet.attempted,packet.rc,packet.error,timed_out?"true":"false",wait_ok?"true":"false",wait_ok&&WIFEXITED(status)?WEXITSTATUS(status):-1,wait_ok&&WIFSIGNALED(status)?WTERMSIG(status):0,packet_complete?"true":"false",after_ok?"true":"false",packet.before_ok?"true":"false",packet.after_ok?"true":"false",parent_same?"true":"false",complete&&packet.before_ok&&packet.after_ok&&same_groups(&packet.before,&packet.after)?"true":"false");
    print_snapshot("parentBefore",&parent_before);printf(",");print_snapshot("parentAfter",&parent_after);printf(",");print_snapshot("childBefore",&packet.before);printf(",");print_snapshot("childAfter",&packet.after);
    puts(",\"allowlistModified\":false,\"shellCommandExecuted\":false,\"filteredChildOnly\":true,\"scope\":\"one_child_allow_all_filter_then_native_grant_not_global_policy_or_ia32_validation\"}");
    return passed?0:68;
}
#endif
