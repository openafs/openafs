

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NON_CONFORMING_SWPRINTFS
#define UNICODE 1

#include <ntstatus.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <devioctl.h>

#include <tchar.h>
#include <winbase.h>
#include <winreg.h>
#include <strsafe.h>

#include "..\\Common\\AFSUserCommon.h"

extern "C" {
#include <osilog.h>
extern osi_log_t *afsd_logp;

#include <WINNT/afsreg.h>
#include "../../afsd/cm_config.h"
}
#include <RDRPrototypes.h>

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

DWORD  dwOvEvIdx = 0;

/* returns 0 on success */
extern "C" DWORD
RDR_Initialize(void)
{
    
    DWORD dwRet = 0;
    HKEY parmKey;
    DWORD dummyLen;
    DWORD numSvThreads = CM_CONFIGDEFAULT_SVTHREADS;

    dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (dwRet == ERROR_SUCCESS) {
        dummyLen = sizeof(numSvThreads);
        dwRet = RegQueryValueEx(parmKey, TEXT("ServerThreads"), NULL, NULL,
                                (BYTE *) &numSvThreads, &dummyLen);
        numSvThreads = CM_CONFIGDEFAULT_SVTHREADS;
        RegCloseKey (parmKey);
    }
		
    // Initialize the Thread local storage index for the overlapped i/o 
    // Event Handle
    dwOvEvIdx = TlsAlloc();

    Exit = false;

    RDR_InitIoctl();

    //
    // Launch our workers down to the
    // filters control device for processing requests
    //

    dwRet = RDR_ProcessWorkerThreads(numSvThreads);

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

    rc = DeviceIoControl( hDevice,
                          dwIoControlCode,
                          lpInBuffer,
                          nInBufferSize,
                          lpOutBuffer,
                          nOutBufferSize,
                          NULL,
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

        glWorkerThreadInfo[ glThreadHandleIndex].Flags = 
            (glThreadHandleIndex % 5) ? AFS_REQUEST_RELEASE_THREAD : 0;
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

            if( !RDR_DeviceIoControl( hDevHandle,
                                      IOCTL_AFS_PROCESS_IRP_REQUEST,
                                      (void *)requestBuffer,
                                      sizeof( AFSCommRequest),
                                      (void *)requestBuffer,
                                      sizeof( AFSCommRequest) + AFS_PAYLOAD_BUFFER_SIZE,
                                      &bytesReturned ))
            {

                //
                // Error condition back from driver
                //

                break;
            }

            //
            // Go process the request
            //

            RDR_ProcessRequest( requestBuffer);
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
    BOOL                bRetry = FALSE;

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
                          L"ProcessRequest Processing AFS_REQUEST_TYPE_DIR_ENUM Index %08lX\n",
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
                          L"ProcessRequest Processing AFS_REQUEST_TYPE_EVAL_TARGET_BY_ID Index %08lX\n",
                          RequestBuffer->RequestIndex);
                        
                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }
            //
            // Here is where the specified node is evaluated.
            //

            RDR_EvaluateNodeByID( userp, pEvalTargetCB->ParentId,
                                  RequestBuffer->FileId,
                                  bWow64, bFast,
                                  RequestBuffer->ResultBufferLength,
                                  &pResultCB);
            break;
        }

        case AFS_REQUEST_TYPE_EVAL_TARGET_BY_NAME:
        {
            AFSEvalTargetCB *pEvalTargetCB = (AFSEvalTargetCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer,
                          L"ProcessRequest Processing AFS_REQUEST_TYPE_EVAL_TARGET_BY_NAME Index %08lX\n",
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
                                    bWow64, bFast,
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

                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_CREATE_FILE Index %08lX File %S\n", 
                          RequestBuffer->RequestIndex, wchFileName);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_CreateFileEntry( userp, 
                                 RequestBuffer->Name,
				 RequestBuffer->NameLength,
				 pCreateCB,
                                 bWow64,
				 RequestBuffer->ResultBufferLength,
				 &pResultCB);
    
            break;
        }

        case AFS_REQUEST_TYPE_UPDATE_FILE:
        {

            AFSFileUpdateCB *pUpdateCB = (AFSFileUpdateCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_UPDATE_FILE Index %08lX File %08lX.%08lX.%08lX.%08lX\n", 
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
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_DELETE_FILE Index %08lX %08lX.%08lX.%08lX.%08lX\n", 
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume, 
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_DeleteFileEntry( userp, 
                                 pDeleteCB->ParentId,
                                 RequestBuffer->Name,
				 RequestBuffer->NameLength,
                                 bWow64,
				 RequestBuffer->ResultBufferLength,
				 &pResultCB);

            break;
        }

        case AFS_REQUEST_TYPE_RENAME_FILE:
        {

            AFSFileRenameCB *pFileRenameCB = (AFSFileRenameCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_RENAME_FILE Index %08lX File %08lX.%08lX.%08lX.%08lX NameLength %08lX Name %*S\n", 
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
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS Index %08lX File %08lX.%08lX.%08lX.%08lX %S\n", 
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume, 
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique,
                          BooleanFlagOn( RequestBuffer->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS) ? L"Sync" : L"Async");

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            if (BooleanFlagOn( RequestBuffer->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS))
                RDR_RequestFileExtentsSync( userp, RequestBuffer->FileId,
                                            pFileRequestExtentsCB,
                                            bWow64,
                                            RequestBuffer->ResultBufferLength,
                                            &pResultCB);
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
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS Index %08lX File %08lX.%08lX.%08lX.%08lX\n", 
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
//            AFSFileFlushCB *pFileFlushCB = (AFSFileFlushCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_FLUSH_FILE Index %08lX File %08lX.%08lX.%08lX.%08lX\n", 
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume, 
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_FlushFileEntry( userp, RequestBuffer->FileId,
                                // pFileFlushCB,
                                bWow64,
                                RequestBuffer->ResultBufferLength,
                                &pResultCB);
            break;
        }

        case AFS_REQUEST_TYPE_OPEN_FILE:
        {
            AFSFileOpenCB *pFileOpenCB = (AFSFileOpenCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_OPEN_FILE Index %08lX File %08lX.%08lX.%08lX.%08lX\n", 
                          RequestBuffer->RequestIndex,
                          RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume, 
                          RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            RDR_OpenFileEntry( userp, RequestBuffer->FileId,
                               pFileOpenCB,
                               bWow64,
                               RequestBuffer->ResultBufferLength,
                               &pResultCB);

            break;
        }

        case AFS_REQUEST_TYPE_PIOCTL_OPEN:
        {
            AFSPIOCtlOpenCloseRequestCB *pPioctlCB = (AFSPIOCtlOpenCloseRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

            if (afsd_logp->enabled) {
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIOCTL_OPEN Index %08lX Parent %08lX.%08lX.%08lX.%08lX\n", 
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
                swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIOCTL_WRITE Index %08lX Parent %08lX.%08lX.%08lX.%08lX\n", 
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
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIOCTL_READ Index %08lX Parent %08lX.%08lX.%08lX.%08lX\n", 
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume, 
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_PioctlRead( userp, 
                                RequestBuffer->FileId,
                                pPioctlCB,
                                bWow64,
                                RequestBuffer->ResultBufferLength,
                                &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_PIOCTL_CLOSE:
            {
                AFSPIOCtlOpenCloseRequestCB *pPioctlCB = (AFSPIOCtlOpenCloseRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_PIOCTL_CLOSE Index %08lX Parent %08lX.%08lX.%08lX.%08lX\n", 
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
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_BYTE_RANGE_LOCK Index %08lX File %08lX.%08lX.%08lX.%08lX %S\n", 
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume, 
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique,
                              BooleanFlagOn( RequestBuffer->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS) ? L"Sync" : L"Async");

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                if (BooleanFlagOn( RequestBuffer->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS)) {
                    AFSByteRangeLockRequestCB *pBRLRequestCB = (AFSByteRangeLockRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                    RDR_ByteRangeLockSync( userp, 
                                           RequestBuffer->ProcessId,
                                           RequestBuffer->FileId,
                                           pBRLRequestCB,
                                           bWow64,
                                           RequestBuffer->ResultBufferLength,
                                           &pResultCB);
                } else {
                    AFSAsyncByteRangeLockRequestCB *pBRLRequestCB = (AFSAsyncByteRangeLockRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                    RDR_ByteRangeLockAsync( userp, 
                                            RequestBuffer->ProcessId,
                                            RequestBuffer->FileId,
                                            pBRLRequestCB,
                                            bWow64,
                                            &dwResultBufferLength,
                                            &SetByteRangeLockResultCB );
                }

                break;
            }

    case AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK:
            {
                AFSByteRangeUnlockRequestCB *pBRURequestCB = (AFSByteRangeUnlockRequestCB *)((char *)RequestBuffer->Name + RequestBuffer->DataOffset);

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK Index %08lX File %08lX.%08lX.%08lX.%08lX\n", 
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume, 
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_ByteRangeUnlock( userp, 
                                     RequestBuffer->ProcessId,
                                     RequestBuffer->FileId,
                                     pBRURequestCB,
                                     bWow64,
                                     RequestBuffer->ResultBufferLength,
                                     &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK_ALL:
            {
                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK_ALL Index %08lX File %08lX.%08lX.%08lX.%08lX\n", 
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume, 
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_ByteRangeUnlockAll( userp, 
                                        RequestBuffer->ProcessId,
                                        RequestBuffer->FileId,
                                        bWow64,
                                        RequestBuffer->ResultBufferLength,
                                        &pResultCB);
                break;
            }

    case AFS_REQUEST_TYPE_GET_VOLUME_INFO:
            {
                if (afsd_logp->enabled) {
                    swprintf( wchBuffer, L"ProcessRequest Processing AFS_REQUEST_TYPE_GET_VOLUME_INFO Index %08lX File %08lX.%08lX.%08lX.%08lX\n", 
                              RequestBuffer->RequestIndex,
                              RequestBuffer->FileId.Cell, RequestBuffer->FileId.Volume, 
                              RequestBuffer->FileId.Vnode, RequestBuffer->FileId.Unique);

                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                RDR_GetVolumeInfo( userp, 
                                   RequestBuffer->ProcessId,
                                   RequestBuffer->FileId,
                                   bWow64,
                                   RequestBuffer->ResultBufferLength,
                                   &pResultCB);
                break;
            }

    default:

            break;
    }

    if( BooleanFlagOn( RequestBuffer->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS))
    {
	if (pResultCB == NULL) {
	    /* We failed probably due to a memory allocation error */
	    pResultCB = &stResultCB;
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
                      L"ProcessRequest Responding to Index %08lX Length %08lX\n",
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
                          L"Failed to post IOCTL_AFS_PROCESS_IRP_RESULT gle %X\n", gle);
                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }

            sprintf( pBuffer,
                     "Failed to post IOCTL_AFS_PROCESS_IRP_RESULT gle %X\n",
                     GetLastError());
            osi_panic(pBuffer, __FILE__, __LINE__);
        }

    }
    else if (RequestBuffer->RequestType == AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS) {

        if (afsd_logp->enabled) {
            swprintf( wchBuffer,
                      L"ProcessRequest Responding Asynchronously to REQUEST_FILE_EXTENTS Index %08lX\n",
                      RequestBuffer->RequestIndex);
                        
            osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
        }


        if (SetFileExtentsResultCB) {
        
            if( SetFileExtentsResultCB->ExtentCount != 0 &&
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
                              L"Failed to post IOCTL_AFS_SET_FILE_EXTENTS gle %X\n",
                              gle);
                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                // The file system returns an error when it can't find the FID
                // This is a bug in the file system but we should try to avoid 
                // the crash and clean up our own memory space.  
                //
                // Since we couldn't deliver the extents to the file system
                // we should release them.
                RDR_ReleaseFailedSetFileExtents( userp,
                                                 SetFileExtentsResultCB, 
                                                 dwResultBufferLength);

                if (gle != ERROR_GEN_FAILURE) {
                    sprintf( pBuffer,
                             "Failed to post IOCTL_AFS_SET_FILE_EXTENTS gle %X\n",
                             gle);
                    osi_panic(pBuffer, __FILE__, __LINE__);
                }
            }

            free(SetFileExtentsResultCB);

      } else {
            /* Must be out of memory */
            AFSSetFileExtentsCB SetFileExtentsResultCB;
            
            dwResultBufferLength = sizeof(AFSSetFileExtentsCB);
            memset( &SetFileExtentsResultCB, '\0', dwResultBufferLength );
            SetFileExtentsResultCB.FileId = RequestBuffer->FileId;
            SetFileExtentsResultCB.ResultStatus = STATUS_NO_MEMORY;

            if( !RDR_DeviceIoControl( glDevHandle,
                                      IOCTL_AFS_SET_FILE_EXTENTS,
                                      (void *)&SetFileExtentsResultCB,
                                      dwResultBufferLength,
                                      (void *)NULL,
                                      0,
                                      &bytesReturned ))
            {
                gle = GetLastError();

                if (afsd_logp->enabled) {
                    swprintf( wchBuffer,
                              L"Failed to post IOCTL_AFS_SET_FILE_EXTENTS gle 0x%x\n", gle);
                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                /* we were out of memory - nothing to do */
            }
        }
    } 
    else if (RequestBuffer->RequestType == AFS_REQUEST_TYPE_BYTE_RANGE_LOCK) {

        if (afsd_logp->enabled) {
            swprintf( wchBuffer,
                      L"ProcessRequest Responding Asynchronously to REQUEST_TYPE_BYTE_RANGELOCK Index %08lX\n",
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
                              L"Failed to post IOCTL_AFS_SET_BYTE_RANGE_LOCKS gle 0x%x\n", gle);
                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }


                // TODO - instead of a panic we should release the locks
                sprintf( pBuffer,
                         "Failed to post IOCTL_AFS_SET_BYTE_RANGE_LOCKS gle %X\n", gle);
                osi_panic(pBuffer, __FILE__, __LINE__);
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
                              L"Failed to post IOCTL_AFS_SET_BYTE_RANGE_LOCKS gle 0x%x\n", gle);
                    osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
                }

                /* We were out of memory - nothing to do */
            }
        }
    } 
    else {

        if (afsd_logp->enabled) {
            swprintf( wchBuffer,
                      L"ProcessRequest Not responding to async Index %08lX\n",
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
RDR_RequestExtentRelease(DWORD numOfExtents, LARGE_INTEGER numOfHeldExtents)
{

    HANDLE hDevHandle = NULL;
    DWORD bytesReturned;
    AFSReleaseFileExtentsCB *requestBuffer = NULL;
    AFSReleaseFileExtentsResultCB *responseBuffer = NULL;
    AFSReleaseFileExtentsResultDoneCB * doneBuffer = NULL;
    DWORD responseBufferLen;
    bool bError = false;
    DWORD rc = 0;
    WCHAR wchBuffer[256];
    DWORD gle;

    if (afsd_logp->enabled) {
        swprintf( wchBuffer,
                  L"IOCTL_AFS_RELEASE_FILE_EXTENTS request - number %08lX\n",
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

    requestBuffer = (AFSReleaseFileExtentsCB *)malloc( sizeof( AFSReleaseFileExtentsCB));
    responseBufferLen = (sizeof( AFSReleaseFileExtentsResultCB) + sizeof( AFSReleaseFileExtentsResultFileCB)) * numOfExtents;
    responseBuffer = (AFSReleaseFileExtentsResultCB *)malloc( responseBufferLen);


    if( requestBuffer && responseBuffer)
    {

	memset( requestBuffer, '\0', sizeof( AFSReleaseFileExtentsCB));
	memset( responseBuffer, '\0', responseBufferLen);

        // Set the number of extents to be freed
        // Leave the rest of the structure as zeros to indicate free anything
        requestBuffer->ExtentCount = numOfExtents;

        requestBuffer->HeldExtentCount = numOfHeldExtents;

        if( !RDR_DeviceIoControl( hDevHandle,
                                  IOCTL_AFS_RELEASE_FILE_EXTENTS,
                                  (void *)requestBuffer,
                                  sizeof( AFSReleaseFileExtentsCB),
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
                          L"Failed to post IOCTL_AFS_RELEASE_FILE_EXTENTS - gle 0x%x\n", gle);
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
                      L"IOCTL_AFS_RELEASE_FILE_EXTENTS returns - serial number %08lX flags %lX FileCount %lX\n",
                      responseBuffer->SerialNumber, responseBuffer->Flags, responseBuffer->FileCount);
            osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
        }

        rc = RDR_ProcessReleaseFileExtentsResult( responseBuffer, bytesReturned);

        doneBuffer = (AFSReleaseFileExtentsResultDoneCB *)(void *)requestBuffer;
	memset( requestBuffer, '\0', sizeof( AFSReleaseFileExtentsResultDoneCB));
        doneBuffer->SerialNumber = responseBuffer->SerialNumber;

        if (afsd_logp->enabled) {
            swprintf( wchBuffer,
                      L"IOCTL_AFS_RELEASE_FILE_EXTENTS_DONE - serial number %08lX\n",
                      doneBuffer->SerialNumber);
                        
            osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
        }

        if( !RDR_DeviceIoControl( hDevHandle,
                                  IOCTL_AFS_RELEASE_FILE_EXTENTS_DONE,
                                  (void *)doneBuffer,
                                  sizeof( AFSReleaseFileExtentsResultDoneCB),
                                  NULL,
                                  0,
                                  &bytesReturned ))
        {
            // log the error, nothing to do

            if (afsd_logp->enabled) {
                gle = GetLastError();
                swprintf( wchBuffer,
                          L"Failed to post IOCTL_AFS_RELEASE_FILE_EXTENTS_DONE - serial number 0x%08lX gle 0x%x\n",
                          doneBuffer->SerialNumber, gle);
                osi_Log1(afsd_logp, "%S", osi_LogSaveStringW(afsd_logp, wchBuffer));
            }
        }

        
    } else
        rc = ENOMEM;

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
                  L"IOCTL_AFS_NETWORK_STATUS request - status %d\n",
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
                          L"Failed to post IOCTL_AFS_NETWORK_STATUS gle 0x%x\n",
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
                  L"IOCTL_AFS_VOLUME_STATUS request - cell 0x%x vol 0x%x online %d\n",
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
                          L"Failed to post IOCTL_AFS_VOLUME_STATUS gle 0x%x\n", gle);
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
                  L"IOCTL_AFS_INVALIDATE_CACHE (vol) request - cell 0x%x vol 0x%x reason %d\n",
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
                          L"Failed to post IOCTL_AFS_INVALIDATE_VOLUME gle 0x%x\n", gle);
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
                  L"IOCTL_AFS_INVALIDATE_CACHE (obj) request - cell 0x%x vol 0x%x vn 0x%x uniq 0x%x hash 0x%x type 0x%x reason %d\n",
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
                          L"Failed to post IOCTL_AFS_INVALIDATE_CACHE gle 0x%x\n", gle);
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
                  L"IOCTL_AFS_SYSNAME_NOTIFICATION request - Arch %d Count %d\n",
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
                          L"Failed to post IOCTL_AFS_SYSNAME_NOTIFICATION gle 0x%x\n", gle);
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



