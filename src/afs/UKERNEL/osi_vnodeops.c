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
    
int
afs_vrdwr(struct usr_vnode *avc, struct usr_uio *uio, int rw, int io,
	  struct usr_ucred *cred)
{
    int rc;

    if (rw == UIO_WRITE) {
	rc = afs_write(VTOAFS(avc), uio, io, cred, 0);
    } else {
	rc = afs_read(VTOAFS(avc), uio, cred, 0);
    }

    return rc;
}

int
afs_inactive(struct vcache *avc, afs_ucred_t *acred)
{
    if (afs_shuttingdown != AFS_RUNNING)
	return 0;

    usr_assert(avc->vrefCount == 0);
    afs_InactiveVCache(avc, acred);

    return 0;
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
