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
AFSExceptionFilter( IN ULONG Code, 
                    IN PEXCEPTION_POINTERS ExceptPtrs)
{

    PEXCEPTION_RECORD ExceptRec;
    PCONTEXT Context;

    __try
    {

        ExceptRec = ExceptPtrs->ExceptionRecord;

        Context = ExceptPtrs->ContextRecord;

        DbgPrint("**** Exception Caught in AFS Redirector ****\n");

        DbgPrint("\n\nPerform the following WnDbg Cmds:\n");
        DbgPrint("\n\t.exr %08lX ;  .cxr %08lX ;  .kb\n\n", ExceptRec, Context);
        
        DbgPrint("**** Exception Complete from AFS Redirector ****\n");

        AFSBreakPoint();
    }
    __except( EXCEPTION_EXECUTE_HANDLER)
    {

        NOTHING;
    }

    return EXCEPTION_EXECUTE_HANDLER;
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

    if( Resource != &AFSDbgLogLock)
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSReleaseResource Releasing lock %08lX Thread %08lX\n",
                      Resource,
                      PsGetCurrentThread());
    }

    ExReleaseResourceLite( Resource);

    KeLeaveCriticalRegion();

    return;
}

void
AFSConvertToShared( IN PERESOURCE Resource)
{

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSConvertToShared Converting lock %08lX Thread %08lX\n",
                  Resource,
                  PsGetCurrentThread());

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
// Function: AFSBuildCRCTable
//
// Description:
//
//      This function builds the CRC table for mapping filenames to a CRC value.
//
// Return:
//
//      A status is returned for the function
//

void 
AFSBuildCRCTable()
{
    ULONG crc;
    int i, j;

    for ( i = 0; i <= 255; i++) 
    {
        crc = i;
        for ( j = 8; j > 0; j--) 
        {
            if (crc & 1)
            {
                crc = ( crc >> 1 ) ^ CRC32_POLYNOMIAL;
            }
            else
            {
                crc >>= 1;
            }
        }

        AFSCRCTable[ i ] = crc;
    }
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

    ULONG crc;
    ULONG temp1, temp2;
    UNICODE_STRING UpcaseString;
    WCHAR *lpbuffer;
    USHORT size = 0;

    if( !AFSCRCTable[1])
    {
        AFSBuildCRCTable();
    }

    crc = 0xFFFFFFFFL;

    if( UpperCaseName)
    {

        RtlUpcaseUnicodeString( &UpcaseString,
                                FileName,
                                TRUE);

        lpbuffer = UpcaseString.Buffer;

        size = (UpcaseString.Length/sizeof( WCHAR));
    }
    else
    {

        lpbuffer = FileName->Buffer;

        size = (FileName->Length/sizeof( WCHAR));
    }

    while (size--) 
    {
        temp1 = (crc >> 8) & 0x00FFFFFFL;        
        temp2 = AFSCRCTable[((int)crc ^ *lpbuffer++) & 0xff];
        crc = temp1 ^ temp2;
    }

    if( UpperCaseName)
    {

        RtlFreeUnicodeString( &UpcaseString);
    }

    crc ^= 0xFFFFFFFFL;

    return crc;
}

void *
AFSLockSystemBuffer( IN PIRP Irp,
                     IN ULONG Length)
{

    NTSTATUS Status = STATUS_SUCCESS;
    void *pAddress = NULL;

    //
    // When locking the buffer, we must ensure the underlying device does not perform buffered IO operations. If it does,
    // then the buffer will come from the AssociatedIrp->SystemAddress pointer, not the user buffer.
    //

    if (Irp->MdlAddress == NULL) 
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
            __except( EXCEPTION_EXECUTE_HANDLER)
            {

                IoFreeMdl( Irp->MdlAddress );
                Irp->MdlAddress = NULL;
                pAddress = NULL;
            }
        }
    } 
    else 
    {

        pAddress = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, NormalPagePriority );
    }

    return pAddress;
}

void *
AFSMapToService( IN PIRP Irp,
                 IN ULONG ByteCount)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    void *pMappedBuffer = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
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
    void *pMappedBuffer = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
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
AFSReadRegistry( IN PUNICODE_STRING RegistryPath)
{

    NTSTATUS ntStatus        = STATUS_SUCCESS;
    ULONG Default            = 0;
    UNICODE_STRING paramPath;
    ULONG Value                = 0;
    RTL_QUERY_REGISTRY_TABLE paramTable[2];
    UNICODE_STRING defaultUnicodeName;
    WCHAR SubKeyString[]    = L"\\Parameters";

    //
    // Setup the paramPath buffer.
    //

    paramPath.MaximumLength = RegistryPath->Length + sizeof( SubKeyString); 
    paramPath.Buffer = (PWSTR)ExAllocatePoolWithTag( PagedPool,
                                                     paramPath.MaximumLength,
                                                     AFS_GENERIC_MEMORY_TAG);
 
    RtlInitUnicodeString( &defaultUnicodeName, 
                          L"NO NAME");

    //
    // If it exists, setup the path.
    //

    if( paramPath.Buffer != NULL) 
    { 
        
        //
        // Move in the paths
        //

        RtlCopyMemory( &paramPath.Buffer[ 0], 
                       &RegistryPath->Buffer[ 0], 
                       RegistryPath->Length);
        
        RtlCopyMemory( &paramPath.Buffer[ RegistryPath->Length / 2], 
                       SubKeyString, 
                       sizeof( SubKeyString)); 
 
        paramPath.Length = paramPath.MaximumLength; 

        RtlZeroMemory( paramTable, 
                       sizeof( paramTable));

        Value = 0;

        //
        // Setup the table to query the registry for the needed value
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT; 
        paramTable[0].Name = AFS_REG_DEBUG_FLAGS; 
        paramTable[0].EntryContext = &Value;
        
        paramTable[0].DefaultType = REG_DWORD; 
        paramTable[0].DefaultData = &Default; 
        paramTable[0].DefaultLength = sizeof (ULONG) ; 

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL, 
                                           paramPath.Buffer, 
                                           paramTable, 
                                           NULL, 
                                           NULL);

        if( NT_SUCCESS( ntStatus))
        {

            AFSDebugFlags = Value;
        }

        RtlZeroMemory( paramTable, 
                       sizeof( paramTable));

        Value = 0;

        //
        // Setup the table to query the registry for the needed value
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT; 
        paramTable[0].Name = AFS_REG_DEBUG_SUBSYSTEM; 
        paramTable[0].EntryContext = &Value;
        
        paramTable[0].DefaultType = REG_DWORD; 
        paramTable[0].DefaultData = &Default; 
        paramTable[0].DefaultLength = sizeof (ULONG) ; 

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL, 
                                           paramPath.Buffer, 
                                           paramTable, 
                                           NULL, 
                                           NULL);

        if( NT_SUCCESS( ntStatus))
        {

            AFSTraceComponent = Value;
        }

        RtlZeroMemory( paramTable, 
                       sizeof( paramTable));

        Value = 0;

        //
        // Setup the table to query the registry for the needed value
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT; 
        paramTable[0].Name = AFS_REG_TRACE_BUFFER_LENGTH; 
        paramTable[0].EntryContext = &Value;
        
        paramTable[0].DefaultType = REG_DWORD; 
        paramTable[0].DefaultData = &Default; 
        paramTable[0].DefaultLength = sizeof (ULONG); 

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL, 
                                           paramPath.Buffer, 
                                           paramTable, 
                                           NULL, 
                                           NULL);

        if( NT_SUCCESS( ntStatus) &&
            Value > 0)
        {

            AFSDbgBufferLength = Value;

            //
            // Let's limit things a bit ...
            //

            if( AFSDbgBufferLength > 10240)
            {

                AFSDbgBufferLength = 1024;
            }
        }
        else
        {

            AFSDbgBufferLength = 0;
        }

        //
        // Make it bytes
        //

        AFSDbgBufferLength *= 1024;

        //
        // Now get ready to set up for MaxServerDirty
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT; 
        paramTable[0].Name = AFS_REG_MAX_DIRTY; 
        paramTable[0].EntryContext = &Value;
        
        paramTable[0].DefaultType = REG_DWORD; 
        paramTable[0].DefaultData = &Default; 
        paramTable[0].DefaultLength = sizeof (ULONG) ; 

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL, 
                                           paramPath.Buffer, 
                                           paramTable, 
                                           NULL, 
                                           NULL);

        if( NT_SUCCESS( ntStatus))
        {

            AFSMaxDirtyFile = Value;
        }

        RtlZeroMemory( paramTable, 
                       sizeof( paramTable));

        Value = 0;

        //
        // Setup the table to query the registry for the needed value
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT; 
        paramTable[0].Name = AFS_REG_DEBUG_LEVEL; 
        paramTable[0].EntryContext = &Value;
        
        paramTable[0].DefaultType = REG_DWORD; 
        paramTable[0].DefaultData = &Default; 
        paramTable[0].DefaultLength = sizeof (ULONG) ; 

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL, 
                                           paramPath.Buffer, 
                                           paramTable, 
                                           NULL, 
                                           NULL);

        if( NT_SUCCESS( ntStatus))
        {

            AFSDebugLevel = Value;
        }

        //
        // MaxIO
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT; 
        paramTable[0].Name = AFS_REG_MAX_IO; 
        paramTable[0].EntryContext = &Value;
        
        paramTable[0].DefaultType = REG_DWORD; 
        paramTable[0].DefaultData = &Default; 
        paramTable[0].DefaultLength = sizeof (ULONG) ; 

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL, 
                                           paramPath.Buffer, 
                                           paramTable, 
                                           NULL, 
                                           NULL);

        if( NT_SUCCESS( ntStatus))
        {

            AFSMaxDirectIo = Value;
        }

        //
        // Free up the buffer
        //

        ExFreePool( paramPath.Buffer);

        ntStatus = STATUS_SUCCESS;
    } 
    else 
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
}

NTSTATUS
AFSUpdateRegistryParameter( IN PUNICODE_STRING ValueName,
                            IN ULONG ValueType,
                            IN void *ValueData,
                            IN ULONG ValueDataLength)
{

    NTSTATUS ntStatus        = STATUS_SUCCESS;
    UNICODE_STRING paramPath, uniParamKey;
    HANDLE hParameters = 0;
    ULONG ulDisposition = 0;
    OBJECT_ATTRIBUTES stObjectAttributes;

    __Enter
    {

        RtlInitUnicodeString( &uniParamKey,
                              L"\\Parameters");
                              
        //
        // Setup the paramPath buffer.
        //

        paramPath.MaximumLength = AFSRegistryPath.Length + uniParamKey.Length; 
        paramPath.Buffer = (PWSTR)ExAllocatePoolWithTag( PagedPool,
                                                         paramPath.MaximumLength,
                                                         AFS_GENERIC_MEMORY_TAG);
 
        if( paramPath.Buffer == NULL) 
        { 

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }
        
        //
        // Move in the paths
        //

        RtlCopyMemory( paramPath.Buffer, 
                       AFSRegistryPath.Buffer, 
                       AFSRegistryPath.Length);
        
        paramPath.Length = AFSRegistryPath.Length;

        RtlCopyMemory( &paramPath.Buffer[ paramPath.Length / 2], 
                       uniParamKey.Buffer, 
                       uniParamKey.Length); 
 
        paramPath.Length += uniParamKey.Length; 

        InitializeObjectAttributes( &stObjectAttributes,
                                    &paramPath,
                                    OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                    NULL,
                                    NULL);

        ntStatus = ZwOpenKey( &hParameters,
                              KEY_ALL_ACCESS,
                              &stObjectAttributes);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Set the value
        //

        ntStatus = ZwSetValueKey( hParameters,
                                  ValueName,
                                  0,
                                  ValueType,
                                  ValueData,
                                  ValueDataLength);

        ZwClose( hParameters);

try_exit:

        if( paramPath.Buffer != NULL)
        {

            //
            // Free up the buffer
            //

            ExFreePool( paramPath.Buffer);
        }
    } 

    return ntStatus;
}

NTSTATUS
AFSInitializeControlDevice()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSProcessCB *pProcessCB = NULL;

    __Enter
    {

        //
        // Initialize the comm pool resources
        //

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.CommServiceCB.IrpPoolLock);

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.CommServiceCB.ResultPoolLock);

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.ExtentReleaseResource);


        //
        // And the events
        //

        KeInitializeEvent( &pDeviceExt->Specific.Control.CommServiceCB.IrpPoolHasEntries,
                           NotificationEvent,
                           FALSE);

        KeInitializeEvent( &pDeviceExt->Specific.Control.ExtentReleaseEvent,
                           NotificationEvent,
                           FALSE);

        pDeviceExt->Specific.Control.ExtentReleaseSequence = 0;

        KeInitializeEvent( &pDeviceExt->Specific.Control.VolumeWorkerCloseEvent,
                           NotificationEvent,
                           TRUE);

        //
        // Set the initial state of the irp pool
        //

        pDeviceExt->Specific.Control.CommServiceCB.IrpPoolControlFlag = POOL_INACTIVE;

        //
        // Initialize the provider list lock
        //

        ExInitializeResourceLite( &AFSProviderListLock);

        //
        // Initialize our process tree information
        //

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.ProcessTreeLock);

        pDeviceExt->Specific.Control.ProcessTree.TreeLock = &pDeviceExt->Specific.Control.ProcessTreeLock;

        pDeviceExt->Specific.Control.ProcessTree.TreeHead = NULL;

        //
        // Initialize an entry in our process tree for the system process
        //

        pProcessCB = (AFSProcessCB *)ExAllocatePoolWithTag( PagedPool,
                                                            sizeof( AFSProcessCB),
                                                            AFS_PROCESS_CB_TAG);

        if( pProcessCB != NULL)
        {

            RtlZeroMemory( pProcessCB,
                           sizeof( AFSProcessCB));

            pProcessCB->TreeEntry.HashIndex = (ULONGLONG)PsGetCurrentProcessId();

#if defined(_WIN64)

            if( !IoIs32bitProcess( NULL))
            {

                SetFlag( pProcessCB->Flags, AFS_PROCESS_FLAG_IS_64BIT);
            }

#endif

            if( pDeviceExt->Specific.Control.ProcessTree.TreeHead == NULL)
            {

                pDeviceExt->Specific.Control.ProcessTree.TreeHead = (AFSBTreeEntry *)pProcessCB;
            }
            else
            {

                AFSInsertHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                    &pProcessCB->TreeEntry);
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSDefaultDispatch( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp)
{

    NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PIO_STACK_LOCATION  pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    AFSCompleteRequest( Irp, 
                        ntStatus);

    return ntStatus;
}

NTSTATUS
AFSInitializeDirectory( IN AFSFcb *Dcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEntryCB *pDirNode = NULL;
    AFSDirEnumEntry stDirEnumEntry;
    UNICODE_STRING uniDirName;

    __Enter
    {

        uniDirName.Length = 0;
        uniDirName.MaximumLength = 2 * sizeof( WCHAR);

        uniDirName.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                            uniDirName.MaximumLength,
                                                            AFS_GENERIC_MEMORY_TAG);

        if( uniDirName.Buffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( &stDirEnumEntry,
                       sizeof( AFSDirEnumEntry));

        stDirEnumEntry.CreationTime = Dcb->ParentFcb->DirEntry->DirectoryEntry.CreationTime;

        stDirEnumEntry.LastAccessTime = Dcb->ParentFcb->DirEntry->DirectoryEntry.LastAccessTime;

        stDirEnumEntry.LastWriteTime = Dcb->ParentFcb->DirEntry->DirectoryEntry.LastAccessTime;

        stDirEnumEntry.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

        stDirEnumEntry.FileType = AFS_FILE_TYPE_DIRECTORY;

        uniDirName.Length = sizeof( WCHAR);

        uniDirName.Buffer[ 0] = L'.';

        uniDirName.Buffer[ 1] = L'\0';

        //
        // Initialize the entry
        //

        pDirNode = AFSInitDirEntry( &Dcb->ParentFcb->DirEntry->DirectoryEntry.FileId,
                                    &uniDirName,
                                    NULL,
                                    &stDirEnumEntry,
                                    (ULONG)-1);

        if( pDirNode == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // And insert the node into the directory list
        // This is the first entry in the lsit
        //

        Dcb->Specific.Directory.DirectoryNodeListHead = pDirNode;

        Dcb->Specific.Directory.DirectoryNodeListTail = pDirNode;

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE | AFS_DIR_ENTRY_FAKE | AFS_DIR_ENTRY_VALID);

        //
        // And the .. entry
        //

        uniDirName.Length = 2 * sizeof( WCHAR);

        uniDirName.Buffer[ 0] = L'.';

        uniDirName.Buffer[ 1] = L'.';

        //
        // Initialize the entry
        //

        pDirNode = AFSInitDirEntry( &Dcb->ParentFcb->DirEntry->DirectoryEntry.FileId,
                                    &uniDirName,
                                    NULL,
                                    &stDirEnumEntry,
                                    (ULONG)-2);

        if( pDirNode == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // And insert the node into the directory list
        // This is the second entry in the list
        //

        Dcb->Specific.Directory.DirectoryNodeListTail->ListEntry.fLink = (void *)pDirNode;

        pDirNode->ListEntry.bLink = (void *)Dcb->Specific.Directory.DirectoryNodeListTail;

        Dcb->Specific.Directory.DirectoryNodeListTail = pDirNode;

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE | AFS_DIR_ENTRY_FAKE | AFS_DIR_ENTRY_VALID);

try_exit:

        if( uniDirName.Buffer != NULL)
        {

            ExFreePool( uniDirName.Buffer);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            if( Dcb->Specific.Directory.DirectoryNodeListHead != NULL)
            {

                ExFreePool( Dcb->Specific.Directory.DirectoryNodeListHead);

                Dcb->Specific.Directory.DirectoryNodeListHead = NULL;

                Dcb->Specific.Directory.DirectoryNodeListTail = NULL;
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSInitNonPagedDirEntry( IN AFSDirEntryCB *DirNode)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        //
        // Initialize the dir entry resource
        //

        ExInitializeResourceLite( &DirNode->NPDirNode->Lock);

    }

    return ntStatus;
}

AFSDirEntryCB *
AFSInitDirEntry( IN AFSFileID *ParentFileID,
                 IN PUNICODE_STRING FileName,
                 IN PUNICODE_STRING TargetName,
                 IN AFSDirEnumEntry *DirEnumEntry,
                 IN ULONG FileIndex)
{

    AFSDirEntryCB *pDirNode = NULL;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulEntryLength = 0;
    AFSDirEnumEntry *pDirEnumCB = NULL;
    AFSFileID stTargetFileID;
    AFSFcb *pVcb = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {
        ulEntryLength = sizeof( AFSDirEntryCB) + 
                                     FileName->Length;
        
        if( TargetName != NULL)
        {
            
            ulEntryLength += TargetName->Length;
        }

        pDirNode = (AFSDirEntryCB *)ExAllocatePoolWithTag( PagedPool,         
                                                           ulEntryLength,
                                                           AFS_DIR_ENTRY_TAG);

        if( pDirNode == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSAllocationMemoryLevel += ulEntryLength;

        RtlZeroMemory( pDirNode,
                       ulEntryLength);

        //
        // Allocate the non paged portion
        //

        pDirNode->NPDirNode = (AFSNonPagedDirNode *)ExAllocatePoolWithTag( NonPagedPool,  
                                                                           sizeof( AFSNonPagedDirNode),
                                                                           AFS_DIR_ENTRY_NP_TAG);

        if( pDirNode->NPDirNode == NULL)
        {

            ExFreePool( pDirNode);

            AFSAllocationMemoryLevel -= ulEntryLength;

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Initialize the non-paged portion of the dire entry
        //

        AFSInitNonPagedDirEntry( pDirNode);

        //
        // Set valid entry
        //

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_VALID);

        //
        // Setup the names in the entry
        //

        if( FileName->Length > 0)
        {

            pDirNode->DirectoryEntry.FileName.Length = FileName->Length;

            pDirNode->DirectoryEntry.FileName.MaximumLength = FileName->Length;

            pDirNode->DirectoryEntry.FileName.Buffer = (WCHAR *)((char *)pDirNode + sizeof( AFSDirEntryCB));

            RtlCopyMemory( pDirNode->DirectoryEntry.FileName.Buffer,
                           FileName->Buffer,
                           pDirNode->DirectoryEntry.FileName.Length);       

            //
            // Create a CRC for the file
            //

            pDirNode->CaseSensitiveTreeEntry.HashIndex = AFSGenerateCRC( &pDirNode->DirectoryEntry.FileName,
                                                                         FALSE);

            pDirNode->CaseInsensitiveTreeEntry.HashIndex = AFSGenerateCRC( &pDirNode->DirectoryEntry.FileName,
                                                                           TRUE);
        }

        if( TargetName != NULL &&
            TargetName->Length > 0)
        {

            pDirNode->DirectoryEntry.TargetName.Length = TargetName->Length;

            pDirNode->DirectoryEntry.TargetName.MaximumLength = pDirNode->DirectoryEntry.TargetName.Length;

            pDirNode->DirectoryEntry.TargetName.Buffer = (WCHAR *)((char *)pDirNode + sizeof( AFSDirEntryCB) + pDirNode->DirectoryEntry.FileName.Length);

            RtlCopyMemory( pDirNode->DirectoryEntry.TargetName.Buffer,
                           TargetName->Buffer,
                           pDirNode->DirectoryEntry.TargetName.Length);                                   
        }

        pDirNode->DirectoryEntry.FileIndex = FileIndex; 

        //
        // Populate the rest of the data
        //

        if( ParentFileID != NULL)
        {

            pDirNode->DirectoryEntry.ParentId = *ParentFileID;
        }

        pDirNode->DirectoryEntry.FileId = DirEnumEntry->FileId;

        pDirNode->DirectoryEntry.TargetFileId = DirEnumEntry->TargetFileId;

        pDirNode->DirectoryEntry.Expiration = DirEnumEntry->Expiration;

        pDirNode->DirectoryEntry.DataVersion = DirEnumEntry->DataVersion;

        pDirNode->DirectoryEntry.FileType = DirEnumEntry->FileType;

        pDirNode->DirectoryEntry.CreationTime = DirEnumEntry->CreationTime;

        pDirNode->DirectoryEntry.LastAccessTime = DirEnumEntry->LastAccessTime;

        pDirNode->DirectoryEntry.LastWriteTime = DirEnumEntry->LastWriteTime;

        pDirNode->DirectoryEntry.ChangeTime = DirEnumEntry->ChangeTime;

        pDirNode->DirectoryEntry.EndOfFile = DirEnumEntry->EndOfFile;

        pDirNode->DirectoryEntry.AllocationSize = DirEnumEntry->AllocationSize;

        pDirNode->DirectoryEntry.FileAttributes = DirEnumEntry->FileAttributes;

        pDirNode->DirectoryEntry.EaSize = DirEnumEntry->EaSize;

        pDirNode->DirectoryEntry.Links = DirEnumEntry->Links;

        //
        // Check for the case where we ahve a filetype of SymLink but both the TargetFid and the 
        // TagrteName are empty. In this case set the filetype to zero so we evaluate it later in
        // the code
        //

        if( pDirNode->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK && 
            pDirNode->DirectoryEntry.TargetFileId.Vnode == 0 &&
            pDirNode->DirectoryEntry.TargetFileId.Unique == 0 &&
            pDirNode->DirectoryEntry.TargetName.Length == 0)
        {

            //
            // This will ensure we perform a validation on the node
            //

            pDirNode->DirectoryEntry.FileType = 0;
        }

        if( pDirNode->DirectoryEntry.FileType == 0)
        {

            SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
        }

        if( pDirNode->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY ||
            pDirNode->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK ||
            pDirNode->DirectoryEntry.FileType == AFS_FILE_TYPE_DFSLINK)
        {

            pDirNode->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        }
        else if( pDirNode->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
        {

            pDirNode->DirectoryEntry.FileAttributes |= (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY);
        }

        //
        // If we are hiding DOT names then check this entry
        //

        if( BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_HIDE_DOT_NAMES) &&
            FileName->Buffer[ 0] == L'.')
        {

            pDirNode->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
        }

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pDirNode != NULL)
            {

                if( pDirNode->NPDirNode != NULL)
                {

                    ExDeleteResourceLite( &pDirNode->NPDirNode->Lock);

                    ExFreePool( pDirNode->NPDirNode);
                }

                ExFreePool( pDirNode);

                pDirNode = NULL;

                AFSAllocationMemoryLevel -= ulEntryLength;
            }
        }
    }

    return pDirNode;
}

BOOLEAN
AFSCheckForReadOnlyAccess( IN ACCESS_MASK DesiredAccess)
{

    BOOLEAN bReturn = TRUE;

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

    if( FlagOn( DesiredAccess, ~( READ_CONTROL |
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
                                 FILE_TRAVERSE))) 
    {

        //
        // A write access is set ...
        //

        bReturn = FALSE;
    }

    return bReturn;
}

NTSTATUS
AFSEvaluateNode( IN AFSFcb *Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEnumEntry *pDirEntry = NULL;

    __Enter
    {

        ntStatus = AFSEvaluateTargetByID( &Fcb->DirEntry->DirectoryEntry.FileId,
                                          0,
                                          FALSE,
                                          &pDirEntry);

        if( !NT_SUCCESS( ntStatus) ||
            pDirEntry->FileType == 0)
        {

            try_return( ntStatus = STATUS_OBJECT_NAME_NOT_FOUND);
        }

        Fcb->DirEntry->DirectoryEntry.TargetFileId = pDirEntry->TargetFileId;

        Fcb->DirEntry->DirectoryEntry.Expiration = pDirEntry->Expiration;

        Fcb->DirEntry->DirectoryEntry.DataVersion = pDirEntry->DataVersion;

        //
        // If we have a different file type we may need to switch fcb
        //

        if( pDirEntry->FileType != Fcb->DirEntry->DirectoryEntry.FileType)
        {

            ASSERT( Fcb->DirEntry->DirectoryEntry.FileType == 0);

            //
            // We default to a file so update the entry according to the new file type
            //

            switch( pDirEntry->FileType)
            {

                case AFS_FILE_TYPE_FILE:
                {

                    //
                    // Nothing to do
                    //

                    break;
                }

                case AFS_FILE_TYPE_DIRECTORY:
                {

                    //
                    // Tear down file specific stuff
                    //

                    FsRtlUninitializeFileLock( &Fcb->Specific.File.FileLock);

                    ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.ExtentsResource);

                    ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.DirtyExtentsListLock);

                    //
                    // And init the directory specific stuff
                    //

                    Fcb->Header.NodeTypeCode = AFS_DIRECTORY_FCB;

                    //
                    // Lock for the directory tree
                    //

                    ExInitializeResourceLite( &Fcb->NPFcb->Specific.Directory.DirectoryTreeLock);

                    Fcb->Specific.Directory.DirectoryNodeHdr.TreeLock = &Fcb->NPFcb->Specific.Directory.DirectoryTreeLock;

                    //
                    // Initialize the directory
                    //

                    AFSInitializeDirectory( Fcb);

                    break;
                }

                case AFS_FILE_TYPE_MOUNTPOINT:
                {

                    //
                    // Tear down file specific stuff
                    //

                    FsRtlUninitializeFileLock( &Fcb->Specific.File.FileLock);

                    ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.ExtentsResource);

                    ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.DirtyExtentsListLock);

                    Fcb->Header.NodeTypeCode = AFS_MOUNT_POINT_FCB;

                    Fcb->Specific.MountPoint.TargetFcb = NULL;

                    break;
                }

                case AFS_FILE_TYPE_SYMLINK:
                {

                    //
                    // Tear down file specific stuff
                    //

                    FsRtlUninitializeFileLock( &Fcb->Specific.File.FileLock);

                    ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.ExtentsResource);

                    ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.DirtyExtentsListLock);

                    Fcb->Header.NodeTypeCode = AFS_SYMBOLIC_LINK_FCB;

                    Fcb->Specific.SymbolicLink.TargetFcb = NULL;

                    break;
                }

                case AFS_FILE_TYPE_DFSLINK:
                {

                    UNICODE_STRING uniTargetName;

                    //
                    // Tear down file specific stuff
                    //

                    FsRtlUninitializeFileLock( &Fcb->Specific.File.FileLock);

                    ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.ExtentsResource);

                    ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.DirtyExtentsListLock);

                    Fcb->Header.NodeTypeCode = AFS_DFS_LINK_FCB;

                    //
                    // Update the target name information if needed
                    //

                    uniTargetName.Length = (USHORT)pDirEntry->TargetNameLength;

                    uniTargetName.MaximumLength = uniTargetName.Length;

                    uniTargetName.Buffer = (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset);

                    if( Fcb->DirEntry->DirectoryEntry.TargetName.Length == 0 ||
                        RtlCompareUnicodeString( &uniTargetName,
                                                 &Fcb->DirEntry->DirectoryEntry.TargetName,
                                                 TRUE) != 0)
                    {

                        //
                        // If we have enough space then just move in the name otherwise
                        // allocate a new buffer
                        //

                        if( Fcb->DirEntry->DirectoryEntry.TargetName.Length < uniTargetName.Length)
                        {

                            WCHAR *pTmpBuffer = NULL;

                            pTmpBuffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                                         uniTargetName.Length,
                                                                         AFS_NAME_BUFFER_TAG);

                            if( pTmpBuffer != NULL)
                            {

                                ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                                break;
                            }

                            if( BooleanFlagOn( Fcb->DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER))
                            {

                                ExFreePool( Fcb->DirEntry->DirectoryEntry.TargetName.Buffer);
                            }

                            Fcb->DirEntry->DirectoryEntry.TargetName.MaximumLength = uniTargetName.Length;

                            Fcb->DirEntry->DirectoryEntry.TargetName.Buffer = pTmpBuffer;    

                            SetFlag( Fcb->DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER);
                        }

                        Fcb->DirEntry->DirectoryEntry.TargetName.Length = uniTargetName.Length;

                        RtlCopyMemory( Fcb->DirEntry->DirectoryEntry.TargetName.Buffer,
                                       uniTargetName.Buffer,
                                       Fcb->DirEntry->DirectoryEntry.TargetName.Length);
                    }

                    break;
                }

                default:

                    ASSERT( FALSE);

                    break;
            }
        }

        Fcb->DirEntry->DirectoryEntry.FileType = pDirEntry->FileType;

        Fcb->DirEntry->DirectoryEntry.CreationTime = pDirEntry->CreationTime;

        Fcb->DirEntry->DirectoryEntry.LastAccessTime = pDirEntry->LastAccessTime;

        Fcb->DirEntry->DirectoryEntry.LastWriteTime = pDirEntry->LastWriteTime;

        Fcb->DirEntry->DirectoryEntry.ChangeTime = pDirEntry->ChangeTime;

        Fcb->DirEntry->DirectoryEntry.EndOfFile = pDirEntry->EndOfFile;

        Fcb->DirEntry->DirectoryEntry.AllocationSize = pDirEntry->AllocationSize;

        Fcb->DirEntry->DirectoryEntry.FileAttributes = pDirEntry->FileAttributes;

        Fcb->DirEntry->DirectoryEntry.EaSize = pDirEntry->EaSize;

        Fcb->DirEntry->DirectoryEntry.Links = pDirEntry->Links;

        if( Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY ||
            Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK ||
            Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DFSLINK)
        {

            Fcb->DirEntry->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        }
        else if( Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
        {

            Fcb->DirEntry->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
        }

try_exit:

        if( pDirEntry != NULL)
        {

            ExFreePool( pDirEntry);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSValidateSymLink( IN AFSDirEntryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEnumEntry *pDirEntry = NULL;
    UNICODE_STRING uniTargetName;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSValidateSymLink Validating possible DFS Link %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      &DirEntry->DirectoryEntry.FileName,
                      DirEntry->DirectoryEntry.FileId.Cell,
                      DirEntry->DirectoryEntry.FileId.Volume,
                      DirEntry->DirectoryEntry.FileId.Vnode,
                      DirEntry->DirectoryEntry.FileId.Unique);

        ntStatus = AFSEvaluateTargetByID( &DirEntry->DirectoryEntry.FileId,
                                          0,
                                          FALSE,
                                          &pDirEntry);

        if( !NT_SUCCESS( ntStatus) ||
            pDirEntry->FileType == 0)
        {

            try_return( ntStatus = STATUS_OBJECT_NAME_NOT_FOUND);
        }

        DirEntry->DirectoryEntry.TargetFileId = pDirEntry->TargetFileId;

        DirEntry->DirectoryEntry.Expiration = pDirEntry->Expiration;

        DirEntry->DirectoryEntry.DataVersion = pDirEntry->DataVersion;

        //
        // If the FileType is the same then nothing to do since it IS
        // a SymLink
        //

        if( pDirEntry->FileType == DirEntry->DirectoryEntry.FileType)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSValidateSymLink Entry %wZ FID %08lX-%08lX-%08lX-%08lX is a SymLink\n",
                          &DirEntry->DirectoryEntry.FileName,
                          DirEntry->DirectoryEntry.FileId.Cell,
                          DirEntry->DirectoryEntry.FileId.Volume,
                          DirEntry->DirectoryEntry.FileId.Vnode,
                          DirEntry->DirectoryEntry.FileId.Unique);

            ASSERT( pDirEntry->FileType == AFS_FILE_TYPE_SYMLINK);

            try_return( ntStatus = STATUS_SUCCESS);
        }

        ASSERT( pDirEntry->FileType == AFS_FILE_TYPE_DFSLINK);

        DirEntry->DirectoryEntry.FileType = pDirEntry->FileType;

        DirEntry->DirectoryEntry.CreationTime = pDirEntry->CreationTime;

        DirEntry->DirectoryEntry.LastAccessTime = pDirEntry->LastAccessTime;

        DirEntry->DirectoryEntry.LastWriteTime = pDirEntry->LastWriteTime;

        DirEntry->DirectoryEntry.ChangeTime = pDirEntry->ChangeTime;

        DirEntry->DirectoryEntry.EndOfFile = pDirEntry->EndOfFile;

        DirEntry->DirectoryEntry.AllocationSize = pDirEntry->AllocationSize;

        DirEntry->DirectoryEntry.FileAttributes = pDirEntry->FileAttributes;

        DirEntry->DirectoryEntry.EaSize = pDirEntry->EaSize;

        DirEntry->DirectoryEntry.Links = pDirEntry->Links;

        DirEntry->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

        //
        // Update the target name information if needed
        //

        uniTargetName.Length = (USHORT)pDirEntry->TargetNameLength;

        uniTargetName.MaximumLength = uniTargetName.Length;

        uniTargetName.Buffer = (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset);

        if( uniTargetName.Length == 0)
        {

            try_return( ntStatus = STATUS_SUCCESS);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSValidateSymLink Updating entry %wZ FID %08lX-%08lX-%08lX-%08lX to a DFS Link with target %wZ\n",
                      &DirEntry->DirectoryEntry.FileName,
                      DirEntry->DirectoryEntry.FileId.Cell,
                      DirEntry->DirectoryEntry.FileId.Volume,
                      DirEntry->DirectoryEntry.FileId.Vnode,
                      DirEntry->DirectoryEntry.FileId.Unique,
                      &uniTargetName);

        if( DirEntry->DirectoryEntry.TargetName.Length == 0 ||
            RtlCompareUnicodeString( &uniTargetName,
                                     &DirEntry->DirectoryEntry.TargetName,
                                     TRUE) != 0)
        {

            //
            // If we have enough space then just move in the name otherwise
            // allocate a new buffer
            //

            if( DirEntry->DirectoryEntry.TargetName.Length < uniTargetName.Length)
            {

                WCHAR *pTmpBuffer = NULL;

                pTmpBuffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                             uniTargetName.Length,
                                                             AFS_NAME_BUFFER_TAG);

                if( pTmpBuffer != NULL)
                {

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER))
                {

                    ExFreePool( DirEntry->DirectoryEntry.TargetName.Buffer);
                }

                DirEntry->DirectoryEntry.TargetName.MaximumLength = uniTargetName.Length;

                DirEntry->DirectoryEntry.TargetName.Buffer = pTmpBuffer;    

                SetFlag( DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER);
            }

            DirEntry->DirectoryEntry.TargetName.Length = uniTargetName.Length;

            RtlCopyMemory( DirEntry->DirectoryEntry.TargetName.Buffer,
                           uniTargetName.Buffer,
                           DirEntry->DirectoryEntry.TargetName.Length);
        }

try_exit:

        if( pDirEntry != NULL)
        {

            ExFreePool( pDirEntry);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSInvalidateCache( IN AFSInvalidateCacheCB *InvalidateCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb      *pDcb = NULL, *pFcb = NULL, *pNextFcb = NULL, *pVcb = NULL;
    AFSFcb      *pTargetDcb = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    AFSDirEntryCB *pCurrentDirEntry = NULL;
    BOOLEAN     bIsChild = FALSE;
    ULONGLONG   ullIndex = 0;

    __Enter
    {

        //
        // Need to locate the Fcb for the directory to purge
        //
    
        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInvalidateCache Acquiring RDR VolumeTreeLock lock %08lX SHARED %08lX\n",
                      &pDevExt->Specific.RDR.VolumeTreeLock,
                      PsGetCurrentThread());

        AFSAcquireShared( &pDevExt->Specific.RDR.VolumeTreeLock, TRUE);

        //
        // Locate the volume node
        //

        ullIndex = AFSCreateHighIndex( &InvalidateCB->FileID);

        ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                       ullIndex,
                                       &pVcb);

        if( pVcb != NULL)
        {

            InterlockedIncrement( &pVcb->OpenReferenceCount);
        }

        AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

        if( !NT_SUCCESS( ntStatus) ||
            pVcb == NULL) 
        {
            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // If this is a whole volume invalidation then go do it now
        //

        if( InvalidateCB->WholeVolume)
        {

            if( InvalidateCB->Reason == AFS_INVALIDATE_DELETED)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInvalidateCache Acquiring Vcb lock %08lX EXCL %08lX\n",
                              &pVcb->NPFcb->Resource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &pVcb->NPFcb->Resource,
                                TRUE);

                ntStatus = AFSInvalidateVolume( pVcb,
                                                InvalidateCB->Reason);

                AFSReleaseResource( &pVcb->NPFcb->Resource);
            }

            try_return( ntStatus);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInvalidateCache Acquiring VolumeRoot FileIDTree.TreeLock lock %08lX SHARED %08lX\n",
                                                                   pVcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                                                   PsGetCurrentThread());

        AFSAcquireShared( pVcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                          TRUE);

        ASSERT( pVcb->OpenReferenceCount != 0);

        InterlockedDecrement( &pVcb->OpenReferenceCount);

        //
        // If this is a volume fid then don;t worry about looking it up we ahve it
        //

        if( !AFSIsVolumeFID( &InvalidateCB->FileID))
        {

            //
            // Now locate the Fcb in this volume
            //

            ullIndex = AFSCreateLowIndex( &InvalidateCB->FileID);

            ntStatus = AFSLocateHashEntry( pVcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                           ullIndex,
                                           &pDcb);
        }
        else
        {

            pDcb = pVcb;
        }

        if( pDcb != NULL)
        {

            //
            // Reference the node so it won't be torn down
            //

            InterlockedIncrement( &pDcb->OpenReferenceCount);
        }

        AFSReleaseResource( pVcb->Specific.VolumeRoot.FileIDTree.TreeLock);

        if( !NT_SUCCESS( ntStatus) ||
            pDcb == NULL) 
        {
            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // Get the target of the node if it is a mount point or sym link
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInvalidateCache Invalidating Fcb %wZ FID %08lX-%08lX-%08lX-%08lX Reason %08lX\n",
                      &pDcb->DirEntry->DirectoryEntry.FileName,
                      pDcb->DirEntry->DirectoryEntry.FileId.Cell,
                      pDcb->DirEntry->DirectoryEntry.FileId.Volume,
                      pDcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      pDcb->DirEntry->DirectoryEntry.FileId.Unique,
                      InvalidateCB->Reason);

        if( pDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK ||
            pDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
        {

            //
            // We only act on the mount point itself, not the target. If the 
            // node has been deleted then mark it as such otherwise indicate
            // it requires verification
            //

            if( InvalidateCB->Reason == AFS_INVALIDATE_DELETED)
            {

                SetFlag( pDcb->Flags, AFS_FCB_INVALID);
            }
            else
            {

                if( InvalidateCB->Reason == AFS_INVALIDATE_FLUSHED)
                {

                    pDcb->DirEntry->DirectoryEntry.DataVersion.QuadPart = (ULONGLONG)-1;
                }

                SetFlag( pDcb->Flags, AFS_FCB_VERIFY);
            }

            ASSERT( pDcb->OpenReferenceCount != 0);

            InterlockedDecrement( &pDcb->OpenReferenceCount);

            try_return( ntStatus);
        }

        //
        // Depending on the reason for invalidation then perform work on the node
        //

        switch( InvalidateCB->Reason)
        {

            case AFS_INVALIDATE_DELETED:
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInvalidateCache Acquiring Dcb lock %08lX EXCL %08lX\n",
                              &pDcb->NPFcb->Resource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &pDcb->NPFcb->Resource,
                                TRUE);

                ASSERT( pDcb->OpenReferenceCount != 0);

                InterlockedDecrement( &pDcb->OpenReferenceCount);

                //
                // Mark this node as invalid
                //

                SetFlag( pDcb->Flags, AFS_FCB_INVALID);

                //
                // In this case we have the most work to do ...
                // Perform the invalidation depending on the type of node
                //

                switch( pDcb->DirEntry->DirectoryEntry.FileType)
                {

                    case AFS_FILE_TYPE_DIRECTORY:
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSInvalidateCache Acquiring DirectoryNodeHdr.TreeLock lock %08lX EXCL %08lX\n",
                                      pDcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                      PsGetCurrentThread());

                        AFSAcquireExcl( pDcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                        TRUE);

                        //
                        // Reset the directory list information
                        //

                        pCurrentDirEntry = pDcb->Specific.Directory.DirectoryNodeListHead;

                        while( pCurrentDirEntry != NULL)
                        {

                            pFcb = pCurrentDirEntry->Fcb;

                            if( pFcb != NULL)
                            {

                                //
                                // Do this prior to blocking the Fcb since there could be a thread
                                // holding the Fcb waiting for an extent
                                //

                                if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                                {

                                    //
                                    // Clear out anybody waiting for the extents.
                                    //

                                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                                  AFS_TRACE_LEVEL_VERBOSE,
                                                  "AFSInvalidateCache Acquiring Fcb extents lock %08lX EXCL %08lX\n",
                                                  &pFcb->NPFcb->Specific.File.ExtentsResource,
                                                  PsGetCurrentThread());

                                    AFSAcquireExcl( &pFcb->NPFcb->Specific.File.ExtentsResource, 
                                                    TRUE);

                                    pFcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                                    KeSetEvent( &pFcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                                0,
                                                FALSE);

                                    AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource);
                                       
                                    //
                                    // And get rid of them (note this involves waiting
                                    // for any writes or reads to the cache to complete)
                                    //

                                    AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                                  AFS_TRACE_LEVEL_VERBOSE,
                                                  "AFSInvalidateCache Tearing down extents for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                                  &pFcb->DirEntry->DirectoryEntry.FileName,
                                                  pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                                  pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                                  pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                                  pFcb->DirEntry->DirectoryEntry.FileId.Unique);        

                                    (VOID) AFSTearDownFcbExtents( pFcb);
                                        
                                }

                                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                              AFS_TRACE_LEVEL_VERBOSE,
                                              "AFSInvalidateCache Acquiring Fcb lock %08lX EXCL %08lX\n",
                                              &pFcb->NPFcb->Resource,
                                              PsGetCurrentThread());

                                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                                TRUE);

                                if( pFcb->DirEntry != NULL)
                                {

                                    pFcb->DirEntry->Fcb = NULL;

                                    SetFlag( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE);
                                }

                                SetFlag( pFcb->Flags, AFS_FCB_INVALID);

                                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                              AFS_TRACE_LEVEL_VERBOSE,
                                              "AFSInvalidateCache Acquiring VolumeRoot FileIDTRee.TreeLock lock %08lX EXCL %08lX\n",
                                              pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                              PsGetCurrentThread());

                                AFSAcquireExcl( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                                TRUE);

                                AFSRemoveHashEntry( &pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                                    &pFcb->TreeEntry);

                                AFSReleaseResource( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock);

                                ClearFlag( pFcb->Flags, AFS_FCB_INSERTED_ID_TREE);

                                AFSReleaseResource( &pFcb->NPFcb->Resource);

                                AFSRemoveDirNodeFromParent( pDcb,
                                                            pCurrentDirEntry);
                            }
                            else
                            {

                                AFSDeleteDirEntry( pDcb,
                                                   pCurrentDirEntry);
                            }

                            pCurrentDirEntry = pDcb->Specific.Directory.DirectoryNodeListHead;
                        }

                        pDcb->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead = NULL;

                        pDcb->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead = NULL;

                        pDcb->Specific.Directory.DirectoryNodeListHead = NULL;

                        pDcb->Specific.Directory.DirectoryNodeListTail = NULL;

                        ClearFlag( pDcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED);

                        AFSReleaseResource( pDcb->Specific.Directory.DirectoryNodeHdr.TreeLock);

                        //
                        // Tag the Fcbs as invalid so they are torn down
                        // Do this for any which has this node as a parent or
                        // this parent somewhere in the chain
                        //

                        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSInvalidateCache Acquiring VolumeRoot FcbListLock lock %08lX SHARED %08lX\n",
                                      pDcb->RootFcb->Specific.VolumeRoot.FcbListLock,
                                      PsGetCurrentThread());

                        AFSAcquireShared( pDcb->RootFcb->Specific.VolumeRoot.FcbListLock,
                                          TRUE);

                        pFcb = pDcb->RootFcb->Specific.VolumeRoot.FcbListHead;

                        while( pFcb != NULL)
                        {

                            bIsChild = AFSIsChildOfParent( pDcb,
                                                           pFcb);

                            if( bIsChild)
                            {
                                if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                                {

                                    //
                                    // Clear out the extents
                                    //

                                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                                  AFS_TRACE_LEVEL_VERBOSE,
                                                  "AFSInvalidateCache Acquiring Fcb extents lock %08lX EXCL %08lX\n",
                                                  &pFcb->NPFcb->Specific.File.ExtentsResource,
                                                  PsGetCurrentThread());

                                    AFSAcquireExcl( &pFcb->NPFcb->Specific.File.ExtentsResource, 
                                                    TRUE);

                                    pFcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                                    KeSetEvent( &pFcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                                0,
                                                FALSE);

                                    AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource);
                                }

                                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                              AFS_TRACE_LEVEL_VERBOSE,
                                              "AFSInvalidateCache Acquiring Fcb lock %08lX EXCL %08lX\n",
                                                                                 &pFcb->NPFcb->Resource,
                                                                                 PsGetCurrentThread());

                                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                                TRUE);

                                SetFlag( pFcb->Flags, AFS_FCB_INVALID);

                                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                              AFS_TRACE_LEVEL_VERBOSE,
                                              "AFSInvalidateCache Acquiring VolumeRoot FileIDTree.TreeLock lock %08lX EXCL %08lX\n",
                                              pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                              PsGetCurrentThread());

                                AFSAcquireExcl( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                                TRUE);

                                AFSRemoveHashEntry( &pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                                    &pFcb->TreeEntry);

                                AFSReleaseResource( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock);

                                ClearFlag( pFcb->Flags, AFS_FCB_INSERTED_ID_TREE);

                                AFSReleaseResource( &pFcb->NPFcb->Resource);
                            }

                            pNextFcb = (AFSFcb *)pFcb->ListEntry.fLink;
                            
                            pFcb = pNextFcb;
                        }

                        AFSReleaseResource( pDcb->RootFcb->Specific.VolumeRoot.FcbListLock);

                        break;
                    }

                    case AFS_FILE_TYPE_FILE:
                    {

                        //
                        // We need to drop the resource here to allow any pending requests through prior
                        // to acquiring the extent lock.
                        //

                        AFSReleaseResource( &pDcb->NPFcb->Resource);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSInvalidateCache Acquiring Fcb extents lock %08lX EXCL %08lX\n",
                                      &pDcb->NPFcb->Specific.File.ExtentsResource,
                                      PsGetCurrentThread());

                        AFSAcquireExcl( &pDcb->NPFcb->Specific.File.ExtentsResource, 
                                        TRUE);

                        pDcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                        KeSetEvent( &pDcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                    0,
                                    FALSE);

                        AFSReleaseResource( &pDcb->NPFcb->Specific.File.ExtentsResource);
                                       
                        //
                        // And get rid of them (note this involves waiting
                        // for any writes or reads to the cache to complete)
                        //

                        AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSInvalidateCache Tearing down extents for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      &pDcb->DirEntry->DirectoryEntry.FileName,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Unique);        

                        (VOID) AFSTearDownFcbExtents( pDcb);

                        break;
                    }

                    default:
                    {

                        ASSERT( FALSE);

                        break;
                    }
                }

                // Need to acquire the parent node in this case so drop the Fcb and re-acquire in correct
                // order. Of course do this only if the dir entry is in the parent list
                //

                if( !BooleanFlagOn( pDcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE) &&
                    pDcb->ParentFcb != NULL)
                {
                   
                    if( ExIsResourceAcquiredLite( &pDcb->NPFcb->Resource))
                    {

                        AFSReleaseResource( &pDcb->NPFcb->Resource);
                    }

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSInvalidateCache Acquiring ParentFcb lock %08lX EXCL %08lX\n",
                                  &pDcb->ParentFcb->NPFcb->Resource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pDcb->ParentFcb->NPFcb->Resource,
                                    TRUE);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSInvalidateCache Acquiring Fcb lock %08lX EXCL %08lX\n",
                                  &pDcb->NPFcb->Resource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pDcb->NPFcb->Resource,
                                    TRUE);

                    //
                    // Remove the Dir entry from the parent list
                    //

                    AFSRemoveDirNodeFromParent( pDcb->ParentFcb,
                                                pDcb->DirEntry);

                    SetFlag( pDcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE);

                    AFSReleaseResource( &pDcb->ParentFcb->NPFcb->Resource);
                }

                AFSReleaseResource( &pDcb->NPFcb->Resource);

                break;
            }

            case AFS_INVALIDATE_FLUSHED:
            {

                pDcb->DirEntry->DirectoryEntry.DataVersion.QuadPart = (ULONGLONG)-1;

                if( pDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_FILE)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSInvalidateCache (AFS_INVALIDATE_FLUSHED) Invalidating file entry for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &pDcb->DirEntry->DirectoryEntry.FileName,
                                  pDcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pDcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pDcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pDcb->DirEntry->DirectoryEntry.FileId.Unique);        

                    //
                    // Purge the system cache for the file
                    //

                    AFSAcquireExcl( &pDcb->NPFcb->Resource,
                                    TRUE);

                    __try
                    {

                        if( !CcPurgeCacheSection( &pDcb->NPFcb->SectionObjectPointers,
                                                  NULL,
                                                  0,
                                                  FALSE))
                        {

                            ntStatus = STATUS_USER_MAPPED_FILE;
                        }
                    }
                    __except( EXCEPTION_EXECUTE_HANDLER)
                    {
                    
                        ntStatus = GetExceptionCode();
                    }

                    AFSReleaseResource( &pDcb->NPFcb->Resource);

                    //
                    // Wait on the in progress queued flushes
                    //

                    AFSWaitOnQueuedFlushes( pDcb);

                    (VOID) AFSTearDownFcbExtents( pDcb);

                    ASSERT( pDcb->OpenReferenceCount != 0);

                    InterlockedDecrement( &pDcb->OpenReferenceCount);

                    break;
                }

                // Fall through to the default processing
            }

            default:
            {

                //
                // Indicate this node requires re-evaluation for the remaining reasons
                //

                SetFlag( pDcb->Flags, AFS_FCB_VERIFY);

                ASSERT( pDcb->OpenReferenceCount != 0);

                InterlockedDecrement( &pDcb->OpenReferenceCount);

                break;
            }
        }        

try_exit:

        NOTHING;
    }

    return ntStatus;
}

BOOLEAN
AFSIsChildOfParent( IN AFSFcb *Dcb,
                    IN AFSFcb *Fcb)
{

    BOOLEAN bIsChild = FALSE;
    AFSFcb *pCurrentFcb = Fcb;

    while( pCurrentFcb != NULL)
    {

        if( pCurrentFcb->ParentFcb == Dcb)
        {

            bIsChild = TRUE;

            break;
        }

        pCurrentFcb = pCurrentFcb->ParentFcb;
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
                IN ACCESS_MASK GrantedAccess)
{

    BOOLEAN bAccessGranted = TRUE;

    //
    // Check if we are asking for read/write and granted only read only
    // NOTE: There will be more checks here
    //

    if( !AFSCheckForReadOnlyAccess( DesiredAccess) &&
        AFSCheckForReadOnlyAccess( GrantedAccess))
    {

        bAccessGranted = FALSE;
    }

    return bAccessGranted;
}

NTSTATUS
AFSGetDriverStatus( IN AFSDriverStatusRespCB *DriverStatus)
{

    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSDeviceExt    *pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    //
    // Start with read
    //

    DriverStatus->Status = AFS_DRIVER_STATUS_READY;

    if( AFSGlobalRoot == NULL ||
        !BooleanFlagOn( AFSGlobalRoot->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
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
AFSSetSysNameInformation( IN AFSSysNameNotificationCB *SysNameInfo,
                          IN ULONG SysNameInfoBufferLength)
{

    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSDeviceExt    *pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSSysNameCB    *pSysName = NULL;
    ERESOURCE       *pSysNameLock = NULL;
    AFSSysNameCB   **pSysNameListHead = NULL, **pSysNameListTail = NULL;
    ULONG            ulIndex = 0;
    __Enter
    {

        //
        // Depending on the architecture of the information, set up the lsit
        //

        if( SysNameInfo->Architecture == AFS_SYSNAME_ARCH_32BIT)
        {

            pSysNameLock = &pControlDevExt->Specific.Control.SysName32ListLock;

            pSysNameListHead = &pControlDevExt->Specific.Control.SysName32ListHead;

            pSysNameListTail = &pControlDevExt->Specific.Control.SysName32ListTail;
        }
        else
        {

#if defined(_WIN64)

            pSysNameLock = &pControlDevExt->Specific.Control.SysName64ListLock;

            pSysNameListHead = &pControlDevExt->Specific.Control.SysName64ListHead;

            pSysNameListTail = &pControlDevExt->Specific.Control.SysName64ListTail;

#else

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
#endif
        }

        //
        // Process the request
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetSysNameInformation Acquiring SysName lock %08lX EXCL %08lX\n",
                      pSysNameLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( pSysNameLock,
                        TRUE);

        //
        // If we already have a list, then tear it down
        //

        if( *pSysNameListHead != NULL)
        {

            AFSResetSysNameList( *pSysNameListHead);

            *pSysNameListHead = NULL;
        }

        //
        // Loop through the entries adding in a node for each
        //

        while( ulIndex < SysNameInfo->NumberOfNames)
        {

            pSysName = (AFSSysNameCB *)ExAllocatePoolWithTag( PagedPool,
                                                              sizeof( AFSSysNameCB) + 
                                                                        SysNameInfo->SysNames[ ulIndex].Length +
                                                                        sizeof( WCHAR),
                                                              AFS_SYS_NAME_NODE_TAG);

            if( pSysName == NULL)
            {

                //
                // Reset the current list
                //

                AFSResetSysNameList( *pSysNameListHead);

                *pSysNameListHead = NULL;

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlZeroMemory( pSysName,
                           sizeof( AFSSysNameCB) + 
                                   SysNameInfo->SysNames[ ulIndex].Length +
                                   sizeof( WCHAR));

            pSysName->SysName.Length = (USHORT)SysNameInfo->SysNames[ ulIndex].Length;

            pSysName->SysName.MaximumLength = pSysName->SysName.Length + sizeof( WCHAR);

            pSysName->SysName.Buffer = (WCHAR *)((char *)pSysName + sizeof( AFSSysNameCB));

            RtlCopyMemory( pSysName->SysName.Buffer,
                           SysNameInfo->SysNames[ ulIndex].String,
                           pSysName->SysName.Length);

            if( *pSysNameListHead == NULL)
            {

                *pSysNameListHead = pSysName;
            }
            else
            {

                (*pSysNameListTail)->fLink = pSysName;
            }

            *pSysNameListTail = pSysName;                               

            ulIndex++;
        }

try_exit:


        AFSReleaseResource( pSysNameLock);
    }

    return ntStatus;
}

void
AFSResetSysNameList( IN AFSSysNameCB *SysNameList)
{

    AFSSysNameCB *pNextEntry = NULL, *pCurrentEntry = SysNameList;

    while( pCurrentEntry != NULL)
    {

        pNextEntry = pCurrentEntry->fLink;

        ExFreePool( pCurrentEntry);

        pCurrentEntry = pNextEntry;
    }

    return;
}

NTSTATUS
AFSSubstituteSysName( IN UNICODE_STRING *ComponentName,
                      IN UNICODE_STRING *SubstituteName,
                      IN ULONG StringIndex)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt    *pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
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

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSubstituteSysName Acquiring SysName lock %08lX SHARED %08lX\n",
                      pSysNameLock,
                      PsGetCurrentThread());

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

            SubstituteName->Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                                     SubstituteName->Length,
                                                                     AFS_NAME_BUFFER_TAG);

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

            SubstituteName->Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                                     SubstituteName->Length,
                                                                     AFS_NAME_BUFFER_TAG);

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
AFSSubstituteNameInPath( IN UNICODE_STRING *FullPathName,
                         IN UNICODE_STRING *ComponentName,
                         IN UNICODE_STRING *SubstituteName,
                         IN BOOLEAN FreePathName)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniPathName;
    USHORT usPrefixNameLen = 0;

    __Enter
    {

        //
        // If the passed in name can handle the additional length
        // then just moves things around
        //

        if( FullPathName->MaximumLength > FullPathName->Length - 
                                                    ComponentName->Length + 
                                                    SubstituteName->Length)
        {

            usPrefixNameLen = (USHORT)(ComponentName->Buffer - FullPathName->Buffer);

            if( FullPathName->Length > usPrefixNameLen + ComponentName->Length)
            {

                RtlMoveMemory( &FullPathName->Buffer[ ((usPrefixNameLen + SubstituteName->Length)/sizeof( WCHAR)) + 1],
                               &FullPathName->Buffer[ ((usPrefixNameLen + ComponentName->Length)/sizeof( WCHAR)) + 1],
                               FullPathName->Length - usPrefixNameLen - ComponentName->Length);
            }

            RtlCopyMemory( &FullPathName->Buffer[ (usPrefixNameLen/sizeof( WCHAR)) + 1],
                           SubstituteName->Buffer,
                           SubstituteName->Length);

            FullPathName->Length = FullPathName->Length - 
                                          ComponentName->Length + 
                                          SubstituteName->Length;

            try_return( ntStatus);
        }
        
        //
        // Need to re-allocate the buffer
        //

        uniPathName.Length = FullPathName->Length - 
                                         ComponentName->Length + 
                                         SubstituteName->Length;

        uniPathName.MaximumLength = FullPathName->MaximumLength + PAGE_SIZE;

        uniPathName.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                             uniPathName.MaximumLength,
                                                             AFS_NAME_BUFFER_TAG);

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

        if( FreePathName)
        {

            ExFreePool( FullPathName->Buffer);
        }

        *FullPathName = uniPathName;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

void
AFSInitServerStrings()
{

    UNICODE_STRING uniFullName;
    WCHAR wchBuffer[ 50];

    //
    // Initialize the server name
    //

    AFSReadServerName();

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

    //
    // Add the server name into the list of resources
    //

    uniFullName.Length = (2 * sizeof( WCHAR)) + AFSServerName.Length;
    uniFullName.MaximumLength = uniFullName.Length + sizeof( WCHAR);

    uniFullName.Buffer = wchBuffer;

    wchBuffer[ 0] = L'\\';
    wchBuffer[ 1] = L'\\';
    
    RtlCopyMemory( &wchBuffer[ 2],
                   AFSServerName.Buffer,
                   AFSServerName.Length);
                   
    AFSAddConnectionEx( &uniFullName,
                        RESOURCEDISPLAYTYPE_SERVER,
                        0);

    //
    // Add in the global share name
    //

    wchBuffer[ uniFullName.Length/sizeof( WCHAR)] = L'\\';

    uniFullName.Length += sizeof( WCHAR);

    RtlCopyMemory( &wchBuffer[ uniFullName.Length/sizeof( WCHAR)],
                   AFSGlobalRootName.Buffer,
                   AFSGlobalRootName.Length);

    uniFullName.Length += AFSGlobalRootName.Length;

    AFSAddConnectionEx( &uniFullName,
                        RESOURCEDISPLAYTYPE_SHARE,
                        AFS_CONNECTION_FLAG_GLOBAL_SHARE);

    return;
}

NTSTATUS
AFSReadServerName()
{

    NTSTATUS ntStatus        = STATUS_SUCCESS;
    ULONG Default            = 0;
    UNICODE_STRING paramPath;
    RTL_QUERY_REGISTRY_TABLE paramTable[2];

    __Enter
    {

        //
        // Setup the paramPath buffer.
        //

        paramPath.MaximumLength = PAGE_SIZE; 
        paramPath.Buffer = (PWSTR)ExAllocatePoolWithTag( PagedPool,
                                                         paramPath.MaximumLength,
                                                         AFS_GENERIC_MEMORY_TAG);
     
        //
        // If it exists, setup the path.
        //

        if( paramPath.Buffer == NULL) 
        { 

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }
        
        //
        // Move in the paths
        //

        RtlCopyMemory( &paramPath.Buffer[ 0], 
                       L"\\TransarcAFSDaemon\\Parameters", 
                       58);
        
        paramPath.Length = 58; 

        RtlZeroMemory( paramTable, 
                       sizeof( paramTable));

        //
        // Setup the table to query the registry for the needed value
        //

        AFSServerName.Length = 0;
        AFSServerName.MaximumLength = 0;
        AFSServerName.Buffer = NULL;

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT; 
        paramTable[0].Name = AFS_NETBIOS_NAME; 
        paramTable[0].EntryContext = &AFSServerName;
        
        paramTable[0].DefaultType = REG_NONE; 
        paramTable[0].DefaultData = NULL; 
        paramTable[0].DefaultLength = 0;

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_SERVICES, 
                                           paramPath.Buffer, 
                                           paramTable, 
                                           NULL, 
                                           NULL);

        //
        // Free up the buffer
        //

        ExFreePool( paramPath.Buffer);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            RtlInitUnicodeString( &AFSServerName,
                                  L"AFS");
        }
    } 

    return ntStatus;
}

NTSTATUS
AFSInvalidateVolume( IN AFSFcb *Vcb,
                     IN ULONG Reason)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pFcb = NULL, *pNextFcb = NULL;

    __Enter
    {

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

                SetFlag( Vcb->Flags, AFS_FCB_INVALID | AFS_FCB_VOLUME_OFFLINE);

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInvalidateVolume Acquiring VolumeRoot FcbListLock lock %08lX EXCL %08lX\n",
                              Vcb->Specific.VolumeRoot.FcbListLock,
                              PsGetCurrentThread());

                AFSAcquireExcl( Vcb->Specific.VolumeRoot.FcbListLock,
                                TRUE);

                pFcb = Vcb->Specific.VolumeRoot.FcbListHead;

                while( pFcb != NULL)
                {
                        
                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSInvalidateVolume Acquiring Fcb lock %08lX EXCL %08lX\n",
                                  &pFcb->NPFcb->Resource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                    TRUE);

                    SetFlag( pFcb->Flags, AFS_FCB_INVALID);

                    pNextFcb = (AFSFcb *)pFcb->ListEntry.fLink;
                            
                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                    {


                        //
                        // Clear out the extents
                        //

                        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSInvalidateVolume Acquiring Fcb extents lock %08lX EXCL %08lX\n",
                                      &pFcb->NPFcb->Specific.File.ExtentsResource,
                                      PsGetCurrentThread());

                        AFSAcquireExcl( &pFcb->NPFcb->Specific.File.ExtentsResource, 
                                        TRUE);

                        pFcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                        KeSetEvent( &pFcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                    0,
                                    FALSE);

                        AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource);
                                        
                        //
                        // And get rid of them (note this involves waiting
                        // for any writes or reads to the cache to complete)
                        //
         
                        AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSInvalidateVolume Tearing down extents for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      &pFcb->DirEntry->DirectoryEntry.FileName,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Unique);        

                        (VOID) AFSTearDownFcbExtents( pFcb);
                    }

                    pFcb = pNextFcb;
                }

                AFSReleaseResource( Vcb->Specific.VolumeRoot.FcbListLock);

                break;
            }

            default:
            {

                //
                // Indicate this node requires re-evaluation for the remaining reasons
                //

                SetFlag( Vcb->Flags, AFS_FCB_VERIFY);

                break;
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSVerifyEntry( IN AFSFcb *Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEnumEntry *pDirEntry = NULL;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSVerifyEntry Verifying Fcb %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique);

        ntStatus = AFSEvaluateTargetByID( &Fcb->DirEntry->DirectoryEntry.FileId,
                                          0, // Called in context
                                          FALSE,
                                          &pDirEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSVerifyEntry Failed to evalute Fcb %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          &Fcb->DirEntry->DirectoryEntry.FileName,
                          Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                          Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                          Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                          Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                          ntStatus);

            //
            // Mark the entry as invalid and fail the request
            //

            ClearFlag( Fcb->Flags, AFS_FCB_VERIFY);

            SetFlag( Fcb->Flags, AFS_FCB_INVALID);

            try_return( ntStatus = STATUS_ACCESS_DENIED);
        }

        //
        // Check the data version of the file
        //

        if( Fcb->DirEntry->DirectoryEntry.DataVersion.QuadPart == pDirEntry->DataVersion.QuadPart)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSVerifyEntry No DV change for Fcb %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                          &Fcb->DirEntry->DirectoryEntry.FileName,
                          Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                          Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                          Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                          Fcb->DirEntry->DirectoryEntry.FileId.Unique);

            //
            // We are ok, just get out
            //

            ClearFlag( Fcb->Flags, AFS_FCB_VERIFY);

            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // New data version so we will need to process the node based on the type
        //

        switch( pDirEntry->FileType)
        {

            case AFS_FILE_TYPE_MOUNTPOINT:
            case AFS_FILE_TYPE_SYMLINK:
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSVerifyEntry Updating metadata for SL/MP Fcb %wZ FID %08lX-%08lX-%08lX-%08lX DV %I64X\n",
                              &Fcb->DirEntry->DirectoryEntry.FileName,
                              Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                              Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                              Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                              pDirEntry->DataVersion.QuadPart);

                //
                // For a mount point or symlink we need to ensure the target is the same
                //

                if( !AFSIsEqualFID( &Fcb->DirEntry->DirectoryEntry.TargetFileId,
                                    &pDirEntry->TargetFileId))
                {

                    //
                    // Clear the target information and set the flag indicating the
                    // nodes requires evaluation
                    //

                    if( Fcb->Specific.SymbolicLink.TargetFcb != NULL)
                    {

                        ASSERT( Fcb->Specific.SymbolicLink.TargetFcb->OpenReferenceCount != 0);

                        InterlockedDecrement( &Fcb->Specific.SymbolicLink.TargetFcb->OpenReferenceCount);

                        Fcb->Specific.SymbolicLink.TargetFcb = NULL;    
                    }
                }

                //
                // Update the metadata for the entry
                //

                AFSUpdateMetaData( Fcb->DirEntry,
                                   pDirEntry);

                ClearFlag( Fcb->Flags, AFS_FCB_VERIFY);

                break;
            }

            case AFS_FILE_FCB:
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSVerifyEntry Updating metadata for file Fcb %wZ FID %08lX-%08lX-%08lX-%08lX DV %I64X\n",
                              &Fcb->DirEntry->DirectoryEntry.FileName,
                              Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                              Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                              Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                              pDirEntry->DataVersion.QuadPart);

                //
                // For a file where the data version has become invalid we need to 
                // fail any current extent requests and purge the cache for the file
                // Can't hold the Fcb resource while doing this
                //

                ClearFlag( Fcb->Flags, AFS_FCB_VERIFY);

                __try
                {

                    CcPurgeCacheSection( &Fcb->NPFcb->SectionObjectPointers,
                                         NULL,
                                         0,
                                         FALSE);
                }
                __except( EXCEPTION_EXECUTE_HANDLER)
                {
                                    
                }

                AFSReleaseResource( &Fcb->NPFcb->Resource);

                //
                // Clear out the extents
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSVerifyEntry Acquiring Fcb extents lock %08lX EXCL %08lX\n",
                              &Fcb->NPFcb->Specific.File.ExtentsResource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &Fcb->NPFcb->Specific.File.ExtentsResource, 
                                TRUE);

                Fcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                KeSetEvent( &Fcb->NPFcb->Specific.File.ExtentsRequestComplete,
                            0,
                            FALSE);

                AFSReleaseResource( &Fcb->NPFcb->Specific.File.ExtentsResource);

                //
                // Get rid of them (note this involves waiting
                // for any writes or reads to the cache to complete)
                //
         
                AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSVerifyEntry Tearing down extents for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              &Fcb->DirEntry->DirectoryEntry.FileName,
                              Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                              Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                              Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              Fcb->DirEntry->DirectoryEntry.FileId.Unique);        

                (VOID) AFSTearDownFcbExtents( Fcb);
  
                //
                // Reacquire the Fcb to purge the cache
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSVerifyEntry Acquiring Fcb lock %08lX EXCL %08lX\n",
                              &Fcb->NPFcb->Resource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &Fcb->NPFcb->Resource,
                                TRUE);

                __try
                {

                    if( !CcPurgeCacheSection( &Fcb->NPFcb->SectionObjectPointers,
                                              NULL,
                                              0,
                                              FALSE))
                    {

                        //
                        // Failed to purge the cache so tag the file as INVALID
                        //

                        SetFlag( Fcb->Flags, AFS_FCB_INVALID);
                    }
                }
                __except( EXCEPTION_EXECUTE_HANDLER)
                {
                                    
                }

                //
                // Update the metadata for the entry
                //

                AFSUpdateMetaData( Fcb->DirEntry,
                                   pDirEntry);

                //
                // Update file sizes
                //

                Fcb->Header.AllocationSize.QuadPart  = Fcb->DirEntry->DirectoryEntry.AllocationSize.QuadPart;
                Fcb->Header.FileSize.QuadPart        = Fcb->DirEntry->DirectoryEntry.EndOfFile.QuadPart;
                Fcb->Header.ValidDataLength.QuadPart = Fcb->DirEntry->DirectoryEntry.EndOfFile.QuadPart;

                break;
            }

            case AFS_ROOT_FCB:
            case AFS_DIRECTORY_FCB:
            {

                AFSFcb *pCurrentFcb = NULL;
                AFSDirEntryCB *pCurrentDirEntry = NULL;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSVerifyEntry Updating metadata for directory Fcb %wZ FID %08lX-%08lX-%08lX-%08lX DV %I64X\n",
                              &Fcb->DirEntry->DirectoryEntry.FileName,
                              Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                              Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                              Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                              pDirEntry->DataVersion.QuadPart);

                //
                // For a directory or root entry flush the content of
                // the directory enumeration.
                //

                ClearFlag( Fcb->Flags, AFS_FCB_VERIFY);

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSVerifyEntry Acquiring DirectoryNodeHdr.TreeLock lock %08lX EXCL %08lX\n",
                              Fcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                              PsGetCurrentThread());

                if( BooleanFlagOn( Fcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                {

                    AFSAcquireExcl( Fcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                    TRUE);

                    if( Fcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
                    {

                        AFSValidateDirectoryCache( Fcb);
                    }
                    else
                    {

                        AFSValidateRootDirectoryCache( Fcb);
                    }

                    AFSReleaseResource( Fcb->Specific.Directory.DirectoryNodeHdr.TreeLock);
                }

                //
                // Update the metadata for the entry
                //

                AFSUpdateMetaData( Fcb->DirEntry,
                                   pDirEntry);

                break;
            }

            case AFS_FILE_TYPE_DFSLINK:
            {

                UNICODE_STRING uniTargetName;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSVerifyEntry Updating metadata for DFS Link Fcb %wZ FID %08lX-%08lX-%08lX-%08lX DV %I64X\n",
                              &Fcb->DirEntry->DirectoryEntry.FileName,
                              Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                              Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                              Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                              pDirEntry->DataVersion.QuadPart);

                //
                // For a DFS link need to check the target name has not changed
                //

                uniTargetName.Length = (USHORT)pDirEntry->TargetNameLength;

                uniTargetName.MaximumLength = uniTargetName.Length;

                uniTargetName.Buffer = (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset);

                if( RtlCompareUnicodeString( &uniTargetName,
                                             &Fcb->DirEntry->DirectoryEntry.TargetName,
                                             TRUE) != 0)
                {

                    //
                    // If we have enough space then just move in the name otherwise
                    // allocate a new buffer
                    //

                    if( Fcb->DirEntry->DirectoryEntry.TargetName.Length < uniTargetName.Length)
                    {

                        WCHAR *pTmpBuffer = NULL;

                        pTmpBuffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                                     uniTargetName.Length,
                                                                     AFS_NAME_BUFFER_TAG);

                        if( pTmpBuffer == NULL)
                        {

                            ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                            break;
                        }

                        if( BooleanFlagOn( Fcb->DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER))
                        {

                            ExFreePool( Fcb->DirEntry->DirectoryEntry.TargetName.Buffer);
                        }

                        Fcb->DirEntry->DirectoryEntry.TargetName.MaximumLength = uniTargetName.Length;

                        Fcb->DirEntry->DirectoryEntry.TargetName.Buffer = pTmpBuffer;    

                        SetFlag( Fcb->DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER);
                    }

                    Fcb->DirEntry->DirectoryEntry.TargetName.Length = uniTargetName.Length;

                    RtlCopyMemory( Fcb->DirEntry->DirectoryEntry.TargetName.Buffer,
                                   uniTargetName.Buffer,
                                   Fcb->DirEntry->DirectoryEntry.TargetName.Length);
                }

                //
                // Update the metadata for the entry
                //

                AFSUpdateMetaData( Fcb->DirEntry,
                                   pDirEntry);

                ClearFlag( Fcb->Flags, AFS_FCB_VERIFY);

                break;
            }

            default:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSVerifyEntry Attempt to verify node of type %d\n", 
                              Fcb->DirEntry->DirectoryEntry.FileType);

                break;
        }

 try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSVerifyEntry Verification for Fcb %wZ FID %08lX-%08lX-%08lX-%08lX Complete Status %08lX\n",
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                      ntStatus);

        if( pDirEntry != NULL)
        {

            ExFreePool( pDirEntry);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSSetVolumeState( IN AFSVolumeStatusCB *VolumeStatus)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pVcb = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    ULONGLONG   ullIndex = 0;
    AFSFcb  *pFcb = NULL, *pNextFcb = NULL;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetVolumeState Marking volume state %d Volume Cell %08lX Volume %08lX\n",
                      VolumeStatus->Online,
                      VolumeStatus->FileID.Cell,
                      VolumeStatus->FileID.Volume);

        //
        // Need to locate the Fcb for the directory to purge
        //
    
        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetVolumeState Acquiring RDR VolumeTreeLock lock %08lX SHARED %08lX\n",
                      &pDevExt->Specific.RDR.VolumeTreeLock,
                      PsGetCurrentThread());

        AFSAcquireShared( &pDevExt->Specific.RDR.VolumeTreeLock, TRUE);

        //
        // Locate the volume node
        //

        ullIndex = AFSCreateHighIndex( &VolumeStatus->FileID);

        ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                       ullIndex,
                                       &pVcb);

        if( pVcb != NULL)
        {

            //
            // Set the volume state accordingly
            //

            if( VolumeStatus->Online)
            {

                InterlockedAnd( (LONG *)&(pVcb->Flags), ~AFS_FCB_VOLUME_OFFLINE);
            }
            else
            {

                InterlockedOr( (LONG *)&(pVcb->Flags), AFS_FCB_VOLUME_OFFLINE);
            }

            //
            // Go through the volume Fcb list marking the entries as invalid if the 
            // volume has been taken offline ...
            //

            AFSAcquireExcl( pVcb->Specific.VolumeRoot.FcbListLock,
                            TRUE);

            pFcb = pVcb->Specific.VolumeRoot.FcbListHead;

            while( pFcb != NULL)
            {
                        
                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetVolumeState Acquiring Fcb lock %08lX EXCL %08lX\n",
                              &pFcb->NPFcb->Resource,
                              PsGetCurrentThread());

                if( VolumeStatus->Online)
                {

                    if( pFcb->DirEntry != NULL)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSSetVolumeState Marking %wZ FID %08lX-%08lX-%08lX-%08lX on Volume Cell %08lX Volume %08lX VALID\n",
                                      &pFcb->DirEntry->DirectoryEntry.FileName,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                      VolumeStatus->FileID.Cell,
                                      VolumeStatus->FileID.Volume);
                    }

                    InterlockedAnd( (LONG *)&pFcb->Flags, ~AFS_FCB_INVALID);
                }
                else
                {

                    if( pFcb->DirEntry != NULL)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSSetVolumeState Marking %wZ FID %08lX-%08lX-%08lX-%08lX on Volume Cell %08lX Volume %08lX INVALID\n",
                                      &pFcb->DirEntry->DirectoryEntry.FileName,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                      VolumeStatus->FileID.Cell,
                                      VolumeStatus->FileID.Volume);
                    }

                    InterlockedOr( (LONG *)&pFcb->Flags, AFS_FCB_INVALID);
                }

                pNextFcb = (AFSFcb *)pFcb->ListEntry.fLink;
                            
                if( !(VolumeStatus->Online) &&
                    pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                {

                    //
                    // Clear out the extents
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSSetVolumeState Acquiring Fcb extents lock %08lX EXCL %08lX\n",
                                  &pFcb->NPFcb->Specific.File.ExtentsResource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pFcb->NPFcb->Specific.File.ExtentsResource, 
                                    TRUE);

                    pFcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                    KeSetEvent( &pFcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                0,
                                FALSE);

                    AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource);
                                       
                    //
                    // And get rid of them (note this involves waiting
                    // for any writes or reads to the cache to complete)
                    //
         
                    AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSSetVolumeState Tearing down extents for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &pFcb->DirEntry->DirectoryEntry.FileName,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Unique);        

                    (VOID) AFSTearDownFcbExtents( pFcb);
                }

                pFcb = pNextFcb;
            }

            AFSReleaseResource( pVcb->Specific.VolumeRoot.FcbListLock);
        }

        AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);
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

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetNetworkState Acquiring GlobalRoot lock %08lX EXCL %08lX\n",
                      &AFSGlobalRoot->NPFcb->Resource,
                      PsGetCurrentThread());

        AFSAcquireExcl( &AFSGlobalRoot->NPFcb->Resource,
                        TRUE);

        //
        // Set the network state according to the information
        //

        if( NetworkStatus->Online)
        {

            ClearFlag( AFSGlobalRoot->Flags, AFS_FCB_VOLUME_OFFLINE);
        }
        else
        {

            SetFlag( AFSGlobalRoot->Flags, AFS_FCB_VOLUME_OFFLINE);
        }

        AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSValidateDirectoryCache( IN AFSFcb *Dcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN  bAcquiredLock = FALSE;
    AFSDirEntryCB *pCurrentDirEntry = NULL;
    AFSFcb *pFcb = NULL;

    __Enter
    {

        if( !ExIsResourceAcquiredLite( Dcb->Specific.Directory.DirectoryNodeHdr.TreeLock))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSValidateDirectoryCache Acquiring DirectoryNodeHdr.TreeLock lock %08lX EXCL %08lX\n",
                          Dcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                          PsGetCurrentThread());

            AFSAcquireExcl( Dcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                            TRUE);

            bAcquiredLock = TRUE;
        }

        //
        // Reset the directory list information by clearing all valid entries
        //

        pCurrentDirEntry = Dcb->Specific.Directory.DirectoryNodeListHead;

        while( pCurrentDirEntry != NULL)
        {

            if( !BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_FAKE))
            {

                ClearFlag( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_VALID);
            }

            pCurrentDirEntry = (AFSDirEntryCB *)pCurrentDirEntry->ListEntry.fLink;
        }

        //
        // Reget the directory contents
        //

        AFSVerifyDirectoryContent( Dcb);

        //
        // Now start again and tear down any entries not valid
        //

        pCurrentDirEntry = Dcb->Specific.Directory.DirectoryNodeListHead;

        while( pCurrentDirEntry != NULL)
        {

            if( BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_VALID))
            {

                pCurrentDirEntry = (AFSDirEntryCB *)pCurrentDirEntry->ListEntry.fLink;

                continue;
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSValidateDirectoryCache Removing entry %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                          &pCurrentDirEntry->DirectoryEntry.FileName,
                          pCurrentDirEntry->DirectoryEntry.FileId.Cell,
                          pCurrentDirEntry->DirectoryEntry.FileId.Volume,
                          pCurrentDirEntry->DirectoryEntry.FileId.Vnode,
                          pCurrentDirEntry->DirectoryEntry.FileId.Unique);

            pFcb = pCurrentDirEntry->Fcb;

            if( pFcb != NULL)
            {

                //
                // Do this prior to blocking the Fcb since there could be a thread
                // holding the Fcb waiting for an extent
                //

                if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                {

                    //
                    // Clear out anybody waiting for the extents.
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateDirectoryCache Acquiring Fcb extents lock %08lX EXCL %08lX\n",
                                  &pFcb->NPFcb->Specific.File.ExtentsResource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pFcb->NPFcb->Specific.File.ExtentsResource, 
                                    TRUE);

                    pFcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                    KeSetEvent( &pFcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                0,
                                FALSE);

                    AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource);
                                      
                    //
                    // And get rid of them (note this involves waiting
                    // for any writes or reads to the cache to complete)
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateDirectoryCache Tearing down extents for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &pFcb->DirEntry->DirectoryEntry.FileName,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Unique);        

                    (VOID) AFSTearDownFcbExtents( pFcb);                                       
                }

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSValidateDirectoryCache Acquiring Fcb lock %08lX EXCL %08lX\n",
                                                                           &pFcb->NPFcb->Resource,
                                                                           PsGetCurrentThread());

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                if( pFcb->DirEntry != NULL)
                {

                    pFcb->DirEntry->Fcb = NULL;

                    SetFlag( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE);
                }

                SetFlag( pFcb->Flags, AFS_FCB_INVALID);

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSValidateDirectoryCache Acquiring VolumeRoot FileIDTree.TreeLock lock %08lX EXCL %08lX\n",
                              pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                              PsGetCurrentThread());

                AFSAcquireExcl( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                TRUE);

                AFSRemoveHashEntry( &pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                    &pFcb->TreeEntry);

                AFSReleaseResource( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock);

                ClearFlag( pFcb->Flags, AFS_FCB_INSERTED_ID_TREE);

                AFSReleaseResource( &pFcb->NPFcb->Resource);

                AFSRemoveDirNodeFromParent( Dcb,
                                            pCurrentDirEntry);
            }
            else
            {

                AFSDeleteDirEntry( Dcb,
                                   pCurrentDirEntry);
            }

            pCurrentDirEntry = Dcb->Specific.Directory.DirectoryNodeListHead;
        }

        if( bAcquiredLock)
        {

            AFSReleaseResource( Dcb->Specific.Directory.DirectoryNodeHdr.TreeLock);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSValidateRootDirectoryCache( IN AFSFcb *Dcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN  bAcquiredLock = FALSE;
    AFSDirEntryCB *pCurrentDirEntry = NULL;
    AFSFcb *pFcb = NULL;

    __Enter
    {

        if( !ExIsResourceAcquiredLite( Dcb->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSValidateRootDirectoryCache Acquiring VolumeRoot DirectoryNodeHdr.TreeLock lock %08lX EXCL %08lX\n",
                          Dcb->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock,
                          PsGetCurrentThread());

            AFSAcquireExcl( Dcb->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock,
                            TRUE);

            bAcquiredLock = TRUE;
        }

        //
        // Reset the directory list information by clearing all valid entries
        //

        pCurrentDirEntry = Dcb->Specific.VolumeRoot.DirectoryNodeListHead;

        while( pCurrentDirEntry != NULL)
        {

            if( !BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_FAKE))
            {

                ClearFlag( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_VALID);
            }

            pCurrentDirEntry = (AFSDirEntryCB *)pCurrentDirEntry->ListEntry.fLink;
        }

        //
        // Reget the directory contents
        //

        AFSVerifyDirectoryContent( Dcb);

        //
        // Now start again and tear down any entries not valid
        //

        pCurrentDirEntry = Dcb->Specific.VolumeRoot.DirectoryNodeListHead;

        while( pCurrentDirEntry != NULL)
        {

            if( BooleanFlagOn( pCurrentDirEntry->Flags, AFS_DIR_ENTRY_VALID))
            {

                pCurrentDirEntry = (AFSDirEntryCB *)pCurrentDirEntry->ListEntry.fLink;

                continue;
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSValidateRootDirectoryCache Removing entry %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                          &pCurrentDirEntry->DirectoryEntry.FileName,
                          pCurrentDirEntry->DirectoryEntry.FileId.Cell,
                          pCurrentDirEntry->DirectoryEntry.FileId.Volume,
                          pCurrentDirEntry->DirectoryEntry.FileId.Vnode,
                          pCurrentDirEntry->DirectoryEntry.FileId.Unique);

            pFcb = pCurrentDirEntry->Fcb;

            if( pFcb != NULL)
            {

                //
                // Do this prior to blocking the Fcb since there could be a thread
                // holding the Fcb waiting for an extent
                //

                if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                {

                    //
                    // Clear out anybody waiting for the extents.
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateRootDirectoryCache Acquiring Fcb extents lock %08lX EXCL %08lX\n",
                                  &pFcb->NPFcb->Specific.File.ExtentsResource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pFcb->NPFcb->Specific.File.ExtentsResource, 
                                    TRUE);

                    pFcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                    KeSetEvent( &pFcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                0,
                                FALSE);

                    AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource);
                                      
                    //
                    // And get rid of them (not this involves waiting
                    // for any writes or reads to the cache to complete
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateRootDirectoryCache Tearing down extents for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &pFcb->DirEntry->DirectoryEntry.FileName,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pFcb->DirEntry->DirectoryEntry.FileId.Unique);        

                    (VOID) AFSTearDownFcbExtents( pFcb);                                       
                }

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSValidateRootDirectoryCache Acquiring Fcb lock %08lX EXCL %08lX\n",
                              &pFcb->NPFcb->Resource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                if( pFcb->DirEntry != NULL)
                {

                    pFcb->DirEntry->Fcb = NULL;

                    SetFlag( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE);
                }

                SetFlag( pFcb->Flags, AFS_FCB_INVALID);

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSValidateRootDirectoryCache Acquiring VolumeRoot FileIDTree.TreeLock lock %08lX EXCL %08lX\n",
                              pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                              PsGetCurrentThread());

                AFSAcquireExcl( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                TRUE);

                AFSRemoveHashEntry( &pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                    &pFcb->TreeEntry);

                AFSReleaseResource( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock);

                ClearFlag( pFcb->Flags, AFS_FCB_INSERTED_ID_TREE);

                AFSReleaseResource( &pFcb->NPFcb->Resource);

                AFSRemoveDirNodeFromParent( Dcb,
                                            pCurrentDirEntry);
            }
            else
            {

                AFSDeleteDirEntry( Dcb,
                                   pCurrentDirEntry);
            }

            pCurrentDirEntry = Dcb->Specific.VolumeRoot.DirectoryNodeListHead;
        }

        if( bAcquiredLock)
        {

            AFSReleaseResource( Dcb->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSWalkTargetChain( IN AFSFcb *ParentFcb,
                    OUT AFSFcb **TargetFcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pCurrentFcb = ParentFcb, *pTargetFcb = NULL;

    __Enter
    {

        while( TRUE)
        {

            //
            // The initial parent is acquired before calling in
            //

            pTargetFcb = pCurrentFcb->Specific.SymbolicLink.TargetFcb;

            ASSERT( pTargetFcb != NULL);

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSWalkTargetChain Acquiring Fcb lock %08lX EXCL %08lX\n",
                          &pTargetFcb->NPFcb->Resource,
                          PsGetCurrentThread());

            AFSAcquireExcl( &pTargetFcb->NPFcb->Resource,
                            TRUE);

            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

            //
            // If this is a root node then see if it is OFFLINE
            //

            if( pTargetFcb->Header.NodeTypeCode == AFS_ROOT_FCB &&
                BooleanFlagOn( pTargetFcb->Flags, AFS_FCB_VOLUME_OFFLINE))
            {

                //
                // The volume has been taken off line so fail the access
                //

                AFSReleaseResource( &pTargetFcb->NPFcb->Resource);

                try_return( ntStatus = STATUS_DEVICE_NOT_READY);
            }

            //
            // Check if we need to verify this node
            //

            if( BooleanFlagOn( pTargetFcb->Flags, AFS_FCB_VERIFY))
            {

                ntStatus = AFSVerifyEntry( pTargetFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &pTargetFcb->NPFcb->Resource);

                    try_return( ntStatus);
                }
            }

            pCurrentFcb = pTargetFcb;

            pTargetFcb = NULL;

            //
            // If we are at a node that is not a symlink or MP then we are done
            //

            if( pCurrentFcb->Header.NodeTypeCode != AFS_MOUNT_POINT_FCB &&
                pCurrentFcb->Header.NodeTypeCode != AFS_SYMBOLIC_LINK_FCB)
            {

                break;
            }
        }

        *TargetFcb = pCurrentFcb;

try_exit:

        NOTHING;
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

void
AFSUpdateMetaData( IN AFSDirEntryCB *DirEntry,
                   IN AFSDirEnumEntry *DirEnumEntry)
{

    __Enter
    {

        DirEntry->DirectoryEntry.TargetFileId = DirEnumEntry->TargetFileId;

        DirEntry->DirectoryEntry.Expiration = DirEnumEntry->Expiration;

        DirEntry->DirectoryEntry.DataVersion = DirEnumEntry->DataVersion;

        DirEntry->DirectoryEntry.FileType = DirEnumEntry->FileType;

        DirEntry->DirectoryEntry.CreationTime = DirEnumEntry->CreationTime;

        DirEntry->DirectoryEntry.LastAccessTime = DirEnumEntry->LastAccessTime;

        DirEntry->DirectoryEntry.LastWriteTime = DirEnumEntry->LastWriteTime;

        DirEntry->DirectoryEntry.ChangeTime = DirEnumEntry->ChangeTime;

        DirEntry->DirectoryEntry.EndOfFile = DirEnumEntry->EndOfFile;

        DirEntry->DirectoryEntry.AllocationSize = DirEnumEntry->AllocationSize;

        DirEntry->DirectoryEntry.FileAttributes = DirEnumEntry->FileAttributes;

        DirEntry->DirectoryEntry.EaSize = DirEnumEntry->EaSize;

        DirEntry->DirectoryEntry.Links = DirEnumEntry->Links;

        if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY)
        {

            DirEntry->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        }
        else if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK)
        {

            DirEntry->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        }
        else if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
        {

            DirEntry->DirectoryEntry.FileAttributes |= (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
        }
        else if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DFSLINK)
        {

            DirEntry->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        }
    }

    return;
}

NTSTATUS
AFSValidateEntry( IN AFSDirEntryCB *DirEntry,
                  IN BOOLEAN PurgeContent,
                  IN BOOLEAN FastCall)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    LARGE_INTEGER liSystemTime, liLocalSystemTime;
    AFSDirEnumEntry *pDirEnumEntry = NULL;
    AFSFcb *pCurrentFcb = NULL;
    BOOLEAN bReleaseDirEntry = FALSE, bReleaseFcb = FALSE;

    __Enter
    {

        //
        // If we have an Fcb hanging off the dire entry then be sure to acquire the locks in the
        // correct order
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSValidateEntry Validating directory entry %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      &DirEntry->DirectoryEntry.FileName,
                      DirEntry->DirectoryEntry.FileId.Cell,
                      DirEntry->DirectoryEntry.FileId.Volume,
                      DirEntry->DirectoryEntry.FileId.Vnode,
                      DirEntry->DirectoryEntry.FileId.Unique);

        //
        // If this is a fake node then bail since the service knows nothing about it
        //

        if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_ENTRY_FAKE))
        {

            try_return( ntStatus);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSValidateEntry Acquiring DirEntry lock %08lX EXCL %08lX\n",
                      &DirEntry->NPDirNode->Lock,
                      PsGetCurrentThread());

        AFSAcquireExcl( &DirEntry->NPDirNode->Lock,
                        TRUE);

        bReleaseDirEntry = TRUE;

        if( PurgeContent &&
            DirEntry->Fcb != NULL)
        {

            pCurrentFcb = DirEntry->Fcb;

            if( !ExIsResourceAcquiredLite( &pCurrentFcb->NPFcb->Resource))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSValidateEntry Acquiring Fcb lock %08lX EXCL %08lX\n",
                              &pCurrentFcb->NPFcb->Resource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &pCurrentFcb->NPFcb->Resource,
                                TRUE);

                bReleaseFcb = TRUE;
            }
        }

        //
        // This routine ensures that the current entry is valid by:
        //
        //      1) Checking that the expiration time is non-zero and after where we
        //         currently are
        //

        KeQuerySystemTime( &liSystemTime);

        ExSystemTimeToLocalTime( &liSystemTime,
                                 &liLocalSystemTime);

        if( DirEntry->DirectoryEntry.Expiration.QuadPart >= liLocalSystemTime.QuadPart)
        {

            ASSERT( DirEntry->DirectoryEntry.FileType != 0);

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSValidateEntry Directory entry %wZ FID %08lX-%08lX-%08lX-%08lX VALID\n",
                          &DirEntry->DirectoryEntry.FileName,
                          DirEntry->DirectoryEntry.FileId.Cell,
                          DirEntry->DirectoryEntry.FileId.Volume,
                          DirEntry->DirectoryEntry.FileId.Vnode,
                          DirEntry->DirectoryEntry.FileId.Unique);

            try_return( ntStatus);
        }

        //
        // This node requires updating
        //

        ntStatus = AFSEvaluateTargetByID( &DirEntry->DirectoryEntry.FileId,
                                          0, // Called in context
                                          FastCall,
                                          &pDirEnumEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSValidateEntry Failed to evaluate entry %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          &DirEntry->DirectoryEntry.FileName,
                          DirEntry->DirectoryEntry.FileId.Cell,
                          DirEntry->DirectoryEntry.FileId.Volume,
                          DirEntry->DirectoryEntry.FileId.Vnode,
                          DirEntry->DirectoryEntry.FileId.Unique,
                          ntStatus);

            //
            // Failed validation of node so return access-denied
            //

            try_return( ntStatus = STATUS_ACCESS_DENIED);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSValidateEntry Validating directory entry %wZ FID %08lX-%08lX-%08lX-%08lX DV %I64X returned DV %I64X FT %d\n",
                      &DirEntry->DirectoryEntry.FileName,
                      DirEntry->DirectoryEntry.FileId.Cell,
                      DirEntry->DirectoryEntry.FileId.Volume,
                      DirEntry->DirectoryEntry.FileId.Vnode,
                      DirEntry->DirectoryEntry.FileId.Unique,
                      DirEntry->DirectoryEntry.DataVersion.QuadPart,
                      pDirEnumEntry->DataVersion.QuadPart,
                      pDirEnumEntry->FileType);


        //
        // Based on the file type, process the node
        //

        switch( pDirEnumEntry->FileType)
        {

            case AFS_FILE_TYPE_MOUNTPOINT:
            case AFS_FILE_TYPE_SYMLINK:
            {

                if( pCurrentFcb != NULL)
                {

                    ClearFlag( pCurrentFcb->Flags, AFS_FCB_VERIFY);

                    //
                    // For a mount point or symlink we need to ensure the target is the same
                    //

                    if( DirEntry->DirectoryEntry.DataVersion.QuadPart != pDirEnumEntry->DataVersion.QuadPart)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE_2,
                                      "AFSValidateEntry Entry %wZ FID %08lX-%08lX-%08lX-%08lX has different target ID\n",
                                      &DirEntry->DirectoryEntry.FileName,
                                      DirEntry->DirectoryEntry.FileId.Cell,
                                      DirEntry->DirectoryEntry.FileId.Volume,
                                      DirEntry->DirectoryEntry.FileId.Vnode,
                                      DirEntry->DirectoryEntry.FileId.Unique);

                        //
                        // Clear the target information and set the flag indicating the
                        // nodes requires evaluation
                        //

                        if( pCurrentFcb->Specific.SymbolicLink.TargetFcb != NULL)
                        {

                            ASSERT( pCurrentFcb->Specific.SymbolicLink.TargetFcb->OpenReferenceCount != 0);

                            InterlockedDecrement( &pCurrentFcb->Specific.SymbolicLink.TargetFcb->OpenReferenceCount);

                            pCurrentFcb->Specific.SymbolicLink.TargetFcb = NULL;   

                            //
                            // Go rebuild the target if this is not a fast call
                            //

                            if( !FastCall)
                            {

                                ntStatus = AFSQueueBuildTargetDirectory( pCurrentFcb);

                                if( !NT_SUCCESS( ntStatus))
                                {

                                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                                  AFS_TRACE_LEVEL_ERROR,
                                                  "AFSValidateEntry Failed to build target of link entry %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                                  &DirEntry->DirectoryEntry.FileName,
                                                  DirEntry->DirectoryEntry.FileId.Cell,
                                                  DirEntry->DirectoryEntry.FileId.Volume,
                                                  DirEntry->DirectoryEntry.FileId.Vnode,
                                                  DirEntry->DirectoryEntry.FileId.Unique,
                                                  ntStatus);

                                    break;
                                }
                            }
                        }
                    }
                }

                //
                // Update the metadata for the entry
                //

                AFSUpdateMetaData( DirEntry,
                                   pDirEnumEntry);

                break;
            }

            case AFS_FILE_FCB:
            {

                //
                // For a file where the data version has become invalid we need to 
                // fail any current extent requests and purge the cache for the file
                // Can't hold the Fcb resource while doing this
                //

                if( pCurrentFcb != NULL &&
                    DirEntry->DirectoryEntry.DataVersion.QuadPart != pDirEnumEntry->DataVersion.QuadPart)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSValidateEntry Entry %wZ FID %08lX-%08lX-%08lX-%08lX has different file data version\n",
                                  &DirEntry->DirectoryEntry.FileName,
                                  DirEntry->DirectoryEntry.FileId.Cell,
                                  DirEntry->DirectoryEntry.FileId.Volume,
                                  DirEntry->DirectoryEntry.FileId.Vnode,
                                  DirEntry->DirectoryEntry.FileId.Unique);

                    ClearFlag( pCurrentFcb->Flags, AFS_FCB_VERIFY);

                    __try
                    {

                        CcPurgeCacheSection( &pCurrentFcb->NPFcb->SectionObjectPointers,
                                             NULL,
                                             0,
                                             FALSE);
                    }
                    __except( EXCEPTION_EXECUTE_HANDLER)
                    {
                                    
                    }

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    //
                    // Clear out the extents
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateEntry Acquiring Fcb extents lock %08lX EXCL %08lX\n",
                                  &pCurrentFcb->NPFcb->Specific.File.ExtentsResource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pCurrentFcb->NPFcb->Specific.File.ExtentsResource, 
                                    TRUE);

                    pCurrentFcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                    KeSetEvent( &pCurrentFcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                0,
                                FALSE);

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Specific.File.ExtentsResource);

                    //
                    // Get rid of them (note this involves waiting
                    // for any writes or reads to the cache to complete)
                    //
             
                    AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateEntry Tearing down extents for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);        

                    (VOID) AFSTearDownFcbExtents( pCurrentFcb);
      
                    //
                    // Reacquire the Fcb to purge the cache
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateEntry Acquiring Fcb lock %08lX EXCL %08lX\n",
                                  &pCurrentFcb->NPFcb->Resource,
                                  PsGetCurrentThread());

                    AFSAcquireExcl( &pCurrentFcb->NPFcb->Resource,
                                    TRUE);

                    if( pCurrentFcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL ||
                        pCurrentFcb->NPFcb->SectionObjectPointers.ImageSectionObject != NULL)
                    {
                        
                        __try
                        {

                            if( !CcPurgeCacheSection( &pCurrentFcb->NPFcb->SectionObjectPointers,
                                                      NULL,
                                                      0,
                                                      FALSE))
                            {

                                //
                                // Failed to purge the cache so tag the file as INVALID
                                //

                                SetFlag( pCurrentFcb->Flags, AFS_FCB_INVALID);
                            }
                        }
                        __except( EXCEPTION_EXECUTE_HANDLER)
                        {
                                            
                        }
                    }
                }

                //
                // Update the metadata for the entry
                //

                AFSUpdateMetaData( DirEntry,
                                   pDirEnumEntry);

                if( DirEntry->Fcb != NULL)
                {

                    //
                    // Update file sizes
                    //

                    DirEntry->Fcb->Header.AllocationSize.QuadPart  = DirEntry->DirectoryEntry.AllocationSize.QuadPart;
                    DirEntry->Fcb->Header.FileSize.QuadPart        = DirEntry->DirectoryEntry.EndOfFile.QuadPart;
                    DirEntry->Fcb->Header.ValidDataLength.QuadPart = DirEntry->DirectoryEntry.EndOfFile.QuadPart;
                }

                break;
            }

            case AFS_ROOT_FCB:
            case AFS_DIRECTORY_FCB:
            {

                AFSDirEntryCB *pCurrentDirEntry = NULL;

                if( pCurrentFcb != NULL &&
                    DirEntry->DirectoryEntry.DataVersion.QuadPart != pDirEnumEntry->DataVersion.QuadPart)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSValidateEntry Entry %wZ FID %08lX-%08lX-%08lX-%08lX has different directory data version\n",
                                  &DirEntry->DirectoryEntry.FileName,
                                  DirEntry->DirectoryEntry.FileId.Cell,
                                  DirEntry->DirectoryEntry.FileId.Volume,
                                  DirEntry->DirectoryEntry.FileId.Vnode,
                                  DirEntry->DirectoryEntry.FileId.Unique);

                    //
                    // For a directory or root entry flush the content of
                    // the directory enumeration.
                    //

                    ClearFlag( pCurrentFcb->Flags, AFS_FCB_VERIFY);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSValidateEntry Acquiring DirectoryNodeHdr.TrreeLock lock %08lX EXCL %08lX\n",
                                  pCurrentFcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                  PsGetCurrentThread());

                    if( BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                    {

                        AFSAcquireExcl( pCurrentFcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                        TRUE);

                        if( pCurrentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
                        {

                            AFSValidateDirectoryCache( pCurrentFcb);
                        }
                        else
                        {

                            AFSValidateRootDirectoryCache( pCurrentFcb);
                        }

                        AFSReleaseResource( pCurrentFcb->Specific.Directory.DirectoryNodeHdr.TreeLock);
                    }

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSValidateEntry Failed to re-enumerate %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      &DirEntry->DirectoryEntry.FileName,
                                      DirEntry->DirectoryEntry.FileId.Cell,
                                      DirEntry->DirectoryEntry.FileId.Volume,
                                      DirEntry->DirectoryEntry.FileId.Vnode,
                                      DirEntry->DirectoryEntry.FileId.Unique,
                                      ntStatus);

                        break;
                    }   
                }

                //
                // Update the metadata for the entry
                //

                AFSUpdateMetaData( DirEntry,
                                   pDirEnumEntry);

                break;
            }

            default:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSValidateEntry Attempt to verify node of type %d\n", 
                              DirEntry->DirectoryEntry.FileType);

                break;
        }

 try_exit:

        if( bReleaseFcb)
        {

            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
        }

        if( bReleaseDirEntry)
        {

            AFSReleaseResource( &DirEntry->NPDirNode->Lock);
        }

        if( pDirEnumEntry != NULL)
        {

            ExFreePool( pDirEnumEntry);
        }
    }

    return ntStatus;
}

void
AFSInitializeSpecialShareNameList()
{

    AFSSpecialShareNameCount = 0;

    RtlInitUnicodeString( &AFSSpecialShareNames[ AFSSpecialShareNameCount],
                          L"PIPE");

    AFSSpecialShareNameCount++;

    RtlInitUnicodeString( &AFSSpecialShareNames[ AFSSpecialShareNameCount],
                          L"IPC$");

    AFSSpecialShareNameCount++;

    return;
}

BOOLEAN
AFSIsSpecialShareName( IN UNICODE_STRING *ShareName)
{

    BOOLEAN bIsShareName = FALSE;
    ULONG ulIndex = 0;

    __Enter
    {

        //
        // Loop through our special share names to see if this is one of them
        //

        while( ulIndex < AFSSpecialShareNameCount)
        {

            if( RtlCompareUnicodeString( ShareName,
                                         &AFSSpecialShareNames[ ulIndex],
                                         TRUE) == 0)
            {

                bIsShareName = TRUE;

                break;
            }

            ulIndex++;
        }
    }

    return bIsShareName;
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

    if( FileId1->Unique == FileId2->Unique &&
        FileId1->Vnode == FileId2->Vnode &&
        FileId1->Volume == FileId2->Volume &&
        FileId1->Cell == FileId2->Cell)
    {

        bIsEqual = TRUE;
    }

    return bIsEqual;
}

NTSTATUS
AFSResetDirectoryContent( IN AFSFcb *Dcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEntryCB *pCurrentDirEntry = NULL;

    __Enter
    {

        //
        // Reset the directory list information
        //

        pCurrentDirEntry = Dcb->Specific.Directory.DirectoryNodeListHead;

        while( pCurrentDirEntry != NULL)
        {

            AFSDeleteDirEntry( Dcb,
                               pCurrentDirEntry);

            pCurrentDirEntry = Dcb->Specific.Directory.DirectoryNodeListHead;
        }

        Dcb->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead = NULL;

        Dcb->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead = NULL;

        Dcb->Specific.Directory.ShortNameTree = NULL;

        Dcb->Specific.Directory.DirectoryNodeListHead = NULL;

        Dcb->Specific.Directory.DirectoryNodeListTail = NULL;
    }

    return ntStatus;
}

NTSTATUS
AFSEnumerateGlobalRoot()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEntryCB *pDirGlobalDirNode = NULL;
    UNICODE_STRING uniFullName;
        
    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSEnumerateGlobalRoot Acquiring GlobalRoot DirectoryNodeHdr.TreeLock lock %08lX EXCL %08lX\n",
                      AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        if( BooleanFlagOn( AFSGlobalRoot->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
        {

            try_return( ntStatus);
        }

        //
        // Initialize the root information
        //

        AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.ContentIndex = 1;

        //
        // Enumerate the shares in the volume
        //

        ntStatus = AFSEnumerateDirectory( AFSGlobalRoot,
                                          &AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr,
                                          &AFSGlobalRoot->Specific.Directory.DirectoryNodeListHead,
                                          &AFSGlobalRoot->Specific.Directory.DirectoryNodeListTail,
                                          NULL,
                                          TRUE);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        pDirGlobalDirNode = AFSGlobalRoot->Specific.Directory.DirectoryNodeListHead;

        //
        // Indicate the node is initialized
        //

        SetFlag( AFSGlobalRoot->Flags, AFS_FCB_DIRECTORY_ENUMERATED);

        uniFullName.MaximumLength = PAGE_SIZE;
        uniFullName.Length = 0;

        uniFullName.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                             uniFullName.MaximumLength,
                                                             AFS_GENERIC_MEMORY_TAG);

        if( uniFullName.Buffer == NULL)
        {

            //
            // Reset the directory content
            //

            AFSResetDirectoryContent( AFSGlobalRoot);

            ClearFlag( AFSGlobalRoot->Flags, AFS_FCB_DIRECTORY_ENUMERATED);

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
                           pDirGlobalDirNode->DirectoryEntry.FileName.Buffer,
                           pDirGlobalDirNode->DirectoryEntry.FileName.Length);

            uniFullName.Length += pDirGlobalDirNode->DirectoryEntry.FileName.Length;

            AFSAddConnectionEx( &uniFullName,
                                RESOURCEDISPLAYTYPE_SHARE,
                                0);

            pDirGlobalDirNode = (AFSDirEntryCB *)pDirGlobalDirNode->ListEntry.fLink;
        }

        ExFreePool( uniFullName.Buffer);
       
try_exit:

        AFSReleaseResource( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock);
    }

    return ntStatus;
}

ULONG
AFSRetrieveTargetType( IN AFSFcb *ParentFcb,
                       IN AFSDirEntryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulFileType = 0;
    AFSFcb *pCurrentFcb = NULL;
    ULONGLONG ullIndex = 0;
    AFSDirEnumEntry *pDirEnumEntry = NULL;

    __Enter
    {

        //
        // Be sure we have at least a target fid or type
        //

        if( DirEntry->DirectoryEntry.FileType == 0 ||
            ( DirEntry->DirectoryEntry.TargetFileId.Vnode == 0 &&
              DirEntry->DirectoryEntry.TargetFileId.Unique == 0))
        {

            ntStatus = AFSEvaluateTargetByID( &DirEntry->DirectoryEntry.FileId,
                                              0,
                                              FALSE,
                                              &pDirEnumEntry);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSRetrieveTargetType Failed to evaluate possible link %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                  &DirEntry->DirectoryEntry.FileName,
                                  DirEntry->DirectoryEntry.FileId.Cell,
                                  DirEntry->DirectoryEntry.FileId.Volume,
                                  DirEntry->DirectoryEntry.FileId.Vnode,
                                  DirEntry->DirectoryEntry.FileId.Unique,
                                  ntStatus);
    
                try_return( ntStatus);
            }

            //
            // Update information for this node
            //

            DirEntry->DirectoryEntry.TargetFileId = pDirEnumEntry->TargetFileId;

            DirEntry->DirectoryEntry.Expiration = pDirEnumEntry->Expiration;

            DirEntry->DirectoryEntry.DataVersion = pDirEnumEntry->DataVersion;

            DirEntry->DirectoryEntry.FileType = pDirEnumEntry->FileType;

            DirEntry->DirectoryEntry.CreationTime = pDirEnumEntry->CreationTime;

            DirEntry->DirectoryEntry.LastAccessTime = pDirEnumEntry->LastAccessTime;

            DirEntry->DirectoryEntry.LastWriteTime = pDirEnumEntry->LastWriteTime;

            DirEntry->DirectoryEntry.ChangeTime = pDirEnumEntry->ChangeTime;

            DirEntry->DirectoryEntry.EndOfFile = pDirEnumEntry->EndOfFile;

            DirEntry->DirectoryEntry.AllocationSize = pDirEnumEntry->AllocationSize;

            DirEntry->DirectoryEntry.FileAttributes = pDirEnumEntry->FileAttributes;

            DirEntry->DirectoryEntry.EaSize = pDirEnumEntry->EaSize;

            DirEntry->DirectoryEntry.Links = pDirEnumEntry->Links;

            if( DirEntry->DirectoryEntry.FileType != AFS_FILE_TYPE_FILE)
            {

                DirEntry->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
            }

            ExFreePool( pDirEnumEntry);
        }

        //
        // Assure we are still looking at a SymLink
        //

        if( DirEntry->DirectoryEntry.FileType != AFS_FILE_TYPE_SYMLINK)
        {

            ulFileType = DirEntry->DirectoryEntry.FileType;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRetrieveTargetType Evaluated node %wZ to be of type %d (Not symlink)\n",
                                  &DirEntry->DirectoryEntry.FileName,
                                  DirEntry->DirectoryEntry.FileType);

            try_return( ntStatus);
        }

        //
        // Locate/create the Fcb for the current portion.
        //

        if( DirEntry->Fcb == NULL)
        {

            pCurrentFcb = NULL;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRetrieveTargetType Locating Fcb for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              &DirEntry->DirectoryEntry.FileName,
                              DirEntry->DirectoryEntry.FileId.Cell,
                              DirEntry->DirectoryEntry.FileId.Volume,
                              DirEntry->DirectoryEntry.FileId.Vnode,
                              DirEntry->DirectoryEntry.FileId.Unique);

            //
            // See if we can first locate the Fcb since this could be a stand alone node
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRetrieveTargetType Acquiring VolumeRoot FileIDTree.TreeLock lock %08lX SHARED %08lX\n",
                          ParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                          PsGetCurrentThread());

            AFSAcquireShared( ParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock, 
                              TRUE);

            ullIndex = AFSCreateLowIndex( &DirEntry->DirectoryEntry.FileId);

            AFSLocateHashEntry( ParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                ullIndex,
                                &pCurrentFcb);

            AFSReleaseResource( ParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock);

            if( pCurrentFcb != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSRetrieveTargetType Acquiring Fcb lock %08lX EXCL %08lX\n",
                              &pCurrentFcb->NPFcb->Resource,
                              PsGetCurrentThread());

                AFSAcquireExcl( &pCurrentFcb->NPFcb->Resource,
                                TRUE);

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSRetrieveTargetType Located stand alone Fcb %08lX for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  pCurrentFcb,
                                  &DirEntry->DirectoryEntry.FileName,
                                  DirEntry->DirectoryEntry.FileId.Cell,
                                  DirEntry->DirectoryEntry.FileId.Volume,
                                  DirEntry->DirectoryEntry.FileId.Vnode,
                                  DirEntry->DirectoryEntry.FileId.Unique);

                //
                // If this is a stand alone Fcb then we will need to swap out the dir entry
                // control block and update the DirEntry Fcb pointer
                //

                if( BooleanFlagOn( pCurrentFcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE))
                {

                    if( BooleanFlagOn( pCurrentFcb->DirEntry->Flags, AFS_DIR_RELEASE_NAME_BUFFER))
                    {

                        ExFreePool( pCurrentFcb->DirEntry->DirectoryEntry.FileName.Buffer);
                    }

                    if( BooleanFlagOn( pCurrentFcb->DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER))
                    {

                        ExFreePool( pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Buffer);
                    }

                    AFSAllocationMemoryLevel -= sizeof( AFSDirEntryCB) + 
                                                        pCurrentFcb->DirEntry->DirectoryEntry.FileName.Length +
                                                        pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Length;

                    ExFreePool( pCurrentFcb->DirEntry);

                    pCurrentFcb->DirEntry = DirEntry;
                }

                if( BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_STANDALONE_NODE))
                {

                    pCurrentFcb->ParentFcb = ParentFcb;

                    pCurrentFcb->RootFcb = ParentFcb->RootFcb;

                    ClearFlag( pCurrentFcb->Flags, AFS_FCB_STANDALONE_NODE);
                }

                DirEntry->Fcb = pCurrentFcb;
            }
            else
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSRetrieveTargetType Initializing Fcb for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &DirEntry->DirectoryEntry.FileName,
                                  DirEntry->DirectoryEntry.FileId.Cell,
                                  DirEntry->DirectoryEntry.FileId.Volume,
                                  DirEntry->DirectoryEntry.FileId.Vnode,
                                  DirEntry->DirectoryEntry.FileId.Unique);

                ntStatus = AFSInitFcb( ParentFcb,
                                       DirEntry,
                                       &pCurrentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSRetrieveTargetType Failed to initialize Fcb for %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      &DirEntry->DirectoryEntry.FileName,
                                      DirEntry->DirectoryEntry.FileId.Cell,
                                      DirEntry->DirectoryEntry.FileId.Volume,
                                      DirEntry->DirectoryEntry.FileId.Vnode,
                                      DirEntry->DirectoryEntry.FileId.Unique,
                                      ntStatus);

                    try_return( ntStatus);
                }

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSRetrieveTargetType Initialized Fcb %08lX for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  pCurrentFcb,
                                  &DirEntry->DirectoryEntry.FileName,
                                  DirEntry->DirectoryEntry.FileId.Cell,
                                  DirEntry->DirectoryEntry.FileId.Volume,
                                  DirEntry->DirectoryEntry.FileId.Vnode,
                                  DirEntry->DirectoryEntry.FileId.Unique);
            }
        }
        else
        {

            AFSAcquireExcl( &DirEntry->Fcb->NPFcb->Resource,
                            TRUE);
        }

        pCurrentFcb = DirEntry->Fcb;

        //
        // Now we have a starting Fcb for the node, walk the links to the end node
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRetrieveTargetType Building link target for parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

        ntStatus = AFSQueueBuildSymLinkTarget( pCurrentFcb,
                                               &ulFileType);

        if( !NT_SUCCESS( ntStatus))
        {

            ulFileType = 0;
        }

        //
        // Drop our lock
        //

        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

try_exit:

        NOTHING;
    }

    return ulFileType;
}
