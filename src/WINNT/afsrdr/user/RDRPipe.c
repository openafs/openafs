/*
 * Copyright (c) 2008 Secure Endpoints, Inc.
 * Copyright (c) 2009-2011 Your File System, Inc.
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
#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <roken.h>

#include <afs/stds.h>

#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <strsafe.h>

#include <osi.h>

#include "afsd.h"

#include "cm_rpc.h"
#include "afs/afsrpc.h"
#include "afs/auth.h"

#include "msrpc.h"
#define RDR_PIPE_PRIVATE
#include "RDRPipe.h"
#include "smb.h"
#include "cm_nls.h"

static RDR_pipe_t * RDR_allPipes = NULL, *RDR_allPipesLast = NULL;
static osi_rwlock_t  RDR_globalPipeLock;
static wchar_t       RDR_UNCName[64]=L"AFS";

void
RDR_InitPipe(void)
{
    lock_InitializeRWLock(&RDR_globalPipeLock, "RDR global pipe lock", LOCK_HIERARCHY_RDR_GLOBAL);
}

void
RDR_ShutdownPipe(void)
{
    while (RDR_allPipes) {
        RDR_CleanupPipe(RDR_allPipes->index);
    }
    lock_FinalizeRWLock(&RDR_globalPipeLock);
}

RDR_pipe_t *
RDR_FindPipe(ULONG index, int locked)
{
    RDR_pipe_t *pipep;

    if ( !locked)
        lock_ObtainRead(&RDR_globalPipeLock);
    for ( pipep=RDR_allPipes; pipep; pipep=pipep->next) {
        if (pipep->index == index)
            break;
    }
    if ( !locked)
        lock_ReleaseRead(&RDR_globalPipeLock);
    return pipep;
}

/* called to make a fid structure into an Pipe fid structure */
DWORD
RDR_SetupPipe( ULONG index, cm_fid_t *parentFid, cm_fid_t *rootFid,
               WCHAR     *Name, DWORD      NameLength,
               cm_user_t *userp)
{
    RDR_pipe_t *pipep;
    cm_req_t req;
    DWORD status;
    char name[MAX_PATH];
    int newpipe = 0;

    cm_InitReq(&req);

    lock_ObtainWrite(&RDR_globalPipeLock);
    pipep = RDR_FindPipe(index, TRUE);

    if (pipep) {
        /* we are reusing a previous pipe */
        if (cm_FidCmp(&pipep->parentFid, parentFid)) {
            pipep->parentFid = *parentFid;
            if (pipep->parentScp) {
                cm_ReleaseSCache(pipep->parentScp);
                pipep->parentScp = NULL;
            }
            cm_GetSCache(parentFid, NULL, &pipep->parentScp, userp, &req);
            pipep->rootFid = *rootFid;
        }
    } else {
        /* need to allocate a new one */
        newpipe = 1;
        pipep = malloc(sizeof(*pipep));
        if (pipep == NULL) {
            status = STATUS_NO_MEMORY;
            goto done;
        }
        memset(pipep, 0, sizeof(*pipep));
        pipep->index = index;
        if (parentFid->cell == 0) {
            pipep->parentFid = cm_data.rootFid;
            pipep->parentScp = cm_RootSCachep(userp, &req);
            cm_HoldSCache(pipep->parentScp);
        } else {
            pipep->parentFid = *parentFid;
            cm_GetSCache(parentFid, NULL, &pipep->parentScp, userp, &req);
        }
        if (rootFid->cell == 0) {
            pipep->rootFid = cm_data.rootFid;
        } else {
            pipep->rootFid = *rootFid;
        }
    }

    StringCbCopyNW( pipep->name, sizeof(pipep->name), Name, NameLength);
    cm_Utf16ToUtf8( pipep->name, -1, name, sizeof(name));

    status = MSRPC_InitConn(&pipep->rpc_conn, name);
    if (status == STATUS_SUCCESS) {
        pipep->flags |= RDR_PIPEFLAG_MESSAGE_MODE;
        pipep->devstate = RDR_DEVICESTATE_READMSGFROMPIPE |
                          RDR_DEVICESTATE_MESSAGEMODEPIPE |
                          RDR_DEVICESTATE_PIPECLIENTEND;

        if (newpipe) {
            if (RDR_allPipes == NULL)
                RDR_allPipes = RDR_allPipesLast = pipep;
            else {
                pipep->prev = RDR_allPipesLast;
                RDR_allPipesLast->next = pipep;
                RDR_allPipesLast = pipep;
            }
        }
    }
    else
    {
        if (pipep->parentScp)
            cm_ReleaseSCache(pipep->parentScp);

	if (pipep->inAllocp != NULL) {
	    SecureZeroMemory(pipep->inAllocp, pipep->inCopied);
            free(pipep->inAllocp);
	}
	if (pipep->outAllocp != NULL) {
	    SecureZeroMemory(pipep->outAllocp, pipep->outCopied);
            free(pipep->outAllocp);
	}

        if (!newpipe) {
            if (RDR_allPipes == RDR_allPipesLast)
                RDR_allPipes = RDR_allPipesLast = NULL;
            else {
                if (pipep->prev == NULL)
                    RDR_allPipes = pipep->next;
                else
                    pipep->prev->next = pipep->next;
                if (pipep->next == NULL) {
                    RDR_allPipesLast = pipep->prev;
                    pipep->prev->next = NULL;
                } else
                    pipep->next->prev = pipep->prev;
            }
        }
        free(pipep);
    }

  done:
    lock_ReleaseWrite(&RDR_globalPipeLock);

    return status;
}


void
RDR_CleanupPipe(ULONG index)
{
    RDR_pipe_t *pipep;

    lock_ObtainWrite(&RDR_globalPipeLock);
    pipep = RDR_FindPipe(index, TRUE);

    if (pipep) {
        if (pipep->parentScp)
            cm_ReleaseSCache(pipep->parentScp);

	if (pipep->inAllocp != NULL) {
	    SecureZeroMemory(pipep->inAllocp, pipep->inCopied);
            free(pipep->inAllocp);
	}
	if (pipep->outAllocp != NULL) {
	    SecureZeroMemory(pipep->outAllocp, pipep->outCopied);
            free(pipep->outAllocp);
	}

        MSRPC_FreeConn(&pipep->rpc_conn);

        if (RDR_allPipes == RDR_allPipesLast)
            RDR_allPipes = RDR_allPipesLast = NULL;
        else {
            if (pipep->prev == NULL)
                RDR_allPipes = pipep->next;
            else
                pipep->prev->next = pipep->next;
            if (pipep->next == NULL) {
                RDR_allPipesLast = pipep->prev;
                pipep->prev->next = NULL;
            } else
                pipep->next->prev = pipep->prev;
        }
        free(pipep);
    }
    lock_ReleaseWrite(&RDR_globalPipeLock);

}

DWORD
RDR_Pipe_Read(ULONG index, ULONG BufferLength, void *MappedBuffer,
              ULONG *pBytesProcessed, cm_req_t *reqp, cm_user_t *userp)
{
    RDR_pipe_t *pipep;
    DWORD   code = STATUS_INVALID_PIPE_STATE;

    lock_ObtainWrite(&RDR_globalPipeLock);
    pipep = RDR_FindPipe(index, TRUE);

    if (pipep) {
        code = MSRPC_PrepareRead(&pipep->rpc_conn);
        if (code == 0) {
            *pBytesProcessed = MSRPC_ReadMessageLength(&pipep->rpc_conn, BufferLength);
            code = MSRPC_ReadMessage(&pipep->rpc_conn, MappedBuffer, *pBytesProcessed);
        }

    }
    lock_ReleaseWrite(&RDR_globalPipeLock);

    return code;
}

DWORD
RDR_Pipe_Write( ULONG index, ULONG BufferLength, void *MappedBuffer,
                cm_req_t *reqp, cm_user_t *userp)
{
    RDR_pipe_t *pipep;
    DWORD   code = STATUS_INVALID_PIPE_STATE;

    lock_ObtainWrite(&RDR_globalPipeLock);
    pipep = RDR_FindPipe(index, TRUE);

    if (pipep) {
        code = MSRPC_WriteMessage( &pipep->rpc_conn, MappedBuffer,
                                   BufferLength, userp);
    }
    lock_ReleaseWrite(&RDR_globalPipeLock);

    return code;
}

DWORD
RDR_Pipe_QueryInfo( ULONG index, ULONG InfoClass,
                    ULONG BufferLength, void *MappedBuffer,
                    ULONG *pBytesProcessed, cm_req_t *reqp, cm_user_t *userp)
{
    RDR_pipe_t *pipep;
    DWORD   code = STATUS_INVALID_PIPE_STATE;
    size_t      length;

    lock_ObtainWrite(&RDR_globalPipeLock);
    pipep = RDR_FindPipe(index, TRUE);

    if (pipep) {
        switch ( InfoClass ) {
        case FileBasicInformation: {
            FILE_BASIC_INFORMATION * fbInfo = (FILE_BASIC_INFORMATION *)MappedBuffer;

            memset(fbInfo, 0, sizeof(FILE_BASIC_INFORMATION));
            fbInfo->FileAttributes = FILE_ATTRIBUTE_NORMAL;
            *pBytesProcessed = (DWORD)(sizeof(FILE_BASIC_INFORMATION));
            code = STATUS_SUCCESS;
            break;
        }
        case FileStandardInformation: {
            FILE_STANDARD_INFORMATION * fsInfo = (FILE_STANDARD_INFORMATION *)MappedBuffer;

            memset(fsInfo, 0, sizeof(FILE_STANDARD_INFORMATION));
            *pBytesProcessed = (DWORD)(sizeof(FILE_STANDARD_INFORMATION));
            code = STATUS_SUCCESS;
            break;
        }
        case FileNameInformation: {
            FILE_NAME_INFORMATION * fnInfo = (FILE_NAME_INFORMATION *)MappedBuffer;

            StringCbLengthW(pipep->name, sizeof(pipep->name), &length);
            if (BufferLength < sizeof(FILE_NAME_INFORMATION) + length) {
                code = STATUS_BUFFER_OVERFLOW;
                goto done;
            }
            fnInfo->FileNameLength = (DWORD)length;
            memcpy( fnInfo->FileName, pipep->name, length);
            *pBytesProcessed = (DWORD)(sizeof(DWORD) + length);
            code = STATUS_SUCCESS;
            break;
        }
        default:
            code = STATUS_NOT_SUPPORTED;
        }
    }

  done:
    lock_ReleaseWrite(&RDR_globalPipeLock);

    if (code)
        *pBytesProcessed = 0;

    return code;
}

DWORD
RDR_Pipe_SetInfo( ULONG index, ULONG InfoClass,
                  ULONG BufferLength, void *MappedBuffer,
                  cm_req_t *reqp, cm_user_t *userp)
{
    RDR_pipe_t *pipep;
    DWORD   code = STATUS_INVALID_PIPE_STATE;

    lock_ObtainWrite(&RDR_globalPipeLock);
    pipep = RDR_FindPipe(index, TRUE);

    if (pipep) {
        switch ( InfoClass ) {
        case FilePipeInformation: {
            FILE_PIPE_INFORMATION * fpInfo = (FILE_PIPE_INFORMATION *)MappedBuffer;

            if (BufferLength != sizeof(FILE_PIPE_INFORMATION)) {
                code = STATUS_INFO_LENGTH_MISMATCH;
                goto done;
            }

            switch ( fpInfo->ReadMode ) {
            case FILE_PIPE_BYTE_STREAM_MODE:
                pipep->flags &= ~RDR_PIPEFLAG_MESSAGE_MODE;
                break;
            case FILE_PIPE_MESSAGE_MODE:
                pipep->flags |= RDR_PIPEFLAG_MESSAGE_MODE;
                break;
            }
            switch ( fpInfo->CompletionMode ) {
            case FILE_PIPE_QUEUE_OPERATION:
                pipep->flags |= RDR_PIPEFLAG_BLOCKING;
                break;
            case FILE_PIPE_COMPLETE_OPERATION:
                pipep->flags &= ~RDR_PIPEFLAG_BLOCKING;
                break;
            }
            code = STATUS_SUCCESS;
            break;
        }
        default:
            code = STATUS_NOT_SUPPORTED;
        }
    }
  done:
    lock_ReleaseWrite(&RDR_globalPipeLock);

    return code;
}
