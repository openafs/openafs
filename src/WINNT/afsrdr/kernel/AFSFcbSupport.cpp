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
    ULONGLONG   ullIndex = 0;

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

        AFSAllocationMemoryLevel += usFcbLength;

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
        // The notification information
        //

        InitializeListHead( &pNPFcb->DirNotifyList);
        FsRtlNotifyInitializeSync( &pNPFcb->NotifySync);

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

        ASSERT( ParentFcb != NULL);

        pFcb->ParentFcb = ParentFcb;

        pFcb->RootFcb = ParentFcb->RootFcb;

        pFcb->DirEntry = DirEntry;

        if( DirEntry != NULL)
        {

            DirEntry->Fcb = pFcb;

            //
            // Set the index for the file id tree entry
            //

            pFcb->TreeEntry.HashIndex = AFSCreateLowIndex( &DirEntry->DirectoryEntry.FileId);

            //
            // Set type specific information
            //

            if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY)
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

                //
                // Initialize the directory
                //

                ntStatus = AFSInitializeDirectory( pFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }
            }
            else if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_FILE)
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
                // Initialize the Extents resources and so forth.  The
                // quiescent state is that no one has the extents for
                // IO (do the extents are not busy) and there is no
                // extents request outstanding (and hence the "last
                // one" is complete).
                //
                ExInitializeResourceLite( &pNPFcb->Specific.File.ExtentsResource );
                KeInitializeEvent( &pNPFcb->Specific.File.ExtentsNotBusy, 
                                   NotificationEvent, 
                                   TRUE );
                KeInitializeEvent( &pNPFcb->Specific.File.ExtentsRequestComplete, 
                                   NotificationEvent, 
                                   TRUE );

                for (ULONG i = 0; i < AFS_NUM_EXTENT_LISTS; i++) 
                {
                    InitializeListHead(&pFcb->Specific.File.ExtentsLists[i]);
                }
            }
            else if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
            {

                //
                // Reset the type to a mount point type
                //

                pFcb->Header.NodeTypeCode = AFS_MOUNT_POINT_FCB;

                //
                // Locate the target of this mount point
                //

                ullIndex = AFSCreateHighIndex( &DirEntry->DirectoryEntry.TargetFileId);

                ASSERT( ullIndex != 0);

                AFSAcquireShared( pDeviceExt->Specific.RDR.VolumeTree.TreeLock,
                                  TRUE);
            
                AFSLocateHashEntry( pDeviceExt->Specific.RDR.VolumeTree.TreeHead,
                                    ullIndex,
                                    &pFcb->Specific.MountPoint.TargetFcb);

                AFSReleaseResource( pDeviceExt->Specific.RDR.VolumeTree.TreeLock);

                ASSERT( pFcb->Specific.MountPoint.TargetFcb != NULL);

            }
            else if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK)
            {

                //
                // Reset the type to a symblic link type
                //

                pFcb->Header.NodeTypeCode = AFS_SYMBOLIC_LINK_FCB;

            }
            else
            {

                ASSERT( FALSE);
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

            AFSAcquireExcl( pFcb->RootFcb->Specific.VolumeRoot.FcbListLock,
                            TRUE);

            if( pFcb->RootFcb->Specific.VolumeRoot.FcbListHead == NULL)
            {

                pFcb->RootFcb->Specific.VolumeRoot.FcbListHead = pFcb;
            }
            else
            {

                pFcb->RootFcb->Specific.VolumeRoot.FcbListTail->ListEntry.fLink = pFcb;

                pFcb->ListEntry.bLink = pFcb->RootFcb->Specific.VolumeRoot.FcbListTail;
            }

            pFcb->RootFcb->Specific.VolumeRoot.FcbListTail = pFcb;

            AFSReleaseResource( pFcb->RootFcb->Specific.VolumeRoot.FcbListLock);

            //
            // And FileID tree
            //

            AFSAcquireExcl( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                            TRUE);

            if( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeHead == NULL)
            {
                
                pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeHead = &pFcb->TreeEntry;
            }
            else
            {

                ntStatus = AFSInsertHashEntry( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                               &pFcb->TreeEntry);

                ASSERT( NT_SUCCESS( ntStatus));
            }

            //
            // Tag the entry as being in the FileID tree
            //

            SetFlag( pFcb->Flags, AFS_FCB_INSERTED_ID_TREE);

            AFSReleaseResource( pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock);
        }

        if( Fcb != NULL)
        {

            //
            // And return the Fcb
            //

            *Fcb = pFcb;
        }

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

                AFSAllocationMemoryLevel -= sizeof( AFSFcb);
            }

            if( pNPFcb != NULL)
            {

                ExFreePool( pNPFcb);
            }

            if( Fcb != NULL)
            {

                *Fcb = NULL;
            }
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

    __Enter
    {

        if( AFSAllRoot == NULL)
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

            AFSAllocationMemoryLevel += sizeof( AFSFcb);

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

            ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.DirectoryTreeLock);

            ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.FileIDTreeLock);

            ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.FcbListLock);

            pFcb->Header.Resource = &pNPFcb->Resource;

            pFcb->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock = &pNPFcb->Specific.VolumeRoot.DirectoryTreeLock;

            pFcb->Specific.VolumeRoot.FileIDTree.TreeLock = &pNPFcb->Specific.VolumeRoot.FileIDTreeLock;

            pFcb->Specific.VolumeRoot.FcbListLock = &pNPFcb->Specific.VolumeRoot.FcbListLock;

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

            AFSAllocationMemoryLevel += sizeof( AFSDirEntryCB) + sizeof( WCHAR);

            RtlZeroMemory( pFcb->DirEntry,
                           sizeof( AFSDirEntryCB) + sizeof( WCHAR));

            pFcb->DirEntry->NPDirNode = (AFSNonPagedDirNode *)ExAllocatePoolWithTag( NonPagedPool,  
                                                                                     sizeof( AFSNonPagedDirNode),
                                                                                     AFS_DIR_ENTRY_NP_TAG);

            if( pFcb->DirEntry->NPDirNode == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            //
            // Initialize the non-paged portion of the dire entry
            //

            AFSInitNonPagedDirEntry( pFcb->DirEntry);

            KeQuerySystemTime( &pFcb->DirEntry->DirectoryEntry.CreationTime);
            KeQuerySystemTime( &pFcb->DirEntry->DirectoryEntry.LastWriteTime);
            KeQuerySystemTime( &pFcb->DirEntry->DirectoryEntry.LastAccessTime);

            pFcb->DirEntry->DirectoryEntry.FileType = AFS_FILE_TYPE_DIRECTORY;

            pFcb->DirEntry->DirectoryEntry.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

            pFcb->DirEntry->DirectoryEntry.FileName.Length = sizeof( WCHAR);
        
            pFcb->DirEntry->DirectoryEntry.FileName.Buffer = (WCHAR *)((char *)pFcb->DirEntry + sizeof( AFSDirEntryCB));

            RtlCopyMemory( pFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                           L"\\",
                           sizeof( WCHAR));

            //
            // TODO: Get this information from the server
            //

            pFcb->DirEntry->Type.Volume.VolumeInformation.TotalAllocationUnits.QuadPart = 0x100000;

            pFcb->DirEntry->Type.Volume.VolumeInformation.AvailableAllocationUnits.QuadPart = 0x100000;

            pFcb->DirEntry->Type.Volume.VolumeInformation.VolumeCreationTime = pFcb->DirEntry->DirectoryEntry.CreationTime;

            pFcb->DirEntry->Type.Volume.VolumeInformation.Characteristics = FILE_REMOTE_DEVICE;
                                        
            pFcb->DirEntry->Type.Volume.VolumeInformation.SectorsPerAllocationUnit = 1;

            pFcb->DirEntry->Type.Volume.VolumeInformation.BytesPerSector = 0x400;

            pFcb->DirEntry->Type.Volume.VolumeInformation.VolumeLabelLength = 12;

            RtlCopyMemory( pFcb->DirEntry->Type.Volume.VolumeInformation.VolumeLabel,
                           L"AFSVol",
                           12);

            AFSAllRoot = pFcb;
        }

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pFcb != NULL)
            {

                AFSRemoveRootFcb( pFcb,
                                  FALSE);
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSRemoveAFSRoot()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSDirEntryCB *pCurrentDirEntry = NULL;
    AFSFcb *pFcb = NULL, *pNextFcb = NULL, *pVcb = NULL, *pNextVcb = NULL;

    __Enter
    {

        if( AFSAllRoot == NULL)
        {

            //
            // Already initialized
            //

            try_return( ntStatus);
        }

        //
        // Reset the Redirector volume tree
        //

        AFSAcquireExcl( &pDevExt->Specific.RDR.VolumeTreeLock, 
                        TRUE);

        pDevExt->Specific.RDR.VolumeTree.TreeHead = NULL;

        AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

        AFSAcquireExcl( &AFSAllRoot->NPFcb->Resource,
                        TRUE);

        AFSAcquireExcl( AFSAllRoot->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock,
                        TRUE);

        AFSAllRoot->Specific.VolumeRoot.DirectoryNodeHdr.CaseSensitiveTreeHead = NULL;

        AFSAllRoot->Specific.VolumeRoot.DirectoryNodeHdr.CaseInsensitiveTreeHead = NULL;

        //
        // Reset the btree and directory list information
        //

        pCurrentDirEntry = AFSAllRoot->Specific.VolumeRoot.DirectoryNodeListHead;

        while( pCurrentDirEntry != NULL)
        {

            pFcb = pCurrentDirEntry->Fcb;

            if( pFcb != NULL)
            {

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                if( pFcb->DirEntry != NULL)
                {

                    pFcb->DirEntry->Fcb = NULL;

                    SetFlag( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE);
                }

                SetFlag( pFcb->Flags, AFS_FCB_INVALID);

                ClearFlag( pFcb->Flags, AFS_FCB_INSERTED_ID_TREE);
                    
                AFSReleaseResource( &pFcb->NPFcb->Resource);

                AFSRemoveDirNodeFromParent( AFSAllRoot,
                                            pCurrentDirEntry);
            }
            else
            {

                AFSDeleteDirEntry( AFSAllRoot,
                                   pCurrentDirEntry);
            }

            pCurrentDirEntry = AFSAllRoot->Specific.Directory.DirectoryNodeListHead;
        }

        AFSAllRoot->Specific.VolumeRoot.DirectoryNodeListHead = NULL;

        AFSAllRoot->Specific.VolumeRoot.DirectoryNodeListTail = NULL;

        //
        // Check if we still have any Fcbs
        //

        AFSAcquireExcl( AFSAllRoot->Specific.VolumeRoot.FcbListLock,
                        TRUE);

        if( AFSAllRoot->Specific.VolumeRoot.FcbListHead != NULL)
        {

            pFcb = AFSAllRoot->Specific.VolumeRoot.FcbListHead;

            while( pFcb != NULL)
            {

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                if( pFcb->OpenReferenceCount == 0)
                {

                    if( pFcb->DirEntry != NULL)
                    {

                        pFcb->DirEntry->Fcb = NULL;

                        ASSERT( BooleanFlagOn( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE));

                        if( BooleanFlagOn( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_NAME_BUFFER))
                        {

                           ExFreePool( pFcb->DirEntry->DirectoryEntry.FileName.Buffer);
                        }

                        //
                        // Free up the dir entry
                        //

                        AFSAllocationMemoryLevel -= sizeof( AFSDirEntryCB) + 
                                                                  pFcb->DirEntry->DirectoryEntry.FileName.Length +
                                                                  pFcb->DirEntry->DirectoryEntry.TargetName.Length;
     
                        ExFreePool( pFcb->DirEntry);

                        pFcb->DirEntry = NULL;
                    }

                    pNextFcb = (AFSFcb *)pFcb->ListEntry.fLink;                                    

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    AFSRemoveFcb( pFcb);
                }
                else
                {

                    SetFlag( pFcb->Flags, AFS_FCB_DELETE_FCB_ON_CLOSE);

                    pNextFcb = (AFSFcb *)pFcb->ListEntry.fLink;

                    AFSReleaseResource( &pFcb->NPFcb->Resource);
                }

                pFcb = pNextFcb;
            }

            AFSAllRoot->Specific.VolumeRoot.FcbListHead = NULL;

            AFSAllRoot->Specific.VolumeRoot.FcbListTail = NULL;

            AFSAllRoot->Specific.VolumeRoot.FileIDTree.TreeHead = NULL;
        }

        AFSReleaseResource( AFSAllRoot->Specific.VolumeRoot.FcbListLock);

        //
        // Indicate the node is NOT initialized
        //

        ClearFlag( AFSAllRoot->Flags, AFS_FCB_DIRECTORY_ENUMERATED);

        AFSReleaseResource( &AFSAllRoot->NPFcb->Resource);

        AFSReleaseResource( AFSAllRoot->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock);

        //
        // Tag the Fcbs as invalid so they are torn down
        //

        AFSAcquireExcl( &pDevExt->Specific.RDR.VolumeListLock,
                        TRUE);

        pVcb = pDevExt->Specific.RDR.VolumeListHead;

        while( pVcb != NULL)
        {

            AFSAcquireExcl( pVcb->Specific.VolumeRoot.FcbListLock,
                            TRUE);

            SetFlag( pVcb->Flags, AFS_FCB_VOLUME_OFFLINE);

            //
            // If this volume has not been enumerated nor does it have
            // a worker then we need to delete it here
            //

            if( pVcb->Specific.VolumeRoot.VolumeWorkerContext.WorkerThreadObject == NULL)
            {

                ASSERT( pVcb->Specific.VolumeRoot.FcbListHead == NULL);

                pNextVcb = (AFSFcb *)pVcb->ListEntry.fLink;

                AFSReleaseResource( pVcb->Specific.VolumeRoot.FcbListLock);

                AFSRemoveRootFcb( pVcb,
                                  FALSE);
            }
            else
            {

                pFcb = pVcb->Specific.VolumeRoot.FcbListHead;

                while( pFcb != NULL)
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


                        //
                        // Now flush any dirty extents
                        //
                        (VOID) AFSFlushExtents( pFcb);
                                
                        //
                        // And get rid of them (not this involves waiting
                        // for any writes or reads to the cache to complete
                        //
                        (VOID) AFSTearDownFcbExtents( pFcb);

                    }

                    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                    TRUE);

                    SetFlag( pFcb->Flags, AFS_FCB_INVALID);

                    pNextFcb = (AFSFcb *)pFcb->ListEntry.fLink;
                    
                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    pFcb = pNextFcb;
                }

                pNextVcb = (AFSFcb *)pVcb->ListEntry.fLink;

                AFSReleaseResource( pVcb->Specific.VolumeRoot.FcbListLock);
            }

            pVcb = pNextVcb;
        }

        pDevExt->Specific.RDR.VolumeListHead = NULL;

        pDevExt->Specific.RDR.VolumeListTail = NULL;

        AFSReleaseResource( &pDevExt->Specific.RDR.VolumeListLock);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {
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
AFSInitRootFcb( IN AFSDirEntryCB *MountPointDirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pFcb = NULL;
    AFSNonPagedFcb *pNPFcb = NULL;
    IO_STATUS_BLOCK stIoStatus = {0,0};
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

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

        AFSAllocationMemoryLevel += sizeof( AFSFcb);

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

        ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.DirectoryTreeLock);

        ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.FileIDTreeLock);

        ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.FcbListLock);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock = &pNPFcb->Specific.VolumeRoot.DirectoryTreeLock;

        pFcb->Specific.VolumeRoot.FileIDTree.TreeLock = &pNPFcb->Specific.VolumeRoot.FileIDTreeLock;

        pFcb->Specific.VolumeRoot.FcbListLock = &pNPFcb->Specific.VolumeRoot.FcbListLock;

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

        AFSAllocationMemoryLevel += sizeof( AFSDirEntryCB) + sizeof( WCHAR);

        RtlZeroMemory( pFcb->DirEntry,
                       sizeof( AFSDirEntryCB) + sizeof( WCHAR));

        pFcb->DirEntry->NPDirNode = (AFSNonPagedDirNode *)ExAllocatePoolWithTag( NonPagedPool,  
                                                                                 sizeof( AFSNonPagedDirNode),
                                                                                 AFS_DIR_ENTRY_NP_TAG);

        if( pFcb->DirEntry->NPDirNode == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Initialize the non-paged portion of the dire entry
        //

        AFSInitNonPagedDirEntry( pFcb->DirEntry);

        //
        // Populate the dir entry for this root
        //

        pFcb->DirEntry->DirectoryEntry.FileId = MountPointDirEntry->DirectoryEntry.TargetFileId;

        pFcb->DirEntry->DirectoryEntry.CreationTime.QuadPart = MountPointDirEntry->DirectoryEntry.CreationTime.QuadPart;
        pFcb->DirEntry->DirectoryEntry.LastWriteTime.QuadPart = MountPointDirEntry->DirectoryEntry.CreationTime.QuadPart;
        pFcb->DirEntry->DirectoryEntry.LastAccessTime.QuadPart = MountPointDirEntry->DirectoryEntry.CreationTime.QuadPart;

        pFcb->DirEntry->DirectoryEntry.FileType = AFS_FILE_TYPE_DIRECTORY;

        pFcb->DirEntry->DirectoryEntry.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

        pFcb->DirEntry->DirectoryEntry.FileName.Length = sizeof( WCHAR);
    
        pFcb->DirEntry->DirectoryEntry.FileName.Buffer = (WCHAR *)((char *)pFcb->DirEntry + sizeof( AFSDirEntryCB));

        RtlCopyMemory( pFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                       L"\\",
                       sizeof( WCHAR));

        //
        // TODO: Get this information from the server
        //

        pFcb->DirEntry->Type.Volume.VolumeInformation.TotalAllocationUnits.QuadPart = 0x100000;

        pFcb->DirEntry->Type.Volume.VolumeInformation.AvailableAllocationUnits.QuadPart = 0x100000;

        pFcb->DirEntry->Type.Volume.VolumeInformation.VolumeCreationTime = MountPointDirEntry->DirectoryEntry.CreationTime;

        pFcb->DirEntry->Type.Volume.VolumeInformation.Characteristics = FILE_REMOTE_DEVICE;
                                    
        pFcb->DirEntry->Type.Volume.VolumeInformation.SectorsPerAllocationUnit = 1;

        pFcb->DirEntry->Type.Volume.VolumeInformation.BytesPerSector = 0x400;

        pFcb->DirEntry->Type.Volume.VolumeInformation.VolumeLabelLength = 12;

        RtlCopyMemory( pFcb->DirEntry->Type.Volume.VolumeInformation.VolumeLabel,
                       L"AFSVol",
                       12);

        //
        // If the fid, the target of the MP, is zero then we need to ask the service to evaluate it for us
        //

        if( pFcb->DirEntry->DirectoryEntry.FileId.Hash == 0)
        {

            AFSDirEnumEntry *pDirEnumCB = NULL;

            ntStatus = AFSEvaluateTargetByID( &MountPointDirEntry->DirectoryEntry.FileId,
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

                MountPointDirEntry->DirectoryEntry.TargetFileId = pDirEnumCB->TargetFileId;
                
                pFcb->DirEntry->DirectoryEntry.FileId = pDirEnumCB->TargetFileId;
            }

            ExFreePool( pDirEnumCB);
        }

        //
        // Insert the root of the volume into our volume list of entries
        //

        pFcb->TreeEntry.HashIndex = AFSCreateHighIndex( &pFcb->DirEntry->DirectoryEntry.FileId);

        AFSAcquireExcl( pDeviceExt->Specific.RDR.VolumeTree.TreeLock,
                        TRUE);

        if( pDeviceExt->Specific.RDR.VolumeTree.TreeHead == NULL)
        {
                
            pDeviceExt->Specific.RDR.VolumeTree.TreeHead = &pFcb->TreeEntry;
        }
        else
        {

            ntStatus = AFSInsertHashEntry( pDeviceExt->Specific.RDR.VolumeTree.TreeHead,
                                           &pFcb->TreeEntry);

            ASSERT( NT_SUCCESS( ntStatus));
        }

        AFSReleaseResource( pDeviceExt->Specific.RDR.VolumeTree.TreeLock);

        AFSAcquireExcl( &pDeviceExt->Specific.RDR.VolumeListLock,
                        TRUE);

        if( pDeviceExt->Specific.RDR.VolumeListHead == NULL)
        {
                
            pDeviceExt->Specific.RDR.VolumeListHead = pFcb;
        }
        else
        {

            pDeviceExt->Specific.RDR.VolumeListTail->ListEntry.fLink = (void *)pFcb;

            pFcb->ListEntry.bLink = pDeviceExt->Specific.RDR.VolumeListTail;
        }

        pDeviceExt->Specific.RDR.VolumeListTail = pFcb;

        AFSReleaseResource( &pDeviceExt->Specific.RDR.VolumeListLock);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pFcb != NULL)
            {

                AFSRemoveRootFcb( pFcb,
                                  TRUE);
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
AFSRemoveRootFcb( IN AFSFcb *RootFcb,
                  IN BOOLEAN CloseWorkerThread)
{

    AFSDirEntryCB *pCurrentDirEntry = NULL;

    if( RootFcb->NPFcb != NULL)
    {

        if( CloseWorkerThread)
        {

            //
            // Close the volume worker
            //

            AFSShutdownVolumeWorker( RootFcb);
        }

        if( RootFcb->Specific.VolumeRoot.DirectoryNodeListHead != NULL)
        {

            pCurrentDirEntry = RootFcb->Specific.VolumeRoot.DirectoryNodeListHead;

            while( pCurrentDirEntry != NULL)
            {

                AFSDeleteDirEntry( RootFcb,
                                   pCurrentDirEntry);

                pCurrentDirEntry = RootFcb->Specific.VolumeRoot.DirectoryNodeListHead;
            }
        }

        //
        // Now the resource
        //

        ExDeleteResourceLite( &RootFcb->NPFcb->Resource);

        ExDeleteResourceLite( &RootFcb->NPFcb->Specific.VolumeRoot.DirectoryTreeLock);

        ExDeleteResourceLite( &RootFcb->NPFcb->Specific.VolumeRoot.FileIDTreeLock);

        ExDeleteResourceLite( &RootFcb->NPFcb->Specific.VolumeRoot.FcbListLock);

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

        if( RootFcb->DirEntry->NPDirNode != NULL)
        {

            ExDeleteResourceLite( &RootFcb->DirEntry->NPDirNode->Lock);

            ExFreePool( RootFcb->DirEntry->NPDirNode);
        }

        ExFreePool( RootFcb->DirEntry);

        AFSAllocationMemoryLevel -= sizeof( AFSDirEntryCB) + sizeof( WCHAR);
    }

    //
    // And the Fcb itself
    //

    ExFreePool( RootFcb);

    AFSAllocationMemoryLevel -= sizeof( AFSFcb);

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

        //
        // The resource we allocated
        //

        ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.ExtentsResource );
    }
    else if( Fcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
    {

        ExDeleteResourceLite( &Fcb->NPFcb->Specific.Directory.DirectoryTreeLock);
    }

    //
    // Sync notif object
    //

    FsRtlNotifyUninitializeSync( &Fcb->NPFcb->NotifySync);

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

    ASSERT( !BooleanFlagOn( Fcb->Flags, AFS_FCB_INSERTED_ID_TREE));

    ExFreePool( Fcb);

    AFSAllocationMemoryLevel -= sizeof( AFSFcb);

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

    if( BooleanFlagOn( Ccb->Flags, CCB_FLAG_FREE_FULL_PATHNAME))
    {

        ExFreePool( Ccb->FullFileName.Buffer);
    }

    //
    // Free up the Ccb
    //

    ExFreePool( Ccb);

    return ntStatus;
}
