/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Fixed freestanding Linux i386 caller. No libc, configurable command, or writes. */
typedef unsigned int u32;
typedef unsigned char u8;
typedef signed int i32;
#define P(x) ((u32)(x))
#define CLOEXEC 0x80000U
#define NONBLOCK 0x800U
#define AT_FDCWD ((u32)-100)
#define MAX_GROUPS 64
#define PACKET_MAGIC 0x53553332U

static int sc5(int n,u32 a,u32 b,u32 c,u32 d,u32 e) {
    int r;__asm__ volatile("int $0x80":"=a"(r):"0"(n),"b"(a),"c"(b),"d"(c),"S"(d),"D"(e):"memory","cc");return r;
}
static int sc0(int n){return sc5(n,0,0,0,0,0);}
static int sc1(int n,u32 a){return sc5(n,a,0,0,0,0);}
static int sc2(int n,u32 a,u32 b){return sc5(n,a,b,0,0,0);}
static int sc3(int n,u32 a,u32 b,u32 c){return sc5(n,a,b,c,0,0);}
static int sc4(int n,u32 a,u32 b,u32 c,u32 d){return sc5(n,a,b,c,d,0);}
static __attribute__((noreturn)) void quit(int rc){sc1(1,(u32)rc);__builtin_unreachable();}
static u32 length(const char *s){u32 n=0;while(s[n])n++;return n;}
static void zero(void *p,u32 n){u8 *b=p;while(n--)*b++=0;}
static int equal(const void *a,const void *b,u32 n){const u8 *x=a,*y=b;while(n--)if(*x++!=*y++)return 0;return 1;}
static int streq(const char *a,const char *b){return length(a)==length(b)&&equal(a,b,length(a));}
static int write_all(int fd,const void *p,u32 n){const u8 *b=p;while(n){int r=sc3(4,(u32)fd,P(b),n);if(r==-4)continue;if(r<=0)return 0;b+=r;n-=(u32)r;}return 1;}
static void close_fd(int fd){if(fd>=0)sc1(6,(u32)fd);}

struct snapshot {
    u32 uid,euid,savedUid,gid,egid,savedGid;
    u32 caps[6]; /* low effective/permitted/inheritable, then high words */
    int nnp,seccomp,groupCount,context;
    u32 groups[MAX_GROUPS];
};
static int snapshot(struct snapshot *s){
    u32 header[2]={0x20080522U,0};char ctx[64];zero(s,sizeof(*s));
    if(sc3(209,P(&s->uid),P(&s->euid),P(&s->savedUid))||sc3(211,P(&s->gid),P(&s->egid),P(&s->savedGid)))return 0;
    if(sc2(184,P(header),P(s->caps)))return 0;
    s->nnp=sc5(172,39,0,0,0,0);s->seccomp=sc5(172,21,0,0,0,0);
    s->groupCount=sc2(205,MAX_GROUPS,P(s->groups));
    if(s->nnp<0||s->seccomp<0||s->groupCount<0||s->groupCount>MAX_GROUPS)return 0;
    int fd=sc3(5,P("/proc/self/attr/current"),CLOEXEC,0);if(fd<0)return 0;
    int n=sc3(3,(u32)fd,P(ctx),sizeof(ctx));close_fd(fd);if(n<=0||n>=(int)sizeof(ctx))return 0;
    while(n&&(!ctx[n-1]||ctx[n-1]=='\n'))n--;ctx[n]=0;
    s->context=streq(ctx,"u:r:shell:s0")?1:streq(ctx,"u:r:ksu:s0")?2:streq(ctx,"u:r:su:s0")?3:0;
    return 1;
}
static int shell_identity(const struct snapshot *s){
    return s->uid==2000&&s->euid==2000&&s->savedUid==2000&&s->gid==2000&&s->egid==2000&&s->savedGid==2000&&s->caps[0]==0&&s->caps[1]==0&&s->caps[3]==0&&s->caps[4]==0&&s->context==1;
}
static int unchanged(const struct snapshot *a,const struct snapshot *b){return equal(a,b,sizeof(*a));}

struct info {u32 version,flags,features,uapi;};
static int verify_driver(struct info *info){
    int fd=-1;char path[64]="/proc/self/fd/";char digits[12],identity[64];u32 n=0,at=length(path),v;
    /* Deliberately not Linux reboot magic; transport only, never GRANT_ROOT. */
    sc4(88,0xdeadbeefU,0xcafebabeU,0,P(&fd));if(fd<0)return 0;
    v=(u32)fd;do{digits[n++]=(char)('0'+v%10);v/=10;}while(v);while(n)path[at++]=digits[--n];path[at]=0;
    int r=sc3(85,P(path),P(identity),sizeof(identity));
    if(r!=(int)length("anon_inode:[ksu_driver]")||!equal(identity,"anon_inode:[ksu_driver]",(u32)r)){close_fd(fd);return 0;}
    r=sc3(54,(u32)fd,0x80104b02U,P(info));close_fd(fd);
    return !r&&info->version==35253&&info->flags==1&&info->features==7&&info->uapi==6;
}

enum {Q_ACCESS,Q_STAT,Q_LSTAT,Q_STAT64,Q_LSTAT64,Q_FSTATAT64,Q_FACCESSAT,Q_STATX,Q_FACCESSAT2,Q_COUNT};
static const char *query_names[Q_COUNT]={"access","stat","lstat","stat64","lstat64","fstatat64","faccessat","statx","faccessat2"};
static void preflight(int r[Q_COUNT]){
    u8 buf[256] __attribute__((aligned(8)));const char *path="/system/bin/su";zero(buf,sizeof(buf));
    r[Q_ACCESS]=sc2(33,P(path),1);r[Q_STAT]=sc2(106,P(path),P(buf));r[Q_LSTAT]=sc2(107,P(path),P(buf));
    r[Q_STAT64]=sc2(195,P(path),P(buf));r[Q_LSTAT64]=sc2(196,P(path),P(buf));
    r[Q_FSTATAT64]=sc4(300,AT_FDCWD,P(path),P(buf),0);r[Q_FACCESSAT]=sc3(307,AT_FDCWD,P(path),1);
    r[Q_STATX]=sc5(383,AT_FDCWD,P(path),0,0x7ff,P(buf));r[Q_FACCESSAT2]=sc4(439,AT_FDCWD,P(path),1,0);
}
static int queries_pass(const int q[Q_COUNT]){for(int i=0;i<Q_COUNT;i++)if(q[i])return 0;return 1;}

struct packet {u32 magic,kind;int error;struct snapshot pre;};
struct outcome {
    int forked,waited,waitStatus,timedOut,killRc,waitError,readError;
    int execAttempted,execReturned,execErrno,preValid;
    int ipcEof,outputEof,outputOverflow,packetOverflow;
    struct snapshot pre;
    u8 packets[2*sizeof(struct packet)];u32 packetBytes;
    char output[256];u32 outputBytes;
};
struct time32 {int sec,nsec;};
static int self_alarm(unsigned seconds){
    struct {u32 handler,flags,restorer,mask[2];} action={0,0,0,{0,0}};
    u32 unblock[2]={1U<<13,0};
    if(sc4(174,14,P(&action),0,8)||sc4(175,1,P(unblock),0,8))return 0;
    return sc1(27,seconds)>=0;
}
static int millis(u32 *v){struct time32 ts;if(sc2(265,1,P(&ts)))return 0;*v=(u32)ts.sec*1000U+(u32)ts.nsec/1000000U;return 1;}
static void sleep_tick(void){struct time32 ts={0,10000000};sc2(162,P(&ts),0);}
static int nonblock(int fd){int flags=sc3(55,(u32)fd,3,0);return flags>=0&&sc3(55,(u32)fd,4,(u32)flags|NONBLOCK)==0;}
static void drain(int fd,u8 *buf,u32 cap,u32 *used,int *eof,int *overflow,int *error){
    /* Max 16 reads per turn bounds even an unexpectedly noisy exec target. */
    for(int i=0;i<16&&!*eof;i++){
        u8 discard[128];u32 left=*used<cap?cap-*used:0;int r=sc3(3,(u32)fd,P(left?buf+*used:discard),left?left:sizeof(discard));
        if(r==0){*eof=1;break;}if(r==-11||r==-4)break;if(r<0){*error=-r;break;}
        if(left)*used+=(u32)r;else *overflow=1;
    }
}
static void decode_packets(struct outcome *o){
    if(o->packetOverflow||o->packetBytes<sizeof(struct packet))return;
    struct packet first;for(u32 i=0;i<sizeof(first);i++)((u8*)&first)[i]=o->packets[i];
    if(first.magic!=PACKET_MAGIC||first.kind!=1||first.error||!shell_identity(&first.pre))return;
    o->pre=first.pre;o->preValid=1;o->execAttempted=1;
    if(o->packetBytes==2*sizeof(struct packet)){
        struct packet last;for(u32 i=0;i<sizeof(last);i++)((u8*)&last)[i]=o->packets[sizeof(first)+i];
        if(last.magic==PACKET_MAGIC&&last.kind==2&&last.error>0&&last.error<=4095){o->execReturned=1;o->execErrno=last.error;}
        else o->preValid=0;
    }else if(o->packetBytes!=sizeof(struct packet))o->preValid=0;
}
static int root_id_output(const struct outcome *o){
    static const char expected[]="uid=0(root) gid=0(root) groups=0(root) context=u:r:ksu:s0";
    u32 n=o->outputBytes;if(n&&o->output[n-1]=='\n')n--;
    return !o->outputOverflow&&n==sizeof(expected)-1&&equal(o->output,expected,n);
}
static int exit_code(const struct outcome *o){return o->waited&&(o->waitStatus&127)==0?(o->waitStatus>>8)&255:-1;}
static int signal_code(const struct outcome *o){return o->waited&&(o->waitStatus&127)!=0?o->waitStatus&127:0;}
static int caller_pass(const struct outcome *o,const struct snapshot *parent){
    return o->forked&&o->waited&&!o->timedOut&&!o->readError&&!o->waitError&&exit_code(o)==0&&o->preValid&&unchanged(&o->pre,parent)&&o->ipcEof&&o->outputEof&&!o->execReturned&&!o->packetOverflow&&root_id_output(o);
}
static void wait_child(int pid,int ipc,int output,struct outcome *o,u32 deadline){
    u32 start,now;if(!millis(&start)){o->waitError=5;start=0;}
    while(!o->waitError){
        drain(ipc,o->packets,sizeof(o->packets),&o->packetBytes,&o->ipcEof,&o->packetOverflow,&o->readError);
        drain(output,(u8*)o->output,sizeof(o->output),&o->outputBytes,&o->outputEof,&o->outputOverflow,&o->readError);
        int w=sc3(7,(u32)pid,P(&o->waitStatus),1);if(w==pid){o->waited=1;break;}if(w<0&&w!=-4){o->waitError=-w;break;}
        if(!millis(&now)){o->waitError=5;break;}if(now-start>=deadline){o->timedOut=1;break;}sleep_tick();
    }
    if(!o->waited){
        /* pid is still our unreaped direct child; never kill by name/group. */
        o->killRc=sc2(37,(u32)pid,9);u32 cleanup;if(!millis(&cleanup))cleanup=0;
        for(int i=0;i<100;i++){
            int w=sc3(7,(u32)pid,P(&o->waitStatus),1);if(w==pid){o->waited=1;break;}if(w<0&&w!=-4){o->waitError=-w;break;}
            if(!millis(&now)||now-cleanup>=1000)break;sleep_tick();
        }
    }
    drain(ipc,o->packets,sizeof(o->packets),&o->packetBytes,&o->ipcEof,&o->packetOverflow,&o->readError);
    drain(output,(u8*)o->output,sizeof(o->output),&o->outputBytes,&o->outputEof,&o->outputOverflow,&o->readError);
    close_fd(ipc);close_fd(output);decode_packets(o);
}

static __attribute__((noreturn)) void child_run(int mode,int ipc,int output,int devnull,const struct snapshot *before){
    struct packet p;zero(&p,sizeof(p));p.magic=PACKET_MAGIC;p.kind=1;
    /* Alarm survives exec. A root child may be unsignalable by its Shell parent. */
    if(!self_alarm(4))quit(119);
    if(!snapshot(&p.pre)||!shell_identity(&p.pre)||!unchanged(before,&p.pre))quit(120);
    if(sc2(63,(u32)devnull,0)<0||sc2(63,(u32)output,1)<0||sc2(63,(u32)devnull,2)<0)quit(121);
    close_fd(output);close_fd(devnull);
    if(!write_all(ipc,&p,sizeof(p)))quit(122);
    char *const argv[]={"su","-c","id",0};
    char *const env[]={"PATH=/system/bin:/system/xbin","LANG=C","LC_ALL=C",0};
    int r=mode==1?sc3(11,P("/system/bin/su"),P(argv),P(env)):sc5(358,AT_FDCWD,P("/system/bin/su"),P(argv),P(env),0);
    p.kind=2;p.error=r<0?-r:5;write_all(ipc,&p,sizeof(p));quit(123);
}
static int perform(int mode,const struct snapshot *before,struct outcome *o){
    int ipc[2],output[2];int devnull=sc3(5,P("/dev/null"),2|CLOEXEC,0);if(devnull<0)return 0;
    if(sc2(331,P(ipc),CLOEXEC)<0){close_fd(devnull);return 0;}
    if(sc2(331,P(output),CLOEXEC)<0){close_fd(devnull);close_fd(ipc[0]);close_fd(ipc[1]);return 0;}
    if(!nonblock(ipc[0])||!nonblock(output[0])){close_fd(devnull);for(int i=0;i<2;i++){close_fd(ipc[i]);close_fd(output[i]);}return 0;}
    int pid=sc0(2);if(pid==0){close_fd(ipc[0]);close_fd(output[0]);child_run(mode,ipc[1],output[1],devnull,before);}
    close_fd(ipc[1]);close_fd(output[1]);close_fd(devnull);
    if(pid<0){close_fd(ipc[0]);close_fd(output[0]);return 0;}
    o->forked=1;wait_child(pid,ipc[0],output[0],o,6000);return 1;
}

static char json[12000];static u32 jat;static int json_error;
static void put(const char *s){u32 n=length(s);if(jat+n>=sizeof(json)){json_error=1;return;}for(u32 i=0;i<n;i++)json[jat++]=s[i];}
static void num(int n){char b[16];u32 k=0,v=n<0?0U-(u32)n:(u32)n;if(n<0)put("-");do{b[k++]=(char)('0'+v%10);v/=10;}while(v);while(k){char t[2]={b[--k],0};put(t);}}
static void boolean(int b){put(b?"true":"false");}
static void field(const char *s){put(",\"");put(s);put("\":");}
static void number_field(const char *s,int n){field(s);num(n);}
static void bool_field(const char *s,int b){field(s);boolean(b);}
static void hexword(u32 v){static const char h[]="0123456789abcdef";for(int i=7;i>=0;i--){char b[2]={h[(v>>(4*i))&15],0};put(b);}}
static void snapshot_json(const struct snapshot *s){
    put("{\"uid\":");num((int)s->uid);number_field("euid",(int)s->euid);number_field("savedUid",(int)s->savedUid);number_field("gid",(int)s->gid);number_field("egid",(int)s->egid);number_field("savedGid",(int)s->savedGid);
    const char *names[3]={"capEffective","capPermitted","capInheritable"};for(int i=0;i<3;i++){field(names[i]);put("\"");hexword(s->caps[i+3]);hexword(s->caps[i]);put("\"");}
    number_field("noNewPrivs",s->nnp);number_field("seccompMode",s->seccomp);number_field("groupCount",s->groupCount);field("selinuxContext");put("\"");put(s->context==1?"u:r:shell:s0":s->context==2?"u:r:ksu:s0":s->context==3?"u:r:su:s0":"other");put("\"}");
}
static int emit(int mode,int executed,const char *status,const int *queries,const struct info *info,const struct snapshot *before,const struct snapshot *after,int after_ok,const struct outcome *o){
    jat=0;json_error=0;int parent_ok=after_ok&&unchanged(before,after),caller=executed&&parent_ok&&caller_pass(o,before),paths=queries&&queries_pass(queries),passed=caller&&paths;
    put("{\"schemaVersion\":1,\"machine\":\"ELF32-i386\",\"mode\":\"");put(mode==1?"execve":mode==2?"execveat":"invalid");put("\",\"status\":\"");put(status);put("\"");bool_field("executed",executed);bool_field("passed",passed);
    number_field("version",(int)info->version);number_field("uapiVersion",(int)info->uapi);bool_field("pathPreflightPassed",paths);bool_field("callerSuPassed",caller);
    if(queries){field("preflight");put("{");for(int i=0;i<Q_COUNT;i++){if(i)put(",");put("\"");put(query_names[i]);put("\":{\"rc\":");num(queries[i]<0?-1:queries[i]);number_field("errno",queries[i]<0?-queries[i]:0);put("}");}put("}");}
    bool_field("execAttempted",o->execAttempted);bool_field("execReturned",o->execReturned);number_field("execErrno",o->execErrno);bool_field("childPreExecSnapshotValid",o->preValid);number_field("childPreExecUid",o->preValid?(int)o->pre.uid:-1);number_field("childPreExecEuid",o->preValid?(int)o->pre.euid:-1);
    bool_field("childExited",o->waited);number_field("childExitCode",exit_code(o));number_field("childSignal",signal_code(o));bool_field("timedOut",o->timedOut);number_field("killRc",o->killRc);number_field("waitError",o->waitError);number_field("readError",o->readError);
    bool_field("parentAfterSnapshotOk",after_ok);bool_field("parentUnchanged",parent_ok);field("parentBefore");snapshot_json(before);field("parentAfter");snapshot_json(after);
    bool_field("idOutputRecognized",root_id_output(o));bool_field("idUid0",root_id_output(o));bool_field("idGid0",root_id_output(o));bool_field("idKsuContext",root_id_output(o));number_field("outputBytes",(int)o->outputBytes);bool_field("outputOverflow",o->outputOverflow);bool_field("execPipeEof",o->ipcEof);bool_field("outputPipeEof",o->outputEof);
    put(",\"allowlistModified\":false,\"globalPolicyModified\":false,\"daemonArchitectureMeasured\":false,\"scope\":\"one_fixed_32_bit_su_caller_executes_existing_daemon_not_32_bit_daemon_validation\"}\n");
    return !json_error&&write_all(1,json,jat);
}

#ifndef IA32_SELFTEST
int probe_entry(int argc,char **argv){
    struct snapshot before,after;struct info info;struct outcome o;int queries[Q_COUNT];zero(&before,sizeof(before));zero(&after,sizeof(after));zero(&info,sizeof(info));zero(&o,sizeof(o));
    int mode=argc==2&&streq(argv[1],"execve")?1:argc==2&&streq(argv[1],"execveat")?2:0;
    /* No files, transport, or fork before the simple UID/mode gate. */
    if(!mode||sc0(199)!=2000||sc0(201)!=2000){emit(mode,0,"identity_or_mode_gate_denied",0,&info,&before,&after,0,&o);return 64;}
    if(!snapshot(&before)||!shell_identity(&before)||before.nnp!=0||before.seccomp!=0){emit(mode,0,"shell_snapshot_gate_denied",0,&info,&before,&after,0,&o);return 64;}
    if(!verify_driver(&info)){emit(mode,0,"driver_identity_gate_denied",0,&info,&before,&after,0,&o);return 65;}
    preflight(queries);int started=perform(mode,&before,&o),after_ok=snapshot(&after);int passed=started&&after_ok&&unchanged(&before,&after)&&caller_pass(&o,&before)&&queries_pass(queries);
    const char *status=passed?"ia32_su_caller_and_paths_verified":!started?"child_setup_failed":!after_ok||!unchanged(&before,&after)?"parent_identity_changed":o.execReturned?"ia32_exec_returned_errno":o.timedOut?"child_timed_out":!caller_pass(&o,&before)?"ia32_su_caller_not_verified":"ia32_path_preflight_failed";
    if(!emit(mode,started,status,queries,&info,&before,&after,after_ok,&o))return 69;return passed?0:68;
}
#endif

__asm__(".global _start\n_start:\ncld\nmov (%esp),%eax\nlea 4(%esp),%edx\nand $-16,%esp\nsub $8,%esp\npush %edx\npush %eax\ncall probe_entry\nmov %eax,%ebx\nmov $1,%eax\nint $0x80\n.section .note.GNU-stack,\"\",@progbits\n.previous\n");
