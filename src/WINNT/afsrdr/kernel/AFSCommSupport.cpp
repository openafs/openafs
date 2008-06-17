//
// File: AFSCommSupport.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSEnumerateDirectory( IN AFSFileID          *ParentFileID,
                       IN OUT AFSDirHdr      *DirectoryHdr,
                       IN OUT AFSDirEntryCB **DirListHead,
                       IN OUT AFSDirEntryCB **DirListTail,
                       IN OUT AFSDirEntryCB **ShortNameTree,
                       IN UNICODE_STRING     *CallerSID)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    void *pBuffer = NULL;
    ULONG ulResultLen = 0;
    AFSDirQueryCB *pDirQueryCB;
    AFSDirEnumEntry *pCurrentDirEntry = NULL;
    AFSDirEntryCB *pDirNode = NULL;
    ULONG  ulEntryLength = 0;
    AFSDirEnumResp *pDirEnumResponse = NULL;
    UNICODE_STRING uniDirName, uniTargetName;

    __Enter
    {

        //
        // Initialize the directory enumeration buffer for the directory
        //

        pBuffer = ExAllocatePoolWithTag( PagedPool,
                                         AFS_DIR_ENUM_BUFFER_LEN,
                                         AFS_DIR_BUFFER_TAG);

        if( pBuffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pBuffer,
                       AFS_DIR_ENUM_BUFFER_LEN);

        ulResultLen = AFS_DIR_ENUM_BUFFER_LEN;

        //
        // Use the payload buffer for information we will pass to the service
        //

        pDirQueryCB = (AFSDirQueryCB *)pBuffer;

        pDirQueryCB->EnumHandle = 0;

        //
        // Loop on the information
        //

        while( TRUE)
        {
           
            //
            // Go and retrieve the directory contents
            //

            ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_DIR_ENUM,
                                          AFS_REQUEST_FLAG_SYNCHRONOUS,
                                          0,
                                          NULL,
                                          ParentFileID,
                                          (void *)pDirQueryCB,
                                          sizeof( AFSDirQueryCB),
                                          pBuffer,
                                          &ulResultLen);

            if( !NT_SUCCESS( ntStatus) ||
                ulResultLen == 0)
            {

                if( ntStatus == STATUS_NO_MORE_FILES ||
                    ntStatus == STATUS_NO_MORE_ENTRIES)
                {

                    ntStatus = STATUS_SUCCESS;
                }

                break;
            }

            pDirEnumResponse = (AFSDirEnumResp *)pBuffer;

            pCurrentDirEntry = (AFSDirEnumEntry *)pDirEnumResponse->Entry;

            while( ulResultLen > 0)
            {

                uniDirName.Length = (USHORT)pCurrentDirEntry->FileNameLength;

                uniDirName.MaximumLength = uniDirName.Length;

                uniDirName.Buffer = (WCHAR *)((char *)pCurrentDirEntry + pCurrentDirEntry->FileNameOffset);

                uniTargetName.Length = (USHORT)pCurrentDirEntry->TargetNameLength;

                uniTargetName.MaximumLength = uniTargetName.Length;

                uniTargetName.Buffer = (WCHAR *)((char *)pCurrentDirEntry + pCurrentDirEntry->TargetNameOffset);

                pDirNode = AFSInitDirEntry( ParentFileID,
                                            &uniDirName,
                                            &uniTargetName,
                                            pCurrentDirEntry,
                                            (ULONG)InterlockedIncrement( &DirectoryHdr->ContentIndex));

                if( pDirNode == NULL)
                {

                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                    break;
                }

                //
                // Init the short name if we have one
                //

                if( pCurrentDirEntry->ShortNameLength > 0)
                {

                    UNICODE_STRING uniShortName;

                    pDirNode->DirectoryEntry.ShortNameLength = pCurrentDirEntry->ShortNameLength;

                    RtlCopyMemory( pDirNode->DirectoryEntry.ShortName,
                                   pCurrentDirEntry->ShortName,
                                   pDirNode->DirectoryEntry.ShortNameLength);

                    //
                    // Only perform the short work if it is a file or directory
                    //

                    if( ShortNameTree != NULL &&
                        ( pCurrentDirEntry->FileType == AFS_FILE_TYPE_DIRECTORY ||
                          pCurrentDirEntry->FileType == AFS_FILE_TYPE_FILE))
                    {

                        //
                        // Generate the short name index
                        //

                        uniShortName.Length = pDirNode->DirectoryEntry.ShortNameLength;
                        uniShortName.Buffer = pDirNode->DirectoryEntry.ShortName;

                        pDirNode->Type.Data.ShortNameTreeEntry.Index = AFSGenerateCRC( &uniShortName);
                    }
                }

                //
                // Insert the node into the name tree
                //

                if( DirectoryHdr->TreeHead == NULL)
                {

                    DirectoryHdr->TreeHead = pDirNode;
                }
                else
                {

                    AFSInsertDirEntry( DirectoryHdr->TreeHead,
                                       pDirNode);
                }              

                //
                // If we are given a list to inser the item into, do it here
                //

                if( DirListHead != NULL)
                {

                    if( *DirListHead == NULL)
                    {

                        *DirListHead = pDirNode;
                    }
                    else
                    {

                        (*DirListTail)->ListEntry.fLink = pDirNode;

                        pDirNode->ListEntry.bLink = *DirListTail;
                    }

                    *DirListTail = pDirNode;
                }

                if( ShortNameTree != NULL &&
                    pDirNode->Type.Data.ShortNameTreeEntry.Index != 0)
                {

                    //
                    // Insert the short name entry if we have a valid short name
                    //

                    if( *ShortNameTree == NULL)
                    {

                        *ShortNameTree = pDirNode;
                    }
                    else
                    {

                        AFSInsertShortNameDirEntry( *ShortNameTree,
                                                    pDirNode);
                    }
                }

                break;

                //
                // Next dir entry
                //

                //pCurrentDirEntry = (AFSDirEnumEntry *)((char *)pCurrentDirEntry + ulEntryLength);

                //ulResultLen -= ulEntryLength;
            }

            //
            // If the query was successful then get out since we are done
            //

            if( ntStatus == STATUS_SUCCESS)
            {

                break;
            }

            ASSERT( ntStatus == STATUS_MORE_ENTRIES);

            ulResultLen = AFS_DIR_ENUM_BUFFER_LEN;

            //
            // Reset the information in the request buffer since it got trampled
            // above
            //

            pDirQueryCB->EnumHandle = pDirEnumResponse->EnumHandle;
        }

try_exit:

        //
        // Cleanup
        //

        if( pBuffer != NULL)
        {

            ExFreePool( pBuffer);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyFileCreate( IN AFSFcb *ParentDcb,
                     IN PLARGE_INTEGER FileSize,
                     IN ULONG FileAttributes,
                     IN UNICODE_STRING *FileName,
                     OUT AFSDirEntryCB **DirNode)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFileCreateCB stCreateCB;
    AFSFileCreateResultCB *pResultCB = NULL;
    ULONG ulResultLen = 0;
    UNICODE_STRING uniTargetName;
    AFSDirEntryCB *pDirNode = NULL;

    __Enter
    {

        //
        // Init the control block for the request
        //

        RtlZeroMemory( &stCreateCB,
                       sizeof( AFSFileCreateCB));

        stCreateCB.ParentId = ParentDcb->DirEntry->DirectoryEntry.FileId;

        stCreateCB.AllocationSize = *FileSize;

        stCreateCB.FileAttributes = FileAttributes;

        stCreateCB.EaSize = 0;

        //
        // Allocate our return buffer
        //

        pResultCB = (AFSFileCreateResultCB *)ExAllocatePoolWithTag( PagedPool,
                                                                    PAGE_SIZE,
                                                                    AFS_GENERIC_MEMORY_TAG);

        if( pResultCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pResultCB,
                       PAGE_SIZE);

        ulResultLen = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_CREATE_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      FileName,
                                      NULL,
                                      &stCreateCB,
                                      sizeof( AFSFileCreateCB),
                                      pResultCB,
                                      &ulResultLen);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Initialize the directory entry
        //

        ASSERT( (USHORT)pResultCB->DirEnum.FileNameLength == FileName->Length);

        uniTargetName.Length = (USHORT)pResultCB->DirEnum.TargetNameLength;

        uniTargetName.MaximumLength = uniTargetName.Length;

        uniTargetName.Buffer = (WCHAR *)((char *)&pResultCB->DirEnum + pResultCB->DirEnum.TargetNameOffset);

        pDirNode = AFSInitDirEntry( &(ParentDcb->DirEntry->DirectoryEntry.FileId),
                                    FileName,
                                    &uniTargetName,
                                    &pResultCB->DirEnum,
                                    (ULONG)InterlockedIncrement( &(ParentDcb->Specific.Directory.DirectoryNodeHdr.ContentIndex)));

        if( pDirNode == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Update the parent data version
        //

        ParentDcb->DirEntry->DirectoryEntry.DataVersion = pResultCB->ParentDataVersion;

        //
        // Init the short name if we have one
        //

        if( pResultCB->DirEnum.ShortNameLength > 0)
        {

            UNICODE_STRING uniShortName;

            pDirNode->DirectoryEntry.ShortNameLength = pResultCB->DirEnum.ShortNameLength;

            RtlCopyMemory( pDirNode->DirectoryEntry.ShortName,
                           pResultCB->DirEnum.ShortName,
                           pDirNode->DirectoryEntry.ShortNameLength);

            //
            // Generate the short name index
            //

            uniShortName.Length = pDirNode->DirectoryEntry.ShortNameLength;
            uniShortName.Buffer = pDirNode->DirectoryEntry.ShortName;

            pDirNode->Type.Data.ShortNameTreeEntry.Index = AFSGenerateCRC( &uniShortName);
        }

        //
        // Return the directory node
        //

        *DirNode = pDirNode;

try_exit:

        if( pResultCB != NULL)
        {

            ExFreePool( pResultCB);
        }

        if( !NT_SUCCESS( ntStatus))
        {

        }
    }

    return ntStatus;
}

WCHAR big[1024];

NTSTATUS
AFSUpdateFileInformation( IN PDEVICE_OBJECT DeviceObject,
                          IN AFSFcb *Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    AFSFileUpdateCB stUpdateCB;
    ULONG ulResultLen = 0;

    __Enter
    {

        //
        // Init the control block for the request
        //

        RtlZeroMemory( &stUpdateCB,
                       sizeof( AFSFileUpdateCB));

        stUpdateCB.AllocationSize = Fcb->DirEntry->DirectoryEntry.EndOfFile;

        stUpdateCB.FileAttributes = Fcb->DirEntry->DirectoryEntry.FileAttributes;

        DbgPrint("Setting %wZ attrib %08lX EOF %08lX\n", 
                                        &Fcb->DirEntry->DirectoryEntry.FileName,
                                        Fcb->DirEntry->DirectoryEntry.FileAttributes,
                                        Fcb->DirEntry->DirectoryEntry.EndOfFile.LowPart);

        stUpdateCB.EaSize = Fcb->DirEntry->DirectoryEntry.EaSize;

        stUpdateCB.ParentId = Fcb->DirEntry->DirectoryEntry.ParentId;

        ULONG sz = sizeof(big);

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_UPDATE_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      NULL,
                                      &Fcb->DirEntry->DirectoryEntry.FileId,
                                      &stUpdateCB,
                                      sizeof( AFSFileUpdateCB),
                                      big,
                                      &sz);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyDelete( IN PDEVICE_OBJECT DeviceObject,
                 IN AFSDirEntryCB *DirEntry)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    ULONG ulResultLen = 0;

    __Enter
    {

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_DELETE_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      NULL,
                                      &DirEntry->DirectoryEntry.FileId,
                                      NULL,
                                      0,
                                      NULL,
                                      0);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyRename( IN PDEVICE_OBJECT DeviceObject,
                 IN AFSFcb *Fcb,
                 IN AFSFcb *ParentDcb,
                 IN AFSFcb *TargetDcb,
                 IN UNICODE_STRING *TargetName)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    AFSFileRenameCB *pRenameCB = NULL;
    AFSFileRenameResultCB stResultCB;
    ULONG ulResultLen = 0;

    __Enter
    {

        /*
        //
        // Init the control block for the request
        //

        pRenameCB = (AFSFileRenameCB *)ExAllocatePoolWithTag( PagedPool,
                                                                sizeof( AFSFileRenameCB) + TargetName->Length,
                                                                AFS_RENAME_REQUEST_TAG);

        if( pRenameCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pRenameCB,
                       sizeof( AFSFileRenameCB) + TargetName->Length);

        pRenameCB->ParentId = ParentDcb->DirEntry->FileId;

        pRenameCB->TargetId = TargetDcb->DirEntry->FileId;

        pRenameCB->TargetNameLength = TargetName->Length;

        RtlCopyMemory( pRenameCB->TargetName,
                       TargetName->Buffer,
                       TargetName->Length);

        RtlZeroMemory( &stResultCB,
                       sizeof( AFSFileRenameResultCB));

        ulResultLen = sizeof( AFSFileRenameResultCB);

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_RENAME_FILE,
                                        AFS_REQUEST_FLAG_SYNCHRONOUS,
                                        0,
                                        NULL,
                                        &Fcb->DirEntry->FileId,
                                        pRenameCB,
                                        sizeof( AFSFileRenameCB) + TargetName->Length,
                                        &stResultCB,
                                        &ulResultLen);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Update the passed back FileIndex for the entry
        //

        Fcb->DirEntry->FileIndex = stResultCB.FileIndex;
        */

try_exit:

        if( pRenameCB != NULL)
        {

            ExFreePool( pRenameCB);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSEvaluateTargetByID( IN AFSFileID *ParentFileId,
                       IN AFSFileID *SourceFileId,
                       OUT AFSDirEnumEntry **DirEnumEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSEvalTargetCB stTargetID;
    ULONG ulResultBufferLength;
    AFSDirEnumEntry *pDirEnumCB = NULL;

    __Enter
    {

        stTargetID.ParentId = *ParentFileId;

        //
        // Allocate our response buffer
        //

        pDirEnumCB = (AFSDirEnumEntry *)ExAllocatePoolWithTag( PagedPool,
                                                               PAGE_SIZE,
                                                               AFS_GENERIC_MEMORY_TAG);

        if( pDirEnumCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Call to the service to evaulate the fid
        //

        ulResultBufferLength = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_EVAL_TARGET_BY_ID,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      NULL,
                                      SourceFileId,
                                      &stTargetID,
                                      sizeof( AFSEvalTargetCB),
                                      pDirEnumCB,
                                      &ulResultBufferLength);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Pass back the dir enum entry 
        //

        *DirEnumEntry = pDirEnumCB;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pDirEnumCB != NULL)
            {

                ExFreePool( pDirEnumCB);
            }

            *DirEnumEntry = NULL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSEvaluateTargetByName( IN AFSFileID *ParentFileId,
                         IN PUNICODE_STRING SourceName,
                         OUT AFSDirEnumEntry **DirEnumEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSEvalTargetCB stTargetID;
    ULONG ulResultBufferLength;
    AFSDirEnumEntry *pDirEnumCB = NULL;

    __Enter
    {

        stTargetID.ParentId = *ParentFileId;

        //
        // Allocate our response buffe
        //

        pDirEnumCB = (AFSDirEnumEntry *)ExAllocatePoolWithTag( PagedPool,
                                                               PAGE_SIZE,
                                                               AFS_GENERIC_MEMORY_TAG);

        if( pDirEnumCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Call to the service to evaulate the fid
        //

        ulResultBufferLength = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_EVAL_TARGET_BY_NAME,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      SourceName,
                                      NULL,
                                      &stTargetID,
                                      sizeof( AFSEvalTargetCB),
                                      pDirEnumCB,
                                      &ulResultBufferLength);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Pass back the dir enum entry 
        //

        *DirEnumEntry = pDirEnumCB;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pDirEnumCB != NULL)
            {

                ExFreePool( pDirEnumCB);
            }

            *DirEnumEntry = NULL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSProcessRequest( IN ULONG RequestType,
                   IN ULONG RequestFlags,
                   IN HANDLE CallerProcess,
                   IN PUNICODE_STRING FileName,
                   IN AFSFileID *FileId,
                   IN void  *Data,
                   IN ULONG DataLength,
                   IN OUT void *ResultBuffer,
                   IN OUT PULONG ResultBufferLength)
{

    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSPoolEntry     stPoolEntry, *pPoolEntry = NULL;
    AFSCommSrvcCB   *pCommSrvc = NULL;
    BOOLEAN          bReleasePool = FALSE;
    AFSDeviceExt    *pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    BOOLEAN          bWait = BooleanFlagOn( RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS);
    ULONG            ulPoolEntryLength = 0;
    __try
    {

        pCommSrvc = &pControlDevExt->Specific.Control.CommServiceCB;

        //
        // Grab the pool resource and check the state
        //

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                        TRUE);

        bReleasePool = TRUE;

        if( pCommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
        {

            //
            // Pool not running so bail. 
            //

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        //
        // If this is an async request we need to allocate a pool entry for the request
        //

        pPoolEntry = &stPoolEntry;

        if( !bWait)
        {

            ASSERT( ResultBuffer == NULL);

            ulPoolEntryLength = sizeof( AFSPoolEntry) + QuadAlign( DataLength);

            if( FileName != NULL)
            {

                ulPoolEntryLength += FileName->Length;
            }

            pPoolEntry = (AFSPoolEntry *)ExAllocatePoolWithTag( NonPagedPool,
                                                                ulPoolEntryLength,
                                                                AFS_POOL_ENTRY_TAG);

            if( pPoolEntry == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlZeroMemory( pPoolEntry,
                           ulPoolEntryLength);

            pPoolEntry->Data = (void *)((char *)pPoolEntry + sizeof( AFSPoolEntry));

            pPoolEntry->FileName.Buffer = (WCHAR *)((char *)pPoolEntry->Data + DataLength); 
        }
        else
        {

            RtlZeroMemory( pPoolEntry,
                           sizeof( AFSPoolEntry));

            KeInitializeEvent( &pPoolEntry->Event,
                               NotificationEvent,
                               FALSE);
        }

        pPoolEntry->RequestType = RequestType;

        pPoolEntry->RequestIndex = pCommSrvc->IrpPoolRequestIndex++;

        pPoolEntry->RequestFlags = RequestFlags;

        pPoolEntry->ResultBufferLength = 0;

        if( FileId != NULL)
        {

            pPoolEntry->FileId = *FileId;
        }

        pPoolEntry->FileName.Length = 0;

        if( FileName != NULL)
        {

            if( bWait)
            {

                pPoolEntry->FileName = *FileName;
            }
            else
            {

                pPoolEntry->FileName.Length = FileName->Length;

                pPoolEntry->FileName.MaximumLength = pPoolEntry->FileName.Length;

                RtlCopyMemory( pPoolEntry->FileName.Buffer,
                               FileName->Buffer,
                               pPoolEntry->FileName.Length);
            }
        }

        //
        // Move in the data if there is some
        //

        pPoolEntry->DataLength = DataLength;

        if( Data != NULL &&
            DataLength > 0)
        {

            if( bWait)
            {

                pPoolEntry->Data = Data;
            }
            else
            {

                RtlCopyMemory( pPoolEntry->Data,
                               Data,
                               DataLength);
            }
        }

        pPoolEntry->ResultBuffer = ResultBuffer;

        pPoolEntry->ResultBufferLength = ResultBufferLength;

        //
        // Store off the process id
        //

        if( CallerProcess != 0)
        {

            pPoolEntry->ProcessID = CallerProcess;
        }
        else
        {

            pPoolEntry->ProcessID = PsGetCurrentProcessId();
        }

        //
        // Insert the entry into the request pool
        //

        ntStatus = AFSInsertRequest( pCommSrvc,
                                     pPoolEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            if( !bWait)
            {

                ExFreePool( pPoolEntry);
            }

            try_return( ntStatus);
        }

        //
        // Drop the lock on the pool prior to waiting
        //

        AFSReleaseResource( &pCommSrvc->IrpPoolLock);

        bReleasePool = FALSE;

        //
        // Wait for the result if this is NOT an asynchronous request
        //

        if( bWait)
        {

            //
            // Wait for the result of the request. We specify no timeout ...
            //

            ntStatus = KeWaitForSingleObject( &pPoolEntry->Event,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              NULL);

            //
            // Process the result of the request
            //

            if( NT_SUCCESS( ntStatus))
            {

                ntStatus = pPoolEntry->ResultStatus;
            }
        }

try_exit:

        if( bReleasePool)
        {

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()))
    {

        ntStatus = STATUS_UNSUCCESSFUL;
    }

    return ntStatus;
}

NTSTATUS
AFSProcessControlRequest( IN PIRP Irp)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION  pIrpSp;    
    ULONG               ulIoControlCode;
    BOOLEAN             bCompleteRequest = TRUE;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    ULONG               ulBytesProcessed = 0;

    __try
    {
        
        pIrpSp = IoGetCurrentIrpStackLocation( Irp);

        ulIoControlCode = pIrpSp->Parameters.DeviceIoControl.IoControlCode;

        switch( ulIoControlCode)
        {

            case IOCTL_AFS_INITIALIZE_CONTROL_DEVICE:
            {

                //
                // Go intialize the pool
                //

                ntStatus = AFSInitIrpPool();

                if( !NT_SUCCESS( ntStatus))
                {

                    //
                    // Don't initialize
                    //

                    break;
                }

                //
                // Tag this instance as the one to close the irp pool when it is closed
                //

                pIrpSp->FileObject->FsContext = (void *)((ULONG_PTR)pIrpSp->FileObject->FsContext | AFS_CONTROL_INSTANCE);

                break;
            }

            case IOCTL_AFS_INITIALIZE_REDIRECTOR_DEVICE:
            {

                AFSCacheFileInfo *pCacheFileInfo = (AFSCacheFileInfo *)Irp->AssociatedIrp.SystemBuffer;

                //
                // Extract off the passed in information which contains the
                // cache file parameters
                //

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSCacheFileInfo) ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < (ULONG)FIELD_OFFSET( AFSCacheFileInfo, CacheFileName) +
                                                                                                    pCacheFileInfo->CacheFileNameLength)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                //
                // Initialize the Redirector device
                //

                ntStatus = AFSInitializeRedirector( pCacheFileInfo);

                if( !NT_SUCCESS( ntStatus))
                {

                    break;
                }

                //
                // Stash away context so we know the instance used to initialize the redirector
                //

                pIrpSp->FileObject->FsContext = (void *)((ULONG_PTR)pIrpSp->FileObject->FsContext | AFS_REDIRECTOR_INSTANCE);

                break;
            }

            case IOCTL_AFS_PROCESS_IRP_REQUEST:
            {

                ntStatus = AFSProcessIrpRequest( Irp);

                break;
            }

            case IOCTL_AFS_PROCESS_IRP_RESULT:
            {

                ntStatus = AFSProcessIrpResult( Irp);

                break;
            }

            case IOCTL_AFS_ADD_CONNECTION:
            {

                RedirConnectionCB *pConnectCB = (RedirConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( RedirConnectionCB) ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < (ULONG)FIELD_OFFSET( RedirConnectionCB, RemoteName) +
                                                                                                            pConnectCB->RemoteNameLength ||
                    pIrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof( ULONG))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSAddConnection( pConnectCB,
                                             (PULONG)Irp->AssociatedIrp.SystemBuffer,
                                             &Irp->IoStatus.Information);

                break;                                                  
            }

            case IOCTL_AFS_CANCEL_CONNECTION:
            {

                RedirConnectionCB *pConnectCB = (RedirConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( RedirConnectionCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSCancelConnection( pConnectCB,
                                                (PULONG)Irp->AssociatedIrp.SystemBuffer,
                                                &Irp->IoStatus.Information);
                      
                break;
            }

            case IOCTL_AFS_GET_CONNECTION:
            {

                RedirConnectionCB *pConnectCB = (RedirConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( RedirConnectionCB) ||
                    pIrpSp->Parameters.DeviceIoControl.OutputBufferLength == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSGetConnection( pConnectCB,
                                             (WCHAR *)Irp->AssociatedIrp.SystemBuffer,
                                             pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                             &Irp->IoStatus.Information);
                      
                break;
            }

            case IOCTL_AFS_LIST_CONNECTIONS:
            {

                if( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSListConnections( (RedirConnectionCB *)Irp->AssociatedIrp.SystemBuffer,
                                                pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                                &Irp->IoStatus.Information);
                      
                break;
            }

            case IOCTL_AFS_SET_FILE_EXTENTS:
            {
                AFSSetFileExtentsCB *pExtents = (AFSSetFileExtentsCB*) Irp->AssociatedIrp.SystemBuffer;
                //
                // Check lengths twice so that if the buffer makes the
                // count invalid we will not Accvio
                //

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( RedirConnectionCB) ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength <
                    (sizeof( RedirConnectionCB) + (sizeof (AFSFileExtentCB) * (pExtents->ExtentCount - 1 ))))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSProcessSetFileExtents( pExtents );

                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = ntStatus;
                      
                break;
            }
            default:
            {

                ntStatus = STATUS_NOT_IMPLEMENTED;

                break;
            }
        }

//try_exit:

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()))
    {

        ntStatus = STATUS_UNSUCCESSFUL;
    }

    if( bCompleteRequest)
    {

        Irp->IoStatus.Status = ntStatus;

        AFSCompleteRequest( Irp,
                              ntStatus);
    }

    return ntStatus;
}

NTSTATUS
AFSInitIrpPool()
{

    NTSTATUS       ntStatus = STATUS_SUCCESS;    
    AFSCommSrvcCB *pCommSrvc = NULL;
    BOOLEAN        bReleasePools = FALSE;        
    AFSDeviceExt  *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        pCommSrvc = &pDevExt->Specific.Control.CommServiceCB;

        //
        // WHenever we change state we must grab both pool locks. On the checking of the state
        // within the processing routines for these respective pools, we only grab one lock to
        // minimize serialization. The ordering is always the Irp pool then the result pool
        // locks. We also do this in the tear down of the pool
        //

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                          TRUE);

        AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                          TRUE);

        bReleasePools = TRUE;

        //
        // The pool can be either ACTIVE or INACTIVE. If the pool state is INACTIVE and we 
        // are receiving the INIT request, then activate it. If the pool is ACTIVE, then we
        // shouldn't be getting this request ...
        //

        if( pCommSrvc->IrpPoolControlFlag == POOL_ACTIVE)
        {

            //
            // We have already been activated so just fail this request
            //

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }
        else if( pCommSrvc->IrpPoolControlFlag == POOL_INACTIVE)
        {

            //
            // The pool is currently INACTIVE so start it up and ready it to 
            // receive irp requests
            //

            pCommSrvc->IrpPoolControlFlag = POOL_ACTIVE;

            pDevExt->Specific.Control.ServiceProcess = (PKPROCESS)PsGetCurrentProcess();

            try_return( ntStatus = STATUS_SUCCESS);
        }
        else
        {

            //
            // The pool is in some mixed state, fail the request.
            //

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

try_exit:

        if( bReleasePools)
        {

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);

            AFSReleaseResource( &pCommSrvc->ResultPoolLock);
        }
    }

    return ntStatus;
}

void
AFSCleanupIrpPool()
{

    NTSTATUS        ntStatus = STATUS_SUCCESS;
    AFSPoolEntry   *pEntry = NULL, *pNextEntry = NULL;
    AFSDeviceExt   *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSCommSrvcCB  *pCommSrvc = (AFSCommSrvcCB *)&pDevExt->Specific.Control.CommServiceCB;

    __Enter
    {

        //
        // When we change the state, grab both pool locks exclusive. The order is always the 
        // Irp pool then the result pool lock
        //

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                        TRUE);

        AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                        TRUE);

        //
        // Indicate we are pending stop
        //

        pCommSrvc->IrpPoolControlFlag = POOL_INACTIVE;

        //
        // Set the event to release any waiting workers
        //

        KeSetEvent( &pCommSrvc->IrpPoolHasEntries,
                    0,
                    FALSE);

        //
        // Go through the pool entries and free up the structures.
        //

        pEntry = pCommSrvc->RequestPoolHead;

        while( pEntry != NULL)
        {

            pNextEntry = pEntry->fLink;

            if( BooleanFlagOn( pEntry->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS))
            {

                //
                // Here we need to complete the irp, cancelled, and delete the data block
                //

                pEntry->ResultStatus = STATUS_CANCELLED;

                KeSetEvent( &pEntry->Event,
                            0,
                            FALSE);
            }
            else
            {

                ExFreePool( pEntry);
            }

            pEntry = pNextEntry;
        }

        //
        // Cleanup the control structure for the request pool
        //

        pCommSrvc->RequestPoolHead = NULL;

        pCommSrvc->RequestPoolTail = NULL;

        pCommSrvc->IrpPoolRequestIndex = 1;

        KeClearEvent( &pCommSrvc->IrpPoolHasEntries);

        //
        // Release the irp pool lock. 
        //

        AFSReleaseResource( &pCommSrvc->IrpPoolLock);

        //
        // Go through the result pool entries and free up the structures.
        //

        pEntry = pCommSrvc->ResultPoolHead;

        while( pEntry != NULL)
        {

            pNextEntry = pEntry->fLink;

            pEntry->ResultStatus = STATUS_CANCELLED;

            //
            // Here we will set the event of the requestor and let the blocked thread
            // free the data block
            //

            KeSetEvent( &pEntry->Event,
                        0,
                        FALSE);

            //
            // Go onto the next entry
            //

            pEntry = pNextEntry;
        }

        //
        // Cleanup the control structure for the result pool
        //

        pCommSrvc->ResultPoolHead = NULL;

        pCommSrvc->ResultPoolTail = NULL;

        //
        // Release the result pool lock. 
        //

        AFSReleaseResource( &pCommSrvc->ResultPoolLock);
    }

    return;
}

NTSTATUS
AFSInsertRequest( IN AFSCommSrvcCB *CommSrvc,
                  IN AFSPoolEntry *Entry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        AFSAcquireExcl( &CommSrvc->IrpPoolLock,
                        TRUE);

        if( CommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
        {

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        if( CommSrvc->RequestPoolHead == NULL)
        {

            CommSrvc->RequestPoolHead = Entry;

            KeSetEvent( &CommSrvc->IrpPoolHasEntries,
                        0,
                        FALSE);
        }
        else
        {

            CommSrvc->RequestPoolTail->fLink = Entry;

            Entry->bLink = CommSrvc->RequestPoolTail;        
        }

        CommSrvc->RequestPoolTail = Entry;

try_exit:

        AFSReleaseResource( &CommSrvc->IrpPoolLock);
    }

    return ntStatus;
}

NTSTATUS 
AFSProcessIrpRequest( IN PIRP Irp)
{

    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSDeviceExt    *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSCommSrvcCB   *pCommSrvc = NULL;
    AFSPoolEntry    *pEntry = NULL;
    AFSCommRequest  *pRequest = NULL;

    __Enter
    {

        pCommSrvc = &pDevExt->Specific.Control.CommServiceCB;

        pRequest = (AFSCommRequest *)Irp->AssociatedIrp.SystemBuffer;

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                        TRUE);

        if( pCommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
        {

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        AFSReleaseResource( &pCommSrvc->IrpPoolLock);

        //
        // Wait on the 'have items' event until we can retrieve an item
        //

        while( TRUE)
        {

            ntStatus = KeWaitForSingleObject( &pCommSrvc->IrpPoolHasEntries,
                                              UserRequest,
                                              UserMode,
                                              TRUE,
                                              NULL);

            if( ntStatus != STATUS_SUCCESS)
            {
                
                ntStatus = STATUS_DEVICE_NOT_READY;

                break;
            }

            //
            // Grab the lock on the request pool
            //

            AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                            TRUE);

            if( pCommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
            {

                AFSReleaseResource( &pCommSrvc->IrpPoolLock);

                try_return( ntStatus = STATUS_DEVICE_NOT_READY);
            }

            pEntry = pCommSrvc->RequestPoolHead;

            if( pEntry != NULL)
            {

                pCommSrvc->RequestPoolHead = pEntry->fLink;

                pEntry->bLink = NULL;

                if( pCommSrvc->RequestPoolHead == NULL)
                {

                    pCommSrvc->RequestPoolTail = NULL;
                }
            }
            else
            {

                KeClearEvent( &pCommSrvc->IrpPoolHasEntries);
            }

            //
            // And release the request pool lock
            //

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);

            //
            // Insert the entry into the result pool, if we have one
            //

            if( pEntry != NULL)
            {
                
                //
                // Move the request data into the passed in buffer
                //

                ASSERT( sizeof( AFSCommRequest) + 
                                    pEntry->FileName.Length +
                                    pEntry->DataLength <= pIrpSp->Parameters.DeviceIoControl.OutputBufferLength);

                pRequest->ProcessId = (ULONG)pEntry->ProcessID;

                pRequest->FileId = pEntry->FileId;

                pRequest->RequestType = pEntry->RequestType;

                pRequest->RequestIndex = pEntry->RequestIndex;

                pRequest->RequestFlags = pEntry->RequestFlags;

                pRequest->NameLength = pEntry->FileName.Length;

                if( pRequest->NameLength > 0)
                {

                    RtlCopyMemory( pRequest->Name,
                                   pEntry->FileName.Buffer,
                                   pRequest->NameLength);
                }

                pRequest->DataOffset = 0;

                pRequest->DataLength = pEntry->DataLength;

                if( pRequest->DataLength > 0)
                {

                    pRequest->DataOffset = pEntry->FileName.Length;

                    RtlCopyMemory( (void *)((char *)pRequest->Name + pRequest->DataOffset),
                                   pEntry->Data,
                                   pRequest->DataLength);
                }

                pRequest->SIDOffset = 0;

                pRequest->SIDLength = 0;

                pRequest->ResultBufferLength = 0;

                if( pEntry->ResultBufferLength != NULL)
                {

                    pRequest->ResultBufferLength = *(pEntry->ResultBufferLength);
                }

                Irp->IoStatus.Information = sizeof( AFSCommRequest) + 
                                                        pEntry->FileName.Length +
                                                        pEntry->DataLength;

                //
                // If this is a synchronous request then move the request into the 
                // result pool
                //

                if( BooleanFlagOn( pEntry->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS))
                {

                    pEntry->fLink = NULL;
                    pEntry->bLink = NULL;

                    AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                                    TRUE);

                    if( pCommSrvc->ResultPoolHead == NULL)
                    {

                        pCommSrvc->ResultPoolHead = pEntry;
                    }
                    else
                    {

                        pCommSrvc->ResultPoolTail->fLink = pEntry;

                        pEntry->bLink = pCommSrvc->ResultPoolTail;
                    }

                    pCommSrvc->ResultPoolTail = pEntry;

                    AFSReleaseResource( &pCommSrvc->ResultPoolLock);
                }
                else
                {

                    //
                    // Free up the pool entry
                    //

                    ExFreePool( pEntry);
                }

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS 
AFSProcessIrpResult( IN PIRP Irp)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    IO_STACK_LOCATION  *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSCommSrvcCB      *pCommSrvc = NULL;
    AFSPoolEntry       *pCurrentEntry = NULL;
    AFSCommResult      *pResult = NULL;

    __Enter
    {

        pCommSrvc = &pDevExt->Specific.Control.CommServiceCB;

        //
        // Get the request for the incoming result
        //

        pResult = (AFSCommResult *)Irp->AssociatedIrp.SystemBuffer;

        if( pResult == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // Go look for our entry
        //

        AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                          TRUE);

        pCurrentEntry = pCommSrvc->ResultPoolHead;

        while( pCurrentEntry != NULL)
        {

            if( pCurrentEntry->RequestIndex == pResult->RequestIndex)
            {

                //
                // Found the entry so remove it from the queue
                //

                if( pCurrentEntry->bLink == NULL)
                {

                    //
                    // At the head of the list
                    //

                    pCommSrvc->ResultPoolHead = pCurrentEntry->fLink;

                    if( pCommSrvc->ResultPoolHead != NULL)
                    {

                        pCommSrvc->ResultPoolHead->bLink = NULL;
                    }
                }
                else
                {

                    pCurrentEntry->bLink->fLink = pCurrentEntry->fLink;
                }

                if( pCurrentEntry->fLink == NULL)
                {

                    pCommSrvc->ResultPoolTail = pCurrentEntry->bLink;

                    if( pCommSrvc->ResultPoolTail != NULL)
                    {

                        pCommSrvc->ResultPoolTail->fLink = NULL;
                    }
                }
                else
                {

                    pCurrentEntry->fLink->bLink = pCurrentEntry->bLink;
                }

                break;
            }

            pCurrentEntry = pCurrentEntry->fLink;
        }

        AFSReleaseResource( &pCommSrvc->ResultPoolLock);

        if( pCurrentEntry == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // OK, move in the result information
        //

        pCurrentEntry->ResultStatus = pResult->ResultStatus;

        if( NT_SUCCESS( pCurrentEntry->ResultStatus) &&
            pCurrentEntry->ResultBufferLength != NULL &&
            pCurrentEntry->ResultBuffer != NULL)
        {

            ASSERT( pResult->ResultBufferLength <= *(pCurrentEntry->ResultBufferLength));                       

            *(pCurrentEntry->ResultBufferLength) = pResult->ResultBufferLength;

            if( pResult->ResultBufferLength > 0)
            {

                RtlCopyMemory( pCurrentEntry->ResultBuffer,
                               pResult->ResultData,
                               pResult->ResultBufferLength);
            }
        }

        KeSetEvent( &pCurrentEntry->Event,
                    0,
                    FALSE);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

        }
    }

    return ntStatus;
}
