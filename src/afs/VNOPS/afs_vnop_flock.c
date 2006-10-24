/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 *
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"
#include "afs/unified_afs.h"

/* Static prototypes */
static int HandleGetLock(register struct vcache *avc,
			 register struct AFS_FLOCK *af,
			 register struct vrequest *areq, int clid);
static int GetFlockCount(struct vcache *avc, struct vrequest *areq);
static int lockIdcmp2(struct AFS_FLOCK *flock1, struct vcache *vp,
		      register struct SimpleLocks *alp, int onlymine,
		      int clid);
static void DoLockWarning(void);

/* int clid;  * non-zero on SGI, OSF, SunOS, Darwin, xBSD ** XXX ptr type */
void
lockIdSet(struct AFS_FLOCK *flock, struct SimpleLocks *slp, int clid)
{
#if	defined(AFS_SUN5_ENV)
    register proc_t *procp = ttoproc(curthread);
#else
#if !defined(AFS_AIX41_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_SGI65_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
#ifdef AFS_SGI_ENV
    struct proc *procp = OSI_GET_CURRENT_PROCP();
#else
    struct proc *procp = u.u_procp;
#endif /* AFS_SGI_ENV */
#endif
#endif
#if defined(AFS_SGI65_ENV)
    flid_t flid;
    get_current_flid(&flid);
#endif

    if (slp) {
#ifdef	AFS_AIX32_ENV
#ifdef	AFS_AIX41_ENV
	slp->sysid = 0;
	slp->pid = getpid();
#else
	slp->sysid = u.u_sysid;
	slp->pid = u.u_epid;
#endif
#else
#if	defined(AFS_AIX_ENV) || defined(AFS_SUN5_ENV)
#ifdef	AFS_SUN53_ENV
	slp->sysid = 0;
	slp->pid = procp->p_pid;
#else
	slp->sysid = procp->p_sysid;
	slp->pid = procp->p_epid;
#endif
#else
#if defined(AFS_SGI_ENV)
#ifdef AFS_SGI65_ENV
	slp->sysid = flid.fl_sysid;
#else
	slp->sysid = OSI_GET_CURRENT_SYSID();
#endif
	slp->pid = clid;
#else
#if	defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	slp->pid = clid;
#else
#if defined(AFS_LINUX20_ENV) || defined(AFS_HPUX_ENV)
	slp->pid = getpid();
#else
	slp->pid = u.u_procp->p_pid;
#endif
#endif
#endif /* AFS_AIX_ENV */
#endif /* AFS_AIX32_ENV */
#endif
    } else {
#if	defined(AFS_AIX32_ENV)
#ifdef	AFS_AIX41_ENV
	flock->l_sysid = 0;
	flock->l_pid = getpid();
#else
	flock->l_sysid = u.u_sysid;
	flock->l_pid = u.u_epid;
#endif
#else
#if	defined(AFS_AIX_ENV)  || defined(AFS_SUN5_ENV)
#ifdef	AFS_SUN53_ENV
	flock->l_sysid = 0;
	flock->l_pid = procp->p_pid;
#else
	flock->l_sysid = procp->p_sysid;
	flock->l_pid = procp->p_epid;
#endif
#else
#if defined(AFS_SGI_ENV)
#ifdef AFS_SGI65_ENV
	flock->l_sysid = flid.fl_sysid;
#else
	flock->l_sysid = OSI_GET_CURRENT_SYSID();
#endif
	flock->l_pid = clid;
#else
#if	defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	flock->l_pid = clid;
#else
#if defined(AFS_LINUX20_ENV) || defined(AFS_HPUX_ENV)
	flock->l_pid = getpid();
#else
	flock->l_pid = u.u_procp->p_pid;
#endif
#endif
#endif
#endif /* AFS_AIX_ENV */
#endif /* AFS_AIX32_ENV */
    }
}

/* return 1 (true) if specified flock does not match alp (if 
 * specified), or any of the slp structs (if alp == 0) 
 */
/* I'm not sure that the comparsion of flock->pid to p_ppid
 * is correct.  Should that be a comparision of alp (or slp) ->pid  
 * to p_ppid?  Especially in the context of the lower loop, where
 * the repeated comparison doesn't make much sense...
 */
/* onlymine - don't match any locks which are held by my parent */
/* clid - only irix 6.5 */

static int
lockIdcmp2(struct AFS_FLOCK *flock1, struct vcache *vp,
	   register struct SimpleLocks *alp, int onlymine, int clid)
{
    register struct SimpleLocks *slp;
#if	defined(AFS_SUN5_ENV)
    register proc_t *procp = ttoproc(curthread);
#else
#if !defined(AFS_AIX41_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_SGI65_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
#ifdef AFS_SGI64_ENV
    struct proc *procp = curprocp;
#else /* AFS_SGI64_ENV */
    struct proc *procp = u.u_procp;
#endif /* AFS_SGI64_ENV */
#endif
#endif

    if (alp) {
#if	defined(AFS_AIX_ENV) || defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
	if (flock1->l_sysid != alp->sysid) {
	    return 1;
	}
#endif
	if ((flock1->l_pid == alp->pid) ||
#if defined(AFS_AIX41_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_HPUX_ENV)
	    (!onlymine && (flock1->l_pid == getppid()))
#else
#if defined(AFS_SGI65_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	    /* XXX check this. used to be *only* irix for some reason. */
	    (!onlymine && (flock1->l_pid == clid))
#else
	    (!onlymine && (flock1->l_pid == procp->p_ppid))
#endif
#endif
	    ) {
	    return 0;
	}
	return 1;
    }

    for (slp = vp->slocks; slp; slp = slp->next) {
#if defined(AFS_HAVE_FLOCK_SYSID)
	if (flock1->l_sysid != slp->sysid) {
	    continue;
	}
#endif
	if (flock1->l_pid == slp->pid) {
	    return 0;
	}
    }
    return (1);			/* failure */
}


/* we don't send multiple read flocks to the server, but rather just count
    them up ourselves.  Of course, multiple write locks are incompatible.
    
    Note that we should always try to release a lock, even if we have
    a network problem sending the release command through, since often
    a lock is released on a close call, when the user can't retry anyway.
    
    After we remove it from our structure, the lock will no longer be
    kept alive, and the server should time it out within a few minutes.
    
    94.04.13 add "force" parameter.  If a child explicitly unlocks a
    file, I guess we'll permit it.  however, we don't want simple,
    innocent closes by children to unlock files in the parent process.
*/
/* clid - nonzero on sgi sunos osf1 only */
int
HandleFlock(register struct vcache *avc, int acom, struct vrequest *areq,
	    pid_t clid, int onlymine)
{
    struct conn *tc;
    struct SimpleLocks *slp, *tlp, **slpp;
    afs_int32 code;
    struct AFSVolSync tsync;
    afs_int32 lockType;
    struct AFS_FLOCK flock;
    XSTATS_DECLS;
    AFS_STATCNT(HandleFlock);
    code = 0;			/* default when we don't make any network calls */
    lockIdSet(&flock, NULL, clid);

#if defined(AFS_SGI_ENV)
    osi_Assert(valusema(&avc->vc_rwlock) <= 0);
    osi_Assert(OSI_GET_LOCKID() == avc->vc_rwlockid);
#endif
    ObtainWriteLock(&avc->lock, 118);
    if (acom & LOCK_UN) {

/* defect 3083 */

#ifdef AFS_AIX_ENV
	/* If the lock is held exclusive, then only the owning process
	 * or a child can unlock it. Use pid and ppid because they are
	 * unique identifiers.
	 */
	if ((avc->flockCount < 0) && (getpid() != avc->ownslock)) {
#ifdef	AFS_AIX41_ENV
	    if (onlymine || (getppid() != avc->ownslock)) {
#else
	    if (onlymine || (u.u_procp->p_ppid != avc->ownslock)) {
#endif
		ReleaseWriteLock(&avc->lock);
		return 0;
	    }
	}
#endif
	if (lockIdcmp2(&flock, avc, NULL, onlymine, clid)) {
	    ReleaseWriteLock(&avc->lock);
	    return 0;
	}
#ifdef AFS_AIX_ENV
	avc->ownslock = 0;
#endif
	if (avc->flockCount == 0) {
	    ReleaseWriteLock(&avc->lock);
	    return 0 /*ENOTTY*/;
	    /* no lock held */
	}
	/* unlock the lock */
	if (avc->flockCount > 0) {
	    slpp = &avc->slocks;
	    for (slp = *slpp; slp;) {
		if (!lockIdcmp2(&flock, avc, slp, onlymine, clid)) {
		    avc->flockCount--;
		    tlp = *slpp = slp->next;
		    osi_FreeSmallSpace(slp);
		    slp = tlp;
		} else {
		    slpp = &slp->next;
		    slp = *slpp;
		}
	    }
	} else if (avc->flockCount == -1) {
	    afs_StoreAllSegments(avc, areq, AFS_ASYNC);	/* fsync file early */
	    avc->flockCount = 0;
	    /* And remove the (only) exclusive lock entry from the list... */
	    osi_FreeSmallSpace(avc->slocks);
	    avc->slocks = 0;
	}
	if (avc->flockCount == 0) {
	    do {
		tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
		if (tc) {
		    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_RELEASELOCK);
		    RX_AFS_GUNLOCK();
		    code = RXAFS_ReleaseLock(tc->id, (struct AFSFid *)
					     &avc->fid.Fid, &tsync);
		    RX_AFS_GLOCK();
		    XSTATS_END_TIME;
		} else
		    code = -1;
	    } while (afs_Analyze
		     (tc, code, &avc->fid, areq,
		      AFS_STATS_FS_RPCIDX_RELEASELOCK, SHARED_LOCK, NULL));
	}
    } else {
	while (1) {		/* set a new lock */
	    /*
	     * Upgrading from shared locks to Exclusive and vice versa
	     * is a bit tricky and we don't really support it yet. But
	     * we try to support the common used one which is upgrade
	     * a shared lock to an exclusive for the same process...
	     */
	    if ((avc->flockCount > 0 && (acom & LOCK_EX))
		|| (avc->flockCount == -1 && (acom & LOCK_SH))) {
		/*
		 * Upgrading from shared locks to an exclusive one:
		 * For now if all the shared locks belong to the
		 * same process then we unlock them on the server
		 * and proceed with the upgrade.  Unless we change the
		 * server's locking interface impl we prohibit from
		 * unlocking other processes's shared locks...
		 * Upgrading from an exclusive lock to a shared one:
		 * Again only allowed to be done by the same process.
		 */
		slpp = &avc->slocks;
		for (slp = *slpp; slp;) {
		    if (!lockIdcmp2
			(&flock, avc, slp, 1 /*!onlymine */ , clid)) {
			if (acom & LOCK_EX)
			    avc->flockCount--;
			else
			    avc->flockCount = 0;
			tlp = *slpp = slp->next;
			osi_FreeSmallSpace(slp);
			slp = tlp;
		    } else {
			code = EWOULDBLOCK;
			slpp = &slp->next;
			slp = *slpp;
		    }
		}
		if (!code && avc->flockCount == 0) {
		    do {
			tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
			if (tc) {
			    XSTATS_START_TIME
				(AFS_STATS_FS_RPCIDX_RELEASELOCK);
			    RX_AFS_GUNLOCK();
			    code =
				RXAFS_ReleaseLock(tc->id,
						  (struct AFSFid *)&avc->fid.
						  Fid, &tsync);
			    RX_AFS_GLOCK();
			    XSTATS_END_TIME;
			} else
			    code = -1;
		    } while (afs_Analyze
			     (tc, code, &avc->fid, areq,
			      AFS_STATS_FS_RPCIDX_RELEASELOCK, SHARED_LOCK,
			      NULL));
		}
	    } else if (avc->flockCount == -1 && (acom & LOCK_EX)) {
		if (lockIdcmp2(&flock, avc, NULL, 1, clid)) {
		    code = EWOULDBLOCK;
		} else
		    code = 0;
	    }
	    if (code == 0) {
		/* compatible here, decide if needs to go to file server.  If
		 * we've already got the file locked (and thus read-locked, since
		 * we've already checked for compatibility), we shouldn't send
		 * the call through to the server again */
		if (avc->flockCount == 0) {
		    /* we're the first on our block, send the call through */
		    lockType = ((acom & LOCK_EX) ? LockWrite : LockRead);
		    do {
			tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
			if (tc) {
			    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_SETLOCK);
			    RX_AFS_GUNLOCK();
			    code = RXAFS_SetLock(tc->id, (struct AFSFid *)
						 &avc->fid.Fid, lockType,
						 &tsync);
			    RX_AFS_GLOCK();
			    XSTATS_END_TIME;
			} else
			    code = -1;
		    } while (afs_Analyze
			     (tc, code, &avc->fid, areq,
			      AFS_STATS_FS_RPCIDX_SETLOCK, SHARED_LOCK,
			      NULL));
		} else
		    code = 0;	/* otherwise, pretend things worked */
	    }
	    if (code == 0) {
		slp = (struct SimpleLocks *)
		    osi_AllocSmallSpace(sizeof(struct SimpleLocks));
		if (acom & LOCK_EX) {

/* defect 3083 */

#ifdef AFS_AIX_ENV
		    /* Record unique id of process owning exclusive lock. */
		    avc->ownslock = getpid();
#endif

		    slp->type = LockWrite;
		    slp->next = NULL;
		    avc->slocks = slp;
		    avc->flockCount = -1;
		} else {
		    slp->type = LockRead;
		    slp->next = avc->slocks;
		    avc->slocks = slp;
		    avc->flockCount++;
		}

		lockIdSet(&flock, slp, clid);
		break;
	    }
	    /* now, if we got EWOULDBLOCK, and we're supposed to wait, we do */
	    if (((code == EWOULDBLOCK) || (code == EAGAIN) || 
		 (code == UAEWOULDBLOCK) || (code == UAEAGAIN))
		&& !(acom & LOCK_NB)) {
		/* sleep for a second, allowing interrupts */
		ReleaseWriteLock(&avc->lock);
#if defined(AFS_SGI_ENV)
		AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#endif
		code = afs_osi_Wait(1000, NULL, 1);
#if defined(AFS_SGI_ENV)
		AFS_RWLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#endif
		ObtainWriteLock(&avc->lock, 120);
		if (code) {
		    code = EINTR;	/* return this if ^C typed */
		    break;
		}
	    } else
		break;
	}			/* while loop */
    }
    ReleaseWriteLock(&avc->lock);
    code = afs_CheckCode(code, areq, 1);	/* defeat a buggy AIX optimization */
    return code;
}


/* warn a user that a lock has been ignored */
afs_int32 lastWarnTime = 0;	/* this is used elsewhere */
static void
DoLockWarning(void)
{
    register afs_int32 now;
    now = osi_Time();

    AFS_STATCNT(DoLockWarning);
    /* check if we've already warned someone recently */
    if (now < lastWarnTime + 120)
	return;

    /* otherwise, it is time to nag the user */
    lastWarnTime = now;
    afs_warn
	("afs: byte-range lock/unlock ignored; make sure no one else is running this program.\n");
}


#ifdef	AFS_OSF_ENV
int afs_lockctl(struct vcache * avc, struct eflock * af, int flag,
		struct AFS_UCRED * acred, pid_t clid, off_t offset)
#elif defined(AFS_SGI_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
int afs_lockctl(struct vcache * avc, struct AFS_FLOCK * af, int acmd,
		struct AFS_UCRED * acred, pid_t clid)
#else
u_int clid = 0;
int afs_lockctl(struct vcache * avc, struct AFS_FLOCK * af, int acmd,
		struct AFS_UCRED * acred)
#endif
{
    struct vrequest treq;
    afs_int32 code;
#ifdef	AFS_OSF_ENV
    int acmd = 0;
#endif
    struct afs_fakestat_state fakestate;

    AFS_STATCNT(afs_lockctl);
    if ((code = afs_InitReq(&treq, acred)))
	return code;
    afs_InitFakeStat(&fakestate);
    code = afs_EvalFakeStat(&avc, &fakestate, &treq);
    if (code) {
	afs_PutFakeStat(&fakestate);
	return code;
    }
#ifdef	AFS_OSF_ENV
    if (flag & VNOFLCK) {
	afs_PutFakeStat(&fakestate);
	return 0;
    }
    if (flag & CLNFLCK) {
	acmd = LOCK_UN;
    } else if ((flag & GETFLCK) || (flag & RGETFLCK)) {
	acmd = F_GETLK;
    } else if ((flag & SETFLCK) || (flag & RSETFLCK)) {
	acmd = F_SETLK;
    }
#endif
#if (defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)) && !defined(AFS_SUN58_ENV)
    if ((acmd == F_GETLK) || (acmd == F_RGETLK)) {
#else
    if (acmd == F_GETLK) {
#endif
	if (af->l_type == F_UNLCK) {
	    afs_PutFakeStat(&fakestate);
	    return 0;
	}
#ifndef	AFS_OSF_ENV		/* getlock is a no-op for osf (for now) */
	code = HandleGetLock(avc, af, &treq, clid);
#endif
	code = afs_CheckCode(code, &treq, 2);	/* defeat buggy AIX optimz */
	afs_PutFakeStat(&fakestate);
	return code;
    } else if ((acmd == F_SETLK) || (acmd == F_SETLKW)
#if (defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)) && !defined(AFS_SUN58_ENV)
	       || (acmd == F_RSETLK) || (acmd == F_RSETLKW)) {
#else
	) {
#endif
    /* this next check is safer when left out, but more applications work
     * with it in.  However, they fail in race conditions.  The question is
     * what to do for people who don't have source to their application;
     * this way at least, they can get work done */
#ifdef AFS_LINUX24_ENV
    if (af->l_len == OFFSET_MAX)
	af->l_len = 0;		/* since some systems indicate it as EOF */
#else
    if (af->l_len == 0x7fffffff)
	af->l_len = 0;		/* since some systems indicate it as EOF */
#ifdef AFS_LINUX_64BIT_KERNEL
    if (af->l_len == LONG_MAX)
	af->l_len = 0;		/* since some systems indicate it as EOF */
#endif
#endif
    /* next line makes byte range locks always succeed,
     * even when they should block */
    if (af->l_whence != 0 || af->l_start != 0 || af->l_len != 0) {
	DoLockWarning();
	afs_PutFakeStat(&fakestate);
	return 0;
    }
    /* otherwise we can turn this into a whole-file flock */
    if (af->l_type == F_RDLCK)
	code = LOCK_SH;
    else if (af->l_type == F_WRLCK)
	code = LOCK_EX;
    else if (af->l_type == F_UNLCK)
	code = LOCK_UN;
    else {
	afs_PutFakeStat(&fakestate);
	return EINVAL;		/* unknown lock type */
    }
    if (((acmd == F_SETLK)
#if 	(defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)) && !defined(AFS_SUN58_ENV)
	 || (acmd == F_RSETLK)
#endif
	) && code != LOCK_UN)
	code |= LOCK_NB;	/* non-blocking, s.v.p. */
#if	defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV)
    code = HandleFlock(avc, code, &treq, clid, 0 /*!onlymine */ );
#elif defined(AFS_SGI_ENV)
    AFS_RWLOCK((vnode_t *) avc, VRWLOCK_WRITE);
    code = HandleFlock(avc, code, &treq, clid, 0 /*!onlymine */ );
    AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#else
    code = HandleFlock(avc, code, &treq, 0, 0 /*!onlymine */ );
#endif
    code = afs_CheckCode(code, &treq, 3);	/* defeat AIX -O bug */
    afs_PutFakeStat(&fakestate);
    return code;
    }
    afs_PutFakeStat(&fakestate);
    return EINVAL;
}


/*
 * Get a description of the first lock which would
 * block the lock specified.  If the specified lock
 * would succeed, fill in the lock structure with 'F_UNLCK'.
 *
 * To do that, we have to ask the server for the lock
 * count if:
 *    1. The file is not locked by this machine.
 *    2. Asking for write lock, and only the current
 *       PID has the file read locked.
 */
#ifndef	AFS_OSF_ENV		/* getlock is a no-op for osf (for now) */
static int
HandleGetLock(register struct vcache *avc, register struct AFS_FLOCK *af,
	      register struct vrequest *areq, int clid)
{
    register afs_int32 code;
    struct AFS_FLOCK flock;

    lockIdSet(&flock, NULL, clid);

    ObtainWriteLock(&avc->lock, 122);
    if (avc->flockCount == 0) {
	/*
	 * We don't know ourselves, so ask the server. Unfortunately, we
	 * don't know the pid.  Not even the server knows the pid.  Besides,
	 * the process with the lock is on another machine
	 */
	code = GetFlockCount(avc, areq);
	if (code == 0 || (af->l_type == F_RDLCK && code > 0)) {
	    af->l_type = F_UNLCK;
	    goto unlck_leave;
	}
	if (code > 0)
	    af->l_type = F_RDLCK;
	else
	    af->l_type = F_WRLCK;

	af->l_pid = 0;
#if defined(AFS_HAVE_FLOCK_SYSID)
	af->l_sysid = 0;
#endif
	goto done;
    }

    if (af->l_type == F_RDLCK) {
	/*
	 * We want a read lock.  If there are only
	 * read locks, or we are the one with the
	 * write lock, say it is unlocked.
	 */
	if (avc->flockCount > 0 ||	/* only read locks */
	    !lockIdcmp2(&flock, avc, NULL, 1, clid)) {
	    af->l_type = F_UNLCK;
	    goto unlck_leave;
	}

	/* one write lock, but who? */
	af->l_type = F_WRLCK;	/* not us, so lock would block */
	if (avc->slocks) {	/* we know who, so tell */
	    af->l_pid = avc->slocks->pid;
#if defined(AFS_HAVE_FLOCK_SYSID)
	    af->l_sysid = avc->slocks->sysid;
#endif
	} else {
	    af->l_pid = 0;	/* XXX can't happen?? */
#if defined(AFS_HAVE_FLOCK_SYSID)
	    af->l_sysid = 0;
#endif
	}
	goto done;
    }

    /*
     * Ok, we want a write lock.  If there is a write lock
     * already, and it is not this process, we fail.
     */
    if (avc->flockCount < 0) {
	if (lockIdcmp2(&flock, avc, NULL, 1, clid)) {
	    af->l_type = F_WRLCK;
	    if (avc->slocks) {
		af->l_pid = avc->slocks->pid;
#if defined(AFS_HAVE_FLOCK_SYSID)
		af->l_sysid = avc->slocks->sysid;
#endif
	    } else {
		af->l_pid = 0;	/* XXX can't happen?? */
#if defined(AFS_HAVE_FLOCK_SYSID)
		af->l_sysid = 0;
#endif
	    }
	    goto done;
	}
	/* we are the one with the write lock */
	af->l_type = F_UNLCK;
	goto unlck_leave;
    }

    /*
     * Want a write lock, and we know there are read locks.
     * If there is more than one, or it isn't us, we cannot lock.
     */
    if ((avc->flockCount > 1)
	|| lockIdcmp2(&flock, avc, NULL, 1, clid)) {
	struct SimpleLocks *slp;

	af->l_type = F_RDLCK;
	af->l_pid = 0;
#if defined(AFS_HAVE_FLOCK_SYSID)
	af->l_sysid = 0;
#endif
	/* find a pid that isn't our own */
	for (slp = avc->slocks; slp; slp = slp->next) {
	    if (lockIdcmp2(&flock, NULL, slp, 1, clid)) {
		af->l_pid = slp->pid;
#if defined(AFS_HAVE_FLOCK_SYSID)
		af->l_sysid = avc->slocks->sysid;
#endif
		break;
	    }
	}
	goto done;
    }

    /*
     * Ok, we want a write lock.  If there is a write lock
     * already, and it is not this process, we fail.
     */
    if (avc->flockCount < 0) {
	if (lockIdcmp2(&flock, avc, NULL, 1, clid)) {
	    af->l_type = F_WRLCK;
	    if (avc->slocks) {
		af->l_pid = avc->slocks->pid;
#if defined(AFS_HAVE_FLOCK_SYSID)
		af->l_sysid = avc->slocks->sysid;
#endif
	    } else {
		af->l_pid = 0;	/* XXX can't happen?? */
#if defined(AFS_HAVE_FLOCK_SYSID)
		af->l_sysid = 0;
#endif
	    }
	    goto done;
	}
	/* we are the one with the write lock */
	af->l_type = F_UNLCK;
	goto unlck_leave;
    }

    /*
     * Want a write lock, and we know there are read locks.
     * If there is more than one, or it isn't us, we cannot lock.
     */
    if ((avc->flockCount > 1)
	|| lockIdcmp2(&flock, avc, NULL, 1, clid)) {
	struct SimpleLocks *slp;
	af->l_type = F_RDLCK;
	af->l_pid = 0;
#if defined(AFS_HAVE_FLOCK_SYSID)
	af->l_sysid = 0;
#endif
	/* find a pid that isn't our own */
	for (slp = avc->slocks; slp; slp = slp->next) {
	    if (lockIdcmp2(&flock, NULL, slp, 1, clid)) {
		af->l_pid = slp->pid;
#if defined(AFS_HAVE_FLOCK_SYSID)
		af->l_sysid = avc->slocks->sysid;
#endif
		break;
	    }
	}
	goto done;
    }

    /*
     * Want a write lock, and there is just one read lock, and it
     * is this process with a read lock.  Ask the server if there
     * are any more processes with the file locked.
     */
    code = GetFlockCount(avc, areq);
    if (code == 0 || code == 1) {
	af->l_type = F_UNLCK;
	goto unlck_leave;
    }
    if (code > 0)
	af->l_type = F_RDLCK;
    else
	af->l_type = F_WRLCK;
    af->l_pid = 0;
#if defined(AFS_HAVE_FLOCK_SYSID)
    af->l_sysid = 0;
#endif

  done:
    af->l_whence = 0;
    af->l_start = 0;
    af->l_len = 0;		/* to end of file */

  unlck_leave:
    ReleaseWriteLock(&avc->lock);
    return 0;
}

/* Get the 'flock' count from the server.  This comes back in a 'spare'
 * field from a GetStatus RPC.  If we have any problems with the RPC,
 * we lie and say the file is unlocked.  If we ask any 'old' fileservers,
 * the spare field will be a zero, saying the file is unlocked.  This is
 * OK, as a further 'lock' request will do the right thing.
 */
static int
GetFlockCount(struct vcache *avc, struct vrequest *areq)
{
    register struct conn *tc;
    register afs_int32 code;
    struct AFSFetchStatus OutStatus;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    int temp;
    XSTATS_DECLS;
    temp = areq->flags & O_NONBLOCK;
    areq->flags |= O_NONBLOCK;

    do {
	tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_FETCHSTATUS);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_FetchStatus(tc->id, (struct AFSFid *)&avc->fid.Fid,
				  &OutStatus, &CallBack, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, code, &avc->fid, areq, AFS_STATS_FS_RPCIDX_FETCHSTATUS,
	      SHARED_LOCK, NULL));

    if (temp)
	areq->flags &= ~O_NONBLOCK;

    if (code) {
	return (0);		/* failed, say it is 'unlocked' */
    } else {
	return ((int)OutStatus.lockCount);
    }
}
#endif


#if	!defined(AFS_AIX_ENV) && !defined(AFS_HPUX_ENV) && !defined(AFS_SUN5_ENV) && !defined(AFS_SGI_ENV) && !defined(UKERNEL) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
/* Flock not support on System V systems */
#ifdef AFS_OSF_ENV
extern struct fileops afs_fileops;

int
afs_xflock(struct proc *p, void *args, int *retval)
#else /* AFS_OSF_ENV */
int
afs_xflock(void)
#endif
{
    int code = 0;
    struct a {
	int fd;
	int com;
    } *uap;
    struct file *fd;
    struct vrequest treq;
    struct vcache *tvc;
    int flockDone;
    struct afs_fakestat_state fakestate;

    afs_InitFakeStat(&fakestate);
    AFS_STATCNT(afs_xflock);
    flockDone = 0;
#ifdef AFS_OSF_ENV
    uap = (struct a *)args;
    getf(&fd, uap->fd, FILE_FLAGS_NULL, &u.u_file_state);
#else /* AFS_OSF_ENV */
    uap = (struct a *)u.u_ap;
    fd = getf(uap->fd);
#endif
    if (!fd) {
	afs_PutFakeStat(&fakestate);
	return;
    }

    if (flockDone = afs_InitReq(&treq, u.u_cred)) {
	afs_PutFakeStat(&fakestate);
	return flockDone;
    }
    /* first determine whether this is any sort of vnode */
    if (fd->f_type == DTYPE_VNODE) {
	/* good, this is a vnode; next see if it is an AFS vnode */
	tvc = VTOAFS(fd->f_data);	/* valid, given a vnode */
	if (IsAfsVnode(AFSTOV(tvc))) {
	    /* This is an AFS vnode, so do the work */
	    code = afs_EvalFakeStat(&tvc, &fakestate, &treq);
	    if (code) {
		afs_PutFakeStat(&fakestate);
		return code;
	    }
	    if ((fd->f_flag & (FEXLOCK | FSHLOCK)) && !(uap->com & LOCK_UN)) {
		/* First, if fd already has lock, release it for relock path */
#if defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV)
		HandleFlock(tvc, LOCK_UN, &treq, u.u_procp->p_pid,
			    0 /*!onlymine */ );
#else
		HandleFlock(tvc, LOCK_UN, &treq, 0, 0 /*!onlymine */ );
#endif
		fd->f_flag &= ~(FEXLOCK | FSHLOCK);
	    }
	    /* now try the requested operation */

#if defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV)
	    code =
		HandleFlock(tvc, uap->com, &treq, u.u_procp->p_pid,
			    0 /*!onlymine */ );
#else
	    code = HandleFlock(tvc, uap->com, &treq, 0, 0 /*!onlymine */ );
#endif
#ifndef AFS_OSF_ENV
	    u.u_error = code;
#endif

	    if (uap->com & LOCK_UN) {
		/* gave up lock */
		fd->f_flag &= ~(FEXLOCK | FSHLOCK);
	    } else {
#ifdef AFS_OSF_ENV
		if (!code) {
#else /* AFS_OSF_ENV */
		if (!u.u_error) {
#endif
		    if (uap->com & LOCK_SH)
			fd->f_flag |= FSHLOCK;
		    else if (uap->com & LOCK_EX)
			fd->f_flag |= FEXLOCK;
		}
	    }
	    flockDone = 1;
	    fd->f_ops = &afs_fileops;
	}
    }
#ifdef	AFS_OSF_ENV
    if (!flockDone)
	code = flock(p, args, retval);
#ifdef	AFS_OSF30_ENV
    FP_UNREF_ALWAYS(fd);
#else
    FP_UNREF(fd);
#endif
    afs_PutFakeStat(&fakestate);
    return code;
#else /* AFS_OSF_ENV */
    if (!flockDone)
#ifdef DYNEL
	(*afs_longcall_procs.LC_flock) ();
#else
	flock();
#endif
    afs_PutFakeStat(&fakestate);
    return;
#endif
}
#endif /* !defined(AFS_AIX_ENV) && !defined(AFS_HPUX_ENV) && !defined(AFS_SUN5_ENV) && !defined(UKERNEL)  && !defined(AFS_LINUX20_ENV) */
