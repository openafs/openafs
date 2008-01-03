/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_xsetgroups (asserts FALSE)
 * setpag (aliased to use_setpag in sysincludes.h)
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */


int
afs_xsetgroups()
{
    usr_assert(0);
    return 0;
}

static int
afs_getgroups(struct AFS_UCRED *cred, gid_t * gidset)
{
    int ngrps, savengrps;
    gid_t *gp;

    AFS_STATCNT(afs_getgroups);

    gidset[0] = gidset[1] = 0;
    savengrps = ngrps = cred->cr_ngroups;
    gp = cred->cr_groups;
    while (ngrps--)
	*gidset++ = *gp++;
    return savengrps;
}



static int
afs_setgroups(struct AFS_UCRED **cred, int ngroups, gid_t * gidset,
	      int change_parent)
{
    int ngrps;
    int i;
    gid_t *gp;

    AFS_STATCNT(afs_setgroups);

    if (ngroups > NGROUPS_MAX)
	return EINVAL;
    if (!change_parent)
	*cred = (struct AFS_UCRED *)crcopy(*cred);
    (*cred)->cr_ngroups = ngroups;
    gp = (*cred)->cr_groups;
    while (ngroups--)
	*gp++ = *gidset++;
    return (0);
}

int
usr_setpag(struct usr_ucred **cred, afs_uint32 pagvalue, afs_uint32 * newpag,
	   int change_parent)
{
    gid_t *gidset;
    int ngroups, code;
    int j;

    AFS_STATCNT(setpag);

    gidset = (gid_t *) osi_AllocSmallSpace(AFS_SMALLOCSIZ);
    ngroups = afs_getgroups(*cred, gidset);

    if (afs_get_pag_from_groups(gidset[0], gidset[1]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if ((sizeof gidset[0]) * (ngroups + 2) > AFS_SMALLOCSIZ) {
	    osi_FreeSmallSpace((char *)gidset);
	    return (E2BIG);
	}
	for (j = ngroups - 1; j >= 0; j--) {
	    gidset[j + 2] = gidset[j];
	}
	ngroups += 2;
    }
    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
    afs_get_groups_from_pag(*newpag, &gidset[0], &gidset[1]);
    if ((code = afs_setgroups(cred, ngroups, gidset, change_parent))) {
	osi_FreeSmallSpace((char *)gidset);
	return (code);
    }
    osi_FreeSmallSpace((char *)gidset);

    return 0;
}
