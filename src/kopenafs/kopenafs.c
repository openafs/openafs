/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Glue code for the kopenafs API.  Mostly just wrappers around the functions
 * included in the libsys code.
 */

#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/signal.h>

#include <afsconfig.h>
#include <afs/afssyscalls.h>
#include <kopenafs.h>

static volatile sig_atomic_t syscall_okay = 1;

/* Signal handler to catch failed system calls and change the okay flag. */
#ifdef SIGSYS
static RETSIGTYPE
sigsys_handler(int s)
{
    syscall_okay = 0;
    signal(SIGSYS, sigsys_handler);
}
#endif /* SIGSYS */

int
k_hasafs(void)
{
    struct ViceIoctl iob;
    int okay, saved_errno;
    RETSIGTYPE (*saved_func)(int);

    saved_errno = errno;

#ifdef SIGSYS
    saved_func = signal(SIGSYS, sigsys_handler);
#endif

    iob.in = NULL;
    iob.in_size = 0;
    iob.out = NULL;
    iob.out_size = 0;
    lpioctl(NULL, VIOCSETTOK, (char *) &iob, 0);

#ifdef SIGSYS
    signal(SIGSYS, saved_func);
#endif

    okay = 1;
    if (!syscall_okay || errno != EINVAL)
        okay = 0;
    errno = saved_errno;
    return okay;
}

int
k_setpag(void)
{
    return lsetpag();
}

int
k_pioctl(char *path, int cmd, struct ViceIoctl *cmarg, int follow)
{
    return lpioctl(path, cmd, (char *) cmarg, follow);
}

int
k_unlog(void)
{
    struct ViceIoctl iob;

    iob.in = NULL;
    iob.in_size = 0;
    iob.out = NULL;
    iob.out_size = 0;
    return lpioctl(NULL, VIOCUNLOG, (char *) &iob, 0);
}
