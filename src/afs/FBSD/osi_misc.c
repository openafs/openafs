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
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include <sys/namei.h>

int
osi_lookupname(char *aname, enum uio_seg seg, int followlink,
	       struct vnode **vpp)
{
    struct nameidata n;
    int flags, error, glocked;

    glocked = ISAFS_GLOCK();
    if (glocked)
	AFS_GUNLOCK();

#if __FreeBSD_version >= 1000021 /* MPSAFE is gone for good! */
    flags = 0;
#else
    flags = MPSAFE; /* namei must take Giant if needed */
#endif
    if (followlink)
	flags |= FOLLOW;
    else
	flags |= NOFOLLOW;
    NDINIT(&n, LOOKUP, flags, seg, aname, curthread);
    if ((error = namei(&n)) != 0) {
	if (glocked)
	    AFS_GLOCK();
	return error;
    }
    *vpp = n.ni_vp;
    NDFREE(&n, NDF_ONLY_PNBUF);
    if (glocked)
	AFS_GLOCK();
    return 0;
}

/*
 * Replace all of the bogus special-purpose memory allocators...
 */
void *
osi_fbsd_alloc(size_t size, int dropglobal)
{
	void *rv;
	int glocked;

	if (dropglobal) {
	    glocked = ISAFS_GLOCK();
	    if (glocked)
		AFS_GUNLOCK();
	    rv = malloc(size, M_AFS, M_WAITOK);
	    if (glocked)
		AFS_GLOCK();
	} else
	    rv = malloc(size, M_AFS, M_NOWAIT);

	return (rv);
}

void
osi_fbsd_free(void *p)
{
       free(p, M_AFS);
}

/**
 * check if a vcache is in use
 *
 * @return status
 *  @retcode 0 success
 *  @retcode EBUSY vcache is in use by someone else
 *  @retcode otherwise other error
 *
 * @pre  The caller must hold the vnode interlock for the associated vnode
 * @post The vnode interlock for the associated vnode will still be held
 *       and must be VI_UNLOCK'd by the caller
 */
int
osi_fbsd_checkinuse(struct vcache *avc)
{
    struct vnode *vp = AFSTOV(avc);

    ASSERT_VI_LOCKED(vp, "osi_fbsd_checkinuse");

    /* The interlock is needed to check the usecount. */
    if (vp->v_usecount > 0) {
	return EBUSY;
    }

    /* XXX
     * The value of avc->opens here came to be, at some point,
     * typically -1.  This was caused by incorrectly performing afs_close
     * processing on vnodes being recycled */
    if (avc->opens) {
	return EBUSY;
    }

    /* if a lock is held, give up */
    if (CheckLock(&avc->lock)) {
	return EBUSY;
    }

    return 0;
}
