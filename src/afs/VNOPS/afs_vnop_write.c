/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/* afs_vnop_write.c
 *
 * Implements:
 * afs_UFSWrite
 * afs_MemWrite
 * afs_StoreOnLastReference
 * afs_close
 * afs_closex
 * afs_fsync
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h" /* statistics */
#include "../afs/afs_cbqueue.h"
#include "../afs/nfsclient.h"
#include "../afs/afs_osidnlc.h"


extern unsigned char *afs_indexFlags;

/* Called by all write-on-close routines: regular afs_close,
 * store via background daemon and store via the
 * afs_FlushActiveVCaches routine (when CCORE is on).
 * avc->lock must be write-locked.
 */
afs_StoreOnLastReference(avc, treq)
register struct vcache *avc;
register struct vrequest *treq;
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
	AFS_RELE((struct vnode *)avc); /* VN_HOLD at set CCore(afs_FakeClose)*/
	crfree((struct AFS_UCRED *)avc->linkData);	/* "crheld" in afs_FakeClose */
	avc->linkData =	(char *)0;
    }
    /* Now, send the file back.  Used to require 0 writers left, but now do
     * it on every close for write, since two closes in a row are harmless
     * since first will clean all chunks, and second will be noop.  Note that
     * this will also save confusion when someone keeps a file open 
     * inadvertently, since with old system, writes to the server would never
     * happen again. 
     */
    code = afs_StoreAllSegments(avc, treq, AFS_LASTSTORE/*!sync-to-disk*/);
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



afs_MemWrite(avc, auio, aio, acred, noLock)
    register struct vcache *avc;
    struct uio *auio;
    int aio, noLock;
    struct AFS_UCRED *acred; {
    afs_int32 totalLength;
    afs_int32 transferLength;
    afs_int32 filePos;
    afs_int32 startDate;
    afs_int32 max;
    register struct dcache *tdc;
#ifdef _HIGHC_
    volatile
#endif
    afs_int32 offset, len, error;
    struct uio tuio;
    struct iovec *tvec;  /* again, should have define */
    char *tfile;
    register afs_int32 code;
    struct vrequest treq;

    AFS_STATCNT(afs_MemWrite);
    if (avc->vc_error)
 	return avc->vc_error;

    startDate = osi_Time();
    if (code = afs_InitReq(&treq, acred)) return code;
    /* otherwise we read */
    totalLength = auio->afsio_resid;
    filePos = auio->afsio_offset;
    error = 0;
    transferLength = 0;
    afs_Trace4(afs_iclSetp, CM_TRACE_WRITE, ICL_TYPE_POINTER, avc, 
	       ICL_TYPE_INT32, filePos, ICL_TYPE_INT32, totalLength,
	       ICL_TYPE_INT32, avc->m.Length);
    if (!noLock) {
	afs_MaybeWakeupTruncateDaemon();
	ObtainWriteLock(&avc->lock,126);
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
    auio->afsio_resid = MIN(totalLength, diff);
    totalLength = auio->afsio_resid;
    }
#else
    if (aio & IO_APPEND) {
	/* append mode, start it at the right spot */
#if	defined(AFS_SUN56_ENV)
	auio->uio_loffset = 0;
#endif
	filePos = auio->afsio_offset = avc->m.Length;
    }
#endif
    /*
     * Note that we use startDate rather than calling osi_Time() here.
     * This is to avoid counting lock-waiting time in file date (for ranlib).
     */
    avc->m.Date	= startDate;

#if	defined(AFS_HPUX_ENV) || defined(AFS_GFS_ENV)
#if	defined(AFS_HPUX101_ENV)
    if ((totalLength + filePos) >> 9 > (p_rlimit(u.u_procp))[RLIMIT_FSIZE].rlim_cur) {
#else
#ifdef	AFS_HPUX_ENV
    if ((totalLength + filePos) >> 9 > u.u_rlimit[RLIMIT_FSIZE].rlim_cur) {
#else
    if (totalLength + filePos > u.u_rlimit[RLIMIT_FSIZE].rlim_cur) {
#endif
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
	printf("WARNING: afs_ufswr vp=%x, exOrW=%d\n", avc, avc->execsOrWriters);
    }
#else
    afs_FakeOpen(avc);
#endif
    avc->states |= CDirty;
    tvec = (struct iovec *) osi_AllocSmallSpace(sizeof(struct iovec));
    while (totalLength > 0) {
	/* Read the cached info. If we call GetDCache while the cache
	 * truncate daemon is running we risk overflowing the disk cache.
	 * Instead we check for an existing cache slot. If we cannot
	 * find an existing slot we wait for the cache to drain
	 * before calling GetDCache.
	 */
	if (noLock) {
	    tdc = afs_FindDCache(avc, filePos);
	    if (tdc) {
		offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
		len = tdc->f.chunkBytes - offset;
	    }
	} else if (afs_blocksUsed > (CM_WAITFORDRAINPCT*afs_cacheBlocks)/100) {
	    tdc = afs_FindDCache(avc, filePos);
	    if (tdc) {
		if (!hsame(tdc->f.versionNo, avc->m.DataVersion) ||
		    (tdc->flags & DFFetching)) {
		    afs_PutDCache(tdc);
		    tdc = NULL;
		} else {
		    offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
		    len = tdc->f.chunkBytes - offset;
		}
	    }
	    if (!tdc) {
		afs_MaybeWakeupTruncateDaemon();
		while (afs_blocksUsed >
		       (CM_WAITFORDRAINPCT*afs_cacheBlocks)/100) {
		    ReleaseWriteLock(&avc->lock);
		    if (afs_blocksUsed - afs_blocksDiscarded >
			(CM_WAITFORDRAINPCT*afs_cacheBlocks)/100) {
			afs_WaitForCacheDrain = 1;
			afs_osi_Sleep(&afs_WaitForCacheDrain);
		    }
		    afs_MaybeFreeDiscardedDCache();
		    afs_MaybeWakeupTruncateDaemon();
		    ObtainWriteLock(&avc->lock,506);
		}
		avc->states |= CDirty;
		tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 4);
	    }
	} else {
	    tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 4);
	}
	if (!tdc) {
	    error = EIO;
	    break;
	}
	if (!(afs_indexFlags[tdc->index] & IFDataMod)) {
	  afs_stats_cmperf.cacheCurrDirtyChunks++;
	  afs_indexFlags[tdc->index] |= IFDataMod;    /* so it doesn't disappear */
	}
	if (!(tdc->f.states & DWriting)) {
	    /* don't mark entry as mod if we don't have to */
	    tdc->f.states |= DWriting;
	    tdc->flags |= DFEntryMod;
	}
	len = totalLength;	/* write this amount by default */
	max = AFS_CHUNKTOSIZE(tdc->f.chunk);	/* max size of this chunk */
	if (max	<= len + offset)	{   /*if we'd go past the end of this chunk */
	    /* it won't all fit in this chunk, so write as much
		as will fit */
	    len = max - offset;
	}
	/* mung uio structure to be right for this transfer */
	afsio_copy(auio, &tuio, tvec);
	afsio_trim(&tuio, len);
	tuio.afsio_offset = offset;

	code = afs_MemWriteUIO(tdc->f.inode, &tuio);
	if (code) {
	    error = code;
	    ZapDCE(tdc);		/* bad data */
	    afs_MemCacheTruncate(tdc->f.inode, 0);
	    afs_stats_cmperf.cacheCurrDirtyChunks--;
	    afs_indexFlags[tdc->index] &= ~IFDataMod;    /* so it does disappear */
	    afs_PutDCache(tdc);
	    break;
	}
	/* otherwise we've written some, fixup length, etc and continue with next seg */
	len = len - tuio.afsio_resid; /* compute amount really transferred */
	afsio_skip(auio, len);	    /* advance auio over data written */
	/* compute new file size */
	if (offset + len > tdc->f.chunkBytes)
	    afs_AdjustSize(tdc, offset+len);
	totalLength -= len;
	transferLength += len;
	filePos += len;
#if defined(AFS_SGI_ENV)
        /* afs_xwrite handles setting m.Length */
        osi_Assert(filePos <= avc->m.Length);
#else
	if (filePos > avc->m.Length)
	    avc->m.Length = filePos;
#endif
#ifndef	AFS_VM_RDWR_ENV
	/*
	 * If write is implemented via VM, afs_DoPartialWrite() is called from
	 * the high-level write op.
	 */
	if (!noLock) {
	    code = afs_DoPartialWrite(avc, &treq);
	    if (code) {
		error = code;
		afs_PutDCache(tdc);
		break;
	    }
	}
#endif
	afs_PutDCache(tdc);
    }
#ifndef	AFS_VM_RDWR_ENV
    afs_FakeClose(avc, acred);
#endif
    if (error && !avc->vc_error)
	avc->vc_error = error;
    if (!noLock)
	ReleaseWriteLock(&avc->lock);
    osi_FreeSmallSpace(tvec);
#ifdef AFS_DEC_ENV
    /* next, on GFS systems, we update g_size so that lseek's relative to EOF will
       work.  GFS is truly a poorly-designed interface!  */
    afs_gfshack((struct gnode *) avc);
#endif
    error = afs_CheckCode(error, &treq, 6);
    return error;
}


/* called on writes */
afs_UFSWrite(avc, auio, aio, acred, noLock)
    register struct vcache *avc;
    struct uio *auio;
    int aio, noLock;
    struct AFS_UCRED *acred; {
    afs_int32 totalLength;
    afs_int32 transferLength;
    afs_int32 filePos;
    afs_int32 startDate;
    afs_int32 max;
    register struct dcache *tdc;
#ifdef _HIGHC_
    volatile
#endif
    afs_int32 offset, len, error;
    struct uio tuio;
    struct iovec *tvec;  /* again, should have define */
    struct osi_file *tfile;
    register afs_int32 code;
    struct vnode *vp;
    struct vrequest treq;

    AFS_STATCNT(afs_UFSWrite);
    if (avc->vc_error)
 	return avc->vc_error;

    startDate = osi_Time();
    if (code = afs_InitReq(&treq, acred)) return code;
    /* otherwise we read */
    totalLength = auio->afsio_resid;
    filePos = auio->afsio_offset;
    error = 0;
    transferLength = 0;
    afs_Trace4(afs_iclSetp, CM_TRACE_WRITE, ICL_TYPE_POINTER, avc, 
	       ICL_TYPE_INT32, filePos, ICL_TYPE_INT32, totalLength,
	       ICL_TYPE_INT32, avc->m.Length);
    if (!noLock) {
	afs_MaybeWakeupTruncateDaemon();
	ObtainWriteLock(&avc->lock,556);
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
    auio->afsio_resid = MIN(totalLength, diff);
    totalLength = auio->afsio_resid;
    }
#else
    if (aio & IO_APPEND) {
	/* append mode, start it at the right spot */
#if     defined(AFS_SUN56_ENV)
        auio->uio_loffset = 0;
#endif
	filePos = auio->afsio_offset = avc->m.Length;
    }
#endif
    /*
     * Note that we use startDate rather than calling osi_Time() here.
     * This is to avoid counting lock-waiting time in file date (for ranlib).
     */
    avc->m.Date	= startDate;

#if	defined(AFS_HPUX_ENV) || defined(AFS_GFS_ENV)
#if 	defined(AFS_HPUX101_ENV)
    if ((totalLength + filePos) >> 9 > p_rlimit(u.u_procp)[RLIMIT_FSIZE].rlim_cur) {
#else
#ifdef	AFS_HPUX_ENV
    if ((totalLength + filePos) >> 9 > u.u_rlimit[RLIMIT_FSIZE].rlim_cur) {
#else
    if (totalLength + filePos > u.u_rlimit[RLIMIT_FSIZE].rlim_cur) {
#endif
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
	printf("WARNING: afs_ufswr vp=%x, exOrW=%d\n", avc, avc->execsOrWriters);
    }
#else
    afs_FakeOpen(avc);
#endif
    avc->states |= CDirty;
    tvec = (struct iovec *) osi_AllocSmallSpace(sizeof(struct iovec));
    while (totalLength > 0) {
	/* read the cached info */
	if (noLock) {
	    tdc = afs_FindDCache(avc, filePos);
	    if (tdc) {
		offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
		len = tdc->f.chunkBytes - offset;
	    }
	} else if (afs_blocksUsed > (CM_WAITFORDRAINPCT*afs_cacheBlocks)/100) {
	    tdc = afs_FindDCache(avc, filePos);
	    if (tdc) {
		if (!hsame(tdc->f.versionNo, avc->m.DataVersion) ||
		    (tdc->flags & DFFetching)) {
		    afs_PutDCache(tdc);
		    tdc = NULL;
		} else {
		    offset = filePos - AFS_CHUNKTOBASE(tdc->f.chunk);
		    len = tdc->f.chunkBytes - offset;
		}
	    }
	    if (!tdc) {
		afs_MaybeWakeupTruncateDaemon();
		while (afs_blocksUsed >
		       (CM_WAITFORDRAINPCT*afs_cacheBlocks)/100) {
		    ReleaseWriteLock(&avc->lock);
		    if (afs_blocksUsed - afs_blocksDiscarded >
			(CM_WAITFORDRAINPCT*afs_cacheBlocks)/100) {
			afs_WaitForCacheDrain = 1;
			afs_osi_Sleep(&afs_WaitForCacheDrain);
		    }
		    afs_MaybeFreeDiscardedDCache();
		    afs_MaybeWakeupTruncateDaemon();
		    ObtainWriteLock(&avc->lock,509);
		}
		avc->states |= CDirty;
		tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 4);
	    }
	} else {
	    tdc = afs_GetDCache(avc, filePos, &treq, &offset, &len, 4);
	}
	if (!tdc) {
	    error = EIO;
	    break;
	}
	if (!(afs_indexFlags[tdc->index] & IFDataMod)) {
	  afs_stats_cmperf.cacheCurrDirtyChunks++;
	  afs_indexFlags[tdc->index] |= IFDataMod;    /* so it doesn't disappear */
	}
	if (!(tdc->f.states & DWriting)) {
	    /* don't mark entry as mod if we don't have to */
	    tdc->f.states |= DWriting;
	    tdc->flags |= DFEntryMod;
	}
	tfile = (struct osi_file *)osi_UFSOpen(tdc->f.inode);
	len = totalLength;	/* write this amount by default */
	max = AFS_CHUNKTOSIZE(tdc->f.chunk);	/* max size of this chunk */
	if (max	<= len + offset)	{   /*if we'd go past the end of this chunk */
	    /* it won't all fit in this chunk, so write as much
		as will fit */
	    len = max - offset;
	}
	/* mung uio structure to be right for this transfer */
	afsio_copy(auio, &tuio, tvec);
	afsio_trim(&tuio, len);
	tuio.afsio_offset = offset;
#ifdef	AFS_AIX_ENV
#ifdef	AFS_AIX41_ENV
	AFS_GUNLOCK();
	code = VNOP_RDWR(tfile->vnode, UIO_WRITE, FWRITE, &tuio, NULL, NULL, NULL, &afs_osi_cred);
	AFS_GLOCK();
#else
#ifdef AFS_AIX32_ENV
	code = VNOP_RDWR(tfile->vnode, UIO_WRITE, FWRITE, &tuio, NULL, NULL);
#else
	code = VNOP_RDWR(tfile->vnode, UIO_WRITE, FWRITE, (off_t)&offset, &tuio, NULL, NULL, -1);
#endif
#endif /* AFS_AIX41_ENV */
#else /* AFS_AIX_ENV */
#ifdef	AFS_SUN5_ENV
	AFS_GUNLOCK();
	VOP_RWLOCK(tfile->vnode, 1);
	code = VOP_WRITE(tfile->vnode, &tuio, 0, &afs_osi_cred);
	VOP_RWUNLOCK(tfile->vnode, 1);
	AFS_GLOCK();
	if (code == ENOSPC) afs_warnuser("\n\n\n*** Cache partition is full - decrease cachesize!!! ***\n\n\n"); 
#else
#if defined(AFS_SGI_ENV)
	AFS_GUNLOCK();
        avc->states |= CWritingUFS;
	AFS_VOP_RWLOCK(tfile->vnode, VRWLOCK_WRITE);
	AFS_VOP_WRITE(tfile->vnode, &tuio, IO_ISLOCKED, &afs_osi_cred, code);
	AFS_VOP_RWUNLOCK(tfile->vnode, VRWLOCK_WRITE);
        avc->states &= ~CWritingUFS;
	AFS_GLOCK();
#else
#ifdef	AFS_OSF_ENV
    {
	struct ucred *tmpcred = u.u_cred;
	u.u_cred = &afs_osi_cred;
	tuio.uio_rw = UIO_WRITE;
	AFS_GUNLOCK();
	VOP_WRITE(tfile->vnode, &tuio, 0, &afs_osi_cred, code);
	AFS_GLOCK();
	u.u_cred = tmpcred;
    }
#else	/* AFS_OSF_ENV */
#if defined(AFS_HPUX100_ENV)
    {
	AFS_GUNLOCK();
	code = VOP_RDWR(tfile->vnode, &tuio, UIO_WRITE, 0, &afs_osi_cred);
	AFS_GLOCK();
    }
#else
#ifdef	AFS_HPUX_ENV
	tuio.uio_fpflags &= ~FSYNCIO;	/* don't do sync io */
#endif
#if defined(AFS_LINUX20_ENV)
	AFS_GUNLOCK();
	code = osi_file_uio_rdwr(tfile, &tuio, UIO_WRITE);
	AFS_GLOCK();
#else
	code = VOP_RDWR(tfile->vnode, &tuio, UIO_WRITE, 0, &afs_osi_cred);
#endif /* AFS_LINUX20_ENV */
#endif /* AFS_HPUX100_ENV */
#endif /* AFS_OSF_ENV */
#endif /* AFS_SGI_ENV */
#endif /* AFS_SUN5_ENV */
#endif /* AFS_AIX41_ENV */
	if (code) {
	    error = code;
	    ZapDCE(tdc);		/* bad data */
	    osi_UFSTruncate(tfile,0);	/* fake truncate the segment */
	    afs_AdjustSize(tdc,	0);	/* sets f.chunkSize to 0 */
	    afs_stats_cmperf.cacheCurrDirtyChunks--;
	    afs_indexFlags[tdc->index] &= ~IFDataMod;    /* so it does disappear */
	    afs_PutDCache(tdc);
	    afs_CFileClose(tfile);
	    break;
	}
	/* otherwise we've written some, fixup length, etc and continue with next seg */
	len = len - tuio.afsio_resid; /* compute amount really transferred */
	afsio_skip(auio, len);	    /* advance auio over data written */
	/* compute new file size */
	if (offset + len > tdc->f.chunkBytes)
	    afs_AdjustSize(tdc, offset+len);
	totalLength -= len;
	transferLength += len;
	filePos += len;
#if defined(AFS_SGI_ENV)
        /* afs_xwrite handles setting m.Length */
        osi_Assert(filePos <= avc->m.Length);
#else
	if (filePos > avc->m.Length) {
	    avc->m.Length = filePos;
	}
#endif
	osi_UFSClose(tfile);
#ifndef	AFS_VM_RDWR_ENV
	/*
	 * If write is implemented via VM, afs_DoPartialWrite() is called from
	 * the high-level write op.
	 */
	if (!noLock) {
	    code = afs_DoPartialWrite(avc, &treq);
	    if (code) {
		error = code;
		afs_PutDCache(tdc);
		break;
	    }
	}
#endif
	afs_PutDCache(tdc);
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
    osi_FreeSmallSpace(tvec);
#ifdef AFS_DEC_ENV
    /* next, on GFS systems, we update g_size so that lseek's relative to EOF will
       work.  GFS is truly a poorly-designed interface!  */
    afs_gfshack((struct gnode *) avc);
#endif
#ifndef	AFS_VM_RDWR_ENV
    /*
     * If write is implemented via VM, afs_fsync() is called from the high-level
     * write op.
     */
#ifdef	AFS_HPUX_ENV
    /* On hpux on synchronous writes syncio will be set to IO_SYNC. If
     * we're doing them because the file was opened with O_SYNCIO specified,
     * we have to look in the u area. No single mechanism here!!
     */
    if (noLock && ((aio & IO_SYNC) | (auio->uio_fpflags & FSYNCIO))) {    
#else
    if (noLock && (aio & FSYNC)) {
#endif
	if (!AFS_NFSXLATORREQ(acred))
	    afs_fsync(avc, acred);
    }
#endif
    return error;
}

/* do partial write if we're low on unmodified chunks */
afs_DoPartialWrite(avc, areq)
register struct vcache *avc;
struct vrequest *areq; {
    register afs_int32 code;

    if (afs_stats_cmperf.cacheCurrDirtyChunks <= afs_stats_cmperf.cacheMaxDirtyChunks) 
	return 0;	/* nothing to do */
    /* otherwise, call afs_StoreDCache (later try to do this async, if possible) */
    afs_Trace2(afs_iclSetp, CM_TRACE_PARTIALWRITE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->m.Length);
#if	defined(AFS_SUN5_ENV)
    code = afs_StoreAllSegments(avc, areq, AFS_ASYNC | AFS_VMSYNC_INVAL);
#else
    code = afs_StoreAllSegments(avc, areq, AFS_ASYNC);
#endif
    return code;
}



#if !defined (AFS_AIX_ENV) && !defined (AFS_HPUX_ENV) && !defined (AFS_SUN5_ENV) && !defined(AFS_SGI_ENV) && !defined(AFS_LINUX20_ENV)
#ifdef AFS_DUX40_ENV
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
afs_closex(afd)
    register struct file *afd; {
    struct vrequest treq;
    register struct vcache *tvc;
    afs_int32 flags;
    int closeDone;
    afs_int32 code = 0;

    AFS_STATCNT(afs_closex);
    /* setup the credentials */
    if (code = afs_InitReq(&treq, u.u_cred)) return code;

    closeDone = 0;
    /* we're the last one.  If we're an AFS vnode, clear the flags,
     * close the file and release the lock when done.  Otherwise, just
     * let the regular close code work.      */
    if (afd->f_type == DTYPE_VNODE) {
	tvc = (struct vcache *) afd->f_data;
	if (IsAfsVnode((struct vnode *)tvc)) {
	    VN_HOLD((struct vnode *) tvc);
	    flags = afd->f_flag & (FSHLOCK | FEXLOCK);
	    afd->f_flag &= ~(FSHLOCK | FEXLOCK);
	    code = vno_close(afd);
	    if (flags) 
#if defined(AFS_SGI_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
		HandleFlock(tvc, LOCK_UN, &treq,
			    u.u_procp->p_pid, 1/*onlymine*/);
#else
		HandleFlock(tvc, LOCK_UN, &treq, 0, 1/*onlymine*/);
#endif
#ifdef	AFS_DEC_ENV
	    grele((struct gnode *) tvc);
#else
	    AFS_RELE((struct vnode *) tvc);
#endif
	    closeDone = 1;
	}
    }
    /* now, if close not done, do it */
    if (!closeDone) {
	code = vno_close(afd);
    }
    return code;	/* return code from vnode layer */
}
#endif


/* handle any closing cleanup stuff */
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
#else /* SGI */
#if	defined(AFS_SUN_ENV) || defined(AFS_SUN5_ENV)
#ifdef	AFS_SUN5_ENV
afs_close(OSI_VC_ARG(avc), aflags, count, offset, acred)
    offset_t offset;
#else
afs_close(OSI_VC_ARG(avc), aflags, count, acred)
#endif
int count;
#else
afs_close(OSI_VC_ARG(avc), aflags, acred)
#endif
#endif
    OSI_VC_DECL(avc);
    afs_int32 aflags;
    struct AFS_UCRED *acred; 
{
    register afs_int32 code, initreq=0;
    register struct brequest *tb;
    struct vrequest treq;
#ifdef AFS_SGI65_ENV
    struct flid flid;
#endif
    OSI_VC_CONVERT(avc)

    AFS_STATCNT(afs_close);
    afs_Trace2(afs_iclSetp, CM_TRACE_CLOSE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, aflags);
#ifdef	AFS_SUN5_ENV
    if (avc->flockCount) {
	if (code = afs_InitReq(&treq, acred)) return code;
	initreq = 1;
	HandleFlock(avc, LOCK_UN, &treq, 0, 1/*onlymine*/);
    }
#endif
#if defined(AFS_SGI_ENV)
    if (!lastclose)
 	return 0;
#else
#if	defined(AFS_SUN_ENV) || defined(AFS_SGI_ENV)
    if (count > 1) {
	/* The vfs layer may call this repeatedly with higher "count"; only on the last close (i.e. count = 1) we should actually proceed with the close. */
	return 0;
    }
#endif
#ifdef	AFS_SUN5_ENV
    if (!initreq) {
#endif
#endif
	if (code = afs_InitReq(&treq, acred)) return code;
#ifdef	AFS_SUN5_ENV
    }
#endif
#ifndef	AFS_SUN5_ENV
#if defined(AFS_SGI_ENV)
    /* unlock any locks for pid - could be wrong for child .. */
    AFS_RWLOCK((vnode_t *)avc, VRWLOCK_WRITE);
#ifdef AFS_SGI65_ENV
    get_current_flid(&flid);
    cleanlocks((vnode_t *)avc, flid.fl_pid, flid.fl_sysid);
    HandleFlock(avc, LOCK_UN, &treq, flid.fl_pid, 1/*onlymine*/);
#else
#ifdef AFS_SGI64_ENV
    cleanlocks((vnode_t *)avc, flp);
#else /* AFS_SGI64_ENV */
    cleanlocks((vnode_t *)avc, u.u_procp->p_epid, u.u_procp->p_sysid);
#endif /* AFS_SGI64_ENV */
    HandleFlock(avc, LOCK_UN, &treq, OSI_GET_CURRENT_PID(), 1/*onlymine*/);
#endif /* AFS_SGI65_ENV */
    /* afs_chkpgoob will drop and re-acquire the global lock. */
    afs_chkpgoob(&avc->v, btoc(avc->m.Length));
#else
    if (avc->flockCount) {		/* Release Lock */
#if	defined(AFS_OSF_ENV) || defined(AFS_SUN_ENV)
	HandleFlock(avc, LOCK_UN, &treq, u.u_procp->p_pid, 1/*onlymine*/);
#else
	HandleFlock(avc, LOCK_UN, &treq, 0, 1/*onlymine*/);
#endif
    }
#endif
#endif
    if (aflags & (FWRITE | FTRUNC)) {
	if (afs_BBusy()) {
	    /* do it yourself if daemons are all busy */
	    ObtainWriteLock(&avc->lock,124);
	    code = afs_StoreOnLastReference(avc, &treq);
	    ReleaseWriteLock(&avc->lock);
#if defined(AFS_SGI_ENV)
	    AFS_RWUNLOCK((vnode_t *)avc, VRWLOCK_WRITE);
#endif
	}
	else {
#if defined(AFS_SGI_ENV)
	    AFS_RWUNLOCK((vnode_t *)avc, VRWLOCK_WRITE);
#endif
	    /* at least one daemon is idle, so ask it to do the store.
		Also, note that  we don't lock it any more... */
	    tb = afs_BQueue(BOP_STORE, avc, 0, 1, acred, (long)acred->cr_uid,
			    0L, 0L, 0L);
	    /* sleep waiting for the store to start, then retrieve error code */
	    while ((tb->flags & BUVALID) == 0) {
		tb->flags |= BUWAIT;
		afs_osi_Sleep(tb);
	    }
	    code = tb->code;
	    afs_BRelease(tb);
	}

	/* VNOVNODE is "acceptable" error code from close, since
	    may happen when deleting a file on another machine while
	    it is open here. We do the same for ENOENT since in afs_CheckCode we map VNOVNODE -> ENOENT */
	if (code == VNOVNODE || code == ENOENT)
	    code = 0;
	
	/* Ensure last closer gets the error. If another thread caused
	 * DoPartialWrite and this thread does not actually store the data,
	 * it may not see the quota error.
	 */
	ObtainWriteLock(&avc->lock,406);
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
	    afs_warnuser("afs: failed to store file (over quota or partition full)\n");
	}
#else
	else if (code == ENOSPC) {
	    afs_warnuser("afs: failed to store file (partition full)\n");
	}
	else if (code == EDQUOT) {
	    afs_warnuser("afs: failed to store file (over quota)\n");
	}
#endif
	else if (code != 0)
	    afs_warnuser("afs: failed to store file (%d)\n", code);

	/* finally, we flush any text pages lying around here */
	hzero(avc->flushDV);
	osi_FlushText(avc);
    }
    else {
#if defined(AFS_SGI_ENV)
	AFS_RWUNLOCK((vnode_t *)avc, VRWLOCK_WRITE);
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
    if ((avc->vrefCount <= 2) && (avc->states & CUnlinked)) {
	afs_remunlink(avc, 1);	/* ignore any return code */
    }
#endif
    code = afs_CheckCode(code, &treq, 5);
    return code;
}



#ifdef	AFS_OSF_ENV
afs_fsync(avc, fflags, acred, waitfor)
int fflags;
int waitfor;
#else	/* AFS_OSF_ENV */
#if defined(AFS_SGI_ENV) || defined(AFS_SUN53_ENV)
afs_fsync(OSI_VC_ARG(avc), flag, acred
#ifdef AFS_SGI65_ENV
          , start, stop
#endif
          )
#else
afs_fsync(avc, acred)
#endif
#endif
    OSI_VC_DECL(avc);
     struct AFS_UCRED *acred;
#if defined(AFS_SGI_ENV) || defined(AFS_SUN53_ENV)
int flag;
#ifdef AFS_SGI65_ENV
off_t start, stop;
#endif
#endif
{
    register afs_int32 code;
    struct vrequest treq;
    OSI_VC_CONVERT(avc)

    if (avc->vc_error)
	return avc->vc_error;

#if defined(AFS_SUN5_ENV)
     /* back out if called from NFS server */
    if (curthread->t_flag & T_DONTPEND)
	return 0;
#endif

    AFS_STATCNT(afs_fsync);
    afs_Trace1(afs_iclSetp, CM_TRACE_FSYNC, ICL_TYPE_POINTER, avc);
    if (code = afs_InitReq(&treq, acred)) return code;

#if defined(AFS_SGI_ENV)
    AFS_RWLOCK((vnode_t *)avc, VRWLOCK_WRITE);
    if (flag & FSYNC_INVAL)
	osi_VM_FSyncInval(avc);
#endif /* AFS_SGI_ENV */

    ObtainSharedLock(&avc->lock,18);
    code = 0;
    if (avc->execsOrWriters > 0) {
	/* put the file back */
	UpgradeSToWLock(&avc->lock,41);
	code = afs_StoreAllSegments(avc, &treq, AFS_SYNC);
	ConvertWToSLock(&avc->lock);
    }

#if defined(AFS_SGI_ENV)
    AFS_RWUNLOCK((vnode_t *)avc, VRWLOCK_WRITE);
    if (code == VNOVNODE) {
	/* syncing an unlinked file! - non-informative to pass an errno
	 * 102 (== VNOVNODE) to user
	 */
	code =  ENOENT;
    }
#endif

    code = afs_CheckCode(code, &treq, 33);
    ReleaseSharedLock(&avc->lock);
    return code;
}
