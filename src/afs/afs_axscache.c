/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/stds.h"
static struct axscache *afs_axsfreelist = NULL;

#define NAXSs (1000 / sizeof(struct axscache))
static struct xfreelist {
    struct xfreelist *next;
    struct axscache data[NAXSs];
} *xfreemallocs = 0;
static int afs_xaxscnt = 0;
afs_rwlock_t afs_xaxs;

/* takes an address of an access cache & uid, returns ptr */
/* PRECONDITION: first field has been checked and doesn't match!
 * INVARIANT:  isparent(i,j) ^ isparent(j,i)  (ie, they switch around)
 */
struct axscache *
afs_SlowFindAxs(struct axscache **cachep, afs_int32 id)
{
    struct axscache *i, *j;

    j = (*cachep);
    i = j->next;
    while (i) {
	if (i->uid == id) {
	    axs_Front(cachep, j, i);	/* maintain LRU queue */
	    return (i);
	}

	if ((j = i->next)) {	/* ASSIGNMENT HERE! */
	    if (j->uid == id) {
		axs_Front(cachep, i, j);
		return (j);
	    }
	} else
	    return ((struct axscache *)NULL);
	i = j->next;
    }
    return ((struct axscache *)NULL);
}


struct axscache *
axs_Alloc(void)
{
    struct axscache *i, *j;
    struct xfreelist *h, *xsp;
    int k;

    ObtainWriteLock(&afs_xaxs, 174);
    if ((i = afs_axsfreelist)) {
	afs_axsfreelist = i->next;
	ReleaseWriteLock(&afs_xaxs);
	return i;
    } else {
	h = afs_osi_Alloc(sizeof(struct xfreelist));
	osi_Assert(h != NULL);
	afs_xaxscnt++;
	xsp = xfreemallocs;
	xfreemallocs = h;
	xfreemallocs->next = xsp;
	i = j = h->data;
	for (k = 0; k < NAXSs - 1; k++, i++) {
	    i->uid = -2;
	    i->axess = 0;
	    i->next = ++j;	/* need j because order of evaluation not defined */
	}
	i->uid = -2;
	i->axess = 0;
	i->next = NULL;
	afs_axsfreelist = (h->data)->next;
    }
    ReleaseWriteLock(&afs_xaxs);
    return (h->data);
}


#define axs_Free(axsp) { \
 ObtainWriteLock(&afs_xaxs,175);             \
 axsp->next = afs_axsfreelist;           \
 afs_axsfreelist = axsp;                 \
 ReleaseWriteLock(&afs_xaxs);            \
}


/* I optimize for speed on lookup, and don't give a RIP about delete.
 */
void
afs_RemoveAxs(struct axscache **headp, struct axscache *axsp)
{
    struct axscache *i, *j;

    if (*headp && axsp) {	/* is bullet-proofing really neccessary? */
	if (*headp == axsp) {	/* most common case, I think */
	    *headp = axsp->next;
	    axs_Free(axsp);
	    return;
	}

	i = *headp;
	j = i->next;

	while (j) {
	    if (j == axsp) {
		i->next = j->next;
		axs_Free(axsp);
		return;
	    }
	    if ((i = j->next)) {	/* ASSIGNMENT HERE! */
		j->next = i->next;
		axs_Free(axsp);
		return;
	    }
	}
    }
    /* end of "if neither pointer is NULL" */
    return;			/* !#@  FAILED to find it! */
}


/*
 * Takes an entire list of access cache structs and prepends them, lock, stock,
 * and barrel, to the front of the freelist.
 */
void
afs_FreeAllAxs(struct axscache **headp)
{
    struct axscache *i, *j;

    i = *headp;
    j = NULL;

    while (i) {			/* chase down the list 'til we reach the end */
	j = i->next;
	if (!j) {
	    ObtainWriteLock(&afs_xaxs, 176);
	    i->next = afs_axsfreelist;	/* tack on the freelist to the end */
	    afs_axsfreelist = *headp;
	    ReleaseWriteLock(&afs_xaxs);
	    *headp = NULL;
	    return;
	}
	i = j->next;
    }

    if (j) {			/* we ran off the end of the list... */
	ObtainWriteLock(&afs_xaxs, 177);
	j->next = afs_axsfreelist;	/* tack on the freelist to the end */
	afs_axsfreelist = *headp;
	ReleaseWriteLock(&afs_xaxs);
    }
    *headp = NULL;
    return;
}


void
shutdown_xscache(void)
{
    struct xfreelist *xp, *nxp;

    AFS_RWLOCK_INIT(&afs_xaxs, "afs_xaxs");
    xp = xfreemallocs;
    while (xp) {
	nxp = xp->next;
	afs_osi_Free(xp, sizeof(struct xfreelist));
	xp = nxp;
    }
    afs_axsfreelist = NULL;
    xfreemallocs = NULL;
}
