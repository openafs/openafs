/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011, 2012, 2013, 2015 Your File System, Inc.
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
// File: AFSGeneric.cpp
//

#include "AFSCommon.h"

//
// Function: AFSExceptionFilter
//
// Description:
//
//      This function is the exception handler
//
// Return:
//
//      A status is returned for the function
//

ULONG
AFSExceptionFilter( IN CHAR *FunctionString,
                    IN ULONG Code,
                    IN PEXCEPTION_POINTERS ExceptPtrs)
{

    UNREFERENCED_PARAMETER(Code);
    PEXCEPTION_RECORD ExceptRec;
    PCONTEXT Context;

    __try
    {

        ExceptRec = ExceptPtrs->ExceptionRecord;

        Context = ExceptPtrs->ContextRecord;

        AFSDbgTrace(( 0,
                      0,
                      "AFSExceptionFilter (Library) - EXR %p CXR %p Function %s Code %08lX Address %p Routine %p\n",
                      ExceptRec,
                      Context,
                      FunctionString,
                      ExceptRec->ExceptionCode,
                      ExceptRec->ExceptionAddress,
                      (void *)AFSExceptionFilter));

        DbgPrint("**** Exception Caught in AFS Redirector Library ****\n");

        DbgPrint("\n\nPerform the following WnDbg Cmds:\n");
        DbgPrint("\n\t.exr %p ;  .cxr %p\n\n", ExceptRec, Context);

        DbgPrint("**** Exception Complete from AFS Redirector Library ****\n");

        if( BooleanFlagOn( AFSDebugFlags, AFS_DBG_BUGCHECK_EXCEPTION))
        {

            KeBugCheck( (ULONG)-2);
        }
        else
        {

            AFSBreakPoint();
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER)
    {

        NOTHING;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

//
// Function: AFSLibExAllocatePoolWithTag()
//
// Purpose: Allocate Pool Memory.  If BugCheck Exception flag
//          is configured on, then bugcheck the system if
//          a memory allocation fails.  The routine should be
//          used for all memory allocations that are to be freed
//          when the library is unloaded.  Memory allocations that
//          are to survive library unload and reload should be
//          performed using AFSExAllocatePoolWithTag() which is
//          provided by the AFS Framework.
//
// Parameters:
//                POOL_TYPE PoolType - Paged or NonPaged
//                SIZE_T NumberOfBytes - requested allocation size
//                ULONG  Tag - Pool Allocation Tag to be applied for tracking
//
// Return:
//                void * - the memory allocation
//

void *
AFSLibExAllocatePoolWithTag( IN POOL_TYPE  PoolType,
                             IN SIZE_T  NumberOfBytes,
                             IN ULONG  Tag)
{

    void *pBuffer = NULL;

    pBuffer = ExAllocatePoolWithTag( PoolType,
                                     NumberOfBytes,
                                     Tag);

    if( pBuffer == NULL)
    {

        if( BooleanFlagOn( AFSDebugFlags, AFS_DBG_BUGCHECK_EXCEPTION))
        {

            KeBugCheck( (ULONG)-2);
        }
        else
        {

            AFSDbgTrace(( 0,
                          0,
                          "AFSLibExAllocatePoolWithTag failure Type %08lX Size %08lX Tag %08lX %08lX\n",
                          PoolType,
                          NumberOfBytes,
                          Tag,
                          PsGetCurrentThread()));

            AFSBreakPoint();
        }
    }

    return pBuffer;
}

//
// Function: AFSAcquireExcl()
//
// Purpose: Called to acquire a resource exclusive with optional wait
//
// Parameters:
//                PERESOURCE Resource - Resource to acquire
//                BOOLEAN Wait - Whether to block
//
// Return:
//                BOOLEAN - Whether the mask was acquired
//

BOOLEAN
AFSAcquireExcl( IN PERESOURCE Resource,
                IN BOOLEAN wait)
{

    BOOLEAN bStatus = FALSE;

    //
    // Normal kernel APCs must be disabled before calling
    // ExAcquireResourceExclusiveLite. Otherwise a bugcheck occurs.
    //

    KeEnterCriticalRegion();

    bStatus = ExAcquireResourceExclusiveLite( Resource,
                                              wait);

    if( !bStatus)
    {

        KeLeaveCriticalRegion();
    }

    return bStatus;
}

BOOLEAN
AFSAcquireSharedStarveExclusive( IN PERESOURCE Resource,
                                 IN BOOLEAN Wait)
{

    BOOLEAN bStatus = FALSE;

    KeEnterCriticalRegion();

    bStatus = ExAcquireSharedStarveExclusive( Resource,
                                              Wait);

    if( !bStatus)
    {

        KeLeaveCriticalRegion();
    }

    return bStatus;
}

//
// Function: AFSAcquireShared()
//
// Purpose: Called to acquire a resource shared with optional wait
//
// Parameters:
//                PERESOURCE Resource - Resource to acquire
//                BOOLEAN Wait - Whether to block
//
// Return:
//                BOOLEAN - Whether the mask was acquired
//

BOOLEAN
AFSAcquireShared( IN PERESOURCE Resource,
                  IN BOOLEAN wait)
{

    BOOLEAN bStatus = FALSE;

    KeEnterCriticalRegion();

    bStatus = ExAcquireResourceSharedLite( Resource,
                                           wait);

    if( !bStatus)
    {

        KeLeaveCriticalRegion();
    }

    return bStatus;
}

//
// Function: AFSReleaseResource()
//
// Purpose: Called to release a resource
//
// Parameters:
//                PERESOURCE Resource - Resource to release
//
// Return:
//                None
//

void
AFSReleaseResource( IN PERESOURCE Resource)
{

    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSReleaseResource Releasing lock %p Thread %08lX\n",
                  Resource,
                  PsGetCurrentThread()));

    ExReleaseResourceLite( Resource);

    KeLeaveCriticalRegion();

    return;
}

void
AFSConvertToShared( IN PERESOURCE Resource)
{

    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSConvertToShared Converting lock %p Thread %08lX\n",
                  Resource,
                  PsGetCurrentThread()));

    ExConvertExclusiveToSharedLite( Resource);

    return;
}

//
// Function: AFSCompleteRequest
//
// Description:
//
//      This function completes irps
//
// Return:
//
//      A status is returned for the function
//

void
AFSCompleteRequest( IN PIRP Irp,
                    IN ULONG Status)
{

    Irp->IoStatus.Status = Status;

    IoCompleteRequest( Irp,
                       IO_NO_INCREMENT);

    return;
}

//
// Function: AFSGenerateCRC
//
// Description:
//
//      Given a device and filename this function generates a CRC
//
// Return:
//
//      A status is returned for the function
//

ULONG
AFSGenerateCRC( IN PUNICODE_STRING FileName,
                IN BOOLEAN UpperCaseName)
{

    ULONG ulCRC = 0;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    ntStatus = RtlHashUnicodeString( FileName,
                                     UpperCaseName,
                                     HASH_STRING_ALGORITHM_DEFAULT,
                                     &ulCRC);

    if( !NT_SUCCESS( ntStatus))
    {
        ulCRC = 0;
    }

    return ulCRC;
}

void *
AFSLockSystemBuffer( IN PIRP Irp,
                     IN ULONG Length)
{

    void *pAddress = NULL;

    if( Irp->MdlAddress != NULL)
    {

        pAddress = MmGetSystemAddressForMdlSafe( Irp->MdlAddress,
                                                 NormalPagePriority);
    }
    else if( Irp->AssociatedIrp.SystemBuffer != NULL)
    {

        pAddress = Irp->AssociatedIrp.SystemBuffer;
    }
    else if( Irp->UserBuffer != NULL)
    {

        Irp->MdlAddress = IoAllocateMdl( Irp->UserBuffer,
                                         Length,
                                         FALSE,
                                         FALSE,
                                         Irp);

        if( Irp->MdlAddress != NULL)
        {

            //
            //  Lock the new Mdl in memory.
            //

            __try
            {
                PIO_STACK_LOCATION pIoStack;
                pIoStack = IoGetCurrentIrpStackLocation( Irp);


                MmProbeAndLockPages( Irp->MdlAddress, KernelMode,
                                     (pIoStack->MajorFunction == IRP_MJ_READ) ? IoWriteAccess : IoReadAccess);

                pAddress = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, NormalPagePriority );

            }
            __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
            {

                AFSDumpTraceFilesFnc();

                IoFreeMdl( Irp->MdlAddress );
                Irp->MdlAddress = NULL;
                pAddress = NULL;
            }
        }
    }

    return pAddress;
}

void *
AFSLockUserBuffer( IN void *UserBuffer,
                   IN ULONG BufferLength,
                   OUT MDL ** Mdl)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    void *pAddress = NULL;
    MDL *pMdl = NULL;

    __Enter
    {

        pMdl = IoAllocateMdl( UserBuffer,
                              BufferLength,
                              FALSE,
                              FALSE,
                              NULL);

            if( pMdl == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

        //
        //  Lock the new Mdl in memory.
        //

        __try
        {

            MmProbeAndLockPages( pMdl,
                                 KernelMode,
                                 IoWriteAccess);

            pAddress = MmGetSystemAddressForMdlSafe( pMdl,
                                                     NormalPagePriority);
        }
        __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
        {

            AFSDumpTraceFilesFnc();

            IoFreeMdl( pMdl);
            pMdl = NULL;
            pAddress = NULL;
        }

        if( pMdl != NULL)
        {

            *Mdl = pMdl;
        }

try_exit:

        NOTHING;
    }

    return pAddress;
}

void *
AFSMapToService( IN PIRP Irp,
                 IN ULONG ByteCount)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    void *pMappedBuffer = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;
    KAPC stApcState;

    __Enter
    {

        if( pDevExt->Specific.Control.ServiceProcess == NULL)
        {

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        if( Irp->MdlAddress == NULL)
        {

            if( AFSLockSystemBuffer( Irp,
                                     ByteCount) == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }
        }

        //
        // Attach to the service process for mapping
        //

        KeStackAttachProcess( pDevExt->Specific.Control.ServiceProcess,
                              (PRKAPC_STATE)&stApcState);

        pMappedBuffer = MmMapLockedPagesSpecifyCache( Irp->MdlAddress,
                                                      UserMode,
                                                      MmCached,
                                                      NULL,
                                                      FALSE,
                                                      NormalPagePriority);

        KeUnstackDetachProcess( (PRKAPC_STATE)&stApcState);

try_exit:

        NOTHING;
    }

    return pMappedBuffer;
}

NTSTATUS
AFSUnmapServiceMappedBuffer( IN void *MappedBuffer,
                             IN PMDL Mdl)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;
    KAPC stApcState;

    __Enter
    {

        if( pDevExt->Specific.Control.ServiceProcess == NULL)
        {

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        if( Mdl != NULL)
        {

            //
            // Attach to the service process for mapping
            //

            KeStackAttachProcess( pDevExt->Specific.Control.ServiceProcess,
                                  (PRKAPC_STATE)&stApcState);

            MmUnmapLockedPages( MappedBuffer,
                                Mdl);

            KeUnstackDetachProcess( (PRKAPC_STATE)&stApcState);
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSInitializeLibraryDevice()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = NULL;

    __Enter
    {

        pDeviceExt = (AFSDeviceExt *)AFSLibraryDeviceObject->DeviceExtension;

        //
        // The PIOCtl file name
        //

        RtlInitUnicodeString( &AFSPIOCtlName,
                              AFS_PIOCTL_FILE_INTERFACE_NAME);

        //
        // And the global root share name
        //

        RtlInitUnicodeString( &AFSGlobalRootName,
                              AFS_GLOBAL_ROOT_SHARE_NAME);

    }

    return ntStatus;
}

NTSTATUS
AFSRemoveLibraryDevice()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

    }

    return ntStatus;
}

NTSTATUS
AFSDefaultDispatch( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp)
{

    UNREFERENCED_PARAMETER(DeviceObject);
    NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    AFSCompleteRequest( Irp,
                        ntStatus);

    return ntStatus;
}

NTSTATUS
AFSInitializeGlobalDirectoryEntries()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB *pDirNode = NULL;
    ULONG ulEntryLength = 0;
    AFSObjectInfoCB *pObjectInfoCB = NULL;
    AFSNonPagedDirectoryCB *pNonPagedDirEntry = NULL;
    LONG lCount;

    __Enter
    {

        //
        // Initialize the global . entry
        //

        pObjectInfoCB = AFSAllocateObjectInfo( &AFSGlobalRoot->ObjectInformation,
                                               0);

        if( pObjectInfoCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeGlobalDirectory AFSAllocateObjectInfo failure %08lX\n",
                          ntStatus));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        lCount = AFSObjectInfoIncrement( pObjectInfoCB,
                                         AFS_OBJECT_REFERENCE_GLOBAL);

        AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitializeGlobalDirectoryEntries Increment count on object %p Cnt %d\n",
                      pObjectInfoCB,
                      lCount));

        ntStatus = STATUS_SUCCESS;

        ulEntryLength = sizeof( AFSDirectoryCB) +
                                     sizeof( WCHAR);

        pDirNode = (AFSDirectoryCB *)AFSLibExAllocatePoolWithTag( PagedPool,
                                                                  ulEntryLength,
                                                                  AFS_DIR_ENTRY_TAG);

        if( pDirNode == NULL)
        {

            lCount = AFSObjectInfoDecrement( pObjectInfoCB,
                                             AFS_OBJECT_REFERENCE_GLOBAL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitializeGlobalDirectoryEntries Decrement count on object %p Cnt %d\n",
                          pObjectInfoCB,
                          lCount));

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &pObjectInfoCB);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING | AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeGlobalDirectory AFS_DIR_ENTRY_TAG allocation failure\n"));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitializeGlobalDirectory AFS_DIR_ENTRY_TAG allocated %p\n",
                      pDirNode));

        pNonPagedDirEntry = (AFSNonPagedDirectoryCB *)AFSLibExAllocatePoolWithTag( NonPagedPool,
                                                                                   sizeof( AFSNonPagedDirectoryCB),
                                                                                   AFS_DIR_ENTRY_NP_TAG);

        if( pNonPagedDirEntry == NULL)
        {

	    AFSLibExFreePoolWithTag( pDirNode,
				     AFS_DIR_ENTRY_TAG);

            lCount = AFSObjectInfoDecrement( pObjectInfoCB,
                                             AFS_OBJECT_REFERENCE_GLOBAL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitializeGlobalDirectoryEntries Decrement count on object %p Cnt %d\n",
                          pObjectInfoCB,
                          lCount));

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &pObjectInfoCB);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeGlobalDirectory AFS_DIR_ENTRY_NP_TAG allocation failure\n"));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pDirNode,
                       ulEntryLength);

        RtlZeroMemory( pNonPagedDirEntry,
                       sizeof( AFSNonPagedDirectoryCB));

        ExInitializeResourceLite( &pNonPagedDirEntry->Lock);

        pDirNode->NonPaged = pNonPagedDirEntry;

        pDirNode->ObjectInformation = pObjectInfoCB;

        //
        // Set valid entry
        //

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE | AFS_DIR_ENTRY_FAKE | AFS_DIR_ENTRY_VALID);

        pDirNode->FileIndex = (ULONG)AFS_DIR_ENTRY_DOT_INDEX;

        //
        // Setup the names in the entry
        //

        pDirNode->NameInformation.FileName.Length = sizeof( WCHAR);

        pDirNode->NameInformation.FileName.MaximumLength = sizeof( WCHAR);

        pDirNode->NameInformation.FileName.Buffer = (WCHAR *)((char *)pDirNode + sizeof( AFSDirectoryCB));

        pDirNode->NameInformation.FileName.Buffer[ 0] = L'.';

        //
        // Populate the rest of the data
        //

        pObjectInfoCB->FileType = AFS_FILE_TYPE_DIRECTORY;

        pObjectInfoCB->FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

        AFSGlobalDotDirEntry = pDirNode;

        //
        // Now the .. entry
        //

        pObjectInfoCB = AFSAllocateObjectInfo( &AFSGlobalRoot->ObjectInformation,
                                               0);

        if( pObjectInfoCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeGlobalDirectory AFSAllocateObjectInfo (2) failure %08lX\n",
                          ntStatus));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        lCount = AFSObjectInfoIncrement( pObjectInfoCB,
                                         AFS_OBJECT_REFERENCE_GLOBAL);

        AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitializeGlobalDirectoryEntries Increment count on object %p Cnt %d\n",
                      pObjectInfoCB,
                      lCount));

        ntStatus = STATUS_SUCCESS;

        ulEntryLength = sizeof( AFSDirectoryCB) +
                                     ( 2 * sizeof( WCHAR));

        pDirNode = (AFSDirectoryCB *)AFSLibExAllocatePoolWithTag( PagedPool,
                                                                  ulEntryLength,
                                                                  AFS_DIR_ENTRY_TAG);

        if( pDirNode == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING | AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeGlobalDirectoryEntries AFS_DIR_ENTRY_TAG allocation failure\n"));

            lCount = AFSObjectInfoDecrement( pObjectInfoCB,
                                             AFS_OBJECT_REFERENCE_GLOBAL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitializeGlobalDirectoryEntries Decrement count on object %p Cnt %d\n",
                          pObjectInfoCB,
                          lCount));

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &pObjectInfoCB);
            }

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitializeGlobalDirectoryEntries AFS_DIR_ENTRY_TAG allocated %p\n",
                      pDirNode));

        pNonPagedDirEntry = (AFSNonPagedDirectoryCB *)AFSLibExAllocatePoolWithTag( NonPagedPool,
                                                                                   sizeof( AFSNonPagedDirectoryCB),
                                                                                   AFS_DIR_ENTRY_NP_TAG);

        if( pNonPagedDirEntry == NULL)
        {

	    AFSLibExFreePoolWithTag( pDirNode,
				     AFS_DIR_ENTRY_TAG);

            lCount = AFSObjectInfoDecrement( pObjectInfoCB,
                                             AFS_OBJECT_REFERENCE_GLOBAL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitializeGlobalDirectoryEntries Decrement count on object %p Cnt %d\n",
                          pObjectInfoCB,
                          lCount));

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &pObjectInfoCB);
            }

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pDirNode,
                       ulEntryLength);

        RtlZeroMemory( pNonPagedDirEntry,
                       sizeof( AFSNonPagedDirectoryCB));

        ExInitializeResourceLite( &pNonPagedDirEntry->Lock);

        pDirNode->NonPaged = pNonPagedDirEntry;

        pDirNode->ObjectInformation = pObjectInfoCB;

        //
        // Set valid entry
        //

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE | AFS_DIR_ENTRY_FAKE | AFS_DIR_ENTRY_VALID);

        pDirNode->FileIndex = (ULONG)AFS_DIR_ENTRY_DOT_DOT_INDEX;

        //
        // Setup the names in the entry
        //

        pDirNode->NameInformation.FileName.Length = 2 * sizeof( WCHAR);

        pDirNode->NameInformation.FileName.MaximumLength = 2 * sizeof( WCHAR);

        pDirNode->NameInformation.FileName.Buffer = (WCHAR *)((char *)pDirNode + sizeof( AFSDirectoryCB));

        pDirNode->NameInformation.FileName.Buffer[ 0] = L'.';

        pDirNode->NameInformation.FileName.Buffer[ 1] = L'.';

        //
        // Populate the rest of the data
        //

        pObjectInfoCB->FileType = AFS_FILE_TYPE_DIRECTORY;

        pObjectInfoCB->FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

        AFSGlobalDotDotDirEntry = pDirNode;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( AFSGlobalDotDirEntry != NULL)
            {

                lCount = AFSObjectInfoDecrement( AFSGlobalDotDirEntry->ObjectInformation,
                                                 AFS_OBJECT_REFERENCE_GLOBAL);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInitializeGlobalDirectoryEntries Decrement count on object %p Cnt %d\n",
                              AFSGlobalDotDirEntry->ObjectInformation,
                              lCount));

                if ( lCount == 0)
                {

                    AFSDeleteObjectInfo( &AFSGlobalDotDirEntry->ObjectInformation);
                }

                ExDeleteResourceLite( &AFSGlobalDotDirEntry->NonPaged->Lock);

		AFSLibExFreePoolWithTag( AFSGlobalDotDirEntry->NonPaged,
					 AFS_DIR_ENTRY_NP_TAG);

		AFSLibExFreePoolWithTag( AFSGlobalDotDirEntry,
					 AFS_DIR_ENTRY_TAG);

                AFSGlobalDotDirEntry = NULL;
            }

            if( AFSGlobalDotDotDirEntry != NULL)
            {

                lCount = AFSObjectInfoDecrement( AFSGlobalDotDotDirEntry->ObjectInformation,
                                                 AFS_OBJECT_REFERENCE_GLOBAL);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInitializeGlobalDirectoryEntries Decrement count on object %p Cnt %d\n",
                              AFSGlobalDotDotDirEntry->ObjectInformation,
                              lCount));

                if ( lCount == 0)
                {

                    AFSDeleteObjectInfo( &AFSGlobalDotDotDirEntry->ObjectInformation);
                }

                ExDeleteResourceLite( &AFSGlobalDotDotDirEntry->NonPaged->Lock);

		AFSLibExFreePoolWithTag( AFSGlobalDotDotDirEntry->NonPaged,
					 AFS_DIR_ENTRY_NP_TAG);

		AFSLibExFreePoolWithTag( AFSGlobalDotDotDirEntry,
					 AFS_DIR_ENTRY_TAG);

                AFSGlobalDotDotDirEntry = NULL;
            }
        }
    }

    return ntStatus;
}

AFSDirectoryCB *
AFSInitDirEntry( IN AFSObjectInfoCB *ParentObjectInfo,
                 IN PUNICODE_STRING FileName,
                 IN PUNICODE_STRING TargetName,
                 IN AFSDirEnumEntry *DirEnumEntry,
                 IN ULONG FileIndex)
{

    AFSDirectoryCB *pDirNode = NULL;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulEntryLength = 0;
    AFSObjectInfoCB *pObjectInfoCB = NULL;
    ULONGLONG ullIndex = 0;
    AFSNonPagedDirectoryCB *pNonPagedDirEntry = NULL;
    LONG lCount;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitDirEntry Initializing entry %wZ parent FID %08lX-%08lX-%08lX-%08lX\n",
                      FileName,
                      ParentObjectInfo->FileId.Cell,
                      ParentObjectInfo->FileId.Volume,
                      ParentObjectInfo->FileId.Vnode,
                      ParentObjectInfo->FileId.Unique));

        //
        // First thing is to locate/create our object information block
        // for this entry
        //

        AFSAcquireExcl( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeLock,
                        TRUE);

        ullIndex = AFSCreateLowIndex( &DirEnumEntry->FileId);

        ntStatus = AFSLocateHashEntry( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeHead,
                                       ullIndex,
                                       (AFSBTreeEntry **)&pObjectInfoCB);

        if( !NT_SUCCESS( ntStatus))
        {

            //
            // Allocate our object info cb
            //

            pObjectInfoCB = AFSAllocateObjectInfo( ParentObjectInfo,
                                                   ullIndex);

            if( pObjectInfoCB == NULL)
            {

                AFSReleaseResource( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeLock);

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitDirEntry initialized object %p Parent Object %p for %wZ\n",
                          pObjectInfoCB,
                          ParentObjectInfo,
                          FileName));

            //
            // If we allocated the object information cb then set the information
            //

            pObjectInfoCB->FileId = DirEnumEntry->FileId;

            pObjectInfoCB->TargetFileId = DirEnumEntry->TargetFileId;

            pObjectInfoCB->FileType = DirEnumEntry->FileType;

            pObjectInfoCB->FileAttributes = DirEnumEntry->FileAttributes;

            if( pObjectInfoCB->FileType == AFS_FILE_TYPE_MOUNTPOINT ||
                pObjectInfoCB->FileType == AFS_FILE_TYPE_DFSLINK)
            {

                pObjectInfoCB->FileAttributes |= (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
            }

            if (pObjectInfoCB->FileType == AFS_FILE_TYPE_SYMLINK)
            {

                if ( pObjectInfoCB->FileAttributes == FILE_ATTRIBUTE_NORMAL)
                {

                    pObjectInfoCB->FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
                }
                else
                {

                    pObjectInfoCB->FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
                }
            }

            //
            // Check for the case where we have a filetype of SymLink but both the TargetFid and the
            // TargetName are empty. In this case set the filetype to zero so we evaluate it later in
            // the code
            //

            if( pObjectInfoCB->FileType == AFS_FILE_TYPE_SYMLINK &&
                pObjectInfoCB->TargetFileId.Vnode == 0 &&
                pObjectInfoCB->TargetFileId.Unique == 0 &&
                (TargetName == NULL || TargetName->Length == 0))
            {

                //
                // This will ensure we perform a validation on the node
                //

                pObjectInfoCB->FileType = AFS_FILE_TYPE_UNKNOWN;
            }

            if( pObjectInfoCB->FileType == AFS_FILE_TYPE_UNKNOWN)
            {

                SetFlag( pObjectInfoCB->Flags, AFS_OBJECT_FLAGS_NOT_EVALUATED);
            }

            SetFlag( pObjectInfoCB->Flags, AFS_OBJECT_FLAGS_VERIFY);
        }

        if ( BooleanFlagOn( pObjectInfoCB->Flags, AFS_OBJECT_FLAGS_VERIFY))
        {

            pObjectInfoCB->CreationTime = DirEnumEntry->CreationTime;

            pObjectInfoCB->LastAccessTime = DirEnumEntry->LastAccessTime;

            pObjectInfoCB->LastWriteTime = DirEnumEntry->LastWriteTime;

            pObjectInfoCB->ChangeTime = DirEnumEntry->ChangeTime;

            pObjectInfoCB->EndOfFile = DirEnumEntry->EndOfFile;

            pObjectInfoCB->AllocationSize = DirEnumEntry->AllocationSize;

            pObjectInfoCB->EaSize = DirEnumEntry->EaSize;

            pObjectInfoCB->Links = DirEnumEntry->Links;

            pObjectInfoCB->Expiration = DirEnumEntry->Expiration;

	    pObjectInfoCB->DataVersion = DirEnumEntry->DataVersion;

	    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
			  AFS_TRACE_LEVEL_VERBOSE,
			  "AFSInitDirEntry FID %08lX-%08lX-%08lX-%08lX Clearing Verify Flag\n",
			  pObjectInfoCB->FileId.Cell,
			  pObjectInfoCB->FileId.Volume,
			  pObjectInfoCB->FileId.Vnode,
			  pObjectInfoCB->FileId.Unique));

	    ClearFlag( pObjectInfoCB->Flags, AFS_OBJECT_FLAGS_VERIFY);
	}

        //
        // This reference count is either stored into the return DirectoryCB
        // or released before function exit.
        //

        lCount = AFSObjectInfoIncrement( pObjectInfoCB,
                                         AFS_OBJECT_REFERENCE_DIRENTRY);

        AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitDirEntry Increment count on object %p Cnt %d\n",
                      pObjectInfoCB,
                      lCount));

        KeQueryTickCount( &pObjectInfoCB->LastAccessCount);

        AFSReleaseResource( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeLock);

        ntStatus = STATUS_SUCCESS;

        ulEntryLength = sizeof( AFSDirectoryCB) +
                                     FileName->Length;

        if( TargetName != NULL)
        {

            ulEntryLength += TargetName->Length;
        }

        pDirNode = (AFSDirectoryCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                               ulEntryLength,
                                                               AFS_DIR_ENTRY_TAG);

        if( pDirNode == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitDirEntry AFS_DIR_ENTRY_TAG allocated %p\n",
                      pDirNode));

        pNonPagedDirEntry = (AFSNonPagedDirectoryCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                                                sizeof( AFSNonPagedDirectoryCB),
                                                                                AFS_DIR_ENTRY_NP_TAG);

        if( pNonPagedDirEntry == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pDirNode,
                       ulEntryLength);

        RtlZeroMemory( pNonPagedDirEntry,
                       sizeof( AFSNonPagedDirectoryCB));

        ExInitializeResourceLite( &pNonPagedDirEntry->Lock);

        pDirNode->NonPaged = pNonPagedDirEntry;

        pDirNode->ObjectInformation = pObjectInfoCB;

        //
        // Set valid entry and NOT_IN_PARENT flag
        //

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_VALID | AFS_DIR_ENTRY_NOT_IN_PARENT_TREE);

        pDirNode->FileIndex = FileIndex;

        //
        // Setup the names in the entry
        //

        if( FileName->Length > 0)
        {

            pDirNode->NameInformation.FileName.Length = FileName->Length;

            pDirNode->NameInformation.FileName.MaximumLength = FileName->Length;

            pDirNode->NameInformation.FileName.Buffer = (WCHAR *)((char *)pDirNode + sizeof( AFSDirectoryCB));

            RtlCopyMemory( pDirNode->NameInformation.FileName.Buffer,
                           FileName->Buffer,
                           pDirNode->NameInformation.FileName.Length);

            //
            // Create a CRC for the file
            //

            pDirNode->CaseSensitiveTreeEntry.HashIndex = AFSGenerateCRC( &pDirNode->NameInformation.FileName,
                                                                         FALSE);

            pDirNode->CaseInsensitiveTreeEntry.HashIndex = AFSGenerateCRC( &pDirNode->NameInformation.FileName,
                                                                           TRUE);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitDirEntry Initialized DE %p for %wZ in parent FID %08lX-%08lX-%08lX-%08lX\n",
                      pDirNode,
                      FileName,
                      ParentObjectInfo->FileId.Cell,
                      ParentObjectInfo->FileId.Volume,
                      ParentObjectInfo->FileId.Vnode,
                      ParentObjectInfo->FileId.Unique));

        if( TargetName != NULL &&
            TargetName->Length > 0)
        {

            pDirNode->NameInformation.TargetName.Length = TargetName->Length;

            pDirNode->NameInformation.TargetName.MaximumLength = pDirNode->NameInformation.TargetName.Length;

            pDirNode->NameInformation.TargetName.Buffer = (WCHAR *)((char *)pDirNode +
                                                                            sizeof( AFSDirectoryCB) +
                                                                            pDirNode->NameInformation.FileName.Length);

            RtlCopyMemory( pDirNode->NameInformation.TargetName.Buffer,
                           TargetName->Buffer,
                           pDirNode->NameInformation.TargetName.Length);
        }


try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pNonPagedDirEntry != NULL)
            {

                ExDeleteResourceLite( &pNonPagedDirEntry->Lock);

                AFSExFreePoolWithTag( pNonPagedDirEntry, AFS_DIR_ENTRY_NP_TAG);
            }

            if( pDirNode != NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInitDirEntry AFS_DIR_ENTRY_TAG deallocating %p\n",
                              pDirNode));

                AFSExFreePoolWithTag( pDirNode, AFS_DIR_ENTRY_TAG);

                pDirNode = NULL;
            }

            //
            // Dereference our object info block if we have one
            //

            if( pObjectInfoCB != NULL)
            {

                AFSAcquireShared( pObjectInfoCB->VolumeCB->ObjectInfoTree.TreeLock,
                                  TRUE);

                lCount = AFSObjectInfoDecrement( pObjectInfoCB,
                                                 AFS_OBJECT_REFERENCE_DIRENTRY);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInitDirEntry Decrement count on object %p Cnt %d\n",
                              pObjectInfoCB,
                              lCount));

                AFSReleaseResource( pObjectInfoCB->VolumeCB->ObjectInfoTree.TreeLock);
            }
        }
    }

    return pDirNode;
}

BOOLEAN
AFSCheckForReadOnlyAccess( IN ACCESS_MASK DesiredAccess,
                           IN BOOLEAN DirectoryEntry)
{

    BOOLEAN bReturn = TRUE;
    ACCESS_MASK stAccessMask = 0;

    //
    // Get rid of anything we don't know about
    //

    DesiredAccess = (DesiredAccess   &
                          ( DELETE |
                            READ_CONTROL |
                            WRITE_OWNER |
                            WRITE_DAC |
                            SYNCHRONIZE |
                            ACCESS_SYSTEM_SECURITY |
                            FILE_WRITE_DATA |
                            FILE_READ_EA |
                            FILE_WRITE_EA |
                            FILE_READ_ATTRIBUTES |
                            FILE_WRITE_ATTRIBUTES |
                            FILE_LIST_DIRECTORY |
                            FILE_TRAVERSE |
                            FILE_DELETE_CHILD |
                            FILE_APPEND_DATA));

    //
    // Our 'read only' access mask. These are the accesses we will
    // allow for a read only file
    //

    stAccessMask = DELETE |
                        READ_CONTROL |
                        WRITE_OWNER |
                        WRITE_DAC |
                        SYNCHRONIZE |
                        ACCESS_SYSTEM_SECURITY |
                        FILE_READ_DATA |
                        FILE_READ_EA |
                        FILE_WRITE_EA |
                        FILE_READ_ATTRIBUTES |
                        FILE_WRITE_ATTRIBUTES |
                        FILE_EXECUTE |
                        FILE_LIST_DIRECTORY |
                        FILE_TRAVERSE;

    //
    // For a directory, add in the directory specific accesses
    //

    if( DirectoryEntry)
    {

        stAccessMask |= FILE_ADD_SUBDIRECTORY |
                                FILE_ADD_FILE |
                                FILE_DELETE_CHILD;
    }

    if( FlagOn( DesiredAccess, ~stAccessMask))
    {

        //
        // A write access is set ...
        //

        bReturn = FALSE;
    }

    return bReturn;
}

NTSTATUS
AFSEvaluateNode( IN GUID *AuthGroup,
                 IN AFSDirectoryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEnumEntry *pDirEntry = NULL;
    UNICODE_STRING uniTargetName;

    __Enter
    {

        ntStatus = AFSEvaluateTargetByID( DirEntry->ObjectInformation,
                                          AuthGroup,
                                          FALSE,
                                          &pDirEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        DirEntry->ObjectInformation->TargetFileId = pDirEntry->TargetFileId;

        DirEntry->ObjectInformation->Expiration = pDirEntry->Expiration;

        DirEntry->ObjectInformation->DataVersion = pDirEntry->DataVersion;

        DirEntry->ObjectInformation->FileType = pDirEntry->FileType;

        DirEntry->ObjectInformation->CreationTime = pDirEntry->CreationTime;

        DirEntry->ObjectInformation->LastAccessTime = pDirEntry->LastAccessTime;

        DirEntry->ObjectInformation->LastWriteTime = pDirEntry->LastWriteTime;

        DirEntry->ObjectInformation->ChangeTime = pDirEntry->ChangeTime;

        DirEntry->ObjectInformation->EndOfFile = pDirEntry->EndOfFile;

        DirEntry->ObjectInformation->AllocationSize = pDirEntry->AllocationSize;

        DirEntry->ObjectInformation->FileAttributes = pDirEntry->FileAttributes;

        if( pDirEntry->FileType == AFS_FILE_TYPE_MOUNTPOINT ||
            pDirEntry->FileType == AFS_FILE_TYPE_DFSLINK)
        {

            DirEntry->ObjectInformation->FileAttributes |= (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
        }

        if( pDirEntry->FileType == AFS_FILE_TYPE_SYMLINK)
        {

            if ( DirEntry->ObjectInformation->FileAttributes == FILE_ATTRIBUTE_NORMAL)
            {

                DirEntry->ObjectInformation->FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
            }
            else
            {

                DirEntry->ObjectInformation->FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
            }
        }

        DirEntry->ObjectInformation->EaSize = pDirEntry->EaSize;

        DirEntry->ObjectInformation->Links = pDirEntry->Links;

        //
        // If we have a target name then see if it needs updating ...
        //

        if( pDirEntry->TargetNameLength > 0)
        {

            //
            // Update the target name information if needed
            //

            uniTargetName.Length = (USHORT)pDirEntry->TargetNameLength;

            uniTargetName.MaximumLength = uniTargetName.Length;

            uniTargetName.Buffer = (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset);

            AFSAcquireExcl( &DirEntry->NonPaged->Lock,
                            TRUE);

            if( DirEntry->NameInformation.TargetName.Length == 0 ||
                RtlCompareUnicodeString( &uniTargetName,
                                         &DirEntry->NameInformation.TargetName,
                                         TRUE) != 0)
            {

                //
                // Update the target name
                //

                ntStatus = AFSUpdateTargetName( &DirEntry->NameInformation.TargetName,
                                                &DirEntry->Flags,
                                                uniTargetName.Buffer,
                                                uniTargetName.Length);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &DirEntry->NonPaged->Lock);

                    try_return( ntStatus);
                }
            }

            AFSReleaseResource( &DirEntry->NonPaged->Lock);
        }

try_exit:

        if( pDirEntry != NULL)
        {

            AFSExFreePoolWithTag( pDirEntry, AFS_GENERIC_MEMORY_2_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSValidateSymLink( IN GUID *AuthGroup,
                    IN AFSDirectoryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEnumEntry *pDirEntry = NULL;
    UNICODE_STRING uniTargetName;

    __Enter
    {

        ntStatus = AFSEvaluateTargetByID( DirEntry->ObjectInformation,
                                          AuthGroup,
                                          FALSE,
                                          &pDirEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        if( pDirEntry->FileType == AFS_FILE_TYPE_UNKNOWN ||
            pDirEntry->FileType == AFS_FILE_TYPE_INVALID)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSValidateSymLink Invalid type Status %08lX\n",
                          STATUS_OBJECT_NAME_NOT_FOUND));

            try_return( ntStatus = STATUS_OBJECT_NAME_NOT_FOUND);
        }

        DirEntry->ObjectInformation->TargetFileId = pDirEntry->TargetFileId;

        DirEntry->ObjectInformation->Expiration = pDirEntry->Expiration;

        DirEntry->ObjectInformation->DataVersion = pDirEntry->DataVersion;

        //
        // Update the target name information if needed
        //

        uniTargetName.Length = (USHORT)pDirEntry->TargetNameLength;

        uniTargetName.MaximumLength = uniTargetName.Length;

        uniTargetName.Buffer = (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset);

        if( uniTargetName.Length > 0)
        {

            AFSAcquireExcl( &DirEntry->NonPaged->Lock,
                            TRUE);

            if( DirEntry->NameInformation.TargetName.Length == 0 ||
                RtlCompareUnicodeString( &uniTargetName,
                                         &DirEntry->NameInformation.TargetName,
                                         TRUE) != 0)
            {

                //
                // Update the target name
                //

                ntStatus = AFSUpdateTargetName( &DirEntry->NameInformation.TargetName,
                                                &DirEntry->Flags,
                                                uniTargetName.Buffer,
                                                uniTargetName.Length);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &DirEntry->NonPaged->Lock);

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }
            }

            AFSReleaseResource( &DirEntry->NonPaged->Lock);
        }

        //
        // If the FileType is the same then nothing to do since it IS
        // a SymLink
        //

        if( pDirEntry->FileType == DirEntry->ObjectInformation->FileType)
        {

            ASSERT( pDirEntry->FileType == AFS_FILE_TYPE_SYMLINK);

            try_return( ntStatus = STATUS_SUCCESS);
        }

        DirEntry->ObjectInformation->FileType = pDirEntry->FileType;

        DirEntry->ObjectInformation->CreationTime = pDirEntry->CreationTime;

        DirEntry->ObjectInformation->LastAccessTime = pDirEntry->LastAccessTime;

        DirEntry->ObjectInformation->LastWriteTime = pDirEntry->LastWriteTime;

        DirEntry->ObjectInformation->ChangeTime = pDirEntry->ChangeTime;

        DirEntry->ObjectInformation->EndOfFile = pDirEntry->EndOfFile;

        DirEntry->ObjectInformation->AllocationSize = pDirEntry->AllocationSize;

        DirEntry->ObjectInformation->FileAttributes = pDirEntry->FileAttributes;

        if( pDirEntry->FileType == AFS_FILE_TYPE_MOUNTPOINT ||
            pDirEntry->FileType == AFS_FILE_TYPE_DFSLINK)
        {

            DirEntry->ObjectInformation->FileAttributes |= (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
        }

        if( pDirEntry->FileType == AFS_FILE_TYPE_SYMLINK)
        {

            if ( DirEntry->ObjectInformation->FileAttributes == FILE_ATTRIBUTE_NORMAL)
            {

                DirEntry->ObjectInformation->FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
            }
            else
            {

                DirEntry->ObjectInformation->FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
            }
        }

        DirEntry->ObjectInformation->EaSize = pDirEntry->EaSize;

        DirEntry->ObjectInformation->Links = pDirEntry->Links;

try_exit:

        if( pDirEntry != NULL)
        {

            AFSExFreePoolWithTag( pDirEntry, AFS_GENERIC_MEMORY_2_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSInvalidateObject( IN OUT AFSObjectInfoCB **ppObjectInfo,
                     IN     ULONG Reason)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    IO_STATUS_BLOCK stIoStatus;
    ULONG ulFilter = 0;
    AFSObjectInfoCB * pParentObjectInfo = NULL;

    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInvalidateObject Invalidation on node type %d for fid %08lX-%08lX-%08lX-%08lX Reason %d\n",
                  (*ppObjectInfo)->FileType,
                  (*ppObjectInfo)->FileId.Cell,
                  (*ppObjectInfo)->FileId.Volume,
                  (*ppObjectInfo)->FileId.Vnode,
                  (*ppObjectInfo)->FileId.Unique,
                  Reason));

    if ( BooleanFlagOn( (*ppObjectInfo)->Flags, AFS_OBJECT_FLAGS_PARENT_FID))
    {

        pParentObjectInfo = AFSFindObjectInfo( (*ppObjectInfo)->VolumeCB,
                                               &(*ppObjectInfo)->ParentFileId,
                                               FALSE);
    }

    if( (*ppObjectInfo)->FileType == AFS_FILE_TYPE_SYMLINK ||
        (*ppObjectInfo)->FileType == AFS_FILE_TYPE_DFSLINK ||
        (*ppObjectInfo)->FileType == AFS_FILE_TYPE_MOUNTPOINT)
    {
        //
        // We only act on the mount point itself, not the target. If the
        // node has been deleted then mark it as such otherwise indicate
        // it requires verification
        //

        if( Reason == AFS_INVALIDATE_DELETED)
        {
            SetFlag( (*ppObjectInfo)->Flags, AFS_OBJECT_FLAGS_OBJECT_INVALID);
        }
        else
        {

            if( Reason == AFS_INVALIDATE_FLUSHED)
            {

                (*ppObjectInfo)->DataVersion.QuadPart = (ULONGLONG)-1;
            }

            (*ppObjectInfo)->Expiration.QuadPart = 0;

            (*ppObjectInfo)->TargetFileId.Vnode = 0;

            (*ppObjectInfo)->TargetFileId.Unique = 0;

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateObject Setting VERIFY flag on fid %08lX-%08lX-%08lX-%08lX\n",
                          (*ppObjectInfo)->FileId.Cell,
                          (*ppObjectInfo)->FileId.Volume,
                          (*ppObjectInfo)->FileId.Vnode,
                          (*ppObjectInfo)->FileId.Unique));

            SetFlag( (*ppObjectInfo)->Flags, AFS_OBJECT_FLAGS_VERIFY);
        }

        if ( pParentObjectInfo != NULL)
        {

            ulFilter = FILE_NOTIFY_CHANGE_FILE_NAME;

            if( Reason == AFS_INVALIDATE_CREDS)
            {
                ulFilter |= FILE_NOTIFY_CHANGE_SECURITY;
            }

            if( Reason == AFS_INVALIDATE_DATA_VERSION ||
                Reason == AFS_INVALIDATE_FLUSHED)
            {
                ulFilter |= FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE;
            }
            else
            {
                ulFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
            }

            AFSFsRtlNotifyFullReportChange( pParentObjectInfo,
                                            NULL,
                                            ulFilter,
                                            FILE_ACTION_MODIFIED);
        }

        try_return( ntStatus);
    }

    //
    // Depending on the reason for invalidation then perform work on the node
    //

    switch( Reason)
    {

    case AFS_INVALIDATE_DELETED:
        {

            //
            // Mark this node as invalid
            //

            (*ppObjectInfo)->Links = 0;

            SetFlag( (*ppObjectInfo)->Flags, AFS_OBJECT_FLAGS_DELETED);

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateObject Set DELETE flag on fid %08lX-%08lX-%08lX-%08lX\n",
                          (*ppObjectInfo)->FileId.Cell,
                          (*ppObjectInfo)->FileId.Volume,
                          (*ppObjectInfo)->FileId.Vnode,
                          (*ppObjectInfo)->FileId.Unique));

            if( pParentObjectInfo != NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInvalidateObject Set VERIFY flag on parent fid %08lX-%08lX-%08lX-%08lX\n",
                              pParentObjectInfo->FileId.Cell,
                              pParentObjectInfo->FileId.Volume,
                              pParentObjectInfo->FileId.Vnode,
                              pParentObjectInfo->FileId.Unique));

                SetFlag( pParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

                pParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;

                pParentObjectInfo->Expiration.QuadPart = 0;

                if( (*ppObjectInfo)->FileType == AFS_FILE_TYPE_DIRECTORY)
                {
                    ulFilter = FILE_NOTIFY_CHANGE_DIR_NAME;
                }
                else
                {
                    ulFilter = FILE_NOTIFY_CHANGE_FILE_NAME;
                }

                AFSFsRtlNotifyFullReportChange( pParentObjectInfo,
                                                NULL,
                                                ulFilter,
                                                FILE_ACTION_REMOVED);
            }

            if( NT_SUCCESS( AFSQueueInvalidateObject( (*ppObjectInfo),
                                                      Reason)))
            {
                (*ppObjectInfo) = NULL; // We'll dec the count in the worker item
            }

            break;
        }

    case AFS_INVALIDATE_FLUSHED:
        {

            if( (*ppObjectInfo)->FileType == AFS_FILE_TYPE_FILE &&
                (*ppObjectInfo)->Fcb != NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInvalidateObject Flush/purge file fid %08lX-%08lX-%08lX-%08lX\n",
                              (*ppObjectInfo)->FileId.Cell,
                              (*ppObjectInfo)->FileId.Volume,
                              (*ppObjectInfo)->FileId.Vnode,
                              (*ppObjectInfo)->FileId.Unique));

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
			      AFS_TRACE_LEVEL_VERBOSE,
			      "AFSInvalidateObject Flush/purge Acquiring Fcb lock %p EXCL %08lX\n",
			      &(*ppObjectInfo)->Fcb->NPFcb->Resource,
			      PsGetCurrentThread()));

		AFSAcquireExcl( &(*ppObjectInfo)->Fcb->NPFcb->Resource,
				TRUE);

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
			      AFS_TRACE_LEVEL_VERBOSE,
			      "AFSInvalidateObject Flush/purge Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
			      &(*ppObjectInfo)->Fcb->NPFcb->SectionObjectResource,
			      PsGetCurrentThread()));

                AFSAcquireExcl( &(*ppObjectInfo)->Fcb->NPFcb->SectionObjectResource,
                                TRUE);

                __try
                {

                    CcFlushCache( &(*ppObjectInfo)->Fcb->NPFcb->SectionObjectPointers,
                                  NULL,
                                  0,
                                  &stIoStatus);

                    if( !NT_SUCCESS( stIoStatus.Status))
                    {

                        AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSInvalidateObject CcFlushCache failure FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX Bytes 0x%08lX\n",
                                      (*ppObjectInfo)->FileId.Cell,
                                      (*ppObjectInfo)->FileId.Volume,
                                      (*ppObjectInfo)->FileId.Vnode,
                                      (*ppObjectInfo)->FileId.Unique,
                                      stIoStatus.Status,
                                      stIoStatus.Information));

                        ntStatus = stIoStatus.Status;
                    }


                    if ( (*ppObjectInfo)->Fcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL)
                    {

                        if ( !CcPurgeCacheSection( &(*ppObjectInfo)->Fcb->NPFcb->SectionObjectPointers,
                                                   NULL,
                                                   0,
                                                   FALSE))
                        {

                            AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                          AFS_TRACE_LEVEL_WARNING,
                                          "AFSInvalidateObject CcPurgeCacheSection failure FID %08lX-%08lX-%08lX-%08lX\n",
                                          (*ppObjectInfo)->FileId.Cell,
                                          (*ppObjectInfo)->FileId.Volume,
                                          (*ppObjectInfo)->FileId.Vnode,
                                          (*ppObjectInfo)->FileId.Unique));

                            SetFlag( (*ppObjectInfo)->Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
                        }
                    }
                }
		__except( EXCEPTION_EXECUTE_HANDLER)
                {

                    ntStatus = GetExceptionCode();

                    AFSDbgTrace(( 0,
                                  0,
                                  "EXCEPTION - AFSInvalidateObject Cc FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
                                  (*ppObjectInfo)->FileId.Cell,
                                  (*ppObjectInfo)->FileId.Volume,
                                  (*ppObjectInfo)->FileId.Vnode,
                                  (*ppObjectInfo)->FileId.Unique,
                                  ntStatus));

                    SetFlag( (*ppObjectInfo)->Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
                }

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
			      AFS_TRACE_LEVEL_VERBOSE,
			      "AFSInvalidateObject Flush/purge Releasing Fcb SectionObject lock %p EXCL %08lX\n",
			      &(*ppObjectInfo)->Fcb->NPFcb->SectionObjectResource,
			      PsGetCurrentThread()));

                AFSReleaseResource( &(*ppObjectInfo)->Fcb->NPFcb->SectionObjectResource);

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
			      AFS_TRACE_LEVEL_VERBOSE,
			      "AFSInvalidateObject Flush/purge Releasing Fcb lock %p EXCL %08lX\n",
			      &(*ppObjectInfo)->Fcb->NPFcb->Resource,
			      PsGetCurrentThread()));

		AFSReleaseResource( &(*ppObjectInfo)->Fcb->NPFcb->Resource);

		if( !BooleanFlagOn( pRDRDevExt->DeviceFlags, AFS_DEVICE_FLAG_DIRECT_SERVICE_IO))
		{

		    //
		    // Clear out the extents
		    // Get rid of them (note this involves waiting
		    // for any writes or reads to the cache to complete)
		    //

		    AFSTearDownFcbExtents( (*ppObjectInfo)->Fcb,
					   NULL);
		}
            }

            (*ppObjectInfo)->DataVersion.QuadPart = (ULONGLONG)-1;


            if( (*ppObjectInfo)->FileType == AFS_FILE_TYPE_FILE)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInvalidateObject Setting VERIFY_DATA flag on fid %08lX-%08lX-%08lX-%08lX\n",
                              (*ppObjectInfo)->FileId.Cell,
                              (*ppObjectInfo)->FileId.Volume,
                              (*ppObjectInfo)->FileId.Vnode,
                              (*ppObjectInfo)->FileId.Unique));

                SetFlag( (*ppObjectInfo)->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA);
            }

            // Fall through to the default processing
        }

    default:
        {

            if( (*ppObjectInfo)->FileType == AFS_FILE_TYPE_DIRECTORY)
            {
                ulFilter = FILE_NOTIFY_CHANGE_DIR_NAME;
            }
            else
            {
                ulFilter = FILE_NOTIFY_CHANGE_FILE_NAME;
            }

            if( Reason == AFS_INVALIDATE_CREDS)
            {
                ulFilter |= FILE_NOTIFY_CHANGE_SECURITY;
            }

            if( Reason == AFS_INVALIDATE_DATA_VERSION)
            {
                ulFilter |= FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE;
            }
            else
            {
                ulFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;
            }

            if( (*ppObjectInfo)->FileType == AFS_FILE_TYPE_DIRECTORY)
            {

                AFSFsRtlNotifyFullReportChange( (*ppObjectInfo),
                                                NULL,
                                                ulFilter,
                                                FILE_ACTION_MODIFIED);
            }
            else if ( pParentObjectInfo != NULL)
            {

                AFSFsRtlNotifyFullReportChange( pParentObjectInfo,
                                                NULL,
                                                ulFilter,
                                                FILE_ACTION_MODIFIED);
            }

            //
            // Indicate this node requires re-evaluation for the remaining reasons
            //

            (*ppObjectInfo)->Expiration.QuadPart = 0;

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateObject Setting VERIFY flag on fid %08lX-%08lX-%08lX-%08lX\n",
                          (*ppObjectInfo)->FileId.Cell,
                          (*ppObjectInfo)->FileId.Volume,
                          (*ppObjectInfo)->FileId.Vnode,
                          (*ppObjectInfo)->FileId.Unique));

            SetFlag( (*ppObjectInfo)->Flags, AFS_OBJECT_FLAGS_VERIFY);

            if( Reason == AFS_INVALIDATE_DATA_VERSION ||
                (*ppObjectInfo)->FileType == AFS_FILE_TYPE_FILE &&
                ( Reason == AFS_INVALIDATE_CALLBACK ||
                  Reason == AFS_INVALIDATE_EXPIRED))
            {
                if ( NT_SUCCESS( AFSQueueInvalidateObject( (*ppObjectInfo),
                                                           AFS_INVALIDATE_DATA_VERSION)))
                {

                    (*ppObjectInfo) = NULL; // We'll dec the count in the worker item
                }
            }

            break;
        }
    }

  try_exit:

    if ( pParentObjectInfo != NULL)
    {

        AFSReleaseObjectInfo( &pParentObjectInfo);
    }

    return ntStatus;
}

NTSTATUS
AFSInvalidateCache( IN AFSInvalidateCacheCB *InvalidateCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSVolumeCB *pVolumeCB = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    ULONGLONG   ullIndex = 0;
    AFSObjectInfoCB *pObjectInfo = NULL;
    LONG lCount;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInvalidateCache Invalidation FID %08lX-%08lX-%08lX-%08lX Type %d WholeVolume %d Reason %d\n",
                      InvalidateCB->FileID.Cell,
                      InvalidateCB->FileID.Volume,
                      InvalidateCB->FileID.Vnode,
                      InvalidateCB->FileID.Unique,
                      InvalidateCB->FileType,
                      InvalidateCB->WholeVolume,
                      InvalidateCB->Reason));

        //
        // Need to locate the Fcb for the directory to purge
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInvalidateCache Acquiring RDR VolumeTreeLock lock %p SHARED %08lX\n",
                      &pDevExt->Specific.RDR.VolumeTreeLock,
                      PsGetCurrentThread()));

        //
        // Starve any exclusive waiters on this paticular call
        //

        AFSAcquireSharedStarveExclusive( &pDevExt->Specific.RDR.VolumeTreeLock, TRUE);

        //
        // Locate the volume node
        //

        ullIndex = AFSCreateHighIndex( &InvalidateCB->FileID);

        ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                       ullIndex,
                                       (AFSBTreeEntry **)&pVolumeCB);

        if( pVolumeCB != NULL)
        {

            lCount = AFSVolumeIncrement( pVolumeCB,
                                         AFS_VOLUME_REFERENCE_INVALIDATE);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateCache Increment count on volume %p Cnt %d\n",
                          pVolumeCB,
                          lCount));
        }

        AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

        if( !NT_SUCCESS( ntStatus) ||
            pVolumeCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSInvalidateCache Invalidation FAILURE Unable to locate volume node FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          InvalidateCB->FileID.Cell,
                          InvalidateCB->FileID.Volume,
                          InvalidateCB->FileID.Vnode,
                          InvalidateCB->FileID.Unique,
                          ntStatus));

            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // If this is a whole volume invalidation then go do it now
        //

        if( InvalidateCB->WholeVolume)
        {

            ntStatus = AFSInvalidateVolume( pVolumeCB,
                                            InvalidateCB->Reason);

            try_return( ntStatus);
        }

        AFSAcquireExcl( pVolumeCB->ObjectInfoTree.TreeLock,
                          TRUE);

        if ( AFSIsVolumeFID( &InvalidateCB->FileID))
        {

            pObjectInfo = &pVolumeCB->ObjectInformation;
        }
        else
        {

            ullIndex = AFSCreateLowIndex( &InvalidateCB->FileID);

            ntStatus = AFSLocateHashEntry( pVolumeCB->ObjectInfoTree.TreeHead,
                                           ullIndex,
                                           (AFSBTreeEntry **)&pObjectInfo);
        }

        if( pObjectInfo != NULL)
        {

            //
            // Reference the node so it won't be torn down
            //

            lCount = AFSObjectInfoIncrement( pObjectInfo,
                                             AFS_OBJECT_REFERENCE_INVALIDATION);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateCache Increment count on object %p Cnt %d\n",
                          pObjectInfo,
                          lCount));
        }

        AFSReleaseResource( pVolumeCB->ObjectInfoTree.TreeLock);

        if( !NT_SUCCESS( ntStatus) ||
            pObjectInfo == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateCache Invalidation FAILURE Unable to locate object FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          InvalidateCB->FileID.Cell,
                          InvalidateCB->FileID.Volume,
                          InvalidateCB->FileID.Vnode,
                          InvalidateCB->FileID.Unique,
                          ntStatus));

            try_return( ntStatus = STATUS_SUCCESS);
        }

        AFSInvalidateObject( &pObjectInfo,
                             InvalidateCB->Reason);

try_exit:

        if( pObjectInfo != NULL)
        {

            lCount = AFSObjectInfoDecrement( pObjectInfo,
                                             AFS_OBJECT_REFERENCE_INVALIDATION);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateCache Decrement count on object %p Cnt %d\n",
                          pObjectInfo,
                          lCount));
        }

        if ( pVolumeCB != NULL)
        {

            lCount = AFSVolumeDecrement( pVolumeCB,
                                         AFS_VOLUME_REFERENCE_INVALIDATE);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateCache Decrement count on volume %p Cnt %d\n",
                          pVolumeCB,
                          lCount));
        }
    }

    return ntStatus;
}

BOOLEAN
AFSIsChildOfParent( IN AFSFcb *Dcb,
                    IN AFSFcb *Fcb)
{

    BOOLEAN bIsChild = FALSE;
    AFSFcb *pCurrentFcb = Fcb;
    AFSObjectInfoCB * pParentObjectInfo = NULL;

    while( pCurrentFcb != NULL)
    {

        if( BooleanFlagOn( pCurrentFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_PARENT_FID) &&
            AFSIsEqualFID( &pCurrentFcb->ObjectInformation->ParentFileId, &Dcb->ObjectInformation->FileId))
        {

            bIsChild = TRUE;

            break;
        }

        pParentObjectInfo = AFSFindObjectInfo( pCurrentFcb->ObjectInformation->VolumeCB,
                                               &pCurrentFcb->ObjectInformation->ParentFileId,
                                               FALSE);

        if ( pParentObjectInfo != NULL)
        {

            pCurrentFcb = pParentObjectInfo->Fcb;

            AFSReleaseObjectInfo( &pParentObjectInfo);
        }
        else
        {

            pCurrentFcb = NULL;
        }
    }

    return bIsChild;
}

inline
ULONGLONG
AFSCreateHighIndex( IN AFSFileID *FileID)
{

    ULONGLONG ullIndex = 0;

    ullIndex = (((ULONGLONG)FileID->Cell << 32) | FileID->Volume);

    return ullIndex;
}

inline
ULONGLONG
AFSCreateLowIndex( IN AFSFileID *FileID)
{

    ULONGLONG ullIndex = 0;

    ullIndex = (((ULONGLONG)FileID->Vnode << 32) | FileID->Unique);

    return ullIndex;
}

BOOLEAN
AFSCheckAccess( IN ACCESS_MASK DesiredAccess,
                IN ACCESS_MASK GrantedAccess,
                IN BOOLEAN DirectoryEntry)
{

    BOOLEAN bAccessGranted = TRUE;

    //
    // Check if we are asking for read/write and granted only read only
    // NOTE: There will be more checks here
    //

    if( !AFSCheckForReadOnlyAccess( DesiredAccess,
                                    DirectoryEntry) &&
        AFSCheckForReadOnlyAccess( GrantedAccess,
                                   DirectoryEntry))
    {

        bAccessGranted = FALSE;
    }

    return bAccessGranted;
}

NTSTATUS
AFSGetDriverStatus( IN AFSDriverStatusRespCB *DriverStatus)
{

    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSDeviceExt    *pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

    //
    // Start with read
    //

    DriverStatus->Status = AFS_DRIVER_STATUS_READY;

    if( AFSGlobalRoot == NULL)
    {

        //
        // We are not ready
        //

        DriverStatus->Status = AFS_DRIVER_STATUS_NOT_READY;
    }

    if( pControlDevExt->Specific.Control.CommServiceCB.IrpPoolControlFlag != POOL_ACTIVE)
    {

        //
        // No service yet
        //

        DriverStatus->Status = AFS_DRIVER_STATUS_NO_SERVICE;
    }

    return ntStatus;
}

NTSTATUS
AFSSubstituteSysName( IN UNICODE_STRING *ComponentName,
                      IN UNICODE_STRING *SubstituteName,
                      IN ULONG StringIndex)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt    *pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;
    AFSSysNameCB    *pSysName = NULL;
    ERESOURCE       *pSysNameLock = NULL;
    ULONG            ulIndex = 1;
    USHORT           usIndex = 0;
    UNICODE_STRING   uniSysName;

    __Enter
    {

#if defined(_WIN64)

        if( IoIs32bitProcess( NULL))
        {

            pSysNameLock = &pControlDevExt->Specific.Control.SysName32ListLock;

            pSysName = pControlDevExt->Specific.Control.SysName32ListHead;
        }
        else
        {

            pSysNameLock = &pControlDevExt->Specific.Control.SysName64ListLock;

            pSysName = pControlDevExt->Specific.Control.SysName64ListHead;
        }
#else

        pSysNameLock = &pControlDevExt->Specific.Control.SysName32ListLock;

        pSysName = pControlDevExt->Specific.Control.SysName32ListHead;

#endif

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSubstituteSysName Acquiring SysName lock %p SHARED %08lX\n",
                      pSysNameLock,
                      PsGetCurrentThread()));

        AFSAcquireShared( pSysNameLock,
                          TRUE);

        //
        // Find where we are in the list
        //

        while( pSysName != NULL &&
            ulIndex < StringIndex)
        {

            pSysName = pSysName->fLink;

            ulIndex++;
        }

        if( pSysName == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSSubstituteSysName No sysname %wZ Status %08lX\n",
			  ComponentName,
                          STATUS_OBJECT_NAME_NOT_FOUND));

            try_return( ntStatus = STATUS_OBJECT_NAME_NOT_FOUND);
        }

        RtlInitUnicodeString( &uniSysName,
                              L"@SYS");
        //
        // If it is a full component of @SYS then just substitue the
        // name in
        //

        if( RtlCompareUnicodeString( &uniSysName,
                                     ComponentName,
                                     TRUE) == 0)
        {

            SubstituteName->Length = pSysName->SysName.Length;
            SubstituteName->MaximumLength = SubstituteName->Length;

            SubstituteName->Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                        SubstituteName->Length,
                                                                        AFS_SUBST_BUFFER_TAG);

            if( SubstituteName->Buffer == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlCopyMemory( SubstituteName->Buffer,
                           pSysName->SysName.Buffer,
                           pSysName->SysName.Length);
        }
        else
        {

            usIndex = 0;

            while( ComponentName->Buffer[ usIndex] != L'@')
            {

                usIndex++;
            }

            SubstituteName->Length = (usIndex * sizeof( WCHAR)) + pSysName->SysName.Length;
            SubstituteName->MaximumLength = SubstituteName->Length;

            SubstituteName->Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                        SubstituteName->Length,
                                                                        AFS_SUBST_BUFFER_TAG);

            if( SubstituteName->Buffer == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlCopyMemory( SubstituteName->Buffer,
                           ComponentName->Buffer,
                           usIndex * sizeof( WCHAR));

            RtlCopyMemory( &SubstituteName->Buffer[ usIndex],
                           pSysName->SysName.Buffer,
                           pSysName->SysName.Length);
        }

try_exit:

        AFSReleaseResource( pSysNameLock);
    }

    return ntStatus;
}

NTSTATUS
AFSSubstituteNameInPath( IN OUT UNICODE_STRING *FullPathName,
                         IN OUT UNICODE_STRING *ComponentName,
                         IN UNICODE_STRING *SubstituteName,
                         IN OUT UNICODE_STRING *RemainingPath,
                         IN BOOLEAN FreePathName)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniPathName;
    USHORT usPrefixNameLen = 0;
    SHORT  sNameLenDelta = 0;

    __Enter
    {

        //
        // If the passed in name can handle the additional length
        // then just moves things around
        //

        sNameLenDelta = SubstituteName->Length - ComponentName->Length;

        usPrefixNameLen = (USHORT)(ComponentName->Buffer - FullPathName->Buffer);

        if( FullPathName->MaximumLength > FullPathName->Length + sNameLenDelta)
        {

            if( FullPathName->Length > usPrefixNameLen + ComponentName->Length)
            {

                RtlMoveMemory( &FullPathName->Buffer[ ((usPrefixNameLen*sizeof( WCHAR) + SubstituteName->Length)/sizeof( WCHAR))],
                               &FullPathName->Buffer[ ((usPrefixNameLen*sizeof( WCHAR) + ComponentName->Length)/sizeof( WCHAR))],
                               FullPathName->Length - usPrefixNameLen*sizeof( WCHAR) - ComponentName->Length);
            }

            RtlCopyMemory( &FullPathName->Buffer[ usPrefixNameLen],
                           SubstituteName->Buffer,
                           SubstituteName->Length);

            FullPathName->Length += sNameLenDelta;

            ComponentName->Length += sNameLenDelta;

            ComponentName->MaximumLength = ComponentName->Length;

            if ( RemainingPath->Buffer)
            {

                RemainingPath->Buffer += sNameLenDelta/sizeof( WCHAR);
            }

            try_return( ntStatus);
        }

        //
        // Need to re-allocate the buffer
        //

        uniPathName.Length = FullPathName->Length -
                                         ComponentName->Length +
                                         SubstituteName->Length;

        uniPathName.MaximumLength = FullPathName->MaximumLength + PAGE_SIZE;

        uniPathName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                uniPathName.MaximumLength,
                                                                AFS_NAME_BUFFER_FOUR_TAG);

        if( uniPathName.Buffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        usPrefixNameLen = (USHORT)(ComponentName->Buffer - FullPathName->Buffer);

        usPrefixNameLen *= sizeof( WCHAR);

        RtlZeroMemory( uniPathName.Buffer,
                       uniPathName.MaximumLength);

        RtlCopyMemory( uniPathName.Buffer,
                       FullPathName->Buffer,
                       usPrefixNameLen);

        RtlCopyMemory( &uniPathName.Buffer[ (usPrefixNameLen/sizeof( WCHAR))],
                       SubstituteName->Buffer,
                       SubstituteName->Length);

        if( FullPathName->Length > usPrefixNameLen + ComponentName->Length)
        {

            RtlCopyMemory( &uniPathName.Buffer[ (usPrefixNameLen + SubstituteName->Length)/sizeof( WCHAR)],
                           &FullPathName->Buffer[ (usPrefixNameLen + ComponentName->Length)/sizeof( WCHAR)],
                           FullPathName->Length - usPrefixNameLen - ComponentName->Length);
        }

        ComponentName->Buffer = uniPathName.Buffer + (ComponentName->Buffer - FullPathName->Buffer);

        ComponentName->Length += sNameLenDelta;

        ComponentName->MaximumLength = ComponentName->Length;

        if ( RemainingPath->Buffer)
        {

            RemainingPath->Buffer = uniPathName.Buffer
                + (RemainingPath->Buffer - FullPathName->Buffer)
                + sNameLenDelta/sizeof( WCHAR);
        }

        if( FreePathName)
        {
            AFSExFreePoolWithTag( FullPathName->Buffer, 0);
        }

        *FullPathName = uniPathName;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSInvalidateVolume( IN AFSVolumeCB *VolumeCB,
                     IN ULONG Reason)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSObjectInfoCB *pCurrentObject = NULL;
    AFSObjectInfoCB *pNextObject = NULL;
    LONG lCount;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInvalidateVolume Invalidate volume fid %08lX-%08lX-%08lX-%08lX Reason %08lX\n",
                      VolumeCB->ObjectInformation.FileId.Cell,
                      VolumeCB->ObjectInformation.FileId.Volume,
                      VolumeCB->ObjectInformation.FileId.Vnode,
                      VolumeCB->ObjectInformation.FileId.Unique,
                      Reason));

        //
        // Depending on the reason for invalidation then perform work on the node
        //

        switch( Reason)
        {

            case AFS_INVALIDATE_DELETED:
            {

                //
                // Mark this volume as invalid
                //

                SetFlag( VolumeCB->ObjectInformation.Flags, AFS_OBJECT_FLAGS_OBJECT_INVALID);

                SetFlag( VolumeCB->Flags, AFS_VOLUME_FLAGS_OFFLINE);

                break;
            }
        }

        AFSAcquireShared( VolumeCB->ObjectInfoTree.TreeLock,
                          TRUE);

        //
        // Invalidate the volume root directory
        //

        pCurrentObject = &VolumeCB->ObjectInformation;

        if ( pCurrentObject )
        {

            lCount = AFSObjectInfoIncrement( pCurrentObject,
                                             AFS_OBJECT_REFERENCE_INVALIDATION);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateVolumeObjects Increment count on object %p Cnt %d\n",
                          pCurrentObject,
                          lCount));

            AFSReleaseResource( VolumeCB->ObjectInfoTree.TreeLock);

            AFSInvalidateObject( &pCurrentObject,
                                 Reason);

            AFSAcquireShared( VolumeCB->ObjectInfoTree.TreeLock,
                              TRUE);

            if ( pCurrentObject)
            {

                lCount = AFSObjectInfoDecrement( pCurrentObject,
                                                 AFS_OBJECT_REFERENCE_INVALIDATION);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInvalidateVolumeObjects Decrement count on object %p Cnt %d\n",
                              pCurrentObject,
                              lCount));
            }
        }

        //
        // Apply invalidation to all other volume objects
        //

        pCurrentObject = VolumeCB->ObjectInfoListHead;

        if ( pCurrentObject)
        {

            //
            // Reference the node so it won't be torn down
            //

            lCount = AFSObjectInfoIncrement( pCurrentObject,
                                             AFS_OBJECT_REFERENCE_INVALIDATION);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateVolumeObjects Increment count on object %p Cnt %d\n",
                          pCurrentObject,
                          lCount));
        }

        while( pCurrentObject != NULL)
        {

            pNextObject = (AFSObjectInfoCB *)pCurrentObject->ListEntry.fLink;

            if ( pNextObject)
            {

                //
                // Reference the node so it won't be torn down
                //

                lCount = AFSObjectInfoIncrement( pNextObject,
                                                 AFS_OBJECT_REFERENCE_INVALIDATION);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInvalidateVolumeObjects Increment count on object %p Cnt %d\n",
                              pNextObject,
                              lCount));
            }

            AFSReleaseResource( VolumeCB->ObjectInfoTree.TreeLock);

            AFSInvalidateObject( &pCurrentObject,
                                 Reason);

            AFSAcquireShared( VolumeCB->ObjectInfoTree.TreeLock,
                              TRUE);

            if ( pCurrentObject )
            {

                lCount = AFSObjectInfoDecrement( pCurrentObject,
                                                 AFS_OBJECT_REFERENCE_INVALIDATION);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInvalidateVolumeObjects Decrement count on object %p Cnt %d\n",
                              pCurrentObject,
                              lCount));
            }

            pCurrentObject = pNextObject;
        }

        AFSReleaseResource( VolumeCB->ObjectInfoTree.TreeLock);
    }

    return ntStatus;
}

VOID
AFSInvalidateAllVolumes( VOID)
{
    AFSVolumeCB *pVolumeCB = NULL;
    AFSVolumeCB *pNextVolumeCB = NULL;
    AFSDeviceExt *pRDRDeviceExt = NULL;
    LONG lCount;

    pRDRDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSInvalidateAllVolumes Acquiring RDR VolumeListLock lock %p SHARED %08lX\n",
                  &pRDRDeviceExt->Specific.RDR.VolumeListLock,
                  PsGetCurrentThread()));

    AFSAcquireShared( &pRDRDeviceExt->Specific.RDR.VolumeListLock,
                      TRUE);

    pVolumeCB = pRDRDeviceExt->Specific.RDR.VolumeListHead;

    if ( pVolumeCB)
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInvalidateAllVolumes Acquiring VolumeRoot ObjectInfoTree lock %p SHARED %08lX\n",
                      pVolumeCB->ObjectInfoTree.TreeLock,
                      PsGetCurrentThread()));

        lCount = AFSVolumeIncrement( pVolumeCB,
                                     AFS_VOLUME_REFERENCE_INVALIDATE);

        AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInvalidateAllVolumes Increment count on volume %p Cnt %d\n",
                      pVolumeCB,
                      lCount));
    }

    while( pVolumeCB != NULL)
    {

        pNextVolumeCB = (AFSVolumeCB *)pVolumeCB->ListEntry.fLink;

        if ( pNextVolumeCB)
        {

            lCount = AFSVolumeIncrement( pNextVolumeCB,
                                         AFS_VOLUME_REFERENCE_INVALIDATE);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInvalidateAllVolumes Increment count on volume %p Cnt %d\n",
                          pVolumeCB,
                          lCount));
        }

        AFSReleaseResource( &pRDRDeviceExt->Specific.RDR.VolumeListLock);

        // do I need to hold the volume lock here?

        AFSInvalidateVolume( pVolumeCB, AFS_INVALIDATE_EXPIRED);

        AFSAcquireShared( &pRDRDeviceExt->Specific.RDR.VolumeListLock,
                          TRUE);

        lCount = AFSVolumeDecrement( pVolumeCB,
                                     AFS_VOLUME_REFERENCE_INVALIDATE);

        AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInvalidateAllVolumes Decrement count on volume %p Cnt %d\n",
                      pVolumeCB,
                      lCount));

        pVolumeCB = pNextVolumeCB;
    }

    AFSReleaseResource( &pRDRDeviceExt->Specific.RDR.VolumeListLock);
}

NTSTATUS
AFSVerifyEntry( IN GUID *AuthGroup,
		IN AFSDirectoryCB *DirEntry,
		IN BOOLEAN bFollowMountPoint)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    AFSDirEnumEntry *pDirEnumEntry = NULL;
    AFSObjectInfoCB *pObjectInfo = DirEntry->ObjectInformation;
    IO_STATUS_BLOCK stIoStatus;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSVerifyEntry Verifying entry %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      &DirEntry->NameInformation.FileName,
                      pObjectInfo->FileId.Cell,
                      pObjectInfo->FileId.Volume,
                      pObjectInfo->FileId.Vnode,
                      pObjectInfo->FileId.Unique));

        ntStatus = AFSEvaluateTargetByID( pObjectInfo,
                                          AuthGroup,
					  bFollowMountPoint ? FALSE : TRUE,
                                          &pDirEnumEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSVerifyEntry Evaluate Target failed %wZ FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
                          &DirEntry->NameInformation.FileName,
                          pObjectInfo->FileId.Cell,
                          pObjectInfo->FileId.Volume,
                          pObjectInfo->FileId.Vnode,
                          pObjectInfo->FileId.Unique,
                          ntStatus));

            try_return( ntStatus);
        }

        //
        // Check the data version of the file
        //

	if( pObjectInfo->DataVersion.QuadPart == pDirEnumEntry->DataVersion.QuadPart &&
	    !BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA))
	{

	    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
			  AFS_TRACE_LEVEL_VERBOSE,
			  "AFSVerifyEntry No DV change %I64X for Fcb %wZ FID %08lX-%08lX-%08lX-%08lX Clearing Verify Flag\n",
			  pObjectInfo->DataVersion.QuadPart,
			  &DirEntry->NameInformation.FileName,
			  pObjectInfo->FileId.Cell,
			  pObjectInfo->FileId.Volume,
			  pObjectInfo->FileId.Vnode,
			  pObjectInfo->FileId.Unique));

	    //
	    // We are ok, just get out
	    //

	    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

	    try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // New data version so we will need to process the node based on the type
        //

        switch( pDirEnumEntry->FileType)
        {

            case AFS_FILE_TYPE_MOUNTPOINT:
            {

                //
                // For a mount point we need to ensure the target is the same
                //

                if( !AFSIsEqualFID( &pObjectInfo->TargetFileId,
                                    &pDirEnumEntry->TargetFileId))
                {

                }

                //
                // Update the metadata for the entry
                //

                ntStatus = AFSUpdateMetaData( DirEntry,
                                              pDirEnumEntry);

                if( NT_SUCCESS( ntStatus))
                {

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSVerifyEntry MountPoint FID %08lX-%08lX-%08lX-%08lX Clearing Verify Flag\n",
				  pObjectInfo->FileId.Cell,
				  pObjectInfo->FileId.Volume,
				  pObjectInfo->FileId.Vnode,
				  pObjectInfo->FileId.Unique));

                    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);
                }

                break;
            }

            case AFS_FILE_TYPE_SYMLINK:
            {

                //
                // Update the metadata for the entry
                //

                ntStatus = AFSUpdateMetaData( DirEntry,
                                              pDirEnumEntry);

                if( NT_SUCCESS( ntStatus))
                {

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSVerifyEntry Symlink FID %08lX-%08lX-%08lX-%08lX Clearing Verify Flag\n",
				  pObjectInfo->FileId.Cell,
				  pObjectInfo->FileId.Volume,
				  pObjectInfo->FileId.Vnode,
				  pObjectInfo->FileId.Unique));

                    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);
                }

                break;
            }

            case AFS_FILE_TYPE_FILE:
            {
                FILE_OBJECT * pCCFileObject = NULL;

                if ( BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA))
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVerifyEntry Clearing VERIFY_DATA flag %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &DirEntry->NameInformation.FileName,
                                  pObjectInfo->FileId.Cell,
                                  pObjectInfo->FileId.Volume,
                                  pObjectInfo->FileId.Vnode,
                                  pObjectInfo->FileId.Unique));

                    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA);
                }

                if( pObjectInfo->Fcb != NULL)
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVerifyEntry Flush/purge entry %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &DirEntry->NameInformation.FileName,
                                  pObjectInfo->FileId.Cell,
                                  pObjectInfo->FileId.Volume,
                                  pObjectInfo->FileId.Vnode,
                                  pObjectInfo->FileId.Unique));

		    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSVerifyEntry Acquiring Fcb lock %p EXCL %08lX\n",
				  &pObjectInfo->Fcb->NPFcb->Resource,
				  PsGetCurrentThread()));

		    AFSAcquireExcl( &pObjectInfo->Fcb->NPFcb->Resource,
				    TRUE);

		    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSVerifyEntry Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
				  &pObjectInfo->Fcb->NPFcb->SectionObjectResource,
				  PsGetCurrentThread()));

                    AFSAcquireExcl( &pObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                    TRUE);

                    __try
                    {

                        CcFlushCache( &pObjectInfo->Fcb->NPFcb->SectionObjectPointers,
                                      NULL,
                                      0,
                                      &stIoStatus);

                        if( !NT_SUCCESS( stIoStatus.Status))
                        {

                            AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSVerifyEntry CcFlushCache failure %wZ FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX Bytes 0x%08lX\n",
                                          &DirEntry->NameInformation.FileName,
                                          pObjectInfo->FileId.Cell,
                                          pObjectInfo->FileId.Volume,
                                          pObjectInfo->FileId.Vnode,
                                          pObjectInfo->FileId.Unique,
                                          stIoStatus.Status,
                                          stIoStatus.Information));

                            ntStatus = stIoStatus.Status;
                        }

			if ( pObjectInfo->Fcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL)
                        {

                            if ( !CcPurgeCacheSection( &pObjectInfo->Fcb->NPFcb->SectionObjectPointers,
                                                       NULL,
                                                       0,
                                                       FALSE))
                            {

                                AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                              AFS_TRACE_LEVEL_WARNING,
                                              "AFSVerifyEntry CcPurgeCacheSection failure %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                              &DirEntry->NameInformation.FileName,
                                              pObjectInfo->FileId.Cell,
                                              pObjectInfo->FileId.Volume,
                                              pObjectInfo->FileId.Vnode,
                                              pObjectInfo->FileId.Unique));

                                SetFlag( pObjectInfo->Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
                            }
                        }
                    }
		    __except( EXCEPTION_EXECUTE_HANDLER)
                    {
                        ntStatus = GetExceptionCode();

                        AFSDbgTrace(( 0,
                                      0,
                                      "EXCEPTION - AFSVerifyEntry CcFlushCache or CcPurgeCacheSection %wZ FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
                                      &DirEntry->NameInformation.FileName,
                                      pObjectInfo->FileId.Cell,
                                      pObjectInfo->FileId.Volume,
                                      pObjectInfo->FileId.Vnode,
                                      pObjectInfo->FileId.Unique,
                                      ntStatus));

                        SetFlag( pObjectInfo->Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
                    }

		    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSVerifyEntry Releasing Fcb SectionObject lock %p EXCL %08lX\n",
				  &pObjectInfo->Fcb->NPFcb->SectionObjectResource,
				  PsGetCurrentThread()));

                    AFSReleaseResource( &pObjectInfo->Fcb->NPFcb->SectionObjectResource);

		    if( !BooleanFlagOn( pRDRDevExt->DeviceFlags, AFS_DEVICE_FLAG_DIRECT_SERVICE_IO))
		    {

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
				      AFS_TRACE_LEVEL_VERBOSE,
				      "AFSVerifyEntry Releasing Fcb lock %p EXCL %08lX\n",
				      &pObjectInfo->Fcb->NPFcb->Resource,
				      PsGetCurrentThread()));

			AFSReleaseResource( &pObjectInfo->Fcb->NPFcb->Resource);

			AFSFlushExtents( pObjectInfo->Fcb,
					 AuthGroup);

			//
			// Acquire the Fcb to purge the cache
			//

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
				      AFS_TRACE_LEVEL_VERBOSE,
				      "AFSVerifyEntry Acquiring Fcb lock %p EXCL %08lX\n",
				      &pObjectInfo->Fcb->NPFcb->Resource,
				      PsGetCurrentThread()));

			AFSAcquireExcl( &pObjectInfo->Fcb->NPFcb->Resource,
					TRUE);
		    }

                    //
                    // Update the metadata for the entry
                    //

                    ntStatus = AFSUpdateMetaData( DirEntry,
                                                  pDirEnumEntry);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSVerifyEntry Meta Data Update failed %wZ FID %08lX-%08lX-%08lX-%08lX ntStatus %08lX\n",
                                      &DirEntry->NameInformation.FileName,
                                      pObjectInfo->FileId.Cell,
                                      pObjectInfo->FileId.Volume,
                                      pObjectInfo->FileId.Vnode,
                                      pObjectInfo->FileId.Unique,
                                      ntStatus));
                    }
		    else
		    {

			//
			// Update file sizes
			//

			pObjectInfo->Fcb->Header.AllocationSize.QuadPart  = pObjectInfo->AllocationSize.QuadPart;
			pObjectInfo->Fcb->Header.FileSize.QuadPart        = pObjectInfo->EndOfFile.QuadPart;
			pObjectInfo->Fcb->Header.ValidDataLength.QuadPart = pObjectInfo->EndOfFile.QuadPart;

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
				      AFS_TRACE_LEVEL_VERBOSE,
				      "AFSVerifyEntry Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
				      &pObjectInfo->Fcb->NPFcb->SectionObjectResource,
				      PsGetCurrentThread()));

			AFSAcquireExcl( &pObjectInfo->Fcb->NPFcb->SectionObjectResource,
					TRUE);

			__try
			{

			    pCCFileObject = CcGetFileObjectFromSectionPtrs( &pObjectInfo->Fcb->NPFcb->SectionObjectPointers);

			    if ( pCCFileObject != NULL)
			    {
				CcSetFileSizes( pCCFileObject,
						(PCC_FILE_SIZES)&pObjectInfo->Fcb->Header.AllocationSize);
			    }
			}
			__except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()))
			{

			    ntStatus = GetExceptionCode();

			    AFSDbgTrace(( 0,
					  0,
					  "EXCEPTION - AFSVerifyEntry CcSetFileSized failed FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
					  pObjectInfo->FileId.Cell,
					  pObjectInfo->FileId.Volume,
					  pObjectInfo->FileId.Vnode,
					  pObjectInfo->FileId.Unique,
					  ntStatus));
			}

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
				      AFS_TRACE_LEVEL_VERBOSE,
				      "AFSVerifyEntry Releasing Fcb SectionObject lock %p EXCL %08lX\n",
				      &pObjectInfo->Fcb->NPFcb->SectionObjectResource,
				      PsGetCurrentThread()));

			AFSReleaseResource( &pObjectInfo->Fcb->NPFcb->SectionObjectResource);
                    }

                    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSVerifyEntry Releasing Fcb lock %p EXCL %08lX\n",
				  &pObjectInfo->Fcb->NPFcb->Resource,
                                  PsGetCurrentThread()));

                    AFSReleaseResource( &pObjectInfo->Fcb->NPFcb->Resource);
                }
                else
                {

                    //
                    // Update the metadata for the entry
                    //

                    ntStatus = AFSUpdateMetaData( DirEntry,
                                                  pDirEnumEntry);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSVerifyEntry Meta Data Update failed %wZ FID %08lX-%08lX-%08lX-%08lX ntStatus %08lX\n",
                                      &DirEntry->NameInformation.FileName,
                                      pObjectInfo->FileId.Cell,
                                      pObjectInfo->FileId.Volume,
                                      pObjectInfo->FileId.Vnode,
                                      pObjectInfo->FileId.Unique,
                                      ntStatus));
                    }
		    else
		    {

			AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				      AFS_TRACE_LEVEL_WARNING,
				      "AFSVerifyEntry Fcb NULL %wZ FID %08lX-%08lX-%08lX-%08lX\n",
				      &DirEntry->NameInformation.FileName,
				      pObjectInfo->FileId.Cell,
				      pObjectInfo->FileId.Volume,
				      pObjectInfo->FileId.Vnode,
				      pObjectInfo->FileId.Unique));
		    }
                }

		if ( NT_SUCCESS( ntStatus))
		{

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSVerifyEntry File FID %08lX-%08lX-%08lX-%08lX Clearing Verify Flag\n",
				  pObjectInfo->FileId.Cell,
				  pObjectInfo->FileId.Volume,
				  pObjectInfo->FileId.Vnode,
				  pObjectInfo->FileId.Unique));

		    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);
		}
                break;
            }

            case AFS_FILE_TYPE_DIRECTORY:
            {

                //
                // For a directory or root entry flush the content of
                // the directory enumeration.
                //

                if( BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_FLAGS_DIRECTORY_ENUMERATED))
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSVerifyEntry Validating directory content for entry %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &DirEntry->NameInformation.FileName,
                                  pObjectInfo->FileId.Cell,
                                  pObjectInfo->FileId.Volume,
                                  pObjectInfo->FileId.Vnode,
                                  pObjectInfo->FileId.Unique));

                    AFSAcquireExcl( pObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                    TRUE);

                    ntStatus = AFSValidateDirectoryCache( pObjectInfo,
                                                          AuthGroup);

                    AFSReleaseResource( pObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);

                    if ( !NT_SUCCESS( ntStatus))
                    {

                        try_return( ntStatus);
                    }
                }

                //
                // Update the metadata for the entry
                //

                ntStatus = AFSUpdateMetaData( DirEntry,
                                              pDirEnumEntry);

                if( NT_SUCCESS( ntStatus))
                {

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSVerifyEntry Directory FID %08lX-%08lX-%08lX-%08lX Clearing Verify Flag\n",
				  pObjectInfo->FileId.Cell,
				  pObjectInfo->FileId.Volume,
				  pObjectInfo->FileId.Vnode,
				  pObjectInfo->FileId.Unique));

                    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);
                }

                break;
            }

            case AFS_FILE_TYPE_DFSLINK:
            {

                UNICODE_STRING uniTargetName;

                //
                // For a DFS link need to check the target name has not changed
                //

                uniTargetName.Length = (USHORT)pDirEnumEntry->TargetNameLength;

                uniTargetName.MaximumLength = uniTargetName.Length;

                uniTargetName.Buffer = (WCHAR *)((char *)pDirEnumEntry + pDirEnumEntry->TargetNameOffset);

                AFSAcquireExcl( &DirEntry->NonPaged->Lock,
                                TRUE);

                if( DirEntry->NameInformation.TargetName.Length == 0 ||
                    RtlCompareUnicodeString( &uniTargetName,
                                             &DirEntry->NameInformation.TargetName,
                                             TRUE) != 0)
                {

                    //
                    // Update the target name
                    //

                    ntStatus = AFSUpdateTargetName( &DirEntry->NameInformation.TargetName,
                                                    &DirEntry->Flags,
                                                    uniTargetName.Buffer,
                                                    uniTargetName.Length);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSReleaseResource( &DirEntry->NonPaged->Lock);

                        break;
                    }
                }

                AFSReleaseResource( &DirEntry->NonPaged->Lock);

                //
                // Update the metadata for the entry
                //

                ntStatus = AFSUpdateMetaData( DirEntry,
                                              pDirEnumEntry);

                if( NT_SUCCESS( ntStatus))
                {

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSVerifyEntry DFSLink FID %08lX-%08lX-%08lX-%08lX Clearing Verify Flag\n",
				  pObjectInfo->FileId.Cell,
				  pObjectInfo->FileId.Volume,
				  pObjectInfo->FileId.Vnode,
				  pObjectInfo->FileId.Unique));

                    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);
                }

                break;
            }

            default:

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSVerifyEntry Attempt to verify node of type %d %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              pObjectInfo->FileType,
                              &DirEntry->NameInformation.FileName,
                              pObjectInfo->FileId.Cell,
                              pObjectInfo->FileId.Volume,
                              pObjectInfo->FileId.Vnode,
                              pObjectInfo->FileId.Unique));

                break;
        }

 try_exit:

        if( pDirEnumEntry != NULL)
        {

            AFSExFreePoolWithTag( pDirEnumEntry, AFS_GENERIC_MEMORY_2_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSSetVolumeState( IN AFSVolumeStatusCB *VolumeStatus)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    ULONGLONG   ullIndex = 0;
    AFSVolumeCB *pVolumeCB = NULL;
    LONG lCount;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetVolumeState Marking volume state %d Volume Cell %08lX Volume %08lX\n",
                      VolumeStatus->Online,
                      VolumeStatus->FileID.Cell,
                      VolumeStatus->FileID.Volume));

        //
        // Need to locate the Fcb for the directory to purge
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetVolumeState Acquiring RDR VolumeTreeLock lock %p SHARED %08lX\n",
                      &pDevExt->Specific.RDR.VolumeTreeLock,
                      PsGetCurrentThread()));

        AFSAcquireShared( &pDevExt->Specific.RDR.VolumeTreeLock, TRUE);

        //
        // Locate the volume node
        //

        ullIndex = AFSCreateHighIndex( &VolumeStatus->FileID);

        ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                       ullIndex,
                                       (AFSBTreeEntry **)&pVolumeCB);

        if( pVolumeCB != NULL)
        {

            lCount = AFSVolumeIncrement( pVolumeCB,
                                         AFS_VOLUME_REFERENCE_INVALIDATE);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetVolumeState Increment count on volume %p Cnt %d\n",
                          pVolumeCB,
                          lCount));

            AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

            //
            // Set the volume state accordingly
            //

            if( VolumeStatus->Online)
            {

                InterlockedAnd( (LONG *)&(pVolumeCB->Flags), ~AFS_VOLUME_FLAGS_OFFLINE);
            }
            else
            {

                InterlockedOr( (LONG *)&(pVolumeCB->Flags), AFS_VOLUME_FLAGS_OFFLINE);
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSSetNetworkState( IN AFSNetworkStatusCB *NetworkStatus)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        if( AFSGlobalRoot == NULL)
        {

            try_return( ntStatus);
        }

        AFSAcquireExcl( AFSGlobalRoot->VolumeLock,
                        TRUE);

        //
        // Set the network state according to the information
        //

        if( NetworkStatus->Online)
        {

            ClearFlag( AFSGlobalRoot->Flags, AFS_VOLUME_FLAGS_OFFLINE);
        }
        else
        {

            SetFlag( AFSGlobalRoot->Flags, AFS_VOLUME_FLAGS_OFFLINE);
        }

        AFSReleaseResource( AFSGlobalRoot->VolumeLock);

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSValidateDirectoryCache( IN AFSObjectInfoCB *ObjectInfo,
                           IN GUID *AuthGroup)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    BOOLEAN  bAcquiredLock = FALSE;
    AFSDirectoryCB *pCurrentDirEntry = NULL, *pNextDirEntry = NULL;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSValidateDirectoryCache Validating content for FID %08lX-%08lX-%08lX-%08lX\n",
                      ObjectInfo->FileId.Cell,
                      ObjectInfo->FileId.Volume,
                      ObjectInfo->FileId.Vnode,
                      ObjectInfo->FileId.Unique));

        if( !ExIsResourceAcquiredLite( ObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSValidateDirectoryCache Acquiring DirectoryNodeHdr.TreeLock lock %p EXCL %08lX\n",
                          ObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                          PsGetCurrentThread()));

            AFSAcquireExcl( ObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                            TRUE);

            bAcquiredLock = TRUE;
        }

        //
        // Check for inconsistency between DirectoryNodeList and DirectoryNodeCount
        //

        if ( ObjectInfo->Specific.Directory.DirectoryNodeListHead == NULL &&
             ObjectInfo->Specific.Directory.DirectoryNodeCount > 0)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSValidateDirectoryCache Empty Node List but Non-Zero Node Count %d for dir FID %08lX-%08lX-%08lX-%08lX\n",
                          ObjectInfo->Specific.Directory.DirectoryNodeCount,
                          ObjectInfo->FileId.Cell,
                          ObjectInfo->FileId.Volume,
                          ObjectInfo->FileId.Vnode,
                          ObjectInfo->FileId.Unique));
        }

        //
        // Reset the directory list information by clearing all valid entries
        //

        pCurrentDirEntry = ObjectInfo->Specific.Directory.DirectoryNodeListHead;

        while( pCurrentDirEntry != NULL)
        {

            pNextDirEntry = (AFSDirectoryCB *)pCurrentDirEntry->ListEntry.fLink;

            if( !BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_FAKE))
            {

                //
                // If this entry has been deleted then process it here
                //

                if( BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_DELETED) &&
                    pCurrentDirEntry->DirOpenReferenceCount <= 0 &&
                    pCurrentDirEntry->NameArrayReferenceCount <= 0)
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateDirectoryCache Deleting dir entry %p name %wZ\n",
                                  pCurrentDirEntry,
                                  &pCurrentDirEntry->NameInformation.FileName));

                    AFSDeleteDirEntry( ObjectInfo,
                                       &pCurrentDirEntry);
                }
                else
                {

                    ClearFlag( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_VALID);

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateDirectoryCache Clear VALID flag on DE %p Reference count %d\n",
                                  pCurrentDirEntry,
                                  pCurrentDirEntry->DirOpenReferenceCount));

                    //
                    // We pull the short name from the parent tree since it could change below
                    //

                    if( BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME))
                    {

                        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSValidateDirectoryCache Removing DE %p (%08lX) from shortname tree for %wZ\n",
                                      pCurrentDirEntry,
                                      pCurrentDirEntry->Type.Data.ShortNameTreeEntry.HashIndex,
                                      &pCurrentDirEntry->NameInformation.FileName));

                        AFSRemoveShortNameDirEntry( &ObjectInfo->Specific.Directory.ShortNameTree,
                                                    pCurrentDirEntry);

                        ClearFlag( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME);
                    }
                }
            }

            pCurrentDirEntry = pNextDirEntry;
        }

        //
        // Reget the directory contents
        //

        ntStatus = AFSVerifyDirectoryContent( ObjectInfo,
                                              AuthGroup);

        if ( !NT_SUCCESS( ntStatus))
        {
            try_return( ntStatus);
        }

        //
        // Now start again and tear down any entries not valid
        //

        pCurrentDirEntry = ObjectInfo->Specific.Directory.DirectoryNodeListHead;

        while( pCurrentDirEntry != NULL)
        {

            pNextDirEntry = (AFSDirectoryCB *)pCurrentDirEntry->ListEntry.fLink;

            if( BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_VALID))
            {

                if( !BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
                    !BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME) &&
                    pCurrentDirEntry->Type.Data.ShortNameTreeEntry.HashIndex > 0)
                {

                    if( ObjectInfo->Specific.Directory.ShortNameTree == NULL)
                    {

                        ObjectInfo->Specific.Directory.ShortNameTree = pCurrentDirEntry;

                        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSValidateDirectoryCache Insert DE %p to head of shortname tree for %wZ\n",
                                      pCurrentDirEntry,
                                      &pCurrentDirEntry->NameInformation.FileName));

                        SetFlag( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME);
                    }
                    else
                    {

                        if( !NT_SUCCESS( AFSInsertShortNameDirEntry( ObjectInfo->Specific.Directory.ShortNameTree,
                                                                     pCurrentDirEntry)))
                        {
                            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSValidateDirectoryCache Failed to insert DE %p (%08lX) to shortname tree for %wZ\n",
                                          pCurrentDirEntry,
                                          pCurrentDirEntry->Type.Data.ShortNameTreeEntry.HashIndex,
                                          &pCurrentDirEntry->NameInformation.FileName));
                        }
                        else
                        {
                            SetFlag( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME);

                            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSValidateDirectoryCache Insert DE %p to shortname tree for %wZ\n",
                                          pCurrentDirEntry,
                                          &pCurrentDirEntry->NameInformation.FileName));
                        }
                    }
                }

                pCurrentDirEntry = pNextDirEntry;

                continue;
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSValidateDirectoryCache Processing INVALID DE %p Reference count %d\n",
                          pCurrentDirEntry,
                          pCurrentDirEntry->DirOpenReferenceCount));

            if( pCurrentDirEntry->DirOpenReferenceCount <= 0 &&
                pCurrentDirEntry->NameArrayReferenceCount <= 0)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSValidateDirectoryCache Deleting dir entry %wZ from parent FID %08lX-%08lX-%08lX-%08lX\n",
                              &pCurrentDirEntry->NameInformation.FileName,
                              ObjectInfo->FileId.Cell,
                              ObjectInfo->FileId.Volume,
                              ObjectInfo->FileId.Vnode,
                              ObjectInfo->FileId.Unique));

                AFSDeleteDirEntry( ObjectInfo,
                                   &pCurrentDirEntry);
            }
            else
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSValidateDirectoryCache Setting dir entry %p Name %wZ DELETED in parent FID %08lX-%08lX-%08lX-%08lX\n",
                              pCurrentDirEntry,
                              &pCurrentDirEntry->NameInformation.FileName,
                              ObjectInfo->FileId.Cell,
                              ObjectInfo->FileId.Volume,
                              ObjectInfo->FileId.Vnode,
                              ObjectInfo->FileId.Unique));

                SetFlag( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_DELETED);

                AFSRemoveNameEntry( ObjectInfo,
                                    pCurrentDirEntry);
            }

            pCurrentDirEntry = pNextDirEntry;
        }

#if DBG
        if( !AFSValidateDirList( ObjectInfo))
        {

            AFSPrint("AFSValidateDirectoryCache Invalid count ...\n");
        }
#endif

try_exit:

        if( bAcquiredLock)
        {

            AFSReleaseResource( ObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);
        }
    }

    return ntStatus;
}

BOOLEAN
AFSIsVolumeFID( IN AFSFileID *FileID)
{

    BOOLEAN bIsVolume = FALSE;

    if( FileID->Vnode == 1 &&
        FileID->Unique == 1)
    {

        bIsVolume = TRUE;
    }

    return bIsVolume;
}

BOOLEAN
AFSIsFinalNode( IN AFSFcb *Fcb)
{

    BOOLEAN bIsFinalNode = FALSE;

    if( Fcb->Header.NodeTypeCode == AFS_ROOT_FCB ||
        Fcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB ||
        Fcb->Header.NodeTypeCode == AFS_FILE_FCB ||
        Fcb->Header.NodeTypeCode == AFS_DFS_LINK_FCB ||
        Fcb->Header.NodeTypeCode == AFS_INVALID_FCB )
    {

        bIsFinalNode = TRUE;
    }
    else
    {

        ASSERT( Fcb->Header.NodeTypeCode == AFS_MOUNT_POINT_FCB ||
                Fcb->Header.NodeTypeCode == AFS_SYMBOLIC_LINK_FCB);
    }

    return bIsFinalNode;
}

NTSTATUS
AFSUpdateMetaData( IN AFSDirectoryCB *DirEntry,
                   IN AFSDirEnumEntry *DirEnumEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniTargetName;
    AFSObjectInfoCB *pObjectInfo = DirEntry->ObjectInformation;

    __Enter
    {

        pObjectInfo->TargetFileId = DirEnumEntry->TargetFileId;

        pObjectInfo->Expiration = DirEnumEntry->Expiration;

        pObjectInfo->DataVersion = DirEnumEntry->DataVersion;

        pObjectInfo->FileType = DirEnumEntry->FileType;

        pObjectInfo->CreationTime = DirEnumEntry->CreationTime;

        pObjectInfo->LastAccessTime = DirEnumEntry->LastAccessTime;

        pObjectInfo->LastWriteTime = DirEnumEntry->LastWriteTime;

        pObjectInfo->ChangeTime = DirEnumEntry->ChangeTime;

        pObjectInfo->EndOfFile = DirEnumEntry->EndOfFile;

        pObjectInfo->AllocationSize = DirEnumEntry->AllocationSize;

        pObjectInfo->FileAttributes = DirEnumEntry->FileAttributes;

        if( pObjectInfo->FileType == AFS_FILE_TYPE_MOUNTPOINT ||
            pObjectInfo->FileType == AFS_FILE_TYPE_DFSLINK)
        {

            pObjectInfo->FileAttributes |= (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
        }

        if( pObjectInfo->FileType == AFS_FILE_TYPE_SYMLINK)
        {

            if ( pObjectInfo->FileAttributes == FILE_ATTRIBUTE_NORMAL)
            {

                pObjectInfo->FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
            }
            else
            {

                pObjectInfo->FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
            }
        }

        pObjectInfo->EaSize = DirEnumEntry->EaSize;

        pObjectInfo->Links = DirEnumEntry->Links;

        if( DirEnumEntry->TargetNameLength > 0 &&
            ( DirEntry->NameInformation.TargetName.Length != DirEnumEntry->TargetNameLength ||
              DirEntry->ObjectInformation->DataVersion.QuadPart != DirEnumEntry->DataVersion.QuadPart))
        {

            //
            // Update the target name information if needed
            //

            uniTargetName.Length = (USHORT)DirEnumEntry->TargetNameLength;

            uniTargetName.MaximumLength = uniTargetName.Length;

            uniTargetName.Buffer = (WCHAR *)((char *)DirEnumEntry + DirEnumEntry->TargetNameOffset);

            AFSAcquireExcl( &DirEntry->NonPaged->Lock,
                            TRUE);

            if( DirEntry->NameInformation.TargetName.Length == 0 ||
                RtlCompareUnicodeString( &uniTargetName,
                                         &DirEntry->NameInformation.TargetName,
                                         TRUE) != 0)
            {

                //
                // Update the target name
                //

                ntStatus = AFSUpdateTargetName( &DirEntry->NameInformation.TargetName,
                                                &DirEntry->Flags,
                                                uniTargetName.Buffer,
                                                uniTargetName.Length);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &DirEntry->NonPaged->Lock);

                    try_return( ntStatus);
                }
            }

            AFSReleaseResource( &DirEntry->NonPaged->Lock);
        }
        else if( DirEntry->NameInformation.TargetName.Length > 0 &&
                 DirEntry->ObjectInformation->DataVersion.QuadPart != DirEnumEntry->DataVersion.QuadPart)
        {

            AFSAcquireExcl( &DirEntry->NonPaged->Lock,
                            TRUE);

            if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER) &&
                DirEntry->NameInformation.TargetName.Buffer != NULL)
            {
                AFSExFreePoolWithTag( DirEntry->NameInformation.TargetName.Buffer, AFS_NAME_BUFFER_FIVE_TAG);
            }

            ClearFlag( DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER);

            DirEntry->NameInformation.TargetName.Length = 0;
            DirEntry->NameInformation.TargetName.MaximumLength = 0;
            DirEntry->NameInformation.TargetName.Buffer = NULL;

            AFSReleaseResource( &DirEntry->NonPaged->Lock);
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSValidateEntry( IN AFSDirectoryCB *DirEntry,
                  IN GUID *AuthGroup,
                  IN BOOLEAN FastCall,
                  IN BOOLEAN bSafeToPurge)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    LARGE_INTEGER liSystemTime;
    AFSDirEnumEntry *pDirEnumEntry = NULL;
    AFSFcb *pCurrentFcb = NULL;
    BOOLEAN bReleaseFcb = FALSE;
    AFSObjectInfoCB *pObjectInfo = DirEntry->ObjectInformation;

    __Enter
    {

        //
        // If we have an Fcb hanging off the directory entry then be sure to acquire the locks in the
        // correct order
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSValidateEntry Validating entry %wZ FID %08lX-%08lX-%08lX-%08lX FastCall %u\n",
                      &DirEntry->NameInformation.FileName,
                      pObjectInfo->FileId.Cell,
                      pObjectInfo->FileId.Volume,
                      pObjectInfo->FileId.Vnode,
                      pObjectInfo->FileId.Unique,
                      FastCall));

        //
        // If this is a fake node then bail since the service knows nothing about it
        //

        if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_ENTRY_FAKE))
        {

            try_return( ntStatus);
        }

        //
        // This routine ensures that the current entry is valid by:
        //
        //      1) Checking that the expiration time is non-zero and after where we
        //         currently are
        //

        KeQuerySystemTime( &liSystemTime);

        if( !BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_FLAGS_NOT_EVALUATED) &&
            !BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY) &&
            !BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA) &&
            pObjectInfo->Expiration.QuadPart >= liSystemTime.QuadPart)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSValidateEntry Directory entry %wZ FID %08lX-%08lX-%08lX-%08lX VALID\n",
                          &DirEntry->NameInformation.FileName,
                          pObjectInfo->FileId.Cell,
                          pObjectInfo->FileId.Volume,
                          pObjectInfo->FileId.Vnode,
                          pObjectInfo->FileId.Unique));

            try_return( ntStatus);
        }

        //
        // This node requires updating
        //

        ntStatus = AFSEvaluateTargetByID( pObjectInfo,
                                          AuthGroup,
                                          FastCall,
                                          &pDirEnumEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSValidateEntry Failed to evaluate entry FastCall %d %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          FastCall,
                          &DirEntry->NameInformation.FileName,
                          pObjectInfo->FileId.Cell,
                          pObjectInfo->FileId.Volume,
                          pObjectInfo->FileId.Vnode,
                          pObjectInfo->FileId.Unique,
                          ntStatus));

            //
            // Failed validation of node so return access-denied
            //

            try_return( ntStatus);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSValidateEntry Validating entry FastCall %d %wZ FID %08lX-%08lX-%08lX-%08lX DV %I64X returned DV %I64X FT %d\n",
                      FastCall,
                      &DirEntry->NameInformation.FileName,
                      pObjectInfo->FileId.Cell,
                      pObjectInfo->FileId.Volume,
                      pObjectInfo->FileId.Vnode,
                      pObjectInfo->FileId.Unique,
                      pObjectInfo->DataVersion.QuadPart,
                      pDirEnumEntry->DataVersion.QuadPart,
                      pDirEnumEntry->FileType));


        //
        // Based on the file type, process the node
        //

        switch( pDirEnumEntry->FileType)
        {

            case AFS_FILE_TYPE_MOUNTPOINT:
            {

                //
                // Update the metadata for the entry
                //

                ntStatus = AFSUpdateMetaData( DirEntry,
                                              pDirEnumEntry);

                if( NT_SUCCESS( ntStatus))
                {

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSValidateEntry MountPoint FID %08lX-%08lX-%08lX-%08lX Clearing Verify Flag\n",
				  pObjectInfo->FileId.Cell,
				  pObjectInfo->FileId.Volume,
				  pObjectInfo->FileId.Vnode,
				  pObjectInfo->FileId.Unique));

                    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY | AFS_OBJECT_FLAGS_NOT_EVALUATED);
                }

                break;
            }

            case AFS_FILE_TYPE_SYMLINK:
            case AFS_FILE_TYPE_DFSLINK:
            {

                //
                // Update the metadata for the entry
                //

                ntStatus = AFSUpdateMetaData( DirEntry,
                                              pDirEnumEntry);

                if( NT_SUCCESS( ntStatus))
                {

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSValidateEntry Symlink FID %08lX-%08lX-%08lX-%08lX Clearing Verify Flag\n",
				  pObjectInfo->FileId.Cell,
				  pObjectInfo->FileId.Volume,
				  pObjectInfo->FileId.Vnode,
				  pObjectInfo->FileId.Unique));

                    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY | AFS_OBJECT_FLAGS_NOT_EVALUATED);
                }

                break;
            }

            case AFS_FILE_TYPE_FILE:
            {

                BOOLEAN bPurgeExtents = FALSE;

                //
                // For a file where the data version has become invalid we need to
                // fail any current extent requests and purge the cache for the file
                // Can't hold the Fcb resource while doing this
                //

                if( pObjectInfo->Fcb != NULL &&
                    (pObjectInfo->DataVersion.QuadPart != pDirEnumEntry->DataVersion.QuadPart ||
                      BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA)))
                {

                    pCurrentFcb = pObjectInfo->Fcb;

                    if( !ExIsResourceAcquiredLite( &pCurrentFcb->NPFcb->Resource))
                    {

                        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSValidateEntry Acquiring Fcb lock %p EXCL %08lX\n",
                                      &pCurrentFcb->NPFcb->Resource,
                                      PsGetCurrentThread()));

                        AFSAcquireExcl( &pCurrentFcb->NPFcb->Resource,
                                        TRUE);

			bReleaseFcb = TRUE;
		    }

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE_2,
				  "AFSValidateEntry Flush/purge entry %wZ FID %08lX-%08lX-%08lX-%08lX\n",
				  &DirEntry->NameInformation.FileName,
				  pObjectInfo->FileId.Cell,
				  pObjectInfo->FileId.Volume,
				  pObjectInfo->FileId.Vnode,
				  pObjectInfo->FileId.Unique));

		    if ( pObjectInfo->DataVersion.QuadPart != pDirEnumEntry->DataVersion.QuadPart)
		    {

			AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				      AFS_TRACE_LEVEL_VERBOSE,
				      "AFSValidateEntry DV Change %wZ FID %08lX-%08lX-%08lX-%08lX (%08lX != %08lX)\n",
				      &DirEntry->NameInformation.FileName,
				      pObjectInfo->FileId.Cell,
				      pObjectInfo->FileId.Volume,
				      pObjectInfo->FileId.Vnode,
				      pObjectInfo->FileId.Unique,
				      pObjectInfo->DataVersion.LowPart,
				      pDirEnumEntry->DataVersion.LowPart));

			bPurgeExtents = TRUE;
		    }

		    if ( bSafeToPurge)
		    {

			if ( BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA))
			{
			    bPurgeExtents = TRUE;

			    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
					  AFS_TRACE_LEVEL_VERBOSE,
					  "AFSVerifyEntry Clearing VERIFY_DATA flag %wZ FID %08lX-%08lX-%08lX-%08lX\n",
					  &DirEntry->NameInformation.FileName,
					  pObjectInfo->FileId.Cell,
					  pObjectInfo->FileId.Volume,
					  pObjectInfo->FileId.Vnode,
					  pObjectInfo->FileId.Unique));

			    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA);
			}

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
				      AFS_TRACE_LEVEL_VERBOSE,
				      "AFSValidateEntry Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
				      &pCurrentFcb->NPFcb->SectionObjectResource,
				      PsGetCurrentThread()));

			AFSAcquireExcl( &pCurrentFcb->NPFcb->SectionObjectResource,
					TRUE);

			__try
			{

			    IO_STATUS_BLOCK stIoStatus;

			    CcFlushCache( &pCurrentFcb->NPFcb->SectionObjectPointers,
					  NULL,
					  0,
					  &stIoStatus);

			    if( !NT_SUCCESS( stIoStatus.Status))
			    {

				AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
					      AFS_TRACE_LEVEL_ERROR,
					      "AFSValidateEntry CcFlushCache failure %wZ FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX Bytes 0x%08lX\n",
					      &DirEntry->NameInformation.FileName,
					      pObjectInfo->FileId.Cell,
					      pObjectInfo->FileId.Volume,
					      pObjectInfo->FileId.Vnode,
					      pObjectInfo->FileId.Unique,
					      stIoStatus.Status,
					      stIoStatus.Information));

				ntStatus = stIoStatus.Status;
			    }

			    if ( bPurgeExtents &&
				 pCurrentFcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL)
			    {

				if ( !CcPurgeCacheSection( &pCurrentFcb->NPFcb->SectionObjectPointers,
							   NULL,
							   0,
							   FALSE))
				{

				    AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
						  AFS_TRACE_LEVEL_WARNING,
						  "AFSValidateEntry CcPurgeCacheSection failure %wZ FID %08lX-%08lX-%08lX-%08lX\n",
						  &DirEntry->NameInformation.FileName,
						  pObjectInfo->FileId.Cell,
						  pObjectInfo->FileId.Volume,
						  pObjectInfo->FileId.Vnode,
						  pObjectInfo->FileId.Unique));

				    SetFlag( pObjectInfo->Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
				}
			    }
			}
			__except( EXCEPTION_EXECUTE_HANDLER)
			{
			    ntStatus = GetExceptionCode();

			    AFSDbgTrace(( 0,
					  0,
					  "EXCEPTION - AFSValidateEntry CcFlushCache or CcPurgeCacheSection %wZ FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
					  &DirEntry->NameInformation.FileName,
					  pObjectInfo->FileId.Cell,
					  pObjectInfo->FileId.Volume,
					  pObjectInfo->FileId.Vnode,
					  pObjectInfo->FileId.Unique,
					  ntStatus));

			    SetFlag( pCurrentFcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
			}

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
				      AFS_TRACE_LEVEL_VERBOSE,
				      "AFSValidateEntry Releasing Fcb SectionObject lock %p EXCL %08lX\n",
				      &pCurrentFcb->NPFcb->SectionObjectResource,
				      PsGetCurrentThread()));

			AFSReleaseResource( &pCurrentFcb->NPFcb->SectionObjectResource);
		    }
		    else
		    {

			if ( bPurgeExtents)
			{

			    SetFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA);
			}
		    }

		    if (bReleaseFcb)
		    {
			AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

			bReleaseFcb = FALSE;
		    }

		    if ( bPurgeExtents &&
			 bSafeToPurge)
		    {

			if( !BooleanFlagOn( pRDRDevExt->DeviceFlags, AFS_DEVICE_FLAG_DIRECT_SERVICE_IO))
			{
			    AFSFlushExtents( pCurrentFcb,
					     AuthGroup);
			}
		    }
		}

                //
                // Update the metadata for the entry but only if it is safe to do so.
                // If it was determined that a data version change has occurred or
                // that a pending data verification was required, do not update the
                // ObjectInfo meta data or the FileObject size information.  That
                // way it is consistent for the next time that the data is verified
                // or validated.
                //

                if ( !(bPurgeExtents && bSafeToPurge))
                {

                    ntStatus = AFSUpdateMetaData( DirEntry,
                                                  pDirEnumEntry);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSValidateEntry Meta Data Update failed %wZ FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
                                      &DirEntry->NameInformation.FileName,
                                      pObjectInfo->FileId.Cell,
                                      pObjectInfo->FileId.Volume,
                                      pObjectInfo->FileId.Vnode,
                                      pObjectInfo->FileId.Unique,
                                      ntStatus));

                        break;
                    }

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSValidateEntry File FID %08lX-%08lX-%08lX-%08lX No Purge Clearing Verify Flag\n",
				  pObjectInfo->FileId.Cell,
				  pObjectInfo->FileId.Volume,
				  pObjectInfo->FileId.Vnode,
				  pObjectInfo->FileId.Unique));

                    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY | AFS_OBJECT_FLAGS_NOT_EVALUATED);

                    //
                    // Update file sizes
                    //

                    if( pObjectInfo->Fcb != NULL)
                    {
                        FILE_OBJECT *pCCFileObject;

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSValidateEntry Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
                                      &pObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                      PsGetCurrentThread()));

                        AFSAcquireExcl( &pObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                        TRUE);

			__try
			{

			    pCCFileObject = CcGetFileObjectFromSectionPtrs( &pObjectInfo->Fcb->NPFcb->SectionObjectPointers);

			    pObjectInfo->Fcb->Header.AllocationSize.QuadPart  = pObjectInfo->AllocationSize.QuadPart;
			    pObjectInfo->Fcb->Header.FileSize.QuadPart        = pObjectInfo->EndOfFile.QuadPart;
			    pObjectInfo->Fcb->Header.ValidDataLength.QuadPart = pObjectInfo->EndOfFile.QuadPart;

			    if ( pCCFileObject != NULL)
			    {
				CcSetFileSizes( pCCFileObject,
						(PCC_FILE_SIZES)&pObjectInfo->Fcb->Header.AllocationSize);
			    }
			}
			__except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()))
                        {

			    ntStatus = GetExceptionCode();

			    AFSDbgTrace(( 0,
					  0,
					  "EXCEPTION - AFSValidateEntry CcSetFileSizes failed FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
					  pObjectInfo->FileId.Cell,
					  pObjectInfo->FileId.Volume,
					  pObjectInfo->FileId.Vnode,
					  pObjectInfo->FileId.Unique,
					  ntStatus));
                        }

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSValidateEntry Releasing Fcb SectionObject lock %p EXCL %08lX\n",
                                      &pObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                      PsGetCurrentThread()));

                        AFSReleaseResource( &pObjectInfo->Fcb->NPFcb->SectionObjectResource);
                    }
                }
                break;
            }

            case AFS_FILE_TYPE_DIRECTORY:
            {

                if( pObjectInfo->DataVersion.QuadPart != pDirEnumEntry->DataVersion.QuadPart)
                {

                    //
                    // For a directory or root entry flush the content of
                    // the directory enumeration.
                    //

                    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateEntry Acquiring DirectoryNodeHdr.TreeLock lock %p EXCL %08lX\n",
                                  pObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                  PsGetCurrentThread()));

                    if( BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_FLAGS_DIRECTORY_ENUMERATED))
                    {

                        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE_2,
                                      "AFSValidateEntry Validating directory content for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      &DirEntry->NameInformation.FileName,
                                      pObjectInfo->FileId.Cell,
                                      pObjectInfo->FileId.Volume,
                                      pObjectInfo->FileId.Vnode,
                                      pObjectInfo->FileId.Unique));

                        AFSAcquireExcl( pObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                        TRUE);

                        ntStatus = AFSValidateDirectoryCache( pObjectInfo,
                                                              AuthGroup);

                        AFSReleaseResource( pObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);
                    }

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSValidateEntry Failed to re-enumerate %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      &DirEntry->NameInformation.FileName,
                                      pObjectInfo->FileId.Cell,
                                      pObjectInfo->FileId.Volume,
                                      pObjectInfo->FileId.Vnode,
                                      pObjectInfo->FileId.Unique,
                                      ntStatus));

                        break;
                    }
                }

                //
                // Update the metadata for the entry
                //

                ntStatus = AFSUpdateMetaData( DirEntry,
                                              pDirEnumEntry);

                if( NT_SUCCESS( ntStatus))
                {

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSValidateEntry Directory FID %08lX-%08lX-%08lX-%08lX Clearing Verify Flag\n",
				  pObjectInfo->FileId.Cell,
				  pObjectInfo->FileId.Volume,
				  pObjectInfo->FileId.Vnode,
				  pObjectInfo->FileId.Unique));

                    ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY | AFS_OBJECT_FLAGS_NOT_EVALUATED);
                }

                break;
            }

            default:

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSValidateEntry Attempt to verify node of type %d FastCall %d %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              pObjectInfo->FileType,
                              FastCall,
                              &DirEntry->NameInformation.FileName,
                              pObjectInfo->FileId.Cell,
                              pObjectInfo->FileId.Volume,
                              pObjectInfo->FileId.Vnode,
                              pObjectInfo->FileId.Unique));

                break;
        }

 try_exit:

        if( bReleaseFcb)
        {

            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
        }

        if( pDirEnumEntry != NULL)
        {

            AFSExFreePoolWithTag( pDirEnumEntry, AFS_GENERIC_MEMORY_2_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSInitializeSpecialShareNameList()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB *pDirNode = NULL, *pLastDirNode = NULL;
    AFSObjectInfoCB *pObjectInfoCB = NULL;
    UNICODE_STRING uniShareName;
    ULONG ulEntryLength = 0;
    AFSNonPagedDirectoryCB *pNonPagedDirEntry = NULL;
    LONG lCount;

    __Enter
    {

        RtlInitUnicodeString( &uniShareName,
                              L"PIPE");

        pObjectInfoCB = AFSAllocateObjectInfo( &AFSGlobalRoot->ObjectInformation,
                                               0);

        if( pObjectInfoCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        lCount = AFSObjectInfoIncrement( pObjectInfoCB,
                                         AFS_OBJECT_REFERENCE_GLOBAL);

        AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitializeSpecialShareNameList (srvsvc) Increment count on object %p Cnt %d\n",
                      pObjectInfoCB,
                      lCount));

        pObjectInfoCB->FileType = (ULONG) AFS_FILE_TYPE_SPECIAL_SHARE_NAME;

        ulEntryLength = sizeof( AFSDirectoryCB) +
                                     uniShareName.Length;

        pDirNode = (AFSDirectoryCB *)AFSLibExAllocatePoolWithTag( PagedPool,
                                                                  ulEntryLength,
                                                                  AFS_DIR_ENTRY_TAG);

        if( pDirNode == NULL)
        {

            lCount = AFSObjectInfoDecrement( pObjectInfoCB,
                                             AFS_OBJECT_REFERENCE_GLOBAL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitializeSpecialShareNameList Decrement count on object %p Cnt %d\n",
                          pObjectInfoCB,
                          lCount));

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &pObjectInfoCB);
            }

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitializeSpecialShareNameList (srvsvc) AFS_DIR_ENTRY_TAG allocated %p\n",
                      pDirNode));

        pNonPagedDirEntry = (AFSNonPagedDirectoryCB *)AFSLibExAllocatePoolWithTag( NonPagedPool,
                                                                                   sizeof( AFSNonPagedDirectoryCB),
                                                                                   AFS_DIR_ENTRY_NP_TAG);

        if( pNonPagedDirEntry == NULL)
        {

	    AFSLibExFreePoolWithTag( pDirNode,
				     AFS_DIR_ENTRY_TAG);

            lCount = AFSObjectInfoDecrement( pObjectInfoCB,
                                             AFS_OBJECT_REFERENCE_GLOBAL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitializeSpecialShareNameList Decrement count on object %p Cnt %d\n",
                          pObjectInfoCB,
                          lCount));

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &pObjectInfoCB);
            }

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pDirNode,
                       ulEntryLength);

        RtlZeroMemory( pNonPagedDirEntry,
                       sizeof( AFSNonPagedDirectoryCB));

        ExInitializeResourceLite( &pNonPagedDirEntry->Lock);

        pDirNode->NonPaged = pNonPagedDirEntry;

        pDirNode->ObjectInformation = pObjectInfoCB;

        //
        // Set valid entry
        //

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_VALID | AFS_DIR_ENTRY_PIPE_SERVICE);

        pDirNode->NameInformation.FileName.Length = uniShareName.Length;

        pDirNode->NameInformation.FileName.MaximumLength = uniShareName.Length;

        pDirNode->NameInformation.FileName.Buffer = (WCHAR *)((char *)pDirNode + sizeof( AFSDirectoryCB));

        RtlCopyMemory( pDirNode->NameInformation.FileName.Buffer,
                       uniShareName.Buffer,
                       pDirNode->NameInformation.FileName.Length);

        pDirNode->CaseInsensitiveTreeEntry.HashIndex = AFSGenerateCRC( &pDirNode->NameInformation.FileName,
                                                                       TRUE);

        AFSSpecialShareNames = pDirNode;

        pLastDirNode = pDirNode;


        RtlInitUnicodeString( &uniShareName,
                              L"IPC$");

        pObjectInfoCB = AFSAllocateObjectInfo( &AFSGlobalRoot->ObjectInformation,
                                               0);

        if( pObjectInfoCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        lCount = AFSObjectInfoIncrement( pObjectInfoCB,
                                         AFS_OBJECT_REFERENCE_GLOBAL);

        AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitializeSpecialShareNameList (ipc$) Incrementing count on object %p Cnt %d\n",
                      pObjectInfoCB,
                      lCount));

        pObjectInfoCB->FileType = (ULONG) AFS_FILE_TYPE_SPECIAL_SHARE_NAME;

        ulEntryLength = sizeof( AFSDirectoryCB) +
                                     uniShareName.Length;

        pDirNode = (AFSDirectoryCB *)AFSLibExAllocatePoolWithTag( PagedPool,
                                                                  ulEntryLength,
                                                                  AFS_DIR_ENTRY_TAG);

        if( pDirNode == NULL)
        {

            lCount = AFSObjectInfoDecrement( pObjectInfoCB,
                                             AFS_OBJECT_REFERENCE_GLOBAL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitializeSpecialShareNameList Decrement count on object %p Cnt %d\n",
                          pObjectInfoCB,
                          lCount));

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &pObjectInfoCB);
            }

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitializeSpecialShareNameList (ipc$) AFS_DIR_ENTRY_TAG allocated %p\n",
                      pDirNode));

        pNonPagedDirEntry = (AFSNonPagedDirectoryCB *)AFSLibExAllocatePoolWithTag( NonPagedPool,
                                                                                   sizeof( AFSNonPagedDirectoryCB),
                                                                                   AFS_DIR_ENTRY_NP_TAG);

        if( pNonPagedDirEntry == NULL)
        {

	    AFSLibExFreePoolWithTag( pDirNode,
				     AFS_DIR_ENTRY_TAG);

            lCount = AFSObjectInfoDecrement( pObjectInfoCB,
                                             AFS_OBJECT_REFERENCE_GLOBAL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitializeSpecialShareNameList Decrement count on object %p Cnt %d\n",
                          pObjectInfoCB,
                          lCount));

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &pObjectInfoCB);
            }

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pDirNode,
                       ulEntryLength);

        RtlZeroMemory( pNonPagedDirEntry,
                       sizeof( AFSNonPagedDirectoryCB));

        ExInitializeResourceLite( &pNonPagedDirEntry->Lock);

        pDirNode->NonPaged = pNonPagedDirEntry;

        pDirNode->ObjectInformation = pObjectInfoCB;

        //
        // Set valid entry
        //

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_VALID | AFS_DIR_ENTRY_IPC);

        pDirNode->NameInformation.FileName.Length = uniShareName.Length;

        pDirNode->NameInformation.FileName.MaximumLength = uniShareName.Length;

        pDirNode->NameInformation.FileName.Buffer = (WCHAR *)((char *)pDirNode + sizeof( AFSDirectoryCB));

        RtlCopyMemory( pDirNode->NameInformation.FileName.Buffer,
                       uniShareName.Buffer,
                       pDirNode->NameInformation.FileName.Length);

        pDirNode->CaseInsensitiveTreeEntry.HashIndex = AFSGenerateCRC( &pDirNode->NameInformation.FileName,
                                                                       TRUE);

        pLastDirNode->ListEntry.fLink = pDirNode;

        pDirNode->ListEntry.bLink = pLastDirNode;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( AFSSpecialShareNames != NULL)
            {

                pDirNode = AFSSpecialShareNames;

                while( pDirNode != NULL)
                {

                    pLastDirNode = (AFSDirectoryCB *)pDirNode->ListEntry.fLink;

                    lCount = AFSObjectInfoDecrement( pDirNode->ObjectInformation,
                                                     AFS_OBJECT_REFERENCE_GLOBAL);

                    AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSInitializeSpecialShareNameList Decrement count on object %p Cnt %d\n",
                                  pDirNode->ObjectInformation,
                                  lCount));

                    if ( lCount == 0)
                    {

                        AFSDeleteObjectInfo( &pDirNode->ObjectInformation);
                    }

                    ExDeleteResourceLite( &pDirNode->NonPaged->Lock);

		    AFSLibExFreePoolWithTag( pDirNode->NonPaged,
					     AFS_DIR_ENTRY_NP_TAG);

		    AFSLibExFreePoolWithTag( pDirNode,
					     AFS_DIR_ENTRY_TAG);

                    pDirNode = pLastDirNode;
                }

                AFSSpecialShareNames = NULL;
            }
        }
    }

    return ntStatus;
}

AFSDirectoryCB *
AFSGetSpecialShareNameEntry( IN UNICODE_STRING *ShareName,
                             IN UNICODE_STRING *SecondaryName)
{

    AFSDirectoryCB *pDirectoryCB = NULL;
    ULONGLONG ullHash = 0;
    UNICODE_STRING uniFullShareName;

    __Enter
    {


        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSGetSpecialShareNameEntry share name %wZ secondary name %wZ\n",
                      ShareName,
                      SecondaryName));

        uniFullShareName = *ShareName;

        //
        // Generate our hash value
        //

        ullHash = AFSGenerateCRC( &uniFullShareName,
                                  TRUE);

        //
        // Loop through our special share names to see if this is one of them
        //

        pDirectoryCB = AFSSpecialShareNames;

        while( pDirectoryCB != NULL)
        {

            if( ullHash == pDirectoryCB->CaseInsensitiveTreeEntry.HashIndex)
            {

                break;
            }

            pDirectoryCB = (AFSDirectoryCB *)pDirectoryCB->ListEntry.fLink;
        }
    }

    return pDirectoryCB;
}

void
AFSWaitOnQueuedFlushes( IN AFSFcb *Fcb)
{

    //
    // Block on the queue flush event
    //

    KeWaitForSingleObject( &Fcb->NPFcb->Specific.File.QueuedFlushEvent,
                           Executive,
                           KernelMode,
                           FALSE,
                           NULL);

    return;
}

void
AFSWaitOnQueuedReleases()
{

    AFSDeviceExt *pRDRDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    //
    // Block on the queue flush event
    //

    KeWaitForSingleObject( &pRDRDeviceExt->Specific.RDR.QueuedReleaseExtentEvent,
                           Executive,
                           KernelMode,
                           FALSE,
                           NULL);

    return;
}

BOOLEAN
AFSIsEqualFID( IN AFSFileID *FileId1,
               IN AFSFileID *FileId2)
{

    BOOLEAN bIsEqual = FALSE;

    if( FileId1->Hash == FileId2->Hash &&
        FileId1->Unique == FileId2->Unique &&
        FileId1->Vnode == FileId2->Vnode &&
        FileId1->Volume == FileId2->Volume &&
        FileId1->Cell == FileId2->Cell)
    {

        bIsEqual = TRUE;
    }

    return bIsEqual;
}

NTSTATUS
AFSResetDirectoryContent( IN AFSObjectInfoCB *ObjectInfoCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB *pCurrentDirEntry = NULL, *pNextDirEntry = NULL;

    __Enter
    {

        ASSERT( ExIsResourceAcquiredExclusiveLite( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock));

        //
        // Reset the directory list information
        //

        pCurrentDirEntry = ObjectInfoCB->Specific.Directory.DirectoryNodeListHead;

        while( pCurrentDirEntry != NULL)
        {

            pNextDirEntry = (AFSDirectoryCB *)pCurrentDirEntry->ListEntry.fLink;

            if( pCurrentDirEntry->DirOpenReferenceCount <= 0 &&
                pCurrentDirEntry->NameArrayReferenceCount <= 0)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSResetDirectoryContent Deleting dir entry %p for %wZ\n",
                              pCurrentDirEntry,
                              &pCurrentDirEntry->NameInformation.FileName));

                AFSDeleteDirEntry( ObjectInfoCB,
                                   &pCurrentDirEntry);
            }
            else
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSResetDirectoryContent Setting DELETE flag in dir entry %p for %wZ\n",
                              pCurrentDirEntry,
                              &pCurrentDirEntry->NameInformation.FileName));

                SetFlag( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_DELETED);

                AFSRemoveNameEntry( ObjectInfoCB,
                                    pCurrentDirEntry);
            }

            pCurrentDirEntry = pNextDirEntry;
        }

        ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead = NULL;

        ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead = NULL;

        ObjectInfoCB->Specific.Directory.ShortNameTree = NULL;

        ObjectInfoCB->Specific.Directory.DirectoryNodeListHead = NULL;

        ObjectInfoCB->Specific.Directory.DirectoryNodeListTail = NULL;

        ObjectInfoCB->Specific.Directory.DirectoryNodeCount = 0;

        AFSDbgTrace(( AFS_SUBSYSTEM_DIR_NODE_COUNT,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSResetDirectoryContent Reset count to 0 on parent FID %08lX-%08lX-%08lX-%08lX\n",
                      ObjectInfoCB->FileId.Cell,
                      ObjectInfoCB->FileId.Volume,
                      ObjectInfoCB->FileId.Vnode,
                      ObjectInfoCB->FileId.Unique));
    }

    return ntStatus;
}

NTSTATUS
AFSEnumerateGlobalRoot( IN GUID *AuthGroup)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB *pDirGlobalDirNode = NULL;
    UNICODE_STRING uniFullName;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSEnumerateGlobalRoot Acquiring GlobalRoot DirectoryNodeHdr.TreeLock lock %p EXCL %08lX\n",
                      AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock,
                      PsGetCurrentThread()));

        AFSAcquireExcl( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        if( BooleanFlagOn( AFSGlobalRoot->ObjectInformation.Flags, AFS_OBJECT_FLAGS_DIRECTORY_ENUMERATED))
        {

            try_return( ntStatus);
        }

        //
        // Initialize the root information
        //

        AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.ContentIndex = 1;

        //
        // Enumerate the shares in the volume
        //

        ntStatus = AFSEnumerateDirectory( AuthGroup,
                                          &AFSGlobalRoot->ObjectInformation,
                                          TRUE);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        pDirGlobalDirNode = AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeListHead;

        uniFullName.MaximumLength = PAGE_SIZE;
        uniFullName.Length = 0;

        uniFullName.Buffer = (WCHAR *)AFSLibExAllocatePoolWithTag( PagedPool,
                                                                   uniFullName.MaximumLength,
                                                                   AFS_GENERIC_MEMORY_12_TAG);

        if( uniFullName.Buffer == NULL)
        {

            //
            // Reset the directory content
            //

            AFSResetDirectoryContent( &AFSGlobalRoot->ObjectInformation);

            ClearFlag( AFSGlobalRoot->ObjectInformation.Flags, AFS_OBJECT_FLAGS_DIRECTORY_ENUMERATED);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Populate our list of entries in the NP enumeration list
        //

        while( pDirGlobalDirNode != NULL)
        {

            uniFullName.Buffer[ 0] = L'\\';
            uniFullName.Buffer[ 1] = L'\\';

            uniFullName.Length = 2 * sizeof( WCHAR);

            RtlCopyMemory( &uniFullName.Buffer[ 2],
                           AFSServerName.Buffer,
                           AFSServerName.Length);

            uniFullName.Length += AFSServerName.Length;

            uniFullName.Buffer[ uniFullName.Length/sizeof( WCHAR)] = L'\\';

            uniFullName.Length += sizeof( WCHAR);

            RtlCopyMemory( &uniFullName.Buffer[ uniFullName.Length/sizeof( WCHAR)],
                           pDirGlobalDirNode->NameInformation.FileName.Buffer,
                           pDirGlobalDirNode->NameInformation.FileName.Length);

            uniFullName.Length += pDirGlobalDirNode->NameInformation.FileName.Length;

            AFSAddConnectionEx( &uniFullName,
                                RESOURCEDISPLAYTYPE_SHARE,
                                0);

            pDirGlobalDirNode = (AFSDirectoryCB *)pDirGlobalDirNode->ListEntry.fLink;
        }

	AFSLibExFreePoolWithTag( uniFullName.Buffer,
				 AFS_GENERIC_MEMORY_12_TAG);

try_exit:

        AFSReleaseResource( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock);
    }

    return ntStatus;
}

BOOLEAN
AFSIsRelativeName( IN UNICODE_STRING *Name)
{

    BOOLEAN bIsRelative = FALSE;

    if( Name->Length > 0 &&
        Name->Buffer[ 0] != L'\\')
    {

        bIsRelative = TRUE;
    }

    return bIsRelative;
}

BOOLEAN
AFSIsAbsoluteAFSName( IN UNICODE_STRING *Name)
{
    UNICODE_STRING uniTempName;
    BOOLEAN        bIsAbsolute = FALSE;

    //
    // An absolute AFS path must begin with \afs\... or equivalent
    //

    if ( Name->Length == 0 ||
         Name->Length <= AFSMountRootName.Length + sizeof( WCHAR) ||
         Name->Buffer[ 0] != L'\\' ||
         Name->Buffer[ AFSMountRootName.Length/sizeof( WCHAR)] != L'\\')
    {

        return FALSE;
    }

    uniTempName.Length = AFSMountRootName.Length;
    uniTempName.MaximumLength = AFSMountRootName.Length;

    uniTempName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                            uniTempName.MaximumLength,
                                                            AFS_NAME_BUFFER_TWO_TAG);

    if( uniTempName.Buffer == NULL)
    {

        return FALSE;
    }

    RtlCopyMemory( uniTempName.Buffer,
                   Name->Buffer,
                   AFSMountRootName.Length);

    bIsAbsolute = (0 == RtlCompareUnicodeString( &uniTempName,
                                                 &AFSMountRootName,
                                                 TRUE));

    AFSExFreePoolWithTag( uniTempName.Buffer,
                          AFS_NAME_BUFFER_TWO_TAG);

    return bIsAbsolute;
}


void
AFSUpdateName( IN UNICODE_STRING *Name)
{

    USHORT usIndex = 0;

    while( usIndex < Name->Length/sizeof( WCHAR))
    {

        if( Name->Buffer[ usIndex] == L'/')
        {

            Name->Buffer[ usIndex] = L'\\';
        }

        usIndex++;
    }

    return;
}

NTSTATUS
AFSUpdateTargetName( IN OUT UNICODE_STRING *TargetName,
                     IN OUT ULONG *Flags,
                     IN WCHAR *NameBuffer,
                     IN USHORT NameLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    WCHAR *pTmpBuffer = NULL;

    __Enter
    {

        //
        // If we have enough space then just move in the name otherwise
        // allocate a new buffer
        //

        if( TargetName->Length < NameLength)
        {

            pTmpBuffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                            NameLength,
                                                            AFS_NAME_BUFFER_FIVE_TAG);

            if( pTmpBuffer == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            if( BooleanFlagOn( *Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER))
            {

                AFSExFreePoolWithTag( TargetName->Buffer, AFS_NAME_BUFFER_FIVE_TAG);
            }

            TargetName->MaximumLength = NameLength;

            TargetName->Buffer = pTmpBuffer;

            SetFlag( *Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER);
        }

        TargetName->Length = NameLength;

        RtlCopyMemory( TargetName->Buffer,
                       NameBuffer,
                       TargetName->Length);

        //
        // Update the name in the buffer
        //

        AFSUpdateName( TargetName);

try_exit:

        NOTHING;
    }

    return ntStatus;
}

void
AFSSetEnumerationEvent( IN AFSFcb *Fcb)
{
    LONG lCount;

    //
    // Depending on the type of node, set the event
    //

    switch( Fcb->Header.NodeTypeCode)
    {

        case AFS_DIRECTORY_FCB:
        case AFS_ROOT_FCB:
        case AFS_ROOT_ALL:
        {

            lCount = InterlockedIncrement( &Fcb->NPFcb->Specific.Directory.DirectoryEnumCount);

            break;
        }
    }

    return;
}

void
AFSClearEnumerationEvent( IN AFSFcb *Fcb)
{

    LONG lCount;

    //
    // Depending on the type of node, set the event
    //

    switch( Fcb->Header.NodeTypeCode)
    {

        case AFS_DIRECTORY_FCB:
        case AFS_ROOT_FCB:
        case AFS_ROOT_ALL:
        {

            ASSERT( Fcb->NPFcb->Specific.Directory.DirectoryEnumCount > 0);

            lCount = InterlockedDecrement( &Fcb->NPFcb->Specific.Directory.DirectoryEnumCount);

            break;
        }
    }

    return;
}

BOOLEAN
AFSIsEnumerationInProcess( IN AFSObjectInfoCB *ObjectInfo)
{

    BOOLEAN bIsInProcess = FALSE;

    __Enter
    {

        if( ObjectInfo->Fcb == NULL)
        {

            try_return( bIsInProcess);
        }

        switch( ObjectInfo->Fcb->Header.NodeTypeCode)
        {

            case AFS_DIRECTORY_FCB:
            case AFS_ROOT_FCB:
            case AFS_ROOT_ALL:
            {

                if( ObjectInfo->Fcb->NPFcb->Specific.Directory.DirectoryEnumCount > 0)
                {

                    bIsInProcess = TRUE;
                }

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return bIsInProcess;
}

NTSTATUS
AFSVerifyVolume( IN ULONGLONG ProcessId,
                 IN AFSVolumeCB *VolumeCB)
{

    UNREFERENCED_PARAMETER(ProcessId);
    UNREFERENCED_PARAMETER(VolumeCB);
    NTSTATUS ntStatus = STATUS_SUCCESS;


    return ntStatus;
}

NTSTATUS
AFSInitPIOCtlDirectoryCB( IN AFSObjectInfoCB *ParentObjectInfo)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSObjectInfoCB *pObjectInfoCB = NULL;
    AFSDirectoryCB *pDirNode = NULL;
    ULONG ulEntryLength = 0;
    AFSNonPagedDirectoryCB *pNonPagedDirEntry = NULL;
    LONG lCount;

    __Enter
    {

        AFSAcquireExcl( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeLock,
                        TRUE);

        pObjectInfoCB = AFSAllocateObjectInfo( ParentObjectInfo,
                                               0);

        if( pObjectInfoCB == NULL)
        {

            AFSReleaseResource( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeLock);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        lCount = AFSObjectInfoIncrement( pObjectInfoCB,
                                         AFS_OBJECT_REFERENCE_PIOCTL);

        AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitPIOCtlDirectoryCB Increment count on object %p Cnt %d\n",
                      pObjectInfoCB,
                      lCount));

        AFSReleaseResource( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeLock);

        pObjectInfoCB->FileType = (ULONG) AFS_FILE_TYPE_PIOCTL;

        pObjectInfoCB->FileAttributes = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;

        ulEntryLength = sizeof( AFSDirectoryCB) + AFSPIOCtlName.Length;

        pDirNode = (AFSDirectoryCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                               ulEntryLength,
                                                               AFS_DIR_ENTRY_TAG);

        if( pDirNode == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitPIOCtlDirectoryCB AFS_DIR_ENTRY_TAG allocated %p\n",
                      pDirNode));

        pNonPagedDirEntry = (AFSNonPagedDirectoryCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                                                sizeof( AFSNonPagedDirectoryCB),
                                                                                AFS_DIR_ENTRY_NP_TAG);

        if( pNonPagedDirEntry == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pDirNode,
                       ulEntryLength);

        RtlZeroMemory( pNonPagedDirEntry,
                       sizeof( AFSNonPagedDirectoryCB));

        ExInitializeResourceLite( &pNonPagedDirEntry->Lock);

        pDirNode->NonPaged = pNonPagedDirEntry;

        pDirNode->ObjectInformation = pObjectInfoCB;

        pDirNode->FileIndex = (ULONG)AFS_DIR_ENTRY_PIOCTL_INDEX;

        //
        // Set valid entry
        //

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_VALID | AFS_DIR_ENTRY_FAKE);

        pDirNode->NameInformation.FileName.Length = AFSPIOCtlName.Length;

        pDirNode->NameInformation.FileName.MaximumLength = AFSPIOCtlName.Length;

        pDirNode->NameInformation.FileName.Buffer = (WCHAR *)((char *)pDirNode + sizeof( AFSDirectoryCB));

        RtlCopyMemory( pDirNode->NameInformation.FileName.Buffer,
                       AFSPIOCtlName.Buffer,
                       pDirNode->NameInformation.FileName.Length);

        pDirNode->CaseInsensitiveTreeEntry.HashIndex = AFSGenerateCRC( &pDirNode->NameInformation.FileName,
                                                                       TRUE);

        if ( InterlockedCompareExchangePointer( (PVOID *)&ParentObjectInfo->Specific.Directory.PIOCtlDirectoryCB, pDirNode, NULL) != NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSInitPIOCtlDirectoryCB Raced PIOCtlDirectoryCB %p pFcb %p\n",
                          ParentObjectInfo->Specific.Directory.PIOCtlDirectoryCB,
                          pDirNode));

            try_return( ntStatus = STATUS_REPARSE);
        }

try_exit:

        if ( ntStatus != STATUS_SUCCESS)
        {

            if ( pDirNode != NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInitPIOCtlDirectoryCB AFS_DIR_ENTRY_TAG deallocating %p\n",
                              pDirNode));

                AFSExFreePoolWithTag( pDirNode, AFS_DIR_ENTRY_TAG);
            }

            if( pNonPagedDirEntry != NULL)
            {

                ExDeleteResourceLite( &pNonPagedDirEntry->Lock);

                AFSExFreePoolWithTag( pNonPagedDirEntry, AFS_DIR_ENTRY_NP_TAG);
            }

            if ( pObjectInfoCB != NULL)
            {

                lCount = AFSObjectInfoDecrement( pObjectInfoCB,
                                                 AFS_OBJECT_REFERENCE_PIOCTL);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInitPIOCtlDirectoryCB Decrement count on object %p Cnt %d\n",
                              pObjectInfoCB,
                              lCount));

                if ( lCount == 0)
                {

                    AFSDeleteObjectInfo( &pObjectInfoCB);
                }
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSRetrieveFileAttributes( IN AFSDirectoryCB *ParentDirectoryCB,
                           IN AFSDirectoryCB *DirectoryCB,
                           IN UNICODE_STRING *ParentPathName,
                           IN AFSNameArrayHdr *RelatedNameArray,
                           IN GUID           *AuthGroup,
                           OUT AFSFileInfoCB *FileInfo)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEnumEntry *pDirEntry = NULL;
    UNICODE_STRING uniFullPathName = {0};
    AFSNameArrayHdr    *pNameArray = NULL;
    AFSVolumeCB *pVolumeCB = NULL;
    LONG VolumeReferenceReason = AFS_VOLUME_REFERENCE_INVALID;
    AFSVolumeCB *pNewVolumeCB = NULL;
    LONG NewVolumeReferenceReason = AFS_VOLUME_REFERENCE_INVALID;
    AFSDirectoryCB *pDirectoryEntry = NULL, *pParentDirEntry = NULL;
    AFSDirectoryCB *pNewParentDirEntry = NULL;
    WCHAR *pwchBuffer = NULL;
    UNICODE_STRING uniComponentName, uniRemainingPath, uniParsedName;
    ULONG ulNameDifference = 0;
    LONG lCount;
    UNICODE_STRING uniDFSTargetName;

    __Enter
    {

	uniDFSTargetName.Length = 0;
	uniDFSTargetName.MaximumLength = 0;
	uniDFSTargetName.Buffer = NULL;

	//
        // Retrieve a target name for the entry
        //

        AFSAcquireShared( &DirectoryCB->NonPaged->Lock,
                          TRUE);

        if( DirectoryCB->NameInformation.TargetName.Length == 0)
        {

            AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

            ntStatus = AFSEvaluateTargetByID( DirectoryCB->ObjectInformation,
                                              AuthGroup,
                                              FALSE,
                                              &pDirEntry);

            if( !NT_SUCCESS( ntStatus) ||
                pDirEntry->TargetNameLength == 0)
            {

                if( pDirEntry != NULL)
                {

                    ntStatus = STATUS_REPARSE_POINT_NOT_RESOLVED;
                }

                try_return( ntStatus);
            }

            AFSAcquireExcl( &DirectoryCB->NonPaged->Lock,
                            TRUE);

            if( DirectoryCB->NameInformation.TargetName.Length == 0)
            {

                //
                // Update the target name
                //

                ntStatus = AFSUpdateTargetName( &DirectoryCB->NameInformation.TargetName,
                                                &DirectoryCB->Flags,
                                                (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset),
                                                (USHORT)pDirEntry->TargetNameLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

                    try_return( ntStatus);
                }
            }

            AFSConvertToShared( &DirectoryCB->NonPaged->Lock);
        }

        //
        // Need to pass the full path in for parsing.
        //

        if( AFSIsRelativeName( &DirectoryCB->NameInformation.TargetName))
        {

            uniFullPathName.Length = 0;
            uniFullPathName.MaximumLength = ParentPathName->Length +
                                                    sizeof( WCHAR) +
                                                    DirectoryCB->NameInformation.TargetName.Length;

            uniFullPathName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                        uniFullPathName.MaximumLength,
                                                                        AFS_NAME_BUFFER_SIX_TAG);

            if( uniFullPathName.Buffer == NULL)
            {

                AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            pwchBuffer = uniFullPathName.Buffer;

            RtlZeroMemory( uniFullPathName.Buffer,
                           uniFullPathName.MaximumLength);

	    if ( ParentPathName->Length > 0)
	    {

		RtlCopyMemory( uniFullPathName.Buffer,
			       ParentPathName->Buffer,
			       ParentPathName->Length);

		uniFullPathName.Length = ParentPathName->Length;

		if( uniFullPathName.Buffer[ (uniFullPathName.Length/sizeof( WCHAR)) - 1] != L'\\' &&
		    DirectoryCB->NameInformation.TargetName.Buffer[ 0] != L'\\')
		{

		    uniFullPathName.Buffer[ uniFullPathName.Length/sizeof( WCHAR)] = L'\\';

		    uniFullPathName.Length += sizeof( WCHAR);
		}
	    }

	    RtlCopyMemory( &uniFullPathName.Buffer[ uniFullPathName.Length/sizeof( WCHAR)],
                           DirectoryCB->NameInformation.TargetName.Buffer,
                           DirectoryCB->NameInformation.TargetName.Length);

            uniFullPathName.Length += DirectoryCB->NameInformation.TargetName.Length;

            uniParsedName.Length = uniFullPathName.Length - ParentPathName->Length;
            uniParsedName.MaximumLength = uniParsedName.Length;

            uniParsedName.Buffer = &uniFullPathName.Buffer[ ParentPathName->Length/sizeof( WCHAR)];

            AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

            //
            // We populate up to the current parent
            //

            if( RelatedNameArray != NULL)
            {

                pNameArray = AFSInitNameArray( NULL,
                                               RelatedNameArray->MaxElementCount);

                if( pNameArray == NULL)
                {

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                ntStatus = AFSPopulateNameArrayFromRelatedArray( pNameArray,
                                                                 RelatedNameArray,
                                                                 ParentDirectoryCB);
            }
            else
            {

                pNameArray = AFSInitNameArray( NULL,
                                               0);

                if( pNameArray == NULL)
                {

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                ntStatus = AFSPopulateNameArray( pNameArray,
                                                 NULL,
                                                 ParentDirectoryCB);
            }

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }

            pVolumeCB = ParentDirectoryCB->ObjectInformation->VolumeCB;

            pParentDirEntry = ParentDirectoryCB;
        }
        else
        {

            uniFullPathName.Length = 0;
            uniFullPathName.MaximumLength = DirectoryCB->NameInformation.TargetName.Length;

            uniFullPathName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                        uniFullPathName.MaximumLength,
                                                                        AFS_NAME_BUFFER_SEVEN_TAG);

            if( uniFullPathName.Buffer == NULL)
            {

                AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            pwchBuffer = uniFullPathName.Buffer;

            RtlZeroMemory( uniFullPathName.Buffer,
                           uniFullPathName.MaximumLength);

            RtlCopyMemory( uniFullPathName.Buffer,
                           DirectoryCB->NameInformation.TargetName.Buffer,
                           DirectoryCB->NameInformation.TargetName.Length);

            uniFullPathName.Length = DirectoryCB->NameInformation.TargetName.Length;

            //
            // This name should begin with the \afs server so parse it off and check it
            //

            FsRtlDissectName( uniFullPathName,
                              &uniComponentName,
                              &uniRemainingPath);

            if( RtlCompareUnicodeString( &uniComponentName,
                                         &AFSServerName,
                                         TRUE) != 0)
            {

                AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSRetrieveFileAttributes Name %wZ contains invalid server name\n",
                              &uniFullPathName));

                try_return( ntStatus = STATUS_OBJECT_PATH_INVALID);
            }

            uniFullPathName = uniRemainingPath;

            uniParsedName = uniFullPathName;

            ulNameDifference = (ULONG)(uniFullPathName.Length > 0 ? ((char *)uniFullPathName.Buffer - (char *)pwchBuffer) : 0);

            AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

            //
            // Our name array
            //

            pNameArray = AFSInitNameArray( AFSGlobalRoot->DirectoryCB,
                                           0);

            if( pNameArray == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            pVolumeCB = AFSGlobalRoot;

            pParentDirEntry = AFSGlobalRoot->DirectoryCB;
        }

        //
        // Increment the ref count on the volume and dir entry for correct processing below
        //

        VolumeReferenceReason = AFS_VOLUME_REFERENCE_FILE_ATTRS;

        lCount = AFSVolumeIncrement( pVolumeCB,
                                     VolumeReferenceReason);

        AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRetrieveFileAttributes Increment count on volume %p Reason %u Cnt %d\n",
                      pVolumeCB,
                      VolumeReferenceReason,
                      lCount));

        lCount = InterlockedIncrement( &pParentDirEntry->DirOpenReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRetrieveFileAttributes Increment count on %wZ DE %p Ccb %p Cnt %d\n",
                      &pParentDirEntry->NameInformation.FileName,
                      pParentDirEntry,
                      NULL,
                      lCount));

        ntStatus = AFSLocateNameEntry( NULL,
                                       NULL,
                                       &uniFullPathName,
                                       &uniParsedName,
                                       pNameArray,
                                       AFS_LOCATE_FLAGS_NO_MP_TARGET_EVAL,
                                       pVolumeCB,
                                       pParentDirEntry,
                                       &pNewVolumeCB,
                                       &NewVolumeReferenceReason,
                                       &pNewParentDirEntry,
                                       &pDirectoryEntry,
                                       NULL,
				       &uniDFSTargetName);

        if ( pNewVolumeCB != NULL)
        {
            //
            // AFSLocateNameEntry returns pNewVolumeCB with a reference held
            // even if pVolumeCB == pNewVolumeCB.  It is always safe to release
            // the reference on pVolumeCB that was held prior to the call.
            // If pVolumeCB == pNewVolumeCB, the reference from AFSLocateNameEntry
            // will be released second.
            //

            lCount = AFSVolumeDecrement( pVolumeCB,
                                         VolumeReferenceReason);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRetrieveFileAttributes Decrement count on volume %p Reason %u Cnt %d\n",
                          pVolumeCB,
                          VolumeReferenceReason,
                          lCount));

            pVolumeCB = pNewVolumeCB;

            pNewVolumeCB = NULL;

            VolumeReferenceReason = NewVolumeReferenceReason;

            NewVolumeReferenceReason = AFS_VOLUME_REFERENCE_INVALID;
        }

        //
        // AFSLocateNameEntry does not alter the reference count of
        // pParentDirectoryCB and it returns pNewParentDirectoryCB with
        // a reference held.
        //

        lCount = InterlockedDecrement( &pParentDirEntry->DirOpenReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRetrieveFileAttributes DecrementX count on %wZ DE %p Cnt %d\n",
                      &pParentDirEntry->NameInformation.FileName,
                      pParentDirEntry,
                      lCount));

        pParentDirEntry = pNewParentDirEntry;

        pNewParentDirEntry = NULL;

	if( !NT_SUCCESS( ntStatus))
	{
	    try_return( ntStatus);
	}

	//
	// If the status is STATUS_REPARSE then attempt to retrieve the attributes from the target name returned
	//

	if( ntStatus == STATUS_REPARSE)
        {

	    ntStatus = AFSRetrieveTargetFileInfo( &uniDFSTargetName,
						  FileInfo);

            try_return( ntStatus);
        }

        //
        // Store off the information
        //

        FileInfo->FileAttributes = pDirectoryEntry->ObjectInformation->FileAttributes;

        //
        // Check for the mount point being returned
        //

        if( pDirectoryEntry->ObjectInformation->FileType == AFS_FILE_TYPE_MOUNTPOINT ||
            pDirectoryEntry->ObjectInformation->FileType == AFS_FILE_TYPE_DFSLINK)
        {

            FileInfo->FileAttributes |= (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
        }
        else if( pDirectoryEntry->ObjectInformation->FileType == AFS_FILE_TYPE_SYMLINK)
        {

            if ( FileInfo->FileAttributes == FILE_ATTRIBUTE_NORMAL)
            {

                FileInfo->FileAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
            }
            else
            {

                FileInfo->FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
            }
        }

        FileInfo->AllocationSize = pDirectoryEntry->ObjectInformation->AllocationSize;

        FileInfo->EndOfFile = pDirectoryEntry->ObjectInformation->EndOfFile;

        FileInfo->CreationTime = pDirectoryEntry->ObjectInformation->CreationTime;

        FileInfo->LastAccessTime = pDirectoryEntry->ObjectInformation->LastAccessTime;

        FileInfo->LastWriteTime = pDirectoryEntry->ObjectInformation->LastWriteTime;

        FileInfo->ChangeTime = pDirectoryEntry->ObjectInformation->ChangeTime;

try_exit:

        if( pDirEntry != NULL)
        {

            AFSExFreePoolWithTag( pDirEntry, AFS_GENERIC_MEMORY_2_TAG);
        }

        if( pDirectoryEntry != NULL)
        {

            lCount = InterlockedDecrement( &pDirectoryEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRetrieveFileAttributes Decrement1 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pDirectoryEntry->NameInformation.FileName,
                          pDirectoryEntry,
                          NULL,
                          lCount));

            ASSERT( lCount >= 0);
        }

        if ( pParentDirEntry != NULL)
        {

            lCount = InterlockedDecrement( &pParentDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRetrieveFileAttributes Decrement2 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pParentDirEntry->NameInformation.FileName,
                          pParentDirEntry,
                          NULL,
                          lCount));

            ASSERT( lCount >= 0);
        }

        if( pVolumeCB != NULL)
        {

            lCount = AFSVolumeDecrement( pVolumeCB,
                                         VolumeReferenceReason);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRetrieveFileAttributes Decrement2 count on volume %p Reason %u Cnt %d\n",
                          pVolumeCB,
                          VolumeReferenceReason,
                          lCount));
        }

        if( pNameArray != NULL)
        {

            AFSFreeNameArray( pNameArray);
        }

        if( pwchBuffer != NULL)
        {

            //
            // Always free the buffer that we allocated as AFSLocateNameEntry
            // will not free it.  If uniFullPathName.Buffer was allocated by
            // AFSLocateNameEntry, then we must free that as well.
            // Check that the uniFullPathName.Buffer in the string is not the same
            // offset by the length of the server name
            //

            if( uniFullPathName.Length > 0 &&
                pwchBuffer != (WCHAR *)((char *)uniFullPathName.Buffer - ulNameDifference))
            {

                AFSExFreePoolWithTag( uniFullPathName.Buffer, 0);
            }

            AFSExFreePoolWithTag( pwchBuffer, 0);
        }

	if ( uniDFSTargetName.Buffer != NULL)
	{

	    AFSExFreePoolWithTag( uniDFSTargetName.Buffer,
				  AFS_REPARSE_NAME_TAG);
	}
    }

    return ntStatus;
}

AFSObjectInfoCB *
AFSAllocateObjectInfo( IN AFSObjectInfoCB *ParentObjectInfo,
                       IN ULONGLONG HashIndex)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSObjectInfoCB *pObjectInfo = NULL;
    LONG lCount;

    __Enter
    {

        pObjectInfo = (AFSObjectInfoCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                   sizeof( AFSObjectInfoCB),
                                                                   AFS_OBJECT_INFO_TAG);

        if( pObjectInfo == NULL)
        {

            try_return( pObjectInfo);
        }

        RtlZeroMemory( pObjectInfo,
                       sizeof( AFSObjectInfoCB));

        pObjectInfo->NonPagedInfo = (AFSNonPagedObjectInfoCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                                                         sizeof( AFSNonPagedObjectInfoCB),
                                                                                         AFS_NP_OBJECT_INFO_TAG);

        if( pObjectInfo->NonPagedInfo == NULL)
        {

            AFSExFreePoolWithTag( pObjectInfo, AFS_OBJECT_INFO_TAG);

            try_return( pObjectInfo = NULL);
        }

        ExInitializeResourceLite( &pObjectInfo->NonPagedInfo->DirectoryNodeHdrLock);

        ExInitializeResourceLite( &pObjectInfo->NonPagedInfo->ObjectInfoLock);

        pObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock = &pObjectInfo->NonPagedInfo->DirectoryNodeHdrLock;

        if( ParentObjectInfo != NULL)
        {

            pObjectInfo->VolumeCB = ParentObjectInfo->VolumeCB;

            pObjectInfo->ParentFileId = ParentObjectInfo->FileId;

            SetFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_PARENT_FID);

            AFSAcquireShared( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeLock,
                              TRUE);

            lCount = AFSObjectInfoIncrement( ParentObjectInfo,
                                             AFS_OBJECT_REFERENCE_CHILD);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSAllocateObjectInfo Increment count on parent object %p Cnt %d\n",
                          ParentObjectInfo,
                          lCount));

            AFSReleaseResource( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeLock);
        }

        //
        // Initialize the access time
        //

        KeQueryTickCount( &pObjectInfo->LastAccessCount);

        if( HashIndex != 0)
        {

            ASSERT( ParentObjectInfo);

            //
            // Insert the entry into the object tree and list
            //

            pObjectInfo->TreeEntry.HashIndex = HashIndex;

            if( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeHead == NULL)
            {

                ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeHead = &pObjectInfo->TreeEntry;
            }
            else
            {

                ntStatus = AFSInsertHashEntry( ParentObjectInfo->VolumeCB->ObjectInfoTree.TreeHead,
                                               &pObjectInfo->TreeEntry);

                ASSERT( NT_SUCCESS( ntStatus));
            }

            //
            // And the object list in the volume
            //

            if( ParentObjectInfo->VolumeCB->ObjectInfoListHead == NULL)
            {

                ParentObjectInfo->VolumeCB->ObjectInfoListHead = pObjectInfo;
            }
            else
            {

                ParentObjectInfo->VolumeCB->ObjectInfoListTail->ListEntry.fLink = (void *)pObjectInfo;

                pObjectInfo->ListEntry.bLink = (void *)ParentObjectInfo->VolumeCB->ObjectInfoListTail;
            }

            ParentObjectInfo->VolumeCB->ObjectInfoListTail = pObjectInfo;

            //
            // Indicate the object is in the hash tree and linked list in the volume
            //

            SetFlag( pObjectInfo->Flags, AFS_OBJECT_INSERTED_HASH_TREE | AFS_OBJECT_INSERTED_VOLUME_LIST);
        }

try_exit:

        NOTHING;
    }

    return pObjectInfo;
}

LONG
AFSObjectInfoIncrement( IN AFSObjectInfoCB *ObjectInfo,
                        IN LONG Reason)
{

    LONG lCount;

    if ( ObjectInfo->ObjectReferenceCount == 0)
    {

        AFSAcquireExcl( &ObjectInfo->NonPagedInfo->ObjectInfoLock,
                        TRUE);

        lCount = InterlockedIncrement( &ObjectInfo->ObjectReferenceCount);
    }
    else
    {

        AFSAcquireShared( &ObjectInfo->NonPagedInfo->ObjectInfoLock,
                          TRUE);

        lCount = InterlockedIncrement( &ObjectInfo->ObjectReferenceCount);

        if ( lCount == 1)
        {

            AFSReleaseResource( &ObjectInfo->NonPagedInfo->ObjectInfoLock);

            AFSAcquireExcl( &ObjectInfo->NonPagedInfo->ObjectInfoLock,
                            TRUE);
        }
    }

    InterlockedIncrement( &ObjectInfo->ObjectReferences[ Reason]);

    AFSReleaseResource( &ObjectInfo->NonPagedInfo->ObjectInfoLock);

    return lCount;
}

LONG
AFSObjectInfoDecrement( IN AFSObjectInfoCB *ObjectInfo,
                        IN LONG Reason)
{

    LONG lCount, lCount2;

    AFSAcquireShared( &ObjectInfo->NonPagedInfo->ObjectInfoLock,
                      TRUE);

    lCount = InterlockedDecrement( &ObjectInfo->ObjectReferenceCount);

    if ( lCount == 0)
    {

        lCount = InterlockedIncrement( &ObjectInfo->ObjectReferenceCount);

        AFSReleaseResource(&ObjectInfo->NonPagedInfo->ObjectInfoLock);

        AFSAcquireExcl( &ObjectInfo->NonPagedInfo->ObjectInfoLock,
                        TRUE);

        lCount = InterlockedDecrement( &ObjectInfo->ObjectReferenceCount);
    }

    lCount2 = InterlockedDecrement( &ObjectInfo->ObjectReferences[ Reason]);

    ASSERT( lCount2 >= 0);

    AFSReleaseResource( &ObjectInfo->NonPagedInfo->ObjectInfoLock);

    return lCount;
}

AFSObjectInfoCB *
AFSFindObjectInfo( IN AFSVolumeCB *VolumeCB,
                   IN AFSFileID   *FileId,
                   IN BOOLEAN      bUpdateLastUse)
{
    DWORD            ntStatus = STATUS_SUCCESS;
    ULONGLONG        ullIndex;
    AFSObjectInfoCB *pObjectInfo = NULL;
    LONG             lCount;

    ullIndex = AFSCreateLowIndex( FileId);

    AFSAcquireShared( VolumeCB->ObjectInfoTree.TreeLock,
                      TRUE);

    if ( AFSIsEqualFID( &VolumeCB->ObjectInformation.FileId, FileId))
    {

        pObjectInfo = &VolumeCB->ObjectInformation;
    }
    else
    {

        ntStatus = AFSLocateHashEntry( VolumeCB->ObjectInfoTree.TreeHead,
                                       ullIndex,
                                       (AFSBTreeEntry **)&pObjectInfo);
    }

    if ( NT_SUCCESS( ntStatus)) {

        lCount = AFSObjectInfoIncrement( pObjectInfo,
                                         AFS_OBJECT_REFERENCE_FIND);

        AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSFindObjectInfo Decrement count on object %p Cnt %d\n",
                      pObjectInfo,
                      lCount));

        if ( bUpdateLastUse)
        {

            KeQueryTickCount( &pObjectInfo->LastAccessCount);
        }
    }

    AFSReleaseResource( VolumeCB->ObjectInfoTree.TreeLock);

    return pObjectInfo;
}

void
AFSReleaseObjectInfo( IN AFSObjectInfoCB **ppObjectInfo)
{
    LONG lCount;

    lCount = AFSObjectInfoDecrement( *ppObjectInfo,
                                     AFS_OBJECT_REFERENCE_FIND);

    AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSReleaseObjectInfo Decrement count on object %p Cnt %d\n",
                  *ppObjectInfo,
                  lCount));

    *ppObjectInfo = NULL;
}

void
AFSDeleteObjectInfo( IN AFSObjectInfoCB **ppObjectInfo)
{
    BOOLEAN bAcquiredTreeLock = FALSE;
    AFSObjectInfoCB *pObjectInfo = NULL;
    AFSVolumeCB * pVolume = NULL;
    BOOLEAN bHeldInService;
    AFSObjectInfoCB * pParentObjectInfo = NULL;
    AFSFileID FileId;
    LONG lCount;

    __Enter
    {
        if ( BooleanFlagOn( (*ppObjectInfo)->Flags, AFS_OBJECT_ROOT_VOLUME))
        {

            //
            // AFSDeleteObjectInfo should never be called on the ObjectInformationCB
            // embedded in the VolumeCB.
            //

            ASSERT( FALSE);

            return;
        }

        pVolume = (*ppObjectInfo)->VolumeCB;

        if( !ExIsResourceAcquiredExclusiveLite( pVolume->ObjectInfoTree.TreeLock))
        {

            ASSERT( !ExIsResourceAcquiredLite( pVolume->ObjectInfoTree.TreeLock));

            AFSAcquireExcl( pVolume->ObjectInfoTree.TreeLock,
                            TRUE);

            bAcquiredTreeLock = TRUE;
        }

        for ( lCount = 0; lCount < AFS_OBJECT_REFERENCE_MAX; lCount++)
        {

            ASSERT( (*ppObjectInfo)->ObjectReferences[ lCount] >= 0);
        }

        ASSERT( (*ppObjectInfo)->ObjectReferenceCount == 0);

        pObjectInfo = (AFSObjectInfoCB *) InterlockedCompareExchangePointer( (PVOID *)ppObjectInfo,
                                                                             NULL,
                                                                             *ppObjectInfo);

        if ( pObjectInfo == NULL)
        {

            try_return( NOTHING);
        }

        ASSERT( *ppObjectInfo == NULL);

        if ( BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_FLAGS_PARENT_FID))
        {

            pParentObjectInfo = AFSFindObjectInfo( pVolume,
                                                   &pObjectInfo->ParentFileId,
                                                   FALSE);

            if( pParentObjectInfo != NULL)
            {

                ClearFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_PARENT_FID);

                AFSAcquireShared( pParentObjectInfo->VolumeCB->ObjectInfoTree.TreeLock,
                                  TRUE);

                lCount = AFSObjectInfoDecrement( pParentObjectInfo,
                                                 AFS_OBJECT_REFERENCE_CHILD);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSDeleteObjectInfo Decrement count on parent object %p Cnt %d\n",
                              pParentObjectInfo,
                              lCount));

                AFSReleaseResource( pParentObjectInfo->VolumeCB->ObjectInfoTree.TreeLock);

                AFSReleaseObjectInfo( &pParentObjectInfo);
            }
        }

        //
        // Remove it from the tree and list if it was inserted
        //

        if( BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_INSERTED_HASH_TREE))
        {

            AFSRemoveHashEntry( &pVolume->ObjectInfoTree.TreeHead,
                                &pObjectInfo->TreeEntry);
        }

        if( BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_INSERTED_VOLUME_LIST))
        {

            if( pObjectInfo->ListEntry.fLink == NULL)
            {

                pVolume->ObjectInfoListTail = (AFSObjectInfoCB *)pObjectInfo->ListEntry.bLink;

                if( pVolume->ObjectInfoListTail != NULL)
                {

                    pVolume->ObjectInfoListTail->ListEntry.fLink = NULL;
                }
            }
            else
            {

                ((AFSObjectInfoCB *)(pObjectInfo->ListEntry.fLink))->ListEntry.bLink = pObjectInfo->ListEntry.bLink;
            }

            if( pObjectInfo->ListEntry.bLink == NULL)
            {

                pVolume->ObjectInfoListHead = (AFSObjectInfoCB *)pObjectInfo->ListEntry.fLink;

                if( pVolume->ObjectInfoListHead != NULL)
                {

                    pVolume->ObjectInfoListHead->ListEntry.bLink = NULL;
                }
            }
            else
            {

                ((AFSObjectInfoCB *)(pObjectInfo->ListEntry.bLink))->ListEntry.fLink = pObjectInfo->ListEntry.fLink;
            }
        }

        bHeldInService = BooleanFlagOn( pObjectInfo->Flags, AFS_OBJECT_HELD_IN_SERVICE);

        if( bHeldInService)
        {

            FileId = pObjectInfo->FileId;
        }

        ASSERT( pObjectInfo->ObjectReferenceCount == 0);

        ExDeleteResourceLite( &pObjectInfo->NonPagedInfo->ObjectInfoLock);

        ExDeleteResourceLite( &pObjectInfo->NonPagedInfo->DirectoryNodeHdrLock);

        AFSExFreePoolWithTag( pObjectInfo->NonPagedInfo, AFS_NP_OBJECT_INFO_TAG);

        AFSExFreePoolWithTag( pObjectInfo, AFS_OBJECT_INFO_TAG);

try_exit:

        if( bAcquiredTreeLock)
        {

            AFSReleaseResource( pVolume->ObjectInfoTree.TreeLock);
        }

        //
        // Release the fid in the service
        //

        if( bHeldInService)
        {

            AFSReleaseFid( &FileId);
        }
    }

    return;
}

NTSTATUS
AFSEvaluateRootEntry( IN AFSDirectoryCB *DirectoryCB,
                      OUT AFSDirectoryCB **TargetDirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEnumEntry *pDirEntry = NULL;
    UNICODE_STRING uniFullPathName = {0};
    AFSNameArrayHdr    *pNameArray = NULL;
    AFSVolumeCB *pVolumeCB = NULL;
    LONG VolumeReferenceReason = AFS_VOLUME_REFERENCE_INVALID;
    AFSVolumeCB *pNewVolumeCB = NULL;
    LONG NewVolumeReferenceReason = AFS_VOLUME_REFERENCE_INVALID;
    AFSDirectoryCB *pDirectoryEntry = NULL, *pParentDirEntry = NULL;
    AFSDirectoryCB *pNewParentDirEntry = NULL;
    WCHAR *pwchBuffer = NULL;
    UNICODE_STRING uniComponentName, uniRemainingPath, uniParsedName;
    ULONG ulNameDifference = 0;
    GUID    stAuthGroup;
    LONG lCount;

    __Enter
    {

        ntStatus = AFSRetrieveValidAuthGroup( NULL,
                                              DirectoryCB->ObjectInformation,
                                              FALSE,
                                              &stAuthGroup);

        if( !NT_SUCCESS( ntStatus))
        {
            try_return( ntStatus);
        }

        //
        // Retrieve a target name for the entry
        //

        AFSAcquireShared( &DirectoryCB->NonPaged->Lock,
                          TRUE);

        if( DirectoryCB->NameInformation.TargetName.Length == 0)
        {

            AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

            ntStatus = AFSEvaluateTargetByID( DirectoryCB->ObjectInformation,
                                              &stAuthGroup,
                                              FALSE,
                                              &pDirEntry);

            if( !NT_SUCCESS( ntStatus) ||
                pDirEntry->TargetNameLength == 0)
            {

                if( pDirEntry != NULL)
                {

                    ntStatus = STATUS_REPARSE_POINT_NOT_RESOLVED;
                }

                try_return( ntStatus);
            }

            AFSAcquireExcl( &DirectoryCB->NonPaged->Lock,
                            TRUE);

            if( DirectoryCB->NameInformation.TargetName.Length == 0)
            {

                //
                // Update the target name
                //

                ntStatus = AFSUpdateTargetName( &DirectoryCB->NameInformation.TargetName,
                                                &DirectoryCB->Flags,
                                                (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset),
                                                (USHORT)pDirEntry->TargetNameLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

                    try_return( ntStatus);
                }
            }

            AFSConvertToShared( &DirectoryCB->NonPaged->Lock);
        }

        //
        // Need to pass the full path in for parsing.
        //

        uniFullPathName.Length = 0;
        uniFullPathName.MaximumLength = DirectoryCB->NameInformation.TargetName.Length;

        uniFullPathName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                    uniFullPathName.MaximumLength,
                                                                    AFS_NAME_BUFFER_EIGHT_TAG);

        if( uniFullPathName.Buffer == NULL)
        {

            AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        pwchBuffer = uniFullPathName.Buffer;

        RtlZeroMemory( uniFullPathName.Buffer,
                       uniFullPathName.MaximumLength);

        RtlCopyMemory( uniFullPathName.Buffer,
                       DirectoryCB->NameInformation.TargetName.Buffer,
                       DirectoryCB->NameInformation.TargetName.Length);

        uniFullPathName.Length = DirectoryCB->NameInformation.TargetName.Length;

        //
        // This name should begin with the \afs server so parse it off and chech it
        //

        FsRtlDissectName( uniFullPathName,
                          &uniComponentName,
                          &uniRemainingPath);

        if( RtlCompareUnicodeString( &uniComponentName,
                                     &AFSServerName,
                                     TRUE) != 0)
        {

            //
            // Try evaluating the full path
            //

            uniFullPathName.Buffer = pwchBuffer;

            uniFullPathName.Length = DirectoryCB->NameInformation.TargetName.Length;

            uniFullPathName.MaximumLength = uniFullPathName.Length;
        }
        else
        {

            uniFullPathName = uniRemainingPath;
        }

        uniParsedName = uniFullPathName;

        ulNameDifference = (ULONG)(uniFullPathName.Length > 0 ? ((char *)uniFullPathName.Buffer - (char *)pwchBuffer) : 0);

        AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

        //
        // Our name array
        //

        pNameArray = AFSInitNameArray( AFSGlobalRoot->DirectoryCB,
                                       0);

        if( pNameArray == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        pVolumeCB = AFSGlobalRoot;

        pParentDirEntry = AFSGlobalRoot->DirectoryCB;

        VolumeReferenceReason = AFS_VOLUME_REFERENCE_EVAL_ROOT;

        lCount = AFSVolumeIncrement( pVolumeCB,
                                     VolumeReferenceReason);

        AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSEvaluateRootEntry Increment count on volume %p Reason %u Cnt %d\n",
                      pVolumeCB,
                      VolumeReferenceReason,
                      lCount));

        lCount = InterlockedIncrement( &pParentDirEntry->DirOpenReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSEvaluateRootEntry Increment count on %wZ DE %p Ccb %p Cnt %d\n",
                      &pParentDirEntry->NameInformation.FileName,
                      pParentDirEntry,
                      NULL,
                      lCount));

        ntStatus = AFSLocateNameEntry( NULL,
                                       NULL,
                                       &uniFullPathName,
                                       &uniParsedName,
                                       pNameArray,
                                       0,
                                       pVolumeCB,
                                       pParentDirEntry,
                                       &pNewVolumeCB,
                                       &VolumeReferenceReason,
                                       &pNewParentDirEntry,
                                       &pDirectoryEntry,
                                       NULL,
				       NULL);

        if ( pNewVolumeCB != NULL)
        {
            //
            // AFSLocateNameEntry returns pNewVolumeCB with a reference held
            // even if pVolumeCB == pNewVolumeCB.  It is always safe to release
            // the reference on pVolumeCB that was held prior to the call.
            // If pVolumeCB == pNewVolumeCB, the reference from AFSLocateNameEntry
            // will be released second.
            //

            lCount = AFSVolumeDecrement( pVolumeCB,
                                         VolumeReferenceReason);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSEvaluateRootEntry Decrement count on volume %p Reason %u Cnt %d\n",
                          pVolumeCB,
                          VolumeReferenceReason,
                          lCount));

            pVolumeCB = pNewVolumeCB;

            pNewVolumeCB = NULL;

            VolumeReferenceReason = NewVolumeReferenceReason;

            NewVolumeReferenceReason = AFS_VOLUME_REFERENCE_INVALID;
        }

        //
        // AFSLocateNameEntry does not alter the reference count of
        // pParentDirectoryCB and it returns pNewParentDirectoryCB with
        // a reference held.
        //

        lCount = InterlockedDecrement( &pParentDirEntry->DirOpenReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSEvaluateRootEntry DecrementX count on %wZ DE %p Cnt %d\n",
                      &pParentDirEntry->NameInformation.FileName,
                      pParentDirEntry,
                      lCount));

        pParentDirEntry = pNewParentDirEntry;

        pNewParentDirEntry = NULL;

        if( !NT_SUCCESS( ntStatus) ||
            ntStatus == STATUS_REPARSE)
        {

            pVolumeCB = NULL;

            try_return( ntStatus);
        }

        //
        // Pass back the target dir entry for this request
        // The caller must release the DirOpenReferenceCount
        //

        *TargetDirEntry = pDirectoryEntry;

        pDirectoryEntry = NULL;

try_exit:

        if( pDirectoryEntry != NULL)
        {

            lCount = InterlockedDecrement( &pDirectoryEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSEvaluateRootEntry Decrement1 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pDirectoryEntry->NameInformation.FileName,
                          pDirectoryEntry,
                          NULL,
                          lCount));

            ASSERT( lCount >= 0);
        }

        if ( pParentDirEntry != NULL)
        {

            lCount = InterlockedDecrement( &pParentDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSEvaluateRootEntry Decrement2 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pParentDirEntry->NameInformation.FileName,
                          pParentDirEntry,
                          NULL,
                          lCount));

            ASSERT( lCount >= 0);
        }

        if( pDirEntry != NULL)
        {

            AFSExFreePoolWithTag( pDirEntry, AFS_GENERIC_MEMORY_2_TAG);
        }

        if( pVolumeCB != NULL)
        {

            lCount = AFSVolumeDecrement( pVolumeCB,
                                         VolumeReferenceReason);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSEvaluateRootEntry Decrement2 count on volume %p Reason %u Cnt %d\n",
                          pVolumeCB,
                          VolumeReferenceReason,
                          lCount));
        }

        if( pNameArray != NULL)
        {

            AFSFreeNameArray( pNameArray);
        }

        if( pwchBuffer != NULL)
        {

            //
            // Always free the buffer that we allocated as AFSLocateNameEntry
            // will not free it.  If uniFullPathName.Buffer was allocated by
            // AFSLocateNameEntry, then we must free that as well.
            // Check that the uniFullPathName.Buffer in the string is not the same
            // offset by the length of the server name
            //

            if( uniFullPathName.Length > 0 &&
                pwchBuffer != (WCHAR *)((char *)uniFullPathName.Buffer - ulNameDifference))
            {

                AFSExFreePoolWithTag( uniFullPathName.Buffer, 0);
            }

            AFSExFreePoolWithTag( pwchBuffer, 0);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSCleanupFcb( IN AFSFcb *Fcb,
               IN BOOLEAN ForceFlush)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pRDRDeviceExt = NULL, *pControlDeviceExt = NULL;
    LARGE_INTEGER liTime;
    IO_STATUS_BLOCK stIoStatus;

    __Enter
    {

        pControlDeviceExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

        pRDRDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

        if( BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN))
        {

            if( !BooleanFlagOn( Fcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_OBJECT_INVALID) &&
                !BooleanFlagOn( Fcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_DELETED))
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
			      "AFSCleanupEntry Acquiring Fcb lock %p EXCL %08lX\n",
                              &Fcb->NPFcb->Resource,
                              PsGetCurrentThread()));

		AFSAcquireExcl( &Fcb->NPFcb->Resource,
                                  TRUE);

                if( Fcb->OpenReferenceCount > 0)
                {

		    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCleanupEntry Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
                                  &Fcb->NPFcb->SectionObjectResource,
                                  PsGetCurrentThread()));

                    AFSAcquireExcl( &Fcb->NPFcb->SectionObjectResource,
                                    TRUE);

                    __try
                    {

                        CcFlushCache( &Fcb->NPFcb->SectionObjectPointers,
                                      NULL,
                                      0,
                                      &stIoStatus);

                        if( !NT_SUCCESS( stIoStatus.Status))
                        {

                            AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSCleanupFcb CcFlushCache [1] failure FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX Bytes 0x%08lX\n",
                                          Fcb->ObjectInformation->FileId.Cell,
                                          Fcb->ObjectInformation->FileId.Volume,
                                          Fcb->ObjectInformation->FileId.Vnode,
                                          Fcb->ObjectInformation->FileId.Unique,
                                          stIoStatus.Status,
                                          stIoStatus.Information));

                            ntStatus = stIoStatus.Status;
                        }

                        if (  Fcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL)
                        {

                            if ( !CcPurgeCacheSection( &Fcb->NPFcb->SectionObjectPointers,
                                                       NULL,
                                                       0,
                                                       FALSE))
                            {

                                AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                              AFS_TRACE_LEVEL_WARNING,
                                              "AFSCleanupFcb CcPurgeCacheSection [1] failure FID %08lX-%08lX-%08lX-%08lX\n",
                                              Fcb->ObjectInformation->FileId.Cell,
                                              Fcb->ObjectInformation->FileId.Volume,
                                              Fcb->ObjectInformation->FileId.Vnode,
                                              Fcb->ObjectInformation->FileId.Unique));

                                SetFlag( Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
                            }
                        }
                    }
		    __except( EXCEPTION_EXECUTE_HANDLER)
                    {

                        ntStatus = GetExceptionCode();

                        AFSDbgTrace(( 0,
                                      0,
                                      "EXCEPTION - AFSCleanupFcb Cc [1] FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
                                      Fcb->ObjectInformation->FileId.Cell,
                                      Fcb->ObjectInformation->FileId.Volume,
                                      Fcb->ObjectInformation->FileId.Vnode,
                                      Fcb->ObjectInformation->FileId.Unique,
                                      ntStatus));

                        SetFlag( Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
                    }

		    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCleanupFcb Releasing Fcb SectionObject lock %p EXCL %08lX\n",
                                  &Fcb->NPFcb->SectionObjectResource,
                                  PsGetCurrentThread()));

                    AFSReleaseResource( &Fcb->NPFcb->SectionObjectResource);
                }

                AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
			      "AFSCleanupEntry Releasing Fcb lock %p EXCL %08lX\n",
                              &Fcb->NPFcb->Resource,
                              PsGetCurrentThread()));

                AFSReleaseResource( &Fcb->NPFcb->Resource);

		if( !BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_DIRECT_SERVICE_IO))
		{
		    //
		    // Wait for any currently running flush or release requests to complete
		    //

		    AFSWaitOnQueuedFlushes( Fcb);

		    //
		    // Now perform another flush on the file
		    //

		    if( !NT_SUCCESS( AFSFlushExtents( Fcb,
						      NULL)))
		    {

			AFSReleaseExtentsWithFlush( Fcb,
						    NULL,
						    TRUE);
		    }
		}
	    }

	    if( !BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_DIRECT_SERVICE_IO))
	    {

		if( Fcb->OpenReferenceCount == 0 ||
		    BooleanFlagOn( Fcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_OBJECT_INVALID) ||
		    BooleanFlagOn( Fcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_DELETED))
		{

		    AFSTearDownFcbExtents( Fcb,
					   NULL);
		}
	    }

            try_return( ntStatus);
        }

        KeQueryTickCount( &liTime);

	if( !BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_DIRECT_SERVICE_IO))
	{
	    //
	    // First up are there dirty extents in the cache to flush?
	    //

	    if( BooleanFlagOn( Fcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_OBJECT_INVALID) ||
		BooleanFlagOn( Fcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_DELETED))
	    {

		//
		// The file has been marked as invalid.  Dump it
		//

		AFSTearDownFcbExtents( Fcb,
				       NULL);
	    }
	    else if( ForceFlush ||
		     ( ( Fcb->Specific.File.ExtentsDirtyCount ||
			 Fcb->Specific.File.ExtentCount) &&
		       (liTime.QuadPart - Fcb->Specific.File.LastServerFlush.QuadPart)
		       >= pControlDeviceExt->Specific.Control.FcbFlushTimeCount.QuadPart))
	    {

		if( !NT_SUCCESS( AFSFlushExtents( Fcb,
						  NULL)) &&
		    Fcb->OpenReferenceCount == 0)
		{

		    AFSReleaseExtentsWithFlush( Fcb,
						NULL,
						TRUE);
		}
	    }
        }

        //
        // If there are extents and they haven't been used recently *and*
        // are not being used
        //

        if( ( ForceFlush ||
              ( 0 != Fcb->Specific.File.ExtentCount &&
                0 != Fcb->Specific.File.LastExtentAccess.QuadPart &&
                (liTime.QuadPart - Fcb->Specific.File.LastExtentAccess.QuadPart) >=
                                        (AFS_SERVER_PURGE_SLEEP * pControlDeviceExt->Specific.Control.FcbPurgeTimeCount.QuadPart))))
        {

	    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
			  AFS_TRACE_LEVEL_VERBOSE,
			  "AFSCleanupFcb Acquiring Fcb lock %p EXCL %08lX\n",
			  &Fcb->NPFcb->Resource,
			  PsGetCurrentThread()));

	    if ( AFSAcquireExcl( &Fcb->NPFcb->Resource, ForceFlush) == FALSE)
	    {

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
			      AFS_TRACE_LEVEL_VERBOSE,
			      "AFSCleanupFcb Failed to Acquire Fcb lock %p EXCL %08lX\n",
			      &Fcb->NPFcb->Resource,
			      PsGetCurrentThread()));

		try_return( ntStatus = STATUS_RETRY);
	    }

	    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCleanupFcb Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
                          &Fcb->NPFcb->SectionObjectResource,
                          PsGetCurrentThread()));

            if ( AFSAcquireExcl( &Fcb->NPFcb->SectionObjectResource, ForceFlush))
            {

                __try
                {

                    CcFlushCache( &Fcb->NPFcb->SectionObjectPointers,
                                  NULL,
                                  0,
                                  &stIoStatus);

                    if( !NT_SUCCESS( stIoStatus.Status))
                    {

                        AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSCleanupFcb CcFlushCache [2] failure FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX Bytes 0x%08lX\n",
                                      Fcb->ObjectInformation->FileId.Cell,
                                      Fcb->ObjectInformation->FileId.Volume,
                                      Fcb->ObjectInformation->FileId.Vnode,
                                      Fcb->ObjectInformation->FileId.Unique,
                                      stIoStatus.Status,
                                      stIoStatus.Information));

                        ntStatus = stIoStatus.Status;
                    }

                    if( ForceFlush &&
                        Fcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL)
                    {

                        if ( !CcPurgeCacheSection( &Fcb->NPFcb->SectionObjectPointers,
                                                   NULL,
                                                   0,
                                                   FALSE))
                        {

                            AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                          AFS_TRACE_LEVEL_WARNING,
                                          "AFSCleanupFcb CcPurgeCacheSection [2] failure FID %08lX-%08lX-%08lX-%08lX\n",
                                          Fcb->ObjectInformation->FileId.Cell,
                                          Fcb->ObjectInformation->FileId.Volume,
                                          Fcb->ObjectInformation->FileId.Vnode,
                                          Fcb->ObjectInformation->FileId.Unique));

                            SetFlag( Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
                        }
                    }
                }
		__except( EXCEPTION_EXECUTE_HANDLER)
                {

                    ntStatus = GetExceptionCode();

                    AFSDbgTrace(( 0,
                                  0,
                                  "EXCEPTION - AFSCleanupFcb Cc [2] FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
                                  Fcb->ObjectInformation->FileId.Cell,
                                  Fcb->ObjectInformation->FileId.Volume,
                                  Fcb->ObjectInformation->FileId.Vnode,
                                  Fcb->ObjectInformation->FileId.Unique,
                                  ntStatus));
                }

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCleanupFcb Releasing Fcb SectionObject lock %p EXCL %08lX\n",
                              &Fcb->NPFcb->SectionObjectResource,
                              PsGetCurrentThread()));

                AFSReleaseResource( &Fcb->NPFcb->SectionObjectResource);

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
			      AFS_TRACE_LEVEL_VERBOSE,
			      "AFSCleanupFcb Releasing Fcb lock %p EXCL %08lX\n",
			      &Fcb->NPFcb->Resource,
			      PsGetCurrentThread()));

		AFSReleaseResource( &Fcb->NPFcb->Resource);

		if( !BooleanFlagOn( pRDRDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_DIRECT_SERVICE_IO))
		{

		    if( Fcb->OpenReferenceCount <= 0)
		    {

			//
			// Tear em down we'll not be needing them again
			//

			AFSTearDownFcbExtents( Fcb,
					       NULL);
		    }
		}
            }
            else
            {

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
			      AFS_TRACE_LEVEL_VERBOSE,
			      "AFSCleanupFcb Failed to Acquire Fcb SectionObject lock %p EXCL %08lX\n",
			      &Fcb->NPFcb->SectionObjectResource,
			      PsGetCurrentThread()));

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
			      AFS_TRACE_LEVEL_VERBOSE,
			      "AFSCleanupFcb Releasing Fcb lock %p EXCL %08lX\n",
			      &Fcb->NPFcb->Resource,
			      PsGetCurrentThread()));

		AFSReleaseResource( &Fcb->NPFcb->Resource);

                ntStatus = STATUS_RETRY;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSUpdateDirEntryName( IN AFSDirectoryCB *DirectoryCB,
                       IN UNICODE_STRING *NewFileName)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    WCHAR *pTmpBuffer = NULL;

    __Enter
    {

        if( NewFileName->Length > DirectoryCB->NameInformation.FileName.Length)
        {

            if( BooleanFlagOn( DirectoryCB->Flags, AFS_DIR_RELEASE_NAME_BUFFER))
            {

                AFSExFreePoolWithTag( DirectoryCB->NameInformation.FileName.Buffer, 0);

                ClearFlag( DirectoryCB->Flags, AFS_DIR_RELEASE_NAME_BUFFER);

                DirectoryCB->NameInformation.FileName.Buffer = NULL;
            }

            //
            // OK, we need to allocate a new name buffer
            //

            pTmpBuffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                            NewFileName->Length,
                                                            AFS_NAME_BUFFER_NINE_TAG);

            if( pTmpBuffer == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            DirectoryCB->NameInformation.FileName.Buffer = pTmpBuffer;

            DirectoryCB->NameInformation.FileName.MaximumLength = NewFileName->Length;

            SetFlag( DirectoryCB->Flags, AFS_DIR_RELEASE_NAME_BUFFER);
        }

        DirectoryCB->NameInformation.FileName.Length = NewFileName->Length;

        RtlCopyMemory( DirectoryCB->NameInformation.FileName.Buffer,
                       NewFileName->Buffer,
                       NewFileName->Length);

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSReadCacheFile( IN void *ReadBuffer,
                  IN LARGE_INTEGER *ReadOffset,
                  IN ULONG RequestedDataLength,
                  IN OUT PULONG BytesRead)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PIRP                pIrp = NULL;
    KEVENT              kEvent;
    PIO_STACK_LOCATION  pIoStackLocation = NULL;
    DEVICE_OBJECT      *pTargetDeviceObject = NULL;
    FILE_OBJECT        *pCacheFileObject = NULL;

    __Enter
    {

        pCacheFileObject = AFSReferenceCacheFileObject();

        if( pCacheFileObject == NULL)
        {
            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        pTargetDeviceObject = IoGetRelatedDeviceObject( pCacheFileObject);

        //
        // Initialize the event
        //

        KeInitializeEvent( &kEvent,
                           SynchronizationEvent,
                           FALSE);

        //
        // Allocate an irp for this request.  This could also come from a
        // private pool, for instance.
        //

        pIrp = IoAllocateIrp( pTargetDeviceObject->StackSize,
                              FALSE);

        if( pIrp == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Build the IRP's main body
        //

        pIrp->UserBuffer = ReadBuffer;

        pIrp->Tail.Overlay.Thread = PsGetCurrentThread();
        pIrp->RequestorMode = KernelMode;
        pIrp->Flags |= IRP_READ_OPERATION;

        //
        // Set up the I/O stack location.
        //

        pIoStackLocation = IoGetNextIrpStackLocation( pIrp);
        pIoStackLocation->MajorFunction = IRP_MJ_READ;
        pIoStackLocation->DeviceObject = pTargetDeviceObject;
        pIoStackLocation->FileObject = pCacheFileObject;
        pIoStackLocation->Parameters.Read.Length = RequestedDataLength;

        pIoStackLocation->Parameters.Read.ByteOffset.QuadPart = ReadOffset->QuadPart;

        //
        // Set the completion routine.
        //

        IoSetCompletionRoutine( pIrp,
                                AFSIrpComplete,
                                &kEvent,
                                TRUE,
                                TRUE,
                                TRUE);

        //
        // Send it to the FSD
        //

        ntStatus = IoCallDriver( pTargetDeviceObject,
                                 pIrp);

        if( NT_SUCCESS( ntStatus))
        {

            //
            // Wait for the I/O
            //

            ntStatus = KeWaitForSingleObject( &kEvent,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              0);

            if( NT_SUCCESS( ntStatus))
            {

                ntStatus = pIrp->IoStatus.Status;

                *BytesRead = (ULONG)pIrp->IoStatus.Information;
            }
        }

try_exit:

        if( pCacheFileObject != NULL)
        {
            AFSReleaseCacheFileObject( pCacheFileObject);
        }

        if( pIrp != NULL)
        {

            if( pIrp->MdlAddress != NULL)
            {

                if( FlagOn( pIrp->MdlAddress->MdlFlags, MDL_PAGES_LOCKED))
                {

                    MmUnlockPages( pIrp->MdlAddress);
                }

                IoFreeMdl( pIrp->MdlAddress);
            }

            pIrp->MdlAddress = NULL;

            //
            // Free the Irp
            //

            IoFreeIrp( pIrp);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSIrpComplete( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP           Irp,
                IN PVOID          Context)
{

    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(DeviceObject);
    KEVENT *pEvent = (KEVENT *)Context;

    KeSetEvent( pEvent,
                0,
                FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

BOOLEAN
AFSIsDirectoryEmptyForDelete( IN AFSFcb *Fcb)
{

    BOOLEAN bIsEmpty = FALSE;
    AFSDirectoryCB *pDirEntry = NULL;

    __Enter
    {

        ASSERT( Fcb->ObjectInformation->FileType == AFS_FILE_TYPE_DIRECTORY);

        AFSAcquireShared( Fcb->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                          TRUE);

        bIsEmpty = TRUE;

        if( Fcb->ObjectInformation->Specific.Directory.DirectoryNodeListHead != NULL)
        {

            pDirEntry = Fcb->ObjectInformation->Specific.Directory.DirectoryNodeListHead;

            while( pDirEntry != NULL)
            {

                if( !BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_FAKE) &&
                    !BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_DELETED))
                {

                    bIsEmpty = FALSE;

                    break;
                }

                pDirEntry = (AFSDirectoryCB *)pDirEntry->ListEntry.fLink;
            }

        }

        AFSReleaseResource( Fcb->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);
    }

    return bIsEmpty;
}

void
AFSRemoveNameEntry( IN AFSObjectInfoCB *ParentObjectInfo,
                    IN AFSDirectoryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRemoveNameEntry DE %p for %wZ has NOT_IN flag set\n",
                          DirEntry,
                          &DirEntry->NameInformation.FileName));

            try_return( ntStatus);
        }

        ASSERT( ExIsResourceAcquiredExclusiveLite( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock));

        //
        // Remove the entry from the parent tree
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveNameEntry DE %p for %wZ removing from case sensitive tree\n",
                      DirEntry,
                      &DirEntry->NameInformation.FileName));

        AFSRemoveCaseSensitiveDirEntry( &ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                        DirEntry);

        AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveNameEntry DE %p for %wZ removing from case insensitive tree\n",
                      DirEntry,
                      &DirEntry->NameInformation.FileName));

        AFSRemoveCaseInsensitiveDirEntry( &ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                          DirEntry);

        if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME))
        {

            //
            // From the short name tree
            //

            AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRemoveNameEntry DE %p for %wZ removing from shortname tree\n",
                          DirEntry,
                          &DirEntry->NameInformation.FileName));

            AFSRemoveShortNameDirEntry( &ParentObjectInfo->Specific.Directory.ShortNameTree,
                                        DirEntry);

            ClearFlag( DirEntry->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveNameEntry DE %p for %wZ setting NOT_IN flag\n",
                      DirEntry,
                      &DirEntry->NameInformation.FileName));

        SetFlag( DirEntry->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE);

        ClearFlag( DirEntry->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);

try_exit:

        NOTHING;
    }

    return;
}

NTSTATUS
AFSGetAuthenticationId( OUT LARGE_INTEGER *pliAuthId)
{
    AFSWorkItem *pWorkItem = NULL;
    NTSTATUS ntStatus;

    __try
    {

	RtlZeroMemory( pliAuthId, sizeof(LARGE_INTEGER));

	pWorkItem = (AFSWorkItem *) AFSExAllocatePoolWithTag( NonPagedPool,
							      sizeof(AFSWorkItem),
							      AFS_WORK_ITEM_TAG);
	if (NULL == pWorkItem)
	{

	    AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
			  AFS_TRACE_LEVEL_ERROR,
			  "AFSGetAuthenticationId Failed to allocate work item\n"));

	    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES );
	}

	RtlZeroMemory( pWorkItem,
		       sizeof(AFSWorkItem));

	pWorkItem->Size = sizeof( AFSWorkItem);

	pWorkItem->RequestType = AFS_WORK_GET_AUTH_ID;

	pWorkItem->RequestFlags = AFS_SYNCHRONOUS_REQUEST;

	KeInitializeEvent(&pWorkItem->Event,
			  NotificationEvent,
			  FALSE);

	pWorkItem->Specific.GetAuthId.peProcess = PsGetCurrentProcess();

	pWorkItem->Specific.GetAuthId.peThread = PsGetCurrentThread();

	AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
		      AFS_TRACE_LEVEL_VERBOSE,
		      "AFSGetAuthenticationId Workitem %p\n",
		      pWorkItem));

	ntStatus = AFSQueueWorkerRequest( pWorkItem);

try_exit:

	if( NT_SUCCESS( ntStatus))
	{

	    *pliAuthId = pWorkItem->Specific.GetAuthId.AuthId;

	    ntStatus = pWorkItem->Status;

	    AFSDbgTrace(( AFS_SUBSYSTEM_WORKER_PROCESSING,
			  AFS_TRACE_LEVEL_VERBOSE,
			  "AFSGetAuthenticationId Request complete Status %08lX\n",
			  ntStatus));
	}
	else {

	    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
			  AFS_TRACE_LEVEL_ERROR,
			  "AFSGetAuthenticationId Failed to queue request Status %08lX\n",
			  ntStatus));
	}

	if( pWorkItem != NULL)
	{

	    AFSExFreePoolWithTag( pWorkItem,
				  AFS_WORK_ITEM_TAG);
	}
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

	AFSDbgTrace(( 0,
		      0,
		      "EXCEPTION - AFSGetAuthenticationId\n"));

	AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

NTSTATUS
AFSPerformGetAuthId( IN PEPROCESS peProcess,
		     IN PETHREAD peThread,
		     OUT LARGE_INTEGER *outAuthId)
{

    LARGE_INTEGER liAuthId = {0,0};
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PACCESS_TOKEN hToken = NULL;
    PTOKEN_STATISTICS pTokenInfo = NULL;
    BOOLEAN bCopyOnOpen = FALSE;
    BOOLEAN bEffectiveOnly = FALSE;
    BOOLEAN bPrimaryToken = FALSE;
    SECURITY_IMPERSONATION_LEVEL stImpersonationLevel;

    __Enter
    {

	hToken = PsReferenceImpersonationToken( peThread,
                                                &bCopyOnOpen,
                                                &bEffectiveOnly,
                                                &stImpersonationLevel);

        if( hToken == NULL)
        {

	    hToken = PsReferencePrimaryToken( peProcess);

            if( hToken == NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_ERROR,
			      "AFSPerformGetAuthId Failed to retrieve impersonation or primary token\n"));

                try_return( ntStatus);
            }

            bPrimaryToken = TRUE;
        }

        ntStatus = SeQueryInformationToken( hToken,
                                            TokenStatistics,
                                            (PVOID *)&pTokenInfo);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_ERROR,
			  "AFSPerformGetAuthId Failed to retrieve information Status %08lX\n",
                          ntStatus));

            try_return( ntStatus);
        }

        liAuthId.HighPart = pTokenInfo->AuthenticationId.HighPart;
        liAuthId.LowPart = pTokenInfo->AuthenticationId.LowPart;

        AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                      AFS_TRACE_LEVEL_VERBOSE,
		      "AFSPerformGetAuthId Successfully retrieved authentication ID %I64X\n",
                      liAuthId.QuadPart));

try_exit:

        if( hToken != NULL)
        {

            if( !bPrimaryToken)
            {

                PsDereferenceImpersonationToken( hToken);
            }
            else
            {

                PsDereferencePrimaryToken( hToken);
            }
        }

        if( pTokenInfo != NULL)
        {

            ExFreePool( pTokenInfo);    // Allocated by SeQueryInformationToken
        }

	*outAuthId = liAuthId;
    }

    return ntStatus;
}

void
AFSUnwindFileInfo( IN AFSFcb *Fcb,
                   IN AFSCcb *Ccb)
{

    UNREFERENCED_PARAMETER(Fcb);
    if( Ccb->FileUnwindInfo.FileAttributes != (ULONG)-1)
    {
        Fcb->ObjectInformation->FileAttributes = Ccb->FileUnwindInfo.FileAttributes;
    }

    if( Ccb->FileUnwindInfo.CreationTime.QuadPart != (ULONGLONG)-1)
    {
        Fcb->ObjectInformation->CreationTime.QuadPart = Ccb->FileUnwindInfo.CreationTime.QuadPart;
    }

    if( Ccb->FileUnwindInfo.LastAccessTime.QuadPart != (ULONGLONG)-1)
    {
        Fcb->ObjectInformation->LastAccessTime.QuadPart = Ccb->FileUnwindInfo.LastAccessTime.QuadPart;
    }

    if( Ccb->FileUnwindInfo.LastWriteTime.QuadPart != (ULONGLONG)-1)
    {
        Fcb->ObjectInformation->LastWriteTime.QuadPart = Ccb->FileUnwindInfo.LastWriteTime.QuadPart;
    }

    if( Ccb->FileUnwindInfo.ChangeTime.QuadPart != (ULONGLONG)-1)
    {
        Fcb->ObjectInformation->ChangeTime.QuadPart = Ccb->FileUnwindInfo.ChangeTime.QuadPart;
    }

    return;
}

BOOLEAN
AFSValidateDirList( IN AFSObjectInfoCB *ObjectInfo)
{

    BOOLEAN bIsValid = TRUE;
    ULONG ulCount = 0;
    AFSDirectoryCB *pCurrentDirEntry = NULL, *pDirEntry = NULL;

    pCurrentDirEntry = ObjectInfo->Specific.Directory.DirectoryNodeListHead;

    while( pCurrentDirEntry != NULL)
    {

        if( !BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_FAKE))
        {
            ulCount++;

            if( !BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE))
            {

                pDirEntry = NULL;

                AFSLocateCaseSensitiveDirEntry( ObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                (ULONG)pCurrentDirEntry->CaseSensitiveTreeEntry.HashIndex,
                                                &pDirEntry);

                if( pDirEntry == NULL)
                {
                    DbgBreakPoint();
                }
            }
        }

        pCurrentDirEntry = (AFSDirectoryCB *)pCurrentDirEntry->ListEntry.fLink;
    }

    if( ulCount != (ULONG) ObjectInfo->Specific.Directory.DirectoryNodeCount)
    {

        AFSPrint("AFSValidateDirList Count off Calc: %d Stored: %d\n",
                  ulCount,
                  ObjectInfo->Specific.Directory.DirectoryNodeCount);

        ObjectInfo->Specific.Directory.DirectoryNodeCount = ulCount;

        bIsValid = FALSE;
    }

    return bIsValid;
}

PFILE_OBJECT
AFSReferenceCacheFileObject()
{

    AFSDeviceExt       *pRdrDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    FILE_OBJECT        *pCacheFileObject = NULL;

    AFSAcquireShared( &pRdrDevExt->Specific.RDR.CacheFileLock,
                      TRUE);

    pCacheFileObject = pRdrDevExt->Specific.RDR.CacheFileObject;

    if( pCacheFileObject != NULL)
    {
        ObReferenceObject( pCacheFileObject);
    }

    AFSReleaseResource( &pRdrDevExt->Specific.RDR.CacheFileLock);

    return pCacheFileObject;
}

void
AFSReleaseCacheFileObject( IN PFILE_OBJECT CacheFileObject)
{

    ASSERT( CacheFileObject != NULL);

    ObDereferenceObject( CacheFileObject);

    return;
}

NTSTATUS
AFSInitializeLibrary( IN AFSLibraryInitCB *LibraryInit)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pControlDevExt = NULL;
    ULONG ulTimeIncrement = 0;
    LONG lCount;

    __Enter
    {

        AFSControlDeviceObject = LibraryInit->AFSControlDeviceObject;

        AFSRDRDeviceObject = LibraryInit->AFSRDRDeviceObject;

        AFSServerName = LibraryInit->AFSServerName;

        AFSMountRootName = LibraryInit->AFSMountRootName;

        AFSDebugFlags = LibraryInit->AFSDebugFlags;

        //
        // Callbacks in the framework
        //

        AFSProcessRequest = LibraryInit->AFSProcessRequest;

        AFSDbgLogMsg = LibraryInit->AFSDbgLogMsg;

        AFSDebugTraceFnc = AFSDbgLogMsg;

        AFSAddConnectionEx = LibraryInit->AFSAddConnectionEx;

        AFSExAllocatePoolWithTag = LibraryInit->AFSExAllocatePoolWithTag;

        AFSExFreePoolWithTag = LibraryInit->AFSExFreePoolWithTag;

        AFSDumpTraceFilesFnc = LibraryInit->AFSDumpTraceFiles;

        AFSRetrieveAuthGroupFnc = LibraryInit->AFSRetrieveAuthGroup;

        AFSLibCacheManagerCallbacks = LibraryInit->AFSCacheManagerCallbacks;

        if( LibraryInit->AFSCacheBaseAddress != NULL)
        {

            SetFlag( AFSLibControlFlags, AFS_REDIR_LIB_FLAGS_NONPERSISTENT_CACHE);

            AFSLibCacheBaseAddress = LibraryInit->AFSCacheBaseAddress;

            AFSLibCacheLength = LibraryInit->AFSCacheLength;
        }

        //
        // Initialize some flush parameters
        //

        pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;

        ulTimeIncrement = KeQueryTimeIncrement();

        pControlDevExt->Specific.Control.ObjectLifeTimeCount.QuadPart = (ULONGLONG)((ULONGLONG)AFS_OBJECT_LIFETIME / (ULONGLONG)ulTimeIncrement);
        pControlDevExt->Specific.Control.FcbPurgeTimeCount.QuadPart = AFS_SERVER_PURGE_DELAY;
        pControlDevExt->Specific.Control.FcbPurgeTimeCount.QuadPart /= ulTimeIncrement;
        pControlDevExt->Specific.Control.FcbFlushTimeCount.QuadPart = (ULONGLONG)((ULONGLONG)AFS_SERVER_FLUSH_DELAY / (ULONGLONG)ulTimeIncrement);
        pControlDevExt->Specific.Control.ExtentRequestTimeCount.QuadPart = (ULONGLONG)((ULONGLONG)AFS_EXTENT_REQUEST_TIME/(ULONGLONG)ulTimeIncrement);

        //
        // Initialize the global root entry
        //

        ntStatus = AFSInitVolume( NULL,
                                  &LibraryInit->GlobalRootFid,
                                  AFS_VOLUME_REFERENCE_GLOBAL_ROOT,
                                  &AFSGlobalRoot);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeLibrary AFSInitVolume failure %08lX\n",
                          ntStatus));

            try_return( ntStatus);
        }

        ntStatus = AFSInitRootFcb( (ULONGLONG)PsGetCurrentProcessId(),
                                   AFSGlobalRoot);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeLibrary AFSInitRootFcb failure %08lX\n",
                          ntStatus));

            lCount = AFSVolumeDecrement( AFSGlobalRoot,
                                         AFS_VOLUME_REFERENCE_GLOBAL_ROOT);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitializeLibrary Decrement count on volume %p Cnt %d\n",
                          AFSGlobalRoot,
                          lCount));

            AFSReleaseResource( AFSGlobalRoot->VolumeLock);

            try_return( ntStatus);
        }

        //
        // Update the node type code to AFS_ROOT_ALL
        //

        AFSGlobalRoot->ObjectInformation.Fcb->Header.NodeTypeCode = AFS_ROOT_ALL;

        SetFlag( AFSGlobalRoot->Flags, AFS_VOLUME_ACTIVE_GLOBAL_ROOT);

        //
        // Invalidate all known volumes since contact with the service and therefore
        // the file server was lost.
        //

        AFSInvalidateAllVolumes();

        //
        // Drop the locks acquired above
        //

        AFSInitVolumeWorker( AFSGlobalRoot);

        lCount = AFSVolumeDecrement( AFSGlobalRoot,
                                     AFS_VOLUME_REFERENCE_GLOBAL_ROOT);

        AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitializeLibrary Decrement count on volume %p Cnt %d\n",
                      AFSGlobalRoot,
                      lCount));

        AFSReleaseResource( AFSGlobalRoot->VolumeLock);

        AFSReleaseResource( AFSGlobalRoot->ObjectInformation.Fcb->Header.Resource);

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSCloseLibrary()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB *pDirNode = NULL, *pLastDirNode = NULL;
    LONG lCount;

    __Enter
    {

        if( AFSGlobalDotDirEntry != NULL)
        {

            lCount = AFSObjectInfoDecrement( AFSGlobalDotDirEntry->ObjectInformation,
                                             AFS_OBJECT_REFERENCE_GLOBAL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCloseLibrary Decrement count on parent object %p Cnt %d\n",
                          AFSGlobalDotDirEntry->ObjectInformation,
                          lCount));

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &AFSGlobalDotDirEntry->ObjectInformation);
            }

            ExDeleteResourceLite( &AFSGlobalDotDirEntry->NonPaged->Lock);

	    AFSLibExFreePoolWithTag( AFSGlobalDotDirEntry->NonPaged,
				     AFS_DIR_ENTRY_NP_TAG);

	    AFSLibExFreePoolWithTag( AFSGlobalDotDirEntry,
				     AFS_DIR_ENTRY_TAG);

            AFSGlobalDotDirEntry = NULL;
        }

        if( AFSGlobalDotDotDirEntry != NULL)
        {

            lCount = AFSObjectInfoDecrement( AFSGlobalDotDotDirEntry->ObjectInformation,
                                             AFS_OBJECT_REFERENCE_GLOBAL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCloseLibrary Decrement count on parent object %p Cnt %d\n",
                          AFSGlobalDotDotDirEntry->ObjectInformation,
                          lCount));

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &AFSGlobalDotDotDirEntry->ObjectInformation);
            }

            ExDeleteResourceLite( &AFSGlobalDotDotDirEntry->NonPaged->Lock);

	    AFSLibExFreePoolWithTag( AFSGlobalDotDotDirEntry->NonPaged,
				  AFS_DIR_ENTRY_NP_TAG);

	    AFSLibExFreePoolWithTag( AFSGlobalDotDotDirEntry,
				  AFS_DIR_ENTRY_TAG);

            AFSGlobalDotDotDirEntry = NULL;
        }

        if( AFSSpecialShareNames != NULL)
        {

            pDirNode = AFSSpecialShareNames;

            while( pDirNode != NULL)
            {

                pLastDirNode = (AFSDirectoryCB *)pDirNode->ListEntry.fLink;

                lCount = AFSObjectInfoDecrement( pDirNode->ObjectInformation,
                                                 AFS_OBJECT_REFERENCE_GLOBAL);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCloseLibrary Decrement count on parent object %p Cnt %d\n",
                              pDirNode->ObjectInformation,
                              lCount));

                if ( lCount == 0)
                {

                    AFSDeleteObjectInfo( &pDirNode->ObjectInformation);
                }

                ExDeleteResourceLite( &pDirNode->NonPaged->Lock);

		AFSLibExFreePoolWithTag( pDirNode->NonPaged,
					 AFS_DIR_ENTRY_NP_TAG);

		AFSLibExFreePoolWithTag( pDirNode,
					 AFS_DIR_ENTRY_TAG);

                pDirNode = pLastDirNode;
            }

            AFSSpecialShareNames = NULL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSDefaultLogMsg( IN ULONG Subsystem,
                  IN ULONG Level,
                  IN PCCH Format,
                  ...)
{

    UNREFERENCED_PARAMETER(Subsystem);
    UNREFERENCED_PARAMETER(Level);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    va_list va_args;
    char chDebugBuffer[ 256];

    __Enter
    {

        va_start( va_args, Format);

        ntStatus = RtlStringCbVPrintfA( chDebugBuffer,
                                        256,
                                        Format,
                                        va_args);

        if( NT_SUCCESS( ntStatus))
        {
            DbgPrint( chDebugBuffer);
        }

        va_end( va_args);
    }

    return ntStatus;
}

NTSTATUS
AFSGetObjectStatus( IN AFSGetStatusInfoCB *GetStatusInfo,
                    IN ULONG InputBufferLength,
                    IN AFSStatusInfoCB *StatusInfo,
                    OUT ULONG *ReturnLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSVolumeCB *pVolumeCB = NULL;
    LONG VolumeReferenceReason = AFS_VOLUME_REFERENCE_INVALID;
    AFSVolumeCB *pNewVolumeCB = NULL;
    LONG NewVolumeReferenceReason = AFS_VOLUME_REFERENCE_INVALID;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    AFSObjectInfoCB *pObjectInfo = NULL;
    ULONGLONG   ullIndex = 0;
    UNICODE_STRING uniFullPathName, uniRemainingPath, uniComponentName, uniParsedName;
    AFSNameArrayHdr *pNameArray = NULL;
    AFSDirectoryCB *pDirectoryEntry = NULL, *pParentDirEntry = NULL;
    AFSDirectoryCB *pNewParentDirEntry = NULL;
    LONG lCount;

    __Enter
    {

        //
        // If we are given a FID then look up the entry by that, otherwise
        // do it by name
        //

        if( GetStatusInfo->FileID.Cell != 0 &&
            GetStatusInfo->FileID.Volume != 0 &&
            GetStatusInfo->FileID.Vnode != 0 &&
            GetStatusInfo->FileID.Unique != 0)
        {

            AFSAcquireShared( &pDevExt->Specific.RDR.VolumeTreeLock, TRUE);

            //
            // Locate the volume node
            //

            ullIndex = AFSCreateHighIndex( &GetStatusInfo->FileID);

            ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                           ullIndex,
                                           (AFSBTreeEntry **)&pVolumeCB);

            if( pVolumeCB != NULL)
            {

                VolumeReferenceReason = AFS_VOLUME_REFERENCE_GET_OBJECT;

                lCount = AFSVolumeIncrement( pVolumeCB,
                                             VolumeReferenceReason);

                AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSGetObjectStatus Increment count on volume %p Reason %u Cnt %d\n",
                              pVolumeCB,
                              VolumeReferenceReason,
                              lCount));
            }

            AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

            if( !NT_SUCCESS( ntStatus) ||
                pVolumeCB == NULL)
            {
                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }

            if( AFSIsVolumeFID( &GetStatusInfo->FileID))
            {

                pObjectInfo = &pVolumeCB->ObjectInformation;

                AFSAcquireShared( pVolumeCB->ObjectInfoTree.TreeLock,
                                  TRUE);

                lCount = AFSObjectInfoIncrement( pObjectInfo,
                                                 AFS_OBJECT_REFERENCE_STATUS);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSGetObjectStatus Increment1 count on object %p Cnt %d\n",
                              pObjectInfo,
                              lCount));

                AFSReleaseResource( pVolumeCB->ObjectInfoTree.TreeLock);
            }
            else
            {

                AFSAcquireExcl( pVolumeCB->ObjectInfoTree.TreeLock,
                                TRUE);

                ullIndex = AFSCreateLowIndex( &GetStatusInfo->FileID);

                ntStatus = AFSLocateHashEntry( pVolumeCB->ObjectInfoTree.TreeHead,
                                               ullIndex,
                                               (AFSBTreeEntry **)&pObjectInfo);

                if( pObjectInfo != NULL)
                {

                    //
                    // Reference the node so it won't be torn down
                    //

                    lCount = AFSObjectInfoIncrement( pObjectInfo,
                                                     AFS_OBJECT_REFERENCE_STATUS);

                    AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSGetObjectStatus Increment2 count on object %p Cnt %d\n",
                                  pObjectInfo,
                                  lCount));

                    KeQueryTickCount( &pObjectInfo->LastAccessCount);
                }

                AFSReleaseResource( pVolumeCB->ObjectInfoTree.TreeLock);

                if( !NT_SUCCESS( ntStatus) ||
                    pObjectInfo == NULL)
                {
                    try_return( ntStatus = STATUS_INVALID_PARAMETER);
                }
            }
        }
        else
        {

            if( GetStatusInfo->FileNameLength == 0 ||
                InputBufferLength < (ULONG)(FIELD_OFFSET( AFSGetStatusInfoCB, FileName) + GetStatusInfo->FileNameLength))
            {
                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }

            uniFullPathName.Length = GetStatusInfo->FileNameLength;
            uniFullPathName.MaximumLength = uniFullPathName.Length;

            uniFullPathName.Buffer = (WCHAR *)GetStatusInfo->FileName;

            //
            // This name should begin with the \afs server so parse it off and check it
            //

            FsRtlDissectName( uniFullPathName,
                              &uniComponentName,
                              &uniRemainingPath);

            if( RtlCompareUnicodeString( &uniComponentName,
                                         &AFSServerName,
                                         TRUE) != 0)
            {
                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSGetObjectStatus Name %wZ contains invalid server name\n",
                              &uniFullPathName));

                try_return( ntStatus = STATUS_OBJECT_PATH_INVALID);
            }

            uniFullPathName = uniRemainingPath;

            uniParsedName = uniFullPathName;

            //
            // Our name array
            //

            pNameArray = AFSInitNameArray( AFSGlobalRoot->DirectoryCB,
                                           0);

            if( pNameArray == NULL)
            {
                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            pVolumeCB = AFSGlobalRoot;

            pParentDirEntry = AFSGlobalRoot->DirectoryCB;

            //
            // Increment the ref count on the volume and dir entry for correct processing below
            //

            VolumeReferenceReason = AFS_VOLUME_REFERENCE_GET_OBJECT;

            lCount = AFSVolumeIncrement( pVolumeCB,
                                         VolumeReferenceReason);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetObjectStatus Increment2 count on volume %p Reason %u Cnt %d\n",
                          pVolumeCB,
                          VolumeReferenceReason,
                          lCount));

            lCount = InterlockedIncrement( &pParentDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetObjectStatus Increment count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pParentDirEntry->NameInformation.FileName,
                          pParentDirEntry,
                          NULL,
                          lCount));

            ntStatus = AFSLocateNameEntry( NULL,
                                           NULL,
                                           &uniFullPathName,
                                           &uniParsedName,
                                           pNameArray,
                                           AFS_LOCATE_FLAGS_NO_MP_TARGET_EVAL |
                                               AFS_LOCATE_FLAGS_NO_SL_TARGET_EVAL,
                                           pVolumeCB,
                                           pParentDirEntry,
                                           &pNewVolumeCB,
                                           &NewVolumeReferenceReason,
                                           &pNewParentDirEntry,
                                           &pDirectoryEntry,
                                           NULL,
					   NULL);

            if ( pNewVolumeCB != NULL)
            {

                //
                // AFSLocateNameEntry returns pNewVolumeCB with a reference held
                // even if pVolumeCB == pNewVolumeCB.  It is always safe to release
                // the reference on pVolumeCB that was held prior to the call.
                // If pVolumeCB == pNewVolumeCB, the reference from AFSLocateNameEntry
                // will be released second.
                //

                lCount = AFSVolumeDecrement( pVolumeCB,
                                             VolumeReferenceReason);

                AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSGetObjectStatus Decrement count on volume %p Reason %u Cnt %d\n",
                              pVolumeCB,
                              VolumeReferenceReason,
                              lCount));

                pVolumeCB = pNewVolumeCB;

                pNewVolumeCB = NULL;

                VolumeReferenceReason = NewVolumeReferenceReason;

                NewVolumeReferenceReason = AFS_VOLUME_REFERENCE_INVALID;
            }

            //
            // AFSLocateNameEntry does not alter the reference count of
            // pParentDirectoryCB and it returns pNewParentDirectoryCB with
            // a reference held.
            //

            lCount = InterlockedDecrement( &pParentDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetObjectStatus DecrementX count on %wZ DE %p Cnt %d\n",
                          &pParentDirEntry->NameInformation.FileName,
                          pParentDirEntry,
                          lCount));

            pParentDirEntry = pNewParentDirEntry;

            pNewParentDirEntry = NULL;

            if( !NT_SUCCESS( ntStatus) ||
                ntStatus == STATUS_REPARSE)
            {

                pVolumeCB = NULL;

                try_return( ntStatus);
            }

            pObjectInfo = pDirectoryEntry->ObjectInformation;

            AFSAcquireExcl( pObjectInfo->VolumeCB->ObjectInfoTree.TreeLock,
                            TRUE);

            lCount = AFSObjectInfoIncrement( pObjectInfo,
                                             AFS_OBJECT_REFERENCE_STATUS);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetObjectStatus Increment3 count on object %p Cnt %d\n",
                          pObjectInfo,
                          lCount));

            AFSReleaseResource( pObjectInfo->VolumeCB->ObjectInfoTree.TreeLock);
        }

        //
        // At this point we have an object info block, return the information
        //

        StatusInfo->FileId = pObjectInfo->FileId;

        StatusInfo->TargetFileId = pObjectInfo->TargetFileId;

        StatusInfo->Expiration = pObjectInfo->Expiration;

        StatusInfo->DataVersion = pObjectInfo->DataVersion;

        StatusInfo->FileType = pObjectInfo->FileType;

        StatusInfo->ObjectFlags = pObjectInfo->Flags;

        StatusInfo->CreationTime = pObjectInfo->CreationTime;

        StatusInfo->LastAccessTime = pObjectInfo->LastAccessTime;

        StatusInfo->LastWriteTime = pObjectInfo->LastWriteTime;

        StatusInfo->ChangeTime = pObjectInfo->ChangeTime;

        StatusInfo->FileAttributes = pObjectInfo->FileAttributes;

        StatusInfo->EndOfFile = pObjectInfo->EndOfFile;

        StatusInfo->AllocationSize = pObjectInfo->AllocationSize;

        StatusInfo->EaSize = pObjectInfo->EaSize;

        StatusInfo->Links = pObjectInfo->Links;

        //
        // Return the information length
        //

        *ReturnLength = sizeof( AFSStatusInfoCB);

try_exit:

        if( pDirectoryEntry != NULL)
        {

            lCount = InterlockedDecrement( &pDirectoryEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetObjectStatus Decrement1 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pDirectoryEntry->NameInformation.FileName,
                          pDirectoryEntry,
                          NULL,
                          lCount));

            ASSERT( lCount >= 0);
        }

        if ( pParentDirEntry != NULL)
        {

            lCount = InterlockedDecrement( &pParentDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetObjectStatus Decrement2 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pParentDirEntry->NameInformation.FileName,
                          pParentDirEntry,
                          NULL,
                          lCount));

            ASSERT( lCount >= 0);
        }

        if( pVolumeCB != NULL)
        {

            if( pObjectInfo != NULL)
            {

                AFSAcquireShared( pVolumeCB->ObjectInfoTree.TreeLock,
                                  TRUE);

                lCount = AFSObjectInfoDecrement( pObjectInfo,
                                                 AFS_OBJECT_REFERENCE_STATUS);

                AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSGetObjectStatus Decrement count on object %p Cnt %d\n",
                              pObjectInfo,
                              lCount));

                AFSReleaseResource( pVolumeCB->ObjectInfoTree.TreeLock);
            }

            lCount = AFSVolumeDecrement( pVolumeCB,
                                         VolumeReferenceReason);

            AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetObjectStatus Decrement4 count on volume %p Reason %u Cnt %d\n",
                          pVolumeCB,
                          VolumeReferenceReason,
                          lCount));
        }

        if( pNameArray != NULL)
        {

            AFSFreeNameArray( pNameArray);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSCheckSymlinkAccess( IN AFSDirectoryCB *ParentDirectoryCB,
                       IN UNICODE_STRING *ComponentName)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSDirectoryCB *pDirEntry = NULL;
    ULONG ulCRC = 0;
    LONG lCount;

    __Enter
    {

        //
        // Search for the entry in the parent
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSCheckSymlinkAccess Searching for entry %wZ case sensitive\n",
                      ComponentName));

        ulCRC = AFSGenerateCRC( ComponentName,
                                FALSE);

        AFSAcquireShared( ParentDirectoryCB->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                          TRUE);

        AFSLocateCaseSensitiveDirEntry( ParentDirectoryCB->ObjectInformation->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                        ulCRC,
                                        &pDirEntry);

        if( pDirEntry == NULL)
        {

            //
            // Missed so perform a case insensitive lookup
            //

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSCheckSymlinkAccess Searching for entry %wZ case insensitive\n",
                          ComponentName));

            ulCRC = AFSGenerateCRC( ComponentName,
                                    TRUE);

            AFSLocateCaseInsensitiveDirEntry( ParentDirectoryCB->ObjectInformation->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                              ulCRC,
                                              &pDirEntry);

            if( pDirEntry == NULL)
            {

                //
                // OK, if this component is a valid short name then try
                // a lookup in the short name tree
                //

                if( !BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
                    RtlIsNameLegalDOS8Dot3( ComponentName,
                                            NULL,
                                            NULL))
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSCheckSymlinkAccess Searching for entry %wZ short name\n",
                                  ComponentName));

                    AFSLocateShortNameDirEntry( ParentDirectoryCB->ObjectInformation->Specific.Directory.ShortNameTree,
                                                ulCRC,
                                                &pDirEntry);
                }
            }
	    else
	    {

		//
		// Here we have a match on the case insensitive lookup for the name. If there
		// Is more than one link entry for this node then fail the lookup request
		//

		if( !BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD) ||
		    pDirEntry->CaseInsensitiveList.fLink != NULL)
		{

		    AFSReleaseResource( ParentDirectoryCB->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

		    try_return(ntStatus = STATUS_OBJECT_NAME_COLLISION);
		}
	    }
        }

        if( pDirEntry != NULL)
        {
            lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCheckSymlinkAccess Increment count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pDirEntry->NameInformation.FileName,
                          pDirEntry,
                          NULL,
                          lCount));

            ASSERT( lCount >= 0);
        }

        AFSReleaseResource( ParentDirectoryCB->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

        if( pDirEntry == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSCheckSymlinkAccess Failed to locate entry %wZ ntStatus %08X\n",
                          ComponentName,
                          STATUS_OBJECT_NAME_NOT_FOUND));

            try_return( ntStatus = STATUS_OBJECT_NAME_NOT_FOUND);
        }

        //
        // We have the symlink object but previously failed to process it so return access
        // denied.
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSCheckSymlinkAccess Failing symlink access to entry %wZ REPARSE_POINT_NOT_RESOLVED\n",
                      ComponentName));

        ntStatus = STATUS_REPARSE_POINT_NOT_RESOLVED;

        lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSCheckSymlinkAccess Decrement count on %wZ DE %p Ccb %p Cnt %d\n",
                      &pDirEntry->NameInformation.FileName,
                      pDirEntry,
                      NULL,
                      lCount));

        ASSERT( lCount >= 0);

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSRetrieveFinalComponent( IN UNICODE_STRING *FullPathName,
                           OUT UNICODE_STRING *ComponentName)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniFullPathName, uniRemainingPath, uniComponentName;

    uniFullPathName = *FullPathName;

    while( TRUE)
    {

        FsRtlDissectName( uniFullPathName,
                          &uniComponentName,
                          &uniRemainingPath);

        if( uniRemainingPath.Length == 0)
        {
            break;
        }

        uniFullPathName = uniRemainingPath;
    }

    if( uniComponentName.Length > 0)
    {
        *ComponentName = uniComponentName;
    }

    return ntStatus;
}

void
AFSDumpTraceFiles_Default()
{
    return;
}

BOOLEAN
AFSValidNameFormat( IN UNICODE_STRING *FileName)
{

    BOOLEAN bIsValidName = TRUE;
    USHORT usIndex = 0;

    __Enter
    {

        while( usIndex < FileName->Length/sizeof( WCHAR))
        {

            if( FileName->Buffer[ usIndex] == L':' ||
                FileName->Buffer[ usIndex] == L'*' ||
                FileName->Buffer[ usIndex] == L'?' ||
                FileName->Buffer[ usIndex] == L'"' ||
                FileName->Buffer[ usIndex] == L'<' ||
                FileName->Buffer[ usIndex] == L'>')
            {
                bIsValidName = FALSE;
                break;
            }

            usIndex++;
        }
    }

    return bIsValidName;
}

NTSTATUS
AFSCreateDefaultSecurityDescriptor()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PACL pSACL = NULL;
    ULONG ulSACLSize = 0;
    SYSTEM_MANDATORY_LABEL_ACE* pACE = NULL;
    ULONG ulACESize = 0;
    SECURITY_DESCRIPTOR *pSecurityDescr = NULL;
    ULONG ulSDLength = 0;
    SECURITY_DESCRIPTOR *pRelativeSecurityDescr = NULL;
    PSID pWorldSID = NULL;
    ULONG *pulSubAuthority = NULL;
    ULONG ulWorldSIDLEngth = 0;

    __Enter
    {

        ulWorldSIDLEngth = RtlLengthRequiredSid( 1);

        pWorldSID = (PSID)ExAllocatePoolWithTag( PagedPool,
                                                 ulWorldSIDLEngth,
                                                 AFS_GENERIC_MEMORY_29_TAG);

        if( pWorldSID == NULL)
        {
            AFSPrint( "AFSCreateDefaultSecurityDescriptor unable to allocate World SID\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pWorldSID,
                       ulWorldSIDLEngth);

        RtlInitializeSid( pWorldSID,
                          &SeWorldSidAuthority,
                          1);

        pulSubAuthority = RtlSubAuthoritySid(pWorldSID, 0);
        *pulSubAuthority = SECURITY_WORLD_RID;

        if( AFSRtlSetSaclSecurityDescriptor == NULL)
        {

            AFSPrint( "AFSCreateDefaultSecurityDescriptor AFSRtlSetSaclSecurityDescriptor == NULL\n");
        }
        else
        {

            ulACESize = sizeof( SYSTEM_MANDATORY_LABEL_ACE) + 128;

            pACE = (SYSTEM_MANDATORY_LABEL_ACE *)ExAllocatePoolWithTag( PagedPool,
                                                                        ulACESize,
                                                                        AFS_GENERIC_MEMORY_29_TAG);

            if( pACE == NULL)
            {

                AFSPrint( "AFSCreateDefaultSecurityDescriptor unable to allocate AFS_GENERIC_MEMORY_29_TAG\n");

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlZeroMemory( pACE,
                           ulACESize);

            pACE->Header.AceFlags = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
            pACE->Header.AceType = SYSTEM_MANDATORY_LABEL_ACE_TYPE;
            pACE->Header.AceSize = FIELD_OFFSET( SYSTEM_MANDATORY_LABEL_ACE, SidStart) + (USHORT)RtlLengthSid( SeExports->SeLowMandatorySid);
            pACE->Mask = SYSTEM_MANDATORY_LABEL_NO_WRITE_UP;

            RtlCopySid( RtlLengthSid( SeExports->SeLowMandatorySid),
                        &pACE->SidStart,
                        SeExports->SeLowMandatorySid);

            ulSACLSize = sizeof(ACL) + RtlLengthSid( SeExports->SeLowMandatorySid) +
                FIELD_OFFSET( SYSTEM_MANDATORY_LABEL_ACE, SidStart) + ulACESize;

            pSACL = (PACL)ExAllocatePoolWithTag( PagedPool,
                                                 ulSACLSize,
                                                 AFS_GENERIC_MEMORY_29_TAG);

            if( pSACL == NULL)
            {

                AFSPrint( "AFSCreateDefaultSecurityDescriptor unable to allocate AFS_GENERIC_MEMORY_29_TAG\n");

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            ntStatus = RtlCreateAcl( pSACL,
                                     ulSACLSize,
                                     ACL_REVISION);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint( "AFSCreateDefaultSecurityDescriptor RtlCreateAcl ntStatus %08lX\n",
                          ntStatus);

                try_return( ntStatus);
            }

            ntStatus = RtlAddAce( pSACL,
                                  ACL_REVISION,
                                  0,
                                  pACE,
                                  pACE->Header.AceSize);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint( "AFSCreateDefaultSecurityDescriptor RtlAddAce ntStatus %08lX\n",
                          ntStatus);

                try_return( ntStatus);
            }
        }

        pSecurityDescr = (SECURITY_DESCRIPTOR *)ExAllocatePoolWithTag( NonPagedPool,
                                                                       sizeof( SECURITY_DESCRIPTOR),
                                                                       AFS_GENERIC_MEMORY_27_TAG);

        if( pSecurityDescr == NULL)
        {

            AFSPrint( "AFSCreateDefaultSecurityDescriptor unable to allocate AFS_GENERIC_MEMORY_27_TAG\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        ntStatus = RtlCreateSecurityDescriptor( pSecurityDescr,
                                                SECURITY_DESCRIPTOR_REVISION);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint( "AFSCreateDefaultSecurityDescriptor RtlCreateSecurityDescriptor ntStatus %08lX\n",
                      ntStatus);

            try_return( ntStatus);
        }

        if( AFSRtlSetSaclSecurityDescriptor != NULL)
        {
            ntStatus = AFSRtlSetSaclSecurityDescriptor( pSecurityDescr,
                                                        TRUE,
                                                        pSACL,
                                                        FALSE);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint( "AFSCreateDefaultSecurityDescriptor AFSRtlSetSaclSecurityDescriptor ntStatus %08lX\n",
                          ntStatus);

                try_return( ntStatus);
            }
        }

        //
        // Add in the group and owner to the SD
        //

        if( AFSRtlSetGroupSecurityDescriptor != NULL)
        {
            ntStatus = AFSRtlSetGroupSecurityDescriptor( pSecurityDescr,
                                                         pWorldSID,
                                                         FALSE);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint( "AFSCreateDefaultSecurityDescriptor RtlSetGroupSecurityDescriptor failed ntStatus %08lX\n",
                          ntStatus);

                try_return( ntStatus);
            }
        }

        ntStatus = RtlSetOwnerSecurityDescriptor( pSecurityDescr,
                                                  pWorldSID,
                                                  FALSE);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint( "AFSCreateDefaultSecurityDescriptor RtlSetOwnerSecurityDescriptor failed ntStatus %08lX\n",
                      ntStatus);

            try_return( ntStatus);
        }

        if( !RtlValidSecurityDescriptor( pSecurityDescr))
        {

            AFSPrint( "AFSCreateDefaultSecurityDescriptor RtlValidSecurityDescriptor NOT\n");

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

	pRelativeSecurityDescr = (SECURITY_DESCRIPTOR *)AFSLibExAllocatePoolWithTag( NonPagedPool,
										     PAGE_SIZE,
										     AFS_GENERIC_MEMORY_27_TAG);

        if( pRelativeSecurityDescr == NULL)
        {

            AFSPrint( "AFSCreateDefaultSecurityDescriptor unable to allocate AFS_GENERIC_MEMORY_27_TAG\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        ulSDLength = PAGE_SIZE;

        ntStatus = RtlAbsoluteToSelfRelativeSD( pSecurityDescr,
                                                pRelativeSecurityDescr,
                                                &ulSDLength);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint( "AFSCreateDefaultSecurityDescriptor RtlAbsoluteToSelfRelativeSD ntStatus %08lX\n",
                      ntStatus);

            try_return( ntStatus);
        }

        AFSDefaultSD = pRelativeSecurityDescr;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pRelativeSecurityDescr != NULL)
            {

		AFSLibExFreePoolWithTag( pRelativeSecurityDescr,
					 AFS_GENERIC_MEMORY_27_TAG);
            }
        }

        if( pSecurityDescr != NULL)
        {

	    AFSLibExFreePoolWithTag( pSecurityDescr,
				     AFS_GENERIC_MEMORY_27_TAG);
        }

        if( pSACL != NULL)
        {

	    AFSLibExFreePoolWithTag( pSACL,
				     AFS_GENERIC_MEMORY_29_TAG);
        }

        if( pACE != NULL)
        {

	    AFSLibExFreePoolWithTag( pACE,
				     AFS_GENERIC_MEMORY_29_TAG);
        }

        if( pWorldSID != NULL)
        {

	    AFSLibExFreePoolWithTag( pWorldSID,
				     AFS_GENERIC_MEMORY_29_TAG);
        }
    }

    return ntStatus;
}

void
AFSRetrieveParentPath( IN UNICODE_STRING *FullFileName,
                       OUT UNICODE_STRING *ParentPath)
{

    *ParentPath = *FullFileName;

    //
    // If the final character is a \, jump over it
    //

    if( ParentPath->Length >= sizeof( WCHAR)
	&& ParentPath->Buffer[ (ParentPath->Length/sizeof( WCHAR)) - 1] == L'\\')
    {
        ParentPath->Length -= sizeof( WCHAR);
    }

    while( ParentPath->Length >= sizeof( WCHAR)
	   && ParentPath->Buffer[ (ParentPath->Length/sizeof( WCHAR)) - 1] != L'\\')
    {
        ParentPath->Length -= sizeof( WCHAR);
    }

    //
    // And the separator
    //

    if ( ParentPath->Length >= sizeof( WCHAR))
    {
	ParentPath->Length -= sizeof( WCHAR);
    }

    return;
}

NTSTATUS
AFSRetrieveValidAuthGroup( IN AFSFcb *Fcb,
                           IN AFSObjectInfoCB *ObjectInfo,
                           IN BOOLEAN WriteAccess,
                           OUT GUID *AuthGroup)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    GUID     stAuthGroup, stZeroAuthGroup;
    BOOLEAN  bFoundAuthGroup = FALSE;
    AFSCcb  *pCcb = NULL;
    AFSFcb *pFcb = Fcb;

    __Enter
    {

        RtlZeroMemory( &stAuthGroup,
                       sizeof( GUID));

        RtlZeroMemory( &stZeroAuthGroup,
                       sizeof( GUID));

        if( Fcb == NULL)
        {

            if( ObjectInfo != NULL &&
                ObjectInfo->Fcb != NULL)
            {
                pFcb = ObjectInfo->Fcb;
            }
        }

        if( pFcb != NULL)
        {

            AFSAcquireShared( &Fcb->NPFcb->CcbListLock,
                              TRUE);

            pCcb = Fcb->CcbListHead;

            while( pCcb != NULL)
            {

                if( WriteAccess &&
                    pCcb->GrantedAccess & FILE_WRITE_DATA)
                {
                    RtlCopyMemory( &stAuthGroup,
                                   &pCcb->AuthGroup,
                                   sizeof( GUID));

                    bFoundAuthGroup = TRUE;

                    break;
                }
                else if( pCcb->GrantedAccess & FILE_READ_DATA)
                {
                    //
                    // At least get the read-only access
                    //

                    RtlCopyMemory( &stAuthGroup,
                                   &pCcb->AuthGroup,
                                   sizeof( GUID));

                    bFoundAuthGroup = TRUE;
                }

                pCcb = (AFSCcb *)pCcb->ListEntry.fLink;
            }

            AFSReleaseResource( &Fcb->NPFcb->CcbListLock);
        }

        if( !bFoundAuthGroup)
        {

            AFSRetrieveAuthGroupFnc( (ULONGLONG)PsGetCurrentProcessId(),
                                     (ULONGLONG)PsGetCurrentThreadId(),
                                      &stAuthGroup);

            if( RtlCompareMemory( &stZeroAuthGroup,
                                  &stAuthGroup,
                                  sizeof( GUID)) == sizeof( GUID))
            {

                DbgPrint("AFSRetrieveValidAuthGroup Failed to locate PAG\n");

                try_return( ntStatus = STATUS_ACCESS_DENIED);
            }
        }

        RtlCopyMemory( AuthGroup,
                       &stAuthGroup,
                       sizeof( GUID));

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSPerformObjectInvalidate( IN AFSObjectInfoCB *ObjectInfo,
                            IN ULONG InvalidateReason)
{

    AFSDeviceExt       *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    LONG                lCount;

    __Enter
    {

	switch( InvalidateReason)
        {

            case AFS_INVALIDATE_DELETED:
            {

		AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
			      AFS_TRACE_LEVEL_VERBOSE,
			      "AFSPerformObjectInvalidation on node type %d for FID %08lX-%08lX-%08lX-%08lX Reason DELETED\n",
			      ObjectInfo->FileType,
			      ObjectInfo->FileId.Cell,
			      ObjectInfo->FileId.Volume,
			      ObjectInfo->FileId.Vnode,
			      ObjectInfo->FileId.Unique));

		if( ObjectInfo->FileType == AFS_FILE_TYPE_FILE &&
                    ObjectInfo->Fcb != NULL)
                {

		    if( !BooleanFlagOn( pRDRDevExt->DeviceFlags, AFS_DEVICE_FLAG_DIRECT_SERVICE_IO))
		    {

			AFSAcquireExcl( &ObjectInfo->Fcb->NPFcb->Specific.File.ExtentsResource,
					TRUE);

			ObjectInfo->Fcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_FILE_DELETED;

			KeSetEvent( &ObjectInfo->Fcb->NPFcb->Specific.File.ExtentsRequestComplete,
				    0,
				    FALSE);

			//
			// Clear out the extents
			// And get rid of them (note this involves waiting
			// for any writes or reads to the cache to complete)
			//

			AFSTearDownFcbExtents( ObjectInfo->Fcb,
					       NULL);

			AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->Specific.File.ExtentsResource);
		    }
		}

		ObjectInfo->Links = 0;

                break;
            }

            case AFS_INVALIDATE_DATA_VERSION:
            {

                if( ObjectInfo->FileType == AFS_FILE_TYPE_FILE &&
                    ObjectInfo->Fcb != NULL)
                {

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSPerformObjectInvalidation on node type %d for FID %08lX-%08lX-%08lX-%08lX Reason DATA_VERSION FCB %0p\n",
				  ObjectInfo->FileType,
				  ObjectInfo->FileId.Cell,
				  ObjectInfo->FileId.Volume,
				  ObjectInfo->FileId.Vnode,
				  ObjectInfo->FileId.Unique,
				  ObjectInfo->Fcb));

                    if( BooleanFlagOn( pRDRDevExt->DeviceFlags, AFS_DEVICE_FLAG_DIRECT_SERVICE_IO))
                    {

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
				      AFS_TRACE_LEVEL_VERBOSE,
				      "AFSPerformObjectInvalidate Acquiring Fcb lock %p EXCL %08lX\n",
				      &ObjectInfo->Fcb->NPFcb->Resource,
				      PsGetCurrentThread()));

			AFSAcquireExcl( &ObjectInfo->Fcb->NPFcb->Resource,
					TRUE);

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSPerformObjectInvalidation DirectIO Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
                                      &ObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                      PsGetCurrentThread()));

                        AFSAcquireExcl( &ObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                        TRUE);

			__try
                        {

                            if( ObjectInfo->Fcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL &&
                                !CcPurgeCacheSection( &ObjectInfo->Fcb->NPFcb->SectionObjectPointers,
                                                      NULL,
                                                      0,
                                                      FALSE))
                            {

                                AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                              AFS_TRACE_LEVEL_WARNING,
                                              "AFSPerformObjectInvalidation DirectIO CcPurgeCacheSection failure FID %08lX-%08lX-%08lX-%08lX\n",
                                              ObjectInfo->FileId.Cell,
                                              ObjectInfo->FileId.Volume,
                                              ObjectInfo->FileId.Vnode,
                                              ObjectInfo->FileId.Unique));

                                SetFlag( ObjectInfo->Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);

				SetFlag( ObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA);
                            }
                        }
			__except( EXCEPTION_EXECUTE_HANDLER)
                        {

                            ntStatus = GetExceptionCode();

                            AFSDbgTrace(( 0,
                                          0,
                                          "EXCEPTION - AFSPerformObjectInvalidation DirectIO FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
                                          ObjectInfo->FileId.Cell,
                                          ObjectInfo->FileId.Volume,
                                          ObjectInfo->FileId.Vnode,
                                          ObjectInfo->FileId.Unique,
                                          ntStatus));

                            SetFlag( ObjectInfo->Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);

			    SetFlag( ObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA);
			}

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSPerformObjectInvalidation DirectIO Releasing Fcb SectionObject lock %p EXCL %08lX\n",
                                      &ObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                      PsGetCurrentThread()));

                        AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->SectionObjectResource);

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
				      AFS_TRACE_LEVEL_VERBOSE,
				      "AFSPerformObjectInvalidation DirectIO Releasing Fcb lock %p EXCL %08lX\n",
				      &ObjectInfo->Fcb->NPFcb->Resource,
				      PsGetCurrentThread()));

			AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->Resource);
		    }
                    else
                    {
			LIST_ENTRY         *le;
			AFSExtent          *pEntry;
			ULONG               ulProcessCount = 0;
			ULONG               ulCount = 0;
			LARGE_INTEGER liCurrentOffset = {0,0};
			LARGE_INTEGER liFlushLength = {0,0};
			ULONG ulFlushLength = 0;
			BOOLEAN bLocked = FALSE;
			BOOLEAN bExtentsLocked = FALSE;
			BOOLEAN bCleanExtents = FALSE;

			AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
				      AFS_TRACE_LEVEL_VERBOSE,
				      "AFSPerformObjectInvalidate Acquiring Fcb lock %p EXCL %08lX\n",
				      &ObjectInfo->Fcb->NPFcb->Resource,
				      PsGetCurrentThread()));

			AFSAcquireExcl( &ObjectInfo->Fcb->NPFcb->Resource,
					TRUE);

			bLocked = TRUE;

                        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSPerformObjectInvalidate Acquiring Fcb extents lock %p SHARED %08lX\n",
                                      &ObjectInfo->Fcb->NPFcb->Specific.File.ExtentsResource,
                                      PsGetCurrentThread()));

                        AFSAcquireShared( &ObjectInfo->Fcb->NPFcb->Specific.File.ExtentsResource,
                                          TRUE);

                        bExtentsLocked = TRUE;

                        //
                        // There are several possibilities here:
                        //
                        // 0. If there are no extents or all of the extents are dirty, do nothing.
                        //
                        // 1. There could be nothing dirty and an open reference count of zero
                        //    in which case we can just tear down all of the extents without
                        //    holding any resources.
                        //
                        // 2. There could be nothing dirty and a non-zero open reference count
                        //    in which case we can issue a CcPurge against the entire file
                        //    while holding just the Fcb Resource.
                        //
                        // 3. There can be dirty extents in which case we need to identify
                        //    the non-dirty ranges and then perform a CcPurge on just the
                        //    non-dirty ranges while holding just the Fcb Resource.
                        //

                        if ( ObjectInfo->Fcb->Specific.File.ExtentCount != ObjectInfo->Fcb->Specific.File.ExtentsDirtyCount)
                        {

                            if ( ObjectInfo->Fcb->Specific.File.ExtentsDirtyCount == 0)
                            {

                                AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->Specific.File.ExtentsResource );

                                bExtentsLocked = FALSE;

                                if ( ObjectInfo->Fcb->OpenReferenceCount == 0)
                                {

                                    AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->Resource);

                                    bLocked = FALSE;

                                    AFSTearDownFcbExtents( ObjectInfo->Fcb,
                                                           NULL);
                                }
                                else
                                {

				    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                                                  AFS_TRACE_LEVEL_VERBOSE,
                                                  "AFSPerformObjectInvalidation Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
                                                  &ObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                                  PsGetCurrentThread()));

                                    AFSAcquireExcl( &ObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                                    TRUE);

                                    __try
                                    {

                                        if( ObjectInfo->Fcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL &&
                                            !CcPurgeCacheSection( &ObjectInfo->Fcb->NPFcb->SectionObjectPointers,
                                                                  NULL,
                                                                  0,
                                                                  FALSE))
                                        {

                                            AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                                          AFS_TRACE_LEVEL_WARNING,
                                                          "AFSPerformObjectInvalidation CcPurgeCacheSection failure FID %08lX-%08lX-%08lX-%08lX\n",
                                                          ObjectInfo->FileId.Cell,
                                                          ObjectInfo->FileId.Volume,
                                                          ObjectInfo->FileId.Vnode,
                                                          ObjectInfo->FileId.Unique));

					    SetFlag( ObjectInfo->Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);

					    SetFlag( ObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA);
					}
                                        else
                                        {

                                            bCleanExtents = TRUE;
                                        }
                                    }
				    __except( EXCEPTION_EXECUTE_HANDLER)
                                    {

                                        ntStatus = GetExceptionCode();

                                        AFSDbgTrace(( 0,
                                                      0,
                                                      "EXCEPTION - AFSPerformObjectInvalidation FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
                                                      ObjectInfo->FileId.Cell,
                                                      ObjectInfo->FileId.Volume,
                                                      ObjectInfo->FileId.Vnode,
                                                      ObjectInfo->FileId.Unique,
                                                      ntStatus));

                                        SetFlag( ObjectInfo->Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);

					SetFlag( ObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA);
                                    }

				    AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                                                  AFS_TRACE_LEVEL_VERBOSE,
                                                  "AFSPerformObjectInvalidation Releasing Fcb SectionObject lock %p EXCL %08lX\n",
                                                  &ObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                                  PsGetCurrentThread()));

                                    AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->SectionObjectResource);

				    AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->Resource);

				    bLocked = FALSE;
                                }
                            }
                            else
                            {

                                AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->Specific.File.ExtentsResource );

                                bExtentsLocked = FALSE;

				AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                                              AFS_TRACE_LEVEL_VERBOSE,
                                              "AFSPerformObjectInvalidation Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
                                              &ObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                              PsGetCurrentThread()));

                                AFSAcquireExcl( &ObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                                TRUE);

                                //
                                // Must build a list of non-dirty ranges from the beginning of the file
                                // to the end.  There can be at most (Fcb->Specific.File.ExtentsDirtyCount + 1)
                                // ranges.  In all but the most extreme random data write scenario there will
                                // be significantly fewer.
                                //
                                // For each range we need offset and size.
                                //

                                AFSByteRange * ByteRangeList = NULL;
                                ULONG          ulByteRangeCount = 0;
                                ULONG          ulIndex;
                                BOOLEAN        bPurgeOnClose = FALSE;

                                __try
                                {

                                    ulByteRangeCount = AFSConstructCleanByteRangeList( ObjectInfo->Fcb,
                                                                                       &ByteRangeList);

                                    if ( ByteRangeList != NULL ||
                                         ulByteRangeCount == 0)
                                    {

                                        for ( ulIndex = 0; ulIndex < ulByteRangeCount; ulIndex++)
                                        {

                                            ULONG ulSize;

                                            do {

                                                ulSize = ByteRangeList[ulIndex].Length.QuadPart > DWORD_MAX ? DWORD_MAX : ByteRangeList[ulIndex].Length.LowPart;

						__try
						{

						    if( ObjectInfo->Fcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL &&
							!CcPurgeCacheSection( &ObjectInfo->Fcb->NPFcb->SectionObjectPointers,
									      &ByteRangeList[ulIndex].FileOffset,
									      ulSize,
									      FALSE))
						    {

							AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
								      AFS_TRACE_LEVEL_WARNING,
								      "AFSPerformObjectInvalidation [1] CcPurgeCacheSection failure FID %08lX-%08lX-%08lX-%08lX\n",
								      ObjectInfo->FileId.Cell,
								      ObjectInfo->FileId.Volume,
								      ObjectInfo->FileId.Vnode,
								      ObjectInfo->FileId.Unique));

							bPurgeOnClose = TRUE;
						    }
						    else
						    {

							bCleanExtents = TRUE;
						    }
						}
						__except( EXCEPTION_EXECUTE_HANDLER)
						{

						    ntStatus = GetExceptionCode();

						    AFSDbgTrace(( 0,
								  0,
								  "EXCEPTION - AFSPerformObjectInvalidation CcPurgeCacheSection (1) FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
								  ObjectInfo->FileId.Cell,
								  ObjectInfo->FileId.Volume,
								  ObjectInfo->FileId.Vnode,
								  ObjectInfo->FileId.Unique,
								  ntStatus));
						}

                                                ByteRangeList[ulIndex].Length.QuadPart -= ulSize;

                                                ByteRangeList[ulIndex].FileOffset.QuadPart += ulSize;

                                            } while ( ByteRangeList[ulIndex].Length.QuadPart > 0);
                                        }
                                    }
                                    else
                                    {

                                        //
                                        // We couldn't allocate the memory to build the purge list
                                        // so just walk the extent list while holding the ExtentsList Resource.
                                        // This could deadlock but we do not have much choice.
                                        //

                                        AFSAcquireExcl(  &ObjectInfo->Fcb->NPFcb->Specific.File.ExtentsResource,
                                                        TRUE);
                                        bExtentsLocked = TRUE;

                                        le = ObjectInfo->Fcb->Specific.File.ExtentsLists[AFS_EXTENTS_LIST].Flink;

                                        ulProcessCount = 0;

                                        ulCount = (ULONG)ObjectInfo->Fcb->Specific.File.ExtentCount;

                                        if( ulCount > 0)
                                        {
                                            pEntry = ExtentFor( le, AFS_EXTENTS_LIST );

                                            while( ulProcessCount < ulCount)
                                            {
                                                pEntry = ExtentFor( le, AFS_EXTENTS_LIST );

                                                if( !BooleanFlagOn( pEntry->Flags, AFS_EXTENT_DIRTY))
                                                {
						    __try
						    {
							if( !CcPurgeCacheSection( &ObjectInfo->Fcb->NPFcb->SectionObjectPointers,
										  &pEntry->FileOffset,
										  pEntry->Size,
										  FALSE))
							{

							    AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
									  AFS_TRACE_LEVEL_WARNING,
									  "AFSPerformObjectInvalidation [2] CcPurgeCacheSection failure FID %08lX-%08lX-%08lX-%08lX\n",
									  ObjectInfo->FileId.Cell,
									  ObjectInfo->FileId.Volume,
									  ObjectInfo->FileId.Vnode,
									  ObjectInfo->FileId.Unique));

							    bPurgeOnClose = TRUE;
							}
							else
							{

							    bCleanExtents = TRUE;
							}
						    }
						    __except( EXCEPTION_EXECUTE_HANDLER)
						    {

							ntStatus = GetExceptionCode();

							AFSDbgTrace(( 0,
								      0,
								      "EXCEPTION - AFSPerformObjectInvalidation CcPurgeCacheSection (2) FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
								      ObjectInfo->FileId.Cell,
								      ObjectInfo->FileId.Volume,
								      ObjectInfo->FileId.Vnode,
								      ObjectInfo->FileId.Unique,
								      ntStatus));
						    }
                                                }

                                                if( liCurrentOffset.QuadPart < pEntry->FileOffset.QuadPart)
                                                {

                                                    liFlushLength.QuadPart = pEntry->FileOffset.QuadPart - liCurrentOffset.QuadPart;

                                                    while( liFlushLength.QuadPart > 0)
                                                    {

                                                        if( liFlushLength.QuadPart > 512 * 1024000)
                                                        {
                                                            ulFlushLength = 512 * 1024000;
                                                        }
                                                        else
                                                        {
                                                            ulFlushLength = liFlushLength.LowPart;
                                                        }

							__try
							{
							    if( !CcPurgeCacheSection( &ObjectInfo->Fcb->NPFcb->SectionObjectPointers,
										      &liCurrentOffset,
										      ulFlushLength,
										      FALSE))
							    {

								AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
									      AFS_TRACE_LEVEL_WARNING,
									      "AFSPerformObjectInvalidation [3] CcPurgeCacheSection failure FID %08lX-%08lX-%08lX-%08lX\n",
									      ObjectInfo->FileId.Cell,
									      ObjectInfo->FileId.Volume,
									      ObjectInfo->FileId.Vnode,
									      ObjectInfo->FileId.Unique));

								bPurgeOnClose = TRUE;
							    }
							    else
							    {

								bCleanExtents = TRUE;
							    }
							}
							__except( EXCEPTION_EXECUTE_HANDLER)
							{

							    ntStatus = GetExceptionCode();

							    AFSDbgTrace(( 0,
									  0,
									  "EXCEPTION - AFSPerformObjectInvalidation CcPurgeCacheSection (3) FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
									  ObjectInfo->FileId.Cell,
									  ObjectInfo->FileId.Volume,
									  ObjectInfo->FileId.Vnode,
									  ObjectInfo->FileId.Unique,
									  ntStatus));
							}

                                                        liFlushLength.QuadPart -= ulFlushLength;
                                                    }
                                                }

                                                liCurrentOffset.QuadPart = pEntry->FileOffset.QuadPart + pEntry->Size;

                                                ulProcessCount++;
                                                le = le->Flink;
                                            }
                                        }
                                        else
                                        {
					    __try
					    {
						if( !CcPurgeCacheSection( &ObjectInfo->Fcb->NPFcb->SectionObjectPointers,
									  NULL,
									  0,
									  FALSE))
						{

						    AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
								  AFS_TRACE_LEVEL_WARNING,
								  "AFSPerformObjectInvalidation [4] CcPurgeCacheSection failure FID %08lX-%08lX-%08lX-%08lX\n",
								  ObjectInfo->FileId.Cell,
								  ObjectInfo->FileId.Volume,
								  ObjectInfo->FileId.Vnode,
								  ObjectInfo->FileId.Unique));

						    bPurgeOnClose = TRUE;
						}
						else
						{

						    bCleanExtents = TRUE;
						}
					    }
					    __except( EXCEPTION_EXECUTE_HANDLER)
					    {

						ntStatus = GetExceptionCode();

						AFSDbgTrace(( 0,
							      0,
							      "EXCEPTION - AFSPerformObjectInvalidation CcPurgeCacheSection (4) FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
							      ObjectInfo->FileId.Cell,
							      ObjectInfo->FileId.Volume,
							      ObjectInfo->FileId.Vnode,
							      ObjectInfo->FileId.Unique,
							      ntStatus));
					    }
                                        }

                                        if ( bPurgeOnClose)
                                        {

                                            SetFlag( ObjectInfo->Fcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
                                        }
                                    }
                                }
				__except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()))
                                {

                                    ntStatus = GetExceptionCode();

                                    AFSDbgTrace(( 0,
                                                  0,
                                                  "EXCEPTION - AFSPerformObjectInvalidation FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                                  ObjectInfo->FileId.Cell,
                                                  ObjectInfo->FileId.Volume,
                                                  ObjectInfo->FileId.Vnode,
                                                  ObjectInfo->FileId.Unique,
                                                  ntStatus));
                                }

				AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                                              AFS_TRACE_LEVEL_VERBOSE,
                                              "AFSPerformObjectInvalidation Releasing Fcb SectionObject lock %p EXCL %08lX\n",
                                              &ObjectInfo->Fcb->NPFcb->SectionObjectResource,
                                              PsGetCurrentThread()));

                                AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->SectionObjectResource);

				AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->Resource);

				bLocked = FALSE;
                            }
                        }

                        if ( bExtentsLocked)
                        {

                            AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->Specific.File.ExtentsResource );
                        }

			if ( bLocked)
			{

			    AFSReleaseResource( &ObjectInfo->Fcb->NPFcb->Resource);
			}

			if ( bCleanExtents)
			{

			    AFSReleaseCleanExtents( ObjectInfo->Fcb,
						    NULL);
			}
		    }
                }
		else if ( ObjectInfo->FileType == AFS_FILE_TYPE_FILE)
		{

		    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
				  AFS_TRACE_LEVEL_VERBOSE,
				  "AFSPerformObjectInvalidation on node type %d for FID %08lX-%08lX-%08lX-%08lX Reason DATA_VERSION FCB NULL\n",
				  ObjectInfo->FileType,
				  ObjectInfo->FileId.Cell,
				  ObjectInfo->FileId.Volume,
				  ObjectInfo->FileId.Vnode,
				  ObjectInfo->FileId.Unique));

		    SetFlag( ObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY_DATA);
		}

                break;
            }
	}

        //
        // Destroy the reference passed in by the caller to AFSInvalidateObject
        // or AFSQueueInvalidateObject
        //

        AFSAcquireShared( ObjectInfo->VolumeCB->ObjectInfoTree.TreeLock,
                          TRUE);

        lCount = AFSObjectInfoDecrement( ObjectInfo,
                                         AFS_OBJECT_REFERENCE_INVALIDATION);

        AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSPerformObjectInvalidation Decrement count on object %p Cnt %d\n",
                      ObjectInfo,
                      lCount));

        AFSReleaseResource( ObjectInfo->VolumeCB->ObjectInfoTree.TreeLock);
    }

    return ntStatus;
}

BOOLEAN
AFSIgnoreReparsePointToFile( void)
{
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    BOOLEAN bIgnoreReparsePoint;

    {

	bIgnoreReparsePoint = BooleanFlagOn( pDeviceExt->Specific.RDR.ReparsePointPolicy,
					     AFS_REPARSE_POINT_TO_FILE_AS_FILE);
    }

    return bIgnoreReparsePoint;
}

NTSTATUS
AFSRetrieveTargetFileInfo( IN PUNICODE_STRING TargetName,
			   OUT AFSFileInfoCB *FileInfo)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    OBJECT_ATTRIBUTES stObjectAttribs;
    HANDLE hFile = NULL;
    IO_STATUS_BLOCK stIoStatus;
    FILE_NETWORK_OPEN_INFORMATION stFileInfo;

    __Enter
    {

	InitializeObjectAttributes( &stObjectAttribs,
				    TargetName,
				    OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
				    NULL,
				    NULL);

	ntStatus = ZwCreateFile( &hFile,
				 FILE_READ_ATTRIBUTES,
				 &stObjectAttribs,
				 &stIoStatus,
				 NULL,
				 0,
				 FILE_SHARE_READ | FILE_SHARE_WRITE,
				 FILE_OPEN,
				 FILE_SYNCHRONOUS_IO_NONALERT,
				 NULL,
				 0);

	if( !NT_SUCCESS( ntStatus))
	{

	    try_return( ntStatus);
	}

	ntStatus = ZwQueryInformationFile( hFile,
					   &stIoStatus,
					   &stFileInfo,
					   sizeof( FILE_NETWORK_OPEN_INFORMATION),
					   FileNetworkOpenInformation);

	if( !NT_SUCCESS( ntStatus))
	{

	    try_return( ntStatus);
	}

	FileInfo->FileAttributes = stFileInfo.FileAttributes;

	FileInfo->AllocationSize = stFileInfo.AllocationSize;

	FileInfo->EndOfFile = stFileInfo.EndOfFile;

	FileInfo->CreationTime = stFileInfo.CreationTime;

	FileInfo->LastAccessTime = stFileInfo.LastAccessTime;

	FileInfo->LastWriteTime = stFileInfo.LastWriteTime;

	FileInfo->ChangeTime = stFileInfo.ChangeTime;

try_exit:

	if( hFile != NULL)
	{
	    ZwClose( hFile);
	}
    }

    return ntStatus;
}

BOOLEAN
AFSIsShareName( IN UNICODE_STRING *FileName)
{

    BOOLEAN     bIsShareName = TRUE;
    USHORT      usIndex = 1; // Skip the first \

    //
    // A share name will be of the form \Share so only a single \ at the beginning
    //

    while( usIndex < FileName->Length/sizeof( WCHAR))
    {

	if( FileName->Buffer[ usIndex] == L'\\')
	{
	    bIsShareName = FALSE;
	    break;
	}

	usIndex++;
    }

    return bIsShareName;
}