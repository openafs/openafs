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
        ntStatus = AFSRequestExtents( pFcb, &StartingByte, ByteCount, &mapped );

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
    BOOLEAN            locked = FALSE;
    AFSGatherIo       *pGatherIo = NULL;
    AFSIoRun          *pIoRuns = NULL;
    AFSIoRun           stIoRuns[AFS_MAX_STACK_IO_RUNS];
    ULONG              extentsCount = 0;
    AFSExtent         *pStartExtent;
    AFSExtent         *pEndExtent;
    BOOLEAN            bExtentsMapped = FALSE;
    BOOLEAN            bCompleteIrp = TRUE;
    ULONG              ulReadByteCount;
    ULONG              ulByteCount;
    AFSDeviceExt      *pDevExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;

    __Enter
    {

        ulByteCount = pIrpSp->Parameters.Read.Length;

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
        while (TRUE) 
        {
            ntStatus = AFSRequestExtents( pFcb, &StartingByte, ulReadByteCount, &bExtentsMapped );

            if (!NT_SUCCESS(ntStatus)) 
            {
                try_return( ntStatus );
            }

            if (bExtentsMapped)
            {
                break;
            }

            ntStatus =  AFSWaitForExtentMapping ( pFcb );

            if (!NT_SUCCESS(ntStatus)) 
            {
                try_return( ntStatus );
            }
        }

        //
        // At this stage we know that the extents are fully mapped and
        // that, because we too a reference they won't be unmapped.
        // Thus the list will not move between the start and end.
        // Hence we only actually need to lock the extents when we get
        // the first extent inside AFSGetExtents, however unless the
        // lock gets very hot it feels better to keep things
        // protected.
        // 
        AFSAcquireShared( &pFcb->NPFcb->Specific.File.ExtentsResource, TRUE );
        locked = TRUE;

        ntStatus = AFSGetExtents( pFcb, 
                                  &StartingByte, 
                                  ulReadByteCount, 
                                  &pStartExtent, 
                                  &pEndExtent, 
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
                                  extentsCount );

        if (!NT_SUCCESS(ntStatus)) 
        {
            try_return( ntStatus );
        }

        AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource );
        locked = FALSE;


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

        if (locked) 
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
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    IO_STACK_LOCATION *pIrpSp;
    AFSFcb *pFcb = NULL;
    BOOLEAN bReleaseMain = FALSE;
    BOOLEAN bReleasePaging = FALSE;
    BOOLEAN bPagingIo = FALSE;
    BOOLEAN bNonCachedIo = FALSE;
    PFILE_OBJECT pFileObject = NULL;
    LARGE_INTEGER liStartingByte;
    ULONG ulByteCount;
    void *pSystemBuffer = NULL;
    BOOLEAN bDerefExtents = FALSE;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    
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

        AFSReferenceExtents( pFcb );

        bDerefExtents = TRUE;

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
        // TODO
        // If we get a non cached IO for a cached file we should do a purge.  
        // For now we will just promote to cached
        //
        if (CcIsFileCached(pFileObject) && !bPagingIo) {
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
        // We must now handle this request as a 'typical' read. 
        //

        if( !bPagingIo &&
            !bNonCachedIo)
        {
            ntStatus = CachedRead( DeviceObject, Irp, liStartingByte, ulByteCount);

            try_return( ntStatus );
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

        if( bDerefExtents)
        {

            AFSDereferenceExtents( pFcb);
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
    UNICODE_STRING uniFullName;
    AFSPIOCtlIOResultCB stIOResultCB;
    ULONG ulBytesReturned = 0;

    __Enter
    {

        uniFullName.Length = 0;
        uniFullName.Buffer = NULL;

        stIORequestCB.MappedBuffer = NULL;
        stIORequestCB.BufferLength = 0;

        if( pIrpSp->Parameters.Read.Length == 0)
        {

            //
            // Nothing to do in this case
            //

            try_return( ntStatus);
        }

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        AFSAcquireShared( &pFcb->NPFcb->Resource,
                          TRUE);

        ntStatus = AFSGetFullName( pFcb,
                                   &uniFullName);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Locak down the buffer
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
                                      &uniFullName,
                                      NULL,
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

        if( uniFullName.Buffer != NULL)
        {

            ExFreePool( uniFullName.Buffer);
        }

        if( pFcb != NULL)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }

        //
        // Complete the request
        //

        AFSCompleteRequest( Irp,
                            ntStatus);
    }

    return ntStatus;
}
