/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * FreeBSD inode operations
 *
 * Implements:
 *
 */
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/osi_inode.h"
#include "afs/afs_stats.h"	/* statistics stuff */
#include <sys/queue.h>
#include <sys/lock.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ufsmount.h>

int
getinode(fs, dev, inode, ipp, perror)
     struct mount *fs;
     struct inode **ipp;
#ifdef AFS_FBSD53_ENV
     struct cdev *dev;
#else
     dev_t dev;
#endif
     ino_t inode;
     int *perror;
{
    struct vnode *vp;
    int code;

    *ipp = 0;
    *perror = 0;
    if (!fs) {
	struct ufsmount *ump;
	struct mount *mp;

	mtx_lock(&mountlist_mtx);
	if ((mp = TAILQ_FIRST(&mountlist)) != NULL)
	    do {
		/*
		 * XXX Also do the test for MFS
		 */
#undef m_data
#undef m_next
		if (!strcmp(mp->mnt_stat.f_fstypename, MOUNT_UFS)) {
		    ump = VFSTOUFS(mp);
		    if (ump->um_fs == NULL)
			break;
		    if (ump->um_dev == dev) {
			fs = ump->um_mountp;
		    }
		}
		mp = TAILQ_NEXT(mp, mnt_list);
	    } while (mp != TAILQ_FIRST(&mountlist));
	mtx_unlock(&mountlist_mtx);
	if (!fs)
	    return (ENXIO);
    }
    code = VFS_VGET(fs, inode, 0, &vp);
    if (code != 0) {
	*perror = BAD_IGET;
	return code;
    } else {
	*ipp = VTOI(vp);
	return (0);
    }
}

int
igetinode(vfsp, dev, inode, ipp, perror)
     struct inode **ipp;
     struct mount *vfsp;
#ifdef AFS_FBSD53_ENV
     struct cdev *dev;
#else
     dev_t dev;
#endif
     ino_t inode;
     int *perror;
{
    struct inode *ip;
    int code = 0;

    *perror = 0;

    AFS_STATCNT(igetinode);

    if ((code = getinode(vfsp, dev, inode, &ip, perror)) != 0) {
	return (code);
    }

    if (ip->i_mode == 0) {
	/* Not an allocated inode */
	vput(ITOV(ip));
	return (ENOENT);
    }

    if (ip->i_nlink == 0 || (ip->i_mode & IFMT) != IFREG) {
	vput(ITOV(ip));
	return (ENOENT);
    }

    *ipp = ip;
    return (0);
}

#if 0
/*
 * icreate system call -- create an inode
 */
afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4, retval)
     long *retval;
     long dev, near_inode, param1, param2, param3, param4;
{
    int dummy, err = 0;
    struct inode *ip, *newip;
    int code;
    struct vnode *vp;

    AFS_STATCNT(afs_syscall_icreate);

    if (!afs_suser(NULL))
	return (EPERM);

    code = getinode(0, (dev_t) dev, 2, &ip, &dummy);
    if (code) {
	return (ENOENT);
    }
    code = ialloc(ip, (ino_t) near_inode, 0, &newip);
    iput(ip);
    if (code) {
	return (code);
    }
    IN_LOCK(newip);
    newip->i_flag |= IACC | IUPD | ICHG;

    newip->i_nlink = 1;

    newip->i_mode = IFREG;

    IN_UNLOCK(newip);
    vp = ITOV(newip);
    VN_LOCK(vp);
    vp->v_type = VREG;
    VN_UNLOCK(vp);

    /*
     * if ( !vp->v_object)
     * {
     * extern struct vfs_ubcops ufs_ubcops;
     * extern struct vm_ubc_object* ubc_object_allocate();
     * struct vm_ubc_object* vop;
     * vop = ubc_object_allocate(&vp, &ufs_ubcops,
     * vp->v_mount->m_funnel);
     * VN_LOCK(vp);
     * vp->v_object = vop;
     * VN_UNLOCK(vp);
     * }
     */

    IN_LOCK(newip);
    /*    newip->i_flags |= IC_XUID|IC_XGID; */
    /*    newip->i_flags &= ~IC_PROPLIST; */
    newip->i_vicep1 = param1;
    if (param2 == 0x1fffffff /*INODESPECIAL*/) {
	newip->i_vicep2 = ((0x1fffffff << 3) + (param4 & 0x3));
	newip->i_vicep3a = (u_short) (param3 >> 16);
	newip->i_vicep3b = (u_short) param3;
    } else {
	newip->i_vicep2 =
	    (((param2 >> 16) & 0x1f) << 27) +
	    (((param4 >> 16) & 0x1f) << 22) + (param3 & 0x3fffff);
	newip->i_vicep3a = (u_short) param4;
	newip->i_vicep3b = (u_short) param2;
    }
    newip->i_vicemagic = VICEMAGIC;

    *retval = newip->i_number;
    IN_UNLOCK(newip);
    iput(newip);
    return (code);
}


int
afs_syscall_iopen(dev, inode, usrmod, retval)
     long *retval;
     int dev, inode, usrmod;
{
    struct file *fp;
    struct inode *ip;
    struct vnode *vp = NULL;
    int dummy;
    int fd;
    extern struct fileops vnops;
    int code;

    AFS_STATCNT(afs_syscall_iopen);

    if (!afs_suser(NULL))
	return (EPERM);

    code = igetinode(0, (dev_t) dev, (ino_t) inode, &ip, &dummy);
    if (code) {
	return (code);
    }
    if ((code = falloc(curproc, &fp, &fd)) != 0) {
	iput(ip);
	return (code);
    }
    IN_UNLOCK(ip);

    /* FreeBSD doesn't do much mp stuff yet :( */
    /* FP_LOCK(fp); */
    fp->f_flag = (usrmod) & FMASK;
    fp->f_type = DTYPE_VNODE;
    fp->f_ops = &vnops;
    fp->f_data = (caddr_t) ITOV(ip);

    /* FP_UNLOCK(fp); */
    return (0);
}


/*
 * Support for iinc() and idec() system calls--increment or decrement
 * count on inode.
 * Restricted to super user.
 * Only VICEMAGIC type inodes.
 */
int
afs_syscall_iincdec(dev, inode, inode_p1, amount)
     int dev, inode, inode_p1, amount;
{
    int dummy;
    struct inode *ip;
    int code;

    if (!afs_suser(NULL))
	return (EPERM);

    code = igetinode(0, (dev_t) dev, (ino_t) inode, &ip, &dummy);
    if (code) {
	return (code);
    }
    if (!IS_VICEMAGIC(ip)) {
	return (EPERM);
    } else if (ip->i_vicep1 != inode_p1) {
	return (ENXIO);
    }
    ip->i_nlink += amount;
    if (ip->i_nlink == 0) {
	CLEAR_VICEMAGIC(ip);
    }
    ip->i_flag |= ICHG;
    iput(ip);
    return (0);
}
#else
int
afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4, retval)
     long *retval;
     long dev, near_inode, param1, param2, param3, param4;
{
    return EOPNOTSUPP;
}

int
afs_syscall_iopen(dev, inode, usrmod, retval)
     long *retval;
     int dev, inode, usrmod;
{
    return EOPNOTSUPP;
}

int
afs_syscall_iincdec(dev, inode, inode_p1, amount)
     int dev, inode, inode_p1, amount;
{
    return EOPNOTSUPP;
}
#endif
