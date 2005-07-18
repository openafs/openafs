/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Linux support routines.
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"
#if defined(AFS_LINUX24_ENV)
#include "h/smp_lock.h"
#endif
#if defined(AFS_LINUX26_ENV)
#include "h/namei.h"
#endif

#if defined(AFS_LINUX24_ENV)
/* LOOKUP_POSITIVE is becoming the default */
#ifndef LOOKUP_POSITIVE
#define LOOKUP_POSITIVE 0
#endif
/* Lookup name and return vnode for same. */
int
osi_lookupname_internal(char *aname, int followlink, struct vfsmount **mnt,
			struct dentry **dpp)
{
    int code;
    struct nameidata nd;
    int flags = LOOKUP_POSITIVE;
    code = ENOENT;

    if (followlink)
       flags |= LOOKUP_FOLLOW;
#if defined(AFS_LINUX26_ENV)
    code = path_lookup(aname, flags, &nd);
#else
    if (path_init(aname, flags, &nd))
        code = path_walk(aname, &nd);
#endif

    if (!code) {
	*dpp = dget(nd.dentry);
        if (mnt)
           *mnt = mntget(nd.mnt);
	path_release(&nd);
    }
    return code;
}
int
osi_lookupname(char *aname, uio_seg_t seg, int followlink, 
			struct dentry **dpp)
{
    int code;
    char *tname;
    code = ENOENT;
    if (seg == AFS_UIOUSER) {
        tname = getname(aname);
        if (IS_ERR(tname)) 
            return PTR_ERR(tname);
    } else {
        tname = aname;
    }
    code = osi_lookupname_internal(tname, followlink, NULL, dpp);   
    if (seg == AFS_UIOUSER) {
        putname(tname);
    }
    return code;
}
#else
int
osi_lookupname(char *aname, uio_seg_t seg, int followlink, struct dentry **dpp)
{
    struct dentry *dp = NULL;
    int code;

    code = ENOENT;
    if (seg == AFS_UIOUSER) {
	dp = followlink ? namei(aname) : lnamei(aname);
    } else {
	dp = lookup_dentry(aname, NULL, followlink ? 1 : 0);
    }

    if (dp && !IS_ERR(dp)) {
	if (dp->d_inode) {
	    *dpp = dp;
	    code = 0;
	} else
	    dput(dp);
    }

    return code;
}
#endif

/* Intialize cache device info and fragment size for disk cache partition. */
int
osi_InitCacheInfo(char *aname)
{
    int code;
    struct dentry *dp;
    extern ino_t cacheInode;
    extern struct osi_dev cacheDev;
    extern afs_int32 afs_fsfragsize;
    extern struct super_block *afs_cacheSBp;
    extern struct vfsmount *afs_cacheMnt;
    code = osi_lookupname_internal(aname, 1, &afs_cacheMnt, &dp);
    if (code)
	return ENOENT;

    cacheInode = dp->d_inode->i_ino;
    cacheDev.dev = dp->d_inode->i_sb->s_dev;
    afs_fsfragsize = dp->d_inode->i_sb->s_blocksize - 1;
    afs_cacheSBp = dp->d_inode->i_sb;

    dput(dp);

    return 0;
}


#define FOP_READ(F, B, C) (F)->f_op->read(F, B, (size_t)(C), &(F)->f_pos)
#define FOP_WRITE(F, B, C) (F)->f_op->write(F, B, (size_t)(C), &(F)->f_pos)

/* osi_rdwr
 * seek, then read or write to an open inode. addrp points to data in
 * kernel space.
 */
int
osi_rdwr(struct osi_file *osifile, uio_t * uiop, int rw)
{
#ifdef AFS_LINUX26_ENV
    struct file *filp = osifile->filp;
#else
    struct file *filp = &osifile->file;
#endif
    KERNEL_SPACE_DECL;
    int code = 0;
    struct iovec *iov;
    afs_size_t count;
    unsigned long savelim;

    savelim = current->TASK_STRUCT_RLIM[RLIMIT_FSIZE].rlim_cur;
    current->TASK_STRUCT_RLIM[RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;

    if (uiop->uio_seg == AFS_UIOSYS)
	TO_USER_SPACE();

    /* seek to the desired position. Return -1 on error. */
    if (filp->f_op->llseek) {
	if (filp->f_op->llseek(filp, (loff_t) uiop->uio_offset, 0) != uiop->uio_offset)
	    return -1;
    } else
	filp->f_pos = uiop->uio_offset;

    while (code == 0 && uiop->uio_resid > 0 && uiop->uio_iovcnt > 0) {
	iov = uiop->uio_iov;
	count = iov->iov_len;
	if (count == 0) {
	    uiop->uio_iov++;
	    uiop->uio_iovcnt--;
	    continue;
	}

	if (rw == UIO_READ)
	    code = FOP_READ(filp, iov->iov_base, count);
	else
	    code = FOP_WRITE(filp, iov->iov_base, count);

	if (code < 0) {
	    code = -code;
	    break;
	} else if (code == 0) {
	    /*
	     * This is bad -- we can't read any more data from the
	     * file, but we have no good way of signaling a partial
	     * read either.
	     */
	    code = EIO;
	    break;
	}

	iov->iov_base += code;
	iov->iov_len -= code;
	uiop->uio_resid -= code;
	uiop->uio_offset += code;
	code = 0;
    }

    if (uiop->uio_seg == AFS_UIOSYS)
	TO_KERNEL_SPACE();

    current->TASK_STRUCT_RLIM[RLIMIT_FSIZE].rlim_cur = savelim;

    return code;
}

/* setup_uio 
 * Setup a uio struct.
 */
void
setup_uio(uio_t * uiop, struct iovec *iovecp, const char *buf, afs_offs_t pos,
	  int count, uio_flag_t flag, uio_seg_t seg)
{
    iovecp->iov_base = (char *)buf;
    iovecp->iov_len = count;
    uiop->uio_iov = iovecp;
    uiop->uio_iovcnt = 1;
    uiop->uio_offset = pos;
    uiop->uio_seg = seg;
    uiop->uio_resid = count;
    uiop->uio_flag = flag;
}


/* uiomove
 * UIO_READ : dp -> uio
 * UIO_WRITE : uio -> dp
 */
int
uiomove(char *dp, int length, uio_flag_t rw, uio_t * uiop)
{
    int count;
    struct iovec *iov;
    int code;

    while (length > 0 && uiop->uio_resid > 0 && uiop->uio_iovcnt > 0) {
	iov = uiop->uio_iov;
	count = iov->iov_len;

	if (!count) {
	    uiop->uio_iov++;
	    uiop->uio_iovcnt--;
	    continue;
	}

	if (count > length)
	    count = length;

	switch (uiop->uio_seg) {
	case AFS_UIOSYS:
	    switch (rw) {
	    case UIO_READ:
		memcpy(iov->iov_base, dp, count);
		break;
	    case UIO_WRITE:
		memcpy(dp, iov->iov_base, count);
		break;
	    default:
		printf("uiomove: Bad rw = %d\n", rw);
		return -EINVAL;
	    }
	    break;
	case AFS_UIOUSER:
	    switch (rw) {
	    case UIO_READ:
		AFS_COPYOUT(dp, iov->iov_base, count, code);
		break;
	    case UIO_WRITE:
		AFS_COPYIN(iov->iov_base, dp, count, code);
		break;
	    default:
		printf("uiomove: Bad rw = %d\n", rw);
		return -EINVAL;
	    }
	    break;
	default:
	    printf("uiomove: Bad seg = %d\n", uiop->uio_seg);
	    return -EINVAL;
	}

	dp += count;
	length -= count;
	iov->iov_base += count;
	iov->iov_len -= count;
	uiop->uio_offset += count;
	uiop->uio_resid -= count;
    }
    return 0;
}

void
afs_osi_SetTime(osi_timeval_t * tvp)
{
#if defined(AFS_LINUX24_ENV)

#if defined(AFS_LINUX26_ENV)
    struct timespec tv;
    tv.tv_sec = tvp->tv_sec;
    tv.tv_nsec = tvp->tv_usec * NSEC_PER_USEC;
#else
    struct timeval tv;
    tv.tv_sec = tvp->tv_sec;
    tv.tv_usec = tvp->tv_usec;
#endif

    AFS_STATCNT(osi_SetTime);

    do_settimeofday(&tv);
#else
    extern int (*sys_settimeofdayp) (struct timeval * tv,
				     struct timezone * tz);

    KERNEL_SPACE_DECL;

    AFS_STATCNT(osi_SetTime);

    TO_USER_SPACE();
    if (sys_settimeofdayp)
	(void)(*sys_settimeofdayp) (tvp, NULL);
    TO_KERNEL_SPACE();
#endif
}

/* Free all the pages on any of the vnodes in the vlru. Must be done before
 * freeing all memory.
 */
void
osi_linux_free_inode_pages(void)
{
    int i;
    struct vcache *tvc;
    struct inode *ip;
    extern struct vcache *afs_vhashT[VCSIZE];

    for (i = 0; i < VCSIZE; i++) {
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    ip = AFSTOI(tvc);
#if defined(AFS_LINUX24_ENV)
	    if (ip->i_data.nrpages) {
#else
	    if (ip->i_nrpages) {
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
		truncate_inode_pages(&ip->i_data, 0);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,15)
		truncate_inode_pages(ip, 0);
#else
		invalidate_inode_pages(ip);
#endif
#if defined(AFS_LINUX24_ENV)
		if (ip->i_data.nrpages) {
#else
		if (ip->i_nrpages) {
#endif
		    printf("Failed to invalidate all pages on inode 0x%lx\n",
			   (unsigned long)ip);
		}
	    }
	}
    }
}

#if !defined(AFS_LINUX24_ENV)
void
osi_clear_inode(struct inode *ip)
{
    cred_t *credp = crref();
    struct vcache *vcp = ITOAFS(ip);

    if (ip->i_count > 1)
	printf("afs_put_inode: ino %ld (0x%lx) has count %ld\n",
	       (long)ip->i_ino, (unsigned long)ip, (long)ip->i_count);

    afs_InactiveVCache(vcp, credp);
    ObtainWriteLock(&vcp->lock, 504);
    ip->i_nlink = 0;		/* iput checks this after calling this routine. */
#ifdef I_CLEAR
    ip->i_state = I_CLEAR;
#endif
    ReleaseWriteLock(&vcp->lock);
    crfree(credp);
}

/* iput an inode. Since we still have a separate inode pool, we don't want
 * to call iput on AFS inodes, since they would then end up on Linux's
 * inode_unsed list.
 */
void
osi_iput(struct inode *ip)
{
    extern struct vfs *afs_globalVFS;

    AFS_GLOCK();

    if (afs_globalVFS && ip->i_sb != afs_globalVFS)
	osi_Panic("IPUT Not an afs inode\n");

#if defined(AFS_LINUX24_ENV)
    if (atomic_read(&ip->i_count) == 0)
#else
    if (ip->i_count == 0)
#endif
	osi_Panic("IPUT Bad refCount %d on inode 0x%x\n",
#if defined(AFS_LINUX24_ENV)
		  atomic_read(&ip->i_count),
#else
		  ip->i_count,
#endif
				ip);

#if defined(AFS_LINUX24_ENV)
    if (atomic_dec_and_test(&ip->i_count))
#else
    if (!--ip->i_count)
#endif
					   {
	osi_clear_inode(ip);
	ip->i_state = 0;
    }
    AFS_GUNLOCK();
}
#endif

/* check_bad_parent() : Checks if this dentry's vcache is a root vcache
 * that has its mvid (parent dir's fid) pointer set to the wrong directory
 * due to being mounted in multiple points at once. If so, check_bad_parent()
 * calls afs_lookup() to correct the vcache's mvid, as well as the volume's
 * dotdotfid and mtpoint fid members.
 * Parameters:
 *  dp - dentry to be checked.
 * Return Values:
 *  None.
 * Sideeffects:
 *   This dentry's vcache's mvid will be set to the correct parent directory's
 *   fid.
 *   This root vnode's volume will have its dotdotfid and mtpoint fids set
 *    to the correct parent and mountpoint fids.
 */

void
check_bad_parent(struct dentry *dp)
{
    cred_t *credp;
    struct vcache *vcp = ITOAFS(dp->d_inode), *avc = NULL;
    struct vcache *pvc = ITOAFS(dp->d_parent->d_inode);

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
	    AFS_RELE(avc);
	crfree(credp);
    }
    /* if bad parent */
    return;
}

struct task_struct *rxk_ListenerTask;

void
osi_linux_mask(void)
{
    SIG_LOCK(current);
    sigfillset(&current->blocked);
    RECALC_SIGPENDING(current);
    SIG_UNLOCK(current);
}

void
osi_linux_rxkreg(void)
{
    rxk_ListenerTask = current;
}
