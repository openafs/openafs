/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * ALL RIGHTS RESERVED
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1998
 * LICENSED MATERIALS - PROPERTY OF IBM
 */


#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <afs/cmd.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#endif
#include <afs/com_err.h>

#include <afs/bubasics.h>
#include "bc.h"

static char db_dsvs = 0;	/*Assume debugging output turned off */
static char mn[] = "dsvs";	/*Module name */

struct ubik_client *cstructp;	/*Ptr to Ubik client structure */

extern struct bc_volumeSet *bc_FindVolumeSet(struct bc_config *cf,
					     char *name);

static FreeVolumeEntryList();
static FreeVolumeEntry();

/* Code to maintain dump schedule and volume set abstractions.
 * A volume set looks like this:
 *	vsname: servername partition-name <volume list>*
 * A dump schedule looks like this:
 *	dsname: vsname period parent-ds
 */

/* get partition id from a name */
afs_int32
bc_GetPartitionID(aname, aval)
     afs_int32 *aval;
     char *aname;
{

    /*bc_GetPartitionID */

    register char tc;
    char ascii[3];

    /* special-case "anything" */
    if (strcmp(aname, ".*") == 0) {
	*aval = -1;
	return 0;
    }
    tc = *aname;
    if (tc == 0)
	return -1;		/* unknown */
    /* numbers go straight through */
    if (tc >= '0' && tc <= '9') {
	*aval = bc_SafeATOI(aname);
	return 0;
    }
    /* otherwise check for vicepa or /vicepa, or just plain "a" */
    ascii[2] = 0;
    if (strlen(aname) <= 2) {
	strcpy(ascii, aname);
    } else if (!strncmp(aname, "/vicep", 6)) {
	strncpy(ascii, aname + 6, 2);
    } else if (!strncmp(aname, "vicep", 5)) {
	strncpy(ascii, aname + 5, 2);
    } else
	return (BC_NOPARTITION);	/* bad partition name */
    /* now partitions are named /vicepa ... /vicepz, /vicepaa, /vicepab, .../vicepzz, and are numbered
     * from 0.  Do the appropriate conversion */
    if (ascii[1] == 0) {
	/* one char name, 0..25 */
	if (ascii[0] < 'a' || ascii[0] > 'z')
	    return -1;		/* wrongo */
	*aval = ascii[0] - 'a';
	return 0;
    } else {
	/* two char name, 26 .. <whatever> */
	if (ascii[0] < 'a' || ascii[0] > 'z')
	    return -1;		/* wrongo */
	if (ascii[1] < 'a' || ascii[1] > 'z')
	    return -1;		/* just as bad */
	*aval = (ascii[0] - 'a') * 26 + (ascii[1] - 'a') + 26;
	return 0;
    }
}				/*bc_GetPartitionID */

/*----------------------------------------------------------------------------
 * bc_ParseHost
 *
 * Description:
 *	Given a string containing a host name or its IP address in dot notation, fill in
 *	the given sockaddr with all the corresponding info.
 *
 * Arguments:
 *	aname : Host name or dotted IP address.
  *	asockaddr: Ptr to sockaddr to fill in for the above host.
 *
 * Returns:
 *	0 if everything went well,
 *	-1 if it couldn't be translated.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	None.
 *----------------------------------------------------------------------------
 */

int
bc_ParseHost(aname, asockaddr)
     char *aname;
     struct sockaddr_in *asockaddr;

{				/*bc_ParseHost */

    register struct hostent *th;	/*Host entry */
    afs_int32 addr;		/*Converted address */
    afs_int32 b1, b2, b3, b4;	/*Byte-sized address chunks */
    register afs_int32 code;	/*Return code from sscanf() */
    afs_int32 tmp1, tmp2;

    /*
     * Try to parse the given name as a dot-notation IP address first.
     */
    code = sscanf(aname, "%d.%d.%d.%d", &b1, &b2, &b3, &b4);
    if (code == 4) {
	/*
	 * Four chunks were read, so we assume success.  Construct the socket.
	 */
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	asockaddr->sin_len = sizeof(struct sockaddr_in);
#endif
	asockaddr->sin_family = AF_INET;
	asockaddr->sin_port = 0;
	addr = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
	memcpy(&tmp1, &addr, sizeof(afs_int32));
	tmp2 = htonl(tmp1);
	memcpy(&asockaddr->sin_addr.s_addr, &tmp2, sizeof(afs_int32));
	return (0);
    }

    /*
     * The given string isn't a dotted IP address.  Try to map it as a host
     * name, or leave it as a wild-card.
     */

    if (strcmp(aname, ".*") == 0) {
	memset(asockaddr, 0, sizeof(struct sockaddr_in));
	return 0;
    }

    th = gethostbyname(aname);
    if (!th)
	/*
	 * No such luck, return failure.
	 */
	return (BC_NOHOST);

    /*
     * We found a mapping; construct the socket.
     */
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    asockaddr->sin_len = sizeof(struct sockaddr_in);
#endif
    asockaddr->sin_family = AF_INET;
    asockaddr->sin_port = 0;
    memcpy(&tmp1, th->h_addr, sizeof(afs_int32));
    tmp2 = htonl(tmp1);
    memcpy(&(asockaddr->sin_addr.s_addr), &tmp2,
	   sizeof(asockaddr->sin_addr.s_addr));
    return (0);

}				/*bc_ParseHost */


/* create an empty volume set, new items are added via bc_AddVolumeItem */
int
bc_CreateVolumeSet(struct bc_config *aconfig, char *avolName,
		   afs_int32 aflags)
{
    register struct bc_volumeSet **tlast, *tset, *nset;

    if (bc_FindVolumeSet(aconfig, avolName))
	return -1;		/* already exists */
    /* move to end of the list */

    nset = (struct bc_volumeSet *)malloc(sizeof(struct bc_volumeSet));
    memset(nset, 0, sizeof(*nset));
    nset->flags = aflags;
    nset->name = (char *)malloc(strlen(avolName) + 1);
    strcpy(nset->name, avolName);
    if (aflags & VSFLAG_TEMPORARY) {
	/* Add to beginning of list */
	nset->next = aconfig->vset;
	aconfig->vset = nset;
    } else {
	/* Add to end of list */
	for (tlast = &aconfig->vset, tset = *tlast; tset;
	     tlast = &tset->next, tset = *tlast);
	*tlast = nset;
    }
    return 0;
}

static int
FreeVolumeEntry(register struct bc_volumeEntry *aentry)
{
    free(aentry->name);
    free(aentry->serverName);
    free(aentry->partname);
    free(aentry);
    return 0;
}

static int
FreeVolumeEntryList(register struct bc_volumeEntry *aentry)
{
    register struct bc_volumeEntry *tnext;

    while (aentry) {
	tnext = aentry->next;
	FreeVolumeEntry(aentry);
	aentry = tnext;
    }
    return 0;
}



void
FreeVolumeSet(struct bc_volumeSet *avset)
{
    FreeVolumeEntryList(avset->ventries);
    free(avset->name);
    free(avset);
}

int
bc_DeleteVolumeSet(struct bc_config *aconfig, char *avolName,
		   afs_int32 *flags)
{
    register struct bc_volumeSet **tlast, *tset;

    *flags = 0;
    tlast = &aconfig->vset;
    for (tset = *tlast; tset; tlast = &tset->next, tset = *tlast) {
	if (strcmp(avolName, tset->name) == 0) {
	    *flags = tset->flags;	/* Remember flags */
	    *tlast = tset->next;	/* Remove from chain */
	    FreeVolumeSet(tset);	/* Free the volume set */
	    return 0;
	}
    }

    /* if we get here, we didn't find the item */
    return -1;
}

int
bc_DeleteVolumeItem(struct bc_config *aconfig, char *avolName,
		    afs_int32 anumber)
{
    register afs_int32 i;
    register struct bc_volumeSet *tset;
    register struct bc_volumeEntry *tentry, **tlast;

    tset = bc_FindVolumeSet(aconfig, avolName);
    if (!tset)
	return -1;

    tlast = &tset->ventries;
    for (i = 1, tentry = *tlast; tentry;
	 tlast = &tentry->next, tentry = *tlast, i++) {
	if (anumber == i) {
	    /* found entry we want */
	    *tlast = tentry->next;
	    FreeVolumeEntry(tentry);
	    return 0;
	}
    }
    return -2;			/* not found */
}

bc_AddVolumeItem(aconfig, avolName, ahost, apart, avol)
     struct bc_config *aconfig;
     char *avolName;
     char *ahost, *apart, *avol;
{
    struct bc_volumeSet *tset;
    register struct bc_volumeEntry **tlast, *tentry;
    register afs_int32 code;

    tset = bc_FindVolumeSet(aconfig, avolName);
    if (!tset)
	return (BC_NOVOLSET);

    /* otherwise append this item to the end of the real list */
    tlast = &tset->ventries;

    /* move to end of the list */
    for (tentry = *tlast; tentry; tlast = &tentry->next, tentry = *tlast);
    tentry = (struct bc_volumeEntry *)malloc(sizeof(struct bc_volumeEntry));
    memset(tentry, 0, sizeof(*tentry));
    tentry->serverName = (char *)malloc(strlen(ahost) + 1);
    strcpy(tentry->serverName, ahost);
    tentry->partname = (char *)malloc(strlen(apart) + 1);
    strcpy(tentry->partname, apart);
    tentry->name = (char *)malloc(strlen(avol) + 1);
    strcpy(tentry->name, avol);

    code = bc_ParseHost(tentry->serverName, &tentry->server);
    if (code)
	return (code);

    code = bc_GetPartitionID(tentry->partname, &tentry->partition);
    if (code)
	return (code);

    *tlast = tentry;		/* thread on the list */
    return 0;
}

struct bc_volumeSet *
bc_FindVolumeSet(struct bc_config *aconfig, char *aname)
{				/*bc_FindVolumeSet */

    register struct bc_volumeSet *tvs;

    for (tvs = aconfig->vset; tvs; tvs = tvs->next) {
	if (!strcmp(tvs->name, aname))
	    return (tvs);
    }
    return (struct bc_volumeSet *)0;

}				/*bc_FindVolumeSet */

/* ------------------------------------
 * dumpschedule management code
 * ------------------------------------
 */

/* bc_CreateDumpSchedule
 *	Add another node to the dump schedule.
 * entry:
 *	aconfig - in core configuration structures
 *	adumpName - name of new dump node
 *	expDate - expiration date
 *	expType - absolute or relative
 */

bc_CreateDumpSchedule(aconfig, adumpName, expDate, expType)
     struct bc_config *aconfig;
     char *adumpName;
     afs_int32 expDate;
     afs_int32 expType;
{
    register struct bc_dumpSchedule *tdump;
    struct bc_dumpSchedule *parent, *node;
    afs_int32 code;

    if (strcmp(adumpName, "none") == 0)
	return -2;		/* invalid name */

    code = FindDump(aconfig, adumpName, &parent, &node);
    if (code == 0)
	return -1;		/* node already exists */
    else if (code != -1)
	return -2;		/* name specification error */

    tdump = (struct bc_dumpSchedule *)malloc(sizeof(struct bc_dumpSchedule));
    memset(tdump, 0, sizeof(*tdump));

    /* prepend this node to the dump schedule list */
    tdump->next = aconfig->dsched;
    aconfig->dsched = tdump;

    /* save the name of this dump node */
    tdump->name = (char *)malloc(strlen(adumpName) + 1);
    strcpy(tdump->name, adumpName);

    /* expiration information */
    tdump->expDate = expDate;
    tdump->expType = expType;

    bc_ProcessDumpSchedule(aconfig);	/* redo tree */
    return 0;
}


/* Recursively remove this node and all of its children from aconfig's
 * list of dumps.  Note that this leaves the sibling pointers damaged (pointing
 * to strange places), so we must call bc_ProcessDumpSchedule when we're done.
 */

bc_DeleteDumpScheduleAddr(aconfig, adumpAddr)
     struct bc_config *aconfig;
     struct bc_dumpSchedule *adumpAddr;
{
    register struct bc_dumpSchedule **tlast, *tdump;
    register struct bc_dumpSchedule *tnext;

    /* knock off all children first */
    for (tdump = adumpAddr->firstChild; tdump; tdump = tnext) {
	/* extract next ptr now, since will be freed by recursive call below */
	tnext = tdump->nextSibling;
	bc_DeleteDumpScheduleAddr(aconfig, tdump);
    }

    /* finally, remove us from the list of good dudes */
    for (tlast = &aconfig->dsched, tdump = *tlast; tdump;
	 tlast = &tdump->next, tdump = *tlast) {
	if (tdump == adumpAddr) {
	    /* found the one we're looking for */
	    *tlast = tdump->next;	/* remove us from basic list */
	    free(tdump->name);
	    free(tdump);
	    return 0;
	}
    }
    return 0;
}

/* bc_FindDumpSchedule
 *	Finds dump schedule aname by doing a linear search
 * entry:
 *	aconfig - handle for incore configuration tables
 *	aname - (path)name to match on
 * exit:
 *	0 for failure, ptr to dumpschedule for success
 */

struct bc_dumpSchedule *
bc_FindDumpSchedule(aconfig, aname)
     char *aname;
     struct bc_config *aconfig;
{
    register struct bc_dumpSchedule *tds;
    for (tds = aconfig->dsched; tds; tds = tds->next) {
	if (!strcmp(tds->name, aname))
	    return tds;
    }
    return (struct bc_dumpSchedule *)0;
}

/* bc_DeleteDumpSchedule
 *	Delete dump node adumpName from the dump schedule
 */

bc_DeleteDumpSchedule(aconfig, adumpName)
     struct bc_config *aconfig;
     char *adumpName;
{
    register struct bc_dumpSchedule *tdump;

    /* does a linear search of the dump schedules in order to find
     * the one to delete
     */
    for (tdump = aconfig->dsched; tdump; tdump = tdump->next) {
	if (strcmp(tdump->name, adumpName) == 0) {
	    /* found it, we can zap recursively */
	    bc_DeleteDumpScheduleAddr(aconfig, tdump);
	    /* tree's been pruned, but we have to recompute the internal pointers
	     * from first principles, since we didn't bother to maintain
	     * the sibling and children pointers during the call to delete
	     * the nodes */
	    bc_ProcessDumpSchedule(aconfig);
	    return 0;
	}
    }
    /* if we make it here, there's no such dump schedule entry */
    return -1;
}


/* bc_ProcessDumpSchedule
 *	Walk over the dump schedule list, building it into a tree.  This
 * algorithm is simple, but takes O(N*2) operations to run, with N=number
 * of dump schedule nodes. It probably will never matter
 */

bc_ProcessDumpSchedule(aconfig)
     register struct bc_config *aconfig;
{
    register struct bc_dumpSchedule *tds;
    struct bc_dumpSchedule *parentptr, *nodeptr;
    int retval;

    /* first, clear all the links on all entries so that this function
     * may be called any number of times with no ill effects
     */
    for (tds = aconfig->dsched; tds; tds = tds->next) {
	tds->parent = (struct bc_dumpSchedule *)0;
	tds->nextSibling = (struct bc_dumpSchedule *)0;
	tds->firstChild = (struct bc_dumpSchedule *)0;
    }

    for (tds = aconfig->dsched; tds; tds = tds->next) {
	retval = FindDump(aconfig, tds->name, &parentptr, &nodeptr);
	if (retval != 0) {
	    printf("bc_processdumpschedule: finddump returns %d\n", retval);
	    exit(1);
	}

	/* only need to do work if it is not a root node */
	if (parentptr != 0) {
	    nodeptr->parent = parentptr;
	    nodeptr->nextSibling = parentptr->firstChild;
	    parentptr->firstChild = nodeptr;
	}
    }
    return 0;
}


/* FindDump
 * entry:
 * exit:
 *	parentptr - set to parent node, if one exists
 *	nodeptr - set to node requested
 *
 *	return values are:
 *	0 - success, parentptr and nodeptr set appropriately
 *	-1 - node not found, parent exists if reqd. Will be 0 for root nodes.
 *	-2 - path search error. Some node on the path does not exist.
 *	-3 - name specification error
 * notes:
 *	pathname checking should be done externally. In particular, trailing
 *	/ symbols may return confusing error codes. (e.g on missing last
 *	node returns -2 rather than -1)
 */

int
FindDump(aconfig, nodeString, parentptr, nodeptr)
     struct bc_config *aconfig;
     char *nodeString;
     struct bc_dumpSchedule **parentptr;
     struct bc_dumpSchedule **nodeptr;
{
    struct bc_dumpSchedule *dsptr;
    char *separator;
    int matchLength;
    char *curptr;

    *parentptr = 0;
    *nodeptr = 0;

    /* ensure first char is correct separator */
    if ((nodeString[0] != '/')
	|| (strlen(&nodeString[0]) <= 1)
	) {
	printf("FindDump: %s, error in dump name specification\n",
	       nodeString);
	return (-3);
    }

    matchLength = 0;
    curptr = &nodeString[1];	/* past first / */
    separator = strchr(curptr, '/');
    if (separator == 0)
	matchLength = strlen(curptr) + 1;	/* +1 for leading / */
    else
	matchLength = (separator - &nodeString[0]);

    /* printf("matchLength = %d\n", matchLength); */
    while (1) {
	/* now search all the nodes for this name */
	for (dsptr = aconfig->dsched; dsptr != 0; dsptr = dsptr->next) {
	    /* printf("compare %s with %s for %d\n",
	     * dsptr->name, nodeString, matchLength); */
	    if (strlen(dsptr->name) != matchLength)
		continue;

	    if (strncmp(dsptr->name, nodeString, matchLength) == 0) {
		*nodeptr = dsptr;
		break;
	    }
	}

	if (nodeString[matchLength] == 0) {
	    /* last node in the path */
	    if (*nodeptr)
		return (0);	/* all ok */
	    else
		/* node not found; parent exists for non root nodes */
		return (-1);
	}

	if (*nodeptr == 0)
	    /* failed to find a node in the path */
	    return (-2);

	curptr = separator + 1;
	if (*curptr == 0) {
	    printf("FindDump: trailing / in %s\n", nodeString);
	    return (-3);
	}

	separator = strchr(curptr, '/');
	if (separator == 0)
	    matchLength = strlen(&nodeString[0]);
	else
	    matchLength = separator - &nodeString[0];

	*parentptr = *nodeptr;
	*nodeptr = 0;
    }
}
