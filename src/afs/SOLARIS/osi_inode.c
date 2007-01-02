/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * SOLARIS inode operations
 *
 * Implements:
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/osi_inode.h"
#include "afs/afs_stats.h"	/* statistics stuff */

extern int (*ufs_iallocp) (), (*ufs_iupdatp) (), (*ufs_igetp) (),
    (*ufs_itimes_nolockp) ();

#define AFS_ITIMES(ip) { \
        mutex_enter(&(ip)->i_tlock); \
        (*ufs_itimes_nolockp)(ip); \
        mutex_exit(&(ip)->i_tlock); \
}

#define AFS_ITIMES_NOLOCK(ip) \
	(*ufs_itimes_nolockp)(ip);

getinode(vfsp, dev, inode, ipp, credp, perror)
     struct vfs *vfsp;
     struct AFS_UCRED *credp;
     struct inode **ipp;
     dev_t dev;
     ino_t inode;
     int *perror;
{
    struct inode *ip;
    register afs_int32 code;
    struct vnode *vp;
    struct fs *fs;
    struct inode *pip;
    struct ufsvfs *ufsvfsp;

    AFS_STATCNT(getinode);

    *perror = 0;

    if (!vfsp
#if !defined(AFS_SUN58_ENV)
	&& !(vfsp = vfs_devsearch(dev))
#else
	&& !(vfsp = vfs_dev2vfsp(dev))
#endif
	) {
	return (ENODEV);
    }
    ufsvfsp = (struct ufsvfs *)vfsp->vfs_data;

#ifdef HAVE_VFS_DQRWLOCK
    rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
#endif
    code = (*ufs_igetp) (vfsp, inode, &ip, credp);
#ifdef HAVE_VFS_DQRWLOCK
    rw_exit(&ufsvfsp->vfs_dqrwlock);
#endif

    if (code) {
	*perror = BAD_IGET;
	return code;
    }
    *ipp = ip;
    return code;
}

/* get an existing inode.  Common code for iopen, iread/write, iinc/dec. */
igetinode(vfsp, dev, inode, ipp, credp, perror)
     struct AFS_UCRED *credp;
     struct inode **ipp;
     struct vfs *vfsp;
     dev_t dev;
     ino_t inode;
     int *perror;
{
    struct inode *pip, *ip;
    extern struct osi_dev cacheDev;
    register int code = 0;

    *perror = 0;

    AFS_STATCNT(igetinode);

    code = getinode(vfsp, dev, inode, &ip, credp, perror);
    if (code)
	return code;

    rw_enter(&ip->i_contents, RW_READER);

    if (ip->i_mode == 0) {
	/* Not an allocated inode */
	rw_exit(&ip->i_contents);
	VN_RELE(ITOV(ip));
	return (ENOENT);
    }

    if (ip->i_nlink == 0 || (ip->i_mode & IFMT) != IFREG) {
	AFS_ITIMES(ip);
	rw_exit(&ip->i_contents);
	VN_RELE(ITOV(ip));
	return (ENOENT);
    }

    /* On VFS40 systems, iput does major synchronous write action, but only
     * when the reference count on the vnode goes to 0.  Normally, Sun users
     * don't notice this because the DNLC keep references for them, but we
     * notice 'cause we don't.  So, we make a fake dnlc entry which gets
     * cleaned up by iget when it needs the space. */
    if (dev != cacheDev.dev) {
	/* 
	 * Don't call dnlc for the cm inodes since it's a big performance 
	 * penalty there!
	 */
#ifdef AFS_SUN510_ENV
	dnlc_enter(ITOV(ip), "a", ITOV(ip));
#else
	dnlc_enter(ITOV(ip), "a", ITOV(ip), (struct AFS_UCRED *)0);
#endif
    }

    *ipp = ip;
    rw_exit(&ip->i_contents);
    return (code);
}

int CrSync = 1;

afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4, rvp,
		    credp)
     rval_t *rvp;
     struct AFS_UCRED *credp;
     long near_inode, param1, param2, param3, param4;
     dev_t dev;
{
    int dummy, err = 0;
    struct inode *ip, *newip;
    register int code;
    dev_t newdev;
    struct ufsvfs *ufsvfsp;

    AFS_STATCNT(afs_syscall_icreate);

    if (!afs_osi_suser(credp))
	return (EPERM);

    /** Code to convert a 32 bit dev_t into a 64 bit dev_t
      * This conversion is needed only for the 64 bit OS.
      */

#ifdef AFS_SUN57_64BIT_ENV
    newdev = expldev((dev32_t) dev);
#else
    newdev = dev;
#endif

    code = getinode(0, (dev_t) newdev, 2, &ip, credp, &dummy);
    if (code) {
	return (code);
    }

    ufsvfsp = ip->i_ufsvfs;
    rw_enter(&ip->i_rwlock, RW_WRITER);
#ifdef HAVE_VFS_DQRWLOCK
    rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
#endif
    rw_enter(&ip->i_contents, RW_WRITER);
    code = (*ufs_iallocp) (ip, near_inode, 0, &newip, credp);
    AFS_ITIMES_NOLOCK(ip);
    rw_exit(&ip->i_contents);
#ifdef HAVE_VFS_DQRWLOCK
    rw_exit(&ufsvfsp->vfs_dqrwlock);
#endif
    rw_exit(&ip->i_rwlock);
    VN_RELE(ITOV(ip));

    if (code) {
	return (code);
    }
    rw_enter(&newip->i_contents, RW_WRITER);
    newip->i_flag |= IACC | IUPD | ICHG;

#if	defined(AFS_SUN56_ENV)
    newip->i_vicemagic = VICEMAGIC;
#else
    newip->i_uid = 0;
    newip->i_gid = -2;
#endif
    newip->i_nlink = 1;
    newip->i_mode = IFREG;
#ifdef AFS_SUN510_ENV
    newip->i_vnode->v_type = VREG;
#else
    newip->i_vnode.v_type = VREG;
#endif

    newip->i_vicep1 = param1;
    if (param2 == 0x1fffffff /*INODESPECIAL*/) {
	newip->i_vicep2 = ((0x1fffffff << 3) + (param4 & 0x3));
	newip->i_vicep3 = param3;
    } else {
	newip->i_vicep2 =
	    (((param2 >> 16) & 0x1f) << 27) +
	    (((param4 >> 16) & 0x1f) << 22) + (param3 & 0x3fffff);
	newip->i_vicep3 = ((param4 << 16) + (param2 & 0xffff));
    }
#ifdef AFS_SUN57_64BIT_ENV
    rvp->r_vals = newip->i_number;
#else
    rvp->r_val1 = newip->i_number;
#endif

    /* 
     * We're being conservative and sync to the disk
     */
    if (CrSync)
	(*ufs_iupdatp) (newip, 1);
    AFS_ITIMES_NOLOCK(newip);
    rw_exit(&newip->i_contents);
    VN_RELE(ITOV(newip));
    return (code);
}

afs_syscall_iopen(dev, inode, usrmod, rvp, credp)
     rval_t *rvp;
     struct AFS_UCRED *credp;
     int inode, usrmod;
     dev_t dev;
{
    struct file *fp;
    struct inode *ip;
    struct vnode *vp = NULL;
    int dummy;
    int fd;
    register int code;
    dev_t newdev;

    AFS_STATCNT(afs_syscall_iopen);

    if (!afs_osi_suser(credp))
	return (EPERM);

    /** Code to convert a 32 bit dev_t into a 64 bit dev_t
      * This conversion is needed only for the 64 bit OS.
      */

#ifdef AFS_SUN57_64BIT_ENV
    newdev = expldev((dev32_t) dev);
#else
    newdev = dev;
#endif

    code = igetinode(0, (dev_t) newdev, (ino_t) inode, &ip, credp, &dummy);
    if (code) {
	return (code);
    }
    code = falloc((struct vnode *)NULL, FWRITE | FREAD, &fp, &fd);
    if (code) {
	rw_enter(&ip->i_contents, RW_READER);
	AFS_ITIMES(ip);
	rw_exit(&ip->i_contents);
	VN_RELE(ITOV(ip));
	return (code);
    }

    /* fp->f_count, f_audit_data are set by falloc */
    fp->f_vnode = ITOV(ip);

    fp->f_flag = (usrmod + 1) & (FMASK);

    /* fp->f_count, f_msgcount are set by falloc */

    /* fp->f_offset zeroed by falloc */
    /* f_cred set by falloc */
    /*
     * falloc returns the fp write locked 
     */
    mutex_exit(&fp->f_tlock);
    /*
     * XXX We should set the fp to null since we don't need it in the icalls
     */
    setf(fd, fp);
#ifdef AFS_SUN57_64BIT_ENV
    rvp->r_val2 = fd;
#else
    rvp->r_val1 = fd;
#endif

    return code;
}

int IncSync = 1;

afs_syscall_iincdec(dev, inode, inode_p1, amount, rvp, credp)
     rval_t *rvp;
     struct AFS_UCRED *credp;
     int inode, inode_p1, amount;
     dev_t dev;
{
    int dummy;
    struct inode *ip;
    register afs_int32 code;
    dev_t newdev;

    if (!afs_osi_suser(credp))
	return (EPERM);

	/** Code to convert a 32 bit dev_t into a 64 bit dev_t
      * This conversion is needed only for the 64 bit OS.
      */

#ifdef AFS_SUN57_64BIT_ENV
    newdev = expldev((dev32_t) dev);
#else
    newdev = dev;
#endif

    code = igetinode(0, (dev_t) newdev, (ino_t) inode, &ip, credp, &dummy);
    if (code) {
	return (code);
    }
    if (!IS_VICEMAGIC(ip)) {
	code = EPERM;
	rw_enter(&ip->i_contents, RW_READER);
	AFS_ITIMES(ip);
	rw_exit(&ip->i_contents);
	VN_RELE(ITOV(ip));
    } else {
	rw_enter(&ip->i_contents, RW_WRITER);
	ip->i_nlink += amount;
	if (ip->i_nlink == 0) {
	    /* remove the "a" name added by igetinode so that the space is reclaimed. */
	    dnlc_remove(ITOV(ip), "a");
	    CLEAR_VICEMAGIC(ip);
	}
	ip->i_flag |= ICHG;
	/* We may want to force the inode to the disk in case of crashes, other references, etc. */
	if (IncSync)
	    (*ufs_iupdatp) (ip, 1);
	AFS_ITIMES_NOLOCK(ip);
	rw_exit(&ip->i_contents);
	VN_RELE(ITOV(ip));
    }
    return (code);
}
