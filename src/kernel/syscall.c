#include <xbook/syscall.h>
#include <xbook/process.h>
#include <xbook/vmspace.h>
#include <xbook/resource.h>
#include <xbook/trigger.h>
#include <xbook/alarm.h>
#include <xbook/ktime.h>

/* 系统调用表 */ 
syscall_t syscall_table[SYSCALL_NR];

void init_syscall()
{
    /* 进程管理 */
    syscall_table[SYS_EXIT] = sys_exit;
    syscall_table[SYS_FORK] = sys_fork;
    syscall_table[SYS_EXECR] = sys_exec_raw;
    syscall_table[SYS_EXECF] = sys_exec_file;
    syscall_table[SYS_WAITPID] = sys_waitpid;
    syscall_table[SYS_GETPID] = sys_get_pid;
    syscall_table[SYS_GETPPID] = sys_get_ppid;
    
    syscall_table[SYS_TRIGGER] = sys_trigger_handler;
    syscall_table[SYS_TRIGGERON] = sys_trigger_active;
    syscall_table[SYS_TRIGGERACT] = sys_trigger_action;
    syscall_table[SYS_TRIGRET] = sys_trigger_return;
    
    syscall_table[SYS_SLEEP] = sys_sleep;
    
    syscall_table[SYS_THREAD_CREATE] = sys_thread_create;
    syscall_table[SYS_THREAD_EXIT] = sys_thread_exit;
    syscall_table[SYS_THREAD_JOIN] = sys_thread_join;
    syscall_table[SYS_THREAD_DETACH] = sys_thread_detach;
    
    syscall_table[SYS_GETTID] = sys_get_tid;
    /* 内存管理 */
    syscall_table[SYS_HEAP] = sys_vmspace_heap;
    
    /* 设备资源管理 */
    syscall_table[SYS_GETRES] = sys_getres;
    syscall_table[SYS_PUTRES] = sys_putres;
    syscall_table[SYS_READRES] = sys_readres;
    syscall_table[SYS_WRITERES] = sys_writeres;
    syscall_table[SYS_CTLRES] = sys_ctlres;
    
    syscall_table[SYS_ALARM] = sys_alarm;
    syscall_table[SYS_KTIME] = sys_get_ktime;
}