/*
 * Copyright (c) 2012 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Secure Endpoints Inc. nor the names of its contributors
 *   may be used to endorse or promote products derived from this software without
 *   specific prior written permission from Secure Endpoints, Inc. and
 *   Your File System, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _CM_EACCES_H_
#define _CM_EACCES_H_

#include <osi.h>
#include "cm_scache.h"

#define CM_EACCES_MAGIC    ('E' | 'A' <<8 | 'C'<<16 | 'C'<<24)

/*
 * Structure to hold EACCES error info for FID,User pairs
 */

typedef struct cm_eacces {
    struct osi_queue q;        /* fid hash table or free list */
    afs_uint32  magic;
    struct osi_queue parentq;
    struct osi_queue userq;
    cm_fid_t    fid;
    cm_fid_t    parentFid;
    cm_user_t  *userp;
    time_t      errorTime;
} cm_eacces_t;

#define parentq_to_cm_eacces_t(q) ((q) ? (cm_eacces_t *)((char *) (q) - offsetof(cm_eacces_t, parentq)) : NULL)
#define userq_to_cm_eacces_t(q) ((q) ? (cm_eacces_t *)((char *) (q) - offsetof(cm_eacces_t, userq)) : NULL)

#define CM_EACCES_FID_HASH(fidp) (opr_jhash(&(fidp)->cell, 4, 0) & (cm_eaccesFidHashTableSize - 1))

#define CM_EACCES_PARENT_HASH(fidp) (opr_jhash(&(fidp)->cell, 4, 0) & (cm_eaccesParentHashTableSize - 1))

#define CM_EACCES_USER_HASH(userp) (opr_jhash((const uint32_t *)&userp, sizeof(cm_user_t *)/4, 0) & (cm_eaccesUserHashTableSize - 1))

extern void cm_EAccesInitCache(void);

extern cm_eacces_t * cm_EAccesFindEntry(cm_user_t* userp, cm_fid_t *fidp);

extern afs_uint32 cm_EAccesAddEntry(cm_user_t* userp, cm_fid_t *fidp, cm_fid_t *parentFidp);

extern void cm_EAccesClearParentEntries(cm_fid_t *parentFip);

extern void cm_EAccesClearUserEntries(cm_user_t *userp, afs_uint32 CellID);

extern void cm_EAccesClearOutdatedEntries(void);


/*
 * The EACCES cache works by storing EACCES events by the FID and User
 * for which the event occurred, when it occurred and the FID of the parent
 * directory.  By definition, the parent FID of a volume root directory
 * is itself.
 *
 * Entries are removed from the cache under the following circumstances:
 *  1. When the parent FID's callback expires or is replaced.
 *  2. When the parent FID's cm_scache object is recycled.
 *  3. When the user's tokens expire or are replaced.
 *
 * Entries are not removed when the FID's cm_scache object is recycled.
 */
#endif /* _CM_EACCES_H_ */
