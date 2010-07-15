/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 * setgroups (syscall)
 * setpag
 *
 */
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */

static int
  afs_getgroups(struct ucred *cred, int ngroups, gid_t * gidset);

static int
  afs_setgroups(struct ucred **cred, int ngroups, gid_t * gidset,
		int change_parent);

int
Afs_xsetgroups()
{
    int code = 0;
    struct vrequest treq;

    AFS_STATCNT(afs_xsetgroups);
    AFS_GLOCK();
    code = afs_InitReq(&treq, p_cred(u.u_procp));
    AFS_GUNLOCK();
    if (code)
	return code;
    setgroups();

    /* Note that if there is a pag already in the new groups we don't
     * overwrite it with the old pag.
     */
    if (PagInCred(p_cred(u.u_procp)) == NOPAG) {
	if (((treq.uid >> 24) & 0xff) == 'A') {
	    struct ucred *cred;
	    AFS_GLOCK();
	    /* we've already done a setpag, so now we redo it */
	    cred = p_cred(u.u_procp);
	    AddPag(treq.uid, &cred);
	    AFS_GUNLOCK();
	}
    }
    return code;
}

int
setpag(cred, pagvalue, newpag, change_parent)
     struct ucred **cred;
     afs_uint32 pagvalue;
     afs_uint32 *newpag;
     afs_uint32 change_parent;
{
    gid_t gidset[NGROUPS];
    int ngroups, code;
    int j;

    AFS_STATCNT(setpag);
    ngroups = afs_getgroups(*cred, NGROUPS, gidset);
    if (afs_get_pag_from_groups(gidset[0], gidset[1]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if (ngroups + 2 > NGROUPS) {
	    return (setuerror(E2BIG), E2BIG);
	}
	for (j = ngroups - 1; j >= 0; j--) {
	    gidset[j + 2] = gidset[j];
	}
	ngroups += 2;
    }
    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
    afs_get_groups_from_pag(*newpag, &gidset[0], &gidset[1]);

    if (code = afs_setgroups(cred, ngroups, gidset, change_parent)) {
	return (setuerror(code), code);
    }
    return code;
}


static int
afs_getgroups(struct ucred *cred, int ngroups, gid_t * gidset)
{
    int ngrps, savengrps;
    int *gp;

    gidset[0] = gidset[1] = 0;
    AFS_STATCNT(afs_getgroups);

    for (gp = &cred->cr_groups[NGROUPS]; gp > cred->cr_groups; gp--) {
	if (gp[-1] != NOGROUP)
	    break;
    }
    savengrps = ngrps = MIN(ngroups, gp - cred->cr_groups);
    for (gp = cred->cr_groups; ngrps--;)
	*gidset++ = *gp++;
    return savengrps;
}



static int
afs_setgroups(struct ucred **cred, int ngroups, gid_t * gidset,
	      int change_parent)
{
    int ngrps;
    int i;
    int *gp;
    struct ucred *newcr;
    ulong_t s;
#if defined(AFS_HPUX110_ENV)
    ulong_t context;
#endif

    AFS_STATCNT(afs_setgroups);

    if (!change_parent) {
	newcr = (struct ucred *)crdup(*cred);
	/* nobody else has the pointer to newcr because we
	 ** just allocated it, so no need for locking */
    } else {
	/* somebody else might have a pointer to this structure.
	 ** make sure we do not have a race condition */
	newcr = *cred;
#if defined(AFS_HPUX110_ENV)
	/* all of the uniprocessor spinlocks are not defined. */
	/* I assume the UP and MP are now handled together */
	MP_SPINLOCK_USAV(cred_lock, context);
#else
	s = UP_SPL6();
	SPINLOCK(cred_lock);
#endif
    }


    /* copy the group info */
    gp = newcr->cr_groups;
    while (ngroups--)
	*gp++ = *gidset++;
    for (; gp < &(newcr)->cr_groups[NGROUPS]; gp++)
	*gp = ((gid_t) - 1);

    if (!change_parent) {
	/* replace the new cred structure in the proc area */
	struct ucred *tmp;
	tmp = *cred;
	set_p_cred(u.u_procp, newcr);
	crfree(tmp);
    } else {
#if defined(AFS_HPUX110_ENV)
	MP_SPINUNLOCK_USAV(cred_lock, context);
#else
	(void)UP_SPLX(s);
	SPINUNLOCK(cred_lock);
#endif
    }

    return (setuerror(0), 0);
}
