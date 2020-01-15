/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi_groups.c
 *
 * Implements:
 * Afs_xsetgroups (syscall)
 * setpag
 *
 */
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"
#include "afs/afsincludes.h"
#include "afs/opr.h"
#include "afs/afs_stats.h"	/* statistics */
#include "sys/syscallargs.h"

#define NOUID   ((uid_t) -1)
#define NOGID   ((gid_t) -1)

/*
 * NetBSD has a very flexible and elegant replacement for Unix
 * groups KPIs, see kauth(9).
 *
 */

static int osi_getgroups(afs_ucred_t *, int, gid_t *);

/* why **? are we returning or reallocating creat? */
static int osi_setgroups(afs_proc_t *, afs_ucred_t **, int, gid_t *, int);

int Afs_xsetgroups(afs_proc_t *, const void *, register_t *);

int
Afs_xsetgroups(afs_proc_t *p, const void *args, register_t *retval)
{
    int code = 0;
    struct vrequest treq;
    afs_ucred_t *cred = osi_proccred(p);

    AFS_STATCNT(afs_xsetgroups);

    AFS_GLOCK();
    code = afs_InitReq(&treq, cred);
    AFS_GUNLOCK();

    if (code)
	return code;

    /* results visible via kauth_cred_getgroups. also does other work */
    code = sys_setgroups(p, args, retval);

    /*
     * Note that if there is a pag already in the new groups we don't
     * overwrite it with the old pag.
     */
    if (PagInCred(cred) == NOPAG) {
	if (afs_IsPagId(treq.uid)) {
	    AFS_GLOCK();
	    /* we've already done a setpag, so now we redo it */
	    AddPag(p, treq.uid, &cred);
	    AFS_GUNLOCK();
	}
    }
    return code;
}

int
setpag(afs_proc_t *proc, afs_ucred_t **cred, afs_uint32 pagvalue,
       afs_uint32 * newpag, int change_parent)
{
    gid_t gidset[NGROUPS];
    int ngroups, code;
    int j;

    AFS_STATCNT(setpag);

    if (pagvalue == -1) {
	code = afs_genpag(*cred, &pagvalue);
	if (code != 0) {
	    return code;
	}
    }
    ngroups = osi_getgroups(*cred, NGROUPS, gidset);
    if (afs_get_pag_from_groups(gidset[1], gidset[2]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if (ngroups + 2 > NGROUPS) {
	    return (E2BIG);
	}
	for (j = ngroups - 1; j >= 1; j--) {
	    gidset[j + 2] = gidset[j];
	}
	ngroups += 2;
    }
    *newpag = pagvalue;
    afs_get_groups_from_pag(*newpag, &gidset[1], &gidset[2]);
    code = osi_setgroups(proc, cred, ngroups, gidset, change_parent);
    return code;
}


static int
osi_getgroups(afs_ucred_t *cred, int ngroups, gid_t *gidset)
{
    AFS_STATCNT(afs_getgroups);

    ngroups = opr_min(kauth_cred_ngroups(cred), ngroups);

    kauth_cred_getgroups(cred, gidset, ngroups, UIO_SYSSPACE);
    return ngroups;
}


static int
osi_setgroups(afs_proc_t *proc, afs_ucred_t **cred, int ngroups,
	      gid_t * gidset, int change_parent)
{
    int code;
    afs_ucred_t *ocred;

    AFS_STATCNT(afs_setgroups); /* XXX rename statcnt */

    if (ngroups > NGROUPS)
	return EINVAL;

    proc_crmod_enter();

    if (!change_parent) {
	ocred = *cred;
	*cred = kauth_cred_dup(ocred);
    }

    code = kauth_cred_setgroups(*cred, gidset, ngroups, -1, UIO_SYSSPACE);

    if (!change_parent) {
	proc_crmod_leave(*cred, ocred, false);
    }

    return code;
}
