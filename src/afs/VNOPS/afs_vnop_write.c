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
 * afs_UFSWrite
 * afs_MemWrite
 * afs_StoreOnLastReference
 * afs_close
 * afs_closex
 * afs_fsync
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


extern unsigned char *afs_indexFlags;

/* Called by all write-on-close routines: regular afs_close,
 * store via background daemon and store via the
 * afs_FlushActiveVCaches routine (when CCORE is on).
 * avc->lock must be write-locked.
 */
int
afs_StoreOnLastReference(register struct vcache *avc,
			 register struct vrequest *treq)
{
    int code = 0;

    AFS_STATCNT(afs_StoreOnLastReference);
    /* if CCore flag is set, we clear it and do the extra decrement
     * ourselves now. If we're called by the CCore clearer, the CCore
     * flag will already be clear, so we don't have to worry about
     * clearing it twice. */
    if (avc->states & CCore) {
	avc->states &= ~CCore;
#if defined(AFS_SGI_ENV)
	osi_Assert(avc->opens > 0 && avc->execsOrWriters > 0);
#endif
	/* WARNING: Our linux cm code treats the execsOrWriters counter differently 
	 * depending on the flags the file was opened with. So, if you make any 
	 * changes to the way the execsOrWriters flag is handled check with the 
	 * top level code.  */
	avc->opens--;
	avc->execsOrWriters--;
	AFS_RELE(AFSTOV(avc));	/* VN_HOLD at set CCore(afs_FakeClose) */
	crfree((struct AFS_UCRED *)avc->linkData);	/* "crheld" in afs_FakeClose */
	avc->linkData = NULL;
    }
    /* Now, send the file back.  Used to require 0 writers left, but now do
     * it on every close for write, since two closes in a row are harmless
     * since first will clean all chunks, and second will be noop.  Note that
     * this will also save confusion when someone keeps a file open 
     * inadvertently, since with old system, writes to the server would never
     * happen again. 
     */
    code = afs_StoreAllSegments(avc, treq, AFS_LASTSTORE /*!sync-to-disk */ );
    /*
     * We have to do these after the above store in done: in some systems like
     * aix they'll need to flush all the vm dirty pages to the disk via the
     * strategy routine. During that all procedure (done under no avc locks)
     * opens, refcounts would be zero, since it didn't reach the afs_{rd,wr}
     * routines which means the vcache is a perfect candidate for flushing!
     */
#if defined(AFS_SGI_ENV)
    osi_Assert(avc->opens > 0 && avc->execsOrWriters > 0);
#endif
    avc->opens--;
    avc->execsOrWriters--;
    return code;
}



int
afs_MemWrite(register struct vcache *avc, struct uio *auio, int aio,
	     struct AFS_UCRED *acred, int noLock)
{
    afs_size_t totalLength;
    afs_size_t transferLength;
    afs_size_t filePos;
    afs_size_t offset, len;
    afs_int32 tlen, trimlen;
    afs_int32 startDate;
    afs_int32 max;
    register struct dcache *tdc;
#ifdef _HIGHC_
    volatile
#endif
    afs_int32 error;
#ifdef AFS_DARWIN80_ENV
    uio_t tuiop = NULL;
#else
    struct uio tuio;
    struct uio *tuiop = &tuio;
    struct iovec *tvec;		/* again, should have define */
#endif
    register afs_int32 code;
    struct vrequest treq;

    AFS_STATCNT(afs_MemWrite);
    if (avc->vc_error)
	return avc->vc_error;

    startDate = osi_Time();
    if ((code = afs_InitReq(&treq, acred)))
	return code;
    /* otherwise we read */
    totalLength = AFS_UIO_RESID(auio);
    filePos = AFS_UIO_OFFSET(auio);
    error = 0;
    transferLength = 0;
    afs_Trace4(afs_iclSetp, CM_TRACE_WRITE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(filePos), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(totalLength), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->m.Length));
    if (!noLock) {
	afs_MaybeWakeupTruncateDaemon();
	ObtainWriteLock(&avc->lock, 126);
    }
#if defined(AFS_SGI_ENV)
    {
	off_t diff;
	/*
	 * afs_xwrite handles setting m.Length
	 * and handles APPEND mode.
	 * Since we are called via strategy, we need to trim the write to
	 * the actual size of the file
	 */
	osi_Assert(filePos <= avc->m.Length);
	diff = avc->m.Length - filePos;
	AFS_UIO_SETRESID(auio, MIN(totalLength, diff));
	totalLength = AFS_UIO_RESID(auio);
    }
#else
    if (aio & IO_APPEND) {
	/* append mode, start it at the right spot */
#if	defined(AFS_SUN56_ENV)
	auio->uio_loffset = 0;
#endif
	filePos = avc->m.Length;
	AFS_UIO_SETOFFSET(auio, filePos);
    }
#endif
    /*
     * Note that we use startDate rather than calling osi_Time() here.
     * This is to avoid counting lock-waiting time in file date (for ranlib).
     */
    avc->m.Date = startDate;

#if	defined(AFS_HPUX_ENV)
#if	defined(AFS_HPUX101_ENV)
    if ((totalLength + filePos) >> 9 >
	(p_rlimit(u.u_procp))[RLIMIT_FSIZE].rlim_cur) {
#else
    if ((totalLength + filePos) >> 9 > u.u_rlimit[RLIMIT_FSIZE].rlim_cur) {
#endif
	if (!noLock)
	    ReleaseWriteLock(&avc->lock);
	return (EFBIG);
    }
#endif
#ifdef AFS_VM_RDWR_ENV
    /*
     * If write is implemented via VM, afs_FakeOpen() is called from the
     * high-level write op.
     */
    if (avc->execsOrWriters <= 0) {
	printf("WARNING: afs_ufswr vp=%lx, exOrW=%d\n", (unsigned long)avc,
	       avc->execsOrWriters);
    }
#else
    afs_FakeOpen(avc);
#endif
    avc->states |= CDirty;
#ifndef AFS_DARWIN80_ENV
    tvec = (struct iovec *)osi_AllocSmallSpace(sizeof(struct iovec));
#endif
    while (totalLength > 0) {
	/* 
	 *  The following line is necessary because afs_GetDCache with
	 *  flag == 4 expects the length field to be filled. It decides
	 *  from this whether it's necessary to fetch data into the chunk
	 *  before writing or not (when the whole chunk is overwritten!).
	 */
	len = totalLength;	/* write this amount by default */
	if (noLock) {
	    tdc = afs_FindDCache(avc, filePos);
	    if (tdc)
		ObtainWriteLock(&tdc->lock, 653);
	} else if (afs_blocksUsed >
		   PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
	    tdc = afs_FindDCache(avc, filePos);
	    if (tdc) {
		ObtainWriteLock(&tdc->lock, 654);
		if (!hsame(tdc->f.versionNo, avc->m.DataVersion)
		    || (tdc->dflags & DFFetching)) {
		    ReleaseWriteLock(&tdc->lock);
		    afs_PutDCache(tdc);
		    tdc = NULL;
		}
	    }
	    if (!tdc) {
		afs_MaybeWakeupTruncateDaemon();
		while (afs_blocksUsed >
		       PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
		    ReleaseWriteLock(&avc->lock);
		    if (afs_blocksUsed - afs_blocksDiscarded >
			PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
			afs_WaitForCacheDrain = 1;
			afs_osi_Sleep(&afs_WaitForCacheDrain);
		    }
		    afs_MaybeFreeDiscardedDCache();
		    afs_MaybeWakeupTruncateDaemon();
		    ObtainWriteLock(&avc->lock, 506);
		}
		avc->states |= CDirty;
		tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 4);
		if (tdc)
		    ObtainWriteLock(&tdc->lock, 655);
	    }
	} else {
	    tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 4);
	    if (tdc)
		ObtainWriteLock(&tdc->lock, 656);
	}
	if (!tdc) {
	    error = EIO;
	    break;
	}
	if (!(afs_indexFlags[tdc->index] & IFDataMod)) {
	    afs_stats_cmperf.cacheCurrDirtyChunks++;
	    afs_indexFlags[tdc->index] |= IFDataMod;	/* so it doesn't disappear */
	}
	if (!(tdc->f.states & DWriting)) {
	    /* don't mark entry as mod if we don't have to */
	    tdc->f.states |= DWriting;
	    tdc->dflags |= DFEntryMod;
	}
	len = totalLength;	/* write this amount by default */
	offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
	max = AFS_CHUNKTOSIZE(tdc->f.chunk);	/* max size of this chunk */
	if (max <= len + offset) {	/*if we'd go past the end of this chunk */
	    /* it won't all fit in this chunk, so write as much
	     * as will fit */
	    len = max - offset;
	}

#ifdef  AFS_DARWIN80_ENV
        if (tuiop)
	    uio_free(tuiop);
	trimlen = len;
	tuiop = afsio_darwin_partialcopy(auio, trimlen);
#else
	/* mung uio structure to be right for this transfer */
	afsio_copy(auio, &tuio, tvec);
	trimlen = len;
	afsio_trim(&tuio, trimlen);
#endif
	AFS_UIO_SETOFFSET(tuiop, offset);

	code = afs_MemWriteUIO(tdc->f.inode, tuiop);
	if (code) {
	    void *mep;		/* XXX in prototype world is struct memCacheEntry * */
	    error = code;
	    ZapDCE(tdc);	/* bad data */
	    mep = afs_MemCacheOpen(tdc->f.inode);
	    afs_MemCacheTruncate(mep, 0);
	    afs_MemCacheClose(mep);
	    afs_stats_cmperf.cacheCurrDirtyChunks--;
	    afs_indexFlags[tdc->index] &= ~IFDataMod;	/* so it does disappear */
	    ReleaseWriteLock(&tdc->lock);
	    afs_PutDCache(tdc);
	    break;
	}
	/* otherwise we've written some, fixup length, etc and continue with next seg */
	len = len - AFS_UIO_RESID(tuiop);	/* compute amount really transferred */
	tlen = len;
	afsio_skip(auio, tlen);	/* advance auio over data written */
	/* compute new file size */
	if (offset + len > tdc->f.chunkBytes) {
	    afs_int32 tlength = offset + len;
	    afs_AdjustSize(tdc, tlength);
	    if (tdc->validPos < filePos + len)
		tdc->validPos = filePos + len;
	}
	totalLength -= len;
	transferLength += len;
	filePos += len;
#if defined(AFS_SGI_ENV)
	/* afs_xwrite handles setting m.Length */
	osi_Assert(filePos <= avc->m.Length);
#else
	if (filePos > avc->m.Length) {
	    afs_Trace4(afs_iclSetp, CM_TRACE_SETLENGTH, ICL_TYPE_STRING,
		       __FILE__, ICL_TYPE_LONG, __LINE__, ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(avc->m.Length), ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(filePos));
	    avc->m.Length = filePos;
	}
#endif
	ReleaseWriteLock(&tdc->lock);
	afs_PutDCache(tdc);
#if !defined(AFS_VM_RDWR_ENV)
	/*
	 * If write is implemented via VM, afs_DoPartialWrite() is called from
	 * the high-level write op.
	 */
	if (!noLock) {
	    code = afs_DoPartialWrite(avc, &treq);
	    if (code) {
		error = code;
		break;
	    }
	}
#endif
    }
#ifndef	AFS_VM_RDWR_ENV
    afs_FakeClose(avc, acred);
#endif
    if (error && !avc->vc_error)
	avc->vc_error = error;
    if (!noLock)
	ReleaseWriteLock(&avc->lock);
#ifdef AFS_DARWIN80_ENV
    uio_free(tuiop);
#else
    osi_FreeSmallSpace(tvec);
#endif
    error = afs_CheckCode(error, &treq, 6);
    return error;
}


/* called on writes */
int
afs_UFSWrite(register struct vcache *avc, struct uio *auio, int aio,
	     struct AFS_UCRED *acred, int noLock)
{
    afs_size_t totalLength;
    afs_size_t transferLength;
    afs_size_t filePos;
    afs_size_t offset, len;
    afs_int32 tlen;
    afs_int32 trimlen;
    afs_int32 startDate;
    afs_int32 max;
    register struct dcache *tdc;
#ifdef _HIGHC_
    volatile
#endif
    afs_int32 error;
#ifdef AFS_DARWIN80_ENV
    uio_t tuiop = NULL;
#else
    struct uio tuio;
    struct uio *tuiop = &tuio;
    struct iovec *tvec;		/* again, should have define */
#endif
    struct osi_file *tfile;
    register afs_int32 code;
    struct vrequest treq;

    AFS_STATCNT(afs_UFSWrite);
    if (avc->vc_error)
	return avc->vc_error;

    startDate = osi_Time();
    if ((code = afs_InitReq(&treq, acred)))
	return code;
    /* otherwise we read */
    totalLength = AFS_UIO_RESID(auio);
    filePos = AFS_UIO_OFFSET(auio);
    error = 0;
    transferLength = 0;
    afs_Trace4(afs_iclSetp, CM_TRACE_WRITE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(filePos), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(totalLength), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(avc->m.Length));
    if (!noLock) {
	afs_MaybeWakeupTruncateDaemon();
	ObtainWriteLock(&avc->lock, 556);
    }
#if defined(AFS_SGI_ENV)
    {
	off_t diff;
	/*
	 * afs_xwrite handles setting m.Length
	 * and handles APPEND mode.
	 * Since we are called via strategy, we need to trim the write to
	 * the actual size of the file
	 */
	osi_Assert(filePos <= avc->m.Length);
	diff = avc->m.Length - filePos;
	AFS_UIO_SETRESID(auio, MIN(totalLength, diff));
	totalLength = AFS_UIO_RESID(auio);
    }
#else
    if (aio & IO_APPEND) {
	/* append mode, start it at the right spot */
#if     defined(AFS_SUN56_ENV)
	auio->uio_loffset = 0;
#endif
	filePos = avc->m.Length;
	AFS_UIO_SETOFFSET(auio, avc->m.Length);
    }
#endif
    /*
     * Note that we use startDate rather than calling osi_Time() here.
     * This is to avoid counting lock-waiting time in file date (for ranlib).
     */
    avc->m.Date = startDate;

#if	defined(AFS_HPUX_ENV)
#if 	defined(AFS_HPUX101_ENV)
    if ((totalLength + filePos) >> 9 >
	p_rlimit(u.u_procp)[RLIMIT_FSIZE].rlim_cur) {
#else
    if ((totalLength + filePos) >> 9 > u.u_rlimit[RLIMIT_FSIZE].rlim_cur) {
#endif
	if (!noLock)
	    ReleaseWriteLock(&avc->lock);
	return (EFBIG);
    }
#endif
#ifdef	AFS_VM_RDWR_ENV
    /*
     * If write is implemented via VM, afs_FakeOpen() is called from the
     * high-level write op.
     */
    if (avc->execsOrWriters <= 0) {
	printf("WARNING: afs_ufswr vcp=%lx, exOrW=%d\n", (unsigned long)avc,
	       avc->execsOrWriters);
    }
#else
    afs_FakeOpen(avc);
#endif
    avc->states |= CDirty;
#ifndef AFS_DARWIN80_ENV
    tvec = (struct iovec *)osi_AllocSmallSpace(sizeof(struct iovec));
#endif
    while (totalLength > 0) {
	/* 
	 *  The following line is necessary because afs_GetDCache with
	 *  flag == 4 expects the length field to be filled. It decides
	 *  from this whether it's necessary to fetch data into the chunk
	 *  before writing or not (when the whole chunk is overwritten!).
	 */
	len = totalLength;	/* write this amount by default */
	/* read the cached info */
	if (noLock) {
	    tdc = afs_FindDCache(avc, filePos);
	    if (tdc)
		ObtainWriteLock(&tdc->lock, 657);
	} else if (afs_blocksUsed >
		   PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
	    tdc = afs_FindDCache(avc, filePos);
	    if (tdc) {
		ObtainWriteLock(&tdc->lock, 658);
		if (!hsame(tdc->f.versionNo, avc->m.DataVersion)
		    || (tdc->dflags & DFFetching)) {
		    ReleaseWriteLock(&tdc->lock);
		    afs_PutDCache(tdc);
		    tdc = NULL;
		}
	    }
	    if (!tdc) {
		afs_MaybeWakeupTruncateDaemon();
		while (afs_blocksUsed >
		       PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
		    ReleaseWriteLock(&avc->lock);
		    if (afs_blocksUsed - afs_blocksDiscarded >
			PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
			afs_WaitForCacheDrain = 1;
			afs_osi_Sleep(&afs_WaitForCacheDrain);
		    }
		    afs_MaybeFreeDiscardedDCache();
		    afs_MaybeWakeupTruncateDaemon();
		    ObtainWriteLock(&avc->lock, 509);
		}
		avc->states |= CDirty;
		tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 4);
		if (tdc)
		    ObtainWriteLock(&tdc->lock, 659);
	    }
	} else {
	    tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 4);
	    if (tdc)
		ObtainWriteLock(&tdc->lock, 660);
	}
	if (!tdc) {
	    error = EIO;
	    break;
	}
	if (!(afs_indexFlags[tdc->index] & IFDataMod)) {
	    afs_stats_cmperf.cacheCurrDirtyChunks++;
	    afs_indexFlags[tdc->index] |= IFDataMod;	/* so it doesn't disappear */
	}
	if (!(tdc->f.states & DWriting)) {
	    /* don't mark entry as mod if we don't have to */
	    tdc->f.states |= DWriting;
	    tdc->dflags |= DFEntryMod;
	}
	tfile = (struct osi_file *)osi_UFSOpen(tdc->f.inode);
	len = totalLength;	/* write this amount by default */
	offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
	max = AFS_CHUNKTOSIZE(tdc->f.chunk);	/* max size of this chunk */
	if (max <= len + offset) {	/*if we'd go past the end of this chunk */
	    /* it won't all fit in this chunk, so write as much
	     * as will fit */
	    len = max - offset;
	}

#ifdef  AFS_DARWIN80_ENV
	if (tuiop)
	    uio_free(tuiop);
	trimlen = len;
	tuiop = afsio_darwin_partialcopy(auio, trimlen);
#else
	/* mung uio structure to be right for this transfer */
	afsio_copy(auio, &tuio, tvec);
	trimlen = len;
	afsio_trim(&tuio, trimlen);
#endif
	AFS_UIO_SETOFFSET(tuiop, offset);

#if defined(AFS_AIX41_ENV)
	AFS_GUNLOCK();
	code =
	    VNOP_RDWR(tfile->vnode, UIO_WRITE, FWRITE, &tuio, NULL, NULL,
		      NULL, afs_osi_credp);
	AFS_GLOCK();
#elif defined(AFS_AIX32_ENV)
	code = VNOP_RDWR(tfile->vnode, UIO_WRITE, FWRITE, &tuio, NULL, NULL);
#elif defined(AFS_AIX_ENV)
	code =
	    VNOP_RDWR(tfile->vnode, UIO_WRITE, FWRITE, (off_t) & offset,
		      &tuio, NULL, NULL, -1);
#elif defined(AFS_SUN5_ENV)
	AFS_GUNLOCK();
#ifdef AFS_SUN510_ENV
	{
	    caller_context_t ct;

	    VOP_RWLOCK(tfile->vnode, 1, &ct);
	    code = VOP_WRITE(tfile->vnode, &tuio, 0, afs_osi_credp, &ct);
	    VOP_RWUNLOCK(tfile->vnode, 1, &ct);
	}
#else
	VOP_RWLOCK(tfile->vnode, 1);
	code = VOP_WRITE(tfile->vnode, &tuio, 0, afs_osi_credp);
	VOP_RWUNLOCK(tfile->vnode, 1);
#endif
	AFS_GLOCK();
	if (code == ENOSPC)
	    afs_warnuser
		("\n\n\n*** Cache partition is full - decrease cachesize!!! ***\n\n\n");
#elif defined(AFS_SGI_ENV)
	AFS_GUNLOCK();
	avc->states |= CWritingUFS;
	AFS_VOP_RWLOCK(tfile->vnode, VRWLOCK_WRITE);
	AFS_VOP_WRITE(tfile->vnode, &tuio, IO_ISLOCKED, afs_osi_credp, code);
	AFS_VOP_RWUNLOCK(tfile->vnode, VRWLOCK_WRITE);
	avc->states &= ~CWritingUFS;
	AFS_GLOCK();
#elif defined(AFS_OSF_ENV)
	{
	    struct ucred *tmpcred = u.u_cred;
	    u.u_cred = afs_osi_credp;
	    tuio.uio_rw = UIO_WRITE;
	    AFS_GUNLOCK();
	    VOP_WRITE(tfile->vnode, &tuio, 0, afs_osi_credp, code);
	    AFS_GLOCK();
	    u.u_cred = tmpcred;
	}
#elif defined(AFS_HPUX100_ENV)
	{
	    AFS_GUNLOCK();
	    code = VOP_RDWR(tfile->vnode, &tuio, UIO_WRITE, 0, afs_osi_credp);
	    AFS_GLOCK();
	}
#elif defined(AFS_LINUX20_ENV)
	AFS_GUNLOCK();
	code = osi_rdwr(tfile, &tuio, UIO_WRITE);
	AFS_GLOCK();
#elif defined(AFS_DARWIN80_ENV)
	AFS_GUNLOCK();
	code = VNOP_WRITE(tfile->vnode, tuiop, 0, afs_osi_ctxtp);
	AFS_GLOCK();
#elif defined(AFS_DARWIN_ENV)
	AFS_GUNLOCK();
	VOP_LOCK(tfile->vnode, LK_EXCLUSIVE, current_proc());
	code = VOP_WRITE(tfile->vnode, &tuio, 0, afs_osi_credp);
	VOP_UNLOCK(tfile->vnode, 0, current_proc());
	AFS_GLOCK();
#elif defined(AFS_FBSD50_ENV)
	AFS_GUNLOCK();
	VOP_LOCK(tfile->vnode, LK_EXCLUSIVE, curthread);
	code = VOP_WRITE(tfile->vnode, &tuio, 0, afs_osi_credp);
	VOP_UNLOCK(tfile->vnode, 0, curthread);
	AFS_GLOCK();
#elif defined(AFS_XBSD_ENV)
	AFS_GUNLOCK();
	VOP_LOCK(tfile->vnode, LK_EXCLUSIVE, curproc);
	code = VOP_WRITE(tfile->vnode, &tuio, 0, afs_osi_credp);
	VOP_UNLOCK(tfile->vnode, 0, curproc);
	AFS_GLOCK();
#else
#ifdef	AFS_HPUX_ENV
	tuio.uio_fpflags &= ~FSYNCIO;	/* don't do sync io */
#endif
	code = VOP_RDWR(tfile->vnode, &tuio, UIO_WRITE, 0, afs_osi_credp);
#endif
	if (code) {
	    error = code;
	    ZapDCE(tdc);	/* bad data */
	    osi_UFSTruncate(tfile, 0);	/* fake truncate the segment */
	    afs_AdjustSize(tdc, 0);	/* sets f.chunkSize to 0 */
	    afs_stats_cmperf.cacheCurrDirtyChunks--;
	    afs_indexFlags[tdc->index] &= ~IFDataMod;	/* so it does disappear */
	    afs_CFileClose(tfile);
	    ReleaseWriteLock(&tdc->lock);
	    afs_PutDCache(tdc);
	    break;
	}
	/* otherwise we've written some, fixup length, etc and continue with next seg */
	len = len - AFS_UIO_RESID(tuiop);	/* compute amount really transferred */
	tlen = len;
	afsio_skip(auio, tlen);	/* advance auio over data written */
	/* compute new file size */
	if (offset + len > tdc->f.chunkBytes) {
	    afs_int32 tlength = offset + len;
	    afs_AdjustSize(tdc, tlength);
	    if (tdc->validPos < filePos + len)
		tdc->validPos = filePos + len;
	}
	totalLength -= len;
	transferLength += len;
	filePos += len;
#if defined(AFS_SGI_ENV)
	/* afs_xwrite handles setting m.Length */
	osi_Assert(filePos <= avc->m.Length);
#else
	if (filePos > avc->m.Length) {
	    afs_Trace4(afs_iclSetp, CM_TRACE_SETLENGTH, ICL_TYPE_STRING,
		       __FILE__, ICL_TYPE_LONG, __LINE__, ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(avc->m.Length), ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(filePos));
	    avc->m.Length = filePos;
	}
#endif
	osi_UFSClose(tfile);
	ReleaseWriteLock(&tdc->lock);
	afs_PutDCache(tdc);
#if !defined(AFS_VM_RDWR_ENV)
	/*
	 * If write is implemented via VM, afs_DoPartialWrite() is called from
	 * the high-level write op.
	 */
	if (!noLock) {
	    code = afs_DoPartialWrite(avc, &treq);
	    if (code) {
		error = code;
		break;
	    }
	}
#endif
    }
#ifndef	AFS_VM_RDWR_ENV
    afs_FakeClose(avc, acred);
#endif
    error = afs_CheckCode(error, &treq, 7);
    /* This set is here so we get the CheckCode. */
    if (error && !avc->vc_error)
	avc->vc_error = error;
    if (!noLock)
	ReleaseWriteLock(&avc->lock);
#ifdef AFS_DARWIN80_ENV
    uio_free(tuiop);
#else
    osi_FreeSmallSpace(tvec);
#endif
#ifndef	AFS_VM_RDWR_ENV
    /*
     * If write is implemented via VM, afs_fsync() is called from the high-level
     * write op.
     */
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    if (noLock && (aio & IO_SYNC)) {
#else
#ifdef	AFS_HPUX_ENV
    /* On hpux on synchronous writes syncio will be set to IO_SYNC. If
     * we're doing them because the file was opened with O_SYNCIO specified,
     * we have to look in the u area. No single mechanism here!!
     */
    if (noLock && ((aio & IO_SYNC) | (auio->uio_fpflags & FSYNCIO))) {
#else
    if (noLock && (aio & FSYNC)) {
#endif
#endif
	if (!AFS_NFSXLATORREQ(acred))
	    afs_fsync(avc, acred);
    }
#endif
    return error;
}

/* do partial write if we're low on unmodified chunks */
int
afs_DoPartialWrite(register struct vcache *avc, struct vrequest *areq)
{
    register afs_int32 code;

    if (afs_stats_cmperf.cacheCurrDirtyChunks <=
	afs_stats_cmperf.cacheMaxDirtyChunks)
	return 0;		/* nothing to do */
    /* otherwise, call afs_StoreDCache (later try to do this async, if possible) */
    afs_Trace2(afs_iclSetp, CM_TRACE_PARTIALWRITE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->m.Length));
#if	defined(AFS_SUN5_ENV)
    code = afs_StoreAllSegments(avc, areq, AFS_ASYNC | AFS_VMSYNC_INVAL);
#else
    code = afs_StoreAllSegments(avc, areq, AFS_ASYNC);
#endif
    return code;
}

#ifdef AFS_OSF_ENV
#ifdef AFS_DUX50_ENV
#define vno_close(X) vn_close((X), 0, NOCRED)
#elif defined(AFS_DUX40_ENV)
#define      vno_close       vn_close
#endif
/* We don't need this for AIX since: 
 * (1) aix doesn't use fileops and it call close directly intead
 * (where the unlocking should be done) and 
 * (2) temporarily, the aix lockf isn't supported yet.
 *
 *  this stupid routine is used to release the flocks held on a
 *  particular file descriptor.  Sun doesn't pass file descr. info
 *  through to the vnode layer, and yet we must unlock flocked files
 *  on the *appropriate* (not first, as in System V) close call.  Thus
 *  this code.
 * How does this code get invoked? The afs AFS_FLOCK plugs in the new afs
 * file ops structure into any afs file when it gets flocked. 
 * N.B: Intercepting close syscall doesn't trap aborts or exit system
 * calls.
*/
int
afs_closex(register struct file *afd)
{
    struct vrequest treq;
    struct vcache *tvc;
    afs_int32 flags;
    int closeDone;
    afs_int32 code = 0;
    struct afs_fakestat_state fakestat;

    AFS_STATCNT(afs_closex);
    /* setup the credentials */
    if ((code = afs_InitReq(&treq, u.u_cred)))
	return code;
    afs_InitFakeStat(&fakestat);

    closeDone = 0;
    /* we're the last one.  If we're an AFS vnode, clear the flags,
     * close the file and release the lock when done.  Otherwise, just
     * let the regular close code work.      */
    if (afd->f_type == DTYPE_VNODE) {
	tvc = VTOAFS(afd->f_data);
	if (IsAfsVnode(AFSTOV(tvc))) {
	    code = afs_EvalFakeStat(&tvc, &fakestat, &treq);
	    if (code) {
		afs_PutFakeStat(&fakestat);
		return code;
	    }
	    VN_HOLD(AFSTOV(tvc));
	    flags = afd->f_flag & (FSHLOCK | FEXLOCK);
	    afd->f_flag &= ~(FSHLOCK | FEXLOCK);
	    code = vno_close(afd);
	    if (flags)
		HandleFlock(tvc, LOCK_UN, &treq, u.u_procp->p_pid,
			    1 /*onlymine */ );
	    AFS_RELE(AFSTOV(tvc));
	    closeDone = 1;
	}
    }
    /* now, if close not done, do it */
    if (!closeDone) {
	code = vno_close(afd);
    }
    afs_PutFakeStat(&fakestat);
    return code;		/* return code from vnode layer */
}
#endif


/* handle any closing cleanup stuff */
int
#ifdef	AFS_SGI_ENV
afs_close(OSI_VC_ARG(avc), aflags, lastclose,
#if !defined(AFS_SGI65_ENV)
	  offset,
#endif
	  acred
#if defined(AFS_SGI64_ENV) && !defined(AFS_SGI65_ENV)
	  , flp
#endif
    )
     lastclose_t lastclose;
#if !defined(AFS_SGI65_ENV)
     off_t offset;
#if defined(AFS_SGI64_ENV)
     struct flid *flp;
#endif
#endif
#elif defined(AFS_SUN5_ENV)
afs_close(OSI_VC_ARG(avc), aflags, count, offset, acred)
     offset_t offset;
     int count;
#else
afs_close(OSI_VC_ARG(avc), aflags, acred)
#endif
     OSI_VC_DECL(avc);
     afs_int32 aflags;
     struct AFS_UCRED *acred;
{
    register afs_int32 code;
    register struct brequest *tb;
    struct vrequest treq;
#ifdef AFS_SGI65_ENV
    struct flid flid;
#endif
    struct afs_fakestat_state fakestat;
    OSI_VC_CONVERT(avc);

    AFS_STATCNT(afs_close);
    afs_Trace2(afs_iclSetp, CM_TRACE_CLOSE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, aflags);
    code = afs_InitReq(&treq, acred);
    if (code)
	return code;
    afs_InitFakeStat(&fakestat);
    code = afs_EvalFakeStat(&avc, &fakestat, &treq);
    if (code) {
	afs_PutFakeStat(&fakestat);
	return code;
    }
#ifdef	AFS_SUN5_ENV
    if (avc->flockCount) {
	HandleFlock(avc, LOCK_UN, &treq, 0, 1 /*onlymine */ );
    }
#endif
#if defined(AFS_SGI_ENV)
    if (!lastclose) {
	afs_PutFakeStat(&fakestat);
	return 0;
    }
    /* unlock any locks for pid - could be wrong for child .. */
    AFS_RWLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#ifdef AFS_SGI65_ENV
    get_current_flid(&flid);
    cleanlocks((vnode_t *) avc, flid.fl_pid, flid.fl_sysid);
    HandleFlock(avc, LOCK_UN, &treq, flid.fl_pid, 1 /*onlymine */ );
#else
#ifdef AFS_SGI64_ENV
    cleanlocks((vnode_t *) avc, flp);
#else /* AFS_SGI64_ENV */
    cleanlocks((vnode_t *) avc, u.u_procp->p_epid, u.u_procp->p_sysid);
#endif /* AFS_SGI64_ENV */
    HandleFlock(avc, LOCK_UN, &treq, OSI_GET_CURRENT_PID(), 1 /*onlymine */ );
#endif /* AFS_SGI65_ENV */
    /* afs_chkpgoob will drop and re-acquire the global lock. */
    afs_chkpgoob(&avc->v, btoc(avc->m.Length));
#elif	defined(AFS_SUN5_ENV)
    if (count > 1) {
	/* The vfs layer may call this repeatedly with higher "count"; only on the last close (i.e. count = 1) we should actually proceed with the close. */
	afs_PutFakeStat(&fakestat);
	return 0;
    }
#else /* AFS_SGI_ENV */
    if (avc->flockCount) {	/* Release Lock */
#if	defined(AFS_OSF_ENV) 
	HandleFlock(avc, LOCK_UN, &treq, u.u_procp->p_pid, 1 /*onlymine */ );
#else
	HandleFlock(avc, LOCK_UN, &treq, 0, 1 /*onlymine */ );
#endif
    }
#endif /* AFS_SGI_ENV */
    if (aflags & (FWRITE | FTRUNC)) {
	if (afs_BBusy()) {
	    /* do it yourself if daemons are all busy */
	    ObtainWriteLock(&avc->lock, 124);
	    code = afs_StoreOnLastReference(avc, &treq);
	    ReleaseWriteLock(&avc->lock);
#if defined(AFS_SGI_ENV)
	    AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#endif
	} else {
#if defined(AFS_SGI_ENV)
	    AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_WRITE);
#endif
	    /* at least one daemon is idle, so ask it to do the store.
	     * Also, note that  we don't lock it any more... */
	    tb = afs_BQueue(BOP_STORE, avc, 0, 1, acred,
			    (afs_size_t) acred->cr_uid, (afs_size_t) 0,
			    (void *)0);
	    /* sleep waiting for the store to start, then retrieve error code */
	    while ((tb->flags & BUVALID) == 0) {
		tb->flags |= BUWAIT;
		afs_osi_Sleep(tb);
	    }
	    code = tb->code;
	    afs_BRelease(tb);
	}

	/* VNOVNODE is "acceptable" error code from close, since
	 * may happen when deleting a file on another machine while
	 * it is open here. We do the same for ENOENT since in afs_CheckCode we map VNOVNODE -> ENOENT */
	if (code == VNOVNODE || code == ENOENT)
	    code = 0;

	/* Ensure last closer gets the error. If another thread caused
	 * DoPartialWrite and this thread does not actually store the data,
	 * it may not see the quota error.
	 */
	ObtainWriteLock(&avc->lock, 406);
	if (avc->vc_error) {
#ifdef AFS_AIX32_ENV
	    osi_ReleaseVM(avc, acred);
#endif
	    code = avc->vc_error;
	    avc->vc_error = 0;
	}
	ReleaseWriteLock(&avc->lock);

	/* some codes merit specific complaint */
	if (code < 0) {
	    afs_warnuser("afs: failed to store file (network problems)\n");
	}
#ifdef	AFS_SUN5_ENV
	else if (code == ENOSPC) {
	    afs_warnuser
		("afs: failed to store file (over quota or partition full)\n");
	}
#else
	else if (code == ENOSPC) {
	    afs_warnuser("afs: failed to store file (partition full)\n");
	} else if (code == EDQUOT) {
	    afs_warnuser("afs: failed to store file (over quota)\n");
	}
#endif
	else if (code != 0)
	    afs_warnuser("afs: failed to store file (%d)\n", code);

	/* finally, we flush any text pages lying around here */
	hzero(avc->flushDV);
	osi_FlushText(avc);
    } else {
#if defined(AFS_SGI_ENV)
	AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_WRITE);
	osi_Assert(avc->opens > 0);
#endif
	/* file open for read */
	ObtainWriteLock(&avc->lock, 411);
	if (avc->vc_error) {
#ifdef AFS_AIX32_ENV
	    osi_ReleaseVM(avc, acred);
#endif
	    code = avc->vc_error;
	    avc->vc_error = 0;
	}
	avc->opens--;
	ReleaseWriteLock(&avc->lock);
    }
#ifdef	AFS_OSF_ENV
    if ((VREFCOUNT(avc) <= 2) && (avc->states & CUnlinked)) {
	afs_remunlink(avc, 1);	/* ignore any return code */
    }
#endif
    afs_PutFakeStat(&fakestat);
    code = afs_CheckCode(code, &treq, 5);
    return code;
}


int
#ifdef	AFS_OSF_ENV
afs_fsync(OSI_VC_DECL(avc), int fflags, struct AFS_UCRED *acred, int waitfor)
#else				/* AFS_OSF_ENV */
#if defined(AFS_SGI_ENV) || defined(AFS_SUN53_ENV)
afs_fsync(OSI_VC_DECL(avc), int flag, struct AFS_UCRED *acred
#ifdef AFS_SGI65_ENV
	  , off_t start, off_t stop
#endif /* AFS_SGI65_ENV */
    )
#else /* !OSF && !SUN53 && !SGI */
afs_fsync(OSI_VC_DECL(avc), struct AFS_UCRED *acred)
#endif 
#endif
{
    register afs_int32 code;
    struct vrequest treq;
    OSI_VC_CONVERT(avc);

    if (avc->vc_error)
	return avc->vc_error;

#if defined(AFS_SUN5_ENV)
    /* back out if called from NFS server */
    if (curthread->t_flag & T_DONTPEND)
	return 0;
#endif

    AFS_STATCNT(afs_fsync);
    afs_Trace1(afs_iclSetp, CM_TRACE_FSYNC, ICL_TYPE_POINTER, avc);
    if ((code = afs_InitReq(&treq, acred)))
	return code;

#if defined(AFS_SGI_ENV)
    AFS_RWLOCK((vnode_t *) avc, VRWLOCK_WRITE);
    if (flag & FSYNC_INVAL)
	osi_VM_FSyncInval(avc);
#endif /* AFS_SGI_ENV */

    ObtainSharedLock(&avc->lock, 18);
    code = 0;
    if (avc->execsOrWriters > 0) {
	/* put the file back */
	UpgradeSToWLock(&avc->lock, 41);
	code = afs_StoreAllSegments(avc, &treq, AFS_SYNC);
	ConvertWToSLock(&avc->lock);
    }
#if defined(AFS_SGI_ENV)
    AFS_RWUNLOCK((vnode_t *) avc, VRWLOCK_WRITE);
    if (code == VNOVNODE) {
	/* syncing an unlinked file! - non-informative to pass an errno
	 * 102 (== VNOVNODE) to user
	 */
	code = ENOENT;
    }
#endif

    code = afs_CheckCode(code, &treq, 33);
    ReleaseSharedLock(&avc->lock);
    return code;
}
