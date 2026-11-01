/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Self-owned Bionic process for bounded, read-only Frida attachment validation. */
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
static volatile sig_atomic_t stop;
static char run_nonce[33];
__attribute__((visibility("default"),noinline,used)) int resukisu_probe_value(void){return 3525306;}
__attribute__((visibility("default"),noinline,used)) const char *resukisu_probe_nonce(void){return run_nonce;}
static void stopped(int signal){(void)signal;stop=1;}
static int nonce_valid(const char *s){if(strlen(s)!=32)return 0;for(unsigned i=0;i<32;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;return 1;}
static int target_platform(void){
#ifdef __ANDROID__
    char sdk[PROP_VALUE_MAX];return __system_property_get("ro.build.version.sdk",sdk)>0&&!strcmp(sdk,"35");
#else
    return 0;
#endif
}
int main(int argc,char **argv){
    if(argc!=3||strcmp(argv[1],"--task-run")||!nonce_valid(argv[2])||getuid()!=2000||geteuid()!=2000||!target_platform()){
        puts("{\"schemaVersion\":1,\"status\":\"identity_platform_or_nonce_gate_denied\",\"executed\":false}");return 64;
    }
    memcpy(run_nonce,argv[2],33);
    struct sigaction sa={.sa_handler=stopped};sigemptyset(&sa.sa_mask);
    if(sigaction(SIGTERM,&sa,NULL)||sigaction(SIGINT,&sa,NULL)||sigaction(SIGALRM,&sa,NULL))return 65;
    sigset_t unblocked;sigemptyset(&unblocked);sigaddset(&unblocked,SIGTERM);sigaddset(&unblocked,SIGALRM);sigprocmask(SIG_UNBLOCK,&unblocked,NULL);
    alarm(45);
    printf("{\"schemaVersion\":1,\"status\":\"task_target_ready\",\"pid\":%d,\"uid\":%u,\"euid\":%u,\"nonce\":\"%s\",\"expectedValue\":%d,\"selfDeadlineSeconds\":45}\n",getpid(),getuid(),geteuid(),run_nonce,resukisu_probe_value());fflush(stdout);
    while(!stop){struct timespec delay={.tv_sec=0,.tv_nsec=100000000};nanosleep(&delay,NULL);}
    puts("{\"schemaVersion\":1,\"status\":\"task_target_stopped\"}");return 0;
}
