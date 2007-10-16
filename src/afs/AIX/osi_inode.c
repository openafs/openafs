/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * AIX inode operations
 *
 * Implements:
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/AIX/osi_inode.c,v 1.8.2.3 2007/08/16 03:54:26 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/osi_inode.h"
#include "afs/afs_stats.h"	/* statistics stuff */
#include "sys/syspest.h"

#if !defined(offsetof)
#include <stddef.h>		/* for definition of offsetof() */
#endif

extern Simple_lock jfs_icache_lock;
#define ICACHE_LOCK()           simple_lock(&jfs_icache_lock)
#define ICACHE_UNLOCK()         simple_unlock(&jfs_icache_lock)

/*
 * In AIX 4.2.0, the inode member to lock is i_rdwrlock.  The inode
 * structure conditionally contains a member called i_nodelock which we
 * do NOT want to use.
 *
 * In AIX 4.2.1 and later, the inode member to lock is i_nodelock, which
 * was relocated to coincide with the AIX 4.2.0 position of i_rdwrlock.
 * The AIX 4.2.1 and later inode structure does not contain a field
 * named i_rdwrlock.
 *
 * We use an accident of the system header files to distinguish between
 * AIX 4.2.0 and AIX 4.2.1 and later.  This allows this part of the code
 * to compile on AIX 4.2.1 and later without introducing a new system
 * type.
 *
 * The macro IACTIVITY is 0x0020 on AIX 4.2.0, and 0x0010 on AIX 4.2.1
 * and later (at least through AIX 4.3.3).  If IACTIVITY is undefined,
 * or has an unknown value, then the system header files are different
 * than what we've seen and we'll need to take a look at them anyway to
 * get AFS to work.
 *
 * The osi_Assert() statement in igetinode() checks that the lock field
 * is at an expected offset.
 */

#if IACTIVITY == 0x0020		/* in <jfs/inode.h> on AIX 4.2.0 */
#define afs_inode_lock	i_rdwrlock
#endif

#if IACTIVITY == 0x0010		/* in <jfs/inode.h> on AIX 4.2.1 and later */
#define afs_inode_lock	i_nodelock
#endif

#define IREAD_LOCK(ip)          simple_lock(&((ip)->afs_inode_lock))
#define IREAD_UNLOCK(ip)        simple_unlock(&((ip)->afs_inode_lock))
#define IWRITE_LOCK(ip)         simple_lock(&((ip)->afs_inode_lock))
#define IWRITE_UNLOCK(ip)       simple_unlock(&((ip)->afs_inode_lock))

#define	SYSENT(name, arglist, decls)			\
name arglist						\
decls {							\
	lock_t lockt;					\
	int rval1 = 0;					\
	label_t jmpbuf;					\
	int rc;						\
							\
							\
 	setuerror(0);					\
							\
	if ((rc = setjmpx(&jmpbuf)) == 0) {		\
		rval1 = afs_syscall_ ## name arglist;	\
		clrjmpx(&jmpbuf);			\
	} else	{					\
		if (rc != EINTR) {			\
			longjmpx(rc);			\
		}					\
		setuerror(rc);				\
	}						\
							\
							\
	return(getuerror() ? -1 : rval1);		\
}							\
afs_syscall_ ## name arglist				\
decls							\


/*
 * This structure is used to pass information between devtovfs() and
 * devtovfs_func() via vfs_search().
 */

struct devtovfs_args {
    dev_t dev;
    struct vfs *ans;
};

/*
 * If the given vfs matches rock->dev, set rock->ans to vfsp and return
 * nonzero.  Otherwise return zero.
 *
 * (Returning nonzero causes vfs_search() to terminate the search.)
 */

static int
devtovfs_func(struct vfs *vfsp, struct devtovfs_args *rock)
{
    if (vfsp->vfs_mntd != NULL && vfsp->vfs_type == MNT_JFS
	&& (vfsp->vfs_flag & VFS_DEVMOUNT)) {
	struct inode *ip = VTOIP(vfsp->vfs_mntd);
	if (brdev(ip->i_dev) == brdev(rock->dev)) {
	    rock->ans = vfsp;
	    return 1;
	}
    }

    return 0;
}

/*
 * Return the vfs entry for the given device.
 */

static struct vfs *
devtovfs(dev_t dev)
{
    struct devtovfs_args a;

    AFS_STATCNT(devtovfs);

    a.dev = dev;
    a.ans = NULL;
    vfs_search(devtovfs_func, (caddr_t) &a);
    return a.ans;
}

/* Keep error values around in case iget fails. UFSOpen panics when
 * igetinode fails.
 */
int IGI_error;
ino_t IGI_inode;
int IGI_nlink;
int IGI_mode;
/* get an existing inode.  Common code for iopen, iread/write, iinc/dec. */
/* Also used by rmt_remote to support passing of inode number from venus */
extern int iget();
extern struct vnode *filevp;
struct inode *
igetinode(dev, vfsp, inode, vpp, perror)
     struct vfs *vfsp;
     struct vnode **vpp;	/* vnode associated with the inode */
     dev_t dev;
     ino_t inode;
     int *perror;
{
    struct inode *ip;
    register was_locked;
    struct vfs *nvfsp = NULL;
    int code;
    *perror = 0;
    *vpp = NULL;
    AFS_STATCNT(igetinode);

    /*
     * Double check that the inode lock is at a known offset.
     *
     * If it isn't, then we need to reexamine our code to make
     * sure that it is still okay.
     */
#ifdef __64BIT__
/*      osi_Assert(offsetof(struct inode, afs_inode_lock) == 208); */
#else
    osi_Assert(offsetof(struct inode, afs_inode_lock) == 128);
#endif

    if (!vfsp && !(vfsp = devtovfs((dev_t) dev))) {
	afs_warn("Dev=%d not mounted!!; quitting\n", dev);
	setuerror(ENODEV);
	ip = 0;
	goto out;
    }
    if (vfsp->vfs_flag & VFS_DEVMOUNT)
	nvfsp = vfsp;

    /* Check if inode 0. This is the mount inode for the device
     * and will panic the aix system if removed: defect 11434.
     * No file should ever point to this inode.
     */
    if (inode == 0) {
	afs_warn("Dev=%d zero inode.\n", dev);
	setuerror(ENOENT);
	ip = 0;
	goto out;
    }

    ICACHE_LOCK();
#ifdef __64BIT__
    if ((code = iget(dev, inode, &ip, (afs_size_t) 1, nvfsp))) {
#else
    if ((code = iget(dev, inode, &ip, 1, nvfsp))) {
#endif
	IGI_error = code;
	IGI_inode = inode;
	*perror = BAD_IGET;
	ICACHE_UNLOCK();
	setuerror(ENOENT);	/* Well... */
	ip = 0;
	goto out;
    }
    ICACHE_UNLOCK();
    IREAD_LOCK(ip);
    if (ip->i_nlink == 0 || (ip->i_mode & IFMT) != IFREG) {
	IGI_error = 0;
	IGI_inode = inode;
	IGI_nlink = ip->i_nlink;
	IGI_mode = ip->i_mode;
	IREAD_UNLOCK(ip);
	ICACHE_LOCK();
	iput(ip, NULL);
	ICACHE_UNLOCK();
	setuerror(ENOENT);
	ip = 0;
	goto out;
    }
    if (vpp) {
	if (nvfsp)
	    *vpp = ip->i_gnode.gn_vnode;
	else
	    setuerror(iptovp(vfsp, ip, vpp));
    }
    IREAD_UNLOCK(ip);
  out:
    return ip;
}


#ifndef INODESPECIAL
/*
 * `INODESPECIAL' type inodes are ones that describe volumes.  These are
 * marked as journalled.  We would also like to journal inodes corresponding
 * to directory information...
 */
#define INODESPECIAL	0xffffffff	/* ... from ../vol/viceonode.h  */
#endif

SYSENT(icreate, (dev, near_inode, param1, param2, param3, param4),)
{
    struct inode *ip, *newip, *pip;
    register int err, rval1, rc = 0;
    struct vnode *vp = NULL;
    register struct vfs *vfsp;
    struct vfs *nvfsp = NULL;
    char error;
    ino_t ino = near_inode;

    AFS_STATCNT(afs_syscall_icreate);
    if (!suser(&error)) {
	setuerror(error);
	return -1;
    }

    if ((vfsp = devtovfs((dev_t) dev)) == 0) {
	afs_warn("Dev=%d not mounted!!; quitting\n", dev);
	setuerror(ENODEV);
	return -1;
    }
    if (vfsp->vfs_flag & VFS_DEVMOUNT)
	nvfsp = vfsp;
    ICACHE_LOCK();
    rc = iget(dev, 0, &pip, 1, nvfsp);
    if (!rc) {
	/*
	 * this is the mount inode, and thus we should be
	 * safe putting it back.
	 */
	iput(pip, nvfsp);
    }
    ICACHE_UNLOCK();

    if (rc) {
	setuerror(EINVAL);
	return -1;
    }

    if (setuerror(dev_ialloc(pip, ino, IFREG, nvfsp, &newip)))
	return -1;
    newip->i_flag |= IACC | IUPD | ICHG;
    newip->i_gid = -2;		/* Put special gid flag */
    newip->i_vicemagic = VICEMAGIC;
    newip->i_vicep1 = param1;
    newip->i_vicep2 = param2;
    newip->i_vicep3 = param3;
    newip->i_vicep4 = param4;
    IWRITE_UNLOCK(newip);
    if (nvfsp) {
	vp = newip->i_gnode.gn_vnode;
    } else {
	rc = iptovp(vfsp, newip, &vp);
    }
    setuerror(rc);

    rval1 = newip->i_number;
    if (vp) {
	VNOP_RELE(vp);
    }
    return getuerror()? -1 : rval1;
}

SYSENT(iopen, (dev, inode, usrmod),)
{
    struct file *fp;
    register struct inode *ip;
    struct vnode *vp = NULL;
    extern struct fileops vnodefops;
    register struct vfs *vfsp;
    int fd;
    char error;
    struct ucred *credp;
    int dummy;

    AFS_STATCNT(afs_syscall_iopen);
    if (!suser(&error)) {
	setuerror(error);
	return -1;
    }

    if ((vfsp = devtovfs((dev_t) dev)) == 0) {
	afs_warn("Dev=%d not mounted!!; quitting\n", dev);
	setuerror(ENODEV);
	return -1;
    }
    ip = igetinode((dev_t) dev, vfsp, (ino_t) inode, &vp, &dummy);
    if (getuerror())
	return -1;

    credp = crref();
    if (setuerror
	(ufdcreate
	 ((usrmod - FOPEN) & FMASK, &vnodefops, vp, DTYPE_VNODE, &fd,
	  credp))) {
	crfree(credp);
	VNOP_RELE(vp);
	return -1;
    }

    if (setuerror(VNOP_OPEN(vp, (usrmod - FOPEN) & FMASK, 0, 0, credp))) {
	close(fd);
	crfree(credp);
	return -1;
    }
    crfree(credp);
    return fd;
}


/*
 * Support for iinc() and idec() system calls--increment or decrement
 * count on inode.
 * Restricted to super user.
 * Only VICEMAGIC type inodes.
 */
iinc(dev, inode, inode_p1)
{

    AFS_STATCNT(iinc);
    return iincdec(dev, inode, inode_p1, 1);
}

idec(dev, inode, inode_p1)
{

    AFS_STATCNT(idec);
    return iincdec(dev, inode, inode_p1, -1);
}


SYSENT(iincdec, (dev, inode, inode_p1, amount),)
{
    register struct inode *ip;
    char error;
    struct vnode *vp = NULL;
    int dummy;

    AFS_STATCNT(afs_syscall_iincdec);
    if (!suser(&error)) {
	setuerror(error);
	return -1;
    }

    ip = igetinode((dev_t) dev, 0, (ino_t) inode, &vp, &dummy);
    if (getuerror()) {
	return -1;
    }
    IWRITE_LOCK(ip);
    if (ip->i_vicemagic != VICEMAGIC)
	setuerror(EPERM);
    else if (ip->i_vicep1 != inode_p1)
	setuerror(ENXIO);
    else {
	ip->i_nlink += amount;
	if (ip->i_nlink == 0) {
	    ip->i_vicemagic = 0;
	    ip->i_cflag &= ~CMNEW;
	}
	ip->i_flag |= ICHG;
	commit(1, ip);		/* always commit */
    }
    IWRITE_UNLOCK(ip);
    VNOP_RELE(vp);
/*
	ICACHE_LOCK();
	iput(ip, 0);
	ICACHE_UNLOCK();
*/
    return getuerror()? -1 : 0;
}
