/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFSAXCACHEH
#define AFSAXCACHEH

/*  Access cache -
 *  The way to use this package is:
 *  If the cache pointer is not NULL, call afs_findAxs, and if it's not
 *                                    NULL, use its access field
 *  Otherwise,
 *       axs_Alloc a new one,
 *       fill it in,
 *       insert it at the head of the list.
 *
 *  Of course, don't forget to axs_Free them occasionally,
 *
 *  Alloc and Free need a lock on the freelist, the other guys are safe if the
 *  parent structure is locked, but probably REQUIRE the parent to be locked...
 */

struct axscache {
    afs_int32 uid;		/* most frequently used field, I think */
    afs_int32 axess;
    struct axscache *next;
};

/* DON'T use this with a NULL pointer!
 * the quick check should cover 99.9% of the cases
 */
#define afs_FindAxs(cachep,id) (((cachep)->uid == id) ? (cachep) : afs_SlowFindAxs(&(cachep),(id)))

#define axs_Front(head,pp,p) {(pp)->next = (p)->next; (p)->next= *(head);*(head)=(p);}

#define afs_AddAxs(cachep,id,bits) {   \
 struct axscache *ac;                  \
 if ((ac = axs_Alloc())) {               \
 ac->uid = (id);                         \
 ac->axess = (afs_int32)(bits);               \
 ac->next = (cachep);                    \
 cachep = ac; }}

#endif
