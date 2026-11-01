/* SPDX-License-Identifier: GPL-3.0-or-later */
#define GRANT_PROBE_UNIT_TEST
#include "probe.c"
#include <assert.h>
int main(void) {
    struct packet p={.magic=0x47525036,.before_ok=1,.after_ok=1,.attempted=1,.rc=-1,.error=EPERM};
    p.before.uid=p.before.euid=p.before.suid=2000;p.before.gid=p.before.egid=p.before.sgid=2000;
    strcpy(p.before.context,"u:r:shell:s0");p.after=p.before;
    assert(!strcmp(classify(0,1,1,1,&p),"denied_and_child_identity_unchanged"));
    assert(!strcmp(classify(1,1,1,1,&p),"denied_after_expected_grant"));
    assert(!strcmp(classify(0,0,1,1,&p),"probe_incomplete"));
    assert(!strcmp(classify(0,1,0,1,&p),"probe_incomplete"));
    assert(!strcmp(classify(0,1,1,0,&p),"parent_identity_changed"));
    p.after.euid=0;assert(!strcmp(classify(0,1,1,1,&p),"unexpected_authorization_result"));p.after=p.before;
    p.after.cap_effective=1;assert(!strcmp(classify(0,1,1,1,&p),"unexpected_authorization_result"));p.after=p.before;
    p.after.seccomp=2;assert(!strcmp(classify(0,1,1,1,&p),"unexpected_authorization_result"));p.after=p.before;
    p.after.groups[0]=3003;p.after.group_count=1;assert(!same_groups(&p.before,&p.after));assert(!strcmp(classify(0,1,1,1,&p),"unexpected_authorization_result"));p.after=p.before;
    strcpy(p.after.context,"u:r:ksu:s0");assert(!strcmp(classify(0,1,1,1,&p),"unexpected_authorization_result"));p.after=p.before;
    p.rc=0;p.error=0;p.after.uid=p.after.euid=p.after.gid=p.after.egid=0;
    assert(!strcmp(classify(0,1,1,1,&p),"unexpected_grant_child_exited"));
    assert(!strcmp(classify(1,1,1,1,&p),"granted_default_root_identity_child_exited"));
    p.error=EPERM;assert(!strcmp(classify(1,1,1,1,&p),"unexpected_authorization_result"));
    p.before_ok=0;assert(!strcmp(classify(1,1,1,1,&p),"probe_incomplete"));
    assert(!strcmp(context_label("u:r:shell:s0"),"u:r:shell:s0"));
    assert(!strcmp(context_label("u:r:ksu:s0"),"u:r:ksu:s0"));
    assert(!strcmp(context_label("private-label-with-\"quotes"),"other"));
    int st=0,to=0;pid_t pid=fork();assert(pid>=0);if(!pid)_exit(19);
    assert(!bounded_wait(pid,1000,&st,&to)&&WIFEXITED(st)&&WEXITSTATUS(st)==19&&!to);
    pid=fork();assert(pid>=0);if(!pid){for(;;)pause();}
    assert(!bounded_wait(pid,0,&st,&to)&&WIFSIGNALED(st)&&WTERMSIG(st)==SIGKILL&&to);
    to=0;pid=fork();assert(pid>=0);if(!pid){assert(!child_alarm(1));for(;;)pause();}
    assert(!bounded_wait(pid,3000,&st,&to)&&WIFSIGNALED(st)&&WTERMSIG(st)==SIGALRM&&!to);
    /* Mock outcomes test classification only; no KSU ioctl occurs on host. */
    puts("PASS mocked denial/grant/failure classification; identity and capability/context/group changes; label redaction; owned-child exit, parent timeout, child self-alarm");
    return 0;
}
