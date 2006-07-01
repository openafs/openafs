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

#ifndef AFS_NT40_ENV
#include <sys/time.h>
#endif
#include <rx/xdr.h>
#include <rx/rx.h>
#include <lock.h>
#include <lwp.h>
#include <errno.h>
#include <afs/tcdata.h>

#include "error_macros.h"

/*extern int debugLevel;*/
static struct dumpNode *dumpQHeader;	/* ptr to head of the dumpNode list */
static struct dumpNode headNode;	/* the dummy header of the node list */
static afs_int32 maxTaskID;	/* the largest task Id allotted so far, this is never reused */

/* allocTaskId
 *	allocate a dump (task) id
 */
afs_int32
allocTaskId()
{
    return (maxTaskID++);
}


/* initialize the node list used to keep track of the active dumps */
void
InitNodeList(afs_int32 portOffset)
{
    maxTaskID = (portOffset * 1000) + 1;	/* this is the first task id alotted */
    headNode.taskID = -1;
    headNode.next = NULL;
    headNode.dumps = (struct tc_dumpDesc *)0;
    headNode.restores = (struct tc_restoreDesc *)0;
    dumpQHeader = &headNode;	/* noone in the list to start with */
}

/* CreateNode
 *	Create a <newNode> for the dump, put it in the list of active dumps
 *	and return a pointer to it
 * entry:
 *	newNode - for return ptr
 * exit:
 *	newNode ptr set to point to a node.
 */

void
CreateNode(struct dumpNode **newNode)
{
    /* get space */
    *newNode = (struct dumpNode *)(malloc(sizeof(struct dumpNode)));

    memset(*newNode, 0, sizeof(struct dumpNode));

    (*newNode)->next = dumpQHeader->next;
    dumpQHeader->next = *newNode;
    (*newNode)->taskID = allocTaskId();

/*  if (debugLevel) DisplayNode(*newNode); */
}

/* free the space allotted to the node with <taskID> */
void
FreeNode(afs_int32 taskID)
{
    struct dumpNode *oldPtr, *newPtr, *curPtr;
    int done;

    curPtr = dumpQHeader;
    oldPtr = dumpQHeader;
    if (curPtr)
	newPtr = dumpQHeader->next;
    else
	newPtr = NULL;
    done = 0;
    while ((!done) && (curPtr != NULL)) {
	if (curPtr->taskID == taskID) {
	    done = 1;
	    oldPtr->next = newPtr;

	    /* free the node and its structures */
	    if (curPtr->dumpName)
		free(curPtr->dumpName);
	    if (curPtr->volumeSetName)
		free(curPtr->volumeSetName);
	    if (curPtr->restores)
		free(curPtr->restores);
	    if (curPtr->dumps)
		free(curPtr->dumps);
	    free(curPtr);
	} else {
	    oldPtr = curPtr;
	    curPtr = newPtr;
	    if (newPtr)
		newPtr = newPtr->next;

	}
    }
    return;

}

afs_int32
GetNthNode(afs_int32 aindex, afs_int32 *aresult)
{
    register struct dumpNode *tn;
    register int i;

    tn = dumpQHeader->next;
    for (i = 0;; i++) {
	if (!tn)
	    return ENOENT;
	/* see if this is the desired node ID */
	if (i == aindex) {
	    *aresult = tn->taskID;
	    return 0;
	}
	/* otherwise, skip to next one and keep looking */
	tn = tn->next;
    }
}

/* return the node with <taskID> into <resultNode> */
afs_int32
GetNode(afs_int32 taskID, struct dumpNode **resultNode)
{
    struct dumpNode *tmpPtr;
    int done;

    done = 0;
    tmpPtr = dumpQHeader;
    while ((!done) && (tmpPtr != NULL)) {
	if (tmpPtr->taskID == taskID) {
	    *resultNode = tmpPtr;
	    done = 1;
	} else
	    tmpPtr = tmpPtr->next;
    }
    if (done)
	return 0;
    else
	return TC_NODENOTFOUND;
}
