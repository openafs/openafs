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
#include "../afs/param.h"       /* Should be always first */
#include "../afs/sysincludes.h" /* Standard vendor system headers */
#include "../afs/afsincludes.h" /* Afs-based standard headers */
#include "../afs/osi_inode.h"
#include "../afs/afs_stats.h" /* statistics stuff */
#include <ufs/ufs/ufsmount.h>
extern struct ucred afs_osi_cred;

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
	if (mp = rootfs) do {
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
	    return(ENXIO);
    }
    code=VFS_VGET(fs, (void *)inode, &vp);
    if (code) {
	*perror = BAD_IGET;
	return code;
    } else { 
	*vpp = vp;
	return(0);
    }
}
extern int afs_CacheFSType;
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
	return(code);
    }
    if (vp->v_type != VREG && vp->v_type != VDIR && vp->v_type != VLNK) {
        printf("igetinode: bad type %d\n", vp->v_type);
	iforget(vp);        
	return(ENOENT);
    }
    VOP_GETATTR(vp, va, &afs_osi_cred, current_proc());
    if (va->va_mode == 0) {
	/* Not an allocated inode */
	iforget(vp);        
	return(ENOENT);
    }
    if (vfsp && afs_CacheFSType == AFS_APPL_HFS_CACHE && va->va_nlink == 0) {
        printf("igetinode: hfs nlink 0\n");
    }
    if (va->va_nlink == 0) {
	vput(vp);
	return(ENOENT);
    }

    VOP_UNLOCK(vp, 0, current_proc());
    *vpp = vp;
    return(0);
}

iforget(vp)
struct vnode *vp;
{

    AFS_STATCNT(iforget);
    /* XXX could sleep */
    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, current_proc());
    /* this whole thing is too wierd.  Why??? XXX */
    if (vp->v_usecount == 1) {
	vp->v_usecount=0;
	VOP_UNLOCK(vp,0, current_proc());
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

#if 0
/*
 * icreate system call -- create an inode
 */
afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4, retval)
     long *retval;
     long dev, near_inode, param1, param2, param3, param4;
{
    int dummy, err=0;
    struct inode *ip, *newip;
    register int code;
    struct vnode *vp;
    
    AFS_STATCNT(afs_syscall_icreate);
    
    if (!afs_suser())
	return(EPERM);

    code = getinode(0, (dev_t)dev, 2, &ip, &dummy);
    if (code) {
	return(ENOENT);
    }
    code = ialloc(ip, (ino_t)near_inode, 0, &newip);
    iput(ip);
    if (code) {
	return(code);
    }
    IN_LOCK(newip);
    newip->i_flag |= IACC|IUPD|ICHG;
    
    newip->i_nlink = 1;

    newip->i_mode = IFREG;
    
    IN_UNLOCK(newip);
    vp = ITOV(newip);
    VN_LOCK(vp);
    vp->v_type = VREG;
    VN_UNLOCK(vp);
    
    if ( !vp->v_object)
	{
	    extern struct vfs_ubcops ufs_ubcops;
	    extern struct vm_ubc_object* ubc_object_allocate();
	    struct vm_ubc_object* vop;
	    vop = ubc_object_allocate(&vp, &ufs_ubcops,
	                              vp->v_mount->m_funnel);
	    VN_LOCK(vp);
	    vp->v_object = vop;
	    VN_UNLOCK(vp);
	}
    
    
    IN_LOCK(newip);
    newip->i_flags |= IC_XUID|IC_XGID;
    newip->i_flags &= ~IC_PROPLIST;
    newip->i_vicep1 = param1;
    if (param2 == 0x1fffffff/*INODESPECIAL*/)   {
	newip->i_vicep2 = ((0x1fffffff << 3) + (param4 & 0x3));
	newip->i_vicep3a = (u_short)(param3 >> 16);
	newip->i_vicep3b = (u_short)param3;
    } else {
	newip->i_vicep2 = (((param2 >> 16) & 0x1f) << 27) +
	    (((param4 >> 16) & 0x1f) << 22) + 
	        (param3 & 0x3fffff);        
	newip->i_vicep3a = (u_short)param4;
	newip->i_vicep3b = (u_short)param2;
    }
    newip->i_vicemagic = VICEMAGIC;
    
    *retval = newip->i_number;
    IN_UNLOCK(newip);
    iput(newip);
    return(code);
}


afs_syscall_iopen(dev, inode, usrmod, retval)
     long *retval;
     int dev, inode, usrmod;
{
    struct file *fp;
    struct inode *ip;
    struct vnode *vp = (struct vnode *)0;
    int dummy;
    int fd;
    extern struct fileops vnops;
    register int code;
    
    AFS_STATCNT(afs_syscall_iopen);
    
    if (!afs_suser())
	return(EPERM);

    code = igetinode(0, (dev_t)dev, (ino_t)inode, &ip, &dummy);
    if (code) {
	return(code);
    }
    if ((code = falloc(&fp, &fd)) != 0) {
	iput(ip);
	return(code);
    }
    IN_UNLOCK(ip);
    
    FP_LOCK(fp);
    fp->f_flag = (usrmod-FOPEN) & FMASK;
    fp->f_type = DTYPE_VNODE;
    fp->f_ops = &vnops;
    fp->f_data = (caddr_t)ITOV(ip);
    
    FP_UNLOCK(fp);
    U_FD_SET(fd, fp, &u.u_file_state);
    *retval = fd;
    return(0);
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
    
    if (!afs_suser())
	return(EPERM);

    code = igetinode(0, (dev_t)dev, (ino_t)inode, &ip, &dummy);
    if (code) {
	return(code);
    }
    if (!IS_VICEMAGIC(ip)) {
	return(EPERM);
    } else if (ip->i_vicep1 != inode_p1) {
	return(ENXIO);
    }
    ip->i_nlink += amount;
    if (ip->i_nlink == 0) {
	CLEAR_VICEMAGIC(ip);
    }
    ip->i_flag |= ICHG;
    iput(ip);
    return(0);
}
#else
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
#endif
