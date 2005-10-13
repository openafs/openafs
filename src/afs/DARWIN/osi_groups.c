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

RCSID
    ("$Header$");

/* We should be doing something better anyway */
#ifdef AFS_DARWIN80_ENV
int
setpag(proc, cred, pagvalue, newpag, change_parent)
     struct proc *proc;
     struct ucred **cred;
     afs_uint32 pagvalue;
     afs_uint32 *newpag;
     afs_uint32 change_parent;
{ 
    return -1;
}
#else
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/afs_stats.h"	/* statistics */

static int
  afs_getgroups(struct ucred *cred, int ngroups, gid_t * gidset);

static int
  afs_setgroups(struct proc *proc, struct ucred **cred, int ngroups,
		gid_t * gidset, int change_parent);

int
Afs_xsetgroups(p, args, retval)
     struct proc *p;
     void *args;
     int *retval;
{
    int code = 0;
    struct vrequest treq;
    struct ucred *cr;

    pcred_readlock(p);
    cr = crdup(p->p_cred->pc_ucred);
    pcred_unlock(p);

    AFS_STATCNT(afs_xsetgroups);
    AFS_GLOCK();

    code = afs_InitReq(&treq, cr);
    AFS_GUNLOCK();
    crfree(cr);
    if (code)
	return setgroups(p, args, retval);	/* afs has shut down */

    code = setgroups(p, args, retval);
    /* Note that if there is a pag already in the new groups we don't
     * overwrite it with the old pag.
     */
    pcred_readlock(p);
    cr = crdup(p->p_cred->pc_ucred);
    pcred_unlock(p);

    if (PagInCred(cr) == NOPAG) {
	if (((treq.uid >> 24) & 0xff) == 'A') {
	    AFS_GLOCK();
	    /* we've already done a setpag, so now we redo it */
	    AddPag(p, treq.uid, &cr);
	    AFS_GUNLOCK();
	}
    }
    crfree(cr);
    return code;
}


    gid_t gidset[NGROUPS];
    int ngroups, code;
    int j;

    AFS_STATCNT(setpag);
    ngroups = afs_getgroups(*cred, NGROUPS, gidset);
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
    *newpag = (pagvalue == -1 ? genpag() : pagvalue);
    afs_get_groups_from_pag(*newpag, &gidset[1], &gidset[2]);
    code = afs_setgroups(proc, cred, ngroups, gidset, change_parent);
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
afs_setgroups(struct proc *proc, struct ucred **cred, int ngroups,
	      gid_t * gidset, int change_parent)
{
    int ngrps;
    int i;
    gid_t *gp;
    struct ucred *oldcr, *cr;

    AFS_STATCNT(afs_setgroups);
    /*
     * The real setgroups() call does this, so maybe we should too.
     *
     */
    if (ngroups > NGROUPS)
	return EINVAL;
    cr = *cred;
    cr->cr_ngroups = ngroups;
    gp = cr->cr_groups;
    while (ngroups--)
	*gp++ = *gidset++;
    if (change_parent) {
	crhold(cr);
	pcred_writelock(proc->p_pptr);
	oldcr = proc->p_pptr->p_cred->pc_ucred;
	proc->p_pptr->p_cred->pc_ucred = cr;
	pcred_unlock(proc->p_pptr);
	crfree(oldcr);
    }
    crhold(cr);
    pcred_writelock(proc);
    oldcr = proc->p_cred->pc_ucred;
    proc->p_cred->pc_ucred = cr;
    pcred_unlock(proc);
    crfree(oldcr);
    return (0);
}
#endif
