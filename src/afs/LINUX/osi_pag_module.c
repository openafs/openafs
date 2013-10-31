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
#include <afsconfig.h>
#include "afs/param.h"

#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include <linux/unistd.h>		/* For syscall numbers. */
#include <linux/mm.h>

#ifdef AFS_AMD64_LINUX20_ENV
#include <asm/ia32_unistd.h>
#endif

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>

static unsigned long nfs_server_addr = 0;
#if defined(module_param) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param(nfs_server_addr, long, 0);
#else
MODULE_PARM(nfs_server_addr,  "l");
#endif
MODULE_PARM_DESC(nfs_server_addr,  "IP Address of NFS Server");

static char *this_cell = 0;
#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param(this_cell, charp, 0);
#else
MODULE_PARM(this_cell, "s");
#endif
MODULE_PARM_DESC(this_cell, "Local cell name");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
DEFINE_MUTEX(afs_global_lock);
#else
DECLARE_MUTEX(afs_global_lock);
#endif
struct proc_dir_entry *openafs_procfs;
int afs_global_owner = 0;

#ifdef HAVE_LINUX_KUID_T
struct user_namespace *afs_ns;
#endif

int __init
afspag_init(void)
{
#if !defined(EXPORTED_PROC_ROOT_FS)
    char path[64];
#endif
    int err;

#ifdef HAVE_LINUX_KUID_T
    afs_ns = afs_current_user_ns();
#endif

    osi_Init();

    err = osi_syscall_init();
    if (err)
	return err;
#if defined(EXPORTED_PROC_ROOT_FS)
    openafs_procfs = proc_mkdir(PROC_FSDIRNAME, proc_root_fs);
#else
    sprintf(path, "fs/%s", PROC_FSDIRNAME);
    openafs_procfs = proc_mkdir(path, NULL);
#endif
    osi_ioctl_init();

    afspag_Init(htonl(nfs_server_addr));
    if (this_cell)
	afspag_SetPrimaryCell(this_cell);

    return 0;
}

void __exit
afspag_cleanup(void)
{
#if !defined(EXPORTED_PROC_ROOT_FS)
    char path[64];
#endif
    osi_syscall_clean();

    osi_linux_free_afs_memory();

    osi_ioctl_clean();
#if defined(EXPORTED_PROC_ROOT_FS)
    remove_proc_entry(PROC_FSDIRNAME, proc_root_fs);
#else
    sprintf(path, "fs/%s", PROC_FSDIRNAME);
    remove_proc_entry(path, NULL);
#endif
    return;
}

MODULE_LICENSE("http://www.openafs.org/dl/license10.html");
module_init(afspag_init);
module_exit(afspag_cleanup);

/* Hack alert!
 * These will never be called in the standalone PAG manager, because
 * they are only referenced in afs_InitReq, and nothing calls that.
 * However, we need to define them in order to resolve the reference,
 * unless we want to move afs_InitReq out of afs_osi_pag.c.
 */
int osi_linux_nfs_initreq(struct vrequest *av, afs_ucred_t *cr, int *code)
{
    *code = EACCES;
    return 1;
}

int
afs_nfsclient_reqhandler(struct afs_exporter *exporter,
			 afs_ucred_t **cred,
			 afs_uint32 host, afs_int32 *pagparam,
			 struct afs_exporter **outexporter)
{
    return EINVAL;
}
