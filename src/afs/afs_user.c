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

RCSID
    ("$Header$");

#ifdef AFS_RXK5
/* BEWARE: this code uses "u".  Must include heimdal krb5.h (u field name)
 * before libuafs afs/sysincludes.h (libuafs makes u a function.)
 */
#ifdef USING_K5SSL
#include <k5ssl.h>
#else
#include <krb5.h>
#endif
#endif

#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */

#if !defined(UKERNEL)
#if !defined(AFS_LINUX20_ENV)
#include <net/if.h>
#endif
#include <netinet/in.h>

#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN60_ENV)
#include <netinet/in_var.h>
#endif /* ! AFS_HPUX110_ENV */
#endif /* !defined(UKERNEL) */

#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#if	defined(AFS_SUN56_ENV)
#include <inet/led.h>
#include <inet/common.h>
#if   defined(AFS_SUN58_ENV)
#include <netinet/ip6.h>
#endif
#include <inet/ip.h>
#endif
#ifdef AFS_RXK5
#include <rx/rxk5.h>
#include <afs/rxk5_tkt.h>
#endif


/* Exported variables */
afs_rwlock_t afs_xuser;
struct unixuser *afs_users[NUSERS];


#ifndef AFS_PAG_MANAGER
/* Forward declarations */
void afs_ResetAccessCache(afs_int32 uid, int alock);

/*
 * Called with afs_xuser, afs_xserver and afs_xconn locks held, to delete
 * appropriate conn structures for au
 */
static void
RemoveUserConns(register struct unixuser *au)
{
    register int i;
    register struct server *ts;
    register struct srvAddr *sa;
    register struct conn *tc, **lc;

    AFS_STATCNT(RemoveUserConns);
    for (i = 0; i < NSERVERS; i++) {
	for (ts = afs_servers[i]; ts; ts = ts->next) {
	    for (sa = ts->addr; sa; sa = sa->next_sa) {
		lc = &sa->conns;
		for (tc = *lc; tc; lc = &tc->next, tc = *lc) {
		    if (tc->user == au && tc->refCount == 0) {
			*lc = tc->next;
			AFS_GUNLOCK();
			rx_DestroyConnection(tc->id);
			AFS_GLOCK();
			afs_osi_Free(tc, sizeof(struct conn));
			break;	/* at most one instance per server */
		    }		/*Found unreferenced connection for user */
		}		/*For each connection on the server */
	    }
	}			/*For each server on chain */
    }				/*For each chain */

}				/*RemoveUserConns */
#endif /* !AFS_PAG_MANAGER */


/* Called from afs_Daemon to garbage collect unixusers no longer using system,
 * and their conns.  The aforce parameter tells the function to flush all
 * *unauthenticated* conns, no matter what their expiration time; it exists
 * because after we choose our final rx epoch, we want to stop using calls with
 * other epochs as soon as possible (old file servers act bizarrely when they
 * see epoch changes).
 */
void
afs_GCUserData(int aforce)
{
    register struct unixuser *tu, **lu, *nu;
    register int i;
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
		if (tu->states & UHasTokens) {
#ifdef AFS_RXK5
		  rxk5_creds *rxk5creds = (rxk5_creds*) tu->rxk5creds;
		  if( rxk5creds ? rxk5creds->k5creds->times.endtime < (now - NOTOKTIMEOUT):
		      tu->ct.EndTimestamp < (now - NOTOKTIMEOUT)) {
		    struct cell *tcell = afs_GetCell(tu->cell, READ_LOCK);
		    afs_warn
		      ("afs: Tokens for user of AFS id %d for cell %s expired now\n",
		       tu->vid, tcell->cellName);
		    afs_PutCell(tcell, READ_LOCK);
#else
		    /*
		     * Give ourselves a little extra slack, in case we
		     * reauthenticate
		     */
		    if (tu->ct.EndTimestamp < now - NOTOKTIMEOUT) {
#endif
			delFlag = 1;
		    }
		} else {
		    if (aforce || (tu->tokenTime < now - NOTOKTIMEOUT))
			delFlag = 1;
		}
	    }
	    nu = tu->next;
	    if (delFlag) {
#ifdef AFS_RXK5
	      if(tu->rxk5creds) {
		krb5_context k5context;
		k5context = rxk5_get_context(0);
		afs_warn("Expired rxk5 connection found for user %d, and GC'd\n",
			 tu->vid);
		rxk5_free_creds(k5context, (rxk5_creds*) tu->rxk5creds);
		tu->rxk5creds = NULL;
	      }
#endif
		*lu = tu->next;
#ifndef AFS_PAG_MANAGER
		RemoveUserConns(tu);
#endif
		if (tu->stp)
		    afs_osi_Free(tu->stp, tu->stLen);
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


#ifndef AFS_PAG_MANAGER
/*
 * Check for unixusers who encountered bad tokens, and reset the access
 * cache for these guys.  Can't do this when token expiration detected,
 * since too many locks are set then.
 */
void
afs_CheckTokenCache(void)
{
    register int i;
    register struct unixuser *tu;
    afs_int32 now;

    AFS_STATCNT(afs_CheckCacheResets);
    ObtainReadLock(&afs_xvcache);
    ObtainReadLock(&afs_xuser);
    now = osi_Time();
    for (i = 0; i < NUSERS; i++) {
	for (tu = afs_users[i]; tu; tu = tu->next) {
	    register afs_int32 uid;

	    /*
	     * If tokens are still good and user has Kerberos tickets,
	     * check expiration
	     */
	    if (!(tu->states & UTokensBad) && tu->vid != UNDEFVID) {
#ifdef AFS_RXK5
	      rxk5_creds *rxk5creds = (rxk5_creds*) tu->rxk5creds;
	      if( rxk5creds ? rxk5creds->k5creds->times.endtime < now :
		  tu->ct.EndTimestamp < now) {
#else
		if (tu->ct.EndTimestamp < now) {
#endif
		    /*
		     * This token has expired, warn users and reset access
		     * cache.
		     */
#ifdef AFS_RXK5
		  /* I really hate this message - MLK */
		  {
		    struct cell *tcell = afs_GetCell(tu->cell, READ_LOCK);
		    afs_warn
		      ("afs: Tokens for user of AFS id %d for cell %s expired now\n",
			 tu->vid, tcell->cellName);
		    afs_PutCell(tcell, READ_LOCK);
		  }
#endif
		    tu->states |= (UTokensBad | UNeedsReset);
		}
	    }
	    if (tu->states & UNeedsReset) {
		tu->states &= ~UNeedsReset;
		uid = tu->uid;
		afs_ResetAccessCache(uid, 0);
	    }
	}
    }
    ReleaseReadLock(&afs_xuser);
    ReleaseReadLock(&afs_xvcache);

}				/*afs_CheckTokenCache */


void
afs_ResetAccessCache(afs_int32 uid, int alock)
{
    register int i;
    register struct vcache *tvc;
    struct axscache *ac;

    AFS_STATCNT(afs_ResetAccessCache);
    if (alock)
	ObtainReadLock(&afs_xvcache);
    for (i = 0; i < VCSIZE; i++) {
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    /* really should do this under cache write lock, but that.
	     * is hard to under locking hierarchy */
	    if (tvc->Access && (ac = afs_FindAxs(tvc->Access, uid))) {
		afs_RemoveAxs(&tvc->Access, ac);
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
afs_ResetUserConns(register struct unixuser *auser)
{
    int i;
    struct srvAddr *sa;
    struct conn *tc;

    AFS_STATCNT(afs_ResetUserConns);
    ObtainReadLock(&afs_xsrvAddr);
    ObtainWriteLock(&afs_xconn, 98);

    for (i = 0; i < NSERVERS; i++) {
	for (sa = afs_srvAddrs[i]; sa; sa = sa->next_bkt) {
	    for (tc = sa->conns; tc; tc = tc->next) {
		if (tc->user == auser) {
		    tc->forceConnectFS = 1;
		}
	    }
	}
    }

    ReleaseWriteLock(&afs_xconn);
    ReleaseReadLock(&afs_xsrvAddr);
    afs_ResetAccessCache(auser->uid, 1);
    auser->states &= ~UNeedsReset;
}				/*afs_ResetUserConns */
#endif /* !AFS_PAG_MANAGER */


struct unixuser *
afs_FindUser(afs_int32 auid, afs_int32 acell, afs_int32 locktype)
{
    register struct unixuser *tu;
    register afs_int32 i;

    AFS_STATCNT(afs_FindUser);
    i = UHash(auid);
    ObtainWriteLock(&afs_xuser, 99);
    for (tu = afs_users[i]; tu; tu = tu->next) {
	if (tu->uid == auid && ((tu->cell == acell) || (acell == -1))) {
	    tu->refCount++;
	    ReleaseWriteLock(&afs_xuser);
	    return tu;
	}
    }
    ReleaseWriteLock(&afs_xuser);
    return NULL;

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
    register struct unixuser *currPAGP;	/*Ptr to curr PAG */
    register struct unixuser *cmpPAGP;	/*Ptr to PAG being compared */
    register struct afs_stats_AuthentInfo *authP;	/*Ptr to stats area */
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
	    if (currPAGP->vid == UNDEFVID)
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


struct unixuser *
afs_GetUser(register afs_int32 auid, afs_int32 acell, afs_int32 locktype)
{
    register struct unixuser *tu, *pu = 0;
    register afs_int32 i;
    register afs_int32 RmtUser = 0;

    AFS_STATCNT(afs_GetUser);
    i = UHash(auid);
    ObtainWriteLock(&afs_xuser, 104);
    for (tu = afs_users[i]; tu; tu = tu->next) {
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
		ReleaseWriteLock(&afs_xuser);
		return tu;
	    } else if (tu->cell == acell || acell == -1) {
		tu->refCount++;
		ReleaseWriteLock(&afs_xuser);
		return tu;
	    }
	}
    }
    tu = (struct unixuser *)afs_osi_Alloc(sizeof(struct unixuser));
#ifndef AFS_NOSTATS
    afs_stats_cmfullperf.authent.PAGCreations++;
#endif /* AFS_NOSTATS */
    memset((char *)tu, 0, sizeof(struct unixuser));
    tu->next = afs_users[i];
    afs_users[i] = tu;
    if (RmtUser) {
	/*
	 * This is for the case where an additional unixuser struct is
	 * created because the remote client is accessing a different cell;
	 * we simply rerecord relevant information from the original
	 * structure
	 */
	if (pu && pu->exporter) {
	    (void)EXP_HOLD(tu->exporter = pu->exporter);
	}
    }
    tu->uid = auid;
    tu->cell = acell;
    tu->vid = UNDEFVID;
    tu->refCount = 1;
    tu->tokenTime = osi_Time();
    ReleaseWriteLock(&afs_xuser);
    return tu;

}				/*afs_GetUser */


void
afs_PutUser(register struct unixuser *au, afs_int32 locktype)
{
    AFS_STATCNT(afs_PutUser);
    --au->refCount;
}				/*afs_PutUser */


/*
 * Set the primary flag on a unixuser structure, ensuring that exactly one
 * dude has the flag set at any time for a particular unix uid.
 */
void
afs_SetPrimary(register struct unixuser *au, register int aflag)
{
    register struct unixuser *tu;
    register int i;
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
void
afs_GCPAGs_perproc_func(AFS_PROC * pproc)
{
    afs_int32 pag, hash, uid;
    const struct AFS_UCRED *pcred;

    afs_GCPAGs_perproc_count++;

    pcred = afs_osi_proc2cred(pproc);
    if (!pcred)
	return;

    afs_GCPAGs_cred_count++;

    pag = PagInCred(pcred);
#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD40_ENV) || defined(AFS_LINUX22_ENV)
    uid = (pag != NOPAG ? pag : pcred->cr_uid);
#elif defined(AFS_SUN510_ENV)
    uid = (pag != NOPAG ? pag : crgetruid(pcred));
#else
    uid = (pag != NOPAG ? pag : pcred->cr_ruid);
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
		    /* set the expire times to 0, causes 
		     * afs_GCUserData to remove this entry 
		     */
		    pu->ct.EndTimestamp = 0;
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
