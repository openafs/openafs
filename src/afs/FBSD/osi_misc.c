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
 * Implements:
 * afs_suser
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include <sys/namei.h>

#ifdef AFS_FBSD50_ENV
/* serious cheating */
#undef curproc
#define curproc curthread
#endif

#ifndef AFS_FBSD50_ENV
/*
 * afs_suser() returns true if the caller is superuser, false otherwise.
 *
 * Note that it must NOT set errno.
 */

afs_suser()
{
    int error;

    if (suser(curproc) == 0) {
	return (1);
    }
    return (0);
}
#endif

int
osi_lookupname(char *aname, enum uio_seg seg, int followlink,
	       struct vnode **dirvpp, struct vnode **vpp)
{
    struct nameidata n;
    int flags, error;
    flags = 0;
    flags = LOCKLEAF;
    if (followlink)
	flags |= FOLLOW;
    else
	flags |= NOFOLLOW;
    /*   if (dirvpp) flags|=WANTPARENT; *//* XXX LOCKPARENT? */
    NDINIT(&n, LOOKUP, flags, seg, aname, curproc);
    if (error = namei(&n))
	return error;
    *vpp = n.ni_vp;
/*
   if (dirvpp)
      *dirvpp = n.ni_dvp;
*/
    /* should we do this? */
    VOP_UNLOCK(n.ni_vp, 0, curproc);
    NDFREE(&n, NDF_ONLY_PNBUF);
    return 0;
}

/*
 * does not implement security features of kern_time.c:settime()
 */
void
afs_osi_SetTime(osi_timeval_t * atv)
{
#ifdef AFS_FBSD50_ENV
    printf("afs attempted to set clock; use \"afsd -nosettime\"\n");
#else
    struct timespec ts;
    struct timeval tv, delta;
    int s;

    AFS_GUNLOCK();
    s = splclock();
    microtime(&tv);
    delta = *atv;
    timevalsub(&delta, &tv);
    ts.tv_sec = atv->tv_sec;
    ts.tv_nsec = atv->tv_usec * 1000;
    set_timecounter(&ts);
    (void)splsoftclock();
    lease_updatetime(delta.tv_sec);
    splx(s);
    resettodr();
    AFS_GLOCK();
#endif
}
