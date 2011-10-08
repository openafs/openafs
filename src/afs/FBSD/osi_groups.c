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
#include <sys/param.h>
#include <sys/sysproto.h>


#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */

static int
  afs_getgroups(struct ucred *cred, int ngroups, gid_t * gidset);

static int
  afs_setgroups(struct thread *td, struct ucred **cred, int ngroups,
		gid_t * gidset, int change_parent);


int
Afs_xsetgroups(struct thread *td, struct setgroups_args *uap)
{
    int code = 0;
    struct vrequest treq;
    struct ucred *cr;

    cr = crdup(td->td_ucred);

    AFS_STATCNT(afs_xsetgroups);
    AFS_GLOCK();

    code = afs_InitReq(&treq, cr);
    AFS_GUNLOCK();
    crfree(cr);
    if (code)
#if (__FreeBSD_version >= 900044)
	return sys_setgroups(td, uap);	/* afs has shut down */
#else
	return setgroups(td, uap);	/* afs has shut down */
#endif

#if (__FreeBSD_version >= 900044)
    code = sys_setgroups(td, uap);
#else
    code = setgroups(td, uap);
#endif
    /* Note that if there is a pag already in the new groups we don't
     * overwrite it with the old pag.
     */
    cr = crdup(td->td_ucred);

    if (PagInCred(cr) == NOPAG) {
	if (((treq.uid >> 24) & 0xff) == 'A') {
	    AFS_GLOCK();
	    /* we've already done a setpag, so now we redo it */
	    AddPag(td, treq.uid, &cr);
	    AFS_GUNLOCK();
	}
    }
    crfree(cr);
    return code;
}


int
setpag(struct thread *td, struct ucred **cred, afs_uint32 pagvalue,
       afs_uint32 * newpag, int change_parent)
{
#if defined(AFS_FBSD81_ENV)
    gid_t *gidset;
    int gidset_len = ngroups_max + 1;
#elif defined(AFS_FBSD80_ENV)
    gid_t *gidset;
    int gidset_len = NGROUPS;	/* 1024 */
#else
    gid_t gidset[NGROUPS];
    int gidset_len = NGROUPS;	/* 16 */
#endif
    int ngroups, code;
    int j;

    AFS_STATCNT(setpag);
#ifdef AFS_FBSD80_ENV
    gidset = osi_Alloc(gidset_len * sizeof(gid_t));
#endif
    ngroups = afs_getgroups(*cred, gidset_len, gidset);
    if (afs_get_pag_from_groups(gidset[1], gidset[2]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if (ngroups + 2 > gidset_len) {
	    return (E2BIG);
	}
	for (j = ngroups - 1; j >= 1; j--) {
	    gidset[j + 2] = gidset[j];
	}
	ngroups += 2;
    }
    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
    afs_get_groups_from_pag(*newpag, &gidset[1], &gidset[2]);
    code = afs_setgroups(td, cred, ngroups, gidset, change_parent);
#ifdef AFS_FBSD80_ENV
    osi_Free(gidset, gidset_len * sizeof(gid_t));
#endif
    return code;
}


static int
afs_getgroups(struct ucred *cred, int ngroups, gid_t * gidset)
{
    int ngrps, savengrps;
    gid_t *gp;

    AFS_STATCNT(afs_getgroups);
    savengrps = ngrps = MIN(ngroups, cred->cr_ngroups);
    gp = cred->cr_groups;
    while (ngrps--)
	*gidset++ = *gp++;
    return savengrps;
}


static int
afs_setgroups(struct thread *td, struct ucred **cred, int ngroups,
	      gid_t * gidset, int change_parent)
{
    return (0);
}
