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

extern "C" {
#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>
}

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NON_CONFORMING_SWPRINTFS
#define UNICODE 1
#define STRSAFE_NO_DEPRECATE

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windows.h>
typedef LONG NTSTATUS, *PNTSTATUS;      // not declared in ntstatus.h

#include <roken.h>

#include <devioctl.h>

#include <tchar.h>
#include <winbase.h>
#include <winreg.h>
#include <strsafe.h>

#include "..\\Common\\AFSUserDefines.h"
#include "..\\Common\\AFSUserIoctl.h"
#include "..\\Common\\AFSUserStructs.h"

extern "C" {
#include <osilog.h>
extern osi_log_t *afsd_logp;

#include <WINNT/afsreg.h>
#include <afs/cm_config.h>
#include <afs/cm_error.h>
}
#include <RDRPrototypes.h>

static DWORD
RDR_SetFileStatus2( AFSFileID * pFileId,
                    GUID *pAuthGroup,
                    DWORD dwStatus);

#ifndef FlagOn
#define FlagOn(_F,_SF)        ((_F) & (_SF))
#endif

#ifndef BooleanFlagOn
#define BooleanFlagOn(F,SF)   ((BOOLEAN)(((F) & (SF)) != 0))
#endif

#ifndef SetFlag
#define SetFlag(_F,_SF)       ((_F) |= (_SF))
#endif

#ifndef ClearFlag
#define ClearFlag(_F,_SF)     ((_F) &= ~(_SF))
#endif

#define QuadAlign(Ptr) (                \
    ((((ULONG)(Ptr)) + 7) & 0xfffffff8) \
    )

#define MIN_WORKER_THREADS 5
#define MAX_WORKER_THREADS 512

typedef struct _worker_thread_info {

    HANDLE hThread;

    ULONG  Flags;

    HANDLE hEvent;

} WorkerThreadInfo;

WorkerThreadInfo glWorkerThreadInfo[ MAX_WORKER_THREADS];

UINT   glThreadHandleIndex = 0;

HANDLE glDevHandle = INVALID_HANDLE_VALUE;

static DWORD Exit = false;

static DWORD ExitPending = false;

DWORD  dwOvEvIdx = 0;

extern "C" wchar_t RDR_UNCName[64]=L"AFS";

HANDLE RDR_SuspendEvent = INVALID_HANDLE_VALUE;

/* returns 0 on success */
extern "C" DWORD
RDR_Initialize(void)
{

    DWORD dwRet = 0;
    HKEY parmKey;
    DWORD dummyLen;
    DWORD numSvThreads = CM_CONFIGDEFAULT_SVTHREADS;

    // Initialize the Suspend Event
    RDR_SuspendEvent = CreateEvent( NULL,
                                    TRUE, // manual reset event
                                    TRUE, // signaled
                                    NULL);

    dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (dwRet == ERROR_SUCCESS) {
        dummyLen = sizeof(numSvThreads);
        dwRet = RegQueryValueEx(parmKey, TEXT("ServerThreads"), NULL, NULL,
                                (BYTE *) &numSvThreads, &dummyLen);

        dummyLen = sizeof(RDR_UNCName);
        dwRet = RegQueryValueExW(parmKey, L"NetbiosName", NULL, NULL,
                                 (BYTE *) RDR_UNCName, &dummyLen);

        RegCloseKey (parmKey);
    }

    // Initialize the Thread local storage index for the overlapped i/o
    // Event Handle
    dwOvEvIdx = TlsAlloc();

    Exit = false;

    //
    // Launch our workers down to the
    // filters control device for processing requests
    //

    dwRet = RDR_ProcessWorkerThreads(numSvThreads);

    if (dwRet == ERROR_SUCCESS) {

        RDR_InitIoctl();
        RDR_InitPipe();
    }

    return dwRet;
}

BOOL RDR_DeviceIoControl( HANDLE hDevice,
                          DWORD dwIoControlCode,
                          LPVOID lpInBuffer,
                          DWORD nInBufferSize,
                          LPVOID lpOutBuffer,
                          DWORD nOutBufferSize,
                          LPDWORD lpBytesReturned )
{
    OVERLAPPED ov;
    HANDLE hEvent;
    BOOL rc = FALSE;
    DWORD gle;

    ZeroMemory(&ov, sizeof(OVERLAPPED));

    hEvent = (HANDLE)TlsGetValue(dwOvEvIdx);
    if (hEvent == NULL) {
        hEvent = CreateEvent( NULL, TRUE, TRUE, NULL );
        if (hEvent == INVALID_HANDLE_VALUE || hEvent == NULL)
            return FALSE;
        TlsSetValue( dwOvEvIdx, (LPVOID) hEvent );
    }

    ResetEvent( hEvent);
    ov.hEvent = hEvent;
    *lpBytesReturned = 0;

    rc = DeviceIoControl( hDevice,
                          dwIoControlCode,
                          lpInBuffer,
                          nInBufferSize,
                          lpOutBuffer,
                          nOutBufferSize,
                          lpBytesReturned,
                          &ov );
    if ( !rc ) {
        gle = GetLastError();

        if ( gle == ERROR_IO_PENDING )
            rc = GetOverlappedResult( hDevice, &ov, lpBytesReturned, TRUE );
    }

    return rc;
}

extern "C" DWORD
RDR_ShutdownFinal(void)
{

    DWORD dwIndex = 0;

    Exit = true;

    //
    // Close all the worker thread handles
    //

    while( dwIndex < glThreadHandleIndex)
    {

        CloseHandle( glWorkerThreadInfo[ dwIndex].hThread);

        dwIndex++;
    }

    if( glDevHandle != INVALID_HANDLE_VALUE)
    {

        CloseHandle( glDevHandle);
    }

    return 0;
}

extern "C" DWORD
RDR_ShutdownNotify(void)
{

    HANDLE hDevHandle = NULL;
    DWORD bytesReturned;

    //
    // We use the global handle to the control device instance
    //

    hDevHandle = glDevHandle;


    //
    // First, notify the file system driver that
    // we are shutting down.
    //

    ExitPending = true;

    if( !RDR_DeviceIoControl( hDevHandle,
                              IOCTL_AFS_SHUTDOWN,
                              NULL,
                              0,
                              NULL,
                              0,
                              &bytesReturned ))
    {
        // log the error, nothing to do
    }


    RDR_ShutdownIoctl();
    RDR_ShutdownPipe();

    return 0;
}

//
// Here we launch the worker threads for the given volume
//

DWORD
RDR_ProcessWorkerThreads(DWORD numThreads)
{
    DWORD WorkerID;
    HANDLE hEvent;
    DWORD index = 0;
    DWORD bytesReturned = 0;
    DWORD dwRedirInitInfo;
    AFSRedirectorInitInfo * redirInitInfo = NULL;
    DWORD dwErr;

    if (dwErr = RDR_SetInitParams(&redirInitInfo, &dwRedirInitInfo))
        return dwErr;

    glDevHandle = CreateFile( AFS_SYMLINK_W,
			      GENERIC_READ | GENERIC_WRITE,
			      FILE_SHARE_READ | FILE_SHARE_WRITE,
			      NULL,
			      OPEN_EXISTING,
			      FILE_FLAG_OVERLAPPED,
			      NULL);

    if( glDevHandle == INVALID_HANDLE_VALUE)
    {
        free(redirInitInfo);
        return GetLastError();
    }

    //
    // Now call down to initialize the pool.
    //

    if( !RDR_DeviceIoControl( glDevHandle,
                              IOCTL_AFS_INITIALIZE_CONTROL_DEVICE,
                              NULL,
                              0,
                              NULL,
                              0,
                              &bytesReturned ))
    {

        CloseHandle( glDevHandle);

        glDevHandle = NULL;

        free(redirInitInfo);

        return GetLastError();
    }

    //
    // OK, now launch the workers
    //

    hEvent = CreateEvent( NULL,
			  TRUE,
			  FALSE,
			  NULL);

    //
    // Here we create a pool of worker threads but you can create the pool with as many requests
    // as you want
    //

    if (numThreads < MIN_WORKER_THREADS)
        numThreads = MIN_WORKER_THREADS;
    else if (numThreads > MAX_WORKER_THREADS)
        numThreads = MAX_WORKER_THREADS;

    for (index = 0; index < numThreads; index++)
    {
        //
        // 20% of worker threads should be reserved for release extent
        // event processing
        //
        glWorkerThreadInfo[ glThreadHandleIndex].Flags =
            (glThreadHandleIndex % 5) ? 0 : AFS_REQUEST_RELEASE_THREAD;
        glWorkerThreadInfo[ glThreadHandleIndex].hEvent = hEvent;
        glWorkerThreadInfo[ glThreadHandleIndex].hThread =
            CreateThread( NULL,
                          0,
                          RDR_RequestWorkerThread,
                          (void *)&glWorkerThreadInfo[ glThreadHandleIndex],
                          0,
                          &WorkerID);

        if( glWorkerThreadInfo[ glThreadHandleIndex].hThread != NULL)
        {

            //
            // Wait for the thread to signal it is ready for processing
            //

            WaitForSingleObject( hEvent,
				 INFINITE);

            glThreadHandleIndex++;

            ResetEvent( hEvent);
        }
        else
        {

            //
            // Perform cleanup specific to your application
            //

        }
    }

    if( !RDR_DeviceIoControl( glDevHandle,
                              IOCTL_AFS_INITIALIZE_REDIRECTOR_DEVICE,
                              redirInitInfo,
                              dwRedirInitInfo,
                              NULL,
                              0,
                              &bytesReturned ))
    {

        CloseHandle( glDevHandle);

        glDevHandle = NULL;

        free(redirInitInfo);

        return GetLastError();
    }

    free(redirInitInfo);

    return 0;
}

//
// Entry point for the worker thread
//

DWORD
WINAPI
RDR_RequestWorkerThread( LPVOID lpParameter)
{

    HANDLE hDevHandle = NULL;
    DWORD bytesReturned;
    AFSCommRequest *requestBuffer;
    bool bError = false;
    WorkerThreadInfo * pInfo = (WorkerThreadInfo *)lpParameter;

    //
    // We use the global handle to the control device instance
    //

    hDevHandle = glDevHandle;

    //
    // Allocate a request buffer.
    //

    requestBuffer = (AFSCommRequest *)malloc( sizeof( AFSCommRequest) + AFS_PAYLOAD_BUFFER_SIZE);

    if( requestBuffer)
    {

        //
        // Here we simply signal back to the main thread that we ahve started
        //

        SetEvent( pInfo->hEvent);

        //
        // Process requests until we are told to stop
        //

        while( !Exit)
	{

            memset( requestBuffer, '\0', sizeof( AFSCommRequest) + AFS_PAYLOAD_BUFFER_SIZE);

            requestBuffer->RequestFlags = pInfo->Flags;

            if( RDR_DeviceIoControl( hDevHandle,
                                      IOCTL_AFS_PROCESS_IRP_REQUEST,
                                      (void *)requestBuffer,
                                      sizeof( AFSCommRequest),
                                      (void *)requestBuffer,
                                      sizeof( AFSCommRequest) + AFS_PAYLOAD_BUFFER_SIZE,
                                      &bytesReturned ))
            {

                WaitForSingleObject( RDR_SuspendEvent, INFINITE);

                //
                // Go process the request
                //

                if (!Exit)
                    RDR_ProcessRequest( requestBuffer);
            }
            else
            {

                if (afsd_logp->enabled) {
                    WCHAR wchBuffer[256];
                    DWORD gle = GetLastError();

                    swprintf( wchBuffer,
                              L"Failed to post IOCTL_AFS_IRP_REQUEST gle 0x%x", gle);
                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }
            }
        }

	free( requestBuffer);
    }

    ExitThread( 0);

    return 0;
}

//
// This is the entry point for the worker threads to process the request from the TC Filter driver
//

void
RDR_ProcessRequest( AFSCommRequest *RequestBuffer)
{

    DWORD       	bytesReturned;
    DWORD       	result = 0;
    ULONG       	ulIndex = 0;
    ULONG       	ulCreateFlags = 0;
    AFSCommResult *     pResultCB = NULL;
    AFSCommResult 	stResultCB;
    DWORD       	dwResultBufferLength = 0;
    AFSSetFileExtentsCB * SetFileExtentsResultCB = NULL;
    AFSSetByteRangeLockResultCB *SetByteRangeLockResultCB = NULL;
    WCHAR 		wchBuffer[1024];
    char *pBuffer = (char *)wchBuffer;
    DWORD gle;
    cm_user_t *         userp = NULL;
    BOOL                bWow64 = (RequestBuffer->RequestFlags & AFS_REQUEST_FLAG_WOW64) ? TRUE : FALSE;
    BOOL                bFast  = (RequestBuffer->RequestFlags & AFS_REQUEST_FLAG_FAST_REQUEST) ? TRUE : FALSE;
    BOOL                bHoldFid = (RequestBuffer->RequestFlags & AFS_REQUEST_FLAG_HOLD_FID) ? TRUE : FALSE;
    BOOL                bFlushFile = (RequestBuffer->RequestFlags & AFS_REQUEST_FLAG_FLUSH_FILE) ? TRUE : FALSE;
    BOOL                bDeleteFile = (RequestBuffer->RequestFlags & AFS_REQUEST_FLAG_FILE_DELETED) ? TRUE : FALSE;
    BOOL                bUnlockFile = (RequestBuffer->RequestFlags & AFS_REQUEST_FLAG_BYTE_RANGE_UNLOCK_ALL) ? TRUE : FALSE;
    BOOL                bCheckOnly = (RequestBuffer->RequestFlags & AFS_REQUEST_FLAG_CHECK_ONLY) ? TRUE : FALSE;
    BOOL                bRetry = FALSE;
    BOOL                bUnsupported = FALSE;
    BOOL                bIsLocalSystem = (RequestBuffer->RequestFlags & AFS_REQUEST_LOCAL_SYSTEM_PAG) ? TRUE : FALSE;

    userp = RDR_UserFromCommRequest(RequestBuffer);

  retry:
    //
    // Build up the string to display based on the request type.
    //

    switch( RequestBuffer->RequestType)
    {

        case AFS_REQUEST_TYPE_DIR_ENUM:
        {

            AFSDirQueryCB *pQueryCB = (AFSDirQueryCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer,
                          L"ProcessRequest Processing AFS_REQUEST_TYPE_DIR_ENUM Index %08lX",
                          RequestBuffer->RequestIndex);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            //
            // Here is where the content of the specific directory is enumerated.
            //

            RDR_EnumerateDirectory( userp, RequestBuffer->FileId,
				    pQueryCB, bWow64, bFast,
				    RequestBuffer->ResultBufferLength,
				    &pResultCB);
            break;
        }

        case AFS_REQUEST_TYPE_EVAL_TARGET_BY_ID:
        {
            AFSEvalTargetCB *pEvalTargetCB = (AFSEvalTargetCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer,
                          L"ProcessRequest Processing AFS_REQUEST_TYPE_EVAL_TARGET_BY_ID Index %08lX",
                          RequestBuffer->RequestIndex);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }
            //
            // Here is where the specified node is evaluated.
            //

            RDR_EvaluateNodeByID( userp, pEvalTargetCB->ParentId,
                                  RequestBuffer->FileId,
                                  bWow64, bFast, bHoldFid,
                                  RequestBuffer->ResultBufferLength,
                                  &pResultCB);
            break;
        }

        case AFS_REQUEST_TYPE_EVAL_TARGET_BY_NAME:
        {
            AFSEvalTargetCB *pEvalTargetCB = (AFSEvalTargetCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer,
                          L"ProcessRequest Processing AFS_REQUEST_TYPE_EVAL_TARGET_BY_NAME Index %08lX",
                          RequestBuffer->RequestIndex);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }
            //
            // Here is where the specified node is evaluated.
            //

            RDR_EvaluateNodeByName( userp, pEvalTargetCB->ParentId,
                                    RequestBuffer->Name,
                                    RequestBuffer->NameLength,
                                    RequestBuffer->RequestFlags & AFS_REQUEST_FLAG_CASE_SENSITIVE ? TRUE : FALSE,
                                    bWow64, bFast, bHoldFid,
                                    RequestBuffer->ResultBufferLength,
                                    &pResultCB);
            break;
        }

        case AFS_REQUEST_TYPE_CREATE_FILE:
        {

            AFSFileCreateCB *pCreateCB = (AFSFileCreateCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            WCHAR wchFileName[ 256];

            if (afsd_logp->enabled) {
                memset( wchFileName, '\0', 256 * sizeof( WCHAR));

                memcpy( wchFileName,
                        RequestBuffer->Name,
                        RequestBuffer->NameLength);

                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_CREATE_FILE Index %08lX File %S",
                          RequestBuffer->RequestIndex, wchFileName);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_CreateFileEntry( userp,
                                 RequestBuffer->Name,
				 RequestBuffer->NameLength,
				 pCreateCB,
                                 bWow64,
                                 bHoldFid,
				 RequestBuffer->ResultBufferLength,
				 &pResultCB);

            break;
        }

        case AFS_REQUEST_TYPE_UPDATE_FILE:
        {

            AFSFileUpdateCB *pUpdateCB = (AFSFileUpdateCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_UPDATE_FILE Index %08lX File %08lX.%08lX.%08lX.%08lX",
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_UpdateFileEntry( userp, RequestBuffer->FileId,
				 pUpdateCB,
                                 bWow64,
				 RequestBuffer->ResultBufferLength,
				 &pResultCB);

            break;
        }

        case AFS_REQUEST_TYPE_DELETE_FILE:
        {

            AFSFileDeleteCB *pDeleteCB = (AFSFileDeleteCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_DELETE_FILE Index %08lX %08lX.%08lX.%08lX.%08lX CheckOnly %X",
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique,
                          bCheckOnly);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_DeleteFileEntry( userp,
                                 pDeleteCB->ParentId,
                                 pDeleteCB->ProcessId,
                                 RequestBuffer->Name,
				 RequestBuffer->NameLength,
                                 bWow64,
                                 bCheckOnly,
				 RequestBuffer->ResultBufferLength,
				 &pResultCB);

            break;
        }

        case AFS_REQUEST_TYPE_RENAME_FILE:
        {

            AFSFileRenameCB *pFileRenameCB = (AFSFileRenameCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_RENAME_FILE Index %08lX File %08lX.%08lX.%08lX.%08lX NameLength %08lX Name %*S",
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique,
                          RequestBuffer->NameLength, (int)RequestBuffer->NameLength, RequestBuffer->Name);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_RenameFileEntry( userp,
                                 RequestBuffer->Name,
				 RequestBuffer->NameLength,
                                 RequestBuffer->FileId,
				 pFileRenameCB,
                                 bWow64,
				 RequestBuffer->ResultBufferLength,
				 &pResultCB);

            break;
        }

        case AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS:
        {

            AFSRequestExtentsCB *pFileRequestExtentsCB = (AFSRequestExtentsCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS Index %08lX File %08lX.%08lX.%08lX.%08lX %S",
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique,
                          BooleanFlagOn( RequestBuffer->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS) ? L"Sync" : L"Async");

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            if (BooleanFlagOn( RequestBuffer->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS))
                osi_panic("SYNCHRONOUS AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS not supported",
                          __FILE__, __LINE__);
            else
                bRetry = RDR_RequestFileExtentsAsync( userp, RequestBuffer->FileId,
                                                      pFileRequestExtentsCB,
                                                      bWow64,
                                                      &dwResultBufferLength,
                                                      &SetFileExtentsResultCB );
            break;
        }

        case AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS:
        {

            AFSReleaseExtentsCB *pFileReleaseExtentsCB = (AFSReleaseExtentsCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS Index %08lX File %08lX.%08lX.%08lX.%08lX",
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_ReleaseFileExtents( userp, RequestBuffer->FileId,
				    pFileReleaseExtentsCB,
                                    bWow64,
				    RequestBuffer->ResultBufferLength,
				    &pResultCB);

            break;
        }

        case AFS_REQUEST_TYPE_FLUSH_FILE:
        {
            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_FLUSH_FILE Index %08lX File %08lX.%08lX.%08lX.%08lX",
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_FlushFileEntry( userp, RequestBuffer->FileId,
                                bWow64,
                                RequestBuffer->ResultBufferLength,
                                &pResultCB);
            break;
        }

        case AFS_REQUEST_TYPE_OPEN_FILE:
        {
            AFSFileOpenCB *pFileOpenCB = (AFSFileOpenCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_OPEN_FILE Index %08lX File %08lX.%08lX.%08lX.%08lX",
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_OpenFileEntry( userp, RequestBuffer->FileId,
                               pFileOpenCB,
                               bWow64,
                               bHoldFid,
                               RequestBuffer->ResultBufferLength,
                               &pResultCB);

            break;
        }

        case AFS_REQUEST_TYPE_RELEASE_FILE_ACCESS:
        {
            AFSFileAccessReleaseCB *pFileAccessReleaseCB = (AFSFileAccessReleaseCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_RELEASE_FILE_ACCESS Index %08lX File %08lX.%08lX.%08lX.%08lX",
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_ReleaseFileAccess( userp,
                                   RequestBuffer->FileId,
                                   pFileAccessReleaseCB,
                                   bWow64,
                                   RequestBuffer->ResultBufferLength,
                                   &pResultCB);

            break;
        }

        case AFS_REQUEST_TYPE_PIOCTL_OPEN:
        {
            AFSPIOCtlOpenCloseRequestCB *pPioctlCB = (AFSPIOCtlOpenCloseRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIOCTL_OPEN Index %08lX Parent %08lX.%08lX.%08lX.%08lX",
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_PioctlOpen( userp,
                            RequestBuffer->FileId,
                            pPioctlCB,
                            bWow64,
                            RequestBuffer->ResultBufferLength,
                            &pResultCB);
            break;
        }

        case AFS_REQUEST_TYPE_PIOCTL_WRITE:
        {
            AFSPIOCtlIORequestCB *pPioctlCB = (AFSPIOCtlIORequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIOCTL_WRITE Index %08lX Parent %08lX.%08lX.%08lX.%08lX",
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_PioctlWrite( userp,
                             RequestBuffer->FileId,
                             pPioctlCB,
                             bWow64,
                             RequestBuffer->ResultBufferLength,
                             &pResultCB);
            break;
        }

    case AFS_REQUEST_TYPE_PIOCTL_READ:
            {
                AFSPIOCtlIORequestCB *pPioctlCB = (AFSPIOCtlIORequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIOCTL_READ Index %08lX Parent %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_PioctlRead( userp,
                                RequestBuffer->FileId,
                                pPioctlCB,
                                bWow64,
                                bIsLocalSystem,
                                RequestBuffer->ResultBufferLength,
                                &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_PIOCTL_CLOSE:
            {
                AFSPIOCtlOpenCloseRequestCB *pPioctlCB = (AFSPIOCtlOpenCloseRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIOCTL_CLOSE Index %08lX Parent %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_PioctlClose( userp,
                                RequestBuffer->FileId,
                                pPioctlCB,
                                bWow64,
                                RequestBuffer->ResultBufferLength,
                                &pResultCB);
                break;
            }


    case AFS_REQUEST_TYPE_BYTE_RANGE_LOCK:
            {
                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_BYTE_RANGE_LOCK Index %08lX File %08lX.%08lX.%08lX.%08lX %S",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique,
                              BooleanFlagOn( RequestBuffer->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS) ? L"Sync" : L"Async");

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                AFSByteRangeLockRequestCB *pBRLRequestCB = (AFSByteRangeLockRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                RDR_ByteRangeLockSync( userp,
                                       RequestBuffer->FileId,
                                       pBRLRequestCB,
                                       bWow64,
                                       RequestBuffer->ResultBufferLength,
                                       &pResultCB);

                break;
            }

    case AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK:
            {
                AFSByteRangeUnlockRequestCB *pBRURequestCB = (AFSByteRangeUnlockRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK Index %08lX File %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_ByteRangeUnlock( userp,
                                     RequestBuffer->FileId,
                                     pBRURequestCB,
                                     bWow64,
                                     RequestBuffer->ResultBufferLength,
                                     &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK_ALL:
            {
                AFSByteRangeUnlockRequestCB *pBRURequestCB = (AFSByteRangeUnlockRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK_ALL Index %08lX File %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_ByteRangeUnlockAll( userp,
                                        RequestBuffer->FileId,
                                        pBRURequestCB,
                                        bWow64,
                                        RequestBuffer->ResultBufferLength,
                                        &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_GET_VOLUME_INFO:
            {
                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_GET_VOLUME_INFO Index %08lX File %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_GetVolumeInfo( userp,
                                   RequestBuffer->FileId,
                                   bWow64,
                                   RequestBuffer->ResultBufferLength,
                                   &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_GET_VOLUME_SIZE_INFO:
            {
                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_GET_VOLUME_SIZE_INFO Index %08lX File %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_GetVolumeSizeInfo( userp,
                                       RequestBuffer->FileId,
                                       bWow64,
                                       RequestBuffer->ResultBufferLength,
                                       &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_HOLD_FID:
            {

                AFSHoldFidRequestCB *pHoldFidCB = (AFSHoldFidRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_HOLD_FID Index %08lX",
                              RequestBuffer->RequestIndex);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_HoldFid( userp,
                             pHoldFidCB,
                             bFast,
                             RequestBuffer->ResultBufferLength,
                             &pResultCB);

                break;
            }

    case AFS_REQUEST_TYPE_RELEASE_FID:
            {

                AFSReleaseFidRequestCB *pReleaseFidCB = (AFSReleaseFidRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_RELEASE_FID Index %08lX",
                              RequestBuffer->RequestIndex);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_ReleaseFid( userp,
                                pReleaseFidCB,
                                bFast,
                                RequestBuffer->ResultBufferLength,
                                &pResultCB);

                break;
            }

    case AFS_REQUEST_TYPE_CLEANUP_PROCESSING:
            {

                AFSFileCleanupCB *pCleanupCB = (AFSFileCleanupCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_CLEANUP_FILE Index %08lX File %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_CleanupFileEntry( userp,
                                      RequestBuffer->FileId,
                                      RequestBuffer->Name,
                                      RequestBuffer->NameLength,
                                      pCleanupCB,
                                      bWow64,
                                      bFlushFile,
                                      bDeleteFile,
                                      bUnlockFile,
                                      RequestBuffer->ResultBufferLength,
                                      &pResultCB);

                break;
            }

    case AFS_REQUEST_TYPE_PIPE_OPEN:
            {
                AFSPipeOpenCloseRequestCB *pPipeCB = (AFSPipeOpenCloseRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIPE_OPEN Index %08lX Parent %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_PipeOpen( userp,
                              RequestBuffer->FileId,
                              RequestBuffer->Name,
                              RequestBuffer->NameLength,
                              pPipeCB,
                              bWow64,
                              RequestBuffer->ResultBufferLength,
                              &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_PIPE_WRITE:
            {
                AFSPipeIORequestCB *pPipeCB = (AFSPipeIORequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);
                BYTE *pPipeData = ((BYTE *)RequestBuffer->Name + RequestBuffer->DataOffset + sizeof(AFSPipeIORequestCB));

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIPE_WRITE Index %08lX Parent %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_PipeWrite( userp,
                               RequestBuffer->FileId,
                               pPipeCB,
                               pPipeData,
                               bWow64,
                               RequestBuffer->ResultBufferLength,
                               &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_PIPE_READ:
            {
                AFSPipeIORequestCB *pPipeCB = (AFSPipeIORequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIPE_READ Index %08lX Parent %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_PipeRead( userp,
                              RequestBuffer->FileId,
                              pPipeCB,
                              bWow64,
                              RequestBuffer->ResultBufferLength,
                              &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_PIPE_CLOSE:
            {
                AFSPipeOpenCloseRequestCB *pPipeCB = (AFSPipeOpenCloseRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIPE_CLOSE Index %08lX Parent %08lX.%08lX.%08lX.%08lX",
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume,
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_PipeClose( userp,
                                RequestBuffer->FileId,
                                pPipeCB,
                                bWow64,
                                RequestBuffer->ResultBufferLength,
                                &pResultCB);
                break;
            }


    case AFS_REQUEST_TYPE_PIPE_TRANSCEIVE:
            {
                AFSPipeIORequestCB *pPipeCB = (AFSPipeIORequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);
                BYTE *pPipeData = ((BYTE *)RequestBuffer->Name + RequestBuffer->DataOffset + sizeof(AFSPipeIORequestCB));

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIPE_TRANSCEIVE Index %08lX",
                              RequestBuffer->RequestIndex);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_PipeTransceive( userp,
                                    RequestBuffer->FileId,
                                    pPipeCB,
                                    pPipeData,
                                    bWow64,
                                    RequestBuffer->ResultBufferLength,
                                    &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_PIPE_QUERY_INFO:
            {
                AFSPipeInfoRequestCB *pPipeInfoCB = (AFSPipeInfoRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIPE_QUERY_INFO Index %08lX",
                              RequestBuffer->RequestIndex);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_PipeQueryInfo( userp,
                                   RequestBuffer->FileId,
                                   pPipeInfoCB,
                                   bWow64,
                                   RequestBuffer->ResultBufferLength,
                                   &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_PIPE_SET_INFO:
            {
                AFSPipeInfoRequestCB *pPipeInfoCB = (AFSPipeInfoRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);
                BYTE *pPipeData = ((BYTE *)RequestBuffer->Name + RequestBuffer->DataOffset + sizeof(AFSPipeInfoRequestCB));

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIPE_SET_INFO Index %08lX",
                              RequestBuffer->RequestIndex);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_PipeSetInfo( userp,
                                 RequestBuffer->FileId,
                                 pPipeInfoCB,
                                 pPipeData,
                                 bWow64,
                                 RequestBuffer->ResultBufferLength,
                                 &pResultCB);

                break;
            }

    default:
            bUnsupported = TRUE;

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Received unknown request type %08lX Index %08lX",
                          RequestBuffer->RequestType,
                          RequestBuffer->RequestIndex);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            break;
    }

    if( BooleanFlagOn( RequestBuffer->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS))
    {
	if (pResultCB == NULL) {
	    // We failed probably due to a memory allocation error
            // unless the unsupported flag was set.
	    pResultCB = &stResultCB;
            memset(&stResultCB, 0, sizeof(stResultCB));
            if ( bUnsupported )
                pResultCB->ResultStatus = STATUS_NOT_IMPLEMENTED;
            else
                pResultCB->ResultStatus = STATUS_NO_MEMORY;
	}

        //
        // This is how the filter associates the response information passed in the IOCtl below to the
        // original call. This request index is setup by the filter and should not be modified, otherwise the
        // filter will not be able to locate the request in its internal queue and the blocking thread will
        // not be awakened
        //

        pResultCB->RequestIndex = RequestBuffer->RequestIndex;

        if (afsd_logp->enabled) {
            swprintf( wchBuffer,
                      L"ProcessRequest Responding to Index %08lX Length %08lX",
                      pResultCB->RequestIndex,
                      pResultCB->ResultBufferLength);

            osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
        }

        //
        // Now post the result back to the driver.
        //

        if( !RDR_DeviceIoControl( glDevHandle,
                                  IOCTL_AFS_PROCESS_IRP_RESULT,
                                  (void *)pResultCB,
                                  sizeof( AFSCommResult) + pResultCB->ResultBufferLength,
                                  (void *)NULL,
                                  0,
                                  &bytesReturned ))
        {
            char *pBuffer = (char *)wchBuffer;
            gle = GetLastError();
            if (afsd_logp->enabled) {
                swprintf( wchBuffer,
                          L"Failed to post IOCTL_AFS_PROCESS_IRP_RESULT gle %X", gle);
                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            if (gle != ERROR_NOT_READY) {
                sprintf( pBuffer,
                         "Failed to post IOCTL_AFS_PROCESS_IRP_RESULT gle %X",
                         GetLastError());
                osi_panic(pBuffer, __FILE__, __LINE__);
            }
        }

    }
    else if (RequestBuffer->RequestType == AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS) {

        if (SetFileExtentsResultCB) {

            if (1 || afsd_logp->enabled) {
                if (SetFileExtentsResultCB->ResultStatus != 0)
                    swprintf( wchBuffer,
                          L"ProcessRequest Responding Asynchronously with FAILURE to REQUEST_FILE_EXTENTS Index %08lX Count %08lX Status %08lX",
                          RequestBuffer->RequestIndex, SetFileExtentsResultCB->ExtentCount, SetFileExtentsResultCB->ResultStatus);
                else
                    swprintf( wchBuffer,
                          L"ProcessRequest Responding Asynchronously with SUCCESS to REQUEST_FILE_EXTENTS Index %08lX Count %08lX",
                          RequestBuffer->RequestIndex, SetFileExtentsResultCB->ExtentCount);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            if( (SetFileExtentsResultCB->ExtentCount != 0 ||
                 SetFileExtentsResultCB->ResultStatus != 0) &&
                !RDR_DeviceIoControl( glDevHandle,
                                       IOCTL_AFS_SET_FILE_EXTENTS,
                                      (void *)SetFileExtentsResultCB,
                                      dwResultBufferLength,
                                      (void *)NULL,
                                      0,
                                      &bytesReturned ))
            {
                gle = GetLastError();
                if (afsd_logp->enabled) {
                    swprintf( wchBuffer,
                              L"Failed to post IOCTL_AFS_SET_FILE_EXTENTS gle %X",
                              gle);
                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                // The file system returns an error when it can't find the FID
                // This is a bug in the file system but we should try to avoid
                // the crash and clean up our own memory space.
                //
                // Since we couldn't deliver the extents to the file system
                // we should release them.
                if ( SetFileExtentsResultCB->ExtentCount != 0)
                {
                    RDR_ReleaseFailedSetFileExtents( userp,
                                                 SetFileExtentsResultCB,
                                                 dwResultBufferLength);
                }

                if (gle != ERROR_GEN_FAILURE &&
                    gle != ERROR_NOT_READY) {
                    sprintf( pBuffer,
                             "Failed to post IOCTL_AFS_SET_FILE_EXTENTS gle %X",
                             gle);
                    osi_panic(pBuffer, __FILE__, __LINE__);
                }
            }

            free(SetFileExtentsResultCB);

      } else {
          /* Must be out of memory */
          if (afsd_logp->enabled) {
              swprintf( wchBuffer,
                        L"ProcessRequest Responding Asynchronously STATUS_NO_MEMORY to REQUEST_FILE_EXTENTS Index %08lX",
                        RequestBuffer->RequestIndex);

              osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
          }

          RDR_SetFileStatus2( &RequestBuffer->FileId, &RequestBuffer->AuthGroup, STATUS_NO_MEMORY);
       }
    }
    else if (RequestBuffer->RequestType == AFS_REQUEST_TYPE_BYTE_RANGE_LOCK) {

        if (afsd_logp->enabled) {
            swprintf( wchBuffer,
                      L"ProcessRequest Responding Asynchronously to REQUEST_TYPE_BYTE_RANGELOCK Index %08lX",
                      RequestBuffer->RequestIndex);

            osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
        }


        if (SetByteRangeLockResultCB) {

            if( !RDR_DeviceIoControl( glDevHandle,
                                  IOCTL_AFS_SET_BYTE_RANGE_LOCKS,
                                  (void *)SetByteRangeLockResultCB,
                                  dwResultBufferLength,
                                  (void *)NULL,
                                  0,
                                  &bytesReturned ))
            {
                gle = GetLastError();

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer,
                              L"Failed to post IOCTL_AFS_SET_BYTE_RANGE_LOCKS gle 0x%x", gle);
                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }


                if (gle != ERROR_NOT_READY) {
                    // TODO - instead of a panic we should release the locks
                    sprintf( pBuffer,
                             "Failed to post IOCTL_AFS_SET_BYTE_RANGE_LOCKS gle %X", gle);
                    osi_panic(pBuffer, __FILE__, __LINE__);
                }
            }

            free(SetByteRangeLockResultCB);
        } else {
            /* Must be out of memory */
            AFSSetByteRangeLockResultCB SetByteRangeLockResultCB;

            dwResultBufferLength = sizeof(AFSSetByteRangeLockResultCB);
            memset( &SetByteRangeLockResultCB, '\0', dwResultBufferLength );
            SetByteRangeLockResultCB.FileId = RequestBuffer->FileId;
            SetByteRangeLockResultCB.Result[0].Status = STATUS_NO_MEMORY;

            if( !RDR_DeviceIoControl( glDevHandle,
                                      IOCTL_AFS_SET_BYTE_RANGE_LOCKS,
                                      (void *)&SetByteRangeLockResultCB,
                                      dwResultBufferLength,
                                      (void *)NULL,
                                      0,
                                      &bytesReturned ))
            {
                gle = GetLastError();

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer,
                              L"Failed to post IOCTL_AFS_SET_BYTE_RANGE_LOCKS gle 0x%x", gle);
                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                /* We were out of memory - nothing to do */
            }
        }
    }
    else {

        if (afsd_logp->enabled) {
            swprintf( wchBuffer,
                      L"ProcessRequest Not responding to async Index %08lX",
                      RequestBuffer->RequestIndex);

            osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
        }
    }

    if (bRetry)
        goto retry;

    if (userp)
        RDR_ReleaseUser(userp);


    if( pResultCB && pResultCB != &stResultCB)
    {

	free( pResultCB);
    }
    return;
}


extern "C" DWORD
RDR_SetFileExtents( AFSSetFileExtentsCB *pSetFileExtentsResultCB,
                    DWORD dwResultBufferLength)
{
    WCHAR wchBuffer[1024];
    DWORD bytesReturned;
    DWORD gle;

    if (1 || afsd_logp->enabled) {
        if (pSetFileExtentsResultCB->ResultStatus != 0)
            swprintf( wchBuffer,
               L"RDR_SetFileExtents IOCTL_AFS_SET_FILE_EXTENTS FAILURE Count %08lX Status %08lX",
               pSetFileExtentsResultCB->ExtentCount, pSetFileExtentsResultCB->ResultStatus);
        else
            swprintf( wchBuffer,
               L"RDR_SetFileExtents IOCTL_AFS_SET_FILE_EXTENTS SUCCESS Count %08lX",
               pSetFileExtentsResultCB->ExtentCount);

        osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
    }

    if( !RDR_DeviceIoControl( glDevHandle,
                              IOCTL_AFS_SET_FILE_EXTENTS,
                              (void *)pSetFileExtentsResultCB,
                              dwResultBufferLength,
                              (void *)NULL,
                              0,
                              &bytesReturned ))
    {
        gle = GetLastError();
        return gle;
    }

    return 0;
}


extern "C" DWORD
RDR_SetFileStatus( cm_fid_t *fidp,
                   GUID *pAuthGroup,
                   DWORD dwStatus)
{
    WCHAR               wchBuffer[1024];
    AFSExtentFailureCB  SetFileStatusCB;
    DWORD               bytesReturned;
    DWORD               gle;

    RDR_fid2FID(fidp, &SetFileStatusCB.FileId);
    memcpy(&SetFileStatusCB.AuthGroup, pAuthGroup, sizeof(GUID));
    SetFileStatusCB.FailureStatus = dwStatus;

    if (afsd_logp->enabled) {
        swprintf( wchBuffer, L"RDR_SetFileStatus IOCTL_AFS_EXTENT_FAILURE_CB Fid %08lX.%08lX.%08lX.%08lX Status 0x%lX",
                  SetFileStatusCB.FileId.Cell, SetFileStatusCB.FileId.Volume,
                  SetFileStatusCB.FileId.Vnode, SetFileStatusCB.FileId.Unique,
                  dwStatus);

        osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
    }

    if( !RDR_DeviceIoControl( glDevHandle,
                              IOCTL_AFS_SET_FILE_EXTENT_FAILURE,
                              (void *)&SetFileStatusCB,
                              sizeof(AFSExtentFailureCB),
                              (void *)NULL,
                              0,
                              &bytesReturned ))
    {
        gle = GetLastError();
        return gle;
    }

    return 0;
}

static DWORD
RDR_SetFileStatus2( AFSFileID *pFileId,
                   GUID *pAuthGroup,
                   DWORD dwStatus)
{
    WCHAR               wchBuffer[1024];
    AFSExtentFailureCB  SetFileStatusCB;
    DWORD               bytesReturned;
    DWORD               gle;

    memcpy(&SetFileStatusCB.FileId, pFileId, sizeof(AFSFileID));
    memcpy(&SetFileStatusCB.AuthGroup, pAuthGroup, sizeof(GUID));
    SetFileStatusCB.FailureStatus = dwStatus;

    if (afsd_logp->enabled) {
        swprintf( wchBuffer, L"RDR_SetFileStatus2 IOCTL_AFS_EXTENT_FAILURE_CB Fid %08lX.%08lX.%08lX.%08lX Status 0x%lX",
                  SetFileStatusCB.FileId.Cell, SetFileStatusCB.FileId.Volume,
                  SetFileStatusCB.FileId.Vnode, SetFileStatusCB.FileId.Unique,
                  dwStatus);

        osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
    }

    if( !RDR_DeviceIoControl( glDevHandle,
                              IOCTL_AFS_SET_FILE_EXTENT_FAILURE,
                              (void *)&SetFileStatusCB,
                              sizeof(AFSExtentFailureCB),
                              (void *)NULL,
                              0,
                              &bytesReturned ))
    {
        gle = GetLastError();
        return gle;
    }

    return 0;
}

extern "C" DWORD
RDR_RequestExtentRelease(cm_fid_t *fidp, LARGE_INTEGER numOfHeldExtents, DWORD numOfExtents, AFSFileExtentCB *extentList)
{

    HANDLE hDevHandle = NULL;
    DWORD bytesReturned;
    AFSReleaseFileExtentsCB *requestBuffer = NULL;
    AFSReleaseFileExtentsResultCB *responseBuffer = NULL;
    DWORD requestBufferLen, responseBufferLen;
    bool bError = false;
    DWORD rc = 0;
    WCHAR wchBuffer[256];
    DWORD gle;

    if (ExitPending) {
        if (afsd_logp->enabled) {
            swprintf( wchBuffer,
                      L"IOCTL_AFS_RELEASE_FILE_EXTENTS request ignored due to shutdown pending");

            osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
        }

        OutputDebugString(L"RDR_RequestExtentRequest ignored - shutdown pending\n");
        return CM_ERROR_WOULDBLOCK;
    }

    if (afsd_logp->enabled) {
        swprintf( wchBuffer,
                  L"IOCTL_AFS_RELEASE_FILE_EXTENTS request - number %08lX",
                  numOfExtents);

        osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
    }

    //
    // We use the global handle to the control device instance
    //

    hDevHandle = glDevHandle;

    //
    // Allocate a request buffer.
    //

    requestBufferLen = sizeof( AFSReleaseFileExtentsCB) + sizeof(AFSFileExtentCB) * numOfExtents;
    requestBuffer = (AFSReleaseFileExtentsCB *)malloc( requestBufferLen);
    responseBufferLen = (sizeof( AFSReleaseFileExtentsResultCB) + sizeof( AFSReleaseFileExtentsResultFileCB)) * numOfExtents;
    responseBuffer = (AFSReleaseFileExtentsResultCB *)malloc( responseBufferLen);


    if( requestBuffer && responseBuffer)
    {

	memset( requestBuffer, '\0', sizeof( AFSReleaseFileExtentsCB));
	memset( responseBuffer, '\0', responseBufferLen);

        // If there is a FID provided, use it
        if (fidp && extentList)
        {
            RDR_fid2FID( fidp, &requestBuffer->FileId);

            memcpy(&requestBuffer->FileExtents, extentList, numOfExtents * sizeof(AFSFileExtentCB));

            requestBuffer->Flags = 0;
        } else {

            requestBuffer->Flags = AFS_RELEASE_EXTENTS_FLAGS_RELEASE_ALL;
        }

        // Set the number of extents to be freed
        // Leave the rest of the structure as zeros to indicate free anything
        requestBuffer->ExtentCount = numOfExtents;

        requestBuffer->HeldExtentCount = numOfHeldExtents;

        if( !RDR_DeviceIoControl( hDevHandle,
                                  IOCTL_AFS_RELEASE_FILE_EXTENTS,
                                  (void *)requestBuffer,
                                  requestBufferLen,
                                  (void *)responseBuffer,
                                  responseBufferLen,
                                  &bytesReturned ))
        {
            //
            // Error condition back from driver
            //
            if (afsd_logp->enabled) {
                gle = GetLastError();
                swprintf( wchBuffer,
                          L"Failed to post IOCTL_AFS_RELEASE_FILE_EXTENTS - gle 0x%x", gle);
                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }
            rc = -1;
            goto cleanup;
        }

        //
        // Go process the request
        //

        if (afsd_logp->enabled) {
            swprintf( wchBuffer,
                      L"IOCTL_AFS_RELEASE_FILE_EXTENTS returns - serial number %08lX flags %lX FileCount %lX",
                      responseBuffer->SerialNumber, responseBuffer->Flags, responseBuffer->FileCount);
            osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
        }

        rc = RDR_ProcessReleaseFileExtentsResult( responseBuffer, bytesReturned);
    } else {

        rc = ENOMEM;
    }

  cleanup:
    if (requestBuffer)
        free( requestBuffer);
    if (responseBuffer)
        free( responseBuffer);

    return rc;
}


extern "C" DWORD
RDR_NetworkStatus(BOOLEAN status)
{

    HANDLE hDevHandle = NULL;
    DWORD bytesReturned;
    AFSNetworkStatusCB *requestBuffer = NULL;
    DWORD rc = 0;
    WCHAR wchBuffer[256];
    DWORD gle;

    if (afsd_logp->enabled) {
        swprintf( wchBuffer,
                  L"IOCTL_AFS_NETWORK_STATUS request - status %d",
                  status);

        osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
    }

    //
    // We use the global handle to the control device instance
    //

    hDevHandle = glDevHandle;

    //
    // Allocate a request buffer.
    //

    requestBuffer = (AFSNetworkStatusCB *)malloc( sizeof( AFSNetworkStatusCB));


    if( requestBuffer)
    {

	memset( requestBuffer, '\0', sizeof( AFSNetworkStatusCB));

        // Set the number of extents to be freed
        // Leave the rest of the structure as zeros to indicate free anything
        requestBuffer->Online = status;

        if( !RDR_DeviceIoControl( hDevHandle,
                                  IOCTL_AFS_NETWORK_STATUS,
                                  (void *)requestBuffer,
                                  sizeof( AFSNetworkStatusCB),
                                  NULL,
                                  0,
                                  &bytesReturned ))
        {
            //
            // Error condition back from driver
            //
            if (afsd_logp->enabled) {
                gle = GetLastError();
                swprintf( wchBuffer,
                          L"Failed to post IOCTL_AFS_NETWORK_STATUS gle 0x%x",
                          gle);
                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            rc = -1;
            goto cleanup;
        }
    } else
        rc = ENOMEM;

  cleanup:
    if (requestBuffer)
        free( requestBuffer);

    return rc;
}



extern "C" DWORD
RDR_VolumeStatus(ULONG cellID, ULONG volID, BOOLEAN online)
{

    HANDLE hDevHandle = NULL;
    DWORD bytesReturned;
    AFSVolumeStatusCB *requestBuffer = NULL;
    DWORD rc = 0;
    WCHAR wchBuffer[256];
    DWORD gle;

    if (afsd_logp->enabled) {
        swprintf( wchBuffer,
                  L"IOCTL_AFS_VOLUME_STATUS request - cell 0x%x vol 0x%x online %d",
                  cellID, volID, online);

        osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
    }

    //
    // We use the global handle to the control device instance
    //

    hDevHandle = glDevHandle;

    //
    // Allocate a request buffer.
    //

    requestBuffer = (AFSVolumeStatusCB *)malloc( sizeof( AFSVolumeStatusCB));


    if( requestBuffer)
    {

	memset( requestBuffer, '\0', sizeof( AFSVolumeStatusCB));

        requestBuffer->FileID.Cell = cellID;
        requestBuffer->FileID.Volume = volID;
        requestBuffer->Online = online;

        if( !RDR_DeviceIoControl( hDevHandle,
                                  IOCTL_AFS_VOLUME_STATUS,
                                  (void *)requestBuffer,
                                  sizeof( AFSVolumeStatusCB),
                                  NULL,
                                  0,
                                  &bytesReturned ))
        {
            //
            // Error condition back from driver
            //

            if (afsd_logp->enabled) {
                gle = GetLastError();
                swprintf( wchBuffer,
                          L"Failed to post IOCTL_AFS_VOLUME_STATUS gle 0x%x", gle);
                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            rc = -1;
            goto cleanup;
        }
    } else
        rc = ENOMEM;

  cleanup:
    if (requestBuffer)
        free( requestBuffer);

    return rc;
}

extern "C" DWORD
RDR_NetworkAddrChange(void)
{
    return 0;
}


extern "C" DWORD
RDR_InvalidateVolume(ULONG cellID, ULONG volID, ULONG reason)
{

    HANDLE hDevHandle = NULL;
    DWORD bytesReturned;
    AFSInvalidateCacheCB *requestBuffer = NULL;
    DWORD rc = 0;
    WCHAR wchBuffer[256];
    DWORD gle;

    if (afsd_logp->enabled) {
        swprintf( wchBuffer,
                  L"IOCTL_AFS_INVALIDATE_CACHE (vol) request - cell 0x%x vol 0x%x reason %d",
                  cellID, volID, reason);

        osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
    }

    //
    // We use the global handle to the control device instance
    //

    hDevHandle = glDevHandle;

    //
    // Allocate a request buffer.
    //

    requestBuffer = (AFSInvalidateCacheCB *)malloc( sizeof( AFSInvalidateCacheCB));


    if( requestBuffer)
    {

	memset( requestBuffer, '\0', sizeof( AFSInvalidateCacheCB));

        requestBuffer->FileID.Cell = cellID;
        requestBuffer->FileID.Volume = volID;
        requestBuffer->WholeVolume = TRUE;
        requestBuffer->Reason = reason;

        if( !RDR_DeviceIoControl( hDevHandle,
                                  IOCTL_AFS_INVALIDATE_CACHE,
                                  (void *)requestBuffer,
                                  sizeof( AFSInvalidateCacheCB),
                                  NULL,
                                  0,
                                  &bytesReturned ))
        {
            //
            // Error condition back from driver
            //

            if (afsd_logp->enabled) {
                gle = GetLastError();
                swprintf( wchBuffer,
                          L"Failed to post IOCTL_AFS_INVALIDATE_VOLUME gle 0x%x", gle);
                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            rc = -1;
            goto cleanup;
        }
    } else
        rc = ENOMEM;

  cleanup:
    if (requestBuffer)
        free( requestBuffer);

    return rc;
}


extern "C" DWORD
RDR_InvalidateObject(ULONG cellID, ULONG volID, ULONG vnode, ULONG uniq, ULONG hash, ULONG fileType, ULONG reason)
{

    HANDLE hDevHandle = NULL;
    DWORD bytesReturned;
    AFSInvalidateCacheCB *requestBuffer = NULL;
    DWORD rc = 0;
    WCHAR wchBuffer[256];
    DWORD gle;

    if (afsd_logp->enabled) {
        swprintf( wchBuffer,
                  L"IOCTL_AFS_INVALIDATE_CACHE (obj) request - cell 0x%x vol 0x%x vn 0x%x uniq 0x%x hash 0x%x type 0x%x reason %d",
                  cellID, volID, vnode, uniq, hash, fileType, reason);

        osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
    }

    //
    // We use the global handle to the control device instance
    //

    hDevHandle = glDevHandle;

    //
    // Allocate a request buffer.
    //

    requestBuffer = (AFSInvalidateCacheCB *)malloc( sizeof( AFSInvalidateCacheCB));


    if( requestBuffer)
    {

	memset( requestBuffer, '\0', sizeof( AFSInvalidateCacheCB));

        requestBuffer->FileID.Cell = cellID;
        requestBuffer->FileID.Volume = volID;
        requestBuffer->FileID.Vnode = vnode;
        requestBuffer->FileID.Unique = uniq;
        requestBuffer->FileID.Hash = hash;
        requestBuffer->FileType = fileType;
        requestBuffer->WholeVolume = FALSE;
        requestBuffer->Reason = reason;

        if( !RDR_DeviceIoControl( hDevHandle,
                                  IOCTL_AFS_INVALIDATE_CACHE,
                                  (void *)requestBuffer,
                                  sizeof( AFSInvalidateCacheCB),
                                  NULL,
                                  0,
                                  &bytesReturned ))
        {
            //
            // Error condition back from driver
            //
            if (afsd_logp->enabled) {
                gle = GetLastError();
                swprintf( wchBuffer,
                          L"Failed to post IOCTL_AFS_INVALIDATE_CACHE gle 0x%x", gle);
                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            rc = -1;
            goto cleanup;
        }
    } else
        rc = ENOMEM;

  cleanup:
    if (requestBuffer)
        free( requestBuffer);

    return rc;
}



extern "C" DWORD
RDR_SysName(ULONG Architecture, ULONG Count, WCHAR **NameList)
{

    HANDLE hDevHandle = NULL;
    DWORD bytesReturned;
    AFSSysNameNotificationCB *requestBuffer = NULL;
    DWORD rc = 0;
    WCHAR wchBuffer[256];
    DWORD Length;
    DWORD gle;

    if (afsd_logp->enabled) {
        swprintf( wchBuffer,
                  L"IOCTL_AFS_SYSNAME_NOTIFICATION request - Arch %d Count %d",
                  Architecture, Count);

        osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
    }

    if (Count <= 0 || NameList == NULL)
        return -1;

    //
    // We use the global handle to the control device instance
    //

    hDevHandle = glDevHandle;

    //
    // Allocate a request buffer.
    //

    Length = sizeof (AFSSysNameNotificationCB) + (Count - 1) * sizeof (AFSSysName);
    requestBuffer = (AFSSysNameNotificationCB *)malloc( Length );


    if( requestBuffer)
    {
        unsigned int i;

	memset( requestBuffer, '\0', Length);

        requestBuffer->Architecture = Architecture;
        requestBuffer->NumberOfNames = Count;
        for ( i=0 ; i<Count; i++) {
            size_t len = wcslen(NameList[i]);
            requestBuffer->SysNames[i].Length = (ULONG) (len * sizeof(WCHAR));
            StringCchCopyNW(requestBuffer->SysNames[i].String, AFS_MAX_SYSNAME_LENGTH,
                            NameList[i], len);
        }

        if( !RDR_DeviceIoControl( hDevHandle,
                                  IOCTL_AFS_SYSNAME_NOTIFICATION,
                                  (void *)requestBuffer,
                                  Length,
                                  NULL,
                                  0,
                                  &bytesReturned ))
        {
            //
            // Error condition back from driver
            //
            if (afsd_logp->enabled) {
                gle = GetLastError();
                swprintf( wchBuffer,
                          L"Failed to post IOCTL_AFS_SYSNAME_NOTIFICATION gle 0x%x", gle);
                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            rc = -1;
            goto cleanup;
        }
    } else
        rc = ENOMEM;

  cleanup:
    if (requestBuffer)
        free( requestBuffer);

    return rc;
}

extern "C" VOID
RDR_Suspend( VOID)
{
    ResetEvent( RDR_SuspendEvent);
}

extern "C" VOID
RDR_Resume( VOID)
{
    SetEvent( RDR_SuspendEvent);
}
