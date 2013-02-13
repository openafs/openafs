/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice,
 *   this list of conditions and the following disclaimer in the
 *   documentation
 *   and/or other materials provided with the distribution.
 * - Neither the names of Kernel Drivers, LLC and Your File System, Inc.
 *   nor the names of their contributors may be used to endorse or promote
 *   products derived from this software without specific prior written
 *   permission from Kernel Drivers, LLC and Your File System, Inc.
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

//
// File: AFSCommSupport.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSReleaseFid( IN AFSFileID *FileId)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_RELEASE_FID,
                                      0,
                                      NULL,
                                      NULL,
                                      FileId,
                                      NULL,
                                      0,
                                      NULL,
                                      0,
                                      NULL,
                                      NULL);
    }

    return ntStatus;
}

NTSTATUS
AFSProcessRequest( IN ULONG RequestType,
                   IN ULONG RequestFlags,
                   IN GUID *AuthGroup,
                   IN PUNICODE_STRING FileName,
                   IN AFSFileID *FileId,
                   IN WCHAR *Cell,
                   IN ULONG  CellLength,
                   IN void  *Data,
                   IN ULONG DataLength,
                   IN OUT void *ResultBuffer,
                   IN OUT PULONG ResultBufferLength)
{

    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSPoolEntry     stPoolEntry, *pPoolEntry = NULL;
    AFSCommSrvcCB   *pCommSrvc = NULL;
    BOOLEAN          bReleasePool = FALSE;
    AFSDeviceExt    *pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSDeviceExt    *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    BOOLEAN          bWait = BooleanFlagOn( RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS);
    ULONG            ulPoolEntryLength = 0;
    BOOLEAN          bDecrementCount = FALSE;

    __try
    {

        if( BooleanFlagOn( pRDRDevExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
        {
            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        if( InterlockedIncrement( &pControlDevExt->Specific.Control.OutstandingServiceRequestCount) == 1)
        {
            KeClearEvent( &pControlDevExt->Specific.Control.OutstandingServiceRequestEvent);
        }

        bDecrementCount = TRUE;

        pCommSrvc = &pControlDevExt->Specific.Control.CommServiceCB;

        //
        // Grab the pool resource and check the state
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessRequest Acquiring IrpPoolLock lock %p EXCL %08lX\n",
                      &pCommSrvc->IrpPoolLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                        TRUE);

        bReleasePool = TRUE;

        if( pCommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
        {

            //
            // Pool not running so bail.
            //

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        //
        // If this is an async request we need to allocate a pool entry for the request
        //

        pPoolEntry = &stPoolEntry;

        if( !bWait)
        {

            ASSERT( ResultBuffer == NULL);

            ulPoolEntryLength = sizeof( AFSPoolEntry) + QuadAlign( DataLength);

            if( FileName != NULL)
            {

                ulPoolEntryLength += FileName->Length;
            }

            pPoolEntry = (AFSPoolEntry *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                                   ulPoolEntryLength,
                                                                   AFS_POOL_ENTRY_TAG);

            if( pPoolEntry == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlZeroMemory( pPoolEntry,
                           ulPoolEntryLength);

            pPoolEntry->Data = (void *)((char *)pPoolEntry + sizeof( AFSPoolEntry));

            pPoolEntry->FileName.Buffer = (WCHAR *)((char *)pPoolEntry->Data + DataLength);
        }
        else
        {

            RtlZeroMemory( pPoolEntry,
                           sizeof( AFSPoolEntry));

            KeInitializeEvent( &pPoolEntry->Event,
                               NotificationEvent,
                               FALSE);
        }

        pPoolEntry->RequestType = RequestType;

        pPoolEntry->RequestIndex = pCommSrvc->IrpPoolRequestIndex++;

        pPoolEntry->RequestFlags = RequestFlags;

        pPoolEntry->ResultBufferLength = 0;

        if( FileId != NULL)
        {

            pPoolEntry->FileId = *FileId;
        }

        pPoolEntry->FileName.Length = 0;

        if( FileName != NULL)
        {

            if( bWait)
            {

                pPoolEntry->FileName = *FileName;
            }
            else
            {

                pPoolEntry->FileName.Length = FileName->Length;

                pPoolEntry->FileName.MaximumLength = pPoolEntry->FileName.Length;

                RtlCopyMemory( pPoolEntry->FileName.Buffer,
                               FileName->Buffer,
                               pPoolEntry->FileName.Length);
            }
        }

        //
        // Move in the data if there is some
        //

        pPoolEntry->DataLength = DataLength;

        if( Data != NULL &&
            DataLength > 0)
        {

            if( bWait)
            {

                pPoolEntry->Data = Data;
            }
            else
            {

                RtlCopyMemory( pPoolEntry->Data,
                               Data,
                               DataLength);
            }
        }

        pPoolEntry->ResultBuffer = ResultBuffer;

        pPoolEntry->ResultBufferLength = ResultBufferLength;

        //
        // Store off the auth group
        //

        if( AuthGroup == NULL)
        {
            AFSRetrieveAuthGroup( (ULONGLONG)PsGetCurrentProcessId(),
                                  (ULONGLONG)PsGetCurrentThreadId(),
                                  &pPoolEntry->AuthGroup);
        }
        else
        {
            RtlCopyMemory( &pPoolEntry->AuthGroup,
                           AuthGroup,
                           sizeof( GUID));
        }

        if( AFSIsLocalSystemAuthGroup( &pPoolEntry->AuthGroup))
        {
            SetFlag( pPoolEntry->RequestFlags, AFS_REQUEST_LOCAL_SYSTEM_PAG);
        }

        if( AFSIsNoPAGAuthGroup( &pPoolEntry->AuthGroup))
        {
            AFSDbgLogMsg( 0,
                          0,
                          "AFSProcessRequest NoPAG Auth Group %08lX\n",
                          PsGetCurrentThread());
        }

        //
        // Indicate the type of process
        //

#ifdef AMD64

        if( !AFSIs64BitProcess( (ULONGLONG)PsGetCurrentProcessId()))
        {
            SetFlag( pPoolEntry->RequestFlags, AFS_REQUEST_FLAG_WOW64);
        }

#endif

        //
        // Insert the entry into the request pool
        //

        ntStatus = AFSInsertRequest( pCommSrvc,
                                     pPoolEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            if( !bWait)
            {

                ExFreePool( pPoolEntry);
            }

            try_return( ntStatus);
        }

        //
        // Drop the lock on the pool prior to waiting
        //

        AFSReleaseResource( &pCommSrvc->IrpPoolLock);

        bReleasePool = FALSE;

        //
        // Wait for the result if this is NOT an asynchronous request
        //

        if( bWait)
        {

            //
            // Wait for the result of the request. We specify no timeout ...
            //

            ntStatus = KeWaitForSingleObject( &pPoolEntry->Event,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              NULL);

            //
            // Process the result of the request
            //

            if( ntStatus == STATUS_SUCCESS)
            {

                ntStatus = pPoolEntry->ResultStatus;
            }
            else
            {

                ntStatus = STATUS_DEVICE_NOT_READY;
            }
        }

try_exit:

        if( bReleasePool)
        {

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);
        }

        if( bDecrementCount &&
            InterlockedDecrement( &pControlDevExt->Specific.Control.OutstandingServiceRequestCount) == 0)
        {
            KeSetEvent( &pControlDevExt->Specific.Control.OutstandingServiceRequestEvent,
                        0,
                        FALSE);
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()))
    {

        AFSDumpTraceFilesFnc();

        if( bReleasePool)
        {

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);
        }

        if( bDecrementCount &&
            InterlockedDecrement( &pControlDevExt->Specific.Control.OutstandingServiceRequestCount) == 0)
        {
            KeSetEvent( &pControlDevExt->Specific.Control.OutstandingServiceRequestEvent,
                        0,
                        FALSE);
        }

        if ( ntStatus == STATUS_SUCCESS)
        {

            ntStatus = STATUS_UNSUCCESSFUL;
        }
    }

    return ntStatus;
}

static
NTSTATUS
AFSCheckIoctlPermissions( IN ULONG ControlCode)
{
    switch ( ControlCode)
    {
        //
        // First the FS ioctls
        //

        case IOCTL_AFS_INITIALIZE_CONTROL_DEVICE:

            //
            // Only a System Service can run this (unless we are compiled
            // for debug with the correct flags set.
            //

            if ( !AFSIsUser( SeExports->SeLocalSystemSid)
#if DBG
                && !BooleanFlagOn( AFSDebugFlags, AFS_DBG_DISABLE_SYSTEM_SID_CHECK)
#endif
                )
            {

                return STATUS_ACCESS_DENIED;
            }
            return STATUS_SUCCESS;

        case IOCTL_AFS_INITIALIZE_REDIRECTOR_DEVICE:
        case IOCTL_AFS_PROCESS_IRP_REQUEST:
        case IOCTL_AFS_PROCESS_IRP_RESULT:
        case IOCTL_AFS_SYSNAME_NOTIFICATION:
        case IOCTL_AFS_SHUTDOWN:

            //
            // Once initialized, only the service can call these
            //

            if ( !AFSIsService())
            {

                return STATUS_ACCESS_DENIED;
            }
            return STATUS_SUCCESS;

        case IOCTL_AFS_CONFIGURE_DEBUG_TRACE:
        case IOCTL_AFS_GET_TRACE_BUFFER:
        case IOCTL_AFS_FORCE_CRASH:

            //
            // Any admin can call these
            //

            if ( !AFSIsInGroup( SeExports->SeAliasAdminsSid))
            {

                return STATUS_ACCESS_DENIED;
            }
            return STATUS_SUCCESS;

        case IOCTL_AFS_AUTHGROUP_CREATE_AND_SET:
        case IOCTL_AFS_AUTHGROUP_QUERY:
        case IOCTL_AFS_AUTHGROUP_SET:
        case IOCTL_AFS_AUTHGROUP_RESET:
        case IOCTL_AFS_AUTHGROUP_LOGON_CREATE:
        case IOCTL_AFS_AUTHGROUP_SID_CREATE:
        case IOCTL_AFS_AUTHGROUP_SID_QUERY:

            //
            // Anyone can call these.
            //

            return STATUS_SUCCESS;

        //
        // And now the LIB ioctls
        //

        case IOCTL_AFS_INITIALIZE_LIBRARY_DEVICE:

            //
            // Only the kernel can issue this
            //

            return STATUS_ACCESS_DENIED;

        case IOCTL_AFS_STATUS_REQUEST:
        case 0x140390:      // IOCTL_LMR_DISABLE_LOCAL_BUFFERING

            //
            // Anyone can call these.
            //

            return STATUS_SUCCESS;

        case IOCTL_AFS_ADD_CONNECTION:
        case IOCTL_AFS_CANCEL_CONNECTION:
        case IOCTL_AFS_GET_CONNECTION:
        case IOCTL_AFS_LIST_CONNECTIONS:
        case IOCTL_AFS_GET_CONNECTION_INFORMATION:

            //
            // These must only be called by the network provider but we
            // don't have a method of enforcing that at the moment.
            //

            return STATUS_SUCCESS;

        case IOCTL_AFS_SET_FILE_EXTENTS:
        case IOCTL_AFS_RELEASE_FILE_EXTENTS:
        case IOCTL_AFS_SET_FILE_EXTENT_FAILURE:
        case IOCTL_AFS_INVALIDATE_CACHE:
        case IOCTL_AFS_NETWORK_STATUS:
        case IOCTL_AFS_VOLUME_STATUS:

            //
            // Again, service only
            //

            if ( !AFSIsService())
            {

                return STATUS_ACCESS_DENIED;
            }
            return STATUS_SUCCESS;

        case IOCTL_AFS_GET_OBJECT_INFORMATION:

            //
            // Admins only
            //

            if ( !AFSIsInGroup( SeExports->SeAliasAdminsSid))
            {

                return STATUS_ACCESS_DENIED;
            }
            return STATUS_SUCCESS;

        default:

            //
            // NOTE that for security we police all known functions here
            // and return STATUS_NOT_IMPLEMENTED.  So new ioctls need to
            // be added both here and either below or in
            // ..\lib\AFSDevControl.cpp
            //

            return STATUS_NOT_IMPLEMENTED;
    }
}
NTSTATUS
AFSProcessControlRequest( IN PIRP Irp)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION  pIrpSp;
    ULONG               ulIoControlCode;
    BOOLEAN             bCompleteRequest = TRUE;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __try
    {

        pIrpSp = IoGetCurrentIrpStackLocation( Irp);

        ulIoControlCode = pIrpSp->Parameters.DeviceIoControl.IoControlCode;

        ntStatus = AFSCheckIoctlPermissions( ulIoControlCode);

        if ( !NT_SUCCESS( ntStatus))
        {
            try_return( ntStatus);
        }

        switch( ulIoControlCode)
        {

            case IOCTL_AFS_INITIALIZE_CONTROL_DEVICE:
            {
                //
                // Go intialize the pool
                //
                ntStatus = AFSInitIrpPool();

                if( !NT_SUCCESS( ntStatus))
                {

                    //
                    // Don't initialize
                    //

                    break;
                }

                //
                // Tag this instance as the one to close the irp pool when it is closed
                //

                pIrpSp->FileObject->FsContext = (void *)((ULONG_PTR)pIrpSp->FileObject->FsContext | AFS_CONTROL_INSTANCE);

                AFSRegisterService();

                break;
            }

            case IOCTL_AFS_INITIALIZE_REDIRECTOR_DEVICE:
            {

                AFSRedirectorInitInfo *pRedirInitInfo = (AFSRedirectorInitInfo *)Irp->AssociatedIrp.SystemBuffer;

                //
                // Extract off the passed in information which contains the
                // cache file parameters
                //

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSRedirectorInitInfo) ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < (ULONG)FIELD_OFFSET( AFSRedirectorInitInfo, CacheFileName) +
                                                                                                    pRedirInitInfo->CacheFileNameLength)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                //
                // Initialize the Redirector device
                //

                ntStatus = AFSInitializeRedirector( pRedirInitInfo);

                if( !NT_SUCCESS( ntStatus))
                {

                    break;
                }

                //
                // Stash away context so we know the instance used to initialize the redirector
                //

                pIrpSp->FileObject->FsContext = (void *)((ULONG_PTR)pIrpSp->FileObject->FsContext | AFS_REDIRECTOR_INSTANCE);

                break;
            }

            case IOCTL_AFS_PROCESS_IRP_REQUEST:
            {

                ntStatus = AFSProcessIrpRequest( Irp);

                break;
            }

            case IOCTL_AFS_PROCESS_IRP_RESULT:
            {

                ntStatus = AFSProcessIrpResult( Irp);

                break;
            }

            case IOCTL_AFS_SYSNAME_NOTIFICATION:
            {

                AFSSysNameNotificationCB *pSysNameInfo = (AFSSysNameNotificationCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pSysNameInfo == NULL ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSSysNameNotificationCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSSetSysNameInformation( pSysNameInfo,
                                                     pIrpSp->Parameters.DeviceIoControl.InputBufferLength);

                break;
            }

            case IOCTL_AFS_CONFIGURE_DEBUG_TRACE:
            {

                AFSTraceConfigCB *pTraceInfo = (AFSTraceConfigCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pTraceInfo == NULL ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSTraceConfigCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSConfigureTrace( pTraceInfo);

                break;
            }

            case IOCTL_AFS_GET_TRACE_BUFFER:
            {

                if( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSGetTraceBuffer( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                              Irp->AssociatedIrp.SystemBuffer,
                                              &Irp->IoStatus.Information);

                break;
            }

            case IOCTL_AFS_FORCE_CRASH:
            {

#if DBG

                if( BooleanFlagOn( AFSDebugFlags, AFS_DBG_FLAG_ENABLE_FORCE_CRASH))
                {

                    KeBugCheck( (ULONG)-1);
                }
#endif

                break;
            }

#ifdef NOT_IMPLEMENTED
            case IOCTL_AFS_LOAD_LIBRARY:
            {

                AFSLoadLibraryCB *pLoadLib = (AFSLoadLibraryCB *)Irp->AssociatedIrp.SystemBuffer;
                UNICODE_STRING uniServicePath;

                if( pLoadLib == NULL ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSLoadLibraryCB) ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < (ULONG)FIELD_OFFSET( AFSLoadLibraryCB, LibraryServicePath) +
                                                                                                    pLoadLib->LibraryServicePathLength)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                uniServicePath.Length = pLoadLib->LibraryServicePathLength;
                uniServicePath.MaximumLength = uniServicePath.Length;

                uniServicePath.Buffer = pLoadLib->LibraryServicePath;

                if( uniServicePath.Length == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSLoadLibrary( pLoadLib->Flags,
                                           &uniServicePath);

                if( NT_SUCCESS( ntStatus))
                {

                    //
                    // Intialize the library
                    //

                    ntStatus = AFSInitializeLibrary( NULL,
                                                     FALSE);
                }

                break;
            }

            case IOCTL_AFS_UNLOAD_LIBRARY:
            {

                //
                // Try to unload the library we currently have in place
                //

                ntStatus = AFSUnloadLibrary( FALSE);

                break;
            }
#endif

            case IOCTL_AFS_SHUTDOWN:
            {

                ntStatus = AFSShutdownRedirector();

                break;
            }

            case IOCTL_AFS_AUTHGROUP_CREATE_AND_SET:
            {

                AFSAuthGroupRequestCB *pAuthGroupRequestCB = (AFSAuthGroupRequestCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pAuthGroupRequestCB == NULL ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSAuthGroupRequestCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSCreateSetProcessAuthGroup( pAuthGroupRequestCB);

                break;
            }

            case IOCTL_AFS_AUTHGROUP_QUERY:
            {

                ntStatus = AFSQueryProcessAuthGroupList( ( GUID *)Irp->AssociatedIrp.SystemBuffer,
                                                         pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                                         &Irp->IoStatus.Information);

                break;
            }

            case IOCTL_AFS_AUTHGROUP_SET:
            {

                AFSAuthGroupRequestCB *pAuthGroupRequestCB = (AFSAuthGroupRequestCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pAuthGroupRequestCB == NULL ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSAuthGroupRequestCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSSetActiveProcessAuthGroup( pAuthGroupRequestCB);

                break;
            }

            case IOCTL_AFS_AUTHGROUP_RESET:
            {

                AFSAuthGroupRequestCB *pAuthGroupRequestCB = (AFSAuthGroupRequestCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pAuthGroupRequestCB == NULL ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSAuthGroupRequestCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSResetActiveProcessAuthGroup( pAuthGroupRequestCB);

                break;
            }

            case IOCTL_AFS_AUTHGROUP_LOGON_CREATE:
            case IOCTL_AFS_AUTHGROUP_SID_CREATE:
            {

                AFSAuthGroupRequestCB *pAuthGroupRequestCB = (AFSAuthGroupRequestCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pAuthGroupRequestCB != NULL &&
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSAuthGroupRequestCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSCreateAuthGroupForSIDorLogonSession( pAuthGroupRequestCB,
                                                                   ulIoControlCode == IOCTL_AFS_AUTHGROUP_LOGON_CREATE);

                break;
            }

            case IOCTL_AFS_AUTHGROUP_SID_QUERY:
            {

                AFSAuthGroupRequestCB *pAuthGroupRequestCB = NULL;

                if( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof( GUID))
                {
                    ntStatus = STATUS_INVALID_PARAMETER;
                    break;
                }

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength >= sizeof( AFSAuthGroupRequestCB))
                {
                    pAuthGroupRequestCB = (AFSAuthGroupRequestCB *)Irp->AssociatedIrp.SystemBuffer;
                }

                ntStatus = AFSQueryAuthGroup( pAuthGroupRequestCB,
                                              (GUID *)Irp->AssociatedIrp.SystemBuffer,
                                              &Irp->IoStatus.Information);

                break;
            }

            default:
            {

                //
                // Check the state of the library
                //

                ntStatus = AFSCheckLibraryState( Irp);

                if( !NT_SUCCESS( ntStatus) ||
                    ntStatus == STATUS_PENDING)
                {

                    if( ntStatus == STATUS_PENDING)
                    {
                        bCompleteRequest = FALSE;
                    }

                    break;
                }

                bCompleteRequest = FALSE;

                IoSkipCurrentIrpStackLocation( Irp);

                ntStatus = IoCallDriver( pDevExt->Specific.Control.LibraryDeviceObject,
                                         Irp);

                //
                // Indicate the library is done with the request
                //

                AFSClearLibraryRequest();

                break;
            }
        }

    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()))
    {

        ntStatus = STATUS_UNSUCCESSFUL;

        AFSDumpTraceFilesFnc();
    }

try_exit:

    if( bCompleteRequest)
    {

        Irp->IoStatus.Status = ntStatus;

        AFSCompleteRequest( Irp,
                              ntStatus);
    }

    return ntStatus;
}

NTSTATUS
AFSInitIrpPool()
{

    NTSTATUS       ntStatus = STATUS_SUCCESS;
    AFSCommSrvcCB *pCommSrvc = NULL;
    BOOLEAN        bReleasePools = FALSE;
    AFSDeviceExt  *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        pCommSrvc = &pDevExt->Specific.Control.CommServiceCB;

        //
        // Whenever we change state we must grab both pool locks. On the checking of the state
        // within the processing routines for these respective pools, we only grab one lock to
        // minimize serialization. The ordering is always the Irp pool then the result pool
        // locks. We also do this in the tear down of the pool
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitIrpPool Acquiring IrpPoolLock lock %p EXCL %08lX\n",
                      &pCommSrvc->IrpPoolLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                          TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitIrpPool Acquiring ResultPoolLock lock %p EXCL %08lX\n",
                      &pCommSrvc->ResultPoolLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                          TRUE);

        bReleasePools = TRUE;

        //
        // The pool can be either ACTIVE or INACTIVE. If the pool state is INACTIVE and we
        // are receiving the INIT request, then activate it. If the pool is ACTIVE, then we
        // shouldn't be getting this request ...
        //

        if( pCommSrvc->IrpPoolControlFlag == POOL_ACTIVE)
        {

            //
            // We have already been activated so just fail this request
            //

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }
        else if( pCommSrvc->IrpPoolControlFlag == POOL_INACTIVE)
        {

            //
            // The pool is currently INACTIVE so start it up and ready it to
            // receive irp requests
            //

            pCommSrvc->IrpPoolControlFlag = POOL_ACTIVE;

            pDevExt->Specific.Control.ServiceProcess = (PKPROCESS)PsGetCurrentProcess();

            try_return( ntStatus = STATUS_SUCCESS);
        }
        else
        {

            //
            // The pool is in some mixed state, fail the request.
            //

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

try_exit:

        if( bReleasePools)
        {

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);

            AFSReleaseResource( &pCommSrvc->ResultPoolLock);
        }
    }

    return ntStatus;
}

void
AFSCleanupIrpPool()
{

    AFSPoolEntry   *pEntry = NULL, *pNextEntry = NULL;
    AFSDeviceExt   *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSCommSrvcCB  *pCommSrvc = (AFSCommSrvcCB *)&pDevExt->Specific.Control.CommServiceCB;

    __Enter
    {

        //
        // When we change the state, grab both pool locks exclusive. The order is always the
        // Irp pool then the result pool lock
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSCleanupIrpPool Acquiring IrpPoolLock lock %p EXCL %08lX\n",
                      &pCommSrvc->IrpPoolLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                        TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSCleanupIrpPool Acquiring ResultPoolLock lock %p EXCL %08lX\n",
                      &pCommSrvc->ResultPoolLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                        TRUE);

        //
        // Indicate we are pending stop
        //

        pCommSrvc->IrpPoolControlFlag = POOL_INACTIVE;

        //
        // Set the event to release any waiting workers
        // (everyone waits on IrpPoolHasReleaseEntries)
        //

        KeSetEvent( &pCommSrvc->IrpPoolHasReleaseEntries,
                    0,
                    FALSE);

        //
        // Go through the pool entries and free up the structures.
        //

        pEntry = pCommSrvc->RequestPoolHead;

        while( pEntry != NULL)
        {

            pNextEntry = pEntry->fLink;

            if( BooleanFlagOn( pEntry->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS))
            {

                //
                // Here we need to complete the irp, cancelled, and delete the data block
                //

                pEntry->ResultStatus = (ULONG) STATUS_CANCELLED;

                KeSetEvent( &pEntry->Event,
                            0,
                            FALSE);
            }
            else
            {

                ExFreePool( pEntry);
            }

            pEntry = pNextEntry;
        }

        //
        // Cleanup the control structure for the request pool
        //

        pCommSrvc->RequestPoolHead = NULL;

        pCommSrvc->RequestPoolTail = NULL;

        pCommSrvc->IrpPoolRequestIndex = 1;

        KeClearEvent( &pCommSrvc->IrpPoolHasEntries);

        KeClearEvent( &pCommSrvc->IrpPoolHasReleaseEntries);

        //
        // Release the irp pool lock.
        //

        AFSReleaseResource( &pCommSrvc->IrpPoolLock);

        //
        // Go through the result pool entries and free up the structures.
        //

        pEntry = pCommSrvc->ResultPoolHead;

        while( pEntry != NULL)
        {

            pNextEntry = pEntry->fLink;

            pEntry->ResultStatus = (ULONG) STATUS_CANCELLED;

            //
            // Here we will set the event of the requestor and let the blocked thread
            // free the data block
            //

            KeSetEvent( &pEntry->Event,
                        0,
                        FALSE);

            //
            // Go onto the next entry
            //

            pEntry = pNextEntry;
        }

        //
        // Cleanup the control structure for the result pool
        //

        pCommSrvc->ResultPoolHead = NULL;

        pCommSrvc->ResultPoolTail = NULL;

        //
        // Release the result pool lock.
        //

        AFSReleaseResource( &pCommSrvc->ResultPoolLock);

    }

    return;
}

NTSTATUS
AFSInsertRequest( IN AFSCommSrvcCB *CommSrvc,
                  IN AFSPoolEntry *Entry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInsertRequest Acquiring IrpPoolLock lock %p EXCL %08lX\n",
                      &CommSrvc->IrpPoolLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( &CommSrvc->IrpPoolLock,
                        TRUE);

        if( CommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
        {

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        if( CommSrvc->RequestPoolHead == NULL)
        {

            CommSrvc->RequestPoolHead = Entry;
        }
        else
        {

            CommSrvc->RequestPoolTail->fLink = Entry;

            Entry->bLink = CommSrvc->RequestPoolTail;
        }

        CommSrvc->RequestPoolTail = Entry;

        if( Entry->RequestType == AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS)
        {

            KeSetEvent( &CommSrvc->IrpPoolHasReleaseEntries,
                        0,
                        FALSE);
        }
        else
        {

            KeSetEvent( &CommSrvc->IrpPoolHasEntries,
                        0,
                        FALSE);
        }

        InterlockedIncrement( &CommSrvc->QueueCount);

try_exit:

        AFSReleaseResource( &CommSrvc->IrpPoolLock);
    }

    return ntStatus;
}

NTSTATUS
AFSProcessIrpRequest( IN PIRP Irp)
{

    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSDeviceExt    *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSCommSrvcCB   *pCommSrvc = NULL;
    AFSPoolEntry    *pEntry = NULL, *pPrevEntry = NULL;
    AFSCommRequest  *pRequest = NULL;
    BOOLEAN          bReleaseRequestThread = FALSE;
    PVOID            Objects[2];

    __Enter
    {

        pCommSrvc = &pDevExt->Specific.Control.CommServiceCB;

        pRequest = (AFSCommRequest *)Irp->AssociatedIrp.SystemBuffer;

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessIrpRequest Acquiring IrpPoolLock lock %p EXCL %08lX\n",
                      &pCommSrvc->IrpPoolLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                        TRUE);

        if( pCommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
        {

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        AFSReleaseResource( &pCommSrvc->IrpPoolLock);

        //
        // Is this a dedicated flush thread?
        //

        if( BooleanFlagOn( pRequest->RequestFlags, AFS_REQUEST_RELEASE_THREAD))
        {

            bReleaseRequestThread = TRUE;
        }

        //
        // Populate the objects array for the non release only threads
        // Release only workers can only process release extent events
        // whereas normal workers can process any kind of event.
        // Release only workers are present to ensure there cannot be
        // a deadlock due to all extents held by the redirector and
        // there not be a worker available to release them.
        //

        Objects[0] = &pCommSrvc->IrpPoolHasReleaseEntries;

        Objects[1] = &pCommSrvc->IrpPoolHasEntries;

        //
        // Wait on the 'have items' event until we can retrieve an item
        //

        while( TRUE)
        {

            if( bReleaseRequestThread)
            {

                ntStatus = KeWaitForSingleObject( &pCommSrvc->IrpPoolHasReleaseEntries,
                                                  UserRequest,
                                                  UserMode,
                                                  TRUE,
                                                  NULL);

                if( ntStatus != STATUS_SUCCESS)
                {

                    ntStatus = STATUS_DEVICE_NOT_READY;

                    break;
                }

            }
            else
            {

                ntStatus = KeWaitForMultipleObjects( 2,
                                                     Objects,
                                                     WaitAny,
                                                     UserRequest,
                                                     UserMode,
                                                     TRUE,
                                                     NULL,
                                                     NULL);

                if( ntStatus != STATUS_WAIT_0 &&
                    ntStatus != STATUS_WAIT_1)
                {

                    ntStatus = STATUS_DEVICE_NOT_READY;

                    break;
                }
            }

            //
            // Grab the lock on the request pool
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSProcessIrpRequest Acquiring IrpPoolLock (WAIT) lock %p EXCL %08lX\n",
                          &pCommSrvc->IrpPoolLock,
                          PsGetCurrentThread());

            AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                            TRUE);

            if( pCommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
            {

                AFSReleaseResource( &pCommSrvc->IrpPoolLock);

                //
                // Wake up the next worker since this is a SynchronizationEvent
                //

                KeSetEvent( &pCommSrvc->IrpPoolHasReleaseEntries,
                            0,
                            FALSE);

                try_return( ntStatus = STATUS_DEVICE_NOT_READY);
            }

            //
            // If this is a dedicated flush thread only look for a flush request in the queue
            //

            if( bReleaseRequestThread)
            {

                pEntry = pCommSrvc->RequestPoolHead;

                pPrevEntry = NULL;

                while( pEntry != NULL)
                {

                    if( pEntry->RequestType == AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS)
                    {

                        if( pPrevEntry == NULL)
                        {

                            pCommSrvc->RequestPoolHead = pEntry->fLink;

                            if( pCommSrvc->RequestPoolHead == NULL)
                            {

                                pCommSrvc->RequestPoolTail = NULL;
                            }
                        }
                        else
                        {

                            pPrevEntry->fLink = pEntry->fLink;

                            if( pPrevEntry->fLink == NULL)
                            {

                                pCommSrvc->RequestPoolTail = pPrevEntry;
                            }
                        }

                        break;
                    }

                    pPrevEntry = pEntry;

                    pEntry = pEntry->fLink;
                }

                if( pEntry != NULL)
                {

                    //
                    // There might be another release entry pending
                    //

                    KeSetEvent( &pCommSrvc->IrpPoolHasReleaseEntries,
                                0,
                                FALSE);
                }

                //
                // And release the request pool lock
                //

                AFSReleaseResource( &pCommSrvc->IrpPoolLock);
            }
            else
            {

                pEntry = pCommSrvc->RequestPoolHead;

                if( pEntry != NULL)
                {

                    pCommSrvc->RequestPoolHead = pEntry->fLink;

                    pEntry->bLink = NULL;

                    if( pCommSrvc->RequestPoolHead == NULL)
                    {

                        pCommSrvc->RequestPoolTail = NULL;
                    }
                    else
                    {

                        KeSetEvent( &pCommSrvc->IrpPoolHasEntries,
                                    0,
                                    FALSE);
                    }
                }

                //
                // And release the request pool lock
                //

                AFSReleaseResource( &pCommSrvc->IrpPoolLock);
            }

            //
            // Insert the entry into the result pool, if we have one
            //

            if( pEntry != NULL)
            {

                //
                // Move the request data into the passed in buffer
                //

                ASSERT( sizeof( AFSCommRequest) +
                                    pEntry->FileName.Length +
                                    pEntry->DataLength <= pIrpSp->Parameters.DeviceIoControl.OutputBufferLength);

                RtlCopyMemory( &pRequest->AuthGroup,
                               &pEntry->AuthGroup,
                               sizeof( GUID));

                pRequest->FileId = pEntry->FileId;

                pRequest->RequestType = pEntry->RequestType;

                pRequest->RequestIndex = pEntry->RequestIndex;

                pRequest->RequestFlags = pEntry->RequestFlags;

                pRequest->NameLength = pEntry->FileName.Length;

                pRequest->QueueCount = InterlockedDecrement( &pCommSrvc->QueueCount);

                if( pRequest->NameLength > 0)
                {

                    RtlCopyMemory( pRequest->Name,
                                   pEntry->FileName.Buffer,
                                   pRequest->NameLength);
                }

                pRequest->DataOffset = 0;

                pRequest->DataLength = pEntry->DataLength;

                if( pRequest->DataLength > 0)
                {

                    pRequest->DataOffset = pEntry->FileName.Length;

                    RtlCopyMemory( (void *)((char *)pRequest->Name + pRequest->DataOffset),
                                   pEntry->Data,
                                   pRequest->DataLength);
                }

                pRequest->ResultBufferLength = 0;

                if( pEntry->ResultBufferLength != NULL)
                {

                    pRequest->ResultBufferLength = *(pEntry->ResultBufferLength);
                }

                Irp->IoStatus.Information = sizeof( AFSCommRequest) +
                                                        pEntry->FileName.Length +
                                                        pEntry->DataLength;

                //
                // If this is a synchronous request then move the request into the
                // result pool
                //

                if( BooleanFlagOn( pEntry->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS))
                {

                    pEntry->fLink = NULL;
                    pEntry->bLink = NULL;

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSProcessIrpRequest Acquiring ResultPoolLock lock %p EXCL %08lX\n",
                                  &pCommSrvc->ResultPoolLock,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                                    TRUE);

                    if( pCommSrvc->ResultPoolHead == NULL)
                    {

                        pCommSrvc->ResultPoolHead = pEntry;
                    }
                    else
                    {

                        pCommSrvc->ResultPoolTail->fLink = pEntry;

                        pEntry->bLink = pCommSrvc->ResultPoolTail;
                    }

                    pCommSrvc->ResultPoolTail = pEntry;

                    AFSReleaseResource( &pCommSrvc->ResultPoolLock);
                }
                else
                {

                    //
                    // Free up the pool entry
                    //

                    ExFreePool( pEntry);
                }

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSProcessIrpResult( IN PIRP Irp)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSCommSrvcCB      *pCommSrvc = NULL;
    AFSPoolEntry       *pCurrentEntry = NULL;
    AFSCommResult      *pResult = NULL;
    ULONG               ulCopyLen = 0;

    __Enter
    {

        pCommSrvc = &pDevExt->Specific.Control.CommServiceCB;

        //
        // Get the request for the incoming result
        //

        pResult = (AFSCommResult *)Irp->AssociatedIrp.SystemBuffer;

        if( pResult == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // Go look for our entry
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessIrpResult Acquiring ResultPoolLock lock %p EXCL %08lX\n",
                      &pCommSrvc->ResultPoolLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                          TRUE);

        pCurrentEntry = pCommSrvc->ResultPoolHead;

        while( pCurrentEntry != NULL)
        {

            if( pCurrentEntry->RequestIndex == pResult->RequestIndex)
            {

                //
                // Found the entry so remove it from the queue
                //

                if( pCurrentEntry->bLink == NULL)
                {

                    //
                    // At the head of the list
                    //

                    pCommSrvc->ResultPoolHead = pCurrentEntry->fLink;

                    if( pCommSrvc->ResultPoolHead != NULL)
                    {

                        pCommSrvc->ResultPoolHead->bLink = NULL;
                    }
                }
                else
                {

                    pCurrentEntry->bLink->fLink = pCurrentEntry->fLink;
                }

                if( pCurrentEntry->fLink == NULL)
                {

                    pCommSrvc->ResultPoolTail = pCurrentEntry->bLink;

                    if( pCommSrvc->ResultPoolTail != NULL)
                    {

                        pCommSrvc->ResultPoolTail->fLink = NULL;
                    }
                }
                else
                {

                    pCurrentEntry->fLink->bLink = pCurrentEntry->bLink;
                }

                break;
            }

            pCurrentEntry = pCurrentEntry->fLink;
        }

        AFSReleaseResource( &pCommSrvc->ResultPoolLock);

        if( pCurrentEntry == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // OK, move in the result information
        //

        pCurrentEntry->ResultStatus = pResult->ResultStatus;

        if( ( pCurrentEntry->ResultStatus == STATUS_SUCCESS ||
              pCurrentEntry->ResultStatus == STATUS_BUFFER_OVERFLOW) &&
            pCurrentEntry->ResultBufferLength != NULL &&
            pCurrentEntry->ResultBuffer != NULL)
        {

            ASSERT( pResult->ResultBufferLength <= *(pCurrentEntry->ResultBufferLength));

            ulCopyLen = pResult->ResultBufferLength;

            if( ulCopyLen > *(pCurrentEntry->ResultBufferLength))
            {
                ulCopyLen = *(pCurrentEntry->ResultBufferLength);
            }

            *(pCurrentEntry->ResultBufferLength) = ulCopyLen;

            if( pResult->ResultBufferLength > 0)
            {

                RtlCopyMemory( pCurrentEntry->ResultBuffer,
                               pResult->ResultData,
                               ulCopyLen);
            }
        }

        KeSetEvent( &pCurrentEntry->Event,
                    0,
                    FALSE);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

        }
    }

    return ntStatus;
}
