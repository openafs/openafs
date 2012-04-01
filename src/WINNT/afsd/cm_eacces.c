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

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

#include "afsd.h"

static osi_rwlock_t cm_eaccesLock;

static struct osi_queue ** cm_eaccesFidHashTableH = NULL;
static struct osi_queue ** cm_eaccesParentHashTableH = NULL;
static struct osi_queue ** cm_eaccesUserHashTableH = NULL;

static struct osi_queue ** cm_eaccesFidHashTableT = NULL;
static struct osi_queue ** cm_eaccesParentHashTableT = NULL;
static struct osi_queue ** cm_eaccesUserHashTableT = NULL;

static afs_uint32 cm_eaccesFidHashTableSize = 0;
static afs_uint32 cm_eaccesParentHashTableSize = 0;
static afs_uint32 cm_eaccesUserHashTableSize = 0;

static struct osi_queue * cm_eaccesFreeListH = NULL;

void
cm_EAccesInitCache(void)
{
    static osi_once_t once;

    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_eaccesLock, "cm_eaccesLock", LOCK_HIERARCHY_EACCES_GLOBAL);
        osi_EndOnce(&once);
    }

    lock_ObtainWrite(&cm_eaccesLock);
    cm_eaccesFidHashTableSize = cm_data.stats * 2;
    cm_eaccesParentHashTableSize = cm_data.stats * 2;
    cm_eaccesUserHashTableSize = 32;

    cm_eaccesFidHashTableH = malloc(cm_eaccesFidHashTableSize * sizeof(struct osi_queue *));
    memset(cm_eaccesFidHashTableH, 0, cm_eaccesFidHashTableSize * sizeof(struct osi_queue *));
    cm_eaccesFidHashTableT = malloc(cm_eaccesFidHashTableSize * sizeof(struct osi_queue *));
    memset(cm_eaccesFidHashTableT, 0, cm_eaccesFidHashTableSize * sizeof(struct osi_queue *));

    cm_eaccesParentHashTableH = malloc(cm_eaccesParentHashTableSize * sizeof(struct osi_queue *));
    memset(cm_eaccesParentHashTableH, 0, cm_eaccesParentHashTableSize * sizeof(struct osi_queue *));
    cm_eaccesParentHashTableT = malloc(cm_eaccesParentHashTableSize * sizeof(struct osi_queue *));
    memset(cm_eaccesParentHashTableT, 0, cm_eaccesParentHashTableSize * sizeof(struct osi_queue *));

    cm_eaccesUserHashTableH = malloc(cm_eaccesUserHashTableSize * sizeof(struct osi_queue *));
    memset(cm_eaccesUserHashTableH, 0, cm_eaccesUserHashTableSize * sizeof(struct osi_queue *));
    cm_eaccesUserHashTableT = malloc(cm_eaccesUserHashTableSize * sizeof(struct osi_queue *));
    memset(cm_eaccesUserHashTableT, 0, cm_eaccesUserHashTableSize * sizeof(struct osi_queue *));

    lock_ReleaseWrite(&cm_eaccesLock);
}

afs_uint32
cm_EAccesAddEntry(cm_user_t* userp, cm_fid_t *fidp, cm_fid_t *parentFidp)
{
    cm_eacces_t *eaccesp = NULL;
    afs_uint32   hash;
    int          newEntry = 0;

    hash = CM_EACCES_FID_HASH(fidp);

    lock_ObtainWrite(&cm_eaccesLock);
    for (eaccesp = (cm_eacces_t *)cm_eaccesFidHashTableH[hash];
         eaccesp;
         eaccesp = (cm_eacces_t *)osi_QNext(&eaccesp->q))
    {
        if (eaccesp->userp == userp &&
            !cm_FidCmp(&eaccesp->fid, fidp))
            break;
    }

    if (eaccesp == NULL) {
        if (osi_QIsEmpty(&cm_eaccesFreeListH)) {
            eaccesp = malloc(sizeof(cm_eacces_t));
        } else {
            eaccesp = (cm_eacces_t *)cm_eaccesFreeListH;
            osi_QRemove(&cm_eaccesFreeListH, &eaccesp->q);
        }

        memset(eaccesp, 0, sizeof(cm_eacces_t));
        eaccesp->magic = CM_EACCES_MAGIC;
        eaccesp->fid = *fidp;
        eaccesp->userp = userp;
        cm_HoldUser(userp);

        osi_QAddH( &cm_eaccesFidHashTableH[hash],
                   &cm_eaccesFidHashTableT[hash],
                   &eaccesp->q);

        hash = CM_EACCES_USER_HASH(userp);
        osi_QAddH( &cm_eaccesUserHashTableH[hash],
                   &cm_eaccesUserHashTableT[hash],
                   &eaccesp->userq);

        newEntry = 1;
    }

    if (eaccesp) {
        eaccesp->errorTime = time(NULL);

        if (!newEntry &&
            !cm_FidCmp(parentFidp, &eaccesp->parentFid))
        {
            hash = CM_EACCES_PARENT_HASH(&eaccesp->parentFid);
            osi_QRemoveHT( &cm_eaccesParentHashTableH[hash],
                           &cm_eaccesParentHashTableT[hash],
                           &eaccesp->parentq);
        }

        eaccesp->parentFid = *parentFidp;
        hash = CM_EACCES_PARENT_HASH(&eaccesp->parentFid);
        osi_QAddH( &cm_eaccesParentHashTableH[hash],
                  &cm_eaccesParentHashTableT[hash],
                  &eaccesp->parentq);
    }
    lock_ReleaseWrite(&cm_eaccesLock);

    return 0;
}

cm_eacces_t *
cm_EAccesFindEntry(cm_user_t* userp, cm_fid_t *fidp)
{
    cm_eacces_t *eaccesp = NULL;
    afs_uint32   hash;

    hash = CM_EACCES_FID_HASH(fidp);

    lock_ObtainRead(&cm_eaccesLock);
    for (eaccesp = (cm_eacces_t *)cm_eaccesFidHashTableH[hash];
         eaccesp;
         eaccesp = (cm_eacces_t *)osi_QNext(&eaccesp->q))
    {
        if (eaccesp->userp == userp &&
            !cm_FidCmp(&eaccesp->fid, fidp))
            break;
    }
    lock_ReleaseRead(&cm_eaccesLock);

    return eaccesp;
}

void
cm_EAccesClearParentEntries(cm_fid_t *parentFidp)
{
    cm_eacces_t *eaccesp = NULL;
    cm_eacces_t *nextp = NULL;
    afs_uint32   hash, hash2;

    hash = CM_EACCES_PARENT_HASH(parentFidp);

    lock_ObtainRead(&cm_eaccesLock);
    for (eaccesp = parentq_to_cm_eacces_t(cm_eaccesParentHashTableH[hash]);
         eaccesp;
         eaccesp = nextp)
    {
        nextp = parentq_to_cm_eacces_t(osi_QNext(&eaccesp->parentq));

        if (!cm_FidCmp(&eaccesp->parentFid, parentFidp))
        {
            osi_QRemoveHT( &cm_eaccesParentHashTableH[hash],
                           &cm_eaccesParentHashTableT[hash],
                           &eaccesp->parentq);

            hash2 = CM_EACCES_FID_HASH(&eaccesp->fid);
            osi_QRemoveHT( &cm_eaccesFidHashTableH[hash2],
                           &cm_eaccesFidHashTableT[hash2],
                           &eaccesp->q);

            hash2 = CM_EACCES_USER_HASH(eaccesp->userp);
            osi_QRemoveHT( &cm_eaccesUserHashTableH[hash2],
                           &cm_eaccesUserHashTableT[hash2],
                           &eaccesp->userq);

            cm_ReleaseUser(eaccesp->userp);
            osi_QAdd( &cm_eaccesFreeListH, &eaccesp->q);
        }
    }
    lock_ReleaseRead(&cm_eaccesLock);
}

void
cm_EAccesClearUserEntries(cm_user_t *userp, afs_uint32 cellID)
{
    cm_eacces_t *eaccesp = NULL;
    cm_eacces_t *nextp = NULL;
    afs_uint32   hash, hash2;

    hash = CM_EACCES_USER_HASH(userp);

    lock_ObtainRead(&cm_eaccesLock);
    for (eaccesp = userq_to_cm_eacces_t(cm_eaccesUserHashTableH[hash]);
         eaccesp;
         eaccesp = nextp)
    {
        nextp = userq_to_cm_eacces_t(osi_QNext(&eaccesp->userq));

        if (eaccesp->userp == userp &&
            (cellID == 0 || eaccesp->fid.cell == cellID))
        {
            cm_ReleaseUser(userp);
            osi_QRemoveHT( &cm_eaccesUserHashTableH[hash],
                           &cm_eaccesUserHashTableT[hash],
                           &eaccesp->userq);

            hash2 = CM_EACCES_FID_HASH(&eaccesp->fid);
            osi_QRemoveHT( &cm_eaccesFidHashTableH[hash2],
                           &cm_eaccesFidHashTableT[hash2],
                           &eaccesp->q);

            hash2 = CM_EACCES_PARENT_HASH(&eaccesp->parentFid);
            osi_QRemoveHT( &cm_eaccesParentHashTableH[hash2],
                           &cm_eaccesParentHashTableT[hash2],
                           &eaccesp->parentq);

            osi_QAdd( &cm_eaccesFreeListH, &eaccesp->q);
        }
    }
    lock_ReleaseRead(&cm_eaccesLock);
}

void
cm_EAccesClearOutdatedEntries(void)
{
    cm_eacces_t *eaccesp = NULL;
    cm_eacces_t *nextp = NULL;
    afs_uint32   hash, hash2;
    time_t       now = time(NULL);

    lock_ObtainRead(&cm_eaccesLock);
    for (hash = 0; hash < cm_eaccesFidHashTableSize; hash++)
    {
        for (eaccesp = (cm_eacces_t *)(cm_eaccesFidHashTableH[hash]);
              eaccesp;
              eaccesp = nextp)
        {
            nextp = (cm_eacces_t *)(osi_QNext(&eaccesp->q));

            if (eaccesp->errorTime + 4*60*60 < now)
            {
                osi_QRemoveHT( &cm_eaccesFidHashTableH[hash],
                               &cm_eaccesFidHashTableT[hash],
                               &eaccesp->q);

                hash2 = CM_EACCES_USER_HASH(eaccesp->userp);
                osi_QRemoveHT( &cm_eaccesUserHashTableH[hash2],
                               &cm_eaccesUserHashTableT[hash2],
                               &eaccesp->userq);
                cm_ReleaseUser(eaccesp->userp);

                hash2 = CM_EACCES_PARENT_HASH(&eaccesp->parentFid);
                osi_QRemoveHT( &cm_eaccesParentHashTableH[hash2],
                               &cm_eaccesParentHashTableT[hash2],
                               &eaccesp->parentq);

                osi_QAdd( &cm_eaccesFreeListH, &eaccesp->q);
            }
        }
    }
    lock_ReleaseRead(&cm_eaccesLock);
}
