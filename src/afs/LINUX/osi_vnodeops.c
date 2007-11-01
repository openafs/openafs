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

RCSID
    ("$Header$");

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
#if defined(AFS_LINUX26_ENV)
#include "h/writeback.h"
#endif

#ifdef pgoff2loff
#define pageoff(pp) pgoff2loff((pp)->index)
#else
#define pageoff(pp) pp->offset
#endif

#if defined(AFS_LINUX26_ENV)
#define UnlockPage(pp) unlock_page(pp)
#endif

extern struct vcache *afs_globalVp;
static ssize_t
afs_linux_read(struct file *fp, char *buf, size_t count, loff_t * offp)
{
    ssize_t code;
    struct vcache *vcp = VTOAFS(fp->f_dentry->d_inode);
    cred_t *credp = crref();
    struct vrequest treq;

    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_READOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, offp, ICL_TYPE_INT32, count, ICL_TYPE_INT32,
	       99999);

    /* get a validated vcache entry */
    code = afs_InitReq(&treq, credp);
    if (!code)
	code = afs_VerifyVCache(vcp, &treq);

    if (code)
	code = -code;
    else {
	    osi_FlushPages(vcp, credp);	/* ensure stale pages are gone */
	    AFS_GUNLOCK();
#ifdef DO_SYNC_READ
	    code = do_sync_read(fp, buf, count, offp);
#else
	    code = generic_file_read(fp, buf, count, offp);
#endif
	    AFS_GLOCK();
    }

    afs_Trace4(afs_iclSetp, CM_TRACE_READOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, offp, ICL_TYPE_INT32, count, ICL_TYPE_INT32,
	       code);

    AFS_GUNLOCK();
    crfree(credp);
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
    struct vrequest treq;
    cred_t *credp = crref();

    AFS_GLOCK();

    afs_Trace4(afs_iclSetp, CM_TRACE_WRITEOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, offp, ICL_TYPE_INT32, count, ICL_TYPE_INT32,
	       (fp->f_flags & O_APPEND) ? 99998 : 99999);


    /* get a validated vcache entry */
    code = (ssize_t) afs_InitReq(&treq, credp);
    if (!code)
	code = (ssize_t) afs_VerifyVCache(vcp, &treq);

    ObtainWriteLock(&vcp->lock, 529);
    afs_FakeOpen(vcp);
    ReleaseWriteLock(&vcp->lock);
    if (code)
	code = -code;
    else {
	    AFS_GUNLOCK();
#ifdef DO_SYNC_READ
	    code = do_sync_write(fp, buf, count, offp);
#else
	    code = generic_file_write(fp, buf, count, offp);
#endif
	    AFS_GLOCK();
    }

    ObtainWriteLock(&vcp->lock, 530);
    afs_FakeClose(vcp, credp);
    ReleaseWriteLock(&vcp->lock);

    afs_Trace4(afs_iclSetp, CM_TRACE_WRITEOP, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_OFFSET, offp, ICL_TYPE_INT32, count, ICL_TYPE_INT32,
	       code);

    AFS_GUNLOCK();
    crfree(credp);
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
    extern struct DirEntry *afs_dir_GetBlob();
    struct vcache *avc = VTOAFS(FILE_INODE(fp));
    struct vrequest treq;
    register struct dcache *tdc;
    int code;
    int offset;
    int dirpos;
    struct DirEntry *de;
    ino_t ino;
    int len;
    afs_size_t origOffset, tlen;
    cred_t *credp = crref();
    struct afs_fakestat_state fakestat;

#if defined(AFS_LINUX26_ENV)
    lock_kernel();
#endif
    AFS_GLOCK();
    AFS_STATCNT(afs_readdir);

    code = afs_InitReq(&treq, credp);
    crfree(credp);
    if (code)
	goto out1;

    afs_InitFakeStat(&fakestat);
    code = afs_EvalFakeStat(&avc, &fakestat, &treq);
    if (code)
	goto out;

    /* update the cache entry */
  tagain:
    code = afs_VerifyVCache(avc, &treq);
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
    while ((avc->states & CStatd)
	   && (tdc->dflags & DFFetching)
	   && hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseSharedLock(&avc->lock);
	afs_osi_Sleep(&tdc->validPos);
	ObtainSharedLock(&avc->lock, 812);
	ObtainReadLock(&tdc->lock);
    }
    if (!(avc->states & CStatd)
	|| !hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	ReleaseReadLock(&tdc->lock);
	ReleaseSharedLock(&avc->lock);
	afs_PutDCache(tdc);
	goto tagain;
    }

    /* Set the readdir-in-progress flag, and downgrade the lock
     * to shared so others will be able to acquire a read lock.
     */
    avc->states |= CReadDir;
    avc->dcreaddir = tdc;
    avc->readdir_pid = MyPidxx;
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

	ino = afs_calc_inum (avc->fid.Fid.Volume, ntohl(de->fid.vnode));

	if (de->name)
	    len = strlen(de->name);
	else {
	    printf("afs_linux_readdir: afs_dir_GetBlob failed, null name (inode %lx, dirpos %d)\n", 
		   (unsigned long)&tdc->f.inode, dirpos);
	    DRelease((struct buffer *) de, 0);
	    ReleaseSharedLock(&avc->lock);
	    afs_PutDCache(tdc);
	    code = -ENOENT;
	    goto out;
	}

	/* filldir returns -EINVAL when the buffer is full. */
#if defined(AFS_LINUX26_ENV) || ((defined(AFS_LINUX24_ENV) || defined(pgoff2loff)) && defined(DECLARE_FSTYPE))
	{
	    unsigned int type = DT_UNKNOWN;
	    struct VenusFid afid;
	    struct vcache *tvc;
	    int vtype;
	    afid.Cell = avc->fid.Cell;
	    afid.Fid.Volume = avc->fid.Fid.Volume;
	    afid.Fid.Vnode = ntohl(de->fid.vnode);
	    afid.Fid.Unique = ntohl(de->fid.vunique);
	    if ((avc->states & CForeign) == 0 && (ntohl(de->fid.vnode) & 1)) {
		type = DT_DIR;
	    } else if ((tvc = afs_FindVCache(&afid, 0, 0))) {
		if (tvc->mvstat) {
		    type = DT_DIR;
		} else if (((tvc->states) & (CStatd | CTruth))) {
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
	DRelease((struct buffer *)de, 0);
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
    avc->states &= ~CReadDir;
    avc->dcreaddir = 0;
    avc->readdir_pid = 0;
    ReleaseSharedLock(&avc->lock);
    code = 0;

out:
    afs_PutFakeStat(&fakestat);
out1:
    AFS_GUNLOCK();
#if defined(AFS_LINUX26_ENV)
    unlock_kernel();
#endif
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
    cred_t *credp = crref();
    struct vrequest treq;
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
    code = afs_InitReq(&treq, credp);
    if (code)
	goto out_err;

    code = afs_VerifyVCache(vcp, &treq);
    if (code)
	goto out_err;

    osi_FlushPages(vcp, credp);	/* ensure stale pages are gone */

    AFS_GUNLOCK();
    code = generic_file_mmap(fp, vmap);
    AFS_GLOCK();
    if (!code)
	vcp->states |= CMAPPED;

out:
    AFS_GUNLOCK();
    crfree(credp);
    return code;

out_err:
    code = -code;
    goto out;
}

static int
afs_linux_open(struct inode *ip, struct file *fp)
{
    struct vcache *vcp = VTOAFS(ip);
    cred_t *credp = crref();
    int code;

#ifdef AFS_LINUX24_ENV
    lock_kernel();
#endif
    AFS_GLOCK();
    code = afs_open(&vcp, fp->f_flags, credp);
    AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
    unlock_kernel();
#endif

    crfree(credp);
    return -code;
}

static int
afs_linux_release(struct inode *ip, struct file *fp)
{
    struct vcache *vcp = VTOAFS(ip);
    cred_t *credp = crref();
    int code = 0;

#ifdef AFS_LINUX24_ENV
    lock_kernel();
#endif
    AFS_GLOCK();
    code = afs_close(vcp, fp->f_flags, credp);
    AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
    unlock_kernel();
#endif

    crfree(credp);
    return -code;
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
    lock_kernel();
#endif
    AFS_GLOCK();
    code = afs_fsync(VTOAFS(ip), credp);
    AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
    unlock_kernel();
#endif
    crfree(credp);
    return -code;

}


static int
afs_linux_lock(struct file *fp, int cmd, struct file_lock *flp)
{
    int code = 0;
    struct vcache *vcp = VTOAFS(FILE_INODE(fp));
    cred_t *credp = crref();
    struct AFS_FLOCK flock;
    /* Convert to a lock format afs_lockctl understands. */
    memset((char *)&flock, 0, sizeof(flock));
    flock.l_type = flp->fl_type;
    flock.l_pid = flp->fl_pid;
    flock.l_whence = 0;
    flock.l_start = flp->fl_start;
    flock.l_len = flp->fl_end - flp->fl_start;

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
    code = afs_lockctl(vcp, &flock, cmd, credp);
    AFS_GUNLOCK();

#ifdef AFS_LINUX24_ENV
    if ((code == 0 || flp->fl_type == F_UNLCK) && 
	(cmd == F_SETLK || cmd == F_SETLKW)) {
#ifdef POSIX_LOCK_FILE_WAIT_ARG
        code = posix_lock_file(fp, flp, 0);
#else
        flp->fl_flags &=~ FL_SLEEP;
        code = posix_lock_file(fp, flp);
#endif
       if (code && flp->fl_type != F_UNLCK) {
           struct AFS_FLOCK flock2;
           flock2 = flock;
           flock2.l_type = F_UNLCK;
           AFS_GLOCK();
           afs_lockctl(vcp, &flock2, F_SETLK, credp);
           AFS_GUNLOCK();
       }
    }
#endif
    /* Convert flock back to Linux's file_lock */
    flp->fl_type = flock.l_type;
    flp->fl_pid = flock.l_pid;
    flp->fl_start = flock.l_start;
    flp->fl_end = flock.l_start + flock.l_len;

    crfree(credp);
    return -code;

}

#ifdef STRUCT_FILE_OPERATIONS_HAS_FLOCK
static int
afs_linux_flock(struct file *fp, int cmd, struct file_lock *flp) {
    int code = 0;
    struct vcache *vcp = VTOAFS(FILE_INODE(fp));
    cred_t *credp = crref();
    struct AFS_FLOCK flock;
    /* Convert to a lock format afs_lockctl understands. */
    memset((char *)&flock, 0, sizeof(flock));
    flock.l_type = flp->fl_type;
    flock.l_pid = flp->fl_pid;
    flock.l_whence = 0;
    flock.l_start = 0;
    flock.l_len = OFFSET_MAX;

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
    code = afs_lockctl(vcp, &flock, cmd, credp);
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
    return -code;
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
    struct vcache *vcp = VTOAFS(FILE_INODE(fp));
    cred_t *credp = crref();
    int code;

    AFS_GLOCK();

    code = afs_InitReq(&treq, credp);
    if (code)
	goto out;

    ObtainSharedLock(&vcp->lock, 535);
    if (vcp->execsOrWriters > 0) {
	UpgradeSToWLock(&vcp->lock, 536);
	code = afs_StoreAllSegments(vcp, &treq, AFS_SYNC | AFS_LASTSTORE);
	ConvertWToSLock(&vcp->lock);
    }
    code = afs_CheckCode(code, &treq, 54);
    ReleaseSharedLock(&vcp->lock);

out:
    AFS_GUNLOCK();

    crfree(credp);
    return -code;
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
#ifdef GENERIC_FILE_AIO_READ
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
#if defined(AFS_LINUX26_ENV) && defined(STRUCT_FILE_OPERATIONS_HAS_SENDFILE)
  .sendfile =   generic_file_sendfile,
#endif
  .release =	afs_linux_release,
  .fsync =	afs_linux_fsync,
  .lock =	afs_linux_lock,
#ifdef STRUCT_FILE_OPERATIONS_HAS_FLOCK
  .flock =	afs_linux_flock,
#endif
};


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

    if (vcp->mvid->Fid.Volume != pvc->fid.Fid.Volume) {	/* bad parent */
	credp = crref();

	/* force a lookup, so vcp->mvid is fixed up */
	afs_lookup(pvc, dp->d_name.name, &avc, credp);
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

#ifdef AFS_LINUX24_ENV
    lock_kernel();
#endif
    AFS_GLOCK();

#ifdef notyet
    /* Make this a fast path (no crref), since it's called so often. */
    if (vcp->states & CStatd) {

	if (*dp->d_name.name != '/' && vcp->mvstat == 2)	/* root vnode */
	    check_bad_parent(dp);	/* check and correct mvid */

	AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
	unlock_kernel();
#endif
	return 0;
    }
#endif

    credp = crref();
    code = afs_getattr(vcp, &vattr, credp);
    if (!code)
        vattr2inode(AFSTOV(vcp), &vattr);

    AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
    unlock_kernel();
#endif
    crfree(credp);

    return -code;
}

#if defined(AFS_LINUX26_ENV)
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

#ifdef AFS_LINUX24_ENV
    lock_kernel();
#endif
    AFS_GLOCK();

    if (dp->d_inode) {

	vcp = VTOAFS(dp->d_inode);
	pvcp = VTOAFS(dp->d_parent->d_inode);		/* dget_parent()? */

	if (vcp == afs_globalVp)
	    goto good_dentry;

	if (*dp->d_name.name != '/' && vcp->mvstat == 2)	/* root vnode */
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

	if (hgetlo(pvcp->m.DataVersion) > dp->d_time || !(vcp->states & CStatd)) {

	    credp = crref();
	    afs_lookup(pvcp, dp->d_name.name, &tvc, credp);
	    if (!tvc || tvc != vcp)
		goto bad_dentry;

	    if (afs_getattr(vcp, &vattr, credp))
		goto bad_dentry;

	    vattr2inode(AFSTOV(vcp), &vattr);
	    dp->d_time = hgetlo(pvcp->m.DataVersion);
	}

	/* should we always update the attributes at this point? */
	/* unlikely--the vcache entry hasn't changed */

    } else {
#ifdef notyet
	pvcp = VTOAFS(dp->d_parent->d_inode);		/* dget_parent()? */
	if (hgetlo(pvcp->m.DataVersion) > dp->d_time)
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
    AFS_GUNLOCK();
    if (credp)
	crfree(credp);

    if (!valid) {
	shrink_dcache_parent(dp);
	d_drop(dp);
    }
#ifdef AFS_LINUX24_ENV
    unlock_kernel();
#endif
    return valid;

  bad_dentry:
    valid = 0;
    goto done;
}

static void
afs_dentry_iput(struct dentry *dp, struct inode *ip)
{
    struct vcache *vcp = VTOAFS(ip);

    AFS_GLOCK();
    (void) afs_InactiveVCache(vcp, NULL);
    AFS_GUNLOCK();
#ifdef DCACHE_NFSFS_RENAMED
#ifdef AFS_LINUX26_ENV
    spin_lock(&dp->d_lock);
#endif
    dp->d_flags &= ~DCACHE_NFSFS_RENAMED;   
#ifdef AFS_LINUX26_ENV
    spin_unlock(&dp->d_lock);
#endif
#endif

    iput(ip);
}

static int
afs_dentry_delete(struct dentry *dp)
{
    if (dp->d_inode && (VTOAFS(dp->d_inode)->states & CUnlinked))
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

#if defined(AFS_LINUX26_ENV)
    lock_kernel();
#endif
    AFS_GLOCK();
    code = afs_create(VTOAFS(dip), (char *)name, &vattr, NONEXCL, mode,
		      &vcp, credp);

    if (!code) {
	struct inode *ip = AFSTOV(vcp);

	afs_getattr(vcp, &vattr, credp);
	afs_fill_inode(ip, &vattr);
	dp->d_op = &afs_dentry_operations;
	dp->d_time = hgetlo(VTOAFS(dip)->m.DataVersion);
	d_instantiate(dp, ip);
    }
    AFS_GUNLOCK();

#if defined(AFS_LINUX26_ENV)
    unlock_kernel();
#endif
    crfree(credp);
    return -code;
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
#if defined(AFS_LINUX26_ENV)
    struct dentry *newdp = NULL;
#endif
    int code;

#if defined(AFS_LINUX26_ENV)
    lock_kernel();
#endif
    AFS_GLOCK();
    code = afs_lookup(VTOAFS(dip), comp, &vcp, credp);
    
    if (vcp) {
	struct vattr vattr;

	ip = AFSTOV(vcp);
	afs_getattr(vcp, &vattr, credp);
	afs_fill_inode(ip, &vattr);
    }
    dp->d_op = &afs_dentry_operations;
    dp->d_time = hgetlo(VTOAFS(dip)->m.DataVersion);
    AFS_GUNLOCK();

#if defined(AFS_LINUX24_ENV)
    if (ip && S_ISDIR(ip->i_mode)) {
	struct dentry *alias;

        /* Try to invalidate an existing alias in favor of our new one */
	alias = d_find_alias(ip);
#if defined(AFS_LINUX26_ENV)
        /* But not if it's disconnected; then we want d_splice_alias below */
	if (alias && !(alias->d_flags & DCACHE_DISCONNECTED)) {
#else
	if (alias) {
#endif
	    if (d_invalidate(alias) == 0) {
		dput(alias);
	    } else {
		iput(ip);
#if defined(AFS_LINUX26_ENV)
		unlock_kernel();
#endif
		crfree(credp);
		return alias;
	    }
	}
    }
#endif
#if defined(AFS_LINUX26_ENV)
    newdp = d_splice_alias(ip, dp);
#else
    d_add(dp, ip);
#endif

#if defined(AFS_LINUX26_ENV)
    unlock_kernel();
#endif
    crfree(credp);

    /* It's ok for the file to not be found. That's noted by the caller by
     * seeing that the dp->d_inode field is NULL.
     */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,10)
#if defined(AFS_LINUX26_ENV)
    if (!code || code == ENOENT)
	return newdp;
#else
    if (code == ENOENT)
	return ERR_PTR(0);
#endif
    else
	return ERR_PTR(-code);
#else
    if (code == ENOENT)
	code = 0;
    return -code;
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
    code = afs_link(VTOAFS(oldip), VTOAFS(dip), name, credp);

    AFS_GUNLOCK();
    crfree(credp);
    return -code;
}

static int
afs_linux_unlink(struct inode *dip, struct dentry *dp)
{
    int code = EBUSY;
    cred_t *credp = crref();
    const char *name = dp->d_name.name;
    struct vcache *tvc = VTOAFS(dp->d_inode);

#if defined(AFS_LINUX26_ENV)
    lock_kernel();
#endif
    if (VREFCOUNT(tvc) > 1 && tvc->opens > 0
				&& !(tvc->states & CUnlinked)) {
	struct dentry *__dp;
	char *__name;
	extern char *afs_newname();

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
	code = afs_rename(VTOAFS(dip), dp->d_name.name, VTOAFS(dip), __dp->d_name.name, credp);
	if (!code) {
            tvc->mvid = (void *) __name;
            crhold(credp);
            if (tvc->uncred) {
                crfree(tvc->uncred);
            }
            tvc->uncred = credp;
	    tvc->states |= CUnlinked;
#ifdef DCACHE_NFSFS_RENAMED
#ifdef AFS_LINUX26_ENV
	    spin_lock(&dp->d_lock);
#endif
	    dp->d_flags |= DCACHE_NFSFS_RENAMED;   
#ifdef AFS_LINUX26_ENV
	    spin_unlock(&dp->d_lock);
#endif
#endif
	} else {
	    osi_FreeSmallSpace(__name);	
	}
	AFS_GUNLOCK();

	if (!code) {
	    __dp->d_time = hgetlo(VTOAFS(dip)->m.DataVersion);
	    d_move(dp, __dp);
	}
	dput(__dp);

	goto out;
    }

    AFS_GLOCK();
    code = afs_remove(VTOAFS(dip), name, credp);
    AFS_GUNLOCK();
    if (!code)
	d_drop(dp);
out:
#if defined(AFS_LINUX26_ENV)
    unlock_kernel();
#endif
    crfree(credp);
    return -code;
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
    code = afs_symlink(VTOAFS(dip), name, &vattr, target, credp);
    AFS_GUNLOCK();
    crfree(credp);
    return -code;
}

static int
afs_linux_mkdir(struct inode *dip, struct dentry *dp, int mode)
{
    int code;
    cred_t *credp = crref();
    struct vcache *tvcp = NULL;
    struct vattr vattr;
    const char *name = dp->d_name.name;

#if defined(AFS_LINUX26_ENV)
    lock_kernel();
#endif
    VATTR_NULL(&vattr);
    vattr.va_mask = ATTR_MODE;
    vattr.va_mode = mode;
    AFS_GLOCK();
    code = afs_mkdir(VTOAFS(dip), name, &vattr, &tvcp, credp);

    if (tvcp) {
	struct inode *ip = AFSTOV(tvcp);

	afs_getattr(tvcp, &vattr, credp);
	afs_fill_inode(ip, &vattr);

	dp->d_op = &afs_dentry_operations;
	dp->d_time = hgetlo(VTOAFS(dip)->m.DataVersion);
	d_instantiate(dp, ip);
    }
    AFS_GUNLOCK();

#if defined(AFS_LINUX26_ENV)
    unlock_kernel();
#endif
    crfree(credp);
    return -code;
}

static int
afs_linux_rmdir(struct inode *dip, struct dentry *dp)
{
    int code;
    cred_t *credp = crref();
    const char *name = dp->d_name.name;

    /* locking kernel conflicts with glock? */

    AFS_GLOCK();
    code = afs_rmdir(VTOAFS(dip), name, credp);
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
    return -code;
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

#if defined(AFS_LINUX26_ENV)
    /* Prevent any new references during rename operation. */
    lock_kernel();
#endif
    /* Remove old and new entries from name hash. New one will change below.
     * While it's optimal to catch failures and re-insert newdp into hash,
     * it's also error prone and in that case we're already dealing with error
     * cases. Let another lookup put things right, if need be.
     */
#if defined(AFS_LINUX26_ENV)
    if (!d_unhashed(newdp)) {
	d_drop(newdp);
	rehash = newdp;
    }
#else
    if (!list_empty(&newdp->d_hash)) {
	d_drop(newdp);
	rehash = newdp;
    }
#endif

#if defined(AFS_LINUX24_ENV)
    if (atomic_read(&olddp->d_count) > 1)
	shrink_dcache_parent(olddp);
#endif

    AFS_GLOCK();
    code = afs_rename(VTOAFS(oldip), oldname, VTOAFS(newip), newname, credp);
    AFS_GUNLOCK();

    if (rehash)
	d_rehash(rehash);

#if defined(AFS_LINUX26_ENV)
    unlock_kernel();
#endif

    crfree(credp);
    return -code;
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
    uio_t tuio;
    struct iovec iov;

    setup_uio(&tuio, &iov, target, (afs_offs_t) 0, maxlen, UIO_READ, seg);
    code = afs_readlink(VTOAFS(ip), &tuio, credp);
    crfree(credp);

    if (!code)
	return maxlen - tuio.uio_resid;
    else
	return -code;
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

/* afs_linux_readpage
 * all reads come through here. A strategy-like read call.
 */
static int
afs_linux_readpage(struct file *fp, struct page *pp)
{
    int code;
    cred_t *credp = crref();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    char *address;
    afs_offs_t offset = ((loff_t) pp->index) << PAGE_CACHE_SHIFT;
#else
    ulong address = afs_linux_page_address(pp);
    afs_offs_t offset = pageoff(pp);
#endif
    uio_t tuio;
    struct iovec iovec;
    struct inode *ip = FILE_INODE(fp);
    int cnt = page_count(pp);
    struct vcache *avc = VTOAFS(ip);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    address = kmap(pp);
    ClearPageError(pp);
#else
    atomic_add(1, &pp->count);
    set_bit(PG_locked, &pp->flags);	/* other bits? See mm.h */
    clear_bit(PG_error, &pp->flags);
#endif

    setup_uio(&tuio, &iovec, (char *)address, offset, PAGE_SIZE, UIO_READ,
	      AFS_UIOSYS);
#ifdef AFS_LINUX24_ENV
    lock_kernel();
#endif
    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_READPAGE, ICL_TYPE_POINTER, ip, ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, cnt, ICL_TYPE_INT32, 99999);	/* not a possible code value */
    code = afs_rdwr(avc, &tuio, UIO_READ, 0, credp);
    afs_Trace4(afs_iclSetp, CM_TRACE_READPAGE, ICL_TYPE_POINTER, ip,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, cnt, ICL_TYPE_INT32,
	       code);
    AFS_GUNLOCK();
#ifdef AFS_LINUX24_ENV
    unlock_kernel();
#endif

    if (!code) {
	if (tuio.uio_resid)	/* zero remainder of page */
	    memset((void *)(address + (PAGE_SIZE - tuio.uio_resid)), 0,
		   tuio.uio_resid);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	flush_dcache_page(pp);
	SetPageUptodate(pp);
#else
	set_bit(PG_uptodate, &pp->flags);
#endif
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    kunmap(pp);
    UnlockPage(pp);
#else
    clear_bit(PG_locked, &pp->flags);
    wake_up(&pp->wait);
    free_page(address);
#endif

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

    crfree(credp);
    return -code;
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
    uio_t tuio;
    struct iovec iovec;
    int f_flags = 0;

    buffer = kmap(pp) + offset;
    base = (((loff_t) pp->index) << PAGE_CACHE_SHIFT)  + offset;

    credp = crref();
    lock_kernel();
    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_UPDATEPAGE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, page_count(pp),
	       ICL_TYPE_INT32, 99999);

    setup_uio(&tuio, &iovec, buffer, base, count, UIO_WRITE, AFS_UIOSYS);

    code = afs_write(vcp, &tuio, f_flags, credp, 0);

    ip->i_size = vcp->m.Length;
    ip->i_blocks = ((vcp->m.Length + 1023) >> 10) << 1;

    if (!code) {
	struct vrequest treq;

	ObtainWriteLock(&vcp->lock, 533);
	if (!afs_InitReq(&treq, credp))
	    code = afs_DoPartialWrite(vcp, &treq);
	ReleaseWriteLock(&vcp->lock);
    }
    code = code ? -code : count - tuio.uio_resid;

    afs_Trace4(afs_iclSetp, CM_TRACE_UPDATEPAGE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, page_count(pp),
	       ICL_TYPE_INT32, code);

    AFS_GUNLOCK();
    unlock_kernel();
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

#if defined(AFS_LINUX26_ENV)
    if (PageReclaim(pp)) {
# if defined(WRITEPAGE_ACTIVATE)
	return WRITEPAGE_ACTIVATE;
# else 
	return AOP_WRITEPAGE_ACTIVATE;
# endif
    }
#else
    if (PageLaunder(pp)) {
	return(fail_writepage(pp));
    }
#endif

    inode = (struct inode *)mapping->host;
    end_index = inode->i_size >> PAGE_CACHE_SHIFT;

    /* easy case */
    if (pp->index < end_index)
	goto do_it;
    /* things got complicated... */
    offset = inode->i_size & (PAGE_CACHE_SIZE - 1);
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
    uio_t tuio;
    struct iovec iovec;

    set_bit(PG_locked, &pp->flags);

    credp = crref();
    AFS_GLOCK();
    afs_Trace4(afs_iclSetp, CM_TRACE_UPDATEPAGE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, page_count(pp),
	       ICL_TYPE_INT32, 99999);
    setup_uio(&tuio, &iovec, page_addr + offset,
	      (afs_offs_t) (pageoff(pp) + offset), count, UIO_WRITE,
	      AFS_UIOSYS);

    code = afs_write(vcp, &tuio, fp->f_flags, credp, 0);

    ip->i_size = vcp->m.Length;
    ip->i_blocks = ((vcp->m.Length + 1023) >> 10) << 1;

    if (!code) {
	struct vrequest treq;

	ObtainWriteLock(&vcp->lock, 533);
	vcp->m.Date = osi_Time();   /* set modification time */
	if (!afs_InitReq(&treq, credp))
	    code = afs_DoPartialWrite(vcp, &treq);
	ReleaseWriteLock(&vcp->lock);
    }

    code = code ? -code : count - tuio.uio_resid;
    afs_Trace4(afs_iclSetp, CM_TRACE_UPDATEPAGE, ICL_TYPE_POINTER, vcp,
	       ICL_TYPE_POINTER, pp, ICL_TYPE_INT32, page_count(pp),
	       ICL_TYPE_INT32, code);

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
    return -code;
}

#if defined(AFS_LINUX24_ENV)
static int
afs_linux_commit_write(struct file *file, struct page *page, unsigned offset,
		       unsigned to)
{
    int code;

    code = afs_linux_writepage_sync(file->f_dentry->d_inode, page,
                                    offset, to - offset);
#if !defined(AFS_LINUX26_ENV)
    kunmap(page);
#endif

    return code;
}

static int
afs_linux_prepare_write(struct file *file, struct page *page, unsigned from,
			unsigned to)
{
/* sometime between 2.4.0 and 2.4.19, the callers of prepare_write began to
   call kmap directly instead of relying on us to do it */
#if !defined(AFS_LINUX26_ENV)
    kmap(page);
#endif
    return 0;
}

extern int afs_notify_change(struct dentry *dp, struct iattr *iattrp);
#endif

static struct inode_operations afs_file_iops = {
#if defined(AFS_LINUX26_ENV)
  .permission =		afs_linux_permission,
  .getattr =		afs_linux_getattr,
  .setattr =		afs_notify_change,
#elif defined(AFS_LINUX24_ENV)
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
  .commit_write =	afs_linux_commit_write,
  .prepare_write =	afs_linux_prepare_write,
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
#if defined(AFS_LINUX26_ENV)
  .getattr =		afs_linux_getattr,
#else
  .revalidate =		afs_linux_revalidate,
#endif
  .permission =		afs_linux_permission,
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

    lock_kernel();
    AFS_GLOCK();
    code = afs_linux_ireadlink(ip, p, PAGE_SIZE, AFS_UIOSYS);
    AFS_GUNLOCK();

    if (code < 0)
	goto fail;
    p[code] = '\0';		/* null terminate? */
    unlock_kernel();

    SetPageUptodate(page);
    kunmap(page);
    UnlockPage(page);
    return 0;

  fail:
    unlock_kernel();

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

    /* insert_inode_hash(ip);	-- this would make iget() work (if we used it) */
}
