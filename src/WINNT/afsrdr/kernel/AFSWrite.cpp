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
// File: AFSWrite.cpp
//

#include "AFSCommon.h"

static
NTSTATUS
AFSCachedWrite( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp,
                IN LARGE_INTEGER StartingByte,
                IN ULONG ByteCount,
                IN BOOLEAN ForceFlush);
static
NTSTATUS
AFSNonCachedWrite( IN PDEVICE_OBJECT DeviceObject,
                   IN PIRP Irp,
                   IN LARGE_INTEGER StartingByte,
                   IN ULONG ByteCount);

static
NTSTATUS 
AFSExtendingWrite( IN AFSFcb *Fcb,
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
    return AFSCommonWrite(DeviceObject, Irp, NULL);
}

NTSTATUS
AFSCommonWrite( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp,
                IN HANDLE OnBehalfOf)
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
    BOOLEAN            bCompleteIrp = TRUE;
    BOOLEAN            bForceFlush = FALSE;
    BOOLEAN            bLockOK;
    BOOLEAN            bMapped = TRUE;
    HANDLE             hCallingUser = OnBehalfOf;
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

        //
        // If we are in shutdown mode then fail the request
        //

        if( BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSCommonWrite (%08lX) Open request after shutdown\n",
                          Irp);        

            try_return( ntStatus = STATUS_TOO_LATE);
        }

        liStartingByte = pIrpSp->Parameters.Write.ByteOffset;
        bPagingIo      = BooleanFlagOn( Irp->Flags, IRP_PAGING_IO);
        bNonCachedIo   = BooleanFlagOn( Irp->Flags, IRP_NOCACHE);
        ulByteCount    = pIrpSp->Parameters.Write.Length;

        if( pFcb->Header.NodeTypeCode != AFS_IOCTL_FCB &&
            pFcb->Header.NodeTypeCode != AFS_FILE_FCB)
        {

            if( pFcb->Header.NodeTypeCode == AFS_SPECIAL_SHARE_FCB)
            {

                DbgPrint("AFSCommonWrite on special share\n");

                Irp->IoStatus.Information = ulByteCount;

                try_return( ntStatus = STATUS_SUCCESS);
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCommonWrite Attempted write (%08lX) on an invalid node type %08lX\n",
                          Irp,
                          pFcb->Header.NodeTypeCode);

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // If this is a write against an IOCtl node then handle it 
        // in a different pathway
        //

        if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCommonWrite (%08lX) Processing file (PIOCTL) Offset %I64X Length %08lX Irp Flags %08lX\n",
                          Irp,
                          liStartingByte.QuadPart,
                          ulByteCount,
                          Irp->Flags);

            ntStatus = AFSIOCtlWrite( DeviceObject,
                                      Irp);

            try_return( ntStatus);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSCommonWrite (%08lX) Processing file %wZ Offset %I64X Length %08lX Irp Flags %08lX\n",
                      Irp,
                      &pFcb->DirEntry->DirectoryEntry.FileName,
                      liStartingByte.QuadPart,
                      ulByteCount,
                      Irp->Flags);

        //
        // Is the Cache not there yet?  Exit.
        //
        if( NULL == pDeviceExt->Specific.RDR.CacheFileObject) 
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCommonWrite (%08lX) Request failed due to AFS cache closed\n",
                          Irp);

            try_return( ntStatus = STATUS_TOO_LATE );
        }

        if( pFcb->RootFcb != NULL &&
            BooleanFlagOn( pFcb->RootFcb->DirEntry->Type.Volume.VolumeInformation.Characteristics, FILE_READ_ONLY_DEVICE))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCommonWrite (%08lX) Request failed due to read only volume\n",
                          Irp);

            try_return( ntStatus = STATUS_ACCESS_DENIED);
        }

        //
        // We need to know on whose behalf we have been called (which
        // we will eventually tell to the server - for non paging
        // writes).  If we were posted then we were told.  If this si
        // the first time we saw the irp then we grab it now.
        //
        if( NULL == OnBehalfOf ) 
        {

            hCallingUser = PsGetCurrentProcessId();
        } 
        else
        {

            hCallingUser = OnBehalfOf;
        }

        //
        // Check for zero length write
        //

        if( ulByteCount == 0)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCommonWrite (%08lX) Request completed due to zero length\n",
                          Irp);

            try_return( ntStatus);
        }

        //
        // Is this Fcb valid???
        //

        if( BooleanFlagOn( pFcb->Flags, AFS_FCB_INVALID))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCommonWrite (%08lX) Failing request due to INVALID fcb\n",
                          Irp);        

            Irp->IoStatus.Information = 0;

            try_return( ntStatus = STATUS_FILE_DELETED);
        }

        if( BooleanFlagOn( pFcb->Flags, AFS_FCB_DELETED))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCommonWrite (%08lX) Request failed due to file deleted\n",
                          Irp);        

            try_return( ntStatus = STATUS_FILE_DELETED);
        }

        if( FlagOn( pIrpSp->MinorFunction, IRP_MN_COMPLETE)) 
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCommonWrite (%08lX) IRP_MN_COMPLETE being processed\n",
                          Irp);        

            CcMdlWriteComplete(pFileObject, &pIrpSp->Parameters.Write.ByteOffset, Irp->MdlAddress);

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
        if( NULL != pFileObject->SectionObjectPointer->DataSectionObject && !bPagingIo && bNonCachedIo) 
        {
            bNonCachedIo = FALSE;
            bForceFlush = TRUE;
        }

        if( !bNonCachedIo && pFileObject->PrivateCacheMap == NULL)
        {

            __try
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCommonWrite Initialize caching on Fcb %08lX FileObject %08lX\n",
                                  pFcb,
                                  pFileObject);

                CcInitializeCacheMap( pFileObject,
                                      (PCC_FILE_SIZES)&pFcb->Header.AllocationSize,
                                      FALSE,
                                      &AFSCacheManagerCallbacks,
                                      pFcb);

                CcSetReadAheadGranularity( pFileObject, 
                                           READ_AHEAD_GRANULARITY);

            }
            __except( EXCEPTION_EXECUTE_HANDLER)
            {
                    
                ntStatus = GetExceptionCode();

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSCommonWrite (%08lX) Exception thrown while initializing cache map Status %08lX\n",
                              Irp,
                              ntStatus);        
            }

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }
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
        // Save off the PID if this is not a paging IO
        //

        if( !bPagingIo)
        {

            pFcb->Specific.File.ExtentProcessId = (ULONGLONG)PsGetCurrentProcessId();
        }

        //
        // We should be ready to go.  So first of all ask for the extents
        // Provoke a get of the extents - if we need to.
        //

        if( !bPagingIo && !bNonCachedIo)
        {

            ntStatus = AFSRequestExtentsAsync( pFcb, &liStartingByte, ulByteCount);

            if (!NT_SUCCESS(ntStatus)) 
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSCommonWrite (%08lX) Failed to request extents Status %08lX\n",
                              Irp,
                              ntStatus);        

                try_return( ntStatus );
            }
        }

        //
        // If they are not mapped and we are the Lazy Writer then just
        // say "not now"
        //
        if (!bMapped && pFcb->Specific.File.LazyWriterThread == PsGetCurrentThread())
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCommonWrite (%08lX) Failing lazy writer for unmapped request\n",
                          Irp);        

            try_return ( ntStatus = STATUS_FILE_LOCK_CONFLICT);
        }

        //
        // Take locks 
        //
        //   - if Paging then we need to nothing (the precalls will
        //     have acquired the paging resource), for clarity we will collect 
        //     the paging resource 
        //   - If extending Write then take the fileresource EX (EOF will change, Allocation will only move out)
        //   - Otherwise we collect the file shared, check against extending and 
        //

        bLockOK = FALSE;

        do 
        {

            if( !bPagingIo)
            {

                bExtendingWrite = (((liStartingByte.QuadPart + ulByteCount) >= 
                                    pFcb->Header.FileSize.QuadPart) ||
                                   (liStartingByte.LowPart == FILE_WRITE_TO_END_OF_FILE &&
                                    liStartingByte.HighPart == -1)) ;
            }

            if( bPagingIo) 
            {

                ASSERT( NULL != OnBehalfOf || ExIsResourceAcquiredLite( &pNPFcb->Resource ));
    
                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCommonWrite Acquiring Fcb PagingIo lock %08lX SHARED %08lX\n",
                                                         &pNPFcb->PagingResource,
                                                         PsGetCurrentThread());

                AFSAcquireShared( &pNPFcb->PagingResource,
                                  TRUE);

                bReleasePaging = TRUE;
                
                //
                // We have the correct lock - we cannot have the wrong one
                //
                bLockOK = TRUE;
            } 
            else if( bExtendingWrite) 
            {
                //
                // Check for lock inversion
                //

                ASSERT( !ExIsResourceAcquiredLite( &pNPFcb->PagingResource ));
    
                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCommonWrite Acquiring Fcb lock %08lX EXCL %08lX\n",
                                                         &pNPFcb->Resource,
                                                         PsGetCurrentThread());

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

                //
                // We have the correct lock - even if we don't end up truncating
                //
                bLockOK = TRUE;
            }
            else
            {
                ASSERT( !ExIsResourceAcquiredLite( &pNPFcb->PagingResource ));
    
                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCommonWrite Acquiring Fcb lock %08lX SHARED %08lX\n",
                                                         &pNPFcb->Resource,
                                                         PsGetCurrentThread());

                AFSAcquireShared( &pNPFcb->Resource,
                                  TRUE);

                bReleaseMain = TRUE;

                //
                // Have things moved?  Are we extending? If so, the the lock isn't OK
                //
                bLockOK = (liStartingByte.QuadPart + ulByteCount) < pFcb->Header.FileSize.QuadPart;

                if (!bLockOK)
                {
                    AFSReleaseResource( &pNPFcb->Resource);
                    bReleaseMain = FALSE;
                }
            }
        }
        while (!bLockOK);

        //
        // Check the BR locks on the file. 
        //

        if( !bPagingIo && 
            !FsRtlCheckLockForWriteAccess( &pFcb->Specific.File.FileLock,
                                           Irp)) 
        {
            
            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCommonWrite (%08lX) Request failed due to lock conflict\n",
                          Irp);        

            try_return( ntStatus = STATUS_FILE_LOCK_CONFLICT);
        }

        if( bExtendingWrite)
        {

            ntStatus = AFSExtendingWrite( pFcb, pFileObject, (liStartingByte.QuadPart + ulByteCount));

            if( !NT_SUCCESS(ntStatus)) 
            {
                
                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSCommonWrite (%08lX) Failed extending write request Status %08lX\n",
                              Irp,
                              ntStatus);        

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
            
            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCommonWrite (%08lX) Processing CACHED request\n",
                          Irp);        

            ntStatus = AFSCachedWrite( DeviceObject, Irp, liStartingByte, ulByteCount, bForceFlush);

        }
        else
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCommonWrite (%08lX) Processing NON-CACHED request\n",
                          Irp);        

            ntStatus = AFSNonCachedWrite( DeviceObject, Irp,  liStartingByte, ulByteCount);
        }

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSCommonWrite (%08lX) Process complete Status %08lX\n",
                      Irp,
                      ntStatus);        

        ObDereferenceObject(pFileObject);

        if( bReleaseMain)
        {

            AFSReleaseResource( &pNPFcb->Resource);
        }

        if( bReleasePaging)
        {

            AFSReleaseResource( &pNPFcb->PagingResource);
        }

        if( bCompleteIrp) 
        {
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

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSIOCtlWrite Acquiring Fcb lock %08lX SHARED %08lX\n",
                                                         &pFcb->NPFcb->Resource,
                                                         PsGetCurrentThread());

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
                        
                ASSERT( pParentDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY);

                stParentFID = pParentDcb->DirEntry->DirectoryEntry.FileId;
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
AFSNonCachedWrite( IN PDEVICE_OBJECT DeviceObject,
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
    BOOLEAN            bSynchronousFo = BooleanFlagOn( pFileObject->Flags, FO_SYNCHRONOUS_IO);
    AFSDeviceExt      *pDevExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    ULONG              ulRequestCount = 0;

    __Enter
    {
        Irp->IoStatus.Information = 0;

        if (ByteCount > pDevExt->Specific.RDR.MaxIo.QuadPart) 
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNonCachedWrite (%08lX) Request larger than MaxIO %I64X\n",
                          Irp,
                          pDevExt->Specific.RDR.MaxIo.QuadPart);        

            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        //
        // Get the mapping for the buffer
        //
        pSystemBuffer = AFSLockSystemBuffer( Irp,
                                             ByteCount);

        if( pSystemBuffer == NULL)
        {
                
            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNonCachedWrite (%08lX) Failed to map system buffer\n",
                          Irp);        

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }


        //
        // Provoke a get of the extents - if we need to.
        //

        while (TRUE) 
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNonCachedWrite (%08lX) Requesting extents for Offset %I64X Length %08lX\n",
                          Irp,
                          StartingByte.QuadPart,
                          ByteCount);

            ntStatus = AFSRequestExtents( pFcb, 
                                          &StartingByte, 
                                          ByteCount, 
                                          &bExtentsMapped );

            if (!NT_SUCCESS(ntStatus)) 
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSNonCachedWrite (%08lX) Failed to request extents Status %08lX\n",
                              Irp,
                              ntStatus);        

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            if (bExtentsMapped)
            {
                //
                // We know that they *did* map.  Now lock up and then
                // if we are still mapped pin the extents.
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSNonCachedWrite Acquiring Fcb extents lock %08lX SHARED %08lX\n",
                                                         &pFcb->NPFcb->Specific.File.ExtentsResource,
                                                         PsGetCurrentThread());

                AFSAcquireShared( &pFcb->NPFcb->Specific.File.ExtentsResource, TRUE );
                bLocked = TRUE;
                if ( AFSDoExtentsMapRegion( pFcb, &StartingByte, ByteCount, &pStartExtent, &pIgnoreExtent )) 
                {
                    //
                    // We are all set.  Pin the extents against being truncated
                    //
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
            // Wait for it
            //

            ntStatus =  AFSWaitForExtentMapping ( pFcb );

            if (!NT_SUCCESS(ntStatus)) 
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSNonCachedWrite (%08lX) Failed to wait for mapping Status %08lX\n",
                              Irp,
                              ntStatus);        

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }
        }
        
        //
        // As per the read path - 
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNonCachedWrite (%08lX) Retrieving mapped extents for Offset %I64X Length %08lX\n",
                      Irp,
                      StartingByte.QuadPart,
                      ByteCount);

        ntStatus = AFSGetExtents( pFcb, 
                                  &StartingByte, 
                                  ByteCount, 
                                  pStartExtent, 
                                  &extentsCount);
        
        if (!NT_SUCCESS(ntStatus)) 
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNonCachedWrite (%08lX) Failed to retrieve mapped extents Status %08lX\n",
                          Irp,
                          ntStatus);        

            try_return( ntStatus );
        }
        
        AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNonCachedWrite (%08lX) Successfully retrieved map extents count %08lX\n",
                      Irp,
                      extentsCount);

        if (extentsCount > AFS_MAX_STACK_IO_RUNS) 
        {

            pIoRuns = (AFSIoRun*) ExAllocatePoolWithTag( PagedPool,
                                                         extentsCount * sizeof( AFSIoRun ),
                                                         AFS_IO_RUN_TAG );
            if (NULL == pIoRuns) 
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSNonCachedWrite (%08lX) Failed to allocate IO run block\n",
                              Irp);

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
            }
        } 
        else 
        {
            
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

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNonCachedWrite (%08lX) Failed to initialize IO run block Status %08lX\n",
                          Irp,
                          ntStatus);

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

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNonCachedWrite (%08lX) Failed to allocate IO gather block\n",
                          Irp);

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

        if (pGatherIo->Synchronous) 
        {
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

        AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNonCachedWrite (%08lX) AFSStartIos completed Status %08lX\n",
                      Irp,
                      ntStatus);

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

        if( NT_SUCCESS( ntStatus))
        {

            ntStatus = KeWaitForSingleObject( &pGatherIo->Event,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              NULL);
            
            if( NT_SUCCESS(ntStatus)) 
            {
                ntStatus = pGatherIo->Status;
            } 
        }

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNonCachedWrite (%08lX) AFSStartIos wait completed Status %08lX\n",
                          Irp,
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // Since this is dirty we can mark the extents dirty now
        // (asynch we would fire off a thread to wait for the event and the mark it dirty)
        //

        AFSMarkDirty( pFcb, &StartingByte, ByteCount);

#ifdef AFS_FLUSH_PAGES_SYNCHRONOUSLY

        AFSFlushExtents( pFcb);

#endif

        AFSQueueFlushExtents( pFcb);

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

        AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNonCachedWrite (%08lX) Completed request Status %08lX\n",
                      Irp,
                      ntStatus);

        if (NT_SUCCESS(ntStatus) &&
            !bPagingIo &&
            bSynchronousFo) 
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

        if (pGatherIo) 
        {
            ExFreePoolWithTag(pGatherIo, AFS_GATHER_TAG);
        }

        if (NULL != pIoRuns && stIoRuns != pIoRuns) 
        {
            ExFreePoolWithTag(pIoRuns, AFS_IO_RUN_TAG);
        }

        if (bCompleteIrp) 
        {
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
AFSCachedWrite( IN PDEVICE_OBJECT DeviceObject,
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
    BOOLEAN            bSynchronousFo = BooleanFlagOn( pFileObject->Flags, FO_SYNCHRONOUS_IO);
    BOOLEAN            bMapped = FALSE; 

    Irp->IoStatus.Information = 0;

    __Enter
    {
        //
        // Get the mapping for the buffer
        //

        pSystemBuffer = AFSLockSystemBuffer( Irp,
                                             ByteCount);

        if( pSystemBuffer == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCachedWrite (%08lX) Failed to lock system buffer\n",
                          Irp);        

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        __try 
        {

            if( BooleanFlagOn( pIrpSp->MinorFunction, IRP_MN_MDL))
            {

                CcPrepareMdlWrite( pFileObject,
                                   &StartingByte,
                                   ByteCount,
                                   &Irp->MdlAddress,
                                   &Irp->IoStatus);

                ntStatus = Irp->IoStatus.Status;

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSCommonWrite (%08lX) Failed to process MDL read Status %08lX\n",
                                  Irp,
                                  ntStatus);     

                    try_return( ntStatus);
                }
            }

            if( !CcCopyWrite( pFileObject,
                              &StartingByte,
                              ByteCount,
                              TRUE,
                              pSystemBuffer)) 
            {
                //
                // Failed to process request.
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSCachedWrite (%08lX) Failed to issue CcCopyWrite\n", Irp);

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
        __except( EXCEPTION_EXECUTE_HANDLER)
        {
            ntStatus = GetExceptionCode();

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCachedWrite (%08lX) CcCopyWrite() or CcFlushCache() threw exception Status %08lX\n",
                          Irp,
                          ntStatus);        
        }
        
        if (!NT_SUCCESS(ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCachedWrite (%08lX) Failed cache write Status %08lX\n",
                          Irp,
                          ntStatus);        

            try_return ( ntStatus );
        }

        Irp->IoStatus.Information = ByteCount;
        
        if( bSynchronousFo)
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
        
        NOTHING;
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
AFSExtendingWrite( IN AFSFcb *Fcb,
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
    // Need to drop our lock on teh Fcb while this call is made since it could
    // result in the service invalidating the node requiring the lock
    //

    AFSReleaseResource( &Fcb->NPFcb->Resource);

    ntStatus = AFSUpdateFileInformation( AFSRDRDeviceObject, Fcb);

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSExtendingWrite Acquiring Fcb lock %08lX EXCL %08lX\n",
                                                         &Fcb->NPFcb->Resource,
                                                         PsGetCurrentThread());

    AFSAcquireExcl( &Fcb->NPFcb->Resource,
                    TRUE);

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
