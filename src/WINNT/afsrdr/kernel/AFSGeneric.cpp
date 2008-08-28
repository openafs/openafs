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
                              AFS_PIOCTL_FILE_INTERFACE_NAME);
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

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE);

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

        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE);

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

        pDirNode->CaseSensitiveTreeEntry.Index = AFSGenerateCRC( &pDirNode->DirectoryEntry.FileName,
                                                                 FALSE);

        pDirNode->CaseInsensitiveTreeEntry.Index = AFSGenerateCRC( &pDirNode->DirectoryEntry.FileName,
                                                                   TRUE);

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

        if( pDirNode->DirectoryEntry.FileType == 0)
        {

            //
            // Set this node as a directory and set the AFS_DIR_ENTRY_NOT_EVALUATED flag
            //
            
            pDirNode->DirectoryEntry.FileType = AFS_FILE_TYPE_DIRECTORY;

            SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
        }

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
    AFSFileID stParentFileId;

    __Enter
    {

        if( Fcb->ParentFcb != NULL)
        {

            stParentFileId = Fcb->ParentFcb->DirEntry->DirectoryEntry.FileId;
        }
        else
        {

            RtlZeroMemory( &stParentFileId,
                           sizeof( AFSFileID));           
        }

        ntStatus = AFSEvaluateTargetByID( &stParentFileId,
                                          &Fcb->DirEntry->DirectoryEntry.FileId,
                                          &pDirEntry);

        if( !NT_SUCCESS( ntStatus) ||
            pDirEntry->FileType == 0)
        {

            try_return( ntStatus = STATUS_OBJECT_NAME_NOT_FOUND);
        }

        Fcb->DirEntry->DirectoryEntry.TargetFileId = pDirEntry->TargetFileId;

        Fcb->DirEntry->DirectoryEntry.Expiration = pDirEntry->Expiration;

        Fcb->DirEntry->DirectoryEntry.DataVersion = pDirEntry->DataVersion;

        Fcb->DirEntry->DirectoryEntry.FileType = pDirEntry->FileType;

        Fcb->DirEntry->DirectoryEntry.CreationTime = pDirEntry->CreationTime;

        Fcb->DirEntry->DirectoryEntry.LastAccessTime = pDirEntry->LastAccessTime;

        Fcb->DirEntry->DirectoryEntry.LastWriteTime = pDirEntry->LastWriteTime;

        Fcb->DirEntry->DirectoryEntry.ChangeTime = pDirEntry->ChangeTime;

        Fcb->DirEntry->DirectoryEntry.EndOfFile = pDirEntry->EndOfFile;

        Fcb->DirEntry->DirectoryEntry.AllocationSize = pDirEntry->AllocationSize;

        Fcb->DirEntry->DirectoryEntry.FileAttributes = pDirEntry->FileAttributes;

        if( Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY ||
            Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK ||
            Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
        {

            AFSFileID stFileId;

            Fcb->DirEntry->DirectoryEntry.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

            if( !BooleanFlagOn( Fcb->Flags, AFS_FCB_DIRECTORY_INITIALIZED))
            {

                if( Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY)
                {

                    stFileId = Fcb->DirEntry->DirectoryEntry.FileId;
                }
                else
                {

                    stFileId = Fcb->DirEntry->DirectoryEntry.TargetFileId;
                }

                if( stFileId.Hash != 0)
                {

                    ntStatus = AFSEnumerateDirectory( &Fcb->DirEntry->DirectoryEntry.FileId,
                                                      &Fcb->Specific.Directory.DirectoryNodeHdr,
                                                      &Fcb->Specific.Directory.DirectoryNodeListHead,
                                                      &Fcb->Specific.Directory.DirectoryNodeListTail,
                                                      &Fcb->Specific.Directory.ShortNameTree,
                                                      NULL);
                                                
                    if( !NT_SUCCESS( ntStatus))
                    {

                        try_return( ntStatus);
                    }

                    SetFlag( Fcb->Flags, AFS_FCB_DIRECTORY_INITIALIZED);
                }
            }
        }

        Fcb->DirEntry->DirectoryEntry.EaSize = pDirEntry->EaSize;

        Fcb->DirEntry->DirectoryEntry.Links = pDirEntry->Links;

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
    AFSFcb      *pDcb = NULL, *pFcb = NULL, *pNextFcb = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    AFSDirEntryCB *pCurrentDirEntry = NULL;
    BOOLEAN     bIsChild = FALSE;

    __Enter
    {

        //
        // If this is for the entire volume then flush the root directory
        //

        if( InvalidateCB->WholeVolume)
        {

            AFSRemoveAFSRoot();

            try_return( ntStatus);
        }

        //
        // Need to locate the Fcb for the directory to purge
        //
    
        AFSAcquireShared( &pDevExt->Specific.RDR.FileIDTreeLock, TRUE);
    
        ntStatus = AFSLocateFileIDEntry( pDevExt->Specific.RDR.FileIDTree.TreeHead,
                                         InvalidateCB->FileID.Hash,
                                         &pDcb);

        AFSReleaseResource( &pDevExt->Specific.RDR.FileIDTreeLock );

        if( !NT_SUCCESS( ntStatus) ||
            pDcb == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        AFSAcquireExcl( &pDcb->NPFcb->Resource,
                        TRUE);

        AFSAcquireExcl( pDcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        //
        // Tear down the directory information
        //

        pDcb->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead = NULL;

        pDcb->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead = NULL;

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
                    // Clear out the extents
                    //

                    AFSAcquireExcl( &pFcb->NPFcb->Specific.File.ExtentsResource, 
                                    TRUE);

                    pFcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                    KeSetEvent( &pFcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                0,
                                FALSE);

                    AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource);
                }

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                if( pFcb->DirEntry != NULL)
                {

                    pFcb->DirEntry->Fcb = NULL;
                }

                SetFlag( pFcb->Flags, AFS_FCB_INVALID);

                AFSAcquireExcl( pDevExt->Specific.RDR.FileIDTree.TreeLock,
                                TRUE);

                AFSRemoveFileIDEntry( &pDevExt->Specific.RDR.FileIDTree.TreeHead,
                                      &pFcb->FileIDTreeEntry);

                AFSReleaseResource( pDevExt->Specific.RDR.FileIDTree.TreeLock);

                ClearFlag( pFcb->Flags, AFS_FCB_INSERTED_ID_TREE);

                AFSReleaseResource( &pFcb->NPFcb->Resource);
            }

            AFSDeleteDirEntry( pDcb,
                               pCurrentDirEntry);

            pCurrentDirEntry = pDcb->Specific.Directory.DirectoryNodeListHead;
        }

        pDcb->Specific.Directory.DirectoryNodeListHead = NULL;

        pDcb->Specific.Directory.DirectoryNodeListHead = NULL;

        //
        // Indicate the node is NOT initialized
        //

        ClearFlag( pDcb->Flags, AFS_FCB_DIRECTORY_INITIALIZED);

        //
        // Indicate we need to re-evaluate it on next access
        //

        SetFlag( pDcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);

        AFSReleaseResource( pDcb->Specific.Directory.DirectoryNodeHdr.TreeLock);

        //
        // Tag the Fcbs as invalid so they are torn down
        // Do this for any which has this node as a parent or
        // this parent somewhere in the chain
        //

        AFSAcquireShared( &pDevExt->Specific.RDR.FcbListLock,
                          TRUE);

        pFcb = pDevExt->Specific.RDR.FcbListHead;

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

                    AFSAcquireExcl( &pFcb->NPFcb->Specific.File.ExtentsResource, 
                                    TRUE);

                    pFcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                    KeSetEvent( &pFcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                0,
                                FALSE);

                    AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource);
                }

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                SetFlag( pFcb->Flags, AFS_FCB_INVALID);

                AFSAcquireExcl( pDevExt->Specific.RDR.FileIDTree.TreeLock,
                                TRUE);

                AFSRemoveFileIDEntry( &pDevExt->Specific.RDR.FileIDTree.TreeHead,
                                      &pFcb->FileIDTreeEntry);

                AFSReleaseResource( pDevExt->Specific.RDR.FileIDTree.TreeLock);

                ClearFlag( pFcb->Flags, AFS_FCB_INSERTED_ID_TREE);

                AFSReleaseResource( &pFcb->NPFcb->Resource);
            }

            pNextFcb = (AFSFcb *)pFcb->ListEntry.fLink;
            
            pFcb = pNextFcb;
        }

        AFSReleaseResource( &pDevExt->Specific.RDR.FcbListLock);

        AFSReleaseResource( &pDcb->NPFcb->Resource);

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
