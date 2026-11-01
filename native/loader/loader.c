/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "expected_probe.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define PIN_COUNT 235
#define PROC_SUPER_MAGIC 0x9fa0

static const char *const allowed[PIN_COUNT] = {
    "__SCT__cond_resched",
    "__SCT__might_resched",
    "___ratelimit",
    "__alloc_pages",
    "__check_object_size",
    "__copy_overflow",
    "__cpu_online_mask",
    "__default_kernel_pte_mask",
    "__dynamic_pr_debug",
    "__fentry__",
    "__free_pages",
    "__get_task_comm",
    "__get_user_4",
    "__get_user_8",
    "__hashtab_insert",
    "__init_waitqueue_head",
    "__kmalloc",
    "__mutex_init",
    "__num_online_cpus",
    "__put_cred",
    "__put_task_struct",
    "__put_user_8",
    "__rcu_read_lock",
    "__rcu_read_unlock",
    "__stack_chk_fail",
    "__task_pid_nr_ns",
    "__tracepoint_sys_enter",
    "__wake_up",
    "__warn_printk",
    "__x64_sys_close",
    "__x64_sys_setns",
    "__x64_sys_unshare",
    "__x86_indirect_thunk_r11",
    "__x86_return_thunk",
    "_copy_from_user",
    "_copy_to_iter",
    "_copy_to_user",
    "_find_first_bit",
    "_find_next_bit",
    "_printk",
    "_raw_read_lock",
    "_raw_read_unlock",
    "_raw_spin_lock",
    "_raw_spin_lock_irq",
    "_raw_spin_lock_irqsave",
    "_raw_spin_unlock",
    "_raw_spin_unlock_irq",
    "_raw_spin_unlock_irqrestore",
    "_raw_write_lock_irq",
    "_raw_write_unlock_irq",
    "abort_creds",
    "alloc_uid",
    "anon_inode_getfile",
    "anon_inode_getfile_secure",
    "avc_has_perm",
    "avc_ss_reset",
    "avtab_alloc",
    "avtab_destroy",
    "avtab_insert_nonunique",
    "avtab_search_node",
    "avtab_search_node_next",
    "bcmp",
    "bin2hex",
    "boot_cpu_data",
    "bsearch",
    "change_pid",
    "commit_creds",
    "cond_compute_av",
    "copy_from_user_nofault",
    "copy_to_user_nofault",
    "crypto_alloc_shash",
    "crypto_destroy_tfm",
    "crypto_shash_digest",
    "current_is_single_threaded",
    "current_task",
    "d_path",
    "dentry_open",
    "down_read",
    "down_write",
    "ebitmap_cmp",
    "ebitmap_contains",
    "ebitmap_destroy",
    "ebitmap_get_bit",
    "ebitmap_set_bit",
    "ext4_unregister_sysfs",
    "fd_install",
    "fget",
    "filp_close",
    "filp_open",
    "find_pid_ns",
    "find_task_by_vpid",
    "find_vpid",
    "finish_wait",
    "fput",
    "free_uid",
    "fsnotify_add_mark",
    "fsnotify_alloc_group",
    "fsnotify_destroy_mark",
    "fsnotify_init_mark",
    "fsnotify_put_group",
    "fsnotify_put_mark",
    "generic_file_llseek",
    "get_pid_task",
    "get_task_cred",
    "get_unused_fd_flags",
    "groups_alloc",
    "groups_free",
    "groups_sort",
    "ihold",
    "init_mm",
    "init_pid_ns",
    "init_task",
    "init_wait_entry",
    "install_session_keyring_to_cred",
    "iput",
    "iterate_dir",
    "kallsyms_lookup",
    "kallsyms_lookup_name",
    "kallsyms_lookup_size_offset",
    "kallsyms_on_each_symbol",
    "kasprintf",
    "kern_path",
    "kernel_read",
    "kernel_write",
    "kfree",
    "kmalloc_caches",
    "kmalloc_large",
    "kmalloc_trace",
    "kmemdup",
    "kmemdup_nul",
    "kobject_del",
    "krealloc",
    "kstrdup",
    "kstrtouint",
    "kstrtoull",
    "ktime_get",
    "kvfree",
    "kvfree_call_rcu",
    "kvmalloc_node",
    "make_kgid",
    "memchr",
    "memcpy",
    "memset",
    "mls_compute_context_len",
    "mls_context_to_sid",
    "mls_sid_to_context",
    "mnt_drop_write",
    "mnt_want_write",
    "mntns_operations",
    "module_put",
    "mutex_lock",
    "mutex_lock_interruptible",
    "mutex_unlock",
    "native_set_fixmap",
    "noop_llseek",
    "ns_get_path",
    "override_creds",
    "page_offset_base",
    "param_ops_bool",
    "param_ops_string",
    "path_get",
    "path_mount",
    "path_put",
    "path_umount",
    "policydb_context_isvalid",
    "policydb_destroy",
    "policydb_filenametr_search",
    "policydb_load_isids",
    "policydb_read",
    "policydb_write",
    "prepare_creds",
    "prepare_to_wait_event",
    "put_unused_fd",
    "queue_work_on",
    "refcount_warn_saturate",
    "register_kprobe",
    "register_kretprobe",
    "revert_creds",
    "schedule",
    "scnprintf",
    "seccomp_filter_release",
    "security_bounded_transition",
    "security_release_secctx",
    "security_secctx_to_secid",
    "security_secid_to_secctx",
    "selinux_blob_sizes",
    "selinux_state",
    "selinux_status_update_policyload",
    "selnl_notify_policyload",
    "set_cred_ucounts",
    "set_fs_pwd",
    "set_groups",
    "shmem_file_setup",
    "sidtab_context_to_sid",
    "sidtab_destroy",
    "sidtab_search_entry",
    "sidtab_sid2str_get",
    "sidtab_sid2str_put",
    "snprintf",
    "sort",
    "sprintf",
    "sscanf",
    "static_key_initialized",
    "stop_machine",
    "strchr",
    "strcmp",
    "strlen",
    "strncmp",
    "strncpy",
    "strncpy_from_user",
    "strncpy_from_user_nofault",
    "strndup_user",
    "strnlen",
    "strscpy",
    "strsep",
    "strstr",
    "symtab_insert",
    "symtab_search",
    "synchronize_rcu",
    "synchronize_srcu",
    "system_wq",
    "task_work_add",
    "tasklist_lock",
    "tracepoint_probe_register_prio",
    "tracepoint_probe_unregister",
    "tracepoint_srcu",
    "try_module_get",
    "unregister_kprobe",
    "unregister_kretprobe",
    "up_read",
    "up_write",
    "vfree",
    "vmalloc",
    "vmemmap_base",
    "vzalloc",
};

struct binding { Elf64_Sym *sym; uint64_t addr; };
struct status {
    const char *stage, *module_stage;
    int error, module_error, report_error, proc_owned, pinned;
    unsigned symbols, resolved, missing;
    char boot_id[37];
};
static struct status status = {.stage="entered", .module_stage="not_attempted", .missing=PIN_COUNT};

static uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32-n)); }
static void sha_block(uint32_t h[8], const unsigned char p[64])
{
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t w[64],a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
    for (unsigned i=0;i<16;i++) w[i]=(uint32_t)p[i*4]<<24|(uint32_t)p[i*4+1]<<16|(uint32_t)p[i*4+2]<<8|p[i*4+3];
    for (unsigned i=16;i<64;i++) {
        uint32_t x=w[i-15], y=w[i-2];
        w[i]=w[i-16]+(rotr(x,7)^rotr(x,18)^(x>>3))+w[i-7]+(rotr(y,17)^rotr(y,19)^(y>>10));
    }
    for (unsigned i=0;i<64;i++) {
        uint32_t t1=v+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^((~e)&g))+k[i]+w[i];
        uint32_t t2=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));
        v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
}

static void sha256(const unsigned char *p, size_t n, unsigned char out[32])
{
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint64_t bits=(uint64_t)n*8;
    unsigned char tail[128]={0};
    while (n>=64) { sha_block(h,p);p+=64;n-=64; }
    memcpy(tail,p,n);tail[n]=0x80;
    size_t total=n<56?64:128;
    for (unsigned i=0;i<8;i++) tail[total-1-i]=(unsigned char)(bits>>(8*i));
    sha_block(h,tail);if (total==128) sha_block(h,tail+64);
    for (unsigned i=0;i<8;i++) for (unsigned j=0;j<4;j++) out[i*4+j]=(unsigned char)(h[i]>>(24-j*8));
}

static int range(size_t n, uint64_t off, uint64_t size) { return off<=n && size<=n-off; }
static const char *string_at(const unsigned char *p, size_t size, uint32_t off)
{
    if (off>=size || !memchr(p+off,0,size-off)) return NULL;
    return (const char *)p+off;
}
static int pin_index(const char *name)
{
    for (unsigned i=0;i<PIN_COUNT;i++) if (!strcmp(name,allowed[i])) return (int)i;
    return -1;
}

/* Validate the pinned image again before mutating only its private symbol table. */
static int validate_elf(unsigned char *p, size_t n, struct binding bindings[PIN_COUNT])
{
    unsigned char digest[32];
    Elf64_Ehdr *eh;
    Elf64_Shdr *sh, *symtab=NULL, *strings;
    unsigned seen[PIN_COUNT]={0};
    int modinfo=0, versions=0, this_module=0;
    sha256(p,n,digest);
    if (n!=EXPECTED_PROBE_BYTES || memcmp(digest,expected_probe_sha256,32)) return EKEYREJECTED;
    if (n<sizeof(*eh)) return ENOEXEC;
    eh=(Elf64_Ehdr *)p;
    if (memcmp(eh->e_ident,ELFMAG,SELFMAG) || eh->e_ident[EI_CLASS]!=ELFCLASS64 ||
        eh->e_ident[EI_DATA]!=ELFDATA2LSB || eh->e_ident[EI_VERSION]!=EV_CURRENT ||
        eh->e_type!=ET_REL || eh->e_machine!=EM_X86_64 || eh->e_version!=EV_CURRENT ||
        eh->e_ehsize!=sizeof(*eh) || eh->e_shentsize!=sizeof(Elf64_Shdr) ||
        eh->e_phoff || eh->e_phnum || !eh->e_shnum || eh->e_shnum>128 ||
        eh->e_shstrndx>=eh->e_shnum || eh->e_shstrndx==SHN_UNDEF ||
        eh->e_shoff%_Alignof(Elf64_Shdr) ||
        !range(n,eh->e_shoff,(uint64_t)eh->e_shnum*sizeof(Elf64_Shdr))) return ENOEXEC;
    sh=(Elf64_Shdr *)(p+eh->e_shoff);
    strings=&sh[eh->e_shstrndx];
    if (strings->sh_type!=SHT_STRTAB || !range(n,strings->sh_offset,strings->sh_size)) return ENOEXEC;
    for (unsigned i=0;i<eh->e_shnum;i++) {
        Elf64_Shdr *s=&sh[i];
        if (s->sh_type!=SHT_NOBITS && !range(n,s->sh_offset,s->sh_size)) return ENOEXEC;
        if (s->sh_addralign && (s->sh_addralign&(s->sh_addralign-1))) return ENOEXEC;
        const char *name=string_at(p+strings->sh_offset,strings->sh_size,s->sh_name);
        if (!name) return ENOEXEC;
        if (s->sh_type==SHT_SYMTAB) { if (symtab) return ENOEXEC;symtab=s; }
        if (!strcmp(name,"__versions")) { if (s->sh_size) return ELIBBAD;versions++; }
        if (!strcmp(name,".gnu.linkonce.this_module")) { if (s->sh_size!=960) return ELIBBAD;this_module++; }
        if (!strcmp(name,".modinfo")) {
            if (s->sh_type!=SHT_PROGBITS) return ENOEXEC;
            size_t off=0;int has_name=0,has_vermagic=0;
            while (off<s->sh_size) {
                const char *entry=string_at(p+s->sh_offset,s->sh_size,(uint32_t)off);
                if (!entry) return ENOEXEC;
                if (!strcmp(entry,"name=kernelsu")) has_name++;
                if (!strncmp(entry,"vermagic=",9)) has_vermagic++;
                off+=strlen(entry)+1;
            }
            if (has_name!=1 || has_vermagic!=1) return ELIBBAD;
            modinfo++;
        }
    }
    if (!symtab || modinfo!=1 || versions!=1 || this_module!=1 ||
        symtab->sh_entsize!=sizeof(Elf64_Sym) || symtab->sh_size%sizeof(Elf64_Sym) ||
        symtab->sh_offset%_Alignof(Elf64_Sym) || symtab->sh_link>=eh->e_shnum) return ENOEXEC;
    Elf64_Shdr *str=&sh[symtab->sh_link];
    if (str->sh_type!=SHT_STRTAB || !range(n,str->sh_offset,str->sh_size)) return ENOEXEC;
    Elf64_Sym *syms=(Elf64_Sym *)(p+symtab->sh_offset);
    size_t count=symtab->sh_size/sizeof(Elf64_Sym);
    for (size_t i=1;i<count;i++) {
        const char *name=string_at(p+str->sh_offset,str->sh_size,syms[i].st_name);
        if (!name) return ENOEXEC;
        if (syms[i].st_shndx!=SHN_UNDEF) continue;
        int index=pin_index(name);
        if (index<0 || seen[index]++ || ELF64_ST_BIND(syms[i].st_info)!=STB_GLOBAL) return ELIBBAD;
        bindings[index].sym=&syms[i];
        status.symbols++;
    }
    if (status.symbols!=PIN_COUNT) return ELIBBAD;
    for (unsigned i=0;i<PIN_COUNT;i++) if (seen[i]!=1) return ELIBBAD;
    for (unsigned i=0;i<eh->e_shnum;i++) {
        Elf64_Shdr *s=&sh[i];
        if (s->sh_type==SHT_REL) return ENOEXEC;
        if (s->sh_type!=SHT_RELA) continue;
        if (s->sh_entsize!=sizeof(Elf64_Rela) || s->sh_size%sizeof(Elf64_Rela) ||
            s->sh_offset%_Alignof(Elf64_Rela) || s->sh_link!=(unsigned)(symtab-sh) ||
            s->sh_info>=eh->e_shnum) return ENOEXEC;
        Elf64_Rela *relas=(Elf64_Rela *)(p+s->sh_offset);
        for (size_t j=0;j<s->sh_size/sizeof(*relas);j++) {
            unsigned type=ELF64_R_TYPE(relas[j].r_info),width;
            if (ELF64_R_SYM(relas[j].r_info)>=count) return ENOEXEC;
            switch(type) {
            case R_X86_64_NONE:width=0;break;
            case R_X86_64_64:width=8;break;
            case R_X86_64_PC32:case R_X86_64_PLT32:case R_X86_64_32:case R_X86_64_32S:width=4;break;
            default:return ENOEXEC;
            }
            if (!range(sh[s->sh_info].sh_size,relas[j].r_offset,width)) return ENOEXEC;
        }
    }
    return 0;
}

static int resolve_symbols(FILE *f, struct binding bindings[PIN_COUNT])
{
    char line[1024], name[512], extra[2], type;
    unsigned long long addr;
    while (fgets(line,sizeof(line),f)) {
        if (!strchr(line,'\n') && !feof(f)) return EOVERFLOW;
        int fields=sscanf(line,"%llx %c %511s %1s",&addr,&type,name,extra);
        if (fields!=3) continue; /* Only core kernel entries, never module symbols. */
        char *cut=strchr(name,'$');if (!cut) cut=strstr(name,".llvm.");if(cut)*cut=0;
        int index=pin_index(name);
        if (index<0 || bindings[index].addr || !addr) continue;
        bindings[index].addr=(uint64_t)addr;status.resolved++;
        if (status.resolved==PIN_COUNT) break;
    }
    if (ferror(f)) return EIO;
    status.missing=PIN_COUNT-status.resolved;
    if (status.missing) return ENOENT;
    for (unsigned i=0;i<PIN_COUNT;i++) {
        bindings[i].sym->st_shndx=SHN_ABS;
        bindings[i].sym->st_value=bindings[i].addr;
    }
    return 0;
}

static unsigned char *read_probe(size_t *length)
{
    struct stat st;
    int fd=open("/kernelsu.ko",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return NULL;
    if(fstat(fd,&st)<0){int e=errno;close(fd);errno=e;return NULL;}
    if(!S_ISREG(st.st_mode) || st.st_size!=EXPECTED_PROBE_BYTES){close(fd);errno=EINVAL;return NULL;}
    unsigned char *p=malloc(EXPECTED_PROBE_BYTES);
    if(!p){close(fd);return NULL;}
    size_t got=0;
    while(got<EXPECTED_PROBE_BYTES) {
        ssize_t n=read(fd,p+got,EXPECTED_PROBE_BYTES-got);
        if(n<0 && errno==EINTR)continue;
        if(n<=0){int e=n<0?errno:EIO;close(fd);free(p);errno=e;return NULL;}
        got+=(size_t)n;
    }
    close(fd);*length=got;return p;
}

static void read_boot_id(void)
{
    int fd=open("/proc/sys/kernel/random/boot_id",O_RDONLY|O_CLOEXEC);
    if(fd<0)return;
    char id[38]={0};ssize_t n=read(fd,id,sizeof(id)-1);close(fd);
    if(n<36)return;
    for(unsigned i=0;i<36;i++) {
        int hyphen=i==8||i==13||i==18||i==23;
        if(hyphen?id[i]!='-':!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return;
    }
    memcpy(status.boot_id,id,36);
}

static void write_report(void)
{
    int fd=open("/userspace-driver-boot.json",O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0){status.report_error=errno;return;}
    char text[512];
    int n=snprintf(text,sizeof(text),"{\"schemaVersion\":1,\"stage\":\"%s\",\"errno\":%d,\"moduleStage\":\"%s\",\"moduleErrno\":%d,\"symbolCount\":%u,\"resolvedCount\":%u,\"missingCount\":%u,\"pid\":1,\"probePinned\":%s,\"procOwned\":%s,\"reportErrno\":%d,\"bootId\":\"%s\"}\n",
        status.stage,status.error,status.module_stage,status.module_error,status.symbols,status.resolved,status.missing,status.pinned?"true":"false",status.proc_owned?"true":"false",status.report_error,status.boot_id);
    if(n<0 || (size_t)n>=sizeof(text)){status.report_error=EOVERFLOW;close(fd);return;}
    size_t done=0;
    while(done<(size_t)n) {
        ssize_t w=write(fd,text+done,(size_t)n-done);
        if(w<0 && errno==EINTR)continue;
        if(w<=0){status.report_error=w<0?errno:EIO;break;}
        done+=(size_t)w;
    }
    close(fd);
}

int main(int argc, char **argv, char **envp)
{
    (void)argc;
    /* Keep this gate before every mount, file access, module load or exec. */
    if(getpid()!=1 || getuid()!=0 || geteuid()!=0)return 64;
    struct binding bindings[PIN_COUNT]={0};
    size_t length=0;
    unsigned char *module=NULL;
    FILE *symbols=NULL;
    if(mount("proc","/proc","proc",MS_NOSUID|MS_NODEV|MS_NOEXEC,NULL)==0)status.proc_owned=1;
    else if(errno!=EBUSY){status.error=errno;status.stage="proc_mount_failed";goto handoff;}
    struct statfs fs;
    if(statfs("/proc",&fs)<0){status.error=errno;status.stage="proc_type_failed";goto handoff;}
    if(fs.f_type!=PROC_SUPER_MAGIC){status.error=ENODEV;status.stage="proc_type_failed";goto handoff;}
    read_boot_id();
    module=read_probe(&length);
    if(!module){status.error=errno;status.stage="probe_read_failed";goto handoff;}
    status.error=validate_elf(module,length,bindings);
    if(status.error){status.stage="probe_validation_failed";goto handoff;}
    status.pinned=1;
    symbols=fopen("/proc/kallsyms","re");
    if(!symbols){status.error=errno;status.stage="symbols_open_failed";goto handoff;}
    status.error=resolve_symbols(symbols,bindings);
    fclose(symbols);symbols=NULL;
    if(status.error){status.stage="symbols_unresolved";goto handoff;}
    status.module_stage="attempted";status.stage="module_loading";write_report();
    if(syscall(SYS_init_module,module,length,"norc=1 preinit=0")<0) {
        status.module_error=errno;status.module_stage="failed";
    } else status.module_stage="loaded";
    status.stage="handoff_pending";
handoff:
    free(module);
    if(symbols)fclose(symbols);
    write_report();
    if(status.proc_owned && umount2("/proc",0)<0) {status.error=errno;status.stage="proc_unmount_failed";write_report();}
    execve("/init.real",argv,envp);
    status.error=errno;status.stage="exec_failed";write_report();
    /* PID1 must not exit if the original init itself cannot be executed. */
    for(;;)pause();
}
