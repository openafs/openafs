/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_vnop_symlink.c - symlink and readlink vnodeops.
 *
 * Implements:
 * afs_symlink
 * afs_MemHandleLink
 * afs_UFSHandleLink
 * afs_readlink
 *
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/VNOPS/afs_vnop_symlink.c,v 1.19.2.5 2005/10/15 02:33:12 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"

extern afs_rwlock_t afs_xvcache;
extern afs_rwlock_t afs_xcbhash;

/* Note: There is the bare bones beginning of symlink hints in the now
 * defunct afs/afs_lookup.c file. Since they are not in use, making the call
 * is just a performance hit.
 */


/* don't set CDirty in here because RPC is called synchronously */
int afs_symlink
  (OSI_VC_ARG(adp), aname, attrs, atargetName, acred)
     OSI_VC_DECL(adp);
     char *atargetName;
     char *aname;
     struct vattr *attrs;
     struct AFS_UCRED *acred;
{
    afs_uint32 now = 0;
    struct vrequest treq;
    afs_int32 code;
    struct conn *tc;
    struct VenusFid newFid;
    struct dcache *tdc;
    afs_size_t offset, len;
    afs_int32 alen;
    struct server *hostp = 0;
    struct vcache *tvc;
    struct AFSStoreStatus InStatus;
    struct AFSFetchStatus OutFidStatus, OutDirStatus;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    struct volume *volp = 0;
    struct afs_fakestat_state fakestate;
    XSTATS_DECLS;
    OSI_VC_CONVERT(adp);

    AFS_STATCNT(afs_symlink);
    afs_Trace2(afs_iclSetp, CM_TRACE_SYMLINK, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_STRING, aname);

    if ((code = afs_InitReq(&treq, acred)))
	goto done2;

    afs_InitFakeStat(&fakestate);
    code = afs_EvalFakeStat(&adp, &fakestate, &treq);
    if (code)
	goto done;

    if (strlen(aname) > AFSNAMEMAX || strlen(atargetName) > AFSPATHMAX) {
	code = ENAMETOOLONG;
	goto done;
    }

    if (afs_IsDynroot(adp)) {
	code = afs_DynrootVOPSymlink(adp, acred, aname, atargetName);
	goto done;
    }

    code = afs_VerifyVCache(adp, &treq);
    if (code) {
	code = afs_CheckCode(code, &treq, 30);
	goto done;
    }

    /** If the volume is read-only, return error without making an RPC to the
      * fileserver
      */
    if (adp->states & CRO) {
	code = EROFS;
	goto done;
    }

    InStatus.Mask = AFS_SETMODTIME | AFS_SETMODE;
    InStatus.ClientModTime = osi_Time();
    alen = strlen(atargetName);	/* we want it to include the null */
    if (*atargetName == '#' || *atargetName == '%') {
	InStatus.UnixModeBits = 0644;	/* mt pt: null from "." at end */
	if (alen == 1)
	    alen++;		/* Empty string */
    } else {
	InStatus.UnixModeBits = 0755;
	alen++;			/* add in the null */
    }
    tdc = afs_GetDCache(adp, (afs_size_t) 0, &treq, &offset, &len, 1);
    volp = afs_FindVolume(&adp->fid, READ_LOCK);	/*parent is also in same vol */
    ObtainWriteLock(&adp->lock, 156);
    if (tdc)
	ObtainWriteLock(&tdc->lock, 636);
    ObtainSharedLock(&afs_xvcache, 17);	/* prevent others from creating this entry */
    /* XXX Pay attention to afs_xvcache around the whole thing!! XXX */
    do {
	tc = afs_Conn(&adp->fid, &treq, SHARED_LOCK);
	if (tc) {
	    hostp = tc->srvr->server;
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_SYMLINK);
	    if (adp->states & CForeign) {
		now = osi_Time();
		RX_AFS_GUNLOCK();
		code =
		    RXAFS_DFSSymlink(tc->id, (struct AFSFid *)&adp->fid.Fid,
				     aname, atargetName, &InStatus,
				     (struct AFSFid *)&newFid.Fid,
				     &OutFidStatus, &OutDirStatus, &CallBack,
				     &tsync);
		RX_AFS_GLOCK();
	    } else {
		RX_AFS_GUNLOCK();
		code =
		    RXAFS_Symlink(tc->id, (struct AFSFid *)&adp->fid.Fid,
				  aname, atargetName, &InStatus,
				  (struct AFSFid *)&newFid.Fid, &OutFidStatus,
				  &OutDirStatus, &tsync);
		RX_AFS_GLOCK();
	    }
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, code, &adp->fid, &treq, AFS_STATS_FS_RPCIDX_SYMLINK,
	      SHARED_LOCK, NULL));

    UpgradeSToWLock(&afs_xvcache, 40);
    if (code) {
	if (code < 0) {
	    ObtainWriteLock(&afs_xcbhash, 499);
	    afs_DequeueCallback(adp);
	    adp->states &= ~CStatd;
	    ReleaseWriteLock(&afs_xcbhash);
	    osi_dnlc_purgedp(adp);
	}
	ReleaseWriteLock(&adp->lock);
	ReleaseWriteLock(&afs_xvcache);
	if (tdc) {
	    ReleaseWriteLock(&tdc->lock);
	    afs_PutDCache(tdc);
	}
	goto done;
    }
    /* otherwise, we should see if we can make the change to the dir locally */
    if (afs_LocalHero(adp, tdc, &OutDirStatus, 1)) {
	/* we can do it locally */
	code = afs_dir_Create(tdc, aname, &newFid.Fid);
	if (code) {
	    ZapDCE(tdc);	/* surprise error -- use invalid value */
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

    /* now we're done with parent dir, create the link's entry.  Note that
     * no one can get a pointer to the new cache entry until we release 
     * the xvcache lock. */
    tvc = afs_NewVCache(&newFid, hostp);
    if (!tvc)
    {
	code = -2;
	ReleaseWriteLock(&afs_xvcache);
	goto done;
    }
    ObtainWriteLock(&tvc->lock, 157);
    ObtainWriteLock(&afs_xcbhash, 500);
    tvc->states |= CStatd;	/* have valid info */
    tvc->states &= ~CBulkFetching;

    if (adp->states & CForeign) {
	tvc->states |= CForeign;
	/* We don't have to worry about losing the callback since we're doing it 
	 * under the afs_xvcache lock actually, afs_NewVCache may drop the 
	 * afs_xvcache lock, if it calls afs_FlushVCache */
	tvc->cbExpires = CallBack.ExpirationTime + now;
	afs_QueueCallback(tvc, CBHash(CallBack.ExpirationTime), volp);
    } else {
	tvc->cbExpires = 0x7fffffff;	/* never expires, they can't change */
	/* since it never expires, we don't have to queue the callback */
    }
    ReleaseWriteLock(&afs_xcbhash);
    afs_ProcessFS(tvc, &OutFidStatus, &treq);

    if (!tvc->linkData) {
	tvc->linkData = (char *)afs_osi_Alloc(alen);
	strncpy(tvc->linkData, atargetName, alen - 1);
	tvc->linkData[alen - 1] = 0;
    }
    ReleaseWriteLock(&tvc->lock);
    ReleaseWriteLock(&afs_xvcache);
    afs_PutVCache(tvc);
    code = 0;
  done:
    afs_PutFakeStat(&fakestate);
    if (volp)
	afs_PutVolume(volp, READ_LOCK);
    code = afs_CheckCode(code, &treq, 31);
  done2:
    return code;
}

int
afs_MemHandleLink(register struct vcache *avc, struct vrequest *areq)
{
    register struct dcache *tdc;
    register char *tp, *rbuf;
    afs_size_t offset, len;
    afs_int32 tlen, alen;
    register afs_int32 code;

    AFS_STATCNT(afs_MemHandleLink);
    /* two different formats, one for links protected 644, have a "." at
     * the end of the file name, which we turn into a null.  Others, 
     * protected 755, we add a null to the end of */
    if (!avc->linkData) {
	void *addr;
	tdc = afs_GetDCache(avc, (afs_size_t) 0, areq, &offset, &len, 0);
	if (!tdc) {
	    return EIO;
	}
	/* otherwise we have the data loaded, go for it */
	if (len > 1024) {
	    afs_PutDCache(tdc);
	    return EFAULT;
	}
	if (avc->m.Mode & 0111)
	    alen = len + 1;	/* regular link */
	else
	    alen = len;		/* mt point */
	rbuf = (char *)osi_AllocLargeSpace(AFS_LRALLOCSIZ);
	ObtainReadLock(&tdc->lock);
	addr = afs_MemCacheOpen(tdc->f.inode);
	tlen = len;
	code = afs_MemReadBlk(addr, 0, rbuf, tlen);
	afs_MemCacheClose(addr);
	ReleaseReadLock(&tdc->lock);
	afs_PutDCache(tdc);
	rbuf[alen - 1] = 0;
	alen = strlen(rbuf) + 1;
	tp = afs_osi_Alloc(alen);	/* make room for terminating null */
	memcpy(tp, rbuf, alen);
	osi_FreeLargeSpace(rbuf);
	if (code != len) {
	    afs_osi_Free(tp, alen);
	    return EIO;
	}
	avc->linkData = tp;
    }
    return 0;
}

int
afs_UFSHandleLink(register struct vcache *avc, struct vrequest *areq)
{
    register struct dcache *tdc;
    register char *tp, *rbuf;
    void *tfile;
    afs_size_t offset, len;
    afs_int32 tlen, alen;
    register afs_int32 code;

    /* two different formats, one for links protected 644, have a "." at the
     * end of the file name, which we turn into a null.  Others, protected
     * 755, we add a null to the end of */
    AFS_STATCNT(afs_UFSHandleLink);
    if (!avc->linkData) {
	tdc = afs_GetDCache(avc, (afs_size_t) 0, areq, &offset, &len, 0);
	afs_Trace3(afs_iclSetp, CM_TRACE_UFSLINK, ICL_TYPE_POINTER, avc,
		   ICL_TYPE_POINTER, tdc, ICL_TYPE_OFFSET,
		   ICL_HANDLE_OFFSET(avc->m.Length));
	if (!tdc) {
	    return EIO;
	}
	/* otherwise we have the data loaded, go for it */
	if (len > 1024) {
	    afs_PutDCache(tdc);
	    return EFAULT;
	}
	if (avc->m.Mode & 0111)
	    alen = len + 1;	/* regular link */
	else
	    alen = len;		/* mt point */
	rbuf = (char *)osi_AllocLargeSpace(AFS_LRALLOCSIZ);
	tlen = len;
	ObtainReadLock(&tdc->lock);
	tfile = osi_UFSOpen(tdc->f.inode);
	code = afs_osi_Read(tfile, -1, rbuf, tlen);
	osi_UFSClose(tfile);
	ReleaseReadLock(&tdc->lock);
	afs_PutDCache(tdc);
	rbuf[alen - 1] = '\0';
	alen = strlen(rbuf) + 1;
	tp = afs_osi_Alloc(alen);	/* make room for terminating null */
	memcpy(tp, rbuf, alen);
	osi_FreeLargeSpace(rbuf);
	if (code != tlen) {
	    afs_osi_Free(tp, alen);
	    return EIO;
	}
	avc->linkData = tp;
    }
    return 0;
}

int
afs_readlink(OSI_VC_ARG(avc), auio, acred)
     OSI_VC_DECL(avc);
     struct uio *auio;
     struct AFS_UCRED *acred;
{
    register afs_int32 code;
    struct vrequest treq;
    register char *tp;
    struct afs_fakestat_state fakestat;
    OSI_VC_CONVERT(avc);

    AFS_STATCNT(afs_readlink);
    afs_Trace1(afs_iclSetp, CM_TRACE_READLINK, ICL_TYPE_POINTER, avc);
    if ((code = afs_InitReq(&treq, acred)))
	return code;
    afs_InitFakeStat(&fakestat);
    code = afs_EvalFakeStat(&avc, &fakestat, &treq);
    if (code)
	goto done;
    code = afs_VerifyVCache(avc, &treq);
    if (code)
	goto done;
    if (vType(avc) != VLNK) {
	code = EINVAL;
	goto done;
    }
    ObtainWriteLock(&avc->lock, 158);
    code = afs_HandleLink(avc, &treq);
    /* finally uiomove it to user-land */
    if (code == 0) {
	tp = avc->linkData;
	if (tp)
	    AFS_UIOMOVE(tp, strlen(tp), UIO_READ, auio, code);
	else {
	    code = EIO;
	}
    }
    ReleaseWriteLock(&avc->lock);
  done:
    afs_PutFakeStat(&fakestat);
    code = afs_CheckCode(code, &treq, 32);
    return code;
}
