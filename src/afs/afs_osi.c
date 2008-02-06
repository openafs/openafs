/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

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

afs_lock_t afs_ftf;		/* flush text lock */

#ifdef AFS_SGI53_ENV
lock_t afs_event_lock;
#endif

#ifdef AFS_SGI64_ENV
flid_t osi_flid;
#endif

struct AFS_UCRED *afs_osi_credp;

#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
kmutex_t afs_global_lock;
kmutex_t afs_rxglobal_lock;
#endif

#if defined(AFS_SGI_ENV) && !defined(AFS_SGI64_ENV)
long afs_global_owner;
#endif

#if defined(AFS_OSF_ENV)
simple_lock_data_t afs_global_lock;
#endif

#if defined(AFS_DARWIN_ENV) 
#ifdef AFS_DARWIN80_ENV
lck_mtx_t  *afs_global_lock;
#else
struct lock__bsd__ afs_global_lock;
#endif
#endif

#if defined(AFS_XBSD_ENV) && !defined(AFS_FBSD50_ENV)
struct lock afs_global_lock;
struct proc *afs_global_owner;
#endif
#ifdef AFS_FBSD50_ENV
struct mtx afs_global_mtx;
#endif

#if defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV)
thread_t afs_global_owner;
#endif /* AFS_OSF_ENV */

#if defined(AFS_AIX41_ENV)
simple_lock_data afs_global_lock;
#endif

void
osi_Init(void)
{
    static int once = 0;
    if (once++ > 0)		/* just in case */
	return;
#if	defined(AFS_HPUX_ENV)
    osi_InitGlock();
#else /* AFS_HPUX_ENV */
#if defined(AFS_GLOBAL_SUNLOCK)
#if defined(AFS_SGI62_ENV)
    mutex_init(&afs_global_lock, MUTEX_DEFAULT, "afs_global_lock");
#elif defined(AFS_OSF_ENV)
    usimple_lock_init(&afs_global_lock);
    afs_global_owner = (thread_t) 0;
#elif defined(AFS_FBSD50_ENV)
    mtx_init(&afs_global_mtx, "AFS global lock", NULL, MTX_DEF);
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#if !defined(AFS_DARWIN80_ENV)
    lockinit(&afs_global_lock, PLOCK, "afs global lock", 0, 0);
#endif
    afs_global_owner = 0;
#elif defined(AFS_AIX41_ENV)
    lock_alloc((void *)&afs_global_lock, LOCK_ALLOC_PIN, 1, 1);
    simple_lock_init((void *)&afs_global_lock);
#elif !defined(AFS_LINUX22_ENV)
    /* Linux initialization in osi directory. Should move the others. */
    mutex_init(&afs_global_lock, "afs_global_lock", MUTEX_DEFAULT, NULL);
#endif
#endif /* AFS_GLOBAL_SUNLOCK */
#endif /* AFS_HPUX_ENV */

    if (!afs_osicred_initialized) {
#if defined(AFS_DARWIN80_ENV)
        afs_osi_ctxtp_initialized = 0;
        afs_osi_ctxtp = NULL; /* initialized in afs_Daemon since it has
                                  a proc reference that cannot be changed */
#endif
#if defined(AFS_XBSD_ENV)
	/* Can't just invent one, must use crget() because of mutex */
	afs_osi_credp = crdup(osi_curcred());
#else
	memset(&afs_osi_cred, 0, sizeof(struct AFS_UCRED));
#if defined(AFS_LINUX26_ENV)
        afs_osi_cred.cr_group_info = groups_alloc(0);
#endif
#if defined(AFS_DARWIN80_ENV)
        afs_osi_cred.cr_ref = 1; /* kauth_cred_get_ref needs 1 existing ref */
#else
	crhold(&afs_osi_cred);	/* don't let it evaporate */
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
#ifdef AFS_LINUX22_ENV
    osi_linux_mask();
#endif
}

/* unmask signals in rxk listener */
void
afs_osi_UnmaskRxkSignals(void)
{
}

/* Two hacks to try and fix afsdb */
void 
afs_osi_MaskUserLoop()
{
#ifdef AFS_DARWIN_ENV
    afs_osi_Invisible();
    afs_osi_fullSigMask();
#else
    afs_osi_MaskSignals();
#endif
}

void 
afs_osi_UnmaskUserLoop()
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
#ifdef AFS_LINUX22_ENV
    afs_osi_MaskSignals();
#elif defined(AFS_SUN5_ENV)
    curproc->p_flag |= SSYS;
#elif defined(AFS_HPUX101_ENV) && !defined(AFS_HPUX1123_ENV)
    set_system_proc(u.u_procp);
#elif defined(AFS_DARWIN80_ENV)
#elif defined(AFS_DARWIN_ENV)
    /* maybe call init_process instead? */
    current_proc()->p_flag |= P_SYSTEM;
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
#elif defined(AFS_XBSD_ENV)
    curproc->p_flag &= ~P_SYSTEM;
#endif
}

#if !defined(AFS_LINUX20_ENV) && !defined(AFS_XBSD_ENV)
/* set the real time */
void
afs_osi_SetTime(osi_timeval_t * atv)
{
#if defined(AFS_AIX32_ENV)
    struct timestruc_t t;

    t.tv_sec = atv->tv_sec;
    t.tv_nsec = atv->tv_usec * 1000;
    ksettimer(&t);		/*  Was -> settimer(TIMEOFDAY, &t); */
#elif defined(AFS_SUN55_ENV)
    stime(atv->tv_sec);
#elif defined(AFS_SUN5_ENV)
    /*
     * To get more than second resolution we can use adjtime. The problem
     * is that the usecs from the server are wrong (by now) so it isn't
     * worth complicating the following code.
     */
    struct stimea {
	time_t time;
    } sta;

    sta.time = atv->tv_sec;

    stime(&sta, NULL);
#elif defined(AFS_SGI_ENV)
    struct stimea {
	sysarg_t time;
    } sta;

    AFS_GUNLOCK();
    sta.time = atv->tv_sec;
    stime(&sta);
    AFS_GLOCK();
#elif defined(AFS_DARWIN_ENV)
#ifndef AFS_DARWIN80_ENV
    AFS_GUNLOCK();
    setthetime(atv);
    AFS_GLOCK();
#endif
#else
    /* stolen from kern_time.c */
#ifndef	AFS_AUX_ENV
    boottime.tv_sec += atv->tv_sec - time.tv_sec;
#endif
#ifdef AFS_HPUX_ENV
    {
#if !defined(AFS_HPUX1122_ENV)
	/* drop the setting of the clock for now. spl7 is not
	 * known on hpux11.22
	 */
	register ulong_t s;
	struct timeval t;
	t.tv_sec = atv->tv_sec;
	t.tv_usec = atv->tv_usec;
	s = spl7();
	time = t;
	(void)splx(s);
	resettodr(atv);
#endif
    }
#else
    {
	register int s;
	s = splclock();
	time = *atv;
	(void)splx(s);
    }
    resettodr();
#endif
#ifdef	AFS_AUX_ENV
    logtchg(atv->tv_sec);
#endif
#endif /* AFS_DARWIN_ENV */
    AFS_STATCNT(osi_SetTime);
}
#endif /* AFS_LINUX20_ENV */


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
    shutdown_osisleep();
#endif
    if (afs_cold_shutdown) {
	LOCK_INIT(&afs_ftf, "afs_ftf");
    }
}

#ifndef AFS_OBSD_ENV
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
