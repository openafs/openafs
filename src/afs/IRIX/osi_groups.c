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
 * afsDFS_SetPagInCred (shared with SGI)
 * osi_DFSGetPagFromCred (shared with SGI)
 * Afs_xsetgroups (syscall)
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


/* This is common code between SGI's DFS and our AFS. Do *not* alter it's
 * interface or semantics without notifying SGI.
 */
#ifdef AFS_SGI65_ENV
/* fixup_pags returns error code if relevant or 0 on no error.
 * Sets up the cred for the call to estgroups. This is pretty convoluted
 * in order to avoid including the private proc.h header file.
 */
int
fixup_pags(int **credpp, int ngroups, gid_t * gidset, int old_afs_pag,
	   int old_dfs_pag)
{
    int new_afs_pag = 0;
    int new_dfs_pag = 0;
    int new, old;
    int changed = 0;
    cred_t *cr;
    gid_t groups[NGROUPS_UMAX];
    int code;

    if (ngroups < 0 || ngroups > ngroups_max)
	return EINVAL;

    if (ngroups) {
	AFS_COPYIN(gidset, groups, ngroups * sizeof(gid_t), code);
	if (code)
	    return EFAULT;
    }

    if (ngroups >= 2) {		/* possibly an AFS PAG */
	new_afs_pag =
	    (afs_get_pag_from_groups(groups[0], groups[1]) != NOPAG);
    }
    if (ngroups >= 1) {		/* possibly a DFS PAG */
	new_dfs_pag = (int)groups[ngroups - 1];
	if (((new_dfs_pag >> 24) & 0xff) == 'A')
	    new_dfs_pag = (int)groups[ngroups - 1];
	else
	    new_dfs_pag = 0;
    }

    /* Now compute the number of groups we will need. */
    new = ngroups;
    if (old_afs_pag && !new_afs_pag)	/* prepend old AFS pag */
	new += 2;
    if (old_dfs_pag && !new_dfs_pag)	/* append old DFS pag */
	new++;

    if (new > ngroups_max)
	return EINVAL;		/* sorry */

    cr = crdup(OSI_GET_CURRENT_CRED());	/* we will replace all the groups. */
    memset(&cr->cr_groups, 0, ngroups_max * sizeof(gid_t));

    /* Now cobble the new groups list together. */
    new = 0;
    old = 0;
    if (old_afs_pag && !new_afs_pag) {	/* prepend old AFS pag */
	gid_t g0, g1;
	changed = 1;
	afs_get_groups_from_pag(old_afs_pag, &g0, &g1);
	cr->cr_groups[new++] = g0;
	cr->cr_groups[new++] = g1;
    }

    for (old = 0; old < ngroups; old++)
	cr->cr_groups[new++] = groups[old];

    if (old_dfs_pag && !new_dfs_pag) {	/* append old DFS pag */
	changed = 1;
	cr->cr_groups[new++] = old_dfs_pag;
    }

    /* Now, did we do anything? */
    if (changed) {
	cr->cr_ngroups = new;
	*credpp = cr;
    } else {
	crfree(cr);
	*credpp = NULL;
    }
    return 0;
}
#else
/*
 * Generic routine to set the PAG in the cred for AFS and DFS.
 * If flag = 0 this is a DFS pag held in one group.
 * If flag = 1 this is a AFS pag held in two group entries
 */
static int
afsDFS_SetPagInCred(struct ucred *credp, int pag, int flag)
{
    int *gidset;
    int i, ngrps;
    gid_t g0, g1;
    int n = 0;
    struct ucred *newcredp;
    int groups_taken = (flag ? 2 : 1);

    ngrps = credp->cr_ngroups + groups_taken;
    if (ngrps >= ngroups_max)
	return E2BIG;


    if (flag) {
	/* Break out the AFS pag into two groups */
	afs_get_groups_from_pag(pag, &g0, &g1);
    }

    newcredp = crdup(credp);
    newcredp->cr_ngroups = ngrps;

    if (flag) {
	/* AFS case */
	newcredp->cr_groups[0] = g0;
	newcredp->cr_groups[1] = g1;
    } else {
	/* DFS case */
	if (PagInCred(newcredp) != NOPAG) {
	    /* found an AFS PAG is set in this cred */
	    n = 2;
	}
	newcredp->cr_groups[n] = pag;
    }
    for (i = n; i < credp->cr_ngroups; i++)
	newcredp->cr_groups[i + groups_taken] = credp->cr_groups[i];

    /* estgroups sets current threads cred from newcredp and crfree's credp */
    estgroups(credp, newcredp);

    return 0;
}
#endif /* AFS_SGI65_ENV */

/* SGI's osi_GetPagFromCred - They return a long. */
int
osi_DFSGetPagFromCred(struct ucred *credp)
{
    int pag;
    int ngroups;

    /*
     *  For IRIX, the PAG is stored in the first entry
     *  of the gruop list in the cred structure.  gid_t's
     *  are 32 bits on 64 bit and 32 bit hardware types.
     *  As of Irix 6.5, the DFS pag is the last group in the list.
     */
    ngroups = credp->cr_ngroups;
    if (ngroups < 1)
	return NOPAG;
    /*
     *  Keep in mind that we might be living with AFS here.
     *  This means we don't really know if our DFS PAG is in
     *  the first or third group entry.
     */
#ifdef AFS_SGI65_ENV
    pag = credp->cr_groups[ngroups - 1];
#else
    pag = credp->cr_groups[0];
    if (PagInCred(credp) != NOPAG) {
	/* AFS has a PAG value in the first two group entries */
	if (ngroups < 3)
	    return NOPAG;
	pag = credp->cr_groups[2];
    }
#endif
    if (((pag >> 24) & 0xff) == 'A')
	return pag;
    else
	return NOPAG;
}

int
Afs_xsetgroups(int ngroups, gid_t * gidset)
{
    int old_afs_pag = NOPAG;
    int old_dfs_pag = NOPAG;
    int code = 0;
    struct ucred *credp = OSI_GET_CURRENT_CRED();
    struct ucred *modcredp;


    credp = OSI_GET_CURRENT_CRED();
    /* First get any old PAG's */
    old_afs_pag = PagInCred(credp);
    old_dfs_pag = osi_DFSGetPagFromCred(credp);

    /* Set the passed in group list. */
    if (code = setgroups(ngroups, gidset))
	return code;

#ifdef AFS_SGI65_ENV
    if (old_afs_pag == NOPAG && old_dfs_pag == NOPAG)
	return 0;

    /* Well, we could get the cred, except it's in the proc struct which
     * is not a publicly available header. And the cred won't be valid on
     * the uthread until we return to user space. So, we examine the passed
     * in groups in fixup_pags.
     */
    code =
	fixup_pags(&modcredp, ngroups, gidset,
		   (old_afs_pag == NOPAG) ? 0 : old_afs_pag,
		   (old_dfs_pag == NOPAG) ? 0 : old_dfs_pag);
    if (!code && modcredp)
	estgroups(OSI_GET_CURRENT_PROCP(), modcredp);
#else

    /*
     * The setgroups gave our curent thread a new cred pointer
     * Get the value again
     */
    credp = OSI_GET_CURRENT_CRED();
    if ((PagInCred(credp) == NOPAG) && (old_afs_pag != NOPAG)) {
	/* reset the AFS PAG */
	code = afsDFS_SetPagInCred(credp, old_afs_pag, 1);
    }
    /*
     * Once again get the credp because the afsDFS_SetPagInCred might have
     * assigned a new one.
     */
    credp = OSI_GET_CURRENT_CRED();
    if ((osi_DFSGetPagFromCred(credp) == NOPAG)
	&& (old_dfs_pag != NOPAG)) {
	code = afsDFS_SetPagInCred(credp, old_dfs_pag, 0);
    }
#endif /* AFS_SGI65_ENV */
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
#if defined(KERNEL_HAVE_UERROR)
	    return (setuerror(E2BIG), E2BIG);
#else
	    return (E2BIG);
#endif
	}
	for (j = ngroups - 1; j >= 0; j--) {
	    gidset[j + 2] = gidset[j];
	}
	ngroups += 2;
    }
    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
    afs_get_groups_from_pag(*newpag, &gidset[0], &gidset[1]);
    if (code = afs_setgroups(cred, ngroups, gidset, change_parent)) {
#if defined(KERNEL_HAVE_UERROR)
	return (setuerror(code), code);
#else
	return code;
#endif
    }
    return code;
}


static int
afs_getgroups(struct ucred *cred, int ngroups, gid_t * gidset)
{
    int ngrps, savengrps;
    gid_t *gp;

    gidset[0] = gidset[1] = 0;
    AFS_STATCNT(afs_getgroups);
    savengrps = ngrps = MIN(ngroups, cred->cr_ngroups);
    gp = cred->cr_groups;
    while (ngrps--)
	*gidset++ = *gp++;
    return savengrps;
}



static int
afs_setgroups(struct ucred **cred, int ngroups, gid_t * gidset,
	      int change_parent)
{
    gid_t *gp;
    cred_t *cr, *newcr;

    AFS_STATCNT(afs_setgroups);

    if (ngroups > ngroups_max)
	return EINVAL;
    cr = *cred;
    if (!change_parent)
	newcr = crdup(cr);
    else
	newcr = cr;
    newcr->cr_ngroups = ngroups;
    gp = newcr->cr_groups;
    while (ngroups--)
	*gp++ = *gidset++;
    if (!change_parent) {
#ifdef AFS_SGI65_ENV
	estgroups(OSI_GET_CURRENT_PROCP(), newcr);
#else
	estgroups(cr, newcr);
#endif
    }
    *cred = newcr;
    return (0);
}
