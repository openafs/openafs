/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Linux module support routines.
 *
 */
#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../h/unistd.h" /* For syscall numbers. */
#include "../h/mm.h"

#include <linux/module.h>



asmlinkage int (*sys_settimeofdayp)(struct timeval *tv, struct timezone *tz);
asmlinkage int (*sys_socketcallp)(int call, long *args);
asmlinkage int (*sys_killp)(int pid, int signal);
asmlinkage int (*sys_setgroupsp)(int gidsetsize, gid_t *grouplist);

extern void *sys_call_table[];
extern struct file_system_type afs_file_system;

static long get_page_offset(void);

struct semaphore afs_global_lock = MUTEX;
int afs_global_owner = 0;
unsigned long afs_linux_page_offset = 0; /* contains the PAGE_OFFSET value */

/* Since sys_ni_syscall is not exported, I need to cache it in order to restore
 * it.
 */
static void *afs_ni_syscall = NULL;

int init_module(void)
{
    extern int afs_syscall();
    extern int afs_xsetgroups();

    /* obtain PAGE_OFFSET value */
    afs_linux_page_offset = get_page_offset();

    if (afs_linux_page_offset == 0) {
        /* couldn't obtain page offset so can't continue */
        printf("afs: Unable to obtain PAGE_OFFSET. Exiting..");
        return -EIO;
    }

    /* Initialize pointers to kernel syscalls. */
    sys_settimeofdayp = sys_call_table[__NR_settimeofday];
    sys_socketcallp = sys_call_table[__NR_socketcall];
    sys_killp = sys_call_table[__NR_kill];

    /* setup AFS entry point. */
    if (sys_call_table[__NR_afs_syscall] == afs_syscall) {
	printf("AFS syscall entry point already in use!\n");
	return -EBUSY;
    }


    afs_ni_syscall = sys_call_table[__NR_afs_syscall];
    sys_call_table[__NR_afs_syscall] = afs_syscall;

    osi_Init();
    register_filesystem(&afs_file_system);

    /* Intercept setgroups calls */
    sys_setgroupsp = sys_call_table[__NR_setgroups];
    sys_call_table[__NR_setgroups] = afs_xsetgroups;

    return 0;
}

void cleanup_module(void)
{
    struct task_struct *t;

    sys_call_table[__NR_setgroups] = sys_setgroupsp;
    sys_call_table[__NR_afs_syscall] = afs_ni_syscall;

    unregister_filesystem(&afs_file_system);

    osi_linux_free_inode_pages(); /* Invalidate all pages using AFS inodes. */
    osi_linux_free_afs_memory();

    return;
}

static long get_page_offset(void)
{
    struct task_struct *p;

    /* search backward thru the circular list */
    for(p = current; p; p = p->prev_task)
	if (p->pid == 1)
	    return p->addr_limit.seg;

    return 0;
}
