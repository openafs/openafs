//
// File: AFSRead.cpp
//

#include "AFSCommon.h"

static
NTSTATUS
CachedRead( IN PDEVICE_OBJECT DeviceObject,
            IN PIRP Irp,
            LARGE_INTEGER StartingByte,
            ULONG ByteCount)
{
    NTSTATUS           ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT       pFileObject = pIrpSp->FileObject;
    AFSFcb            *pFcb = (AFSFcb *)pFileObject->FsContext;
    AFSCcb            *pCcb = (AFSCcb *)pFileObject->FsContext2;
    BOOLEAN            bSynchronousIo = BooleanFlagOn( pFileObject->Flags, FO_SYNCHRONOUS_IO);
    VOID              *pSystemBuffer = NULL;
    BOOLEAN            mapped = FALSE;

    __Enter
    {
        //
        // Provoke a get of the extents - if we need to.
        //
        ntStatus = AFSRequestExtents( pFcb, &StartingByte, ByteCount, &mapped);

        if (!NT_SUCCESS(ntStatus)) {
            try_return( ntStatus );
        }

        //
        // This is a top level irp. Init the caching if it has not yet
        // been initialzed for this FO
        //
    
        if( pFileObject->PrivateCacheMap == NULL)
        {
                
            __try 
            {
            
                CcInitializeCacheMap( pFileObject,
                                      (PCC_FILE_SIZES)&pFcb->Header.AllocationSize,
                                      FALSE,
                                      &AFSCacheManagerCallbacks,
                                      pFcb);
                CcSetReadAheadGranularity( pFileObject, 
                                           READ_AHEAD_GRANULARITY);
            }
            __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()))
            {
                ntStatus = GetExceptionCode();
                try_return( ntStatus );
            }

        }

        //
        // Process the read 
        //

        if( FlagOn( pIrpSp->MinorFunction, IRP_MN_MDL)) 
        {
            __try 
            {
            
                CcMdlRead( pFileObject,
                           &StartingByte,
                           ByteCount,
                           &Irp->MdlAddress,
                           &Irp->IoStatus);
                ntStatus = Irp->IoStatus.Status;
            }
            __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()))
            {
                ntStatus = GetExceptionCode();
            }

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint("AFSCommonRead Failed to process mdl read Status %08lX\n", ntStatus);

                try_return( ntStatus);
            }
        }
        else
        {

            pSystemBuffer = AFSLockSystemBuffer( Irp,
                                                 ByteCount);

            if( pSystemBuffer == NULL)
            {

                AFSPrint("AFSCommonRead Failed to retrieve system buffer\n");

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            __try 
            {
                if( !CcCopyRead( pFileObject,
                                 &StartingByte,
                                 ByteCount,
                                 IoIsOperationSynchronous(Irp),
                                 pSystemBuffer,
                                 &Irp->IoStatus)) 
                {

                    //
                    // Failed to process request.
                    //

                    AFSPrint("AFSCommonRead Failed to issue cached read Status %08lX\n", Irp->IoStatus.Status);

                    try_return( ntStatus = Irp->IoStatus.Status);
                }
            }
            __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()))
            {
                ntStatus = GetExceptionCode();
            }

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint("AFSCommonRead Failed to process read Status %08lX\n", ntStatus);

                try_return( ntStatus);
            }
        }

        ntStatus = Irp->IoStatus.Status;

        //
        // Update the CBO if this is a sync read
        //

        if( bSynchronousIo)
        {

            pFileObject->CurrentByteOffset.QuadPart = StartingByte.QuadPart + ByteCount;
        }

        try_return( ntStatus);
try_exit:
        //
        // Iosb was set up by the CcCopyRead
        //
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
NonCachedRead( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp,
               LARGE_INTEGER StartingByte)
{
    NTSTATUS           ntStatus = STATUS_UNSUCCESSFUL;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT       pFileObject = pIrpSp->FileObject;
    AFSFcb            *pFcb = (AFSFcb *)pFileObject->FsContext;
    BOOLEAN            bSynchronousIo = BooleanFlagOn( pFileObject->Flags, FO_SYNCHRONOUS_IO);
    VOID              *pSystemBuffer = NULL;
    BOOLEAN            bPagingIo = BooleanFlagOn( Irp->Flags, IRP_PAGING_IO);
    BOOLEAN            bLocked = FALSE;
    AFSGatherIo       *pGatherIo = NULL;
    AFSIoRun          *pIoRuns = NULL;
    AFSIoRun           stIoRuns[AFS_MAX_STACK_IO_RUNS];
    ULONG              extentsCount = 0;
    AFSExtent         *pStartExtent = NULL;
    AFSExtent         *pIgnoreExtent = NULL;
    BOOLEAN            bExtentsMapped = FALSE;
    BOOLEAN            bCompleteIrp = TRUE;
    ULONG              ulReadByteCount;
    ULONG              ulByteCount;
    AFSDeviceExt      *pDevExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    BOOLEAN            bDerefExtents = FALSE;

    __Enter
    {

        ulByteCount = pIrpSp->Parameters.Read.Length;

        if (ulByteCount > pDevExt->Specific.RDR.MaxIo.QuadPart) {
            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        pSystemBuffer = AFSLockSystemBuffer( Irp,
                                             ulByteCount);

        if( pSystemBuffer == NULL)
        {

            AFSPrint("AFSCommonRead Failed to retrieve system buffer\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        if( StartingByte.QuadPart + ulByteCount > pFcb->Header.FileSize.QuadPart) 
        {
            ULONG zeroCount = (ULONG) (StartingByte.QuadPart + ulByteCount - pFcb->Header.FileSize.QuadPart);
            ulReadByteCount = (ULONG)(pFcb->Header.FileSize.QuadPart - StartingByte.QuadPart);

            //
            // Clear up to EOF
            //
            RtlZeroMemory( ((PCHAR)pSystemBuffer) + ulReadByteCount, zeroCount);
        }
        else
        {
            ulReadByteCount = ulByteCount;
        }


        //
        // Ensure that everything is mapped
        //

        //
        // Provoke a get of the extents - if we need to.
        // TODO - right now we wait for the extents to be there.  We should 
        // Post if we can or return STATUS_FILE_LOCKED otherwise
        //
        //
        // prime the pump for the hint to getting the extents
        //
        pStartExtent = NULL;
        while (TRUE) 
        {
            ntStatus = AFSRequestExtents( pFcb, &StartingByte, ulReadByteCount, &bExtentsMapped );

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
                if ( AFSDoExtentsMapRegion( pFcb, &StartingByte, ulReadByteCount, &pStartExtent, &pIgnoreExtent )) {
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
            //
            // Note that if we are not full mapped then pStartExtent
            // is the next best place to start looking next time
            //
            ntStatus =  AFSWaitForExtentMapping ( pFcb );

            if (!NT_SUCCESS(ntStatus)) 
            {
                try_return( ntStatus );
            }
        }

        //
        // At this stage we know that the extents are fully mapped and
        // that, because we took a reference they won't be unmapped.
        // Thus the list will not move between the start and end.
        // 

        ntStatus = AFSGetExtents( pFcb, 
                                  &StartingByte, 
                                  ulReadByteCount, 
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
                                  ulReadByteCount, 
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
        Irp->IoStatus.Information = ulByteCount;

        ntStatus = AFSStartIos( pDevExt->Specific.RDR.CacheFileObject,
                                IRP_MJ_READ,
                                IRP_READ_OPERATION | IRP_SYNCHRONOUS_API,
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
        
        if (NT_SUCCESS(ntStatus)) {
            ntStatus = pGatherIo->Status;
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
            pFileObject->CurrentByteOffset.QuadPart = StartingByte.QuadPart + ulByteCount;
        }

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

//
// Function: AFSRead
//
// Description:
//
//      This function is the dispatch handler for the IRP_MJ_READ request
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSRead( IN PDEVICE_OBJECT DeviceObject,
         IN PIRP Irp)
{
    
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    AFSDeviceExt       *pDeviceExt;
    IO_STACK_LOCATION  *pIrpSp;
    AFSFcb             *pFcb = NULL;
    BOOLEAN             bReleaseMain = FALSE;
    BOOLEAN             bReleasePaging = FALSE;
    BOOLEAN             bPagingIo = FALSE;
    BOOLEAN             bNonCachedIo = FALSE;
    BOOLEAN             bCompleteIrp = TRUE;
    PFILE_OBJECT        pFileObject = NULL;
    LARGE_INTEGER       liStartingByte;
    ULONG               ulByteCount;
    VOID               *pSystemBuffer = NULL;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    
    __Enter
    {
        
        //
        // Decode the fileobject
        //
        pFileObject = pIrpSp->FileObject;

        //
        // There is a risk (albeit infinitely small) that the Irp will
        // complete before this function exits, then a cleanup and
        // close will happen and the FCB will be torn down before we
        // get to the try_exit.  Pin the file Object which will pin the FCB
        //

        ObReferenceObject( pFileObject);

        pFcb = (AFSFcb *)pFileObject->FsContext;

        if( pFcb->Header.NodeTypeCode != AFS_IOCTL_FCB &&
            pFcb->Header.NodeTypeCode != AFS_FILE_FCB)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // If this is a read against an IOCtl node then handle it 
        // in a different pathway
        //

        if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {

            ntStatus = AFSIOCtlRead( DeviceObject,
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

        if( pIrpSp->Parameters.Read.Length == 0)
        {

            AFSPrint("AFSCommonRead Processed zero length read\n");

            try_return( ntStatus);
        }

        if ( FlagOn(pIrpSp->MinorFunction, IRP_MN_COMPLETE) ) {
        
            AFSPrint("MN_COMPLETE \n");

            CcMdlReadComplete(pIrpSp->FileObject, Irp->MdlAddress);

            //
            // Mdl is now Deallocated
            //

            Irp->MdlAddress = NULL;
        
            try_return( ntStatus = STATUS_SUCCESS );
        }

        bPagingIo = BooleanFlagOn( Irp->Flags, IRP_PAGING_IO);
        bNonCachedIo = BooleanFlagOn( Irp->Flags,IRP_NOCACHE);

        liStartingByte = pIrpSp->Parameters.Read.ByteOffset;
        ulByteCount = pIrpSp->Parameters.Read.Length;

        //
        // If we get a non cached IO for a cached file we should do a purge.  
        // For now we will just promote to cached
        //
        if (NULL != pFileObject->SectionObjectPointer->DataSectionObject && !bPagingIo) {
            bNonCachedIo = FALSE;
        }

        //
        // We acquire the main/paging reosurce first to synchronize
        // against size checks.
        //

        if( bPagingIo)
        {

            AFSAcquireShared( &pFcb->NPFcb->PagingResource,
                              TRUE);

            bReleasePaging = TRUE;
        }
        else
        {

            AFSAcquireShared( &pFcb->NPFcb->Resource,
                              TRUE);

            bReleaseMain = TRUE;

            //
            // Check the BR locks
            //

            if( !FsRtlCheckLockForReadAccess( &pFcb->Specific.File.FileLock,
                                              Irp)) 
            {

                AFSPrint("AFSCommonRead Failed BR lock check for cached I/O\n");

                try_return( ntStatus = STATUS_FILE_LOCK_CONFLICT);
            }
        }

        if( BooleanFlagOn( pFcb->Flags, AFS_FCB_DELETED))
        {

            try_return( ntStatus = STATUS_FILE_DELETED);
        }

        //
        // If the read starts beyond End of File, return EOF.
        //

        if( liStartingByte.QuadPart >= pFcb->Header.FileSize.QuadPart) 
        {

            AFSPrint("AFSCommonRead End of file\n");

            try_return( ntStatus = STATUS_END_OF_FILE);
        }

        //
        //

        if( liStartingByte.QuadPart + ulByteCount > pFcb->Header.FileSize.QuadPart) 
        {

            ulByteCount = (ULONG)(pFcb->Header.FileSize.QuadPart - liStartingByte.QuadPart);
        }

        //
        // Is this Fcb valid???
        //

        if( BooleanFlagOn( pFcb->Flags, AFS_FCB_INVALID))
        {

            AFSPrint("AFSCommonRead Dropping request for invalid Fcb\n");

            //
            // OK, this Fcb was probably deleted then renamed into but we re-used the source fcb
            // hence this read is bogus. Drop it
            //

            Irp->IoStatus.Information = ulByteCount;

            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // We must now handle this request as a 'typical' read.  The
        // called request completes the IRP for us.
        //
        bCompleteIrp = FALSE;

        if( !bPagingIo &&
            !bNonCachedIo)
        {

            ntStatus = CachedRead( DeviceObject, Irp, liStartingByte, ulByteCount);
        }
        else
        {

            ntStatus = NonCachedRead( DeviceObject, Irp,  liStartingByte);
        }

try_exit:

        if( bReleasePaging)
        {

            AFSReleaseResource( &pFcb->NPFcb->PagingResource);
        }

        if( bReleaseMain)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        } 

        if ( bCompleteIrp )
        {
            AFSCompleteRequest( Irp, ntStatus);
        }
            

        ObDereferenceObject( pFileObject);
    }

    return ntStatus;
}

NTSTATUS
AFSIOCtlRead( IN PDEVICE_OBJECT DeviceObject,
              IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSPIOCtlIORequestCB stIORequestCB;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    AFSPIOCtlIOResultCB stIOResultCB;
    ULONG ulBytesReturned = 0;
    AFSFileID   stParentFID;

    __Enter
    {

        RtlZeroMemory( &stIORequestCB,
                       sizeof( AFSPIOCtlIORequestCB));

        if( pIrpSp->Parameters.Read.Length == 0)
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
            // The parent directory FID of the node
            //        

            if( pParentDcb->Header.NodeTypeCode != AFS_ROOT_ALL)
            {
                        
                if( pParentDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY)
                {

                    //
                    // Just the FID of the node
                    //

                    stParentFID = pParentDcb->DirEntry->DirectoryEntry.FileId;
                }
                else
                {

                   ntStatus = AFSRetrieveTargetFID( pParentDcb,
                                                    &stParentFID);

                   if( !NT_SUCCESS( ntStatus))
                   {

                        try_return( ntStatus);
                   }
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
                                                      pIrpSp->Parameters.Read.Length);

        if( stIORequestCB.MappedBuffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        stIORequestCB.BufferLength = pIrpSp->Parameters.Read.Length;

        stIOResultCB.BytesProcessed = 0;

        ulBytesReturned = sizeof( AFSPIOCtlIOResultCB);

        //
        // Issue the request to the service
        //

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_PIOCTL_READ,
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
        // Update the length read
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
