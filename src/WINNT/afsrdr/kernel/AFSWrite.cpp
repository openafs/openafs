//
// File: AFSWrite.cpp
//

#include "AFSCommon.h"

static
NTSTATUS
CachedWrite( IN PDEVICE_OBJECT DeviceObject,
             IN PIRP Irp,
             IN LARGE_INTEGER StartingByte,
             IN ULONG ByteCount,
             IN BOOLEAN ForceFlush);
static
NTSTATUS
NonCachedWrite( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp,
                IN LARGE_INTEGER StartingByte,
                IN ULONG ByteCount);

static
NTSTATUS 
ExtendingWrite( IN AFSFcb *Fcb,
                IN PFILE_OBJECT FileObject,
                IN LONGLONG NewLength);

//
// Function: AFSWrite
//
// Description:
//
//      This is the dispatch handler for the IRP_MJ_WRITE request
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSWrite( IN PDEVICE_OBJECT DeviceObject,
          IN PIRP Irp)
{

    NTSTATUS           ntStatus = STATUS_SUCCESS;
    AFSDeviceExt      *pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    IO_STACK_LOCATION *pIrpSp;
    AFSFcb            *pFcb = NULL;
    AFSNonPagedFcb    *pNPFcb = NULL;
    ULONG              ulByteCount = 0;
    LARGE_INTEGER      liStartingByte;
    PFILE_OBJECT       pFileObject;
    BOOLEAN            bPagingIo = FALSE;
    BOOLEAN            bNonCachedIo = FALSE;
    BOOLEAN            bReleaseMain = FALSE;    
    BOOLEAN            bReleasePaging = FALSE;    
    BOOLEAN            bExtendingWrite = FALSE;
    BOOLEAN            bSynchronousIo = FALSE;
    BOOLEAN            bCanQueueRequest = FALSE;
    BOOLEAN            bCompleteIrp = TRUE;
    BOOLEAN            bForceFlush = FALSE;
    ULONG              ulExtensionLength = 0;
    BOOLEAN            bRetry = FALSE;


    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __Enter
    {

        pFileObject = pIrpSp->FileObject;

        //
        // Extract the fileobject references
        //

        pFcb = (AFSFcb *)pFileObject->FsContext;
        pNPFcb = pFcb->NPFcb;

        ObReferenceObject( pFileObject);

        if( pFcb->Header.NodeTypeCode != AFS_IOCTL_FCB &&
            pFcb->Header.NodeTypeCode != AFS_FILE_FCB)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // If this is a write against an IOCtl node then handle it 
        // in a different pathway
        //

        if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {

            ntStatus = AFSIOCtlWrite( DeviceObject,
                                      Irp);

            try_return( ntStatus);
        }

        //
        // TODO we need some interlock to stop the cache being town
        // down after this check 
        //
        if( NULL == pDeviceExt->Specific.RDR.CacheFileObject) 
        {
            try_return( ntStatus = STATUS_TOO_LATE );
        }

        liStartingByte = pIrpSp->Parameters.Write.ByteOffset;

        bSynchronousIo = BooleanFlagOn( pFileObject->Flags, FO_SYNCHRONOUS_IO);
        bPagingIo      = BooleanFlagOn( Irp->Flags, IRP_PAGING_IO);
        bNonCachedIo   = BooleanFlagOn( Irp->Flags, IRP_NOCACHE);
        ulByteCount    = pIrpSp->Parameters.Write.Length;

        if( !bPagingIo)
        {
            bExtendingWrite = (((liStartingByte.QuadPart + ulByteCount) >= 
                                pFcb->Header.FileSize.QuadPart) ||
                               (liStartingByte.LowPart == FILE_WRITE_TO_END_OF_FILE &&
                                liStartingByte.HighPart == -1)) ;
        }

        bCanQueueRequest = !(IoIsOperationSynchronous( Irp));

        //
        // Check for zero length write
        //

        if( ulByteCount == 0)
        {

            AFSPrint("AFSCommonWrite Processed zero length write\n");

            try_return( ntStatus);
        }

        //
        // Is this Fcb valid???
        //

        if( BooleanFlagOn( pFcb->Flags, AFS_FCB_INVALID))
        {

            AFSPrint("AFSCommonWrite Dropping request for invalid Fcb\n");

            //
            // OK, this Fcb was probably deleted then renamed into but we re-used the source fcb
            // hence this is bogus. Drop it?
            //

            Irp->IoStatus.Information = ulByteCount;

            try_return( ntStatus = STATUS_SUCCESS);
        }

        if( FlagOn(pIrpSp->MinorFunction, IRP_MN_COMPLETE) ) {
        
            AFSPrint("MN_COMPLETE \n");

            CcMdlWriteComplete(pFileObject, &pIrpSp->Parameters.Write.ByteOffset, Irp->MdlAddress);

            //
            // Save off the last writer
            //

            pFcb->Specific.File.ModifyProcessId = PsGetCurrentProcessId();

            //
            // Mdl is now Deallocated
            //

            Irp->MdlAddress = NULL;
        
            try_return( ntStatus = STATUS_SUCCESS );
        }


        //
        // If we get a non cached IO for a cached file we should do a purge.  
        // For now we will just promote to cached
        //
        if( NULL != pFileObject->SectionObjectPointer->DataSectionObject && !bPagingIo && bNonCachedIo) {
            bNonCachedIo = FALSE;
            bForceFlush = TRUE;
        }

        if( !bNonCachedIo && pFileObject->PrivateCacheMap == NULL)
        {

            CcInitializeCacheMap( pFileObject,
                                  (PCC_FILE_SIZES)&pFcb->Header.AllocationSize,
                                  FALSE,
                                  &AFSCacheManagerCallbacks,
                                  pFcb);

            CcSetReadAheadGranularity( pFileObject, 
                                       READ_AHEAD_GRANULARITY);
        }

        while (!bNonCachedIo && !CcCanIWrite( pFileObject,
                                              ulByteCount,
                                              FALSE,
                                              bRetry))
        {
            static const LONGLONG llWriteDelay = (LONGLONG)-100000;
            bRetry = TRUE;
            KeDelayExecutionThread(KernelMode, FALSE, (PLARGE_INTEGER)&llWriteDelay);
        }

        //
        // Take locks 
        //
        //   - if Paging then we need to nothing (the precalls will
        //     have acquired the paging resource), for clarity we will collect 
        //     the paging resource 
        //   - If extending Write then take the fileresource EX (EOF will change, Allocation will only move out)
        //   - Otherwise we collect the file shared
        //
        if( bPagingIo) 
        {
            ASSERT( ExIsResourceAcquiredLite( &pNPFcb->PagingResource ));
    
            AFSAcquireShared( &pNPFcb->PagingResource,
                              TRUE);

            bReleasePaging = TRUE;
        } 
        else if( bExtendingWrite) 
        {
            //
            // Check for lock inversion
            //
            ASSERT( !ExIsResourceAcquiredLite( &pNPFcb->PagingResource ));
    
            AFSAcquireExcl( &pNPFcb->Resource,
                              TRUE);

            if (liStartingByte.LowPart == FILE_WRITE_TO_END_OF_FILE &&
                liStartingByte.HighPart == -1)
            {
                if (pFcb->Header.ValidDataLength.QuadPart > pFcb->Header.FileSize.QuadPart)
                {
                    liStartingByte = pFcb->Header.ValidDataLength;
                } 
                else
                {
                    liStartingByte = pFcb->Header.FileSize;
                }
            }
            bReleaseMain = TRUE;

        }
        else
        {
            ASSERT( !ExIsResourceAcquiredLite( &pNPFcb->PagingResource ));
    
            AFSAcquireShared( &pNPFcb->Resource,
                              TRUE);

            bReleaseMain = TRUE;
        }

        if( BooleanFlagOn( pFcb->Flags, AFS_FCB_DELETED))
        {

            try_return( ntStatus = STATUS_FILE_DELETED);
        }

        //
        // Check the BR locks on the file. TODO: Add OPLock checks and
        // queuing mechanism
        //

        if( !bPagingIo && 
            !FsRtlCheckLockForWriteAccess( &pFcb->Specific.File.FileLock,
                                           Irp)) 
        {
            
            AFSPrint("AFSCommonWrite Failed BR lock check for cached I/O\n");

            try_return( ntStatus = STATUS_FILE_LOCK_CONFLICT);
        }

        if ( bExtendingWrite)
        {

            ntStatus = ExtendingWrite( pFcb, pFileObject, (liStartingByte.QuadPart + ulByteCount));

            if ( !NT_SUCCESS(ntStatus)) 
            {
                
                try_return( ntStatus );
            }
        }

        //
        // Fire off the request as appropriate
        //
        bCompleteIrp = FALSE;

        if( !bPagingIo &&
            !bNonCachedIo)
        {
            
            ntStatus = CachedWrite( DeviceObject, Irp, liStartingByte, ulByteCount, bForceFlush);

            if( NT_SUCCESS( ntStatus))
            {
                //
                // Save off the last writer
                //

                pFcb->Specific.File.ModifyProcessId = PsGetCurrentProcessId();
            }
        }
        else
        {

            ntStatus = NonCachedWrite( DeviceObject, Irp,  liStartingByte, ulByteCount);
        }

try_exit:

        ObDereferenceObject(pFileObject);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSCommonWrite Failed to process write request Status %08lX\n", ntStatus);
        }

        if( bReleaseMain)
        {

            AFSReleaseResource( &pNPFcb->Resource);
        }
        if( bReleasePaging)
        {

            AFSReleaseResource( &pNPFcb->PagingResource);
        }

        if (bCompleteIrp) {
            Irp->IoStatus.Information = 0;

            AFSCompleteRequest( Irp, ntStatus );
        }
    }

    return ntStatus;
}

NTSTATUS
AFSIOCtlWrite( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSPIOCtlIORequestCB stIORequestCB;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    AFSPIOCtlIOResultCB stIOResultCB;
    ULONG ulBytesReturned = 0;
    AFSFileID stParentFID;

    __Enter
    {

        RtlZeroMemory( &stIORequestCB,
                       sizeof( AFSPIOCtlIORequestCB));

        if( pIrpSp->Parameters.Write.Length == 0)
        {

            //
            // Nothing to do in this case
            //

            try_return( ntStatus);
        }

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        AFSAcquireShared( &pFcb->NPFcb->Resource,
                          TRUE);

        //
        // Get the parent fid to pass to the cm
        //

        RtlZeroMemory( &stParentFID,
                       sizeof( AFSFileID));

        if( pFcb->ParentFcb != NULL)
        {

            AFSFcb *pParentDcb = pFcb->ParentFcb;

            //
            // Be sure to get the correct fid for the parent
            //

            if( pParentDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY)
            {

                //
                // Just the FID of the node
                //

                stParentFID = pParentDcb->DirEntry->DirectoryEntry.FileId;
            }
            else
            {

                //
                // MP or SL
                //

                stParentFID = pParentDcb->DirEntry->DirectoryEntry.TargetFileId;

                //
                // If this is zero then we need to evaluate it
                //

                if( stParentFID.Hash == 0)
                {

                    AFSDirEnumEntry *pDirEntry = NULL;
                    AFSFcb *pGrandParentDcb = NULL;

                    if( pParentDcb->ParentFcb == NULL ||
                        pParentDcb->ParentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY)
                    {

                        stParentFID = pParentDcb->ParentFcb->DirEntry->DirectoryEntry.FileId;
                    }
                    else
                    {

                        stParentFID = pParentDcb->ParentFcb->DirEntry->DirectoryEntry.TargetFileId;
                    }

                    ntStatus = AFSEvaluateTargetByID( &stParentFID,
                                                      &pParentDcb->DirEntry->DirectoryEntry.FileId,
                                                      &pDirEntry);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        try_return( ntStatus);
                    }

                    pParentDcb->DirEntry->DirectoryEntry.TargetFileId = pDirEntry->TargetFileId;

                    stParentFID = pDirEntry->TargetFileId;

                    ExFreePool( pDirEntry);
                }
            }
        }

        //
        // Set the control block up
        //

        stIORequestCB.RequestId = pCcb->PIOCtlRequestID;

        if( pFcb->RootFcb != NULL)
        {
            stIORequestCB.RootId = pFcb->RootFcb->DirEntry->DirectoryEntry.FileId;
        }

        //
        // Lock down the buffer
        //

        stIORequestCB.MappedBuffer = AFSMapToService( Irp,
                                                      pIrpSp->Parameters.Write.Length);

        if( stIORequestCB.MappedBuffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        stIORequestCB.BufferLength = pIrpSp->Parameters.Write.Length;

        stIOResultCB.BytesProcessed = 0;

        ulBytesReturned = sizeof( AFSPIOCtlIOResultCB);

        //
        // Issue the request to the service
        //

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_PIOCTL_WRITE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      NULL,
                                      &stParentFID,
                                      (void *)&stIORequestCB,
                                      sizeof( AFSPIOCtlIORequestCB),
                                      &stIOResultCB,
                                      &ulBytesReturned);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Update the length written
        //

        Irp->IoStatus.Information = stIOResultCB.BytesProcessed;

try_exit:

        if( stIORequestCB.MappedBuffer != NULL)
        {

            AFSUnmapServiceMappedBuffer( stIORequestCB.MappedBuffer,
                                         Irp->MdlAddress);
        }

        if( pFcb != NULL)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }
    }

    return ntStatus;
}

//
// This function is called when we know we have to read from the AFS Cache.
//
// It ensures that we have exents for the entirety of the write and
// then pinns the extents into memory (meaning that although we may
// add we will not remopve).  Then it creates a scatter gather write
// and filres off the irps
//
static
NTSTATUS
NonCachedWrite( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp,
                IN LARGE_INTEGER StartingByte,
                IN ULONG ByteCount)
{
    NTSTATUS           ntStatus = STATUS_UNSUCCESSFUL;
    VOID              *pSystemBuffer = NULL;
    BOOLEAN            bPagingIo = BooleanFlagOn( Irp->Flags, IRP_PAGING_IO);
    BOOLEAN            bLocked = FALSE; 
    BOOLEAN            bCompleteIrp = TRUE;
    BOOLEAN            bExtentsMapped = FALSE;
    BOOLEAN            bDerefExtents = FALSE;
    AFSGatherIo       *pGatherIo = NULL;
    AFSIoRun          *pIoRuns = NULL;
    AFSIoRun           stIoRuns[AFS_MAX_STACK_IO_RUNS];
    ULONG              extentsCount = 0;
    AFSExtent         *pStartExtent = NULL;
    AFSExtent         *pIgnoreExtent = NULL;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT       pFileObject = pIrpSp->FileObject;
    AFSFcb            *pFcb = (AFSFcb *)pFileObject->FsContext;
    BOOLEAN            bSynchronousIo = BooleanFlagOn( pFileObject->Flags, FO_SYNCHRONOUS_IO);
    AFSDeviceExt      *pDevExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;

    __Enter
    {
        Irp->IoStatus.Information = 0;

        //
        // Get the mapping for the buffer
        //
        pSystemBuffer = AFSLockSystemBuffer( Irp,
                                             ByteCount);

        if( pSystemBuffer == NULL)
        {
                
            AFSPrint("AFSCommonWrite Failed to retrieve system buffer\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }


        //
        // Provoke a get of the extents - if we need to.
        // TODO - right now we wait for the extents to be there.  We should 
        // Post if we can or return STATUS_FILE_LOCKED otherwise
        //
        // Just as in the read case we could avoid a traverse by getting the first extent from
        // AFSRequestExtents
        //
        while (TRUE) 
        {

            ntStatus = AFSRequestExtents( pFcb, &StartingByte, ByteCount, &bExtentsMapped );

            if (!NT_SUCCESS(ntStatus)) 
            {

                try_return( ntStatus );
            }

            if (bExtentsMapped)
            {
                //
                // We know that they *did* map.  Now lock up and then
                // if we are still mapped pin the extents.
                //
                AFSAcquireShared( &pFcb->NPFcb->Specific.File.ExtentsResource, TRUE );
                bLocked = TRUE;
                if ( AFSDoExtentsMapRegion( pFcb, &StartingByte, ByteCount, &pStartExtent, &pIgnoreExtent )) 
                {
                    AFSReferenceExtents( pFcb ); 
                    bDerefExtents = TRUE;
                    break;
                }
                AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource );
                bLocked= FALSE;
                //
                // Bad things happened in the interim.  Start again from the top
                //
                continue;
            }    

            ntStatus =  AFSWaitForExtentMapping ( pFcb );

            if (!NT_SUCCESS(ntStatus)) 
            {

                try_return( ntStatus );
            }
        }
        
        //
        // As per the read path - 
        //

        ntStatus = AFSGetExtents( pFcb, 
                                  &StartingByte, 
                                  ByteCount, 
                                  pStartExtent, 
                                  &extentsCount);
        
        if (!NT_SUCCESS(ntStatus)) 
        {
            try_return( ntStatus );
        }
        
        if (extentsCount > AFS_MAX_STACK_IO_RUNS) {

            pIoRuns = (AFSIoRun*) ExAllocatePoolWithTag( PagedPool,
                                                         extentsCount * sizeof( AFSIoRun ),
                                                         AFS_IO_RUN_TAG );
            if (NULL == pIoRuns) 
            {
                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
            }
        } else {
            
            pIoRuns = stIoRuns;

        }

        RtlZeroMemory( pIoRuns, extentsCount * sizeof( AFSIoRun ));

        ntStatus = AFSSetupIoRun( pDevExt->Specific.RDR.CacheFileObject->Vpb->DeviceObject, 
                                  Irp, 
                                  pSystemBuffer, 
                                  pIoRuns, 
                                  &StartingByte, 
                                  ByteCount, 
                                  pStartExtent, 
                                  &extentsCount );

        if (!NT_SUCCESS(ntStatus)) 
        {
            try_return( ntStatus );
        }

        ASSERT(bDerefExtents);
        AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource );
        bLocked = FALSE;

        pGatherIo = (AFSGatherIo*) ExAllocatePoolWithTag( NonPagedPool,
                                                      sizeof( AFSGatherIo ),
                                                      AFS_GATHER_TAG );

        if (NULL == pGatherIo) 
        {
            try_return (ntStatus = STATUS_INSUFFICIENT_RESOURCES );
        }

        RtlZeroMemory( pGatherIo, sizeof( AFSGatherIo ));

        //
        // Initialize count to 1, that was we won't get an early
        // completion if the first irp completes before the second is
        // queued.
        //
        pGatherIo->Count = 1;
        pGatherIo->Status = STATUS_SUCCESS;
        pGatherIo->MasterIrp = Irp;
        pGatherIo->Synchronous = TRUE;
        bCompleteIrp = FALSE;

        if (pGatherIo->Synchronous) {
            KeInitializeEvent( &pGatherIo->Event, NotificationEvent, FALSE );
        }

        //
        // Pre-emptively set up the count
        //
        Irp->IoStatus.Information = ByteCount;

        ntStatus = AFSStartIos( pDevExt->Specific.RDR.CacheFileObject,
                                IRP_MJ_WRITE,
                                IRP_WRITE_OPERATION | IRP_SYNCHRONOUS_API,
                                pIoRuns, 
                                extentsCount, 
                                pGatherIo );

        //
        // Regardless of the status we we do the complete - there may
        // be IOs in flight
        //
        // Decrement the count - setting the event if we are done
        //
        AFSCompleteIo( pGatherIo, STATUS_SUCCESS );

        //
        // Wait for completion of All IOs we started.
        //
        (VOID) KeWaitForSingleObject( &pGatherIo->Event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      NULL);
        
        if (NT_SUCCESS(ntStatus)) 
        {
            ntStatus = pGatherIo->Status;
        } 
        else 
        {
            try_return( ntStatus);
        }

        if( !bPagingIo)
        {

            //
            // Save off the last writer
            //

            pFcb->Specific.File.ModifyProcessId = PsGetCurrentProcessId();
        }

        //
        // Since this is dirty we can mark the extents dirty now
        // (asynch we would fire off a thread to wait for the event and the mark it dirty)
        //

        AFSMarkDirty( pFcb, &StartingByte, ByteCount);

        if (!bPagingIo) 
        {
            //
            // This was an uncached user write - tell the server to do
            // the flush when the worker thread next wakes up
            //
            pFcb->Specific.File.LastServerFlush.QuadPart = 0;
        }

        //
        // All done
        //
try_exit:
        if (NT_SUCCESS(ntStatus) &&
            !bPagingIo &&
            bSynchronousIo) 
        {
            //
            // Update the CBO if this is a sync, nopaging read
            //
            pFileObject->CurrentByteOffset.QuadPart = StartingByte.QuadPart + ByteCount;
        }

        SetFlag( pFcb->Flags, AFS_UPDATE_WRITE_TIME);

        if (bLocked) 
        {
            AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource );
        }

        if (pGatherIo) {
            ExFreePoolWithTag(pGatherIo, AFS_GATHER_TAG);
        }

        if (NULL != pIoRuns && stIoRuns != pIoRuns) {
            ExFreePoolWithTag(pIoRuns, AFS_IO_RUN_TAG);
        }

        if (bCompleteIrp) {
            Irp->IoStatus.Information = 0;

            AFSCompleteRequest( Irp, ntStatus );
        }

        if( bDerefExtents)
        {

            AFSDereferenceExtents( pFcb);
        }
    }

    return ntStatus;
}

static
NTSTATUS
CachedWrite( IN PDEVICE_OBJECT DeviceObject,
             IN PIRP Irp,
             IN LARGE_INTEGER StartingByte,
             IN ULONG ByteCount,
             IN BOOLEAN ForceFlush)
{
    PVOID              pSystemBuffer = NULL;
    NTSTATUS           ntStatus = STATUS_SUCCESS;
    IO_STATUS_BLOCK    iosbFlush;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT       pFileObject = pIrpSp->FileObject;
    AFSFcb            *pFcb = (AFSFcb *)pFileObject->FsContext;
    BOOLEAN            bSynchronousIo = BooleanFlagOn( pFileObject->Flags, FO_SYNCHRONOUS_IO);
    BOOLEAN            bMapped = FALSE; 

    Irp->IoStatus.Information = 0;
    __Enter
    {
        //
        // Provoke a get of the extents - if we need to.
        //
        ntStatus = AFSRequestExtents( pFcb, &StartingByte, ByteCount, &bMapped );

        if (!NT_SUCCESS(ntStatus)) {
            try_return( ntStatus );
        }

        //
        // TODO - CcCanIwrite
        //
        

        //
        // Get the mapping for the buffer
        //

        pSystemBuffer = AFSLockSystemBuffer( Irp,
                                             ByteCount);

        if( pSystemBuffer == NULL)
        {
                
            AFSPrint("AFSCommonWrite Failed to retrieve system buffer\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        __try 
        {
            if( !CcCopyWrite( pFileObject,
                              &StartingByte,
                              ByteCount,
                              TRUE,
                              pSystemBuffer)) 

            {
                //
                // Failed to process request.
                //

                AFSPrint("AFSWrite failed to issue cached read Write %08lX\n", Irp->IoStatus.Status);

                try_return( ntStatus = STATUS_UNSUCCESSFUL);
            }

            if (ForceFlush)
            {
                //
                // We have detected a file we do a write through with.
                //
                CcFlushCache(&pFcb->NPFcb->SectionObjectPointers,
                             &StartingByte,
                             ByteCount,
                             &iosbFlush);

                ntStatus = iosbFlush.Status;
            }
            else 
            {
                ntStatus = STATUS_SUCCESS;
            }

        }
        __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()))
        {
            ntStatus = GetExceptionCode() ;
        }
        
        if (!NT_SUCCESS(ntStatus))
        {
            try_return ( ntStatus );
        }

        Irp->IoStatus.Information = ByteCount;
        
        if( bSynchronousIo)
        {

            pFileObject->CurrentByteOffset.QuadPart = StartingByte.QuadPart + ByteCount;
        }

        //
        // If this extended the Vdl, then update it accordingly
        //

        if( StartingByte.QuadPart + ByteCount > pFcb->Header.ValidDataLength.QuadPart)
        {

            pFcb->Header.ValidDataLength.QuadPart = StartingByte.QuadPart + ByteCount;
        }

        if (BooleanFlagOn(pFileObject->Flags, (FO_NO_INTERMEDIATE_BUFFERING + FO_WRITE_THROUGH)))
        {
            //
            // Write through asked for... Set things so that we get 
            // flush when the worker thread next wakes up
            //
            pFcb->Specific.File.LastServerFlush.QuadPart = 0;
        }


        SetFlag( pFcb->Flags, AFS_UPDATE_WRITE_TIME);

    try_exit:
        ;
    }

    //
    // Complete the request.
    //
    AFSCompleteRequest( Irp,
                        ntStatus);

    return ntStatus;
}

static
NTSTATUS 
ExtendingWrite( IN AFSFcb *Fcb,
                IN PFILE_OBJECT FileObject,
                IN LONGLONG NewLength)
{
    LARGE_INTEGER liSaveFileSize = Fcb->Header.FileSize;
    LARGE_INTEGER liSaveAllocation = Fcb->Header.AllocationSize;
    NTSTATUS      ntStatus = STATUS_SUCCESS;

    if( NewLength > Fcb->Header.AllocationSize.QuadPart)
    {

        Fcb->Header.AllocationSize.QuadPart = NewLength;
        
        Fcb->DirEntry->DirectoryEntry.AllocationSize = Fcb->Header.AllocationSize;
    }

    if( NewLength > Fcb->Header.FileSize.QuadPart)
    {
        
        Fcb->Header.FileSize.QuadPart = NewLength;

        Fcb->DirEntry->DirectoryEntry.EndOfFile = Fcb->Header.FileSize;
    }

    //
    // Tell the server
    //
    ntStatus = AFSUpdateFileInformation( AFSRDRDeviceObject, Fcb);

    if (NT_SUCCESS(ntStatus))
    {
        
        SetFlag( Fcb->Flags, AFS_FILE_MODIFIED);
        //
        // If the file is currently cached, then let the MM know about the extension
        //
    
        if( CcIsFileCached( FileObject)) 
        {
            CcSetFileSizes( FileObject, 
                            (PCC_FILE_SIZES)&Fcb->Header.AllocationSize);
        }
    }
    else
    {
        Fcb->Header.FileSize = liSaveFileSize;
        Fcb->Header.AllocationSize = liSaveAllocation;
    }

    //
    // DownConvert file resource to shared
    //
    ExConvertExclusiveToSharedLite( &Fcb->NPFcb->Resource);

    return ntStatus;
}
