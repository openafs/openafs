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
    void *(*open) (afs_dcache_id_t *ainode);
    int (*truncate) (struct osi_file * fp, afs_int32 len);
    int (*fread) (struct osi_file * fp, int offset, void *buf, afs_int32 len);
    int (*fwrite) (struct osi_file * fp, afs_int32 offset, void *buf,
		   afs_int32 len);
    int (*close) (struct osi_file * fp);
    int (*vread) (struct vcache * avc, struct uio * auio,
		  afs_ucred_t * acred, daddr_t albn, struct buf ** abpp,
		  int noLock);
    int (*vwrite) (struct vcache * avc, struct uio * auio, int aio,
		   afs_ucred_t * acred, int noLock);
    struct dcache *(*GetDSlot) (afs_int32 aslot, int indexvalid, int datavalid);
    struct volume *(*GetVolSlot) (void);
    int (*HandleLink) (struct vcache * avc, struct vrequest * areq);
};

/* Ideally we should have used consistent naming - like COP_OPEN, COP_TRUNCATE, etc. */
#define	afs_CFileOpen(inode)	      (void *)(*(afs_cacheType->open))(inode)
#define	afs_CFileTruncate(handle, size)	(*(afs_cacheType->truncate))((handle), size)
#define	afs_CFileRead(file, offset, data, size) (*(afs_cacheType->fread))(file, offset, data, size)
#define	afs_CFileWrite(file, offset, data, size) (*(afs_cacheType->fwrite))(file, offset, data, size)
#define	afs_CFileClose(handle)		(*(afs_cacheType->close))(handle)
#define	afs_GetVolSlot()		(*(afs_cacheType->GetVolSlot))()
#define	afs_HandleLink(avc, areq)	(*(afs_cacheType->HandleLink))(avc, areq)

/* Use afs_GetValidDSlot to get a dcache from a dcache slot number when we
 * know the dcache contains data we want (e.g. it's on the hash table) */
#define	afs_GetValidDSlot(slot)	(*(afs_cacheType->GetDSlot))(slot, 1, 1)
/* Use afs_GetUnusedDSlot when loading a dcache entry that is on the free or
 * discard lists (the dcache does not contain valid data, but we know the
 * dcache entry itself exists). */
#define	afs_GetUnusedDSlot(slot)	(*(afs_cacheType->GetDSlot))(slot, 1, 0)
/* Use afs_GetNewDSlot only when initializing dcache slots (the given slot
 * number may not exist at all). */
#define	afs_GetNewDSlot(slot)	(*(afs_cacheType->GetDSlot))(slot, 0, 0)

/* These memcpys should get optimised to simple assignments when afs_dcache_id_t
 * is simple */
static_inline void afs_copy_inode(afs_dcache_id_t *dst, afs_dcache_id_t *src) {
    memcpy(dst, src, sizeof(afs_dcache_id_t));
}

static_inline void afs_reset_inode(afs_dcache_id_t *i) {
    memset(i, 0, sizeof(afs_dcache_id_t));
}

/* We need to have something we can output as the 'inode' for fstrace calls.
 * This is a hack */
static_inline int afs_inode2trace(afs_dcache_id_t *i) {
    return i->mem;
}


#endif /* AFS_CHUNKOPS */
