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
AFSWrite( IN PDEVICE_OBJECT LibDeviceObject,
          IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __try
    {

        ntStatus = AFSCommonWrite( AFSRDRDeviceObject, Irp, NULL);
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
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
    AFSCcb            *pCcb = NULL;
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
    BOOLEAN            bLockOK;
    HANDLE             hCallingUser = OnBehalfOf;
    ULONG              ulExtensionLength = 0;
    BOOLEAN            bRetry = FALSE;
    ULONGLONG          ullProcessId = (ULONGLONG)PsGetCurrentProcessId();

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __Enter
    {

        pFileObject = pIrpSp->FileObject;

        //
        // Extract the fileobject references
        //

        pFcb = (AFSFcb *)pFileObject->FsContext;
        pCcb = (AFSCcb *)pFileObject->FsContext2;

        ObReferenceObject( pFileObject);

        if( pFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCommonWrite Attempted write (%08lX) when pFcb == NULL\n",
                          Irp);

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        pNPFcb = pFcb->NPFcb;

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
            pFcb->Header.NodeTypeCode != AFS_FILE_FCB  &&
            pFcb->Header.NodeTypeCode != AFS_SPECIAL_SHARE_FCB)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCommonWrite Attempted write (%08lX) on an invalid node type %08lX\n",
                          Irp,
                          pFcb->Header.NodeTypeCode);

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
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
        else if( pFcb->Header.NodeTypeCode == AFS_SPECIAL_SHARE_FCB)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCommonWrite (%08lX) Processing file (SHARE) Offset %I64X Length %08lX Irp Flags %08lX\n",
                          Irp,
                          liStartingByte.QuadPart,
                          ulByteCount,
                          Irp->Flags);

            ntStatus = AFSShareWrite( DeviceObject,
                                      Irp);

            try_return( ntStatus);
        }

        //
        // Is the Cache not there yet?  Exit.
        //
        if( !BooleanFlagOn( AFSLibControlFlags, AFS_REDIR_LIB_FLAGS_NONPERSISTENT_CACHE) &&
            NULL == pDeviceExt->Specific.RDR.CacheFileObject)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCommonWrite (%08lX) Request failed due to AFS cache closed\n",
                          Irp);

            try_return( ntStatus = STATUS_TOO_LATE );
        }

        if( pFcb->ObjectInformation->VolumeCB != NULL &&
            BooleanFlagOn( pFcb->ObjectInformation->VolumeCB->VolumeInformation.Characteristics, FILE_READ_ONLY_DEVICE))
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
        // writes).  If we were posted then we were told.  If this is
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

        if( BooleanFlagOn( pFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_OBJECT_INVALID))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCommonWrite (%08lX) Failing request due to INVALID fcb\n",
                          Irp);

            Irp->IoStatus.Information = 0;

            try_return( ntStatus = STATUS_FILE_DELETED);
        }

        if( BooleanFlagOn( pCcb->DirectoryCB->Flags, AFS_DIR_ENTRY_DELETED) ||
            BooleanFlagOn( pFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_DELETED))
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
        }

        if( (!bPagingIo && !bNonCachedIo))
        {

            if( pFileObject->PrivateCacheMap == NULL)
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
                                          AFSLibCacheManagerCallbacks,
                                          pFcb);

                    CcSetReadAheadGranularity( pFileObject,
                                               pDeviceExt->Specific.RDR.MaximumRPCLength);

                    CcSetDirtyPageThreshold( pFileObject,
                                             AFS_DIRTY_CHUNK_THRESHOLD * pDeviceExt->Specific.RDR.MaximumRPCLength);
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

        if( !bPagingIo &&
            ( pFcb->Specific.File.ExtentRequestProcessId == 0 ||
              ( ullProcessId != (ULONGLONG)AFSSysProcess &&
                pFcb->Specific.File.ExtentRequestProcessId != ullProcessId)))
        {

            pFcb->Specific.File.ExtentRequestProcessId = ullProcessId;

            if( ullProcessId == (ULONGLONG)AFSSysProcess)
            {
                AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "%s Setting LastWriterExtentProcessId to system process for Fcb %p\n",
                              __FUNCTION__,
                              pFcb);
            }
        }

        //
        // We should be ready to go.  So first of all ask for the extents
        // Provoke a get of the extents - if we need to.
        //

        /*
        if( !bPagingIo && !bNonCachedIo)
        {

            ntStatus = AFSRequestExtentsAsync( pFcb, pCcb, &liStartingByte, ulByteCount);

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
        */

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

                //ASSERT( NULL != OnBehalfOf || ExIsResourceAcquiredLite( &pNPFcb->Resource ));

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
                          "AFSCommonWrite (%08lX) Processing CACHED request Offset %I64X Len %08lX\n",
                          Irp,
                          liStartingByte.QuadPart,
                          ulByteCount);

            ntStatus = AFSCachedWrite( DeviceObject, Irp, liStartingByte, ulByteCount, TRUE);

        }
        else
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCommonWrite (%08lX) Processing NON-CACHED request Offset %I64X Len %08lX\n",
                          Irp,
                          liStartingByte.QuadPart,
                          ulByteCount);

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

            AFSCompleteRequest( Irp,
                                ntStatus);
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

        if( pFcb->ObjectInformation->ParentObjectInformation != NULL)
        {

            //
            // The parent directory FID of the node
            //

            stParentFID = pFcb->ObjectInformation->ParentObjectInformation->FileId;
        }

        //
        // Set the control block up
        //

        stIORequestCB.RequestId = pCcb->RequestID;

        if( pFcb->ObjectInformation->VolumeCB != NULL)
        {
            stIORequestCB.RootId = pFcb->ObjectInformation->VolumeCB->ObjectInformation.FileId;
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
                                      &pCcb->AuthGroup,
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
// then pins the extents into memory (meaning that although we may
// add we will not remove).  Then it creates a scatter gather write
// and fires off the IRPs
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
    AFSGatherIo       *pGatherIo = NULL;
    AFSIoRun          *pIoRuns = NULL;
    AFSIoRun           stIoRuns[AFS_MAX_STACK_IO_RUNS];
    ULONG              extentsCount = 0, runCount = 0;
    AFSExtent         *pStartExtent = NULL;
    AFSExtent         *pIgnoreExtent = NULL;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT       pFileObject = pIrpSp->FileObject;
    AFSFcb            *pFcb = (AFSFcb *)pFileObject->FsContext;
    AFSCcb            *pCcb = (AFSCcb *)pFileObject->FsContext2;
    BOOLEAN            bSynchronousFo = BooleanFlagOn( pFileObject->Flags, FO_SYNCHRONOUS_IO);
    AFSDeviceExt      *pDevExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    ULONG              ulRequestCount = 0;
    LARGE_INTEGER      liCurrentTime, liLastRequestTime;
    AFSDeviceExt      *pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;
    PFILE_OBJECT       pCacheFileObject = NULL;
    BOOLEAN            bDerefExtents = FALSE;

    __Enter
    {
        Irp->IoStatus.Information = 0;

        if (ByteCount > pDevExt->Specific.RDR.MaxIo.QuadPart)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNonCachedWrite (%08lX) Request %08lX Actual %08lX larger than MaxIO %I64X\n",
                          Irp,
                          ByteCount,
                          pIrpSp->Parameters.Write.Length,
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

        AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNonCachedWrite Requesting extents for fid %08lX-%08lX-%08lX-%08lX Offset %I64X Length %08lX\n",
                      pFcb->ObjectInformation->FileId.Cell,
                      pFcb->ObjectInformation->FileId.Volume,
                      pFcb->ObjectInformation->FileId.Vnode,
                      pFcb->ObjectInformation->FileId.Unique,
                      StartingByte.QuadPart,
                      ByteCount);

        ntStatus = AFSRequestExtentsAsync( pFcb,
                                           pCcb,
                                           &StartingByte,
                                           ByteCount);

        if (!NT_SUCCESS(ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNonCachedWrite (%08lX) Failed to request extents Status %08lX\n",
                          Irp,
                          ntStatus);

            try_return( ntStatus);
        }

        KeQueryTickCount( &liLastRequestTime);

        while (TRUE)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNonCachedWrite Acquiring Fcb extents lock %08lX SHARED %08lX\n",
                          &pFcb->NPFcb->Specific.File.ExtentsResource,
                          PsGetCurrentThread());

            ASSERT( !ExIsResourceAcquiredLite( &pFcb->NPFcb->Specific.File.ExtentsResource ));

            AFSAcquireShared( &pFcb->NPFcb->Specific.File.ExtentsResource, TRUE );
            bLocked = TRUE;

            pStartExtent = NULL;
            pIgnoreExtent = NULL;

            if ( AFSDoExtentsMapRegion( pFcb, &StartingByte, ByteCount, &pStartExtent, &pIgnoreExtent ))
            {
                break;
            }

            KeClearEvent( &pFcb->NPFcb->Specific.File.ExtentsRequestComplete );

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNonCachedWrite Releasing(1) Fcb extents lock %08lX SHARED %08lX\n",
                          &pFcb->NPFcb->Specific.File.ExtentsResource,
                          PsGetCurrentThread());

            AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource );
            bLocked= FALSE;

            //
            // We will re-request the extents after waiting for them
            //

            KeQueryTickCount( &liCurrentTime);

            if( liCurrentTime.QuadPart - liLastRequestTime.QuadPart >= pControlDevExt->Specific.Control.ExtentRequestTimeCount.QuadPart)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSNonCachedWrite Requesting extents, again, for fid %08lX-%08lX-%08lX-%08lX Offset %I64X Length %08lX\n",
                              pFcb->ObjectInformation->FileId.Cell,
                              pFcb->ObjectInformation->FileId.Volume,
                              pFcb->ObjectInformation->FileId.Vnode,
                              pFcb->ObjectInformation->FileId.Unique,
                              StartingByte.QuadPart,
                              ByteCount);

                ntStatus = AFSRequestExtentsAsync( pFcb,
                                                   pCcb,
                                                   &StartingByte,
                                                   ByteCount);

                if (!NT_SUCCESS(ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSNonCachedWrite (%08lX) Failed to request extents Status %08lX\n",
                                  Irp,
                                  ntStatus);

                    try_return( ntStatus);
                }

                KeQueryTickCount( &liLastRequestTime);
            }


            AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNonCachedWrite Waiting for extents for fid %08lX-%08lX-%08lX-%08lX Offset %I64X Length %08lX\n",
                          pFcb->ObjectInformation->FileId.Cell,
                          pFcb->ObjectInformation->FileId.Volume,
                          pFcb->ObjectInformation->FileId.Vnode,
                          pFcb->ObjectInformation->FileId.Unique,
                          StartingByte.QuadPart,
                          ByteCount);

            //
            // Wait for it
            //

            ntStatus =  AFSWaitForExtentMapping ( pFcb, pCcb);

            if (!NT_SUCCESS(ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSNonCachedWrite Failed wait for extents for fid %08lX-%08lX-%08lX-%08lX Offset %I64X Length %08lX Status %08lX\n",
                              pFcb->ObjectInformation->FileId.Cell,
                              pFcb->ObjectInformation->FileId.Volume,
                              pFcb->ObjectInformation->FileId.Vnode,
                              pFcb->ObjectInformation->FileId.Unique,
                              StartingByte.QuadPart,
                              ByteCount,
                              ntStatus);

                try_return( ntStatus);
            }
        }

        //
        // As per the read path -
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNonCachedWrite Extents located for fid %08lX-%08lX-%08lX-%08lX Offset %I64X Length %08lX\n",
                      pFcb->ObjectInformation->FileId.Cell,
                      pFcb->ObjectInformation->FileId.Volume,
                      pFcb->ObjectInformation->FileId.Vnode,
                      pFcb->ObjectInformation->FileId.Unique,
                      StartingByte.QuadPart,
                      ByteCount);

        ntStatus = AFSGetExtents( pFcb,
                                  &StartingByte,
                                  ByteCount,
                                  pStartExtent,
                                  &extentsCount,
                                  &runCount);

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
                      "AFSNonCachedWrite (%08lX) Successfully retrieved map extents count %08lX run count %08lX\n",
                      Irp,
                      extentsCount,
                      runCount);

        if( BooleanFlagOn( AFSLibControlFlags, AFS_REDIR_LIB_FLAGS_NONPERSISTENT_CACHE))
        {

            Irp->IoStatus.Information = ByteCount;

#if GEN_MD5
            //
            // Setup the MD5 for each extent
            //

            AFSSetupMD5Hash( pFcb,
                             pStartExtent,
                             extentsCount,
                             pSystemBuffer,
                             &StartingByte,
                             ByteCount);
#endif

            ntStatus = AFSProcessExtentRun( pSystemBuffer,
                                            &StartingByte,
                                            ByteCount,
                                            pStartExtent,
                                            TRUE);

            if (!NT_SUCCESS(ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSNonCachedWrite (%08lX) Failed to process extent run for non-persistent cache Status %08lX\n",
                              Irp,
                              ntStatus);
            }

            try_return( ntStatus);
        }

        //
        // Retrieve the cache file object
        //

        pCacheFileObject = AFSReferenceCacheFileObject();

        if( pCacheFileObject == NULL)
        {

            ntStatus = STATUS_DEVICE_NOT_READY;

            AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNonCachedWrite Failed to retrieve cache fileobject for fid %08lX-%08lX-%08lX-%08lX Offset %I64X Length %08lX Status %08lX\n",
                          pFcb->ObjectInformation->FileId.Cell,
                          pFcb->ObjectInformation->FileId.Volume,
                          pFcb->ObjectInformation->FileId.Vnode,
                          pFcb->ObjectInformation->FileId.Unique,
                          StartingByte.QuadPart,
                          ByteCount,
                          ntStatus);

            try_return( ntStatus);
        }

        if (runCount > AFS_MAX_STACK_IO_RUNS)
        {

            pIoRuns = (AFSIoRun*) AFSExAllocatePoolWithTag( PagedPool,
                                                            runCount * sizeof( AFSIoRun ),
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

        RtlZeroMemory( pIoRuns, runCount * sizeof( AFSIoRun ));

        ntStatus = AFSSetupIoRun( IoGetRelatedDeviceObject( pCacheFileObject),
                                  Irp,
                                  pSystemBuffer,
                                  pIoRuns,
                                  &StartingByte,
                                  ByteCount,
                                  pStartExtent,
                                  &runCount );

        if (!NT_SUCCESS(ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNonCachedWrite (%08lX) Failed to initialize IO run block Status %08lX\n",
                          Irp,
                          ntStatus);

            try_return( ntStatus );
        }

        AFSReferenceActiveExtents( pStartExtent,
                                   extentsCount);

        bDerefExtents = TRUE;

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNonCachedWrite Releasing(2) Fcb extents lock %08lX SHARED %08lX\n",
                      &pFcb->NPFcb->Specific.File.ExtentsResource,
                      PsGetCurrentThread());

        AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource );
        bLocked = FALSE;

        pGatherIo = (AFSGatherIo*) AFSExAllocatePoolWithTag( NonPagedPool,
                                                             sizeof( AFSGatherIo),
                                                             AFS_GATHER_TAG);

        if (NULL == pGatherIo)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNonCachedWrite (%08lX) Failed to allocate IO gather block\n",
                          Irp);

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNonCachedWrite Acquiring(1) Fcb extents lock %08lX SHARED %08lX\n",
                          &pFcb->NPFcb->Specific.File.ExtentsResource,
                          PsGetCurrentThread());

            AFSAcquireShared( &pFcb->NPFcb->Specific.File.ExtentsResource,
                              TRUE);
            bLocked = TRUE;

            AFSDereferenceActiveExtents( pStartExtent,
                                         extentsCount);

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
        pGatherIo->CompleteMasterIrp = FALSE;

        bCompleteIrp = TRUE;

        if( pGatherIo->Synchronous)
        {
            KeInitializeEvent( &pGatherIo->Event, NotificationEvent, FALSE );
        }

#if GEN_MD5
        //
        // Setup the MD5 for each extent
        //

        AFSSetupMD5Hash( pFcb,
                         pStartExtent,
                         extentsCount,
                         pSystemBuffer,
                         &StartingByte,
                         ByteCount);
#endif

        //
        // Pre-emptively set up the count
        //

        Irp->IoStatus.Information = ByteCount;

        ntStatus = AFSQueueStartIos( pCacheFileObject,
                                     IRP_MJ_WRITE,
                                     IRP_WRITE_OPERATION | IRP_SYNCHRONOUS_API,
                                     pIoRuns,
                                     runCount,
                                     pGatherIo);

        AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNonCachedWrite (%08lX) AFSStartIos completed Status %08lX\n",
                      Irp,
                      ntStatus);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNonCachedWrite Acquiring(2) Fcb extents lock %08lX SHARED %08lX\n",
                          &pFcb->NPFcb->Specific.File.ExtentsResource,
                          PsGetCurrentThread());

            AFSAcquireShared( &pFcb->NPFcb->Specific.File.ExtentsResource,
                              TRUE);
            bLocked = TRUE;

            AFSDereferenceActiveExtents( pStartExtent,
                                         extentsCount);

            try_return( ntStatus);
        }

        //
        // Wait for completion of All IOs we started.
        //

        ntStatus = KeWaitForSingleObject( &pGatherIo->Event,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);

        if( NT_SUCCESS( ntStatus))
        {

            ntStatus = pGatherIo->Status;
        }

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNonCachedWrite Acquiring(3) Fcb extents lock %08lX SHARED %08lX\n",
                          &pFcb->NPFcb->Specific.File.ExtentsResource,
                          PsGetCurrentThread());

            AFSAcquireShared( &pFcb->NPFcb->Specific.File.ExtentsResource,
                              TRUE);
            bLocked = TRUE;

            AFSDereferenceActiveExtents( pStartExtent,
                                         extentsCount);

            try_return( ntStatus);
        }

try_exit:

        if( NT_SUCCESS( ntStatus) &&
            pStartExtent != NULL &&
            Irp->IoStatus.Information > 0)
        {

            //
            // Since this is dirty we can mark the extents dirty now.
            // AFSMarkDirty will dereference the extents.  Do not call
            // AFSDereferenceActiveExtents() in this code path.
            //

            AFSMarkDirty( pFcb,
                          pStartExtent,
                          extentsCount,
                          &StartingByte,
                          bDerefExtents);

            if (!bPagingIo)
            {
                //
                // This was an uncached user write - tell the server to do
                // the flush when the worker thread next wakes up
                //
                pFcb->Specific.File.LastServerFlush.QuadPart = 0;
            }
        }

        if( pCacheFileObject != NULL)
        {
            AFSReleaseCacheFileObject( pCacheFileObject);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNonCachedWrite (%08lX) Completed request Status %08lX\n",
                      Irp,
                      ntStatus);

        if (NT_SUCCESS(ntStatus) &&
            !bPagingIo &&
            bSynchronousFo)
        {

            pFileObject->CurrentByteOffset.QuadPart = StartingByte.QuadPart + ByteCount;
        }

        if( bLocked)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNonCachedWrite Releasing Fcb extents lock %08lX SHARED %08lX\n",
                          &pFcb->NPFcb->Specific.File.ExtentsResource,
                          PsGetCurrentThread());

            AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource );
        }

        if( pGatherIo)
        {
            AFSExFreePoolWithTag(pGatherIo, AFS_GATHER_TAG);
        }

        if( NULL != pIoRuns &&
            stIoRuns != pIoRuns)
        {
            AFSExFreePoolWithTag(pIoRuns, AFS_IO_RUN_TAG);
        }

        if( bCompleteIrp)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNonCachedWrite Completing Irp %08lX Status %08lX Info %08lX\n",
                          Irp,
                          ntStatus,
                          Irp->IoStatus.Information);

            AFSCompleteRequest( Irp, ntStatus);
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
    AFSCcb            *pCcb = (AFSCcb *)pFileObject->FsContext2;
    BOOLEAN            bSynchronousFo = BooleanFlagOn( pFileObject->Flags, FO_SYNCHRONOUS_IO);
    BOOLEAN            bMapped = FALSE;
    ULONG              ulCurrentIO = 0, ulTotalLen = ByteCount;
    PMDL               pCurrentMdl = Irp->MdlAddress;
    LARGE_INTEGER      liCurrentOffset;

    __Enter
    {

        Irp->IoStatus.Information = 0;

        if( BooleanFlagOn( pIrpSp->MinorFunction, IRP_MN_MDL))
        {

            __try
            {

                CcPrepareMdlWrite( pFileObject,
                                   &StartingByte,
                                   ByteCount,
                                   &Irp->MdlAddress,
                                   &Irp->IoStatus);

                ntStatus = Irp->IoStatus.Status;
            }
            __except( EXCEPTION_EXECUTE_HANDLER)
            {
                ntStatus = GetExceptionCode();

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSCachedWrite (%08lX) Exception thrown while preparing mdl write Status %08lX\n",
                              Irp,
                              ntStatus);
            }

            if( !NT_SUCCESS( ntStatus))
            {

                //
                // Free up any potentially allocated mdl's
                //

                CcMdlWriteComplete( pFileObject,
                                    &StartingByte,
                                    Irp->MdlAddress);

                Irp->MdlAddress = NULL;

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSCachedWrite (%08lX) Failed to process MDL write Status %08lX\n",
                              Irp,
                              ntStatus);
            }

            try_return( ntStatus);
        }

        liCurrentOffset.QuadPart = StartingByte.QuadPart;

        while( ulTotalLen > 0)
        {

            ntStatus = STATUS_SUCCESS;

            if( pCurrentMdl != NULL)
            {

                pSystemBuffer = MmGetSystemAddressForMdlSafe( pCurrentMdl,
                                                              NormalPagePriority);

                ulCurrentIO = MmGetMdlByteCount( pCurrentMdl);

                if( ulCurrentIO > ulTotalLen)
                {
                    ulCurrentIO = ulTotalLen;
                }
            }
            else
            {

                pSystemBuffer = AFSLockSystemBuffer( Irp,
                                                     ulTotalLen);

                ulCurrentIO = ulTotalLen;
            }

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

                if( !CcCopyWrite( pFileObject,
                                  &liCurrentOffset,
                                  ulCurrentIO,
                                  TRUE,
                                  pSystemBuffer))
                {
                    //
                    // Failed to process request.
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSCachedWrite (%08lX) Failed to issue CcCopyWrite %wZ @ %0I64X Status %08lX\n",
                                  Irp,
                                  &pFileObject->FileName,
                                  liCurrentOffset.QuadPart,
                                  Irp->IoStatus.Status);

                    try_return( ntStatus = STATUS_UNSUCCESSFUL);
                }
            }
            __except( EXCEPTION_EXECUTE_HANDLER)
            {

                ntStatus = GetExceptionCode();

                AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSCachedWrite (%08lX) CcCopyWrite Threw exception %wZ @ %0I64X Status %08lX\n",
                              Irp,
                              &pFileObject->FileName,
                              liCurrentOffset.QuadPart,
                              ntStatus);
            }

            if( !NT_SUCCESS( ntStatus))
            {
                try_return( ntStatus);
            }

            if( ForceFlush)
            {

                //
                // We have detected a file we do a write through with.
                //

                CcFlushCache(&pFcb->NPFcb->SectionObjectPointers,
                             &liCurrentOffset,
                             ulCurrentIO,
                             &iosbFlush);

                if( !NT_SUCCESS( iosbFlush.Status))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSCachedWrite (%08lX) CcFlushCache failure %wZ FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX Bytes 0x%08lX\n",
                                  Irp,
                                  &pFileObject->FileName,
                                  pFcb->ObjectInformation->FileId.Cell,
                                  pFcb->ObjectInformation->FileId.Volume,
                                  pFcb->ObjectInformation->FileId.Vnode,
                                  pFcb->ObjectInformation->FileId.Unique,
                                  iosbFlush.Status,
                                  iosbFlush.Information);

                    try_return( ntStatus = iosbFlush.Status);
                }
            }

            if( ulTotalLen <= ulCurrentIO)
            {
                break;
            }

            liCurrentOffset.QuadPart += ulCurrentIO;

            ulTotalLen -= ulCurrentIO;

            pCurrentMdl = pCurrentMdl->Next;
        }

try_exit:

        if( NT_SUCCESS( ntStatus))
        {

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

            if( !BooleanFlagOn( pFcb->Flags, AFS_FCB_FLAG_UPDATE_LAST_WRITE_TIME))
            {

                SetFlag( pFcb->Flags, AFS_FCB_FLAG_UPDATE_WRITE_TIME);

                KeQuerySystemTime( &pFcb->ObjectInformation->LastWriteTime);
            }
        }

        AFSCompleteRequest( Irp,
                            ntStatus);
    }

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
    AFSCcb       *pCcb = (AFSCcb *)FileObject->FsContext2;

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSExtendingWrite Acquiring Fcb PagingIo lock %08lX EXCL %08lX\n",
                  &Fcb->NPFcb->PagingResource,
                  PsGetCurrentThread());

    AFSAcquireExcl( &Fcb->NPFcb->PagingResource,
                    TRUE);

    if( NewLength > Fcb->Header.AllocationSize.QuadPart)
    {

        Fcb->Header.AllocationSize.QuadPart = NewLength;

        Fcb->ObjectInformation->AllocationSize = Fcb->Header.AllocationSize;
    }

    if( NewLength > Fcb->Header.FileSize.QuadPart)
    {

        Fcb->Header.FileSize.QuadPart = NewLength;

        Fcb->ObjectInformation->EndOfFile = Fcb->Header.FileSize;
    }

    //
    // Tell the server
    //

    ntStatus = AFSUpdateFileInformation( &Fcb->ObjectInformation->ParentObjectInformation->FileId,
                                         Fcb->ObjectInformation,
                                         &pCcb->AuthGroup);

    if (NT_SUCCESS(ntStatus))
    {

        KeQuerySystemTime( &Fcb->ObjectInformation->ChangeTime);

        SetFlag( Fcb->Flags, AFS_FCB_FLAG_FILE_MODIFIED | AFS_FCB_FLAG_UPDATE_CHANGE_TIME);

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

    AFSReleaseResource( &Fcb->NPFcb->PagingResource);

    //
    // DownConvert file resource to shared
    //
    ExConvertExclusiveToSharedLite( &Fcb->NPFcb->Resource);

    return ntStatus;
}

NTSTATUS
AFSShareWrite( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSPIOCtlIORequestCB stIORequestCB;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    AFSPipeIORequestCB *pIoRequest = NULL;
    void *pBuffer = NULL;
    AFSPipeIOResultCB stIoResult;
    ULONG ulBytesReturned = 0;

    __Enter
    {

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        AFSDbgLogMsg( AFS_SUBSYSTEM_PIPE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSShareWrite On pipe %wZ Length %08lX\n",
                      &pCcb->DirectoryCB->NameInformation.FileName,
                      pIrpSp->Parameters.Write.Length);

        if( pIrpSp->Parameters.Write.Length == 0)
        {

            //
            // Nothing to do in this case
            //

            try_return( ntStatus);
        }

        //
        // Retrieve the buffer for the read request
        //

        pBuffer = AFSLockSystemBuffer( Irp,
                                       pIrpSp->Parameters.Write.Length);

        if( pBuffer == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_PIPE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSShareWrite Failed to map buffer on pipe %wZ\n",
                          &pCcb->DirectoryCB->NameInformation.FileName);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSAcquireShared( &pFcb->NPFcb->Resource,
                          TRUE);

        pIoRequest = (AFSPipeIORequestCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                     sizeof( AFSPipeIORequestCB) +
                                                                                pIrpSp->Parameters.Write.Length,
                                                                     AFS_GENERIC_MEMORY_14_TAG);

        if( pIoRequest == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pIoRequest,
                       sizeof( AFSPipeIORequestCB) + pIrpSp->Parameters.Write.Length);

        pIoRequest->RequestId = pCcb->RequestID;

        pIoRequest->RootId = pFcb->ObjectInformation->VolumeCB->ObjectInformation.FileId;

        pIoRequest->BufferLength = pIrpSp->Parameters.Write.Length;

        RtlCopyMemory( (void *)((char *)pIoRequest + sizeof( AFSPipeIORequestCB)),
                       pBuffer,
                       pIrpSp->Parameters.Write.Length);

        stIoResult.BytesProcessed = 0;

        ulBytesReturned = sizeof( AFSPipeIOResultCB);

        //
        // Issue the open request to the service
        //

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_PIPE_WRITE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      &pCcb->AuthGroup,
                                      &pCcb->DirectoryCB->NameInformation.FileName,
                                      NULL,
                                      pIoRequest,
                                      sizeof( AFSPipeIORequestCB) +
                                                pIrpSp->Parameters.Write.Length,
                                      &stIoResult,
                                      &ulBytesReturned);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSShareWrite (%08lX) Failed service write Status %08lX\n",
                          Irp,
                          ntStatus);

            try_return( ntStatus);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_PIPE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSShareWrite Completed on pipe %wZ Length read %08lX\n",
                      &pCcb->DirectoryCB->NameInformation.FileName,
                      stIoResult.BytesProcessed);

        Irp->IoStatus.Information = stIoResult.BytesProcessed;

try_exit:

        if( pFcb != NULL)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }

        if( pIoRequest != NULL)
        {

            AFSExFreePoolWithTag( pIoRequest, AFS_GENERIC_MEMORY_14_TAG);
        }
    }

    return ntStatus;
}
