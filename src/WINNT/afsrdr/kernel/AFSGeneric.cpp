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

    ExReleaseResourceLite( Resource);

    KeLeaveCriticalRegion();

    return;
}

void
AFSConvertToShared( IN PERESOURCE Resource)
{

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
AFSGenerateCRC( IN PUNICODE_STRING FileName)
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

    RtlUpcaseUnicodeString( &UpcaseString,
                            FileName,
                            TRUE);

    lpbuffer = UpcaseString.Buffer;

    size = (UpcaseString.Length/sizeof( WCHAR));

    while (size--) 
    {
        temp1 = (crc >> 8) & 0x00FFFFFFL;        
        temp2 = AFSCRCTable[((int)crc ^ *lpbuffer++) & 0xff];
        crc = temp1 ^ temp2;
    }

    RtlFreeUnicodeString( &UpcaseString);

    crc ^= 0xFFFFFFFFL;

    ASSERT( crc != 0);

    return crc;
}

BOOLEAN 
AFSAcquireForLazyWrite( IN PVOID Context,
                        IN BOOLEAN Wait)
{

    AFSPrint("AFSAcquireForLazyWrite Called for Fcb %08lX\n", Context);


    return TRUE;
}

VOID 
AFSReleaseFromLazyWrite( IN PVOID Context)
{

    AFSPrint("AFSReleaseForLazyWrite Called for Fcb %08lX\n", Context);

    return;
}


BOOLEAN 
AFSAcquireForReadAhead( IN PVOID Context,
                        IN BOOLEAN Wait)
{

    AFSPrint("AFSAcquireForReadAhead Called for Fcb %08lX\n", Context);

    return TRUE;
}

VOID 
AFSReleaseFromReadAhead( IN PVOID Context)
{

    AFSPrint("AFSReleaseForReadAhead Called for Fcb %08lX\n", Context);

    return;
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
            __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
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

        RtlZeroMemory( paramTable, 
                       sizeof( paramTable));

        Value = 0;

        AFSServerName.Length = 0;
        AFSServerName.MaximumLength = 0;
        AFSServerName.Buffer = NULL;

        //
        // Setup the table to query the registry for the needed value
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT; 
        paramTable[0].Name = AFS_REG_SERVER_NAME; 
        paramTable[0].EntryContext = &AFSServerName;
        
        paramTable[0].DefaultType = REG_NONE; 
        paramTable[0].DefaultData = NULL; 
        paramTable[0].DefaultLength = 0; 

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL, 
                                           paramPath.Buffer, 
                                           paramTable, 
                                           NULL, 
                                           NULL);

        if( !NT_SUCCESS( ntStatus) ||
            AFSServerName.Length == 0)
        {

            RtlInitUnicodeString( &AFSServerName,
                                  L"AFS");
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
AFSInitializeControlFilter()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        //
        // Initialize the comm pool resources
        //

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.CommServiceCB.IrpPoolLock);

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.CommServiceCB.ResultPoolLock);

        //
        // And the event
        //

        KeInitializeEvent( &pDeviceExt->Specific.Control.CommServiceCB.IrpPoolHasEntries,
                           NotificationEvent,
                           FALSE);

        //
        // Set the initial state of the irp pool
        //

        pDeviceExt->Specific.Control.CommServiceCB.IrpPoolControlFlag = POOL_INACTIVE;

        //
        // Initialize the provider list lock
        //

        ExInitializeResourceLite( &AFSProviderListLock);

        //
        // Initialize the IOCtl interface file name
        //

        RtlInitUnicodeString( &AFSPIOCtlName,
                              L"__AFS_IOCTL__");
    }

    return ntStatus;
}

NTSTATUS
AFSDefaultDispatch( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION  pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    AFSPrint("AFSDefaultDispatch Hanlding %08lX\n", pIrpSp->MajorFunction);

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

        Dcb->Specific.Directory.DirectoryNodeListTail->Type.Data.ListEntry.fLink = (void *)pDirNode;

        pDirNode->Type.Data.ListEntry.bLink = (void *)Dcb->Specific.Directory.DirectoryNodeListTail;

        Dcb->Specific.Directory.DirectoryNodeListTail = pDirNode;

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

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Initialize the non-paged portion of the dire entry
        //

        AFSInitNonPagedDirEntry( pDirNode);

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

        //
        // Create a CRC for the file
        //

        pDirNode->TreeEntry.Index = AFSGenerateCRC( &pDirNode->DirectoryEntry.FileName);

        pDirNode->DirectoryEntry.FileIndex = FileIndex; 

        //
        // Populate the rest of the data
        //

        pDirNode->DirectoryEntry.ParentId = *ParentFileID;

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

        //
        // If this is a symlink then we need to add in the directory attribute
        //

        ASSERT( pDirNode->DirectoryEntry.FileType != 0);

        if( pDirNode->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY ||
            pDirNode->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK ||
            pDirNode->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
        {

            pDirNode->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        }

        pDirNode->DirectoryEntry.EaSize = DirEnumEntry->EaSize;

        pDirNode->DirectoryEntry.Links = DirEnumEntry->Links;

try_exit:

        NOTHING;
    }

    return pDirNode;
}