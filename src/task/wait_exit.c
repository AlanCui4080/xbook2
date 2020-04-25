#include <arch/interrupt.h>
#include <xbook/schedule.h>
#include <xbook/debug.h>
#include <xbook/task.h>
#include <xbook/process.h>
#include <sys/wait.h>

#define DEBUG_LOCAL 1

/*
僵尸进程：当进程P调用exit时，其父进程没有wait，那么它就变成一个僵尸进程。
        只占用PCB，不占用内存其它资源。
        处理方法：父进程退出时，会查看是否有僵尸进程，并且将它们回收。
孤儿进场：当进程P还未调用exit时，其父进程就退出了，那么它就变成一个孤儿进程。
        不过它可以交给init进程收养。
        处理方法：将进程P过继给INIT进程收养。
*/

/**
 * thread_release - 释放线程资源
 * @task: 任务
 * 
 * 线程资源比较少，主要是任务结构体（需要交给父进程处理），
 * 自己要处理的很少很少。。。
 * 
 * @return: 释放成功返回0，释放失败返回-1
 */
int thread_release(task_t *task)
{
    /* 取消定时器 */
    timer_cancel(task->sleep_timer);
    task->sleep_timer = NULL;
    return 0;
}

/**
 * wait_any_hangging_child - 处理一个挂起子进程
 * 
 */
int wait_any_hangging_child(task_t *parent, int *status)
{
    task_t *child, *next;
    /* 可能删除列表元素，需要用safe */
    list_for_each_owner_safe (child, next, &task_global_list, global_list) {
        if (child->parent_pid == parent->pid) { /* find a child process */
            if (child->state == TASK_HANGING) { /* child is hanging, destroy it  */
                pid_t child_pid = child->pid;
#if DEBUG_LOCAL == 1
                printk(KERN_NOTICE "wait_any_hangging_child: find a hanging proc %d \n", child_pid);
#endif              
                /* 状态是可以为空的，不为空才写入子进程退出状态 */
                if (status != NULL)
                    *status = child->exit_status;
                /* 销毁子进程的PCB */
                proc_destroy(child);
                return child_pid;
            }
        }
    }
    return -1;
}


/**
 * wait_one_hangging_child - 处理一个挂起子进程
 * 
 */
int wait_one_hangging_child(task_t *parent, pid_t pid, int *status)
{
    task_t *child, *next;
    /* 可能删除列表元素，需要用safe */
    list_for_each_owner_safe (child, next, &task_global_list, global_list) {
        if (child->parent_pid == parent->pid && child->pid == pid) { /* find a child process we ordered */
            if (child->state == TASK_HANGING) { /* child is hanging, destroy it  */
#if DEBUG_LOCAL == 1
                printk(KERN_NOTICE "wait_one_hangging_child: find a hanging proc %d \n", pid);
#endif              
                /* 状态是可以为空的，不为空才写入子进程退出状态 */
                if (status != NULL)
                    *status = child->exit_status;
                /* 销毁子进程的PCB */
                proc_destroy(child);
                return pid;
            }
        }
    }
    return -1;
}


/**
 * deal_zombie_child - 处理僵尸子进程
 * 
*/
int deal_zombie_child(task_t *parent)
{
    int zombie = 0;
    task_t *child, *next;
    list_for_each_owner_safe (child, next, &task_global_list, global_list) {
        if (child->parent_pid == parent->pid) { /* find a child process */
            if (child->state == TASK_ZOMBIE) { /* child is zombie, destroy it  */
#if DEBUG_LOCAL == 1
                printk(KERN_NOTICE "find a zombie proc %d \n", child->pid);
#endif
                /* 销毁子进程的PCB */
                proc_destroy(child);
                zombie++;
                
                
            }
        }
    }
    return zombie;
}

/**
 * find_child_proc - 查找子进程
 * @parent: 父进程
 * 
 * 返回子进程数，没有则返回0
*/
int find_child_proc(task_t *parent)
{
    int children = 0;
    task_t *child;
    list_for_each_owner (child, &task_global_list, global_list) {
        if (child->parent_pid == parent->pid) { /* find a child process */
            children++;
        }
    }
    return children; /* return child proc number */
}

/**
 * adopt_children_to_init - 过继子进程给INIT进程
 * @parent: 父进程
 * 
 * 返回子进程数，没有则返回0
*/
void adopt_children_to_init(task_t *parent)
{
    task_t *child;
    list_for_each_owner (child, &task_global_list, global_list) {
        if (child->parent_pid == parent->pid) { /* find a child process */
            child->parent_pid = INIT_PROC_PID;
        }
    }
}

void close_child_thread(task_t *parent)
{
    /* 查找所有的子进程 */
    task_t *child, *next;
    list_for_each_owner_safe (child, next, &task_global_list, global_list) {
        /* 查找一个子线程，位于同一个线程组，但不是自己 */
        if (parent->tgid == child->tgid && parent->pid != child->pid) {
#if DEBUG_LOCAL == 1
            printk(KERN_DEBUG "close_child_thread: task=%s pid=%d tgid=%d ppid=%d state=%d\n",
                child->name, child->pid, child->tgid, child->parent_pid, child->state);
#endif
            /* 如果线程处于就绪状态，那么就从就绪队列删除 */
            if (child->state == TASK_READY) {
                /* 从优先级队列移除 */
                list_del_init(&child->list);
                child->prio_queue->length--;
                child->prio_queue = NULL;
#if DEBUG_LOCAL == 1
                printk(KERN_DEBUG "close_child_thread: remove from on ready list.\n");
#endif
            }
            thread_release(child);  /* 释放线程资源 */
            /* 销毁进程 */
            task_free(child);   /* 释放任务 */
        }
    }
}

/**
 * sys_waitpid - 进程等待
 * @pid: 指定的进程id:
 *      为-1时，等待任意子进程，>0时等待指定的子进程
 * @status: 存放子进程退出时的状态 
 * @options: 选项参数：
 *          WNOHANG: 如果没有子进程退出，就不阻塞等待，返回0
 * 
 * 等待流程：
 * 
 * 查看是否有挂起的进程->
 *      有则销毁挂起的进程，并返回进程pid，END
 * 判断是否有子进程->
 *      没有发现子进程，就返回-1，END
 * 继续处于WAITING状态，直到有子进程退出
 * 
 * 返回子进程的pid，无则返回-1
 */
pid_t sys_waitpid(pid_t pid, int *status, int options)
{
    task_t *parent = current_task;  /* 当前进程是父进程 */
    pid_t child_pid;
    unsigned long flags;
    
    while (1) {
#if DEBUG_LOCAL == 1
        //printk(KERN_DEBUG "sys_wait: name=%s pid=%d wait child.\n", parent->name, parent->pid);
#endif    
        save_intr(flags);
        if (pid == -1) {    /* 任意子进程 */            
            /* 先处理挂起的任务 */
            if ((child_pid = wait_any_hangging_child(parent, status)) >= 0) {
#if DEBUG_LOCAL == 1
                printk(KERN_DEBUG "sys_wait: handle a hangging child %d.\n", child_pid);
#endif    
                restore_intr(flags);
                return child_pid;
            }
        } else {    /* 指定子进程 */
            /* 先处理挂起的任务 */
            if ((child_pid = wait_one_hangging_child(parent, pid, status)) >= 0) {
#if DEBUG_LOCAL == 1
                printk(KERN_DEBUG "sys_wait: handle a hangging child %d.\n", child_pid);
#endif    
                restore_intr(flags);
                return child_pid;
            }
        }
        
        /* 处理zombie子进程 */
        deal_zombie_child(parent);

        /* 现在肯定没有子进程处于退出状态 */
        if (options & WNOHANG) {    /* 有不挂起标志 */
            restore_intr(flags);
            return 0;   /* 不阻塞等待，返回0 */
        }

        /* 查看是否有子进程 */
        if (!find_child_proc(parent)) {
#if DEBUG_LOCAL == 1
            //printk(KERN_DEBUG "sys_wait: no children!\n");
#endif    
            //printk("no children!\n");
            restore_intr(flags);
            return -1; /* no child, return -1 */
        }
        //printk(KERN_DEBUG "proc wait: find proc.\n");
        
        //printk(KERN_DEBUG "proc wait: no child exit, waiting...\n");
#if DEBUG_LOCAL == 1
        printk(KERN_DEBUG "sys_wait: no child exit, waiting...\n");
#endif   
        restore_intr(flags);
        /* WATING for children to exit */
        task_block(TASK_WAITING);
        //printk(KERN_DEBUG "proc wait: child unblocked me.\n");
    }
}

/**
 * sys_exit - 进程退出
 * @status: 退出状态
 * 
 * 退出流程：
 * 
 * 设置退出状态->查看是否有父进程->
 *      无父进程->PANIC，END
 * 查看子进程是否有僵尸进程，有就把僵尸进程释放掉，解决僵尸状态->把其它子进程过继给INIT进程，解决孤儿进程问题。
 * 释放占用资源->查看父进程是否处于等待状态->
 *      是等待状态->解除对父进程的阻塞。->自己变成挂起状态，等待父进程来销毁。END
 *      不是等待状态->把自己设置成僵尸状态，等待父进程退出。END
 */
void sys_exit(int status)
{
    unsigned long flags;
    save_intr(flags);

    task_t *cur = current_task;  /* 当前进程是父进程 */
    if (!TASK_IS_MAIN_THREAD(cur))  /* 不是主线程就调用线程退出 */
        sys_thread_exit((void *) status);

    cur->exit_status = status;
    task_t *parent = find_task_by_pid(cur->parent_pid); 
    
    /*if (cur->parent_pid == -1 || parent == NULL)
        panic("sys_exit: proc %s PID=%d exit with parent pid -1!\n", cur->name, cur->pid);
    */
#if DEBUG_LOCAL == 1
    printk(KERN_DEBUG "sys_exit: name=%s pid=%d ppid=%d prio=%d status=%d\n",
        cur->name, cur->pid, cur->parent_pid, cur->priority, cur->exit_status);
#endif    
    /* 处理zombie子进程 */
    deal_zombie_child(cur);

    /* 关闭子线程 */
    close_child_thread(cur);

    /* 过继子进程给init进程 */
    adopt_children_to_init(cur);
    
    /* 释放占用的资源 */
    proc_release(cur); /* 释放资源 */
#if DEBUG_LOCAL == 1
    printk(KERN_DEBUG "sys_exit: release all.\n");
#endif    

    if (parent) {
        /* 查看父进程状态 */
        if (parent->state == TASK_WAITING) {
            restore_intr(flags);
#if DEBUG_LOCAL == 1
            printk(KERN_DEBUG "sys_exit: parent waiting...\n");
#endif    

            //printk("parent waiting...\n");
            task_unblock(parent); /* 唤醒父进程 */
            task_block(TASK_HANGING);   /* 把自己挂起 */
        } else { /* 父进程没有 */
            restore_intr(flags);
#if DEBUG_LOCAL == 1
            printk(KERN_DEBUG "sys_exit: parent not waiting, zombie!\n");
#endif    
            //printk("parent not waiting, zombie!\n");
            task_block(TASK_ZOMBIE);   /* 变成僵尸进程 */
        }
    } else {
        /* 没有父进程，变成不可被收养的孤儿+僵尸进程 */
#if DEBUG_LOCAL == 1
            printk(KERN_DEBUG "sys_exit: no parent! zombie!\n");
#endif    
        //printk("no parent!\n");
        restore_intr(flags);
        task_block(TASK_ZOMBIE); 
    }
}

void kthread_exit(int status)
{
    unsigned long flags;
    save_intr(flags);

    task_t *cur = current_task;  /* 当前进程是父进程 */
    cur->exit_status = status;

    /* 释放内核资源 */
    thread_release(cur);

    /* 内核线程没有实际的父进程，因此把自己过继给init进程 */
    cur->parent_pid = INIT_PROC_PID;

    task_t *parent = find_task_by_pid(cur->parent_pid); 
    if (parent) {
        /* 查看父进程状态 */
        if (parent->state == TASK_WAITING) {
            restore_intr(flags);
#if DEBUG_LOCAL == 1
            printk(KERN_DEBUG "sys_exit: parent waiting...\n");
#endif    

            //printk("parent waiting...\n");
            task_unblock(parent); /* 唤醒父进程 */
            task_block(TASK_HANGING);   /* 把自己挂起 */
        } else { /* 父进程没有 */
            restore_intr(flags);
#if DEBUG_LOCAL == 1
            printk(KERN_DEBUG "sys_exit: parent not waiting, zombie!\n");
#endif    
            //printk("parent not waiting, zombie!\n");
            task_block(TASK_ZOMBIE);   /* 变成僵尸进程 */
        }
    } else {
        /* 没有父进程，变成不可被收养的孤儿+僵尸进程 */
#if DEBUG_LOCAL == 1
            printk(KERN_DEBUG "sys_exit: no parent! zombie!\n");
#endif    
        //printk("no parent!\n");
        restore_intr(flags);
        task_block(TASK_ZOMBIE); 
    }
}