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
 * FetchWholeEnchilada
 * afsremove
 * afs_remove
 * afs_newname
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


extern afs_rwlock_t afs_xvcache;
extern afs_rwlock_t afs_xcbhash;


static void
FetchWholeEnchilada(struct vcache *avc, struct vrequest *areq)
{
    afs_int32 nextChunk;
    struct dcache *tdc;
    afs_size_t pos, offset, len;

    AFS_STATCNT(FetchWholeEnchilada);
    if ((avc->f.states & CStatd) == 0)
	return;			/* don't know size */
    for (nextChunk = 0; nextChunk < 1024; nextChunk++) {	/* sanity check on N chunks */
	pos = AFS_CHUNKTOBASE(nextChunk);
	if (pos >= avc->f.m.Length)
	    return;		/* all done */
	tdc = afs_GetDCache(avc, pos, areq, &offset, &len, 0);
	if (!tdc)
	    return;
	afs_PutDCache(tdc);
    }
}

int
afsremove(struct vcache *adp, struct dcache *tdc,
	  struct vcache *tvc, char *aname, afs_ucred_t *acred,
	  struct vrequest *treqp)
{
    afs_int32 code = 0;
    struct afs_conn *tc;
    struct AFSFetchStatus OutDirStatus;
    struct AFSVolSync tsync;
    struct rx_connection *rxconn;
    XSTATS_DECLS;
    if (!AFS_IS_DISCONNECTED) {
        do {
	  tc = afs_Conn(&adp->f.fid, treqp, SHARED_LOCK, &rxconn);
	    if (tc) {
	        XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_REMOVEFILE);
	        RX_AFS_GUNLOCK();
	        code =
		    RXAFS_RemoveFile(rxconn, (struct AFSFid *)&adp->f.fid.Fid,
		  		     aname, &OutDirStatus, &tsync);
	        RX_AFS_GLOCK();
	        XSTATS_END_TIME;
	    } else
	        code = -1;
        } while (afs_Analyze
	         (tc, rxconn, code, &adp->f.fid, treqp, AFS_STATS_FS_RPCIDX_REMOVEFILE,
	          SHARED_LOCK, NULL));
    }

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
	    adp->f.states &= ~CStatd;
	    ReleaseWriteLock(&afs_xcbhash);
	    osi_dnlc_purgedp(adp);
	}
	ReleaseWriteLock(&adp->lock);
	code = afs_CheckCode(code, treqp, 21);
	return code;
    }
    if (tdc)
	UpgradeSToWLock(&tdc->lock, 637);
    if (AFS_IS_DISCON_RW || afs_LocalHero(adp, tdc, &OutDirStatus, 1)) {
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
	if (afs_mariner)
	    afs_MarinerLog("store$Removing", tvc);
#ifdef AFS_BOZONLOCK_ENV
	afs_BozonLock(&tvc->pvnLock, tvc);
	/* Since afs_TryToSmush will do a pvn_vptrunc */
#endif
	ObtainWriteLock(&tvc->lock, 141);
	/* note that callback will be broken on the deleted file if there are
	 * still >0 links left to it, so we'll get the stat right */
	tvc->f.m.LinkCount--;
	tvc->f.states &= ~CUnique;	/* For the dfs xlator */
	if (tvc->f.m.LinkCount == 0 && !osi_Active(tvc)) {
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
afs_remove(OSI_VC_DECL(adp), char *aname, afs_ucred_t *acred)
{
    struct vrequest *treq = NULL;
    struct dcache *tdc;
    struct VenusFid unlinkFid;
    afs_int32 code;
    struct vcache *tvc;
    afs_size_t offset, len;
    struct afs_fakestat_state fakestate;
    OSI_VC_CONVERT(adp);

    AFS_STATCNT(afs_remove);
    afs_Trace2(afs_iclSetp, CM_TRACE_REMOVE, ICL_TYPE_POINTER, adp,
	       ICL_TYPE_STRING, aname);


    if ((code = afs_CreateReq(&treq, acred))) {
	return code;
    }

    afs_InitFakeStat(&fakestate);
    AFS_DISCON_LOCK();
    code = afs_EvalFakeStat(&adp, &fakestate, treq);
    if (code)
	goto done;

    /* Check if this is dynroot */
    if (afs_IsDynroot(adp)) {
	code = afs_DynrootVOPRemove(adp, acred, aname);
	goto done;
    }
    if (afs_IsDynrootMount(adp)) {
	code = ENOENT;
	goto done;
    }

    if (strlen(aname) > AFSNAMEMAX) {
	code = ENAMETOOLONG;
	goto done;
    }
  tagain:
    code = afs_VerifyVCache(adp, treq);
    tvc = NULL;
    if (code) {
	code = afs_CheckCode(code, treq, 23);
	goto done;
    }

    /** If the volume is read-only, return error without making an RPC to the
      * fileserver
      */
    if (adp->f.states & CRO) {
	code = EROFS;
	goto done;
    }

    /* If we're running disconnected without logging, go no further... */
    if (AFS_IS_DISCONNECTED && !AFS_IS_DISCON_RW) {
        code = ENETDOWN;
	goto done;
    }
    
    tdc = afs_GetDCache(adp, (afs_size_t) 0, treq, &offset, &len, 1);	/* test for error below */
    ObtainWriteLock(&adp->lock, 142);
    if (tdc)
	ObtainSharedLock(&tdc->lock, 638);

    /*
     * Make sure that the data in the cache is current. We may have
     * received a callback while we were waiting for the write lock.
     */
    if (!(adp->f.states & CStatd)
	|| (tdc && !hsame(adp->f.m.DataVersion, tdc->f.versionNo))) {
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
     * done the work.
     */
    if (!tvc)
	if (tdc) {
	    code = afs_dir_Lookup(tdc, aname, &unlinkFid.Fid);
	    if (code == 0) {
		afs_int32 cached = 0;

		unlinkFid.Cell = adp->f.fid.Cell;
		unlinkFid.Fid.Volume = adp->f.fid.Fid.Volume;
		if (unlinkFid.Fid.Unique == 0) {
		    tvc =
			afs_LookupVCache(&unlinkFid, treq, &cached, adp,
					 aname);
		} else {
		    ObtainReadLock(&afs_xvcache);
		    tvc = afs_FindVCache(&unlinkFid, 0, DO_STATS);
		    ReleaseReadLock(&afs_xvcache);
		}
	    }
	}

    if (AFS_IS_DISCON_RW) {
	if (!adp->f.shadow.vnode && !(adp->f.ddirty_flags & VDisconCreate)) {
    	    /* Make shadow copy of parent dir. */
	    afs_MakeShadowDir(adp, tdc);
	}

	/* Can't hold a dcache lock whilst we're getting a vcache one */
	if (tdc)
	    ReleaseSharedLock(&tdc->lock);

        /* XXX - We're holding adp->lock still, and we've got no 
	 * guarantee about whether the ordering matches the lock hierarchy */
	ObtainWriteLock(&tvc->lock, 713);

	/* If we were locally created, then we don't need to do very
	 * much beyond ensuring that we don't exist anymore */	
    	if (tvc->f.ddirty_flags & VDisconCreate) {
	    afs_DisconRemoveDirty(tvc);
	} else {
	    /* Add removed file vcache to dirty list. */
	    afs_DisconAddDirty(tvc, VDisconRemove, 1);
        }
	adp->f.m.LinkCount--;
	ReleaseWriteLock(&tvc->lock);
	if (tdc)
	    ObtainSharedLock(&tdc->lock, 714);
     }

    if (tvc && osi_Active(tvc)) {
	/* about to delete whole file, prefetch it first */
	ReleaseWriteLock(&adp->lock);
	if (tdc)
	    ReleaseSharedLock(&tdc->lock);
	ObtainWriteLock(&tvc->lock, 143);
	FetchWholeEnchilada(tvc, treq);
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
	&& !(tvc->f.states & CUnlinked)) {
#else
    if (tvc && VREFCOUNT_GT(tvc, 1) && tvc->opens > 0
	&& !(tvc->f.states & CUnlinked)) {
#endif
	char *unlname = afs_newname();

	ReleaseWriteLock(&adp->lock);
	if (tdc)
	    ReleaseSharedLock(&tdc->lock);
	code = afsrename(adp, aname, adp, unlname, acred, treq);
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
	    tvc->f.states |= CUnlinked;
	    /* if rename succeeded, remove should not */
	    ObtainWriteLock(&tvc->lock, 715);
	    if (tvc->f.ddirty_flags & VDisconRemove) {
		tvc->f.ddirty_flags &= ~VDisconRemove;
	    }
	    ReleaseWriteLock(&tvc->lock);
	} else {
	    osi_FreeSmallSpace(unlname);
	}
	if (tdc)
	    afs_PutDCache(tdc);
	afs_PutVCache(tvc);
    } else {
	code = afsremove(adp, tdc, tvc, aname, acred, treq);
    }
    done:
    afs_PutFakeStat(&fakestate);
#if !defined(AFS_DARWIN80_ENV) && !defined(UKERNEL)
    /* we can't track by thread, it's not exported in the KPI; only do
       this on !macos */
    osi_Assert(!WriteLocked(&adp->lock) || (adp->lock.pid_writer != MyPidxx));
#endif
    AFS_DISCON_UNLOCK();
    afs_DestroyReq(treq);
    return code;
}


/* afs_remunlink -- This tries to delete the file at the server after it has
 *     been renamed when unlinked locally but now has been finally released.
 *
 * CAUTION -- may be called with avc unheld. */

int
afs_remunlink(struct vcache *avc, int doit)
{
    afs_ucred_t *cred;
    char *unlname;
    struct vcache *adp;
    struct VenusFid dirFid;
    struct dcache *tdc;
    afs_int32 code = 0;

    if (NBObtainWriteLock(&avc->lock, 423))
	return 0;
#if defined(AFS_DARWIN80_ENV)
    if (vnode_get(AFSTOV(avc))) {
	ReleaseWriteLock(&avc->lock);
	return 0;
    }
#endif

    if (avc->mvid && (doit || (avc->f.states & CUnlinkedDel))) {
	struct vrequest *treq = NULL;

	if ((code = afs_CreateReq(&treq, avc->uncred))) {
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
	    AFS_FAST_HOLD(avc);
#endif

	    /* We'll only try this once. If it fails, just release the vnode.
	     * Clear after doing hold so that NewVCache doesn't find us yet.
	     */
	    avc->f.states &= ~(CUnlinked | CUnlinkedDel);

	    ReleaseWriteLock(&avc->lock);

	    dirFid.Cell = avc->f.fid.Cell;
	    dirFid.Fid.Volume = avc->f.fid.Fid.Volume;
	    dirFid.Fid.Vnode = avc->f.parent.vnode;
	    dirFid.Fid.Unique = avc->f.parent.unique;
	    adp = afs_GetVCache(&dirFid, treq, NULL, NULL);

	    if (adp) {
		tdc = afs_FindDCache(adp, (afs_size_t) 0);
		ObtainWriteLock(&adp->lock, 159);
		if (tdc)
		    ObtainSharedLock(&tdc->lock, 639);

		/* afsremove releases the adp & tdc locks, and does vn_rele(avc) */
		code = afsremove(adp, tdc, avc, unlname, cred, treq);
		afs_PutVCache(adp);
	    } else {
		/* we failed - and won't be back to try again. */
		afs_PutVCache(avc);
	    }
	    osi_FreeSmallSpace(unlname);
	    crfree(cred);
	    afs_DestroyReq(treq);
	}
    } else {
#if defined(AFS_DARWIN80_ENV)
	vnode_put(AFSTOV(avc));
#endif
	ReleaseWriteLock(&avc->lock);
    }

    return code;
}
