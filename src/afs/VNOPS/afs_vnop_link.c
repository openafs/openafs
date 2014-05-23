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
 * afs_link
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

extern afs_rwlock_t afs_xcbhash;

/* Note that we don't set CDirty here, this is OK because the link
 * RPC is called synchronously. */

int
#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)
afs_link(OSI_VC_DECL(adp), struct vcache *avc, char *aname, 
	 afs_ucred_t *acred)
#else
afs_link(struct vcache *avc, OSI_VC_DECL(adp), char *aname, 
	 afs_ucred_t *acred)
#endif
{
    struct vrequest *treq = NULL;
    struct dcache *tdc;
    afs_int32 code;
    struct afs_conn *tc;
    afs_size_t offset, len;
    struct AFSFetchStatus *OutFidStatus, *OutDirStatus;
    struct AFSVolSync tsync;
    struct afs_fakestat_state vfakestate, dfakestate;
    struct rx_connection *rxconn;
    XSTATS_DECLS;
    OSI_VC_CONVERT(adp);

    AFS_STATCNT(afs_link);
    afs_Trace3(afs_iclSetp, CM_TRACE_LINK, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_POINTER, avc, ICL_TYPE_STRING, aname);

    OutFidStatus = osi_AllocSmallSpace(sizeof(struct AFSFetchStatus));
    OutDirStatus = osi_AllocSmallSpace(sizeof(struct AFSFetchStatus));

    /* create a hard link; new entry is aname in dir adp */
    if ((code = afs_CreateReq(&treq, acred)))
	goto done2;

    afs_InitFakeStat(&vfakestate);
    afs_InitFakeStat(&dfakestate);

    AFS_DISCON_LOCK();

    code = afs_EvalFakeStat(&avc, &vfakestate, treq);
    if (code)
	goto done;
    code = afs_EvalFakeStat(&adp, &dfakestate, treq);
    if (code)
	goto done;

    if (avc->f.fid.Cell != adp->f.fid.Cell
	|| avc->f.fid.Fid.Volume != adp->f.fid.Fid.Volume) {
	code = EXDEV;
	goto done;
    }
    if (strlen(aname) > AFSNAMEMAX) {
	code = ENAMETOOLONG;
	goto done;
    }
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

    if (AFS_IS_DISCONNECTED) {
        code = ENETDOWN;
        goto done;
    }

    tdc = afs_GetDCache(adp, (afs_size_t) 0, treq, &offset, &len, 1);	/* test for error below */
    ObtainWriteLock(&adp->lock, 145);
    do {
	tc = afs_Conn(&adp->f.fid, treq, SHARED_LOCK, &rxconn);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_LINK);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_Link(rxconn, (struct AFSFid *)&adp->f.fid.Fid, aname,
			   (struct AFSFid *)&avc->f.fid.Fid, OutFidStatus,
			   OutDirStatus, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;

	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, rxconn, code, &adp->f.fid, treq, AFS_STATS_FS_RPCIDX_LINK,
	      SHARED_LOCK, NULL));

    if (code) {
	if (tdc)
	    afs_PutDCache(tdc);
	if (code < 0) {
	    ObtainWriteLock(&afs_xcbhash, 492);
	    afs_DequeueCallback(adp);
	    adp->f.states &= ~CStatd;
	    ReleaseWriteLock(&afs_xcbhash);
	    osi_dnlc_purgedp(adp);
	}
	ReleaseWriteLock(&adp->lock);
	goto done;
    }
    if (tdc)
	ObtainWriteLock(&tdc->lock, 635);
    if (afs_LocalHero(adp, tdc, OutDirStatus, 1)) {
	/* we can do it locally */
	ObtainWriteLock(&afs_xdcache, 290);
	code = afs_dir_Create(tdc, aname, &avc->f.fid.Fid);
	ReleaseWriteLock(&afs_xdcache);
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
    ObtainWriteLock(&avc->lock, 146);	/* correct link count */

    /* we could lock both dir and file; since we get the new fid
     * status back, you'd think we could put it in the cache status
     * entry at that point.  Note that if we don't lock the file over
     * the rpc call, we have no guarantee that the status info
     * returned in ustat is the most recent to store in the file's
     * cache entry */

    ObtainWriteLock(&afs_xcbhash, 493);
    afs_DequeueCallback(avc);
    avc->f.states &= ~CStatd;	/* don't really know new link count */
    ReleaseWriteLock(&afs_xcbhash);
    if (avc->f.fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
	osi_dnlc_purgedp(avc);
    ReleaseWriteLock(&avc->lock);
    code = 0;
  done:
    code = afs_CheckCode(code, treq, 24);
    afs_DestroyReq(treq);
    afs_PutFakeStat(&vfakestate);
    afs_PutFakeStat(&dfakestate);
    AFS_DISCON_UNLOCK();
  done2:
    osi_FreeSmallSpace(OutFidStatus);
    osi_FreeSmallSpace(OutDirStatus);
    return code;
}
