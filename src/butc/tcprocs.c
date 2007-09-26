/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* procedures invoked by the rpc stub */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/afsint.h>
#include <stdio.h>
#include <afs/procmgmt.h>
#include <afs/assert.h>
#include <afs/prs_fs.h>
#include <sys/stat.h>
#include <afs/nfs.h>
#include <lwp.h>
#include <lock.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/tcdata.h>
#include "error_macros.h"
#include "butc_xbsa.h"

int
callPermitted(struct rx_call *call)
{
    /* before this code can be used, the rx connection, on the bucoord side, must */
    /* be changed so that it will set up for token passing instead of using  a    */
    /* simple rx connection that, below, returns a value of 0 from rx_SecurityClassOf */
    return 1;
}

/* -----------------------------
 * misc. routines
 * -----------------------------
 */

static int
CopyDumpDesc(struct tc_dumpDesc *toDump, tc_dumpArray *fromDump)
{
    struct tc_dumpDesc *toPtr, *fromPtr;
    int i;

    toPtr = toDump;
    fromPtr = fromDump->tc_dumpArray_val;
    for (i = 0; i < fromDump->tc_dumpArray_len; i++) {
	toPtr->vid = fromPtr->vid;
	toPtr->vtype = fromPtr->vtype;
	toPtr->partition = fromPtr->partition;
	toPtr->date = fromPtr->date;
	toPtr->cloneDate = fromPtr->cloneDate;
	toPtr->hostAddr = fromPtr->hostAddr;
	strcpy(toPtr->name, fromPtr->name);
	fromPtr++;
	toPtr++;
    }
    return 0;
}


static int
CopyRestoreDesc(struct tc_restoreDesc *toRestore, tc_restoreArray *fromRestore)
{
    struct tc_restoreDesc *toPtr, *fromPtr;
    int i;

    toPtr = toRestore;
    fromPtr = fromRestore->tc_restoreArray_val;
    for (i = 0; i < fromRestore->tc_restoreArray_len; i++) {
	toPtr->flags = fromPtr->flags;
	toPtr->position = fromPtr->position;
	strcpy(toPtr->tapeName, fromPtr->tapeName);
	toPtr->dbDumpId = fromPtr->dbDumpId;
	toPtr->initialDumpId = fromPtr->initialDumpId;
	toPtr->origVid = fromPtr->origVid;
	toPtr->vid = fromPtr->vid;
	toPtr->partition = fromPtr->partition;
	toPtr->dumpLevel = fromPtr->dumpLevel;
	toPtr->hostAddr = fromPtr->hostAddr;
	strcpy(toPtr->newName, fromPtr->newName);
	strcpy(toPtr->oldName, fromPtr->oldName);
	fromPtr++;
	toPtr++;

    }
    return 0;
}

static int
CopyTapeSetDesc(struct tc_tapeSet *toPtr, struct tc_tapeSet *fromPtr)
{

    toPtr->id = fromPtr->id;
    toPtr->maxTapes = fromPtr->maxTapes;
    toPtr->a = fromPtr->a;
    toPtr->b = fromPtr->b;
    strcpy(toPtr->tapeServer, fromPtr->tapeServer);
    strcpy(toPtr->format, fromPtr->format);

    toPtr->expDate = fromPtr->expDate;
    toPtr->expType = fromPtr->expType;
    return 0;
}

/* -------------------------
 * butc - interface routines - alphabetic order
 * -------------------------
 */

afs_int32
STC_LabelTape(struct rx_call *acid, struct tc_tapeLabel *label, afs_uint32 *taskId)
{
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif
    struct labelTapeIf *ptr;
    statusP statusPtr;
    afs_int32 code;

    extern int Labeller();
    extern statusP createStatusNode();
    extern afs_int32 allocTaskId();

#ifdef xbsa
    if (CONF_XBSA)
	return (TC_BADTASK);	/* LabelTape does not apply if XBSA */
#endif

    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    ptr = (struct labelTapeIf *)malloc(sizeof(*ptr));
    if (!ptr)
	ERROR_EXIT(TC_NOMEMORY);
    memcpy(&ptr->label, label, sizeof(ptr->label));

    /* set up the status node */
    *taskId = allocTaskId();	/* for bucoord */
    ptr->taskId = *taskId;

    statusPtr = createStatusNode();
    if (!statusPtr)
	ERROR_EXIT(TC_INTERNALERROR);

    lock_Status();
    statusPtr->taskId = *taskId;
    statusPtr->lastPolled = time(0);
    statusPtr->flags &= ~STARTING;	/* ok to examine */
    strncpy(statusPtr->taskName, "Labeltape", sizeof(statusPtr->taskName));
    unlock_Status();

    /* create the LWP to do the real work behind the scenes */
#ifdef AFS_PTHREAD_ENV
    code = pthread_attr_init(&tattr);
    if (code)
	ERROR_EXIT(code);

    code = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (code)
	ERROR_EXIT(code);

    AFS_SIGSET_CLEAR();
    code = pthread_create(&pid, &tattr, Labeller, ptr);
    AFS_SIGSET_RESTORE();
#else
    code =
	LWP_CreateProcess(Labeller, 32768, 1, (void *)ptr, "labeller process",
			  &pid);
#endif

  error_exit:
    if (code) {
	if (statusPtr)
	    deleteStatusNode(statusPtr);
	if (ptr)
	    free(ptr);
    }

    return (code);
}

/* STC_PerformDump
 *	Tape coordinator server routine to do a dump
 */

afs_int32
STC_PerformDump(struct rx_call *rxCallId, struct tc_dumpInterface *tcdiPtr, tc_dumpArray *tc_dumpArrayPtr, afs_int32 *taskId)
{
    struct dumpNode *newNode = 0;
    statusP statusPtr = 0;
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif
    afs_int32 code = 0;

    extern statusP createStatusNode();
    extern Dumper();

    if (callPermitted(rxCallId) == 0)
	return (TC_NOTPERMITTED);

    /* should be verifying parameter validity */
    *taskId = 0;

    /* this creates a node in list, alots an id for it and prepares it for locking */
    CreateNode(&newNode);

    /*set up the parameters in the node, to be used by LWP */
    strcpy(newNode->dumpSetName, tcdiPtr->dumpName);

    newNode->dumpName = (char *)malloc(strlen(tcdiPtr->dumpPath) + 1);
    strcpy(newNode->dumpName, tcdiPtr->dumpPath);

    newNode->volumeSetName =
	(char *)malloc(strlen(tcdiPtr->volumeSetName) + 1);
    strcpy(newNode->volumeSetName, tcdiPtr->volumeSetName);

    CopyTapeSetDesc(&(newNode->tapeSetDesc), &tcdiPtr->tapeSet);

    newNode->dumps = (struct tc_dumpDesc *)
	malloc(sizeof(struct tc_dumpDesc) *
	       tc_dumpArrayPtr->tc_dumpArray_len);
    newNode->arraySize = tc_dumpArrayPtr->tc_dumpArray_len;
    CopyDumpDesc(newNode->dumps, tc_dumpArrayPtr);

    newNode->parent = tcdiPtr->parentDumpId;
    newNode->level = tcdiPtr->dumpLevel;
    newNode->doAppend = tcdiPtr->doAppend;
#ifdef xbsa
    if (CONF_XBSA)
	newNode->doAppend = 0;	/* Append flag is ignored if talking to XBSA */
#endif

    /* create the status node */
    statusPtr = createStatusNode();
    if (!statusPtr)
	ERROR_EXIT(TC_INTERNALERROR);

    lock_Status();
    statusPtr->taskId = newNode->taskID;
    statusPtr->lastPolled = time(0);
    statusPtr->flags &= ~STARTING;	/* ok to examine */
    strncpy(statusPtr->taskName, "Dump", sizeof(statusPtr->taskName));
    unlock_Status();

    newNode->statusNodePtr = statusPtr;

    /* create the LWP to do the real work behind the scenes */
#ifdef AFS_PTHREAD_ENV
    code = pthread_attr_init(&tattr);
    if (code)
	ERROR_EXIT(code);

    code = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (code)
	ERROR_EXIT(code);

    AFS_SIGSET_CLEAR();
    code = pthread_create(&pid, &tattr, Dumper, newNode);
    AFS_SIGSET_RESTORE();
#else
    code =
	LWP_CreateProcess(Dumper, 32768, 1, (void *)newNode, "dumper process",
			  &pid);
#endif
    if (code)
	ERROR_EXIT(code);

    *taskId = newNode->taskID;

  error_exit:
    if (code) {
	if (statusPtr)
	    deleteStatusNode(statusPtr);
	FreeNode(newNode->taskID);	/*  failed to create LWP to do the dump. */
    }

    return (code);
}

afs_int32
STC_PerformRestore(struct rx_call *acid, char *dumpSetName, tc_restoreArray *arestores, afs_int32 *taskID)
{
    struct dumpNode *newNode;
    statusP statusPtr;
    afs_int32 code = 0;
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif

    extern int Restorer();
    extern statusP createStatusNode();

    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    /* should  verify parameter validity */

    /* this creates a node in list, alots an id for it and prepares it for locking */
    CreateNode(&newNode);

    newNode->restores = (struct tc_restoreDesc *)
	malloc(sizeof(struct tc_restoreDesc) *
	       arestores->tc_restoreArray_len);
    newNode->arraySize = arestores->tc_restoreArray_len;
    CopyRestoreDesc(newNode->restores, arestores);
    *taskID = newNode->taskID;

    /* should log the intent */

    /* create the status node */
    statusPtr = createStatusNode();
    if (!statusPtr)
	ERROR_EXIT(TC_INTERNALERROR);

    lock_Status();
    statusPtr->taskId = newNode->taskID;
    statusPtr->flags &= ~STARTING;	/* ok to examine */
    statusPtr->lastPolled = time(0);
    strncpy(statusPtr->taskName, "Restore", sizeof(statusPtr->taskName));
    unlock_Status();

    newNode->statusNodePtr = statusPtr;

    /* create the LWP to do the real work behind the scenes */
#ifdef AFS_PTHREAD_ENV
    code = pthread_attr_init(&tattr);
    if (code)
	ERROR_EXIT(code);

    code = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (code)
	ERROR_EXIT(code);

    AFS_SIGSET_CLEAR();
    code = pthread_create(&pid, &tattr, Restorer, newNode);
    AFS_SIGSET_RESTORE();
#else
    code =
	LWP_CreateProcess(Restorer, 65368, 1, (void *)newNode,
			  "restorer process", &pid);
#endif

  error_exit:
    if (code) {
	if (statusPtr)
	    deleteStatusNode(statusPtr);
	FreeNode(newNode->taskID);	/*  failed to create LWP to do the dump. */
    }

    return (code);
}

afs_int32
STC_ReadLabel(struct rx_call *acid, struct tc_tapeLabel *label, afs_uint32 *taskId)
{
    afs_int32 code;

    extern int ReadLabel();

#ifdef xbsa
    if (CONF_XBSA)
	return (TC_BADTASK);	/* ReadLabel does not apply if XBSA */
#endif

    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    code = ReadLabel(label);	/* Synchronous */
    return code;
}

/* STC_RestoreDb
 *	restore the backup database from tape
 */

afs_int32
STC_RestoreDb(struct rx_call *rxCall, afs_uint32 *taskId)
{
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif
    statusP statusPtr;
    afs_int32 code = 0;

    extern afs_int32 restoreDbFromTape();
    extern statusP createStatusNode();
    extern afs_int32 allocTaskId();

#ifdef xbsa
    if (CONF_XBSA)
	return (TC_BADTASK);	/* LabelTape does not apply if XBSA */
#endif

    if (callPermitted(rxCall) == 0)
	return (TC_NOTPERMITTED);

    *taskId = allocTaskId();

    /* create the status node */
    statusPtr = createStatusNode();
    if (!statusPtr)
	ERROR_EXIT(TC_INTERNALERROR);

    lock_Status();
    statusPtr->taskId = *taskId;
    statusPtr->flags &= ~STARTING;	/* ok to examine */
    statusPtr->lastPolled = time(0);
    strncpy(statusPtr->taskName, "RestoreDb", sizeof(statusPtr->taskName));
    unlock_Status();

#ifdef AFS_PTHREAD_ENV
    code = pthread_attr_init(&tattr);
    if (code)
	ERROR_EXIT(code);

    code = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (code)
	ERROR_EXIT(code);

    AFS_SIGSET_CLEAR();
    code = pthread_create(&pid, &tattr, restoreDbFromTape, (void *)*taskId);
    AFS_SIGSET_RESTORE();
#else
    code =
	LWP_CreateProcess(restoreDbFromTape, 32768, 1, (void *)*taskId,
			  "Db restore", &pid);
#endif

  error_exit:
    if (code) {
	if (statusPtr)
	    deleteStatusNode(statusPtr);
    }

    return (code);
}

/* STC_SaveDb
 *	restore the backup database from tape
 */

afs_int32
STC_SaveDb(struct rx_call *rxCall, Date archiveTime, afs_uint32 *taskId)
{
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif
    statusP statusPtr;
    afs_int32 code = 0;
    struct saveDbIf *ptr;

    extern afs_int32 saveDbToTape();
    extern statusP createStatusNode();
    extern afs_int32 allocTaskId();

#ifdef xbsa
    if (CONF_XBSA)
	return (TC_BADTASK);	/* LabelTape does not apply if XBSA */
#endif

    if (callPermitted(rxCall) == 0)
	return (TC_NOTPERMITTED);

    *taskId = allocTaskId();

    ptr = (struct saveDbIf *)malloc(sizeof(struct saveDbIf));
    if (!ptr)
	ERROR_EXIT(TC_NOMEMORY);
    ptr->archiveTime = archiveTime;
    ptr->taskId = *taskId;

    /* create the status node */
    statusPtr = createStatusNode();
    if (!statusPtr)
	ERROR_EXIT(TC_INTERNALERROR);

    lock_Status();
    statusPtr->taskId = *taskId;
    statusPtr->lastPolled = time(0);
    statusPtr->flags &= ~STARTING;	/* ok to examine */
    strncpy(statusPtr->taskName, "SaveDb", sizeof(statusPtr->taskName));
    unlock_Status();

    ptr->statusPtr = statusPtr;

#ifdef AFS_PTHREAD_ENV
    code = pthread_attr_init(&tattr);
    if (code)
	ERROR_EXIT(code);

    code = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (code)
	ERROR_EXIT(code);

    AFS_SIGSET_CLEAR();
    code = pthread_create(&pid, &tattr, saveDbToTape, ptr);
    AFS_SIGSET_RESTORE();
#else
    code = LWP_CreateProcess(saveDbToTape, 32768, 1, ptr, "Db save", &pid);
#endif

  error_exit:
    if (code) {
	if (statusPtr)
	    deleteStatusNode(statusPtr);
	if (ptr)
	    free(ptr);
    }

    return (code);
}


/* STC_ScanDumps
 * 	read a dump (maybe more than one tape), and print out a summary
 *	of its contents. If the flag is set, add to the database.
 * entry:
 *	addDbFlag - if set, the information will be added to the database
 */

afs_int32
STC_ScanDumps(struct rx_call *acid, afs_int32 addDbFlag, afs_uint32 *taskId)
{
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif
    struct scanTapeIf *ptr;
    statusP statusPtr;
    afs_int32 code = 0;

    extern afs_int32 ScanDumps();
    extern afs_int32 allocTaskId();
    extern statusP createStatusNode();

#ifdef xbsa
    if (CONF_XBSA)
	return (TC_BADTASK);	/* ScanDumps does not apply if XBSA */
#endif

    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    *taskId = allocTaskId();

    ptr = (struct scanTapeIf *)malloc(sizeof(*ptr));
    if (!ptr)
	ERROR_EXIT(TC_NOMEMORY);
    ptr->addDbFlag = addDbFlag;
    ptr->taskId = *taskId;

    /* create the status node */
    statusPtr = createStatusNode();
    if (!statusPtr)
	ERROR_EXIT(TC_INTERNALERROR);

    lock_Status();
    statusPtr->taskId = *taskId;
    statusPtr->lastPolled = time(0);
    statusPtr->flags &= ~STARTING;	/* ok to examine */
    strncpy(statusPtr->taskName, "Scantape", sizeof(statusPtr->taskName));
    unlock_Status();

#ifdef AFS_PTHREAD_ENV
    code = pthread_attr_init(&tattr);
    if (code)
	ERROR_EXIT(code);

    code = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (code)
	ERROR_EXIT(code);

    AFS_SIGSET_CLEAR();
    code = pthread_create(&pid, &tattr, ScanDumps, ptr);
    AFS_SIGSET_RESTORE();
#else
    code =
	LWP_CreateProcess(ScanDumps, 32768, 1, ptr, "scandump process", &pid);
#endif

  error_exit:
    if (code) {
	if (statusPtr)
	    deleteStatusNode(statusPtr);
	if (ptr)
	    free(ptr);
    }

    return code;
}

/* STC_TCInfo
 *	return information about the tape coordinator. Currently this
 *	is just the version number of the interface
 */

afs_int32
STC_TCInfo(struct rx_call *acid, struct tc_tcInfo *tciptr)
{
    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    tciptr->tcVersion = CUR_BUTC_VERSION;
    return (0);
}

/* STC_DeleteDump
 */
afs_int32
STC_DeleteDump(struct rx_call *acid, afs_uint32 dumpID, afs_uint32 *taskId)
{
    struct deleteDumpIf *ptr = 0;
    statusP statusPtr = 0;
    afs_int32 code = TC_BADTASK;	/* If not compiled -Dxbsa then fail */
#ifdef xbsa
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif
#endif
    extern afs_int32 DeleteDump();
    extern statusP createStatusNode();
    extern afs_int32 allocTaskId();

    *taskId = 0;
    if (!CONF_XBSA)
	return (TC_BADTASK);	/* Only do if butc is started as XBSA */

#ifdef xbsa
    code = 0;
    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    ptr = (struct deleteDumpIf *)malloc(sizeof(*ptr));
    if (!ptr)
	ERROR_EXIT(TC_NOMEMORY);

    *taskId = allocTaskId();
    ptr->dumpID = dumpID;
    ptr->taskId = *taskId;

    statusPtr = createStatusNode();
    if (!statusPtr)
	ERROR_EXIT(TC_INTERNALERROR);

    lock_Status();
    statusPtr->taskId = *taskId;
    statusPtr->lastPolled = time(0);
    statusPtr->flags &= ~STARTING;
    strncpy(statusPtr->taskName, "DeleteDump", sizeof(statusPtr->taskName));
    unlock_Status();

#ifdef AFS_PTHREAD_ENV
    code = pthread_attr_init(&tattr);
    if (code)
	ERROR_EXIT(code);

    code = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (code)
	ERROR_EXIT(code);

    AFS_SIGSET_CLEAR();
    code = pthread_create(&pid, &tattr, DeleteDump, ptr);
    AFS_SIGSET_RESTORE();
#else
    code =
	LWP_CreateProcess(DeleteDump, 32768, 1, ptr, "deletedump process",
			  &pid);
#endif

  error_exit:
    if (code) {
	if (statusPtr)
	    deleteStatusNode(statusPtr);
	if (ptr)
	    free(ptr);
    }
#endif /* xbsa */

    return (code);
}

