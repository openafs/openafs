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
 */
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */

#if !defined(UKERNEL)
#if !defined(AFS_LINUX_ENV)
#include <net/if.h>
#endif
#include <netinet/in.h>

#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX_ENV) && !defined(AFS_DARWIN_ENV)
#include <netinet/in_var.h>
#endif /* ! AFS_HPUX110_ENV */
#endif /* !defined(UKERNEL) */

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#if	defined(AFS_SUN5_ENV)
#include <inet/led.h>
#include <inet/common.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#endif

#include "afs/afs_axscache.h"

/* Exported variables */
afs_rwlock_t afs_xuser;
struct unixuser *afs_users[NUSERS];


#ifndef AFS_PAG_MANAGER
/* Forward declarations */
void afs_ResetAccessCache(afs_int32 uid, afs_int32 cell, int alock);
#endif /* !AFS_PAG_MANAGER */


/* Called from afs_Daemon to garbage collect unixusers no longer using system,
 * and their conns.
 */
void
afs_GCUserData(void)
{
    struct unixuser *tu, **lu, *nu;
    int i;
    afs_int32 now, delFlag;

    AFS_STATCNT(afs_GCUserData);
    /* Obtain locks in valid order */
    ObtainWriteLock(&afs_xuser, 95);
#ifndef AFS_PAG_MANAGER
    ObtainReadLock(&afs_xserver);
    ObtainWriteLock(&afs_xconn, 96);
#endif
    now = osi_Time();
    for (i = 0; i < NUSERS; i++) {
	for (lu = &afs_users[i], tu = *lu; tu; tu = nu) {
	    delFlag = 0;	/* should we delete this dude? */
	    /* Don't garbage collect users in use now (refCount) */
	    if (tu->refCount == 0) {
		if (tu->tokens) {
		    /* Need to walk the token stack, and dispose of
		     * all expired tokens */
		    afs_DiscardExpiredTokens(&tu->tokens, now);
		    if (!afs_HasUsableTokens(tu->tokens, now))
			delFlag = 1;
		} else {
		    if (tu->tokenTime < now - NOTOKTIMEOUT)
			delFlag = 1;
		}
	    }
	    nu = tu->next;
	    if (delFlag) {
		*lu = tu->next;
#ifndef AFS_PAG_MANAGER
                afs_ReleaseConnsUser(tu);
#endif
		afs_FreeTokens(&tu->tokens);

		if (tu->exporter)
		    EXP_RELE(tu->exporter);
		afs_osi_Free(tu, sizeof(struct unixuser));
	    } else {
		lu = &tu->next;
	    }
	}
    }
#ifndef AFS_PAG_MANAGER
    ReleaseWriteLock(&afs_xconn);
#endif
#ifndef AFS_PAG_MANAGER
    ReleaseReadLock(&afs_xserver);
#endif
    ReleaseWriteLock(&afs_xuser);

}				/*afs_GCUserData */

static struct unixuser *
afs_FindUserNoLock(afs_int32 auid, afs_int32 acell)
{
    struct unixuser *tu;
    afs_int32 i;

    AFS_STATCNT(afs_FindUser);
    i = UHash(auid);
    for (tu = afs_users[i]; tu; tu = tu->next) {
	if (tu->uid == auid && ((tu->cell == acell) || (acell == -1))) {
	    tu->refCount++;
	    return tu;
	}
    }
    return NULL;

}

#ifndef AFS_PAG_MANAGER
/*
 * Check for unixusers who encountered bad tokens, and reset the access
 * cache for these guys.  Can't do this when token expiration detected,
 * since too many locks are set then.
 */
void
afs_CheckTokenCache(void)
{
    int i;
    struct unixuser *tu;
    afs_int32 now;
    struct vcache *tvc;
    struct axscache *tofreelist;
    int do_scan = 0;

    AFS_STATCNT(afs_CheckCacheResets);
    ObtainReadLock(&afs_xvcache);
    ObtainReadLock(&afs_xuser);
    now = osi_Time();
    for (i = 0; i < NUSERS; i++) {
	for (tu = afs_users[i]; tu; tu = tu->next) {
	    /*
	     * If tokens are still good and user has Kerberos tickets,
	     * check expiration
	     */
	    if ((tu->states & UHasTokens) && !(tu->states & UTokensBad)) {
		if (!afs_HasUsableTokens(tu->tokens, now)) {
		    /*
		     * This token has expired, warn users and reset access
		     * cache.
		     */
		    tu->states |= (UTokensBad | UNeedsReset);
		}
	    }
	    if (tu->states & UNeedsReset)
		do_scan = 1;
	}
    }
    /* Skip the potentially expensive scan if nothing to do */
    if (!do_scan)
	goto done;

    tofreelist = NULL;
    for (i = 0; i < VCSIZE; i++) {
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    /* really should do this under cache write lock, but that.
	     * is hard to under locking hierarchy */
	    if (tvc->Access) {
		struct axscache **ac, **nac;

		for ( ac = &tvc->Access; *ac;)  {
		    nac = &(*ac)->next;
		    tu = afs_FindUserNoLock((*ac)->uid, tvc->f.fid.Cell);
		    if (tu == NULL || (tu->states & UNeedsReset)) {
			struct axscache *tmp;
			tmp = *ac;
			*ac = *nac;
			tmp->next = tofreelist;
			tofreelist = tmp;
		    } else
			ac = nac;
		    if (tu != NULL)
			tu->refCount--;
		}
	    }
	}
    }
    afs_FreeAllAxs(&tofreelist);
    for (i = 0; i < NUSERS; i++) {
	for (tu = afs_users[i]; tu; tu = tu->next) {
	    if (tu->states & UNeedsReset)
		tu->states &= ~UNeedsReset;
	}
    }

done:
    ReleaseReadLock(&afs_xuser);
    ReleaseReadLock(&afs_xvcache);
}				/*afs_CheckTokenCache */


/* Remove any access caches associated with this uid+cell
 * by scanning the entire vcache table.  Specify cell=-1
 * to remove all access caches associated with this uid
 * regardless of cell.
 */
void
afs_ResetAccessCache(afs_int32 uid, afs_int32 cell, int alock)
{
    int i;
    struct vcache *tvc;
    struct axscache *ac;

    AFS_STATCNT(afs_ResetAccessCache);
    if (alock)
	ObtainReadLock(&afs_xvcache);
    for (i = 0; i < VCSIZE; i++) {
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    /* really should do this under cache write lock, but that.
	     * is hard to under locking hierarchy */
	    if (tvc->Access && (cell == -1 || tvc->f.fid.Cell == cell)) {
		ac = afs_FindAxs(tvc->Access, uid);
		if (ac) {
		    afs_RemoveAxs(&tvc->Access, ac);
		}
	    }
	}
    }
    if (alock)
	ReleaseReadLock(&afs_xvcache);

}				/*afs_ResetAccessCache */


/*
 * Ensure all connections make use of new tokens.  Discard incorrectly-cached
 * access info.
 */
void
afs_ResetUserConns(struct unixuser *auser)
{
    int i, j;
    struct srvAddr *sa;
    struct sa_conn_vector *tcv;

    AFS_STATCNT(afs_ResetUserConns);
    ObtainReadLock(&afs_xsrvAddr);
    ObtainWriteLock(&afs_xconn, 98);

    for (i = 0; i < NSERVERS; i++) {
	for (sa = afs_srvAddrs[i]; sa; sa = sa->next_bkt) {
	    for (tcv = sa->conns; tcv; tcv = tcv->next) {
		if (tcv->user == auser) {
		    for(j = 0; j < CVEC_LEN; ++j) {
		    	(tcv->cvec[j]).forceConnectFS = 1;
		    }
		}
	    }
	}
    }

    ReleaseWriteLock(&afs_xconn);
    ReleaseReadLock(&afs_xsrvAddr);
    afs_ResetAccessCache(auser->uid, auser->cell, 1);
    auser->states &= ~UNeedsReset;
}				/*afs_ResetUserConns */
#endif /* !AFS_PAG_MANAGER */


struct unixuser *
afs_FindUser(afs_int32 auid, afs_int32 acell, afs_int32 locktype)
{
    struct unixuser *tu;

    ObtainWriteLock(&afs_xuser, 99);
    tu = afs_FindUserNoLock(auid, acell);
    ReleaseWriteLock(&afs_xuser);
    if (tu)
	afs_LockUser(tu, locktype, 365);
    return tu;
}				/*afs_FindUser */


/*------------------------------------------------------------------------
 * EXPORTED afs_ComputePAGStats
 *
 * Description:
 *	Compute a set of stats concerning PAGs used by this machine.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	The results are put in the structure responsible for keeping
 *	detailed CM stats.  Note: entries corresponding to a single PAG
 *	will appear on the identical hash chain, so sweeping the chain
 *	will find all entries related to a single PAG.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
afs_ComputePAGStats(void)
{
    struct unixuser *currPAGP;	/*Ptr to curr PAG */
    struct unixuser *cmpPAGP;	/*Ptr to PAG being compared */
    struct afs_stats_AuthentInfo *authP;	/*Ptr to stats area */
    int curr_Record;		/*Curr record */
    int currChain;		/*Curr hash chain */
    int currChainLen;		/*Length of curr hash chain */
    int currPAGRecords;		/*# records in curr PAG */

    /*
     * Lock out everyone else from scribbling on the PAG entries.
     */
    ObtainReadLock(&afs_xuser);

    /*
     * Initialize the tallies, then sweep through each hash chain.  We
     * can't bzero the structure, since some fields are cumulative over
     * the CM's lifetime.
     */
    curr_Record = 0;
    authP = &(afs_stats_cmfullperf.authent);
    authP->curr_PAGs = 0;
    authP->curr_Records = 0;
    authP->curr_AuthRecords = 0;
    authP->curr_UnauthRecords = 0;
    authP->curr_MaxRecordsInPAG = 0;
    authP->curr_LongestChain = 0;

    for (currChain = 0; currChain < NUSERS; currChain++) {
	currChainLen = 0;
	for (currPAGP = afs_users[currChain]; currPAGP;
	     currPAGP = currPAGP->next) {
	    /*
	     * Bump the number of records on this current chain, along with
	     * the total number of records in existence.
	     */
	    currChainLen++;
	    curr_Record++;
	    /*
	     * We've found a previously-uncounted PAG.  If it's been deleted
	     * but just not garbage-collected yet, we step over it.
	     */
	    if (!(currPAGP->states & UHasTokens))
		continue;

	    /*
	     * If this PAG record has already been ``counted', namely
	     * associated with a PAG and tallied, clear its bit and move on.
	     */
	    (authP->curr_Records)++;
	    if (currPAGP->states & UPAGCounted) {
		currPAGP->states &= ~UPAGCounted;
		continue;
	    }



	    /*We've counted this one already */
	    /*
	     * Jot initial info down, then sweep down the rest of the hash
	     * chain, looking for matching PAG entries.  Note: to properly
	     * ``count'' the current record, we first compare it to itself
	     * in the following loop.
	     */
	    (authP->curr_PAGs)++;
	    currPAGRecords = 0;

	    for (cmpPAGP = currPAGP; cmpPAGP; cmpPAGP = cmpPAGP->next) {
		if (currPAGP->uid == cmpPAGP->uid) {
		    /*
		     * The records belong to the same PAG.  First, mark the
		     * new record as ``counted'' and bump the PAG size.
		     * Then, record the state of its ticket, if any.
		     */
		    cmpPAGP->states |= UPAGCounted;
		    currPAGRecords++;
		    if ((cmpPAGP->states & UHasTokens)
			&& !(cmpPAGP->states & UTokensBad))
			(authP->curr_AuthRecords)++;
		    else
			(authP->curr_UnauthRecords)++;
		}		/*Records belong to same PAG */
	    }			/*Compare to rest of PAG records in chain */

	    /*
	     * In the above comparisons, the current PAG record has been
	     * marked as counted.  Erase this mark before moving on.
	     */
	    currPAGP->states &= ~UPAGCounted;

	    /*
	     * We've compared our current PAG record with all remaining
	     * PAG records in the hash chain.  Update our tallies, and
	     * perhaps even our lifetime high water marks.  After that,
	     * remove our search mark and advance to the next comparison
	     * pair.
	     */
	    if (currPAGRecords > authP->curr_MaxRecordsInPAG) {
		authP->curr_MaxRecordsInPAG = currPAGRecords;
		if (currPAGRecords > authP->HWM_MaxRecordsInPAG)
		    authP->HWM_MaxRecordsInPAG = currPAGRecords;
	    }
	}			/*Sweep a hash chain */

	/*
	 * If the chain we just finished zipping through is the longest we've
	 * seen yet, remember this fact before advancing to the next chain.
	 */
	if (currChainLen > authP->curr_LongestChain) {
	    authP->curr_LongestChain = currChainLen;
	    if (currChainLen > authP->HWM_LongestChain)
		authP->HWM_LongestChain = currChainLen;
	}

    }				/*For each hash chain in afs_user */

    /*
     * Now that we've counted everything up, we can consider all-time
     * numbers.
     */
    if (authP->curr_PAGs > authP->HWM_PAGs)
	authP->HWM_PAGs = authP->curr_PAGs;
    if (authP->curr_Records > authP->HWM_Records)
	authP->HWM_Records = authP->curr_Records;

    /*
     * People are free to manipulate the PAG structures now.
     */
    ReleaseReadLock(&afs_xuser);

}				/*afs_ComputePAGStats */

/*!
 * Obtain a unixuser for the specified uid and cell;
 * if no existing match found, allocate a new one.
 *
 * \param[in] auid	uid/PAG value
 * \param[in] acell	cell number; if -1, match on auid only
 * \param[in] locktype  locktype desired on returned unixuser
 *
 * \post unixuser is chained in afs_users[], returned with <locktype> held
 *
 * \note   Maintain unixusers in sorted order within hash bucket to enable
 *         small lookup optimizations.
 */

struct unixuser *
afs_GetUser(afs_int32 auid, afs_int32 acell, afs_int32 locktype)
{
    struct unixuser *tu, *xu = 0, *pu = 0;
    afs_int32 i;
    afs_int32 RmtUser = 0;

    AFS_STATCNT(afs_GetUser);
    i = UHash(auid);
    ObtainWriteLock(&afs_xuser, 104);
    /* unixusers are sorted by uid in each hash bucket */
    for (tu = afs_users[i]; tu && (tu->uid <= auid) ; xu = tu, tu = tu->next) {
	if (tu->uid == auid) {
	    RmtUser = 0;
	    pu = NULL;
	    if (tu->exporter) {
		RmtUser = 1;
		pu = tu;
	    }
	    if (tu->cell == -1 && acell != -1) {
		/* Here we setup the real cell for the client */
		tu->cell = acell;
		tu->refCount++;
		goto done;
	    } else if (tu->cell == acell || acell == -1) {
		tu->refCount++;
		goto done;
	    }
	}
    }
    /* no matching unixuser found; repurpose the tu pointer to
     * allocate a new unixuser.
     * xu will be insertion point for our new unixuser.
     */
    tu = afs_osi_Alloc(sizeof(struct unixuser));
    osi_Assert(tu != NULL);
#ifndef AFS_NOSTATS
    afs_stats_cmfullperf.authent.PAGCreations++;
#endif /* AFS_NOSTATS */
    memset(tu, 0, sizeof(struct unixuser));
    AFS_RWLOCK_INIT(&tu->lock, "unixuser lock");
    /* insert new nu in sorted order after xu */
    if (xu == NULL) {
	tu->next = afs_users[i];
	afs_users[i] = tu;
    } else {
	tu->next = xu->next;
	xu->next = tu;
    }
    if (RmtUser) {
	/*
	 * This is for the case where an additional unixuser struct is
	 * created because the remote client is accessing a different cell;
	 * we simply rerecord relevant information from the original
	 * structure
	 */
	if (pu && pu->exporter) {
	    tu->exporter = pu->exporter;
	    (void)EXP_HOLD(tu->exporter);
	}
    }
    tu->uid = auid;
    tu->cell = acell;
    tu->viceId = UNDEFVID;
    tu->refCount = 1;
    tu->tokenTime = osi_Time();
    /* fall through to return the new one */

 done:
    ReleaseWriteLock(&afs_xuser);
    afs_LockUser(tu, locktype, 364);
    return tu;

}				/*afs_GetUser */

void
afs_LockUser(struct unixuser *au, afs_int32 locktype,
             unsigned int src_indicator)
{
    switch (locktype) {
    case READ_LOCK:
	ObtainReadLock(&au->lock);
	break;
    case WRITE_LOCK:
	ObtainWriteLock(&au->lock, src_indicator);
	break;
    case SHARED_LOCK:
	ObtainSharedLock(&au->lock, src_indicator);
	break;
    default:
	/* noop */
	break;
    }
}

void
afs_PutUser(struct unixuser *au, afs_int32 locktype)
{
    AFS_STATCNT(afs_PutUser);

    switch (locktype) {
    case READ_LOCK:
	ReleaseReadLock(&au->lock);
	break;
    case WRITE_LOCK:
	ReleaseWriteLock(&au->lock);
	break;
    case SHARED_LOCK:
	ReleaseSharedLock(&au->lock);
	break;
    default:
	/* noop */
	break;
    }

    --au->refCount;
}				/*afs_PutUser */


/*
 * Set the primary flag on a unixuser structure, ensuring that exactly one
 * dude has the flag set at any time for a particular unix uid.
 */
void
afs_SetPrimary(struct unixuser *au, int aflag)
{
    struct unixuser *tu;
    int i;
    struct unixuser *pu;

    AFS_STATCNT(afs_SetPrimary);
    i = UHash(au->uid);
    pu = NULL;
    ObtainWriteLock(&afs_xuser, 105);
    /*
     * See if anyone is this uid's primary cell yet; recording in pu the
     * corresponding user
     */
    for (tu = afs_users[i]; tu; tu = tu->next) {
	if (tu->uid == au->uid && (tu->states & UPrimary)) {
	    pu = tu;
	}
    }
    if (pu && !(pu->states & UHasTokens)) {
	/*
	 * Primary user has unlogged, don't treat him as primary any longer;
	 * note that we want to treat him as primary until now, so that
	 * people see a primary identity until now.
	 */
	pu->states &= ~UPrimary;
	pu = NULL;
    }
    if (aflag == 1) {
	/* setting au to be primary */
	if (pu)
	    pu->states &= ~UPrimary;
	au->states |= UPrimary;
    } else if (aflag == 0) {
	/* we don't know if we're supposed to be primary or not */
	if (!pu || au == pu) {
	    au->states |= UPrimary;
	} else
	    au->states &= ~UPrimary;
    }
    ReleaseWriteLock(&afs_xuser);

}				/*afs_SetPrimary */

void
afs_NotifyUser(struct unixuser *auser, int event)
{
#ifdef AFS_DARWIN_ENV
    darwin_notify_perms(auser, event);
#endif
}

/**
 * Mark all of the unixuser records held for a particular PAG as
 * expired
 *
 * @param[in] pag
 * 	PAG to expire records for
 */
void
afs_MarkUserExpired(afs_int32 pag)
{
    afs_int32 i;
    struct unixuser *tu;

    i = UHash(pag);
    ObtainWriteLock(&afs_xuser, 9);
    for (tu = afs_users[i]; tu; tu = tu->next) {
	if (tu->uid == pag) {
	    tu->states &= ~UHasTokens;
	    tu->tokenTime = 0;
	}
    }
    ReleaseWriteLock(&afs_xuser);
}


#if AFS_GCPAGS

/*
 * Called by osi_TraverseProcTable (from afs_GCPAGs) for each
 * process in the system.
 * If the specified process uses a PAG, clear that PAG's temporary
 * 'deleteme' flag.
 */

/*
 * This variable keeps track of the number of UID-base
 * tokens in the afs_users table. When it's zero
 * the per process loop in GCPAGs doesn't have to
 * check processes without pags against the afs_users table.
 */
static afs_int32 afs_GCPAGs_UIDBaseTokenCount = 0;

/*
 * These variables keep track of the number of times
 * afs_GCPAGs_perproc_func() is called.  If it is not called at all when
 * walking the process table, there is something wrong and we should not
 * prematurely expire any tokens.
 */
static size_t afs_GCPAGs_perproc_count = 0;
static size_t afs_GCPAGs_cred_count = 0;

/*
 * LOCKS: afs_GCPAGs_perproc_func requires write lock on afs_xuser
 */
#if !defined(LINUX_KEYRING_SUPPORT) && (!defined(STRUCT_TASK_STRUCT_HAS_CRED) || defined(HAVE_LINUX_RCU_READ_LOCK))
void
afs_GCPAGs_perproc_func(afs_proc_t * pproc)
{
    afs_int32 pag, hash, uid;
    const afs_ucred_t *pcred;

    afs_GCPAGs_perproc_count++;

    pcred = afs_osi_proc2cred(pproc);
    if (!pcred)
	return;

    afs_GCPAGs_cred_count++;

    pag = PagInCred(pcred);
#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV) || defined(AFS_LINUX_ENV)
    uid = (pag != NOPAG ? pag : afs_cr_uid(pcred));
#elif defined(AFS_SUN510_ENV)
    uid = (pag != NOPAG ? pag : crgetruid(pcred));
#else
    uid = (pag != NOPAG ? pag : afs_cr_ruid(pcred));
#endif
    hash = UHash(uid);

    /* if this token is PAG based, or it's UID based and
     * UID-based tokens exist */
    if ((pag != NOPAG) || (afs_GCPAGs_UIDBaseTokenCount)) {
	/* find the entries for this uid in all cells and clear the not
	 * referenced flag.  Can't use afs_FindUser, because it just returns
	 * the specific cell asked for, or the first one found.
	 */
	struct unixuser *pu;
	for (pu = afs_users[hash]; pu; pu = pu->next) {
	    if (pu->uid == uid) {
		if (pu->states & TMP_UPAGNotReferenced) {
		    /* clear the 'deleteme' flag for this entry */
		    pu->states &= ~TMP_UPAGNotReferenced;
		    if (pag == NOPAG) {
			/* This is a uid based token that hadn't
			 * previously been cleared, so decrement the
			 * outstanding uid based token count */
			afs_GCPAGs_UIDBaseTokenCount--;
		    }
		}
	    }
	}
    }
}
#endif

/*
 * Go through the process table, find all unused PAGs
 * and cause them to be deleted during the next GC.
 *
 * returns the number of PAGs marked for deletion
 *
 * On AIX we free PAGs when the last accessing process exits,
 * so this routine is not needed.
 *
 * In AFS WebSecure, we explicitly call unlog when we remove
 * an entry in the login cache, so this routine is not needed.
 */

afs_int32
afs_GCPAGs(afs_int32 * ReleasedCount)
{
    struct unixuser *pu;
    int i;

    if (afs_gcpags != AFS_GCPAGS_OK) {
	return 0;
    }

    *ReleasedCount = 0;

    /* first, loop through afs_users, setting the temporary 'deleteme' flag */
    ObtainWriteLock(&afs_xuser, 419);
    afs_GCPAGs_UIDBaseTokenCount = 0;
    for (i = 0; i < NUSERS; i++) {
	for (pu = afs_users[i]; pu; pu = pu->next) {
	    pu->states |= TMP_UPAGNotReferenced;
	    if (((pu->uid >> 24) & 0xff) != 'A') {
		/* this is a uid-based token, */
		/* increment the count */
		afs_GCPAGs_UIDBaseTokenCount++;
	    }
	}
    }

    /* Now, iterate through the systems process table,
     * for each process, mark it's PAGs (if any) in use.
     * i.e. clear the temporary deleteme flag.
     */
    afs_GCPAGs_perproc_count = 0;
    afs_GCPAGs_cred_count = 0;

    afs_osi_TraverseProcTable();

    /* If there is an internal problem and afs_GCPAGs_perproc_func()
     * does not get called, disable gcpags so that we do not
     * accidentally expire all the tokens in the system.
     */
    if (afs_gcpags == AFS_GCPAGS_OK && !afs_GCPAGs_perproc_count) {
	afs_gcpags = AFS_GCPAGS_EPROCWALK;
    }

    if (afs_gcpags == AFS_GCPAGS_OK && !afs_GCPAGs_cred_count) {
	afs_gcpags = AFS_GCPAGS_ECREDWALK;
    }

    /* Now, go through afs_users again, any that aren't in use
     * (temp deleteme flag still set) will be marked for later deletion,
     * by setting their expire times to 0.
     */
    for (i = 0; i < NUSERS; i++) {
	for (pu = afs_users[i]; pu; pu = pu->next) {
	    if (pu->states & TMP_UPAGNotReferenced) {

		/* clear the temp flag */
		pu->states &= ~TMP_UPAGNotReferenced;

		/* Is this entry on behalf of a 'remote' user ?
		 * i.e. nfs translator, etc.
		 */
		if (!pu->exporter && afs_gcpags == AFS_GCPAGS_OK) {
		    /* make afs_GCUserData remove this entry  */
		    pu->states &= ~UHasTokens;
		    pu->tokenTime = 0;

		    (*ReleasedCount)++;	/* remember how many we marked (info only) */
		}
	    }
	}
    }

    ReleaseWriteLock(&afs_xuser);

    return 0;
}

#endif /* AFS_GCPAGS */
