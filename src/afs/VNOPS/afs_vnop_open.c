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
 * afs_open
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



/* given a vnode ptr, open flags and credentials, open the file.  No access
 * checks are done here, instead they're done by afs_create or afs_access,
 * both called by the vn_open call.
 */
int
#ifdef AFS_SGI64_ENV
afs_open(bhv_desc_t * bhv, struct vcache **avcp, afs_int32 aflags,
	 afs_ucred_t *acred)
#else
afs_open(struct vcache **avcp, afs_int32 aflags, afs_ucred_t *acred)
#endif
{
    afs_int32 code;
    struct vrequest *treq = NULL;
    struct vcache *tvc;
    int writing;
    struct afs_fakestat_state fakestate;

    AFS_STATCNT(afs_open);
    if ((code = afs_CreateReq(&treq, acred)))
	return code;
#ifdef AFS_SGI64_ENV
    /* avcpp can be, but is not necesarily, bhp's vnode. */
    tvc = VTOAFS(BHV_TO_VNODE(bhv));
#else
    tvc = *avcp;
#endif
    afs_Trace2(afs_iclSetp, CM_TRACE_OPEN, ICL_TYPE_POINTER, tvc,
	       ICL_TYPE_INT32, aflags);
    afs_InitFakeStat(&fakestate);

    AFS_DISCON_LOCK();

    code = afs_EvalFakeStat(&tvc, &fakestate, treq);
    if (code)
	goto done;
    code = afs_VerifyVCache(tvc, treq);
    if (code)
	goto done;

    ObtainReadLock(&tvc->lock);

    if (AFS_IS_DISCONNECTED && (afs_DCacheMissingChunks(tvc) != 0)) {
       ReleaseReadLock(&tvc->lock);
       /* printf("Network is down in afs_open: missing chunks\n"); */
       code = ENETDOWN;
       goto done;
    }

    ReleaseReadLock(&tvc->lock);

    if (aflags & (FWRITE | FTRUNC))
	writing = 1;
    else
	writing = 0;
    if (vType(tvc) == VDIR) {
	/* directory */
	if (writing) {
	    code = EISDIR;
	    goto done;
	} else {
	    if (!afs_AccessOK
		(tvc, ((tvc->f.states & CForeign) ? PRSFS_READ : PRSFS_LOOKUP),
		 treq, CHECK_MODE_BITS)) {
		code = EACCES;
		/* printf("afs_Open: no access for dir\n"); */
		goto done;
	    }
	}
    } else {
#ifdef	AFS_SUN5_ENV
	if (AFS_NFSXLATORREQ(acred) && (aflags & FREAD)) {
	    if (!afs_AccessOK
		(tvc, PRSFS_READ, treq,
		 CHECK_MODE_BITS | CMB_ALLOW_EXEC_AS_READ)) {
		code = EACCES;
		goto done;
	    }
	}
#endif
#ifdef	AFS_AIX41_ENV
	if (aflags & FRSHARE) {
	    /*
	     * Hack for AIX 4.1:
	     *  Apparently it is possible for a file to get mapped without
	     *  either VNOP_MAP or VNOP_RDWR being called, if (1) it is a
	     *  sharable library, and (2) it has already been loaded.  We must
	     *  ensure that the credp is up to date.  We detect the situation
	     *  by checking for O_RSHARE at open time.
	     */
	    /*
	     * We keep the caller's credentials since an async daemon will
	     * handle the request at some point. We assume that the same
	     * credentials will be used.
	     */
	    ObtainWriteLock(&tvc->lock, 140);
	    if (!tvc->credp || (tvc->credp != acred)) {
		crhold(acred);
		if (tvc->credp) {
		    struct ucred *crp = tvc->credp;
		    tvc->credp = NULL;
		    crfree(crp);
		}
		tvc->credp = acred;
	    }
	    ReleaseWriteLock(&tvc->lock);
	}
#endif
	/* normal file or symlink */
	osi_FlushText(tvc);	/* only needed to flush text if text locked last time */
#ifdef AFS_BOZONLOCK_ENV
	afs_BozonLock(&tvc->pvnLock, tvc);
#endif
	osi_FlushPages(tvc, acred);
#ifdef AFS_BOZONLOCK_ENV
	afs_BozonUnlock(&tvc->pvnLock, tvc);
#endif
    }
    /* set date on file if open in O_TRUNC mode */
    if (aflags & FTRUNC) {
	/* this fixes touch */
	ObtainWriteLock(&tvc->lock, 123);
	tvc->f.m.Date = osi_Time();
	tvc->f.states |= CDirty;
	ReleaseWriteLock(&tvc->lock);
    }
    ObtainReadLock(&tvc->lock);
    if (writing)
	tvc->execsOrWriters++;
    tvc->opens++;
#if defined(AFS_SGI_ENV) || defined (AFS_LINUX26_ENV)
    if (writing && tvc->cred == NULL) {
	crhold(acred);
	tvc->cred = acred;
    }
#endif
    ReleaseReadLock(&tvc->lock);
    if ((afs_preCache != 0) && (writing == 0) && (vType(tvc) != VDIR) && 
	(!afs_BBusy())) {
	struct dcache *tdc;
	afs_size_t offset, len;

	tdc = afs_GetDCache(tvc, 0, treq, &offset, &len, 1);
	if (tdc) {
	    ObtainSharedLock(&tdc->mflock, 865);
	    if (!(tdc->mflags & DFFetchReq)) {
		struct brequest *bp;

		/* start the daemon (may already be running, however) */
		UpgradeSToWLock(&tdc->mflock, 666);
		tdc->mflags |= DFFetchReq;  /* guaranteed to be cleared by BKG or
					       GetDCache */
		/* last parm (1) tells bkg daemon to do an afs_PutDCache when it
		   is done, since we don't want to wait for it to finish before
		   doing so ourselves.
		*/
		bp = afs_BQueue(BOP_FETCH, tvc, B_DONTWAIT, 0, acred,
				(afs_size_t) 0, (afs_size_t) 1, tdc,
				(void *)0, (void *)0);
		if (!bp) {
		    tdc->mflags &= ~DFFetchReq;
		}
		ReleaseWriteLock(&tdc->mflock);
	    } else {
		ReleaseSharedLock(&tdc->mflock);
	    }
	}
    }	
  done:
    afs_PutFakeStat(&fakestate);
    AFS_DISCON_UNLOCK();

    code = afs_CheckCode(code, treq, 4);	/* avoid AIX -O bug */
    afs_DestroyReq(treq);

    afs_Trace2(afs_iclSetp, CM_TRACE_OPEN, ICL_TYPE_POINTER, tvc,
	       ICL_TYPE_INT32, 999999);

    return code;
}
