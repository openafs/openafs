/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *  Module:	    lockprocs.c
 *  System:	    Volser
 *  Instituition:   ITC, CMU
 *  Date:	    December, 88
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/voldefs.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/vlserver.h>
#include <afs/nfs.h>
#include <afs/afsint.h>

#include "volint.h"
#include "volser.h"
#include "lockdata.h"
#include "vsutils_prototypes.h"
#include "lockprocs_prototypes.h"

/* Finds an index in VLDB entry that matches the volume type, server, and partition.
 * If type is zero, will match first index of ANY type (RW, BK, or RO).
 * If server is zero, will match first index of ANY server and partition
 * Zero is a valid partition field.
 *
 * A genuinely IPv6-only site can be a real match here even though the
 * classic VLDB_SockaddrMatchesIP() comparison below can never succeed
 * for it: that helper only ever matches a real IPv4 address (see the
 * long comment on VLDB_GetUuidByAddr(), vsutils.c). Such a site is
 * registered by uuid instead (VLDB_CreateEntryU()/VLDB_ReplaceEntryU(),
 * see UV_CreateVolume3()/UV_AddSite2() in vsprocs.c) - but the
 * *classic* nvldbentry this function actually receives can't show
 * that directly: vlentry_to_nvldbentry() (src/vlserver/vlprocs.c),
 * which builds every nvldbentry this code ever sees, never sets
 * VLSF_UUID in serverFlags[e] (that flag is wire-only for the U
 * interface, uvldbentry) and picks *some* address to report for
 * serverNumber[e] if the site's underlying Multi-homed Entry has any
 * IPv4 endpoint at all - not necessarily 0, and not necessarily one
 * that matches what the caller supplied. (Verified live against this
 * project's own fleet: a fileserver meant to be "IPv6-only" for
 * testing purposes can still end up with an incidental IPv4-family
 * endpoint from its own multi-homed registration - e.g. a management
 * network alias - which this classic view then reports instead of the
 * fabric address a caller actually asked for, even though the site
 * itself is genuinely uuid-registered.) So there is no reliable
 * classic-side signal ("serverNumber[e] == 0" or otherwise) that a
 * site here *might* be one this format can't precisely represent -
 * the only sound rule is "the classic comparison didn't match, and the
 * caller itself has no IPv4 address of its own to have matched with in
 * the first place".
 *
 * Below, the classic scan runs exactly as before and returns
 * immediately on any real match - no extra cost on the common,
 * all-classic path, and this second pass isn't attempted at all for a
 * NULL server (Lp_GetRwIndex()'s "any server" query - the classic scan
 * alone always settles those) or for a `server` that does have a real
 * IPv4 address (which would have matched classically already, if this
 * really were the same site). Only when the classic scan comes up
 * completely empty AND server is IPv4-less does this pay for two live
 * RPCs: resolve the caller's own uuid (VLDB_GetUuidByAddr()), then
 * re-fetch this same entry through the U interface
 * (VLDB_GetEntryByNameU(), which - unlike this classic view - does
 * preserve every multi-homed site's real uuid), and check every
 * type/partition-matching site's real uuid against it, index for
 * index (both views are built from the same underlying per-site array
 * in the same order, so index e means the same site in both).
 *
 * The one imprecision worth naming: the confirming re-fetch is a
 * separate RPC from whatever earlier call produced `entry`, so it is
 * not perfectly atomic with it - a concurrent VLDB write between the
 * two could in principle shift site ordering. This matches the same
 * class of small, already-accepted gap VLDB_IsSameServer() documents
 * in vsutils.c, not a new one.
 */
static int
FindIndex(struct nvldbentry *entry, const struct rx_sockaddr *server, afs_int32 part, afs_int32 type)
{
    int e;
    afs_int32 error = 0;
    afs_uint32 server_ip;

    for (e = 0; (e < entry->nServers) && !error; e++) {
	if (!type || (entry->serverFlags[e] & type)) {
	    if ((!server || (entry->serverPartition[e] == part))
		&& (!server
		    || VLDB_SockaddrMatchesIP(server, entry->serverNumber[e],
					     &error)))
		return e;	/* direct classic match */

	    if (type == VLSF_RWVOL)
		break;		/* only one RW site - quit scanning either way */
	}
    }

    if (error) {
	fprintf(STDERR,
		"Failed to get info about server's %d address(es) from vlserver (err=%d)\n",
		entry->serverNumber[e], error);
	return -1;
    }

    if (server && !rx_try_sockaddr_to_ipv4(server, &server_ip)) {
	afsUUID server_uuid;
	afs_int32 ucode;

	ucode = VLDB_GetUuidByAddr(server, &server_uuid);
	if (!ucode) {
	    struct uvldbentry uentry;

	    ucode = VLDB_GetEntryByNameU(entry->name, &uentry);
	    if (!ucode) {
		for (e = 0; e < entry->nServers && e < uentry.nServers; e++) {
		    if (type && !(entry->serverFlags[e] & type))
			continue;
		    if (entry->serverPartition[e] != part)
			continue;
		    if ((uentry.serverFlags[e] & VLSF_UUID)
			&& afs_uuid_equal(&uentry.serverNumber[e], &server_uuid))
			return e;
		    if (type == VLSF_RWVOL)
			break;	/* only one RW site */
		}
	    }
	}
	/* ucode, if set, is a genuine RPC error - but this is a
	 * best-effort confirmation on top of an already-failed classic
	 * match, so fall through to "not found" either way rather than
	 * turning a fallback attempt into a hard failure. */
    }

    return -1;			/* Didn't find it */
}

/* Changes the rw site only.
 *
 * nserver == NULL (with npart == 0) means "delete this site" - matching
 * every existing caller's old nserver==0/npart==0 sentinel. A non-NULL
 * nserver that has no IPv4 identity (a genuinely IPv6-only address)
 * cannot be written into entry->serverNumber[e] (see the comment on
 * VLDB_IsSameServer() in vsutils.c, and the longer one in vsprocs.c's
 * UV_CreateVolume3()) - callers are expected to have already rejected
 * that case before reaching here (UV_MoveVolume2() and friends do), so
 * this only has a defensive fallback (leave the site's address alone)
 * rather than a real recovery path. */
static void
SetAValue(struct nvldbentry *entry, const struct rx_sockaddr *oserver, afs_int32 opart,
          const struct rx_sockaddr *nserver, afs_int32 npart, afs_int32 type)
{
    int e;
    afs_uint32 nserver_ip = 0;

    e = FindIndex(entry, oserver, opart, type);
    if (e == -1)
	return;			/* If didn't find it, just return */

    if (nserver && !rx_try_sockaddr_to_ipv4(nserver, &nserver_ip)) {
	fprintf(STDERR,
		"internal error: cannot store an IPv6-only server address "
		"in a VLDB site entry - caller should have checked first\n");
	return;
    }

    entry->serverNumber[e] = nserver_ip;
    entry->serverPartition[e] = npart;

    /* Now move rest of entries up */
    if ((nserver_ip == 0L) && (npart == 0L)) {
	for (e++; e < entry->nServers; e++) {
	    entry->serverNumber[e - 1] = entry->serverNumber[e];
	    entry->serverPartition[e - 1] = entry->serverPartition[e];
	    entry->serverFlags[e - 1] = entry->serverFlags[e];
	}
    }
}

/* Changes the RW site only */
void
Lp_SetRWValue(struct nvldbentry *entry, const struct rx_sockaddr *oserver, afs_int32 opart,
              const struct rx_sockaddr *nserver, afs_int32 npart)
{
    SetAValue(entry, oserver, opart, nserver, npart, VLSF_RWVOL);
}

/* Changes the RO site only */
void
Lp_SetROValue(struct nvldbentry *entry, const struct rx_sockaddr *oserver,
              afs_int32 opart, const struct rx_sockaddr *nserver, afs_int32 npart)
{
    SetAValue(entry, oserver, opart, nserver, npart, VLSF_ROVOL);
}

/* Returns success if this server and partition matches the RW entry */
int
Lp_Match(const struct rx_sockaddr *server, afs_int32 part,
         struct nvldbentry *entry)
{
    if (FindIndex(entry, server, part, VLSF_RWVOL) == -1)
	return 0;
    return 1;
}

/* Return the index of the RO entry (plus 1) if it exists, else return 0 */
int
Lp_ROMatch(const struct rx_sockaddr *server, afs_int32 part, struct nvldbentry *entry)
{
    return (FindIndex(entry, server, part, VLSF_ROVOL) + 1);
}

/* Return the index of the RW entry if it exists, else return -1 */
int
Lp_GetRwIndex(struct nvldbentry *entry)
{
    return (FindIndex(entry, NULL, 0, VLSF_RWVOL));
}

/*initialize queue pointed by <ahead>*/
void
Lp_QInit(struct qHead *ahead)
{
    ahead->count = 0;
    ahead->next = NULL;
}

/*add <elem> in front of queue <ahead> */
void
Lp_QAdd(struct qHead *ahead, struct aqueue *elem)
{
    struct aqueue *temp;

    if (ahead->count == 0) {
	ahead->count += 1;
	ahead->next = elem;
	elem->next = NULL;
    } else {
	temp = ahead->next;
	ahead->count += 1;
	ahead->next = elem;
	elem->next = temp;
    }
}

int
Lp_QScan(struct qHead *ahead, afs_int32 id, int *success, struct aqueue **elem)
{
    struct aqueue *cptr;

    cptr = ahead->next;
    while (cptr != NULL) {
	if (cptr->ids[RWVOL] == id) {
	    *success = 1;
	    *elem = cptr;
	    return 0;
	}
	cptr = cptr->next;
    }
    *success = 0;
    return 0;
}

/*return the element in the beginning of the queue <ahead>, free
*the space used by that element . <success> indicates if enumeration was ok*/
void
Lp_QEnumerate(struct qHead *ahead, int *success, struct aqueue *elem)
{
    int i;
    struct aqueue *temp;

    if (ahead->count > 0) {	/*more elements left */
	ahead->count -= 1;
	temp = ahead->next;
	ahead->next = ahead->next->next;
	strncpy(elem->name, temp->name, VOLSER_OLDMAXVOLNAME);
	for (i = 0; i < 3; i++) {
	    elem->ids[i] = temp->ids[i];
	    elem->copyDate[i] = temp->copyDate[i];
	    elem->isValid[i] = temp->isValid[i];
	}
	elem->next = NULL;
	*success = 1;
	free(temp);
    } else			/*queue is empty */
	*success = 0;
}

void
Lp_QTraverse(struct qHead *ahead)
{
    int count;
    struct aqueue *old, *new;

    old = ahead->next;
    new = old->next;
    count = ahead->count;
    printf
	("traversing the internal queue, which groups all the related volumes on a per partition basis\n");
    while (count > 0) {
	printf("---------------------------\n");
	printf("%s RW-Id %lu", old->name, (unsigned long)old->ids[RWVOL]);
	if (old->isValid[RWVOL])
	    printf(" valid ");
	else
	    printf(" invalid ");
	printf("RO-Id %lu", (unsigned long)old->ids[ROVOL]);
	if (old->isValid[ROVOL])
	    printf(" valid ");
	else
	    printf(" invalid ");
	printf("BACKUP-Id %lu", (unsigned long)old->ids[BACKVOL]);
	if (old->isValid[BACKVOL])
	    printf(" valid ");
	else
	    printf(" invalid ");
	printf("\n");
	printf("---------------------------\n");
	old = new;
	if (count != 1)
	    new = new->next;
	count--;
    }
}
