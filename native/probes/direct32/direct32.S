/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Freestanding IA32 direct ordinary FD trial, no GRANT_ROOT or su. */
.section .text
.global _start
_start:
    cld
    mov $199, %eax
    int $0x80
    cmp $2000, %eax
    jne denied
    mov $201, %eax
    int $0x80
    cmp $2000, %eax
    jne denied
    /* Linux IA32 capget(v3): require no effective CAP_SYS_BOOT. */
    mov $184, %eax
    mov $cap_header, %ebx
    mov $cap_words, %ecx
    int $0x80
    test %eax, %eax
    jne capability_denied
    testl $0x00400000, cap_words
    jne capability_denied
    /* Read-only PR_GET_SECCOMP, require mode0 for this transport-only trial. */
    mov $172, %eax
    mov $21, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    int $0x80
    test %eax, %eax
    jne capability_denied
    /* Non-Linux magic. Return errno is not the FD transport result. */
    mov $88, %eax
    mov $0xdeadbeef, %ebx
    mov $0xcafebabe, %ecx
    xor %edx, %edx
    mov $driver_fd, %esi
    int $0x80
    mov driver_fd, %eax
    test %eax, %eax
    js no_fd
    /* Format only our returned nonnegative fd into a bounded proc path. */
    mov $fd_path_end-1, %edi
fd_digits:
    xor %edx, %edx
    mov $10, %ecx
    div %ecx
    add $48, %dl
    dec %edi
    mov %dl, (%edi)
    test %eax, %eax
    jnz fd_digits
    sub $fd_prefix_len, %edi
    mov %edi, %ebx
    mov $fd_prefix, %esi
    mov $fd_prefix_len, %ecx
    rep movsb
    mov $85, %eax
    mov $fd_buf, %ecx
    mov $64, %edx
    int $0x80
    cmp $fd_name_len, %eax
    jne bad_fd
    mov $fd_buf, %esi
    mov $fd_name, %edi
    mov $fd_name_len, %ecx
    repe cmpsb
    jne bad_fd
    mov $54, %eax
    mov driver_fd, %ebx
    mov $0x80104b02, %ecx
    mov $info, %edx
    int $0x80
    test %eax, %eax
    jne bad_ioctl
    cmpl $35253, info
    jne bad_info
    cmpl $1, info+4
    jne bad_info
    cmpl $7, info+8
    jne bad_info
    cmpl $6, info+12
    jne bad_info
    mov $199, %eax
    int $0x80
    cmp $2000, %eax
    jne changed_uid
    mov $201, %eax
    int $0x80
    cmp $2000, %eax
    jne changed_uid
    mov $ok, %edi
    mov $ok_len, %edx
    xor %ebp, %ebp
    jmp finish
bad_fd:
    mov $fd_error, %edi
    mov $fd_error_len, %edx
    mov $65, %ebp
    jmp finish
bad_ioctl:
    mov $ioctl_error, %edi
    mov $ioctl_error_len, %edx
    mov $66, %ebp
    jmp finish
bad_info:
    mov $info_error, %edi
    mov $info_error_len, %edx
    mov $67, %ebp
    jmp finish
changed_uid:
    mov $uid_error, %edi
    mov $uid_error_len, %edx
    mov $68, %ebp
    jmp finish
no_fd:
    mov $fd_absent, %edi
    mov $fd_absent_len, %edx
    mov $71, %ebp
    jmp finish
capability_denied:
    mov $gate_error, %edi
    mov $gate_error_len, %edx
    mov $72, %ebp
finish:
    mov driver_fd, %ebx
    test %ebx, %ebx
    js output_start
    mov $6, %eax
    int $0x80
output_start:
    mov %edi, %ecx
output:
    mov $4, %eax
    mov $1, %ebx
    int $0x80
    cmp $-4, %eax
    je output
    test %eax, %eax
    jle output_failed
    add %eax, %ecx
    sub %eax, %edx
    jnz output
    mov %ebp, %ebx
    jmp leave
output_failed:
    mov $69, %ebx
    jmp leave
denied:
    mov $64, %ebx
leave:
    mov $1, %eax
    int $0x80

.section .rodata
fd_prefix: .ascii "/proc/self/fd/"
.set fd_prefix_len, .-fd_prefix
fd_name: .ascii "anon_inode:[ksu_driver]"
.set fd_name_len, .-fd_name
ok: .ascii "{\"schemaVersion\":1,\"machine\":\"ELF32-i386\",\"status\":\"direct_fd_get_info_verified\",\"uid\":2000,\"euid\":2000,\"version\":35253,\"flags\":1,\"features\":7,\"uapiVersion\":6,\"directFdAcquisitionTested\":true,\"rootGranted\":false,\"seccomp\":0,\"capSysBoot\":false}\n"
.set ok_len, .-ok
fd_error: .ascii "{\"status\":\"fd_identity_mismatch\"}\n"
.set fd_error_len, .-fd_error
ioctl_error: .ascii "{\"status\":\"get_info_failed\"}\n"
.set ioctl_error_len, .-ioctl_error
info_error: .ascii "{\"status\":\"get_info_values_mismatch\"}\n"
.set info_error_len, .-info_error
uid_error: .ascii "{\"status\":\"uid_changed\"}\n"
.set uid_error_len, .-uid_error
fd_absent: .ascii "{\"schemaVersion\":1,\"machine\":\"ELF32-i386\",\"status\":\"direct_fd_absent\",\"uid\":2000,\"euid\":2000,\"seccomp\":0,\"capSysBoot\":false,\"fdAbsent\":true,\"reason\":\"no_nonnegative_fd_returned\",\"rootGranted\":false}\n"
.set fd_absent_len, .-fd_absent
gate_error: .ascii "{\"status\":\"capability_or_seccomp_gate_denied\"}\n"
.set gate_error_len, .-gate_error
.section .data
.balign 4
driver_fd: .long -1
cap_header: .long 0x20080522,0
.section .bss
.balign 4
info: .skip 16
fd_buf: .skip 64
cap_words: .skip 24
fd_path: .skip 64
fd_path_end:
.section .note.GNU-stack,"",@progbits
