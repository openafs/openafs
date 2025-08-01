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
	return sys_setgroups(td, uap);	/* afs has shut down */

    code = sys_setgroups(td, uap);
    /* Note that if there is a pag already in the new groups we don't
     * overwrite it with the old pag.
     */
    cr = crdup(td->td_ucred);

    if (PagInCred(cr) == NOPAG) {
	if (afs_IsPagId(treq.uid)) {
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
    gid_t *gidset = NULL;
    int gidset_len = ngroups_max + 1;
    int ngroups, code;
    int j;

    AFS_STATCNT(setpag);

    if (pagvalue == -1) {
	code = afs_genpag(*cred, &pagvalue);
	if (code != 0) {
	    goto done;
	}
    }

    gidset = osi_Alloc(gidset_len * sizeof(gid_t));
    if (gidset == NULL) {
	code = ENOMEM;
	goto done;
    }
    ngroups = afs_getgroups(*cred, gidset_len, gidset);
    if (afs_get_pag_from_groups(gidset[1], gidset[2]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if (ngroups + 2 > gidset_len) {
	    code = E2BIG;
	    goto done;
	}
	for (j = ngroups - 1; j >= 1; j--) {
	    gidset[j + 2] = gidset[j];
	}
	ngroups += 2;
    }
    *newpag = pagvalue;
    afs_get_groups_from_pag(*newpag, &gidset[1], &gidset[2]);
    code = afs_setgroups(td, cred, ngroups, gidset, change_parent);

 done:
    if (gidset != NULL) {
	osi_Free(gidset, gidset_len * sizeof(gid_t));
    }
    return code;
}


static int
afs_getgroups(struct ucred *cred, int ngroups, gid_t * gidset)
{
    int ngrps, savengrps;
    gid_t *gp;

    AFS_STATCNT(afs_getgroups);
    savengrps = ngrps = opr_min(ngroups, cred->cr_ngroups);
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
