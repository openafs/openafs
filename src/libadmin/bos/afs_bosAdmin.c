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

#include <stdio.h>
#include <afs/stds.h>
#include "afs_bosAdmin.h"
#include "../adminutil/afs_AdminInternal.h"
#include <afs/afs_AdminErrors.h>
#include <afs/afs_utilAdmin.h>
#include <afs/bosint.h>
#include <afs/bnode.h>
#include <afs/ktime.h>
#include <afs/dirpath.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#else
#include <unistd.h>
#endif
#include <string.h>

typedef struct bos_server {
    int begin_magic;
    int is_valid;
    struct rx_connection *server;
    struct rx_connection *server_encrypt;
    struct rx_connection *server_stats;
    int end_magic;
} bos_server_t, *bos_server_p;

/*
 * isValidServerHandle - validate a bos_server_p.
 *
 * PARAMETERS
 *
 * IN serverHandle - the handle to validate
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int
isValidServerHandle(const bos_server_p serverHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (serverHandle == NULL) {
	tst = ADMBOSSERVERHANDLENULL;
	goto fail_IsValidServerHandle;
    }

    if ((serverHandle->begin_magic != BEGIN_MAGIC)
	|| (serverHandle->end_magic != END_MAGIC)) {
	tst = ADMBOSSERVERHANDLEBADMAGIC;
	goto fail_IsValidServerHandle;
    }

    if (serverHandle->is_valid == 0) {
	tst = ADMBOSSERVERHANDLEINVALID;
	goto fail_IsValidServerHandle;
    }

    if (serverHandle->server == NULL) {
	tst = ADMBOSSERVERHANDLENOSERVER;
	goto fail_IsValidServerHandle;
    }

    if (serverHandle->server_encrypt == NULL) {
	tst = ADMBOSSERVERHANDLENOSERVER;
	goto fail_IsValidServerHandle;
    }
    rc = 1;

  fail_IsValidServerHandle:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * IsValidCellHandle - verify that a cell handle can be used to make bos
 * requests.
 *
 * PARAMETERS
 *
 * IN cellHandle - the cellHandle to be validated.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
IsValidCellHandle(afs_cell_handle_p cellHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    if (!CellHandleIsValid((void *)cellHandle, &tst)) {
	goto fail_IsValidCellHandle;
    }

    if (cellHandle->tokens == NULL) {
	tst = ADMBOSCELLHANDLENOTOKENS;
	goto fail_IsValidCellHandle;
    }
    rc = 1;

  fail_IsValidCellHandle:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ServerOpen - open a bos server for work.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle.
 *
 * IN serverName - the name of the machine that houses the bosserver of 
 * interest.
 *
 * OUT serverHandleP - upon successful completion, this void pointer
 * will point to a valid server handle for use in future operations.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_ServerOpen(const void *cellHandle, const char *serverName,
	       void **serverHandleP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    bos_server_p bos_server = (bos_server_p) malloc(sizeof(bos_server_t));
    int serverAddress;

    /*
     * Validate parameters
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_bos_ServerOpen;
    }

    if ((serverName == NULL) || (*serverName == 0)) {
	tst = ADMBOSSERVERNAMENULL;
	goto fail_bos_ServerOpen;
    }

    if (serverHandleP == NULL) {
	tst = ADMBOSSERVERHANDLEPNULL;
	goto fail_bos_ServerOpen;
    }

    if (util_AdminServerAddressGetFromName(serverName, &serverAddress, &tst)
	== 0) {
	goto fail_bos_ServerOpen;
    }

    if (bos_server == NULL) {
	tst = ADMNOMEM;
	goto fail_bos_ServerOpen;
    }

    bos_server->server =
	rx_GetCachedConnection(htonl(serverAddress), htons(AFSCONF_NANNYPORT),
			       1,
			       c_handle->tokens->afs_sc,
			       c_handle->tokens->sc_index);

    bos_server->server_encrypt =
	rx_GetCachedConnection(htonl(serverAddress), htons(AFSCONF_NANNYPORT),
			       1,
			       c_handle->tokens->afs_encrypt_sc,
			       c_handle->tokens->sc_index);

    bos_server->server_stats =
	rx_GetCachedConnection(htonl(serverAddress), htons(AFSCONF_NANNYPORT),
			       RX_STATS_SERVICE_ID,
			       c_handle->tokens->afs_sc,
			       c_handle->tokens->sc_index);

    if ((bos_server->server == NULL) || (bos_server->server_encrypt == NULL)) {
	tst = ADMBOSSERVERNOCONNECTION;
	goto fail_bos_ServerOpen;
    }

    bos_server->begin_magic = BEGIN_MAGIC;
    bos_server->is_valid = 1;
    bos_server->end_magic = END_MAGIC;

    *serverHandleP = (void *)bos_server;
    rc = 1;

  fail_bos_ServerOpen:

    if ((rc == 0) && (bos_server != NULL)) {
	if (bos_server->server) {
	    rx_ReleaseCachedConnection(bos_server->server);
	}
	if (bos_server->server_encrypt) {
	    rx_ReleaseCachedConnection(bos_server->server_encrypt);
	}
	free(bos_server);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ServerClose - close a bos server handle
 *
 * PARAMETERS
 *
 * IN serverHandle - a bos server handle previously returned by bos_ServerOpen
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_ServerClose(const void *serverHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (isValidServerHandle(b_handle, &tst)) {
	rx_ReleaseCachedConnection(b_handle->server);
	b_handle->is_valid = 0;
	free(b_handle);
	rc = 1;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessCreate - create a new process to run at a bos server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the name of the process to create.
 *
 * IN processType - the type of process to create.
 *
 * IN process - the path to the process binary at the bos server.
 *
 * IN cronTime - the time specification for cron processes.
 *
 * IN notifier - the path to the notifier binary at the bos server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

/*
 * CAUTION - this list must match bos_ProcessType_t definition
 */

static char *processTypes[] = { "simple", "fs", "cron", 0 };

int ADMINAPI
bos_ProcessCreate(const void *serverHandle, const char *processName,
		  bos_ProcessType_t processType, const char *process,
		  const char *cronTime, const char *notifier, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessCreate;
    }

    if (processType == BOS_PROCESS_FS) {
	tst = ADMBOSPROCESSCREATEBADTYPE;
	goto fail_bos_ProcessCreate;
    }

    if ((processName == NULL) || (*processName == 0)) {
	tst = ADMBOSPROCESSNAMENULL;
	goto fail_bos_ProcessCreate;
    }

    if ((process == NULL) || (*process == 0)) {
	tst = ADMBOSPROCESSNULL;
	goto fail_bos_ProcessCreate;
    }

    if ((processType == BOS_PROCESS_CRON) && (cronTime == NULL)) {
	tst = ADMBOSCRONTIMENULL;
	goto fail_bos_ProcessCreate;
    }

    if ((processType == BOS_PROCESS_SIMPLE) && (cronTime != NULL)) {
	tst = ADMBOSCRONTIMENOTNULL;
	goto fail_bos_ProcessCreate;
    }

    tst =
	BOZO_CreateBnode(b_handle->server, processTypes[processType],
			 processName, process, (cronTime) ? cronTime : "", "",
			 "", "", (notifier) ? notifier : NONOTIFIER);
    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ProcessCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_FSProcessCreate - create the fs group of processes at the boserver.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the name of the process to create.
 *
 * IN fileserverPath - the path to the fileserver binary at the bos server.
 *
 * IN volserverPath - the path to the volserver binary at the bos server.
 *
 * IN salvagerPath - the path to the salvager binary at the bos server.
 *
 * IN notifier - the path to the notifier binary at the bos server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_FSProcessCreate(const void *serverHandle, const char *processName,
		    const char *fileserverPath, const char *volserverPath,
		    const char *salvagerPath, const char *notifier,
		    afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessCreate;
    }

    if ((processName == NULL) || (*processName == 0)) {
	tst = ADMBOSPROCESSNAMENULL;
	goto fail_bos_ProcessCreate;
    }

    if ((fileserverPath == NULL) || (*fileserverPath == 0)) {
	tst = ADMBOSFILESERVERPATHNULL;
	goto fail_bos_ProcessCreate;
    }

    if ((volserverPath == NULL) || (*volserverPath == 0)) {
	tst = ADMBOSVOLSERVERPATHNULL;
	goto fail_bos_ProcessCreate;
    }

    if ((salvagerPath == NULL) || (*salvagerPath == 0)) {
	tst = ADMBOSSALVAGERPATHNULL;
	goto fail_bos_ProcessCreate;
    }

    tst =
	BOZO_CreateBnode(b_handle->server, processTypes[BOS_PROCESS_FS],
			 processName, fileserverPath, volserverPath,
			 salvagerPath, "", "",
			 (notifier) ? notifier : NONOTIFIER);
    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ProcessCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessDelete - delete an existing process at a bos server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the name of the process to delete.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_ProcessDelete(const void *serverHandle, const char *processName,
		  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessDelete;
    }

    if ((processName == NULL) || (*processName == 0)) {
	tst = ADMBOSPROCESSNAMENULL;
	goto fail_bos_ProcessDelete;
    }

    tst = BOZO_DeleteBnode(b_handle->server, processName);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ProcessDelete:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessExecutionStateGet - get the current execution state of a 
 * process
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the name of the process to retrieve.
 *
 * OUT processStatusP - upon successful completion the process execution state
 *
 * OUT auxiliaryProcessStatus - set to point to aux proc status if available.
 * Pass a pointer to an char array at least BOS_MAX_NAME_LEN long.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_ProcessExecutionStateGet(const void *serverHandle,
			     const char *processName,
			     bos_ProcessExecutionState_p processStatusP,
			     char *auxiliaryProcessStatus, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_int32 state;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessExecutionStateGet;
    }

    if ((processName == NULL) || (*processName == 0)) {
	tst = ADMBOSPROCESSNAMENULL;
	goto fail_bos_ProcessExecutionStateGet;
    }

    if (processStatusP == NULL) {
	tst = ADMBOSPROCESSSTATUSPNULL;
	goto fail_bos_ProcessExecutionStateGet;
    }

    if (auxiliaryProcessStatus == NULL) {
	tst = ADMBOSAUXILIARYPROCESSSTATUSNULL;
	goto fail_bos_ProcessExecutionStateGet;
    }

    tst =
	BOZO_GetStatus(b_handle->server, processName, &state,
		       &auxiliaryProcessStatus);

    if (tst != 0) {
	goto fail_bos_ProcessExecutionStateGet;
    }

    *processStatusP = (bos_ProcessExecutionState_t) state;
    rc = 1;

  fail_bos_ProcessExecutionStateGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * SetExecutionState - set the execution state of a process
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the name of the process to modify.
 *
 * IN processStatus - the new process state.
 *
 * IN func - the function to call to set the status.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static int
SetExecutionState(const void *serverHandle, const char *processName,
		  const bos_ProcessExecutionState_t processStatus,
		  int (*func) (struct rx_connection *, const char *,
			       afs_int32), afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_int32 state;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_SetExecutionState;
    }

    if ((processName == NULL) || (*processName == 0)) {
	tst = ADMBOSPROCESSNAMENULL;
	goto fail_SetExecutionState;
    }

    if ((processStatus != BOS_PROCESS_STOPPED)
	&& (processStatus != BOS_PROCESS_RUNNING)) {
	tst = ADMBOSPROCESSSTATUSSET;
	goto fail_SetExecutionState;
    }

    state = (afs_int32) processStatus;

    tst = func(b_handle->server, processName, state);

    if (tst == 0) {
	rc = 1;
    }

  fail_SetExecutionState:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessExecutionStateSet - set the execution state of a process
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the name of the process to modify.
 *
 * IN processStatus - the new process state.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_ProcessExecutionStateSet(const void *serverHandle,
			     const char *processName,
			     bos_ProcessExecutionState_t processStatus,
			     afs_status_p st)
{
    return SetExecutionState(serverHandle, processName, processStatus,
			     BOZO_SetStatus, st);
}

/*
 * bos_ProcessExecutionStateSetTemporary - set the execution state of a process
 * temporarily
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the name of the process to modify.
 *
 * IN processStatus - the new process state.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_ProcessExecutionStateSetTemporary(const void *serverHandle,
				      const char *processName,
				      bos_ProcessExecutionState_t
				      processStatus, afs_status_p st)
{
    return SetExecutionState(serverHandle, processName, processStatus,
			     BOZO_SetTStatus, st);
}

/*
 * The iterator functions and data for the process name retrieval functions
 */

typedef struct process_name_get {
    int next;
    struct rx_connection *server;
    char process[CACHED_ITEMS][BOS_MAX_NAME_LEN];
} process_name_get_t, *process_name_get_p;

static int
GetProcessNameRPC(void *rpc_specific, int slot, int *last_item,
		  int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    process_name_get_p proc = (process_name_get_p) rpc_specific;
    char *ptr = (char *)&proc->process[slot];

    tst = BOZO_EnumerateInstance(proc->server, proc->next++, &ptr);

    if (tst == 0) {
	rc = 1;
    } else if (tst == BZDOM) {
	tst = 0;
	rc = 1;
	*last_item = 1;
	*last_item_contains_data = 0;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetProcessNameFromCache(void *rpc_specific, int slot, void *dest,
			afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    process_name_get_p proc = (process_name_get_p) rpc_specific;

    strcpy((char *)dest, (char *)&proc->process[slot]);
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessNameGetBegin - begin iterating over the list of processes
 * at a particular bos server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * OUT iter - an iterator that can be passed to bos_ProcessNameGetNext
 * to retrieve the process names.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessNameGetBegin(const void *serverHandle, void **iterationIdP,
			afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    process_name_get_p proc =
	(process_name_get_p) malloc(sizeof(process_name_get_t));

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessNameGetBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_ProcessNameGetBegin;
    }

    if ((iter == NULL) || (proc == NULL)) {
	tst = ADMNOMEM;
	goto fail_bos_ProcessNameGetBegin;
    }

    proc->next = 0;
    proc->server = b_handle->server;

    if (IteratorInit
	(iter, (void *)proc, GetProcessNameRPC, GetProcessNameFromCache, NULL,
	 NULL, &tst)) {
	*iterationIdP = (void *)iter;
    } else {
	goto fail_bos_ProcessNameGetBegin;
    }
    rc = 1;

  fail_bos_ProcessNameGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (proc != NULL) {
	    free(proc);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessNameGetNext - retrieve the next process name from the bos server.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by bos_ProcessNameGetBegin
 *
 * OUT processName - upon successful completion contains the next process name
 * retrieved from the server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessNameGetNext(const void *iterationId, char *processName,
		       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (processName == NULL) {
	tst = ADMBOSPROCESSNAMENULL;
	goto fail_bos_ProcessNameGetNext;
    }

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_ProcessNameGetNext;
    }

    rc = IteratorNext(iter, (void *)processName, &tst);

  fail_bos_ProcessNameGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessNameGetDone - finish using a process name iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by bos_ProcessNameGetBegin
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessNameGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_ProcessNameGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_bos_ProcessNameGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessInfoGet - get information about a single process
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the process of interest.
 *
 * OUT processTypeP - upon successful completion contains the process type
 *
 * OUT processInfoP - upon successful completion contains the process info
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessInfoGet(const void *serverHandle, const char *processName,
		   bos_ProcessType_p processTypeP,
		   bos_ProcessInfo_p processInfoP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    char type[BOS_MAX_NAME_LEN];
    char *ptr = type;
    struct bozo_status status;
    int i;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessInfoGet;
    }

    if ((processName == NULL) || (*processName == 0)) {
	tst = ADMBOSPROCESSNAMENULL;
	goto fail_bos_ProcessInfoGet;
    }

    if (processTypeP == NULL) {
	tst = ADMBOSPROCESSTYPEPNULL;
	goto fail_bos_ProcessInfoGet;
    }

    if (processInfoP == NULL) {
	tst = ADMBOSPROCESSINFOPNULL;
	goto fail_bos_ProcessInfoGet;
    }

    tst = BOZO_GetInstanceInfo(b_handle->server, processName, &ptr, &status);

    if (tst != 0) {
	goto fail_bos_ProcessInfoGet;
    }


    for (i = 0; (processTypes[i] != NULL); i++) {
	if (!strcmp(processTypes[i], type)) {
	    *processTypeP = (bos_ProcessType_t) i;
	    break;
	}
    }

    if (processTypes[i] == NULL) {
	tst = ADMBOSINVALIDPROCESSTYPE;
	goto fail_bos_ProcessInfoGet;
    }

    processInfoP->processGoal = (bos_ProcessExecutionState_t) status.goal;
    processInfoP->processStartTime = status.procStartTime;
    processInfoP->numberProcessStarts = status.procStarts;
    processInfoP->processExitTime = status.lastAnyExit;
    processInfoP->processExitErrorTime = status.lastErrorExit;
    processInfoP->processErrorCode = status.errorCode;
    processInfoP->processErrorSignal = status.errorSignal;
    processInfoP->state = BOS_PROCESS_OK;

    if (status.flags & BOZO_ERRORSTOP) {
	processInfoP->state |= BOS_PROCESS_TOO_MANY_ERRORS;
    }
    if (status.flags & BOZO_HASCORE) {
	processInfoP->state |= BOS_PROCESS_CORE_DUMPED;
    }
    if (status.flags & BOZO_BADDIRACCESS) {
	processInfoP->state |= BOS_PROCESS_BAD_FILE_ACCESS;
    }
    rc = 1;

  fail_bos_ProcessInfoGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the parameter retrieval functions
 */

typedef struct param_get {
    int next;
    struct rx_connection *server;
    char processName[BOS_MAX_NAME_LEN];
    char param[CACHED_ITEMS][BOS_MAX_NAME_LEN];
} param_get_t, *param_get_p;

static int
GetParameterRPC(void *rpc_specific, int slot, int *last_item,
		int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    param_get_p param = (param_get_p) rpc_specific;
    char *ptr = (char *)&param->param[slot];

    tst =
	BOZO_GetInstanceParm(param->server, param->processName, param->next++,
			     &ptr);

    if (tst == 0) {
	rc = 1;
    } else if (tst == BZDOM) {
	tst = 0;
	rc = 1;
	*last_item = 1;
	*last_item_contains_data = 0;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetParameterFromCache(void *rpc_specific, int slot, void *dest,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    param_get_p param = (param_get_p) rpc_specific;

    strcpy((char *)dest, (char *)&param->param[slot]);
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessParameterGetBegin - begin iterating over the parameters
 * of a particular process.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the process whose parameters are returned.
 *
 * OUT iter - an iterator that can be passed to bos_ProcessParameterGetNext
 * to retrieve the parameters.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessParameterGetBegin(const void *serverHandle,
			     const char *processName, void **iterationIdP,
			     afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    param_get_p param = (param_get_p) malloc(sizeof(param_get_t));

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessParameterGetBegin;
    }

    if ((processName == NULL) || (*processName == 0)) {
	tst = ADMBOSPROCESSNAMENULL;
	goto fail_bos_ProcessParameterGetBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_ProcessParameterGetBegin;
    }

    if ((iter == NULL) || (param == NULL)) {
	tst = ADMNOMEM;
	goto fail_bos_ProcessParameterGetBegin;
    }

    param->next = 0;
    param->server = b_handle->server;
    strcpy(param->processName, processName);

    if (IteratorInit
	(iter, (void *)param, GetParameterRPC, GetParameterFromCache, NULL,
	 NULL, &tst)) {
	*iterationIdP = (void *)iter;
    } else {
	goto fail_bos_ProcessParameterGetBegin;
    }
    rc = 1;

  fail_bos_ProcessParameterGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (param != NULL) {
	    free(param);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessParameterGetNext - retrieve the next parameter 
 * from the bos server.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by 
 * bos_ProcessParameterGetBegin
 *
 * OUT parameter - upon successful completion contains the next parameter
 * retrieved from the server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessParameterGetNext(const void *iterationId, char *parameter,
			    afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_ProcessParameterGetNext;
    }

    if (parameter == NULL) {
	tst = ADMBOSPARAMETERNULL;
	goto fail_bos_ProcessParameterGetNext;
    }

    rc = IteratorNext(iter, (void *)parameter, &tst);

  fail_bos_ProcessParameterGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessParameterGetDone - finish using a process name iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * bos_ProcessParameterGetBegin
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessParameterGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_ProcessParameterGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_bos_ProcessParameterGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessNotifierGet - retrieve the notifier associated with a
 * process.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the process whose notifier we are retrieving.
 *
 * OUT notifier - upon successful completion contains the notifier.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessNotifierGet(const void *serverHandle, const char *processName,
		       char *notifier, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessNotifierGet;
    }

    if ((processName == NULL) || (*processName == 0)) {
	tst = ADMBOSPROCESSNAMENULL;
	goto fail_bos_ProcessNotifierGet;
    }

    if (notifier == NULL) {
	tst = ADMBOSNOTIFIERNULL;
	goto fail_bos_ProcessNotifierGet;
    }

    tst = BOZO_GetInstanceParm(b_handle->server, processName, 999, &notifier);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ProcessNotifierGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessRestart - restart a particular process.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN processName - the process to restart
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessRestart(const void *serverHandle, const char *processName,
		   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessRestart;
    }

    if ((processName == NULL) || (*processName == 0)) {
	tst = ADMBOSPROCESSNAMENULL;
	goto fail_bos_ProcessRestart;
    }

    tst = BOZO_Restart(b_handle->server, processName);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ProcessRestart:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessAllStop - stop all running processes at a server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessAllStop(const void *serverHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessAllStop;
    }

    tst = BOZO_ShutdownAll(b_handle->server);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ProcessAllStop:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessAllStart - start all processes that should be running at a
 * server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessAllStart(const void *serverHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessAllStart;
    }

    tst = BOZO_StartupAll(b_handle->server);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ProcessAllStart:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessAllWaitStop - stop all processes, and block until they have
 * exited.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessAllWaitStop(const void *serverHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessAllWaitStop;
    }

    if (!bos_ProcessAllStop(serverHandle, &tst)) {
	goto fail_bos_ProcessAllWaitStop;
    }

    tst = BOZO_WaitAll(b_handle->server);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ProcessAllWaitStop:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessAllWaitTransition - block until all processes at the bosserver
 * have reached their desired state.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessAllWaitTransition(const void *serverHandle, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessAllWaitTransition;
    }

    tst = BOZO_WaitAll(b_handle->server);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ProcessAllWaitTransition:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ProcessAllStopAndRestart - stop all the running processes, restart
 * them, and optionally restart the bosserver itself.
 * exited.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN restartBosServer - flag to indicate whether to restart bosserver.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ProcessAllStopAndRestart(const void *serverHandle,
			     bos_RestartBosServer_t restartBosServer,
			     afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ProcessAllStopAndRestart;
    }

    if (restartBosServer == BOS_RESTART_BOS_SERVER) {
	tst = BOZO_ReBozo(b_handle->server);
	if (tst != 0) {
	    goto fail_bos_ProcessAllStopAndRestart;
	}
    }

    tst = BOZO_RestartAll(b_handle->server);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ProcessAllStopAndRestart:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_AdminCreate - create a new admin.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN adminName - the new admin name.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_AdminCreate(const void *serverHandle, const char *adminName,
		afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_AdminCreate;
    }

    if ((adminName == NULL) || (*adminName == 0)) {
	tst = ADMBOSADMINNAMENULL;
	goto fail_bos_AdminCreate;
    }

    tst = BOZO_AddSUser(b_handle->server, adminName);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_AdminCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_AdminDelete - delete a new admin.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN adminName - the admin name.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_AdminDelete(const void *serverHandle, const char *adminName,
		afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_AdminDelete;
    }

    if ((adminName == NULL) || (*adminName == 0)) {
	tst = ADMBOSADMINNAMENULL;
	goto fail_bos_AdminDelete;
    }

    tst = BOZO_DeleteSUser(b_handle->server, adminName);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_AdminDelete:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the admin retrieval functions
 */

typedef struct admin_get {
    int next;
    struct rx_connection *server;
    char admin[CACHED_ITEMS][BOS_MAX_NAME_LEN];
} admin_get_t, *admin_get_p;

static int
GetAdminRPC(void *rpc_specific, int slot, int *last_item,
	    int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    admin_get_p admin = (admin_get_p) rpc_specific;
    char *ptr = (char *)&admin->admin[slot];

    tst = BOZO_ListSUsers(admin->server, admin->next++, &ptr);

    /*
     * There's no way to tell the difference between an rpc failure
     * and the end of the list, so we assume that any error means the
     * end of the list
     */

    if (tst != 0) {
	tst = 0;
	*last_item = 1;
	*last_item_contains_data = 0;
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetAdminFromCache(void *rpc_specific, int slot, void *dest, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    admin_get_p admin = (admin_get_p) rpc_specific;

    strcpy((char *)dest, (char *)&admin->admin[slot]);
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_AdminGetBegin - begin iterating over the administrators.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * OUT iter - an iterator that can be passed to bos_AdminGetBegin
 * to retrieve the administrators.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_AdminGetBegin(const void *serverHandle, void **iterationIdP,
		  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    admin_get_p admin = (admin_get_p) malloc(sizeof(admin_get_t));

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_AdminGetBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_AdminGetBegin;
    }

    if ((iter == NULL) || (admin == NULL)) {
	tst = ADMNOMEM;
	goto fail_bos_AdminGetBegin;
    }

    admin->next = 0;
    admin->server = b_handle->server;

    if (IteratorInit
	(iter, (void *)admin, GetAdminRPC, GetAdminFromCache, NULL, NULL,
	 &tst)) {
	*iterationIdP = (void *)iter;
	rc = 1;
    }

  fail_bos_AdminGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (admin != NULL) {
	    free(admin);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_AdminGetNext - retrieve the next administrator 
 * from the bos server.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by 
 * bos_AdminGetBegin
 *
 * OUT adminName - upon successful completion contains the next administrator
 * retrieved from the server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_AdminGetNext(const void *iterationId, char *adminName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_AdminGetNext;
    }

    if (adminName == NULL) {
	tst = ADMBOSADMINNAMENULL;
	goto fail_bos_AdminGetNext;
    }

    rc = IteratorNext(iter, (void *)adminName, &tst);

  fail_bos_AdminGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_AdminGetDone - finish using a administrator iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * bos_AdminGetBegin
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_AdminGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_AdminGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_bos_AdminGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_KeyCreate - add a new key to the keyfile.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN keyVersionNumber - the key version number.
 *
 * IN key -  the new key.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_KeyCreate(const void *serverHandle, int keyVersionNumber,
	      const kas_encryptionKey_p key, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_KeyCreate;
    }

    if (key == NULL) {
	tst = ADMBOSKEYNULL;
	goto fail_bos_KeyCreate;
    }

    tst = BOZO_AddKey(b_handle->server_encrypt, keyVersionNumber, key);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_KeyCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_KeyDelete - delete an existing key from the keyfile.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN keyVersionNumber - the key version number.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_KeyDelete(const void *serverHandle, int keyVersionNumber, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_KeyDelete;
    }

    tst = BOZO_DeleteKey(b_handle->server, keyVersionNumber);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_KeyDelete:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the key retrieval functions
 */

typedef struct key_get {
    int next;
    struct rx_connection *server;
    bos_KeyInfo_t key[CACHED_ITEMS];
} key_get_t, *key_get_p;

static int
GetKeyRPC(void *rpc_specific, int slot, int *last_item,
	  int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    key_get_p key = (key_get_p) rpc_specific;
    struct bozo_keyInfo keyInfo;

    tst =
	BOZO_ListKeys(key->server, key->next++,
		      &key->key[slot].keyVersionNumber, &key->key[slot].key,
		      &keyInfo);


    if (tst == 0) {
	key->key[slot].keyStatus.lastModificationDate = keyInfo.mod_sec;
	key->key[slot].keyStatus.lastModificationMicroSeconds =
	    keyInfo.mod_usec;
	key->key[slot].keyStatus.checkSum = keyInfo.keyCheckSum;
	rc = 1;
    } else if (tst == BZDOM) {
	tst = 0;
	rc = 1;
	*last_item = 1;
	*last_item_contains_data = 0;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetKeyFromCache(void *rpc_specific, int slot, void *dest, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    key_get_p key = (key_get_p) rpc_specific;

    memcpy(dest, &key->key[slot], sizeof(bos_KeyInfo_t));
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_KeyGetBegin - begin iterating over the keys.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * OUT iter - an iterator that can be passed to bos_KeyGetNext
 * to retrieve the keys.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_KeyGetBegin(const void *serverHandle, void **iterationIdP,
		afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    key_get_p key = (key_get_p) malloc(sizeof(key_get_t));

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_KeyGetBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_KeyGetBegin;
    }

    if ((iter == NULL) || (key == NULL)) {
	tst = ADMNOMEM;
	goto fail_bos_KeyGetBegin;
    }

    key->next = 0;
    key->server = b_handle->server_encrypt;

    if (IteratorInit
	(iter, (void *)key, GetKeyRPC, GetKeyFromCache, NULL, NULL, &tst)) {
	*iterationIdP = (void *)iter;
	rc = 1;
    }

  fail_bos_KeyGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (key != NULL) {
	    free(key);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_KeyGetNext - retrieve the next key 
 * from the bos server.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by 
 * bos_KeyGetBegin
 *
 * OUT keyP - upon successful completion contains the next key
 * retrieved from the server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_KeyGetNext(const void *iterationId, bos_KeyInfo_p keyP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_KeyGetNext;
    }

    if (keyP == NULL) {
	tst = ADMBOSKEYPNULL;
	goto fail_bos_KeyGetNext;
    }

    rc = IteratorNext(iter, (void *)keyP, &tst);

  fail_bos_KeyGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_KeyGetDone - finish using a key iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * bos_KeyGetBegin
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_KeyGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_KeyGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_bos_KeyGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_CellSet - set the cell name at a bos server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN cellName - the new cell name.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_CellSet(const void *serverHandle, const char *cellName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_CellSet;
    }

    if ((cellName == NULL) || (*cellName == 0)) {
	tst = ADMCLIENTCELLNAMENULL;
	goto fail_bos_CellSet;
    }

    tst = BOZO_SetCellName(b_handle->server, cellName);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_CellSet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_CellGet - get the cell name at a bos server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * OUT cellName - the cell name.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_CellGet(const void *serverHandle, char *cellName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_CellGet;
    }

    if (cellName == NULL) {
	tst = ADMCLIENTCELLNAMENULL;
	goto fail_bos_CellGet;
    }

    tst = BOZO_GetCellName(b_handle->server, &cellName);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_CellGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_HostCreate - add a new host to the cell.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN hostName - the new host.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_HostCreate(const void *serverHandle, const char *hostName,
	       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_HostCreate;
    }

    if ((hostName == NULL) || (*hostName == 0)) {
	tst = ADMBOSHOSTNAMENULL;
	goto fail_bos_HostCreate;
    }

    tst = BOZO_AddCellHost(b_handle->server, hostName);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_HostCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_HostDelete - delete a host from the cell.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN hostName - the host.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int ADMINAPI
bos_HostDelete(const void *serverHandle, const char *hostName,
	       afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_HostDelete;
    }

    if ((hostName == NULL) || (*hostName == 0)) {
	tst = ADMBOSHOSTNAMENULL;
	goto fail_bos_HostDelete;
    }

    tst = BOZO_DeleteCellHost(b_handle->server, hostName);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_HostDelete:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * The iterator functions and data for the host retrieval functions
 */

typedef struct host_get {
    int next;
    struct rx_connection *server;
    char host[CACHED_ITEMS][BOS_MAX_NAME_LEN];
} host_get_t, *host_get_p;

static int
GetHostRPC(void *rpc_specific, int slot, int *last_item,
	   int *last_item_contains_data, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    host_get_p host = (host_get_p) rpc_specific;
    char *ptr = (char *)&host->host[slot];

    tst = BOZO_GetCellHost(host->server, host->next++, &ptr);

    if (tst == 0) {
	rc = 1;
    } else if (tst == BZDOM) {
	tst = 0;
	rc = 1;
	*last_item = 1;
	*last_item_contains_data = 0;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
GetHostFromCache(void *rpc_specific, int slot, void *dest, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    host_get_p host = (host_get_p) rpc_specific;

    strcpy((char *)dest, (char *)&host->host[slot]);
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_HostGetBegin - begin iterating over the hosts in a cell
 * at a particular bos server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * OUT iter - an iterator that can be passed to bos_HostGetNext
 * to retrieve the process names.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_HostGetBegin(const void *serverHandle, void **iterationIdP,
		 afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_admin_iterator_p iter =
	(afs_admin_iterator_p) malloc(sizeof(afs_admin_iterator_t));
    host_get_p host = (host_get_p) malloc(sizeof(host_get_t));

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_HostGetBegin;
    }

    if (iterationIdP == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_HostGetBegin;
    }

    if ((iter == NULL) || (host == NULL)) {
	tst = ADMNOMEM;
	goto fail_bos_HostGetBegin;
    }

    host->next = 0;
    host->server = b_handle->server;

    if (IteratorInit
	(iter, (void *)host, GetHostRPC, GetHostFromCache, NULL, NULL,
	 &tst)) {
	*iterationIdP = (void *)iter;
	rc = 1;
    }

  fail_bos_HostGetBegin:

    if (rc == 0) {
	if (iter != NULL) {
	    free(iter);
	}
	if (host != NULL) {
	    free(host);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_HostGetNext - retrieve the next host 
 * from the bos server.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by 
 * bos_HostGetBegin
 *
 * OUT hostName - upon successful completion contains the next host
 * retrieved from the server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_HostGetNext(const void *iterationId, char *hostName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_HostGetNext;
    }

    if (hostName == NULL) {
	tst = ADMBOSHOSTNAMENULL;
	goto fail_bos_HostGetNext;
    }

    rc = IteratorNext(iter, (void *)hostName, &tst);

  fail_bos_HostGetNext:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_HostGetDone - finish using a host iterator.
 *
 * PARAMETERS
 *
 * IN iterationId - an iterator previously returned by
 * bos_HostGetBegin
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_HostGetDone(const void *iterationId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_admin_iterator_p iter = (afs_admin_iterator_p) iterationId;

    if (iterationId == NULL) {
	tst = ADMITERATIONIDPNULL;
	goto fail_bos_HostGetDone;
    }

    rc = IteratorDone(iter, &tst);

  fail_bos_HostGetDone:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ExecutableCreate - create a new executable at the bos server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN sourceFile - the executable to install at the bos server.
 *
 * IN destFile - the location where the executable will be installed.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ExecutableCreate(const void *serverHandle, const char *sourceFile,
		     const char *destFile, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    int fd;
    struct stat estat;
    struct rx_call *tcall;

    /*
     * Validate arguments
     */

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ExecutableCreate;
    }

    if ((sourceFile == NULL) || (*sourceFile == 0)) {
	tst = ADMBOSSOURCEFILENULL;
	goto fail_bos_ExecutableCreate;
    }

    if ((destFile == NULL) || (*destFile == 0)) {
	tst = ADMBOSDESTFILENULL;
	goto fail_bos_ExecutableCreate;
    }

    /*
     * Open the file locally and compute its size
     */

    fd = open(sourceFile, O_RDONLY);

    if (fd < 0) {
	tst = ADMBOSCANTOPENSOURCEFILE;
	goto fail_bos_ExecutableCreate;
    }

    if (fstat(fd, &estat)) {
	tst = ADMBOSCANTSTATSOURCEFILE;
	goto fail_bos_ExecutableCreate;
    }

    /*
     * Start a split rpc to the bos server.
     */

    tcall = rx_NewCall(b_handle->server);

    tst =
	StartBOZO_Install(tcall, destFile, estat.st_size,
			  (afs_int32) estat.st_mode, estat.st_mtime);

    if (tst) {
	rx_EndCall(tcall, tst);
	goto fail_bos_ExecutableCreate;
    }

    /*
     * Copy the data to the server
     */

    while (1) {
	char tbuffer[512];
	size_t len;
	len = read(fd, tbuffer, sizeof(tbuffer));
	if (len < 0) {
	    tst = ADMBOSCANTREADSOURCEFILE;
	    rx_EndCall(tcall, len);
	    goto fail_bos_ExecutableCreate;
	}
	if (len == 0) {
	    tst = 0;
	    break;
	}
	tst = rx_Write(tcall, tbuffer, len);
	if (tst != len) {
	    tst = ADMBOSSENDSOURCEFILE;
	    rx_EndCall(tcall, tst);
	    goto fail_bos_ExecutableCreate;
	}
    }

    /*
     * Terminate the rpc to the server
     */

    tst = rx_EndCall(tcall, tst);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ExecutableCreate:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ExecutableRevert - revert an executable to a previous .BAK version.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN execFile - the executable to revert at the bos server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ExecutableRevert(const void *serverHandle, const char *execFile,
		     afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ExecutableRevert;
    }

    if ((execFile == NULL) || (*execFile == 0)) {
	tst = ADMBOSEXECFILENULL;
	goto fail_bos_ExecutableRevert;
    }

    tst = BOZO_UnInstall(b_handle->server, execFile);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ExecutableRevert:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ExecutableTimestampGet - get the last mod times for an executable,
 * the .BAK version of the executable, and the .OLD version of the
 * executable if they exist.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN execFile - the executable to revert at the bos server.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ExecutableTimestampGet(const void *serverHandle, const char *execFile,
			   unsigned long *newTime, unsigned long *oldTime,
			   unsigned long *bakTime, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ExecutableTimestampGet;
    }

    if ((execFile == NULL) || (*execFile == 0)) {
	tst = ADMBOSEXECFILENULL;
	goto fail_bos_ExecutableTimestampGet;
    }

    if (newTime == NULL) {
	tst = ADMBOSNEWTIMENULL;
	goto fail_bos_ExecutableTimestampGet;
    }

    if (oldTime == NULL) {
	tst = ADMBOSOLDTIMENULL;
	goto fail_bos_ExecutableTimestampGet;
    }

    if (bakTime == NULL) {
	tst = ADMBOSBAKTIMENULL;
	goto fail_bos_ExecutableTimestampGet;
    }

    tst =
	BOZO_GetDates(b_handle->server, execFile, newTime, bakTime, oldTime);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ExecutableTimestampGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ExecutablePrune - prune the bak, old, and core files off a server
 * machine.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN oldFiles - prune .OLD files.
 *
 * IN bakFiles - prune .BAK files.
 *
 * IN coreFiles - prune core files.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ExecutablePrune(const void *serverHandle, bos_Prune_t oldFiles,
		    bos_Prune_t bakFiles, bos_Prune_t coreFiles,
		    afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_int32 flags = 0;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ExecutablePrune;
    }

    if (oldFiles == BOS_PRUNE) {
	flags |= BOZO_PRUNEOLD;
    }

    if (bakFiles == BOS_PRUNE) {
	flags |= BOZO_PRUNEBAK;
    }

    if (coreFiles == BOS_PRUNE) {
	flags |= BOZO_PRUNECORE;
    }

    tst = BOZO_Prune(b_handle->server, flags);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ExecutablePrune:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ExecutableRestartTimeSet - set the restart time of the bos server
 * machine.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN type - specifies either weekly restart or daily restart time.
 *
 * IN time - the time to begin restarts.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ExecutableRestartTimeSet(const void *serverHandle, bos_Restart_t type,
			     bos_RestartTime_t time, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_int32 restartType = 0;
    struct ktime restartTime;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ExecutableRestartTimeSet;
    }

    if (type == BOS_RESTART_WEEKLY) {
	restartType = 1;
    } else {
	restartType = 2;
    }

    if ((time.mask & BOS_RESTART_TIME_HOUR)
	&& ((time.hour < 0) || (time.hour > 23))) {
	tst = ADMBOSHOURINVALID;
	goto fail_bos_ExecutableRestartTimeSet;
    }

    if ((time.mask & BOS_RESTART_TIME_MINUTE)
	&& ((time.min < 0) || (time.min > 60))) {
	tst = ADMBOSMINUTEINVALID;
	goto fail_bos_ExecutableRestartTimeSet;
    }

    if ((time.mask & BOS_RESTART_TIME_SECOND)
	&& ((time.sec < 0) || (time.sec > 60))) {
	tst = ADMBOSSECONDINVALID;
	goto fail_bos_ExecutableRestartTimeSet;
    }

    if ((time.mask & BOS_RESTART_TIME_DAY)
	&& ((time.day < 0) || (time.day > 6))) {
	tst = ADMBOSDAYINVALID;
	goto fail_bos_ExecutableRestartTimeSet;
    }

    restartTime.mask = time.mask;
    restartTime.hour = time.hour;
    restartTime.min = time.min;
    restartTime.sec = time.sec;
    restartTime.day = time.day;

    tst = BOZO_SetRestartTime(b_handle->server, restartType, &restartTime);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_ExecutableRestartTimeSet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_ExecutableRestartTimeGet - get the restart time of the bos server
 * machine.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN type - specifies either weekly restart or daily restart time.
 *
 * OUT timeP - the time to begin restarts.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_ExecutableRestartTimeGet(const void *serverHandle, bos_Restart_t type,
			     bos_RestartTime_p timeP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_int32 restartType = 0;
    struct ktime restartTime;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_ExecutableRestartTimeGet;
    }

    if (timeP == NULL) {
	tst = ADMBOSTIMEPNULL;
	goto fail_bos_ExecutableRestartTimeGet;
    }

    if (type == BOS_RESTART_WEEKLY) {
	restartType = 1;
    } else {
	restartType = 2;
    }

    tst = BOZO_GetRestartTime(b_handle->server, restartType, &restartTime);

    if (tst != 0) {
	goto fail_bos_ExecutableRestartTimeGet;
    }

    timeP->mask = restartTime.mask;
    timeP->hour = restartTime.hour;
    timeP->min = restartTime.min;
    timeP->sec = restartTime.sec;
    timeP->day = restartTime.day;
    rc = 1;

  fail_bos_ExecutableRestartTimeGet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_LogGet - get a log file from the bos server machine.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN log - the log file to retrieve.
 *
 * IN/OUT logBufferSizeP - the length of the logData buffer on input,
 * and upon successful completion, the length of data stored in the buffer.
 *
 * OUT logData - the retrieved data upon successful completion.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_LogGet(const void *serverHandle, const char *log,
	   unsigned long *logBufferSizeP, char *logData, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    struct rx_call *tcall = NULL;
    afs_int32 error;
    char buffer;
    int have_call = 0;
    unsigned long bytes_read = 0;

    /*
     * Validate parameters
     */

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_LogGet;
    }

    if ((log == NULL) || (*log == 0)) {
	tst = ADMBOSLOGNULL;
	goto fail_bos_LogGet;
    }

    if (logBufferSizeP == NULL) {
	tst = ADMBOSLOGBUFFERSIZEPNULL;
	goto fail_bos_LogGet;
    }

    if (logData == NULL) {
	tst = ADMBOSLOGDATANULL;
	goto fail_bos_LogGet;
    }

    /*
     * Begin to retrieve the data
     */

    tcall = rx_NewCall(b_handle->server);
    have_call = 1;
    tst = StartBOZO_GetLog(tcall, log);

    if (tst != 0) {
	goto fail_bos_LogGet;
    }

    /*
     * Read the log file data
     */

    while (1) {
	error = rx_Read(tcall, &buffer, 1);
	if (error != 1) {
	    tst = ADMBOSLOGFILEERROR;
	    goto fail_bos_LogGet;
	}

	/*
	 * check for the end of the log
	 */

	if (buffer == 0) {
	    *logBufferSizeP = bytes_read;
	    break;
	}

	/*
	 * We've successfully read another byte, copy it to logData
	 * if there's room
	 */

	bytes_read++;
	if (bytes_read <= *logBufferSizeP) {
	    *logData++ = buffer;
	} else {
	    tst = ADMMOREDATA;
	}
    }
    if (tst == 0) {
	rc = 1;
    }

  fail_bos_LogGet:

    if (have_call) {
	rx_EndCall(tcall, 0);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_AuthSet - set the authorization level required at the bos server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN auth - specifies the new auth level.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_AuthSet(const void *serverHandle, bos_Auth_t auth, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_int32 level = 0;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_AuthSet;
    }

    if (auth == BOS_AUTH_REQUIRED) {
	level = 0;
    } else {
	level = 1;
    }

    tst = BOZO_SetNoAuthFlag(b_handle->server, level);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_AuthSet:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_CommandExecute - execute a command at the bos server.
 *
 * PARAMETERS
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN command - the command to execute.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

int ADMINAPI
bos_CommandExecute(const void *serverHandle, const char *command,
		   afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_CommandExecute;
    }

    if ((command == NULL) || (*command == 0)) {
	tst = ADMBOSCOMMANDNULL;
	goto fail_bos_CommandExecute;
    }

    tst = BOZO_Exec(b_handle->server, command);

    if (tst == 0) {
	rc = 1;
    }

  fail_bos_CommandExecute:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * bos_Salvage - perform a remote salvage operation.
 *
 * PARAMETERS
 *
 * IN cellHandle - a previously opened cellHandle.
 *
 * IN serverHandle - a previously opened serverHandle.
 *
 * IN partitionName - the partition to salvage.  Can be null.
 *
 * IN volumeName - the volume to salvage.  Can be null, if non-null,
 * partitionName cannot be null.
 *
 * IN numSalvagers - the number of salvage processes to run in parallel.
 *
 * IN tmpDir - directory to place temporary files.  Can be null.
 *
 * IN logFile - file where salvage log will be written.  Can be null.
 *
 * IN force - sets salvager -force flag.
 *
 * IN salvageDamagedVolumes - sets salvager -oktozap flag.
 *
 * IN writeInodes - sets salvager -inodes flag.
 *
 * IN writeRootInodes - sets salvager -rootinodes flag.
 *
 * IN forceDirectory - sets salvager -salvagedirs flag.
 *
 * IN forceBlockRead - sets salvager -blockread flag.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 *
 */

#define INITIAL_LOG_LEN 4096

int ADMINAPI
bos_Salvage(const void *cellHandle, const void *serverHandle,
	    const char *partitionName, const char *volumeName,
	    int numSalvagers, const char *tmpDir, const char *logFile,
	    vos_force_t force,
	    bos_SalvageDamagedVolumes_t salvageDamagedVolumes,
	    bos_WriteInodes_t writeInodes,
	    bos_WriteRootInodes_t writeRootInodes,
	    bos_ForceDirectory_t forceDirectory,
	    bos_ForceBlockRead_t forceBlockRead, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    bos_server_p b_handle = (bos_server_p) serverHandle;
    afs_cell_handle_p c_handle = (afs_cell_handle_p) cellHandle;
    int have_partition = 0;
    int have_volume = 0;
    unsigned int part = 0;
    int try_to_stop_fileserver = 0;
    bos_ProcessType_t procType;
    bos_ProcessInfo_t procInfo;
    FILE *log = NULL;
    char command[BOS_MAX_NAME_LEN];
    int command_len = 0;
    int poll_rc;
    char *logData = NULL;
    unsigned long logLen = INITIAL_LOG_LEN;

    /*
     * Validate arguments
     */

    if (!IsValidCellHandle(c_handle, &tst)) {
	goto fail_bos_Salvage;
    }

    if (!isValidServerHandle(b_handle, &tst)) {
	goto fail_bos_Salvage;
    }

    if (c_handle->vos_valid == 0) {
	tst = ADMBOSCELLHANDLENOVOS;
	goto fail_bos_Salvage;
    }

    if ((partitionName != NULL) && (*partitionName != 0)) {
	if (!vos_PartitionNameToId(partitionName, &part, &tst)) {
	    goto fail_bos_Salvage;
	}
	have_partition = 1;
    }

    if ((volumeName != NULL) && (*volumeName != 0)) {
	if (!have_partition) {
	    tst = ADMBOSSALVAGEVOLUME;
	    goto fail_bos_Salvage;
	}
	have_volume = 1;
    }

    if ((logFile != NULL) && (*logFile != 0)) {
	log = fopen(logFile, "w");
	if (!log) {
	    tst = ADMBOSSALVAGEBADLOG;
	    goto fail_bos_Salvage;
	}
    }

    /*
     * If we are salvaging more than a single volume, stop the fileserver
     */

    if (!have_volume) {
	try_to_stop_fileserver = 1;
    }

    /*
     * Only try to stop the fileserver if it is running
     */

    if (try_to_stop_fileserver) {
	if (bos_ProcessInfoGet
	    (serverHandle, "fs", &procType, &procInfo, &tst)) {
	    if (procInfo.processGoal != BOS_PROCESS_RUNNING) {
		try_to_stop_fileserver = 0;
	    }
	}
    }

    /*
     * Make the call to stop the fileserver and wait for it to shutdown
     */

    if (try_to_stop_fileserver) {
	if (!bos_ProcessExecutionStateSetTemporary
	    (serverHandle, "fs", BOS_PROCESS_STOPPED, &tst)) {
	    goto fail_bos_Salvage;
	}
	bos_ProcessAllWaitTransition(serverHandle, &tst);
    }

    /*
     * Create the salvage command line arguments
     */

    command_len =
	sprintf(command, "%s ", AFSDIR_CANONICAL_SERVER_SALVAGER_FILEPATH);
    if (have_partition) {
	command_len +=
	    sprintf(&command[command_len], "-partition %s ", partitionName);
    }

    if (have_volume) {
	command_len +=
	    sprintf(&command[command_len], "-volumeid %s ", volumeName);
    }

    if (salvageDamagedVolumes == BOS_DONT_SALVAGE_DAMAGED_VOLUMES) {
	command_len += sprintf(&command[command_len], "-nowrite ");
    }

    if (writeInodes == BOS_SALVAGE_WRITE_INODES) {
	command_len += sprintf(&command[command_len], "-inodes ");
    }

    if (force == VOS_FORCE) {
	command_len += sprintf(&command[command_len], "-force ");
    }

    if (writeRootInodes == BOS_SALVAGE_WRITE_ROOT_INODES) {
	command_len += sprintf(&command[command_len], "-rootinodes ");
    }

    if (forceDirectory == BOS_SALVAGE_FORCE_DIRECTORIES) {
	command_len += sprintf(&command[command_len], "-salvagedirs ");
    }

    if (forceBlockRead == BOS_SALVAGE_FORCE_BLOCK_READS) {
	command_len += sprintf(&command[command_len], "-blockreads ");
    }

    command_len +=
	sprintf(&command[command_len], "-parallel %d ", numSalvagers);

    if ((tmpDir != NULL) && (*tmpDir != 0)) {
	command_len += sprintf(&command[command_len], "-tmpdir %s ", tmpDir);
    }

    if (command_len > BOS_MAX_NAME_LEN) {
	tst = ADMBOSSALVAGEBADOPTIONS;
	goto fail_bos_Salvage;
    }

    /*
     * Create the process at the bosserver and wait until it completes
     */

    if (!bos_ProcessCreate
	(serverHandle, "salvage-tmp", BOS_PROCESS_CRON, command, "now", 0,
	 &tst)) {
	goto fail_bos_Salvage;
    }

    while ((poll_rc =
	    bos_ProcessInfoGet(serverHandle, "salvage-tmp", &procType,
			       &procInfo, &tst))) {
	sleep(5);
    }

    if (tst != BZNOENT) {
	goto fail_bos_Salvage;
    }

    /*
     * Print out the salvage log if required by the user
     */

    if (log != NULL) {

	logData = (char *)malloc(INITIAL_LOG_LEN);
	if (!logData) {
	    tst = ADMNOMEM;
	    goto fail_bos_Salvage;
	}

	while (!bos_LogGet
	       (serverHandle, AFSDIR_CANONICAL_SERVER_SLVGLOG_FILEPATH,
		&logLen, logData, &tst)) {
	    if (logLen > INITIAL_LOG_LEN) {
		logData = (char *)realloc(logData, (logLen + (logLen / 10)));
		if (logData == NULL) {
		    tst = ADMNOMEM;
		    goto fail_bos_Salvage;
		}
	    } else {
		goto fail_bos_Salvage;
	    }
	}
	fprintf(log, "SalvageLog:\n%s", logData);
    }

    /*
     * Restart the fileserver if we had stopped it previously
     */

    if (try_to_stop_fileserver) {
	try_to_stop_fileserver = 0;
	if (!bos_ProcessExecutionStateSetTemporary
	    (serverHandle, "fs", BOS_PROCESS_RUNNING, &tst)) {
	    goto fail_bos_Salvage;
	}
    }
    rc = 1;

  fail_bos_Salvage:

    if (log != NULL) {
	fclose(log);
    }

    if (logData != NULL) {
	free(logData);
    }

    if (try_to_stop_fileserver) {
	bos_ProcessExecutionStateSetTemporary(serverHandle, "fs",
					      BOS_PROCESS_RUNNING, 0);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}
