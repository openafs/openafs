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
#ifdef HAVE_MM_INLINE_H
#include <linux/mm_inline.h>
#endif
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/pagevec.h>
#include "afs/lock.h"
#include "afs/afs_bypasscache.h"

#include "osi_compat.h"
#include "osi_pagecopy.h"

#ifndef HAVE_LINUX_PAGEVEC_LRU_ADD_FILE
#define __pagevec_lru_add_file __pagevec_lru_add
#endif

#ifndef MAX_ERRNO
#define MAX_ERRNO 1000L
#endif

extern struct backing_dev_info *afs_backing_dev_info;

extern struct vcache *afs_globalVp;

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
	   && hsame(avc->f.m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseWriteLock(&avc->lock);
	afs_osi_Sleep(&tdc->validPos);
	ObtainWriteLock(&avc->lock, 812);
	ObtainReadLock(&tdc->lock);
    }
    if (!(avc->f.states & CStatd)
	|| !hsame(avc->f.m.DataVersion, tdc->f.versionNo)) {
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
    offset = (int) fp->f_pos;
    while (1) {
	dirpos = BlobScan(tdc, offset);
	if (!dirpos)
	    break;

	code = afs_dir_GetVerifiedBlob(tdc, dirpos, &de);
	if (code) {
	    afs_warn("Corrupt directory (inode %lx, dirpos %d)",
		     (unsigned long)&tdc->f.inode, dirpos);
	    ReleaseSharedLock(&avc->lock);
	    afs_PutDCache(tdc);
	    code = -ENOENT;
	    goto out;
        }

	ino = afs_calc_inum(avc->f.fid.Cell, avc->f.fid.Fid.Volume,
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
    mutex_lock(&ip->i_mutex);
#endif
    AFS_GLOCK();
    code = afs_fsync(VTOAFS(ip), credp);
    AFS_GUNLOCK();
#if defined(FOP_FSYNC_TAKES_RANGE)
    mutex_unlock(&ip->i_mutex);
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

    if ((code == 0 || flp->fl_type == F_UNLCK) && 
        (cmd == F_SETLK || cmd == F_SETLKW)) {
	code = afs_posix_lock_file(fp, flp);
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
        afs_posix_test_lock(fp, flp);
        /* If we found a lock in the kernel's structure, return it */
        if (flp->fl_type != F_UNLCK) {
            crfree(credp);
            return 0;
        }
    }
    
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
    int bypasscache = 0;

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

struct file_operations afs_dir_fops = {
  .read =	generic_read_dir,
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
  .llseek =	default_llseek,
#ifdef HAVE_LINUX_NOOP_FSYNC
  .fsync =	noop_fsync,
#else
  .fsync =	simple_sync_file,
#endif
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
#if defined(STRUCT_FILE_OPERATIONS_HAS_SENDFILE)
  .sendfile =   generic_file_sendfile,
#endif
#if defined(STRUCT_FILE_OPERATIONS_HAS_SPLICE)
  .splice_write = generic_file_splice_write,
  .splice_read = generic_file_splice_read,
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
#if defined(D_ALIAS_IS_HLIST)
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

# ifdef HAVE_DCACHE_LOCK
    spin_lock(&dcache_lock);
# else
    spin_lock(&ip->i_lock);
# endif

#if defined(D_ALIAS_IS_HLIST)
    hlist_for_each_entry(cur, p, &ip->i_dentry, d_alias) {
#else
    list_for_each_entry_reverse(cur, &ip->i_dentry, d_alias) {
#endif

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

# ifdef HAVE_DCACHE_LOCK
    if (ret) {
	dget_locked(ret);
    }
    spin_unlock(&dcache_lock);
# else
    if (ret) {
	dget(ret);
    }
    spin_unlock(&ip->i_lock);
# endif

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
    struct dentry *parent;
    struct vcache *vcp, *pvc, *avc = NULL;

    vcp = VTOAFS(dp->d_inode);
    parent = dget_parent(dp);
    pvc = VTOAFS(parent->d_inode);

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

    dput(parent);

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

    if (afs_shuttingdown)
	return EIO;

    AFS_GLOCK();

#ifdef notyet
    /* Make this a fast path (no crref), since it's called so often. */
    if (vcp->f.states & CStatd) {

	if (*dp->d_name.name != '/' && vcp->mvstat == 2)	/* root vnode */
	    check_bad_parent(dp);	/* check and correct mvid */

	AFS_GUNLOCK();
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
	vattrp->va_atime.tv_sec = iattrp->ia_atime.tv_sec;
	vattrp->va_atime.tv_usec = 0;
    }
    if (iattrp->ia_valid & ATTR_MTIME) {
	vattrp->va_mtime.tv_sec = iattrp->ia_mtime.tv_sec;
	vattrp->va_mtime.tv_usec = 0;
    }
    if (iattrp->ia_valid & ATTR_CTIME) {
	vattrp->va_ctime.tv_sec = iattrp->ia_ctime.tv_sec;
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
    ip->i_uid = vp->va_uid;
    ip->i_gid = vp->va_gid;
    i_size_write(ip, vp->va_size);
    ip->i_atime.tv_sec = vp->va_atime.tv_sec;
    ip->i_atime.tv_nsec = 0;
    ip->i_mtime.tv_sec = vp->va_mtime.tv_sec;
    /* Set the mtime nanoseconds to the sysname generation number.
     * This convinces NFS clients that all directories have changed
     * any time the sysname list changes.
     */
    ip->i_mtime.tv_nsec = afs_sysnamegen;
    ip->i_ctime.tv_sec = vp->va_ctime.tv_sec;
    ip->i_ctime.tv_nsec = 0;
}

/* afs_notify_change
 * Linux version of setattr call. What to change is in the iattr struct.
 * We need to set bits in both the Linux inode as well as the vcache.
 */
static int
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

static int
afs_linux_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
        int err = afs_linux_revalidate(dentry);
        if (!err) {
                generic_fillattr(dentry->d_inode, stat);
}
        return err;
}

/* Validate a dentry. Return 1 if unchanged, 0 if VFS layer should re-evaluate.
 * In kernels 2.2.10 and above, we are passed an additional flags var which
 * may have either the LOOKUP_FOLLOW OR LOOKUP_DIRECTORY set in which case
 * we are advised to follow the entry if it is a link or to make sure that 
 * it is a directory. But since the kernel itself checks these possibilities
 * later on, we shouldn't have to do it until later. Perhaps in the future..
 */
static int
#if defined(DOP_REVALIDATE_TAKES_UNSIGNED)
afs_linux_dentry_revalidate(struct dentry *dp, unsigned int flags)
#elif defined(DOP_REVALIDATE_TAKES_NAMEIDATA)
afs_linux_dentry_revalidate(struct dentry *dp, struct nameidata *nd)
#else
afs_linux_dentry_revalidate(struct dentry *dp, int flags)
#endif
{
    struct vattr vattr;
    cred_t *credp = NULL;
    struct vcache *vcp, *pvcp, *tvc = NULL;
    struct dentry *parent;
    int valid;
    struct afs_fakestat_state fakestate;

#ifdef LOOKUP_RCU
    /* We don't support RCU path walking */
# if defined(DOP_REVALIDATE_TAKES_UNSIGNED)
    if (flags & LOOKUP_RCU)
# else
    if (nd->flags & LOOKUP_RCU)
# endif
       return -ECHILD;
#endif
    AFS_GLOCK();

    afs_InitFakeStat(&fakestate);

    if (dp->d_inode) {
	vcp = VTOAFS(dp->d_inode);

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

	parent = dget_parent(dp);
	pvcp = VTOAFS(parent->d_inode);

	/* If the parent's DataVersion has changed or the vnode
	 * is longer valid, we need to do a full lookup.  VerifyVCache
	 * isn't enough since the vnode may have been renamed.
	 */

	if (hgetlo(pvcp->f.m.DataVersion) > dp->d_time || !(vcp->f.states & CStatd)) {

	    credp = crref();
	    afs_lookup(pvcp, (char *)dp->d_name.name, &tvc, credp);
	    if (!tvc || tvc != vcp) {
		dput(parent);
		goto bad_dentry;
	    }

	    if (afs_getattr(vcp, &vattr, credp)) {
		dput(parent);
		goto bad_dentry;
	    }

	    vattr2inode(AFSTOV(vcp), &vattr);
	    dp->d_time = hgetlo(pvcp->f.m.DataVersion);
	}

	/* should we always update the attributes at this point? */
	/* unlikely--the vcache entry hasn't changed */

	dput(parent);
    } else {
#ifdef notyet
	/* If this code is ever enabled, we should use dget_parent to handle
	 * getting the parent, and dput() to dispose of it. See above for an
	 * example ... */
	pvcp = VTOAFS(dp->d_parent->d_inode);
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
afs_dentry_automount(struct path *path)
{
    struct dentry *target;

    /* avoid symlink resolution limits when resolving; we cannot contribute to
     * an infinite symlink loop */
    current->total_link_count--;

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
static int
#if defined(IOP_CREATE_TAKES_BOOL)
afs_linux_create(struct inode *dip, struct dentry *dp, umode_t mode,
		 bool excl)
#elif defined(IOP_CREATE_TAKES_UMODE_T)
afs_linux_create(struct inode *dip, struct dentry *dp, umode_t mode,
		 struct nameidata *nd)
#elif defined(IOP_CREATE_TAKES_NAMEIDATA)
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
#if !defined(STRUCT_SUPER_BLOCK_HAS_S_D_OP)
	dp->d_op = &afs_dentry_operations;
#endif
	dp->d_time = hgetlo(VTOAFS(dip)->f.m.DataVersion);
	d_instantiate(dp, ip);
    }
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
	if (hlist_unhashed(&ip->i_hash))
	    insert_inode_hash(ip);
    }
#if !defined(STRUCT_SUPER_BLOCK_HAS_S_D_OP)
    dp->d_op = &afs_dentry_operations;
#endif
    dp->d_time = hgetlo(VTOAFS(dip)->f.m.DataVersion);
    AFS_GUNLOCK();

    if (ip && S_ISDIR(ip->i_mode)) {
	int retry = 1;
	struct dentry *alias;

	while (retry) {
	    retry = 0;

	    /* Try to invalidate an existing alias in favor of our new one */
	    alias = d_find_alias(ip);
	    /* But not if it's disconnected; then we want d_splice_alias below */
	    if (alias && !(alias->d_flags & DCACHE_DISCONNECTED)) {
		if (d_invalidate(alias) == 0) {
		    /* there may be more aliases; try again until we run out */
		    retry = 1;
		}
	    }

	    dput(alias);
	}

#ifdef STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT
	ip->i_flags |= S_AUTOMOUNT;
#endif
    }
    newdp = d_splice_alias(ip, dp);

 done:
    crfree(credp);

    /* It's ok for the file to not be found. That's noted by the caller by
     * seeing that the dp->d_inode field is NULL.
     */
    if (!code || code == ENOENT)
	return newdp;
    else 
	return ERR_PTR(afs_convert_code(code));
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
	tvc->mvid = (void *) __name;
	crhold(credp);
	if (tvc->uncred) {
	    crfree(tvc->uncred);
	}
	tvc->uncred = credp;
	tvc->f.states |= CUnlinked;
	afs_linux_set_nfsfs_renamed(dentry);
    } else {
	osi_FreeSmallSpace(__name);
    }
    AFS_GUNLOCK();

    if (!code) {
	__dp->d_time = hgetlo(VTOAFS(dir)->f.m.DataVersion);
	d_move(dentry, __dp);
    }
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
    code = afs_symlink(VTOAFS(dip), (char *)name, &vattr, (char *)target, credp);
    AFS_GUNLOCK();
    crfree(credp);
    return afs_convert_code(code);
}

static int
#if defined(IOP_MKDIR_TAKES_UMODE_T)
afs_linux_mkdir(struct inode *dip, struct dentry *dp, umode_t mode)
#else
afs_linux_mkdir(struct inode *dip, struct dentry *dp, int mode)
#endif
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

#if !defined(STRUCT_SUPER_BLOCK_HAS_S_D_OP)
	dp->d_op = &afs_dentry_operations;
#endif
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

    /* Prevent any new references during rename operation. */

    if (!d_unhashed(newdp)) {
	d_drop(newdp);
	rehash = newdp;
    }

#if defined(D_COUNT_INT)
    spin_lock(&olddp->d_lock);
    if (olddp->d_count > 1) {
	spin_unlock(&olddp->d_lock);
	shrink_dcache_parent(olddp);
    } else
	spin_unlock(&olddp->d_lock);
#else
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
	return code;
    }

    name[code] = '\0';
    nd_set_link(nd, name);
    return 0;
}

static void
afs_linux_put_link(struct dentry *dentry, struct nameidata *nd)
{
    char *name = nd_get_link(nd);
    if (name && !IS_ERR(name)) {
	osi_Free(name, PATH_MAX);
    }
}

#endif /* USABLE_KERNEL_PAGE_SYMLINK_CACHE */

/* Populate a page by filling it from the cache file pointed at by cachefp
 * (which contains indicated chunk)
 * If task is NULL, the page copy occurs syncronously, and the routine
 * returns with page still locked. If task is non-NULL, then page copies
 * may occur in the background, and the page will be unlocked when it is
 * ready for use.
 */
static int
afs_linux_read_cache(struct file *cachefp, struct page *page,
		     int chunk, struct pagevec *lrupv,
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
	zero_user_segment(page, 0, PAGE_CACHE_SIZE);
	SetPageUptodate(page);
	if (task)
	    unlock_page(page);
	return 0;
    }

    /* From our offset, we now need to work out which page in the disk
     * file it corresponds to. This will be fun ... */
    pageindex = (offset - AFS_CHUNKTOBASE(chunk)) >> PAGE_CACHE_SHIFT;

    while (cachepage == NULL) {
        cachepage = find_get_page(cachemapping, pageindex);
	if (!cachepage) {
	    if (!newpage)
		newpage = page_cache_alloc_cold(cachemapping);
	    if (!newpage) {
		code = -ENOMEM;
		goto out;
	    }

	    code = add_to_page_cache(newpage, cachemapping,
				     pageindex, GFP_KERNEL);
	    if (code == 0) {
	        cachepage = newpage;
	        newpage = NULL;

	        page_cache_get(cachepage);
                if (!pagevec_add(lrupv, cachepage))
                    __pagevec_lru_add_file(lrupv);

	    } else {
		page_cache_release(newpage);
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
        code = cachemapping->a_ops->readpage(NULL, cachepage);
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

    if (code && task) {
        unlock_page(page);
    }

out:
    if (cachepage)
	page_cache_release(cachepage);

    return code;
}

static int inline
afs_linux_readpage_fastpath(struct file *fp, struct page *pp, int *codep)
{
    loff_t offset = page_offset(pp);
    struct inode *ip = FILE_INODE(fp);
    struct vcache *avc = VTOAFS(ip);
    struct dcache *tdc;
    struct file *cacheFp = NULL;
    int code;
    int dcLocked = 0;
    struct pagevec lrupv;

    /* Not a UFS cache, don't do anything */
    if (cacheDiskType != AFS_FCACHE_TYPE_UFS)
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
    if (!hsame(avc->f.m.DataVersion, tdc->f.versionNo) ||
	(tdc->dflags & DFFetching)) {
	ReleaseWriteLock(&avc->lock);
	ReleaseReadLock(&tdc->lock);
	afs_PutDCache(tdc);
	return 0;
    }

    /* Update our hint for future abuse */
    avc->dchint = tdc;

    /* Okay, so we've now got a cache file that is up to date */

    /* XXX - I suspect we should be locking the inodes before we use them! */
    AFS_GUNLOCK();
    cacheFp = afs_linux_raw_open(&tdc->f.inode);
    pagevec_init(&lrupv, 0);

    code = afs_linux_read_cache(cacheFp, pp, tdc->f.chunk, &lrupv, NULL);

    if (pagevec_count(&lrupv))
       __pagevec_lru_add_file(&lrupv);

    filp_close(cacheFp, NULL);
    AFS_GLOCK();

    ReleaseReadLock(&tdc->lock);
    ReleaseWriteLock(&avc->lock);
    afs_PutDCache(tdc);

    *codep = code;
    return 1;
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

    auio = osi_Alloc(sizeof(struct uio));
    iovecp = osi_Alloc(sizeof(struct iovec));

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

    osi_Free(auio, sizeof(struct uio));
    osi_Free(iovecp, sizeof(struct iovec));

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
	struct vrequest treq;
	cred_t *credp;

	credp = crref();
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
	crfree(credp);
    }
    return afs_convert_code(code);

}

static int
afs_linux_bypass_readpages(struct file *fp, struct address_space *mapping,
			   struct list_head *page_list, unsigned num_pages)
{
    afs_int32 page_ix;
    struct uio *auio;
    afs_offs_t offset;
    struct iovec* iovecp;
    struct nocache_read_request *ancr;
    struct page *pp;
    struct pagevec lrupv;
    afs_int32 code = 0;

    cred_t *credp;
    struct inode *ip = FILE_INODE(fp);
    struct vcache *avc = VTOAFS(ip);
    afs_int32 base_index = 0;
    afs_int32 page_count = 0;
    afs_int32 isize;

    /* background thread must free: iovecp, auio, ancr */
    iovecp = osi_Alloc(num_pages * sizeof(struct iovec));

    auio = osi_Alloc(sizeof(struct uio));
    auio->uio_iov = iovecp;
    auio->uio_iovcnt = num_pages;
    auio->uio_flag = UIO_READ;
    auio->uio_seg = AFS_UIOSYS;
    auio->uio_resid = num_pages * PAGE_SIZE;

    ancr = osi_Alloc(sizeof(struct nocache_read_request));
    ancr->auio = auio;
    ancr->offset = auio->uio_offset;
    ancr->length = auio->uio_resid;

    pagevec_init(&lrupv, 0);

    for(page_ix = 0; page_ix < num_pages; ++page_ix) {

	if(list_empty(page_list))
	    break;

	pp = list_entry(page_list->prev, struct page, lru);
	/* If we allocate a page and don't remove it from page_list,
	 * the page cache gets upset. */
	list_del(&pp->lru);
	isize = (i_size_read(fp->f_mapping->host) - 1) >> PAGE_CACHE_SHIFT;
	if(pp->index > isize) {
	    if(PageLocked(pp))
		unlock_page(pp);
	    continue;
	}

	if(page_ix == 0) {
	    offset = page_offset(pp);
	    auio->uio_offset = offset;
	    base_index = pp->index;
	}
        iovecp[page_ix].iov_len = PAGE_SIZE;
        code = add_to_page_cache(pp, mapping, pp->index, GFP_KERNEL);
        if(base_index != pp->index) {
            if(PageLocked(pp))
		 unlock_page(pp);
            page_cache_release(pp);
	    iovecp[page_ix].iov_base = (void *) 0;
	    base_index++;
	    ancr->length -= PAGE_SIZE;
	    continue;
        }
        base_index++;
        if(code) {
	    if(PageLocked(pp))
		unlock_page(pp);
	    page_cache_release(pp);
	    iovecp[page_ix].iov_base = (void *) 0;
	} else {
	    page_count++;
	    if(!PageLocked(pp)) {
		lock_page(pp);
	    }

            /* increment page refcount--our original design assumed
             * that locking it would effectively pin it;  protect
             * ourselves from the possiblity that this assumption is
             * is faulty, at low cost (provided we do not fail to
             * do the corresponding decref on the other side) */
            get_page(pp);

	    /* save the page for background map */
            iovecp[page_ix].iov_base = (void*) pp;

	    /* and put it on the LRU cache */
	    if (!pagevec_add(&lrupv, pp))
		__pagevec_lru_add_file(&lrupv);
        }
    }

    /* If there were useful pages in the page list, make sure all pages
     * are in the LRU cache, then schedule the read */
    if(page_count) {
	if (pagevec_count(&lrupv))
	    __pagevec_lru_add_file(&lrupv);
	credp = crref();
        code = afs_ReadNoCache(avc, ancr, credp);
	crfree(credp);
    } else {
        /* If there is nothing for the background thread to handle,
         * it won't be freeing the things that we never gave it */
        osi_Free(iovecp, num_pages * sizeof(struct iovec));
        osi_Free(auio, sizeof(struct uio));
        osi_Free(ancr, sizeof(struct nocache_read_request));
    }
    /* we do not flush, release, or unmap pages--that will be
     * done for us by the background thread as each page comes in
     * from the fileserver */
    return afs_convert_code(code);
}


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
	zero_user_segment(pp, 0, PAGE_CACHE_SIZE);
	SetPageUptodate(pp);
	unlock_page(pp);
	return 0;
    }

    ClearPageError(pp);

    /* receiver frees */
    auio = osi_Alloc(sizeof(struct uio));
    iovecp = osi_Alloc(sizeof(struct iovec));

    /* address can be NULL, because we overwrite it with 'pp', below */
    setup_uio(auio, iovecp, NULL, page_offset(pp),
	      PAGE_SIZE, UIO_READ, AFS_UIOSYS);

    /* save the page for background map */
    get_page(pp); /* see above */
    auio->uio_iov->iov_base = (void*) pp;
    /* the background thread will free this */
    ancr = osi_Alloc(sizeof(struct nocache_read_request));
    ancr->auio = auio;
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
	    if(i_size_read(ip) > cache_bypass_threshold)
		return 1;
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
    struct pagevec lrupv;
    struct afs_pagecopy_task *task;

    if (afs_linux_bypass_check(inode))
	return afs_linux_bypass_readpages(fp, mapping, page_list, num_pages);

    if (cacheDiskType == AFS_FCACHE_TYPE_MEM)
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
    pagevec_init(&lrupv, 0);
    for (page_idx = 0; page_idx < num_pages; page_idx++) {
	struct page *page = list_entry(page_list->prev, struct page, lru);
	list_del(&page->lru);
	offset = page_offset(page);

	if (tdc && tdc->f.chunk != AFS_CHUNK(offset)) {
	    AFS_GLOCK();
	    ReleaseReadLock(&tdc->lock);
	    afs_PutDCache(tdc);
	    AFS_GUNLOCK();
	    tdc = NULL;
	    if (cacheFp)
		filp_close(cacheFp, NULL);
	}

	if (!tdc) {
	    AFS_GLOCK();
	    if ((tdc = afs_FindDCache(avc, offset))) {
		ObtainReadLock(&tdc->lock);
		if (!hsame(avc->f.m.DataVersion, tdc->f.versionNo) ||
		    (tdc->dflags & DFFetching)) {
		    ReleaseReadLock(&tdc->lock);
		    afs_PutDCache(tdc);
		    tdc = NULL;
		}
	    }
	    AFS_GUNLOCK();
	    if (tdc)
		cacheFp = afs_linux_raw_open(&tdc->f.inode);
	}

	if (tdc && !add_to_page_cache(page, mapping, page->index,
				      GFP_KERNEL)) {
	    page_cache_get(page);
	    if (!pagevec_add(&lrupv, page))
		__pagevec_lru_add_file(&lrupv);

	    afs_linux_read_cache(cacheFp, page, tdc->f.chunk, &lrupv, task);
	}
	page_cache_release(page);
    }
    if (pagevec_count(&lrupv))
       __pagevec_lru_add_file(&lrupv);

    if (tdc)
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

/* Prepare an AFS vcache for writeback. Should be called with the vcache
 * locked */
static inline int
afs_linux_prepare_writeback(struct vcache *avc) {
    if (avc->f.states & CPageWrite) {
	return AOP_WRITEPAGE_ACTIVATE;
    }
    avc->f.states |= CPageWrite;
    return 0;
}

static inline int
afs_linux_dopartialwrite(struct vcache *avc, cred_t *credp) {
    struct vrequest treq;
    int code = 0;

    if (!afs_InitReq(&treq, credp))
	code = afs_DoPartialWrite(avc, &treq);

    return afs_convert_code(code);
}

static inline void
afs_linux_complete_writeback(struct vcache *avc) {
    avc->f.states &= ~CPageWrite;
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
    unsigned int to = PAGE_CACHE_SIZE;
    loff_t isize;
    int code = 0;
    int code1 = 0;

    if (PageReclaim(pp)) {
	return AOP_WRITEPAGE_ACTIVATE;
	/* XXX - Do we need to redirty the page here? */
    }

    page_cache_get(pp);

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
    page_cache_release(pp);

    if (code1)
	return code1;

    if (code == to)
	return 0;

    return code;
}

/* afs_linux_permission
 * Check access rights - returns error if can't check or permission denied.
 */
static int
#if defined(IOP_PERMISSION_TAKES_FLAGS)
afs_linux_permission(struct inode *ip, int mode, unsigned int flags)
#elif defined(IOP_PERMISSION_TAKES_NAMEIDATA)
afs_linux_permission(struct inode *ip, int mode, struct nameidata *nd)
#else
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

    /* http://kerneltrap.org/node/4941 details the expected behaviour of
     * prepare_write. Essentially, if the page exists within the file,
     * and is not being fully written, then we should populate it.
     */

    if (!PageUptodate(page)) {
	loff_t pagebase = page_offset(page);
	loff_t isize = i_size_read(page->mapping->host);

	/* Is the location we are writing to beyond the end of the file? */
	if (pagebase >= isize ||
	    ((from == 0) && (pagebase + to) >= isize)) {
	    zero_user_segments(page, 0, from, to, PAGE_CACHE_SIZE);
	    SetPageChecked(page);
	/* Are we we writing a full page */
	} else if (from == 0 && to == PAGE_CACHE_SIZE) {
	    SetPageChecked(page);
	/* Is the page readable, if it's wronly, we don't care, because we're
	 * not actually going to read from it ... */
	} else if ((file->f_flags && O_ACCMODE) != O_WRONLY) {
	    /* We don't care if fillpage fails, because if it does the page
	     * won't be marked as up to date
	     */
	    afs_linux_fillpage(file, page);
	}
    }
    return 0;
}

#if defined(STRUCT_ADDRESS_SPACE_OPERATIONS_HAS_WRITE_BEGIN)
static int
afs_linux_write_end(struct file *file, struct address_space *mapping,
                                loff_t pos, unsigned len, unsigned copied,
                                struct page *page, void *fsdata)
{
    int code;
    unsigned int from = pos & (PAGE_CACHE_SIZE - 1);

    code = afs_linux_commit_write(file, page, from, from + len);

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
    unsigned int from = pos & (PAGE_CACHE_SIZE - 1);
    int code;

    page = grab_cache_page_write_begin(mapping, index, flags);
    *pagep = page;

    code = afs_linux_prepare_write(file, page, from, from + len);
    if (code) {
	unlock_page(page);
	page_cache_release(page);
    }

    return code;
}
#endif

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

    return NULL;
}
#endif /* !STRUCT_DENTRY_OPERATIONS_HAS_D_AUTOMOUNT */


static struct inode_operations afs_file_iops = {
  .permission =		afs_linux_permission,
  .getattr =		afs_linux_getattr,
  .setattr =		afs_notify_change,
};

static struct address_space_operations afs_file_aops = {
  .readpage =		afs_linux_readpage,
  .readpages = 		afs_linux_readpages,
  .writepage =		afs_linux_writepage,
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

static struct address_space_operations afs_symlink_aops = {
  .readpage =	afs_symlink_filler
};
#endif	/* USABLE_KERNEL_PAGE_SYMLINK_CACHE */

static struct inode_operations afs_symlink_iops = {
#if defined(USABLE_KERNEL_PAGE_SYMLINK_CACHE)
  .readlink = 		page_readlink,
# if defined(HAVE_LINUX_PAGE_FOLLOW_LINK)
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
};

void
afs_fill_inode(struct inode *ip, struct vattr *vattr)
{
	
    if (vattr)
	vattr2inode(ip, vattr);

    ip->i_mapping->backing_dev_info = afs_backing_dev_info;
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
#if defined(USABLE_KERNEL_PAGE_SYMLINK_CACHE)
	ip->i_data.a_ops = &afs_symlink_aops;
	ip->i_mapping = &ip->i_data;
#endif
    }

}
