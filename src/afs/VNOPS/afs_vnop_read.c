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
 * afs_MemRead
 * afs_PrefetchChunk
 * afs_UFSRead
 * 
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/VNOPS/afs_vnop_read.c,v 1.26.2.4 2006/02/21 04:47:08 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"
#include "afs/afs_osi.h"


extern char afs_zeros[AFS_ZEROS];

afs_int32 maxIHint;
afs_int32 nihints;		/* # of above actually in-use */
afs_int32 usedihint;


/* Imported variables */
extern afs_rwlock_t afs_xdcache;
extern unsigned char *afs_indexFlags;
extern afs_hyper_t *afs_indexTimes;	/* Dcache entry Access times */
extern afs_hyper_t afs_indexCounter;	/* Fake time for marking index */


/* Forward declarations */
void afs_PrefetchChunk(struct vcache *avc, struct dcache *adc,
		       struct AFS_UCRED *acred, struct vrequest *areq);

int
afs_MemRead(register struct vcache *avc, struct uio *auio,
	    struct AFS_UCRED *acred, daddr_t albn, struct buf **abpp,
	    int noLock)
{
    afs_size_t totalLength;
    afs_size_t transferLength;
    afs_size_t filePos;
    afs_size_t offset, len, tlen;
    afs_int32 trimlen;
    struct dcache *tdc = 0;
    afs_int32 error, trybusy = 1;
#ifdef AFS_DARWIN80_ENV
    uio_t tuiop = NULL;
#else
    struct uio tuio;
    struct uio *tuiop = &tuio;
    struct iovec *tvec;
#endif
    afs_int32 code;
    struct vrequest treq;

    AFS_STATCNT(afs_MemRead);
    if (avc->vc_error)
	return EIO;

    /* check that we have the latest status info in the vnode cache */
    if ((code = afs_InitReq(&treq, acred)))
	return code;
    if (!noLock) {
	code = afs_VerifyVCache(avc, &treq);
	if (code) {
	    code = afs_CheckCode(code, &treq, 8);	/* failed to get it */
	    return code;
	}
    }
#ifndef	AFS_VM_RDWR_ENV
    if (AFS_NFSXLATORREQ(acred)) {
	if (!afs_AccessOK
	    (avc, PRSFS_READ, &treq,
	     CHECK_MODE_BITS | CMB_ALLOW_EXEC_AS_READ)) {
	    return afs_CheckCode(EACCES, &treq, 9);
	}
    }
#endif

#ifndef AFS_DARWIN80_ENV
    tvec = (struct iovec *)osi_AllocSmallSpace(sizeof(struct iovec));
#endif
    totalLength = AFS_UIO_RESID(auio);
    filePos = AFS_UIO_OFFSET(auio);
    afs_Trace4(afs_iclSetp, CM_TRACE_READ, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(filePos), ICL_TYPE_INT32,
	       totalLength, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->m.Length));
    error = 0;
    transferLength = 0;
    if (!noLock)
	ObtainReadLock(&avc->lock);
#if	defined(AFS_TEXT_ENV) && !defined(AFS_VM_RDWR_ENV)
    if (avc->flushDV.high == AFS_MAXDV && avc->flushDV.low == AFS_MAXDV) {
	hset(avc->flushDV, avc->m.DataVersion);
    }
#endif

    /*
     * Locks held:
     * avc->lock(R)
     */
    while (totalLength > 0) {
	/* read all of the cached info */
	if (filePos >= avc->m.Length)
	    break;		/* all done */
	if (noLock) {
	    if (tdc) {
		ReleaseReadLock(&tdc->lock);
		afs_PutDCache(tdc);
	    }
	    tdc = afs_FindDCache(avc, filePos);
	    if (tdc) {
		ObtainReadLock(&tdc->lock);
		offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
		len = tdc->f.chunkBytes - offset;
	    }
	} else {
	    /* a tricky question: does the presence of the DFFetching flag
	     * mean that we're fetching the latest version of the file?  No.
	     * The server could update the file as soon as the fetch responsible
	     * for the setting of the DFFetching flag completes.
	     * 
	     * However, the presence of the DFFetching flag (visible under
	     * a dcache read lock since it is set and cleared only under a
	     * dcache write lock) means that we're fetching as good a version
	     * as was known to this client at the time of the last call to
	     * afs_VerifyVCache, since the latter updates the stat cache's
	     * m.DataVersion field under a vcache write lock, and from the
	     * time that the DFFetching flag goes on in afs_GetDCache (before
	     * the fetch starts), to the time it goes off (after the fetch
	     * completes), afs_GetDCache keeps at least a read lock on the
	     * vcache entry.
	     * 
	     * This means that if the DFFetching flag is set, we can use that
	     * data for any reads that must come from the current version of
	     * the file (current == m.DataVersion).
	     * 
	     * Another way of looking at this same point is this: if we're
	     * fetching some data and then try do an afs_VerifyVCache, the
	     * VerifyVCache operation will not complete until after the
	     * DFFetching flag is turned off and the dcache entry's f.versionNo
	     * field is updated.
	     * 
	     * Note, by the way, that if DFFetching is set,
	     * m.DataVersion > f.versionNo (the latter is not updated until
	     * after the fetch completes).
	     */
	    if (tdc) {
		ReleaseReadLock(&tdc->lock);
		afs_PutDCache(tdc);	/* before reusing tdc */
	    }
	    tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 2);
	    ObtainReadLock(&tdc->lock);
	    /* now, first try to start transfer, if we'll need the data.  If
	     * data already coming, we don't need to do this, obviously.  Type
	     * 2 requests never return a null dcache entry, btw.
	     */
	    if (!(tdc->dflags & DFFetching)
		&& !hsame(avc->m.DataVersion, tdc->f.versionNo)) {
		/* have cache entry, it is not coming in now,
		 * and we'll need new data */
	      tagain:
		if (trybusy && !afs_BBusy()) {
		    struct brequest *bp;
		    /* daemon is not busy */
		    ObtainSharedLock(&tdc->mflock, 665);
		    if (!(tdc->mflags & DFFetchReq)) {
			/* start the daemon (may already be running, however) */
			UpgradeSToWLock(&tdc->mflock, 666);
			tdc->mflags |= DFFetchReq;
			bp = afs_BQueue(BOP_FETCH, avc, B_DONTWAIT, 0, acred,
					(afs_size_t) filePos, (afs_size_t) 0,
					tdc);
			if (!bp) {
			    tdc->mflags &= ~DFFetchReq;
			    trybusy = 0;	/* Avoid bkg daemon since they're too busy */
			    ReleaseWriteLock(&tdc->mflock);
			    goto tagain;
			}
			ConvertWToSLock(&tdc->mflock);
			/* don't use bp pointer! */
		    }
		    code = 0;
		    ConvertSToRLock(&tdc->mflock);
		    while (!code && tdc->mflags & DFFetchReq) {
			afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAIT,
				   ICL_TYPE_STRING, __FILE__, ICL_TYPE_INT32,
				   __LINE__, ICL_TYPE_POINTER, tdc,
				   ICL_TYPE_INT32, tdc->dflags);
			/* don't need waiting flag on this one */
			ReleaseReadLock(&tdc->mflock);
			ReleaseReadLock(&tdc->lock);
			ReleaseReadLock(&avc->lock);
			code = afs_osi_SleepSig(&tdc->validPos);
			ObtainReadLock(&avc->lock);
			ObtainReadLock(&tdc->lock);
			ObtainReadLock(&tdc->mflock);
		    }
		    ReleaseReadLock(&tdc->mflock);
		    if (code) {
			error = code;
			break;
		    }
		}
	    }
	    /* now data may have started flowing in (if DFFetching is on).  If
	     * data is now streaming in, then wait for some interesting stuff.
	     */
	    code = 0;
	    while (!code && (tdc->dflags & DFFetching)
		   && tdc->validPos <= filePos) {
		/* too early: wait for DFFetching flag to vanish,
		 * or data to appear */
		afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAIT, ICL_TYPE_STRING,
			   __FILE__, ICL_TYPE_INT32, __LINE__,
			   ICL_TYPE_POINTER, tdc, ICL_TYPE_INT32,
			   tdc->dflags);
		ReleaseReadLock(&tdc->lock);
		ReleaseReadLock(&avc->lock);
		code = afs_osi_SleepSig(&tdc->validPos);
		ObtainReadLock(&avc->lock);
		ObtainReadLock(&tdc->lock);
	    }
	    if (code) {
		error = code;
		break;
	    }
	    /* fetching flag gone, data is here, or we never tried 
	     * (BBusy for instance) */
	    if (tdc->dflags & DFFetching) {
		/* still fetching, some new data is here: 
		 * compute length and offset */
		offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
		len = tdc->validPos - filePos;
	    } else {
		/* no longer fetching, verify data version 
		 * (avoid new GetDCache call) */
		if (hsame(avc->m.DataVersion, tdc->f.versionNo)
		    && ((len = tdc->validPos - filePos) > 0)) {
		    offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
		} else {
		    /* don't have current data, so get it below */
		    afs_Trace3(afs_iclSetp, CM_TRACE_VERSIONNO,
			       ICL_TYPE_INT64, ICL_HANDLE_OFFSET(filePos),
			       ICL_TYPE_HYPER, &avc->m.DataVersion,
			       ICL_TYPE_HYPER, &tdc->f.versionNo);
		    ReleaseReadLock(&tdc->lock);
		    afs_PutDCache(tdc);
		    tdc = NULL;
		}
	    }

	    if (!tdc) {
		/* If we get, it was not possible to start the
		 * background daemon. With flag == 1 afs_GetDCache
		 * does the FetchData rpc synchronously.
		 */
		ReleaseReadLock(&avc->lock);
		tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 1);
		ObtainReadLock(&avc->lock);
		if (tdc)
		    ObtainReadLock(&tdc->lock);
	    }
	}

	afs_Trace3(afs_iclSetp, CM_TRACE_VNODEREAD, ICL_TYPE_POINTER, tdc,
		   ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(offset),
		   ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(len));
	if (!tdc) {
	    error = EIO;
	    break;
	}

	/*
	 * Locks held:
	 * avc->lock(R)
	 * tdc->lock(R)
	 */

	if (len > totalLength)
	    len = totalLength;	/* will read len bytes */
	if (len <= 0) {		/* shouldn't get here if DFFetching is on */
	    /* read past the end of a chunk, may not be at next chunk yet, and yet
	     * also not at eof, so may have to supply fake zeros */
	    len = AFS_CHUNKTOSIZE(tdc->f.chunk) - offset;	/* bytes left in chunk addr space */
	    if (len > totalLength)
		len = totalLength;	/* and still within xfr request */
	    tlen = avc->m.Length - offset;	/* and still within file */
	    if (len > tlen)
		len = tlen;
	    if (len > AFS_ZEROS)
		len = sizeof(afs_zeros);	/* and in 0 buffer */
#ifdef AFS_DARWIN80_ENV
	    trimlen = len;
            tuiop = afsio_darwin_partialcopy(auio, trimlen);
#else
	    afsio_copy(auio, &tuio, tvec);
	    trimlen = len;
	    afsio_trim(&tuio, trimlen);
#endif
	    AFS_UIOMOVE(afs_zeros, trimlen, UIO_READ, tuiop, code);
	    if (code) {
		error = code;
		break;
	    }
	} else {
	    /* get the data from the mem cache */

	    /* mung uio structure to be right for this transfer */
#ifdef AFS_DARWIN80_ENV
	    trimlen = len;
            tuiop = afsio_darwin_partialcopy(auio, trimlen);
	    uio_setoffset(tuiop, offset);
#else
	    afsio_copy(auio, &tuio, tvec);
	    trimlen = len;
	    afsio_trim(&tuio, trimlen);
	    tuio.afsio_offset = offset;
#endif

	    code = afs_MemReadUIO(tdc->f.inode, tuiop);

	    if (code) {
		error = code;
		break;
	    }
	}
	/* otherwise we've read some, fixup length, etc and continue with next seg */
	len = len - AFS_UIO_RESID(tuiop);	/* compute amount really transferred */
	trimlen = len;
	afsio_skip(auio, trimlen);	/* update input uio structure */
	totalLength -= len;
	transferLength += len;
	filePos += len;

	if (len <= 0)
	    break;		/* surprise eof */
#ifdef AFS_DARWIN80_ENV
	if (tuiop) {
	    uio_free(tuiop);
	    tuiop = 0;
	}
#endif
    }				/* the whole while loop */

    /*
     * Locks held:
     * avc->lock(R)
     * tdc->lock(R) if tdc
     */

    /* if we make it here with tdc non-zero, then it is the last chunk we
     * dealt with, and we have to release it when we're done.  We hold on
     * to it in case we need to do a prefetch.
     */
    if (tdc) {
	ReleaseReadLock(&tdc->lock);
#if !defined(AFS_VM_RDWR_ENV)
	/* try to queue prefetch, if needed */
	if (!noLock) {
	    afs_PrefetchChunk(avc, tdc, acred, &treq);
	}
#endif
	afs_PutDCache(tdc);
    }
    if (!noLock)
	ReleaseReadLock(&avc->lock);
#ifdef AFS_DARWIN80_ENV
    if (tuiop)
       uio_free(tuiop);
#else
    osi_FreeSmallSpace(tvec);
#endif
    error = afs_CheckCode(error, &treq, 10);
    return error;
}

/* called with the dcache entry triggering the fetch, the vcache entry involved,
 * and a vrequest for the read call.  Marks the dcache entry as having already
 * triggered a prefetch, starts the prefetch going and sets the DFFetchReq
 * flag in the prefetched block, so that the next call to read knows to wait
 * for the daemon to start doing things.
 *
 * This function must be called with the vnode at least read-locked, and
 * no locks on the dcache, because it plays around with dcache entries.
 */
void
afs_PrefetchChunk(struct vcache *avc, struct dcache *adc,
		  struct AFS_UCRED *acred, struct vrequest *areq)
{
    register struct dcache *tdc;
    afs_size_t offset;
    afs_size_t j1, j2;		/* junk vbls for GetDCache to trash */

    offset = adc->f.chunk + 1;	/* next chunk we'll need */
    offset = AFS_CHUNKTOBASE(offset);	/* base of next chunk */
    ObtainReadLock(&adc->lock);
    ObtainSharedLock(&adc->mflock, 662);
    if (offset < avc->m.Length && !(adc->mflags & DFNextStarted)
	&& !afs_BBusy()) {
	struct brequest *bp;

	UpgradeSToWLock(&adc->mflock, 663);
	adc->mflags |= DFNextStarted;	/* we've tried to prefetch for this guy */
	ReleaseWriteLock(&adc->mflock);
	ReleaseReadLock(&adc->lock);

	tdc = afs_GetDCache(avc, offset, areq, &j1, &j2, 2);	/* type 2 never returns 0 */
	ObtainSharedLock(&tdc->mflock, 651);
	if (!(tdc->mflags & DFFetchReq)) {
	    /* ask the daemon to do the work */
	    UpgradeSToWLock(&tdc->mflock, 652);
	    tdc->mflags |= DFFetchReq;	/* guaranteed to be cleared by BKG or GetDCache */
	    /* last parm (1) tells bkg daemon to do an afs_PutDCache when it is done,
	     * since we don't want to wait for it to finish before doing so ourselves.
	     */
	    bp = afs_BQueue(BOP_FETCH, avc, B_DONTWAIT, 0, acred,
			    (afs_size_t) offset, (afs_size_t) 1, tdc);
	    if (!bp) {
		/* Bkg table full; just abort non-important prefetching to avoid deadlocks */
		tdc->mflags &= ~DFFetchReq;
		ReleaseWriteLock(&tdc->mflock);
		afs_PutDCache(tdc);

		/*
		 * DCLOCKXXX: This is a little sketchy, since someone else
		 * could have already started a prefetch..  In practice,
		 * this probably doesn't matter; at most it would cause an
		 * extra slot in the BKG table to be used up when someone
		 * prefetches this for the second time.
		 */
		ObtainReadLock(&adc->lock);
		ObtainWriteLock(&adc->mflock, 664);
		adc->mflags &= ~DFNextStarted;
		ReleaseWriteLock(&adc->mflock);
		ReleaseReadLock(&adc->lock);
	    } else {
		ReleaseWriteLock(&tdc->mflock);
	    }
	} else {
	    ReleaseSharedLock(&tdc->mflock);
	    afs_PutDCache(tdc);
	}
    } else {
	ReleaseSharedLock(&adc->mflock);
	ReleaseReadLock(&adc->lock);
    }
}

int
afs_UFSRead(register struct vcache *avc, struct uio *auio,
	    struct AFS_UCRED *acred, daddr_t albn, struct buf **abpp,
	    int noLock)
{
    afs_size_t totalLength;
    afs_size_t transferLength;
    afs_size_t filePos;
    afs_size_t offset, len, tlen;
    afs_int32 trimlen;
    struct dcache *tdc = 0;
    afs_int32 error;
#ifdef AFS_DARWIN80_ENV
    uio_t tuiop=NULL;
#else
    struct uio tuio;
    struct uio *tuiop = &tuio;
    struct iovec *tvec;
#endif
    struct osi_file *tfile;
    afs_int32 code;
    int trybusy = 1;
    struct vrequest treq;

    AFS_STATCNT(afs_UFSRead);
    if (avc && avc->vc_error)
	return EIO;

    /* check that we have the latest status info in the vnode cache */
    if ((code = afs_InitReq(&treq, acred)))
	return code;
    if (!noLock) {
	if (!avc)
	    osi_Panic("null avc in afs_UFSRead");
	else {
	    code = afs_VerifyVCache(avc, &treq);
	    if (code) {
		code = afs_CheckCode(code, &treq, 11);	/* failed to get it */
		return code;
	    }
	}
    }
#ifndef	AFS_VM_RDWR_ENV
    if (AFS_NFSXLATORREQ(acred)) {
	if (!afs_AccessOK
	    (avc, PRSFS_READ, &treq,
	     CHECK_MODE_BITS | CMB_ALLOW_EXEC_AS_READ)) {
	    return afs_CheckCode(EACCES, &treq, 12);
	}
    }
#endif

#ifndef AFS_DARWIN80_ENV
    tvec = (struct iovec *)osi_AllocSmallSpace(sizeof(struct iovec));
#endif
    totalLength = AFS_UIO_RESID(auio);
    filePos = AFS_UIO_OFFSET(auio);
    afs_Trace4(afs_iclSetp, CM_TRACE_READ, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(filePos), ICL_TYPE_INT32,
	       totalLength, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->m.Length));
    error = 0;
    transferLength = 0;
    if (!noLock)
	ObtainReadLock(&avc->lock);
#if	defined(AFS_TEXT_ENV) && !defined(AFS_VM_RDWR_ENV)
    if (avc->flushDV.high == AFS_MAXDV && avc->flushDV.low == AFS_MAXDV) {
	hset(avc->flushDV, avc->m.DataVersion);
    }
#endif

    while (totalLength > 0) {
	/* read all of the cached info */
	if (filePos >= avc->m.Length)
	    break;		/* all done */
	if (noLock) {
	    if (tdc) {
		ReleaseReadLock(&tdc->lock);
		afs_PutDCache(tdc);
	    }
	    tdc = afs_FindDCache(avc, filePos);
	    if (tdc) {
		ObtainReadLock(&tdc->lock);
		offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
		len = tdc->validPos - filePos;
	    }
	} else {
	    /* a tricky question: does the presence of the DFFetching flag
	     * mean that we're fetching the latest version of the file?  No.
	     * The server could update the file as soon as the fetch responsible
	     * for the setting of the DFFetching flag completes.
	     * 
	     * However, the presence of the DFFetching flag (visible under
	     * a dcache read lock since it is set and cleared only under a
	     * dcache write lock) means that we're fetching as good a version
	     * as was known to this client at the time of the last call to
	     * afs_VerifyVCache, since the latter updates the stat cache's
	     * m.DataVersion field under a vcache write lock, and from the
	     * time that the DFFetching flag goes on in afs_GetDCache (before
	     * the fetch starts), to the time it goes off (after the fetch
	     * completes), afs_GetDCache keeps at least a read lock on the
	     * vcache entry.
	     * 
	     * This means that if the DFFetching flag is set, we can use that
	     * data for any reads that must come from the current version of
	     * the file (current == m.DataVersion).
	     * 
	     * Another way of looking at this same point is this: if we're
	     * fetching some data and then try do an afs_VerifyVCache, the
	     * VerifyVCache operation will not complete until after the
	     * DFFetching flag is turned off and the dcache entry's f.versionNo
	     * field is updated.
	     * 
	     * Note, by the way, that if DFFetching is set,
	     * m.DataVersion > f.versionNo (the latter is not updated until
	     * after the fetch completes).
	     */
	    if (tdc) {
		ReleaseReadLock(&tdc->lock);
		afs_PutDCache(tdc);	/* before reusing tdc */
	    }
	    tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 2);
	    ObtainReadLock(&tdc->lock);
	    /* now, first try to start transfer, if we'll need the data.  If
	     * data already coming, we don't need to do this, obviously.  Type
	     * 2 requests never return a null dcache entry, btw. */
	    if (!(tdc->dflags & DFFetching)
		&& !hsame(avc->m.DataVersion, tdc->f.versionNo)) {
		/* have cache entry, it is not coming in now, and we'll need new data */
	      tagain:
		if (trybusy && !afs_BBusy()) {
		    struct brequest *bp;
		    /* daemon is not busy */
		    ObtainSharedLock(&tdc->mflock, 667);
		    if (!(tdc->mflags & DFFetchReq)) {
			UpgradeSToWLock(&tdc->mflock, 668);
			tdc->mflags |= DFFetchReq;
			bp = afs_BQueue(BOP_FETCH, avc, B_DONTWAIT, 0, acred,
					(afs_size_t) filePos, (afs_size_t) 0,
					tdc);
			if (!bp) {
			    /* Bkg table full; retry deadlocks */
			    tdc->mflags &= ~DFFetchReq;
			    trybusy = 0;	/* Avoid bkg daemon since they're too busy */
			    ReleaseWriteLock(&tdc->mflock);
			    goto tagain;
			}
			ConvertWToSLock(&tdc->mflock);
		    }
		    code = 0;
		    ConvertSToRLock(&tdc->mflock);
		    while (!code && tdc->mflags & DFFetchReq) {
			afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAIT,
				   ICL_TYPE_STRING, __FILE__, ICL_TYPE_INT32,
				   __LINE__, ICL_TYPE_POINTER, tdc,
				   ICL_TYPE_INT32, tdc->dflags);
			/* don't need waiting flag on this one */
			ReleaseReadLock(&tdc->mflock);
			ReleaseReadLock(&tdc->lock);
			ReleaseReadLock(&avc->lock);
			code = afs_osi_SleepSig(&tdc->validPos);
			ObtainReadLock(&avc->lock);
			ObtainReadLock(&tdc->lock);
			ObtainReadLock(&tdc->mflock);
		    }
		    ReleaseReadLock(&tdc->mflock);
		    if (code) {
			error = code;
			break;
		    }
		}
	    }
	    /* now data may have started flowing in (if DFFetching is on).  If
	     * data is now streaming in, then wait for some interesting stuff.
	     */
	    code = 0;
	    while (!code && (tdc->dflags & DFFetching)
		   && tdc->validPos <= filePos) {
		/* too early: wait for DFFetching flag to vanish,
		 * or data to appear */
		afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAIT, ICL_TYPE_STRING,
			   __FILE__, ICL_TYPE_INT32, __LINE__,
			   ICL_TYPE_POINTER, tdc, ICL_TYPE_INT32,
			   tdc->dflags);
		ReleaseReadLock(&tdc->lock);
		ReleaseReadLock(&avc->lock);
		code = afs_osi_SleepSig(&tdc->validPos);
		ObtainReadLock(&avc->lock);
		ObtainReadLock(&tdc->lock);
	    }
	    if (code) {
		error = code;
		break;
	    }
	    /* fetching flag gone, data is here, or we never tried
	     * (BBusy for instance) */
	    if (tdc->dflags & DFFetching) {
		/* still fetching, some new data is here:
		 * compute length and offset */
		offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
		len = tdc->validPos - filePos;
	    } else {
		/* no longer fetching, verify data version (avoid new
		 * GetDCache call) */
		if (hsame(avc->m.DataVersion, tdc->f.versionNo)
		    && ((len = tdc->validPos - filePos) > 0)) {
		    offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
		} else {
		    /* don't have current data, so get it below */
		    afs_Trace3(afs_iclSetp, CM_TRACE_VERSIONNO,
			       ICL_TYPE_INT64, ICL_HANDLE_OFFSET(filePos),
			       ICL_TYPE_HYPER, &avc->m.DataVersion,
			       ICL_TYPE_HYPER, &tdc->f.versionNo);
		    ReleaseReadLock(&tdc->lock);
		    afs_PutDCache(tdc);
		    tdc = NULL;
		}
	    }

	    if (!tdc) {
		/* If we get, it was not possible to start the 
		 * background daemon. With flag == 1 afs_GetDCache
		 * does the FetchData rpc synchronously.
		 */
		ReleaseReadLock(&avc->lock);
		tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 1);
		ObtainReadLock(&avc->lock);
		if (tdc)
		    ObtainReadLock(&tdc->lock);
	    }
	}

	if (!tdc) {
	    error = EIO;
	    break;
	}
	len = tdc->validPos - filePos;
	afs_Trace3(afs_iclSetp, CM_TRACE_VNODEREAD, ICL_TYPE_POINTER, tdc,
		   ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(offset),
		   ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(len));
	if (len > totalLength)
	    len = totalLength;	/* will read len bytes */
	if (len <= 0) {		/* shouldn't get here if DFFetching is on */
	    afs_Trace4(afs_iclSetp, CM_TRACE_VNODEREAD2, ICL_TYPE_POINTER,
		       tdc, ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(tdc->validPos),
		       ICL_TYPE_INT32, tdc->f.chunkBytes, ICL_TYPE_INT32,
		       tdc->dflags);
	    /* read past the end of a chunk, may not be at next chunk yet, and yet
	     * also not at eof, so may have to supply fake zeros */
	    len = AFS_CHUNKTOSIZE(tdc->f.chunk) - offset;	/* bytes left in chunk addr space */
	    if (len > totalLength)
		len = totalLength;	/* and still within xfr request */
	    tlen = avc->m.Length - offset;	/* and still within file */
	    if (len > tlen)
		len = tlen;
	    if (len > AFS_ZEROS)
		len = sizeof(afs_zeros);	/* and in 0 buffer */
#ifdef AFS_DARWIN80_ENV
	    trimlen = len;
            tuiop = afsio_darwin_partialcopy(auio, trimlen);
#else
	    afsio_copy(auio, &tuio, tvec);
	    trimlen = len;
	    afsio_trim(&tuio, trimlen);
#endif
	    AFS_UIOMOVE(afs_zeros, trimlen, UIO_READ, tuiop, code);
	    if (code) {
		error = code;
		break;
	    }
	} else {
	    /* get the data from the file */
#ifdef IHINT
	    if (tfile = tdc->ihint) {
		if (tdc->f.inode != tfile->inum) {
		    afs_warn("afs_UFSRead: %x hint mismatch tdc %d inum %d\n",
			     tdc, tdc->f.inode, tfile->inum);
		    osi_UFSClose(tfile);
		    tdc->ihint = tfile = 0;
		    nihints--;
		}
	    }
	    if (tfile != 0) {
		usedihint++;
	    } else
#endif /* IHINT */

		tfile = (struct osi_file *)osi_UFSOpen(tdc->f.inode);
#ifdef AFS_DARWIN80_ENV
	    trimlen = len;
            tuiop = afsio_darwin_partialcopy(auio, trimlen);
	    uio_setoffset(tuiop, offset);
#else
	    /* mung uio structure to be right for this transfer */
	    afsio_copy(auio, &tuio, tvec);
	    trimlen = len;
	    afsio_trim(&tuio, trimlen);
	    tuio.afsio_offset = offset;
#endif

#if defined(AFS_AIX41_ENV)
	    AFS_GUNLOCK();
	    code =
		VNOP_RDWR(tfile->vnode, UIO_READ, FREAD, &tuio, NULL, NULL,
			  NULL, afs_osi_credp);
	    AFS_GLOCK();
#elif defined(AFS_AIX32_ENV)
	    code =
		VNOP_RDWR(tfile->vnode, UIO_READ, FREAD, &tuio, NULL, NULL);
	    /* Flush all JFS pages now for big performance gain in big file cases
	     * If we do something like this, must check to be sure that AFS file 
	     * isn't mmapped... see afs_gn_map() for why.
	     */
/*
	  if (tfile->vnode->v_gnode && tfile->vnode->v_gnode->gn_seg) {
 many different ways to do similar things:
   so far, the best performing one is #2, but #1 might match it if we
   straighten out the confusion regarding which pages to flush.  It 
   really does matter.
   1.	    vm_flushp(tfile->vnode->v_gnode->gn_seg, 0, len/PAGESIZE - 1);
   2.	    vm_releasep(tfile->vnode->v_gnode->gn_seg, offset/PAGESIZE, 
			(len + PAGESIZE-1)/PAGESIZE);
   3.	    vms_inactive(tfile->vnode->v_gnode->gn_seg) Doesn't work correctly
   4.  	    vms_delete(tfile->vnode->v_gnode->gn_seg) probably also fails
	    tfile->vnode->v_gnode->gn_seg = NULL;
   5.       deletep
   6.       ipgrlse
   7.       ifreeseg
          Unfortunately, this seems to cause frequent "cache corruption" episodes.
   	    vm_releasep(tfile->vnode->v_gnode->gn_seg, offset/PAGESIZE, 
			(len + PAGESIZE-1)/PAGESIZE);
	  }	
*/
#elif defined(AFS_AIX_ENV)
	    code =
		VNOP_RDWR(tfile->vnode, UIO_READ, FREAD, (off_t) & offset,
			  &tuio, NULL, NULL, -1);
#elif defined(AFS_SUN5_ENV)
	    AFS_GUNLOCK();
#ifdef AFS_SUN510_ENV
	    {
		caller_context_t ct;

		VOP_RWLOCK(tfile->vnode, 0, &ct);
		code = VOP_READ(tfile->vnode, &tuio, 0, afs_osi_credp, &ct);
		VOP_RWUNLOCK(tfile->vnode, 0, &ct);
	    }
#else
	    VOP_RWLOCK(tfile->vnode, 0);
	    code = VOP_READ(tfile->vnode, &tuio, 0, afs_osi_credp);
	    VOP_RWUNLOCK(tfile->vnode, 0);
#endif
	    AFS_GLOCK();
#elif defined(AFS_SGI_ENV)
	    AFS_GUNLOCK();
	    AFS_VOP_RWLOCK(tfile->vnode, VRWLOCK_READ);
	    AFS_VOP_READ(tfile->vnode, &tuio, IO_ISLOCKED, afs_osi_credp,
			 code);
	    AFS_VOP_RWUNLOCK(tfile->vnode, VRWLOCK_READ);
	    AFS_GLOCK();
#elif defined(AFS_OSF_ENV)
	    tuio.uio_rw = UIO_READ;
	    AFS_GUNLOCK();
	    VOP_READ(tfile->vnode, &tuio, 0, afs_osi_credp, code);
	    AFS_GLOCK();
#elif defined(AFS_HPUX100_ENV)
	    AFS_GUNLOCK();
	    code = VOP_RDWR(tfile->vnode, &tuio, UIO_READ, 0, afs_osi_credp);
	    AFS_GLOCK();
#elif defined(AFS_LINUX20_ENV)
	    AFS_GUNLOCK();
	    code = osi_rdwr(tfile, &tuio, UIO_READ);
	    AFS_GLOCK();
#elif defined(AFS_DARWIN80_ENV)
	    AFS_GUNLOCK();
	    code = VNOP_READ(tfile->vnode, tuiop, 0, afs_osi_ctxtp);
	    AFS_GLOCK();
#elif defined(AFS_DARWIN_ENV)
	    AFS_GUNLOCK();
	    VOP_LOCK(tfile->vnode, LK_EXCLUSIVE, current_proc());
	    code = VOP_READ(tfile->vnode, &tuio, 0, afs_osi_credp);
	    VOP_UNLOCK(tfile->vnode, 0, current_proc());
	    AFS_GLOCK();
#elif defined(AFS_FBSD50_ENV)
	    AFS_GUNLOCK();
	    VOP_LOCK(tfile->vnode, LK_EXCLUSIVE, curthread);
	    code = VOP_READ(tfile->vnode, &tuio, 0, afs_osi_credp);
	    VOP_UNLOCK(tfile->vnode, 0, curthread);
	    AFS_GLOCK();
#elif defined(AFS_XBSD_ENV)
	    AFS_GUNLOCK();
	    VOP_LOCK(tfile->vnode, LK_EXCLUSIVE, curproc);
	    code = VOP_READ(tfile->vnode, &tuio, 0, afs_osi_credp);
	    VOP_UNLOCK(tfile->vnode, 0, curproc);
	    AFS_GLOCK();
#else
	    code = VOP_RDWR(tfile->vnode, &tuio, UIO_READ, 0, afs_osi_credp);
#endif

#ifdef IHINT
	    if (!tdc->ihint && nihints < maxIHint) {
		tdc->ihint = tfile;
		nihints++;
	    } else
#endif /* IHINT */
		osi_UFSClose(tfile);

	    if (code) {
		error = code;
		break;
	    }
	}
	/* otherwise we've read some, fixup length, etc and continue with next seg */
	len = len - AFS_UIO_RESID(tuiop);	/* compute amount really transferred */
	trimlen = len;
	afsio_skip(auio, trimlen);	/* update input uio structure */
	totalLength -= len;
	transferLength += len;
	filePos += len;
	if (len <= 0)
	    break;		/* surprise eof */
#ifdef AFS_DARWIN80_ENV
	if (tuiop) {
	    uio_free(tuiop);
	    tuiop = 0;
	}
#endif
    }

    /* if we make it here with tdc non-zero, then it is the last chunk we
     * dealt with, and we have to release it when we're done.  We hold on
     * to it in case we need to do a prefetch, obviously.
     */
    if (tdc) {
	ReleaseReadLock(&tdc->lock);
#if !defined(AFS_VM_RDWR_ENV)
	/* try to queue prefetch, if needed */
	if (!noLock) {
	    if (!(tdc->mflags & DFNextStarted))
		afs_PrefetchChunk(avc, tdc, acred, &treq);
	}
#endif
	afs_PutDCache(tdc);
    }
    if (!noLock)
	ReleaseReadLock(&avc->lock);

#ifdef AFS_DARWIN80_ENV
    if (tuiop)
       uio_free(tuiop);
#else
    osi_FreeSmallSpace(tvec);
#endif
    error = afs_CheckCode(error, &treq, 13);
    return error;
}
