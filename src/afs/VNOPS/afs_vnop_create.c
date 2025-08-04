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
 * afs_create
 * afs_LocalHero
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

/* question: does afs_create need to set CDirty in the adp or the avc?
 * I think we can get away without it, but I'm not sure.  Note that
 * afs_setattr is called in here for truncation.
 */
#ifdef AFS_SGI_ENV
int
afs_create(OSI_VC_DECL(adp), char *aname, struct vattr *attrs, int flags,
	   int amode, struct vcache **avcp, afs_ucred_t *acred)
#else /* AFS_SGI_ENV */
int
afs_create(OSI_VC_DECL(adp), char *aname, struct vattr *attrs,
	   enum vcexcl aexcl, int amode, struct vcache **avcp,
	   afs_ucred_t *acred)
#endif				/* AFS_SGI_ENV */
{
    afs_int32 origCBs, origZaps, finalZaps;
    struct vrequest *treq = NULL;
    afs_int32 code;
    struct afs_conn *tc;
    struct VenusFid newFid;
    struct AFSStoreStatus InStatus;
    struct AFSFetchStatus *OutFidStatus, *OutDirStatus;
    struct AFSVolSync tsync;
    struct AFSCallBack CallBack;
    afs_int32 now;
    struct dcache *tdc;
    afs_size_t offset, len;
    struct server *hostp = 0;
    struct vcache *tvc;
    struct volume *volp = 0;
    struct afs_fakestat_state fakestate;
    struct rx_connection *rxconn;
    XSTATS_DECLS;
    OSI_VC_CONVERT(adp);

    AFS_STATCNT(afs_create);

    OutFidStatus = osi_AllocSmallSpace(sizeof(struct AFSFetchStatus));
    OutDirStatus = osi_AllocSmallSpace(sizeof(struct AFSFetchStatus));
    memset(&InStatus, 0, sizeof(InStatus));

    if ((code = afs_CreateReq(&treq, acred)))
	goto done2;

    afs_Trace3(afs_iclSetp, CM_TRACE_CREATE, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_STRING, aname, ICL_TYPE_INT32, amode);

    afs_InitFakeStat(&fakestate);

#ifdef AFS_SGI_ENV
    /* If avcp is passed not null, it's the old reference to this file.
     * We can use this to avoid create races. For now, just decrement
     * the reference count on it.
     */
    if (*avcp) {
	AFS_RELE(AFSTOV(*avcp));
	*avcp = NULL;
    }
#endif

    if (strlen(aname) > AFSNAMEMAX) {
	code = ENAMETOOLONG;
	goto done3;
    }

    if (!afs_ENameOK(aname)) {
	code = EINVAL;
	goto done3;
    }
    switch (attrs->va_type) {
    case VBLK:
    case VCHR:
#if	!defined(AFS_SUN5_ENV)
    case VSOCK:
#endif
    case VFIFO:
	/* We don't support special devices or FIFOs */
	code = EINVAL;
	goto done3;
    default:
	;
    }
    AFS_DISCON_LOCK();

    code = afs_EvalFakeStat(&adp, &fakestate, treq);
    if (code)
	goto done;
  tagain:
    code = afs_VerifyVCache(adp, treq);
    if (code)
	goto done;

    /** If the volume is read-only, return error without making an RPC to the
      * fileserver
      */
    if (adp->f.states & CRO) {
	code = EROFS;
	goto done;
    }

    if (AFS_IS_DISCONNECTED && !AFS_IS_DISCON_RW) {
        code = ENETDOWN;
        goto done;
    }

    tdc = afs_GetDCache(adp, (afs_size_t) 0, treq, &offset, &len, 1);

    /** Prevent multiple fetchStatus calls to fileserver when afs_GetDCache()
      * returns NULL for an error condition
      */
    if (!tdc) {
      code = EIO;
      goto done;
    }

    ObtainWriteLock(&adp->lock, 135);
    if (tdc)
	ObtainSharedLock(&tdc->lock, 630);

    /*
     * Make sure that the data in the cache is current. We may have
     * received a callback while we were waiting for the write lock.
     */
    if (!(adp->f.states & CStatd)
	|| (tdc && !afs_IsDCacheFresh(tdc, adp))) {
	ReleaseWriteLock(&adp->lock);
	if (tdc) {
	    ReleaseSharedLock(&tdc->lock);
	    afs_PutDCache(tdc);
	}
	goto tagain;
    }
    if (tdc) {
	/* see if file already exists.  If it does, we only set 
	 * the size attributes (to handle O_TRUNC) */
	code = afs_dir_Lookup(tdc, aname, &newFid.Fid);	/* use dnlc first xxx */
	if (code == 0) {
	    ReleaseSharedLock(&tdc->lock);
	    afs_PutDCache(tdc);
	    ReleaseWriteLock(&adp->lock);
#ifdef AFS_SGI_ENV
	    if (flags & VEXCL) {
#else
	    if (aexcl != NONEXCL) {
#endif
		code = EEXIST;	/* file exists in excl mode open */
		goto done;
	    }
	    /* found the file, so use it */
	    newFid.Cell = adp->f.fid.Cell;
	    newFid.Fid.Volume = adp->f.fid.Fid.Volume;
	    tvc = NULL;
	    if (newFid.Fid.Unique == 0) {
		tvc = afs_LookupVCache(&newFid, treq, adp, aname);
	    }
	    if (!tvc)		/* lookup failed or wasn't called */
		tvc = afs_GetVCache(&newFid, treq);

	    if (tvc) {
		/* if the thing exists, we need the right access to open it.
		 * we must check that here, since no other checks are
		 * made by the open system call */
		len = attrs->va_size;	/* only do the truncate */
		/*
		 * We used to check always for READ access before; the
		 * problem is that we will fail if the existing file
		 * has mode -w-w-w, which is wrong.
		 */
		if ((amode & VREAD)
		    && !afs_AccessOK(tvc, PRSFS_READ, treq, CHECK_MODE_BITS)) {
		    afs_PutVCache(tvc);
		    code = EACCES;
		    goto done;
		}
#if defined(AFS_DARWIN80_ENV)
		if ((amode & VWRITE) || VATTR_IS_ACTIVE(attrs, va_data_size))
#elif defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
		if ((amode & VWRITE) || (attrs->va_mask & AT_SIZE))
#else
		if ((amode & VWRITE) || len != 0xffffffff)
#endif
		{
		    /* needed for write access check */
		    tvc->f.parent.vnode = adp->f.fid.Fid.Vnode;
		    tvc->f.parent.unique = adp->f.fid.Fid.Unique;
		    /* need write mode for these guys */
		    if (!afs_AccessOK
			(tvc, PRSFS_WRITE, treq, CHECK_MODE_BITS)) {
			afs_PutVCache(tvc);
			code = EACCES;
			goto done;
		    }
		}
#if defined(AFS_DARWIN80_ENV)
		if (VATTR_IS_ACTIVE(attrs, va_data_size))
#elif defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
		if (attrs->va_mask & AT_SIZE)
#else
		if (len != 0xffffffff)
#endif
		{
		    if (vType(tvc) != VREG) {
			afs_PutVCache(tvc);
			code = EISDIR;
			goto done;
		    }
		    /* do a truncate */
#if defined(AFS_DARWIN80_ENV)
		    VATTR_INIT(attrs);
		    VATTR_SET_SUPPORTED(attrs, va_data_size);
		    VATTR_SET_ACTIVE(attrs, va_data_size);
#elif defined(UKERNEL)
		    attrs->va_mask = ATTR_SIZE;
#elif defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
		    attrs->va_mask = AT_SIZE;
#else
		    VATTR_NULL(attrs);
#endif
		    attrs->va_size = len;
		    ObtainWriteLock(&tvc->lock, 136);
		    tvc->f.states |= CCreating;
		    ReleaseWriteLock(&tvc->lock);
#if defined(AFS_SGI_ENV)
		    code =
			afs_setattr(VNODE_TO_FIRST_BHV((vnode_t *) tvc),
				    attrs, 0, acred);
#elif defined(AFS_SUN5_ENV)
		    code = afs_setattr(tvc, attrs, 0, acred);
#else
		    code = afs_setattr(tvc, attrs, acred);
#endif
		    ObtainWriteLock(&tvc->lock, 137);
		    tvc->f.states &= ~CCreating;
		    ReleaseWriteLock(&tvc->lock);
		    if (code) {
			afs_PutVCache(tvc);
			goto done;
		    }
		}
		*avcp = tvc;

	    } else {
		/* Directory entry already exists, but we cannot fetch the
		 * fid it points to. */
		code = EIO;
	    }
	    /* make sure vrefCount bumped only if code == 0 */
	    goto done;
	}
    }
    
    /* if we create the file, we don't do any access checks, since
     * that's how O_CREAT is supposed to work */
    if (adp->f.states & CForeign) {
	origCBs = afs_allCBs;
	origZaps = afs_allZaps;
    } else {
	origCBs = afs_evenCBs;	/* if changes, we don't really have a callback */
	origZaps = afs_evenZaps;	/* number of even numbered vnodes discarded */
    }
    InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE | AFS_SETGROUP;
    InStatus.ClientModTime = osi_Time();
    InStatus.Group = (afs_int32) afs_cr_gid(acred);
    if (AFS_NFSXLATORREQ(acred)) {
	/*
	 * XXX The following is mainly used to fix a bug in the HP-UX
	 * nfs client where they create files with mode of 0 without
	 * doing any setattr later on to fix it.  * XXX
	 */
#if	defined(AFS_AIX_ENV)
	if (attrs->va_mode != -1) {
#else
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
	if (attrs->va_mask & AT_MODE) {
#else
	if (attrs->va_mode != ((unsigned short)-1)) {
#endif
#endif
	    if (!attrs->va_mode)
		attrs->va_mode = 0x1b6;	/* XXX default mode: rw-rw-rw XXX */
	}
    }

    if (!AFS_IS_DISCONNECTED) {
	/* If not disconnected, connect to the server.*/

    	InStatus.UnixModeBits = attrs->va_mode & 0xffff;	/* only care about protection bits */
    	do {
	    tc = afs_Conn(&adp->f.fid, treq, SHARED_LOCK, &rxconn);
	    if (tc) {
	    	hostp = tc->parent->srvr->server; /* remember for callback processing */
	    	now = osi_Time();
	    	XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_CREATEFILE);
	    	RX_AFS_GUNLOCK();
	    	code =
		    RXAFS_CreateFile(rxconn, (struct AFSFid *)&adp->f.fid.Fid,
				 aname, &InStatus, (struct AFSFid *)
				 &newFid.Fid, OutFidStatus, OutDirStatus,
				 &CallBack, &tsync);
	    	RX_AFS_GLOCK();
	    	XSTATS_END_TIME;
	    	CallBack.ExpirationTime += now;
	    } else
	    	code = -1;
    	} while (afs_Analyze
	         (tc, rxconn, code, &adp->f.fid, treq, AFS_STATS_FS_RPCIDX_CREATEFILE,
	          SHARED_LOCK, NULL));

	if ((code == EEXIST || code == UAEEXIST) &&
#ifdef AFS_SGI_ENV
    	!(flags & VEXCL)
#else /* AFS_SGI_ENV */
    	aexcl == NONEXCL
#endif
    	) {
	    /* if we get an EEXIST in nonexcl mode, just do a lookup */
	    if (tdc) {
	    	ReleaseSharedLock(&tdc->lock);
	    	afs_PutDCache(tdc);
	    }
	    ReleaseWriteLock(&adp->lock);


#if defined(AFS_SGI_ENV)
	    code = afs_lookup(VNODE_TO_FIRST_BHV((vnode_t *) adp), aname, avcp,
				  NULL, 0, NULL, acred);
#elif defined(AFS_SUN5_ENV)
	    code = afs_lookup(adp, aname, avcp, NULL, 0, NULL, acred);
#elif defined(UKERNEL)
	    code = afs_lookup(adp, aname, avcp, acred, 0);
#elif !defined(AFS_DARWIN_ENV)
	    code = afs_lookup(adp, aname, avcp, acred);
#endif
	goto done;
        }

	if (code) {
	    if (code < 0) {
		afs_StaleVCache(adp);
	    }
	    ReleaseWriteLock(&adp->lock);
	    if (tdc) {
	    	ReleaseSharedLock(&tdc->lock);
	    	afs_PutDCache(tdc);
	    }
	goto done;
	}

    } else {
	/* Generate a fake FID for disconnected mode. */
	newFid.Cell = adp->f.fid.Cell;
	newFid.Fid.Volume = adp->f.fid.Fid.Volume;
	afs_GenFakeFid(&newFid, VREG, 1);
    }				/* if (!AFS_IS_DISCON_RW) */

    /* otherwise, we should see if we can make the change to the dir locally */
    if (tdc)
	UpgradeSToWLock(&tdc->lock, 631);
    if (AFS_IS_DISCON_RW || afs_LocalHero(adp, tdc, OutDirStatus, 1)) {
	/* we can do it locally */
	ObtainWriteLock(&afs_xdcache, 291);
	code = afs_dir_Create(tdc, aname, &newFid.Fid);
	ReleaseWriteLock(&afs_xdcache);
	if (code) {
	    ZapDCE(tdc);
	    DZap(tdc);
	}
    }
    if (tdc) {
	ReleaseWriteLock(&tdc->lock);
	afs_PutDCache(tdc);
    }
    if (AFS_IS_DISCON_RW)
	adp->f.m.LinkCount++;

    newFid.Cell = adp->f.fid.Cell;
    newFid.Fid.Volume = adp->f.fid.Fid.Volume;
    ReleaseWriteLock(&adp->lock);
    volp = afs_FindVolume(&newFid, READ_LOCK);

    /* New tricky optimistic callback handling algorithm for file creation works
     * as follows.  We create the file essentially with no locks set at all.  File
     * server may thus handle operations from others cache managers as well as from
     * this very own cache manager that reference the file in question before
     * we managed to create the cache entry.  However, if anyone else changes
     * any of the status information for a file, we'll see afs_evenCBs increase
     * (files always have even fids).  If someone on this workstation manages
     * to do something to the file, they'll end up having to create a cache
     * entry for the new file.  Either we'll find it once we've got the afs_xvcache
     * lock set, or it was also *deleted* the vnode before we got there, in which case
     * we will find evenZaps has changed, too.  Thus, we only assume we have the right
     * status information if no callbacks or vnode removals have occurred to even
     * numbered files from the time the call started until the time that we got the xvcache
     * lock set.  Of course, this also assumes that any call that modifies a file first
     * gets a write lock on the file's vnode, but if that weren't true, the whole cache manager
     * would fail, since no call would be able to update the local vnode status after modifying
     * a file on a file server. */
    ObtainWriteLock(&afs_xvcache, 138);
    if (adp->f.states & CForeign)
	finalZaps = afs_allZaps;	/* do this before calling newvcache */
    else
	finalZaps = afs_evenZaps;	/* do this before calling newvcache */
    /* don't need to call RemoveVCB, since only path leaving a callback is the
     * one where we pass through afs_NewVCache.  Can't have queued a VCB unless
     * we created and freed an entry between file creation time and here, and the
     * freeing of the vnode will change evenZaps.  Don't need to update the VLRU
     * queue, since the find will only succeed in the event of a create race, and 
     * then the vcache will be at the front of the VLRU queue anyway...  */
    if (!(tvc = afs_FindVCache(&newFid, DO_STATS))) {
	tvc = afs_NewVCache(&newFid, hostp);
	if (tvc) {
	    int finalCBs;
	    ObtainWriteLock(&tvc->lock, 139);

	    ObtainWriteLock(&afs_xcbhash, 489);
	    finalCBs = afs_evenCBs;
	    /* add the callback in */
	    if (adp->f.states & CForeign) {
		tvc->f.states |= CForeign;
		finalCBs = afs_allCBs;
	    }
	    if (origCBs == finalCBs && origZaps == finalZaps) {
		tvc->f.states |= CStatd;	/* we've fake entire thing, so don't stat */
		tvc->f.states &= ~CBulkFetching;
		if (!AFS_IS_DISCON_RW) {
		    tvc->cbExpires = CallBack.ExpirationTime;
		    afs_QueueCallback(tvc, CBHash(CallBack.ExpirationTime), volp);
		}
	    } else {
		afs_StaleVCacheFlags(tvc,
				     AFS_STALEVC_CBLOCKED | AFS_STALEVC_CLEARCB,
				     CUnique);
	    }
	    ReleaseWriteLock(&afs_xcbhash);
	    if (AFS_IS_DISCON_RW) {
		afs_DisconAddDirty(tvc, VDisconCreate, 0);
		afs_GenDisconStatus(adp, tvc, &newFid, attrs, treq, VREG);
	    } else {
		afs_ProcessFS(tvc, OutFidStatus, treq);
	    }

	    tvc->f.parent.vnode = adp->f.fid.Fid.Vnode;
	    tvc->f.parent.unique = adp->f.fid.Fid.Unique;
	    ReleaseWriteLock(&tvc->lock);
	    *avcp = tvc;
	    code = 0;

	} else {
	    /* Cannot create a new vcache. */
	    code = EIO;
	}
    } else {
	/* otherwise cache entry already exists, someone else must
	 * have created it.  Comments used to say:  "don't need write
	 * lock to *clear* these flags" but we should do it anyway.
	 * Code used to clear stat bit and callback, but I don't see 
	 * the point -- we didn't have a create race, somebody else just
	 * snuck into NewVCache before we got here, probably a racing 
	 * lookup.
	 */
	*avcp = tvc;
	code = 0;
    }
    ReleaseWriteLock(&afs_xvcache);

  done:
    AFS_DISCON_UNLOCK();

  done3:
    if (volp)
	afs_PutVolume(volp, READ_LOCK);

    if (code == 0) {
	if (afs_mariner)
	    afs_AddMarinerName(aname, *avcp);
	/* return the new status in vattr */
	afs_CopyOutAttrs(*avcp, attrs);
	if (afs_mariner)
	    afs_MarinerLog("store$Creating", *avcp);
    }

    afs_PutFakeStat(&fakestate);
    code = afs_CheckCode(code, treq, 20);
    afs_DestroyReq(treq);

  done2:
    osi_FreeSmallSpace(OutFidStatus);
    osi_FreeSmallSpace(OutDirStatus);
    return code;
}


/*
 * Check to see if we can track the change locally: requires that
 * we have sufficiently recent info in data cache.  If so, we
 * know the new DataVersion number, and place it correctly in both the
 * data and stat cache entries.  This routine returns 1 if we should
 * do the operation locally, and 0 otherwise.
 *
 * This routine must be called with the stat cache entry write-locked,
 * and dcache entry write-locked.
 */
int
afs_LocalHero(struct vcache *avc, struct dcache *adc,
	      AFSFetchStatus * astat, int aincr)
{
    afs_int32 ok;
    afs_hyper_t avers;

    AFS_STATCNT(afs_LocalHero);
    hset64(avers, astat->dataVersionHigh, astat->DataVersion);
    /* avers *is* the version number now, no matter what */

    if (adc) {
	/* does what's in the dcache *now* match what's in the vcache *now*,
	 * and do we have a valid callback? if not, our local copy is not "ok" */
	ok = (afs_IsDCacheFresh(adc, avc) && avc->callback
	      && (avc->f.states & CStatd) && avc->cbExpires >= osi_Time());
    } else {
	ok = 0;
    }
    if (ok) {
	/* check that the DV on the server is what we expect it to be */
	afs_hyper_t newDV;
	hset(newDV, adc->f.versionNo);
	hadd32(newDV, aincr);
	if (!hsame(avers, newDV)) {
	    ok = 0;
	}
    }
#if defined(AFS_SGI_ENV)
    osi_Assert(avc->v.v_type == VDIR);
#endif
    /* The bulk status code used the length as a sequence number.  */
    /* Don't update the vcache entry unless the stats are current. */
    if (avc->f.states & CStatd) {
	afs_SetDataVersion(avc, &avers);
#ifdef AFS_64BIT_CLIENT
	FillInt64(avc->f.m.Length, astat->Length_hi, astat->Length);
#else /* AFS_64BIT_CLIENT */
	avc->f.m.Length = astat->Length;
#endif /* AFS_64BIT_CLIENT */
	avc->f.m.Date = astat->ClientModTime;
    }
    if (ok) {
	/* we've been tracking things correctly */
	adc->dflags |= DFEntryMod;
	adc->f.versionNo = avers;
	return 1;
    } else {
	if (adc) {
	    ZapDCE(adc);
	    DZap(adc);
	}
	if (avc->f.states & CStatd) {
	    osi_dnlc_purgedp(avc);
	}
	return 0;
    }
}
