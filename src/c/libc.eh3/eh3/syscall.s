format elf

#include <stdlib.h>

section .text
__syscall:
    ;; load args
    mov a3, a2
    mov a2, a1
    mov a1, a0
    ;; syscall num in t0
    mov a0, t0
    ;; abi: arg3-6 in t0-2
    load word t0, [sp : 0]
    load word t1, [sp : 4]
    load word t2, [sp : 8]
    syscall
    ;; error?
    jb a0, -125, .done
    neg a0, a0
    store word [errno], a0
    mov a0, -1
.done:
    ret

global _exit
alignb _exit:
    mov t0, SYS_exit
    jmp __syscall

global fork
alignb fork:
    mov t0, SYS_fork
    jmp __syscall

global read
alignb read:
    mov t0, SYS_read
    jmp __syscall

global write
alignb write:
    mov t0, SYS_write
    jmp __syscall

global open
alignb open:
    mov t0, SYS_open
    jmp __syscall

global close
alignb close:
    mov t0, SYS_close
    jmp __syscall

global waitpid
alignb waitpid:
    mov t0, SYS_wait4
    jmp __syscall

global creat
alignb creat:
    mov t0, SYS_creat
    jmp __syscall

global link
alignb link:
    mov t0, SYS_link
    jmp __syscall

global unlink
alignb unlink:
    mov t0, SYS_unlink
    jmp __syscall

global execve
alignb execve:
    mov t0, SYS_execve
    jmp __syscall

global chdir
alignb chdir:
    mov t0, SYS_chdir
    jmp __syscall

global time
alignb time:
    mov t0, SYS_time
    jmp __syscall

global mknod
alignb mknod:
    mov t0, SYS_mknod
    jmp __syscall

global chmod
alignb chmod:
    mov t0, SYS_chmod
    jmp __syscall

global lseek
alignb lseek:
    mov t0, SYS_lseek
    jmp __syscall

global getpid
alignb getpid:
    mov t0, SYS_getpid
    jmp __syscall

global mount
alignb mount:
    mov t0, SYS_mount
    jmp __syscall

global umount
alignb umount:
    mov t0, SYS_umount2
    jmp __syscall

global setuid
alignb setuid:
    mov t0, SYS_setuid
    jmp __syscall

global getuid
alignb getuid:
    mov t0, SYS_getuid
    jmp __syscall

global utime
alignb utime:
    mov t0, SYS_utime
    jmp __syscall

global access
alignb access:
    mov t0, SYS_access
    jmp __syscall

global sync
alignb sync:
    mov t0, SYS_sync
    jmp __syscall

global kill
alignb kill:
    mov t0, SYS_kill
    jmp __syscall

global mkdir
alignb mkdir:
    mov t0, SYS_mkdir
    jmp __syscall

global rmdir
alignb rmdir:
    mov t0, SYS_rmdir
    jmp __syscall

global dup
alignb dup:
    mov t0, SYS_dup
    jmp __syscall

global pipe
alignb pipe:
    mov t0, SYS_pipe
    jmp __syscall

global brk
alignb brk:
    mov t0, SYS_brk
    jmp __syscall

global setgid
alignb setgid:
    mov t0, SYS_setgid
    jmp __syscall

global getgid
alignb getgid:
    mov t0, SYS_getgid
    jmp __syscall

global geteuid
alignb geteuid:
    mov t0, SYS_geteuid
    jmp __syscall

global getegid
alignb getegid:
    mov t0, SYS_getegid
    jmp __syscall

global ioctl
alignb ioctl:
    mov t0, SYS_ioctl
    jmp __syscall

global fcntl
alignb fcntl:
    mov t0, SYS_fcntl
    jmp __syscall

global dup2
alignb dup2:
    mov t0, SYS_dup2
    jmp __syscall

global getppid
alignb getppid:
    mov t0, SYS_getppid
    jmp __syscall

global setsid
alignb setsid:
    mov t0, SYS_setsid
    jmp __syscall

global gettimeofday
alignb gettimeofday:
    mov t0, SYS_gettimeofday
    jmp __syscall

global settimeofday
alignb settimeofday:
    mov t0, SYS_settimeofday
    jmp __syscall

global mmap
alignb mmap:
    mov t0, SYS_mmap
    jmp __syscall

global munmap
alignb munmap:
    mov t0, SYS_munmap
    jmp __syscall

global stat
alignb stat:
    mov t0, SYS_stat
    jmp __syscall

global lstat
alignb lstat:
    mov t0, SYS_lstat
    jmp __syscall

global fstat
alignb fstat:
    mov t0, SYS_fstat
    jmp __syscall

global clone
alignb clone:
    mov t0, SYS_clone
    jmp __syscall

global uname
alignb uname:
    mov t0, SYS_uname
    jmp __syscall

global fchdir
alignb fchdir:
    mov t0, SYS_fchdir
    jmp __syscall

global getdents
alignb getdents:
    mov t0, SYS_getdents
    jmp __syscall

global nanosleep
alignb nanosleep:
    mov t0, SYS_nanosleep
    jmp __syscall

global poll
alignb poll:
    mov t0, SYS_poll
    jmp __syscall

global chown
alignb chown:
    mov t0, SYS_chown
    jmp __syscall

global getcwd
alignb getcwd:
    mov t0, SYS_getcwd
    jmp __syscall

global sigaction
alignb sigaction:
    mov t0, SYS_rt_sigaction
    jmp __syscall

global sigreturn
alignb sigreturn:
    mov t0, SYS_rt_sigreturn
    jmp __syscall

global fsync
alignb fsync:
    mov t0, SYS_fsync
    jmp __syscall

global fdatasync
alignb fdatasync:
    mov t0, SYS_fdatasync
    jmp __syscall

global truncate
alignb truncate:
    mov t0, SYS_truncate
    jmp __syscall

global ftruncate
alignb ftruncate:
    mov t0, SYS_ftruncate
    jmp __syscall

section .data
global errno
errno:   dw 0
