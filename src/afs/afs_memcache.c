/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/afs_memcache.c,v 1.15.2.6 2007/06/23 15:31:11 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#ifndef AFS_LINUX22_ENV
#include "rpc/types.h"
#endif
#ifdef	AFS_OSF_ENV
#undef kmem_alloc
#undef kmem_free
#undef mem_alloc
#undef mem_free
#undef register
#endif /* AFS_OSF_ENV */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */

/* memory cache routines */
static struct memCacheEntry *memCache;
static int memCacheBlkSize = 8192;
static int memMaxBlkNumber = 0;
static int memAllocMaySleep = 0;

extern int cacheDiskType;

int
afs_InitMemCache(int blkCount, int blkSize, int flags)
{
    int index;

    AFS_STATCNT(afs_InitMemCache);
    if (blkSize)
	memCacheBlkSize = blkSize;

    memMaxBlkNumber = blkCount;
    memCache = (struct memCacheEntry *)
	afs_osi_Alloc(memMaxBlkNumber * sizeof(struct memCacheEntry));
    if (flags & AFSCALL_INIT_MEMCACHE_SLEEP) {
	memAllocMaySleep = 1;
    }

    for (index = 0; index < memMaxBlkNumber; index++) {
	char *blk;
	(memCache + index)->size = 0;
	(memCache + index)->dataSize = memCacheBlkSize;
	LOCK_INIT(&((memCache + index)->afs_memLock), "afs_memLock");
	if (memAllocMaySleep) {
	    blk = afs_osi_Alloc(memCacheBlkSize);
	} else {
	    blk = afs_osi_Alloc_NoSleep(memCacheBlkSize);
	}
	if (blk == NULL)
	    goto nomem;
	(memCache + index)->data = blk;
	memset((memCache + index)->data, 0, memCacheBlkSize);
    }
#if defined(AFS_SGI62_ENV) || defined(AFS_HAVE_VXFS)
    afs_InitDualFSCacheOps((struct vnode *)0);
#endif

    return 0;

  nomem:
    printf("afsd:  memCache allocation failure at %d KB.\n",
	   (index * memCacheBlkSize) / 1024);
    while (--index >= 0) {
	afs_osi_Free((memCache + index)->data, memCacheBlkSize);
	(memCache + index)->data = NULL;
    }
    return ENOMEM;

}

int
afs_MemCacheClose(struct osi_file *file)
{
    return 0;
}

#if defined(AFS_SUN57_64BIT_ENV) || defined(AFS_SGI62_ENV)
void *
afs_MemCacheOpen(ino_t blkno)
#else
void *
afs_MemCacheOpen(afs_int32 blkno)
#endif
{
    struct memCacheEntry *mep;

    if (blkno < 0 || blkno > memMaxBlkNumber) {
	osi_Panic("afs_MemCacheOpen: invalid block #");
    }
    mep = (memCache + blkno);
    afs_Trace3(afs_iclSetp, CM_TRACE_MEMOPEN, ICL_TYPE_INT32, blkno,
	       ICL_TYPE_POINTER, mep, ICL_TYPE_POINTER, mep ? mep->data : 0);
    return (void *)mep;
}

/*
 * this routine simulates a read in the Memory Cache 
 */
int
afs_MemReadBlk(register struct osi_file *fP, int offset, void *dest,
	       int size)
{
    register struct memCacheEntry *mceP = (struct memCacheEntry *)fP;
    int bytesRead;

    MObtainReadLock(&mceP->afs_memLock);
    AFS_STATCNT(afs_MemReadBlk);
    if (offset < 0) {
	MReleaseReadLock(&mceP->afs_memLock);
	return 0;
    }
    /* use min of bytes in buffer or requested size */
    bytesRead = (size < mceP->size - offset) ? size : mceP->size - offset;

    if (bytesRead > 0) {
	AFS_GUNLOCK();
	memcpy(dest, mceP->data + offset, bytesRead);
	AFS_GLOCK();
    } else
	bytesRead = 0;

    MReleaseReadLock(&mceP->afs_memLock);
    return bytesRead;
}

/*
 * this routine simulates a readv in the Memory Cache 
 */
int
afs_MemReadvBlk(register struct memCacheEntry *mceP, int offset,
		struct iovec *iov, int nio, int size)
{
    int i;
    int bytesRead;
    int bytesToRead;

    MObtainReadLock(&mceP->afs_memLock);
    AFS_STATCNT(afs_MemReadBlk);
    if (offset < 0) {
	MReleaseReadLock(&mceP->afs_memLock);
	return 0;
    }
    /* use min of bytes in buffer or requested size */
    bytesRead = (size < mceP->size - offset) ? size : mceP->size - offset;

    if (bytesRead > 0) {
	for (i = 0, size = bytesRead; i < nio && size > 0; i++) {
	    bytesToRead = (size < iov[i].iov_len) ? size : iov[i].iov_len;
	    AFS_GUNLOCK();
	    memcpy(iov[i].iov_base, mceP->data + offset, bytesToRead);
	    AFS_GLOCK();
	    offset += bytesToRead;
	    size -= bytesToRead;
	}
	bytesRead -= size;
    } else
	bytesRead = 0;

    MReleaseReadLock(&mceP->afs_memLock);
    return bytesRead;
}

int
afs_MemReadUIO(ino_t blkno, struct uio *uioP)
{
    register struct memCacheEntry *mceP =
	(struct memCacheEntry *)afs_MemCacheOpen(blkno);
    int length = mceP->size - AFS_UIO_OFFSET(uioP);
    afs_int32 code;

    AFS_STATCNT(afs_MemReadUIO);
    MObtainReadLock(&mceP->afs_memLock);
    length = (length < AFS_UIO_RESID(uioP)) ? length : AFS_UIO_RESID(uioP);
    AFS_UIOMOVE(mceP->data + AFS_UIO_OFFSET(uioP), length, UIO_READ, uioP, code);
    MReleaseReadLock(&mceP->afs_memLock);
    return code;
}

/*XXX: this extends a block arbitrarily to support big directories */
int
afs_MemWriteBlk(register struct osi_file *fP, int offset, void *src,
		int size)
{
    register struct memCacheEntry *mceP = (struct memCacheEntry *)fP;
    AFS_STATCNT(afs_MemWriteBlk);
    MObtainWriteLock(&mceP->afs_memLock, 560);
    if (size + offset > mceP->dataSize) {
	char *oldData = mceP->data;

	if (memAllocMaySleep) {
	    mceP->data = afs_osi_Alloc(size + offset);
	} else {
	    mceP->data = afs_osi_Alloc_NoSleep(size + offset);
	}
	if (mceP->data == NULL) {	/* no available memory */
	    mceP->data = oldData;	/* revert back change that was made */
	    MReleaseWriteLock(&mceP->afs_memLock);
	    afs_warn("afs: afs_MemWriteBlk mem alloc failure (%d bytes)\n",
		     size + offset);
	    return -ENOMEM;
	}

	/* may overlap, but this is OK */
	AFS_GUNLOCK();
	memcpy(mceP->data, oldData, mceP->size);
	AFS_GLOCK();
	afs_osi_Free(oldData, mceP->dataSize);
	mceP->dataSize = size + offset;
    }
    AFS_GUNLOCK();
    if (mceP->size < offset)
	memset(mceP->data + mceP->size, 0, offset - mceP->size);
    memcpy(mceP->data + offset, src, size);
    AFS_GLOCK();
    mceP->size = (size + offset < mceP->size) ? mceP->size : size + offset;

    MReleaseWriteLock(&mceP->afs_memLock);
    return size;
}

/*XXX: this extends a block arbitrarily to support big directories */
int
afs_MemWritevBlk(register struct memCacheEntry *mceP, int offset,
		 struct iovec *iov, int nio, int size)
{
    int i;
    int bytesWritten;
    int bytesToWrite;
    AFS_STATCNT(afs_MemWriteBlk);
    MObtainWriteLock(&mceP->afs_memLock, 561);
    if (offset + size > mceP->dataSize) {
	char *oldData = mceP->data;

	mceP->data = afs_osi_Alloc(size + offset);
	if (mceP->data == NULL) {	/* no available memory */
	    mceP->data = oldData;	/* revert back change that was made */
	    MReleaseWriteLock(&mceP->afs_memLock);
	    afs_warn("afs: afs_MemWriteBlk mem alloc failure (%d bytes)\n",
		     size + offset);
	    return -ENOMEM;
	}

	/* may overlap, but this is OK */
	AFS_GUNLOCK();
	memcpy(mceP->data, oldData, mceP->size);
	AFS_GLOCK();
	afs_osi_Free(oldData, mceP->dataSize);
	mceP->dataSize = size + offset;
    }
    AFS_GUNLOCK();
    if (mceP->size < offset)
	memset(mceP->data + mceP->size, 0, offset - mceP->size);
    for (bytesWritten = 0, i = 0; i < nio && size > 0; i++) {
	bytesToWrite = (size < iov[i].iov_len) ? size : iov[i].iov_len;
	memcpy(mceP->data + offset, iov[i].iov_base, bytesToWrite);
	offset += bytesToWrite;
	bytesWritten += bytesToWrite;
	size -= bytesToWrite;
    }
    mceP->size = (offset < mceP->size) ? mceP->size : offset;
    AFS_GLOCK();

    MReleaseWriteLock(&mceP->afs_memLock);
    return bytesWritten;
}

int
afs_MemWriteUIO(ino_t blkno, struct uio *uioP)
{
    register struct memCacheEntry *mceP =
	(struct memCacheEntry *)afs_MemCacheOpen(blkno);
    afs_int32 code;

    AFS_STATCNT(afs_MemWriteUIO);
    MObtainWriteLock(&mceP->afs_memLock, 312);
    if (AFS_UIO_RESID(uioP) + AFS_UIO_OFFSET(uioP) > mceP->dataSize) {
	char *oldData = mceP->data;

	mceP->data = afs_osi_Alloc(AFS_UIO_RESID(uioP) + AFS_UIO_OFFSET(uioP));
	if (mceP->data == NULL) {	/* no available memory */
	    mceP->data = oldData;	/* revert back change that was made */
	    MReleaseWriteLock(&mceP->afs_memLock);
	    afs_warn("afs: afs_MemWriteBlk mem alloc failure (%d bytes)\n",
		     AFS_UIO_RESID(uioP) + AFS_UIO_OFFSET(uioP));
	    return -ENOMEM;
	}

	AFS_GUNLOCK();
	memcpy(mceP->data, oldData, mceP->size);
	AFS_GLOCK();

	afs_osi_Free(oldData, mceP->dataSize);
	mceP->dataSize = AFS_UIO_RESID(uioP) + AFS_UIO_OFFSET(uioP);
    }
    if (mceP->size < AFS_UIO_OFFSET(uioP))
	memset(mceP->data + mceP->size, 0,
	       (int)(AFS_UIO_OFFSET(uioP) - mceP->size));
    AFS_UIOMOVE(mceP->data + AFS_UIO_OFFSET(uioP), AFS_UIO_RESID(uioP), UIO_WRITE,
		uioP, code);
    if (AFS_UIO_OFFSET(uioP) > mceP->size)
	mceP->size = AFS_UIO_OFFSET(uioP);

    MReleaseWriteLock(&mceP->afs_memLock);
    return code;
}

int
afs_MemCacheTruncate(register struct osi_file *fP, int size)
{
    register struct memCacheEntry *mceP = (struct memCacheEntry *)fP;
    AFS_STATCNT(afs_MemCacheTruncate);

    MObtainWriteLock(&mceP->afs_memLock, 313);
    /* old directory entry; g.c. */
    if (size == 0 && mceP->dataSize > memCacheBlkSize) {
	char *oldData = mceP->data;
	mceP->data = afs_osi_Alloc(memCacheBlkSize);
	if (mceP->data == NULL) {	/* no available memory */
	    mceP->data = oldData;
	    MReleaseWriteLock(&mceP->afs_memLock);
	    afs_warn("afs: afs_MemWriteBlk mem alloc failure (%d bytes)\n",
		     memCacheBlkSize);
	} else {
	    afs_osi_Free(oldData, mceP->dataSize);
	    mceP->dataSize = memCacheBlkSize;
	}
    }

    if (size < mceP->size)
	mceP->size = size;

    MReleaseWriteLock(&mceP->afs_memLock);
    return 0;
}

int
afs_MemCacheStoreProc(register struct rx_call *acall,
		      register struct osi_file *fP,
		      register afs_int32 alen, struct vcache *avc,
		      int *shouldWake, afs_size_t * abytesToXferP,
		      afs_size_t * abytesXferredP)
{
    register struct memCacheEntry *mceP = (struct memCacheEntry *)fP;

    register afs_int32 code;
    register int tlen;
    int offset = 0;
    struct iovec *tiov;		/* no data copying with iovec */
    int tnio;			/* temp for iovec size */

    AFS_STATCNT(afs_MemCacheStoreProc);
#ifndef AFS_NOSTATS
    /*
     * In this case, alen is *always* the amount of data we'll be trying
     * to ship here.
     */
    *(abytesToXferP) = alen;
    *(abytesXferredP) = 0;
#endif /* AFS_NOSTATS */

    /* 
     * We need to alloc the iovecs on the heap so that they are "pinned" rather than
     * declare them on the stack - defect 11272
     */
    tiov =
	(struct iovec *)osi_AllocSmallSpace(sizeof(struct iovec) *
					    RX_MAXIOVECS);
    if (!tiov) {
	osi_Panic
	    ("afs_MemCacheStoreProc: osi_AllocSmallSpace for iovecs returned NULL\n");
    }
#ifdef notdef
    /* do this at a higher level now -- it's a parameter */
    /* for now, only do 'continue from close' code if file fits in one
     * chunk.  Could clearly do better: if only one modified chunk
     * then can still do this.  can do this on *last* modified chunk */
    tlen = avc->m.Length - 1;	/* byte position of last byte we'll store */
    if (shouldWake) {
	if (AFS_CHUNK(tlen) != 0)
	    *shouldWake = 0;
	else
	    *shouldWake = 1;
    }
#endif /* notdef */

    while (alen > 0) {
	tlen = (alen > AFS_LRALLOCSIZ ? AFS_LRALLOCSIZ : alen);
	RX_AFS_GUNLOCK();
	code = rx_WritevAlloc(acall, tiov, &tnio, RX_MAXIOVECS, tlen);
	RX_AFS_GLOCK();
	if (code <= 0) {
	    code = rx_Error(acall);
	    osi_FreeSmallSpace(tiov);
	    return code ? code : -33;
	}
	tlen = code;
	code = afs_MemReadvBlk(mceP, offset, tiov, tnio, tlen);
	if (code != tlen) {
	    osi_FreeSmallSpace(tiov);
	    return -33;
	}
	RX_AFS_GUNLOCK();
	code = rx_Writev(acall, tiov, tnio, tlen);
	RX_AFS_GLOCK();
#ifndef AFS_NOSTATS
	(*abytesXferredP) += code;
#endif /* AFS_NOSTATS */
	if (code != tlen) {
	    code = rx_Error(acall);
	    osi_FreeSmallSpace(tiov);
	    return code ? code : -33;
	}
	offset += tlen;
	alen -= tlen;
	/* if file has been locked on server, can allow store to continue */
	if (shouldWake && *shouldWake && (rx_GetRemoteStatus(acall) & 1)) {
	    *shouldWake = 0;	/* only do this once */
	    afs_wakeup(avc);
	}
    }
    osi_FreeSmallSpace(tiov);
    return 0;
}

int
afs_MemCacheFetchProc(register struct rx_call *acall,
		      register struct osi_file *fP, afs_size_t abase,
		      struct dcache *adc, struct vcache *avc,
		      afs_size_t * abytesToXferP, afs_size_t * abytesXferredP,
		      afs_int32 lengthFound)
{
    register struct memCacheEntry *mceP = (struct memCacheEntry *)fP;
    register afs_int32 code;
    afs_int32 length;
    int moredata = 0;
    struct iovec *tiov;		/* no data copying with iovec */
    register int tlen, offset = 0;
    int tnio;			/* temp for iovec size */

    AFS_STATCNT(afs_MemCacheFetchProc);
    length = lengthFound;
    afs_Trace4(afs_iclSetp, CM_TRACE_MEMFETCH, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_POINTER, mceP, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(abase), ICL_TYPE_INT32, length);
#ifndef AFS_NOSTATS
    (*abytesToXferP) = 0;
    (*abytesXferredP) = 0;
#endif /* AFS_NOSTATS */
    /* 
     * We need to alloc the iovecs on the heap so that they are "pinned" rather than
     * declare them on the stack - defect 11272
     */
    tiov =
	(struct iovec *)osi_AllocSmallSpace(sizeof(struct iovec) *
					    RX_MAXIOVECS);
    if (!tiov) {
	osi_Panic
	    ("afs_MemCacheFetchProc: osi_AllocSmallSpace for iovecs returned NULL\n");
    }
    adc->validPos = abase;
    do {
	if (moredata) {
	    RX_AFS_GUNLOCK();
	    code = rx_Read(acall, (char *)&length, sizeof(afs_int32));
	    length = ntohl(length);
	    RX_AFS_GLOCK();
	    if (code != sizeof(afs_int32)) {
		code = rx_Error(acall);
		osi_FreeSmallSpace(tiov);
		return (code ? code : -1);	/* try to return code, not -1 */
	    }
	}
	/*
	 * The fetch protocol is extended for the AFS/DFS translator
	 * to allow multiple blocks of data, each with its own length,
	 * to be returned. As long as the top bit is set, there are more
	 * blocks expected.
	 *
	 * We do not do this for AFS file servers because they sometimes
	 * return large negative numbers as the transfer size.
	 */
	if (avc->states & CForeign) {
	    moredata = length & 0x80000000;
	    length &= ~0x80000000;
	} else {
	    moredata = 0;
	}
#ifndef AFS_NOSTATS
	(*abytesToXferP) += length;
#endif /* AFS_NOSTATS */
	while (length > 0) {
	    tlen = (length > AFS_LRALLOCSIZ ? AFS_LRALLOCSIZ : length);
	    RX_AFS_GUNLOCK();
	    code = rx_Readv(acall, tiov, &tnio, RX_MAXIOVECS, tlen);
	    RX_AFS_GLOCK();
#ifndef AFS_NOSTATS
	    (*abytesXferredP) += code;
#endif /* AFS_NOSTATS */
	    if (code <= 0) {
		afs_Trace3(afs_iclSetp, CM_TRACE_FETCH64READ,
			   ICL_TYPE_POINTER, avc, ICL_TYPE_INT32, code,
			   ICL_TYPE_INT32, length);
		osi_FreeSmallSpace(tiov);
		return -34;
	    }
	    tlen = code;
	    afs_MemWritevBlk(mceP, offset, tiov, tnio, tlen);
	    offset += tlen;
	    abase += tlen;
	    length -= tlen;
	    adc->validPos = abase;
	    if (afs_osi_Wakeup(&adc->validPos) == 0)
		afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAKE, ICL_TYPE_STRING,
			   __FILE__, ICL_TYPE_INT32, __LINE__,
			   ICL_TYPE_POINTER, adc, ICL_TYPE_INT32,
			   adc->dflags);
	}
    } while (moredata);
    /* max of two sizes */
    osi_FreeSmallSpace(tiov);
    return 0;
}


void
shutdown_memcache(void)
{
    register int index;

    if (cacheDiskType != AFS_FCACHE_TYPE_MEM)
	return;
    memCacheBlkSize = 8192;
    for (index = 0; index < memMaxBlkNumber; index++) {
	LOCK_INIT(&((memCache + index)->afs_memLock), "afs_memLock");
	afs_osi_Free((memCache + index)->data, (memCache + index)->dataSize);
    }
    afs_osi_Free((char *)memCache,
		 memMaxBlkNumber * sizeof(struct memCacheEntry));
    memMaxBlkNumber = 0;
}
