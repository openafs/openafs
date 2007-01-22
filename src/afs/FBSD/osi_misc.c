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

int
osi_lookupname(char *aname, enum uio_seg seg, int followlink,
	       struct vnode **vpp)
{
    struct nameidata n;
    int flags, error, wasowned;

#ifdef AFS_FBSD50_ENV
    wasowned = mtx_owned(&afs_global_mtx);
    if (wasowned)
	mtx_unlock(&afs_global_mtx);
#endif

    flags = 0;
    flags = LOCKLEAF;
    if (followlink)
	flags |= FOLLOW;
    else
	flags |= NOFOLLOW;
    NDINIT(&n, LOOKUP, flags, seg, aname, curproc);
    if ((error = namei(&n)) != 0) {
#ifdef AFS_FBSD50_ENV
	if (wasowned)
	    mtx_lock(&afs_global_mtx);
#endif
	return error;
    }
    *vpp = n.ni_vp;
    /* should we do this? */
    VOP_UNLOCK(n.ni_vp, 0, curproc);
    NDFREE(&n, NDF_ONLY_PNBUF);
#ifdef AFS_FBSD50_ENV
    if (wasowned)
	mtx_lock(&afs_global_mtx);
#endif
    return 0;
}

/*
 * does not implement security features of kern_time.c:settime()
 */
void
afs_osi_SetTime(afs_timeval_t * atv)
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

/*
 * Replace all of the bogus special-purpose memory allocators...
 */
void *
osi_fbsd_alloc(size_t size, int dropglobal)
{
	void *rv;
#ifdef AFS_FBSD50_ENV
	int wasowned;

	if (dropglobal) {
		wasowned = mtx_owned(&afs_global_mtx);
		if (wasowned)
			mtx_unlock(&afs_global_mtx);
		rv = malloc(size, M_AFS, M_WAITOK);
		if (wasowned)
			mtx_lock(&afs_global_mtx);
	} else
#endif
		rv = malloc(size, M_AFS, M_NOWAIT);

	return (rv);
}

void
osi_fbsd_free(void *p)
{

	free(p, M_AFS);
}

void
osi_AllocMoreSSpace(afs_int32 preallocs)
{
	;
}

void
osi_FreeLargeSpace(void *p)
{
	osi_fbsd_free(p);
}

void
osi_FreeSmallSpace(void *p)
{
	osi_fbsd_free(p);
}

void *
osi_AllocLargeSpace(size_t size)
{
	AFS_ASSERT_GLOCK();
	AFS_STATCNT(osi_AllocLargeSpace);
	return (osi_fbsd_alloc(size, 1));
}

void *
osi_AllocSmallSpace(size_t size)
{
	AFS_ASSERT_GLOCK();
	AFS_STATCNT(osi_AllocSmallSpace);
	return (osi_fbsd_alloc(size, 1));
}

void
shutdown_osinet(void)
{
	;
}
