/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "lockprocs.h"
#include <string.h>

/* Finds an index in VLDB entry that matches the volume type, server, and partition.
 * If type is zero, will match first index of ANY type (RW, BK, or RO).
 * If server is zero, will match first index of ANY server and partition 
 * Zero is a valid partition field.
 */
static int
FindIndex(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
	  afs_int32 server, afs_int32 part, afs_int32 type)
{
    afs_status_t tst = 0;
    int e;
    int equal = 0;

    for (e = 0; (e < entry->nServers) && !tst; e++) {
	if (!type || (entry->serverFlags[e] & type)) {
	    VLDB_IsSameAddrs(cellHandle, entry->serverNumber[e], server,
			     &equal, &tst);
	    if ((!server || (entry->serverPartition[e] == part))
		&& (!server || equal)) {
		break;
	    }
	    if (type == ITSRWVOL) {
		/* quit when we are looking for RW entry (there's only 1) */
		return -1;
	    }
	}
    }

    if (e >= entry->nServers) {
	return -1;		/* Didn't find it */
    }

    return e;			/* return the index */
}

/* Changes the rw site only */
static void
SetAValue(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
	  afs_int32 oserver, afs_int32 opart, afs_int32 nserver,
	  afs_int32 npart, afs_int32 type)
{
    int e;

    e = FindIndex(cellHandle, entry, oserver, opart, type);
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
void
Lp_SetRWValue(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
	      afs_int32 oserver, afs_int32 opart, afs_int32 nserver,
	      afs_int32 npart)
{
    SetAValue(cellHandle, entry, oserver, opart, nserver, npart, ITSRWVOL);
}

/* Changes the RO site only */
void
Lp_SetROValue(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
	      afs_int32 oserver, afs_int32 opart, afs_int32 nserver,
	      afs_int32 npart)
{
    SetAValue(cellHandle, entry, oserver, opart, nserver, npart, ITSROVOL);
}

/* Returns success if this server and partition matches the RW entry */
int
Lp_Match(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
	 afs_int32 server, afs_int32 part, afs_status_p st)
{
    if (FindIndex(cellHandle, entry, server, part, ITSRWVOL) == -1)
	return 0;
    return 1;
}

/* Return the index of the RO entry (plus 1) if it exists, else return 0 */
int
Lp_ROMatch(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
	   afs_int32 server, afs_int32 part, afs_status_p st)
{
    return (FindIndex(cellHandle, entry, server, part, ITSROVOL) + 1);
}

/* Return the index of the RW entry if it exists, else return -1 */
int
Lp_GetRwIndex(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
	      afs_status_p st)
{
    return (FindIndex(cellHandle, entry, 0, 0, ITSRWVOL));
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
Lp_QScan(struct qHead *ahead, afs_int32 id, int *success,
	 struct aqueue **elem, afs_status_p st)
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
Lp_QEnumerate(struct qHead *ahead, int *success, struct aqueue *elem,
	      afs_status_p st)
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
