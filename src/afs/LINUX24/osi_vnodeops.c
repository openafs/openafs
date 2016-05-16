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
#include "h/mm.h"
#ifdef HAVE_MM_INLINE_H
#include "h/mm_inline.h"
#endif
#include "h/pagemap.h"
#if defined(AFS_LINUX24_ENV)
#include "h/smp_lock.h"
#endif
#include "afs/lock.h"
#include "afs/afs_bypasscache.h"

#ifdef pgoff2loff
#define pageoff(pp) pgoff2loff((pp)->index)
#else
#define pageoff(pp) pp->offset
#endif

#ifndef MAX_ERRNO
#define MAX_ERRNO 1000L
#endif

extern struct vcache *afs_globalVp;
#if defined(AFS_LINUX24_ENV)
/* Some uses of BKL are perhaps not needed for bypass or memcache--
 * why don't we try it out? */
extern struct afs_cacheOps afs_UfsCacheOps;
#define maybe_lock_kernel()			\
    do {					       \
	if(afs_cacheType == &afs_UfsCacheOps)	       \
	    lock_kernel();			       \
    } while(0);


#define maybe_unlock_kernel()			\
    do {					       \
	if(afs_cacheType == &afs_UfsCacheOps)	       \
	    unlock_kernel();			       \
    } while(0);
#endif /* AFS_LINUX24_ENV */


/* This function converts a positive error code from AFS into a negative
 * code suitable for passing into the Linux VFS layer. It checks that the
 * error code is within the permissable bounds for the ERR_PTR mechanism.
 *
 * _All_ error codes which come from the AFS layer should be passed through
 * this function before being returned to the kernel.
 */

static inline int afs_convert_code(int code) {
    if ((code >= 0) && (code <= MAX_ERRNO))
	return -code;
    else
	return -EIO;
}

/* Linux doesn't require a credp for many functions, and crref is an expensive
 * operation. This helper function avoids obtaining it for VerifyVCache calls
 */

static inline int afs_linux_VerifyVCache(struct vcache *avc, cred_t **retcred) {
    cred_t *credp = NULL;
    struct vrequest treq;
    int code;

    if (avc->f.states & CStatd) {
        if (retcred)
            *retcred = NULL;
	return 0;
    }

    credp = crref();

    code = afs_InitReq(&treq, credp);
    if (code == 0)
        code = afs_VerifyVCache2(avc, &treq);

    if (retcred != NULL)
        *retcred = credp;
    else
        crfree(credp);

    return afs_convert_code(code);
}

static ssize_t
afs_linux_read(struct file *fp, char *buf, size_t count, loff_t * offp)
{
    ssize_t code = 0;
    struct vcache *vcp = VTOAFS(fp->f_dentry->d_inode);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    afs_size_t isize, offindex;
#endif

    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_READOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, offp, ICL_TYPE_INT32, count, ICL_TYPE_INT32,
	       99999);
    code = afs_linux_VerifyVCache(vcp, NULL);

    if (code == 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	isize = (i_size_read(fp->f_mapping->host) - 1) >> PAGE_CACHE_SHIFT;
        offindex = *offp >> PAGE_CACHE_SHIFT;
        if(offindex > isize) {
            code=0;
            goto done;
        }
#endif
	/* Linux's FlushPages implementation doesn't ever use credp,
	 * so we optimise by not using it */
	osi_FlushPages(vcp, NULL);	/* ensure stale pages are gone */
	AFS_GUNLOCK();
#ifdef HAVE_LINUX_DO_SYNC_READ
	code = do_sync_read(fp, buf, count, offp);
#else
	code = generic_file_read(fp, buf, count, offp);
#endif
	AFS_GLOCK();
    }

    afs_Trace4(afs_iclSetp, CM_TRACE_READOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, offp, ICL_TYPE_INT32, count, ICL_TYPE_INT32,
	       code);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
done:
#endif
    AFS_GUNLOCK();
    return code;
}


/* Now we have integrated VM for writes as well as reads. generic_file_write
 * also takes care of re-positioning the pointer if file is open in append
 * mode. Call fake open/close to ensure we do writes of core dumps.
 */
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
#ifdef DO_SYNC_READ
	    code = do_sync_write(fp, buf, count, offp);
#else
	    code = generic_file_write(fp, buf, count, offp);
#endif
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

extern int BlobScan(struct dcache * afile, afs_int32 ablob);

/* This is a complete rewrite of afs_readdir, since we can make use of
 * filldir instead of afs_readdir_move. Note that changes to vcache/dcache
 * handling and use of bulkstats will need to be reflected here as well.
 */
static int
afs_linux_readdir(struct file *fp, void *dirbuf, filldir_t filldir)
{
    struct vcache *avc = VTOAFS(FILE_INODE(fp));
    struct vrequest treq;
    struct dcache *tdc;
    int code;
    int offset;
    int dirpos;
    struct DirEntry *de;
    ino_t ino;
    int len;
    afs_size_t origOffset, tlen;
    cred_t *credp = crref();
    struct afs_fakestat_state fakestat;

    AFS_GLOCK();
    AFS_STATCNT(afs_readdir);

    code = afs_convert_code(afs_InitReq(&treq, credp));
    crfree(credp);
    if (code)
	goto out1;

    afs_InitFakeStat(&fakestat);
    code = afs_convert_code(afs_EvalFakeStat(&avc, &fakestat, &treq));
    if (code)
	goto out;

    /* update the cache entry */
  tagain:
    code = afs_convert_code(afs_VerifyVCache2(avc, &treq));
    if (code)
	goto out;

    /* get a reference to the entire directory */
    tdc = afs_GetDCache(avc, (afs_size_t) 0, &treq, &origOffset, &tlen, 1);
    len = tlen;
    if (!tdc) {
	code = -ENOENT;
	goto out;
    }
    ObtainSharedLock(&avc->lock, 810);
    UpgradeSToWLock(&avc->lock, 811);
    ObtainReadLock(&tdc->lock);
    /*
     * Make sure that the data in the cache is current. There are two
     * cases we need to worry about:
     * 1. The cache data is being fetched by another process.
     * 2. The cache data is no longer valid
     */
    while ((avc->f.states & CStatd)
	   && (tdc->dflags & DFFetching)
	   && hsame(avc->f.m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseSharedLock(&avc->lock);
	afs_osi_Sleep(&tdc->validPos);
	ObtainSharedLock(&avc->lock, 812);
	ObtainReadLock(&tdc->lock);
    }
    if (!(avc->f.states & CStatd)
	|| !hsame(avc->f.m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseSharedLock(&avc->lock);
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
    offset = (int) fp->f_pos;
    while (1) {
	dirpos = BlobScan(tdc, offset);
	if (!dirpos)
	    break;

	de = afs_dir_GetBlob(tdc, dirpos);
	if (!de)
	    break;

	ino = afs_calc_inum(avc->f.fid.Cell, avc->f.fid.Fid.Volume,
	                    ntohl(de->fid.vnode));

	if (de->name)
	    len = strlen(de->name);
	else {
	    printf("afs_linux_readdir: afs_dir_GetBlob failed, null name (inode %lx, dirpos %d)\n", 
		   (unsigned long)&tdc->f.inode, dirpos);
	    DRelease(de, 0);
	    ReleaseSharedLock(&avc->lock);
	    afs_PutDCache(tdc);
	    code = -ENOENT;
	    goto out;
	}

	/* filldir returns -EINVAL when the buffer is full. */
#if (defined(AFS_LINUX24_ENV) || defined(pgoff2loff)) && defined(DECLARE_FSTYPE)
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
		if (tvc->mvstat) {
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
	    code = (*filldir) (dirbuf, de->name, len, offset, ino, type);
	    AFS_GLOCK();
	}
#else
	code = (*filldir) (dirbuf, de->name, len, offset, ino);
#endif
	DRelease(de, 0);
	if (code)
	    break;
	offset = dirpos + 1 + ((len + 16) >> 5);
    }
    /* If filldir didn't fill in the last one this is still pointing to that
     * last attempt.
     */
    fp->f_pos = (loff_t) offset;

    ReleaseReadLock(&tdc->lock);
    afs_PutDCache(tdc);
    UpgradeSToWLock(&avc->lock, 813);
    avc->f.states &= ~CReadDir;
    avc->dcreaddir = 0;
    avc->readdir_pid = 0;
    ReleaseSharedLock(&avc->lock);
    code = 0;

out:
    afs_PutFakeStat(&fakestat);
out1:
    AFS_GUNLOCK();
    return code;
}


/* in afs_pioctl.c */
extern int afs_xioctl(struct inode *ip, struct file *fp, unsigned int com,
		      unsigned long arg);

#if defined(HAVE_UNLOCKED_IOCTL) || defined(HAVE_COMPAT_IOCTL)
static long afs_unlocked_xioctl(struct file *fp, unsigned int com,
                               unsigned long arg) {
    return afs_xioctl(FILE_INODE(fp), fp, com, arg);

}
#endif


static int
afs_linux_mmap(struct file *fp, struct vm_area_struct *vmap)
{
    struct vcache *vcp = VTOAFS(FILE_INODE(fp));
    int code;

    AFS_GLOCK();
#if defined(AFS_LINUX24_ENV)
    afs_Trace3(afs_iclSetp, CM_TRACE_GMAP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, vmap->vm_start, ICL_TYPE_INT32,
	       vmap->vm_end - vmap->vm_start);
#else
    afs_Trace4(afs_iclSetp, CM_TRACE_GMAP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, vmap->vm_start, ICL_TYPE_INT32,
	       vmap->vm_end - vmap->vm_start, ICL_TYPE_INT32,
	       vmap->vm_offset);
#endif

    /* get a validated vcache entry */
    code = afs_linux_VerifyVCache(vcp, NULL);

    /* Linux's Flushpage implementation doesn't use credp, so optimise
     * our code to not need to crref() it */
    osi_FlushPages(vcp, NULL); /* ensure stale pages are gone */
    AFS_GUNLOCK();
    code = generic_file_mmap(fp, vmap);
    AFS_GLOCK();
    if (!code)
	vcp->f.states |= CMAPPED;

    AFS_GUNLOCK();
    return code;
}

static int
afs_linux_open(struct inode *ip, struct file *fp)
{
    struct vcache *vcp = VTOAFS(ip);
    cred_t *credp = crref();
    int code;

#ifdef AFS_LINUX24_ENV
    maybe_lock_kernel();
#endif
    AFS_GLOCK();
    code = afs_open(&vcp, fp->f_flags, credp);
    AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
    maybe_unlock_kernel();
#endif

    crfree(credp);
    return afs_convert_code(code);
}

static int
afs_linux_release(struct inode *ip, struct file *fp)
{
    struct vcache *vcp = VTOAFS(ip);
    cred_t *credp = crref();
    int code = 0;

#ifdef AFS_LINUX24_ENV
    maybe_lock_kernel();
#endif
    AFS_GLOCK();
    code = afs_close(vcp, fp->f_flags, credp);
    AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
    maybe_unlock_kernel();
#endif

    crfree(credp);
    return afs_convert_code(code);
}

static int
#if defined(AFS_LINUX24_ENV)
afs_linux_fsync(struct file *fp, struct dentry *dp, int datasync)
#else
afs_linux_fsync(struct file *fp, struct dentry *dp)
#endif
{
    int code;
    struct inode *ip = FILE_INODE(fp);
    cred_t *credp = crref();

#ifdef AFS_LINUX24_ENV
    maybe_lock_kernel();
#endif
    AFS_GLOCK();
    code = afs_fsync(VTOAFS(ip), credp);
    AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
    maybe_unlock_kernel();
#endif
    crfree(credp);
    return afs_convert_code(code);

}


static int
afs_linux_lock(struct file *fp, int cmd, struct file_lock *flp)
{
    int code = 0;
    struct vcache *vcp = VTOAFS(FILE_INODE(fp));
    cred_t *credp = crref();
    struct AFS_FLOCK flock;
#if defined(POSIX_TEST_LOCK_CONFLICT_ARG)
    struct file_lock conflict;
#elif defined(POSIX_TEST_LOCK_RETURNS_CONFLICT)
    struct file_lock *conflict;
#endif
    
    /* Convert to a lock format afs_lockctl understands. */
    memset(&flock, 0, sizeof(flock));
    flock.l_type = flp->fl_type;
    flock.l_pid = flp->fl_pid;
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

#ifdef AFS_LINUX24_ENV
    if ((code == 0 || flp->fl_type == F_UNLCK) && 
        (cmd == F_SETLK || cmd == F_SETLKW)) {
# ifdef POSIX_LOCK_FILE_WAIT_ARG
	code = posix_lock_file(fp, flp, 0);
# else
	flp->fl_flags &=~ FL_SLEEP;
	code = posix_lock_file(fp, flp);
# endif 
	if (code && flp->fl_type != F_UNLCK) {
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
# if defined(POSIX_TEST_LOCK_CONFLICT_ARG)
        if (posix_test_lock(fp, flp, &conflict)) {
            locks_copy_lock(flp, &conflict);
            flp->fl_type = F_UNLCK;
            crfree(credp);
            return 0;
        }
# elif defined(POSIX_TEST_LOCK_RETURNS_CONFLICT)
	if ((conflict = posix_test_lock(fp, flp))) {
	    locks_copy_lock(flp, conflict);
	    flp->fl_type = F_UNLCK;
	    crfree(credp);
	    return 0;
	}
# else
        posix_test_lock(fp, flp);
        /* If we found a lock in the kernel's structure, return it */
        if (flp->fl_type != F_UNLCK) {
            crfree(credp);
            return 0;
        }
# endif
    }
    
#endif
    /* Convert flock back to Linux's file_lock */
    flp->fl_type = flock.l_type;
    flp->fl_pid = flock.l_pid;
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
    flock.l_type = flp->fl_type;
    flock.l_pid = flp->fl_pid;
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

    if ((code == 0 || flp->fl_type == F_UNLCK) && 
        (cmd == F_SETLK || cmd == F_SETLKW)) {
	flp->fl_flags &=~ FL_SLEEP;
	code = flock_lock_file_wait(fp, flp);
	if (code && flp->fl_type != F_UNLCK) {
	    struct AFS_FLOCK flock2;
	    flock2 = flock;
	    flock2.l_type = F_UNLCK;
	    AFS_GLOCK();
	    afs_lockctl(vcp, &flock2, F_SETLK, credp);
	    AFS_GUNLOCK();
	}
    }
    /* Convert flock back to Linux's file_lock */
    flp->fl_type = flock.l_type;
    flp->fl_pid = flock.l_pid;

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
    struct vrequest treq;
    struct vcache *vcp;
    cred_t *credp;
    int code;
    int bypasscache;

    AFS_GLOCK();

    if ((fp->f_flags & O_ACCMODE) == O_RDONLY) { /* readers dont flush */
	AFS_GUNLOCK();
	return 0;
    }

    AFS_DISCON_LOCK();

    credp = crref();
    vcp = VTOAFS(FILE_INODE(fp));

    code = afs_InitReq(&treq, credp);
    if (code)
	goto out;
	/* If caching is bypassed for this file, or globally, just return 0 */
	if(cache_bypass_strategy == ALWAYS_BYPASS_CACHE)
		bypasscache = 1;
	else {
		ObtainReadLock(&vcp->lock);
		if(vcp->cachingStates & FCSBypass)
			bypasscache = 1;
		ReleaseReadLock(&vcp->lock);
	}
	if(bypasscache) {
            /* future proof: don't rely on 0 return from afs_InitReq */
            code = 0; goto out;
        }

    ObtainSharedLock(&vcp->lock, 535);
    if ((vcp->execsOrWriters > 0) && (file_count(fp) == 1)) {
	UpgradeSToWLock(&vcp->lock, 536);
	if (!AFS_IS_DISCONNECTED) {
		code = afs_StoreAllSegments(vcp,
				&treq,
				AFS_SYNC | AFS_LASTSTORE);
	} else {
		afs_DisconAddDirty(vcp, VDisconWriteOsiFlush, 1);
	}
	ConvertWToSLock(&vcp->lock);
    }
    code = afs_CheckCode(code, &treq, 54);
    ReleaseSharedLock(&vcp->lock);

out:
    AFS_DISCON_UNLOCK();
    AFS_GUNLOCK();

    crfree(credp);
    return afs_convert_code(code);
}

#if !defined(AFS_LINUX24_ENV)
/* Not allowed to directly read a directory. */
ssize_t
afs_linux_dir_read(struct file * fp, char *buf, size_t count, loff_t * ppos)
{
    return -EISDIR;
}
#endif



struct file_operations afs_dir_fops = {
#if !defined(AFS_LINUX24_ENV)
  .read =	afs_linux_dir_read,
  .lock =	afs_linux_lock,
  .fsync =	afs_linux_fsync,
#else
  .read =	generic_read_dir,
#endif
  .readdir =	afs_linux_readdir,
#ifdef HAVE_UNLOCKED_IOCTL
  .unlocked_ioctl = afs_unlocked_xioctl,
#else
  .ioctl =	afs_xioctl,
#endif
#ifdef HAVE_COMPAT_IOCTL
  .compat_ioctl = afs_unlocked_xioctl,
#endif
  .open =	afs_linux_open,
  .release =	afs_linux_release,
};

struct file_operations afs_file_fops = {
  .read =	afs_linux_read,
  .write =	afs_linux_write,
#ifdef HAVE_LINUX_GENERIC_FILE_AIO_READ
  .aio_read =	generic_file_aio_read,
  .aio_write =	generic_file_aio_write,
#endif
#ifdef HAVE_UNLOCKED_IOCTL
  .unlocked_ioctl = afs_unlocked_xioctl,
#else
  .ioctl =	afs_xioctl,
#endif
#ifdef HAVE_COMPAT_IOCTL
  .compat_ioctl = afs_unlocked_xioctl,
#endif
  .mmap =	afs_linux_mmap,
  .open =	afs_linux_open,
  .flush =	afs_linux_flush,
  .release =	afs_linux_release,
  .fsync =	afs_linux_fsync,
  .lock =	afs_linux_lock,
#ifdef STRUCT_FILE_OPERATIONS_HAS_FLOCK
  .flock =	afs_linux_flock,
#endif
};

static struct dentry *
canonical_dentry(struct inode *ip)
{
    struct vcache *vcp = VTOAFS(ip);
    struct dentry *first = NULL, *ret = NULL, *cur;
    struct list_head *head, *prev, *tmp;

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

    spin_lock(&dcache_lock);

    head = &ip->i_dentry;
    prev = ip->i_dentry.prev;

    while (prev != head) {
	tmp = prev;
	prev = tmp->prev;
	cur = list_entry(tmp, struct dentry, d_alias);

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
	dget_locked(ret);
    }
    spin_unlock(&dcache_lock);

    return ret;
}

/**********************************************************************
 * AFS Linux dentry operations
 **********************************************************************/

/* check_bad_parent() : Checks if this dentry's vcache is a root vcache
 * that has its mvid (parent dir's fid) pointer set to the wrong directory
 * due to being mounted in multiple points at once. If so, check_bad_parent()
 * calls afs_lookup() to correct the vcache's mvid, as well as the volume's
 * dotdotfid and mtpoint fid members.
 * Parameters:
 *   dp - dentry to be checked.
 * Return Values:
 *   None.
 * Sideeffects:
 *   This dentry's vcache's mvid will be set to the correct parent directory's
 *   fid.
 *   This root vnode's volume will have its dotdotfid and mtpoint fids set
 *   to the correct parent and mountpoint fids.
 */

static inline void
check_bad_parent(struct dentry *dp)
{
    cred_t *credp;
    struct vcache *vcp = VTOAFS(dp->d_inode), *avc = NULL;
    struct vcache *pvc = VTOAFS(dp->d_parent->d_inode);

    if (vcp->mvid->Fid.Volume != pvc->f.fid.Fid.Volume) {	/* bad parent */
	credp = crref();

	/* force a lookup, so vcp->mvid is fixed up */
	afs_lookup(pvc, (char *)dp->d_name.name, &avc, credp);
	if (!avc || vcp != avc) {	/* bad, very bad.. */
	    afs_Trace4(afs_iclSetp, CM_TRACE_TMP_1S3L, ICL_TYPE_STRING,
		       "check_bad_parent: bad pointer returned from afs_lookup origvc newvc dentry",
		       ICL_TYPE_POINTER, vcp, ICL_TYPE_POINTER, avc,
		       ICL_TYPE_POINTER, dp);
	}
	if (avc)
	    AFS_RELE(AFSTOV(avc));
	crfree(credp);
    }

    return;
}

/* afs_linux_revalidate
 * Ensure vcache is stat'd before use. Return 0 if entry is valid.
 */
static int
afs_linux_revalidate(struct dentry *dp)
{
    struct vattr vattr;
    struct vcache *vcp = VTOAFS(dp->d_inode);
    cred_t *credp;
    int code;

    if (afs_shuttingdown != AFS_RUNNING)
	return EIO;

#ifdef AFS_LINUX24_ENV
    maybe_lock_kernel();
#endif
    AFS_GLOCK();

#ifdef notyet
    /* Make this a fast path (no crref), since it's called so often. */
    if (vcp->f.states & CStatd) {

	if (*dp->d_name.name != '/' && vcp->mvstat == 2)	/* root vnode */
	    check_bad_parent(dp);	/* check and correct mvid */

	AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
	unlock_kernel();
#endif
	return 0;
    }
#endif

    /* This avoids the crref when we don't have to do it. Watch for
     * changes in afs_getattr that don't get replicated here!
     */
    if (vcp->f.states & CStatd &&
        (!afs_fakestat_enable || vcp->mvstat != 1) &&
	!afs_nfsexporter &&
	(vType(vcp) == VDIR || vType(vcp) == VLNK)) {
	code = afs_CopyOutAttrs(vcp, &vattr);
    } else {
        credp = crref();
	code = afs_getattr(vcp, &vattr, credp);
	crfree(credp);
    }
    if (!code)
        afs_fill_inode(AFSTOV(vcp), &vattr);

    AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
    maybe_unlock_kernel();
#endif

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
	vattrp->va_uid = iattrp->ia_uid;
    if (iattrp->ia_valid & ATTR_GID)
	vattrp->va_gid = iattrp->ia_gid;
    if (iattrp->ia_valid & ATTR_SIZE)
	vattrp->va_size = iattrp->ia_size;
    if (iattrp->ia_valid & ATTR_ATIME) {
	vattrp->va_atime.tv_sec = iattrp->ia_atime;
	vattrp->va_atime.tv_usec = 0;
    }
    if (iattrp->ia_valid & ATTR_MTIME) {
	vattrp->va_mtime.tv_sec = iattrp->ia_mtime;
	vattrp->va_mtime.tv_usec = 0;
    }
    if (iattrp->ia_valid & ATTR_CTIME) {
	vattrp->va_ctime.tv_sec = iattrp->ia_ctime;
	vattrp->va_ctime.tv_usec = 0;
    }
}

/* vattr2inode
 * Rewrite the inode cache from the attr. Assumes all vattr fields are valid.
 */
void
vattr2inode(struct inode *ip, struct vattr *vp)
{
    ip->i_ino = vp->va_nodeid;
    ip->i_nlink = vp->va_nlink;
    ip->i_blocks = vp->va_blocks;
#ifdef STRUCT_INODE_HAS_I_BLKBITS
    ip->i_blkbits = AFS_BLKBITS;
#endif
#ifdef STRUCT_INODE_HAS_I_BLKSIZE
    ip->i_blksize = vp->va_blocksize;
#endif
    ip->i_rdev = vp->va_rdev;
    ip->i_mode = vp->va_mode;
    ip->i_uid = vp->va_uid;
    ip->i_gid = vp->va_gid;
    i_size_write(ip, vp->va_size);
    ip->i_atime = vp->va_atime.tv_sec;
    ip->i_mtime = vp->va_mtime.tv_sec;
    ip->i_ctime = vp->va_ctime.tv_sec;
}

/* afs_notify_change
 * Linux version of setattr call. What to change is in the iattr struct.
 * We need to set bits in both the Linux inode as well as the vcache.
 */
int
afs_notify_change(struct dentry *dp, struct iattr *iattrp)
{
    struct vattr vattr;
    cred_t *credp = crref();
    struct inode *ip = dp->d_inode;
    int code;

    VATTR_NULL(&vattr);
    iattr2vattr(&vattr, iattrp);	/* Convert for AFS vnodeops call. */

    AFS_GLOCK();
    code = afs_setattr(VTOAFS(ip), &vattr, credp);
    if (!code) {
	afs_getattr(VTOAFS(ip), &vattr, credp);
	vattr2inode(ip, &vattr);
    }
    AFS_GUNLOCK();
    crfree(credp);
    return afs_convert_code(code);
}

/* Validate a dentry. Return 1 if unchanged, 0 if VFS layer should re-evaluate.
 * In kernels 2.2.10 and above, we are passed an additional flags var which
 * may have either the LOOKUP_FOLLOW OR LOOKUP_DIRECTORY set in which case
 * we are advised to follow the entry if it is a link or to make sure that 
 * it is a directory. But since the kernel itself checks these possibilities
 * later on, we shouldn't have to do it until later. Perhaps in the future..
 */
static int
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,10)
#ifdef DOP_REVALIDATE_TAKES_NAMEIDATA
afs_linux_dentry_revalidate(struct dentry *dp, struct nameidata *nd)
#else
afs_linux_dentry_revalidate(struct dentry *dp, int flags)
#endif
#else
afs_linux_dentry_revalidate(struct dentry *dp)
#endif
{
    struct vattr vattr;
    cred_t *credp = NULL;
    struct vcache *vcp, *pvcp, *tvc = NULL;
    int valid;
    struct afs_fakestat_state fakestate;

#ifdef AFS_LINUX24_ENV
    maybe_lock_kernel();
#endif
    AFS_GLOCK();
    afs_InitFakeStat(&fakestate);

    if (dp->d_inode) {

	vcp = VTOAFS(dp->d_inode);
	pvcp = VTOAFS(dp->d_parent->d_inode);		/* dget_parent()? */

	if (vcp == afs_globalVp)
	    goto good_dentry;

	if (vcp->mvstat == 1) {         /* mount point */
	    if (vcp->mvid && (vcp->f.states & CMValid)) {
		int tryEvalOnly = 0;
		int code = 0;
		struct vrequest treq;

		credp = crref();
		code = afs_InitReq(&treq, credp);
		if (
#ifdef AFS_DARWIN_ENV
		    (strcmp(dp->d_name.name, ".DS_Store") == 0) ||
		    (strcmp(dp->d_name.name, "Contents") == 0) ||
#endif
		    (strcmp(dp->d_name.name, ".directory") == 0)) {
		    tryEvalOnly = 1;
		}
		if (tryEvalOnly)
		    code = afs_TryEvalFakeStat(&vcp, &fakestate, &treq);
		else
		    code = afs_EvalFakeStat(&vcp, &fakestate, &treq);
		if ((tryEvalOnly && vcp->mvstat == 1) || code) {
		    /* a mount point, not yet replaced by its directory */
		    goto bad_dentry;
		}
	    }
	} else
	    if (*dp->d_name.name != '/' && vcp->mvstat == 2) /* root vnode */
		check_bad_parent(dp);	/* check and correct mvid */

#ifdef notdef
	/* If the last looker changes, we should make sure the current
	 * looker still has permission to examine this file.  This would
	 * always require a crref() which would be "slow".
	 */
	if (vcp->last_looker != treq.uid) {
	    if (!afs_AccessOK(vcp, (vType(vcp) == VREG) ? PRSFS_READ : PRSFS_LOOKUP, &treq, CHECK_MODE_BITS))
		goto bad_dentry;

	    vcp->last_looker = treq.uid;
	}
#endif

	/* If the parent's DataVersion has changed or the vnode
	 * is longer valid, we need to do a full lookup.  VerifyVCache
	 * isn't enough since the vnode may have been renamed.
	 */

	if (hgetlo(pvcp->f.m.DataVersion) > dp->d_time || !(vcp->f.states & CStatd)) {

	    credp = crref();
	    afs_lookup(pvcp, (char *)dp->d_name.name, &tvc, credp);
	    if (!tvc || tvc != vcp)
		goto bad_dentry;

	    if (afs_getattr(vcp, &vattr, credp))
		goto bad_dentry;

	    vattr2inode(AFSTOV(vcp), &vattr);
	    dp->d_time = hgetlo(pvcp->f.m.DataVersion);
	}

	/* should we always update the attributes at this point? */
	/* unlikely--the vcache entry hasn't changed */

    } else {
#ifdef notyet
	pvcp = VTOAFS(dp->d_parent->d_inode);		/* dget_parent()? */
	if (hgetlo(pvcp->f.m.DataVersion) > dp->d_time)
	    goto bad_dentry;
#endif

	/* No change in parent's DataVersion so this negative
	 * lookup is still valid.  BUT, if a server is down a
	 * negative lookup can result so there should be a
	 * liftime as well.  For now, always expire.
	 */

	goto bad_dentry;
    }

  good_dentry:
    valid = 1;

  done:
    /* Clean up */
    if (tvc)
	afs_PutVCache(tvc);
    afs_PutFakeStat(&fakestate);
    AFS_GUNLOCK();
    if (credp)
	crfree(credp);

    if (!valid) {
	shrink_dcache_parent(dp);
	d_drop(dp);
    }
#ifdef AFS_LINUX24_ENV
    maybe_unlock_kernel();
#endif
    return valid;

  bad_dentry:
    if (have_submounts(dp))
	valid = 1;
    else 
	valid = 0;
    goto done;
}

static void
afs_dentry_iput(struct dentry *dp, struct inode *ip)
{
    struct vcache *vcp = VTOAFS(ip);

    AFS_GLOCK();
    if (!AFS_IS_DISCONNECTED || (vcp->f.states & CUnlinked)) {
	(void) afs_InactiveVCache(vcp, NULL);
    }
    AFS_GUNLOCK();
#ifdef DCACHE_NFSFS_RENAMED
    dp->d_flags &= ~DCACHE_NFSFS_RENAMED;   
#endif

    iput(ip);
}

static int
afs_dentry_delete(struct dentry *dp)
{
    if (dp->d_inode && (VTOAFS(dp->d_inode)->f.states & CUnlinked))
	return 1;		/* bad inode? */

    return 0;
}

struct dentry_operations afs_dentry_operations = {
  .d_revalidate =	afs_linux_dentry_revalidate,
  .d_delete =		afs_dentry_delete,
  .d_iput =		afs_dentry_iput,
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
static int
#ifdef IOP_CREATE_TAKES_NAMEIDATA
afs_linux_create(struct inode *dip, struct dentry *dp, int mode,
		 struct nameidata *nd)
#else
afs_linux_create(struct inode *dip, struct dentry *dp, int mode)
#endif
{
    struct vattr vattr;
    cred_t *credp = crref();
    const char *name = dp->d_name.name;
    struct vcache *vcp;
    int code;

    VATTR_NULL(&vattr);
    vattr.va_mode = mode;
    vattr.va_type = mode & S_IFMT;

    AFS_GLOCK();
    code = afs_create(VTOAFS(dip), (char *)name, &vattr, NONEXCL, mode,
		      &vcp, credp);

    if (!code) {
	struct inode *ip = AFSTOV(vcp);

	afs_getattr(vcp, &vattr, credp);
	afs_fill_inode(ip, &vattr);
	insert_inode_hash(ip);
	dp->d_op = &afs_dentry_operations;
	dp->d_time = hgetlo(VTOAFS(dip)->f.m.DataVersion);
	d_instantiate(dp, ip);
    }
    AFS_GUNLOCK();

    crfree(credp);
    return afs_convert_code(code);
}

/* afs_linux_lookup */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,10)
static struct dentry *
#ifdef IOP_LOOKUP_TAKES_NAMEIDATA
afs_linux_lookup(struct inode *dip, struct dentry *dp,
		 struct nameidata *nd)
#else
afs_linux_lookup(struct inode *dip, struct dentry *dp)
#endif
#else
static int
afs_linux_lookup(struct inode *dip, struct dentry *dp)
#endif
{
    cred_t *credp = crref();
    struct vcache *vcp = NULL;
    const char *comp = dp->d_name.name;
    struct inode *ip = NULL;
    int code;

    AFS_GLOCK();
    code = afs_lookup(VTOAFS(dip), (char *)comp, &vcp, credp);
    
    if (vcp) {
	struct vattr vattr;
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

	ip = AFSTOV(vcp);
	afs_getattr(vcp, &vattr, credp);
	afs_fill_inode(ip, &vattr);
	if (
#ifdef HAVE_LINUX_HLIST_UNHASHED
	    hlist_unhashed(&ip->i_hash)
#else
	    ip->i_hash.prev == NULL
#endif
	    )
	    insert_inode_hash(ip);
    }
    dp->d_op = &afs_dentry_operations;
    dp->d_time = hgetlo(VTOAFS(dip)->f.m.DataVersion);
    AFS_GUNLOCK();

#if defined(AFS_LINUX24_ENV)
    if (ip && S_ISDIR(ip->i_mode)) {
	d_prune_aliases(ip);
    }
#endif
    d_add(dp, ip);

 done:
    crfree(credp);

    /* It's ok for the file to not be found. That's noted by the caller by
     * seeing that the dp->d_inode field is NULL.
     */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,10)
    if (code == ENOENT)
	return ERR_PTR(0);
    else 
	return ERR_PTR(afs_convert_code(code));
#else
    if (code == ENOENT)
	code = 0;
    return afs_convert_code(code);
#endif
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

static int
afs_linux_unlink(struct inode *dip, struct dentry *dp)
{
    int code = EBUSY;
    cred_t *credp = crref();
    const char *name = dp->d_name.name;
    struct vcache *tvc = VTOAFS(dp->d_inode);

    if (VREFCOUNT(tvc) > 1 && tvc->opens > 0
				&& !(tvc->f.states & CUnlinked)) {
	struct dentry *__dp;
	char *__name;

	__dp = NULL;
	__name = NULL;
	do {
	    dput(__dp);

	    AFS_GLOCK();
	    if (__name)
		osi_FreeSmallSpace(__name);
	    __name = afs_newname();
	    AFS_GUNLOCK();

	    __dp = lookup_one_len(__name, dp->d_parent, strlen(__name));
		
	    if (IS_ERR(__dp))
		goto out;
	} while (__dp->d_inode != NULL);

	AFS_GLOCK();
	code = afs_rename(VTOAFS(dip), (char *)dp->d_name.name, VTOAFS(dip), (char *)__dp->d_name.name, credp);
	if (!code) {
            tvc->mvid = (void *) __name;
            crhold(credp);
            if (tvc->uncred) {
                crfree(tvc->uncred);
            }
            tvc->uncred = credp;
	    tvc->f.states |= CUnlinked;
#ifdef DCACHE_NFSFS_RENAMED
	    dp->d_flags |= DCACHE_NFSFS_RENAMED;   
#endif
	} else {
	    osi_FreeSmallSpace(__name);	
	}
	AFS_GUNLOCK();

	if (!code) {
	    __dp->d_time = hgetlo(VTOAFS(dip)->f.m.DataVersion);
	    d_move(dp, __dp);
	}
	dput(__dp);

	goto out;
    }

    AFS_GLOCK();
    code = afs_remove(VTOAFS(dip), (char *)name, credp);
    AFS_GUNLOCK();
    if (!code)
	d_drop(dp);
out:
    crfree(credp);
    return afs_convert_code(code);
}


static int
afs_linux_symlink(struct inode *dip, struct dentry *dp, const char *target)
{
    int code;
    cred_t *credp = crref();
    struct vattr vattr;
    const char *name = dp->d_name.name;

    /* If afs_symlink returned the vnode, we could instantiate the
     * dentry. Since it's not, we drop this one and do a new lookup.
     */
    d_drop(dp);

    VATTR_NULL(&vattr);
    AFS_GLOCK();
    code = afs_symlink(VTOAFS(dip), (char *)name, &vattr, (char *)target, NULL,
    		       credp);
    AFS_GUNLOCK();
    crfree(credp);
    return afs_convert_code(code);
}

static int
afs_linux_mkdir(struct inode *dip, struct dentry *dp, int mode)
{
    int code;
    cred_t *credp = crref();
    struct vcache *tvcp = NULL;
    struct vattr vattr;
    const char *name = dp->d_name.name;

    VATTR_NULL(&vattr);
    vattr.va_mask = ATTR_MODE;
    vattr.va_mode = mode;
    AFS_GLOCK();
    code = afs_mkdir(VTOAFS(dip), (char *)name, &vattr, &tvcp, credp);

    if (tvcp) {
	struct inode *ip = AFSTOV(tvcp);

	afs_getattr(tvcp, &vattr, credp);
	afs_fill_inode(ip, &vattr);

	dp->d_op = &afs_dentry_operations;
	dp->d_time = hgetlo(VTOAFS(dip)->f.m.DataVersion);
	d_instantiate(dp, ip);
    }
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


static int
afs_linux_rename(struct inode *oldip, struct dentry *olddp,
		 struct inode *newip, struct dentry *newdp)
{
    int code;
    cred_t *credp = crref();
    const char *oldname = olddp->d_name.name;
    const char *newname = newdp->d_name.name;
    struct dentry *rehash = NULL;

    if (!list_empty(&newdp->d_hash)) {
	d_drop(newdp);
	rehash = newdp;
    }

#if defined(AFS_LINUX24_ENV)
    if (atomic_read(&olddp->d_count) > 1)
	shrink_dcache_parent(olddp);
#endif

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
#if defined(AFS_LINUX24_ENV)
static int afs_linux_follow_link(struct dentry *dentry, struct nameidata *nd)
{
    int code;
    char *name;

    name = osi_Alloc(PATH_MAX);
    if (!name) {
	return -EIO;
    }

    AFS_GLOCK();
    code = afs_linux_ireadlink(dentry->d_inode, name, PATH_MAX - 1, AFS_UIOSYS);
    AFS_GUNLOCK();

    if (code < 0) {
	goto out;
    }

    name[code] = '\0';
    code = vfs_follow_link(nd, name);

out:
    osi_Free(name, PATH_MAX);

    return code;
}

#else /* !defined(AFS_LINUX24_ENV) */

static struct dentry *
afs_linux_follow_link(struct dentry *dp, struct dentry *basep,
		      unsigned int follow)
{
    int code = 0;
    char *name;
    struct dentry *res;


    AFS_GLOCK();
    name = osi_Alloc(PATH_MAX + 1);
    if (!name) {
	AFS_GUNLOCK();
	dput(basep);
	return ERR_PTR(-EIO);
    }

    code = afs_linux_ireadlink(dp->d_inode, name, PATH_MAX, AFS_UIOSYS);
    AFS_GUNLOCK();

    if (code < 0) {
	dput(basep);
	if (code < -MAX_ERRNO)
	    res = ERR_PTR(-EIO);
	else
	    res = ERR_PTR(code);
    } else {
	name[code] = '\0';
	res = lookup_dentry(name, basep, follow);
    }

    AFS_GLOCK();
    osi_Free(name, PATH_MAX + 1);
    AFS_GUNLOCK();
    return res;
}
#endif /* AFS_LINUX24_ENV */
#endif /* USABLE_KERNEL_PAGE_SYMLINK_CACHE */

static inline int
afs_linux_can_bypass(struct inode *ip) {
    switch(cache_bypass_strategy) {
	case NEVER_BYPASS_CACHE:
	    return 0;
	case ALWAYS_BYPASS_CACHE:
	    return 1;
	case LARGE_FILES_BYPASS_CACHE:
	    if(i_size_read(ip) > cache_bypass_threshold)
		return 1;
	default:
	    return 0;
     }
}

/* afs_linux_readpage
 * all reads come through here. A strategy-like read call.
 */
static int
afs_linux_readpage(struct file *fp, struct page *pp)
{
    afs_int32 code;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    char *address;
    afs_offs_t offset = ((loff_t) pp->index) << PAGE_CACHE_SHIFT;
#else
    ulong address = afs_linux_page_address(pp);
    afs_offs_t offset = pageoff(pp);
#endif
    afs_int32 bypasscache = 0; /* bypass for this read */
    struct nocache_read_request *ancr;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    afs_int32 isize;
#endif
    struct uio *auio;
    struct iovec *iovecp;
    struct inode *ip = FILE_INODE(fp);
    afs_int32 cnt = page_count(pp);
    struct vcache *avc = VTOAFS(ip);
    cred_t *credp;

    credp = crref();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    address = kmap(pp);
    ClearPageError(pp);
#else
    atomic_add(1, &pp->count);
    set_bit(PG_locked, &pp->flags);	/* other bits? See mm.h */
    clear_bit(PG_error, &pp->flags);
#endif
    /* if bypasscache, receiver frees, else we do */
    auio = osi_Alloc(sizeof(struct uio));
    iovecp = osi_Alloc(sizeof(struct iovec));

    setup_uio(auio, iovecp, (char *)address, offset, PAGE_SIZE, UIO_READ,
	      AFS_UIOSYS);

    bypasscache = afs_linux_can_bypass(ip);

    /* In the new incarnation of selective caching, a file's caching policy
     * can change, eg because file size exceeds threshold, etc. */
    trydo_cache_transition(avc, credp, bypasscache);
	
    if(bypasscache) {
	if(address)
	    kunmap(pp);
	/* save the page for background map */
	auio->uio_iov->iov_base = (void*) pp;
	/* the background thread will free this */
	ancr = osi_Alloc(sizeof(struct nocache_read_request));
	ancr->auio = auio;
	ancr->offset = offset;
	ancr->length = PAGE_SIZE;

	maybe_lock_kernel();
	code = afs_ReadNoCache(avc, ancr, credp);
	maybe_unlock_kernel();

	goto done; /* skips release page, doing it in bg thread */
    }
		  
#ifdef AFS_LINUX24_ENV
    maybe_lock_kernel();
#endif
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
#ifdef AFS_LINUX24_ENV
    maybe_unlock_kernel();
#endif
    if (!code) {
	/* XXX valid for no-cache also?  Check last bits of files... :)
	 * Cognate code goes in afs_NoCacheFetchProc.  */
	if (auio->uio_resid)	/* zero remainder of page */
	     memset((void *)(address + (PAGE_SIZE - auio->uio_resid)), 0,
		    auio->uio_resid);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	flush_dcache_page(pp);
	SetPageUptodate(pp);
#else
	set_bit(PG_uptodate, &pp->flags);
#endif
    } /* !code */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    kunmap(pp);
    UnlockPage(pp);
#else
    clear_bit(PG_locked, &pp->flags);
    wake_up(&pp->wait);
    free_page(address);
#endif

    /* do not call afs_GetDCache if cache is bypassed */
    if(bypasscache)
	goto done;

    /* free if not bypassing cache */
    osi_Free(auio, sizeof(struct uio));
    osi_Free(iovecp, sizeof(struct iovec));

    if (!code && AFS_CHUNKOFFSET(offset) == 0) {
	struct dcache *tdc;
	struct vrequest treq;

	AFS_GLOCK();
	code = afs_InitReq(&treq, credp);
	if (!code && !NBObtainWriteLock(&avc->lock, 534)) {
	    tdc = afs_FindDCache(avc, offset);
	    if (tdc) {
		if (!(tdc->mflags & DFNextStarted))
		    afs_PrefetchChunk(avc, tdc, credp, &treq);
		    afs_PutDCache(tdc);
	    }
	    ReleaseWriteLock(&avc->lock);
	}
	AFS_GUNLOCK();
    }

done:
    crfree(credp);
    return afs_convert_code(code);
}

#if defined(AFS_LINUX24_ENV)
static int
afs_linux_writepage_sync(struct inode *ip, struct page *pp,
			 unsigned long offset, unsigned int count)
{
    struct vcache *vcp = VTOAFS(ip);
    char *buffer;
    afs_offs_t base;
    int code = 0;
    cred_t *credp;
    struct uio tuio;
    struct iovec iovec;
    int f_flags = 0;

    memset(&tuio, 0, sizeof(tuio));
    memset(&iovec, 0, sizeof(iovec));

    buffer = kmap(pp) + offset;
    base = (((loff_t) pp->index) << PAGE_CACHE_SHIFT)  + offset;

    credp = crref();
    maybe_lock_kernel();
    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_UPDATEPAGE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, page_count(pp),
	       ICL_TYPE_INT32, 99999);

    ObtainWriteLock(&vcp->lock, 532);
    if (vcp->f.states & CPageWrite) {
	ReleaseWriteLock(&vcp->lock);
	AFS_GUNLOCK();
	maybe_unlock_kernel();
	crfree(credp);
	kunmap(pp);
	/* should mark it dirty? */
	return(0); 
    }
    vcp->f.states |= CPageWrite;
    ReleaseWriteLock(&vcp->lock);

    setup_uio(&tuio, &iovec, buffer, base, count, UIO_WRITE, AFS_UIOSYS);

    code = afs_write(vcp, &tuio, f_flags, credp, 0);

    i_size_write(ip, vcp->f.m.Length);
    ip->i_blocks = ((vcp->f.m.Length + 1023) >> 10) << 1;

    ObtainWriteLock(&vcp->lock, 533);
    if (!code) {
	struct vrequest treq;

	if (!afs_InitReq(&treq, credp))
	    code = afs_DoPartialWrite(vcp, &treq);
    }
    code = code ? afs_convert_code(code) : count - tuio.uio_resid;

    vcp->f.states &= ~CPageWrite;
    ReleaseWriteLock(&vcp->lock);

    afs_Trace4(afs_iclSetp, CM_TRACE_UPDATEPAGE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, page_count(pp),
	       ICL_TYPE_INT32, code);

    AFS_GUNLOCK();
    maybe_unlock_kernel();
    crfree(credp);
    kunmap(pp);

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
    unsigned long end_index;
    unsigned offset = PAGE_CACHE_SIZE;
    long status;

    if (PageLaunder(pp)) {
	return(fail_writepage(pp));
    }

    inode = (struct inode *)mapping->host;
    end_index = i_size_read(inode) >> PAGE_CACHE_SHIFT;

    /* easy case */
    if (pp->index < end_index)
	goto do_it;
    /* things got complicated... */
    offset = i_size_read(inode) & (PAGE_CACHE_SIZE - 1);
    /* OK, are we completely out? */
    if (pp->index >= end_index + 1 || !offset)
	return -EIO;
  do_it:
    status = afs_linux_writepage_sync(inode, pp, 0, offset);
    SetPageUptodate(pp);
    UnlockPage(pp);
    if (status == offset)
	return 0;
    else
	return status;
}
#else
/* afs_linux_updatepage
 * What one would have thought was writepage - write dirty page to file.
 * Called from generic_file_write. buffer is still in user space. pagep
 * has been filled in with old data if we're updating less than a page.
 */
static int
afs_linux_updatepage(struct file *fp, struct page *pp, unsigned long offset,
		     unsigned int count, int sync)
{
    struct vcache *vcp = VTOAFS(FILE_INODE(fp));
    u8 *page_addr = (u8 *) afs_linux_page_address(pp);
    int code = 0;
    cred_t *credp;
    struct uio tuio;
    struct iovec iovec;

    memset(&tuio, 0, sizeof(tuio));
    memset(&iovec, 0, sizeof(iovec));

    set_bit(PG_locked, &pp->flags);

    credp = crref();
    AFS_GLOCK();
    AFS_DISCON_LOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_UPDATEPAGE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, page_count(pp),
	       ICL_TYPE_INT32, 99999);
    setup_uio(&tuio, &iovec, page_addr + offset,
	      (afs_offs_t) (pageoff(pp) + offset), count, UIO_WRITE,
	      AFS_UIOSYS);

    code = afs_write(vcp, &tuio, fp->f_flags, credp, 0);

    i_size_write(ip, vcp->f.m.Length);
    ip->i_blocks = ((vcp->f.m.Length + 1023) >> 10) << 1;

    if (!code) {
	struct vrequest treq;

	ObtainWriteLock(&vcp->lock, 533);
	vcp->f.m.Date = osi_Time();   /* set modification time */
	if (!afs_InitReq(&treq, credp))
	    code = afs_DoPartialWrite(vcp, &treq);
	ReleaseWriteLock(&vcp->lock);
    }

    code = code ? afs_convert_code(code) : count - tuio.uio_resid;
    afs_Trace4(afs_iclSetp, CM_TRACE_UPDATEPAGE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, page_count(pp),
	       ICL_TYPE_INT32, code);

    AFS_DISCON_UNLOCK();
    AFS_GUNLOCK();
    crfree(credp);

    clear_bit(PG_locked, &pp->flags);
    return code;
}
#endif

/* afs_linux_permission
 * Check access rights - returns error if can't check or permission denied.
 */
static int
#ifdef IOP_PERMISSION_TAKES_NAMEIDATA
afs_linux_permission(struct inode *ip, int mode, struct nameidata *nd)
#else
afs_linux_permission(struct inode *ip, int mode)
#endif
{
    int code;
    cred_t *credp = crref();
    int tmp = 0;

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

#if defined(AFS_LINUX24_ENV) && !defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_WRITE_BEGIN)
static int
afs_linux_commit_write(struct file *file, struct page *page, unsigned offset,
		       unsigned to)
{
    int code;

    code = afs_linux_writepage_sync(file->f_dentry->d_inode, page,
                                    offset, to - offset);
    kunmap(page);

    return code;
}

static int
afs_linux_prepare_write(struct file *file, struct page *page, unsigned from,
			unsigned to)
{
/* sometime between 2.4.0 and 2.4.19, the callers of prepare_write began to
   call kmap directly instead of relying on us to do it */
    kmap(page);
    return 0;
}
#endif

#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_WRITE_BEGIN)
static int
afs_linux_write_end(struct file *file, struct address_space *mapping,
                                loff_t pos, unsigned len, unsigned copied,
                                struct page *page, void *fsdata)
{
    int code;
    unsigned from = pos & (PAGE_CACHE_SIZE - 1);

    code = afs_linux_writepage_sync(file->f_dentry->d_inode, page,
                                    from, copied);
    unlock_page(page);
    page_cache_release(page);
    return code;
}

static int
afs_linux_write_begin(struct file *file, struct address_space *mapping,
                                loff_t pos, unsigned len, unsigned flags,
                                struct page **pagep, void **fsdata)
{
    struct page *page;
    pgoff_t index = pos >> PAGE_CACHE_SHIFT;
#if defined(HAVE_LINUX_GRAB_CACHE_PAGE_WRITE_BEGIN)
    page = grab_cache_page_write_begin(mapping, index, flags);
#else
    page = __grab_cache_page(mapping, index);
#endif
    *pagep = page;

    return 0;
}
#endif

static int
afs_linux_dir_follow_link(struct dentry *dentry, struct nameidata *nd)
{
    struct dentry **dpp;
    struct dentry *target;

    target = canonical_dentry(dentry->d_inode);

    dpp = &nd->dentry;

    dput(*dpp);

    if (target) {
	*dpp = target;
    } else {
	*dpp = dget(dentry);
    }

    nd->last_type = LAST_BIND;

    return 0;
}

static struct inode_operations afs_file_iops = {
#if defined(AFS_LINUX24_ENV)
  .permission =		afs_linux_permission,
  .revalidate =		afs_linux_revalidate,
  .setattr =		afs_notify_change,
#else
  .default_file_ops =	&afs_file_fops,
  .readpage =		afs_linux_readpage,  
  .revalidate =		afs_linux_revalidate,
  .updatepage =		afs_linux_updatepage,
#endif
};

#if defined(AFS_LINUX24_ENV)
static struct address_space_operations afs_file_aops = {
  .readpage =		afs_linux_readpage,
  .writepage =		afs_linux_writepage,
#if defined (STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_WRITE_BEGIN)
  .write_begin =        afs_linux_write_begin,
  .write_end =          afs_linux_write_end,
#else
  .commit_write =       afs_linux_commit_write,
  .prepare_write =      afs_linux_prepare_write,
#endif
};
#endif


/* Separate ops vector for directories. Linux 2.2 tests type of inode
 * by what sort of operation is allowed.....
 */

static struct inode_operations afs_dir_iops = {
#if !defined(AFS_LINUX24_ENV)
  .default_file_ops =	&afs_dir_fops,
#else
  .setattr =		afs_notify_change,
#endif
  .create =		afs_linux_create,
  .lookup =		afs_linux_lookup,
  .link =		afs_linux_link,
  .unlink =		afs_linux_unlink,
  .symlink =		afs_linux_symlink,
  .mkdir =		afs_linux_mkdir,
  .rmdir =		afs_linux_rmdir,
  .rename =		afs_linux_rename,
  .revalidate =		afs_linux_revalidate,
  .permission =		afs_linux_permission,
  .follow_link =	afs_linux_dir_follow_link,
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

    maybe_lock_kernel();
    AFS_GLOCK();
    code = afs_linux_ireadlink(ip, p, PAGE_SIZE, AFS_UIOSYS);
    AFS_GUNLOCK();

    if (code < 0)
	goto fail;
    p[code] = '\0';		/* null terminate? */
    maybe_unlock_kernel();

    SetPageUptodate(page);
    kunmap(page);
    UnlockPage(page);
    return 0;

  fail:
    maybe_unlock_kernel();

    SetPageError(page);
    kunmap(page);
    UnlockPage(page);
    return code;
}

static struct address_space_operations afs_symlink_aops = {
  .readpage =	afs_symlink_filler
};
#endif	/* USABLE_KERNEL_PAGE_SYMLINK_CACHE */

static struct inode_operations afs_symlink_iops = {
#if defined(USABLE_KERNEL_PAGE_SYMLINK_CACHE)
  .readlink = 		page_readlink,
#if defined(HAVE_KERNEL_PAGE_FOLLOW_LINK)
  .follow_link =	page_follow_link,
#else
  .follow_link =	page_follow_link_light,
  .put_link =           page_put_link,
#endif
#else /* !defined(USABLE_KERNEL_PAGE_SYMLINK_CACHE) */
  .readlink = 		afs_linux_readlink,
  .follow_link =	afs_linux_follow_link,
#if !defined(AFS_LINUX24_ENV)
  .permission =		afs_linux_permission,
  .revalidate =		afs_linux_revalidate,
#endif
#endif /* USABLE_KERNEL_PAGE_SYMLINK_CACHE */
#if defined(AFS_LINUX24_ENV)
  .setattr =		afs_notify_change,
#endif
};

void
afs_fill_inode(struct inode *ip, struct vattr *vattr)
{
	
    if (vattr)
	vattr2inode(ip, vattr);

/* Reset ops if symlink or directory. */
    if (S_ISREG(ip->i_mode)) {
	ip->i_op = &afs_file_iops;
#if defined(AFS_LINUX24_ENV)
	ip->i_fop = &afs_file_fops;
	ip->i_data.a_ops = &afs_file_aops;
#endif

    } else if (S_ISDIR(ip->i_mode)) {
	ip->i_op = &afs_dir_iops;
#if defined(AFS_LINUX24_ENV)
	ip->i_fop = &afs_dir_fops;
#endif

    } else if (S_ISLNK(ip->i_mode)) {
	ip->i_op = &afs_symlink_iops;
#if defined(USABLE_KERNEL_PAGE_SYMLINK_CACHE)
	ip->i_data.a_ops = &afs_symlink_aops;
	ip->i_mapping = &ip->i_data;
#endif
    }

}
