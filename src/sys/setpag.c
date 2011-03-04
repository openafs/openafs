/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * This file contains the lsetpag system call.  (setpag is handled by the
 * rmtsys layer and turned into either setpag or a remote call as is
 * appropriate.)  It is kept separate to allow for the creation of a simple
 * shared library containing only setpag.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <afs/afs_args.h>
#include <afs/sys_prototypes.h>
#include <unistd.h>
#include <stdio.h>
#if !defined(AFS_AIX_ENV) && !defined(AFS_NT40_ENV)
# include <sys/syscall.h>
#endif
#include "afssyscalls.h"

#ifdef AFS_AIX32_ENV
/*
 * in VRMIX, system calls look just like function calls, so we don't
 * need to do anything!
 */

#else
#if defined(AFS_SGI_ENV)

#pragma weak xlsetpag = lsetpag

int
lsetpag(void)
{
    return (syscall(AFS_SETPAG));
}

#else /* AFS_SGI_ENV */

int
lsetpag(void)
{
    int errcode;

#ifdef AFS_LINUX20_ENV
    int rval;

    rval = proc_afs_syscall(AFSCALL_SETPAG,0,0,0,0,&errcode);

    if(rval)
      errcode = syscall(AFS_SYSCALL, AFSCALL_SETPAG);
#elif defined(AFS_DARWIN80_ENV)
    int rval;

    rval = ioctl_afs_syscall(AFSCALL_SETPAG,0,0,0,0,0,0,&errcode);
    if (rval) {
	errcode = rval;
    }
#elif defined(AFS_SUN511_ENV)
    int rval;

    rval = ioctl_sun_afs_syscall(AFSCALL_SETPAG,0,0,0,0,0,0,&errcode);
    if (rval) {
	errcode = rval;
    }
#else
    errcode = syscall(AFS_SYSCALL, AFSCALL_SETPAG);
#endif

    return (errcode);
}

#endif /* !AFS_SGI_ENV */
#endif /* !AFS_AIX32_ENV */
