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

RCSID
    ("$Header$");

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
afs_link(OSI_VC_ARG(adp), avc, aname, acred)
#else
afs_link(avc, OSI_VC_ARG(adp), aname, acred)
#endif
     OSI_VC_DECL(adp);
     struct vcache *avc;
     char *aname;
     struct AFS_UCRED *acred;
{
    struct vrequest treq;
    register struct dcache *tdc;
    register afs_int32 code;
    register struct conn *tc;
    afs_size_t offset, len;
    struct AFSFetchStatus OutFidStatus, OutDirStatus;
    struct AFSVolSync tsync;
    struct afs_fakestat_state vfakestate, dfakestate;
    XSTATS_DECLS;
    OSI_VC_CONVERT(adp);

    AFS_STATCNT(afs_link);
    afs_Trace3(afs_iclSetp, CM_TRACE_LINK, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_POINTER, avc, ICL_TYPE_STRING, aname);
    /* create a hard link; new entry is aname in dir adp */
    if ((code = afs_InitReq(&treq, acred)))
	goto done2;

    afs_InitFakeStat(&vfakestate);
    afs_InitFakeStat(&dfakestate);
    code = afs_EvalFakeStat(&avc, &vfakestate, &treq);
    if (code)
	goto done;
    code = afs_EvalFakeStat(&adp, &dfakestate, &treq);
    if (code)
	goto done;

    if (avc->fid.Cell != adp->fid.Cell
	|| avc->fid.Fid.Volume != adp->fid.Fid.Volume) {
	code = EXDEV;
	goto done;
    }
    if (strlen(aname) > AFSNAMEMAX) {
	code = ENAMETOOLONG;
	goto done;
    }
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

    tdc = afs_GetDCache(adp, (afs_size_t) 0, &treq, &offset, &len, 1);	/* test for error below */
    ObtainWriteLock(&adp->lock, 145);
#ifdef  DISCONN
    if (LOG_OPERATIONS(discon_state)) {
        struct VenusFid destFid;
        long    cur_op;
        /*
	**  Make sure this link is in the same directory.  AFS only
        **  supports hard links that are in the same directory
        */

        if((avc->parentVnode != adp->fid.Fid.Vnode) ||
           (avc->parentUnique != adp->fid.Fid.Unique))
           {
           ReleaseWriteLock(&adp->lock);
           code = EXDEV;
           goto done;
           }

        /* make sure we have the dcache or else we can't do this */

        if(!tdc) {
            ReleaseWriteLock(&adp->lock);
            return ENETDOWN;
            }       

        /* make sure that there is not already of file with the dest name */
        code = dir_Lookup(&tdc->f.inode, aname, &destFid.Fid);
        if(!code) {
            afs_PutDCache(tdc);
            ReleaseWriteLock(&adp->lock);
            return EEXIST;
        }
        /* make sure that the file being linked to is not a directory */
        if (vType(avc) == VDIR) {
            afs_PutDCache(tdc);
            ReleaseWriteLock(&adp->lock);
            return(EPERM);
            }


        /*
	** everything is ok, lets go ahead and record our desire to
        ** make this link, and then fall through to cleanup the
        ** local cached copies.
        */

        ObtainWriteLock(&avc->lock);
        cur_op = log_dis_link(avc, adp, aname);

        /* mark the vcaches to lock them into the cache */

        avc->dflags |= (KEEP_VC| VC_DIRTY);
        avc->last_mod_op = cur_op;
        afs_VindexFlags[avc->index] |= KEEP_VC;
        ReleaseWriteLock(&avc->lock);

        adp->last_mod_op = cur_op;
        adp->dflags |= (KEEP_VC| VC_DIRTY);
        afs_VindexFlags[adp->index] |= KEEP_VC;

        /* mark the dcaches that need to be kept in the cache */
        tdc->f.dflags |= KEEP_DC;
        afs_indexFlags[tdc->index] |= IFFKeep_DC;
        tdc->flags |= DFEntryMod;

        code = 0;


        }

    else
#endif  /* DISCONN */
    do {
	tc = afs_Conn(&adp->fid, &treq, SHARED_LOCK);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_LINK);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_Link(tc->id, (struct AFSFid *)&adp->fid.Fid, aname,
			   (struct AFSFid *)&avc->fid.Fid, &OutFidStatus,
			   &OutDirStatus, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;

	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, code, &adp->fid, &treq, AFS_STATS_FS_RPCIDX_LINK,
	      SHARED_LOCK, NULL));

    if (code) {
	if (tdc)
	    afs_PutDCache(tdc);
	if (code < 0) {
	    ObtainWriteLock(&afs_xcbhash, 492);
	    afs_DequeueCallback(adp);
	    adp->states &= ~CStatd;
	    ReleaseWriteLock(&afs_xcbhash);
	    osi_dnlc_purgedp(adp);
	}
	ReleaseWriteLock(&adp->lock);
	goto done;
    }
    if (tdc)
	ObtainWriteLock(&tdc->lock, 635);
    if (afs_LocalHero(adp, tdc, &OutDirStatus, 1)) {
	/* we can do it locally */
	code = afs_dir_Create(tdc, aname, &avc->fid.Fid);
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
#ifdef  DISCONN
    if (LOG_OPERATIONS(discon_state)) {
        avc->m.LinkCount++;
        avc->dflags |= VC_DIRTY;
    } else {
	avc->states     &= ~CStatd;     /* don't really know new link count */
        avc->dflags |= VC_DIRTY;
    }
#else   /* DISCONN */
    avc->states &= ~CStatd;     /* don't really know new link count */
#endif  /*DISCONN */
    ReleaseWriteLock(&afs_xcbhash);
    if (avc->fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
	osi_dnlc_purgedp(avc);
    ReleaseWriteLock(&avc->lock);
    code = 0;
  done:
    code = afs_CheckCode(code, &treq, 24);
    afs_PutFakeStat(&vfakestate);
    afs_PutFakeStat(&dfakestate);
  done2:
    return code;
}
