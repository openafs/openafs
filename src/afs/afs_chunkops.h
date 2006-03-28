/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_CHUNKOPS
#define AFS_CHUNKOPS 1

/* macros to compute useful numbers from offsets.  AFS_CHUNK gives the chunk
    number for a given offset; AFS_CHUNKOFFSET gives the offset into the chunk
    and AFS_CHUNKBASE gives the byte offset of the base of the chunk.
      AFS_CHUNKSIZE gives the size of the chunk containing an offset.
      AFS_CHUNKTOBASE converts a chunk # to a base position.
      Chunks are 0 based and go up by exactly 1, covering the file.
      The other fields are internal and shouldn't be used */
/* basic parameters */

#define AFS_OTHERCSIZE  (afs_OtherCSize)
#define AFS_LOGCHUNK    (afs_LogChunk)
#define AFS_FIRSTCSIZE  (afs_FirstCSize)

#define AFS_DEFAULTCSIZE 0x10000
#define AFS_DEFAULTLSIZE 16

#define AFS_CHUNKOFFSET(offset) ((offset < afs_FirstCSize) ? offset : \
			 ((offset - afs_FirstCSize) & (afs_OtherCSize - 1)))

#define AFS_CHUNK(offset) ((offset < afs_FirstCSize) ? 0 : \
	                 (((offset - afs_FirstCSize) >> afs_LogChunk) + 1))

#define AFS_CHUNKBASE(offset) ((offset < afs_FirstCSize) ? 0 : \
	(((offset - afs_FirstCSize) & ~(afs_OtherCSize - 1)) + afs_FirstCSize))

#define AFS_CHUNKSIZE(offset) ((offset < afs_FirstCSize) ? afs_FirstCSize : \
			       afs_OtherCSize)

#define AFS_CHUNKTOBASE(chunk) ((chunk == 0) ? 0 :               \
	((afs_size_t) afs_FirstCSize + ((afs_size_t) (chunk - 1) << afs_LogChunk)))

#define AFS_CHUNKTOSIZE(chunk) ((chunk == 0) ? afs_FirstCSize :	afs_OtherCSize)

/* sizes are a power of two */
#define AFS_SETCHUNKSIZE(chunk) { afs_LogChunk = chunk; \
		      afs_FirstCSize = afs_OtherCSize = (1 << chunk);  }

/*
 * Functions exported by a cache type 
 */

struct afs_cacheOps {
#if defined(AFS_SUN57_64BIT_ENV) || defined(AFS_SGI62_ENV)
    void *(*open) (ino_t ainode);
#else
    void *(*open) (afs_int32 ainode);
#endif
    int (*truncate) (struct osi_file * fp, afs_int32 len);
    int (*fread) (struct osi_file * fp, int offset, void *buf, afs_int32 len);
    int (*fwrite) (struct osi_file * fp, afs_int32 offset, void *buf,
		   afs_int32 len);
    int (*close) (struct osi_file * fp);
    int (*vread) (register struct vcache * avc, struct uio * auio,
		  struct AFS_UCRED * acred, daddr_t albn, struct buf ** abpp,
		  int noLock);
    int (*vwrite) (register struct vcache * avc, struct uio * auio, int aio,
		   struct AFS_UCRED * acred, int noLock);
    int (*FetchProc) (register struct rx_call * acall,
		      struct osi_file * afile, afs_size_t abase,
		      struct dcache * adc, struct vcache * avc,
		      afs_size_t * abytesToXferP, afs_size_t * abytesXferredP,
		      afs_int32 lengthFound);
    int (*StoreProc) (register struct rx_call * acall,
		      struct osi_file * afile, register afs_int32 alen,
		      struct vcache * avc, int *shouldWake,
		      afs_size_t * abytesToXferP,
		      afs_size_t * abytesXferredP);
    struct dcache *(*GetDSlot) (register afs_int32 aslot,
				register struct dcache * tmpdc);
    struct volume *(*GetVolSlot) (void);
    int (*HandleLink) (register struct vcache * avc, struct vrequest * areq);
};

/* Ideally we should have used consistent naming - like COP_OPEN, COP_TRUNCATE, etc. */
#define	afs_CFileOpen(inode)	      (void *)(*(afs_cacheType->open))(inode)
#define	afs_CFileTruncate(handle, size)	(*(afs_cacheType->truncate))((handle), size)
#define	afs_CFileRead(file, offset, data, size) (*(afs_cacheType->fread))(file, offset, data, size)
#define	afs_CFileWrite(file, offset, data, size) (*(afs_cacheType->fwrite))(file, offset, data, size)
#define	afs_CFileClose(handle)		(*(afs_cacheType->close))(handle)
#define	afs_GetDSlot(slot, adc)		(*(afs_cacheType->GetDSlot))(slot, adc)
#define	afs_GetVolSlot()		(*(afs_cacheType->GetVolSlot))()
#define	afs_HandleLink(avc, areq)	(*(afs_cacheType->HandleLink))(avc, areq)

#define	afs_CacheFetchProc(call, file, base, adc, avc, toxfer, xfered, length) \
          (*(afs_cacheType->FetchProc))(call, file, (afs_size_t)base, adc, avc, (afs_size_t *)toxfer, (afs_size_t *)xfered, length)
#define	afs_CacheStoreProc(call, file, bytes, avc, wake, toxfer, xfered) \
          (*(afs_cacheType->StoreProc))(call, file, bytes, avc, wake, toxfer, xfered)

#endif /* AFS_CHUNKOPS */
