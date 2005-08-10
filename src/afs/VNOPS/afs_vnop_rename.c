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
 * afsrename
 * afs_rename
 *
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/VNOPS/afs_vnop_rename.c,v 1.16.2.7 2005/06/10 21:11:49 jaltman Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"

extern afs_rwlock_t afs_xcbhash;

/* Note that we don't set CDirty here, this is OK because the rename
 * RPC is called synchronously. */

int
afsrename(struct vcache *aodp, char *aname1, struct vcache *andp,
	  char *aname2, struct AFS_UCRED *acred, struct vrequest *areq)
{
    register struct conn *tc;
    register afs_int32 code;
    afs_int32 returnCode;
    int oneDir, doLocally;
    afs_size_t offset, len;
    struct VenusFid unlinkFid, fileFid;
    struct vcache *tvc;
    struct dcache *tdc1, *tdc2;
    struct AFSFetchStatus OutOldDirStatus, OutNewDirStatus;
    struct AFSVolSync tsync;
    XSTATS_DECLS;
    AFS_STATCNT(afs_rename);
    afs_Trace4(afs_iclSetp, CM_TRACE_RENAME, ICL_TYPE_POINTER, aodp,
	       ICL_TYPE_STRING, aname1, ICL_TYPE_POINTER, andp,
	       ICL_TYPE_STRING, aname2);

    if (strlen(aname1) > AFSNAMEMAX || strlen(aname2) > AFSNAMEMAX) {
	code = ENAMETOOLONG;
	goto done;
    }

    /* verify the latest versions of the stat cache entries */
  tagain:
    code = afs_VerifyVCache(aodp, areq);
    if (code)
	goto done;
    code = afs_VerifyVCache(andp, areq);
    if (code)
	goto done;

    /* lock in appropriate order, after some checks */
    if (aodp->fid.Cell != andp->fid.Cell
	|| aodp->fid.Fid.Volume != andp->fid.Fid.Volume) {
	code = EXDEV;
	goto done;
    }
    oneDir = 0;
    code = 0;
    if (andp->fid.Fid.Vnode == aodp->fid.Fid.Vnode) {
	if (!strcmp(aname1, aname2)) {
	    /* Same directory and same name; this is a noop and just return success
	     * to save cycles and follow posix standards */

	    code = 0;
	    goto done;
	}
	ObtainWriteLock(&andp->lock, 147);
	tdc1 = afs_GetDCache(aodp, (afs_size_t) 0, areq, &offset, &len, 0);
	if (!tdc1) {
	    code = ENOENT;
	} else {
	    ObtainWriteLock(&tdc1->lock, 643);
	}
	tdc2 = tdc1;
	oneDir = 1;		/* only one dude locked */
    } else if ((andp->states & CRO) || (aodp->states & CRO)) {
	code = EROFS;
	goto done;
    } else if (andp->fid.Fid.Vnode < aodp->fid.Fid.Vnode) {
	ObtainWriteLock(&andp->lock, 148);	/* lock smaller one first */
	ObtainWriteLock(&aodp->lock, 149);
	tdc2 = afs_FindDCache(andp, (afs_size_t) 0);
	if (tdc2)
	    ObtainWriteLock(&tdc2->lock, 644);
	tdc1 = afs_GetDCache(aodp, (afs_size_t) 0, areq, &offset, &len, 0);
	if (tdc1)
	    ObtainWriteLock(&tdc1->lock, 645);
	else
	    code = ENOENT;
    } else {
	ObtainWriteLock(&aodp->lock, 150);	/* lock smaller one first */
	ObtainWriteLock(&andp->lock, 557);
	tdc1 = afs_GetDCache(aodp, (afs_size_t) 0, areq, &offset, &len, 0);
	if (tdc1)
	    ObtainWriteLock(&tdc1->lock, 646);
	else
	    code = ENOENT;
	tdc2 = afs_FindDCache(andp, (afs_size_t) 0);
	if (tdc2)
	    ObtainWriteLock(&tdc2->lock, 647);
    }

    osi_dnlc_remove(aodp, aname1, 0);
    osi_dnlc_remove(andp, aname2, 0);

    /*
     * Make sure that the data in the cache is current. We may have
     * received a callback while we were waiting for the write lock.
     */
    if (tdc1) {
	if (!(aodp->states & CStatd)
	    || !hsame(aodp->m.DataVersion, tdc1->f.versionNo)) {

	    ReleaseWriteLock(&aodp->lock);
	    if (!oneDir) {
		if (tdc2) {
		    ReleaseWriteLock(&tdc2->lock);
		    afs_PutDCache(tdc2);
		}
		ReleaseWriteLock(&andp->lock);
	    }
	    ReleaseWriteLock(&tdc1->lock);
	    afs_PutDCache(tdc1);
	    goto tagain;
	}
    }

    if (code == 0)
	code = afs_dir_Lookup(tdc1, aname1, &fileFid.Fid);
    if (code) {
	if (tdc1) {
	    ReleaseWriteLock(&tdc1->lock);
	    afs_PutDCache(tdc1);
	}
	ReleaseWriteLock(&aodp->lock);
	if (!oneDir) {
	    if (tdc2) {
		ReleaseWriteLock(&tdc2->lock);
		afs_PutDCache(tdc2);
	    }
	    ReleaseWriteLock(&andp->lock);
	}
	goto done;
    }

    /* locks are now set, proceed to do the real work */
    do {
	tc = afs_Conn(&aodp->fid, areq, SHARED_LOCK);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_RENAME);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_Rename(tc->id, (struct AFSFid *)&aodp->fid.Fid, aname1,
			     (struct AFSFid *)&andp->fid.Fid, aname2,
			     &OutOldDirStatus, &OutNewDirStatus, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;

    } while (afs_Analyze
	     (tc, code, &andp->fid, areq, AFS_STATS_FS_RPCIDX_RENAME,
	      SHARED_LOCK, NULL));

    returnCode = code;		/* remember for later */

    /* Now we try to do things locally.  This is really loathsome code. */
    unlinkFid.Fid.Vnode = 0;
    if (code == 0) {
	/*  In any event, we don't really care if the data (tdc2) is not
	 * in the cache; if it isn't, we won't do the update locally.  */
	/* see if version numbers increased properly */
	doLocally = 1;
	if (oneDir) {
	    /* number increases by 1 for whole rename operation */
	    if (!afs_LocalHero(aodp, tdc1, &OutOldDirStatus, 1)) {
		doLocally = 0;
	    }
	} else {
	    /* two separate dirs, each increasing by 1 */
	    if (!afs_LocalHero(aodp, tdc1, &OutOldDirStatus, 1))
		doLocally = 0;
	    if (!afs_LocalHero(andp, tdc2, &OutNewDirStatus, 1))
		doLocally = 0;
	    if (!doLocally) {
		if (tdc1) {
		    ZapDCE(tdc1);
		    DZap(tdc1);
		}
		if (tdc2) {
		    ZapDCE(tdc2);
		    DZap(tdc2);
		}
	    }
	}
	/* now really do the work */
	if (doLocally) {
	    /* first lookup the fid of the dude we're moving */
	    code = afs_dir_Lookup(tdc1, aname1, &fileFid.Fid);
	    if (code == 0) {
		/* delete the source */
		code = afs_dir_Delete(tdc1, aname1);
	    }
	    /* first see if target is there */
	    if (code == 0
		&& afs_dir_Lookup(tdc2, aname2,
				  &unlinkFid.Fid) == 0) {
		/* target already exists, and will be unlinked by server */
		code = afs_dir_Delete(tdc2, aname2);
	    }
	    if (code == 0) {
		code = afs_dir_Create(tdc2, aname2, &fileFid.Fid);
	    }
	    if (code != 0) {
		ZapDCE(tdc1);
		DZap(tdc1);
		if (!oneDir) {
		    ZapDCE(tdc2);
		    DZap(tdc2);
		}
	    }
	}

	/* update dir link counts */
	aodp->m.LinkCount = OutOldDirStatus.LinkCount;
	if (!oneDir)
	    andp->m.LinkCount = OutNewDirStatus.LinkCount;

    } else {			/* operation failed (code != 0) */
	if (code < 0) {
	    /* if failed, server might have done something anyway, and 
	     * assume that we know about it */
	    ObtainWriteLock(&afs_xcbhash, 498);
	    afs_DequeueCallback(aodp);
	    afs_DequeueCallback(andp);
	    andp->states &= ~CStatd;
	    aodp->states &= ~CStatd;
	    ReleaseWriteLock(&afs_xcbhash);
	    osi_dnlc_purgedp(andp);
	    osi_dnlc_purgedp(aodp);
	}
    }

    /* release locks */
    if (tdc1) {
	ReleaseWriteLock(&tdc1->lock);
	afs_PutDCache(tdc1);
    }

    if ((!oneDir) && tdc2) {
	ReleaseWriteLock(&tdc2->lock);
	afs_PutDCache(tdc2);
    }

    ReleaseWriteLock(&aodp->lock);
    if (!oneDir)
	ReleaseWriteLock(&andp->lock);

    if (returnCode) {
	code = returnCode;
	goto done;
    }

    /* now, some more details.  if unlinkFid.Fid.Vnode then we should decrement
     * the link count on this file.  Note that if fileFid is a dir, then we don't
     * have to invalidate its ".." entry, since its DataVersion # should have
     * changed. However, interface is not good enough to tell us the
     * *file*'s new DataVersion, so we're stuck.  Our hack: delete mark
     * the data as having an "unknown" version (effectively discarding the ".."
     * entry */
    if (unlinkFid.Fid.Vnode) {
	unlinkFid.Fid.Volume = aodp->fid.Fid.Volume;
	unlinkFid.Cell = aodp->fid.Cell;
	tvc = NULL;
	if (!unlinkFid.Fid.Unique) {
	    tvc = afs_LookupVCache(&unlinkFid, areq, NULL, aodp, aname1);
	}
	if (!tvc)		/* lookup failed or wasn't called */
	    tvc = afs_GetVCache(&unlinkFid, areq, NULL, NULL);

	if (tvc) {
#ifdef AFS_BOZONLOCK_ENV
	    afs_BozonLock(&tvc->pvnLock, tvc);	/* Since afs_TryToSmush will do a pvn_vptrunc */
#endif
	    ObtainWriteLock(&tvc->lock, 151);
	    tvc->m.LinkCount--;
	    tvc->states &= ~CUnique;	/* For the dfs xlator */
	    if (tvc->m.LinkCount == 0 && !osi_Active(tvc)) {
		/* if this was last guy (probably) discard from cache.
		 * We have to be careful to not get rid of the stat
		 * information, since otherwise operations will start
		 * failing even if the file was still open (or
		 * otherwise active), and the server no longer has the
		 * info.  If the file still has valid links, we'll get
		 * a break-callback msg from the server, so it doesn't
		 * matter that we don't discard the status info */
		if (!AFS_NFSXLATORREQ(acred))
		    afs_TryToSmush(tvc, acred, 0);
	    }
	    ReleaseWriteLock(&tvc->lock);
#ifdef AFS_BOZONLOCK_ENV
	    afs_BozonUnlock(&tvc->pvnLock, tvc);
#endif
	    afs_PutVCache(tvc);
	}
    }

    /* now handle ".." invalidation */
    if (!oneDir) {
	fileFid.Fid.Volume = aodp->fid.Fid.Volume;
	fileFid.Cell = aodp->fid.Cell;
	if (!fileFid.Fid.Unique)
	    tvc = afs_LookupVCache(&fileFid, areq, NULL, andp, aname2);
	else
	    tvc = afs_GetVCache(&fileFid, areq, NULL, (struct vcache *)0);
	if (tvc && (vType(tvc) == VDIR)) {
	    ObtainWriteLock(&tvc->lock, 152);
	    tdc1 = afs_FindDCache(tvc, (afs_size_t) 0);
	    if (tdc1) {
		ObtainWriteLock(&tdc1->lock, 648);
		ZapDCE(tdc1);	/* mark as unknown */
		DZap(tdc1);
		ReleaseWriteLock(&tdc1->lock);
		afs_PutDCache(tdc1);	/* put it back */
	    }
	    osi_dnlc_remove(tvc, "..", 0);
	    ReleaseWriteLock(&tvc->lock);
	    afs_PutVCache(tvc);
	} else if (tvc) {
	    /* True we shouldn't come here since tvc SHOULD be a dir, but we
	     * 'syntactically' need to unless  we change the 'if' above...
	     */
	    afs_PutVCache(tvc);
	}
    }
    code = returnCode;
  done:
    return code;
}

int
#if defined(AFS_SGI_ENV)
afs_rename(OSI_VC_DECL(aodp), char *aname1, struct vcache *andp, char *aname2, struct pathname *npnp, struct AFS_UCRED *acred)
#else
afs_rename(OSI_VC_DECL(aodp), char *aname1, struct vcache *andp, char *aname2, struct AFS_UCRED *acred)
#endif
{
    register afs_int32 code;
    struct afs_fakestat_state ofakestate;
    struct afs_fakestat_state nfakestate;
    struct vrequest treq;
    OSI_VC_CONVERT(aodp);

    code = afs_InitReq(&treq, acred);
    if (code)
	return code;
    afs_InitFakeStat(&ofakestate);
    afs_InitFakeStat(&nfakestate);
    code = afs_EvalFakeStat(&aodp, &ofakestate, &treq);
    if (code)
	goto done;
    code = afs_EvalFakeStat(&andp, &nfakestate, &treq);
    if (code)
	goto done;
    code = afsrename(aodp, aname1, andp, aname2, acred, &treq);
  done:
    afs_PutFakeStat(&ofakestate);
    afs_PutFakeStat(&nfakestate);
    code = afs_CheckCode(code, &treq, 25);
    return code;
}
