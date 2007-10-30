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

RCSID
    ("$Header$");

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <string.h>
#include <afs/voldefs.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/vlserver.h>
#include <afs/nfs.h>
#include <afs/afsint.h>
#include "volint.h"
#include "volser.h"
#include "lockdata.h"

/* Finds an index in VLDB entry that matches the volume type, server, and partition.
 * If type is zero, will match first index of ANY type (RW, BK, or RO).
 * If server is zero, will match first index of ANY server and partition 
 * Zero is a valid partition field.
 */
int
FindIndex(entry, server, part, type)
     struct nvldbentry *entry;
     afs_int32 server, part, type;
{
    int e;
    afs_int32 error = 0;

    for (e = 0; (e < entry->nServers) && !error; e++) {
	if (!type || (entry->serverFlags[e] & type)) {
	    if ((!server || (entry->serverPartition[e] == part))
		&& (!server
		    || VLDB_IsSameAddrs(entry->serverNumber[e], server,
					&error)))
		break;
	    if (type == ITSRWVOL)
		return -1;	/* quit when we are looking for RW entry (there's only 1) */
	}
    }

    if (error) {
	fprintf(STDERR,
		"Failed to get info about server's %d address(es) from vlserver (err=%d)\n",
		entry->serverNumber[e], error);
	return -1;
    }

    if (e >= entry->nServers)
	return -1;		/* Didn't find it */

    return e;			/* return the index */
}

/* Changes the rw site only */
void
SetAValue(entry, oserver, opart, nserver, npart, type)
     struct nvldbentry *entry;
     afs_int32 oserver, opart, nserver, npart, type;
{
    int e;
    afs_int32 error = 0;

    e = FindIndex(entry, oserver, opart, type);
    if (e == -1)
	return;			/* If didn't find it, just return */

    entry->serverNumber[e] = nserver;
    entry->serverPartition[e] = npart;

    /* Now move rest of entries up */
    if ((nserver == 0L) && (npart == 0L)) {
	for (e++; e < entry->nServers; e++) {
	    entry->serverNumber[e - 1] = entry->serverNumber[e];
	    entry->serverPartition[e - 1] = entry->serverPartition[e];
	    entry->serverFlags[e - 1] = entry->serverFlags[e];
	}
    }
}

/* Changes the RW site only */
Lp_SetRWValue(entry, oserver, opart, nserver, npart)
     struct nvldbentry *entry;
     afs_int32 oserver, opart, nserver, npart;
{
    SetAValue(entry, oserver, opart, nserver, npart, ITSRWVOL);
}

/* Changes the RO site only */
Lp_SetROValue(entry, oserver, opart, nserver, npart)
     struct nvldbentry *entry;
     afs_int32 oserver, opart, nserver, npart;
{
    SetAValue(entry, oserver, opart, nserver, npart, ITSROVOL);
}

/* Returns success if this server and partition matches the RW entry */
Lp_Match(server, part, entry)
     afs_int32 server, part;
     struct nvldbentry *entry;
{
    if (FindIndex(entry, server, part, ITSRWVOL) == -1)
	return 0;
    return 1;
}

/* Return the index of the RO entry (plus 1) if it exists, else return 0 */
Lp_ROMatch(server, part, entry)
     afs_int32 server, part;
     struct nvldbentry *entry;
{
    return (FindIndex(entry, server, part, ITSROVOL) + 1);
}

/* Return the index of the RW entry if it exists, else return -1 */
Lp_GetRwIndex(entry)
     struct nvldbentry *entry;
{
    return (FindIndex(entry, 0, 0, ITSRWVOL));
}

/*initialize queue pointed by <ahead>*/
Lp_QInit(ahead)
     struct qHead *ahead;
{
    ahead->count = 0;
    ahead->next = NULL;
}

/*add <elem> in front of queue <ahead> */
Lp_QAdd(ahead, elem)
     struct qHead *ahead;
     struct aqueue *elem;
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

Lp_QScan(ahead, id, success, elem)
     struct qHead *ahead;
     struct aqueue **elem;
     afs_int32 id;
     int *success;
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
Lp_QEnumerate(ahead, success, elem)
     struct qHead *ahead;
     struct aqueue *elem;
     int *success;
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

Lp_QTraverse(ahead)
     struct qHead *ahead;
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
