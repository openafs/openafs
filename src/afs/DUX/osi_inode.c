/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * DUX inode operations
 *
 * Implements:
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/DUX/Attic/osi_inode.c,v 1.13 2004/07/29 03:13:44 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/osi_inode.h"
#include "afs/afs_stats.h"	/* statistics stuff */
#include <ufs/ufsmount.h>

/* given a vnode, return the `inode number'; if it's a UFS vnode just
   return the i_number from the in kernel inode struct, if not stat
   the file to get the inumber from there */
afs_uint32
osi_vnodeToInumber(struct vnode *vnode)
{
    int error;
    struct vattr attr;

    /* use faster version with UFS */
    if (vnode->v_tag == VT_UFS)
	return VTOI(vnode)->i_number;
    /* otherwise stat the file */
    VOP_GETATTR(vnode, &attr, NULL, error);
    if (error)
	osi_Panic("VOP_GETATTR = %d", error);
    return attr.va_fileid;
}

/* return the id of the device containing the file */
afs_uint32
osi_vnodeToDev(struct vnode * vnode)
{
    int error;
    struct vattr attr;

    /* use faster version with UFS */
    if (vnode->v_tag == VT_UFS)
	return VTOI(vnode)->i_dev;
    /* otherwise stat the file */
    VOP_GETATTR(vnode, &attr, NULL, error);
    if (error)
	osi_Panic("VOP_GETATTR = %d", error);
    return attr.va_rdev;
}

getinode(fs, dev, inode, ipp, perror)
     struct mount *fs;
     struct inode **ipp;
     dev_t dev;
     ino_t inode;
     int *perror;
{
    register struct vnode *vp;
    char fake_vnode[FAKE_INODE_SIZE];
    struct inode *ip;
    int code;

    *ipp = 0;
    *perror = 0;
    if (!fs) {
	register struct ufsmount *ump;
	register struct vnode *vp;
	register struct mount *mp;

	MOUNTLIST_LOCK();
	if (mp = rootfs)
	    do {
		/*
		 * XXX Also do the test for MFS 
		 */
#undef m_data
#undef m_next
		if (mp->m_stat.f_type == MOUNT_UFS) {
		    MOUNTLIST_UNLOCK();
		    ump = VFSTOUFS(mp);
		    if (ump->um_fs == NULL)
			break;
		    if (ump->um_dev == dev) {
			fs = ump->um_mountp;
		    }
		    MOUNTLIST_LOCK();
		}
#ifdef AFS_DUX50_ENV
#define m_next m_nxt
#endif
		mp = mp->m_next;
	    } while (mp != rootfs);
	MOUNTLIST_UNLOCK();
	if (!fs)
	    return (ENXIO);
    }
    vp = (struct vnode *)fake_vnode;
    fake_inode_init(vp, fs);
    assert(vp->v_tag == VT_UFS);
    code = iget(VTOI(vp), inode, &ip, 0);
    if (code != 0) {
	*perror = BAD_IGET;
	return code;
    } else {
	*ipp = ip;
	return (0);
    }
}

igetinode(vfsp, dev, inode, ipp, perror)
     struct inode **ipp;
     struct mount *vfsp;
     dev_t dev;
     ino_t inode;
     int *perror;
{
    struct inode *pip, *ip;
    extern struct osi_dev cacheDev;
    register int code = 0;

    *perror = 0;

    AFS_STATCNT(igetinode);

    if ((code = getinode(vfsp, dev, inode, &ip, perror)) != 0) {
	return (code);
    }

    if (ip->i_mode == 0) {
	/* Not an allocated inode */
	iforget(ip);
	return (ENOENT);
    }

    if (ip->i_nlink == 0 || (ip->i_mode & IFMT) != IFREG) {
	iput(ip);
	return (ENOENT);
    }

    *ipp = ip;
    return (0);
}

iforget(ip)
     struct inode *ip;
{
    struct vnode *vp = ITOV(ip);

    AFS_STATCNT(iforget);

    VN_LOCK(vp);
    /* this whole thing is too wierd.  Why??? XXX */
    if (vp->v_usecount == 1) {
	VN_UNLOCK(vp);
	idrop(ip);
    } else {
	VN_UNLOCK(vp);
    }
}

/*
 * icreate system call -- create an inode
 */
afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4, retval)
     long *retval;
     long dev, near_inode, param1, param2, param3, param4;
{
    int dummy, err = 0;
    struct inode *ip, *newip;
    register int code;
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

    if (!vp->v_object) {
	extern struct vfs_ubcops ufs_ubcops;
	extern struct vm_ubc_object *ubc_object_allocate();
	struct vm_ubc_object *vop;
	vop = ubc_object_allocate(&vp, &ufs_ubcops, vp->v_mount->m_funnel);
	VN_LOCK(vp);
	vp->v_object = vop;
	VN_UNLOCK(vp);
    }


    IN_LOCK(newip);
    newip->i_flags |= IC_XUID | IC_XGID;
    newip->i_flags &= ~IC_PROPLIST;
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
    register int code;
#ifdef AFS_DUX50_ENV
    struct ufile_entry *fe;
#endif

    AFS_STATCNT(afs_syscall_iopen);

    if (!afs_suser(NULL))
	return (EPERM);

    code = igetinode(0, (dev_t) dev, (ino_t) inode, &ip, &dummy);
    if (code) {
	return (code);
    }
#ifdef AFS_DUX50_ENV
    if ((code = falloc(&fp, &fd, &fe)) != 0) {
	iput(ip);
	return (code);
    }
#else
    if ((code = falloc(&fp, &fd)) != 0) {
	iput(ip);
	return (code);
    }
#endif
    IN_UNLOCK(ip);

    FP_LOCK(fp);
    fp->f_flag = (usrmod - FOPEN) & FMASK;
    fp->f_type = DTYPE_VNODE;
    fp->f_ops = &vnops;
    fp->f_data = (caddr_t) ITOV(ip);

    FP_UNLOCK(fp);
#ifdef AFS_DUX50_ENV
    u_set_fe(fd, fe, fp, &u.u_file_state);
#else
    U_FD_SET(fd, fp, &u.u_file_state);
#endif
    *retval = fd;
    return (0);
}


/*
 * Support for iinc() and idec() system calls--increment or decrement
 * count on inode.
 * Restricted to super user.
 * Only VICEMAGIC type inodes.
 */
afs_syscall_iincdec(dev, inode, inode_p1, amount)
     int dev, inode, inode_p1, amount;
{
    int dummy;
    struct inode *ip;
    register int code;

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
