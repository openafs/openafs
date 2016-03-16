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
#include <signal.h>

#include <afs/afs_args.h>
#if defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
#include <unistd.h>
#else
#include <stdio.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include "afssyscalls.h"
#include "sys_prototypes.h"

#ifdef AFS_AIX32_ENV
/*
 * in VRMIX, system calls look just like function calls, so we don't
 * need to do anything!
 */

#elif defined(AFS_SGI_ENV)

#pragma weak xlpioctl = lpioctl

int
lpioctl(char *path, int cmd, void *cmarg, int follow)
{
    return (syscall(AFS_PIOCTL, path, cmd, cmarg, follow));
}

#elif defined(AFS_LINUX20_ENV)

int
lpioctl(char *path, int cmd, void *cmarg, int follow)
{
    int errcode = 0;
    int rval;

    rval = proc_afs_syscall(AFSCALL_PIOCTL, (long)path, cmd, (long)cmarg,
			    follow, &errcode);

    if(rval)
	errcode = syscall(AFS_SYSCALL, AFSCALL_PIOCTL, path, cmd, cmarg,
			  follow);

    return (errcode);
}

#else /* AFS_AIX32_ENV */

int
lpioctl(char *path, int cmd, void *cmarg, int follow)
{
    int errcode = 0;
    /* As kauth/user.c says, handle smoothly the case where no AFS system call
     * exists (yet). */
    void (*old)(int) = signal(SIGSYS, SIG_IGN);

# if defined(AFS_DARWIN80_ENV) || defined(AFS_SUN511_ENV)
    {
	int rval;	/* rval and errcode are distinct for pioctl platforms */
#  if defined(AFS_DARWIN80_ENV)
	rval = ioctl_afs_syscall(AFSCALL_PIOCTL, (long)path, cmd, (long)cmarg,
				follow, 0, 0, &errcode);
#  elif defined(AFS_SUN511_ENV)
	rval = ioctl_sun_afs_syscall(AFSCALL_PIOCTL, (uintptr_t)path, cmd,
				    (uintptr_t)cmarg, follow, 0, 0, &errcode);
#  endif  /* !AFS_DARWIN80_ENV */
	/* non-zero rval takes precedence over errcode */
	if (rval)
	    errcode = rval;
    }
# else   /* ! AFS_DARWIN80_ENV || AFS_SUN511_ENV */
    errcode = syscall(AFS_SYSCALL, AFSCALL_PIOCTL, path, cmd, cmarg, follow);
# endif

    signal(SIGSYS, old);

    return (errcode);
}

#endif /* !AFS_AIX32_ENV */
