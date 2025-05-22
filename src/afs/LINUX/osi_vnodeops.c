/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Linux specific vnodeops. Also includes the glue routines required to call
 * AFS vnodeops.
 *
 * So far the only truly scary part is that Linux relies on the inode cache
 * to be up to date. Don't you dare break a callback and expect an fstat
 * to give you meaningful information. This appears to be fixed in the 2.1
 * development kernels. As it is we can fix this now by intercepting the
 * stat calls.
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"
#include <linux/mm.h>
#include <linux/buffer_head.h>
#ifdef HAVE_MM_INLINE_H
#include <linux/mm_inline.h>
#endif
#include <linux/pagemap.h>
#include <linux/writeback.h>
#if defined(HAVE_LINUX_FOLIO_ADD_LRU) || defined(HAVE_LINUX_LRU_CACHE_ADD_FILE)
# include <linux/swap.h>
#else
# include <linux/pagevec.h>
#endif
#include <linux/aio.h>
#include "afs/lock.h"
#include "afs/afs_bypasscache.h"

#include "osi_compat.h"
#include "osi_pagecopy.h"

#ifndef MAX_ERRNO
#define MAX_ERRNO 1000L
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34)
/* Enable our workaround for a race with d_splice_alias. The race was fixed in
 * 2.6.34, so don't do it after that point. */
# define D_SPLICE_ALIAS_RACE
#endif

#if defined(STRUCT_FILE_OPERATIONS_HAS_ITERATE_SHARED) && defined(HAVE_LINUX_WRAP_DIRECTORY_ITERATOR)
# define USE_FOP_ITERATE 1
#elif defined(STRUCT_FILE_OPERATIONS_HAS_ITERATE) && !defined(FMODE_KABI_ITERATE)
/* Workaround for RH 7.5 which introduced file operation iterate() but requires
 * each file->f_mode to be marked with FMODE_KABI_ITERATE.  Instead OpenAFS will
 * continue to use file opearation readdir() in this case.
 */
# define USE_FOP_ITERATE 1
#else
# undef USE_FOP_ITERATE
#endif

/* Kernels from before 2.6.19 may not be able to return errors from
 * d_revalidate. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
# define ERRORS_FROM_D_REVALIDATE
#endif

int cachefs_noreadpage = 0;

extern struct backing_dev_info *afs_backing_dev_info;

extern struct vcache *afs_globalVp;

/* Handle interfacing with Linux's pagevec/lru facilities */

#if defined(HAVE_LINUX_FOLIO_ADD_LRU) || \
    defined(HAVE_LINUX_LRU_CACHE_ADD_FILE) || defined(HAVE_LINUX_LRU_CACHE_ADD)

/*
 * Linux's lru_cache_add_file provides a simplified LRU interface without
 * needing a pagevec
 */
struct afs_lru_pages {
    char unused;
};

static inline void
afs_lru_cache_init(struct afs_lru_pages *alrupages)
{
    return;
}

static inline void
afs_lru_cache_add(struct afs_lru_pages *alrupages, struct page *page)
{
# if defined(HAVE_LINUX_FOLIO_ADD_LRU)
    struct folio *folio = page_folio(page);
    folio_add_lru(folio);
# elif defined(HAVE_LINUX_LRU_CACHE_ADD)
    lru_cache_add(page);
# elif defined(HAVE_LINUX_LRU_CACHE_ADD_FILE)
    lru_cache_add_file(page);
# else
#  error need a kernel function to add a page to the kernel lru cache
# endif
}

static inline void
afs_lru_cache_finalize(struct afs_lru_pages *alrupages)
{
    return;
}
#else

/* Linux's pagevec/lru interfaces require a pagevec */
struct afs_lru_pages {
    struct pagevec lrupv;
};

static inline void
afs_lru_cache_init(struct afs_lru_pages *alrupages)
{
# if defined(PAGEVEC_INIT_COLD_ARG)
    pagevec_init(&alrupages->lrupv, 0);
# else
    pagevec_init(&alrupages->lrupv);
# endif
}

# ifndef HAVE_LINUX_PAGEVEC_LRU_ADD_FILE
#  define __pagevec_lru_add_file __pagevec_lru_add
# endif

static inline void
afs_lru_cache_add(struct afs_lru_pages *alrupages, struct page *page)
{
    get_page(page);
    if (!pagevec_add(&alrupages->lrupv, page))
	__pagevec_lru_add_file(&alrupages->lrupv);
}

static inline void
afs_lru_cache_finalize(struct afs_lru_pages *alrupages)
{
    if (pagevec_count(&alrupages->lrupv))
	__pagevec_lru_add_file(&alrupages->lrupv);
}
#endif /* !HAVE_LINUX_LRU_ADD_FILE */

static inline int
afs_add_to_page_cache_lru(struct afs_lru_pages *alrupages, struct page *page,
			  struct address_space *mapping,
			  pgoff_t index, gfp_t gfp)
{
#if defined(HAVE_LINUX_ADD_TO_PAGE_CACHE_LRU)
    return add_to_page_cache_lru(page, mapping, index, gfp);
#else
    int code;
    code = add_to_page_cache(page, mapping, index, gfp);
    if (code == 0) {
	afs_lru_cache_add(alrupages, page);
    }
    return code;
#endif
}

/* This function converts a positive error code from AFS into a negative
 * code suitable for passing into the Linux VFS layer. It checks that the
 * error code is within the permissable bounds for the ERR_PTR mechanism.
 *
 * _All_ error codes which come from the AFS layer should be passed through
 * this function before being returned to the kernel.
 */

static inline int
afs_convert_code(int code) {
    if ((code >= 0) && (code <= MAX_ERRNO))
	return -code;
    else
	return -EIO;
}

/* Linux doesn't require a credp for many functions, and crref is an expensive
 * operation. This helper function avoids obtaining it for VerifyVCache calls
 */

static inline int
afs_linux_VerifyVCache(struct vcache *avc, cred_t **retcred) {
    cred_t *credp = NULL;
    struct vrequest *treq = NULL;
    int code;

    if (avc->f.states & CStatd) {
        if (retcred)
            *retcred = NULL;
	return 0;
    }

    credp = crref();

    code = afs_CreateReq(&treq, credp);
    if (code == 0) {
	code = afs_VerifyVCache(avc, treq);
	afs_DestroyReq(treq);
    }

    if (retcred != NULL)
        *retcred = credp;
    else
        crfree(credp);

    return afs_convert_code(code);
}

#if defined(STRUCT_FILE_OPERATIONS_HAS_READ_ITER) || defined(HAVE_LINUX_GENERIC_FILE_AIO_READ)
# if defined(STRUCT_FILE_OPERATIONS_HAS_READ_ITER)
static ssize_t
afs_linux_read_iter(struct kiocb *iocb, struct iov_iter *iter)
# elif defined(LINUX_HAS_NONVECTOR_AIO)
static ssize_t
afs_linux_aio_read(struct kiocb *iocb, char __user *buf, size_t bufsize,
                   loff_t pos)
# else
static ssize_t
afs_linux_aio_read(struct kiocb *iocb, const struct iovec *buf,
                   unsigned long bufsize, loff_t pos)
# endif
{
    struct file *fp = iocb->ki_filp;
    ssize_t code = 0;
    struct vcache *vcp = VTOAFS(fp->f_dentry->d_inode);
# if defined(STRUCT_FILE_OPERATIONS_HAS_READ_ITER)
    loff_t pos = iocb->ki_pos;
    unsigned long bufsize = iter->nr_segs;
# endif


    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_AIOREADOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(pos), ICL_TYPE_INT32,
               (afs_int32)bufsize, ICL_TYPE_INT32, 99999);
    code = afs_linux_VerifyVCache(vcp, NULL);

    if (code == 0) {
	/* Linux's FlushPages implementation doesn't ever use credp,
	 * so we optimise by not using it */
	osi_FlushPages(vcp, NULL);	/* ensure stale pages are gone */
	AFS_GUNLOCK();
# if defined(STRUCT_FILE_OPERATIONS_HAS_READ_ITER)
	code = generic_file_read_iter(iocb, iter);
# else
	code = generic_file_aio_read(iocb, buf, bufsize, pos);
# endif
	AFS_GLOCK();
    }

    afs_Trace4(afs_iclSetp, CM_TRACE_AIOREADOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(pos), ICL_TYPE_INT32,
               (afs_int32)bufsize, ICL_TYPE_INT32, code);
    AFS_GUNLOCK();
    return code;
}
#else
static ssize_t
afs_linux_read(struct file *fp, char *buf, size_t count, loff_t * offp)
{
    ssize_t code = 0;
    struct vcache *vcp = VTOAFS(fp->f_dentry->d_inode);

    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_READOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, offp, ICL_TYPE_INT32, count, ICL_TYPE_INT32,
	       99999);
    code = afs_linux_VerifyVCache(vcp, NULL);

    if (code == 0) {
	/* Linux's FlushPages implementation doesn't ever use credp,
	 * so we optimise by not using it */
	osi_FlushPages(vcp, NULL);	/* ensure stale pages are gone */
	AFS_GUNLOCK();
	code = do_sync_read(fp, buf, count, offp);
	AFS_GLOCK();
    }

    afs_Trace4(afs_iclSetp, CM_TRACE_READOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, offp, ICL_TYPE_INT32, count, ICL_TYPE_INT32,
	       code);
    AFS_GUNLOCK();
    return code;
}
#endif


/* Now we have integrated VM for writes as well as reads. the generic write operations
 * also take care of re-positioning the pointer if file is open in append
 * mode. Call fake open/close to ensure we do writes of core dumps.
 */
#if defined(STRUCT_FILE_OPERATIONS_HAS_READ_ITER) || defined(HAVE_LINUX_GENERIC_FILE_AIO_READ)
# if defined(STRUCT_FILE_OPERATIONS_HAS_READ_ITER)
static ssize_t
afs_linux_write_iter(struct kiocb *iocb, struct iov_iter *iter)
# elif defined(LINUX_HAS_NONVECTOR_AIO)
static ssize_t
afs_linux_aio_write(struct kiocb *iocb, const char __user *buf, size_t bufsize,
                    loff_t pos)
# else
static ssize_t
afs_linux_aio_write(struct kiocb *iocb, const struct iovec *buf,
                    unsigned long bufsize, loff_t pos)
# endif
{
    ssize_t code = 0;
    struct vcache *vcp = VTOAFS(iocb->ki_filp->f_dentry->d_inode);
    cred_t *credp;
# if defined(STRUCT_FILE_OPERATIONS_HAS_READ_ITER)
    loff_t pos = iocb->ki_pos;
    unsigned long bufsize = iter->nr_segs;
# endif

    AFS_GLOCK();

    afs_Trace4(afs_iclSetp, CM_TRACE_AIOWRITEOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(pos), ICL_TYPE_INT32,
               (afs_int32)bufsize, ICL_TYPE_INT32,
	       (iocb->ki_filp->f_flags & O_APPEND) ? 99998 : 99999);

    code = afs_linux_VerifyVCache(vcp, &credp);

    ObtainWriteLock(&vcp->lock, 529);
    afs_FakeOpen(vcp);
    ReleaseWriteLock(&vcp->lock);
    if (code == 0) {
	    AFS_GUNLOCK();
# if defined(STRUCT_FILE_OPERATIONS_HAS_READ_ITER)
	    code = generic_file_write_iter(iocb, iter);
# else
	    code = generic_file_aio_write(iocb, buf, bufsize, pos);
# endif
	    AFS_GLOCK();
    }

    ObtainWriteLock(&vcp->lock, 530);

    if (vcp->execsOrWriters == 1 && !credp)
      credp = crref();

    afs_FakeClose(vcp, credp);
    ReleaseWriteLock(&vcp->lock);

    afs_Trace4(afs_iclSetp, CM_TRACE_AIOWRITEOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(pos), ICL_TYPE_INT32,
               (afs_int32)bufsize, ICL_TYPE_INT32, code);

    if (credp)
      crfree(credp);
    AFS_GUNLOCK();
    return code;
}
#else
static ssize_t
afs_linux_write(struct file *fp, const char *buf, size_t count, loff_t * offp)
{
    ssize_t code = 0;
    struct vcache *vcp = VTOAFS(fp->f_dentry->d_inode);
    cred_t *credp;

    AFS_GLOCK();

    afs_Trace4(afs_iclSetp, CM_TRACE_WRITEOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, offp, ICL_TYPE_INT32, count, ICL_TYPE_INT32,
	       (fp->f_flags & O_APPEND) ? 99998 : 99999);

    code = afs_linux_VerifyVCache(vcp, &credp);

    ObtainWriteLock(&vcp->lock, 529);
    afs_FakeOpen(vcp);
    ReleaseWriteLock(&vcp->lock);
    if (code == 0) {
	    AFS_GUNLOCK();
	    code = do_sync_write(fp, buf, count, offp);
	    AFS_GLOCK();
    }

    ObtainWriteLock(&vcp->lock, 530);

    if (vcp->execsOrWriters == 1 && !credp)
      credp = crref();

    afs_FakeClose(vcp, credp);
    ReleaseWriteLock(&vcp->lock);

    afs_Trace4(afs_iclSetp, CM_TRACE_WRITEOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, offp, ICL_TYPE_INT32, count, ICL_TYPE_INT32,
	       code);

    if (credp)
      crfree(credp);
    AFS_GUNLOCK();
    return code;
}
#endif

/* This is a complete rewrite of afs_readdir, since we can make use of
 * filldir instead of afs_readdir_move. Note that changes to vcache/dcache
 * handling and use of bulkstats will need to be reflected here as well.
 */
static int
#if defined(USE_FOP_ITERATE)
afs_linux_readdir(struct file *fp, struct dir_context *ctx)
#else
afs_linux_readdir(struct file *fp, void *dirbuf, filldir_t filldir)
#endif
{
    struct vcache *avc = VTOAFS(FILE_INODE(fp));
    struct vrequest *treq = NULL;
    struct dcache *tdc;
    int code;
    int offset;
    afs_int32 dirpos;
    struct DirEntryFlex *de;
    struct DirBuffer entry;
    ino_t ino;
    int len;
    afs_size_t origOffset, tlen;
    cred_t *credp = crref();
    struct afs_fakestat_state fakestat;

    AFS_GLOCK();
    AFS_STATCNT(afs_readdir);

    code = afs_convert_code(afs_CreateReq(&treq, credp));
    crfree(credp);
    if (code)
	goto out1;

    afs_InitFakeStat(&fakestat);
    code = afs_convert_code(afs_EvalFakeStat(&avc, &fakestat, treq));
    if (code)
	goto out;

    /* update the cache entry */
  tagain:
    code = afs_convert_code(afs_VerifyVCache(avc, treq));
    if (code)
	goto out;

    /* get a reference to the entire directory */
    tdc = afs_GetDCache(avc, (afs_size_t) 0, treq, &origOffset, &tlen, 1);
    len = tlen;
    if (!tdc) {
	code = -EIO;
	goto out;
    }
    ObtainWriteLock(&avc->lock, 811);
    ObtainReadLock(&tdc->lock);
    /*
     * Make sure that the data in the cache is current. There are two
     * cases we need to worry about:
     * 1. The cache data is being fetched by another process.
     * 2. The cache data is no longer valid
     */
    while ((avc->f.states & CStatd)
	   && (tdc->dflags & DFFetching)
	   && afs_IsDCacheFresh(tdc, avc)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseWriteLock(&avc->lock);
	afs_osi_Sleep(&tdc->validPos);
	ObtainWriteLock(&avc->lock, 812);
	ObtainReadLock(&tdc->lock);
    }
    if (!(avc->f.states & CStatd)
	|| !afs_IsDCacheFresh(tdc, avc)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseWriteLock(&avc->lock);
	afs_PutDCache(tdc);
	goto tagain;
    }

    /* Set the readdir-in-progress flag, and downgrade the lock
     * to shared so others will be able to acquire a read lock.
     */
    avc->f.states |= CReadDir;
    avc->dcreaddir = tdc;
    avc->readdir_pid = MyPidxx2Pid(MyPidxx);
    ConvertWToSLock(&avc->lock);

    /* Fill in until we get an error or we're done. This implementation
     * takes an offset in units of blobs, rather than bytes.
     */
    code = 0;
#if defined(USE_FOP_ITERATE)
    offset = ctx->pos;
#else
    offset = (int) fp->f_pos;
#endif
    while (1) {
        dirpos = 0;
	code = BlobScan(tdc, offset, &dirpos);
        if (code == 0 && dirpos == 0) {
            /* We've reached EOF of the dir blob, so we can stop looking for
             * entries. */
            break;
        }

        if (code == 0) {
            code = afs_dir_GetVerifiedBlob(tdc, dirpos, &entry);
        }
	if (code) {
	    if (!(avc->f.states & CCorrupt)) {
		struct cell *tc = afs_GetCellStale(avc->f.fid.Cell, READ_LOCK);
		afs_warn("afs: Corrupt directory (%d.%d.%d.%d [%s] @%lx, pos %d)\n",
			 avc->f.fid.Cell, avc->f.fid.Fid.Volume,
			 avc->f.fid.Fid.Vnode, avc->f.fid.Fid.Unique,
			 tc ? tc->cellName : "",
			 (unsigned long)&tdc->f.inode, dirpos);
		if (tc)
		    afs_PutCell(tc, READ_LOCK);
		UpgradeSToWLock(&avc->lock, 814);
		avc->f.states |= CCorrupt;
	    }
	    code = -EIO;
	    goto unlock_out;
        }

	de = entry.data;
	ino = afs_calc_inum (avc->f.fid.Cell, avc->f.fid.Fid.Volume,
			     ntohl(de->fid.vnode));
	len = strlen(de->name);

	/* filldir returns -EINVAL when the buffer is full. */
	{
	    unsigned int type = DT_UNKNOWN;
	    struct VenusFid afid;
	    struct vcache *tvc;
	    int vtype;
	    afid.Cell = avc->f.fid.Cell;
	    afid.Fid.Volume = avc->f.fid.Fid.Volume;
	    afid.Fid.Vnode = ntohl(de->fid.vnode);
	    afid.Fid.Unique = ntohl(de->fid.vunique);
	    if ((avc->f.states & CForeign) == 0 && (ntohl(de->fid.vnode) & 1)) {
		type = DT_DIR;
	    } else if ((tvc = afs_FindVCache(&afid, 0, 0))) {
		if (tvc->mvstat != AFS_MVSTAT_FILE) {
		    type = DT_DIR;
		} else if (((tvc->f.states) & (CStatd | CTruth))) {
		    /* CTruth will be set if the object has
		     *ever* been statd */
		    vtype = vType(tvc);
		    if (vtype == VDIR)
			type = DT_DIR;
		    else if (vtype == VREG)
			type = DT_REG;
		    /* Don't do this until we're sure it can't be a mtpt */
		    /* else if (vtype == VLNK)
		     * type=DT_LNK; */
		    /* what other types does AFS support? */
		}
		/* clean up from afs_FindVCache */
		afs_PutVCache(tvc);
	    }
	    /*
	     * If this is NFS readdirplus, then the filler is going to
	     * call getattr on this inode, which will deadlock if we're
	     * holding the GLOCK.
	     */
	    AFS_GUNLOCK();
#if defined(USE_FOP_ITERATE)
	    /* dir_emit returns a bool - true when it succeeds.
	     * Inverse the result to fit with how we check "code" */
	    code = !dir_emit(ctx, de->name, len, ino, type);
#else
	    code = (*filldir) (dirbuf, de->name, len, offset, ino, type);
#endif
	    AFS_GLOCK();
	}
	DRelease(&entry, 0);
	if (code)
	    break;
	offset = dirpos + 1 + ((len + 16) >> 5);
    }
    /* If filldir didn't fill in the last one this is still pointing to that
     * last attempt.
     */
    code = 0;

unlock_out:
#if defined(USE_FOP_ITERATE)
    ctx->pos = (loff_t) offset;
#else
    fp->f_pos = (loff_t) offset;
#endif
    ReleaseReadLock(&tdc->lock);
    afs_PutDCache(tdc);
    UpgradeSToWLock(&avc->lock, 813);
    avc->f.states &= ~CReadDir;
    avc->dcreaddir = 0;
    avc->readdir_pid = 0;
    ReleaseSharedLock(&avc->lock);

out:
    afs_PutFakeStat(&fakestat);
    afs_DestroyReq(treq);
out1:
    AFS_GUNLOCK();
    return code;
}


static long afs_unlocked_xioctl(struct file *fp, unsigned int com,
                               unsigned long arg) {
    return afs_xioctl(FILE_INODE(fp), fp, com, arg);

}


static int
afs_linux_mmap(struct file *fp, struct vm_area_struct *vmap)
{
    struct vcache *vcp = VTOAFS(FILE_INODE(fp));
    int code;

    AFS_GLOCK();
    afs_Trace3(afs_iclSetp, CM_TRACE_GMAP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, vmap->vm_start, ICL_TYPE_INT32,
	       vmap->vm_end - vmap->vm_start);

    /* get a validated vcache entry */
    code = afs_linux_VerifyVCache(vcp, NULL);

    if (code == 0) {
        /* Linux's Flushpage implementation doesn't use credp, so optimise
         * our code to not need to crref() it */
        osi_FlushPages(vcp, NULL); /* ensure stale pages are gone */
        AFS_GUNLOCK();
        code = generic_file_mmap(fp, vmap);
        AFS_GLOCK();
        if (!code)
            vcp->f.states |= CMAPPED;
    }
    AFS_GUNLOCK();

    return code;
}

static int
afs_linux_open(struct inode *ip, struct file *fp)
{
    struct vcache *vcp = VTOAFS(ip);
    cred_t *credp = crref();
    int code;

    AFS_GLOCK();
    code = afs_open(&vcp, fp->f_flags, credp);
    AFS_GUNLOCK();

    crfree(credp);
    return afs_convert_code(code);
}

static int
afs_linux_release(struct inode *ip, struct file *fp)
{
    struct vcache *vcp = VTOAFS(ip);
    cred_t *credp = crref();
    int code = 0;

    AFS_GLOCK();
    code = afs_close(vcp, fp->f_flags, credp);
    ObtainWriteLock(&vcp->lock, 807);
    if (vcp->cred) {
	crfree(vcp->cred);
	vcp->cred = NULL;
    }
    ReleaseWriteLock(&vcp->lock);
    AFS_GUNLOCK();

    crfree(credp);
    return afs_convert_code(code);
}

static int
#if defined(FOP_FSYNC_TAKES_DENTRY)
afs_linux_fsync(struct file *fp, struct dentry *dp, int datasync)
#elif defined(FOP_FSYNC_TAKES_RANGE)
afs_linux_fsync(struct file *fp, loff_t start, loff_t end, int datasync)
#else
afs_linux_fsync(struct file *fp, int datasync)
#endif
{
    int code;
    struct inode *ip = FILE_INODE(fp);
    cred_t *credp = crref();

#if defined(FOP_FSYNC_TAKES_RANGE)
    afs_linux_lock_inode(ip);
#endif
    AFS_GLOCK();
    code = afs_fsync(VTOAFS(ip), credp);
    AFS_GUNLOCK();
#if defined(FOP_FSYNC_TAKES_RANGE)
    afs_linux_unlock_inode(ip);
#endif
    crfree(credp);
    return afs_convert_code(code);

}

/* Handle getting/setting file_lock type */

static inline unsigned char
afs_get_flock_type(struct file_lock *flp)
{
#if defined(HAVE_FILE_LOCK_CORE)
    return flp->c.flc_type;
#else
    return flp->fl_type;
#endif
}

static inline void
afs_set_flock_type(struct file_lock *flp, unsigned char type)
{
#if defined(HAVE_FILE_LOCK_CORE)
    flp->c.flc_type = type;
#else
    flp->fl_type = type;
#endif
}

/* Handle getting/setting file_lock pid */

static inline pid_t
afs_get_flock_pid(struct file_lock *flp)
{
#if defined(HAVE_FILE_LOCK_CORE)
    return flp->c.flc_pid;
#else
    return flp->fl_pid;
#endif
}

static inline void
afs_set_flock_pid(struct file_lock *flp, pid_t pid)
{
#if defined(HAVE_FILE_LOCK_CORE)
    flp->c.flc_pid = pid;
#else
    flp->fl_pid = pid;
#endif
}

/* Handle clearing file_lock sleep */

static inline void
afs_clear_flock_sleep(struct file_lock *flp)
{
#if defined(HAVE_FILE_LOCK_CORE)
	flp->c.flc_flags &= ~FL_SLEEP;
#else
	flp->fl_flags &= ~FL_SLEEP;
#endif
}

static int
afs_linux_lock(struct file *fp, int cmd, struct file_lock *flp)
{
    int code = 0;
    struct vcache *vcp = VTOAFS(FILE_INODE(fp));
    cred_t *credp = crref();
    struct AFS_FLOCK flock;

    /* Convert to a lock format afs_lockctl understands. */
    memset(&flock, 0, sizeof(flock));
    flock.l_type = afs_get_flock_type(flp);
    flock.l_pid = afs_get_flock_pid(flp);
    flock.l_whence = 0;
    flock.l_start = flp->fl_start;
    if (flp->fl_end == OFFSET_MAX)
	flock.l_len = 0; /* Lock to end of file */
    else
	flock.l_len = flp->fl_end - flp->fl_start + 1;

    /* Safe because there are no large files, yet */
#if defined(F_GETLK64) && (F_GETLK != F_GETLK64)
    if (cmd == F_GETLK64)
	cmd = F_GETLK;
    else if (cmd == F_SETLK64)
	cmd = F_SETLK;
    else if (cmd == F_SETLKW64)
	cmd = F_SETLKW;
#endif /* F_GETLK64 && F_GETLK != F_GETLK64 */

    AFS_GLOCK();
    code = afs_convert_code(afs_lockctl(vcp, &flock, cmd, credp));
    AFS_GUNLOCK();

    if ((code == 0 || afs_get_flock_type(flp) == F_UNLCK) &&
	(cmd == F_SETLK || cmd == F_SETLKW)) {
	code = afs_posix_lock_file(fp, flp);
	if (code && afs_get_flock_type(flp) != F_UNLCK) {
	    struct AFS_FLOCK flock2;
	    flock2 = flock;
	    flock2.l_type = F_UNLCK;
	    AFS_GLOCK();
	    afs_lockctl(vcp, &flock2, F_SETLK, credp);
	    AFS_GUNLOCK();
	}
    }
    /* If lockctl says there are no conflicting locks, then also check with the
     * kernel, as lockctl knows nothing about byte range locks
     */
    if (code == 0 && cmd == F_GETLK && flock.l_type == F_UNLCK) {
	afs_posix_test_lock(fp, flp);
	/* If we found a lock in the kernel's structure, return it */
	if (afs_get_flock_type(flp) != F_UNLCK) {
	    crfree(credp);
	    return 0;
	}
    }

    /* Convert flock back to Linux's file_lock */
    afs_set_flock_type(flp, flock.l_type);
    afs_set_flock_pid(flp, flock.l_pid);
    flp->fl_start = flock.l_start;
    if (flock.l_len == 0)
	flp->fl_end = OFFSET_MAX; /* Lock to end of file */
    else
	flp->fl_end = flock.l_start + flock.l_len - 1;

    crfree(credp);
    return code;
}

#ifdef STRUCT_FILE_OPERATIONS_HAS_FLOCK
static int
afs_linux_flock(struct file *fp, int cmd, struct file_lock *flp) {
    int code = 0;
    struct vcache *vcp = VTOAFS(FILE_INODE(fp));
    cred_t *credp = crref();
    struct AFS_FLOCK flock;
    /* Convert to a lock format afs_lockctl understands. */
    memset(&flock, 0, sizeof(flock));
    flock.l_type = afs_get_flock_type(flp);
    flock.l_pid = afs_get_flock_pid(flp);
    flock.l_whence = 0;
    flock.l_start = 0;
    flock.l_len = 0;

    /* Safe because there are no large files, yet */
#if defined(F_GETLK64) && (F_GETLK != F_GETLK64)
    if (cmd == F_GETLK64)
	cmd = F_GETLK;
    else if (cmd == F_SETLK64)
	cmd = F_SETLK;
    else if (cmd == F_SETLKW64)
	cmd = F_SETLKW;
#endif /* F_GETLK64 && F_GETLK != F_GETLK64 */

    AFS_GLOCK();
    code = afs_convert_code(afs_lockctl(vcp, &flock, cmd, credp));
    AFS_GUNLOCK();

    if ((code == 0 || afs_get_flock_type(flp) == F_UNLCK) &&
	(cmd == F_SETLK || cmd == F_SETLKW)) {
	afs_clear_flock_sleep(flp);
	code = flock_lock_file_wait(fp, flp);
	if (code && afs_get_flock_type(flp) != F_UNLCK) {
	    struct AFS_FLOCK flock2;
	    flock2 = flock;
	    flock2.l_type = F_UNLCK;
	    AFS_GLOCK();
	    afs_lockctl(vcp, &flock2, F_SETLK, credp);
	    AFS_GUNLOCK();
	}
    }
    /* Convert flock back to Linux's file_lock */
    afs_set_flock_type(flp, flock.l_type);
    afs_set_flock_pid(flp, flock.l_pid);

    crfree(credp);
    return code;
}
#endif

/* afs_linux_flush
 * essentially the same as afs_fsync() but we need to get the return
 * code for the sys_close() here, not afs_linux_release(), so call
 * afs_StoreAllSegments() with AFS_LASTSTORE
 */
static int
#if defined(FOP_FLUSH_TAKES_FL_OWNER_T)
afs_linux_flush(struct file *fp, fl_owner_t id)
#else
afs_linux_flush(struct file *fp)
#endif
{
    struct vrequest *treq = NULL;
    struct vcache *vcp;
    cred_t *credp;
    int code;
    int bypasscache = 0;

    AFS_GLOCK();

    if ((fp->f_flags & O_ACCMODE) == O_RDONLY) { /* readers dont flush */
	AFS_GUNLOCK();
	return 0;
    }

    AFS_DISCON_LOCK();

    credp = crref();
    vcp = VTOAFS(FILE_INODE(fp));

    code = afs_CreateReq(&treq, credp);
    if (code)
	goto out;
    /* If caching is bypassed for this file, or globally, just return 0 */
    if (cache_bypass_strategy == ALWAYS_BYPASS_CACHE)
	bypasscache = 1;
    else {
	ObtainReadLock(&vcp->lock);
	if (vcp->cachingStates & FCSBypass)
	    bypasscache = 1;
	ReleaseReadLock(&vcp->lock);
    }
    if (bypasscache) {
	/* future proof: don't rely on 0 return from afs_InitReq */
	code = 0;
	goto out;
    }

    ObtainSharedLock(&vcp->lock, 535);
    if ((vcp->execsOrWriters > 0) && (file_count(fp) == 1)) {
	UpgradeSToWLock(&vcp->lock, 536);
	if (!AFS_IS_DISCONNECTED) {
		code = afs_StoreAllSegments(vcp,
				treq,
				AFS_SYNC | AFS_LASTSTORE);
	} else {
		afs_DisconAddDirty(vcp, VDisconWriteOsiFlush, 1);
	}
	ConvertWToSLock(&vcp->lock);
    }
    code = afs_CheckCode(code, treq, 54);
    ReleaseSharedLock(&vcp->lock);

out:
    afs_DestroyReq(treq);
    AFS_DISCON_UNLOCK();
    AFS_GUNLOCK();

    crfree(credp);
    return afs_convert_code(code);
}
#if defined(STRUCT_FILE_OPERATIONS_HAS_ITERATE_SHARED) && defined(HAVE_LINUX_WRAP_DIRECTORY_ITERATOR)
# if defined(WRAP_DIR_ITER)
WRAP_DIR_ITER(afs_linux_readdir)	/* Adds necessary locking for iterate_shared */
# else
#  error the Linux provided macro WRAP_DIR_ITER is not available
# endif
#endif

struct file_operations afs_dir_fops = {
  .read =	generic_read_dir,
#if defined(STRUCT_FILE_OPERATIONS_HAS_ITERATE_SHARED) && defined(HAVE_LINUX_WRAP_DIRECTORY_ITERATOR)
  .iterate_shared = shared_afs_linux_readdir,
#elif defined(USE_FOP_ITERATE)
  .iterate =	afs_linux_readdir,
#else
  .readdir =	afs_linux_readdir,
#endif
  .unlocked_ioctl = afs_unlocked_xioctl,
  .compat_ioctl = afs_unlocked_xioctl,
  .open =	afs_linux_open,
  .release =	afs_linux_release,
  .llseek =	default_llseek,
#ifdef HAVE_LINUX_NOOP_FSYNC
  .fsync =	noop_fsync,
#else
  .fsync =	simple_sync_file,
#endif
};

struct file_operations afs_file_fops = {
#ifdef STRUCT_FILE_OPERATIONS_HAS_READ_ITER
  .read_iter =	afs_linux_read_iter,
  .write_iter =	afs_linux_write_iter,
# if !defined(HAVE_LINUX___VFS_WRITE) && !defined(HAVE_LINUX_KERNEL_WRITE)
  .read =	new_sync_read,
  .write =	new_sync_write,
# endif
#elif defined(HAVE_LINUX_GENERIC_FILE_AIO_READ)
  .aio_read =	afs_linux_aio_read,
  .aio_write =	afs_linux_aio_write,
  .read =	do_sync_read,
  .write =	do_sync_write,
#else
  .read =	afs_linux_read,
  .write =	afs_linux_write,
#endif
  .unlocked_ioctl = afs_unlocked_xioctl,
  .compat_ioctl = afs_unlocked_xioctl,
  .mmap =	afs_linux_mmap,
  .open =	afs_linux_open,
  .flush =	afs_linux_flush,
#if defined(STRUCT_FILE_OPERATIONS_HAS_SENDFILE)
  .sendfile =   generic_file_sendfile,
#endif
#if defined(STRUCT_FILE_OPERATIONS_HAS_SPLICE) && !defined(HAVE_LINUX_DEFAULT_FILE_SPLICE_READ)
# if defined(HAVE_LINUX_ITER_FILE_SPLICE_WRITE)
  .splice_write = iter_file_splice_write,
# else
  .splice_write = generic_file_splice_write,
# endif
# if defined(HAVE_LINUX_NO_GENERIC_FILE_SPLICE_READ)
  .splice_read = filemap_splice_read,
# else
  .splice_read = generic_file_splice_read,
# endif
#endif
  .release =	afs_linux_release,
  .fsync =	afs_linux_fsync,
  .lock =	afs_linux_lock,
#ifdef STRUCT_FILE_OPERATIONS_HAS_FLOCK
  .flock =	afs_linux_flock,
#endif
  .llseek = 	default_llseek,
};

static struct dentry *
canonical_dentry(struct inode *ip)
{
    struct vcache *vcp = VTOAFS(ip);
    struct dentry *first = NULL, *ret = NULL, *cur;
#if defined(D_ALIAS_IS_HLIST) && !defined(HLIST_ITERATOR_NO_NODE)
    struct hlist_node *p;
#endif

    /* general strategy:
     * if vcp->target_link is set, and can be found in ip->i_dentry, use that.
     * otherwise, use the first dentry in ip->i_dentry.
     * if ip->i_dentry is empty, use the 'dentry' argument we were given.
     */
    /* note that vcp->target_link specifies which dentry to use, but we have
     * no reference held on that dentry. so, we cannot use or dereference
     * vcp->target_link itself, since it may have been freed. instead, we only
     * use it to compare to pointers in the ip->i_dentry list. */

    d_prune_aliases(ip);

    afs_d_alias_lock(ip);

    afs_d_alias_foreach_reverse(cur, ip, p) {
	if (!vcp->target_link || cur == vcp->target_link) {
	    ret = cur;
	    break;
	}

	if (!first) {
	    first = cur;
	}
    }
    if (!ret && first) {
	ret = first;
    }

    vcp->target_link = ret;

    if (ret) {
	afs_linux_dget(ret);
    }
    afs_d_alias_unlock(ip);

    return ret;
}

/**********************************************************************
 * AFS Linux dentry operations
 **********************************************************************/

/* afs_linux_revalidate
 * Ensure vcache is stat'd before use. Return 0 if entry is valid.
 */
static int
afs_linux_revalidate(struct dentry *dp)
{
    struct vattr *vattr = NULL;
    struct vcache *vcp = VTOAFS(dp->d_inode);
    cred_t *credp;
    int code;

    if (afs_shuttingdown != AFS_RUNNING)
	return EIO;

    AFS_GLOCK();

    code = afs_CreateAttr(&vattr);
    if (code) {
	goto out;
    }

    /* This avoids the crref when we don't have to do it. Watch for
     * changes in afs_getattr that don't get replicated here!
     */
    if (vcp->f.states & CStatd &&
        (!afs_fakestat_enable || vcp->mvstat != AFS_MVSTAT_MTPT) &&
	!afs_nfsexporter &&
	(vType(vcp) == VDIR || vType(vcp) == VLNK)) {
	code = afs_CopyOutAttrs(vcp, vattr);
    } else {
        credp = crref();
	code = afs_getattr(vcp, vattr, credp);
	crfree(credp);
    }

    if (!code)
        afs_fill_inode(AFSTOV(vcp), vattr);

    afs_DestroyAttr(vattr);

out:
    AFS_GUNLOCK();

    return afs_convert_code(code);
}

/* vattr_setattr
 * Set iattr data into vattr. Assume vattr cleared before call.
 */
static void
iattr2vattr(struct vattr *vattrp, struct iattr *iattrp)
{
    vattrp->va_mask = iattrp->ia_valid;
    if (iattrp->ia_valid & ATTR_MODE)
	vattrp->va_mode = iattrp->ia_mode;
    if (iattrp->ia_valid & ATTR_UID)
	vattrp->va_uid = afs_from_kuid(iattrp->ia_uid);
    if (iattrp->ia_valid & ATTR_GID)
	vattrp->va_gid = afs_from_kgid(iattrp->ia_gid);
    if (iattrp->ia_valid & ATTR_SIZE)
	vattrp->va_size = iattrp->ia_size;
    if (iattrp->ia_valid & ATTR_ATIME) {
	vattrp->va_atime.tv_sec = iattrp->ia_atime.tv_sec;
	vattrp->va_atime.tv_nsec = 0;
    }
    if (iattrp->ia_valid & ATTR_MTIME) {
	vattrp->va_mtime.tv_sec = iattrp->ia_mtime.tv_sec;
	vattrp->va_mtime.tv_nsec = 0;
    }
    if (iattrp->ia_valid & ATTR_CTIME) {
	vattrp->va_ctime.tv_sec = iattrp->ia_ctime.tv_sec;
	vattrp->va_ctime.tv_nsec = 0;
    }
}

/* vattr2inode
 * Rewrite the inode cache from the attr. Assumes all vattr fields are valid.
 */
void
vattr2inode(struct inode *ip, struct vattr *vp)
{
    ip->i_ino = vp->va_nodeid;
#ifdef HAVE_LINUX_SET_NLINK
    set_nlink(ip, vp->va_nlink);
#else
    ip->i_nlink = vp->va_nlink;
#endif
    ip->i_blocks = vp->va_blocks;
#ifdef STRUCT_INODE_HAS_I_BLKBITS
    ip->i_blkbits = AFS_BLKBITS;
#endif
#ifdef STRUCT_INODE_HAS_I_BLKSIZE
    ip->i_blksize = vp->va_blocksize;
#endif
    ip->i_rdev = vp->va_rdev;
    ip->i_mode = vp->va_mode;
    ip->i_uid = afs_make_kuid(vp->va_uid);
    ip->i_gid = afs_make_kgid(vp->va_gid);
    i_size_write(ip, vp->va_size);
    afs_inode_set_atime(ip, vp->va_atime.tv_sec, 0);
    /* Set the mtime nanoseconds to the sysname generation number.
     * This convinces NFS clients that all directories have changed
     * any time the sysname list changes.
     */
    afs_inode_set_mtime(ip, vp->va_mtime.tv_sec, afs_sysnamegen);
    afs_inode_set_ctime(ip, vp->va_ctime.tv_sec, 0);
}

/* afs_notify_change
 * Linux version of setattr call. What to change is in the iattr struct.
 * We need to set bits in both the Linux inode as well as the vcache.
 */
#if defined(IOP_TAKES_MNT_IDMAP)
static int
afs_notify_change(struct mnt_idmap *idmap, struct dentry *dp, struct iattr *iattrp)
#elif defined(IOP_TAKES_USER_NAMESPACE)
static int
afs_notify_change(struct user_namespace *mnt_userns, struct dentry *dp, struct iattr *iattrp)
#else
static int
afs_notify_change(struct dentry *dp, struct iattr *iattrp)
#endif
{
    struct vattr *vattr = NULL;
    cred_t *credp = crref();
    struct inode *ip = dp->d_inode;
    int code;

    AFS_GLOCK();
    code = afs_CreateAttr(&vattr);
    if (code) {
	goto out;
    }

    iattr2vattr(vattr, iattrp);	/* Convert for AFS vnodeops call. */

    code = afs_setattr(VTOAFS(ip), vattr, credp);
    if (!code) {
	afs_getattr(VTOAFS(ip), vattr, credp);
	vattr2inode(ip, vattr);
    }
    afs_DestroyAttr(vattr);

out:
    AFS_GUNLOCK();
    crfree(credp);
    return afs_convert_code(code);
}

#if defined(IOP_TAKES_MNT_IDMAP)
static int
afs_linux_getattr(struct mnt_idmap *idmap, const struct path *path, struct kstat *stat,
		  u32 request_mask, unsigned int sync_mode)
{
	int err = afs_linux_revalidate(path->dentry);
	if (!err) {
# if defined(GENERIC_FILLATTR_TAKES_REQUEST_MASK)
		generic_fillattr(afs_mnt_idmap, request_mask, path->dentry->d_inode, stat);
# else
		generic_fillattr(afs_mnt_idmap, path->dentry->d_inode, stat);
# endif
	}
	return err;
}
#elif defined(IOP_TAKES_USER_NAMESPACE)
static int
afs_linux_getattr(struct user_namespace *mnt_userns, const struct path *path, struct kstat *stat,
		  u32 request_mask, unsigned int sync_mode)
{
	int err = afs_linux_revalidate(path->dentry);
	if (!err) {
		generic_fillattr(afs_ns, path->dentry->d_inode, stat);
	}
	return err;
}
#elif defined(IOP_GETATTR_TAKES_PATH_STRUCT)
static int
afs_linux_getattr(const struct path *path, struct kstat *stat, u32 request_mask, unsigned int sync_mode)
{
	int err = afs_linux_revalidate(path->dentry);
	if (!err) {
                generic_fillattr(path->dentry->d_inode, stat);
	}
	return err;
}
#else
static int
afs_linux_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
        int err = afs_linux_revalidate(dentry);
        if (!err) {
                generic_fillattr(dentry->d_inode, stat);
	}
	return err;
}
#endif

static afs_uint32
parent_vcache_dv(struct inode *inode, cred_t *credp)
{
    int free_cred = 0;
    struct vcache *pvcp;

    /*
     * If parent is a mount point and we are using fakestat, we may need
     * to look at the fake vcache entry instead of what the vfs is giving
     * us.  The fake entry is the one with the useful DataVersion.
     */
    pvcp = VTOAFS(inode);
    if (pvcp->mvstat == AFS_MVSTAT_MTPT && afs_fakestat_enable) {
	struct vrequest treq;
	struct afs_fakestat_state fakestate;

	if (!credp) {
	    credp = crref();
	    free_cred = 1;
	}
	afs_InitReq(&treq, credp);
	afs_InitFakeStat(&fakestate);
	afs_TryEvalFakeStat(&pvcp, &fakestate, &treq);
	if (free_cred)
	    crfree(credp);
	afs_PutFakeStat(&fakestate);
    }
    return hgetlo(pvcp->f.m.DataVersion);
}

static inline int
filter_enoent(int code)
{
#ifdef HAVE_LINUX_FATAL_SIGNAL_PENDING
    if (code == ENOENT && fatal_signal_pending(current)) {
        return EINTR;
    }
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
# error fatal_signal_pending not available, but it should be
#endif
    return code;
}

#ifndef D_SPLICE_ALIAS_RACE

static inline void dentry_race_lock(void) {}
static inline void dentry_race_unlock(void) {}

#else

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
static DEFINE_MUTEX(dentry_race_sem);
# else
static DECLARE_MUTEX(dentry_race_sem);
# endif

static inline void
dentry_race_lock(void)
{
    mutex_lock(&dentry_race_sem);
}
static inline void
dentry_race_unlock(void)
{
    mutex_unlock(&dentry_race_sem);
}

/* Leave some trace that this code is enabled; otherwise it's pretty hard to
 * tell. */
static __attribute__((used)) const char dentry_race_marker[] = "d_splice_alias race workaround enabled";

static int
check_dentry_race(struct dentry *dp)
{
    int raced = 0;
    if (!dp->d_inode) {
        /* In Linux, before commit 4919c5e45a91b5db5a41695fe0357fbdff0d5767,
         * d_splice_alias can momentarily hash a dentry before it's fully
         * populated. This only happens for a moment, since it's unhashed again
         * right after (in d_move), but this can make the dentry be found by
         * __d_lookup, and then given to us.
         *
         * So check if the dentry is unhashed; if it is, then the dentry is not
         * valid. We lock dentry_race_lock() to ensure that d_splice_alias is
         * no longer running. Locking d_lock is required to check the dentry's
         * flags, so lock that, too.
         */
        dentry_race_lock();
        spin_lock(&dp->d_lock);
        if (d_unhashed(dp)) {
            raced = 1;
        }
        spin_unlock(&dp->d_lock);
        dentry_race_unlock();
    }
    return raced;
}
#endif /* D_SPLICE_ALIAS_RACE */

/*
 * Validate a dentry. Return 1 if unchanged, 0 if VFS layer should re-evaluate.
 *
 * @param[in] pvcp  vcache for the parent directory containing 'dp'
 * @param[in] name  the name of the directory entry for 'dp'
 * @param[in] dp    the dentry we are checking
 */
static int
dentry_revalidate_common(struct vcache *pvcp, const char *name, struct dentry *dp)
{
    cred_t *credp = NULL;
    struct vcache *vcp, *tvc = NULL;
    int valid;
    struct afs_fakestat_state fakestate;
    int force_drop = 0;
    afs_uint32 parent_dv;
    int code = 0;

#ifdef D_SPLICE_ALIAS_RACE
    if (check_dentry_race(dp)) {
        valid = 0;
        return valid;
    }
#endif

    AFS_GLOCK();
    afs_InitFakeStat(&fakestate);

    if (dp->d_inode) {
	vcp = VTOAFS(dp->d_inode);

	if (vcp == afs_globalVp)
	    goto good_dentry;

	if (vcp->mvstat == AFS_MVSTAT_MTPT) {
	    if (vcp->mvid.target_root && (vcp->f.states & CMValid)) {
		int tryEvalOnly = 0;
		struct vrequest *treq = NULL;

		credp = crref();

		code = afs_CreateReq(&treq, credp);
		if (code) {
		    goto error;
		}
		if ((strcmp(name, ".directory") == 0)) {
		    tryEvalOnly = 1;
		}
		if (tryEvalOnly)
		    code = afs_TryEvalFakeStat(&vcp, &fakestate, treq);
		else
		    code = afs_EvalFakeStat(&vcp, &fakestate, treq);
		afs_DestroyReq(treq);
		if (code != 0) {
		    goto error;
		}
		if (tryEvalOnly && vcp->mvstat == AFS_MVSTAT_MTPT) {
		    /* a mount point, not yet replaced by its directory */
		    goto bad_dentry;
		}
	    }
	} else if (vcp->mvstat == AFS_MVSTAT_ROOT && name[0] != '/') {
	    osi_Assert(vcp->mvid.parent != NULL);
	}

	parent_dv = parent_vcache_dv(AFSTOV(pvcp), credp);

	/* If the parent's DataVersion has changed or the vnode
	 * is longer valid, we need to do a full lookup.  VerifyVCache
	 * isn't enough since the vnode may have been renamed.
	 */

	if (parent_dv > dp->d_time || !(vcp->f.states & CStatd)) {
	    struct vattr *vattr = NULL;

	    if (credp == NULL) {
		credp = crref();
	    }
	    code = afs_lookup(pvcp, (char *)name, &tvc, credp);
            code = filter_enoent(code);
	    if (code == ENOENT) {
		/* ENOENT is not an error here. */
		code = 0;
		osi_Assert(tvc == NULL);
	    }

	    if (code) {
		/* We couldn't perform the lookup, so we don't know if the
		 * dentry is valid or not. */
		goto error;
	    }

	    if (tvc == vcp) {
		/* We got back the same vcache, so we're good. */

	    } else if (tvc == VTOAFS(dp->d_inode)) {
		/* We got back the same vcache, so we're good. This is
		 * different from the above case, because sometimes 'vcp' is
		 * not the same as the vcache for dp->d_inode, if 'vcp' was a
		 * mtpt and we evaluated it to a root dir. In rare cases,
		 * afs_lookup might not evalute the mtpt when we do, or vice
		 * versa, so the previous case will not succeed. But this is
		 * still 'correct', so make sure not to mark the dentry as
		 * invalid; it still points to the same thing! */

	    } else {
		/*
		 * We got back a different file, so we know this dentry is
		 * _not_ okay. Force it to be unhashed, since the given name
		 * doesn't point to this file anymore.
		 */
		force_drop = 1;
		goto bad_dentry;
	    }

	    code = afs_CreateAttr(&vattr);
	    if (code) {
		goto error;
	    }

	    if (afs_getattr(vcp, vattr, credp)) {
		afs_DestroyAttr(vattr);
		code = EIO;
		goto error;
	    }

	    vattr2inode(AFSTOV(vcp), vattr);
	    dp->d_time = parent_dv;

	    afs_DestroyAttr(vattr);
	}

	/* should we always update the attributes at this point? */
	/* unlikely--the vcache entry hasn't changed */

    } else {
	/* 'dp' represents a cached negative lookup. */

	parent_dv = parent_vcache_dv(AFSTOV(pvcp), credp);

	if (parent_dv > dp->d_time || !(pvcp->f.states & CStatd)
	    || afs_IsDynroot(pvcp)) {
	    goto bad_dentry;
	}
    }

  good_dentry:
    code = 0;
    valid = 1;
    goto done;

  bad_dentry:
    code = 0;
    valid = 0;
#ifndef D_INVALIDATE_IS_VOID
    /* When (v3.18) d_invalidate was converted to void, it also started
     * being called automatically from revalidate, and automatically
     * handled:
     *  - shrink_dcache_parent
     *  - automatic detach of submounts
     *  - d_drop
     * Therefore, after that point, OpenAFS revalidate logic no longer needs
     * to do any of those things itself for invalid dentry structs.  We only need
     * to tell VFS it's invalid (by returning 0), and VFS will handle the rest.
     */
    if (have_submounts(dp))
	valid = 1;
#endif

  done:
    /* Clean up */
    if (tvc)
	afs_PutVCache(tvc);
    afs_PutFakeStat(&fakestate);
    AFS_GUNLOCK();
    if (credp)
	crfree(credp);

#ifdef ERRORS_FROM_D_REVALIDATE
    if (code != 0) {
	/*
	 * If code is nonzero, we don't know whether this dentry is valid or
	 * not; we couldn't successfully perform the relevant lookup in order
	 * to tell. So we must not return 'valid' (1) or 'not valid' (0); we
	 * need to return an error (e.g. -EIO).
	 */
	return -code;
    }
#endif

#ifndef D_INVALIDATE_IS_VOID
    if (!valid) {
	/*
	 * If we had a negative lookup for the name we want to forcibly
	 * unhash the dentry.
	 * Otherwise use d_invalidate which will not unhash it if still in use.
	 */
	if (force_drop) {
	    shrink_dcache_parent(dp);
	    d_drop(dp);
	} else
	    d_invalidate(dp);
    }
#endif
    return valid;

 error:
    if (code <= 0) {
	code = EIO;
    }
#ifdef ERRORS_FROM_D_REVALIDATE
    valid = 0;
    goto done;
#else
    /* We can't return an error, so default to saying the dentry is invalid. */
    goto bad_dentry;
#endif
}

#if defined(DOP_REVALIDATE_TAKES_PARENT_INODE)
static int
afs_linux_dentry_revalidate(struct inode *parent_inode, const struct qstr *name,
			    struct dentry *dp, unsigned int flags)
{
    if ((flags & LOOKUP_RCU) != 0) {
	return -ECHILD;
    }
    return dentry_revalidate_common(VTOAFS(parent_inode), name->name, dp);
}
#else
# if defined(DOP_REVALIDATE_TAKES_UNSIGNED)
static int
afs_linux_dentry_revalidate(struct dentry *dp, unsigned int flags)
# elif defined(DOP_REVALIDATE_TAKES_NAMEIDATA)
static int
afs_linux_dentry_revalidate(struct dentry *dp, struct nameidata *nd)
# else
static int
afs_linux_dentry_revalidate(struct dentry *dp, int flags)
# endif
{
    int code;
    struct dentry *parent;

# ifdef LOOKUP_RCU
    /* We don't support RCU path walking */
#  if defined(DOP_REVALIDATE_TAKES_UNSIGNED)
    if ((flags & LOOKUP_RCU) != 0) {
       return -ECHILD;
    }
#  else
    if ((nd->flags & LOOKUP_RCU) != 0) {
       return -ECHILD;
    }
#  endif
# endif

    parent = dget_parent(dp);
    code = dentry_revalidate_common(VTOAFS(parent->d_inode),
				    dp->d_name.name, dp);
    dput(parent);

    return code;
}
#endif /* DOP_REVALIDATE_TAKES_PARENT_INODE */

static void
afs_dentry_iput(struct dentry *dp, struct inode *ip)
{
    struct vcache *vcp = VTOAFS(ip);
    int haveGlock = ISAFS_GLOCK();

    if (!haveGlock) {
        AFS_GLOCK();
    }

    if (!AFS_IS_DISCONNECTED || (vcp->f.states & CUnlinked)) {
	(void) afs_InactiveVCache(vcp, NULL);
    }

    if (!haveGlock) {
        AFS_GUNLOCK();
    }

    afs_linux_clear_nfsfs_renamed(dp);

    iput(ip);
}

static int
#if defined(DOP_D_DELETE_TAKES_CONST)
afs_dentry_delete(const struct dentry *dp)
#else
afs_dentry_delete(struct dentry *dp)
#endif
{
    if (dp->d_inode && (VTOAFS(dp->d_inode)->f.states & CUnlinked))
	return 1;		/* bad inode? */

    return 0;
}

#ifdef STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT
static struct vfsmount *
afs_dentry_automount(afs_linux_path_t *path)
{
    struct dentry *target;

    /*
     * Avoid symlink resolution limits when resolving; we cannot contribute to
     * an infinite symlink loop.
     *
     * On newer kernels the field has moved to the private nameidata structure
     * so we can't adjust it here.  This may cause ELOOP when using a path with
     * 40 or more directories that are not already in the dentry cache.
     */
#if defined(STRUCT_TASK_STRUCT_HAS_TOTAL_LINK_COUNT)
    current->total_link_count--;
#endif

    target = canonical_dentry(path->dentry->d_inode);

    if (target == path->dentry) {
	dput(target);
	target = NULL;
    }

    if (target) {
	dput(path->dentry);
	path->dentry = target;

    } else {
	spin_lock(&path->dentry->d_lock);
	path->dentry->d_flags &= ~DCACHE_NEED_AUTOMOUNT;
	spin_unlock(&path->dentry->d_lock);
    }

    return NULL;
}
#endif /* STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT */

struct dentry_operations afs_dentry_operations = {
  .d_revalidate =	afs_linux_dentry_revalidate,
  .d_delete =		afs_dentry_delete,
  .d_iput =		afs_dentry_iput,
#ifdef STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT
  .d_automount =        afs_dentry_automount,
#endif /* STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT */
};

/**********************************************************************
 * AFS Linux inode operations
 **********************************************************************/

/* afs_linux_create
 *
 * Merely need to set enough of vattr to get us through the create. Note
 * that the higher level code (open_namei) will take care of any tuncation
 * explicitly. Exclusive open is also taken care of in open_namei.
 *
 * name is in kernel space at this point.
 */

#if defined(IOP_TAKES_MNT_IDMAP)
static int
afs_linux_create(struct mnt_idmap *idmap, struct inode *dip,
		 struct dentry *dp, umode_t mode, bool excl)
#elif defined(IOP_TAKES_USER_NAMESPACE)
static int
afs_linux_create(struct user_namespace *mnt_userns, struct inode *dip,
		 struct dentry *dp, umode_t mode, bool excl)
#elif defined(IOP_CREATE_TAKES_BOOL)
static int
afs_linux_create(struct inode *dip, struct dentry *dp, umode_t mode,
		 bool excl)
#elif defined(IOP_CREATE_TAKES_UMODE_T)
static int
afs_linux_create(struct inode *dip, struct dentry *dp, umode_t mode,
		 struct nameidata *nd)
#elif defined(IOP_CREATE_TAKES_NAMEIDATA)
static int
afs_linux_create(struct inode *dip, struct dentry *dp, int mode,
		 struct nameidata *nd)
#else
static int
afs_linux_create(struct inode *dip, struct dentry *dp, int mode)
#endif
{
    struct vattr *vattr = NULL;
    cred_t *credp = crref();
    const char *name = dp->d_name.name;
    struct vcache *vcp;
    int code;

    AFS_GLOCK();

    code = afs_CreateAttr(&vattr);
    if (code) {
	goto out;
    }
    vattr->va_mode = mode;
    vattr->va_type = mode & S_IFMT;

    code = afs_create(VTOAFS(dip), (char *)name, vattr, NONEXCL, mode,
		      &vcp, credp);

    if (!code) {
	struct inode *ip = AFSTOV(vcp);

	afs_getattr(vcp, vattr, credp);
	afs_fill_inode(ip, vattr);
	insert_inode_hash(ip);
#if !defined(STRUCT_SUPER_BLOCK_HAS_S_D_OP)
	dp->d_op = &afs_dentry_operations;
#endif
	dp->d_time = parent_vcache_dv(dip, credp);
	d_instantiate(dp, ip);
    }

    afs_DestroyAttr(vattr);

out:
    AFS_GUNLOCK();

    crfree(credp);
    return afs_convert_code(code);
}

/* afs_linux_lookup */
static struct dentry *
#if defined(IOP_LOOKUP_TAKES_UNSIGNED)
afs_linux_lookup(struct inode *dip, struct dentry *dp,
		 unsigned flags)
#elif defined(IOP_LOOKUP_TAKES_NAMEIDATA)
afs_linux_lookup(struct inode *dip, struct dentry *dp,
		 struct nameidata *nd)
#else
afs_linux_lookup(struct inode *dip, struct dentry *dp)
#endif
{
    cred_t *credp = crref();
    struct vcache *vcp = NULL;
    const char *comp = dp->d_name.name;
    struct inode *ip = NULL;
    struct dentry *newdp = NULL;
    int code;

    AFS_GLOCK();

    code = afs_lookup(VTOAFS(dip), (char *)comp, &vcp, credp);
    code = filter_enoent(code);
    if (code == ENOENT) {
        /* It's ok for the file to not be found. That's noted by the caller by
         * seeing that the dp->d_inode field is NULL (set by d_splice_alias or
         * d_add, below). */
        code = 0;
        osi_Assert(vcp == NULL);
    }
    if (code) {
        AFS_GUNLOCK();
        goto done;
    }

    if (vcp) {
	struct vattr *vattr = NULL;
	struct vcache *parent_vc = VTOAFS(dip);

	if (parent_vc == vcp) {
	    /* This is possible if the parent dir is a mountpoint to a volume,
	     * and the dir entry we looked up is a mountpoint to the same
	     * volume. Linux cannot cope with this, so return an error instead
	     * of risking a deadlock or panic. */
	    afs_PutVCache(vcp);
	    code = EDEADLK;
	    AFS_GUNLOCK();
	    goto done;
	}

	code = afs_CreateAttr(&vattr);
	if (code) {
	    afs_PutVCache(vcp);
	    AFS_GUNLOCK();
	    goto done;
	}

	ip = AFSTOV(vcp);
	afs_getattr(vcp, vattr, credp);
	afs_fill_inode(ip, vattr);
	if (hlist_unhashed(&ip->i_hash))
	    insert_inode_hash(ip);

	afs_DestroyAttr(vattr);
    }
#if !defined(STRUCT_SUPER_BLOCK_HAS_S_D_OP)
    dp->d_op = &afs_dentry_operations;
#endif
    dp->d_time = parent_vcache_dv(dip, credp);

    AFS_GUNLOCK();

    if (ip && S_ISDIR(ip->i_mode)) {
	d_prune_aliases(ip);

#ifdef STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT
	/* Only needed if this is a volume root */
	if (vcp->mvstat == 2)
	    ip->i_flags |= S_AUTOMOUNT;
#endif
    }
    /*
     * Take an extra reference so the inode doesn't go away if
     * d_splice_alias drops our reference on error.
     */
    if (ip)
#ifdef HAVE_LINUX_IHOLD
	ihold(ip);
#else
	igrab(ip);
#endif

    dentry_race_lock();
    newdp = d_splice_alias(ip, dp);
    dentry_race_unlock();

 done:
    crfree(credp);

    if (IS_ERR(newdp)) {
        /* d_splice_alias can return an error (EIO) if there is an existing
         * connected directory alias for this dentry. Add our dentry manually
         * ourselves if this happens. */
        d_add(dp, ip);

#if defined(D_SPLICE_ALIAS_LEAK_ON_ERROR)
        /* Depending on the kernel version, d_splice_alias may or may not drop
         * the inode reference on error. If it didn't, do it here. */
        iput(ip);
#endif
        return NULL;
    }

    if (code) {
	if (ip)
	    iput(ip);
	return ERR_PTR(afs_convert_code(code));
    }

    iput(ip);
    return newdp;
}

static int
afs_linux_link(struct dentry *olddp, struct inode *dip, struct dentry *newdp)
{
    int code;
    cred_t *credp = crref();
    const char *name = newdp->d_name.name;
    struct inode *oldip = olddp->d_inode;

    /* If afs_link returned the vnode, we could instantiate the
     * dentry. Since it's not, we drop this one and do a new lookup.
     */
    d_drop(newdp);

    AFS_GLOCK();
    code = afs_link(VTOAFS(oldip), VTOAFS(dip), (char *)name, credp);

    AFS_GUNLOCK();
    crfree(credp);
    return afs_convert_code(code);
}

/* We have to have a Linux specific sillyrename function, because we
 * also have to keep the dcache up to date when we're doing a silly
 * rename - so we don't want the generic vnodeops doing this behind our
 * back.
 */

static int
afs_linux_sillyrename(struct inode *dir, struct dentry *dentry,
		      cred_t *credp)
{
    struct vcache *tvc = VTOAFS(dentry->d_inode);
    struct dentry *__dp = NULL;
    char *__name = NULL;
    int code;

    if (afs_linux_nfsfs_renamed(dentry))
	return EBUSY;

    do {
	dput(__dp);

	AFS_GLOCK();
	if (__name)
	    osi_FreeSmallSpace(__name);
	__name = afs_newname();
	AFS_GUNLOCK();

	__dp = lookup_one_len(__name, dentry->d_parent, strlen(__name));

	if (IS_ERR(__dp)) {
	    osi_FreeSmallSpace(__name);
	    return EBUSY;
	}
    } while (__dp->d_inode != NULL);

    AFS_GLOCK();
    code = afs_rename(VTOAFS(dir), (char *)dentry->d_name.name,
		      VTOAFS(dir), (char *)__dp->d_name.name,
		      credp);
    if (!code) {
	tvc->mvid.silly_name = __name;
	crhold(credp);
	if (tvc->uncred) {
	    crfree(tvc->uncred);
	}
	tvc->uncred = credp;
	tvc->f.states |= CUnlinked;
	afs_linux_set_nfsfs_renamed(dentry);

	__dp->d_time = 0;		/* force to revalidate */
	d_move(dentry, __dp);
    } else {
	osi_FreeSmallSpace(__name);
    }
    AFS_GUNLOCK();

    dput(__dp);

    return code;
}


static int
afs_linux_unlink(struct inode *dip, struct dentry *dp)
{
    int code = EBUSY;
    cred_t *credp = crref();
    const char *name = dp->d_name.name;
    struct vcache *tvc = VTOAFS(dp->d_inode);

    if (VREFCOUNT(tvc) > 1 && tvc->opens > 0
				&& !(tvc->f.states & CUnlinked)) {

	code = afs_linux_sillyrename(dip, dp, credp);
    } else {
	AFS_GLOCK();
	code = afs_remove(VTOAFS(dip), (char *)name, credp);
	AFS_GUNLOCK();
	if (!code)
	    d_drop(dp);
    }

    crfree(credp);
    return afs_convert_code(code);
}


#if defined(IOP_TAKES_MNT_IDMAP)
static int
afs_linux_symlink(struct mnt_idmap *idmap, struct inode *dip,
		  struct dentry *dp, const char *target)
#elif defined(IOP_TAKES_USER_NAMESPACE)
static int
afs_linux_symlink(struct user_namespace *mnt_userns, struct inode *dip,
		  struct dentry *dp, const char *target)
#else
static int
afs_linux_symlink(struct inode *dip, struct dentry *dp, const char *target)
#endif
{
    int code;
    cred_t *credp = crref();
    struct vattr *vattr = NULL;
    const char *name = dp->d_name.name;

    /* If afs_symlink returned the vnode, we could instantiate the
     * dentry. Since it's not, we drop this one and do a new lookup.
     */
    d_drop(dp);

    AFS_GLOCK();
    code = afs_CreateAttr(&vattr);
    if (code) {
	goto out;
    }

    code = afs_symlink(VTOAFS(dip), (char *)name, vattr, (char *)target, NULL,
			credp);
    afs_DestroyAttr(vattr);

out:
    AFS_GUNLOCK();
    crfree(credp);
    return afs_convert_code(code);
}

#if defined(IOP_TAKES_MNT_IDMAP)
static int
afs_linux_mkdir(struct mnt_idmap *idmap, struct inode *dip,
		struct dentry *dp, umode_t mode)
#elif defined(IOP_TAKES_USER_NAMESPACE)
static int
afs_linux_mkdir(struct user_namespace *mnt_userns, struct inode *dip,
		struct dentry *dp, umode_t mode)
#elif defined(IOP_MKDIR_TAKES_UMODE_T)
static int
afs_linux_mkdir(struct inode *dip, struct dentry *dp, umode_t mode)
#else
static int
afs_linux_mkdir(struct inode *dip, struct dentry *dp, int mode)
#endif
{
    int code;
    cred_t *credp = crref();
    struct vcache *tvcp = NULL;
    struct vattr *vattr = NULL;
    const char *name = dp->d_name.name;

    AFS_GLOCK();
    code = afs_CreateAttr(&vattr);
    if (code) {
	goto out;
    }

    vattr->va_mask = ATTR_MODE;
    vattr->va_mode = mode;

    code = afs_mkdir(VTOAFS(dip), (char *)name, vattr, &tvcp, credp);

    if (tvcp) {
	struct inode *ip = AFSTOV(tvcp);

	afs_getattr(tvcp, vattr, credp);
	afs_fill_inode(ip, vattr);

#if !defined(STRUCT_SUPER_BLOCK_HAS_S_D_OP)
	dp->d_op = &afs_dentry_operations;
#endif
	dp->d_time = parent_vcache_dv(dip, credp);
	d_instantiate(dp, ip);
    }
    afs_DestroyAttr(vattr);

out:
    AFS_GUNLOCK();

    crfree(credp);
    return afs_convert_code(code);
}

static int
afs_linux_rmdir(struct inode *dip, struct dentry *dp)
{
    int code;
    cred_t *credp = crref();
    const char *name = dp->d_name.name;

    /* locking kernel conflicts with glock? */

    AFS_GLOCK();
    code = afs_rmdir(VTOAFS(dip), (char *)name, credp);
    AFS_GUNLOCK();

    /* Linux likes to see ENOTEMPTY returned from an rmdir() syscall
     * that failed because a directory is not empty. So, we map
     * EEXIST to ENOTEMPTY on linux.
     */
    if (code == EEXIST) {
	code = ENOTEMPTY;
    }

    if (!code) {
	d_drop(dp);
    }

    crfree(credp);
    return afs_convert_code(code);
}


#if defined(IOP_TAKES_MNT_IDMAP)
static int
afs_linux_rename(struct mnt_idmap *idmap,
		 struct inode *oldip, struct dentry *olddp,
		 struct inode *newip, struct dentry *newdp,
		 unsigned int flags)
#elif defined(IOP_TAKES_USER_NAMESPACE)
static int
afs_linux_rename(struct user_namespace *mnt_userns,
		 struct inode *oldip, struct dentry *olddp,
		 struct inode *newip, struct dentry *newdp,
		 unsigned int flags)
#elif defined(HAVE_LINUX_INODE_OPERATIONS_RENAME_TAKES_FLAGS)
static int
afs_linux_rename(struct inode *oldip, struct dentry *olddp,
		 struct inode *newip, struct dentry *newdp,
		 unsigned int flags)
#else
static int
afs_linux_rename(struct inode *oldip, struct dentry *olddp,
		 struct inode *newip, struct dentry *newdp)
#endif
{
    int code;
    cred_t *credp = crref();
    const char *oldname = olddp->d_name.name;
    const char *newname = newdp->d_name.name;
    struct dentry *rehash = NULL;

#if defined(HAVE_LINUX_INODE_OPERATIONS_RENAME_TAKES_FLAGS) || \
    defined(IOP_TAKES_MNT_IDMAP) || defined(IOP_TAKES_USER_NAMESPACE)
    if (flags)
	return -EINVAL;		/* no support for new flags yet */
#endif

    /* Prevent any new references during rename operation. */

    if (!d_unhashed(newdp)) {
	d_drop(newdp);
	rehash = newdp;
    }

    afs_maybe_shrink_dcache(olddp);

    AFS_GLOCK();
    code = afs_rename(VTOAFS(oldip), (char *)oldname, VTOAFS(newip), (char *)newname, credp);
    AFS_GUNLOCK();

    if (!code)
	olddp->d_time = 0;      /* force to revalidate */

    if (rehash)
	d_rehash(rehash);

    crfree(credp);
    return afs_convert_code(code);
}


/* afs_linux_ireadlink
 * Internal readlink which can return link contents to user or kernel space.
 * Note that the buffer is NOT supposed to be null-terminated.
 */
static int
afs_linux_ireadlink(struct inode *ip, char *target, int maxlen, uio_seg_t seg)
{
    int code;
    cred_t *credp = crref();
    struct uio tuio;
    struct iovec iov;

    memset(&tuio, 0, sizeof(tuio));
    memset(&iov, 0, sizeof(iov));

    setup_uio(&tuio, &iov, target, (afs_offs_t) 0, maxlen, UIO_READ, seg);
    code = afs_readlink(VTOAFS(ip), &tuio, credp);
    crfree(credp);

    if (!code)
	return maxlen - tuio.uio_resid;
    else
	return afs_convert_code(code);
}

#if !defined(USABLE_KERNEL_PAGE_SYMLINK_CACHE)
/* afs_linux_readlink
 * Fill target (which is in user space) with contents of symlink.
 */
static int
afs_linux_readlink(struct dentry *dp, char *target, int maxlen)
{
    int code;
    struct inode *ip = dp->d_inode;

    AFS_GLOCK();
    code = afs_linux_ireadlink(ip, target, maxlen, AFS_UIOUSER);
    AFS_GUNLOCK();
    return code;
}


/* afs_linux_follow_link
 * a file system dependent link following routine.
 */
#if defined(HAVE_LINUX_INODE_OPERATIONS_FOLLOW_LINK_NO_NAMEIDATA)
static const char *afs_linux_follow_link(struct dentry *dentry, void **link_data)
#else
static int afs_linux_follow_link(struct dentry *dentry, struct nameidata *nd)
#endif
{
    int code;
    char *name;

    name = kmalloc(PATH_MAX, GFP_NOFS);
    if (!name) {
#if defined(HAVE_LINUX_INODE_OPERATIONS_FOLLOW_LINK_NO_NAMEIDATA)
	return ERR_PTR(-EIO);
#else
	return -EIO;
#endif
    }

    AFS_GLOCK();
    code = afs_linux_ireadlink(dentry->d_inode, name, PATH_MAX - 1, AFS_UIOSYS);
    AFS_GUNLOCK();

    if (code < 0) {
#if defined(HAVE_LINUX_INODE_OPERATIONS_FOLLOW_LINK_NO_NAMEIDATA)
	return ERR_PTR(code);
#else
	return code;
#endif
    }

    name[code] = '\0';
#if defined(HAVE_LINUX_INODE_OPERATIONS_FOLLOW_LINK_NO_NAMEIDATA)
    return *link_data = name;
#else
    nd_set_link(nd, name);
    return 0;
#endif
}

#if defined(HAVE_LINUX_INODE_OPERATIONS_PUT_LINK_NO_NAMEIDATA)
static void
afs_linux_put_link(struct inode *inode, void *link_data)
{
    char *name = link_data;

    if (name && !IS_ERR(name))
	kfree(name);
}
#else
static void
afs_linux_put_link(struct dentry *dentry, struct nameidata *nd)
{
    char *name = nd_get_link(nd);

    if (name && !IS_ERR(name))
	kfree(name);
}
#endif /* HAVE_LINUX_INODE_OPERATIONS_PUT_LINK_NO_NAMEIDATA */

#endif /* USABLE_KERNEL_PAGE_SYMLINK_CACHE */

/*
 * Call the mapping function that reads data for a given page.
 * Note: When we return, it is expected that the page is unlocked.  It is the
 * responsibility of the called function (e.g. ->readpage) to unlock the given
 * page, even when an error occurs.
 */
static int
mapping_read_page(struct address_space *mapping, struct page *page)
{
#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READ_FOLIO)
    return mapping->a_ops->read_folio(NULL, page_folio(page));
#else
    return mapping->a_ops->readpage(NULL, page);
#endif
}

/*
 * small compat wrapper for filemap_alloc_folio/page_cache_alloc
 */
static struct page *
afs_page_cache_alloc(struct address_space *cachemapping)
{
#if defined(HAVE_LINUX_FILEMAP_ALLOC_FOLIO)
    struct folio *folio;
    folio = filemap_alloc_folio(mapping_gfp_mask(cachemapping), 0);
    if (folio == NULL) {
	return NULL;
    }
    return &folio->page;
#else
    return page_cache_alloc(cachemapping);
#endif
}

/* Populate a page by filling it from the cache file pointed at by cachefp
 * (which contains indicated chunk)
 * If task is NULL, the page copy occurs syncronously, and the routine
 * returns with page still locked. If task is non-NULL, then page copies
 * may occur in the background, and the page will be unlocked when it is
 * ready for use. Note that if task is non-NULL and we encounter an error
 * before we start the background copy, we MUST unlock 'page' before we return.
 */
static int
afs_linux_read_cache(struct file *cachefp, struct page *page,
		     int chunk, struct afs_lru_pages *alrupages,
		     struct afs_pagecopy_task *task) {
    loff_t offset = page_offset(page);
    struct inode *cacheinode = cachefp->f_dentry->d_inode;
    struct page *newpage, *cachepage;
    struct address_space *cachemapping;
    int pageindex;
    int code = 0;

    cachemapping = cacheinode->i_mapping;
    newpage = NULL;
    cachepage = NULL;

    /* If we're trying to read a page that's past the end of the disk
     * cache file, then just return a zeroed page */
    if (AFS_CHUNKOFFSET(offset) >= i_size_read(cacheinode)) {
	zero_user_segment(page, 0, PAGE_SIZE);
	SetPageUptodate(page);
	if (task)
	    unlock_page(page);
	return 0;
    }

    /* From our offset, we now need to work out which page in the disk
     * file it corresponds to. This will be fun ... */
    pageindex = (offset - AFS_CHUNKTOBASE(chunk)) >> PAGE_SHIFT;

    while (cachepage == NULL) {
	cachepage = find_get_page(cachemapping, pageindex);
	if (!cachepage) {
	    if (newpage == NULL) {
		newpage = afs_page_cache_alloc(cachemapping);
	    }
	    if (newpage == NULL) {
		code = -ENOMEM;
		goto out;
	    }

	    code = afs_add_to_page_cache_lru(alrupages, newpage, cachemapping,
					     pageindex, GFP_KERNEL);
	    if (code == 0) {
	        cachepage = newpage;
	        newpage = NULL;
	    } else {
		put_page(newpage);
		newpage = NULL;
		if (code != -EEXIST)
		    goto out;
	    }
        } else {
	    lock_page(cachepage);
	}
    }

    if (!PageUptodate(cachepage)) {
	ClearPageError(cachepage);
	/* Note that mapping_read_page always handles unlocking the given page,
	 * even when an error is returned. */
	code = mapping_read_page(cachemapping, cachepage);
	if (!code && !task) {
	    wait_on_page_locked(cachepage);
	}
    } else {
        unlock_page(cachepage);
    }

    if (!code) {
	if (PageUptodate(cachepage)) {
	    copy_highpage(page, cachepage);
	    flush_dcache_page(page);
	    SetPageUptodate(page);

	    if (task)
		unlock_page(page);
        } else if (task) {
	    afs_pagecopy_queue_page(task, cachepage, page);
	} else {
	    code = -EIO;
	}
    }

 out:
    if (code && task) {
        unlock_page(page);
    }

    if (cachepage)
	put_page(cachepage);

    return code;
}

/*
 * Return true if the file has a mapping that can read pages
 */
static inline int
file_can_read_pages(struct file *fp)
{
#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READ_FOLIO)
    if (fp->f_dentry->d_inode->i_mapping->a_ops->read_folio != NULL)
	return 1;
#else
    if (fp->f_dentry->d_inode->i_mapping->a_ops->readpage != NULL)
	return 1;
#endif
    return 0;
}

static inline int
afs_linux_readpage_fastpath(struct file *fp, struct page *pp, int *codep)
{
    loff_t offset = page_offset(pp);
    struct inode *ip = FILE_INODE(fp);
    struct vcache *avc = VTOAFS(ip);
    struct dcache *tdc;
    struct file *cacheFp = NULL;
    int code;
    int dcLocked = 0;
    struct afs_lru_pages lrupages;

    /* Not a UFS cache, don't do anything */
    if (cacheDiskType != AFS_FCACHE_TYPE_UFS)
	return 0;

    /* No readpage (ex: tmpfs) , skip */
    if (cachefs_noreadpage)
	return 0;

    /* Can't do anything if the vcache isn't statd , or if the read
     * crosses a chunk boundary.
     */
    if (!(avc->f.states & CStatd) ||
        AFS_CHUNK(offset) != AFS_CHUNK(offset + PAGE_SIZE)) {
	return 0;
    }

    ObtainWriteLock(&avc->lock, 911);

    /* XXX - See if hinting actually makes things faster !!! */

    /* See if we have a suitable entry already cached */
    tdc = avc->dchint;

    if (tdc) {
        /* We need to lock xdcache, then dcache, to handle situations where
         * the hint is on the free list. However, we can't safely do this
         * according to the locking hierarchy. So, use a non blocking lock.
         */
	ObtainReadLock(&afs_xdcache);
	dcLocked = ( 0 == NBObtainReadLock(&tdc->lock));

	if (dcLocked && (tdc->index != NULLIDX)
	    && !FidCmp(&tdc->f.fid, &avc->f.fid)
	    && tdc->f.chunk == AFS_CHUNK(offset)
	    && !(afs_indexFlags[tdc->index] & (IFFree | IFDiscarded))) {
	    /* Bonus - the hint was correct */
	    afs_RefDCache(tdc);
	} else {
	    /* Only destroy the hint if its actually invalid, not if there's
	     * just been a locking failure */
	    if (dcLocked) {
		ReleaseReadLock(&tdc->lock);
		avc->dchint = NULL;
	    }

	    tdc = NULL;
	    dcLocked = 0;
	}
        ReleaseReadLock(&afs_xdcache);
    }

    /* No hint, or hint is no longer valid - see if we can get something
     * directly from the dcache
     */
    if (!tdc)
	tdc = afs_FindDCache(avc, offset);

    if (!tdc) {
	ReleaseWriteLock(&avc->lock);
	return 0;
    }

    if (!dcLocked)
	ObtainReadLock(&tdc->lock);

    /* Is the dcache we've been given currently up to date */
    if (!afs_IsDCacheFresh(tdc, avc) ||
	(tdc->dflags & DFFetching))
	goto out;

    /* Update our hint for future abuse */
    avc->dchint = tdc;

    /* Okay, so we've now got a cache file that is up to date */

    /* XXX - I suspect we should be locking the inodes before we use them! */
    AFS_GUNLOCK();
    cacheFp = afs_linux_raw_open(&tdc->f.inode);
    if (cacheFp == NULL) {
	/* Problem getting the inode */
	AFS_GLOCK();
	goto out;
    }

    if (!file_can_read_pages(cacheFp)) {
	cachefs_noreadpage = 1;
	AFS_GLOCK();
	goto out;
    }

    afs_lru_cache_init(&lrupages);

    code = afs_linux_read_cache(cacheFp, pp, tdc->f.chunk, &lrupages, NULL);

    afs_lru_cache_finalize(&lrupages);

    filp_close(cacheFp, NULL);
    AFS_GLOCK();

    ReleaseReadLock(&tdc->lock);
    ReleaseWriteLock(&avc->lock);
    afs_PutDCache(tdc);

    *codep = code;
    return 1;

out:
    if (cacheFp != NULL) {
	filp_close(cacheFp, NULL);
    }
    ReleaseWriteLock(&avc->lock);
    ReleaseReadLock(&tdc->lock);
    afs_PutDCache(tdc);
    return 0;
}

/* afs_linux_readpage
 *
 * This function is split into two, because prepare_write/begin_write
 * require a readpage call which doesn't unlock the resulting page upon
 * success.
 */
static int
afs_linux_fillpage(struct file *fp, struct page *pp)
{
    afs_int32 code;
    char *address;
    struct uio *auio;
    struct iovec *iovecp;
    struct inode *ip = FILE_INODE(fp);
    afs_int32 cnt = page_count(pp);
    struct vcache *avc = VTOAFS(ip);
    afs_offs_t offset = page_offset(pp);
    cred_t *credp;

    AFS_GLOCK();
    if (afs_linux_readpage_fastpath(fp, pp, &code)) {
	AFS_GUNLOCK();
	return code;
    }
    AFS_GUNLOCK();

    credp = crref();
    address = kmap(pp);
    ClearPageError(pp);

    auio = kmalloc(sizeof(struct uio), GFP_NOFS);
    iovecp = kmalloc(sizeof(struct iovec), GFP_NOFS);

    setup_uio(auio, iovecp, (char *)address, offset, PAGE_SIZE, UIO_READ,
              AFS_UIOSYS);

    AFS_GLOCK();
    AFS_DISCON_LOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_READPAGE, ICL_TYPE_POINTER, ip,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, cnt, ICL_TYPE_INT32,
	       99999);	/* not a possible code value */

    code = afs_rdwr(avc, auio, UIO_READ, 0, credp);

    afs_Trace4(afs_iclSetp, CM_TRACE_READPAGE, ICL_TYPE_POINTER, ip,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, cnt, ICL_TYPE_INT32,
	       code);
    AFS_DISCON_UNLOCK();
    AFS_GUNLOCK();
    if (!code) {
	/* XXX valid for no-cache also?  Check last bits of files... :)
	 * Cognate code goes in afs_NoCacheFetchProc.  */
	if (auio->uio_resid)	/* zero remainder of page */
	     memset((void *)(address + (PAGE_SIZE - auio->uio_resid)), 0,
		    auio->uio_resid);

	flush_dcache_page(pp);
	SetPageUptodate(pp);
    } /* !code */

    kunmap(pp);

    kfree(auio);
    kfree(iovecp);

    crfree(credp);
    return afs_convert_code(code);
}

static int
afs_linux_prefetch(struct file *fp, struct page *pp)
{
    int code = 0;
    struct vcache *avc = VTOAFS(FILE_INODE(fp));
    afs_offs_t offset = page_offset(pp);

    if (AFS_CHUNKOFFSET(offset) == 0) {
	struct dcache *tdc;
	struct vrequest *treq = NULL;
	cred_t *credp;

	credp = crref();
	AFS_GLOCK();
	code = afs_CreateReq(&treq, credp);
	if (!code && !NBObtainWriteLock(&avc->lock, 534)) {
	    tdc = afs_FindDCache(avc, offset);
	    if (tdc) {
		if (!(tdc->mflags & DFNextStarted))
		    afs_PrefetchChunk(avc, tdc, credp, treq);
		afs_PutDCache(tdc);
	    }
	    ReleaseWriteLock(&avc->lock);
	}
	afs_DestroyReq(treq);
	AFS_GUNLOCK();
	crfree(credp);
    }
    return afs_convert_code(code);

}

#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READAHEAD)
/*
 * Bypass the cache while performing a readahead.
 * See the comments for afs_linux_readahead for the semantics
 * for 'rac'.
 */
static void
afs_linux_bypass_readahead(struct readahead_control *rac)
{
    struct file *fp = rac->file;
    unsigned num_pages = readahead_count(rac);
    afs_int32 page_ix;
    afs_offs_t offset;
    struct iovec* iovecp;
    struct nocache_read_request *ancr;
    struct page *pp;
    afs_int32 code = 0;

    cred_t *credp;
    struct inode *ip = FILE_INODE(fp);
    struct vcache *avc = VTOAFS(ip);
    afs_int32 base_index = 0;
    afs_int32 page_count = 0;
    afs_int32 isize;

    ancr = afs_alloc_ncr(num_pages);
    if (ancr == NULL)
	goto done;

    iovecp = ancr->auio->uio_iov;

    for (page_ix = 0; page_ix < num_pages; ++page_ix) {
	pp = readahead_page(rac);
	if (pp == NULL)
	    break;

	isize = (i_size_read(fp->f_mapping->host) - 1) >> PAGE_SHIFT;
	if (pp->index > isize) {
	    if (PageLocked(pp))
		unlock_page(pp);
	    put_page(pp);
	    continue;
	}

	if (page_ix == 0) {
	    offset = page_offset(pp);
	    ancr->offset = ancr->auio->uio_offset = offset;
	    base_index = pp->index;
	}
	iovecp[page_ix].iov_len = PAGE_SIZE;
	if (base_index != pp->index) {
	    if (PageLocked(pp))
		 unlock_page(pp);
	    put_page(pp);
	    iovecp[page_ix].iov_base = NULL;
	    base_index++;
	    ancr->length -= PAGE_SIZE;
	    continue;
	}
	base_index++;
	page_count++;
	/* save the page for background map */
	iovecp[page_ix].iov_base = pp;
    }

    /* If there were useful pages in the page list, schedule
     * the read */
    if (page_count > 0) {
	credp = crref();
	/* The background thread frees the ancr */
	code = afs_ReadNoCache(avc, ancr, credp);
	crfree(credp);
    } else {
	/* If there is nothing for the background thread to handle,
	 * it won't be freeing the things that we never gave it */
	afs_free_ncr(&ancr);
    }
    /* we do not flush, release, or unmap pages--that will be
     * done for us by the background thread as each page comes in
     * from the fileserver */

 done:
    /* The vfs layer will unlock/put any of the pages in the rac that were not
     * processed */
    return;
}
#else /* STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READAHEAD */
static int
afs_linux_bypass_readpages(struct file *fp, struct address_space *mapping,
			   struct list_head *page_list, unsigned num_pages)
{
    afs_int32 page_ix;
    afs_offs_t offset;
    struct iovec* iovecp;
    struct nocache_read_request *ancr;
    struct page *pp;
    struct afs_lru_pages lrupages;
    afs_int32 code = 0;

    cred_t *credp;
    struct inode *ip = FILE_INODE(fp);
    struct vcache *avc = VTOAFS(ip);
    afs_int32 base_index = 0;
    afs_int32 page_count = 0;
    afs_int32 isize;

    ancr = afs_alloc_ncr(num_pages);
    if (ancr == NULL)
	return afs_convert_code(ENOMEM);
    iovecp = ancr->auio->uio_iov;

    afs_lru_cache_init(&lrupages);

    for(page_ix = 0; page_ix < num_pages; ++page_ix) {

	if(list_empty(page_list))
	    break;

	pp = list_entry(page_list->prev, struct page, lru);
	/* If we allocate a page and don't remove it from page_list,
	 * the page cache gets upset. */
	list_del(&pp->lru);
	isize = (i_size_read(fp->f_mapping->host) - 1) >> PAGE_SHIFT;
	if(pp->index > isize) {
	    if(PageLocked(pp))
		unlock_page(pp);
	    put_page(pp);
	    continue;
	}

	if(page_ix == 0) {
	    offset = page_offset(pp);
	    ancr->offset = ancr->auio->uio_offset = offset;
	    base_index = pp->index;
	}
        iovecp[page_ix].iov_len = PAGE_SIZE;
        code = add_to_page_cache(pp, mapping, pp->index, GFP_KERNEL);
        if(base_index != pp->index) {
            if(PageLocked(pp))
		 unlock_page(pp);
            put_page(pp);
	    iovecp[page_ix].iov_base = (void *) 0;
	    base_index++;
	    ancr->length -= PAGE_SIZE;
	    continue;
        }
        base_index++;
        if(code) {
	    if(PageLocked(pp))
		unlock_page(pp);
	    put_page(pp);
	    iovecp[page_ix].iov_base = (void *) 0;
	} else {
	    page_count++;
	    if(!PageLocked(pp)) {
		lock_page(pp);
	    }

	    /* save the page for background map */
            iovecp[page_ix].iov_base = (void*) pp;

	    /* and put it on the LRU cache */
	    afs_lru_cache_add(&lrupages, pp);
        }
    }

    /* If there were useful pages in the page list, make sure all pages
     * are in the LRU cache, then schedule the read */
    if(page_count) {
	afs_lru_cache_finalize(&lrupages);
	credp = crref();
	/* background thread frees the ancr */
        code = afs_ReadNoCache(avc, ancr, credp);
	crfree(credp);
    } else {
        /* If there is nothing for the background thread to handle,
         * it won't be freeing the things that we never gave it */
	afs_free_ncr(&ancr);
    }
    /* we do not flush, release, or unmap pages--that will be
     * done for us by the background thread as each page comes in
     * from the fileserver */
    return afs_convert_code(code);
}
#endif /* STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READAHEAD */

static int
afs_linux_bypass_readpage(struct file *fp, struct page *pp)
{
    cred_t *credp = NULL;
    struct uio *auio;
    struct iovec *iovecp;
    struct nocache_read_request *ancr;
    int code;

    /*
     * Special case: if page is at or past end of file, just zero it and set
     * it as up to date.
     */
    if (page_offset(pp) >=  i_size_read(fp->f_mapping->host)) {
	zero_user_segment(pp, 0, PAGE_SIZE);
	SetPageUptodate(pp);
	unlock_page(pp);
	return 0;
    }

    ClearPageError(pp);

    /* receiver frees */
    ancr = afs_alloc_ncr(1);
    if (ancr == NULL) {
	SetPageError(pp);
	return afs_convert_code(ENOMEM);
    }
    /*
     * afs_alloc_ncr has already set the auio->uio_iov, make sure setup_uio
     * uses the existing value when it sets auio->uio_iov.
     */
    auio = ancr->auio;
    iovecp = auio->uio_iov;

    /* address can be NULL, because we overwrite it with 'pp', below */
    setup_uio(auio, iovecp, NULL, page_offset(pp),
	      PAGE_SIZE, UIO_READ, AFS_UIOSYS);

    /* save the page for background map */
    get_page(pp); /* see above */
    auio->uio_iov->iov_base = (void*) pp;
    /* the background thread will free this */
    ancr->offset = page_offset(pp);
    ancr->length = PAGE_SIZE;

    credp = crref();
    code = afs_ReadNoCache(VTOAFS(FILE_INODE(fp)), ancr, credp);
    crfree(credp);

    return afs_convert_code(code);
}

static inline int
afs_linux_can_bypass(struct inode *ip) {

    switch(cache_bypass_strategy) {
	case NEVER_BYPASS_CACHE:
	    return 0;
	case ALWAYS_BYPASS_CACHE:
	    return 1;
	case LARGE_FILES_BYPASS_CACHE:
	    if (i_size_read(ip) > cache_bypass_threshold)
		return 1;
	    AFS_FALLTHROUGH;
	default:
	    return 0;
     }
}

/* Check if a file is permitted to bypass the cache by policy, and modify
 * the cache bypass state recorded for that file */

static inline int
afs_linux_bypass_check(struct inode *ip) {
    cred_t* credp;

    int bypass = afs_linux_can_bypass(ip);

    credp = crref();
    trydo_cache_transition(VTOAFS(ip), credp, bypass);
    crfree(credp);

    return bypass;
}


static int
afs_linux_readpage(struct file *fp, struct page *pp)
{
    int code;

    if (afs_linux_bypass_check(FILE_INODE(fp))) {
	code = afs_linux_bypass_readpage(fp, pp);
    } else {
	code = afs_linux_fillpage(fp, pp);
	if (!code)
	    code = afs_linux_prefetch(fp, pp);
	unlock_page(pp);
    }

    return code;
}

#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READ_FOLIO)
static int
afs_linux_read_folio(struct file *fp, struct folio *folio)
{
    struct page *pp = &folio->page;

    return afs_linux_readpage(fp, pp);
}
#endif

/*
 * Updates the adc and acacheFp parameters
 * Returns:
 *    0 - success
 *   -1 - problem getting inode or no mapping function
 */
static int
get_dcache_readahead(struct dcache **adc, struct file **acacheFp,
		     struct vcache *avc, loff_t offset)
{
    struct dcache *tdc = *adc;
    struct file *cacheFp = *acacheFp;
    int code;

    if (tdc != NULL && tdc->f.chunk != AFS_CHUNK(offset)) {
	AFS_GLOCK();
	ReleaseReadLock(&tdc->lock);
	afs_PutDCache(tdc);
	AFS_GUNLOCK();
	tdc = NULL;
	if (cacheFp != NULL) {
	    filp_close(cacheFp, NULL);
	    cacheFp = NULL;
	}
    }

    if (tdc == NULL) {
	AFS_GLOCK();
	tdc = afs_FindDCache(avc, offset);
	if (tdc != NULL) {
	    ObtainReadLock(&tdc->lock);
	    if (!afs_IsDCacheFresh(tdc, avc) ||
		(tdc->dflags & DFFetching) != 0) {
		ReleaseReadLock(&tdc->lock);
		afs_PutDCache(tdc);
		tdc = NULL;
	    }
	}
	AFS_GUNLOCK();
	if (tdc != NULL) {
	    cacheFp = afs_linux_raw_open(&tdc->f.inode);
	    if (cacheFp == NULL) {
		/* Problem getting the inode */
		code = -1;
		goto out;
	    }
	    if (!file_can_read_pages(cacheFp)) {
		cachefs_noreadpage = 1;
		/* No mapping function */
		code = -1;
		goto out;
	    }
	}
    }
    code = 0;

 out:
    if (code != 0) {
	if (cacheFp != NULL) {
	    filp_close(cacheFp, NULL);
	    cacheFp = NULL;
	}
	if (tdc != NULL) {
	    AFS_GLOCK();
	    ReleaseReadLock(&tdc->lock);
	    afs_PutDCache(tdc);
	    AFS_GUNLOCK();
	    tdc = NULL;
	}
    }
    *adc = tdc;
    *acacheFp = cacheFp;
    return code;
}

#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READAHEAD)
/*
 * Readahead reads a number of pages for a particular file. We use
 * this to optimise the reading, by limiting the number of times upon which
 * we have to lookup, lock and open vcaches and dcaches.
 *
 * Upon return, the vfs layer handles unlocking and putting any pages in the
 * rac that we did not process here.
 *
 * Note: any errors detected during readahead are ignored at this stage by the
 * vfs. We just need to unlock/put the page and return.  Errors will be detected
 * later in the vfs processing.
 */
static void
afs_linux_readahead(struct readahead_control *rac)
{
    struct page *page;
    struct address_space *mapping = rac->mapping;
    struct inode *inode = mapping->host;
    struct vcache *avc = VTOAFS(inode);
    struct dcache *tdc;
    struct file *cacheFp = NULL;
    int code;
    loff_t offset;
    struct afs_lru_pages lrupages;
    struct afs_pagecopy_task *task;

    if (afs_linux_bypass_check(inode)) {
	afs_linux_bypass_readahead(rac);
	return;
    }
    if (cacheDiskType == AFS_FCACHE_TYPE_MEM)
	return;

    /* No readpage (ex: tmpfs) , skip */
    if (cachefs_noreadpage)
	return;

    AFS_GLOCK();
    code = afs_linux_VerifyVCache(avc, NULL);
    if (code != 0) {
	AFS_GUNLOCK();
	return;
    }

    ObtainWriteLock(&avc->lock, 912);
    AFS_GUNLOCK();

    task = afs_pagecopy_init_task();

    tdc = NULL;

    afs_lru_cache_init(&lrupages);

    while ((page = readahead_page(rac)) != NULL) {
	offset = page_offset(page);

	code = get_dcache_readahead(&tdc, &cacheFp, avc, offset);
	if (code != 0) {
	    if (PageLocked(page)) {
		unlock_page(page);
	    }
	    put_page(page);
	    goto done;
	}

	if (tdc != NULL) {
	    /* afs_linux_read_cache will unlock the page */
	    afs_linux_read_cache(cacheFp, page, tdc->f.chunk, &lrupages, task);
	} else if (PageLocked(page)) {
	    unlock_page(page);
	}
	put_page(page);
    }

 done:
    afs_lru_cache_finalize(&lrupages);

    if (cacheFp != NULL)
	filp_close(cacheFp, NULL);

    afs_pagecopy_put_task(task);

    AFS_GLOCK();
    if (tdc != NULL) {
	ReleaseReadLock(&tdc->lock);
	afs_PutDCache(tdc);
    }

    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    return;
}
#else /* STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READAHEAD */
/* Readpages reads a number of pages for a particular file. We use
 * this to optimise the reading, by limiting the number of times upon which
 * we have to lookup, lock and open vcaches and dcaches
 */
static int
afs_linux_readpages(struct file *fp, struct address_space *mapping,
		    struct list_head *page_list, unsigned int num_pages)
{
    struct inode *inode = mapping->host;
    struct vcache *avc = VTOAFS(inode);
    struct dcache *tdc;
    struct file *cacheFp = NULL;
    int code;
    unsigned int page_idx;
    loff_t offset;
    struct afs_lru_pages lrupages;
    struct afs_pagecopy_task *task;

    if (afs_linux_bypass_check(inode))
	return afs_linux_bypass_readpages(fp, mapping, page_list, num_pages);

    if (cacheDiskType == AFS_FCACHE_TYPE_MEM)
	return 0;

    /* No readpage (ex: tmpfs) , skip */
    if (cachefs_noreadpage)
	return 0;

    AFS_GLOCK();
    if ((code = afs_linux_VerifyVCache(avc, NULL))) {
	AFS_GUNLOCK();
	return code;
    }

    ObtainWriteLock(&avc->lock, 912);
    AFS_GUNLOCK();

    task = afs_pagecopy_init_task();

    tdc = NULL;

    afs_lru_cache_init(&lrupages);

    for (page_idx = 0; page_idx < num_pages; page_idx++) {
	struct page *page = list_entry(page_list->prev, struct page, lru);
	list_del(&page->lru);
	offset = page_offset(page);

	code = get_dcache_readahead(&tdc, &cacheFp, avc, offset);
	if (code != 0) {
	    put_page(page);
	    goto out;
	}

	if (tdc && !afs_add_to_page_cache_lru(&lrupages, page, mapping, page->index,
					      GFP_KERNEL)) {
	    /* Note that afs_add_to_page_cache_lru() locks the 'page'.
	     * afs_linux_read_cache() is guaranteed to handle unlocking it. */
	    afs_linux_read_cache(cacheFp, page, tdc->f.chunk, &lrupages, task);
	}
	put_page(page);
    }
    afs_lru_cache_finalize(&lrupages);

out:
    if (cacheFp)
	filp_close(cacheFp, NULL);

    afs_pagecopy_put_task(task);

    AFS_GLOCK();
    if (tdc) {
	ReleaseReadLock(&tdc->lock);
        afs_PutDCache(tdc);
    }

    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    return 0;
}
#endif /* STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READAHEAD */

/* Prepare an AFS vcache for writeback. Should be called with the vcache
 * locked */
static inline int
afs_linux_prepare_writeback(struct vcache *avc) {
    pid_t pid;
    struct pagewriter *pw;

    pid = MyPidxx2Pid(MyPidxx);
    /* Prevent recursion into the writeback code */
    spin_lock(&avc->pagewriter_lock);
    list_for_each_entry(pw, &avc->pagewriters, link) {
	if (pw->writer == pid) {
	    spin_unlock(&avc->pagewriter_lock);
	    return AOP_WRITEPAGE_ACTIVATE;
	}
    }
    spin_unlock(&avc->pagewriter_lock);

    /* Add ourselves to writer list */
    pw = osi_Alloc(sizeof(struct pagewriter));
    pw->writer = pid;
    spin_lock(&avc->pagewriter_lock);
    list_add_tail(&pw->link, &avc->pagewriters);
    spin_unlock(&avc->pagewriter_lock);

    return 0;
}

static inline int
afs_linux_dopartialwrite(struct vcache *avc, cred_t *credp) {
    struct vrequest *treq = NULL;
    int code = 0;

    if (!afs_CreateReq(&treq, credp)) {
	code = afs_DoPartialWrite(avc, treq);
	afs_DestroyReq(treq);
    }

    return afs_convert_code(code);
}

static inline void
afs_linux_complete_writeback(struct vcache *avc) {
    struct pagewriter *pw, *store;
    pid_t pid;
    struct list_head tofree;

    INIT_LIST_HEAD(&tofree);
    pid = MyPidxx2Pid(MyPidxx);
    /* Remove ourselves from writer list */
    spin_lock(&avc->pagewriter_lock);
    list_for_each_entry_safe(pw, store, &avc->pagewriters, link) {
	if (pw->writer == pid) {
	    list_del(&pw->link);
	    /* osi_Free may sleep so we need to defer it */
	    list_add_tail(&pw->link, &tofree);
	}
    }
    spin_unlock(&avc->pagewriter_lock);
    list_for_each_entry_safe(pw, store, &tofree, link) {
	list_del(&pw->link);
	osi_Free(pw, sizeof(struct pagewriter));
    }
}

/* Writeback a given page syncronously. Called with no AFS locks held */
static int
afs_linux_page_writeback(struct inode *ip, struct page *pp,
			 unsigned long offset, unsigned int count,
			 cred_t *credp)
{
    struct vcache *vcp = VTOAFS(ip);
    char *buffer;
    afs_offs_t base;
    int code = 0;
    struct uio tuio;
    struct iovec iovec;
    int f_flags = 0;

    memset(&tuio, 0, sizeof(tuio));
    memset(&iovec, 0, sizeof(iovec));

    buffer = kmap(pp) + offset;
    base = page_offset(pp) + offset;

    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_UPDATEPAGE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, page_count(pp),
	       ICL_TYPE_INT32, 99999);

    setup_uio(&tuio, &iovec, buffer, base, count, UIO_WRITE, AFS_UIOSYS);

    code = afs_write(vcp, &tuio, f_flags, credp, 0);

    i_size_write(ip, vcp->f.m.Length);
    ip->i_blocks = ((vcp->f.m.Length + 1023) >> 10) << 1;

    code = code ? afs_convert_code(code) : count - tuio.uio_resid;

    afs_Trace4(afs_iclSetp, CM_TRACE_UPDATEPAGE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, page_count(pp),
	       ICL_TYPE_INT32, code);

    AFS_GUNLOCK();
    kunmap(pp);

    return code;
}

static int
afs_linux_writepage_sync(struct inode *ip, struct page *pp,
			 unsigned long offset, unsigned int count)
{
    int code;
    int code1 = 0;
    struct vcache *vcp = VTOAFS(ip);
    cred_t *credp;

    /* Catch recursive writeback. This occurs if the kernel decides
     * writeback is required whilst we are writing to the cache, or
     * flushing to the server. When we're running syncronously (as
     * opposed to from writepage) we can't actually do anything about
     * this case - as we can't return AOP_WRITEPAGE_ACTIVATE to write()
     */
    AFS_GLOCK();
    ObtainWriteLock(&vcp->lock, 532);
    afs_linux_prepare_writeback(vcp);
    ReleaseWriteLock(&vcp->lock);
    AFS_GUNLOCK();

    credp = crref();
    code = afs_linux_page_writeback(ip, pp, offset, count, credp);

    AFS_GLOCK();
    ObtainWriteLock(&vcp->lock, 533);
    if (code > 0)
	code1 = afs_linux_dopartialwrite(vcp, credp);
    afs_linux_complete_writeback(vcp);
    ReleaseWriteLock(&vcp->lock);
    AFS_GUNLOCK();
    crfree(credp);

    if (code1)
	return code1;

    return code;
}

static int
#ifdef AOP_WRITEPAGE_TAKES_WRITEBACK_CONTROL
afs_linux_writepage(struct page *pp, struct writeback_control *wbc)
#else
afs_linux_writepage(struct page *pp)
#endif
{
    struct address_space *mapping = pp->mapping;
    struct inode *inode;
    struct vcache *vcp;
    cred_t *credp;
    unsigned int to = PAGE_SIZE;
    loff_t isize;
    int code = 0;
    int code1 = 0;

    get_page(pp);

    inode = mapping->host;
    vcp = VTOAFS(inode);
    isize = i_size_read(inode);

    /* Don't defeat an earlier truncate */
    if (page_offset(pp) > isize) {
	set_page_writeback(pp);
	unlock_page(pp);
	goto done;
    }

    AFS_GLOCK();
    ObtainWriteLock(&vcp->lock, 537);
    code = afs_linux_prepare_writeback(vcp);
    if (code == AOP_WRITEPAGE_ACTIVATE) {
	/* WRITEPAGE_ACTIVATE is the only return value that permits us
	 * to return with the page still locked */
	ReleaseWriteLock(&vcp->lock);
	AFS_GUNLOCK();
	return code;
    }

    /* Grab the creds structure currently held in the vnode, and
     * get a reference to it, in case it goes away ... */
    credp = vcp->cred;
    if (credp)
	crhold(credp);
    else
	credp = crref();
    ReleaseWriteLock(&vcp->lock);
    AFS_GUNLOCK();

    set_page_writeback(pp);

    SetPageUptodate(pp);

    /* We can unlock the page here, because it's protected by the
     * page_writeback flag. This should make us less vulnerable to
     * deadlocking in afs_write and afs_DoPartialWrite
     */
    unlock_page(pp);

    /* If this is the final page, then just write the number of bytes that
     * are actually in it */
    if ((isize - page_offset(pp)) < to )
	to = isize - page_offset(pp);

    code = afs_linux_page_writeback(inode, pp, 0, to, credp);

    AFS_GLOCK();
    ObtainWriteLock(&vcp->lock, 538);

    /* As much as we might like to ignore a file server error here,
     * and just try again when we close(), unfortunately StoreAllSegments
     * will invalidate our chunks if the server returns a permanent error,
     * so we need to at least try and get that error back to the user
     */
    if (code == to)
	code1 = afs_linux_dopartialwrite(vcp, credp);

    afs_linux_complete_writeback(vcp);
    ReleaseWriteLock(&vcp->lock);
    crfree(credp);
    AFS_GUNLOCK();

done:
    end_page_writeback(pp);
    put_page(pp);

    if (code1)
	return code1;

    if (code == to)
	return 0;

    return code;
}

/* afs_linux_permission
 * Check access rights - returns error if can't check or permission denied.
 */

#if defined(IOP_TAKES_MNT_IDMAP)
static int
afs_linux_permission(struct mnt_idmap *idmap, struct inode *ip, int mode)
#elif defined(IOP_TAKES_USER_NAMESPACE)
static int
afs_linux_permission(struct user_namespace *mnt_userns, struct inode *ip, int mode)
#elif defined(IOP_PERMISSION_TAKES_FLAGS)
static int
afs_linux_permission(struct inode *ip, int mode, unsigned int flags)
#elif defined(IOP_PERMISSION_TAKES_NAMEIDATA)
static int
afs_linux_permission(struct inode *ip, int mode, struct nameidata *nd)
#else
static int
afs_linux_permission(struct inode *ip, int mode)
#endif
{
    int code;
    cred_t *credp;
    int tmp = 0;

    /* Check for RCU path walking */
#if defined(IOP_PERMISSION_TAKES_FLAGS)
    if (flags & IPERM_FLAG_RCU)
       return -ECHILD;
#elif defined(MAY_NOT_BLOCK)
    if (mode & MAY_NOT_BLOCK)
       return -ECHILD;
#endif

    credp = crref();
    AFS_GLOCK();
    if (mode & MAY_EXEC)
	tmp |= VEXEC;
    if (mode & MAY_READ)
	tmp |= VREAD;
    if (mode & MAY_WRITE)
	tmp |= VWRITE;
    code = afs_access(VTOAFS(ip), tmp, credp);

    AFS_GUNLOCK();
    crfree(credp);
    return afs_convert_code(code);
}

static int
afs_linux_commit_write(struct file *file, struct page *page, unsigned offset,
		       unsigned to)
{
    int code;
    struct inode *inode = FILE_INODE(file);
    loff_t pagebase = page_offset(page);

    if (i_size_read(inode) < (pagebase + offset))
	i_size_write(inode, pagebase + offset);

    if (PageChecked(page)) {
	SetPageUptodate(page);
	ClearPageChecked(page);
    }

    code = afs_linux_writepage_sync(inode, page, offset, to - offset);

    return code;
}

static int
afs_linux_prepare_write(struct file *file, struct page *page, unsigned from,
			unsigned to)
{

    /*
     * Linux's Documentation/filesystems/vfs.txt (.rst) details the expected
     * behaviour of prepare_write (prior to 2.6.28) and write_begin (2.6.28).
     * Essentially, if the page exists within the file, and is not being fully
     * written, then we should populate it.
     */

    if (!PageUptodate(page)) {
	loff_t pagebase = page_offset(page);
	loff_t isize = i_size_read(page->mapping->host);

	/* Is the location we are writing to beyond the end of the file? */
	if (pagebase >= isize ||
	    ((from == 0) && (pagebase + to) >= isize)) {
	    zero_user_segments(page, 0, from, to, PAGE_SIZE);
	    SetPageChecked(page);
	/* Are we we writing a full page */
	} else if (from == 0 && to == PAGE_SIZE) {
	    SetPageChecked(page);
	/* Is the page readable, if it's wronly, we don't care, because we're
	 * not actually going to read from it ... */
	} else if ((file->f_flags & O_ACCMODE) != O_WRONLY) {
	    /* We don't care if fillpage fails, because if it does the page
	     * won't be marked as up to date
	     */
	    afs_linux_fillpage(file, page);
	}
    }
    return 0;
}

#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_WRITE_BEGIN)
# if defined(HAVE_LINUX_WRITE_BEGIN_END_FOLIO)
static int
afs_linux_write_end(struct file *file, struct address_space *mapping,
		    loff_t pos, unsigned len, unsigned copied,
		    struct folio *folio, void *fsdata)
# else
static int
afs_linux_write_end(struct file *file, struct address_space *mapping,
		    loff_t pos, unsigned len, unsigned copied,
		    struct page *page, void *fsdata)
# endif
{
    int code;
    unsigned int from = pos & (PAGE_SIZE - 1);
# if defined(HAVE_LINUX_WRITE_BEGIN_END_FOLIO)
    struct page *page = &folio->page;
# endif

    code = afs_linux_commit_write(file, page, from, from + copied);

    unlock_page(page);
    put_page(page);
    return code;
}

# if defined(HAVE_LINUX_WRITE_BEGIN_END_FOLIO)
static int
afs_linux_write_begin(struct file *file, struct address_space *mapping,
		      loff_t pos, unsigned len,
		      struct folio **foliop, void **fsdata)
# elif defined(HAVE_LINUX_GRAB_CACHE_PAGE_WRITE_BEGIN_NOFLAGS)
static int
afs_linux_write_begin(struct file *file, struct address_space *mapping,
		      loff_t pos, unsigned len,
		      struct page **pagep, void **fsdata)
# else
static int
afs_linux_write_begin(struct file *file, struct address_space *mapping,
		      loff_t pos, unsigned len, unsigned flags,
		      struct page **pagep, void **fsdata)
# endif
{
    struct page *page;
    pgoff_t index = pos >> PAGE_SHIFT;
    unsigned int from = pos & (PAGE_SIZE - 1);
    int code;

# if defined(HAVE_LINUX_GRAB_CACHE_PAGE_WRITE_BEGIN_NOFLAGS)
    page = grab_cache_page_write_begin(mapping, index);
# else
    page = grab_cache_page_write_begin(mapping, index, flags);
# endif
    if (!page) {
	return -ENOMEM;
    }

# if defined(HAVE_LINUX_WRITE_BEGIN_END_FOLIO)
    *foliop = page_folio(page);
# else
    *pagep = page;
# endif

    code = afs_linux_prepare_write(file, page, from, from + len);
    if (code) {
	unlock_page(page);
	put_page(page);
    }

    return code;
}
#endif /* STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_WRITE_BEGIN */

#ifndef STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT
static void *
afs_linux_dir_follow_link(struct dentry *dentry, struct nameidata *nd)
{
    struct dentry **dpp;
    struct dentry *target;

    if (current->total_link_count > 0) {
	/* avoid symlink resolution limits when resolving; we cannot contribute to
	 * an infinite symlink loop */
	/* only do this for follow_link when total_link_count is positive to be
	 * on the safe side; there is at least one code path in the Linux
	 * kernel where it seems like it may be possible to get here without
	 * total_link_count getting incremented. it is not clear on how that
	 * path is actually reached, but guard against it just to be safe */
	current->total_link_count--;
    }

    target = canonical_dentry(dentry->d_inode);

# ifdef STRUCT_NAMEIDATA_HAS_PATH
    dpp = &nd->path.dentry;
# else
    dpp = &nd->dentry;
# endif

    dput(*dpp);

    if (target) {
	*dpp = target;
    } else {
	*dpp = dget(dentry);
    }

    nd->last_type = LAST_BIND;

    return NULL;
}
#endif /* !STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT */


static struct inode_operations afs_file_iops = {
  .permission =		afs_linux_permission,
  .getattr =		afs_linux_getattr,
  .setattr =		afs_notify_change,
};

static struct address_space_operations afs_file_aops = {
#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READ_FOLIO)
  .read_folio =		afs_linux_read_folio,
#else
  .readpage =		afs_linux_readpage,
#endif
#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READAHEAD)
  .readahead =		afs_linux_readahead,
#else
  .readpages = 		afs_linux_readpages,
#endif
  .writepage =		afs_linux_writepage,
#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_DIRTY_FOLIO) && \
    defined(HAVE_LINUX_BLOCK_DIRTY_FOLIO)
  .dirty_folio =	block_dirty_folio,
#else
  .set_page_dirty =	__set_page_dirty_buffers,
#endif
#if defined (STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_WRITE_BEGIN)
  .write_begin =        afs_linux_write_begin,
  .write_end =          afs_linux_write_end,
#else
  .commit_write =       afs_linux_commit_write,
  .prepare_write =      afs_linux_prepare_write,
#endif
};


/* Separate ops vector for directories. Linux 2.2 tests type of inode
 * by what sort of operation is allowed.....
 */

static struct inode_operations afs_dir_iops = {
  .setattr =		afs_notify_change,
  .create =		afs_linux_create,
  .lookup =		afs_linux_lookup,
  .link =		afs_linux_link,
  .unlink =		afs_linux_unlink,
  .symlink =		afs_linux_symlink,
  .mkdir =		afs_linux_mkdir,
  .rmdir =		afs_linux_rmdir,
  .rename =		afs_linux_rename,
  .getattr =		afs_linux_getattr,
  .permission =		afs_linux_permission,
#ifndef STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT
  .follow_link =        afs_linux_dir_follow_link,
#endif
};

/* We really need a separate symlink set of ops, since do_follow_link()
 * determines if it _is_ a link by checking if the follow_link op is set.
 */
#if defined(USABLE_KERNEL_PAGE_SYMLINK_CACHE)
static int
afs_symlink_filler(struct file *file, struct page *page)
{
    struct inode *ip = (struct inode *)page->mapping->host;
    char *p = (char *)kmap(page);
    int code;

    AFS_GLOCK();
    code = afs_linux_ireadlink(ip, p, PAGE_SIZE, AFS_UIOSYS);
    AFS_GUNLOCK();

    if (code < 0)
	goto fail;
    p[code] = '\0';		/* null terminate? */

    SetPageUptodate(page);
    kunmap(page);
    unlock_page(page);
    return 0;

  fail:
    SetPageError(page);
    kunmap(page);
    unlock_page(page);
    return code;
}
#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READ_FOLIO)
static int
afs_symlink_filler_folio(struct file *file, struct folio *folio)
{
    struct page *page = &folio->page;
    return afs_symlink_filler(file, page);
}
#endif


static struct address_space_operations afs_symlink_aops = {
#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_READ_FOLIO)
  .read_folio =	afs_symlink_filler_folio
#else
  .readpage =	afs_symlink_filler
#endif
};
#endif	/* USABLE_KERNEL_PAGE_SYMLINK_CACHE */

static struct inode_operations afs_symlink_iops = {
#if defined(USABLE_KERNEL_PAGE_SYMLINK_CACHE)
  .readlink = 		page_readlink,
# if defined(HAVE_LINUX_PAGE_GET_LINK)
  .get_link =		page_get_link,
# elif defined(HAVE_LINUX_PAGE_FOLLOW_LINK)
  .follow_link =	page_follow_link,
# else
  .follow_link =	page_follow_link_light,
  .put_link =           page_put_link,
# endif
#else /* !defined(USABLE_KERNEL_PAGE_SYMLINK_CACHE) */
  .readlink = 		afs_linux_readlink,
  .follow_link =	afs_linux_follow_link,
  .put_link =		afs_linux_put_link,
#endif /* USABLE_KERNEL_PAGE_SYMLINK_CACHE */
  .setattr =		afs_notify_change,
  .getattr =		afs_linux_getattr,
};

void
afs_fill_inode(struct inode *ip, struct vattr *vattr)
{
    if (vattr)
	vattr2inode(ip, vattr);

#ifdef STRUCT_ADDRESS_SPACE_HAS_BACKING_DEV_INFO
    ip->i_mapping->backing_dev_info = afs_backing_dev_info;
#endif
/* Reset ops if symlink or directory. */
    if (S_ISREG(ip->i_mode)) {
	ip->i_op = &afs_file_iops;
	ip->i_fop = &afs_file_fops;
	ip->i_data.a_ops = &afs_file_aops;

    } else if (S_ISDIR(ip->i_mode)) {
	ip->i_op = &afs_dir_iops;
	ip->i_fop = &afs_dir_fops;

    } else if (S_ISLNK(ip->i_mode)) {
	ip->i_op = &afs_symlink_iops;
#if defined(HAVE_LINUX_INODE_NOHIGHMEM)
	inode_nohighmem(ip);
#endif
#if defined(USABLE_KERNEL_PAGE_SYMLINK_CACHE)
	ip->i_data.a_ops = &afs_symlink_aops;
	ip->i_mapping = &ip->i_data;
#endif
    }

}
