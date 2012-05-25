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


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#ifndef AFS_LINUX22_ENV
#include "rpc/types.h"
#endif
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */

/* memory cache routines */
static struct memCacheEntry *memCache;
static int memCacheBlkSize = 8192;
static int memMaxBlkNumber = 0;

extern int cacheDiskType;

int
afs_InitMemCache(int blkCount, int blkSize, int flags)
{
    int index;

    AFS_STATCNT(afs_InitMemCache);
    if (blkSize)
	memCacheBlkSize = blkSize;

    memMaxBlkNumber = blkCount;
    memCache =
	afs_osi_Alloc(memMaxBlkNumber * sizeof(struct memCacheEntry));
    osi_Assert(memCache != NULL);

    for (index = 0; index < memMaxBlkNumber; index++) {
	char *blk;
	(memCache + index)->size = 0;
	(memCache + index)->dataSize = memCacheBlkSize;
	LOCK_INIT(&((memCache + index)->afs_memLock), "afs_memLock");
	blk = afs_osi_Alloc(memCacheBlkSize);
	if (blk == NULL)
	    goto nomem;
	(memCache + index)->data = blk;
	memset((memCache + index)->data, 0, memCacheBlkSize);
    }
#if defined(AFS_HAVE_VXFS)
    afs_InitDualFSCacheOps((struct vnode *)0);
#endif
    for (index = 0; index < blkCount; index++)
	afs_InitCacheFile(NULL, 0);
    return 0;

  nomem:
    afs_warn("afsd:  memCache allocation failure at %d KB.\n",
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

void *
afs_MemCacheOpen(afs_dcache_id_t *ainode)
{
    struct memCacheEntry *mep;

    if (ainode->mem < 0 || ainode->mem > memMaxBlkNumber) {
	osi_Panic("afs_MemCacheOpen: invalid block #");
    }
    mep = (memCache + ainode->mem);
    afs_Trace3(afs_iclSetp, CM_TRACE_MEMOPEN, ICL_TYPE_INT32, ainode->mem,
	       ICL_TYPE_POINTER, mep, ICL_TYPE_POINTER, mep ? mep->data : 0);
    return (void *)mep;
}

/*
 * this routine simulates a read in the Memory Cache
 */
int
afs_MemReadBlk(struct osi_file *fP, int offset, void *dest,
	       int size)
{
    struct memCacheEntry *mceP = (struct memCacheEntry *)fP;
    int bytesRead;

    ObtainReadLock(&mceP->afs_memLock);
    AFS_STATCNT(afs_MemReadBlk);
    if (offset < 0) {
	ReleaseReadLock(&mceP->afs_memLock);
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

    ReleaseReadLock(&mceP->afs_memLock);
    return bytesRead;
}

/*
 * this routine simulates a readv in the Memory Cache
 */
int
afs_MemReadvBlk(struct memCacheEntry *mceP, int offset,
		struct iovec *iov, int nio, int size)
{
    int i;
    int bytesRead;
    int bytesToRead;

    ObtainReadLock(&mceP->afs_memLock);
    AFS_STATCNT(afs_MemReadBlk);
    if (offset < 0) {
	ReleaseReadLock(&mceP->afs_memLock);
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

    ReleaseReadLock(&mceP->afs_memLock);
    return bytesRead;
}

int
afs_MemReadUIO(afs_dcache_id_t *ainode, struct uio *uioP)
{
    struct memCacheEntry *mceP =
	(struct memCacheEntry *)afs_MemCacheOpen(ainode);
    int length = mceP->size - AFS_UIO_OFFSET(uioP);
    afs_int32 code;

    AFS_STATCNT(afs_MemReadUIO);
    ObtainReadLock(&mceP->afs_memLock);
    length = (length < AFS_UIO_RESID(uioP)) ? length : AFS_UIO_RESID(uioP);
    AFS_UIOMOVE(mceP->data + AFS_UIO_OFFSET(uioP), length, UIO_READ, uioP, code);
    ReleaseReadLock(&mceP->afs_memLock);
    return code;
}

int
afs_MemWriteBlk(struct osi_file *fP, int offset, void *src,
		int size)
{
    struct memCacheEntry *mceP = (struct memCacheEntry *)fP;
    struct iovec tiov;

    tiov.iov_base = src;
    tiov.iov_len = size;
    return afs_MemWritevBlk(mceP, offset, &tiov, 1, size);
}

/*XXX: this extends a block arbitrarily to support big directories */
int
afs_MemWritevBlk(struct memCacheEntry *mceP, int offset,
		 struct iovec *iov, int nio, int size)
{
    int i;
    int bytesWritten;
    int bytesToWrite;
    AFS_STATCNT(afs_MemWriteBlk);
    ObtainWriteLock(&mceP->afs_memLock, 561);
    if (offset + size > mceP->dataSize) {
	char *oldData = mceP->data;

	mceP->data = afs_osi_Alloc(size + offset);
	if (mceP->data == NULL) {	/* no available memory */
	    mceP->data = oldData;	/* revert back change that was made */
	    ReleaseWriteLock(&mceP->afs_memLock);
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

    ReleaseWriteLock(&mceP->afs_memLock);
    return bytesWritten;
}

int
afs_MemWriteUIO(afs_dcache_id_t *ainode, struct uio *uioP)
{
    struct memCacheEntry *mceP =
	(struct memCacheEntry *)afs_MemCacheOpen(ainode);
    afs_int32 code;

    AFS_STATCNT(afs_MemWriteUIO);
    ObtainWriteLock(&mceP->afs_memLock, 312);
    if (AFS_UIO_RESID(uioP) + AFS_UIO_OFFSET(uioP) > mceP->dataSize) {
	char *oldData = mceP->data;

	mceP->data = afs_osi_Alloc(AFS_UIO_RESID(uioP) + AFS_UIO_OFFSET(uioP));
	if (mceP->data == NULL) {	/* no available memory */
	    mceP->data = oldData;	/* revert back change that was made */
	    ReleaseWriteLock(&mceP->afs_memLock);
	    afs_warn("afs: afs_MemWriteBlk mem alloc failure (%ld bytes)\n",
		     (long)(AFS_UIO_RESID(uioP) + AFS_UIO_OFFSET(uioP)));
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

    ReleaseWriteLock(&mceP->afs_memLock);
    return code;
}

int
afs_MemCacheTruncate(struct osi_file *fP, int size)
{
    struct memCacheEntry *mceP = (struct memCacheEntry *)fP;
    AFS_STATCNT(afs_MemCacheTruncate);

    ObtainWriteLock(&mceP->afs_memLock, 313);
    /* old directory entry; g.c. */
    if (size == 0 && mceP->dataSize > memCacheBlkSize) {
	char *oldData = mceP->data;
	mceP->data = afs_osi_Alloc(memCacheBlkSize);
	if (mceP->data == NULL) {	/* no available memory */
	    mceP->data = oldData;
	    ReleaseWriteLock(&mceP->afs_memLock);
	    afs_warn("afs: afs_MemWriteBlk mem alloc failure (%d bytes)\n",
		     memCacheBlkSize);
	} else {
	    afs_osi_Free(oldData, mceP->dataSize);
	    mceP->dataSize = memCacheBlkSize;
	}
    }

    if (size < mceP->size)
	mceP->size = size;

    ReleaseWriteLock(&mceP->afs_memLock);
    return 0;
}


void
shutdown_memcache(void)
{
    int index;

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
