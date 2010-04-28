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
#include "afs/afs_stats.h"	/* statistics */
#include "sys/syscallargs.h"

#define NOUID   ((uid_t) -1)
#define NOGID   ((gid_t) -1)

/*
 * NetBSD has a very flexible and elegant replacement for Unix
 * groups KPIs, see KAUTH(9).
 *
 */

static int
osi_getgroups(kauth_cred_t cred, int ngroups, gid_t * gidset);


/* why **? are we returning or reallocating creat? */
static int
osi_setgroups(struct proc *proc, kauth_cred_t *cred, int ngroups,
	      gid_t * gidset, int change_parent);

int
Afs_xsetgroups(struct proc *p, void *args, int *retval)
{
    int code = 0;
    struct vrequest treq;
    kauth_cred_t cred = osi_proccred(p);

    AFS_STATCNT(afs_xsetgroups);

    AFS_GLOCK();
    code = afs_InitReq(&treq, (afs_ucred_t *) cred);
    AFS_GUNLOCK();

    if (code)
	return code;

    /*
     * XXX Does treq.uid == osi_crgetruid(cred)?
     */

    code = kauth_cred_setgroups(cred, args, retval, osi_crgetruid(cred));
    /*
     * Note that if there is a pag already in the new groups we don't
     * overwrite it with the old pag.
     */
    if (PagInCred(cred) == NOPAG) {
	if (((treq.uid >> 24) & 0xff) == 'A') {
	    AFS_GLOCK();
	    /* we've already done a setpag, so now we redo it */
	    AddPag(p, treq.uid, &cred);
	    AFS_GUNLOCK();
	}
    }
    return code;
}


int
setpag(struct proc *proc, afs_ucred_t *cred, afs_uint32 pagvalue,
       afs_uint32 * newpag, int change_parent)
{
    gid_t gidset[NGROUPS];
    int ngroups, code;
    int j;

    AFS_STATCNT(setpag);
    ngroups = osi_getgroups(*cred, NGROUPS, gidset);
    if (afs_get_pag_from_groups(gidset[1], gidset[2]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if (ngroups + 2 > NGROUPS) {
	    return (E2BIG);
	}
	for (j = ngroups - 1; j >= 0; j--) {
	    gidset[j + 2] = gidset[j];
	}
	ngroups += 2;
    }
    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
    afs_get_groups_from_pag(*newpag, &gidset[1], &gidset[2]);
    code = osi_setgroups(proc, cred, ngroups, gidset, change_parent);
    return code;
}


static int
osi_getgroups(kauth_cred_t cred, int ngroups, gid_t * gidset)
{
    int ngrps, savengrps;
    struct kauth_cred *cr;
    gid_t *gp;

    AFS_STATCNT(afs_getgroups);

    cr = (struct kauth_cred *) cred;
    savengrps = ngrps = MIN(ngroups, kauth_cred_ngroups(cred));
    gp = cred->cr_groups;
    while (ngrps--)
	*gidset++ = *gp++;
    return savengrps;
}


static int
osi_setgroups(struct proc *proc, kauth_cred_t *cred, int ngroups,
	      gid_t * gidset, int change_parent)
{
    int i;
    struct kauth_cred *cr;

    AFS_STATCNT(afs_setgroups); /* XXX rename statcnt */

    if (ngroups > NGROUPS)
	return EINVAL;

    cr = (struct kauth_cred *) *cred;
    if (!change_parent)
	cr = kauth_cred_copy(cr);

    for (i = 0; i < ngroups; i++)
	cr->cr_groups[i] = gidset[i];
    for (i = ngroups; i < NGROUPS; i++)
	cr->cr_groups[i] = NOGROUP;
    cr->cr_ngroups = ngroups;

    *cred = cr;
    return (0);
}
