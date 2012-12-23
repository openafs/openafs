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
// File: AFSLibrarySupport.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSLoadLibrary( IN ULONG Flags,
                IN UNICODE_STRING *ServicePath)
{
    UNREFERENCED_PARAMETER(Flags);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    UNICODE_STRING uniLibraryName;
    AFSDeviceExt *pLibDevExt = NULL;
    PFILE_OBJECT pLibraryFileObject = NULL;
    PDEVICE_OBJECT pLibraryDeviceObject = NULL;

    __Enter
    {

        //
        // Wait on the load library event so we don't race with any
        // other requests coming through
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Start load library\n",
                      __FUNCTION__);

        ntStatus = KeWaitForSingleObject( &pDevExt->Specific.Control.LoadLibraryEvent,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSLoadLibrary Wait for LoadLibraryEvent failure %08lX\n",
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // Check our current state to ensure we currently do not have a library loaded
        //

        if( BooleanFlagOn( pDevExt->Specific.Control.LibraryState, AFS_LIBRARY_LOADED))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Library already loaded\n",
                          __FUNCTION__);

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        pDevExt->Specific.Control.LibraryServicePath.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                                                 ServicePath->Length,
                                                                                                 AFS_GENERIC_MEMORY_25_TAG);

        if( pDevExt->Specific.Control.LibraryServicePath.Buffer == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSLoadLibrary AFS_GENERIC_MEMORY_25_TAG allocation error\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pDevExt->Specific.Control.LibraryServicePath.Buffer,
                       ServicePath->Length);

        pDevExt->Specific.Control.LibraryServicePath.Length = ServicePath->Length;
        pDevExt->Specific.Control.LibraryServicePath.MaximumLength = pDevExt->Specific.Control.LibraryServicePath.Length;

        RtlCopyMemory( pDevExt->Specific.Control.LibraryServicePath.Buffer,
                       ServicePath->Buffer,
                       pDevExt->Specific.Control.LibraryServicePath.Length);

        //
        // Load the library
        //

        ntStatus = ZwLoadDriver( ServicePath);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to load library Status %08lX\n",
                          __FUNCTION__,
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // Open up the control device and grab teh entry points for the library
        //

        RtlInitUnicodeString( &uniLibraryName,
                              AFS_LIBRARY_CONTROL_DEVICE_NAME);

        ntStatus = IoGetDeviceObjectPointer( &uniLibraryName,
                                             FILE_ALL_ACCESS,
                                             &pLibraryFileObject,
                                             &pLibraryDeviceObject);

        if( !NT_SUCCESS( ntStatus))
        {
            AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSLoadLibrary IoGetDeviceObjectPointer failure %08lX\n",
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // We have our reference to the library device object. Grab the
        // device extension and setup our callbacks
        //

        pLibDevExt = (AFSDeviceExt *)pLibraryDeviceObject->DeviceExtension;

        //
        // Save off our references
        //

        pDevExt->Specific.Control.LibraryFileObject = pLibraryFileObject;

        pDevExt->Specific.Control.LibraryDeviceObject = pLibraryDeviceObject;

        //
        // Reset the state for our library
        //

        AFSAcquireExcl( &pDevExt->Specific.Control.LibraryStateLock,
                        TRUE);

        SetFlag( pDevExt->Specific.Control.LibraryState, AFS_LIBRARY_LOADED);

        ClearFlag( pDevExt->Specific.Control.LibraryState, AFS_LIBRARY_QUEUE_CANCELLED);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Completed load library, processing queued requests\n",
                      __FUNCTION__);

        AFSReleaseResource( &pDevExt->Specific.Control.LibraryStateLock);

        //
        // Process the queued requests
        //

        AFSProcessQueuedResults( FALSE);

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Library load complete Status %08lX\n",
                      __FUNCTION__,
                      ntStatus);

        if( !NT_SUCCESS( ntStatus))
        {

            if( pDevExt->Specific.Control.LibraryServicePath.Buffer != NULL)
            {

                ZwUnloadDriver( &pDevExt->Specific.Control.LibraryServicePath);

                ExFreePool( pDevExt->Specific.Control.LibraryServicePath.Buffer);

                pDevExt->Specific.Control.LibraryServicePath.Buffer = NULL;
                pDevExt->Specific.Control.LibraryServicePath.Length = 0;
                pDevExt->Specific.Control.LibraryServicePath.MaximumLength = 0;
            }
        }

        KeSetEvent( &pDevExt->Specific.Control.LoadLibraryEvent,
                    0,
                    FALSE);
    }

    return ntStatus;
}

NTSTATUS
AFSUnloadLibrary( IN BOOLEAN CancelQueue)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    LARGE_INTEGER       liTimeout;

    __Enter
    {

        //
        // Wait on the load library event so we don't race with any
        // other requests coming through
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Start unload library\n",
                      __FUNCTION__);

        ntStatus = KeWaitForSingleObject( &pDevExt->Specific.Control.LoadLibraryEvent,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);

        if( !NT_SUCCESS( ntStatus))
        {
            try_return( ntStatus);
        }

        if( !BooleanFlagOn( pDevExt->Specific.Control.LibraryState, AFS_LIBRARY_LOADED))
        {
            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        //
        // Clear all outstanding requests
        //

        AFSAcquireExcl( &pDevExt->Specific.Control.LibraryStateLock,
                        TRUE);

        ClearFlag( pDevExt->Specific.Control.LibraryState, AFS_LIBRARY_LOADED);

        if( CancelQueue)
        {
            SetFlag( pDevExt->Specific.Control.LibraryState, AFS_LIBRARY_QUEUE_CANCELLED);
        }

        //
        // We'll wait on the inflight event to be set, checking for the inflight
        // request count to reach zero
        //

        while( pDevExt->Specific.Control.InflightLibraryRequests > 0)
        {

            liTimeout.QuadPart = -(AFS_ONE_SECOND);

            //
            // If the count is non-zero make sure the event is cleared
            //

            KeClearEvent( &pDevExt->Specific.Control.InflightLibraryEvent);

            AFSReleaseResource( &pDevExt->Specific.Control.LibraryStateLock);

            ntStatus = KeWaitForSingleObject( &pDevExt->Specific.Control.InflightLibraryEvent,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              &liTimeout);

            AFSAcquireExcl( &pDevExt->Specific.Control.LibraryStateLock,
                            TRUE);

            if( ntStatus != STATUS_TIMEOUT &&
                ntStatus != STATUS_SUCCESS)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Failed request event Status %08lX\n",
                              __FUNCTION__,
                              ntStatus);

                SetFlag( pDevExt->Specific.Control.LibraryState, AFS_LIBRARY_LOADED);

                AFSReleaseResource( &pDevExt->Specific.Control.LibraryStateLock);

                AFSProcessQueuedResults( TRUE);

                try_return( ntStatus);
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Wait for inflight requests to complete %08lX\n",
                          __FUNCTION__,
                          pDevExt->Specific.Control.InflightLibraryRequests);
        }

        AFSReleaseResource( &pDevExt->Specific.Control.LibraryStateLock);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Processing queued results\n",
                      __FUNCTION__);

        AFSProcessQueuedResults( TRUE);

        //
        // Unload the current library implementation
        //

        if( pDevExt->Specific.Control.LibraryFileObject != NULL)
        {
            ObDereferenceObject( pDevExt->Specific.Control.LibraryFileObject);
        }

        pDevExt->Specific.Control.LibraryFileObject = NULL;

        pDevExt->Specific.Control.LibraryDeviceObject = NULL;

        ZwUnloadDriver( &pDevExt->Specific.Control.LibraryServicePath);

        ExFreePool( pDevExt->Specific.Control.LibraryServicePath.Buffer);

        pDevExt->Specific.Control.LibraryServicePath.Length = 0;

        pDevExt->Specific.Control.LibraryServicePath.MaximumLength = 0;

        pDevExt->Specific.Control.LibraryServicePath.Buffer = NULL;

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Library unload complete Status %08lX\n",
                      __FUNCTION__,
                      ntStatus);

        KeSetEvent( &pDevExt->Specific.Control.LoadLibraryEvent,
                    0,
                    FALSE);
    }

    return ntStatus;
}

NTSTATUS
AFSCheckLibraryState( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __Enter
    {

        AFSAcquireShared( &pDevExt->Specific.Control.LibraryStateLock,
                          TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Entry State %08lX Irp %p Function %08lX\n",
                      __FUNCTION__,
                      pRDRDevExt->DeviceFlags,
                      Irp,
                      pIrpSp->MajorFunction);

        if( BooleanFlagOn( pRDRDevExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
        {

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        if( !BooleanFlagOn( pDevExt->Specific.Control.LibraryState, AFS_LIBRARY_LOADED))
        {

            if( Irp != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Queuing request %p\n",
                              __FUNCTION__,
                              Irp);

                ntStatus = AFSQueueLibraryRequest( Irp);

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Queued request %p Status %08lX\n",
                              __FUNCTION__,
                              Irp,
                              ntStatus);
            }
            else
            {

                ntStatus = STATUS_TOO_LATE;

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Failing request %p\n",
                              __FUNCTION__,
                              Irp);
            }

            try_return( ntStatus);
        }

        if( InterlockedIncrement( &pDevExt->Specific.Control.InflightLibraryRequests) == 1)
        {
            KeClearEvent( &pDevExt->Specific.Control.InflightLibraryEvent);
        }

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Completed Irp %p Status %08lX Inflight Count %08lX\n",
                      __FUNCTION__,
                      Irp,
                      ntStatus,
                      pDevExt->Specific.Control.InflightLibraryRequests);

        AFSReleaseResource( &pDevExt->Specific.Control.LibraryStateLock);
    }

    return ntStatus;
}

NTSTATUS
AFSClearLibraryRequest()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {
        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Inflight Count %08lX\n",
                      __FUNCTION__,
                      pDevExt->Specific.Control.InflightLibraryRequests);

        if( InterlockedDecrement( &pDevExt->Specific.Control.InflightLibraryRequests) == 0)
        {

            KeSetEvent( &pDevExt->Specific.Control.InflightLibraryEvent,
                        0,
                        FALSE);
        }

    }

    return ntStatus;
}

NTSTATUS
AFSQueueLibraryRequest( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSLibraryQueueRequestCB *pRequest = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __Enter
    {

        AFSAcquireExcl( &pDevExt->Specific.Control.LibraryQueueLock,
                        TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Entry for Irp %p Function %08lX\n",
                      __FUNCTION__,
                      Irp,
                      pIrpSp->MajorFunction);

        //
        // Has the load processing timed out and we are no longer
        // queuing requests?
        //

        if( BooleanFlagOn( pDevExt->Specific.Control.LibraryState, AFS_LIBRARY_QUEUE_CANCELLED) ||
            BooleanFlagOn( pRDRDevExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Library not loaded for Irp %p\n",
                          __FUNCTION__,
                          Irp);

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        pRequest = (AFSLibraryQueueRequestCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                         sizeof( AFSLibraryQueueRequestCB),
                                                                         AFS_LIBRARY_QUEUE_TAG);

        if( pRequest == NULL)
        {
            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pRequest,
                       sizeof( AFSLibraryQueueRequestCB));

        pRequest->Irp = Irp;

        if( pDevExt->Specific.Control.LibraryQueueHead == NULL)
        {
            pDevExt->Specific.Control.LibraryQueueHead = pRequest;
        }
        else
        {
            pDevExt->Specific.Control.LibraryQueueTail->fLink = pRequest;
        }

        pDevExt->Specific.Control.LibraryQueueTail = pRequest;

        IoMarkIrpPending( Irp);

        ntStatus = STATUS_PENDING;

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Completed for Irp %p Status %08lX\n",
                      __FUNCTION__,
                      Irp,
                      ntStatus);

        AFSReleaseResource( &pDevExt->Specific.Control.LibraryQueueLock);
    }

    return ntStatus;
}

NTSTATUS
AFSProcessQueuedResults( IN BOOLEAN CancelRequest)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSLibraryQueueRequestCB *pRequest = NULL;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Entry\n",
                      __FUNCTION__);

        //
        // Loop through the queue either resubmitting requests or cancelling them
        //

        while( TRUE)
        {

            AFSAcquireExcl( &pDevExt->Specific.Control.LibraryQueueLock,
                            TRUE);

            if( pDevExt->Specific.Control.LibraryQueueHead == NULL)
            {

                AFSReleaseResource( &pDevExt->Specific.Control.LibraryQueueLock);

                break;
            }

            pRequest = pDevExt->Specific.Control.LibraryQueueHead;

            pDevExt->Specific.Control.LibraryQueueHead = pRequest->fLink;

            if( pDevExt->Specific.Control.LibraryQueueHead == NULL)
            {

                pDevExt->Specific.Control.LibraryQueueTail = NULL;
            }

            AFSReleaseResource( &pDevExt->Specific.Control.LibraryQueueLock);

            if( CancelRequest)
            {

                pRequest->Irp->IoStatus.Status = STATUS_CANCELLED;

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Cancelling request Irp %p\n",
                              __FUNCTION__,
                              pRequest->Irp);

                IoCompleteRequest( pRequest->Irp,
                                   IO_NO_INCREMENT);
            }
            else
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Resubmitting request Irp %p\n",
                              __FUNCTION__,
                              pRequest->Irp);

                AFSSubmitLibraryRequest( pRequest->Irp);
            }

            ExFreePool( pRequest);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Completed\n",
                      __FUNCTION__);
    }

    return ntStatus;
}

NTSTATUS
AFSSubmitLibraryRequest( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Submitting Irp %p Function %08lX\n",
                      __FUNCTION__,
                      Irp,
                      pIrpSp->MajorFunction);

        switch( pIrpSp->MajorFunction)
        {

            case IRP_MJ_CREATE:
            {
                AFSCreate( AFSRDRDeviceObject,
                           Irp);
                break;
            }

            case IRP_MJ_CLOSE:
            {
                AFSClose( AFSRDRDeviceObject,
                          Irp);
                break;
            }

            case IRP_MJ_READ:
            {
                AFSRead( AFSRDRDeviceObject,
                         Irp);
                break;
            }

            case IRP_MJ_WRITE:
            {
                AFSWrite( AFSRDRDeviceObject,
                          Irp);
                break;
            }

            case IRP_MJ_QUERY_INFORMATION:
            {
                AFSQueryFileInfo( AFSRDRDeviceObject,
                                  Irp);
                break;
            }

            case IRP_MJ_SET_INFORMATION:
            {
                AFSSetFileInfo( AFSRDRDeviceObject,
                                Irp);
                break;
            }

            case IRP_MJ_QUERY_EA:
            {
                AFSQueryEA( AFSRDRDeviceObject,
                            Irp);
                break;
            }

            case IRP_MJ_SET_EA:
            {
                AFSSetEA( AFSRDRDeviceObject,
                          Irp);
                break;
            }

            case IRP_MJ_FLUSH_BUFFERS:
            {
                AFSFlushBuffers( AFSRDRDeviceObject,
                                 Irp);
                break;
            }

            case IRP_MJ_QUERY_VOLUME_INFORMATION:
            {
                AFSQueryVolumeInfo( AFSRDRDeviceObject,
                                    Irp);
                break;
            }

            case IRP_MJ_SET_VOLUME_INFORMATION:
            {
                AFSSetVolumeInfo( AFSRDRDeviceObject,
                                  Irp);
                break;
            }

            case IRP_MJ_DIRECTORY_CONTROL:
            {
                AFSDirControl( AFSRDRDeviceObject,
                               Irp);
                break;
            }

            case IRP_MJ_FILE_SYSTEM_CONTROL:
            {
                AFSFSControl( AFSRDRDeviceObject,
                              Irp);
                break;
            }

            case IRP_MJ_DEVICE_CONTROL:
            {
                AFSDevControl( AFSRDRDeviceObject,
                               Irp);
                break;
            }

            case IRP_MJ_INTERNAL_DEVICE_CONTROL:
            {
                AFSInternalDevControl( AFSRDRDeviceObject,
                                       Irp);
                break;
            }

            case IRP_MJ_SHUTDOWN:
            {
                AFSShutdown( AFSRDRDeviceObject,
                             Irp);
                break;
            }

            case IRP_MJ_LOCK_CONTROL:
            {
                AFSLockControl( AFSRDRDeviceObject,
                                Irp);
                break;
            }

            case IRP_MJ_CLEANUP:
            {
                AFSCleanup( AFSRDRDeviceObject,
                            Irp);
                break;
            }

            case IRP_MJ_QUERY_SECURITY:
            {
                AFSQuerySecurity( AFSRDRDeviceObject,
                                  Irp);
                break;
            }

            case IRP_MJ_SET_SECURITY:
            {
                AFSSetSecurity( AFSRDRDeviceObject,
                                Irp);
                break;
            }

            case IRP_MJ_SYSTEM_CONTROL:
            {
                AFSSystemControl( AFSRDRDeviceObject,
                                  Irp);
                break;
            }

            default:
            {
                AFSDefaultDispatch( AFSRDRDeviceObject,
                                    Irp);
                break;
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSInitializeLibrary( IN AFSFileID *GlobalRootFid,
                      IN BOOLEAN QueueRootEnumeration)
{
    UNREFERENCED_PARAMETER(QueueRootEnumeration);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSLibraryInitCB    stInitLib;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSDeviceExt       *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        RtlZeroMemory( &stInitLib,
                       sizeof( AFSLibraryInitCB));

        //
        // Initialize the parameters to pass to the library
        //

        stInitLib.AFSControlDeviceObject = AFSDeviceObject;

        stInitLib.AFSRDRDeviceObject = AFSRDRDeviceObject;

        stInitLib.AFSServerName = AFSServerName;

        stInitLib.AFSMountRootName = AFSMountRootName;

        stInitLib.AFSDebugFlags = AFSDebugFlags;

        if( GlobalRootFid != NULL)
        {
            stInitLib.GlobalRootFid = *GlobalRootFid;
        }

        stInitLib.AFSCacheManagerCallbacks = &AFSCacheManagerCallbacks;

        stInitLib.AFSCacheBaseAddress = pRDRDevExt->Specific.RDR.CacheBaseAddress;

        stInitLib.AFSCacheLength = pRDRDevExt->Specific.RDR.CacheLength;

        //
        // Initialize the callback functions for the library
        //

        stInitLib.AFSProcessRequest = AFSProcessRequest;

        stInitLib.AFSDbgLogMsg = AFSDbgLogMsg;

        stInitLib.AFSAddConnectionEx = AFSAddConnectionEx;

        stInitLib.AFSExAllocatePoolWithTag = AFSExAllocatePoolWithTag;

        stInitLib.AFSExFreePoolWithTag = AFSExFreePoolWithTag;

        stInitLib.AFSDumpTraceFiles = AFSDumpTraceFiles;

        stInitLib.AFSRetrieveAuthGroup = AFSRetrieveAuthGroup;

        ntStatus = AFSSendDeviceIoControl( pDevExt->Specific.Control.LibraryDeviceObject,
                                           IOCTL_AFS_INITIALIZE_LIBRARY_DEVICE,
                                           &stInitLib,
                                           sizeof( AFSLibraryInitCB),
                                           NULL,
                                           0,
                                           NULL);

        if ( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeLibrary AFSSendDeviceIoControl failure %08lX\n",
                          ntStatus);
        }
    }

    return ntStatus;
}
