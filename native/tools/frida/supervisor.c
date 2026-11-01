/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Fixed official server, private read-only SELinux view, bounded owned child. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
#include "sha256.h"
#include "server-pin.h"
#include "target-pin.h"
#include "runtime-config.h"
#define SERVER "/data/local/tmp/resukisu-frida-server"
#define SELINUX "/sys/fs/selinux"
#define BLOCKED_CAPS ((1ULL<<CAP_SYS_ADMIN)|(1ULL<<CAP_MAC_ADMIN))
static volatile sig_atomic_t stopping;
static void stop_signal(int sig){(void)sig;stopping=1;}
static int nonce_valid(const char *s){if(strlen(s)!=32)return 0;for(int i=0;i<32;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;return 1;}
static int platform(void){
#ifdef __ANDROID__
    char sdk[PROP_VALUE_MAX];return __system_property_get("ro.build.version.sdk",sdk)>0&&!strcmp(sdk,"35");
#else
    return 0;
#endif
}
static int root_context(void){char b[64]={0};int f=open("/proc/self/attr/current",O_RDONLY|O_CLOEXEC);if(f<0)return 0;ssize_t n=read(f,b,63);close(f);while(n>0&&(b[n-1]=='\n'||!b[n-1]))b[--n]=0;return n>0&&!strcmp(b,"u:r:ksu:s0");}
static int write_all(int f,const void *v,size_t n){const char *p=v;while(n){ssize_t k=write(f,p,n);if(k<0&&errno==EINTR)continue;if(k<=0)return -1;p+=k;n-=k;}return 0;}
static int read_all(int f,void *v,size_t n){char *p=v;while(n&&!stopping){ssize_t k=read(f,p,n);if(k<0&&errno==EINTR)continue;if(k<=0)return -1;p+=k;n-=k;}return n?-1:0;}
static int stable(const struct stat *a,const struct stat *b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&a->st_mode==b->st_mode&&a->st_uid==b->st_uid&&a->st_gid==b->st_gid&&a->st_nlink==b->st_nlink&&a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;}
static int server_open(struct stat *identity){
    int f=open(SERVER,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(f<0)return -1;struct stat end;
    if(fstat(f,identity)||!S_ISREG(identity->st_mode)||identity->st_uid||identity->st_gid||identity->st_nlink!=1||(identity->st_mode&07777)!=0700||identity->st_size!=SERVER_BYTES){close(f);errno=EPERM;return -1;}
    void *p=mmap(NULL,SERVER_BYTES,PROT_READ,MAP_PRIVATE,f,0);if(p==MAP_FAILED){close(f);return -1;}unsigned char hash[32];sha256(p,SERVER_BYTES,hash);munmap(p,SERVER_BYTES);
    if(memcmp(hash,server_hash,32)||fstat(f,&end)||!stable(identity,&end)){close(f);errno=EKEYREJECTED;return -1;}return f;
}
static int policy_hash(char out[65]){
    int f=open(SELINUX "/policy",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(f<0)return -1;
    const size_t limit=32*1024*1024;unsigned char *buf=malloc(limit);if(!buf){close(f);return -1;}size_t n=0;int ok=0;
    while(n<limit){ssize_t k=read(f,buf+n,limit-n);if(k<0&&errno==EINTR)continue;if(k<0)break;if(!k){ok=1;break;}n+=(size_t)k;}
    close(f);unsigned char digest[32];if(ok&&n){sha256(buf,n,digest);for(int i=0;i<32;i++)sprintf(out+i*2,"%02x",digest[i]);}else ok=0;free(buf);return ok?0:-1;
}
static int enforce(void){char b[4]={0};int f=open(SELINUX "/enforce",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(f<0)return -1;ssize_t n=read(f,b,3);close(f);return n>0&&(b[0]=='0'||b[0]=='1')?b[0]-'0':-1;}
static int aliases_only_expected(void){
    FILE *f=fopen("/proc/self/mountinfo","re");if(!f)return 0;char *line=NULL;size_t cap=0;int count=0,ok=1;
    while(getline(&line,&cap,f)>0){char *suffix=strstr(line," - selinuxfs ");if(!suffix)continue;char point[4096];if(sscanf(line,"%*u %*u %*u:%*u %*s %4095s",point)!=1||strcmp(point,SELINUX)){ok=0;break;}count++;}
    if(ferror(f))ok=0;free(line);fclose(f);return ok&&count>0;
}
static int namespace_inode(pid_t pid,unsigned long long *out){char p[64];struct stat st;snprintf(p,sizeof(p),"/proc/%d/ns/mnt",pid);if(stat(p,&st))return -1;*out=st.st_ino;return 0;}
static int drop_caps(void){
    struct __user_cap_header_struct h={.version=_LINUX_CAPABILITY_VERSION_3};struct __user_cap_data_struct d[2];
    if(prctl(PR_CAPBSET_DROP,CAP_SYS_ADMIN,0,0,0)||prctl(PR_CAPBSET_DROP,CAP_MAC_ADMIN,0,0,0)||prctl(PR_CAP_AMBIENT,PR_CAP_AMBIENT_CLEAR_ALL,0,0,0))return -1;
    if(syscall(SYS_capget,&h,d))return -1;
    for(int i=0;i<2;i++){uint32_t mask=(uint32_t)(BLOCKED_CAPS>>(32*i));d[i].effective&=~mask;d[i].permitted&=~mask;d[i].inheritable&=~mask;}
    if(syscall(SYS_capset,&h,d)||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)||syscall(SYS_capget,&h,d))return -1;
    for(int i=0;i<2;i++){uint32_t mask=(uint32_t)(BLOCKED_CAPS>>(32*i));if((d[i].effective|d[i].permitted|d[i].inheritable)&mask)return -1;}
    if(prctl(PR_CAPBSET_READ,CAP_SYS_ADMIN,0,0,0)!=0||prctl(PR_CAPBSET_READ,CAP_MAC_ADMIN,0,0,0)!=0||prctl(PR_GET_NO_NEW_PRIVS,0,0,0,0)!=1)return -1;
    return 0;
}
struct isolation {int ok,error,load_errno,enforce_errno;unsigned long long ns;};
static int isolate(unsigned long long parent_ns,struct isolation *r){
    struct statfs fs;struct statvfs v;
    if(unshare(CLONE_NEWNS)||namespace_inode(getpid(),&r->ns)||r->ns==parent_ns||mount(NULL,"/",NULL,MS_REC|MS_PRIVATE,NULL)||!aliases_only_expected())return -1;
    if(statfs(SELINUX,&fs)||(unsigned long)fs.f_type!=0xf97cff8cUL)return -1;
    if(mount(SELINUX,SELINUX,NULL,MS_BIND,NULL)||mount(NULL,SELINUX,NULL,MS_REMOUNT|MS_BIND|MS_RDONLY|MS_NOSUID|MS_NODEV|MS_NOEXEC,NULL))return -1;
    if(statvfs(SELINUX,&v)||!(v.f_flag&ST_RDONLY)||!aliases_only_expected())return -1;
    int fd=open(SELINUX "/load",O_WRONLY|O_CLOEXEC);r->load_errno=errno;if(fd>=0){close(fd);return -1;}
    fd=open(SELINUX "/enforce",O_WRONLY|O_CLOEXEC);r->enforce_errno=errno;if(fd>=0){close(fd);return -1;}
    if(r->load_errno!=EROFS||r->enforce_errno!=EROFS||drop_caps())return -1;
    r->ok=1;return 0;
}
static int post_exec_caps(pid_t pid){
    char p[64],line[256];snprintf(p,sizeof(p),"/proc/%d/status",pid);FILE *f=fopen(p,"re");if(!f)return 0;unsigned seen=0;int ok=1;
    while(fgets(line,sizeof(line),f)){unsigned long long value;
        const char *keys[]={"CapEff:","CapPrm:","CapInh:","CapBnd:","CapAmb:"};
        for(unsigned i=0;i<5;i++)if(!strncmp(line,keys[i],7)){if(sscanf(line+7,"%llx",&value)!=1||(value&BLOCKED_CAPS))ok=0;seen|=1U<<i;}
        if(!strncmp(line,"NoNewPrivs:",11)){if(sscanf(line+11,"%llu",&value)!=1||value!=1)ok=0;seen|=32;}
    }if(ferror(f))ok=0;fclose(f);return ok&&seen==63;
}
static int child_exec_identity(pid_t pid,const struct stat *wanted){char p[64];struct stat s;snprintf(p,sizeof(p),"/proc/%d/exe",pid);return !stat(p,&s)&&s.st_dev==wanted->st_dev&&s.st_ino==wanted->st_ino;}
static long long millis(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (long long)t.tv_sec*1000+t.tv_nsec/1000000;}
static void delay(void){struct timespec t={.tv_nsec=10000000};nanosleep(&t,NULL);}
static unsigned long long decimal(const char *s){char *end;errno=0;unsigned long long n=strtoull(s,&end,10);return !*s||*end||errno?0:n;}
/* A pidfd is acquired before inspection, so cleanup never signals a recycled PID. */
static int stop_owned(char **argv){
    const char *role=argv[2],*nonce=argv[5];unsigned long long raw=decimal(argv[3]),ticks=decimal(argv[4]);
    if((strcmp(role,"target")&&strcmp(role,"supervisor"))||raw<=1||raw>=4194304||!ticks||!nonce_valid(nonce))return 64;
    pid_t pid=(pid_t)raw;int fd=(int)syscall(SYS_pidfd_open,pid,0);if(fd<0)return 69;
    const char *path=!strcmp(role,"target")?"/data/local/tmp/resukisu-frida-probe":"/data/local/tmp/resukisu-frida-supervisor";
    char proc[64],b[4096],expected[256];int ok=1;snprintf(proc,sizeof(proc),"/proc/%d/stat",pid);int f=open(proc,O_RDONLY|O_CLOEXEC);ssize_t n=f<0?-1:read(f,b,sizeof(b)-1);if(f>=0)close(f);
    if(n<=0)ok=0;else{b[n]=0;char *p=strrchr(b,')');if(!p)ok=0;else{p+=2;for(int i=0;i<19&&p;i++){p=strchr(p,' ');if(p)p++;}if(!p||strtoull(p,NULL,10)!=ticks)ok=0;}}
    snprintf(proc,sizeof(proc),"/proc/%d/cmdline",pid);f=open(proc,O_RDONLY|O_CLOEXEC);n=f<0?-1:read(f,b,sizeof(b));if(f>=0)close(f);
    size_t expected_size=strlen(path)+1+sizeof("--task-run")+33;memset(expected,0,sizeof(expected));memcpy(expected,path,strlen(path));memcpy(expected+strlen(path)+1,"--task-run",sizeof("--task-run"));memcpy(expected+strlen(path)+1+sizeof("--task-run"),nonce,33);
    if(n!=(ssize_t)expected_size||memcmp(b,expected,expected_size))ok=0;
    snprintf(proc,sizeof(proc),"/proc/%d/exe",pid);n=readlink(proc,b,sizeof(b)-1);if(n<0)ok=0;else{b[n]=0;if(strcmp(b,path))ok=0;}
    struct stat st,self;f=open(proc,O_RDONLY|O_CLOEXEC);if(f<0||fstat(f,&st)||!S_ISREG(st.st_mode))ok=0;
    else if(!strcmp(role,"supervisor")){if(stat("/proc/self/exe",&self)||self.st_dev!=st.st_dev||self.st_ino!=st.st_ino)ok=0;}
    else{if(st.st_size!=TARGET_BYTES)ok=0;else{void *m=mmap(NULL,TARGET_BYTES,PROT_READ,MAP_PRIVATE,f,0);if(m==MAP_FAILED)ok=0;else{unsigned char h[32];sha256(m,TARGET_BYTES,h);munmap(m,TARGET_BYTES);if(memcmp(h,target_hash,32))ok=0;}}}if(f>=0)close(f);
    snprintf(proc,sizeof(proc),"/proc/%d/status",pid);FILE *status=fopen(proc,"re");int uid_ok=0;unsigned uid,euid,suid,fsuid,want=!strcmp(role,"target")?2000:0;
    if(status){while(fgets(b,sizeof(b),status))if(sscanf(b,"Uid: %u %u %u %u",&uid,&euid,&suid,&fsuid)==4){uid_ok=uid==want&&euid==want&&suid==want&&fsuid==want;break;}fclose(status);}if(!uid_ok)ok=0;
    int sent=ok&&!syscall(SYS_pidfd_send_signal,fd,SIGTERM,NULL,0);close(fd);printf("{\"status\":\"owned_stop_result\",\"identityVerified\":%s,\"termSent\":%s,\"pidfdUsed\":true}\n",ok?"true":"false",sent?"true":"false");return sent?0:69;
}
int main(int argc,char **argv){
    if(argc==6&&!strcmp(argv[1],"--stop-owned")){if(getuid()!=0||geteuid()!=0||!platform()||!root_context())return 64;return stop_owned(argv);}
    if(argc!=3||strcmp(argv[1],"--task-run")||!nonce_valid(argv[2])||getuid()!=0||geteuid()!=0||!platform()||!root_context()){puts("{\"status\":\"root_platform_or_nonce_gate_denied\",\"executed\":false}");return 64;}
    struct sigaction sa={.sa_handler=stop_signal},chld={.sa_handler=SIG_DFL};sigemptyset(&sa.sa_mask);sigemptyset(&chld.sa_mask);if(sigaction(SIGTERM,&sa,NULL)||sigaction(SIGINT,&sa,NULL)||sigaction(SIGALRM,&sa,NULL))return 65;
    sigset_t unblock;sigemptyset(&unblock);sigaddset(&unblock,SIGCHLD);sigaddset(&unblock,SIGTERM);sigaddset(&unblock,SIGALRM);
    if(sigaction(SIGCHLD,&chld,NULL)||sigprocmask(SIG_UNBLOCK,&unblock,NULL))return 65;alarm(75);
    struct stat identity;struct statvfs parent_flags,after_flags;unsigned long long parent_ns=0,after_ns=0;char hash_before[65]={0},hash_after[65]={0};int before_enforce=-1;
    int server=server_open(&identity);if(server<0||namespace_inode(getpid(),&parent_ns)||statvfs(SELINUX,&parent_flags)||!aliases_only_expected()||policy_hash(hash_before)||(before_enforce=enforce())<0){if(server>=0)close(server);puts("{\"status\":\"pinned_input_or_parent_snapshot_failed\",\"executed\":false}");return 65;}
    /* Frida enumerates an existing fixed temp directory: provide a new task-only one. */
    char cache[64];snprintf(cache,sizeof(cache),"resukisu-frida-%s",argv[2]);
    int tmp=open("/data/local/tmp",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);struct stat cache_st;
    if(tmp<0||mkdirat(tmp,cache,0755)){if(tmp>=0)close(tmp);close(server);return 65;}
    int cachefd=openat(tmp,cache,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(cachefd<0||fchmod(cachefd,0755)||fstat(cachefd,&cache_st)||cache_st.st_uid||cache_st.st_gid||(cache_st.st_mode&07777)!=0755){if(cachefd>=0)close(cachefd);close(tmp);close(server);return 65;}close(cachefd);close(tmp);
    int pipefd[2],permit[2];if(pipe2(pipefd,O_CLOEXEC)||pipe2(permit,O_CLOEXEC)){close(server);return 65;}
    pid_t owner=getpid(),child=fork();if(child<0){close(server);close(pipefd[0]);close(pipefd[1]);close(permit[0]);close(permit[1]);return 65;}
    if(!child){
        close(pipefd[0]);close(permit[1]);struct isolation info={0};char allowed=0;
        if(prctl(PR_SET_PDEATHSIG,SIGKILL)||getppid()!=owner||read_all(permit[0],&allowed,1)||allowed!=1)_exit(66);close(permit[0]);
        if(isolate(parent_ns,&info)){info.error=errno?errno:EPERM;write_all(pipefd[1],&info,sizeof(info));_exit(66);}
        /* Prevent inherited su-session or other descriptors from reaching server. */
        if(syscall(SYS_close_range,3,~0U,4)){info.ok=0;info.error=errno;write_all(pipefd[1],&info,sizeof(info));_exit(66);}
        if(write_all(pipefd[1],&info,sizeof(info)))_exit(66);
        int null=open("/dev/null",O_RDWR|O_CLOEXEC);if(null<0)_exit(66);dup2(null,0);dup2(null,1);dup2(null,2);if(null>2)close(null);
        char *const arguments[]={SERVER,"--listen",FRIDA_LISTEN,"--ignore-crashes","--disable-preload","--directory",cache,"--token",argv[2],NULL};
        char *const environment[]={"PATH=/system/bin","TMPDIR=/data/local/tmp",NULL};
        syscall(SYS_execveat,server,"",arguments,environment,AT_EMPTY_PATH);_exit(67);
    }
    close(server);close(pipefd[1]);close(permit[0]);int pidfd=(int)syscall(SYS_pidfd_open,child,0);char allowed=1;
    if(pidfd<0){close(permit[1]);close(pipefd[0]);long long until=millis()+2000;while(!stopping&&millis()<until){pid_t w=waitpid(child,NULL,WNOHANG);if(w==child||(w<0&&errno==ECHILD))break;delay();}return 65;}
    write_all(permit[1],&allowed,1);close(permit[1]);struct isolation info={0};int isolated=!read_all(pipefd[0],&info,sizeof(info))&&info.ok;close(pipefd[0]);
    int status=0,reaped=0,exec_ok=0,timed_out=0,forced=0,ownership_lost=0;
    long long start=millis();
    while(isolated&&!stopping&&millis()-start<3000){unsigned long long child_ns=0;pid_t w=waitpid(child,&status,WNOHANG);if(w==child){reaped=1;break;}if(w<0&&errno==ECHILD){ownership_lost=1;break;}if(child_exec_identity(child,&identity)&&post_exec_caps(child)&&!namespace_inode(child,&child_ns)&&child_ns==info.ns&&child_ns!=parent_ns){exec_ok=1;break;}delay();}
    if(exec_ok){printf("{\"schemaVersion\":1,\"status\":\"server_exec_verified\",\"supervisorPid\":%d,\"serverPid\":%d,\"privateMountNamespace\":true,\"selinuxReadOnly\":true,\"loadWriteErrno\":%d,\"enforceWriteErrno\":%d,\"postExecAdminCapsAbsent\":true,\"postExecNoNewPrivs\":1,\"policySha256Before\":\"%s\",\"enforceBefore\":%d,\"deadlineSeconds\":60}\n",getpid(),child,info.load_errno,info.enforce_errno,hash_before,before_enforce);fflush(stdout);}
    while(exec_ok&&!stopping&&!reaped&&!ownership_lost){pid_t w=waitpid(child,&status,WNOHANG);if(w==child){reaped=1;break;}if(w<0&&errno==ECHILD){ownership_lost=1;break;}if(millis()-start>=60000){timed_out=1;break;}delay();}
    if(!reaped&&!ownership_lost){syscall(SYS_pidfd_send_signal,pidfd,SIGTERM,NULL,0);long long until=millis()+2000;while(millis()<until){pid_t w=waitpid(child,&status,WNOHANG);if(w==child){reaped=1;break;}if(w<0&&errno==ECHILD){ownership_lost=1;break;}delay();}}
    if(!reaped&&!ownership_lost){forced=1;syscall(SYS_pidfd_send_signal,pidfd,SIGKILL,NULL,0);long long until=millis()+2000;while(millis()<until){pid_t w=waitpid(child,&status,WNOHANG);if(w==child){reaped=1;break;}if(w<0&&errno==ECHILD){ownership_lost=1;break;}delay();}}close(pidfd);
    int unchanged=!policy_hash(hash_after)&&!strcmp(hash_before,hash_after)&&enforce()==before_enforce&&!namespace_inode(getpid(),&after_ns)&&after_ns==parent_ns&&!statvfs(SELINUX,&after_flags)&&after_flags.f_flag==parent_flags.f_flag;
    printf("{\"schemaVersion\":1,\"status\":\"server_supervision_finished\",\"serverPid\":%d,\"serverExecVerified\":%s,\"serverReaped\":%s,\"timedOut\":%s,\"forcedKill\":%s,\"parentPolicyEnforceMountUnchanged\":%s,\"policySha256After\":\"%s\",\"childExitCode\":%d,\"childSignal\":%d}\n",child,exec_ok?"true":"false",reaped?"true":"false",timed_out?"true":"false",forced?"true":"false",unchanged?"true":"false",hash_after,reaped&&WIFEXITED(status)?WEXITSTATUS(status):-1,reaped&&WIFSIGNALED(status)?WTERMSIG(status):0);
    return exec_ok&&reaped&&unchanged&&!forced?0:68;
}
