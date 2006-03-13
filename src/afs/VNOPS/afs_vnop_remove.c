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
 * afs_Wire (DUX)
 * FetchWholeEnchilada
 * afs_IsWired (DUX)
 * afsremove
 * afs_remove
 * afs_newname
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


extern afs_rwlock_t afs_xvcache;
extern afs_rwlock_t afs_xcbhash;


#ifdef	AFS_OSF_ENV
/*
 *  Wire down file in cache: prefetch all data, and turn on CWired flag
 *  so that callbacks/callback expirations are (temporarily) ignored
 *  and cache file(s) are kept in cache. File will be unwired when
 *  afs_inactive is called (ie no one has VN_HOLD on vnode), or when
 *  afs_IsWired notices that the file is no longer Active.
 */
afs_Wire(avc, areq)
#else /* AFS_OSF_ENV */
static void
FetchWholeEnchilada(avc, areq)
#endif
     struct vrequest *areq;
     register struct vcache *avc;
{
    register afs_int32 nextChunk;
    register struct dcache *tdc;
    afs_size_t pos, offset, len;

    AFS_STATCNT(FetchWholeEnchilada);
    if ((avc->states & CStatd) == 0)
	return;			/* don't know size */
    for (nextChunk = 0; nextChunk < 1024; nextChunk++) {	/* sanity check on N chunks */
	pos = AFS_CHUNKTOBASE(nextChunk);
#if	defined(AFS_OSF_ENV)
	if (pos >= avc->m.Length)
	    break;		/* all done */
#else /* AFS_OSF_ENV */
	if (pos >= avc->m.Length)
	    return;		/* all done */
#endif
	tdc = afs_GetDCache(avc, pos, areq, &offset, &len, 0);
	if (!tdc)
#if	defined(AFS_OSF_ENV)
	    break;
#else /* AFS_OSF_ENV */
	    return;
#endif
	afs_PutDCache(tdc);
    }
#if defined(AFS_OSF_ENV)
    avc->states |= CWired;
#endif /* AFS_OSF_ENV */
}

#if	defined(AFS_OSF_ENV)
/*
 *  Tests whether file is wired down, after unwiring the file if it
 *  is found to be inactive (ie not open and not being paged from).
 */
afs_IsWired(avc)
     register struct vcache *avc;
{
    if (avc->states & CWired) {
	if (osi_Active(avc)) {
	    return 1;
	}
	avc->states &= ~CWired;
    }
    return 0;
}
#endif /* AFS_OSF_ENV */

int
afsremove(register struct vcache *adp, register struct dcache *tdc,
	  register struct vcache *tvc, char *aname, struct AFS_UCRED *acred,
	  struct vrequest *treqp)
{
    register afs_int32 code;
    register struct conn *tc;
    struct AFSFetchStatus OutDirStatus;
    struct AFSVolSync tsync;
    XSTATS_DECLS;
    do {
	tc = afs_Conn(&adp->fid, treqp, SHARED_LOCK);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEFILE);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_RemoveFile(tc->id, (struct AFSFid *)&adp->fid.Fid,
				 aname, &OutDirStatus, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, code, &adp->fid, treqp, AFS_STATS_FS_RPCIDX_REMOVEFILE,
	      SHARED_LOCK, NULL));

    osi_dnlc_remove(adp, aname, tvc);

    if (code) {
	if (tdc) {
	    ReleaseSharedLock(&tdc->lock);
	    afs_PutDCache(tdc);
	}
	if (tvc)
	    afs_PutVCache(tvc);

	if (code < 0) {
	    ObtainWriteLock(&afs_xcbhash, 497);
	    afs_DequeueCallback(adp);
	    adp->states &= ~CStatd;
	    ReleaseWriteLock(&afs_xcbhash);
	    osi_dnlc_purgedp(adp);
	}
	ReleaseWriteLock(&adp->lock);
	code = afs_CheckCode(code, treqp, 21);
	return code;
    }
    if (tdc)
	UpgradeSToWLock(&tdc->lock, 637);
    if (afs_LocalHero(adp, tdc, &OutDirStatus, 1)) {
	/* we can do it locally */
	code = afs_dir_Delete(tdc, aname);
	if (code) {
	    ZapDCE(tdc);	/* surprise error -- invalid value */
	    DZap(tdc);
	}
    }
    if (tdc) {
	ReleaseWriteLock(&tdc->lock);
	afs_PutDCache(tdc);	/* drop ref count */
    }
    ReleaseWriteLock(&adp->lock);

    /* now, get vnode for unlinked dude, and see if we should force it
     * from cache.  adp is now the deleted files vnode.  Note that we
     * call FindVCache instead of GetVCache since if the file's really
     * gone, we won't be able to fetch the status info anyway.  */
    if (tvc) {
#ifdef AFS_BOZONLOCK_ENV
	afs_BozonLock(&tvc->pvnLock, tvc);
	/* Since afs_TryToSmush will do a pvn_vptrunc */
#endif
	ObtainWriteLock(&tvc->lock, 141);
	/* note that callback will be broken on the deleted file if there are
	 * still >0 links left to it, so we'll get the stat right */
	tvc->m.LinkCount--;
	tvc->states &= ~CUnique;	/* For the dfs xlator */
	if (tvc->m.LinkCount == 0 && !osi_Active(tvc)) {
	    if (!AFS_NFSXLATORREQ(acred))
		afs_TryToSmush(tvc, acred, 0);
	}
	ReleaseWriteLock(&tvc->lock);
#ifdef AFS_BOZONLOCK_ENV
	afs_BozonUnlock(&tvc->pvnLock, tvc);
#endif
	afs_PutVCache(tvc);
    }
    return (0);
}

char *
afs_newname(void)
{
    char *name, *sp, *p = ".__afs";
    afs_int32 rd = afs_random() & 0xffff;

    sp = name = osi_AllocSmallSpace(AFS_SMALLOCSIZ);
    while (*p != '\0')
	*sp++ = *p++;
    while (rd) {
	*sp++ = "0123456789ABCDEF"[rd & 0x0f];
	rd >>= 4;
    }
    *sp = '\0';
    return (name);
}

/* these variables appear to exist for debugging purposes */
struct vcache *Tadp1, *Ttvc;
int Tadpr, Ttvcr;
char *Tnam;
char *Tnam1;

/* Note that we don't set CDirty here, this is OK because the unlink
 * RPC is called synchronously */
int
afs_remove(OSI_VC_ARG(adp), aname, acred)
     OSI_VC_DECL(adp);
     char *aname;
     struct AFS_UCRED *acred;
{
    struct vrequest treq;
    register struct dcache *tdc;
    struct VenusFid unlinkFid;
    register afs_int32 code;
    register struct vcache *tvc;
    afs_size_t offset, len;
    struct afs_fakestat_state fakestate;
    OSI_VC_CONVERT(adp);

    AFS_STATCNT(afs_remove);
    afs_Trace2(afs_iclSetp, CM_TRACE_REMOVE, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_STRING, aname);

#ifdef	AFS_OSF_ENV
    tvc = (struct vcache *)ndp->ni_vp;	/* should never be null */
#endif

    if ((code = afs_InitReq(&treq, acred))) {
#ifdef  AFS_OSF_ENV
	afs_PutVCache(tvc);
#endif
	return code;
    }

    afs_InitFakeStat(&fakestate);
    code = afs_EvalFakeStat(&adp, &fakestate, &treq);
    if (code) {
	afs_PutFakeStat(&fakestate);
#ifdef  AFS_OSF_ENV
	afs_PutVCache(tvc);
#endif
	return code;
    }

    /* Check if this is dynroot */
    if (afs_IsDynroot(adp)) {
	code = afs_DynrootVOPRemove(adp, acred, aname);
	afs_PutFakeStat(&fakestate);
#ifdef  AFS_OSF_ENV
	afs_PutVCache(tvc);
#endif
	return code;
    }

    if (strlen(aname) > AFSNAMEMAX) {
	afs_PutFakeStat(&fakestate);
#ifdef  AFS_OSF_ENV
	afs_PutVCache(tvc);
#endif
	return ENAMETOOLONG;
    }
  tagain:
    code = afs_VerifyVCache(adp, &treq);
#ifdef	AFS_OSF_ENV
    tvc = VTOAFS(ndp->ni_vp);	/* should never be null */
    if (code) {
	afs_PutVCache(tvc);
	afs_PutFakeStat(&fakestate);
	return afs_CheckCode(code, &treq, 22);
    }
#else /* AFS_OSF_ENV */
    tvc = NULL;
    if (code) {
	code = afs_CheckCode(code, &treq, 23);
	afs_PutFakeStat(&fakestate);
	return code;
    }
#endif

    /** If the volume is read-only, return error without making an RPC to the
      * fileserver
      */
    if (adp->states & CRO) {
#ifdef  AFS_OSF_ENV
	afs_PutVCache(tvc);
#endif
	code = EROFS;
	afs_PutFakeStat(&fakestate);
	return code;
    }

    tdc = afs_GetDCache(adp, (afs_size_t) 0, &treq, &offset, &len, 1);	/* test for error below */
    ObtainWriteLock(&adp->lock, 142);
    if (tdc)
	ObtainSharedLock(&tdc->lock, 638);

    /*
     * Make sure that the data in the cache is current. We may have
     * received a callback while we were waiting for the write lock.
     */
    if (!(adp->states & CStatd)
	|| (tdc && !hsame(adp->m.DataVersion, tdc->f.versionNo))) {
	ReleaseWriteLock(&adp->lock);
	if (tdc) {
	    ReleaseSharedLock(&tdc->lock);
	    afs_PutDCache(tdc);
	}
	goto tagain;
    }

    unlinkFid.Fid.Vnode = 0;
    if (!tvc) {
	tvc = osi_dnlc_lookup(adp, aname, WRITE_LOCK);
    }
    /* This should not be necessary since afs_lookup() has already
     * done the work */
    if (!tvc)
	if (tdc) {
	    code = afs_dir_Lookup(tdc, aname, &unlinkFid.Fid);
	    if (code == 0) {
		afs_int32 cached = 0;

		unlinkFid.Cell = adp->fid.Cell;
		unlinkFid.Fid.Volume = adp->fid.Fid.Volume;
		if (unlinkFid.Fid.Unique == 0) {
		    tvc =
			afs_LookupVCache(&unlinkFid, &treq, &cached, adp,
					 aname);
		} else {
		    ObtainReadLock(&afs_xvcache);
		    tvc = afs_FindVCache(&unlinkFid, 0, DO_STATS);
		    ReleaseReadLock(&afs_xvcache);
		}
	    }
	}

    if (tvc && osi_Active(tvc)) {
	/* about to delete whole file, prefetch it first */
	ReleaseWriteLock(&adp->lock);
	if (tdc)
	    ReleaseSharedLock(&tdc->lock);
	ObtainWriteLock(&tvc->lock, 143);
#if	defined(AFS_OSF_ENV)
	afs_Wire(tvc, &treq);
#else /* AFS_OSF_ENV */
	FetchWholeEnchilada(tvc, &treq);
#endif
	ReleaseWriteLock(&tvc->lock);
	ObtainWriteLock(&adp->lock, 144);
	/* Technically I don't think we need this back, but let's hold it 
	   anyway; The "got" reference should actually be sufficient. */
	if (tdc) 
	    ObtainSharedLock(&tdc->lock, 640);
    }

    osi_dnlc_remove(adp, aname, tvc);

    Tadp1 = adp;
#ifndef AFS_DARWIN80_ENV
    Tadpr = VREFCOUNT(adp);
#endif
    Ttvc = tvc;
    Tnam = aname;
    Tnam1 = 0;
#ifndef AFS_DARWIN80_ENV
    if (tvc)
	Ttvcr = VREFCOUNT(tvc);
#endif
#ifdef	AFS_AIX_ENV
    if (tvc && VREFCOUNT_GT(tvc, 2) && tvc->opens > 0
	&& !(tvc->states & CUnlinked)) {
#else
    if (tvc && VREFCOUNT_GT(tvc, 1) && tvc->opens > 0
	&& !(tvc->states & CUnlinked)) {
#endif
	char *unlname = afs_newname();

	ReleaseWriteLock(&adp->lock);
	if (tdc)
	    ReleaseSharedLock(&tdc->lock);
	code = afsrename(adp, aname, adp, unlname, acred, &treq);
	Tnam1 = unlname;
	if (!code) {
	    struct VenusFid *oldmvid = NULL;
	    if (tvc->mvid) 
		oldmvid = tvc->mvid;
	    tvc->mvid = (struct VenusFid *)unlname;
	    if (oldmvid)
		osi_FreeSmallSpace(oldmvid);
	    crhold(acred);
	    if (tvc->uncred) {
		crfree(tvc->uncred);
	    }
	    tvc->uncred = acred;
	    tvc->states |= CUnlinked;
	} else {
	    osi_FreeSmallSpace(unlname);
	}
	if (tdc)
	    afs_PutDCache(tdc);
	afs_PutVCache(tvc);
    } else {
	code = afsremove(adp, tdc, tvc, aname, acred, &treq);
    }
    afs_PutFakeStat(&fakestate);
    osi_Assert(!WriteLocked(&adp->lock) || (adp->lock.pid_writer != MyPidxx));
    return code;
}


/* afs_remunlink -- This tries to delete the file at the server after it has
 *     been renamed when unlinked locally but now has been finally released.
 *
 * CAUTION -- may be called with avc unheld. */

int
afs_remunlink(register struct vcache *avc, register int doit)
{
    struct AFS_UCRED *cred;
    char *unlname;
    struct vcache *adp;
    struct vrequest treq;
    struct VenusFid dirFid;
    register struct dcache *tdc;
    afs_int32 code = 0;

    if (NBObtainWriteLock(&avc->lock, 423))
	return 0;
#if defined(AFS_DARWIN80_ENV)
    if (vnode_get(AFSTOV(avc))) {
	ReleaseWriteLock(&avc->lock);
	return 0;
    }
#endif

    if (avc->mvid && (doit || (avc->states & CUnlinkedDel))) {
	if ((code = afs_InitReq(&treq, avc->uncred))) {
	    ReleaseWriteLock(&avc->lock);
	} else {
	    /* Must bump the refCount because GetVCache may block.
	     * Also clear mvid so no other thread comes here if we block.
	     */
	    unlname = (char *)avc->mvid;
	    avc->mvid = NULL;
	    cred = avc->uncred;
	    avc->uncred = NULL;

#if defined(AFS_DARWIN_ENV) && !defined(AFS_DARWIN80_ENV)
	    VREF(AFSTOV(avc));
#else
	    VN_HOLD(AFSTOV(avc));
#endif

	    /* We'll only try this once. If it fails, just release the vnode.
	     * Clear after doing hold so that NewVCache doesn't find us yet.
	     */
	    avc->states &= ~(CUnlinked | CUnlinkedDel);

	    ReleaseWriteLock(&avc->lock);

	    dirFid.Cell = avc->fid.Cell;
	    dirFid.Fid.Volume = avc->fid.Fid.Volume;
	    dirFid.Fid.Vnode = avc->parentVnode;
	    dirFid.Fid.Unique = avc->parentUnique;
	    adp = afs_GetVCache(&dirFid, &treq, NULL, NULL);

	    if (adp) {
		tdc = afs_FindDCache(adp, (afs_size_t) 0);
		ObtainWriteLock(&adp->lock, 159);
		if (tdc)
		    ObtainSharedLock(&tdc->lock, 639);

		/* afsremove releases the adp & tdc locks, and does vn_rele(avc) */
		code = afsremove(adp, tdc, avc, unlname, cred, &treq);
		afs_PutVCache(adp);
	    } else {
		/* we failed - and won't be back to try again. */
		afs_PutVCache(avc);
	    }
	    osi_FreeSmallSpace(unlname);
	    crfree(cred);
	}
    } else {
#if defined(AFS_DARWIN80_ENV)
	vnode_put(AFSTOV(avc));
#endif
	ReleaseWriteLock(&avc->lock);
    }

    return code;
}
