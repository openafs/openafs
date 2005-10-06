/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 * afs_config
 * mem_getbytes
 * mem_freebytes
 * kmem_alloc
 * kmem_free
 * VN_RELE
 * VN_HOLD
 * kludge_init
 * ufdalloc
 * ufdfree
 * ffree
 * iptovp
 * dev_ialloc
 * iget
 * iput
 * commit
 * fs_simple_lock
 * fs_read_lock
 * fs_write_lock
 * fs_complex_unlock
 * ksettimer
 *
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/AIX/osi_config.c,v 1.8.2.2 2005/09/21 00:03:56 shadow Exp $");

#include "sys/limits.h"
#include "sys/types.h"
#include "sys/user.h"
#include "sys/vnode.h"
#include "sys/conf.h"
#include "sys/errno.h"
#include "sys/device.h"
#include "sys/vfs.h"
#include "sys/vmount.h"
#include "sys/gfs.h"
#include "sys/uio.h"
#include "sys/pri.h"
#include "sys/priv.h"		/* XXX */
#include "sys/lockl.h"
#include "sys/malloc.h"
#include <sys/syspest.h>	/* to define the assert and ASSERT macros       */
#include <sys/timer.h>		/* For the timer related defines                */
#include <sys/intr.h>		/* for the serialization defines                */
#include <sys/malloc.h>		/* for the parameters to xmalloc()              */
#include "afs/afs_osi.h"	/* pick up osi_timeval_t for afs_stats.h */
#include "afs/afs_stats.h"
#include "../export.h"

#ifdef KOFF
#define KOFF_PRESENT	1
#else
#define KOFF_PRESENT	0
#endif

#if !KOFF_PRESENT
_db_trace()
{;
}

long db_tflags = 0;
#endif

extern struct gfs afs_gfs;
extern struct vnodeops locked_afs_gn_vnodeops;

#ifdef __64BIT__
afs_uint64 get_toc();
#else
afs_uint32 get_toc();
#endif

#define	AFS_CALLOUT_TBL_SIZE	256

#include <sys/lock_alloc.h>
extern Simple_lock afs_callout_lock;

/*
 * afs_config	-	handle AFS configuration requests
 *
 * Input:
 *	cmd	-	add/delete command
 *	uiop	-	uio vector describing any config params
 */
afs_config(cmd, uiop)
     struct uio *uiop;
{
    int err;
    extern struct vnodeops *afs_ops;

    AFS_STATCNT(afs_config);

    err = 0;
    AFS_GLOCK();
    if (cmd == CFG_INIT) {	/* add AFS gfs          */
	/*
	 * init any vrmix mandated kluges
	 */
	if (err = kluge_init())
	    goto out;
	/*
	 * make sure that we pin everything
	 */
	if (err = pincode(afs_config))
	    goto out;
	err = gfsadd(AFS_MOUNT_AFS, &afs_gfs);
	/*
	 * ok, if already installed
	 */
	if (err == EBUSY)
	    err = 0;
	if (!err) {
	    pin(&afs_callout_lock, sizeof(afs_callout_lock));
	    lock_alloc(&afs_callout_lock, LOCK_ALLOC_PIN, 0, 5);
	    simple_lock_init(&afs_callout_lock);
	    afs_ops = &locked_afs_gn_vnodeops;
	    timeoutcf(AFS_CALLOUT_TBL_SIZE);
	} else {
	    unpincode(afs_config);
	    goto out;
	}
	if (KOFF_PRESENT) {
	    extern void *db_syms[];
	    extern db_nsyms;

	    koff_addsyms(db_syms, db_nsyms);
	}
    } else if (cmd == CFG_TERM) {	/* delete AFS gfs       */
	err = gfsdel(AFS_MOUNT_AFS);
	/*
	 * ok, if already deleted
	 */
	if (err == ENOENT)
	    err = 0;
#ifndef AFS_AIX51_ENV
	else
#endif
	if (!err) {
#ifdef AFS_AIX51_ENV
	    if (err = unpin(&afs_callout_lock))
		err = 0;
#endif
	    if (err = unpincode(afs_config))
		err = 0;

	    timeoutcf(-AFS_CALLOUT_TBL_SIZE);
	}
    } else			/* unknown command */
	err = EINVAL;

  out:
    AFS_GUNLOCK();
    if (!err && (cmd == CFG_INIT))
	osi_Init();

    return err;
}

/*
 * The following stuff is (hopefully) temporary.
 */


/*
 * mem_getbytes	-	memory allocator
 *
 * Seems we can't make up our mind what to call these
 */
char *
mem_getbytes(size)
{

    return malloc(size);
}

/*
 * mem_freebytes	-	memory deallocator
 */
mem_freebytes(p, size)
     char *p;
{

    free(p);
}

char *
kmem_alloc(size)
{

    return malloc(size);
}

kmem_free(p, size)
     char *p;
{

    free(p);
}

VN_RELE(vp)
     register struct vnode *vp;
{

    VNOP_RELE(vp);
}

VN_HOLD(vp)
     register struct vnode *vp;
{

    VNOP_HOLD(vp);
}

/*
 * The following stuff is to account for the fact that stuff we need exported
 * from the kernel isn't, so we must be devious.
 */

int (*kluge_ufdalloc) ();
int (*kluge_fpalloc) ();
void *(*kluge_ufdfree) ();
void *(*kluge_ffree) ();
int (*kluge_iptovp) ();
int (*kluge_dev_ialloc) ();
int (*kluge_iget) ();
int (*kluge_iput) ();
int (*kluge_commit) ();
void *(*kluge_ksettimer) ();
void *(*kluge_fsSimpleLock) ();
void *(*kluge_fsSimpleUnlock) ();
void *(*kluge_fsReadLock) ();
void *(*kluge_fsWriteLock) ();
void *(*kluge_fsCxUnlock) ();

/*
 * kernel function import list
 */

struct k_func kfuncs[] = {
    {(void *(**)())&kluge_ufdalloc, ".ufdalloc"},
    {(void *(**)())&kluge_fpalloc, ".fpalloc"},
    {&kluge_ufdfree, ".ufdfree"},
    {&kluge_ffree, ".ffree"},
    {(void *(**)())&kluge_iptovp, ".iptovp"},
    {(void *(**)())&kluge_dev_ialloc, ".dev_ialloc"},
    {(void *(**)())&kluge_iget, ".iget"},
    {(void *(**)())&kluge_iput, ".iput"},
    {(void *(**)())&kluge_commit, ".commit"},
    {&kluge_ksettimer, ".ksettimer"},
#ifdef _FSDEBUG
    {&kluge_fsSimpleLock, ".fs_simple_lock"},
    {&kluge_fsSimpleUnlock, ".fs_simple_unlock"},
    {&kluge_fsReadLock, ".fs_read_lock"},
    {&kluge_fsWriteLock, ".fs_write_lock"},
    {&kluge_fsCxUnlock, ".fs_complex_unlock"},
#endif
    {0, 0},
};

void *vnodefops;		/* dummy vnodeops       */
struct ifnet *ifnet;
Simple_lock jfs_icache_lock;
Simple_lock proc_tbl_lock;

/*
 * kernel variable import list
 */
struct k_var kvars[] = {
    {(void *)&vnodefops, "vnodefops"},
    {(void *)&ifnet, "ifnet"},
    {(void *)&jfs_icache_lock, "jfs_icache_lock"},
#ifndef AFS_AIX51_ENV
    {(void *)&proc_tbl_lock, "proc_tbl_lock"},
#endif
    {0, 0},
};

/*
 * kluge_init -	initialise the kernel imports kluge
 */
kluge_init()
{
    register struct k_func *kf;
    register struct k_var *kv;
#ifdef __64BIT__
    register afs_uint64 toc;
#else
    register afs_uint32 toc;
#endif
    register err = 0;

    toc = get_toc();
    for (kf = kfuncs; !err && kf->name; ++kf) {
	err = import_kfunc(kf);
    }
    for (kv = kvars; !err && kv->name; ++kv) {
	err = import_kvar(kv, toc);
    }

    return err;
}

ufdalloc(i, fdp)
     int *fdp;
{

    return (*kluge_ufdalloc) (i, fdp);
}

fpalloc(vp, flag, type, ops, fpp)
     struct vnode *vp;
     struct fileops *ops;
     struct file **fpp;
{

    return (*kluge_fpalloc) (vp, flag, type, ops, fpp);
}

void
ufdfree(fd)
{

    (void)(*kluge_ufdfree) (fd);
}

void
ffree(fp)
     struct file *fp;
{

    (void)(*kluge_ffree) (fp);
}

iptovp(vfsp, ip, vpp)
     struct vfs *vfsp;
     struct inode *ip, **vpp;
{

    return (*kluge_iptovp) (vfsp, ip, vpp);
}

dev_ialloc(pip, ino, mode, vfsp, ipp)
     struct inode *pip;
     ino_t ino;
     mode_t mode;
     struct vfs *vfsp;
     struct inode **ipp;
{

    return (*kluge_dev_ialloc) (pip, ino, mode, vfsp, ipp);

}

iget(dev, ino, ipp, doscan, vfsp)
     dev_t dev;
     ino_t ino;
#ifdef __64BIT__
     afs_size_t doscan;
#endif
     struct vfs *vfsp;
     struct inode **ipp;
{
#ifdef __64BIT__
    afs_int64 dummy[10];
    dummy[0] = doscan;

    return (*kluge_iget) (dev, ino, ipp, (afs_size_t) doscan, vfsp, &dummy);
#else
    return (*kluge_iget) (dev, ino, ipp, doscan, vfsp);
#endif
}

iput(ip, vfsp)
     struct vfs *vfsp;
     struct inode *ip;
{
    return (*kluge_iput) (ip, vfsp);
}

commit(n, i0, i1, i2)
     struct inode *i0, *i1, *i2;
{

    return (*kluge_commit) (n, i0, i1, i2);
}


#ifdef _FSDEBUG
fs_simple_lock(void *lp, int type)
{
    return (*kluge_fsSimpleLock) (lp, type);
}

fs_simple_unlock(void *lp, int type)
{
    return (*kluge_fsSimpleUnlock) (lp, type);
}

fs_read_lock(complex_lock_t lp, int type)
{
    return (*kluge_fsReadLock) (lp, type);
}

fs_write_lock(complex_lock_t lp, int type)
{
    return (*kluge_fsWriteLock) (lp, type);
}

fs_complex_unlock(complex_lock_t lp, int type)
{
    return (*kluge_fsCxUnlock) (lp, type);
}
#endif
