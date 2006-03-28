/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
/*
 * MACOS inode operations
 *
 * Implements:
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/DARWIN/osi_inode.c,v 1.7.2.1 2005/10/05 05:58:29 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/osi_inode.h"
#include "afs/afs_stats.h"	/* statistics stuff */
#ifndef AFS_DARWIN80_ENV
#include <ufs/ufs/ufsmount.h>
#endif
extern struct ucred afs_osi_cred;
extern int afs_CacheFSType;

#ifdef AFS_DARWIN80_ENV
getinode(fs, dev, inode, vpp, perror)
    mount_t fs;
    vnode_t *vpp;
    dev_t dev;
    ino_t inode;
    int *perror;
{
    struct vnode *vp;
    int code;
    vfs_context_t ctx;
    char volfspath[64];
    
    *vpp = 0;
    *perror = 0;
    sprintf(volfspath, "/.vol/%d/%d", dev, inode);
    code = vnode_open(volfspath, O_RDWR, 0, 0, &vp, afs_osi_ctxtp);
    if (code) {
	*perror = BAD_IGET;
	return code;
    } else {
	*vpp = vp;
	return (0);
    }
}    
    
  
igetinode(vfsp, dev, inode, vpp, va, perror)
    vnode_t *vpp;
    mount_t vfsp;
    dev_t dev;
    ino_t inode;
    struct vattr *va;
    int *perror;
{
    vnode_t pvp, vp;
    extern struct osi_dev cacheDev;
    register int code = 0;
    
    *perror = 0;
    
    AFS_STATCNT(igetinode);
    if ((code = getinode(vfsp, dev, inode, &vp, perror)) != 0) {
	return (code);
    }
    if (vnode_vtype(vp) != VREG && vnode_vtype(vp) != VDIR && vnode_vtype(vp) != VLNK) {
	vnode_close(vp, O_RDWR, afs_osi_ctxtp);
	printf("igetinode: bad type %d\n", vnode_vtype(vp));
	return (ENOENT);
    }
    VATTR_INIT(va);
    VATTR_WANTED(va, va_mode);
    VATTR_WANTED(va, va_nlink);
    VATTR_WANTED(va, va_size);
    code = vnode_getattr(vp, va, afs_osi_ctxtp);
    if (code) {
	vnode_close(vp, O_RDWR, afs_osi_ctxtp);
        return code;
    }
    if (!VATTR_ALL_SUPPORTED(va)) {
	vnode_close(vp, O_RDWR, afs_osi_ctxtp);
        return ENOENT;
    }
    if (va->va_mode == 0) {
	vnode_close(vp, O_RDWR, afs_osi_ctxtp);
	/* Not an allocated inode */
	return (ENOENT);
    }
    if (va->va_nlink == 0) {
	vnode_close(vp, O_RDWR, afs_osi_ctxtp);
	return (ENOENT);
    }
    
    *vpp = vp;
    return (0);
}
#else
getinode(fs, dev, inode, vpp, perror)
     struct mount *fs;
     struct vnode **vpp;
     dev_t dev;
     ino_t inode;
     int *perror;
{
    struct vnode *vp;
    int code;

    *vpp = 0;
    *perror = 0;
    if (!fs) {
	register struct ufsmount *ump;
#ifdef VFSTOHFS
	register struct hfsmount *hmp;
#endif
	register struct vnode *vp;
	register struct mount *mp;
	extern struct mount *rootfs;
	if (mp = rootfs)
	    do {
		/*
		 * XXX Also do the test for MFS 
		 */
		if (!strcmp(mp->mnt_vfc->vfc_name, "ufs")) {
		    ump = VFSTOUFS(mp);
		    if (ump->um_fs == NULL)
			break;
		    if (ump->um_dev == dev) {
			fs = ump->um_mountp;
		    }
		}
#ifdef VFSTOHFS
		if (!strcmp(mp->mnt_vfc->vfc_name, "hfs")) {
		    hmp = VFSTOHFS(mp);
#if 0
		    if (hmp->hfs_mp == NULL)
			break;
#endif
		    if (hmp->hfs_raw_dev == dev) {
			fs = hmp->hfs_mp;
		    }
		}
#endif

		mp = CIRCLEQ_NEXT(mp, mnt_list);
	    } while (mp != rootfs);
	if (!fs)
	    return (ENXIO);
    }
    code = VFS_VGET(fs, (void *)inode, &vp);
    if (code) {
	*perror = BAD_IGET;
	return code;
    } else {
	*vpp = vp;
	return (0);
    }
}

igetinode(vfsp, dev, inode, vpp, va, perror)
     struct vnode **vpp;
     struct mount *vfsp;
     dev_t dev;
     ino_t inode;
     struct vattr *va;
     int *perror;
{
    struct vnode *pvp, *vp;
    extern struct osi_dev cacheDev;
    register int code = 0;

    *perror = 0;

    AFS_STATCNT(igetinode);
    if ((code = getinode(vfsp, dev, inode, &vp, perror)) != 0) {
	return (code);
    }
    if (vp->v_type != VREG && vp->v_type != VDIR && vp->v_type != VLNK) {
	printf("igetinode: bad type %d\n", vp->v_type);
	iforget(vp);
	return (ENOENT);
    }
    VOP_GETATTR(vp, va, &afs_osi_cred, current_proc());
    if (va->va_mode == 0) {
	/* Not an allocated inode */
	iforget(vp);
	return (ENOENT);
    }
    if (vfsp && afs_CacheFSType == AFS_APPL_HFS_CACHE && va->va_nlink == 0) {
	printf("igetinode: hfs nlink 0\n");
    }
    if (va->va_nlink == 0) {
	vput(vp);
	return (ENOENT);
    }

    VOP_UNLOCK(vp, 0, current_proc());
    *vpp = vp;
    return (0);
}

iforget(vp)
     struct vnode *vp;
{

    AFS_STATCNT(iforget);
    /* XXX could sleep */
    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, current_proc());
    /* this whole thing is too wierd.  Why??? XXX */
    if (vp->v_usecount == 1) {
	vp->v_usecount = 0;
	VOP_UNLOCK(vp, 0, current_proc());
#if 0
	simple_lock(&vnode_free_list_slock);
	TAILQ_INSERT_TAIL(&vnode_free_list, vp, v_freelist);
	freevnodes++;
	simple_unlock(&vnode_free_list_slock);
#else
	printf("iforget: leaking vnode\n");
#endif
    } else {
	vput(vp);
    }
}
#endif

afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4, retval)
     long *retval;
     long dev, near_inode, param1, param2, param3, param4;
{
    return EOPNOTSUPP;
}

afs_syscall_iopen(dev, inode, usrmod, retval)
     long *retval;
     int dev, inode, usrmod;
{
    return EOPNOTSUPP;
}

afs_syscall_iincdec(dev, inode, inode_p1, amount)
     int dev, inode, inode_p1, amount;
{
    return EOPNOTSUPP;
}
