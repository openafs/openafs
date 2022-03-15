/*
 * Copyrigh 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#ifdef AFS_AIX_ENV
#include <sys/adspace.h>	/* for vm_att(), vm_det() */
#endif

/* osi_Init -- do once per kernel installation initialization.
 *     -- On Solaris this is called from modload initialization.
 *     -- On AIX called from afs_config.
 *     -- On HP called from afsc_link.
 *     -- On SGI called from afs_init. */

/* No hckernel-specific header for this prototype. */
#ifndef UKERNEL
extern void init_hckernel_mutex(void);
#endif

afs_lock_t afs_ftf;		/* flush text lock */

#ifdef AFS_SGI53_ENV
lock_t afs_event_lock;
#endif

#ifdef AFS_SGI64_ENV
flid_t osi_flid;
#endif

afs_ucred_t *afs_osi_credp;

#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
kmutex_t afs_global_lock;
#endif

#if defined(AFS_SGI_ENV) && !defined(AFS_SGI64_ENV)
long afs_global_owner;
#endif

#if defined(AFS_DARWIN_ENV)
thread_t afs_global_owner;
#ifdef AFS_DARWIN80_ENV
lck_mtx_t  *afs_global_lock;
#else
struct lock__bsd__ afs_global_lock;
#endif
#endif

#if defined(AFS_XBSD_ENV) && !defined(AFS_FBSD_ENV)
# if defined(AFS_NBSD50_ENV)
kmutex_t afs_global_mtx;
# else
struct lock afs_global_lock;
afs_proc_t *afs_global_owner;
# endif
#elif defined(AFS_FBSD_ENV)
struct mtx afs_global_mtx;
struct thread *afs_global_owner;
#endif

#if defined(AFS_AIX41_ENV)
simple_lock_data afs_global_lock;
#endif

void
osi_Init(void)
{
    static int once = 0;
    if (once++ > 0)		/* just in case */
	return;

    osi_InitGlock();

    /* Initialize a lock for the kernel hcrypto bits. */
#ifndef UKERNEL
    init_hckernel_mutex();
#endif


    if (!afs_osicred_initialized) {
#if defined(AFS_DARWIN80_ENV)
        afs_osi_ctxtp_initialized = 0;
        afs_osi_ctxtp = NULL; /* initialized in afs_Daemon since it has
                                  a proc reference that cannot be changed */
#endif
#if defined(AFS_XBSD_ENV)
	/* Can't just invent one, must use crget() because of mutex */
	afs_osi_credp =
	  crdup(osi_curcred());
#elif defined(AFS_SUN5_ENV)
	afs_osi_credp = kcred;
#elif defined(AFS_DARWIN_ENV)
	afs_osi_credp = afs_osi_Alloc(sizeof(*afs_osi_credp));
	osi_Assert(afs_osi_credp != NULL);
	memset(afs_osi_credp, 0, sizeof(*afs_osi_credp));
# if defined(AFS_DARWIN80_ENV)
	afs_osi_credp->cr_ref = 1; /* kauth_cred_get_ref needs 1 existing ref */
# endif
#else
	memset(&afs_osi_cred, 0, sizeof(afs_ucred_t));
#if defined(AFS_LINUX_ENV)
        afs_set_cr_group_info(&afs_osi_cred, groups_alloc(0));
#endif
#if !(defined(AFS_LINUX_ENV) && defined(STRUCT_TASK_STRUCT_HAS_CRED))
	crhold(&afs_osi_cred);  /* don't let it evaporate */
#endif

	afs_osi_credp = &afs_osi_cred;
#endif
	afs_osicred_initialized = 1;
    }
#ifdef AFS_SGI64_ENV
    osi_flid.fl_pid = osi_flid.fl_sysid = 0;
#endif

    init_et_to_sys_error();
}

/* mask signals in afsds */
void
afs_osi_MaskSignals(void)
{
#ifdef AFS_LINUX_ENV
    osi_linux_mask();
#endif
}

/* unmask signals in rxk listener */
void
afs_osi_UnmaskRxkSignals(void)
{
#ifdef AFS_LINUX_ENV
    osi_linux_unmaskrxk();
#endif
}

/* Two hacks to try and fix afsdb */
void
afs_osi_MaskUserLoop(void)
{
#ifdef AFS_DARWIN_ENV
    afs_osi_Invisible();
    afs_osi_fullSigMask();
#else
    afs_osi_MaskSignals();
#endif
}

void
afs_osi_UnmaskUserLoop(void)
{
#ifdef AFS_DARWIN_ENV
    afs_osi_fullSigRestore();
#endif
}

/* register rxk listener proc info */
void
afs_osi_RxkRegister(void)
{
}

/* procedure for making our processes as invisible as we can */
void
afs_osi_Invisible(void)
{
#ifdef AFS_LINUX_ENV
    afs_osi_MaskSignals();
#elif defined(AFS_SUN5_ENV)
    curproc->p_flag |= SSYS;
#elif defined(AFS_HPUX101_ENV) && !defined(AFS_HPUX1123_ENV)
    set_system_proc(u.u_procp);
#elif defined(AFS_DARWIN80_ENV)
#elif defined(AFS_DARWIN_ENV)
    /* maybe call init_process instead? */
    current_proc()->p_flag |= P_SYSTEM;
#elif defined(AFS_NBSD50_ENV)
    /* XXX in netbsd a system thread is more than invisible */
#elif defined(AFS_XBSD_ENV)
    curproc->p_flag |= P_SYSTEM;
#elif defined(AFS_SGI_ENV)
    vrelvm();
#endif

    AFS_STATCNT(osi_Invisible);
}

void
afs_osi_Visible(void)
{
#if defined(AFS_SUN5_ENV)
    curproc->p_flag &= ~SSYS;
#elif defined(AFS_DARWIN80_ENV)
#elif defined(AFS_DARWIN_ENV)
    /* maybe call init_process instead? */
    current_proc()->p_flag &= ~P_SYSTEM;
#elif defined(AFS_NBSD50_ENV)
    /* XXX in netbsd a system thread is more than invisible */
#elif defined(AFS_XBSD_ENV)
    curproc->p_flag &= ~P_SYSTEM;
#endif
}

void
shutdown_osi(void)
{
    AFS_STATCNT(shutdown_osi);
#ifdef AFS_DARWIN80_ENV
    if (afs_osi_ctxtp_initialized && afs_osi_ctxtp) {
       vfs_context_rele(afs_osi_ctxtp);
       afs_osi_ctxtp = NULL;
       afs_osi_ctxtp_initialized = 0;
    }
#endif
#if !defined(AFS_HPUX_ENV) && !defined(UKERNEL) && !defined(AFS_DFBSD_ENV) && !defined(AFS_LINUX_ENV)
    /* LINUX calls this from afs_cleanup() which hooks into module_exit */
    shutdown_osisleep();
#endif
    if (afs_cold_shutdown) {
	LOCK_INIT(&afs_ftf, "afs_ftf");
    }
}

#if !defined(AFS_HPUX_ENV) && !defined(UKERNEL) && !defined(AFS_DFBSD_ENV) && !defined(AFS_DARWIN_ENV)
/* DARWIN uses locking, and so must provide its own */
void
shutdown_osisleep(void)
{
    afs_event_t *tmp;
    int i;

    for (i=0;i<AFS_EVHASHSIZE;i++) {
	while ((tmp = afs_evhasht[i]) != NULL) {
	    afs_evhasht[i] = tmp->next;
	    if (tmp->refcount > 0) {
		afs_warn("nonzero refcount in shutdown_osisleep()\n");
	    } else {
#if defined(AFS_AIX_ENV)
		xmfree(tmp);
#elif defined(AFS_FBSD_ENV)
		afs_osi_Free(tmp, sizeof(*tmp));
#elif defined(AFS_SGI_ENV) || defined(AFS_XBSD_ENV) || defined(AFS_SUN5_ENV)
		osi_FreeSmallSpace(tmp);
#elif defined(AFS_LINUX_ENV)
		kfree(tmp);
#endif
	    }
	}
    }
}
#endif

#if !defined(AFS_OBSD_ENV) && !defined(AFS_NBSD40_ENV)
int
afs_osi_suser(void *cr)
{
#if defined(AFS_SUN510_ENV)
    return (priv_policy(cr, PRIV_SYS_SUSER_COMPAT, B_FALSE, EPERM, NULL) == 0);
#elif defined(AFS_SUN5_ENV)
    return afs_suser(cr);
#else
    return afs_suser(NULL);
#endif
}
#endif
