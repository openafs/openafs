//
// File: AFSFcbSupport.cpp
//

#include "AFSCommon.h"

//
// Function: AFSInitFcb
//
// Description:
//
//      This function performs Fcb initialization
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSInitFcb( IN AFSFcb          *ParentFcb,
            IN PUNICODE_STRING  FileName,
            IN AFSDirEntryCB   *DirEntry,
            IN OUT AFSFcb     **Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    AFSNonPagedFcb *pNPFcb = NULL;
    IO_STATUS_BLOCK stIoSb = {0,0};
    BOOLEAN bUninitFileLock = FALSE;
    USHORT  usFcbLength = 0;

    __Enter
    {

        //
        // Allocate the Fcb and the nonpaged portion of the Fcb.
        //

        usFcbLength = sizeof( AFSFcb);

        pFcb = (AFSFcb *)ExAllocatePoolWithTag( PagedPool,
                                                usFcbLength,
                                                AFS_FCB_ALLOCATION_TAG);

        if( pFcb == NULL)
        {

            AFSPrint("AFSInitFcb Failed to allocate the Fcb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pFcb,
                       usFcbLength);

        pFcb->Header.NodeByteSize = usFcbLength;
        pFcb->Header.NodeTypeCode = AFS_FILE_FCB;

        pNPFcb = (AFSNonPagedFcb *)ExAllocatePoolWithTag( NonPagedPool,
                                                          sizeof( AFSNonPagedFcb),
                                                          AFS_FCB_NP_ALLOCATION_TAG);

        if( pNPFcb == NULL)
        {

            AFSPrint("AFSInitFcb Failed to allocate nonpaged portion of fcb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pNPFcb,
                       sizeof( AFSNonPagedFcb));

        pNPFcb->Size = sizeof( AFSNonPagedFcb);
        pNPFcb->Type = AFS_NON_PAGED_FCB;

        //
        // Initialize the advanced header
        //

        ExInitializeFastMutex( &pNPFcb->AdvancedHdrMutex);

        FsRtlSetupAdvancedHeader( &pFcb->Header, &pNPFcb->AdvancedHdrMutex);

        //
        // OK, initialize the entry
        //
        ExInitializeResourceLite( &pNPFcb->Resource);

        ExInitializeResourceLite( &pNPFcb->PagingResource);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Header.PagingIoResource = &pNPFcb->PagingResource;

        //
        // Grab teh Fcb for processing
        //

        AFSAcquireExcl( &pNPFcb->Resource,
                        TRUE);

        pFcb->NPFcb = pNPFcb;

        //
        // Initialize some fields in the Fcb
        //

        pFcb->ParentFcb = ParentFcb;

        pFcb->RootFcb = ParentFcb->RootFcb;

        DirEntry->Type.Data.Fcb = pFcb;

        pFcb->DirEntry = DirEntry;

        pFcb->VolumeNode = ParentFcb->VolumeNode;

        if( DirEntry != NULL)
        {

            //
            // Set the index for the file id tree entry
            //

            pFcb->FileIDTreeEntry.Index = DirEntry->DirectoryEntry.FileId.Hash;

            //
            // Set type specific information
            //

            if( BooleanFlagOn( DirEntry->DirectoryEntry.FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
            {

                //
                // Reset the type to a directory type
                //

                pFcb->Header.NodeTypeCode = AFS_DIRECTORY_FCB;

                //
                // Lock for the directory tree
                //

                ExInitializeResourceLite( &pNPFcb->Specific.Directory.DirectoryTreeLock);

                pFcb->Specific.Directory.DirectoryNodeHdr.TreeLock = &pNPFcb->Specific.Directory.DirectoryTreeLock;
            }
            else
            {

                //
                // Initialize the file specific informaiton
                //

                FsRtlInitializeFileLock( &pFcb->Specific.File.FileLock,
                                         NULL,
                                         NULL);

                bUninitFileLock = TRUE;

                //
                // Initialize the header file sizes to our dir entry information
                //

                pFcb->Header.AllocationSize.QuadPart = pFcb->DirEntry->DirectoryEntry.AllocationSize.QuadPart;
                pFcb->Header.FileSize.QuadPart = pFcb->DirEntry->DirectoryEntry.EndOfFile.QuadPart;
                pFcb->Header.ValidDataLength.QuadPart = pFcb->DirEntry->DirectoryEntry.EndOfFile.QuadPart;
                //
                // Initialize the Extents resources and so forth
                //
                ExInitializeResourceLite( &pNPFcb->Specific.File.ExtentsResource );
                KeInitializeEvent( &pNPFcb->Specific.File.ExtentsNotBusy, 
                                   NotificationEvent, 
                                   FALSE );
                KeInitializeEvent( &pNPFcb->Specific.File.ExtentsRequestComplete, 
                                   NotificationEvent, 
                                   TRUE );

                pFcb->Specific.File.ExtentsList.bLink = &pFcb->Specific.File.ExtentsList;
                pFcb->Specific.File.ExtentsList.fLink = &pFcb->Specific.File.ExtentsList;
            }

            //
            // Increment the reference on the parent. We do this for any newly created fcb
            // to control the tear down of the fcbs. This way the child will always be torn
            // down before the parent.
            //

            InterlockedIncrement( &ParentFcb->Specific.Directory.ChildReferenceCount);

            //
            // Initialize the access time
            //

            KeQueryTickCount( &pFcb->LastAccessCount);

            //
            // Insert the Fcb into our linked list
            // Note: In this case only can we have the Fcb lock held while acquiring the
            // tree locks since the Fcb is not in the list or tree yet there will be
            // no caller waiting on the Fcb.
            //

            AFSAcquireExcl( &pDeviceExt->Specific.RDR.FcbListLock,
                            TRUE);

            if( pDeviceExt->Specific.RDR.FcbListHead == NULL)
            {

                pDeviceExt->Specific.RDR.FcbListHead = pFcb;
            }
            else
            {

                pDeviceExt->Specific.RDR.FcbListTail->ListEntry.fLink = pFcb;

                pFcb->ListEntry.bLink = pDeviceExt->Specific.RDR.FcbListTail;
            }

            pDeviceExt->Specific.RDR.FcbListTail = pFcb;

            AFSReleaseResource( &pDeviceExt->Specific.RDR.FcbListLock);

            //
            // And FileID tree
            //

            AFSAcquireExcl( pDeviceExt->Specific.RDR.FileIDTree.TreeLock,
                            TRUE);

            if( pDeviceExt->Specific.RDR.FileIDTree.TreeHead == NULL)
            {
                pFcb->FileIDTreeEntry.parentLink = NULL;
                pDeviceExt->Specific.RDR.FileIDTree.TreeHead = &pFcb->FileIDTreeEntry;
            }
            else
            {

                ntStatus = AFSInsertFileIDEntry( pDeviceExt->Specific.RDR.FileIDTree.TreeHead,
                                                 &pFcb->FileIDTreeEntry);

                ASSERT( NT_SUCCESS( ntStatus));
            }

            AFSReleaseResource( pDeviceExt->Specific.RDR.FileIDTree.TreeLock);
        }

        //
        // And return the Fcb and Ccb
        //

        *Fcb = pFcb;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSInitFcb Failed to initialize Fcb Status %08lX\n", ntStatus);

            if( pFcb != NULL)
            {

                if( bUninitFileLock)
                {

                    FsRtlUninitializeFileLock( &pFcb->Specific.File.FileLock);
                }

                if( pNPFcb != NULL)
                {

                    AFSReleaseResource( &pNPFcb->Resource);
                    
                    ExDeleteResourceLite( &pNPFcb->PagingResource);

                    ExDeleteResourceLite( &pNPFcb->Resource);
                }

                ExFreePool( pFcb);
            }

            if( pNPFcb != NULL)
            {

                ExFreePool( pNPFcb);
            }

            *Fcb = NULL;
        }
    }

    return ntStatus;
}

//
// Function: AFSInitAFSRoot
//
// Description:
//
//      This function performs AFS Root node Fcb initialization
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSInitAFSRoot()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pFcb = NULL;
    AFSNonPagedFcb *pNPFcb = NULL;
    IO_STATUS_BLOCK stIoStatus = {0,0};
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSDirEntryCB *pVolumeDirEntry = NULL;

    __Enter
    {

        if( AFSAllRoot != NULL)
        {

            //
            // Already initialized
            //

            try_return( ntStatus);
        }

        //
        // Allocate our volume directory entry for the root node
        //

        pVolumeDirEntry = (AFSDirEntryCB *)ExAllocatePoolWithTag( PagedPool,
                                                                  sizeof( AFSDirEntryCB),
                                                                  AFS_DIR_ENTRY_TAG);

        if( pVolumeDirEntry == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pVolumeDirEntry,
                       sizeof( AFSDirEntryCB));

        pVolumeDirEntry->Type.Volume.VolumeInformation.TotalAllocationUnits.QuadPart = 0x100000;

        pVolumeDirEntry->Type.Volume.VolumeInformation.AvailableAllocationUnits.QuadPart = 0x100000;        

        pVolumeDirEntry->Type.Volume.VolumeInformation.Characteristics = FILE_REMOTE_DEVICE;
                                    
        pVolumeDirEntry->Type.Volume.VolumeInformation.SectorsPerAllocationUnit = 1;

        pVolumeDirEntry->Type.Volume.VolumeInformation.BytesPerSector = 0x400;

        pVolumeDirEntry->Type.Volume.VolumeInformation.VolumeLabelLength = 12;

        RtlCopyMemory( pVolumeDirEntry->Type.Volume.VolumeInformation.VolumeLabel,
                       L"AFSVol",
                       12);

        KeQuerySystemTime( &pVolumeDirEntry->DirectoryEntry.CreationTime);
        KeQuerySystemTime( &pVolumeDirEntry->DirectoryEntry.LastWriteTime);
        KeQuerySystemTime( &pVolumeDirEntry->DirectoryEntry.LastAccessTime);

        pVolumeDirEntry->Type.Volume.VolumeInformation.VolumeCreationTime = pVolumeDirEntry->DirectoryEntry.CreationTime;

        //
        // Allocate the non paged portion
        //

        pVolumeDirEntry->NPDirNode = (AFSNonPagedDirNode *)ExAllocatePoolWithTag( NonPagedPool,  
                                                                                  sizeof( AFSNonPagedDirNode),
                                                                                  AFS_DIR_ENTRY_NP_TAG);

        if( pVolumeDirEntry->NPDirNode == NULL)
        {

            ExFreePool( pVolumeDirEntry);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Initialize the non-paged portion of the dire entry
        //

        AFSInitNonPagedDirEntry( pVolumeDirEntry);

        //
        // Initialize the root fcb
        //

        pFcb = (AFSFcb *)ExAllocatePoolWithTag( PagedPool,
                                                  sizeof( AFSFcb),
                                                  AFS_FCB_ALLOCATION_TAG);

        if( pFcb == NULL)
        {

            AFSPrint("AFSInitRootFcb Failed to allocate the root fcb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pFcb,
                       sizeof( AFSFcb)); 

        pFcb->Header.NodeByteSize = sizeof( AFSFcb);
        pFcb->Header.NodeTypeCode = AFS_ROOT_ALL;

        pNPFcb = (AFSNonPagedFcb *)ExAllocatePoolWithTag( NonPagedPool,
                                                          sizeof( AFSNonPagedFcb),
                                                          AFS_FCB_NP_ALLOCATION_TAG);

        if( pNPFcb == NULL)
        {

            AFSPrint("AFSInitRootFcb Failed to allocate the nonpaged fcb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pNPFcb,
                       sizeof( AFSNonPagedFcb));

        pNPFcb->Size = sizeof( AFSNonPagedFcb);
        pNPFcb->Type = AFS_NON_PAGED_FCB;

        //
        // OK, initialize the entry
        //

        ExInitializeFastMutex( &pNPFcb->AdvancedHdrMutex);

        FsRtlSetupAdvancedHeader( &pFcb->Header, &pNPFcb->AdvancedHdrMutex);

        ExInitializeResourceLite( &pNPFcb->Resource);

        ExInitializeResourceLite( &pNPFcb->Specific.Directory.DirectoryTreeLock);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Specific.Directory.DirectoryNodeHdr.TreeLock = &pNPFcb->Specific.Directory.DirectoryTreeLock;

        pFcb->NPFcb = pNPFcb;

        //
        // Point the root at itself
        //

        pFcb->RootFcb = pFcb;

        //
        // The notification information
        //

        InitializeListHead( &pFcb->NPFcb->DirNotifyList);
        FsRtlNotifyInitializeSync( &pFcb->NPFcb->NotifySync);

        //
        // Initialize the Root directory DirEntry
        //

        pFcb->DirEntry = (AFSDirEntryCB *)ExAllocatePoolWithTag( PagedPool,
                                                                 sizeof( AFSDirEntryCB) + sizeof( WCHAR),
                                                                 AFS_DIR_ENTRY_TAG);

        if( pFcb->DirEntry == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pFcb->DirEntry,
                       sizeof( AFSDirEntryCB) + sizeof( WCHAR));

        pFcb->DirEntry->DirectoryEntry.CreationTime.QuadPart = pVolumeDirEntry->DirectoryEntry.CreationTime.QuadPart;
        pFcb->DirEntry->DirectoryEntry.LastWriteTime.QuadPart = pVolumeDirEntry->DirectoryEntry.CreationTime.QuadPart;
        pFcb->DirEntry->DirectoryEntry.LastAccessTime.QuadPart = pVolumeDirEntry->DirectoryEntry.CreationTime.QuadPart;

        pFcb->DirEntry->DirectoryEntry.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

        pFcb->DirEntry->DirectoryEntry.FileName.Length = sizeof( WCHAR);
    
        pFcb->DirEntry->DirectoryEntry.FileName.Buffer = (WCHAR *)((char *)pFcb->DirEntry + sizeof( AFSDirEntryCB));

        RtlCopyMemory( pFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                       L"&",
                       sizeof( WCHAR));

        pFcb->VolumeNode = pVolumeDirEntry;

        //
        // Initialize the root information
        //

        pDevExt->Specific.RDR.DirectoryNameTree.ContentIndex = 1;

        //
        // Enumerate the shares in the volume
        //

        ntStatus = AFSEnumerateDirectory( &pFcb->DirEntry->DirectoryEntry.FileId,
                                          &pDevExt->Specific.RDR.DirectoryNameTree,
                                          &pFcb->Specific.Directory.DirectoryNodeListHead,
                                          &pFcb->Specific.Directory.DirectoryNodeListTail,
                                          NULL,
                                          NULL);

        //
        // Indicate the node is initialized
        //

        SetFlag( pFcb->Flags, AFS_FCB_DIRECTORY_INITIALIZED);

        AFSAllRoot = pFcb;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pFcb != NULL)
            {

                AFSRemoveRootFcb( pFcb);
            }
        }
    }

    return ntStatus;
}

//
// Function: AFSInitRootFcb
//
// Description:
//
//      This function performs Root node Fcb initialization
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSInitRootFcb( IN AFSDirEntryCB *VolumeDirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pFcb = NULL;
    AFSNonPagedFcb *pNPFcb = NULL;
    IO_STATUS_BLOCK stIoStatus = {0,0};

    __Enter
    {

        //
        // Initialize the root fcb
        //

        pFcb = (AFSFcb *)ExAllocatePoolWithTag( PagedPool,
                                                  sizeof( AFSFcb),
                                                  AFS_FCB_ALLOCATION_TAG);

        if( pFcb == NULL)
        {

            AFSPrint("AFSInitRootFcb Failed to allocate the root fcb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pFcb,
                       sizeof( AFSFcb)); 

        pFcb->Header.NodeByteSize = sizeof( AFSFcb);
        pFcb->Header.NodeTypeCode = AFS_ROOT_FCB;

        pNPFcb = (AFSNonPagedFcb *)ExAllocatePoolWithTag( NonPagedPool,
                                                          sizeof( AFSNonPagedFcb),
                                                          AFS_FCB_NP_ALLOCATION_TAG);

        if( pNPFcb == NULL)
        {

            AFSPrint("AFSInitRootFcb Failed to allocate the nonpaged fcb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pNPFcb,
                       sizeof( AFSNonPagedFcb));

        pNPFcb->Size = sizeof( AFSNonPagedFcb);
        pNPFcb->Type = AFS_NON_PAGED_FCB;

        //
        // OK, initialize the entry
        //

        ExInitializeFastMutex( &pNPFcb->AdvancedHdrMutex);

        FsRtlSetupAdvancedHeader( &pFcb->Header, &pNPFcb->AdvancedHdrMutex);

        ExInitializeResourceLite( &pNPFcb->Resource);

        ExInitializeResourceLite( &pNPFcb->Specific.Directory.DirectoryTreeLock);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Specific.Directory.DirectoryNodeHdr.TreeLock = &pNPFcb->Specific.Directory.DirectoryTreeLock;

        pFcb->NPFcb = pNPFcb;

        //
        // Point the root at itself
        //

        pFcb->RootFcb = pFcb;

        //
        // The notification information
        //

        InitializeListHead( &pFcb->NPFcb->DirNotifyList);
        FsRtlNotifyInitializeSync( &pFcb->NPFcb->NotifySync);

        //
        // Initialize the Root directory DirEntry
        //

        pFcb->DirEntry = (AFSDirEntryCB *)ExAllocatePoolWithTag( PagedPool,
                                                                 sizeof( AFSDirEntryCB) + sizeof( WCHAR),
                                                                 AFS_DIR_ENTRY_TAG);

        if( pFcb->DirEntry == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pFcb->DirEntry,
                       sizeof( AFSDirEntryCB) + sizeof( WCHAR));

        pFcb->DirEntry->DirectoryEntry.FileId = VolumeDirEntry->DirectoryEntry.TargetFileId;

        pFcb->DirEntry->DirectoryEntry.CreationTime.QuadPart = VolumeDirEntry->DirectoryEntry.CreationTime.QuadPart;
        pFcb->DirEntry->DirectoryEntry.LastWriteTime.QuadPart = VolumeDirEntry->DirectoryEntry.CreationTime.QuadPart;
        pFcb->DirEntry->DirectoryEntry.LastAccessTime.QuadPart = VolumeDirEntry->DirectoryEntry.CreationTime.QuadPart;

        pFcb->DirEntry->DirectoryEntry.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

        pFcb->DirEntry->DirectoryEntry.FileName.Length = sizeof( WCHAR);
    
        pFcb->DirEntry->DirectoryEntry.FileName.Buffer = (WCHAR *)((char *)pFcb->DirEntry + sizeof( AFSDirEntryCB));

        RtlCopyMemory( pFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                       L"\\",
                       sizeof( WCHAR));

        pFcb->VolumeNode = VolumeDirEntry;

        //
        // If the fid, the target of the MP, is zero then we need to ask the service to evaluate it for us
        //

        if( pFcb->DirEntry->DirectoryEntry.FileId.Hash == 0)
        {

            AFSDirEnumEntry *pDirEnumCB = NULL;

            ntStatus = AFSEvaluateTargetByID( &VolumeDirEntry->DirectoryEntry.ParentId,
                                              &VolumeDirEntry->DirectoryEntry.FileId,
                                              &pDirEnumCB);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }

            //
            // If the target fid is zero it could not be evaluated
            //

            if( pDirEnumCB->TargetFileId.Hash != 0)
            {

                //
                // Update the target fid in the volume entry and Fcb
                //

                VolumeDirEntry->DirectoryEntry.TargetFileId = pDirEnumCB->TargetFileId;
                
                pFcb->DirEntry->DirectoryEntry.FileId = pDirEnumCB->TargetFileId;
            }

            ExFreePool( pDirEnumCB);
        }

        //
        // Go prime the root directory node for this volume if we have a valid fid
        //

        if( pFcb->DirEntry->DirectoryEntry.FileId.Hash != 0)
        {

            ntStatus = AFSEnumerateDirectory( &pFcb->DirEntry->DirectoryEntry.FileId,
                                              &pFcb->Specific.Directory.DirectoryNodeHdr,
                                              &pFcb->Specific.Directory.DirectoryNodeListHead,
                                              &pFcb->Specific.Directory.DirectoryNodeListTail,
                                              &pFcb->Specific.Directory.ShortNameTree,
                                              NULL);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }
        }

        //
        // Set the root node in the passed in entry
        //

        VolumeDirEntry->Type.Volume.RootFcb = pFcb;


        //
        // Indicate the node is initialized
        //

        SetFlag( pFcb->Flags, AFS_FCB_DIRECTORY_INITIALIZED);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pFcb != NULL)
            {

                AFSRemoveRootFcb( pFcb);
            }
        }
    }

    return ntStatus;
}

//
// Function: AFSRemoveRootFcb
//
// Description:
//
//      This function performs root Fcb removal/deallocation
//
// Return:
//
//      A status is returned for the function
//

void
AFSRemoveRootFcb( IN AFSFcb *RootFcb)
{

    if( RootFcb->NPFcb != NULL)
    {

        //
        // Now the resource
        //

        ExDeleteResourceLite( &RootFcb->NPFcb->Resource);

        ExDeleteResourceLite( &RootFcb->NPFcb->Specific.Directory.DirectoryTreeLock);

        //
        // Sync notif object
        //

        FsRtlNotifyUninitializeSync( &RootFcb->NPFcb->NotifySync);

        //
        // The non paged region
        //

        ExFreePool( RootFcb->NPFcb);
    }

    if( RootFcb->DirEntry != NULL)
    {

        ExFreePool( RootFcb->DirEntry);
    }

    //
    // And the Fcb itself
    //

    ExFreePool( RootFcb);

    return;
}

//
// Function: AFSRemoveFcb
//
// Description:
//
//      This function performs Fcb removal/deallocation
//
// Return:
//
//      A status is returned for the function
//

void
AFSRemoveFcb( IN AFSFcb *Fcb)
{

    //
    // Uninitialize the file lock if it is a file
    //

    if( Fcb->Header.NodeTypeCode == AFS_FILE_FCB)
    {

        FsRtlUninitializeFileLock( &Fcb->Specific.File.FileLock);
    }
    else if( Fcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
    {

        ExDeleteResourceLite( &Fcb->NPFcb->Specific.Directory.DirectoryTreeLock);
    }

    //
    // Tear down the FM specific contexts
    //
    
    FsRtlTeardownPerStreamContexts( &Fcb->Header);

    //
    // Delete the resources
    //

    ExDeleteResourceLite( &Fcb->NPFcb->Resource);

    ExDeleteResourceLite( &Fcb->NPFcb->PagingResource);

    //
    // The non paged region
    //

    ExFreePool( Fcb->NPFcb);

    //
    // And the Fcb itself, which includes the name
    //

    ExFreePool( Fcb);

    return;
}

NTSTATUS
AFSInitCcb( IN AFSFcb     *Fcb,
            IN OUT AFSCcb **Ccb)
{

    NTSTATUS Status = STATUS_SUCCESS;
    AFSCcb *pCcb = NULL;

    __Enter
    {

        //
        // Allocate our context control block
        //

        pCcb = (AFSCcb *)ExAllocatePoolWithTag( PagedPool,
                                                sizeof( AFSCcb),
                                                AFS_CCB_ALLOCATION_TAG);

        if( pCcb == NULL)
        {

            AFSPrint("AFSInitCcb Failed to allocate Ccb\n");

            try_return( Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pCcb,
                       sizeof( AFSCcb));

        pCcb->Size = sizeof( AFSCcb);
        pCcb->Type = AFS_CCB;

        //
        // Return the Ccb
        //

        *Ccb = pCcb;

try_exit:

        if( !NT_SUCCESS( Status))
        {

            AFSPrint("AFSInitCcb Failed to initialize Ccb Status %08lX\n", Status);

            if( pCcb != NULL)
            {

                ExFreePool( pCcb);
            }

            *Ccb = NULL;
        }
    }

    return Status;
}

//
// Function: AFSRemoveCcb
//
// Description:
//
//      This function performs Ccb removal/deallocation
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSRemoveCcb( IN AFSFcb *Fcb,
              IN AFSCcb *Ccb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if( Ccb->MaskName.Buffer != NULL)
    {

        ExFreePool( Ccb->MaskName.Buffer);
    }

    //
    // Free up the Ccb
    //

    ExFreePool( Ccb);

    return ntStatus;
}

NTSTATUS
AFSInitializeVolume( IN AFSDirEntryCB *VolumeDirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        //
        // Initializing the volume node we need to create the root node
        // and populate the directory enum information
        //

        ntStatus = AFSInitRootFcb( VolumeDirEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // TODO: Get this information from the server
        //

        VolumeDirEntry->Type.Volume.VolumeInformation.TotalAllocationUnits.QuadPart = 0x100000;

        VolumeDirEntry->Type.Volume.VolumeInformation.AvailableAllocationUnits.QuadPart = 0x100000;

        VolumeDirEntry->Type.Volume.VolumeInformation.VolumeCreationTime = VolumeDirEntry->DirectoryEntry.CreationTime;

        VolumeDirEntry->Type.Volume.VolumeInformation.Characteristics = FILE_REMOTE_DEVICE;
                                    
        VolumeDirEntry->Type.Volume.VolumeInformation.SectorsPerAllocationUnit = 1;

        VolumeDirEntry->Type.Volume.VolumeInformation.BytesPerSector = 0x400;

        VolumeDirEntry->Type.Volume.VolumeInformation.VolumeLabelLength = 12;

        RtlCopyMemory( VolumeDirEntry->Type.Volume.VolumeInformation.VolumeLabel,
                       L"AFSVol",
                       12);

try_exit:

        NOTHING;
    }

    return ntStatus;
}
