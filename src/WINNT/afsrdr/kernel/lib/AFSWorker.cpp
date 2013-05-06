/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011, 2012, 2013 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
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
// File: AFSWorker.cpp
//

#include "AFSCommon.h"

static
VOID
AFSPostedDeferredWrite( IN PVOID Context1,
                        IN PVOID Context2);

//
// Function: AFSInitializeWorkerPool
//
// Description:
//
//      This function initializes the worker thread pool
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSInitializeWorkerPool()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkQueueContext        *pCurrentWorker = NULL, *pLastWorker = NULL;
    AFSDeviceExt *pDevExt = NULL;

    __Enter
    {

        pDevExt = (AFSDeviceExt *)AFSLibraryDeviceObject->DeviceExtension;

        //
        // Initialize the worker threads.
        //

        while( pDevExt->Specific.Library.WorkerCount < AFS_WORKER_COUNT)
        {

            pCurrentWorker = (AFSWorkQueueContext *)AFSLibExAllocatePoolWithTag( NonPagedPool,
                                                                                 sizeof( AFSWorkQueueContext),
                                                                                 AFS_WORKER_CB_TAG);

            if( pCurrentWorker == NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitializeWorkerPool Failed to allocate worker context\n"));

                ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                break;
            }

            RtlZeroMemory( pCurrentWorker,
                           sizeof( AFSWorkQueueContext));

            ntStatus = AFSInitWorkerThread( pCurrentWorker,
                                            (PKSTART_ROUTINE)AFSWorkerThread);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitializeWorkerPool Failed to initialize worker thread Status %08lX\n",
                              ntStatus));

		AFSLibExFreePoolWithTag( pCurrentWorker,
					 AFS_WORKER_CB_TAG);

                break;
            }

            if( pDevExt->Specific.Library.PoolHead == NULL)
            {

                pDevExt->Specific.Library.PoolHead = pCurrentWorker;
            }
            else
            {

                pLastWorker->fLink = pCurrentWorker;
            }

            pLastWorker = pCurrentWorker;

            pDevExt->Specific.Library.WorkerCount++;
        }

        //
        // If there was a failure but there is at least one worker, then go with it.
        //

        if( !NT_SUCCESS( ntStatus) &&
            pDevExt->Specific.Library.WorkerCount == 0)
        {

            try_return( ntStatus);
        }

        ntStatus = STATUS_SUCCESS;

        //
        // Now our IO Worker queue
        //

        while( pDevExt->Specific.Library.IOWorkerCount < AFS_IO_WORKER_COUNT)
        {

            pCurrentWorker = (AFSWorkQueueContext *)AFSLibExAllocatePoolWithTag( NonPagedPool,
                                                                                 sizeof( AFSWorkQueueContext),
                                                                                 AFS_WORKER_CB_TAG);

            if( pCurrentWorker == NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitializeWorkerPool Failed to allocate IO worker context\n"));

                ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                break;
            }

            RtlZeroMemory( pCurrentWorker,
                           sizeof( AFSWorkQueueContext));

            ntStatus = AFSInitWorkerThread( pCurrentWorker,
                                            (PKSTART_ROUTINE)AFSIOWorkerThread);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitializeWorkerPool Failed to initialize IO worker thread Status %08lX\n",
                              ntStatus));

		AFSLibExFreePoolWithTag( pCurrentWorker,
					 AFS_WORKER_CB_TAG);

                break;
            }

            if( pDevExt->Specific.Library.IOPoolHead == NULL)
            {

                pDevExt->Specific.Library.IOPoolHead = pCurrentWorker;
            }
            else
            {

                pLastWorker->fLink = pCurrentWorker;
            }

            pLastWorker = pCurrentWorker;

            pDevExt->Specific.Library.IOWorkerCount++;
        }

        //
        // If there was a failure but there is at least one worker, then go with it.
        //

        if( !NT_SUCCESS( ntStatus) &&
            pDevExt->Specific.Library.IOWorkerCount == 0)
        {

            try_return( ntStatus);
        }

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            //
            // Failed to initialize the pool so tear it down
            //

            AFSRemoveWorkerPool();
        }
    }

    return ntStatus;
}

//
// Function: AFSRemoveWorkerPool
//
// Description:
//
//      This function tears down the worker thread pool
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSRemoveWorkerPool()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG index = 0;
    AFSWorkQueueContext        *pCurrentWorker = NULL, *pNextWorker = NULL;
    AFSDeviceExt *pDevExt = NULL;

    pDevExt = (AFSDeviceExt *)AFSLibraryDeviceObject->DeviceExtension;

    //
    // Loop through the workers shutting them down in two stages.
    // First, clear AFS_WORKER_PROCESS_REQUESTS so that workers
    // stop processing requests.  Second, call AFSShutdownWorkerThread()
    // to wake the workers and wait for them to exit.
    //

    pCurrentWorker = pDevExt->Specific.Library.PoolHead;

    while( index < pDevExt->Specific.Library.WorkerCount)
    {

        ClearFlag( pCurrentWorker->State, AFS_WORKER_PROCESS_REQUESTS);

        pCurrentWorker = pCurrentWorker->fLink;

        if ( pCurrentWorker == NULL)
        {

            break;
        }

        index++;
    }

    pCurrentWorker = pDevExt->Specific.Library.PoolHead;

    index = 0;

    while( index < pDevExt->Specific.Library.WorkerCount)
    {

        ntStatus = AFSShutdownWorkerThread( pCurrentWorker);

        pNextWorker = pCurrentWorker->fLink;

	AFSLibExFreePoolWithTag( pCurrentWorker,
				 AFS_WORKER_CB_TAG);

        pCurrentWorker = pNextWorker;

        if( pCurrentWorker == NULL)
        {

            break;
        }

        index++;
    }

    pDevExt->Specific.Library.PoolHead = NULL;

    //
    // Loop through the IO workers shutting them down in two stages.
    // First, clear AFS_WORKER_PROCESS_REQUESTS so that workers
    // stop processing requests.  Second, call AFSShutdownIOWorkerThread()
    // to wake the workers and wait for them to exit.
    //

    pCurrentWorker = pDevExt->Specific.Library.IOPoolHead;

    index = 0;

    while( index < pDevExt->Specific.Library.IOWorkerCount)
    {

        ClearFlag( pCurrentWorker->State, AFS_WORKER_PROCESS_REQUESTS);

        pCurrentWorker = pCurrentWorker->fLink;

        if ( pCurrentWorker == NULL)
        {

            break;
        }

        index++;
    }

    pCurrentWorker = pDevExt->Specific.Library.IOPoolHead;

    index = 0;

    while( index < pDevExt->Specific.Library.IOWorkerCount)
    {

        ntStatus = AFSShutdownIOWorkerThread( pCurrentWorker);

        pNextWorker = pCurrentWorker->fLink;

	AFSLibExFreePoolWithTag( pCurrentWorker,
				 AFS_WORKER_CB_TAG);

        pCurrentWorker = pNextWorker;

        if( pCurrentWorker == NULL)
        {

            break;
        }

        index++;
    }

    pDevExt->Specific.Library.IOPoolHead = NULL;

    return ntStatus;
}

NTSTATUS
AFSInitVolumeWorker( IN AFSVolumeCB *VolumeCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkQueueContext *pWorker = &VolumeCB->NonPagedVcb->VolumeWorkerContext;
    HANDLE hThread;
    AFSDeviceExt *pControlDeviceExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;
    PKSTART_ROUTINE pStartRoutine = NULL;
    LONG lCount;

    __Enter
    {

        if ( VolumeCB != AFSGlobalRoot)
        {

            return STATUS_INVALID_PARAMETER;
        }

        pStartRoutine = AFSPrimaryVolumeWorkerThread;

        //
        // Initialize the worker thread
        //

        KeInitializeEvent( &pWorker->WorkerThreadReady,
                           NotificationEvent,
                           FALSE);

        //
        // Set the worker to process requests
        //

        pWorker->State = AFS_WORKER_PROCESS_REQUESTS;

        //
        // Launch the thread
        //

        ntStatus =  PsCreateSystemThread( &hThread,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          pStartRoutine,
                                          (void *)VolumeCB);

        if( NT_SUCCESS( ntStatus))
        {

            ObReferenceObjectByHandle( hThread,
                                       GENERIC_READ | GENERIC_WRITE,
                                       NULL,
                                       KernelMode,
                                       (PVOID *)&pWorker->WorkerThreadObject,
                                       NULL);

            ntStatus = KeWaitForSingleObject( &pWorker->WorkerThreadReady,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              NULL);

            lCount = InterlockedIncrement( &pControlDeviceExt->Specific.Control.VolumeWorkerThreadCount);

            if( lCount > 0)
            {

                KeClearEvent( &pControlDeviceExt->Specific.Control.VolumeWorkerCloseEvent);
            }

            ZwClose( hThread);
        }
    }

    return ntStatus;
}

//
// Function: AFSInitWorkerThread
//
// Description:
//
//      This function initializes a worker thread in the pool
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSInitWorkerThread( IN AFSWorkQueueContext *PoolContext,
                     IN PKSTART_ROUTINE WorkerRoutine)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    HANDLE Handle;

    //
    // INitialize the worker signal thread
    //

    KeInitializeEvent( &PoolContext->WorkerThreadReady,
                       NotificationEvent,
                       FALSE);

    //
    // Set the worker to process requests
    //

    PoolContext->State = AFS_WORKER_PROCESS_REQUESTS;

    //
    // Launch the thread
    //

    ntStatus =  PsCreateSystemThread( &Handle,
                                      0,
                                      NULL,
                                      NULL,
                                      NULL,
                                      WorkerRoutine,
                                      (void *)PoolContext);

    if( NT_SUCCESS( ntStatus))
    {

        ObReferenceObjectByHandle( Handle,
                                   GENERIC_READ | GENERIC_WRITE,
                                   NULL,
                                   KernelMode,
                                   (PVOID *)&PoolContext->WorkerThreadObject,
                                   NULL);

        ntStatus = KeWaitForSingleObject( &PoolContext->WorkerThreadReady,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);

        ZwClose( Handle);
    }

    return ntStatus;
}

NTSTATUS
AFSShutdownVolumeWorker( IN AFSVolumeCB *VolumeCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkQueueContext *pWorker = &VolumeCB->NonPagedVcb->VolumeWorkerContext;

    //
    // Clear the 'keep processing' flag
    //

    ClearFlag( pWorker->State, AFS_WORKER_PROCESS_REQUESTS);

    if( pWorker->WorkerThreadObject != NULL)
    {
        while ( BooleanFlagOn( pWorker->State, AFS_WORKER_INITIALIZED) )
        {

            ntStatus = KeWaitForSingleObject( pWorker->WorkerThreadObject,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              NULL);
        }

        ObDereferenceObject( pWorker->WorkerThreadObject);

        pWorker->WorkerThreadObject = NULL;
    }

    return ntStatus;
}

//
// Function: AFSShutdownWorkerThread
//
// Description:
//
//      This function shutsdown a worker thread in the pool
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSShutdownWorkerThread( IN AFSWorkQueueContext *PoolContext)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pControlDeviceExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

    if( PoolContext->WorkerThreadObject != NULL)
    {

        while ( BooleanFlagOn( PoolContext->State, AFS_WORKER_INITIALIZED) )
        {

            //
            // Wake up the thread if it is a sleep
            //

            KeSetEvent( &pControlDeviceExt->Specific.Control.WorkerQueueHasItems,
                        0,
                        FALSE);

            ntStatus = KeWaitForSingleObject( PoolContext->WorkerThreadObject,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              NULL);
        }

        ObDereferenceObject( PoolContext->WorkerThreadObject);

        PoolContext->WorkerThreadObject = NULL;
    }

    return ntStatus;
}

//
// Function: AFSShutdownIOWorkerThread
//
// Description:
//
//      This function shutsdown an IO worker thread in the pool
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSShutdownIOWorkerThread( IN AFSWorkQueueContext *PoolContext)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pControlDeviceExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

    if( PoolContext->WorkerThreadObject != NULL)
    {

        while ( BooleanFlagOn( PoolContext->State, AFS_WORKER_INITIALIZED) )
        {

            //
            // Wake up the thread if it is a sleep
            //

            KeSetEvent( &pControlDeviceExt->Specific.Control.IOWorkerQueueHasItems,
                        0,
                        FALSE);

            ntStatus = KeWaitForSingleObject( PoolContext->WorkerThreadObject,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              NULL);
        }

        ObDereferenceObject( PoolContext->WorkerThreadObject);

        PoolContext->WorkerThreadObject = NULL;
    }

    return ntStatus;
}

//
// Function: AFSWorkerThread
//
// Description:
//
//      This is the worker thread entry point.
//
// Return:
//
//      A status is returned for the function
//

void
AFSWorkerThread( IN PVOID Context)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkQueueContext *pPoolContext = (AFSWorkQueueContext *)Context;
    AFSWorkItem *pWorkItem;
    BOOLEAN freeWorkItem = TRUE;
    AFSDeviceExt *pControlDevExt = NULL;
    LONG lCount;

    pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

    //
    // Indicate that we are initialized and ready
    //

    KeSetEvent( &pPoolContext->WorkerThreadReady,
                0,
                FALSE);

    //
    // Indicate we are initialized
    //

    SetFlag( pPoolContext->State, AFS_WORKER_INITIALIZED);

    ntStatus = KeWaitForSingleObject( &pControlDevExt->Specific.Control.WorkerQueueHasItems,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      NULL);

    while( BooleanFlagOn( pPoolContext->State, AFS_WORKER_PROCESS_REQUESTS))
    {

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSWorkerThread Wait for queue items failed Status %08lX\n",
                          ntStatus));

            ntStatus = STATUS_SUCCESS;
        }
        else
        {

            pWorkItem = AFSRemoveWorkItem();

            if( pWorkItem == NULL)
            {

                ntStatus = KeWaitForSingleObject( &pControlDevExt->Specific.Control.WorkerQueueHasItems,
                                                  Executive,
                                                  KernelMode,
                                                  FALSE,
                                                  NULL);
            }
            else
            {

                freeWorkItem = TRUE;

                //
                // Switch on the type of work item to process
                //

                switch( pWorkItem->RequestType)
                {

                    case AFS_WORK_FLUSH_FCB:
                    {

                        ntStatus = AFSFlushExtents( pWorkItem->Specific.Fcb.Fcb,
                                                    &pWorkItem->AuthGroup);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSReleaseExtentsWithFlush( pWorkItem->Specific.Fcb.Fcb,
                                                        &pWorkItem->AuthGroup,
                                                        FALSE);
                        }

                        ASSERT( pWorkItem->Specific.Fcb.Fcb->OpenReferenceCount != 0);

                        lCount = InterlockedDecrement( &pWorkItem->Specific.Fcb.Fcb->OpenReferenceCount);

                        break;
                    }

                    case AFS_WORK_ENUMERATE_GLOBAL_ROOT:
                    {

                        AFSEnumerateGlobalRoot( NULL);

                        break;
                    }

                    case AFS_WORK_INVALIDATE_OBJECT:
                    {

                        AFSPerformObjectInvalidate( pWorkItem->Specific.Invalidate.ObjectInfo,
                                                    pWorkItem->Specific.Invalidate.InvalidateReason);

                        freeWorkItem = TRUE;

                        break;
                    }

                    case AFS_WORK_START_IOS:
                    {

                        freeWorkItem = TRUE;

                        break;
                    }

                    default:

                        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSWorkerThread Unknown request type %d\n",
                                      pWorkItem->RequestType));

                        break;
                }

                if( freeWorkItem)
                {

		    AFSExFreePoolWithTag( pWorkItem,
					  AFS_WORK_ITEM_TAG);
                }

                ntStatus = STATUS_SUCCESS;
            }
        }
    } // worker thread loop

    ClearFlag( pPoolContext->State, AFS_WORKER_INITIALIZED);

    // Wake up another worker so they too can exit

    KeSetEvent( &pControlDevExt->Specific.Control.WorkerQueueHasItems,
                0,
                FALSE);

    PsTerminateSystemThread( 0);

    return;
}

void
AFSIOWorkerThread( IN PVOID Context)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkQueueContext *pPoolContext = (AFSWorkQueueContext *)Context;
    AFSWorkItem *pWorkItem;
    BOOLEAN freeWorkItem = TRUE;
    AFSDeviceExt *pControlDevExt = NULL, *pRdrDevExt = NULL;

    pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

    //
    // Indicate that we are initialized and ready
    //

    KeSetEvent( &pPoolContext->WorkerThreadReady,
                0,
                FALSE);


    //
    // Indicate we are initialized
    //

    SetFlag( pPoolContext->State, AFS_WORKER_INITIALIZED);

    ntStatus = KeWaitForSingleObject( &pControlDevExt->Specific.Control.IOWorkerQueueHasItems,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      NULL);

    while( BooleanFlagOn( pPoolContext->State, AFS_WORKER_PROCESS_REQUESTS))
    {

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSIOWorkerThread Wait for queue items failed Status %08lX\n",
                          ntStatus));

            ntStatus = STATUS_SUCCESS;
        }
        else
        {

            pWorkItem = AFSRemoveIOWorkItem();

            if( pWorkItem == NULL)
            {

                ntStatus = KeWaitForSingleObject( &pControlDevExt->Specific.Control.IOWorkerQueueHasItems,
                                                  Executive,
                                                  KernelMode,
                                                  FALSE,
                                                  NULL);
            }
            else
            {

                freeWorkItem = TRUE;

                //
                // Switch on the type of work item to process
                //

                switch( pWorkItem->RequestType)
                {

                    case AFS_WORK_START_IOS:
                    {

                        pRdrDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

                        //
                        // The final status is in the gather io
                        //

                        ntStatus = AFSStartIos( pWorkItem->Specific.CacheAccess.CacheFileObject,
                                                pWorkItem->Specific.CacheAccess.FunctionCode,
                                                pWorkItem->Specific.CacheAccess.RequestFlags,
                                                pWorkItem->Specific.CacheAccess.IoRuns,
                                                pWorkItem->Specific.CacheAccess.RunCount,
                                                pWorkItem->Specific.CacheAccess.GatherIo);

                        //
                        // Regardless of the status we we do the complete - there may
                        // be IOs in flight
                        // Decrement the count - setting the event if we were told
                        // to. This may trigger completion.
                        //

                        AFSCompleteIo( pWorkItem->Specific.CacheAccess.GatherIo, ntStatus );

                        freeWorkItem = TRUE;

                        break;
                    }

                    case AFS_WORK_DEFERRED_WRITE:
                    {

                        ntStatus = AFSCommonWrite( pWorkItem->Specific.AsynchIo.Device,
                                                   pWorkItem->Specific.AsynchIo.Irp,
                                                   pWorkItem->Specific.AsynchIo.CallingProcess,
                                                   TRUE);

                        freeWorkItem = TRUE;

                        break;
                    }

                    default:

                        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSIOWorkerThread Unknown request type %d\n",
                                      pWorkItem->RequestType));

                        break;
                }

                if( freeWorkItem)
                {

		    AFSLibExFreePoolWithTag( pWorkItem,
					     AFS_WORK_ITEM_TAG);
                }

                ntStatus = STATUS_SUCCESS;
            }
        }
    } // worker thread loop

    ClearFlag( pPoolContext->State, AFS_WORKER_INITIALIZED);

    // Wake up another IOWorker so they too can exit

    KeSetEvent( &pControlDevExt->Specific.Control.IOWorkerQueueHasItems,
                0,
                FALSE);

    PsTerminateSystemThread( 0);

    return;
}

//
// Called with VolumeCB->ObjectInfoTree.TreeLock held exclusive.
// pCurrentObject->ObjectReferenceCount is incremented by the caller.
//
// The *pbReleaseVolumeLock is set to FALSE if the TreeLock is dropped
// before returning.
//
// pCurrentObject must either be destroyed or the reference count
// decremented before returning.
//

static void
AFSExamineObjectInfo( IN AFSObjectInfoCB * pCurrentObject,
                      IN BOOLEAN           bVolumeObject,
                      IN OUT BOOLEAN     * pbReleaseVolumeLock)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pControlDeviceExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;
    AFSDeviceExt *pRDRDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSDirectoryCB *pCurrentDirEntry = NULL, *pNextDirEntry = NULL;
    AFSObjectInfoCB *pCurrentChildObject = NULL;
    AFSVolumeCB * pVolumeCB = pCurrentObject->VolumeCB;
    LARGE_INTEGER liCurrentTime;
    LONG lCount;
    BOOLEAN bTemp;

    __Enter
    {

        ASSERT( ExIsResourceAcquiredExclusiveLite( pVolumeCB->ObjectInfoTree.TreeLock));

        switch ( pCurrentObject->FileType)
        {

        case AFS_FILE_TYPE_DIRECTORY:
        {

            if ( BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
            {

                try_return( ntStatus);
            }

            //
            // If this object is deleted then remove it from the parent, if we can
            //

            if( BooleanFlagOn( pCurrentObject->Flags, AFS_OBJECT_FLAGS_DELETED))
            {

                if ( pCurrentObject->ObjectReferenceCount == 1 &&
                     ( pCurrentObject->Fcb == NULL ||
                       pCurrentObject->Fcb->OpenReferenceCount == 0) &&
                     pCurrentObject->Specific.Directory.DirectoryNodeListHead == NULL &&
                     pCurrentObject->Specific.Directory.ChildOpenReferenceCount == 0)
                {

                    AFSAcquireExcl( &pCurrentObject->NonPagedInfo->ObjectInfoLock,
                                    TRUE);

                    if ( pCurrentObject->Fcb != NULL)
                    {

                        AFSRemoveFcb( &pCurrentObject->Fcb);
                    }

                    if( pCurrentObject->Specific.Directory.PIOCtlDirectoryCB != NULL)
                    {

                        AFSAcquireExcl( &pCurrentObject->Specific.Directory.PIOCtlDirectoryCB->ObjectInformation->NonPagedInfo->ObjectInfoLock,
                                        TRUE);

                        AFSRemoveFcb( &pCurrentObject->Specific.Directory.PIOCtlDirectoryCB->ObjectInformation->Fcb);

                        AFSReleaseResource( &pCurrentObject->Specific.Directory.PIOCtlDirectoryCB->ObjectInformation->NonPagedInfo->ObjectInfoLock);

                        lCount = AFSObjectInfoDecrement( pCurrentObject->Specific.Directory.PIOCtlDirectoryCB->ObjectInformation,
                                                         AFS_OBJECT_REFERENCE_PIOCTL);

                        AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSExamineObjectInfo Decrement count on object %p Cnt %d\n",
                                      pCurrentObject->Specific.Directory.PIOCtlDirectoryCB->ObjectInformation,
                                      lCount));

                        ASSERT( lCount == 0);

                        if ( lCount == 0)
                        {

                            AFSDeleteObjectInfo( &pCurrentObject->Specific.Directory.PIOCtlDirectoryCB->ObjectInformation);
                        }

                        ExDeleteResourceLite( &pCurrentChildObject->Specific.Directory.PIOCtlDirectoryCB->NonPaged->Lock);

			AFSExFreePoolWithTag( pCurrentChildObject->Specific.Directory.PIOCtlDirectoryCB->NonPaged,
					      AFS_DIR_ENTRY_NP_TAG);

                        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSExamineObjectInfo (pioctl) AFS_DIR_ENTRY_TAG deallocating %p\n",
                                      pCurrentObject->Specific.Directory.PIOCtlDirectoryCB));

			AFSExFreePoolWithTag( pCurrentObject->Specific.Directory.PIOCtlDirectoryCB,
					      AFS_DIR_ENTRY_TAG);

                        pCurrentObject->Specific.Directory.PIOCtlDirectoryCB = NULL;
                    }

                    AFSReleaseResource( &pCurrentObject->NonPagedInfo->ObjectInfoLock);

                    lCount = AFSObjectInfoDecrement( pCurrentObject,
                                                     AFS_OBJECT_REFERENCE_WORKER);

                    AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSExamineObjectInfo Decrement1 count on object %p Cnt %d\n",
                                  pCurrentObject,
                                  lCount));

                    AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSExamineObjectInfo Deleting deleted object %p\n",
                                  pCurrentObject));

                    //
                    // The CurrentReferenceCount must be zero or we would not
                    // have gotten this far.   It is safe to delete the ObjectInfoCB.
                    //

                    AFSDeleteObjectInfo( &pCurrentObject);
                }

                //
                // Finished processing the AFS_OBJECT_FLAGS_DELETED case.
                //

                try_return( ntStatus);
            }

            //
            // pCurrentObject not marked Deleted.
            //

            if ( pCurrentObject->Fcb != NULL &&
                 pCurrentObject->Fcb->CcbListHead != NULL)
            {

                try_return( ntStatus);
            }

            if( pCurrentObject->Specific.Directory.ChildOpenReferenceCount > 0 ||
                ( pCurrentObject->Fcb != NULL &&
                  pCurrentObject->Fcb->OpenReferenceCount > 0))
            {

                try_return( ntStatus);
            }

            if ( pCurrentObject->FileType == AFS_FILE_TYPE_DIRECTORY &&
                 pCurrentObject->Specific.Directory.DirectoryNodeListHead != NULL)
            {

                //
                // Directory Entry Processing
                //
                // First pass is performed with the TreeLock held shared.
                // If we detect any objects in use, we give up quickly without
                // making any changes and without blocking other threads.
                // The second pass is performed with the TreeLock held exclusive
                // so deletions can take place.
                //

                if( !AFSAcquireShared( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                       FALSE))
                {

                    try_return( ntStatus);
                }

                KeQueryTickCount( &liCurrentTime);

                pCurrentDirEntry = pCurrentObject->Specific.Directory.DirectoryNodeListHead;

                while( pCurrentDirEntry != NULL)
                {

                    if( pCurrentDirEntry->DirOpenReferenceCount > 0 ||
                        pCurrentDirEntry->NameArrayReferenceCount > 0)
                    {

                        AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);

                        try_return( ntStatus);
                    }

                    if ( pCurrentDirEntry->ObjectInformation != NULL)
                    {

                        if ( pCurrentDirEntry->ObjectInformation->Fcb != NULL &&
                             pCurrentDirEntry->ObjectInformation->Fcb->OpenReferenceCount > 0)
                        {

                            AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);

                            try_return( ntStatus);
                        }

                        if ( liCurrentTime.QuadPart <= pCurrentDirEntry->ObjectInformation->LastAccessCount.QuadPart ||
                             liCurrentTime.QuadPart - pCurrentDirEntry->ObjectInformation->LastAccessCount.QuadPart <
                             pControlDeviceExt->Specific.Control.ObjectLifeTimeCount.QuadPart)
                        {

                            AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);

                            try_return( ntStatus);
                        }

                        if ( pCurrentDirEntry->ObjectInformation->FileType == AFS_FILE_TYPE_DIRECTORY)
                        {

                            if ( pCurrentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeListHead != NULL)
                            {

                                break;
                            }

                            if ( pCurrentDirEntry->ObjectInformation->Specific.Directory.ChildOpenReferenceCount > 0)
                            {

                                break;
                            }
                        }

                        if ( pCurrentDirEntry->ObjectInformation->FileType == AFS_FILE_TYPE_FILE)
                        {

                            if ( pCurrentDirEntry->ObjectInformation->Fcb != NULL &&
                                 pCurrentDirEntry->ObjectInformation->Fcb->Specific.File.ExtentsDirtyCount > 0)
                            {

                                break;
                            }
                        }
                    }

                    pCurrentDirEntry = (AFSDirectoryCB *)pCurrentDirEntry->ListEntry.fLink;
                }

                AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);

                if( pCurrentDirEntry != NULL)
                {

                    try_return( ntStatus);
                }

                //
                // Attempt the second pass with the TreeLock held exclusive.
                // The the TreeLock cannot be obtained without blocking it means that
                // the directory is in active use, so do nothing.
                //

                if( AFSAcquireExcl( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                    FALSE))
                {

                    if( pCurrentObject->Specific.Directory.ChildOpenReferenceCount > 0)
                    {

                        AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);

                        try_return( ntStatus);
                    }

                    KeQueryTickCount( &liCurrentTime);

                    pCurrentDirEntry = pCurrentObject->Specific.Directory.DirectoryNodeListHead;

                    while( pCurrentDirEntry != NULL)
                    {

                        if( pCurrentDirEntry->DirOpenReferenceCount > 0)
                        {

                            break;
                        }

                        if ( pCurrentDirEntry->NameArrayReferenceCount > 0)
                        {

                            break;
                        }

                        if ( pCurrentDirEntry->ObjectInformation != NULL)
                        {

                            if ( pCurrentDirEntry->ObjectInformation->Fcb != NULL &&
                                 pCurrentDirEntry->ObjectInformation->Fcb->OpenReferenceCount > 0)
                            {

                                break;
                            }

                            if ( liCurrentTime.QuadPart <= pCurrentDirEntry->ObjectInformation->LastAccessCount.QuadPart ||
                                 liCurrentTime.QuadPart - pCurrentDirEntry->ObjectInformation->LastAccessCount.QuadPart <
                                 pControlDeviceExt->Specific.Control.ObjectLifeTimeCount.QuadPart)
                            {

                                break;
                            }

                            if ( pCurrentDirEntry->ObjectInformation->FileType == AFS_FILE_TYPE_DIRECTORY)
                            {

                                if ( pCurrentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeListHead != NULL)
                                {

                                    break;
                                }

                                if ( pCurrentDirEntry->ObjectInformation->Specific.Directory.ChildOpenReferenceCount > 0)
                                {

                                    break;
                                }
                            }

                            if ( pCurrentDirEntry->ObjectInformation->FileType == AFS_FILE_TYPE_FILE)
                            {

                                if ( pCurrentDirEntry->ObjectInformation->Fcb != NULL &&
                                     pCurrentDirEntry->ObjectInformation->Fcb->Specific.File.ExtentsDirtyCount > 0)
                                {

                                    break;
                                }
                            }
                        }

                        pCurrentDirEntry = (AFSDirectoryCB *)pCurrentDirEntry->ListEntry.fLink;
                    }

                    if( pCurrentDirEntry != NULL)
                    {

                        //
                        // At least one entry in the directory is actively in use.
                        // Drop the lock and exit without removing anything.
                        //

                        AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);

                        try_return( ntStatus);
                    }

                    //
                    // Third pass, process each directory entry and remove what we can.
                    // The VolumeCB TreeLock and the ObjectInfo TreeLock are still held exclusive.
                    //

                    pCurrentDirEntry = pCurrentObject->Specific.Directory.DirectoryNodeListHead;

                    while( pCurrentDirEntry != NULL)
                    {

                        pNextDirEntry = (AFSDirectoryCB *)pCurrentDirEntry->ListEntry.fLink;

                        //
                        // Delete the DirectoryCB which in turn removes the DIRENTRY reference
                        // count from the associated ObjectInfoCB.  The reference count held above
                        // may now be the only one left.
                        //

                        AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING | AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSExamineObjectInfo Deleting DE %wZ Object %p\n",
                                      &pCurrentDirEntry->NameInformation.FileName,
                                      pCurrentDirEntry->ObjectInformation));

                        AFSDeleteDirEntry( pCurrentObject,
                                           &pCurrentDirEntry);

                        pCurrentDirEntry = pNextDirEntry;
                    }

                    //
                    // Clear our enumerated flag on this object so we retrieve info again on next access
                    //

                    ClearFlag( pCurrentObject->Flags, AFS_OBJECT_FLAGS_DIRECTORY_ENUMERATED);

                    AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);
                }
            }
            else if ( bVolumeObject == FALSE)
            {
                //
                // No children
                //

                if( pCurrentObject->ObjectReferenceCount > 1 ||
                    pCurrentObject->Fcb != NULL &&
                    pCurrentObject->Fcb->OpenReferenceCount > 0)
                {

                    try_return( ntStatus);
                }

                AFSAcquireExcl( &pCurrentObject->NonPagedInfo->ObjectInfoLock,
                                TRUE);

                KeQueryTickCount( &liCurrentTime);

                if( pCurrentObject->ObjectReferenceCount == 1 &&
                    ( pCurrentObject->Fcb == NULL ||
                      pCurrentObject->Fcb->OpenReferenceCount == 0) &&
                    liCurrentTime.QuadPart > pCurrentObject->LastAccessCount.QuadPart &&
                    liCurrentTime.QuadPart - pCurrentObject->LastAccessCount.QuadPart >
                    pControlDeviceExt->Specific.Control.ObjectLifeTimeCount.QuadPart)
                {

                    AFSRemoveFcb( &pCurrentObject->Fcb);

                    AFSReleaseResource( &pCurrentObject->NonPagedInfo->ObjectInfoLock);

                    lCount = AFSObjectInfoDecrement( pCurrentObject,
                                                     AFS_OBJECT_REFERENCE_WORKER);

                    AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSExamineObjectInfo Decrement4 count on object %p Cnt %d\n",
                                  pCurrentObject,
                                  lCount));

                    //
                    // The Volume TreeLock is held exclusive.  Therefore, the ObjectReferenceCount
                    // cannot change.  It is therefore safe to delete the ObjectInfoCB
                    //

                    AFSDeleteObjectInfo( &pCurrentObject);
                }
                else
                {

                    AFSReleaseResource( &pCurrentObject->NonPagedInfo->ObjectInfoLock);
                }
            }

            break;
        }

        case AFS_FILE_TYPE_FILE:
        {

            if( pCurrentObject->ObjectReferenceCount > 1 ||
                pCurrentObject->Fcb != NULL &&
                pCurrentObject->Fcb->OpenReferenceCount > 0)
            {

                try_return( ntStatus);
            }

            if( pCurrentObject->Fcb != NULL)
            {

                AFSReleaseResource( pVolumeCB->ObjectInfoTree.TreeLock);

                //
                // Dropping the VolumeCB TreeLock permits the
                // pCurrentObject->ObjectReferenceCount to change.
                // But it cannot be held across the AFSCleanupFcb
                // call.
                //

                ntStatus = AFSCleanupFcb( pCurrentObject->Fcb,
                                          FALSE);

                if (!AFSAcquireExcl( pVolumeCB->ObjectInfoTree.TreeLock,
                                     FALSE))
                {

                    *pbReleaseVolumeLock = FALSE;
                }

                if ( ntStatus == STATUS_RETRY ||
                     *pbReleaseVolumeLock == FALSE)
                {

                    //
                    // The Fcb is in use.
                    //

                    try_return( ntStatus);
                }
            }

            //
            // VolumeCB is held exclusive
            //

            AFSAcquireExcl( &pCurrentObject->NonPagedInfo->ObjectInfoLock,
                            TRUE);

            KeQueryTickCount( &liCurrentTime);

            if( pCurrentObject->ObjectReferenceCount == 1 &&
                ( pCurrentObject->Fcb == NULL ||
                  ( pCurrentObject->Fcb->OpenReferenceCount == 0 &&
                    pCurrentObject->Fcb->Specific.File.ExtentsDirtyCount == 0)) &&
                liCurrentTime.QuadPart > pCurrentObject->LastAccessCount.QuadPart &&
                liCurrentTime.QuadPart - pCurrentObject->LastAccessCount.QuadPart >
                pControlDeviceExt->Specific.Control.ObjectLifeTimeCount.QuadPart)
            {

                AFSRemoveFcb( &pCurrentObject->Fcb);

                AFSReleaseResource( &pCurrentObject->NonPagedInfo->ObjectInfoLock);

                lCount = AFSObjectInfoDecrement( pCurrentObject,
                                                 AFS_OBJECT_REFERENCE_WORKER);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSExamineObjectInfo Decrement5 count on object %p Cnt %d\n",
                              pCurrentObject,
                              lCount));

                //
                // The VolumeCB TreeLock is held exclusive so the
                // ObjectReferenceCount cannot change.
                //

                AFSDeleteObjectInfo( &pCurrentObject);
            }
            else
            {

                AFSReleaseResource( &pCurrentObject->NonPagedInfo->ObjectInfoLock);
            }

            break;
        }

        default:
        {

            AFSAcquireExcl( &pCurrentObject->NonPagedInfo->ObjectInfoLock,
                            TRUE);

            KeQueryTickCount( &liCurrentTime);

            if( pCurrentObject->ObjectReferenceCount == 1 &&
                ( pCurrentObject->Fcb == NULL ||
                  pCurrentObject->Fcb->OpenReferenceCount == 0) &&
                liCurrentTime.QuadPart > pCurrentObject->LastAccessCount.QuadPart &&
                liCurrentTime.QuadPart - pCurrentObject->LastAccessCount.QuadPart >
                pControlDeviceExt->Specific.Control.ObjectLifeTimeCount.QuadPart)
            {

                AFSRemoveFcb( &pCurrentObject->Fcb);

                AFSReleaseResource( &pCurrentObject->NonPagedInfo->ObjectInfoLock);

                lCount = AFSObjectInfoDecrement( pCurrentObject,
                                                 AFS_OBJECT_REFERENCE_WORKER);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSExamineObjectInfo Decrement6 count on object %p Cnt %d\n",
                              pCurrentObject,
                              lCount));

                //
                // The VolumeCB TreeLock is held exclusive so the
                // ObjectReferenceCount cannot change.
                //

                AFSDeleteObjectInfo( &pCurrentObject);
            }
            else
            {

                AFSReleaseResource( &pCurrentObject->NonPagedInfo->ObjectInfoLock);
            }
        }
        }

      try_exit:

        if ( pCurrentObject != NULL)
        {

            lCount = AFSObjectInfoDecrement( pCurrentObject,
                                             AFS_OBJECT_REFERENCE_WORKER);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSExamineObjectInfo Decrement count on object %p Cnt %d\n",
                          pCurrentObject,
                          lCount));
        }
    }
}

//
// Called with VolumeCB->VolumeLock held shared.
//

static void
AFSExamineVolume( IN AFSVolumeCB *pVolumeCB)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSObjectInfoCB *pCurrentObject = NULL, *pNextObject = NULL;
    BOOLEAN bReleaseVolumeTreeLock = FALSE;
    BOOLEAN bVolumeObject = FALSE;
    LONG lCount;

    //
    // The Volume ObjectInfoTree TreeLock must be held exclusive to
    // prevent other threads from obtaining a reference to an ObjectInfoCB
    // via AFSFindObjectInfo() while it is being deleted.  This is
    // annoying but the alternative is to hold the TreeLock shared during
    // garbage collection and exclusive during find operations.
    //

    if( AFSAcquireExcl( pVolumeCB->ObjectInfoTree.TreeLock,
                        TRUE))
    {

        bReleaseVolumeTreeLock = TRUE;

        pCurrentObject = pVolumeCB->ObjectInfoListHead;

        lCount = AFSObjectInfoIncrement( pCurrentObject,
                                         AFS_OBJECT_REFERENCE_WORKER);

        AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSExamineVolume Increment count on object %p Cnt %d\n",
                      pCurrentObject,
                      lCount));

        pNextObject = NULL;

        while( pCurrentObject != NULL &&
               bReleaseVolumeTreeLock == TRUE)
        {

            if( pCurrentObject != &pVolumeCB->ObjectInformation)
            {

                pNextObject = (AFSObjectInfoCB *)pCurrentObject->ListEntry.fLink;

                //
                // If the end of the VolumeCB ObjectInfo List is reached, then
                // the next ObjectInformationCB to examine is the one embedded within
                // the VolumeCB itself except when the VolumeCB is the AFSGlobalRoot.
                //
                // bVolumeObject is used to indicate whether the embedded ObjectInfoCB
                // is being examined.
                //

                if( pNextObject == NULL &&
                    pVolumeCB != AFSGlobalRoot)  // Don't free up the root of the global
                {

                    pNextObject = &pVolumeCB->ObjectInformation;
                }

                bVolumeObject = FALSE;

                if ( pNextObject)
                {

                    lCount = AFSObjectInfoIncrement( pNextObject,
                                                     AFS_OBJECT_REFERENCE_WORKER);

                    AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSExamineVolume Increment count on object %p Cnt %d\n",
                                  pNextObject,
                                  lCount));
                }
            }
            else
            {

                pNextObject = NULL;

                bVolumeObject = TRUE;
            }

            AFSExamineObjectInfo( pCurrentObject, bVolumeObject, &bReleaseVolumeTreeLock);

            if ( bReleaseVolumeTreeLock == TRUE &&
                 ( ExGetExclusiveWaiterCount( pVolumeCB->ObjectInfoTree.TreeLock) > 0 ||
                   ExGetSharedWaiterCount( pVolumeCB->ObjectInfoTree.TreeLock) > 0))
            {

                AFSReleaseResource( pVolumeCB->ObjectInfoTree.TreeLock);

                bReleaseVolumeTreeLock = FALSE;
            }
            //
            // The CurrentObject is either destroyed or the reference count has been
            // dropped by AFSExamineObjectInfo().
            //

            if ( bReleaseVolumeTreeLock == FALSE)
            {

                //
                // Try to obtain the Volume's ObjectInfoTree.TreeLock after dropping
                // other locks and continue.
                //

                bReleaseVolumeTreeLock = AFSAcquireExcl( pVolumeCB->ObjectInfoTree.TreeLock,
                                                         TRUE);
            }

            pCurrentObject = pNextObject;

            pNextObject = NULL;
        }

        if ( pCurrentObject != NULL)
        {

            lCount = AFSObjectInfoDecrement( pCurrentObject,
                                             AFS_OBJECT_REFERENCE_WORKER);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSExamineVolume Decrement count on object %p Cnt %d\n",
                          pCurrentObject,
                          lCount));
        }

        if( bReleaseVolumeTreeLock)
        {

            AFSReleaseResource( pVolumeCB->ObjectInfoTree.TreeLock);
        }
    }
}

void
AFSPrimaryVolumeWorkerThread( IN PVOID Context)
{

    UNREFERENCED_PARAMETER(Context);
    AFSWorkQueueContext *pPoolContext = (AFSWorkQueueContext *)&AFSGlobalRoot->NonPagedVcb->VolumeWorkerContext;
    AFSDeviceExt *pControlDeviceExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;
    AFSDeviceExt *pRDRDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    LARGE_INTEGER DueTime;
    LONG TimeOut;
    KTIMER Timer;
    AFSVolumeCB *pVolumeCB = NULL, *pNextVolume = NULL;
    LONG lCount;

    AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSPrimaryVolumeWorkerThread Initialized\n"));

    //
    // Initialize the timer for the worker thread
    //

    DueTime.QuadPart = -(5000);

    TimeOut = 5000;

    KeInitializeTimerEx( &Timer,
                         SynchronizationTimer);

    KeSetTimerEx( &Timer,
                  DueTime,
                  TimeOut,
                  NULL);

    //
    // Indicate that we are initialized and ready
    //

    KeSetEvent( &pPoolContext->WorkerThreadReady,
                0,
                FALSE);

    //
    // Indicate we are initialized
    //

    SetFlag( pPoolContext->State, AFS_WORKER_INITIALIZED);

    while( BooleanFlagOn( pPoolContext->State, AFS_WORKER_PROCESS_REQUESTS))
    {

        KeWaitForSingleObject( &Timer,
                               Executive,
                               KernelMode,
                               FALSE,
                               NULL);

        //
        // This is the primary volume worker so it will traverse the volume list
        // looking for cleanup or volumes requiring private workers
        //

        AFSAcquireShared( &pRDRDeviceExt->Specific.RDR.VolumeListLock,
                          TRUE);

        pVolumeCB = pRDRDeviceExt->Specific.RDR.VolumeListHead;

        while( pVolumeCB != NULL)
        {

            if( pVolumeCB == AFSGlobalRoot ||
                !AFSAcquireExcl( pVolumeCB->VolumeLock,
                                 FALSE))
            {

                pVolumeCB = (AFSVolumeCB *)pVolumeCB->ListEntry.fLink;

                continue;
            }

            if( pVolumeCB->ObjectInfoListHead == NULL)
            {

                AFSReleaseResource( pVolumeCB->VolumeLock);

                AFSReleaseResource( &pRDRDeviceExt->Specific.RDR.VolumeListLock);

                AFSAcquireExcl( pRDRDeviceExt->Specific.RDR.VolumeTree.TreeLock,
                                TRUE);

                AFSAcquireExcl( &pRDRDeviceExt->Specific.RDR.VolumeListLock,
                                TRUE);

                if( !AFSAcquireExcl( pVolumeCB->VolumeLock,
                                     FALSE))
                {

                    AFSConvertToShared( &pRDRDeviceExt->Specific.RDR.VolumeListLock);

                    AFSReleaseResource( pRDRDeviceExt->Specific.RDR.VolumeTree.TreeLock);

                    pVolumeCB = (AFSVolumeCB *)pVolumeCB->ListEntry.fLink;

                    continue;
                }

                pNextVolume = (AFSVolumeCB *)pVolumeCB->ListEntry.fLink;

                AFSAcquireExcl( &pVolumeCB->ObjectInformation.NonPagedInfo->ObjectInfoLock,
                                TRUE);

                //
                // If VolumeCB is idle, the Volume can be garbage collected
                //

                if( pVolumeCB->ObjectInfoListHead == NULL &&
                    pVolumeCB->DirectoryCB->DirOpenReferenceCount <= 0 &&
                    pVolumeCB->DirectoryCB->NameArrayReferenceCount <= 0 &&
                    pVolumeCB->VolumeReferenceCount == 0 &&
                    ( pVolumeCB->RootFcb == NULL ||
                      pVolumeCB->RootFcb->OpenReferenceCount == 0) &&
                    pVolumeCB->ObjectInformation.ObjectReferenceCount <= 0)
                {

                    AFSRemoveRootFcb( pVolumeCB);

                    AFSReleaseResource( &pVolumeCB->ObjectInformation.NonPagedInfo->ObjectInfoLock);

                    AFSRemoveVolume( pVolumeCB);
                }
                else
                {

                    AFSReleaseResource( &pVolumeCB->ObjectInformation.NonPagedInfo->ObjectInfoLock);

                    AFSReleaseResource( pVolumeCB->VolumeLock);
                }

                AFSConvertToShared( &pRDRDeviceExt->Specific.RDR.VolumeListLock);

                AFSReleaseResource( pRDRDeviceExt->Specific.RDR.VolumeTree.TreeLock);

                pVolumeCB = pNextVolume;

                continue;
            }

            //
            // Don't need this lock anymore now that we have a volume cb to work with
            //

            AFSReleaseResource( &pRDRDeviceExt->Specific.RDR.VolumeListLock);

            //
            // For now we only need the volume lock shared
            //

            AFSConvertToShared( pVolumeCB->VolumeLock);

            AFSExamineVolume( pVolumeCB);

            //
            // Next volume cb
            //

            AFSReleaseResource( pVolumeCB->VolumeLock);

            AFSAcquireShared( &pRDRDeviceExt->Specific.RDR.VolumeListLock,
                              TRUE);

            pVolumeCB = (AFSVolumeCB *)pVolumeCB->ListEntry.fLink;
        }

        AFSReleaseResource( &pRDRDeviceExt->Specific.RDR.VolumeListLock);

    } // worker thread loop

    KeCancelTimer( &Timer);

    ClearFlag( pPoolContext->State, AFS_WORKER_INITIALIZED);

    AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSPrimaryVolumeWorkerThread Exiting\n"));

    lCount = InterlockedDecrement( &pControlDeviceExt->Specific.Control.VolumeWorkerThreadCount);

    if( lCount == 0)
    {

        KeSetEvent( &pControlDeviceExt->Specific.Control.VolumeWorkerCloseEvent,
                    0,
                    FALSE);
    }

    PsTerminateSystemThread( 0);

    return;
}

NTSTATUS
AFSInsertWorkitem( IN AFSWorkItem *WorkItem)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pControlDevExt = NULL;
    LONG lCount;

    pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInsertWorkitem Acquiring Control QueueLock lock %p EXCL %08lX\n",
                  &pControlDevExt->Specific.Control.QueueLock,
                  PsGetCurrentThread()));

    AFSAcquireExcl( &pControlDevExt->Specific.Control.QueueLock,
                    TRUE);

    lCount = InterlockedIncrement( &pControlDevExt->Specific.Control.QueueItemCount);

    AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInsertWorkitem Inserting work item %p Count %d\n",
                  WorkItem,
                  lCount));

    if( pControlDevExt->Specific.Control.QueueTail != NULL) // queue already has nodes
    {

        pControlDevExt->Specific.Control.QueueTail->next = WorkItem;
    }
    else // first node
    {

        pControlDevExt->Specific.Control.QueueHead = WorkItem;
    }

    WorkItem->next = NULL;
    pControlDevExt->Specific.Control.QueueTail = WorkItem;

    // indicate that the queue has nodes
    KeSetEvent( &(pControlDevExt->Specific.Control.WorkerQueueHasItems),
                0,
                FALSE);

    AFSReleaseResource( &pControlDevExt->Specific.Control.QueueLock);

    return ntStatus;
}

NTSTATUS
AFSInsertIOWorkitem( IN AFSWorkItem *WorkItem)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pControlDevExt = NULL;
    LONG lCount;

    pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInsertIOWorkitem Acquiring Control QueueLock lock %p EXCL %08lX\n",
                  &pControlDevExt->Specific.Control.IOQueueLock,
                  PsGetCurrentThread()));

    AFSAcquireExcl( &pControlDevExt->Specific.Control.IOQueueLock,
                    TRUE);

    lCount = InterlockedIncrement( &pControlDevExt->Specific.Control.IOQueueItemCount);

    AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInsertWorkitem Inserting IO work item %p Count %d\n",
                  WorkItem,
                  lCount));

    if( pControlDevExt->Specific.Control.IOQueueTail != NULL) // queue already has nodes
    {

        pControlDevExt->Specific.Control.IOQueueTail->next = WorkItem;
    }
    else // first node
    {

        pControlDevExt->Specific.Control.IOQueueHead = WorkItem;
    }

    WorkItem->next = NULL;
    pControlDevExt->Specific.Control.IOQueueTail = WorkItem;

    // indicate that the queue has nodes
    KeSetEvent( &(pControlDevExt->Specific.Control.IOWorkerQueueHasItems),
                0,
                FALSE);

    AFSReleaseResource( &pControlDevExt->Specific.Control.IOQueueLock);

    return ntStatus;
}

NTSTATUS
AFSInsertWorkitemAtHead( IN AFSWorkItem *WorkItem)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pControlDevExt = NULL;
    LONG lCount;

    pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInsertWorkitemAtHead Acquiring Control QueueLock lock %p EXCL %08lX\n",
                  &pControlDevExt->Specific.Control.QueueLock,
                  PsGetCurrentThread()));

    AFSAcquireExcl( &pControlDevExt->Specific.Control.QueueLock,
                    TRUE);

    WorkItem->next = pControlDevExt->Specific.Control.QueueHead;

    pControlDevExt->Specific.Control.QueueHead = WorkItem;

    lCount = InterlockedIncrement( &pControlDevExt->Specific.Control.QueueItemCount);

    AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInsertWorkitemAtHead Inserting work item %p Count %d\n",
                  WorkItem,
                  lCount));

    //
    // indicate that the queue has nodes
    //

    KeSetEvent( &(pControlDevExt->Specific.Control.WorkerQueueHasItems),
                0,
                FALSE);

    AFSReleaseResource( &pControlDevExt->Specific.Control.QueueLock);

    return ntStatus;
}

AFSWorkItem *
AFSRemoveWorkItem()
{

    AFSWorkItem        *pWorkItem = NULL;
    AFSDeviceExt *pControlDevExt = NULL;
    LONG lCount;

    pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSRemoveWorkItem Acquiring Control QueueLock lock %p EXCL %08lX\n",
                  &pControlDevExt->Specific.Control.QueueLock,
                  PsGetCurrentThread()));

    AFSAcquireExcl( &pControlDevExt->Specific.Control.QueueLock,
                    TRUE);

    if( pControlDevExt->Specific.Control.QueueHead != NULL) // queue has nodes
    {

        pWorkItem = pControlDevExt->Specific.Control.QueueHead;

        lCount = InterlockedDecrement( &pControlDevExt->Specific.Control.QueueItemCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveWorkItem Removing work item %p Count %d Thread %08lX\n",
                      pWorkItem,
                      lCount,
                      PsGetCurrentThreadId()));

        pControlDevExt->Specific.Control.QueueHead = pControlDevExt->Specific.Control.QueueHead->next;

        if( pControlDevExt->Specific.Control.QueueHead == NULL) // if queue just became empty
        {

            pControlDevExt->Specific.Control.QueueTail = NULL;
        }
        else
        {

            //
            // Wake up another worker
            //

            KeSetEvent( &(pControlDevExt->Specific.Control.WorkerQueueHasItems),
                        0,
                        FALSE);
        }
    }

    AFSReleaseResource( &pControlDevExt->Specific.Control.QueueLock);

    return pWorkItem;
}

AFSWorkItem *
AFSRemoveIOWorkItem()
{

    AFSWorkItem        *pWorkItem = NULL;
    AFSDeviceExt *pControlDevExt = NULL;
    LONG lCount;

    pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSRemoveIOWorkItem Acquiring Control QueueLock lock %p EXCL %08lX\n",
                  &pControlDevExt->Specific.Control.IOQueueLock,
                  PsGetCurrentThread()));

    AFSAcquireExcl( &pControlDevExt->Specific.Control.IOQueueLock,
                    TRUE);

    if( pControlDevExt->Specific.Control.IOQueueHead != NULL) // queue has nodes
    {

        pWorkItem = pControlDevExt->Specific.Control.IOQueueHead;

        lCount = InterlockedDecrement( &pControlDevExt->Specific.Control.IOQueueItemCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveWorkItem Removing work item %p Count %d Thread %08lX\n",
                      pWorkItem,
                      lCount,
                      PsGetCurrentThreadId()));

        pControlDevExt->Specific.Control.IOQueueHead = pControlDevExt->Specific.Control.IOQueueHead->next;

        if( pControlDevExt->Specific.Control.IOQueueHead == NULL) // if queue just became empty
        {

            pControlDevExt->Specific.Control.IOQueueTail = NULL;
        }
        else
        {

            //
            // Wake up another worker
            //

            KeSetEvent( &(pControlDevExt->Specific.Control.IOWorkerQueueHasItems),
                        0,
                        FALSE);
        }
    }

    AFSReleaseResource( &pControlDevExt->Specific.Control.IOQueueLock);

    return pWorkItem;
}

NTSTATUS
AFSQueueWorkerRequest( IN AFSWorkItem *WorkItem)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN bWait = BooleanFlagOn( WorkItem->RequestFlags, AFS_SYNCHRONOUS_REQUEST);

    //
    // Submit the work item to the worker
    //

    ntStatus = AFSInsertWorkitem( WorkItem);

    if( bWait)
    {

        //
        // Sync request so block on the work item event
        //

        ntStatus = KeWaitForSingleObject( &WorkItem->Event,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);
    }

    return ntStatus;
}

NTSTATUS
AFSQueueIOWorkerRequest( IN AFSWorkItem *WorkItem)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN bWait = BooleanFlagOn( WorkItem->RequestFlags, AFS_SYNCHRONOUS_REQUEST);

    //
    // Submit the work item to the worker
    //

    ntStatus = AFSInsertIOWorkitem( WorkItem);

    if( bWait)
    {

        //
        // Sync request so block on the work item event
        //

        ntStatus = KeWaitForSingleObject( &WorkItem->Event,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);
    }

    return ntStatus;
}

NTSTATUS
AFSQueueWorkerRequestAtHead( IN AFSWorkItem *WorkItem)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN bWait = BooleanFlagOn( WorkItem->RequestFlags, AFS_SYNCHRONOUS_REQUEST);

    //
    // Submit the work item to the worker
    //

    ntStatus = AFSInsertWorkitemAtHead( WorkItem);

    if( bWait)
    {

        //
        // Sync request so block on the work item event
        //

        ntStatus = KeWaitForSingleObject( &WorkItem->Event,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);
    }

    return ntStatus;
}

NTSTATUS
AFSQueueFlushExtents( IN AFSFcb *Fcb,
                      IN GUID *AuthGroup)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pRDRDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSWorkItem *pWorkItem = NULL;
    LONG lCount;

    __try
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueFlushExtents Queuing request for FID %08lX-%08lX-%08lX-%08lX\n",
                      Fcb->ObjectInformation->FileId.Cell,
                      Fcb->ObjectInformation->FileId.Volume,
                      Fcb->ObjectInformation->FileId.Vnode,
                      Fcb->ObjectInformation->FileId.Unique));

        //
        // Increment our flush count here just to keep the number of items in the
        // queue down. We'll decrement it just below.
        //

        lCount = InterlockedIncrement( &Fcb->Specific.File.QueuedFlushCount);

        if( lCount > 3)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueueFlushExtents Max queued items for FID %08lX-%08lX-%08lX-%08lX\n",
                          Fcb->ObjectInformation->FileId.Cell,
                          Fcb->ObjectInformation->FileId.Volume,
                          Fcb->ObjectInformation->FileId.Vnode,
                          Fcb->ObjectInformation->FileId.Unique));

            try_return( ntStatus);
        }

        if( BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueFlushExtents Failing request, in shutdown\n"));

            try_return( ntStatus = STATUS_TOO_LATE);
        }

        //
        // Allocate our request structure and send it to the worker
        //

        pWorkItem = (AFSWorkItem *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                             sizeof( AFSWorkItem),
                                                             AFS_WORK_ITEM_TAG);

        if( pWorkItem == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueFlushExtents Failed to allocate work item\n"));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pWorkItem,
                       sizeof( AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        pWorkItem->ProcessID = (ULONGLONG)PsGetCurrentProcessId();

        pWorkItem->RequestType = AFS_WORK_FLUSH_FCB;

        if ( AuthGroup == NULL)
        {

            RtlZeroMemory( &pWorkItem->AuthGroup,
                           sizeof( GUID));

            ntStatus = AFSRetrieveValidAuthGroup( Fcb,
                                                  NULL,
                                                  TRUE,
                                                  &pWorkItem->AuthGroup);
        }
        else
        {
            RtlCopyMemory( &pWorkItem->AuthGroup,
                           AuthGroup,
                           sizeof( GUID));
        }

        pWorkItem->Specific.Fcb.Fcb = Fcb;

        lCount = InterlockedIncrement( &Fcb->OpenReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_FCB_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueFlushExtents Increment count on Fcb %p Cnt %d\n",
                      Fcb,
                      lCount));

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueFlushExtents Workitem %p for FID %08lX-%08lX-%08lX-%08lX\n",
                      pWorkItem,
                      Fcb->ObjectInformation->FileId.Cell,
                      Fcb->ObjectInformation->FileId.Volume,
                      Fcb->ObjectInformation->FileId.Vnode,
                      Fcb->ObjectInformation->FileId.Unique));

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

try_exit:

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueFlushExtents Request complete Status %08lX FID %08lX-%08lX-%08lX-%08lX\n",
                      Fcb->ObjectInformation->FileId.Cell,
                      Fcb->ObjectInformation->FileId.Volume,
                      Fcb->ObjectInformation->FileId.Vnode,
                      Fcb->ObjectInformation->FileId.Unique,
                      ntStatus));

        //
        // Remove the count we added above
        //

        lCount = InterlockedDecrement( &Fcb->Specific.File.QueuedFlushCount);

        ASSERT( lCount >= 0);

        if( lCount == 0)
        {

            KeSetEvent( &Fcb->NPFcb->Specific.File.QueuedFlushEvent,
                        0,
                        FALSE);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            if( pWorkItem != NULL)
            {

                lCount = InterlockedDecrement( &Fcb->OpenReferenceCount);

		AFSExFreePoolWithTag( pWorkItem,
				      AFS_WORK_ITEM_TAG);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueFlushExtents Failed to queue request Status %08lX\n",
                          ntStatus));
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSQueueFlushExtents\n"));

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

NTSTATUS
AFSQueueGlobalRootEnumeration()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem *pWorkItem = NULL;

    __try
    {

        pWorkItem = (AFSWorkItem *) AFSExAllocatePoolWithTag( NonPagedPool,
                                                              sizeof(AFSWorkItem),
                                                              AFS_WORK_ITEM_TAG);
        if (NULL == pWorkItem)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueGlobalRootEnumeration Failed to allocate work item\n"));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        RtlZeroMemory( pWorkItem,
                       sizeof(AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        pWorkItem->RequestType = AFS_WORK_ENUMERATE_GLOBAL_ROOT;

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueGlobalRootEnumeration Workitem %p\n",
                      pWorkItem));

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

try_exit:

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueGlobalRootEnumeration Request complete Status %08lX\n",
                      ntStatus));

        if( !NT_SUCCESS( ntStatus))
        {

            if( pWorkItem != NULL)
            {

		AFSExFreePoolWithTag( pWorkItem,
				      AFS_WORK_ITEM_TAG);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueGlobalRootEnumeration Failed to queue request Status %08lX\n",
                          ntStatus));
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSQueueGlobalRootEnumeration\n"));

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

NTSTATUS
AFSQueueStartIos( IN PFILE_OBJECT CacheFileObject,
                  IN UCHAR FunctionCode,
                  IN ULONG RequestFlags,
                  IN AFSIoRun *IoRuns,
                  IN ULONG RunCount,
                  IN AFSGatherIo *GatherIo)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pRDRDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSWorkItem *pWorkItem = NULL;

    __try
    {

        if( BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueStartIos Failing request, in shutdown\n"));

            try_return( ntStatus = STATUS_TOO_LATE);
        }

        //
        // Allocate our request structure and send it to the worker
        //

        pWorkItem = (AFSWorkItem *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                             sizeof( AFSWorkItem),
                                                             AFS_WORK_ITEM_TAG);

        if( pWorkItem == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueStartIos Failed to allocate work item\n"));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pWorkItem,
                       sizeof( AFSWorkItem));

        KeInitializeEvent( &pWorkItem->Event,
                           NotificationEvent,
                           FALSE);

        pWorkItem->Size = sizeof( AFSWorkItem);

        pWorkItem->ProcessID = (ULONGLONG)PsGetCurrentProcessId();

        pWorkItem->RequestType = AFS_WORK_START_IOS;

        pWorkItem->Specific.CacheAccess.CacheFileObject = CacheFileObject;

        pWorkItem->Specific.CacheAccess.FunctionCode = FunctionCode;

        pWorkItem->Specific.CacheAccess.RequestFlags = RequestFlags;

        pWorkItem->Specific.CacheAccess.IoRuns = IoRuns;

        pWorkItem->Specific.CacheAccess.RunCount = RunCount;

        pWorkItem->Specific.CacheAccess.GatherIo = GatherIo;

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueStartIos Queuing IO Workitem %p\n",
                      pWorkItem));

        ntStatus = AFSQueueIOWorkerRequest( pWorkItem);

try_exit:

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueStartIos Request complete Status %08lX\n",
                      ntStatus));

        if( !NT_SUCCESS( ntStatus))
        {

            if( pWorkItem != NULL)
            {

		AFSExFreePoolWithTag( pWorkItem,
				      AFS_WORK_ITEM_TAG);
            }
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSQueueStartIos\n"));

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

NTSTATUS
AFSQueueInvalidateObject( IN AFSObjectInfoCB *ObjectInfo,
                          IN ULONG InvalidateReason)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem *pWorkItem = NULL;

    __try
    {

        pWorkItem = (AFSWorkItem *) AFSExAllocatePoolWithTag( NonPagedPool,
                                                              sizeof(AFSWorkItem),
                                                              AFS_WORK_ITEM_TAG);
        if (NULL == pWorkItem)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueInvalidateObject Failed to allocate work item\n"));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        RtlZeroMemory( pWorkItem,
                       sizeof(AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        pWorkItem->RequestType = AFS_WORK_INVALIDATE_OBJECT;

        pWorkItem->Specific.Invalidate.ObjectInfo = ObjectInfo;

        pWorkItem->Specific.Invalidate.InvalidateReason = InvalidateReason;

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueInvalidateObject Workitem %p\n",
                      pWorkItem));

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

try_exit:

        AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueInvalidateObject Request complete Status %08lX\n",
                      ntStatus));

        if( !NT_SUCCESS( ntStatus))
        {

            if( pWorkItem != NULL)
            {

		AFSExFreePoolWithTag( pWorkItem,
				      AFS_WORK_ITEM_TAG);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueInvalidateObject Failed to queue request Status %08lX\n",
                          ntStatus));
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSQueueInvalidateObject\n"));

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

NTSTATUS
AFSDeferWrite( IN PDEVICE_OBJECT DeviceObject,
               IN PFILE_OBJECT FileObject,
               IN HANDLE CallingUser,
               IN PIRP Irp,
               IN ULONG BytesToWrite,
               IN BOOLEAN bRetrying)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem *pWorkItem = NULL;

    __try
    {

        //
        // Pin the user buffer (first time round only - AFSLockSystemBuffer is
        // idempotent)
        //

        if ( NULL == AFSLockSystemBuffer( Irp, BytesToWrite ))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Could not pin user memory item\n",
                          __FUNCTION__));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        pWorkItem = (AFSWorkItem *) AFSExAllocatePoolWithTag( NonPagedPool,
                                                              sizeof(AFSWorkItem),
                                                              AFS_WORK_ITEM_TAG);

        if (NULL == pWorkItem)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to allocate work item\n",
                          __FUNCTION__));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        RtlZeroMemory( pWorkItem,
                       sizeof(AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        pWorkItem->RequestType = AFS_WORK_DEFERRED_WRITE;

        pWorkItem->Specific.AsynchIo.CallingProcess = CallingUser;

        pWorkItem->Specific.AsynchIo.Device = DeviceObject;

        pWorkItem->Specific.AsynchIo.Irp = Irp;

        AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING | AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Workitem %p\n",
                      __FUNCTION__,
                      pWorkItem));

        CcDeferWrite( FileObject, AFSPostedDeferredWrite, pWorkItem, NULL, BytesToWrite, bRetrying);

        IoMarkIrpPending(Irp);

        ntStatus = STATUS_PENDING;
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - %s \n",
                      __FUNCTION__));

        ntStatus = GetExceptionCode();
    }

try_exit:

    AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "%s complete Status %08lX\n",
                  __FUNCTION__,
                  ntStatus));

    if( !NT_SUCCESS( ntStatus))
    {

        if( pWorkItem != NULL)
        {

	    AFSExFreePoolWithTag( pWorkItem,
				  AFS_WORK_ITEM_TAG);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_ERROR,
                      "%s Failed to queue request Status %08lX\n",
                      __FUNCTION__,
                      ntStatus));
    }

    return ntStatus;
}

static
VOID
AFSPostedDeferredWrite( IN PVOID Context1,
                        IN PVOID Context2)
{
    UNREFERENCED_PARAMETER( Context2);
    NTSTATUS ntStatus;

    AFSWorkItem *pWorkItem = (AFSWorkItem *) Context1;

    AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING | AFS_SUBSYSTEM_WORKER_PROCESSING,
                  AFS_TRACE_LEVEL_ERROR,
                  "%s Workitem %p\n",
                  __FUNCTION__,
                  pWorkItem));

    ntStatus = AFSQueueIOWorkerRequest( pWorkItem);

    if (!NT_SUCCESS( ntStatus))
    {

        AFSCompleteRequest( pWorkItem->Specific.AsynchIo.Irp, ntStatus);

	AFSExFreePoolWithTag( pWorkItem,
			      AFS_WORK_ITEM_TAG);

        AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING | AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_ERROR,
                      "%s (%p) Failed to queue request Status %08lX\n",
                      __FUNCTION__,
                      pWorkItem->Specific.AsynchIo.Irp,
                      ntStatus));
    }
}
