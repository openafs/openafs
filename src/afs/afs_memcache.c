
/*
 * (C) Copyright 1990 Transarc Corporation
 * All Rights Reserved.
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#ifndef AFS_LINUX22_ENV
#include "../rpc/types.h"
#endif
#ifdef	AFS_ALPHA_ENV
#undef kmem_alloc
#undef kmem_free
#undef mem_alloc
#undef mem_free
#undef register
#endif  /* AFS_ALPHA_ENV */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h" /* statistics */

/* memory cache routines */

struct memCacheEntry {
    int size;      /* # of valid bytes in this entry */
    int dataSize;  /* size of allocated data area */
    afs_lock_t afs_memLock;
    char *data;    /* bytes */
};

static struct memCacheEntry *memCache;
static int memCacheBlkSize = 8192;
static int memMaxBlkNumber = 0;
static int memAllocMaySleep = 0;

extern int cacheDiskType;

afs_InitMemCache(size, blkSize, flags)
     int size;
     int blkSize;
     int flags;
  {
      int index;

      AFS_STATCNT(afs_InitMemCache);
      if(blkSize)
	  memCacheBlkSize = blkSize;
      
      memMaxBlkNumber = size / memCacheBlkSize;
      memCache = (struct memCacheEntry *)
	  afs_osi_Alloc(memMaxBlkNumber * sizeof(struct memCacheEntry));
      if (flags & AFSCALL_INIT_MEMCACHE_SLEEP) {
	  memAllocMaySleep = 1;
      }

      for(index = 0; index < memMaxBlkNumber; index++) {
	  char *blk;
	  (memCache+index)->size = 0;
	  (memCache+index)->dataSize = memCacheBlkSize;
	  LOCK_INIT(&((memCache+index)->afs_memLock), "afs_memLock");
	  if (memAllocMaySleep) {
	      blk = afs_osi_Alloc(memCacheBlkSize);
	  } else {
	      blk = afs_osi_Alloc_NoSleep(memCacheBlkSize);
	  }
	  if (blk == NULL)
	      goto nomem;
	  (memCache+index)->data = blk;
	  bzero((memCache+index)->data, memCacheBlkSize);
      }
#if defined(AFS_SGI62_ENV) || defined(AFS_HAVE_VXFS)
      afs_InitDualFSCacheOps((struct vnode*)0);
#endif

      return 0;

nomem:
      printf("afsd:  memCache allocation failure at %d KB.\n",
	     (index * memCacheBlkSize) / 1024);
      while(--index >= 0) {
	  afs_osi_Free((memCache+index)->data, memCacheBlkSize);
	  (memCache+index)->data = NULL;
      }
      return ENOMEM;

 }

afs_MemCacheClose(file)
    char *file;
{
    return 0;
}

void *afs_MemCacheOpen(ino_t blkno)
  {
      struct memCacheEntry *mep;

      if (blkno < 0 || blkno > memMaxBlkNumber) {
	  osi_Panic("afs_MemCacheOpen: invalid block #");
      }
      mep =  (memCache + blkno);
      return (void *) mep;
  }

/*
 * this routine simulates a read in the Memory Cache 
 */
afs_MemReadBlk(mceP, offset, dest, size)
     int offset;
     register struct memCacheEntry *mceP;
     char *dest;
     int size;
  {
      int bytesRead;
      
      MObtainReadLock(&mceP->afs_memLock);
      AFS_STATCNT(afs_MemReadBlk);
      if (offset < 0) {
	  MReleaseReadLock(&mceP->afs_memLock);
	  return 0;
      }
      /* use min of bytes in buffer or requested size */
      bytesRead = (size < mceP->size - offset) ? size :
	  mceP->size - offset;
      
      if(bytesRead > 0) {
	  AFS_GUNLOCK();
	  bcopy(mceP->data + offset, dest, bytesRead);
	  AFS_GLOCK();
      }
      else
	  bytesRead = 0;

      MReleaseReadLock(&mceP->afs_memLock);
      return bytesRead;
  }

/*
 * this routine simulates a readv in the Memory Cache 
 */
afs_MemReadvBlk(mceP, offset, iov, nio, size)
     int offset;
     register struct memCacheEntry *mceP;
     struct iovec *iov;
     int nio;
     int size;
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
      bytesRead = (size < mceP->size - offset) ? size :
	  mceP->size - offset;
      
      if(bytesRead > 0) {
	  for (i = 0 , size = bytesRead ; i < nio && size > 0 ; i++) {
	      bytesToRead = (size < iov[i].iov_len) ? size : iov[i].iov_len;
	      AFS_GUNLOCK();
	      bcopy(mceP->data + offset, iov[i].iov_base, bytesToRead);
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

afs_MemReadUIO(blkno, uioP)
     ino_t blkno;
     struct uio *uioP;
  {
      register struct memCacheEntry *mceP = (struct memCacheEntry *)afs_MemCacheOpen(blkno);
      int length = mceP->size - uioP->uio_offset;
      afs_int32 code;

      AFS_STATCNT(afs_MemReadUIO);
      MObtainReadLock(&mceP->afs_memLock);
      length = (length < uioP->uio_resid) ? length : uioP->uio_resid;
      AFS_UIOMOVE(mceP->data + uioP->uio_offset, length, UIO_READ, uioP, code);
      MReleaseReadLock(&mceP->afs_memLock);
      return code;
  }

/*XXX: this extends a block arbitrarily to support big directories */
afs_MemWriteBlk(mceP, offset, src, size)
     register struct memCacheEntry *mceP;
     int offset;
     char *src;
     int size;
  {
      AFS_STATCNT(afs_MemWriteBlk);
      MObtainWriteLock(&mceP->afs_memLock,560);
      if (size + offset > mceP->dataSize) {
	  char *oldData = mceP->data;

	  if (memAllocMaySleep) {
	     mceP->data = afs_osi_Alloc(size+offset);
	  } else {
	     mceP->data = afs_osi_Alloc_NoSleep(size+offset);
	  }
	  if ( mceP->data == NULL )	/* no available memory */
	  {
		mceP->data = oldData;   /* revert back change that was made */
		MReleaseWriteLock(&mceP->afs_memLock);
                afs_warn("afs: afs_MemWriteBlk mem alloc failure (%d bytes)\n",
                                size+offset);
		return -ENOMEM;
	  }
	  
	  /* may overlap, but this is OK */
	  AFS_GUNLOCK();
	  bcopy(oldData, mceP->data, mceP->size);
	  AFS_GLOCK();
	  afs_osi_Free(oldData,mceP->dataSize);
	  mceP->dataSize = size+offset;
      }
      AFS_GUNLOCK();
      if (mceP->size < offset)
	  bzero(mceP->data+mceP->size, offset-mceP->size);
      bcopy(src, mceP->data + offset, size);
      AFS_GLOCK();
      mceP->size = (size+offset < mceP->size) ? mceP->size :
	  size + offset;

      MReleaseWriteLock(&mceP->afs_memLock);
      return size;
  }

/*XXX: this extends a block arbitrarily to support big directories */
afs_MemWritevBlk(mceP, offset, iov, nio, size)
     register struct memCacheEntry *mceP;
     int offset;
     struct iovec *iov;
     int nio;
     int size;
  {
      int i;
      int bytesWritten;
      int bytesToWrite;
      AFS_STATCNT(afs_MemWriteBlk);
      MObtainWriteLock(&mceP->afs_memLock,561);
      if (size + offset > mceP->dataSize) {
	  char *oldData = mceP->data;

	  mceP->data = afs_osi_Alloc(size+offset);
	  
	  /* may overlap, but this is OK */
	  AFS_GUNLOCK();
	  bcopy(oldData, mceP->data, mceP->size);
	  AFS_GLOCK();
	  afs_osi_Free(oldData,mceP->dataSize);
	  mceP->dataSize = size+offset;
      }
      if (mceP->size < offset)
	  bzero(mceP->data+mceP->size, offset-mceP->size);
      for (bytesWritten = 0, i = 0 ; i < nio && size > 0 ; i++) {
	  bytesToWrite = (size < iov[i].iov_len) ? size : iov[i].iov_len;
	  AFS_GUNLOCK();
	  bcopy(iov[i].iov_base, mceP->data + offset, bytesToWrite);
	  AFS_GLOCK();
 	  offset += bytesToWrite;
 	  bytesWritten += bytesToWrite;
 	  size -= bytesToWrite;
      }
      mceP->size = (offset < mceP->size) ? mceP->size : offset;

      MReleaseWriteLock(&mceP->afs_memLock);
      return bytesWritten;
  }

afs_MemWriteUIO(blkno, uioP)
     ino_t blkno;
     struct uio *uioP;
  {
      register struct memCacheEntry *mceP = (struct memCacheEntry *)afs_MemCacheOpen(blkno);
      afs_int32 code;

      AFS_STATCNT(afs_MemWriteUIO);
      MObtainWriteLock(&mceP->afs_memLock,312);
      if(uioP->uio_resid + uioP->uio_offset > mceP->dataSize) {
	  char *oldData = mceP->data;

	  mceP->data = afs_osi_Alloc(uioP->uio_resid + uioP->uio_offset);

	  AFS_GUNLOCK();
	  bcopy(oldData, mceP->data, mceP->size);
	  AFS_GLOCK();

	  afs_osi_Free(oldData,mceP->dataSize);
	  mceP->dataSize = uioP->uio_resid + uioP->uio_offset;
      }
      if (mceP->size < uioP->uio_offset)
	  bzero(mceP->data+mceP->size, (int)(uioP->uio_offset-mceP->size));
      AFS_UIOMOVE(mceP->data+uioP->uio_offset, uioP->uio_resid, UIO_WRITE, uioP, code);
      if (uioP->uio_offset > mceP->size)
	  mceP->size = uioP->uio_offset;

      MReleaseWriteLock(&mceP->afs_memLock);
      return code;
  }

afs_MemCacheTruncate(mceP, size)
     register struct memCacheEntry *mceP;
     int size;
  {
      AFS_STATCNT(afs_MemCacheTruncate);

      MObtainWriteLock(&mceP->afs_memLock,313);
      /* old directory entry; g.c. */
      if(size == 0 && mceP->dataSize > memCacheBlkSize) {
	  afs_osi_Free(mceP->data, mceP->dataSize);
	  mceP->data = afs_osi_Alloc(memCacheBlkSize);
	  mceP->dataSize = memCacheBlkSize;
      }

      if (size < mceP->size)
	  mceP->size = size;

      MReleaseWriteLock(&mceP->afs_memLock);
      return 0;
  }

afs_MemCacheStoreProc(acall, mceP, alen, avc, shouldWake, abytesToXferP, abytesXferredP)
     register struct memCacheEntry *mceP;
     register struct rx_call *acall;
     register afs_int32 alen;
     struct vcache *avc;
     int *shouldWake;
     afs_int32 *abytesToXferP;
     afs_int32 *abytesXferredP;

  {

      register afs_int32 code;
      register int tlen;
      int offset = 0;
      struct iovec *tiov;               /* no data copying with iovec */
      int tnio;				/* temp for iovec size */

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
      tiov = (struct iovec *) osi_AllocSmallSpace(sizeof(struct iovec)*RX_MAXIOVECS);
      if(!tiov) {
	osi_Panic("afs_MemCacheFetchProc: osi_AllocSmallSpace for iovecs returned NULL\n");
      }

#ifdef notdef
    /* do this at a higher level now -- it's a parameter */
      /* for now, only do 'continue from close' code if file fits in one
	 chunk.  Could clearly do better: if only one modified chunk
	 then can still do this.  can do this on *last* modified chunk */
      tlen = avc->m.Length-1; /* byte position of last byte we'll store */
      if (shouldWake) {
	if (AFS_CHUNK(tlen) != 0) *shouldWake = 0;
	else *shouldWake = 1;
      }
#endif /* notdef */

      while (alen > 0) {
	  tlen = (alen > AFS_LRALLOCSIZ? AFS_LRALLOCSIZ : alen);
	  code = rx_WritevAlloc(acall, tiov, &tnio, RX_MAXIOVECS, tlen);
	  if (code <= 0) {
	      osi_FreeSmallSpace(tiov);
	      return -33;
	  }
	  tlen = code;
	  code = afs_MemReadvBlk(mceP, offset, tiov, tnio, tlen);
	  if (code != tlen) {
	      osi_FreeSmallSpace(tiov);
	      return -33;
	  }
	  code = rx_Writev(acall, tiov, tnio, tlen);
#ifndef AFS_NOSTATS
	  (*abytesXferredP) += code;
#endif /* AFS_NOSTATS */
	  if (code != tlen) {
	      osi_FreeSmallSpace(tiov);
	      return -33;
	  }
	  offset += tlen;
	  alen -= tlen;
	  /* if file has been locked on server, can allow store to continue */
	  if (shouldWake && *shouldWake && (rx_GetRemoteStatus(acall) & 1)) {
	    *shouldWake = 0;  /* only do this once */
	      afs_wakeup(avc);
	  }
      }
      osi_FreeSmallSpace(tiov);
      return 0;
  }

afs_MemCacheFetchProc(acall, mceP, abase, adc, avc, abytesToXferP, abytesXferredP)
     register struct rx_call *acall;
     afs_int32 abase;
     struct dcache *adc;
     struct vcache *avc;
     register struct memCacheEntry *mceP;
     afs_int32 *abytesToXferP;
     afs_int32 *abytesXferredP;

  {
      afs_int32 length;
      register afs_int32 code;
      register int tlen, offset=0;
      int moredata;
      struct iovec *tiov;               /* no data copying with iovec */
      int tnio;				/* temp for iovec size */

      AFS_STATCNT(afs_MemCacheFetchProc);
#ifndef AFS_NOSTATS
      (*abytesToXferP)  = 0;
      (*abytesXferredP) = 0;
#endif /* AFS_NOSTATS */
      /* 
       * We need to alloc the iovecs on the heap so that they are "pinned" rather than
       * declare them on the stack - defect 11272
       */
      tiov = (struct iovec *) osi_AllocSmallSpace(sizeof(struct iovec)*RX_MAXIOVECS);
      if(!tiov) {
	osi_Panic("afs_MemCacheFetchProc: osi_AllocSmallSpace for iovecs returned NULL\n");
      }
      do {
	  code = rx_Read(acall, (char *)&length, sizeof(afs_int32));
	  if (code != sizeof(afs_int32)) {
	      code = rx_Error(acall);
	      osi_FreeSmallSpace(tiov);
	      return (code?code:-1);	/* try to return code, not -1 */
	  }
	  length = ntohl(length);
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
#endif				/* AFS_NOSTATS */
	  while (length > 0) {
	      tlen = (length > AFS_LRALLOCSIZ? AFS_LRALLOCSIZ : length);
	      code = rx_Readv(acall, tiov, &tnio, RX_MAXIOVECS, tlen);
#ifndef AFS_NOSTATS
	      (*abytesXferredP) += code;
#endif				/* AFS_NOSTATS */
	      if (code <= 0) {
		  osi_FreeSmallSpace(tiov);
		  return -34;
	      }
	      tlen = code;
	      afs_MemWritevBlk(mceP, offset, tiov, tnio, tlen);
	      offset += tlen;
	      abase += tlen;
	      length -= tlen;
	      adc->validPos = abase;
	      if (adc->flags & DFWaiting) {
		  adc->flags &= ~DFWaiting;
		  afs_osi_Wakeup(&adc->validPos);
	      }
	  }
      } while (moredata);
      /* max of two sizes */
      osi_FreeSmallSpace(tiov);
      return 0;
  }


void shutdown_memcache()
{
    register int index;

    if (cacheDiskType != AFS_FCACHE_TYPE_MEM)
	return;
    memCacheBlkSize = 8192;
    for (index = 0; index < memMaxBlkNumber; index++) {
	LOCK_INIT(&((memCache+index)->afs_memLock), "afs_memLock");
	afs_osi_Free((memCache+index)->data, (memCache+index)->dataSize);
    }
    afs_osi_Free((char *)memCache, memMaxBlkNumber * sizeof(struct memCacheEntry));
    memMaxBlkNumber = 0;
}
