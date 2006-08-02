/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * The lpioctl system call.  (pioctl is handled by the rmtsys layer and turned
 * into either lpioctl or a remote call as appropriate.)  It is kept separate
 * to allow for the creation of the libkopenafs shared library without
 * including the other system calls.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <afs/afs_args.h>
#if defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
#include <unistd.h>
#else
#include <stdio.h>
#endif
#include "afssyscalls.h"

#ifdef AFS_AIX32_ENV
/*
 * in VRMIX, system calls look just like function calls, so we don't
 * need to do anything!
 */

#else
#if defined(AFS_SGI_ENV)

#pragma weak xlpioctl = lpioctl

int
lpioctl(char *path, int cmd, char *cmarg, int follow)
{
    return (syscall(AFS_PIOCTL, path, cmd, cmarg, follow));
}

#else /* AFS_SGI_ENV */

int
lpioctl(char *path, int cmd, char *cmarg, int follow)
{
    int errcode, rval;

#if defined(AFS_LINUX20_ENV)
    rval = proc_afs_syscall(AFSCALL_PIOCTL, (long)path, cmd, (long)cmarg, 
			    follow, &errcode);

    if(rval)
	errcode = syscall(AFS_SYSCALL, AFSCALL_PIOCTL, path, cmd, cmarg, 
			  follow);
#elif defined(AFS_DARWIN80_ENV)
    rval = ioctl_afs_syscall(AFSCALL_PIOCTL, (long)path, cmd, (long)cmarg,
			     follow, 0, 0, &errcode);
    if (rval)
	errcode = rval;
#else
    errcode = syscall(AFS_SYSCALL, AFSCALL_PIOCTL, path, cmd, cmarg, follow);
#endif

    return (errcode);
}

#endif /* !AFS_SGI_ENV */
#endif /* !AFS_AIX32_ENV */
