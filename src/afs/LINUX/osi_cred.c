/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi_cred.c - Linux cred handling routines.
 *
 */
#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"

/* Setup a pool for creds. Allocate several at a time. */
#define CRED_ALLOC_STEP 29 /* at 140 bytes/cred = 4060 bytes. */


static cred_t *cred_pool = NULL;
int cred_allocs = 0;
int ncreds_inuse = 0;

/* Cred locking assumes current single threaded non-preemptive kernel.
 * Also assuming a fast path through both down and up if no waiters. Otherwise,
 * test if no creds in pool before grabbing lock in crfree().
 */
static struct semaphore linux_cred_pool_lock = MUTEX;
#define CRED_LOCK() down(&linux_cred_pool_lock)
#define CRED_UNLOCK() up(&linux_cred_pool_lock)

cred_t *crget(void)
{
    cred_t *tmp;
    int i;

    CRED_LOCK();
    if (!cred_pool) {
	cred_allocs++;
	cred_pool = (cred_t*)osi_Alloc(CRED_ALLOC_STEP * sizeof(cred_t));
	if (!cred_pool)
	    osi_Panic("crget: No more memory for creds!\n");
	
	for (i=0; i < CRED_ALLOC_STEP-1; i++)
	    cred_pool[i].cr_ref = (int)&cred_pool[i+1];
	cred_pool[i].cr_ref = 0;
    }
    tmp = cred_pool;
    cred_pool = (cred_t*)tmp->cr_ref;
    ncreds_inuse++;
    CRED_UNLOCK();

    memset(tmp, 0, sizeof(cred_t));
    tmp->cr_ref = 1;
    return tmp;
}

void crfree(cred_t *cr)
{
    if (cr->cr_ref > 1) {
	cr->cr_ref--;
	return;
    }

    CRED_LOCK();
    cr->cr_ref = (int)cred_pool;
    cred_pool = cr;
    CRED_UNLOCK();
    ncreds_inuse --;
}


/* Return a duplicate of the cred. */
cred_t *crdup(cred_t *cr)
{
    cred_t *tmp = crget();
    *tmp = *cr;
    tmp->cr_ref = 1;
    return tmp;
}

cred_t *crref(void)
{
    cred_t *cr = crget();
    cr->cr_uid = current->fsuid;
    cr->cr_ruid = current->uid;
    cr->cr_gid = current->fsgid;
    cr->cr_rgid = current->gid;
    memcpy(cr->cr_groups, current->groups, NGROUPS * sizeof(gid_t));
    cr->cr_ngroups = current->ngroups;
    return cr;
}


/* Set the cred info into the current task */
void crset(cred_t *cr)
{
    current->fsuid = cr->cr_uid;
    current->uid = cr->cr_ruid;
    current->fsgid = cr->cr_gid;
    current->gid = cr->cr_rgid;
    memcpy(current->groups, cr->cr_groups, NGROUPS * sizeof(gid_t));
    current->ngroups = cr->cr_ngroups;
}
