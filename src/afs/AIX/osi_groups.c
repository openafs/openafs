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

#ifndef AFS_AIX51_ENV
int
setgroups(ngroups, gidset)
     int ngroups;
     gid_t *gidset;
{
    int code = 0;
    struct vrequest treq;
    struct ucred *credp;
    struct ucred *credp0;

    AFS_STATCNT(afs_xsetgroups);

    credp = crref();
    AFS_GLOCK();
    code = afs_InitReq(&treq, credp);
    AFS_GUNLOCK();
    crfree(credp);
    if (code)
	return code;

    code = osetgroups(ngroups, gidset);

    /* Note that if there is a pag already in the new groups we don't
     * overwrite it with the old pag.
     */
    credp = crref();
    credp0 = credp;

    if (PagInCred(credp) == NOPAG) {
	if (((treq.uid >> 24) & 0xff) == 'A') {
	    AFS_GLOCK();
	    AddPag(treq.uid, &credp);
	    AFS_GUNLOCK();
	}
    }

    /* If AddPag() didn't make a new cred, then free our cred ref */
    if (credp == credp0) {
	crfree(credp);
    }
    return code;
}
#endif

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
#ifndef AFS_AIX51_ENV
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
#endif
    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
#ifdef AFS_AIX51_ENV
    if (change_parent) {
	code = kcred_setpag(*cred, PAG_AFS, *newpag);
    } else {
	struct ucred *newcr = crdup(*cred);

	crset(newcr);
	code = kcred_setpag(newcr, PAG_AFS, *newpag);
	*cred = newcr;
    }
#else
    afs_get_groups_from_pag(*newpag, &gidset[0], &gidset[1]);
    if (code = afs_setgroups(cred, ngroups, gidset, change_parent)) {
	return (setuerror(code), code);
    }
#endif
    return code;
}


#ifndef AFS_AIX51_ENV
static int
afs_getgroups(struct ucred *cred, int ngroups, gid_t * gidset)
{
    int ngrps, savengrps;
    gid_t *gp;

    gidset[0] = gidset[1] = 0;
    AFS_STATCNT(afs_getgroups);

    savengrps = ngrps = MIN(ngroups, cred->cr_ngrps);
    gp = cred->cr_groups;
    while (ngrps--)
	*gidset++ = *gp++;
    return savengrps;
}

/* the caller is responsible for checking that ngroups <= NGROUPS */

static void
copy_to_cred(newcr, ngroups, gidset)
     struct ucred *newcr;
     int ngroups;
     gid_t *gidset;
{
    gid_t *gp;
    int newngroups;

    newngroups = ngroups;
    gp = newcr->cr_groups;
    while (ngroups--)
	*gp++ = *gidset++;
    newcr->cr_ngrps = newngroups;
}

/*
 * If change_parent is true, then we want to affect the parent process as well
 * as the current process.  We do this by writing into the given cred, on
 * the assumption that it is shared with the parent process.
 *
 * Note that it is important that we do NOT actually do anything to the
 * parent process, because the NFS/AFS translator uses this routine to
 * write into a given cred, and it has no intention of affecting the parent
 * process.
 *
 * If change_parent is false, then we want to affect only the current process.
 */

static int
afs_setgroups(struct ucred **cred, int ngroups, gid_t * gidset,
	      int change_parent)
{
    AFS_STATCNT(afs_setgroups);

    if (ngroups > NGROUPS)
	return EINVAL;

    if (change_parent) {

	/*
	 * klog -setpag goes through this code to change the cred
	 * shared with the parent process.  Historically this did
	 * not work on AIX, but the problem in AIX has now been
	 * fixed.
	 *
	 * The NFS/AFS translator also uses this code in order to
	 * write into a given cred; it certainly doesn't use it
	 * in order to affect any other process.
	 */
	copy_to_cred(*cred, ngroups, gidset);

    } else {

	struct ucred *newcr = crdup(*cred);

	copy_to_cred(newcr, ngroups, gidset);

	crset(newcr);
	*cred = newcr;
    }
    return 0;
}
#endif
