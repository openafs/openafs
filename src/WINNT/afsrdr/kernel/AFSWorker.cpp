/*
 * Copyright (c) 2008, 2009 Kernel Drivers, LLC.
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
 * - Neither the name of Kernel Drivers, LLC nor the names of its
 *   contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission from Kernel Drivers, LLC.
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

    pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    //
    // Initialize the worker threads.
    //

    pDevExt->Specific.Control.WorkerCount = 0;

    KeInitializeEvent( &pDevExt->Specific.Control.WorkerQueueHasItems, 
                       NotificationEvent, 
                       FALSE);

    //
    // Initialize the queue resource
    //

    ExInitializeResourceLite( &pDevExt->Specific.Control.QueueLock);

    while( pDevExt->Specific.Control.WorkerCount < AFS_WORKER_COUNT)
    {

        pCurrentWorker = (AFSWorkQueueContext *)ExAllocatePoolWithTag( NonPagedPool,
                                                                       sizeof( AFSWorkQueueContext),
                                                                       AFS_WORKER_CB_TAG);

        if( pCurrentWorker == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeWorkerPool Failed to allocate worker context\n");

            ntStatus = STATUS_INSUFFICIENT_RESOURCES;

            break;
        }

        RtlZeroMemory( pCurrentWorker,
                       sizeof( AFSWorkQueueContext));

        ntStatus = AFSInitWorkerThread( pCurrentWorker);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeWorkerPool Failed to initialize worker thread Status %08lX\n", ntStatus);

            ExFreePool( pCurrentWorker);

            break;
        }

        if( pDevExt->Specific.Control.PoolHead == NULL)
        {

            pDevExt->Specific.Control.PoolHead = pCurrentWorker;
        }
        else
        {

            pLastWorker->fLink = pCurrentWorker;
        }

        pLastWorker = pCurrentWorker;

        pDevExt->Specific.Control.WorkerCount++;
    }

    //
    // If there was a failure but there is at least one worker, then go with it.
    //

    if( !NT_SUCCESS( ntStatus) &&
        pDevExt->Specific.Control.WorkerCount > 0)
    {

        ntStatus = STATUS_SUCCESS;
    }
   
    //
    // If we have any worker threads then don't fail if something has happened
    //

    if( !NT_SUCCESS( ntStatus))
    {

        // 
        // Failed to initialize the pool so tear it down
        //

        AFSRemoveWorkerPool();
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

    pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    //
    // Loop through the workers shutting them down
    //

    pCurrentWorker = pDevExt->Specific.Control.PoolHead;

    while( index < pDevExt->Specific.Control.WorkerCount)
    {

        ntStatus = AFSShutdownWorkerThread( pCurrentWorker);

        pNextWorker = pCurrentWorker->fLink;
       
        ExFreePool( pCurrentWorker);

        pCurrentWorker = pNextWorker;

        if( pCurrentWorker == NULL)
        {

            break;
        }

        index++;
    }

    pDevExt->Specific.Control.PoolHead = NULL;

    ExDeleteResourceLite( &pDevExt->Specific.Control.QueueLock);
    
    return ntStatus;
}

NTSTATUS
AFSInitVolumeWorker( IN AFSFcb *VolumeVcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSWorkQueueContext *pWorker = &VolumeVcb->Specific.VolumeRoot.VolumeWorkerContext;
    HANDLE hThread;
    AFSDeviceExt *pControlDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

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
                                          AFSVolumeWorkerThread,
                                          (void *)VolumeVcb);

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

            if( InterlockedIncrement( &pControlDeviceExt->Specific.Control.VolumeWorkerThreadCount) > 0)
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
AFSInitWorkerThread( IN AFSWorkQueueContext *PoolContext)
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
                                      AFSWorkerThread,
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
AFSShutdownVolumeWorker( IN AFSFcb *VolumeVcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSWorkQueueContext *pWorker = &VolumeVcb->Specific.VolumeRoot.VolumeWorkerContext;

    if( pWorker->WorkerThreadObject != NULL &&
        BooleanFlagOn( pWorker->State, AFS_WORKER_INITIALIZED))
    {

        //
        // Clear the 'keep processing' flag
        //

        ClearFlag( pWorker->State, AFS_WORKER_PROCESS_REQUESTS);
 
        ntStatus = KeWaitForSingleObject( pWorker->WorkerThreadObject,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);

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
//      This function shusdown a worker thread in the pool
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSShutdownWorkerThread( IN AFSWorkQueueContext *PoolContext)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    if( PoolContext->WorkerThreadObject != NULL &&
        BooleanFlagOn( PoolContext->State, AFS_WORKER_INITIALIZED))
    {

        //
        // Clear the 'keep processing' flag
        //

        ClearFlag( PoolContext->State, AFS_WORKER_PROCESS_REQUESTS);
 
        //
        // Wake up the thread if it is a sleep
        //

        KeSetEvent( &pDeviceExt->Specific.Control.WorkerQueueHasItems,
                    0,
                    FALSE);

        ntStatus = KeWaitForSingleObject( PoolContext->WorkerThreadObject,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);

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
    BOOLEAN exitThread = FALSE;
    LARGE_INTEGER DueTime;
    LONG TimeOut;
    KTIMER Timer;
    void *waitObjects[ 2];
    AFSDeviceExt *pControlDevExt = NULL;

    pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

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

    waitObjects[ 1] = &Timer;

    waitObjects[ 0] = &pControlDevExt->Specific.Control.WorkerQueueHasItems;

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

        ntStatus = KeWaitForMultipleObjects( 2,
                                             waitObjects,
                                             WaitAny,
                                             Executive,
                                             KernelMode,
                                             FALSE,
                                             NULL,
                                             NULL);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSWorkerThread Wait for queue items failed Status %08lX\n", ntStatus);
        }
        else
        {

            pWorkItem = AFSRemoveWorkItem();

            if( pWorkItem != NULL)
            {

                freeWorkItem = TRUE;

                //
                // Switch on the type of work item to process
                //

                switch( pWorkItem->RequestType)
                {

                    case AFS_WORK_REQUEST_RELEASE:
                    {

                        AFSReleaseExtentsWork( pWorkItem);

                        break;
                    }

                    case AFS_WORK_FLUSH_FCB:
                    {

                        (VOID)AFSFlushExtents( pWorkItem->Specific.Fcb.Fcb);
 
                        ASSERT( pWorkItem->Specific.Fcb.Fcb->OpenReferenceCount != 0);

                        InterlockedDecrement( &pWorkItem->Specific.Fcb.Fcb->OpenReferenceCount);

                        break;
                    }

                    case AFS_WORK_ASYNCH_READ:
                    {

                        ASSERT( pWorkItem->Specific.AsynchIo.CallingProcess != NULL);

                        (VOID) AFSCommonRead( pWorkItem->Specific.AsynchIo.Device, 
                                              pWorkItem->Specific.AsynchIo.Irp, 
                                              pWorkItem->Specific.AsynchIo.CallingProcess);

                        break;
                    }

                    case AFS_WORK_ASYNCH_WRITE:
                    {

                        ASSERT( pWorkItem->Specific.AsynchIo.CallingProcess != NULL);

                        (VOID) AFSCommonWrite( pWorkItem->Specific.AsynchIo.Device, 
                                               pWorkItem->Specific.AsynchIo.Irp,
                                               pWorkItem->Specific.AsynchIo.CallingProcess);
                        break;
                    }

                    case AFS_WORK_REQUEST_BUILD_MOUNT_POINT_TARGET:
                    {

                        pWorkItem->Status = AFSBuildMountPointTarget( pWorkItem->ProcessID,
                                                                      pWorkItem->Specific.Fcb.Fcb);

                        freeWorkItem = FALSE;

                        KeSetEvent( &pWorkItem->Event,
                                    0,
                                    FALSE);

                        break;
                    }

                    case AFS_WORK_REQUEST_BUILD_SYMLINK_TARGET_INFO:
                    {

                        AFSFcb *pParentFcb = NULL;
                        UNICODE_STRING uniFullName;
                        AFSNameArrayHdr *pNameArray = NULL;
                        WCHAR *pBuffer = NULL;

                        uniFullName.Length = 0;
                        uniFullName.Buffer = NULL;

                        //
                        // Grab the top fcb lock while performing this call, it gets released in 
                        // the call itself
                        //

                        AFSAcquireExcl( &pWorkItem->Specific.Fcb.Fcb->NPFcb->Resource,
                                        TRUE);

                        pParentFcb = pWorkItem->Specific.Fcb.Fcb->ParentFcb;

                        //
                        // Build up the full name
                        //

                        pWorkItem->Status = AFSBuildRootName( pParentFcb,
                                                              &pWorkItem->Specific.Fcb.Fcb->DirEntry->DirectoryEntry.FileName,
                                                              &uniFullName);

                        if( !NT_SUCCESS( pWorkItem->Status))
                        {

                            AFSReleaseResource( &pWorkItem->Specific.Fcb.Fcb->NPFcb->Resource);

                            freeWorkItem = FALSE;

                            KeSetEvent( &pWorkItem->Event,
                                        0,
                                        FALSE);

                            break;
                        }

                        //
                        // Save this off since we will adjust the string lower ...
                        //

                        pBuffer = uniFullName.Buffer;

                        //
                        // Build a name array for this request
                        //

                        pNameArray = AFSInitNameArray();

                        if( pNameArray == NULL)
                        {

                            ExFreePool( pBuffer);

                            AFSReleaseResource( &pWorkItem->Specific.Fcb.Fcb->NPFcb->Resource);

                            pWorkItem->Status = STATUS_INSUFFICIENT_RESOURCES;

                            freeWorkItem = FALSE;

                            KeSetEvent( &pWorkItem->Event,
                                        0,
                                        FALSE);

                            break;
                        }

                        //
                        // We populate up to the current parent, if they are different
                        //

                        if( pWorkItem->Specific.Fcb.NameArray == NULL)
                        {

                            pWorkItem->Status = AFSPopulateNameArray( pNameArray,
                                                                      &uniFullName,
                                                                      pParentFcb);
                        }
                        else
                        {

                            pWorkItem->Status = AFSPopulateNameArrayFromRelatedArray( pNameArray,
                                                                                      pWorkItem->Specific.Fcb.NameArray,
                                                                                      pParentFcb);

                        }

                        if( !NT_SUCCESS( pWorkItem->Status))
                        {

                            AFSFreeNameArray( pNameArray);

                            ExFreePool( pBuffer);

                            AFSReleaseResource( &pWorkItem->Specific.Fcb.Fcb->NPFcb->Resource);

                            freeWorkItem = FALSE;

                            KeSetEvent( &pWorkItem->Event,
                                        0,
                                        FALSE);

                            break;
                        }

                        //
                        // The name array does not include the current entry so we need
                        // to add this entry in
                        //

                        if( pParentFcb != NULL)
                        {

                            pWorkItem->Status = AFSInsertNextElement( pNameArray,
                                                                      pParentFcb);
                        }

                        if( !NT_SUCCESS( pWorkItem->Status))
                        {

                            AFSFreeNameArray( pNameArray);

                            ExFreePool( pBuffer);

                            AFSReleaseResource( &pWorkItem->Specific.Fcb.Fcb->NPFcb->Resource);

                            freeWorkItem = FALSE;

                            KeSetEvent( &pWorkItem->Event,
                                        0,
                                        FALSE);

                            break;
                        }

                        //
                        // Go get the target ...
                        //

                        pWorkItem->Status = AFSBuildSymLinkTarget( pWorkItem->ProcessID,
                                                                   pWorkItem->Specific.Fcb.Fcb,
                                                                   NULL,
                                                                   pNameArray,
                                                                   AFS_LOCATE_FLAGS_NO_MP_TARGET_EVAL,
                                                                   &pWorkItem->Specific.Fcb.FileInfo,
                                                                   NULL);

                        ASSERT(!ExIsResourceAcquiredLite( &pWorkItem->Specific.Fcb.Fcb->NPFcb->Resource));

                        AFSFreeNameArray( pNameArray);

                        ExFreePool( pBuffer);

                        freeWorkItem = FALSE;

                        KeSetEvent( &pWorkItem->Event,
                                    0,
                                    FALSE);

                        break;
                    }

                    case AFS_WORK_ENUMERATE_GLOBAL_ROOT:
                    {

                        AFSEnumerateGlobalRoot();

                        break;
                    }

                    default:

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSWorkerThread Unknown request type %d\n", pWorkItem->RequestType);

                        break;
                }

                if( freeWorkItem)
                {

                    ExFreePoolWithTag( pWorkItem, AFS_WORK_ITEM_TAG);
                }
            }
        }
    } // worker thread loop

    KeCancelTimer( &Timer);

    PsTerminateSystemThread( 0);

    return;
}

void
AFSVolumeWorkerThread( IN PVOID Context)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pVolumeVcb = (AFSFcb * )Context;
    AFSWorkQueueContext *pPoolContext = (AFSWorkQueueContext *)&pVolumeVcb->Specific.VolumeRoot.VolumeWorkerContext;
    AFSDeviceExt *pControlDeviceExt = NULL;
    AFSDeviceExt *pRDRDeviceExt = NULL;
    BOOLEAN exitThread = FALSE;
    LARGE_INTEGER DueTime;
    LONG TimeOut;
    KTIMER Timer;
    BOOLEAN bDirtyData = FALSE;
    UNICODE_STRING      uniUnknownName;

    RtlInitUnicodeString( &uniUnknownName, L"<unknown>");

    pControlDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    pRDRDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

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

        ntStatus = KeWaitForSingleObject( &Timer,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);

        if( !NT_SUCCESS( ntStatus))
        {
            
            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSVolumeWorkerThread Wait for queue items failed Status %08lX\n", ntStatus);
        }
        else
        {

            AFSFcb *pFcb = NULL, *pNextFcb = NULL;
            LARGE_INTEGER liTime;

            //
            // First step is to look for potential Fcbs to be removed
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSVolumeWorkerThread Acquiring VolumeRoot FcbListLock lock %08lX SHARED %08lX\n",
                          pVolumeVcb->Specific.VolumeRoot.FcbListLock,
                          PsGetCurrentThread());

            AFSAcquireShared( pVolumeVcb->Specific.VolumeRoot.FcbListLock,
                              TRUE);

            pFcb = pVolumeVcb->Specific.VolumeRoot.FcbListHead;

            bDirtyData = FALSE;

            while( pFcb != NULL)
            {

                //
                // If we are in shutdown mode then only process data files and try to flush ALL
                // dirty data out on each of them
                //

                if( BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
                {

                    AFSWaitOnQueuedReleases();

                    if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB &&
                        !BooleanFlagOn( pFcb->Flags, AFS_FCB_INVALID) &&
                        AFSAcquireExcl( &pFcb->NPFcb->Resource, FALSE))
                    {

                        //
                        // Wait for any currently running flush or release requests to complete
                        //

                        AFSWaitOnQueuedFlushes( pFcb);

                        //
                        // Now perform another flush on the file
                        //

                        AFSFlushExtents( pFcb);

                        //
                        // If there is any more dirty extents on this file then we'll
                        // keep going
                        //

                        if( pFcb->Specific.File.ExtentsDirtyCount > 0)
                        {

                            bDirtyData = TRUE;
                        }

                        AFSReleaseResource( &pFcb->NPFcb->Resource);
                    }

                    pFcb = (AFSFcb *)pFcb->ListEntry.fLink;

                    continue;
                }

                KeQueryTickCount( &liTime);

                //
                // First up are there dirty extents in the cache to flush?
                //
                
                if (pFcb->Header.NodeTypeCode == AFS_FILE_FCB &&
                    !BooleanFlagOn( pFcb->Flags, AFS_FCB_INVALID) &&
                    pFcb->Specific.File.ExtentsDirtyCount &&
                    (liTime.QuadPart - pFcb->Specific.File.LastServerFlush.QuadPart) 
                                        >= pControlDeviceExt->Specific.Control.FcbFlushTimeCount.QuadPart)
                {

                    AFSFlushExtents( pFcb);
                }
                else if (pFcb->Header.NodeTypeCode == AFS_FILE_FCB &&
                         BooleanFlagOn( pFcb->Flags, AFS_FCB_INVALID)) 
                {
                    
                    //
                    // The file has been marked as invalid.  Dump it
                    //

                    AFSTearDownFcbExtents( pFcb );
                }

                //
                // If there are extents and they haven't been used recently *and*
                // are not being used
                //
                if (pFcb->Header.NodeTypeCode == AFS_FILE_FCB &&
                    0 != pFcb->Specific.File.ExtentCount &&
                    0 == pFcb->NPFcb->Specific.File.ExtentsRefCount &&
                    0 != pFcb->Specific.File.LastExtentAccess.QuadPart &&
                    (liTime.QuadPart - pFcb->Specific.File.LastExtentAccess.QuadPart) >= 
                                (AFS_SERVER_PURGE_SLEEP * pControlDeviceExt->Specific.Control.FcbPurgeTimeCount.QuadPart) &&
                    AFSAcquireExcl( &pFcb->NPFcb->Resource, FALSE) )
                {

                    __try
                    {

                        CcPurgeCacheSection( &pFcb->NPFcb->SectionObjectPointers,
                                             NULL,
                                             0,
                                             TRUE);
                    }
                    __except( EXCEPTION_EXECUTE_HANDLER)
                    {
                                    
                    }

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    //
                    // Tear em down we'll not be needing them again
                    //

                    AFSTearDownFcbExtents( pFcb );
                }

                //
                // If we are below our threshold for memory
                // allocations then just continue on
                //

                if( !BooleanFlagOn( pVolumeVcb->Flags, AFS_FCB_VOLUME_OFFLINE) &&
                    !BooleanFlagOn( pFcb->Flags, AFS_FCB_INVALID) &&
                    0 != pFcb->Specific.File.ExtentCount )
                    //&&
                    //AFSAllocationMemoryLevel <= 1024000 * 5) // We keep it to 5 MB
                {

                    pFcb = (AFSFcb *)pFcb->ListEntry.fLink;

                    continue;
                }

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSVolumeWorkerThread Attempt to acquire Fcb lock %08lX SHARED %08lX\n",
                              &pFcb->NPFcb->Resource,
                              PsGetCurrentThread());

                if( AFSAcquireShared( &pFcb->NPFcb->Resource,
                                      BooleanFlagOn( pVolumeVcb->Flags, AFS_FCB_VOLUME_OFFLINE)))
                {

                    KeQueryTickCount( &liTime);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVolumeWorkerThread Acquired Fcb lock %08lX SHARED %08lX\n",
                                  &pFcb->NPFcb->Resource,
                                  PsGetCurrentThread());

                    //
                    // If this is a directory ensure the child reference count is zero
                    //

                    if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB &&
                        pFcb->Specific.Directory.ChildReferenceCount > 0)
                    {

                        AFSReleaseResource( &pFcb->NPFcb->Resource);

                        pFcb = (AFSFcb *)pFcb->ListEntry.fLink;

                        continue;
                    }

                    //
                    // Check if this is a potential candidate
                    //

                    if( pFcb->OpenReferenceCount == 0 &&
                        ((liTime.QuadPart - pFcb->LastAccessCount.QuadPart) 
                                        >= pControlDeviceExt->Specific.Control.FcbLifeTimeCount.QuadPart) ||
                        BooleanFlagOn( pFcb->Flags, AFS_FCB_INVALID))
                    {

                        //
                        // Possible candidate so drop the locks and re-acquire them excl
                        //

                        AFSReleaseResource( pVolumeVcb->Specific.VolumeRoot.FcbListLock);

                        AFSReleaseResource( &pFcb->NPFcb->Resource);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSVolumeWorkerThread Acquiring VolumeRoot FcbListLock lock %08lX EXCL %08lX\n",
                                      pVolumeVcb->Specific.VolumeRoot.FcbListLock,
                                      PsGetCurrentThread());

                        AFSAcquireExcl( pVolumeVcb->Specific.VolumeRoot.FcbListLock,
                                        TRUE);

                        //
                        // Need to acquire the parent shared so we do not collide
                        // while updating the DirEntry
                        //

                        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSVolumeWorkerThread Attempt to acquire ParentFcb lock %08lX SHARED %08lX\n",
                                      &pFcb->ParentFcb->NPFcb->Resource,
                                      PsGetCurrentThread());

                        if( AFSAcquireShared( &pFcb->ParentFcb->NPFcb->Resource,
                                              BooleanFlagOn( pVolumeVcb->Flags, AFS_FCB_VOLUME_OFFLINE)))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSVolumeWorkerThread Acquired ParentFcb lock %08lX SHARED %08lX\n",
                                          &pFcb->ParentFcb->NPFcb->Resource,
                                          PsGetCurrentThread());

                            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSVolumeWorkerThread Attempt to acquire Fcb lock %08lX EXCL %08lX\n",
                                          &pFcb->NPFcb->Resource,
                                          PsGetCurrentThread());

                            if( AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                                BooleanFlagOn( pVolumeVcb->Flags, AFS_FCB_VOLUME_OFFLINE)))
                            {

                                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                              AFS_TRACE_LEVEL_VERBOSE,
                                              "AFSVolumeWorkerThread Acquired Fcb lock %08lX EXCL %08lX\n",
                                              &pFcb->NPFcb->Resource,
                                              PsGetCurrentThread());

                                //
                                // If this is a directory, check the child count again
                                //

                                if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB &&
                                    pFcb->Specific.Directory.ChildReferenceCount > 0)
                                {

                                    AFSReleaseResource( &pFcb->ParentFcb->NPFcb->Resource);

                                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                                    pFcb = (AFSFcb *)pFcb->ListEntry.fLink;

                                    continue;
                                }

                                //
                                // Check the counts again and remove it if it passes
                                //

                                if( pFcb->OpenReferenceCount == 0 &&
                                    ((liTime.QuadPart - pFcb->LastAccessCount.QuadPart) 
                                                    >= pControlDeviceExt->Specific.Control.FcbLifeTimeCount.QuadPart) ||
                                    BooleanFlagOn( pFcb->Flags, AFS_FCB_INVALID))
                                {

                                    //
                                    // We do this here to ensure there isn't an outstanding link reference on the Fcb
                                    //

                                    if( BooleanFlagOn( pFcb->Flags, AFS_FCB_INSERTED_ID_TREE))
                                    {

                                        //
                                        // Grab the FileID Tree lock
                                        //

                                        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                                      AFS_TRACE_LEVEL_VERBOSE,
                                                      "AFSVolumeWorkerThread Acquiring VolumeRoot FileIDTree.TreeLock lock %08lX EXCL %08lX\n",
                                                      pVolumeVcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                                      PsGetCurrentThread());

                                        AFSAcquireExcl( pVolumeVcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                                        TRUE);

                                        //
                                        // Check if the lock count is still zero
                                        //

                                        if( pFcb->OpenReferenceCount > 0)
                                        {

                                            //
                                            // Don't touch the Fcb
                                            //

                                            AFSReleaseResource( pVolumeVcb->Specific.VolumeRoot.FileIDTree.TreeLock);

                                            AFSReleaseResource( &pFcb->ParentFcb->NPFcb->Resource);

                                            AFSReleaseResource( &pFcb->NPFcb->Resource);

                                            pFcb = (AFSFcb *)pFcb->ListEntry.fLink;

                                            continue;
                                        }

                                        AFSRemoveHashEntry( &pVolumeVcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                                            &pFcb->TreeEntry);

                                        ClearFlag( pFcb->Flags, AFS_FCB_INSERTED_ID_TREE);

                                        AFSReleaseResource( pVolumeVcb->Specific.VolumeRoot.FileIDTree.TreeLock);
                                    }

                                    //
                                    // If this is a file then tear down extents now
                                    //

                                    if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                                    {

                                        __try
                                        {

                                            CcPurgeCacheSection( &pFcb->NPFcb->SectionObjectPointers,
                                                                 NULL,
                                                                 0,
                                                                 TRUE);
                                        }
                                        __except( EXCEPTION_EXECUTE_HANDLER)
                                        {
                                        
                                        }

                                        (VOID)AFSTearDownFcbExtents( pFcb);
                                    }

                                    //
                                    // Make sure the dir entry reference is removed
                                    //

                                    if( pFcb->DirEntry != NULL)
                                    {

                                        pFcb->DirEntry->Fcb = NULL;

                                        if( BooleanFlagOn( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE))
                                        {

                                            if( BooleanFlagOn( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_NAME_BUFFER))
                                            {

                                                ExFreePool( pFcb->DirEntry->DirectoryEntry.FileName.Buffer);
                                            }

                                            if( BooleanFlagOn( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER))
                                            {

                                                ExFreePool( pFcb->DirEntry->DirectoryEntry.TargetName.Buffer);
                                            }

                                            //
                                            // Free up the dir entry
                                            //

                                            AFSAllocationMemoryLevel -= sizeof( AFSDirEntryCB) + 
                                                                                pFcb->DirEntry->DirectoryEntry.FileName.Length +
                                                                                pFcb->DirEntry->DirectoryEntry.TargetName.Length;
                                            ExFreePool( pFcb->DirEntry);

                                            pFcb->DirEntry = NULL;
                                        }
                                    }

                                    //
                                    // Need to deallocate the Fcb and remove it from the directory entry
                                    //

                                    AFSReleaseResource( &pFcb->ParentFcb->NPFcb->Resource);

                                    if( pFcb->ListEntry.fLink == NULL)
                                    {

                                        pVolumeVcb->Specific.VolumeRoot.FcbListTail = (AFSFcb *)pFcb->ListEntry.bLink;

                                        if( pVolumeVcb->Specific.VolumeRoot.FcbListTail != NULL)
                                        {

                                            pVolumeVcb->Specific.VolumeRoot.FcbListTail->ListEntry.fLink = NULL;
                                        }
                                    }
                                    else
                                    {

                                        ((AFSFcb *)(pFcb->ListEntry.fLink))->ListEntry.bLink = pFcb->ListEntry.bLink;
                                    }

                                    if( pFcb->ListEntry.bLink == NULL)
                                    {

                                        pVolumeVcb->Specific.VolumeRoot.FcbListHead = (AFSFcb *)pFcb->ListEntry.fLink;

                                        if( pVolumeVcb->Specific.VolumeRoot.FcbListHead != NULL)
                                        {

                                            pVolumeVcb->Specific.VolumeRoot.FcbListHead = NULL;
                                        }
                                    }
                                    else
                                    {

                                        ((AFSFcb *)(pFcb->ListEntry.bLink))->ListEntry.fLink = pFcb->ListEntry.fLink;
                                    }

                                    InterlockedDecrement( &pFcb->ParentFcb->Specific.Directory.ChildReferenceCount);

                                    pNextFcb = (AFSFcb *)pFcb->ListEntry.fLink;                                    

                                    //
                                    // If this is a directory then we will need to purge the directory entry list
                                    //

                                    if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
                                    {

                                        AFSDirEntryCB *pCurrentDirEntry = NULL;

                                        pCurrentDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;

                                        while( pCurrentDirEntry != NULL)
                                        {

                                            //
                                            // If this dir entry is a mount point then we will need to 
                                            // dereference the target Fcb
                                            //

                                            if( pCurrentDirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT &&
                                                pCurrentDirEntry->Fcb != NULL)
                                            {

                                                ASSERT( pCurrentDirEntry->Fcb->OpenReferenceCount != 0);

                                                InterlockedDecrement( &pCurrentDirEntry->Fcb->OpenReferenceCount);
                                            }

                                            DbgPrint("AFSVolumeWorker Deleting Entry %08lX %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                                      pCurrentDirEntry,
                                                      &pCurrentDirEntry->DirectoryEntry.FileName,
                                                      pCurrentDirEntry->DirectoryEntry.FileId.Cell,
                                                      pCurrentDirEntry->DirectoryEntry.FileId.Volume,
                                                      pCurrentDirEntry->DirectoryEntry.FileId.Vnode,
                                                      pCurrentDirEntry->DirectoryEntry.FileId.Unique);

                                            AFSDeleteDirEntry( pFcb,
                                                               pCurrentDirEntry);

                                            pCurrentDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;
                                        }
                                    }
                                    else
                                    {


                                    }

                                    AFSRemoveFcb( pFcb);

                                    pFcb = pNextFcb;
                                }
                                else
                                {

                                    AFSReleaseResource( &pFcb->ParentFcb->NPFcb->Resource);

                                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                                    pFcb = (AFSFcb *)pFcb->ListEntry.fLink;
                                }
                            }
                            else
                            {

                                AFSReleaseResource( &pFcb->ParentFcb->NPFcb->Resource);

                                pFcb = (AFSFcb *)pFcb->ListEntry.fLink;
                            }                            
                        }
                        else
                        {

                            pFcb = (AFSFcb *)pFcb->ListEntry.fLink;
                        }

                        //
                        // Go get the listlock shared again
                        //

                        AFSConvertToShared( pVolumeVcb->Specific.VolumeRoot.FcbListLock);
                    }
                    else
                    {

                        AFSReleaseResource( &pFcb->NPFcb->Resource);
                      
                        pFcb = (AFSFcb *)pFcb->ListEntry.fLink;
                    }
                }
                else
                {

                    pFcb = (AFSFcb *)pFcb->ListEntry.fLink;
                }
            }

            AFSReleaseResource( pVolumeVcb->Specific.VolumeRoot.FcbListLock);

            //
            // If we are in shutdown mode and the dirty flag is clear then get out now
            //

            if( BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN) &&
                !bDirtyData)
            {

                break;
            }
        }
    } // worker thread loop

    if( !BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
    {

        AFSRemoveRootFcb( pVolumeVcb,
                          FALSE);
    }

    KeCancelTimer( &Timer);

    if( InterlockedDecrement( &pControlDeviceExt->Specific.Control.VolumeWorkerThreadCount) == 0)
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
    AFSDeviceExt *pDevExt = NULL;

    pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInsertWorkitem Acquiring Control QueueLock lock %08lX EXCL %08lX\n",
                  &pDevExt->Specific.Control.QueueLock,
                  PsGetCurrentThread());

    AFSAcquireExcl( &pDevExt->Specific.Control.QueueLock,
                    TRUE);

    AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInsertWorkitem Inserting work item %08lX Count %08lX\n",
                  WorkItem,
                  InterlockedIncrement( &pDevExt->Specific.Control.QueueItemCount));        

    if( pDevExt->Specific.Control.QueueTail != NULL) // queue already has nodes
    {
            
        pDevExt->Specific.Control.QueueTail->next = WorkItem;
    }
    else // first node
    {
            
        pDevExt->Specific.Control.QueueHead = WorkItem;
    }
        
    WorkItem->next = NULL;
    pDevExt->Specific.Control.QueueTail = WorkItem;

    // indicate that the queue has nodes
    KeSetEvent( &(pDevExt->Specific.Control.WorkerQueueHasItems), 
                0, 
                FALSE);

    AFSReleaseResource( &pDevExt->Specific.Control.QueueLock);

    return ntStatus;
}

NTSTATUS
AFSInsertWorkitemAtHead( IN AFSWorkItem *WorkItem)
{
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = NULL;

    pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInsertWorkitemAtHead Acquiring Control QueueLock lock %08lX EXCL %08lX\n",
                  &pDevExt->Specific.Control.QueueLock,
                  PsGetCurrentThread());

    AFSAcquireExcl( &pDevExt->Specific.Control.QueueLock,
                    TRUE);

    WorkItem->next = pDevExt->Specific.Control.QueueHead;

    pDevExt->Specific.Control.QueueHead = WorkItem;

    AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInsertWorkitemAtHead Inserting work item %08lX Count %08lX\n",
                  WorkItem,
                  InterlockedIncrement( &pDevExt->Specific.Control.QueueItemCount));        

    //
    // indicate that the queue has nodes
    //

    KeSetEvent( &(pDevExt->Specific.Control.WorkerQueueHasItems), 
                0, 
                FALSE);

    AFSReleaseResource( &pDevExt->Specific.Control.QueueLock);

    return ntStatus;
}

AFSWorkItem *
AFSRemoveWorkItem()
{
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem        *pWorkItem = NULL;
    AFSDeviceExt *pDevExt = NULL;

    pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSRemoveWorkItem Acquiring Control QueueLock lock %08lX EXCL %08lX\n",
                  &pDevExt->Specific.Control.QueueLock,
                  PsGetCurrentThread());

    AFSAcquireExcl( &pDevExt->Specific.Control.QueueLock,
                    TRUE);

    if( pDevExt->Specific.Control.QueueHead != NULL) // queue has nodes
    {
            
        pWorkItem = pDevExt->Specific.Control.QueueHead;
            
        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveWorkItem Removing work item %08lX Count %08lX Thread %08lX\n",
                      pWorkItem,
                      InterlockedDecrement( &pDevExt->Specific.Control.QueueItemCount),
                      PsGetCurrentThreadId());        

        pDevExt->Specific.Control.QueueHead = pDevExt->Specific.Control.QueueHead->next;
            
        if( pDevExt->Specific.Control.QueueHead == NULL) // if queue just became empty
        {
                
            pDevExt->Specific.Control.QueueTail = NULL;

            KeResetEvent(&(pDevExt->Specific.Control.WorkerQueueHasItems));
        }
    }

    AFSReleaseResource( &pDevExt->Specific.Control.QueueLock);

    return pWorkItem;
}

NTSTATUS
AFSQueueWorkerRequest( IN AFSWorkItem *WorkItem)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = NULL;

    //
    // Submit the work item to the worker
    //

    ntStatus = AFSInsertWorkitem( WorkItem);

    if( BooleanFlagOn( WorkItem->RequestFlags, AFS_SYNCHRONOUS_REQUEST))
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
    AFSDeviceExt *pDevExt = NULL;

    //
    // Submit the work item to the worker
    //

    ntStatus = AFSInsertWorkitemAtHead( WorkItem);

    if( BooleanFlagOn( WorkItem->RequestFlags, AFS_SYNCHRONOUS_REQUEST))
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
AFSQueueBuildMountPointTarget( IN AFSFcb *Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem *pWorkItem = NULL;
    
    __try
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueBuildMountPointTarget Queuing request for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique);        

        //
        // Allocate our request structure and send it to the worker
        //

        pWorkItem = (AFSWorkItem *)ExAllocatePoolWithTag( NonPagedPool,
                                                          sizeof( AFSWorkItem),
                                                          AFS_WORK_ITEM_TAG);

        if( pWorkItem == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueBuildMountPointTarget Failed to allocate work item\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pWorkItem,
                       sizeof( AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        //
        // This will be a synchronous request
        //

        KeInitializeEvent( &pWorkItem->Event,
                           NotificationEvent,
                           FALSE);

        pWorkItem->ProcessID = (ULONGLONG)PsGetCurrentProcessId();

        pWorkItem->RequestFlags = AFS_SYNCHRONOUS_REQUEST;

        pWorkItem->RequestType = AFS_WORK_REQUEST_BUILD_MOUNT_POINT_TARGET;

        pWorkItem->Specific.Fcb.Fcb = Fcb;

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueBuildMountPointTarget Workitem %08lX for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      pWorkItem,
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique);        

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

        if( NT_SUCCESS( ntStatus))
        {

            ntStatus = pWorkItem->Status;
        }

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueBuildMountPointTarget Processed request for %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                      ntStatus);

        if( pWorkItem != NULL)
        {

            ExFreePool( pWorkItem);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueBuildMountPointTarget Failed to queue request Status %08lX\n", ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSQueueBuildMountPointTarget\n");
    }

    return ntStatus;
}

NTSTATUS
AFSQueueBuildSymLinkTarget( IN AFSFcb *Fcb,
                            IN AFSNameArrayHdr *NameArray,
                            OUT AFSFileInfoCB *FileInformation,
                            OUT AFSFcb **TargetFcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem *pWorkItem = NULL;
    
    __try
    {

        // 
        // Do not queue a build symlink target request if the target is being returned.
        // otherwise resources end up held by the worker thread instead of the caller.
        //

        if( FileInformation == NULL ||
            TargetFcb != NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueBuildSymLinkTarget Queuing request for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique);        

        //
        // Allocate our request structure and send it to the worker
        //

        pWorkItem = (AFSWorkItem *)ExAllocatePoolWithTag( NonPagedPool,
                                                          sizeof( AFSWorkItem),
                                                          AFS_WORK_ITEM_TAG);

        if( pWorkItem == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueBuildSymLinkTarget Failed to allocate work item\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pWorkItem,
                       sizeof( AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        //
        // This will be a synchronous request
        //

        KeInitializeEvent( &pWorkItem->Event,
                           NotificationEvent,
                           FALSE);

        pWorkItem->ProcessID = (ULONGLONG)PsGetCurrentProcessId();

        pWorkItem->RequestFlags = AFS_SYNCHRONOUS_REQUEST;

        pWorkItem->RequestType = AFS_WORK_REQUEST_BUILD_SYMLINK_TARGET_INFO;

        pWorkItem->Specific.Fcb.Fcb = Fcb;

        pWorkItem->Specific.Fcb.TargetFcb = TargetFcb;

        pWorkItem->Specific.Fcb.NameArray = NameArray;

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueBuildSymLinkTarget Workitem %08lX for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      pWorkItem,
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique);        

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

        if( NT_SUCCESS( ntStatus))
        {

            ntStatus = pWorkItem->Status;

            if( NT_SUCCESS( ntStatus) &&
                ntStatus != STATUS_REPARSE)
            {

                RtlCopyMemory( FileInformation,
                               &pWorkItem->Specific.Fcb.FileInfo,
                               sizeof( AFSFileInfoCB));
            }
            else
            {

                ntStatus = STATUS_INVALID_PARAMETER;
            }
        }

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueBuildSymLinkTarget Processed request for %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                      ntStatus);

        if( pWorkItem != NULL)
        {

            ExFreePool( pWorkItem);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueBuildSymLinkTarget Failed to queue request Status %08lX\n", ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSQueueBuildSymLinkTarget\n");
    }

    return ntStatus;
}

NTSTATUS
AFSQueueFlushExtents( IN AFSFcb *Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pRDRDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSWorkItem *pWorkItem = NULL;
    
    __try
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueFlushExtents Queuing request for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique);        

        if( Fcb->Specific.File.QueuedFlushCount > 3)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueueFlushExtents Max queued items for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                          &Fcb->DirEntry->DirectoryEntry.FileName,
                          Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                          Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                          Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                          Fcb->DirEntry->DirectoryEntry.FileId.Unique);        

            try_return( ntStatus);
        }     

        if( BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueFlushExtents Failing request, in shutdown\n");

            try_return( ntStatus = STATUS_TOO_LATE);
        }

        //
        // Allocate our request structure and send it to the worker
        //

        pWorkItem = (AFSWorkItem *)ExAllocatePoolWithTag( NonPagedPool,
                                                          sizeof( AFSWorkItem),
                                                          AFS_WORK_ITEM_TAG);

        if( pWorkItem == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueFlushExtents Failed to allocate work item\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pWorkItem,
                       sizeof( AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        //
        // This will be a synchronous request
        //

        KeInitializeEvent( &pWorkItem->Event,
                           NotificationEvent,
                           FALSE);

        pWorkItem->ProcessID = (ULONGLONG)PsGetCurrentProcessId();

        pWorkItem->RequestType = AFS_WORK_FLUSH_FCB;

        pWorkItem->Specific.Fcb.Fcb = Fcb;

        InterlockedIncrement( &Fcb->OpenReferenceCount);

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueFlushExtents Workitem %08lX for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      pWorkItem,
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique);        

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

        if( NT_SUCCESS( ntStatus))
        {

            ntStatus = pWorkItem->Status;
        }

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueFlushExtents Request for %wZ complete Status %08lX FID %08lX-%08lX-%08lX-%08lX\n",
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                      ntStatus);        

        if( !NT_SUCCESS( ntStatus))
        {

            if( pWorkItem != NULL)
            {

                ExFreePool( pWorkItem);
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueFlushExtents Failed to queue request Status %08lX\n", ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSQueueFlushExtents\n");
    }

    return ntStatus;
}

NTSTATUS
AFSQueueExtentRelease( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem *pWorkItem = NULL;
    AFSDeviceExt *pRDRDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    
    __try
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueExtentRelease Queuing request for Irp %08lX\n",
                      Irp); 

        InterlockedIncrement( &pRDRDeviceExt->Specific.RDR.QueuedReleaseExtentCount);

        KeClearEvent( &pRDRDeviceExt->Specific.RDR.QueuedReleaseExtentEvent);

        if( BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueFlushExtents Failing request, in shutdown\n");

            try_return( ntStatus = STATUS_TOO_LATE);
        }

        pWorkItem = (AFSWorkItem *) ExAllocatePoolWithTag( NonPagedPool,
                                                           sizeof(AFSWorkItem),
                                                           AFS_WORK_ITEM_TAG);
        if (NULL == pWorkItem) 
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueExtentRelease Failed to allocate work item\n");        

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        RtlZeroMemory( pWorkItem, 
                       sizeof(AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        pWorkItem->RequestType = AFS_WORK_REQUEST_RELEASE;

        pWorkItem->Specific.ReleaseExtents.Irp = Irp;

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueExtentRelease Workitem %08lX for Irp %08lX\n",
                      pWorkItem,
                      Irp);        

        ntStatus = AFSQueueWorkerRequestAtHead( pWorkItem);

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueExtentRelease Request for Irp %08lX complete Status %08lX\n",
                      Irp,
                      ntStatus);        

        if( !NT_SUCCESS( ntStatus))
        {

            if( pWorkItem != NULL)
            {

                ExFreePool( pWorkItem);
            }

            if( InterlockedDecrement( &pRDRDeviceExt->Specific.RDR.QueuedReleaseExtentCount) == 0)
            {

                KeSetEvent( &pRDRDeviceExt->Specific.RDR.QueuedReleaseExtentEvent,
                            0,
                            FALSE);
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueExtentRelease Failed to queue request Status %08lX\n", ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSQueueExtentRelease\n");
    }

    return ntStatus;
}

NTSTATUS
AFSQueueAsyncRead( IN PDEVICE_OBJECT DeviceObject,
                   IN PIRP Irp,
                   IN HANDLE CallerProcess)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem *pWorkItem = NULL;
    
    __try
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueAsyncRead Queuing request for Irp %08lX\n",
                      Irp);        

        pWorkItem = (AFSWorkItem *) ExAllocatePoolWithTag( NonPagedPool,
                                                           sizeof(AFSWorkItem),
                                                           AFS_WORK_ITEM_TAG);
        if (NULL == pWorkItem) 
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueAsyncRead Failed to allocate work item\n");        

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        RtlZeroMemory( pWorkItem, 
                       sizeof(AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        pWorkItem->RequestType = AFS_WORK_ASYNCH_READ;
            
        pWorkItem->Specific.AsynchIo.Device = DeviceObject;

        pWorkItem->Specific.AsynchIo.Irp = Irp;

        pWorkItem->Specific.AsynchIo.CallingProcess = CallerProcess;

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueAsyncRead Workitem %08lX for Irp %08lX\n",
                      pWorkItem,
                      Irp);        

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueAsyncRead Request for Irp %08lX complete Status %08lX\n",
                      Irp,
                      ntStatus);        

        if( !NT_SUCCESS( ntStatus))
        {

            if( pWorkItem != NULL)
            {

                ExFreePool( pWorkItem);
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueAsyncRead Failed to queue request Status %08lX\n", ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSQueueAsyncRead\n");
    }

    return ntStatus;
}

NTSTATUS
AFSQueueAsyncWrite( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp,
                    IN HANDLE CallerProcess)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem *pWorkItem = NULL;
    
    __try
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueAsyncWrite Queuing request for Irp %08lX\n",
                      Irp);        

        pWorkItem = (AFSWorkItem *) ExAllocatePoolWithTag( NonPagedPool,
                                                           sizeof(AFSWorkItem),
                                                           AFS_WORK_ITEM_TAG);
        if (NULL == pWorkItem) 
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueAsyncWrite Failed to allocate work item\n");        

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        RtlZeroMemory( pWorkItem, 
                       sizeof(AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        pWorkItem->RequestType = AFS_WORK_ASYNCH_WRITE;
            
        pWorkItem->Specific.AsynchIo.Device = DeviceObject;

        pWorkItem->Specific.AsynchIo.Irp = Irp;

        pWorkItem->Specific.AsynchIo.CallingProcess = CallerProcess;

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueAsyncWrite Workitem %08lX for Irp %08lX\n",
                      pWorkItem,
                      Irp);        

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueAsyncWrite Request for Irp %08lX complete Status %08lX\n",
                      Irp,
                      ntStatus);        

        if( !NT_SUCCESS( ntStatus))
        {

            if( pWorkItem != NULL)
            {

                ExFreePool( pWorkItem);
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueAsyncWrite Failed to queue request Status %08lX\n", ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSQueueAsyncWrite\n");
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

        pWorkItem = (AFSWorkItem *) ExAllocatePoolWithTag( NonPagedPool,
                                                           sizeof(AFSWorkItem),
                                                           AFS_WORK_ITEM_TAG);
        if (NULL == pWorkItem) 
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueGlobalRootEnumeration Failed to allocate work item\n");        

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        RtlZeroMemory( pWorkItem, 
                       sizeof(AFSWorkItem));

        pWorkItem->Size = sizeof( AFSWorkItem);

        pWorkItem->RequestType = AFS_WORK_ENUMERATE_GLOBAL_ROOT;
            
        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueGlobalRootEnumeration Workitem %08lX\n",
                      pWorkItem);        

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_WORKER_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueueGlobalRootEnumeration Request complete Status %08lX\n",
                      ntStatus);        

        if( !NT_SUCCESS( ntStatus))
        {

            if( pWorkItem != NULL)
            {

                ExFreePool( pWorkItem);
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueueGlobalRootEnumeration Failed to queue request Status %08lX\n", 
                          ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSQueueGlobalRootEnumeration\n");
    }

    return ntStatus;
}