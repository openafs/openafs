/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#ifndef DJGPP
#include <windows.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "afsd.h"

/* 
 * This next lock controls access to all cm_aclent structures in the system,
 * in either the free list or in the LRU queue.  A read lock prevents someone
 * from modifying the list(s), and a write lock is required for modifying
 * the list.  The actual data stored in the randomUid and randomAccess fields
 * is actually maintained as up-to-date or not via the scache llock.
 * An aclent structure is free if it has no back vnode pointer.
 */
osi_rwlock_t cm_aclLock;		/* lock for system's aclents */
cm_aclent_t *cm_aclLRUp;                /* LRUQ for dudes in vnode's lists */
cm_aclent_t *cm_aclLRUEndp;             /* ditto */
/* 
 * Get an acl cache entry for a particular user and file, or return that it doesn't exist.
 * Called with the scp locked.
 */
long cm_FindACLCache(cm_scache_t *scp, cm_user_t *userp, long *rightsp)
{
    cm_aclent_t *aclp;
    long retval = -1;

    lock_ObtainWrite(&cm_aclLock);
    for (aclp = scp->randomACLp; aclp; aclp = aclp->nextp) {
        if (aclp->userp == userp) {
            if (aclp->tgtLifetime && aclp->tgtLifetime <= (long) osi_Time()) {
                /* ticket expired */
                aclp->tgtLifetime = 0;
                *rightsp = 0;   /* get a new acl from server */

                /* Shouldn't we remove this entry from the scp?
                 * 2005-01-25 - jaltman@secure-endpoints.com
                 */
            } else {
                *rightsp = aclp->randomAccess;
                if (cm_aclLRUEndp == aclp)
                    cm_aclLRUEndp = (cm_aclent_t *) osi_QPrev(&aclp->q);

                /* move to the head of the LRU queue */
                osi_QRemove((osi_queue_t **) &cm_aclLRUp, &aclp->q);
                osi_QAddH((osi_queue_t **) &cm_aclLRUp,
                           (osi_queue_t **) &cm_aclLRUEndp,
                           &aclp->q);
                retval = 0;     /* success */
            }               
            break;
        }
    }

    lock_ReleaseWrite(&cm_aclLock);
    return retval;
}       


/* 
 * This function returns a free (not in the LRU queue) acl cache entry.
 * It must be called with the cm_aclLock lock held.
 */
static cm_aclent_t *GetFreeACLEnt(void)
{
    cm_aclent_t *aclp;
    cm_aclent_t *taclp;
    cm_aclent_t **laclpp;

    if (cm_aclLRUp == NULL)
        osi_panic("empty aclent LRU", __FILE__, __LINE__);

    aclp = cm_aclLRUEndp;
    if (aclp == cm_aclLRUEndp)
        cm_aclLRUEndp = (cm_aclent_t *) osi_QPrev(&aclp->q);
    osi_QRemove((osi_queue_t **) &cm_aclLRUp, &aclp->q);
    if (aclp->backp) {
        /* 
         * Remove the entry from the vnode's list 
         */
        laclpp = &aclp->backp->randomACLp;
        for (taclp = *laclpp; taclp; laclpp = &taclp->nextp, taclp = *laclpp) {
            if (taclp == aclp) 
                break;
        }
        if (!taclp) 
            osi_panic("GetFreeACLEnt race", __FILE__, __LINE__);
        *laclpp = aclp->nextp;			/* remove from vnode list */
        aclp->backp = NULL;
    }

    /* release the old user */
    if (aclp->userp) {
        cm_ReleaseUser(aclp->userp);
        aclp->userp = NULL;
    }
    return aclp;
}


/* 
 * Add rights to an acl cache entry.  Do the right thing if not present, 
 * including digging up an entry from the LRU queue.
 *
 * The scp must be locked when this function is called.
 */
long cm_AddACLCache(cm_scache_t *scp, cm_user_t *userp, long rights)
{
    register struct cm_aclent *aclp;

    lock_ObtainWrite(&cm_aclLock);
    for (aclp = scp->randomACLp; aclp; aclp = aclp->nextp) {
        if (aclp->userp == userp) {
            aclp->randomAccess = rights;
            if (aclp->tgtLifetime == 0) 
                aclp->tgtLifetime = cm_TGTLifeTime(pag);
            lock_ReleaseWrite(&cm_aclLock);
            return 0;
        }
    }

    /* 
     * Didn't find the dude we're looking for, so take someone from the LRUQ 
     * and  reuse. But first try the free list and see if there's already 
     * someone there.
     */
    aclp = GetFreeACLEnt();		 /* can't fail, panics instead */
    osi_QAddH((osi_queue_t **) &cm_aclLRUp, (osi_queue_t **) &cm_aclLRUEndp, &aclp->q);
    aclp->backp = scp;
    aclp->nextp = scp->randomACLp;
    scp->randomACLp = aclp;
    cm_HoldUser(userp);
    aclp->userp = userp;
    aclp->randomAccess = rights;
    aclp->tgtLifetime = cm_TGTLifeTime(userp);
    lock_ReleaseWrite(&cm_aclLock);

    return 0;
}

/* 
 * Initialize the cache to have an entries.  Called during system startup.
 */
long cm_InitACLCache(long size)
{
    cm_aclent_t *aclp;
    long i;
    static osi_once_t once;

    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_aclLock, "cm_aclLock");
        osi_EndOnce(&once);
    }

    lock_ObtainWrite(&cm_aclLock);
    cm_aclLRUp = cm_aclLRUEndp = NULL;
    aclp = (cm_aclent_t *) malloc(size * sizeof(cm_aclent_t));
    memset(aclp, 0, size * sizeof(cm_aclent_t));

    /* 
     * Put all of these guys on the LRU queue 
     */
    for (i = 0; i < size; i++) {
        osi_QAddH((osi_queue_t **) &cm_aclLRUp, (osi_queue_t **) &cm_aclLRUEndp, &aclp->q);
        aclp++;
    }
    lock_ReleaseWrite(&cm_aclLock);
    return 0;
}


/* 
 * Free all associated acl entries.  We actually just clear the back pointer
 * since the acl entries are already in the free list.  The scp must be locked
 * or completely unreferenced (such as when called while recycling the scp).
 */
void cm_FreeAllACLEnts(cm_scache_t *scp)
{
    cm_aclent_t *aclp;
    cm_aclent_t *taclp;

    lock_ObtainWrite(&cm_aclLock);
    for (aclp = scp->randomACLp; aclp; aclp = taclp) {
        taclp = aclp->nextp;
        if (aclp->userp) {
            cm_ReleaseUser(aclp->userp);
            aclp->userp = NULL;
        }
        aclp->backp = (struct cm_scache *) 0;
    }

    scp->randomACLp = (struct cm_aclent *) 0;
    scp->anyAccess = 0;		/* reset this, too */
    lock_ReleaseWrite(&cm_aclLock);
}


/* 
 * Invalidate all ACL entries for particular user on this particular vnode.
 *
 * The scp must be locked.
 */
void cm_InvalidateACLUser(cm_scache_t *scp, cm_user_t *userp)
{
    cm_aclent_t *aclp;
    cm_aclent_t **laclpp;

    lock_ObtainWrite(&cm_aclLock);
    laclpp = &scp->randomACLp;
    for (aclp = *laclpp; aclp; laclpp = &aclp->nextp, aclp = *laclpp) {
        if (userp == aclp->userp) {	/* One for a given user/scache */
            *laclpp = aclp->nextp;
            cm_ReleaseUser(aclp->userp);
            aclp->userp = NULL;
            aclp->backp = (struct cm_scache *) 0;
            break;
        }
    }
    lock_ReleaseWrite(&cm_aclLock);
}
