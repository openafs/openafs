/*
 * Copyright (c) 2008 Kernel Drivers, LLC.
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

            AFSPrint("AFSInitializeWorkerPool Failed to allocate worker context\n");

            ntStatus = STATUS_INSUFFICIENT_RESOURCES;

            break;
        }

        RtlZeroMemory( pCurrentWorker,
                       sizeof( AFSWorkQueueContext));

        ntStatus = AFSInitWorkerThread( pCurrentWorker);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSInitializeWorkerPool Failed to initialize worker thread Status %08lX\n", ntStatus);

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
    AFSDeviceExt *pDevExt = NULL;

    pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

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

    waitObjects[ 0] = &pDevExt->Specific.Control.WorkerQueueHasItems;

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
            
            AFSPrint("AFSWorkerThread Wait for queue items failed Status %08lX\n", ntStatus);
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

                        (VOID) AFSFlushExtents( pWorkItem->Specific.Fcb.Fcb);
                        //
                        // Give back the ref we took when we posted
                        //
                        InterlockedDecrement( &pWorkItem->Specific.Fcb.Fcb->OpenReferenceCount);
                        break;
                    }

                    case AFS_WORK_ASYNCH_READ:
                    {

                        (VOID) AFSCommonRead( pWorkItem->Specific.AsynchIo.Device, pWorkItem->Specific.AsynchIo.Irp, TRUE );
                        break;
                    }

                    case AFS_WORK_ASYNCH_WRITE:
                    {

                        (VOID) AFSCommonWrite( pWorkItem->Specific.AsynchIo.Device, 
                                               pWorkItem->Specific.AsynchIo.Irp,
                                               pWorkItem->Specific.AsynchIo.CallingProcess);
                        break;
                    }

                    case AFS_WORK_REQUEST_BUILD_TARGET_FCB:
                    {

                        pWorkItem->Status = AFSBuildTargetDirectory( pWorkItem->ProcessID,
                                                                     pWorkItem->Specific.Fcb.Fcb);

                        freeWorkItem = FALSE;

                        KeSetEvent( &pWorkItem->Event,
                                    0,
                                    FALSE);

                        break;
                    }

                    default:

                        AFSPrint("AFSWorkerThread Unknown request type %d\n", pWorkItem->RequestType);

                        break;
                }

                if( freeWorkItem)
                {

                    ExFreePoolWithTag( pWorkItem, AFS_WORK_ITEM_TAG);
                }
            }
        }
    } // worker thread loop

    AFSPrint("AFSWorkerThread Thread %08lX Ending\n", pPoolContext);

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
            
            AFSPrint("AFSVolumeWorkerThread Wait for queue items failed Status %08lX\n", ntStatus);
        }
        else
        {

            AFSFcb *pFcb = NULL, *pNextFcb = NULL;
            LARGE_INTEGER liTime;

            //
            // First step is to look for potential Fcbs to be removed
            //

            AFSAcquireShared( pVolumeVcb->Specific.VolumeRoot.FcbListLock,
                              TRUE);

            pFcb = pVolumeVcb->Specific.VolumeRoot.FcbListHead;

            while( pFcb != NULL)
            {

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
                    //
                    // Yup (the last flush was long enough ago)
                    //
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
                // If the extents haven't been used recently *and*
                // are not being used
                //
                if (pFcb->Header.NodeTypeCode == AFS_FILE_FCB &&
                    0 != pFcb->Specific.File.LastServerFlush.QuadPart &&
                    (liTime.QuadPart - pFcb->Specific.File.LastExtentAccess.QuadPart) 
                    >= 
                    (AFS_SERVER_PURGE_SLEEP * pControlDeviceExt->Specific.Control.FcbPurgeTimeCount.QuadPart))
                {
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
                    !BooleanFlagOn( pFcb->Flags, AFS_FCB_INVALID))
                    //&&
                    //AFSAllocationMemoryLevel <= 1024000 * 5) // We keep it to 5 MB
                {

                    pFcb = (AFSFcb *)pFcb->ListEntry.fLink;

                    continue;
                }

                if( AFSAcquireShared( &pFcb->NPFcb->Resource,
                                      BooleanFlagOn( pVolumeVcb->Flags, AFS_FCB_VOLUME_OFFLINE)))
                {

                    KeQueryTickCount( &liTime);

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

                        AFSAcquireExcl( pVolumeVcb->Specific.VolumeRoot.FcbListLock,
                                        TRUE);

                        //
                        // Need to acquire the parent shared so we do not collide
                        // while updating the DirEntry
                        //

                        if( AFSAcquireShared( &pFcb->ParentFcb->NPFcb->Resource,
                                              BooleanFlagOn( pVolumeVcb->Flags, AFS_FCB_VOLUME_OFFLINE)))
                        {

                            if( AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                                BooleanFlagOn( pVolumeVcb->Flags, AFS_FCB_VOLUME_OFFLINE)))
                            {

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

                                    if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB &&
                                        pFcb->DirEntry != NULL &&
                                        !BooleanFlagOn( pFcb->Flags, AFS_FCB_DELETED))
                                    {

                                        (VOID)AFSTearDownFcbExtents( pFcb);
                                    }

                                    //
                                    // Make sure the dire entry reference is removed
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

                                                InterlockedDecrement( &pCurrentDirEntry->Fcb->OpenReferenceCount);
                                            }

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
        }
    } // worker thread loop

    AFSRemoveRootFcb( pVolumeVcb,
                      FALSE);

    KeCancelTimer( &Timer);

    PsTerminateSystemThread( 0);

    return;
}

NTSTATUS
AFSInsertWorkitem( IN AFSWorkItem *WorkItem)
{
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = NULL;

    pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    AFSAcquireExcl( &pDevExt->Specific.Control.QueueLock,
                    TRUE);

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

AFSWorkItem *
AFSRemoveWorkItem()
{
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem        *pWorkItem = NULL;
    AFSDeviceExt *pDevExt = NULL;

    pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    AFSAcquireExcl( &pDevExt->Specific.Control.QueueLock,
                    TRUE);

    if( pDevExt->Specific.Control.QueueHead != NULL) // queue has nodes
    {
            
        pWorkItem = pDevExt->Specific.Control.QueueHead;
            
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

/*
NTSTATUS
AFSQueueCopyData( IN PFILE_OBJECT SourceFileObject,
                    IN PFILE_OBJECT TargetFileObject)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem    *pWorkItem = NULL;
    
    __try
    {

        //
        // Allocate our request structure and send it to the worker
        //

        pWorkItem = (AFSWorkItem *)ExAllocatePoolWithTag( NonPagedPool,
                                                          sizeof( AFSWorkItem),
                                                          AFS_WORK_ITEM_TAG);

        if( pWorkItem == NULL)
        {

            AFSPrint("AFSQueueCopyData Failed to allocate request block\n");

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

        pWorkItem->RequestFlags = AFS_SYNCHRONOUS_REQUEST;

        //pWorkItem->RequestType = AFS_WORK_REQUEST_COPY_DATA;

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

        if( NT_SUCCESS( ntStatus))
        {

            ntStatus = pWorkItem->Status;
        }

try_exit:

        if( pWorkItem != NULL)
        {

            ExFreePool( pWorkItem);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSQueueCopyData Failed to queue request Status %08lX\n", ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSQueueCopyData\n");
    }

    return ntStatus;
}
*/

NTSTATUS
AFSQueueBuildTargetDirectory( IN AFSFcb *Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSWorkItem *pWorkItem = NULL;
    
    __try
    {

        //
        // Allocate our request structure and send it to the worker
        //

        pWorkItem = (AFSWorkItem *)ExAllocatePoolWithTag( NonPagedPool,
                                                          sizeof( AFSWorkItem),
                                                          AFS_WORK_ITEM_TAG);

        if( pWorkItem == NULL)
        {

            AFSPrint("AFSQueueCopyData Failed to allocate request block\n");

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

        pWorkItem->RequestType = AFS_WORK_REQUEST_BUILD_TARGET_FCB;

        pWorkItem->Specific.Fcb.Fcb = Fcb;

        ntStatus = AFSQueueWorkerRequest( pWorkItem);

        if( NT_SUCCESS( ntStatus))
        {

            ntStatus = pWorkItem->Status;
        }

try_exit:

        if( pWorkItem != NULL)
        {

            ExFreePool( pWorkItem);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSQueueFcbInitialization Failed to queue request Status %08lX\n", ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSQueueFcbInitialization\n");
    }

    return ntStatus;
}