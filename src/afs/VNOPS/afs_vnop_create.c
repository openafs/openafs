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

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"

/* question: does afs_create need to set CDirty in the adp or the avc?
 * I think we can get away without it, but I'm not sure.  Note that
 * afs_setattr is called in here for truncation.
 */
#ifdef AFS_SGI64_ENV
int
afs_create(OSI_VC_DECL(adp), char *aname, struct vattr *attrs, int flags,
	   int amode, struct vcache **avcp, struct AFS_UCRED *acred)
#else /* AFS_SGI64_ENV */
int
afs_create(OSI_VC_DECL(adp), char *aname, struct vattr *attrs,
	   enum vcexcl aexcl, int amode, struct vcache **avcp,
	   struct AFS_UCRED *acred)
#endif				/* AFS_SGI64_ENV */
{
    afs_int32 origCBs, origZaps, finalZaps;
    struct vrequest treq;
    register afs_int32 code;
    register struct conn *tc;
    struct VenusFid newFid;
    struct AFSStoreStatus InStatus;
    struct AFSFetchStatus OutFidStatus, OutDirStatus;
    struct AFSVolSync tsync;
    struct AFSCallBack CallBack;
    afs_int32 now;
    struct dcache *tdc;
    afs_size_t offset, len;
    struct server *hostp = 0;
    struct vcache *tvc;
    struct volume *volp = 0;
    struct afs_fakestat_state fakestate;
    XSTATS_DECLS;
    OSI_VC_CONVERT(adp);


    AFS_STATCNT(afs_create);
    if ((code = afs_InitReq(&treq, acred)))
	goto done2;

    afs_Trace3(afs_iclSetp, CM_TRACE_CREATE, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_STRING, aname, ICL_TYPE_INT32, amode);

    afs_InitFakeStat(&fakestate);

#ifdef AFS_SGI65_ENV
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
	goto done;
    }

    if (!afs_ENameOK(aname)) {
	code = EINVAL;
	goto done;
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
	goto done;
    default:
	;
    }
    code = afs_EvalFakeStat(&adp, &fakestate, &treq);
    if (code)
	goto done;
  tagain:
    code = afs_VerifyVCache(adp, &treq);
    if (code)
	goto done;

    /** If the volume is read-only, return error without making an RPC to the
      * fileserver
      */
    if (adp->states & CRO) {
	code = EROFS;
	goto done;
    }

    tdc = afs_GetDCache(adp, (afs_size_t) 0, &treq, &offset, &len, 1);
    ObtainWriteLock(&adp->lock, 135);
    if (tdc)
	ObtainSharedLock(&tdc->lock, 630);

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
    if (tdc) {
	/* see if file already exists.  If it does, we only set 
	 * the size attributes (to handle O_TRUNC) */
	code = afs_dir_Lookup(tdc, aname, &newFid.Fid);	/* use dnlc first xxx */
	if (code == 0) {
	    ReleaseSharedLock(&tdc->lock);
	    afs_PutDCache(tdc);
	    ReleaseWriteLock(&adp->lock);
#ifdef AFS_SGI64_ENV
	    if (flags & VEXCL) {
#else
	    if (aexcl != NONEXCL) {
#endif
		code = EEXIST;	/* file exists in excl mode open */
		goto done;
	    }
	    /* found the file, so use it */
	    newFid.Cell = adp->fid.Cell;
	    newFid.Fid.Volume = adp->fid.Fid.Volume;
	    tvc = NULL;
	    if (newFid.Fid.Unique == 0) {
		tvc = afs_LookupVCache(&newFid, &treq, NULL, adp, aname);
	    }
	    if (!tvc)		/* lookup failed or wasn't called */
		tvc = afs_GetVCache(&newFid, &treq, NULL, NULL);

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
		    && !afs_AccessOK(tvc, PRSFS_READ, &treq, CHECK_MODE_BITS)) {
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
		    tvc->parentVnode = adp->fid.Fid.Vnode;
		    tvc->parentUnique = adp->fid.Fid.Unique;
		    /* need write mode for these guys */
		    if (!afs_AccessOK
			(tvc, PRSFS_WRITE, &treq, CHECK_MODE_BITS)) {
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
#elif defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
		    attrs->va_mask = AT_SIZE;
#else
		    VATTR_NULL(attrs);
#endif
		    attrs->va_size = len;
		    ObtainWriteLock(&tvc->lock, 136);
		    tvc->states |= CCreating;
		    ReleaseWriteLock(&tvc->lock);
#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
#if defined(AFS_SGI64_ENV)
		    code =
			afs_setattr(VNODE_TO_FIRST_BHV((vnode_t *) tvc),
				    attrs, 0, acred);
#else
		    code = afs_setattr(tvc, attrs, 0, acred);
#endif /* AFS_SGI64_ENV */
#else /* SUN5 || SGI */
		    code = afs_setattr(tvc, attrs, acred);
#endif /* SUN5 || SGI */
		    ObtainWriteLock(&tvc->lock, 137);
		    tvc->states &= ~CCreating;
		    ReleaseWriteLock(&tvc->lock);
		    if (code) {
			afs_PutVCache(tvc);
			goto done;
		    }
		}
		*avcp = tvc;
	    } else
		code = ENOENT;	/* shouldn't get here */
	    /* make sure vrefCount bumped only if code == 0 */
	    goto done;
	}
    }

    /* if we create the file, we don't do any access checks, since
     * that's how O_CREAT is supposed to work */
    if (adp->states & CForeign) {
	origCBs = afs_allCBs;
	origZaps = afs_allZaps;
    } else {
	origCBs = afs_evenCBs;	/* if changes, we don't really have a callback */
	origZaps = afs_evenZaps;	/* number of even numbered vnodes discarded */
    }
    InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE | AFS_SETGROUP;
    InStatus.ClientModTime = osi_Time();
    InStatus.Group = (afs_int32) acred->cr_gid;
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
    InStatus.UnixModeBits = attrs->va_mode & 0xffff;	/* only care about protection bits */
    do {
	tc = afs_Conn(&adp->fid, &treq, SHARED_LOCK);
	if (tc) {
	    hostp = tc->srvr->server;	/* remember for callback processing */
	    now = osi_Time();
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_CREATEFILE);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_CreateFile(tc->id, (struct AFSFid *)&adp->fid.Fid,
				 aname, &InStatus, (struct AFSFid *)
				 &newFid.Fid, &OutFidStatus, &OutDirStatus,
				 &CallBack, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	    CallBack.ExpirationTime += now;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, code, &adp->fid, &treq, AFS_STATS_FS_RPCIDX_CREATEFILE,
	      SHARED_LOCK, NULL));

    if (code == EEXIST &&
#ifdef AFS_SGI64_ENV
    !(flags & VEXCL)
#else /* AFS_SGI64_ENV */
    aexcl == NONEXCL
#endif
    ) {
	/* if we get an EEXIST in nonexcl mode, just do a lookup */
	if (tdc) {
	    ReleaseSharedLock(&tdc->lock);
	    afs_PutDCache(tdc);
	}
	ReleaseWriteLock(&adp->lock);
#if !(defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV))
#if defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
#if defined(AFS_SGI64_ENV)
	code = afs_lookup(VNODE_TO_FIRST_BHV((vnode_t *) adp), aname, avcp, 
			  NULL, 0, NULL, acred);
#else
	code = afs_lookup(adp, aname, avcp, NULL, 0, NULL, acred);
#endif /* AFS_SGI64_ENV */
#else /* SUN5 || SGI */
	code = afs_lookup(adp, aname, avcp, acred);
#endif /* SUN5 || SGI */
#endif /* !(AFS_OSF_ENV || AFS_DARWIN_ENV) */
	goto done;
    }
    if (code) {
	if (code < 0) {
	    ObtainWriteLock(&afs_xcbhash, 488);
	    afs_DequeueCallback(adp);
	    adp->states &= ~CStatd;
	    ReleaseWriteLock(&afs_xcbhash);
	    osi_dnlc_purgedp(adp);
	}
	ReleaseWriteLock(&adp->lock);
	if (tdc) {
	    ReleaseSharedLock(&tdc->lock);
	    afs_PutDCache(tdc);
	}
	goto done;
    }
    /* otherwise, we should see if we can make the change to the dir locally */
    if (tdc)
	UpgradeSToWLock(&tdc->lock, 631);
    if (afs_LocalHero(adp, tdc, &OutDirStatus, 1)) {
	/* we can do it locally */
	code = afs_dir_Create(tdc, aname, &newFid.Fid);
	if (code) {
	    ZapDCE(tdc);
	    DZap(tdc);
	}
    }
    if (tdc) {
	ReleaseWriteLock(&tdc->lock);
	afs_PutDCache(tdc);
    }
    newFid.Cell = adp->fid.Cell;
    newFid.Fid.Volume = adp->fid.Fid.Volume;
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
    if (adp->states & CForeign)
	finalZaps = afs_allZaps;	/* do this before calling newvcache */
    else
	finalZaps = afs_evenZaps;	/* do this before calling newvcache */
    /* don't need to call RemoveVCB, since only path leaving a callback is the
     * one where we pass through afs_NewVCache.  Can't have queued a VCB unless
     * we created and freed an entry between file creation time and here, and the
     * freeing of the vnode will change evenZaps.  Don't need to update the VLRU
     * queue, since the find will only succeed in the event of a create race, and 
     * then the vcache will be at the front of the VLRU queue anyway...  */
    if (!(tvc = afs_FindVCache(&newFid, 0, DO_STATS))) {
	tvc = afs_NewVCache(&newFid, hostp);
	if (tvc) {
	    int finalCBs;
	    ObtainWriteLock(&tvc->lock, 139);

	    ObtainWriteLock(&afs_xcbhash, 489);
	    finalCBs = afs_evenCBs;
	    /* add the callback in */
	    if (adp->states & CForeign) {
		tvc->states |= CForeign;
		finalCBs = afs_allCBs;
	    }
	    if (origCBs == finalCBs && origZaps == finalZaps) {
		tvc->states |= CStatd;	/* we've fake entire thing, so don't stat */
		tvc->states &= ~CBulkFetching;
		tvc->cbExpires = CallBack.ExpirationTime;
		afs_QueueCallback(tvc, CBHash(CallBack.ExpirationTime), volp);
	    } else {
		afs_DequeueCallback(tvc);
		tvc->states &= ~(CStatd | CUnique);
		tvc->callback = 0;
		if (tvc->fid.Fid.Vnode & 1 || (vType(tvc) == VDIR))
		    osi_dnlc_purgedp(tvc);
	    }
	    ReleaseWriteLock(&afs_xcbhash);
	    afs_ProcessFS(tvc, &OutFidStatus, &treq);
	    tvc->parentVnode = adp->fid.Fid.Vnode;
	    tvc->parentUnique = adp->fid.Fid.Unique;
	    ReleaseWriteLock(&tvc->lock);
	    *avcp = tvc;
	    code = 0;
	} else
	    code = ENOENT;
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
    if (volp)
	afs_PutVolume(volp, READ_LOCK);

    if (code == 0) {
	afs_AddMarinerName(aname, *avcp);
	/* return the new status in vattr */
	afs_CopyOutAttrs(*avcp, attrs);
    }
#ifdef	AFS_OSF_ENV
    if (!code && !strcmp(aname, "core"))
	tvc->states |= CCore1;
#endif

    afs_PutFakeStat(&fakestate);
    code = afs_CheckCode(code, &treq, 20);

  done2:
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
afs_LocalHero(register struct vcache *avc, register struct dcache *adc,
	      register AFSFetchStatus * astat, register int aincr)
{
    register afs_int32 ok;
    afs_hyper_t avers;

    AFS_STATCNT(afs_LocalHero);
    hset64(avers, astat->dataVersionHigh, astat->DataVersion);
    /* this *is* the version number, no matter what */
    if (adc) {
	ok = (hsame(avc->m.DataVersion, adc->f.versionNo) && avc->callback
	      && (avc->states & CStatd) && avc->cbExpires >= osi_Time());
    } else {
	ok = 0;
    }
#if defined(AFS_SGI_ENV)
    osi_Assert(avc->v.v_type == VDIR);
#endif
    /* The bulk status code used the length as a sequence number.  */
    /* Don't update the vcache entry unless the stats are current. */
    if (avc->states & CStatd) {
	hset(avc->m.DataVersion, avers);
#ifdef AFS_64BIT_CLIENT
	FillInt64(avc->m.Length, astat->Length_hi, astat->Length);
#else /* AFS_64BIT_ENV */
	avc->m.Length = astat->Length;
#endif /* AFS_64BIT_ENV */
	avc->m.Date = astat->ClientModTime;
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
	if (avc->states & CStatd) {
	    osi_dnlc_purgedp(avc);
	}
	return 0;
    }
}
