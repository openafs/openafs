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


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/osi_inode.h"
#include "afs/afs_stats.h"	/* statistics stuff */
#ifndef AFS_DARWIN80_ENV
#include <ufs/ufs/ufsmount.h>
#endif
extern struct ucred *afs_osi_credp;
extern int afs_CacheFSType;

#ifdef AFS_DARWIN80_ENV
int
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
    size_t len = sizeof(volfspath);
    
    *vpp = 0;
    *perror = 0;
    if (snprintf(volfspath, len, "/.vol/%d/%d", dev, inode) >= len) {
	*perror = BAD_IGET;
	return ENAMETOOLONG;
    }
    code = vnode_open(volfspath, O_RDWR, 0, 0, &vp, afs_osi_ctxtp);
    if (code) {
	*perror = BAD_IGET;
	return code;
    } else {
	*vpp = vp;
	return (0);
    }
}    
    
int
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
    int code = 0;
    
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
	struct ufsmount *ump;
#ifdef VFSTOHFS
	struct hfsmount *hmp;
#endif
	struct vnode *vp;
	struct mount *mp;
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
    int code = 0;

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
    VOP_GETATTR(vp, va, afs_osi_credp, current_proc());
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
	printf("iforget: leaking vnode\n");
    } else {
	vput(vp);
    }
}
#endif

int
afs_syscall_icreate(long dev, long near_inode, long param1, long param2, 
		    long param3, long param4, long *retval)
{
    return ENOTSUP;
}

int
afs_syscall_iopen(int dev, int inode, int usrmod, long *retval)
{
    return ENOTSUP;
}

int
afs_syscall_iincdec(int dev, int inode, int inode_p1, int amount)
{
    return ENOTSUP;
}
