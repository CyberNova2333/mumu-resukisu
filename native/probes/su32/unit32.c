/* SPDX-License-Identifier: GPL-3.0-or-later */
#define IA32_SELFTEST
#include "probe.c"

static int tests;
static void check(int truth){tests++;if(!truth){char b[80];(void)b;jat=0;put("{\"hostUnitFailed\":");num(tests);put("}\n");write_all(1,json,jat);quit(1);}}
static struct snapshot model_shell(void){struct snapshot s;zero(&s,sizeof(s));s.uid=s.euid=s.savedUid=s.gid=s.egid=s.savedGid=2000;s.context=1;return s;}
static struct outcome model_success(const struct snapshot *s){
    struct outcome o;zero(&o,sizeof(o));o.forked=o.waited=o.preValid=o.execAttempted=o.ipcEof=o.outputEof=1;o.pre=*s;
    const char *id="uid=0(root) gid=0(root) groups=0(root) context=u:r:ksu:s0\n";o.outputBytes=length(id);for(u32 i=0;i<o.outputBytes;i++)o.output[i]=id[i];return o;
}
static void classify_tests(void){
    struct snapshot s=model_shell();struct outcome o=model_success(&s);check(caller_pass(&o,&s));
    o.output[4]='2';check(!caller_pass(&o,&s));o=model_success(&s);o.output[o.outputBytes++]='x';check(!caller_pass(&o,&s));
    o=model_success(&s);o.outputOverflow=1;check(!caller_pass(&o,&s));o=model_success(&s);o.timedOut=1;check(!caller_pass(&o,&s));
    o=model_success(&s);o.waitStatus=123<<8;check(!caller_pass(&o,&s));o=model_success(&s);o.waitStatus=14;check(!caller_pass(&o,&s));
    o=model_success(&s);o.execReturned=1;o.execErrno=2;check(!caller_pass(&o,&s));o=model_success(&s);o.pre.uid=0;check(!caller_pass(&o,&s));
    o=model_success(&s);o.ipcEof=0;check(!caller_pass(&o,&s));o=model_success(&s);o.outputEof=0;check(!caller_pass(&o,&s));
    o=model_success(&s);o.pre.groups[0]=123;check(!caller_pass(&o,&s));
    int q[Q_COUNT]={0};check(queries_pass(q));q[Q_STAT]=-75;check(!queries_pass(q));
    struct packet p;zero(&p,sizeof(p));p.magic=PACKET_MAGIC;p.kind=1;p.pre=s;
    zero(&o,sizeof(o));for(u32 i=0;i<sizeof(p);i++)o.packets[i]=((u8*)&p)[i];o.packetBytes=sizeof(p);decode_packets(&o);check(o.preValid&&o.execAttempted&&!o.execReturned);
    p.kind=2;p.error=2;for(u32 i=0;i<sizeof(p);i++)o.packets[sizeof(p)+i]=((u8*)&p)[i];o.packetBytes=2*sizeof(p);decode_packets(&o);check(o.preValid&&o.execReturned&&o.execErrno==2);
    o.packetBytes=sizeof(p)+1;decode_packets(&o);check(!o.preValid);
    zero(&o,sizeof(o));o.packetBytes=sizeof(p);decode_packets(&o);check(!o.preValid);
    o=model_success(&s);struct info info={35253,1,7,6};int all[Q_COUNT]={0};check(emit(1,1,"host_model_only",all,&info,&s,&s,1,&o));
}
static void actual_wait_test(int alarm_child){
    int a[2],b[2];check(sc2(331,P(a),CLOEXEC)==0&&sc2(331,P(b),CLOEXEC)==0);check(nonblock(a[0])&&nonblock(b[0]));
    int pid=sc0(2);check(pid>=0);
    if(pid==0){close_fd(a[0]);close_fd(b[0]);if(alarm_child&&!self_alarm(1))quit(100);for(;;)sc0(29);}
    close_fd(a[1]);close_fd(b[1]);struct outcome o;zero(&o,sizeof(o));o.forked=1;
    wait_child(pid,a[0],b[0],&o,alarm_child?2000:100);
    check(o.waited&&signal_code(&o)==(alarm_child?14:9));check(o.timedOut==!alarm_child);check(o.ipcEof&&o.outputEof);
}
static void actual_exec_test(int mode){
    int a[2],b[2];check(sc2(331,P(a),CLOEXEC)==0&&sc2(331,P(b),CLOEXEC)==0);check(nonblock(a[0])&&nonblock(b[0]));int pid=sc0(2);check(pid>=0);
    if(pid==0){
        close_fd(a[0]);close_fd(b[0]);close_fd(b[1]);if(!self_alarm(2))quit(100);
        struct packet p;zero(&p,sizeof(p));p.magic=PACKET_MAGIC;p.kind=1;p.pre=model_shell();if(!write_all(a[1],&p,sizeof(p)))quit(101);
        char *const av[]={"true",0};char *const env[]={0};int r=mode==1?sc3(11,P("/bin/true"),P(av),P(env)):sc5(358,AT_FDCWD,P("/bin/true"),P(av),P(env),0);(void)r;quit(102);
    }
    close_fd(a[1]);close_fd(b[1]);struct outcome o;zero(&o,sizeof(o));o.forked=1;wait_child(pid,a[0],b[0],&o,3000);
    check(o.waited&&exit_code(&o)==0&&!o.timedOut);check(o.preValid&&!o.execReturned&&o.ipcEof);check(o.outputBytes==0&&o.outputEof);
}
int probe_entry(int argc,char **argv){
    (void)argc;(void)argv;classify_tests();actual_wait_test(1);actual_wait_test(0);actual_exec_test(1);actual_exec_test(2);
    jat=0;put("{\"hostTestsPassed\":");num(tests);put(",\"actualI386AlarmWaitAndBothExecEntrypoints\":true,\"kernelSuTested\":false}\n");return write_all(1,json,jat)?0:1;
}
