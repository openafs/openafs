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
#include <ufs/ufs/ufsmount.h>

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
