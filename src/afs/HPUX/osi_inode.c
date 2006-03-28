/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * HPUX inode operations
 *
 * Implements:
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/HPUX/osi_inode.c,v 1.8.2.2 2006/02/17 17:35:33 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/osi_inode.h"
#include "afs/afs_stats.h"	/* statistics stuff */
#include <sys/mount.h>


static struct inode *
getinode(struct vfs *vfsp, dev_t dev, ino_t inode, int *perror)
{
    struct mount *mp = (vfsp ? VFSTOM(vfsp) : 0);
    struct inode *pip;
    *perror = 0;

    if (!mp && !(mp = getmp(dev))) {
	u.u_error = ENXIO;
	return (NULL);
    }
    pip = iget(dev, mp, inode);
    if (!pip)
	*perror = BAD_IGET;
    return (pip);
}

struct inode *
igetinode(struct vfs *vfsp, dev_t dev, ino_t inode, int *perror)
{
    struct inode *pip, *ip;
    extern struct osi_dev cacheDev;
    register int code = 0;

    *perror = 0;
    AFS_STATCNT(igetinode);
    ip = getinode(vfsp, dev, inode, perror);
    if (ip == NULL) {
	*perror = BAD_IGET;
	u.u_error = ENOENT;	/* Well... */
	return;
    }
    if (ip->i_mode == 0) {
	/* Not an allocated inode */
	iforget(ip);
	u.u_error = ENOENT;
	return;
    }
    if (ip->i_nlink == 0 || (ip->i_mode & IFMT) != IFREG) {
	iput(ip);
	u.u_error = ENOENT;
	return;
    }
    return ip;
}

iforget(ip)
     struct inode *ip;
{
    idrop(ip);
}

afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4)
     long dev, near_inode, param1, param2, param3, param4;
{
    int dummy, err = 0;
    struct inode *ip, *newip;

    AFS_STATCNT(afs_syscall_icreate);

    if (!afs_suser(NULL)) {
	u.u_error = EPERM;
	goto out;
    }

    ip = getinode(0, (dev_t) dev, 2, &dummy);
    if (ip == NULL) {
	u.u_error = ENOENT;	/* Well... */
	goto out;
    }

    newip = (struct inode *)ialloc(ip, near_inode, 0);
    iput(ip);
    if (newip == NULL)
	goto out;

    newip->i_flag |= IACC | IUPD | ICHG;
    newip->i_nlink = 1;
    newip->i_mode = IFREG;
    newip->i_vnode.v_type = VREG;
    newip->i_vicemagic = VICEMAGIC;
    newip->i_vicep1 = param1;
    newip->i_vicep2 = param2;
    I_VICE3(newip) = param3;
    newip->i_vicep4 = param4;
    u.u_r.r_val1 = newip->i_number;

    iput(newip);

  out:
    return err;
}

afs_syscall_iopen(dev, inode, usrmod)
     int dev, inode, usrmod;
{
    struct file *fp;
    struct inode *ip;
    struct vnode *vp = NULL;
    int dummy;
    extern struct fileops vnodefops;
    register int code;
    int fd;

    AFS_STATCNT(afs_syscall_iopen);

    if (!afs_suser(NULL)) {
	u.u_error = EPERM;
	goto out;
    }

    ip = igetinode(0, (dev_t) dev, (ino_t) inode, &dummy);
    if (u.u_error)
	goto out;
    fp = falloc();
    if (!fp) {
	iput(ip);
	goto out;
    }
    fd = u.u_r.r_val1;
    iunlock(ip);

    fp->f_ops = &vnodefops;
    vp = ITOV(ip);
    fp->f_data = (char *)vp;
    fp->f_type = DTYPE_VNODE;
    fp->f_flag = (usrmod + 1) & (FMASK);

    /* Obtained from hp kernel sys/vfs_scalls.c: copen().
     * Otherwise we panic because the v_writecount
     * goes less than 0 during close.
     */
    if ((vp->v_type == VREG) && (fp->f_flag & FWRITE)) {
	VN_INC_WRITECOUNT(vp);
    }

    /* fp->f_count, f_msgcount are set by falloc */
    /* fp->f_offset zeroed by falloc */
    /* f_cred set by falloc */

    /*
     * Obtained from hp kernel sys/vfs_scalls.c: copen() does
     * a PUTF() (defined earlier in the file) before returning,
     * so we parrot what it does.  If this is not done, then
     * threaded processes will get EBADF errors when they try
     * to use the resulting file descriptor (e.g. with lseek()).
     *
     * Note: u.u_r.r_val1 is set by ufalloc(), which is
     * called by falloc(), which is called above.
     */
    if (is_multithreaded(u.u_procp)) {
	putf(fd);
    }

  out:
    return;
}

afs_syscall_iincdec(dev, inode, inode_p1, amount)
     int dev, inode, inode_p1, amount;
{
    int dummy;
    struct inode *ip;
    register afs_int32 code;

    if (!afs_suser(NULL)) {
	u.u_error = EPERM;
	goto out;
    }

    ip = igetinode(0, (dev_t) dev, (ino_t) inode, &dummy);
    if (u.u_error) {
	goto out;
    }

    if (!IS_VICEMAGIC(ip))
	u.u_error = EPERM;
    else if (ip->i_vicep1 != inode_p1)
	u.u_error = ENXIO;
    else {
	ip->i_nlink += amount;
	if (ip->i_nlink == 0) {
	    CLEAR_VICEMAGIC(ip);
	}
	ip->i_flag |= ICHG;
    }

    iput(ip);

  out:
    return;
}
