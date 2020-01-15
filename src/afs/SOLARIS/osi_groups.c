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

#include <unistd.h>
#ifdef AFS_SUN510_ENV
#include <sys/cred.h>
#endif


#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */


static int
  afs_getgroups(struct cred *cred, gid_t * gidset);

static int
  afs_setgroups(struct cred **cred, int ngroups, gid_t * gidset,
		int change_parent);


#ifdef AFS_SUN511_ENV
int64_t
#else
int
#endif
afs_xsetgroups(u_int uap, gid_t *rvp)
{
    /* 'uap' is our gidsetsize; 'rvp' is our gidset */
    int code = 0;
    struct vrequest treq;
    struct proc *proc = ttoproc(curthread);

    AFS_STATCNT(afs_xsetgroups);
    AFS_GLOCK();
    code = afs_InitReq(&treq, proc->p_cred);
    AFS_GUNLOCK();
    if (code)
	return code;
    code = setgroups(uap, rvp);

    /* Note that if there is a pag already in the new groups we don't
     * overwrite it with the old pag.
     */
    if (PagInCred(proc->p_cred) == NOPAG) {
	if (afs_IsPagId(treq.uid)) {
	    AFS_GLOCK();
	    /* we've already done a setpag, so now we redo it */
	    AddPag(treq.uid, &proc->p_cred);
	    AFS_GUNLOCK();
	}
    }
    return code;
}

#ifdef AFS_PAG_ONEGROUP_ENV
/**
 * Take a PAG, and put it into the given array of gids.
 *
 * @param[in] pagvalue     The numeric id for the PAG to assign (must not be -1)
 * @param[in] gidset       An array of gids
 * @param[inout] a_ngroups How many entries in 'gidset' have valid gids
 * @param[in] gidset_sz    The number of bytes allocated for 'gidset'
 *
 * @return error code
 */
static int
pag_to_gidset(afs_uint32 pagvalue, gid_t *gidset, int *a_ngroups,
              size_t gidset_sz)
{
    int i;
    gid_t *gidslot = NULL;
    int ngroups = *a_ngroups;

    osi_Assert(pagvalue != -1);

    /* See if we already have a PAG gid */
    for (i = 0; i < ngroups; i++) {
	if (afs_IsPagId(gidset[i])) {
            gidslot = &gidset[i];
            break;
        }
    }

    if (gidslot == NULL) {
        /* If we don't already have a PAG, grow the groups list by one, and put
         * our PAG in the new empty slot. */
        if ((sizeof(gidset[0])) * (ngroups + 1) > gidset_sz) {
            return E2BIG;
        }
        ngroups += 1;
        gidslot = &gidset[ngroups-1];
    }

    /*
     * For newer Solaris releases (Solaris 11), we cannot control the order of
     * the supplemental groups list of a process, so we can't store PAG gids as
     * the first two gids anymore. To make finding a PAG gid easier to find,
     * just use a single gid to represent a PAG, and just store the PAG id
     * itself in there, like is currently done on Linux. Note that our PAG ids
     * all start with the byte 0x41 ('A'), so we should not collide with
     * anything. GIDs with the highest bit set are special (used for Windows
     * SID mapping), but anything under that range should be fine.
     */
    *gidslot = pagvalue;

    *a_ngroups = ngroups;

    return 0;
}
#else
/* For earlier Solaris releases, convert a PAG number into two gids, and store
 * those gids as the first groups in the supplemental group list. */
static int
pag_to_gidset(afs_uint32 pagvalue, gid_t *gidset, int *a_ngroups,
              size_t gidset_sz)
{
    int j;
    int ngroups = *a_ngroups;

    osi_Assert(pagvalue != -1);

    if (afs_get_pag_from_groups(gidset[0], gidset[1]) == NOPAG) {
	/* We will have to shift grouplist to make room for pag */
	if ((sizeof(gidset[0])) * (ngroups + 2) > gidset_sz) {
	    return E2BIG;
	}
	for (j = ngroups - 1; j >= 0; j--) {
	    gidset[j + 2] = gidset[j];
	}
	ngroups += 2;
    }
    afs_get_groups_from_pag(pagvalue, &gidset[0], &gidset[1]);

    *a_ngroups = ngroups;

    return 0;
}
#endif

int
setpag(cred, pagvalue, newpag, change_parent)
     struct cred **cred;
     afs_uint32 pagvalue;
     afs_uint32 *newpag;
     afs_uint32 change_parent;
{
    gid_t *gidset;
    int ngroups, code;
    size_t gidset_sz;

    AFS_STATCNT(setpag);

    if (pagvalue == -1) {
	code = afs_genpag(*cred, &pagvalue);
	if (code != 0) {
	    return code;
	}
    }

    /* Derive gidset size from running kernel's ngroups_max;
     * default 16, but configurable up to 32 (Sol10) or
     * 1024 (Sol11).
     */
    gidset_sz = sizeof(gidset[0]) * ngroups_max;

    /* must use osi_Alloc, osi_AllocSmallSpace may not be enough. */
    gidset = osi_Alloc(gidset_sz);

    mutex_enter(&curproc->p_crlock);
    ngroups = afs_getgroups(*cred, gidset);

    code = pag_to_gidset(pagvalue, gidset, &ngroups, gidset_sz);
    if (code != 0) {
        mutex_exit(&curproc->p_crlock);
        goto done;
    }

    *newpag = pagvalue;

    /* afs_setgroups will release curproc->p_crlock */
    /* exit action is same regardless of code */
    code = afs_setgroups(cred, ngroups, gidset, change_parent);

 done:
    osi_Free((char *)gidset, gidset_sz);
    return code;
}


static int
afs_getgroups(struct cred *cred, gid_t * gidset)
{
    int ngrps, savengrps;
    const gid_t *gp;

    AFS_STATCNT(afs_getgroups);

    gidset[0] = gidset[1] = 0;
#if defined(AFS_SUN510_ENV)
    savengrps = ngrps = crgetngroups(cred);
    gp = crgetgroups(cred);
#else
    savengrps = ngrps = cred->cr_ngroups;
    gp = cred->cr_groups;
#endif
    while (ngrps--)
	*gidset++ = *gp++;
    return savengrps;
}



static int
afs_setgroups(struct cred **cred, int ngroups, gid_t * gidset,
	      int change_parent)
{
#ifndef AFS_PAG_ONEGROUP_ENV
    gid_t *gp;
#endif

    AFS_STATCNT(afs_setgroups);

    if (ngroups > ngroups_max) {
	mutex_exit(&curproc->p_crlock);
	return EINVAL;
    }
    if (!change_parent)
	*cred = (struct cred *)crcopy(*cred);

#ifdef AFS_PAG_ONEGROUP_ENV
    crsetgroups(*cred, ngroups, gidset);
#else
# if defined(AFS_SUN510_ENV)
    crsetgroups(*cred, ngroups, gidset);
    gp = crgetgroups(*cred);
# else
    (*cred)->cr_ngroups = ngroups;
    gp = (*cred)->cr_groups;
# endif
    while (ngroups--)
	*gp++ = *gidset++;
#endif /* !AFS_PAG_ONEGROUP_ENV */

    mutex_exit(&curproc->p_crlock);
    if (!change_parent)
	crset(curproc, *cred);	/* broadcast to all threads */
    return (0);
}

#ifdef AFS_PAG_ONEGROUP_ENV
afs_int32
osi_get_group_pag(struct cred *cred) {
    const gid_t *gidset;
    int ngroups;
    int i;

    gidset = crgetgroups(cred);
    ngroups = crgetngroups(cred);

    for (i = 0; i < ngroups; i++) {
	if (afs_IsPagId(gidset[i])) {
            return gidset[i];
        }
    }
    return NOPAG;
}
#endif
