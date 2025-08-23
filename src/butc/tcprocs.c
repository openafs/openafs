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

#include <afs/procmgmt.h>
#include <roken.h>

#include <afs/opr.h>
#include <rx/rx.h>
#include <afs/afsint.h>
#include <afs/prs_fs.h>
#include <afs/nfs.h>
#include <lwp.h>
#include <afs/afs_lock.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/tcdata.h>
#include <afs/budb_client.h>
#include <afs/bucoord_prototypes.h>

#include "error_macros.h"
#include "butc_xbsa.h"
#include "butc_prototypes.h"
#include "butc_internal.h"
#include "afs/audit.h"

static int CopyDumpDesc(struct tc_dumpDesc *, tc_dumpArray *);
static int CopyRestoreDesc(struct tc_restoreDesc *, tc_restoreArray *);
static int CopyTapeSetDesc(struct tc_tapeSet *, struct tc_tapeSet *);

/* Helpers implementing RPC backends */
static afs_int32 SLabelTape(struct rx_call *acid, struct tc_tapeLabel *label,
			    afs_uint32 *taskId);
static afs_int32 SPerformDump(struct rx_call *rxCallId,
			      struct tc_dumpInterface *tcdiPtr,
			      tc_dumpArray *tc_dumpArrayPtr, afs_int32 *taskId);
static afs_int32 SPerformRestore(struct rx_call *acid, char *dumpSetName,
				 tc_restoreArray *arestores, afs_int32 *taskId);
static afs_int32 SReadLabel(struct rx_call *acid, struct tc_tapeLabel *label,
			    afs_uint32 *taskId);
static afs_int32 SRestoreDb(struct rx_call *rxCall, afs_uint32 *taskId);
static afs_int32 SSaveDb(struct rx_call *rxCall, Date archiveTime,
			 afs_uint32 *taskId);
static afs_int32 SScanDumps(struct rx_call *acid, afs_int32 addDbFlag,
			    afs_uint32 *taskId);
static afs_int32 STCInfo(struct rx_call *acid, struct tc_tcInfo *tciptr);
static afs_int32 SDeleteDump(struct rx_call *acid, afs_uint32 dumpID,
			     afs_uint32 *taskId);

int
callPermitted(struct rx_call *call)
{
    /*
     * If in backwards compat mode, allow anyone; otherwise, only
     * superusers are allowed.
     */
    if (allow_unauth)
	return 1;
    return afsconf_SuperIdentity(butc_confdir, call, NULL);
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
    afs_int32 code;

    code = SLabelTape(acid, label, taskId);
    osi_auditU(acid, TC_LabelTapeEvent, code,
	       AUD_TLBL, label, AUD_INT, *taskId, AUD_END);
    return code;
}

static afs_int32
SLabelTape(struct rx_call *acid, struct tc_tapeLabel *label, afs_uint32 *taskId)
{
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif
    struct labelTapeIf *ptr;
    statusP statusPtr = NULL;
    afs_int32 code;

    *taskId = 0;
#ifdef xbsa
    if (CONF_XBSA)
	return (TC_BADTASK);	/* LabelTape does not apply if XBSA */
#endif

    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    ptr = malloc(sizeof(*ptr));
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
STC_PerformDump(struct rx_call *call, struct tc_dumpInterface *di,
		tc_dumpArray *da, afs_int32 *taskId)
{
    afs_int32 code;

    code = SPerformDump(call, di, da, taskId);
    osi_auditU(call, TC_PerformDumpEvent, code,
	       AUD_TDI, di, AUD_TDA, da, AUD_INT, *taskId, AUD_END);
    return code;
}

static afs_int32
SPerformDump(struct rx_call *rxCallId, struct tc_dumpInterface *tcdiPtr,
	     tc_dumpArray *tc_dumpArrayPtr, afs_int32 *taskId)
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

    /* should be verifying parameter validity */
    *taskId = 0;

    if (callPermitted(rxCallId) == 0)
	return (TC_NOTPERMITTED);

    /* this creates a node in list, alots an id for it and prepares it for locking */
    CreateNode(&newNode);

    /*set up the parameters in the node, to be used by LWP */
    strcpy(newNode->dumpSetName, tcdiPtr->dumpName);

    newNode->dumpName = strdup(tcdiPtr->dumpPath);
    newNode->volumeSetName = strdup(tcdiPtr->volumeSetName);

    CopyTapeSetDesc(&(newNode->tapeSetDesc), &tcdiPtr->tapeSet);

    newNode->dumps = malloc(sizeof(struct tc_dumpDesc) *
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
STC_PerformRestore(struct rx_call *call, char *dumpSetName,
		   tc_restoreArray *ra, afs_int32 *taskId)
{
    afs_int32 code;

    code = SPerformRestore(call, dumpSetName, ra, taskId);
    osi_auditU(call, TC_PerformRestoreEvent, code,
	       AUD_STR, dumpSetName, AUD_TRA, ra, AUD_INT, *taskId, AUD_END);
    return code;
}

static afs_int32
SPerformRestore(struct rx_call *acid, char *dumpSetName,
	        tc_restoreArray *arestores, afs_int32 *taskId)
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

    *taskId = 0;

    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    /* should  verify parameter validity */

    /* this creates a node in list, alots an id for it and prepares it for locking */
    CreateNode(&newNode);

    newNode->restores = malloc(sizeof(struct tc_restoreDesc) *
		               arestores->tc_restoreArray_len);
    newNode->arraySize = arestores->tc_restoreArray_len;
    CopyRestoreDesc(newNode->restores, arestores);
    *taskId = newNode->taskID;

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
STC_ReadLabel(struct rx_call *call, struct tc_tapeLabel *label, afs_uint32 *taskId)
{
    afs_int32 code;

    code = SReadLabel(call, label, taskId);
    osi_auditU(call, TC_ReadLabelEvent, code,
	       AUD_TLBL, label, AUD_INT, *taskId, AUD_END);
    return code;
}

static afs_int32
SReadLabel(struct rx_call *acid, struct tc_tapeLabel *label, afs_uint32 *taskId)
{
    afs_int32 code;

    memset(label, 0, sizeof(*label));
    /* Synchronous, so no "real" ID; don't send stack garbage on the wire */
    *taskId = 0;
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
STC_RestoreDb(struct rx_call *call, afs_uint32 *taskId)
{
    afs_int32 code;

    code = SRestoreDb(call, taskId);
    osi_auditU(call, TC_RestoreDbEvent, code, AUD_INT, *taskId, AUD_END);
    return code;
}

static afs_int32
SRestoreDb(struct rx_call *rxCall, afs_uint32 *taskId)
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

    *taskId = 0;

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
    code = pthread_create(&pid, &tattr, restoreDbFromTape, (void *)(intptr_t)*taskId);
    AFS_SIGSET_RESTORE();
#else
    code =
	LWP_CreateProcess(restoreDbFromTape, 32768, 1, (void *)(intptr_t)*taskId,
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
STC_SaveDb(struct rx_call *call, Date archiveTime, afs_uint32 *taskId)
{
    afs_int32 code;

    code = SSaveDb(call, archiveTime, taskId);
    osi_auditU(call, TC_SaveDbEvent, code,
	       AUD_DATE, archiveTime, AUD_INT, *taskId, AUD_END);
    return code;
}

static afs_int32
SSaveDb(struct rx_call *rxCall, Date archiveTime, afs_uint32 *taskId)
{
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif
    statusP statusPtr = NULL;
    afs_int32 code = 0;
    struct saveDbIf *ptr;

    *taskId = 0;

#ifdef xbsa
    if (CONF_XBSA)
	return (TC_BADTASK);	/* LabelTape does not apply if XBSA */
#endif

    if (callPermitted(rxCall) == 0)
	return (TC_NOTPERMITTED);

    *taskId = allocTaskId();

    ptr = malloc(sizeof(struct saveDbIf));
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
STC_ScanDumps(struct rx_call *call, afs_int32 addDbFlag, afs_uint32 *taskId)
{
    afs_int32 code;

    code = SScanDumps(call, addDbFlag, taskId);
    osi_auditU(call, TC_ScanDumpsEvent, code,
	       AUD_INT, addDbFlag, AUD_INT, *taskId, AUD_END);
    return code;
}

static afs_int32
SScanDumps(struct rx_call *acid, afs_int32 addDbFlag, afs_uint32 *taskId)
{
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif
    struct scanTapeIf *ptr;
    statusP statusPtr = NULL;
    afs_int32 code = 0;

    *taskId = 0;

#ifdef xbsa
    if (CONF_XBSA)
	return (TC_BADTASK);	/* ScanDumps does not apply if XBSA */
#endif

    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    *taskId = allocTaskId();

    ptr = malloc(sizeof(*ptr));
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
STC_TCInfo(struct rx_call *call, struct tc_tcInfo *ti)
{
    afs_int32 code;

    code = STCInfo(call, ti);
    osi_auditU(call, TC_TCInfoEvent, code, AUD_INT, ti->tcVersion, AUD_END);
    return code;
}

static afs_int32
STCInfo(struct rx_call *acid, struct tc_tcInfo *tciptr)
{
    memset(tciptr, 0, sizeof(*tciptr));

    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    tciptr->tcVersion = CUR_BUTC_VERSION;
    return (0);
}

/* STC_DeleteDump
 */
afs_int32
STC_DeleteDump(struct rx_call *call, afs_uint32 dumpID, afs_uint32 *taskId)
{
    afs_int32 code;

    code = SDeleteDump(call, dumpID, taskId);
    osi_auditU(call, TC_DeleteDumpEvent, code,
	       AUD_DATE, dumpID, AUD_INT, *taskId, AUD_END);
    return code;
}

static afs_int32
SDeleteDump(struct rx_call *acid, afs_uint32 dumpID, afs_uint32 *taskId)
{
    afs_int32 code = TC_BADTASK;	/* If not compiled -Dxbsa then fail */
#ifdef xbsa
    struct deleteDumpIf *ptr = 0;
    statusP statusPtr = 0;
#ifdef AFS_PTHREAD_ENV
    pthread_t pid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS pid;
#endif
#endif

    *taskId = 0;

    if (!CONF_XBSA)
	return (TC_BADTASK);	/* Only do if butc is started as XBSA */

#ifdef xbsa
    code = 0;
    if (callPermitted(acid) == 0)
	return (TC_NOTPERMITTED);

    ptr = malloc(sizeof(*ptr));
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

