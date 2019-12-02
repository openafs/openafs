/*
 * Copyright (c) 2014 Your File System, Inc.
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

#include <afs/stds.h>
#include "afsd.h"
#include "cm_getaddrs.h"

typedef struct _uuid2addrsEntry {
    osi_queue_t q;
    afsUUID     uuid;
    afs_int32   unique;
    afs_uint32  nentries;
    bulkaddrs   addrs;
    afs_int32   refCount;
    afs_uint32  flags;
} uuid2addrsEntry_t;

#define CM_GETADDRS_FLAG_DELETE 	1

/* lock for hash table */
osi_rwlock_t cm_getaddrsLock;

/* hash table */
#define CM_GETADDRS_HASHTABLESIZE 	128
static afs_uint32 	cm_getaddrsHashTableSize = CM_GETADDRS_HASHTABLESIZE;

#define CM_GETADDRS_HASH(uuidp) \
  (opr_jhash((const afs_uint32 *)uuidp, 4, 0) & (cm_getaddrsHashTableSize - 1))

static struct _uuid2addrsHashTableBucket {
    uuid2addrsEntry_t * Firstp;
    uuid2addrsEntry_t * Endp;
} cm_getaddrsHashTable[CM_GETADDRS_HASHTABLESIZE];

/*
 * Return a cached entry that matches the requested
 * uuid if the requested unique is less than or equal
 * to the cached entry.  Otherwise, return NULL.
 *
 * Returned entries are returned with a reference.
 */
static uuid2addrsEntry_t *
cm_getaddrsFind(afsUUID *uuid, afs_int32 srv_unique)
{
    uuid2addrsEntry_t * entry = NULL;
    afs_uint32 hash = CM_GETADDRS_HASH(uuid);

    lock_ObtainRead(&cm_getaddrsLock);

    for ( entry = cm_getaddrsHashTable[hash].Firstp;
	  entry;
	  entry = (uuid2addrsEntry_t *)osi_QNext(&entry->q))
    {
	if (memcmp(uuid, &entry->uuid, sizeof(afsUUID)) == 0 &&
	    srv_unique <= entry->unique &&
	    !(entry->flags & CM_GETADDRS_FLAG_DELETE))
	    break;
    }

    if (entry)
	entry->refCount++;
    lock_ReleaseRead(&cm_getaddrsLock);

    return entry;
}

static void
cm_getaddrsPut(uuid2addrsEntry_t *entry)
{
    lock_ObtainRead(&cm_getaddrsLock);
    entry->refCount--;
    lock_ReleaseRead(&cm_getaddrsLock);
}

/*
 * Add a bulkaddrs list to the getaddrs queue.
 * Once this function is called the bulkaddrs structure
 * no longer belongs to the caller and must not be
 * accessed.  It may be freed if there is a race with
 * another thread.
 */

static uuid2addrsEntry_t *
cm_getaddrsAdd(afsUUID *uuid, afs_int32 srv_unique,
	       afs_uint32 nentries, bulkaddrs *addrsp)
{
    uuid2addrsEntry_t * entry, *next, *retentry = NULL;
    afs_uint32 hash = CM_GETADDRS_HASH(uuid);
    int insert = 1;

    lock_ObtainWrite(&cm_getaddrsLock);
    for ( entry = cm_getaddrsHashTable[hash].Firstp;
	  entry;
	  entry = next)
    {
	next = (uuid2addrsEntry_t *)osi_QNext(&entry->q);

	if ((entry->flags & CM_GETADDRS_FLAG_DELETE) &&
	    entry->refCount == 0)
	{
	    osi_QRemoveHT((osi_queue_t **) &cm_getaddrsHashTable[hash].Firstp,
			  (osi_queue_t **) &cm_getaddrsHashTable[hash].Endp,
			  &entry->q);
	    xdr_free((xdrproc_t) xdr_bulkaddrs, &entry->addrs);
	    continue;
	}

	if (memcmp(uuid, &entry->uuid, sizeof(afsUUID)) == 0)
	{

	    if (srv_unique <= entry->unique) {
		/*
		 * out of date or duplicate entry.
		 * discard the input.
		 */
		xdr_free((xdrproc_t) xdr_bulkaddrs, addrsp);
		insert = 0;
		retentry = entry;
		retentry->refCount++;
		continue;
	    }

	    /*
	     * this entry is newer than the one we found,
	     * if it is unused then update it
	     */
	    if (entry->refCount == 0) {
		xdr_free((xdrproc_t) xdr_bulkaddrs, &entry->addrs);

		entry->unique = srv_unique;
		entry->addrs = *addrsp;
		entry->nentries = nentries;
		insert = 0;
		retentry = entry;
		retentry->refCount++;
		continue;
	    }
	}
    }

    if (insert) {
	entry = calloc(1, sizeof(uuid2addrsEntry_t));
	if (entry) {
	    memcpy(&entry->uuid, uuid, sizeof(afsUUID));
	    entry->unique = srv_unique;
	    entry->addrs = *addrsp;
	    entry->nentries = nentries;

	    osi_QAddH((osi_queue_t **) &cm_getaddrsHashTable[hash].Firstp,
		       (osi_queue_t **) &cm_getaddrsHashTable[hash].Endp,
		       &entry->q);
	    retentry = entry;
	    retentry->refCount++;
	}
    }
    lock_ReleaseWrite(&cm_getaddrsLock);

    return retentry;
}

/*
 * cm_GetAddrsU takes as input a uuid and a unique value which
 * represent the set of addresses that are required.  These values
 * are used as input to the VL_GetAddrsU RPC which returns a list
 * of addresses.  For each returned address a bucket in the provided
 * arrays (serverFlags, serverNumber, serverUUID, serverUnique)
 * are populated.  The serverFlags array entries are filled with the
 * 'Flags' value provided as input.  'serverNumber' is the server's
 * IP address.
 */

afs_uint32
cm_GetAddrsU(cm_cell_t *cellp, cm_user_t *userp, cm_req_t *reqp,
	     afsUUID *Uuid, afs_int32 Unique, afs_int32 Flags,
	     int *index,
	     afs_int32 serverFlags[],
	     afs_int32 serverNumber[],
	     afsUUID serverUUID[],
	     afs_int32 serverUnique[])
{
    afs_uint32 code = 0;
    uuid2addrsEntry_t *entry;

    entry = cm_getaddrsFind(Uuid, Unique);
    if (entry == NULL)
    {
	cm_conn_t *connp;
	struct rx_connection *rxconnp;
	ListAddrByAttributes attrs;
	afs_int32 unique;
	afsUUID uuid;
	afs_uint32 nentries;
	bulkaddrs  addrs;

	memset(&uuid, 0, sizeof(uuid));
	memset(&addrs, 0, sizeof(addrs));
	memset(&attrs, 0, sizeof(attrs));

	attrs.Mask = VLADDR_UUID;
	attrs.uuid = *Uuid;

	do {
	    code = cm_ConnByMServers(cellp->vlServersp, 0, userp, reqp, &connp);
	    if (code)
		continue;
	    rxconnp = cm_GetRxConn(connp);
	    code = VL_GetAddrsU(rxconnp, &attrs, &uuid, &unique, &nentries,
				 &addrs);
	    rx_PutConnection(rxconnp);
	} while (cm_Analyze(connp, userp, reqp, NULL, cellp, 0, NULL, NULL,
			     &cellp->vlServersp, NULL, code));

	code = cm_MapVLRPCError(code, reqp);

	if (afsd_logp->enabled) {
	    struct uuid_fmtbuf uuidstr;
	    afsUUID_to_string(Uuid, &uuidstr);

	    if (code)
		osi_Log2(afsd_logp,
			  "CALL VL_GetAddrsU serverNumber %s FAILURE, code 0x%x",
			  osi_LogSaveString(afsd_logp, uuidstr.buffer),
			  code);
	    else
		osi_Log1(afsd_logp, "CALL VL_GetAddrsU serverNumber %s SUCCESS",
			  osi_LogSaveString(afsd_logp, uuidstr.buffer));
	}

	if (code)
	    return CM_ERROR_RETRY;

	if (nentries == 0) {
	    xdr_free((xdrproc_t) xdr_bulkaddrs, &addrs);
	    code = CM_ERROR_INVAL;
	} else {
	    /* addrs will either be cached or freed by cm_getaddrsAdd() */
	    entry = cm_getaddrsAdd(&uuid, unique, nentries, &addrs);
	}
    }

    if (entry != NULL) {
	afs_uint32 * addrp;
	afs_uint32 n;

	addrp = entry->addrs.bulkaddrs_val;
	for (n = 0; n < entry->nentries && (*index) < NMAXNSERVERS;
	     (*index)++, n++) {
	    serverFlags[*index] = Flags;
	    serverNumber[*index] = addrp[n];
	    serverUUID[*index] = entry->uuid;
	    serverUnique[*index] = entry->unique;
	}
	cm_getaddrsPut(entry);
    }

    return code;
}

void
cm_getaddrsInit(void)
{
    static osi_once_t once;

    if (osi_Once(&once)) {
	lock_InitializeRWLock(&cm_getaddrsLock, "cm_getaddrsLock",
			      LOCK_HIERARCHY_GETADDRS_GLOBAL);
	osi_EndOnce(&once);
    }
}

void
cm_getaddrsShutdown(void)
{
    lock_FinalizeRWLock(&cm_getaddrsLock);
}
