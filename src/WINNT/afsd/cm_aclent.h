/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _CM_ACLENT_H_
#define _CM_ACLENT_H_ 	1

#include <osi.h>

#define cm_TGTLifeTime(x)	(0x7fffffff)

/*
 * Structure to hold an acl entry for a cached file
 */
typedef struct cm_aclent {
    osi_queue_t q;		/* for quick removal from LRUQ */
    struct cm_aclent *nextp;	/* next guy same vnode */
    struct cm_scache *backp;	/* back ptr to vnode */
    struct cm_user *userp;	/* user whose access is cached */
    long randomAccess;		/* watch for more rights in acl.h */
    long tgtLifetime;		/* time this expires */
} cm_aclent_t;

extern osi_rwlock_t cm_aclLock;

extern long cm_InitACLCache(long size);

extern long cm_FindACLCache(struct cm_scache *scp, struct cm_user *userp, long *rightsp);

static cm_aclent_t *GetFreeACLEnt(void);

extern long cm_AddACLCache(struct cm_scache *scp, struct cm_user *userp, long rights);

extern void cm_FreeAllACLEnts(struct cm_scache *scp);

extern void cm_InvalidateACLUser(struct cm_scache *scp, struct cm_user *userp);

#endif  /* _CM_ACLENT_H_ */
