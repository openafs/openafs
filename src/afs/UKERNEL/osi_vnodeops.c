/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */

extern int afs_noop();
extern int afs_badop();

extern int afs_open();
extern int afs_close();
extern int afs_getattr();
extern int afs_setattr();
extern int afs_access();
extern int afs_lookup();
extern int afs_create();
extern int afs_remove();
extern int afs_link();
extern int afs_rename();
extern int afs_mkdir();
extern int afs_rmdir();
extern int afs_readdir();
extern int afs_symlink();
extern int afs_readlink();
extern int afs_fsync();
extern int afs_lockctl();
extern int afs_fid();

int
afs_vrdwr(struct usr_vnode *avc, struct usr_uio *uio, int rw, int io,
	  struct usr_ucred *cred)
{
    int rc;

    if (rw == UIO_WRITE) {
	rc = afs_write(avc, uio, io, cred, 0);
    } else {
	rc = afs_read(avc, uio, cred, 0, 0, 0);
    }

    return rc;
}

int
afs_inactive(struct vcache *avc, struct AFS_UCRED *acred)
{
    struct vnode *vp = AFSTOV(avc);
    if (afs_shuttingdown)
	return;

    usr_assert(avc->vrefCount == 0);
    afs_InactiveVCache(avc, acred);
}

struct usr_vnodeops Afs_vnodeops = {
    afs_open,
    afs_close,
    afs_vrdwr,
    afs_badop,			/* ioctl */
    afs_noop,			/* select */
    afs_getattr,
    afs_setattr,
    afs_access,
    afs_lookup,
    afs_create,
    afs_remove,
    afs_link,
    afs_rename,
    afs_mkdir,
    afs_rmdir,
    afs_readdir,
    afs_symlink,
    afs_readlink,
    afs_fsync,
    afs_inactive,
    afs_badop,			/* bmap */
    afs_badop,			/* strategy */
    afs_badop,			/* bread */
    afs_badop,			/* brelse */
    afs_lockctl,
    afs_fid
};

struct usr_vnodeops *afs_ops = &Afs_vnodeops;
