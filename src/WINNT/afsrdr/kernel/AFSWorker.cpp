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
    // If we have any worker threads then don;t fail if something has happened
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
AFSInitVolumeWorker()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSWorkQueueContext *pWorker = &pDeviceExt->Specific.RDR.VolumeWorkerContext;
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
                                          (void *)pWorker);

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
AFSShutdownVolumeWorker()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSWorkQueueContext *pWorker = &pDeviceExt->Specific.RDR.VolumeWorkerContext;

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
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

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

                    case 0:
                    default:

                        AFSPrint("AFSWorkerThread Unknown request type %d\n", pWorkItem->RequestType);

                        break;
                }

                if( freeWorkItem)
                {

                    ExFreePool( pWorkItem);
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
    AFSWorkQueueContext *pPoolContext = (AFSWorkQueueContext *)Context;
    AFSDeviceExt *pControlDeviceExt = NULL;
    AFSDeviceExt *pRDRDeviceExt = NULL;
    BOOLEAN exitThread = FALSE;
    LARGE_INTEGER DueTime;
    LONG TimeOut;
    KTIMER Timer;
    void *waitObjects[ 2];

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

    waitObjects[ 1] = &Timer;

    waitObjects[ 0] = &pControlDeviceExt->Specific.Control.WorkerQueueHasItems;

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
            
            AFSPrint("AFSVolumeWorkerThread Wait for queue items failed Status %08lX\n", ntStatus);
        }
        else
        {

            AFSFcb *pFcb = NULL, *pNextFcb = NULL;
            LARGE_INTEGER liTime;

            //
            // First step is to look for potential Fcbs to be removed
            //

            AFSAcquireShared( &pRDRDeviceExt->Specific.RDR.FcbListLock,
                              TRUE);

            pFcb = pRDRDeviceExt->Specific.RDR.FcbListHead;

            while( pFcb != NULL)
            {

                if( AFSAcquireShared( &pFcb->NPFcb->Resource,
                                      FALSE))
                {

                    KeQueryTickCount( &liTime);

                    //
                    // If this is a directory ensure the cild reference count is zero
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
                        (liTime.QuadPart - pFcb->LastAccessCount.QuadPart) >= pControlDeviceExt->FcbLifeTimeCount.QuadPart)
                    {

                        //
                        // Possible candidate so drop the locks and re-acquire them excl
                        //

                        AFSReleaseResource( &pRDRDeviceExt->Specific.RDR.FcbListLock);

                        AFSReleaseResource( &pFcb->NPFcb->Resource);

                        AFSAcquireExcl( &pRDRDeviceExt->Specific.RDR.FcbListLock,
                                        TRUE);

                        //
                        // Need to acquire the parent shared so we do not collide
                        // while updating the DirEntry
                        //

                        if( AFSAcquireShared( &pFcb->ParentFcb->NPFcb->Resource,
                                              FALSE))
                        {

                            if( AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                                FALSE))
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
                                    (liTime.QuadPart - pFcb->LastAccessCount.QuadPart) >= pControlDeviceExt->FcbLifeTimeCount.QuadPart)
                                {

                                    //
                                    // Need to deallocate the Fcb and remove it from the directory entry
                                    //

                                    pFcb->DirEntry->Type.Data.Fcb = NULL;

                                    AFSReleaseResource( &pFcb->ParentFcb->NPFcb->Resource);

                                    if( pFcb->ListEntry.fLink == NULL)
                                    {

                                        pRDRDeviceExt->Specific.RDR.FcbListTail = (AFSFcb *)pFcb->ListEntry.bLink;

                                        if( pRDRDeviceExt->Specific.RDR.FcbListTail != NULL)
                                        {

                                            pRDRDeviceExt->Specific.RDR.FcbListTail->ListEntry.fLink = NULL;
                                        }
                                    }
                                    else
                                    {

                                        ((AFSFcb *)(pFcb->ListEntry.fLink))->ListEntry.bLink = pFcb->ListEntry.bLink;
                                    }

                                    if( pFcb->ListEntry.bLink == NULL)
                                    {

                                        pRDRDeviceExt->Specific.RDR.FcbListHead = (AFSFcb *)pFcb->ListEntry.fLink;

                                        if( pRDRDeviceExt->Specific.RDR.FcbListHead != NULL)
                                        {

                                            pRDRDeviceExt->Specific.RDR.FcbListHead = NULL;
                                        }
                                    }
                                    else
                                    {

                                        ((AFSFcb *)(pFcb->ListEntry.bLink))->ListEntry.fLink = pFcb->ListEntry.fLink;
                                    }

                                    InterlockedDecrement( &pFcb->ParentFcb->Specific.Directory.ChildReferenceCount);

                                    pNextFcb = (AFSFcb *)pFcb->ListEntry.fLink;                                    

                                    //
                                    // Grab the FileID Tree lock
                                    //

                                    AFSAcquireExcl( pRDRDeviceExt->Specific.RDR.FileIDTree.TreeLock,
                                                    TRUE);

                                    AFSRemoveFileIDEntry( &pRDRDeviceExt->Specific.RDR.FileIDTree.TreeHead,
                                                          &pFcb->FileIDTreeEntry);

                                    AFSReleaseResource( pRDRDeviceExt->Specific.RDR.FileIDTree.TreeLock);

                                    //
                                    // We won't tear down the directory nodes at this point
                                    //

                                    /*
                                    //
                                    // If this is a directory then we will need to purge the directory entry list
                                    //

                                    if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
                                    {

                                        AFSDirEntryCB *pCurrentDirEntry = NULL;

                                        AFSPrint("AFSVolumeWorker Removing Directory Fcb %08lX\n", pFcb);

                                        pCurrentDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;

                                        while( pCurrentDirEntry != NULL)
                                        {

                                            AFSDeleteDirEntry( pFcb,
                                                                 pCurrentDirEntry);

                                            pCurrentDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;
                                        }
                                    }
                                    else
                                    {

                                        AFSPrint("AFSVolumeWorker Removing File Fcb %08lX\n", pFcb);
                                    }
                                    */

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

                        AFSConvertToShared( &pRDRDeviceExt->Specific.RDR.FcbListLock);
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

            AFSReleaseResource( &pRDRDeviceExt->Specific.RDR.FcbListLock);
        }
    } // worker thread loop

    AFSPrint("AFSVolumeWorkerThread Thread %08lX Ending\n", pPoolContext);

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
