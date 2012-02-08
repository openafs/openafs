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

#include <afsconfig.h>
#include <afs/param.h>

#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#ifdef AFS_AIX51_ENV
# include <sys/cred.h>
# ifdef HAVE_SYS_PAG_H
#  include <sys/pag.h>
# endif
#endif
#include <sys/param.h>
#include <unistd.h>

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
    lpioctl(NULL, VIOCSETTOK, &iob, 0);

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
    return lpioctl(path, cmd, cmarg, follow);
}

int
k_unlog(void)
{
    struct ViceIoctl iob;

    iob.in = NULL;
    iob.in_size = 0;
    iob.out = NULL;
    iob.out_size = 0;
    return lpioctl(NULL, VIOCUNLOG, &iob, 0);
}


/*
 * If we don't have the VIOC_GETPAG pioctl, we try to determine whether we're
 * in a PAG by using either a special OS call (AIX 5.2 and later) or by
 * walking the group list, which works differently for current versions of
 * Linux.
 *
 * These OS differences are encapsulated in the following OS-specific haspag
 * helper functions.
 *
 * This is largely copied from auth/ktc.c and should be merged with that
 * version, but that version calls through the pioctl() interface right now
 * and therefore pulls in Rx for NFS translator support.  This avoids an Rx
 * dependency in the standalone libkopenafs interface.
 */
#if defined(AFS_AIX52_ENV)
static int
os_haspag(void)
{
    return (getpagvalue("afs") < 0) ? 0 : 1;
}
#elif defined(AFS_AIX51_ENV)
static int
os_haspag(void)
{
    return 0;
}
#else
static int
os_haspag(void)
{
    int ngroups;
    gid_t *groups;
    afs_uint32 g0, g1;
    afs_uint32 h, l, pag;
# ifdef AFS_LINUX26_ENV
    int i;
# endif

    ngroups = getgroups(0, NULL);
    groups = malloc(sizeof(*groups) * ngroups);
    if (groups == NULL)
        return 0;
    ngroups = getgroups(ngroups, groups);

    /* Check for AFS_LINUX26_ONEGROUP_ENV PAGs. */
# ifdef AFS_LINUX26_ENV
    for (i = 0; i < ngroups; i++)
        if (((groups[i] >> 24) & 0xff) == 'A') {
            free(groups);
            return 1;
        }
# endif

    /* Check for the PAG group pair. */
    if (ngroups < 2) {
        free(groups);
        return 0;
    }
    g0 = groups[0] & 0xffff;
    g1 = groups[1] & 0xffff;
    free(groups);
    g0 -= 0x3f00;
    g1 -= 0x3f00;
    if (g0 < 0xc000 && g1 < 0xc000) {
        l = ((g0 & 0x3fff) << 14) | (g1 & 0x3fff);
        h = (g0 >> 14);
        h = (g1 >> 14) + h + h + h;
        pag = ((h << 28) | l);
        if (((pag >> 24) & 0xff) == 'A')
            return 1;
        else
            return 0;
    }
    return 0;
}
#endif /* !AFS_AIX51_ENV */

int
k_haspag(void)
{
    int code;
    struct ViceIoctl iob;
    afs_uint32 pag;

    iob.in = NULL;
    iob.in_size = 0;
    iob.out = (caddr_t) &pag;
    iob.out_size = sizeof(afs_uint32);
    code = lpioctl(NULL, VIOC_GETPAG, &iob, 0);
    if (code == 0)
        return pag != (afs_uint32) -1;
    else
        return os_haspag();
}
