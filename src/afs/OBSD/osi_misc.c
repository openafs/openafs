/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* 
 * osi_misc.c
 *
 * OpenBSD version of afs_osi_suser() by Jim Rees.
 * See osi_machdep.h for afs_suser macro. It simply calls afs_osi_suser()
 * with the creds of the current process.
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afs/afsincludes.h"	/* Afs-based standard headers */

/*
 * afs_suser() returns true if the caller is superuser, false otherwise.
 *
 * Note that it must NOT set errno.
 */

int
afs_osi_suser(void *credp)
{
    return (suser((struct ucred *) credp, &curproc->p_acflag) ? 0 : 1);
}

int
afs_syscall_icreate(dev, near_inode, param1, param2, param3, param4, retval)
     long *retval;
     long dev, near_inode, param1, param2, param3, param4;
{
    return EINVAL;
}

int
afs_syscall_iopen(dev, inode, usrmod, retval)
     long *retval;
     int dev, inode, usrmod;
{
    return EINVAL;
}

int
afs_syscall_iincdec(dev, inode, inode_p1, amount)
     int dev, inode, inode_p1, amount;
{
    return EINVAL;
}
