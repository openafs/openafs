/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <rpc.h>
#include <malloc.h>

#include <osi.h>
#include "afsrpc.h"

#include "afsd.h"
#include "afsd_init.h"

#include "smb.h"

#include <rx/rxkad.h>

#include "cm_rpc.h"

/*
 * The motivation for this whole module is that in transmitting tokens
 * between applications and the AFS service, we must not send session keys
 * in the clear.  So the SetToken and GetToken pioctl's also do an RPC using
 * packet privacy to transmit the session key.  The pioctl() generates a UUID
 * and sends it down, and the RPC sends down the same UUID, so that the service
 * can match them up.  A list of session keys, searched by UUID, is maintained.
 */

extern void afsi_log(char *pattern, ...);

token_event_u *tokenEvents = NULL;

osi_mutex_t tokenEventLock;

EVENT_HANDLE rpc_ShutdownEvent = NULL;


/*
 * Add a new uuid and session key to the list.  Old Style.
 */
void cm_RegisterNewTokenEvent(
	afs_uuid_t uuid,
	char sessionKey[8])
{
    token_event_u nte[1];

    nte->tag = CM_TOKEN_KAD;
    nte->uuid = uuid;
    memcpy(nte->sessionKey, sessionKey, sizeof(nte->sessionKey));

    cm_RegisterNewTokenEvent2(nte);
}


/*
 * Add a new uuid and token to the list.  New style.
 */
void cm_RegisterNewTokenEvent2(token_event_u *nte)
{
    token_event_u *te = malloc(sizeof(token_event_u));
    memcpy(te, nte, sizeof(token_event_u));
    lock_ObtainMutex(&tokenEventLock);
    te->next = tokenEvents;
    tokenEvents = te;
    lock_ReleaseMutex(&tokenEventLock);
}

/*
 * Find a uuid on the list.  Old style.
 *
 * Return TRUE if found, FALSE if not found
 */
BOOL cm_FindTokenEvent(afs_uuid_t uuid, char sessionKey[8])
{
    BOOL status;
    token_event_u te[1];

    te->tag = CM_TOKEN_KAD;
    te->uuid = uuid;

    status = cm_FindTokenEvent2(te);

    if(status)
        memcpy(sessionKey, te->sessionKey, sizeof(sessionKey));

    return status;
}

/*
 * Find a uuid on the list.  New style.  If it is there, copy the token and
 * destroy the entry, since it is only used once.
 *
 * Return TRUE if found, FALSE if not found
 */
BOOL cm_FindTokenEvent2(token_event_u fte[1])
{
    RPC_STATUS status;
    token_event_u *te;
    token_event_u **ltep;

    lock_ObtainMutex(&tokenEventLock);
    te = tokenEvents;
    ltep = &tokenEvents;
    while (te) {
        /* looking for a token with same uuid and same type tag */
        if (UuidEqual((UUID *)&(fte->uuid), (UUID *)&te->uuid, &status) &&
            (fte->tag == te->tag) ) {
            *ltep = te->next;
            lock_ReleaseMutex(&tokenEventLock);
            memcpy(fte, te, sizeof(token_event_u));
            free(te);
            return TRUE;
        }
        ltep = &te->next;
        te = te->next;
    }
    lock_ReleaseMutex(&tokenEventLock);
    return FALSE;
}

/*
 * RPC manager entry point vector functions
 */

long AFSRPC_SetToken(
	afs_uuid_t uuid,
	unsigned char __RPC_FAR sessionKey[8])
{
    token_event_u te[1];

    te->tag = CM_TOKEN_KAD;
    te->uuid = uuid;
    memcpy(te->sessionKey, sessionKey, sizeof(te->sessionKey));

    cm_RegisterNewTokenEvent2(te);

    return 0;
}

long AFSRPC_GetToken(
	afs_uuid_t uuid,
	unsigned char __RPC_FAR sessionKey[8])
{
    BOOL found;
    token_event_u te[1];

    te->tag = CM_TOKEN_KAD;
    te->uuid = uuid;
    memcpy(te->sessionKey, sessionKey, sizeof(te->sessionKey));

    found = cm_FindTokenEvent2(te);
    if (!found)
        return 1;

    /* copy out sessionKey */
    memcpy(sessionKey, te->sessionKey, sizeof(sessionKey));

    return 0;
}

long AFSRPC_SetToken2(
	afs_uuid_t uuid,
	afs_token_wrapper_t __RPC_FAR wrapped_token[1])
{
    token_event_u te[1];

    te->tag = CM_TOKEN_K5PLUS;
    te->uuid = uuid;
    memcpy(te->wrapped_token, wrapped_token, sizeof(te->wrapped_token));

    cm_RegisterNewTokenEvent2(te);

    return 0;
}

long AFSRPC_GetToken2(
	afs_uuid_t uuid,
	afs_token_wrapper_t __RPC_FAR wrapped_token[1])
{
    BOOL found;
    token_event_u te[1];

    te->tag = CM_TOKEN_K5PLUS;
    te->uuid = uuid;
    memcpy(te->wrapped_token, wrapped_token, sizeof(te->wrapped_token));

    found = cm_FindTokenEvent2(te);
    if (!found)
        return 1;

    /* copy out token (watch out for sizeof) */
    memcpy(wrapped_token, te->wrapped_token, sizeof(te->wrapped_token));

    return 0;
}

void __RPC_FAR * __RPC_USER midl_user_allocate (size_t cBytes)
{
    return ((void __RPC_FAR *) malloc(cBytes));
}

void __RPC_USER midl_user_free(void __RPC_FAR * p)
{
    free(p);
} 

void RpcListen()
{
    RPC_STATUS		status;
    char			*task;
    RPC_BINDING_VECTOR	*ptrBindingVector = NULL;
    BOOLEAN			ifaceRegistered = FALSE;
    BOOLEAN			epRegistered = FALSE;

#ifdef	NOOSIDEBUGSERVER	/* Use All Protseqs already done in OSI */

    status = RpcServerUseAllProtseqs(1, NULL);
    if (status != RPC_S_OK) {
        task = "Use All Protocol Sequences";
        goto cleanup;
    }

#endif	/* NOOSIDEBUGSERVER */

    status = RpcServerRegisterIf(afsrpc_v1_0_s_ifspec, NULL, NULL);
    if (status != RPC_S_OK) {
        task = "Register Interface";
        goto cleanup;
    }
    ifaceRegistered = TRUE;

    status = RpcServerInqBindings(&ptrBindingVector);
    if (status != RPC_S_OK) {
        task = "Inquire Bindings";
        goto cleanup;
    }

    status = RpcServerRegisterAuthInfo(NULL, RPC_C_AUTHN_WINNT, NULL, NULL);
    if (status != RPC_S_OK) {
        task = "Register Authentication Info";
        goto cleanup;
    }

    status = RpcEpRegister(afsrpc_v1_0_s_ifspec, ptrBindingVector,
                            NULL, "AFS session key interface");
    if (status != RPC_S_OK) {
        task = "Register Endpoints";
        goto cleanup;
    }
    epRegistered = TRUE;

    afsi_log("RPC server listening");

    status = RpcServerListen(OSI_MAXRPCCALLS, OSI_MAXRPCCALLS, 0);
    if (status != RPC_S_OK) {
        task = "Server Listen";
    }

cleanup:
    if (epRegistered)
        (void) RpcEpUnregister(afsrpc_v1_0_s_ifspec, ptrBindingVector,
                                NULL);

    if (ptrBindingVector)
        (void) RpcBindingVectorFree(&ptrBindingVector);

    if (ifaceRegistered)
        (void) RpcServerUnregisterIf(afsrpc_v1_0_s_ifspec, NULL, FALSE);

    if (status != RPC_S_OK)
        afsi_log("RPC problem, code %d for %s", status, task);
    else
        afsi_log("RPC shutdown");

    if (rpc_ShutdownEvent != NULL)
        thrd_SetEvent(rpc_ShutdownEvent);
    return;
}

long RpcInit()
{
    LONG status = ERROR_SUCCESS;
    HANDLE listenThread;
    ULONG listenThreadID = 0;
    char * name = "afsd_rpc_ShutdownEvent";

    lock_InitializeMutex(&tokenEventLock, "token event lock",
                          LOCK_HIERARCHY_TOKEN_EVENT_GLOBAL);

    rpc_ShutdownEvent = thrd_CreateEvent(NULL, FALSE, FALSE, name);
    if ( GetLastError() == ERROR_ALREADY_EXISTS )
        afsi_log("Event Object Already Exists: %s", name);

    listenThread = CreateThread(NULL, 0, (PTHREAD_START_ROUTINE)RpcListen,
                                0, 0, &listenThreadID);

    if (listenThread == NULL) {
        status = GetLastError();
    }
    CloseHandle(listenThread);

    return status;
}

void RpcShutdown(void)
{
    RpcMgmtStopServerListening(NULL);

    if (rpc_ShutdownEvent != NULL) {
        thrd_WaitForSingleObject_Event(rpc_ShutdownEvent, INFINITE);
        CloseHandle(rpc_ShutdownEvent);
    }
}

