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


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"
#include "afs/unified_afs.h"





/* Static prototypes */
static int HandleGetLock(struct vcache *avc,
			 struct AFS_FLOCK *af,
			 struct vrequest *areq, int clid);
static int GetFlockCount(struct vcache *avc, struct vrequest *areq);
static int lockIdcmp2(struct AFS_FLOCK *flock1, struct vcache *vp,
		      struct SimpleLocks *alp, int onlymine,
		      int clid);

/* int clid;  * non-zero on SGI, OSF, SunOS, Darwin, xBSD ** XXX ptr type */

#if defined(AFS_SUN5_ENV)
void
lockIdSet(struct AFS_FLOCK *flock, struct SimpleLocks *slp, int clid)
{
    proc_t *procp = ttoproc(curthread);

    if (slp) {
# ifdef AFS_SUN53_ENV
	slp->sysid = 0;
	slp->pid = procp->p_pid;
# else
	slp->sysid = procp->p_sysid;
	slp->pid = procp->p_epid;
# endif
    } else {
# ifdef AFS_SUN53_ENV
	flock->l_sysid = 0;
	flock->l_pid = procp->p_pid;
# else
	flock->l_sysid = procp->p_sysid;
	flock->l_pid = procp->p_epid;
# endif
    }
}
#elif defined(AFS_SGI_ENV)
void
lockIdSet(struct AFS_FLOCK *flock, struct SimpleLocks *slp, int clid)
{
# if defined(AFS_SGI65_ENV)
    flid_t flid;
    get_current_flid(&flid);
# else
    afs_proc_t *procp = OSI_GET_CURRENT_PROCP();
# endif

    if (slp) {
# ifdef AFS_SGI65_ENV
	slp->sysid = flid.fl_sysid;
# else
	slp->sysid = OSI_GET_CURRENT_SYSID();
# endif
	slp->pid = clid;
    } else {
# ifdef AFS_SGI65_ENV
	flock->l_sysid = flid.fl_sysid;
# else
	flock->l_sysid = OSI_GET_CURRENT_SYSID();
# endif
	flock->l_pid = clid;
    }
}
#elif defined(AFS_AIX_ENV)
void
lockIdSet(struct AFS_FLOCK *flock, struct SimpleLocks *slp, int clid)
{
# if !defined(AFS_AIX32_ENV)
    afs_proc_t *procp = u.u_procp;
# endif

    if (slp) {
# if defined(AFS_AIX41_ENV)
	slp->sysid = 0;
	slp->pid = getpid();
# elif defined(AFS_AIX32_ENV)
	slp->sysid = u.u_sysid;
	slp->pid = u.u_epid;
# else
	slp->sysid = procp->p_sysid;
	slp->pid = prcop->p_epid;
# endif
    } else {
# if defined(AFS_AIX41_ENV)
	flock->l_sysid = 0;
	flock->l_pid = getpid();
# elif defined(AFS_AIX32_ENV)
	flock->l_sysid = u.u_sysid;
	flock->l_pid = u.u_epid;
# else
	flock->l_sysid = procp->p_sysid;
	flock->l_pid = procp->p_epid;
# endif
    }
}
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
void
lockIdSet(struct AFS_FLOCK *flock, struct SimpleLocks *slp, int clid)
{
    if (slp) {
	slp->pid = clid;
    } else {
	flock->l_pid = clid;
    }
}
#elif defined(AFS_LINUX20_ENV) || defined(AFS_HPUX_ENV)
void
lockIdSet(struct AFS_FLOCK *flock, struct SimpleLocks *slp, int clid)
{
    if (slp) {
	slp->pid = getpid();
    } else {
	flock->l_pid = getpid();
    }
}
#elif defined(UKERNEL)
void
lockIdSet(struct AFS_FLOCK *flock, struct SimpleLocks *slp, int clid)
{
    if (slp) {
	slp->pid = get_user_struct()->u_procp->p_pid;
    } else {
	flock->l_pid = get_user_struct()->u_procp->p_pid;
    }
}
#else
void
lockIdSet(struct AFS_FLOCK *flock, struct SimpleLocks *slp, int clid)
{
    if (slp) {
	slp->pid = u.u_procp->p_pid;
    } else {
	flock->l_pid = u.u_procp->p_pid;
    }
}
#endif

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
	   struct SimpleLocks *alp, int onlymine, int clid)
{
    struct SimpleLocks *slp;
#if	defined(AFS_SUN5_ENV)
    proc_t *procp = ttoproc(curthread);
#else
#if !defined(AFS_AIX41_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_SGI65_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
#ifdef AFS_SGI64_ENV
    afs_proc_t *procp = curprocp;
#elif defined(UKERNEL)
    afs_proc_t *procp = get_user_struct()->u_procp;
#else
    afs_proc_t *procp = u.u_procp;
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

    If called when disconnected support is unabled, the discon_lock must
    be held
*/
/* clid - nonzero on sgi sunos osf1 only */
int
HandleFlock(struct vcache *avc, int acom, struct vrequest *areq,
	    pid_t clid, int onlymine)
{
    struct afs_conn *tc;
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
	int stored_segments = 0;
     retry_unlock:

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
	    if (!stored_segments) {
		afs_StoreAllSegments(avc, areq, AFS_SYNC | AFS_VMSYNC);	/* fsync file early */
		/* afs_StoreAllSegments can drop and reacquire the write lock
		 * on avc and GLOCK, so the flocks may be completely different
		 * now. Go back and perform all checks again. */
		 stored_segments = 1;
		 goto retry_unlock;
	    }
	    avc->flockCount = 0;
	    /* And remove the (only) exclusive lock entry from the list... */
	    osi_FreeSmallSpace(avc->slocks);
	    avc->slocks = 0;
	}
	if (avc->flockCount == 0) {
	    if (!AFS_IS_DISCONNECTED) {
		struct rx_connection *rxconn;
	        do {
		  tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
		    if (tc) {
		        XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_RELEASELOCK);
		        RX_AFS_GUNLOCK();
		        code = RXAFS_ReleaseLock(rxconn, (struct AFSFid *)
					         &avc->f.fid.Fid, &tsync);
		        RX_AFS_GLOCK();
		        XSTATS_END_TIME;
		    } else
		    code = -1;
	        } while (afs_Analyze
		         (tc, rxconn, code, &avc->f.fid, areq,
		          AFS_STATS_FS_RPCIDX_RELEASELOCK, SHARED_LOCK, NULL));
	    } else {
	  	/*printf("Network is dooooooowwwwwwwnnnnnnn\n");*/
	       code = ENETDOWN;
	    }
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
		    struct rx_connection *rxconn;
		    if (!AFS_IS_DISCONNECTED) {
		        do {
			    tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
			    if (tc) {
			        XSTATS_START_TIME
				    (AFS_STATS_FS_RPCIDX_RELEASELOCK);
			        RX_AFS_GUNLOCK();
			        code =
				    RXAFS_ReleaseLock(rxconn,
						      (struct AFSFid *)&avc->
						      f.fid.Fid, &tsync);
			        RX_AFS_GLOCK();
				XSTATS_END_TIME;
			    } else
			        code = -1;
		        } while (afs_Analyze
			         (tc, rxconn, code, &avc->f.fid, areq,
			          AFS_STATS_FS_RPCIDX_RELEASELOCK, SHARED_LOCK,
			          NULL));
		    }
		}
	    } else if (avc->flockCount == -1 && (acom & LOCK_EX)) {
		if (lockIdcmp2(&flock, avc, NULL, 1, clid)) {
		    code = EWOULDBLOCK;
		} else {
		    code = 0;
		    /* We've just re-grabbed an exclusive lock, so we don't
		     * need to contact the fileserver, and we don't need to
		     * add the lock to avc->slocks (since we already have a
		     * lock there). So, we are done. */
		    break;
		}
	    }
	    if (code == 0) {
		/* compatible here, decide if needs to go to file server.  If
		 * we've already got the file locked (and thus read-locked, since
		 * we've already checked for compatibility), we shouldn't send
		 * the call through to the server again */
		if (avc->flockCount == 0) {
		    struct rx_connection *rxconn;
		    /* we're the first on our block, send the call through */
		    lockType = ((acom & LOCK_EX) ? LockWrite : LockRead);
		    if (!AFS_IS_DISCONNECTED) {
		        do {
			    tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
			    if (tc) {
			        XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_SETLOCK);
			        RX_AFS_GUNLOCK();
			        code = RXAFS_SetLock(rxconn, (struct AFSFid *)
						     &avc->f.fid.Fid, lockType,
						     &tsync);
			        RX_AFS_GLOCK();
			        XSTATS_END_TIME;
			    } else
			        code = -1;
		        } while (afs_Analyze
			         (tc, rxconn, code, &avc->f.fid, areq,
			          AFS_STATS_FS_RPCIDX_SETLOCK, SHARED_LOCK,
			          NULL));
			if ((lockType == LockWrite) && (code == VREADONLY))
			    code = EBADF; /* per POSIX; VREADONLY == EROFS */
		    } else
		        /* XXX - Should probably try and log this when we're
		         * XXX - running with logging enabled. But it's horrid
		         */
		        code = 0; /* pretend we worked - ick!!! */
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
static void
DoLockWarning(struct vcache *avc, afs_ucred_t * acred)
{
    static afs_uint32 lastWarnTime;
    static pid_t lastWarnPid;

    afs_uint32 now;
    pid_t pid = MyPidxx2Pid(MyPidxx);
    char *procname;
    const char *message;

    now = osi_Time();

    AFS_STATCNT(DoLockWarning);

    /* check if we've already warned this user recently */
    if ((now < lastWarnTime + 120) && (lastWarnPid == pid)) {
	return;
    }
    if (now < avc->lastBRLWarnTime + 120) {
	return;
    }

    procname = afs_osi_Alloc(256);

    if (!procname)
	return;

    /* Copies process name to allocated procname, see osi_machdeps for details of macro */
    osi_procname(procname, 256);
    procname[255] = '\0';

    lastWarnTime = avc->lastBRLWarnTime = now;
    lastWarnPid = pid;

#ifdef AFS_LINUX26_ENV
    message = "byte-range locks only enforced for processes on this machine";
#else
    message = "byte-range lock/unlock ignored; make sure no one else is running this program";
#endif

    afs_warnuser("afs: %s (pid %d (%s), user %ld, fid %lu.%lu.%lu).\n",
                 message, pid, procname, (long)afs_cr_uid(acred),
                 (unsigned long)avc->f.fid.Fid.Volume,
                 (unsigned long)avc->f.fid.Fid.Vnode,
                 (unsigned long)avc->f.fid.Fid.Unique);

    afs_osi_Free(procname, 256);
}


#if defined(AFS_SGI_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
int afs_lockctl(struct vcache * avc, struct AFS_FLOCK * af, int acmd,
		afs_ucred_t * acred, pid_t clid)
#else
u_int clid = 0;
int afs_lockctl(struct vcache * avc, struct AFS_FLOCK * af, int acmd,
		afs_ucred_t * acred)
#endif
{
    struct vrequest *treq = NULL;
    afs_int32 code;
    struct afs_fakestat_state fakestate;

    AFS_STATCNT(afs_lockctl);
    if ((code = afs_CreateReq(&treq, acred)))
	return code;
    afs_InitFakeStat(&fakestate);

    AFS_DISCON_LOCK();

    code = afs_EvalFakeStat(&avc, &fakestate, treq);
    if (code) {
	goto done;
    }
#if (defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)) && !defined(AFS_SUN58_ENV)
    if ((acmd == F_GETLK) || (acmd == F_RGETLK)) {
#else
    if (acmd == F_GETLK) {
#endif
	if (af->l_type == F_UNLCK) {
	    code = 0;
	    goto done;
	}
	code = HandleGetLock(avc, af, treq, clid);
	code = afs_CheckCode(code, treq, 2);	/* defeat buggy AIX optimz */
	goto done;
    } else if ((acmd == F_SETLK) || (acmd == F_SETLKW)
#if (defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)) && !defined(AFS_SUN58_ENV)
	       || (acmd == F_RSETLK) || (acmd == F_RSETLKW)) {
#else
	) {
#endif

    if ((avc->f.states & CRO)) {
	/* for RO volumes, don't do anything for locks; the fileserver doesn't
	 * even track them. A write lock should not be possible, though. */
	if (af->l_type == F_WRLCK) {
	    code = EBADF;
	} else {
	    code = 0;
	}
	goto done;
    }

    /* Java VMs ask for l_len=(long)-1 regardless of OS/CPU */
    if ((sizeof(af->l_len) == 8) && (af->l_len == 0x7fffffffffffffffLL))
	af->l_len = 0;
    /* next line makes byte range locks always succeed,
     * even when they should block */
    if (af->l_whence != 0 || af->l_start != 0 || af->l_len != 0) {
	DoLockWarning(avc, acred);
	code = 0;
	goto done;
    }
    /* otherwise we can turn this into a whole-file flock */
    if (af->l_type == F_RDLCK)
	code = LOCK_SH;
    else if (af->l_type == F_WRLCK)
	code = LOCK_EX;
    else if (af->l_type == F_UNLCK)
	code = LOCK_UN;
    else {
	code = EINVAL;		/* unknown lock type */
	goto done;
    }
    if (((acmd == F_SETLK)
#if 	(defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)) && !defined(AFS_SUN58_ENV)
	 || (acmd == F_RSETLK)
#endif
	) && code != LOCK_UN)
	code |= LOCK_NB;	/* non-blocking, s.v.p. */
#if defined(AFS_DARWIN_ENV)
    code = HandleFlock(avc, code, treq, clid, 0 /*!onlymine */ );
#elif defined(AFS_SGI_ENV)
    AFS_RWLOCK((vnode_t *) avc, VRWLOCK_WRITE);
    code = HandleFlock(avc, code, treq, clid, 0 /*!onlymine */ );
    AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#else
    code = HandleFlock(avc, code, treq, 0, 0 /*!onlymine */ );
#endif
    code = afs_CheckCode(code, treq, 3);	/* defeat AIX -O bug */
    goto done;
    }
    code = EINVAL;
done:
    afs_PutFakeStat(&fakestate);
    AFS_DISCON_UNLOCK();
    afs_DestroyReq(treq);
    return code;
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
static int
HandleGetLock(struct vcache *avc, struct AFS_FLOCK *af,
	      struct vrequest *areq, int clid)
{
    afs_int32 code;
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
    struct afs_conn *tc;
    afs_int32 code;
    struct AFSFetchStatus OutStatus;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    struct rx_connection *rxconn;
    int temp;
    XSTATS_DECLS;
    temp = areq->flags & O_NONBLOCK;
    areq->flags |= O_NONBLOCK;

    /* If we're disconnected, lie and say that we've got no locks. Ick */
    if (AFS_IS_DISCONNECTED)
        return 0;
        
    do {
	tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_FETCHSTATUS);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_FetchStatus(rxconn, (struct AFSFid *)&avc->f.fid.Fid,
				  &OutStatus, &CallBack, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, rxconn, code, &avc->f.fid, areq, AFS_STATS_FS_RPCIDX_FETCHSTATUS,
	      SHARED_LOCK, NULL));

    if (temp)
	areq->flags &= ~O_NONBLOCK;

    if (code) {
	return (0);		/* failed, say it is 'unlocked' */
    } else {
	return ((int)OutStatus.lockCount);
    }
}

