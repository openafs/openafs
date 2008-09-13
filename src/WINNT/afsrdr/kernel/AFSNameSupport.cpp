//
// File: AFSNameSupport.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSLocateNameEntry( IN AFSFcb *RootFcb,
                    IN PFILE_OBJECT FileObject,
                    IN UNICODE_STRING *FullPathName,
                    IN OUT AFSFcb **ParentFcb,
                    OUT AFSFcb **Fcb,
                    OUT PUNICODE_STRING ComponentName)
{

    NTSTATUS          ntStatus = STATUS_SUCCESS;
    AFSFcb           *pParentFcb = NULL, *pCurrentFcb = NULL;
    UNICODE_STRING    uniPathName, uniComponentName, uniRemainingPath;
    ULONG             ulCRC = 0;
    AFSDirEntryCB    *pDirEntry = NULL;
    UNICODE_STRING    uniCurrentPath;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    BOOLEAN           bReleaseRoot = FALSE;
    ULONGLONG         ullIndex = 0;

    __Enter
    {

        //
        // Cleanup some parameters
        //

        if( ComponentName != NULL)
        {

            ComponentName->Length = 0;
            ComponentName->MaximumLength = 0;
            ComponentName->Buffer = NULL;
        }

        //
        // We will parse through the filename, locating the directory nodes until we encoutner a cache miss
        // Starting at the root node
        //

        pParentFcb = *ParentFcb;

        //
        // If the root and the parent are not the same then this is a relative
        // open so we need to drop the root lock and acquire the parent lock
        //

        if( RootFcb != *ParentFcb)
        {

            bReleaseRoot = TRUE;

            AFSAcquireExcl( &pParentFcb->NPFcb->Resource,
                            TRUE);
        }

        uniPathName = *FullPathName;

        uniCurrentPath = *FullPathName;

        //
        // Adjust the length of the current path to be the length of the parent name
        // that we are currently working with
        //

        uniCurrentPath.Length = pParentFcb->DirEntry->DirectoryEntry.FileName.Length;

        while( TRUE)
        {

            FsRtlDissectName( uniPathName,
                              &uniComponentName,
                              &uniRemainingPath);

            uniPathName = uniRemainingPath;
            
            *ComponentName = uniComponentName;

            //
            // Ensure the parent node has been evaluated, if not then go do it now
            //

            if( BooleanFlagOn( pParentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED))
            {

                ntStatus = AFSEvaluateNode( pParentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    try_return( ntStatus);
                }

                ClearFlag( pParentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
            }

            //
            // If this is a mount point or sym link then go get the real directory node
            //

            if( pParentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK)
            {

                ASSERT( pParentFcb->Specific.Directory.DirectoryNodeListHead == NULL);

                //
                // Check if we have a target Fcb for this node
                //

                if( pParentFcb->Specific.SymbolicLink.TargetFcb == NULL)
                {

                    //
                    // Go retrieve the target entry for this node
                    //

                    ntStatus = AFSBuildTargetDirectory( pParentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        break;
                    }
                }

                //
                // Swap out where we are in the chain
                //

                AFSAcquireExcl( &pParentFcb->Specific.SymbolicLink.TargetFcb->NPFcb->Resource,
                                TRUE);

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                pParentFcb = pParentFcb->Specific.SymbolicLink.TargetFcb;
            }
            else if( pParentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
            {

                ASSERT( pParentFcb->Specific.Directory.DirectoryNodeListHead == NULL);

                //
                // Check if we have a target Fcb for this node
                //

                if( pParentFcb->Specific.MountPoint.TargetFcb == NULL)
                {

                    AFSAcquireShared( pDevExt->Specific.RDR.VolumeTree.TreeLock,
                                      TRUE);

                    ullIndex = AFSCreateHighIndex( &pParentFcb->DirEntry->DirectoryEntry.TargetFileId);

                    AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                        ullIndex,
                                        &pParentFcb->Specific.MountPoint.TargetFcb);

                    AFSReleaseResource( pDevExt->Specific.RDR.VolumeTree.TreeLock);
                }

                //
                // Swap out where we are in the chain
                //

                AFSAcquireExcl( &pParentFcb->Specific.MountPoint.TargetFcb->NPFcb->Resource,
                                TRUE);

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                pParentFcb = pParentFcb->Specific.MountPoint.TargetFcb;
            }

            //
            // If the parent is not initialized then do it now
            //

            if( pParentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB &&
                !BooleanFlagOn( pParentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
            {

                ntStatus = AFSEnumerateDirectory( &pParentFcb->DirEntry->DirectoryEntry.FileId,
                                                  &pParentFcb->Specific.Directory.DirectoryNodeHdr,
                                                  &pParentFcb->Specific.Directory.DirectoryNodeListHead,
                                                  &pParentFcb->Specific.Directory.DirectoryNodeListTail,
                                                  &pParentFcb->Specific.Directory.ShortNameTree,
                                                  NULL);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    break;
                }

                SetFlag( pParentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED);
            }
            else if( pParentFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
            {
                
                if( !BooleanFlagOn( pParentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                {

                    ntStatus = AFSEnumerateDirectory( &pParentFcb->DirEntry->DirectoryEntry.FileId,
                                                      &pParentFcb->Specific.VolumeRoot.DirectoryNodeHdr,
                                                      &pParentFcb->Specific.VolumeRoot.DirectoryNodeListHead,
                                                      &pParentFcb->Specific.VolumeRoot.DirectoryNodeListTail,
                                                      &pParentFcb->Specific.VolumeRoot.ShortNameTree,
                                                      NULL);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        break;
                    }

                    SetFlag( pParentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED);
                }

                //
                // Initialize the volume worker if it has not already
                //

                if( pParentFcb->Specific.VolumeRoot.VolumeWorkerContext.WorkerThreadObject == NULL &&
                    !BooleanFlagOn( pParentFcb->Specific.VolumeRoot.VolumeWorkerContext.State, AFS_WORKER_INITIALIZED))
                {

                    AFSInitVolumeWorker( pParentFcb);
                }
            }
            else if( pParentFcb->Header.NodeTypeCode == AFS_FILE_FCB)
            {

                ntStatus = STATUS_OBJECT_NAME_INVALID;

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                break;
            }

            //
            // Generate the CRC on the node and perform a case sensitive lookup
            //

            ulCRC = AFSGenerateCRC( &uniComponentName,
                                    FALSE);

            if( pParentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
            {

                AFSLocateCaseSensitiveDirEntry( pParentFcb->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                ulCRC,
                                                &pDirEntry);
            }
            else
            {

                AFSLocateCaseSensitiveDirEntry( pParentFcb->Specific.VolumeRoot.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                ulCRC,
                                                &pDirEntry);
            }

            if( pDirEntry == NULL)
            {

                //
                // Missed so perform a case insensitive lookup
                //

                ulCRC = AFSGenerateCRC( &uniComponentName,
                                        TRUE);

                if( pParentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
                {

                    AFSLocateCaseInsensitiveDirEntry( pParentFcb->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                                      ulCRC,
                                                      &pDirEntry);
                }
                else
                {

                    AFSLocateCaseInsensitiveDirEntry( pParentFcb->Specific.VolumeRoot.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                                      ulCRC,
                                                      &pDirEntry);
                }

                if( pDirEntry == NULL)
                {

                    //
                    // OK, if this component is a valid short name then try
                    // a lookup in the short name tree
                    //

                    //if( RtlIsNameLegalDOS8Dot3( &uniComponentName,
                    //                            NULL,
                    //                            NULL))
                    if( uniComponentName.Length <= 24)
                    {

                        if( pParentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
                        {

                            AFSLocateShortNameDirEntry( pParentFcb->Specific.Directory.ShortNameTree,
                                                        ulCRC,
                                                        &pDirEntry);
                        }
                        else
                        {

                            AFSLocateShortNameDirEntry( pParentFcb->Specific.VolumeRoot.ShortNameTree,
                                                        ulCRC,
                                                        &pDirEntry);
                        }
                    }

                    if( pDirEntry == NULL)
                    {

                        if( uniRemainingPath.Length > 0)
                        {

                            ntStatus = STATUS_OBJECT_PATH_NOT_FOUND;

                            //
                            // Drop the lock on the parent
                            //

                            AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                        }
                        else
                        {

                            ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;

                            //
                            // Pass back the parent node and the component name
                            //

                            *ParentFcb = pParentFcb;

                            *Fcb = NULL;
                        }

                        //
                        // Node name not found so get out
                        //

                        break;
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

                        ntStatus = STATUS_OBJECT_NAME_COLLISION;

                        //
                        // Drop the lock on the parent
                        //

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        break;
                    }
                }
            }

            //
            // Add in the component name length to the full current path
            //

            uniCurrentPath.Length += uniComponentName.Length;

            if( pParentFcb != RootFcb)
            {

                uniCurrentPath.Length += sizeof( WCHAR);
            }

            //
            // Locate/create the Fcb for the current portion.
            //

            if( pDirEntry->Fcb == NULL)
            {

                pCurrentFcb = NULL;

                //
                // See if we can first locate the Fcb since this could be a stand alone node
                //

                AFSAcquireShared( pParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock, 
                                  TRUE);

                ullIndex = AFSCreateLowIndex( &pDirEntry->DirectoryEntry.FileId);

                AFSLocateHashEntry( pParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                    ullIndex,
                                    &pCurrentFcb);

                AFSReleaseResource( pParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock);

                if( pCurrentFcb != NULL)
                {

                    AFSAcquireExcl( &pCurrentFcb->NPFcb->Resource,
                                    TRUE);

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

                        AFSAllocationMemoryLevel -= sizeof( AFSDirEntryCB) + 
                                                        pCurrentFcb->DirEntry->DirectoryEntry.FileName.Length +
                                                        pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Length;

                        ExFreePool( pCurrentFcb->DirEntry);

                        pCurrentFcb->DirEntry = pDirEntry;
                    }

                    if( BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_STANDALONE_NODE))
                    {

                        pCurrentFcb->ParentFcb = pParentFcb;

                        pCurrentFcb->RootFcb = pParentFcb->RootFcb;

                        ClearFlag( pCurrentFcb->Flags, AFS_FCB_STANDALONE_NODE);
                    }

                    pDirEntry->Fcb = pCurrentFcb;
                }
                else
                {

                    ntStatus = AFSInitFcb( pParentFcb,
                                           pDirEntry,
                                           &pCurrentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        break;
                    }
                }
            }
            else
            {

                pCurrentFcb = pDirEntry->Fcb;
            
                AFSAcquireExcl( &pCurrentFcb->NPFcb->Resource,
                                TRUE);
            }

            if( uniRemainingPath.Length > 0 &&
                pCurrentFcb->Header.NodeTypeCode != AFS_FILE_FCB)
            {

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                pParentFcb = pCurrentFcb;
            }
            else
            {

                if( uniRemainingPath.Length > 0)
                {

                    AFSPrint("AFSLocateNameEntry Have a remaining component %wZ\n", &uniRemainingPath);
                }
                //
                // Ensure the node has been evaluated, if not then go do it now
                //

                if( BooleanFlagOn( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED))
                {

                    ntStatus = AFSEvaluateNode( pCurrentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                        try_return( ntStatus);
                    }

                    ClearFlag( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
                }

                //
                // If this is a mount point or sym link then go get the real directory node
                //

                if( pCurrentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK)
                {

                    ASSERT( pCurrentFcb->Specific.Directory.DirectoryNodeListHead == NULL);

                    //
                    // Check if we have a target Fcb for this node
                    //

                    if( pCurrentFcb->Specific.SymbolicLink.TargetFcb == NULL)
                    {

                        //
                        // Go retrieve the target entry for this node
                        //

                        ntStatus = AFSBuildTargetDirectory( pCurrentFcb);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                            break;
                        }
                    }

                    //
                    // Swap out where we are in the chain, including the parent
                    //

                    if( pCurrentFcb->Specific.SymbolicLink.TargetFcb->ParentFcb != NULL)
                    {

                        AFSAcquireExcl( &pCurrentFcb->Specific.SymbolicLink.TargetFcb->ParentFcb->NPFcb->Resource,
                                        TRUE);

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        pParentFcb = pCurrentFcb->Specific.SymbolicLink.TargetFcb->ParentFcb;
                    }

                    AFSAcquireExcl( &pCurrentFcb->Specific.SymbolicLink.TargetFcb->NPFcb->Resource,
                                    TRUE);

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    pCurrentFcb = pCurrentFcb->Specific.SymbolicLink.TargetFcb;
                }
                else if( pCurrentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
                {

                    ASSERT( pCurrentFcb->Specific.Directory.DirectoryNodeListHead == NULL);

                    //
                    // Check if we have a target Fcb for this node
                    //

                    if( pCurrentFcb->Specific.MountPoint.TargetFcb == NULL)
                    {

                        AFSAcquireShared( pDevExt->Specific.RDR.VolumeTree.TreeLock,
                                          TRUE);

                        ullIndex = AFSCreateHighIndex( &pCurrentFcb->DirEntry->DirectoryEntry.TargetFileId);

                        AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                            ullIndex,
                                            &pCurrentFcb->Specific.MountPoint.TargetFcb);

                        AFSReleaseResource( pDevExt->Specific.RDR.VolumeTree.TreeLock);
                    }

                    //
                    // Swap out where we are in the chain
                    //

                    AFSAcquireExcl( &pCurrentFcb->Specific.MountPoint.TargetFcb->NPFcb->Resource,
                                    TRUE);

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    //
                    // We pass back the root of the target volume
                    //

                    pParentFcb = pCurrentFcb->Specific.MountPoint.TargetFcb;

                    pCurrentFcb = pParentFcb;
                }

                //
                // If the entry is not initialized then do it now
                //

                if( pCurrentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB &&
                    !BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                {

                    ntStatus = AFSEnumerateDirectory( &pCurrentFcb->DirEntry->DirectoryEntry.FileId,
                                                      &pCurrentFcb->Specific.Directory.DirectoryNodeHdr,
                                                      &pCurrentFcb->Specific.Directory.DirectoryNodeListHead,
                                                      &pCurrentFcb->Specific.Directory.DirectoryNodeListTail,
                                                      &pCurrentFcb->Specific.Directory.ShortNameTree,
                                                      NULL);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                        break;
                    }

                    SetFlag( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED);
                }
                else if( pCurrentFcb->Header.NodeTypeCode == AFS_ROOT_FCB &&
                         !BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                {

                    ntStatus = AFSEnumerateDirectory( &pCurrentFcb->DirEntry->DirectoryEntry.FileId,
                                                      &pCurrentFcb->Specific.VolumeRoot.DirectoryNodeHdr,
                                                      &pCurrentFcb->Specific.VolumeRoot.DirectoryNodeListHead,
                                                      &pCurrentFcb->Specific.VolumeRoot.DirectoryNodeListTail,
                                                      &pCurrentFcb->Specific.VolumeRoot.ShortNameTree,
                                                      NULL);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                        break;
                    }

                    SetFlag( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED);

                    //
                    // Initialize the volume worker if it has not already
                    //

                    if( pParentFcb->Specific.VolumeRoot.VolumeWorkerContext.WorkerThreadObject == NULL &&
                        !BooleanFlagOn( pParentFcb->Specific.VolumeRoot.VolumeWorkerContext.State, AFS_WORKER_INITIALIZED))
                    {

                        AFSInitVolumeWorker( pParentFcb);
                    }
                }

                //
                // Pass back the remaining portion of the name
                //

                *ComponentName = uniRemainingPath;

                //
                // Pass back the Parent and Fcb
                //

                *ParentFcb = pParentFcb;

                if( pParentFcb != pCurrentFcb)
                {

                    *Fcb = pCurrentFcb;
                }

                break;
            }
        }

try_exit:

        if( bReleaseRoot)
        {

            AFSReleaseResource( &RootFcb->NPFcb->Resource);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSCreateDirEntry( IN AFSFcb *ParentDcb,
                   IN PUNICODE_STRING FileName,
                   IN PUNICODE_STRING ComponentName,
                   IN ULONG Attributes,
                   IN OUT AFSDirEntryCB **DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEntryCB *pDirNode = NULL;
    UNICODE_STRING uniShortName;
    LARGE_INTEGER liFileSize = {0,0};

    __Enter
    {

        //
        // OK, before inserting the node into the parent tree, issue
        // the request to the service for node creation
        // We will need to drop the lock on the parent node since the create
        // could cause a callback into the file system to invalidate it's cache
        //

        AFSReleaseResource( &ParentDcb->NPFcb->Resource);

        ntStatus = AFSNotifyFileCreate( ParentDcb,
                                        &liFileSize,
                                        Attributes,
                                        ComponentName,
                                        &pDirNode);

        //
        // Grab the lock again before returning
        //

        AFSAcquireExcl( &ParentDcb->NPFcb->Resource,
                        TRUE);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // If we still ahve a valid parent then insert it into the tree
        // otherwise we will re-evaluate the node on next access
        //

        if( BooleanFlagOn( ParentDcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
        {

            //
            // Insert the directory node
            //

            AFSInsertDirectoryNode( ParentDcb,
                                    pDirNode);
        }
        else
        {

            SetFlag( pDirNode->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE);
        }

        //
        // Pass back the dir entry
        //

        *DirEntry = pDirNode;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSGenerateShortName( IN AFSFcb *ParentDcb,
                      IN AFSDirEntryCB *DirNode)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    WCHAR wchShortNameBuffer[ 12];
    GENERATE_NAME_CONTEXT stNameContext;
    UNICODE_STRING uniShortName;
    AFSDirEntryCB *pDirEntry = NULL;
    ULONG ulCRC = 0;

    __Enter
    {

        uniShortName.Length = 0;
        uniShortName.MaximumLength = 24;
        uniShortName.Buffer = wchShortNameBuffer;

        RtlZeroMemory( &stNameContext,
                       sizeof( GENERATE_NAME_CONTEXT));

        //
        // Generate a shortname until we find one that is not being used in the current directory
        //

        while( TRUE)
        {

            RtlGenerate8dot3Name( &DirNode->DirectoryEntry.FileName,
                                  FALSE,
                                  &stNameContext,
                                  &uniShortName);

            //
            // Check if the short name exists in the current directory
            //

            ulCRC = AFSGenerateCRC( &uniShortName,
                                    TRUE);

            AFSLocateShortNameDirEntry( ParentDcb->Specific.Directory.ShortNameTree,
                                        ulCRC,
                                        &pDirEntry);

            if( pDirEntry == NULL)
            {

                //
                // OK, we have a good one.
                //

                DirNode->DirectoryEntry.ShortNameLength = (CCHAR)uniShortName.Length;

                RtlCopyMemory( DirNode->DirectoryEntry.ShortName,
                               uniShortName.Buffer,
                               uniShortName.Length);

                break;
            }
        }
    }

    return ntStatus;
}

void
AFSInsertDirectoryNode( IN AFSFcb *ParentDcb,
                        IN AFSDirEntryCB *DirEntry)
{

    __Enter
    {

        AFSAcquireExcl( ParentDcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        //
        // Insert the node into the directory node tree
        //

        if( ParentDcb->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead == NULL)
        {

            ParentDcb->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead = DirEntry;
        }
        else
        {

            AFSInsertCaseSensitiveDirEntry( ParentDcb->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                            DirEntry);
        }               

        if( ParentDcb->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead == NULL)
        {

            ParentDcb->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead = DirEntry;

            SetFlag( DirEntry->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);
        }
        else
        {

            AFSInsertCaseInsensitiveDirEntry( ParentDcb->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                              DirEntry);
        }               

        //
        // Into the shortname tree
        //

        if( DirEntry->Type.Data.ShortNameTreeEntry.HashIndex != 0)
        {

            if( ParentDcb->Specific.Directory.ShortNameTree == NULL)
            {

                ParentDcb->Specific.Directory.ShortNameTree = DirEntry;
            }
            else
            {

                AFSInsertShortNameDirEntry( ParentDcb->Specific.Directory.ShortNameTree,
                                            DirEntry);
            }              
        }

        //
        // And insert the node into the directory list
        //

        if( ParentDcb->Specific.Directory.DirectoryNodeListHead == NULL)
        {

            ParentDcb->Specific.Directory.DirectoryNodeListHead = DirEntry;
        }
        else
        {

            ParentDcb->Specific.Directory.DirectoryNodeListTail->ListEntry.fLink = (void *)DirEntry;

            DirEntry->ListEntry.bLink = (void *)ParentDcb->Specific.Directory.DirectoryNodeListTail;
        }

        ParentDcb->Specific.Directory.DirectoryNodeListTail = DirEntry;
        
        AFSReleaseResource( ParentDcb->Specific.Directory.DirectoryNodeHdr.TreeLock);
    }

    return;
}

NTSTATUS
AFSDeleteDirEntry( IN AFSFcb *ParentDcb,
                   IN AFSDirEntryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        AFSRemoveDirNodeFromParent( ParentDcb,
                                    DirEntry);

        //
        // Free up the name buffer if it was reallocated
        //

        if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_RELEASE_NAME_BUFFER))
        {

            ExFreePool( DirEntry->DirectoryEntry.FileName.Buffer);
        }

        if( DirEntry->NPDirNode != NULL)
        {

            ExDeleteResourceLite( &DirEntry->NPDirNode->Lock);

            ExFreePool( DirEntry->NPDirNode);
        }

        //
        // Free up the dir entry
        //

        AFSAllocationMemoryLevel -= sizeof( AFSDirEntryCB) + 
                                            DirEntry->DirectoryEntry.FileName.Length +
                                            DirEntry->DirectoryEntry.TargetName.Length;
        ExFreePool( DirEntry);
    }

    return ntStatus;
}

NTSTATUS
AFSRemoveDirNodeFromParent( IN AFSFcb *ParentDcb,
                            IN AFSDirEntryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        AFSAcquireExcl( ParentDcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        if( !BooleanFlagOn( DirEntry->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE))
        {

            //
            // Remove the entry from the parent tree
            //

            AFSRemoveCaseSensitiveDirEntry( &ParentDcb->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                            DirEntry);

            AFSRemoveCaseInsensitiveDirEntry( &ParentDcb->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                              DirEntry);

            if( DirEntry->Type.Data.ShortNameTreeEntry.HashIndex != 0)
            {

                //
                // From the short name tree
                //

                AFSRemoveShortNameDirEntry( &ParentDcb->Specific.Directory.ShortNameTree,
                                            DirEntry);
            }
        }

        //
        // And remove the entry from the enumeration lsit
        //

        if( DirEntry->ListEntry.fLink == NULL)
        {

            ParentDcb->Specific.Directory.DirectoryNodeListTail = (AFSDirEntryCB *)DirEntry->ListEntry.bLink;
        }
        else
        {

            ((AFSDirEntryCB *)DirEntry->ListEntry.fLink)->ListEntry.bLink = DirEntry->ListEntry.bLink;
        }

        if( DirEntry->ListEntry.bLink == NULL)
        {

            ParentDcb->Specific.Directory.DirectoryNodeListHead = (AFSDirEntryCB *)DirEntry->ListEntry.fLink;
        }
        else
        {

            ((AFSDirEntryCB *)DirEntry->ListEntry.bLink)->ListEntry.fLink = DirEntry->ListEntry.fLink;
        }

        DirEntry->ListEntry.fLink = NULL;
        DirEntry->ListEntry.bLink = NULL;

        AFSReleaseResource( ParentDcb->Specific.Directory.DirectoryNodeHdr.TreeLock);
    }

    return ntStatus;
}

NTSTATUS
AFSFixupTargetName( IN OUT PUNICODE_STRING FileName,
                    IN OUT PUNICODE_STRING TargetFileName)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniFileName;

    __Enter
    {

        //
        // We will process backwards from the end of the name looking
        // for the first \ we encounter
        //

        uniFileName.Length = FileName->Length;
        uniFileName.MaximumLength = FileName->MaximumLength;

        uniFileName.Buffer = FileName->Buffer;

        while( TRUE)
        {

            if( uniFileName.Buffer[ (uniFileName.Length/sizeof( WCHAR)) - 1] == L'\\')
            {

                //
                // Subtract one more character off of the filename if it is not the root
                //

                if( uniFileName.Length > sizeof( WCHAR))
                {

                    uniFileName.Length -= sizeof( WCHAR);
                }

                //
                // Now build up the target name
                //

                TargetFileName->Length = FileName->Length - uniFileName.Length;

                //
                // If we are not on the root then fixup the name
                //

                if( uniFileName.Length > sizeof( WCHAR))
                {

                    TargetFileName->Length -= sizeof( WCHAR);

                    TargetFileName->Buffer = &uniFileName.Buffer[ (uniFileName.Length/sizeof( WCHAR)) + 1];
                }
                else
                {

                    TargetFileName->Buffer = &uniFileName.Buffer[ uniFileName.Length/sizeof( WCHAR)];
                }

                //
                // Fixup the passed back filename length
                //

                FileName->Length = uniFileName.Length;

                TargetFileName->MaximumLength = TargetFileName->Length;

                break;
            }

            uniFileName.Length -= sizeof( WCHAR);
        }
    }

    return ntStatus;
}

NTSTATUS 
AFSParseName( IN PIRP Irp,
              OUT PUNICODE_STRING FileName,
              OUT PUNICODE_STRING FullFileName,
              OUT BOOLEAN *FreeNameString,
              OUT AFSFcb **RootFcb,
              OUT AFSFcb **ParentFcb)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION  pIrpSp = IoGetCurrentIrpStackLocation( Irp); 
    AFSDeviceExt       *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSFcb             *pParentFcb = NULL, *pCurrentFcb = NULL, *pRelatedFcb = NULL;
    UNICODE_STRING      uniFullName, uniComponentName, uniRemainingPath;
    ULONG               ulCRC = 0;
    AFSDirEntryCB      *pDirEntry = NULL, *pShareDirEntry = NULL;
    UNICODE_STRING      uniCurrentPath;
    BOOLEAN             bAddTrailingSlash = FALSE;
    USHORT              usIndex = 0;
    UNICODE_STRING      uniAFSAllName, uniRelatedFullName;
    BOOLEAN             bFreeRelatedName = FALSE;
    AFSCcb             *pCcb = NULL;

    __Enter
    {

        *FreeNameString = FALSE;

        if( pIrpSp->FileObject->RelatedFileObject != NULL)
        {

            pRelatedFcb = (AFSFcb *)pIrpSp->FileObject->RelatedFileObject->FsContext;

            pCcb = (AFSCcb *)pIrpSp->FileObject->RelatedFileObject->FsContext2;

            ASSERT( pRelatedFcb != NULL);

            //
            // No wild cards in the name
            //

            if( FsRtlDoesNameContainWildCards( &pIrpSp->FileObject->FileName))
            {

                try_return( ntStatus = STATUS_BAD_NETWORK_PATH);
            }

            uniFullName = pIrpSp->FileObject->FileName;

            *RootFcb = pRelatedFcb->RootFcb;

            *ParentFcb = pRelatedFcb;

            *FileName = pIrpSp->FileObject->FileName;

            //
            // Grab the root node exclusive before returning
            //

            AFSAcquireExcl( &(*RootFcb)->NPFcb->Resource,
                            TRUE);

            //
            // Create our full path name buffer
            //

            uniFullName.MaximumLength = pCcb->FullFileName.Length + 
                                                    sizeof( WCHAR) + 
                                                    pIrpSp->FileObject->FileName.Length;

            uniFullName.Length = 0;

            uniFullName.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                                 uniFullName.MaximumLength,
                                                                 AFS_NAME_BUFFER_TAG);

            if( uniFullName.Buffer != NULL)
            {

                RtlCopyMemory( uniFullName.Buffer,
                               pCcb->FullFileName.Buffer,
                               pCcb->FullFileName.Length);

                uniFullName.Length = pCcb->FullFileName.Length;

                if( uniFullName.Buffer[ (uniFullName.Length/sizeof( WCHAR)) - 1] != L'\\' &&
                    pIrpSp->FileObject->FileName.Length > 0 &&
                    pIrpSp->FileObject->FileName.Buffer[ 0] != L'\\')
                {

                    uniFullName.Buffer[ (uniFullName.Length/sizeof( WCHAR))] = L'\\';

                    uniFullName.Length += sizeof( WCHAR);
                }

                if( pIrpSp->FileObject->FileName.Length > 0)
                {

                    RtlCopyMemory( &uniFullName.Buffer[ uniFullName.Length/sizeof( WCHAR)],
                                   pIrpSp->FileObject->FileName.Buffer,
                                   pIrpSp->FileObject->FileName.Length);

                    uniFullName.Length += pIrpSp->FileObject->FileName.Length;
                }

                *FullFileName = uniFullName;

                *FreeNameString = TRUE;
            }

            try_return( ntStatus);
        }
       
        //
        // The name is a fully qualified name. Parese out the server/share names and 
        // point to the root qualified name
        // Firs thing is to locate the server name
        //

        uniFullName = pIrpSp->FileObject->FileName;

        while( usIndex < uniFullName.Length/sizeof( WCHAR))
        {

            if( uniFullName.Buffer[ usIndex] == L':')
            {

                uniFullName.Buffer = &uniFullName.Buffer[ usIndex + 2];

                uniFullName.Length -= (usIndex + 2) * sizeof( WCHAR);

                break;
            }

            usIndex++;
        }

        //
        // No wild cards in the name
        //

        if( FsRtlDoesNameContainWildCards( &uniFullName))
        {

            try_return( ntStatus = STATUS_BAD_NETWORK_PATH);
        }

        //
        // Check the name to see if it is coming in like \\afs\all\something. In this
        // case make the name \\afs\something. Though if it is \\afs\all\_._AFS_IOCTL_._
        // then don't touch it
        //

        RtlInitUnicodeString( &uniAFSAllName,
                              L"\\AFS\\ALL");

        if( ( uniFullName.Length == uniAFSAllName.Length ||
              ( uniFullName.Length > uniAFSAllName.Length &&
                uniFullName.Buffer[ 8] == L'\\')) &&
            RtlPrefixUnicodeString( &uniAFSAllName,
                                    &uniFullName,
                                    TRUE))
        {

            //
            // Case out where the name is only \\afs\all or \\afs\\all\\
            //

            if( uniFullName.Length == uniAFSAllName.Length ||
                ( uniFullName.Length == uniAFSAllName.Length + sizeof( WCHAR) &&
                  uniFullName.Buffer[ (uniFullName.Length/sizeof( WCHAR)) - 1] == L'\\'))
            {

                *RootFcb = NULL;

                FileName->Length = 0;
                FileName->MaximumLength = 0;
                FileName->Buffer = NULL;

                try_return( ntStatus = STATUS_SUCCESS);
            }

            RtlInitUnicodeString( &uniAFSAllName,
                                  AFS_PIOCTL_FILE_INTERFACE_NAME_ROOT);

            if( !RtlPrefixUnicodeString( &uniAFSAllName,
                                         &uniFullName,
                                         TRUE))
            {
                   
                //
                // Move the memory over
                //

                RtlMoveMemory( &uniFullName.Buffer[ 4],
                               &uniFullName.Buffer[ 8],
                               uniFullName.Length - 16);

                uniFullName.Length -= 8;           
            }
            else
            {

                //
                // Return NULL for all the information but with a success status
                //

                *RootFcb = NULL;

                *FileName = AFSPIOCtlName;

                try_return( ntStatus = STATUS_SUCCESS);
            }
        }

        FsRtlDissectName( uniFullName,
                          &uniComponentName,
                          &uniRemainingPath);

        uniFullName = uniRemainingPath;

        //
        // This component is the server name we are serving
        //

        if( RtlCompareUnicodeString( &uniComponentName,
                                     &AFSServerName,
                                     TRUE) != 0)
        {

            try_return( ntStatus = STATUS_BAD_NETWORK_PATH);
        }

        //
        // Get the 'share' name
        //

        FsRtlDissectName( uniFullName,
                          &uniComponentName,
                          &uniRemainingPath);

        if( uniRemainingPath.Buffer != NULL)
        {

            //
            // This routine strips off the leading slash so add it back in
            //

            uniRemainingPath.Buffer--;
            uniRemainingPath.Length += sizeof( WCHAR);
            uniRemainingPath.MaximumLength += sizeof( WCHAR);
        }

        uniFullName = uniRemainingPath;

        //
        // If we have no component name then get out
        //

        if( uniComponentName.Buffer == NULL)
        {

            try_return( ntStatus = STATUS_BAD_NETWORK_PATH);
        }

        //
        // Generate a CRC on the component and look it up in the name tree
        //

        ulCRC = AFSGenerateCRC( &uniComponentName,
                                FALSE);

        //
        // Grab our tree lock shared
        //

        AFSAcquireExcl( AFSAllRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        //
        // Locate the dir entry for this node
        //

        ntStatus = AFSLocateCaseSensitiveDirEntry( AFSAllRoot->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                   ulCRC,
                                                   &pShareDirEntry);

        if( pShareDirEntry == NULL ||
            !NT_SUCCESS( ntStatus))
        {

            //
            // Perform a case insensitive search
            //

            ulCRC = AFSGenerateCRC( &uniComponentName,
                                    TRUE);

            ntStatus = AFSLocateCaseInsensitiveDirEntry( AFSAllRoot->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                                         ulCRC,
                                                         &pShareDirEntry);

            if( pShareDirEntry == NULL ||
                !NT_SUCCESS( ntStatus))
            {

                //
                // We didn't find the cell name so post it to the CM to see if it
                // exists
                //

                ntStatus = AFSCheckCellName( &uniComponentName,
                                             &pShareDirEntry);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( AFSAllRoot->Specific.Directory.DirectoryNodeHdr.TreeLock);

                    try_return( ntStatus = STATUS_BAD_NETWORK_PATH);
                }
            }
        }

        //
        // Grab the dir node exclusive while we determine the state
        //

        AFSAcquireExcl( &pShareDirEntry->NPDirNode->Lock,
                        TRUE);

        AFSReleaseResource( AFSAllRoot->Specific.Directory.DirectoryNodeHdr.TreeLock);

        //
        // If the root node for this entry is NULL, then we need to initialize 
        // the volume node information
        //

        if( pShareDirEntry->Fcb == NULL)
        {

            ntStatus = AFSInitFcb( AFSAllRoot,
                                   pShareDirEntry,
                                   NULL);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);

                try_return( ntStatus);
            }
        }
        else
        {

            //
            // Grab the root node exclusive before returning
            //

            AFSAcquireExcl( &pShareDirEntry->Fcb->NPFcb->Resource,
                            TRUE);
        }

        //
        // Drop the volume lock
        //

        AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);

        //
        // Ensure the root node has been evaluated, if not then go do it now
        //

        if( BooleanFlagOn( pShareDirEntry->Fcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED))
        {

            ntStatus = AFSEvaluateNode( pShareDirEntry->Fcb);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSReleaseResource( &pShareDirEntry->Fcb->NPFcb->Resource);

                try_return( ntStatus);
            }

            ClearFlag( pShareDirEntry->Fcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
        }

        //
        // Be sure we return a root or a directory
        //

        if( pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK)
        {

            //
            // Check if we have a target Fcb for this node
            //

            if( pShareDirEntry->Fcb->Specific.SymbolicLink.TargetFcb == NULL)
            {

                //
                // Go retrieve the target entry for this node
                //

                ntStatus = AFSBuildTargetDirectory( pShareDirEntry->Fcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &pShareDirEntry->Fcb->NPFcb->Resource);

                    try_return( ntStatus);
                }
            }

            //
            // Swap out where we are in the chain
            //

            AFSAcquireExcl( &pShareDirEntry->Fcb->Specific.SymbolicLink.TargetFcb->NPFcb->Resource,
                            TRUE);

            AFSReleaseResource( &pShareDirEntry->Fcb->NPFcb->Resource);

            *RootFcb = pShareDirEntry->Fcb->Specific.SymbolicLink.TargetFcb;

            *ParentFcb = pShareDirEntry->Fcb->Specific.SymbolicLink.TargetFcb;
        }
        else if( pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
        {

            //
            // Check if we have a target Fcb for this node
            //

            if( pShareDirEntry->Fcb->Specific.MountPoint.TargetFcb == NULL)
            {

                AFSAcquireShared( pDeviceExt->Specific.RDR.VolumeTree.TreeLock,
                                  TRUE);

                AFSLocateHashEntry( pDeviceExt->Specific.RDR.VolumeTree.TreeHead,
                                    AFSCreateHighIndex( &pShareDirEntry->Fcb->DirEntry->DirectoryEntry.TargetFileId),
                                    &pShareDirEntry->Fcb->Specific.MountPoint.TargetFcb);

                AFSReleaseResource( pDeviceExt->Specific.RDR.VolumeTree.TreeLock);
            }

            //
            // Swap out where we are in the chain
            //

            AFSAcquireExcl( &pShareDirEntry->Fcb->Specific.SymbolicLink.TargetFcb->NPFcb->Resource,
                            TRUE);

            AFSReleaseResource( &pShareDirEntry->Fcb->NPFcb->Resource);

            *RootFcb = pShareDirEntry->Fcb->Specific.SymbolicLink.TargetFcb;

            *ParentFcb = pShareDirEntry->Fcb->Specific.SymbolicLink.TargetFcb;
        }
        else
        {

            *RootFcb = pShareDirEntry->Fcb;

            *ParentFcb = pShareDirEntry->Fcb;
        }

        if( uniRemainingPath.Buffer == NULL ||
            ( uniRemainingPath.Length == sizeof(WCHAR) &&
              uniRemainingPath.Buffer[0] == L'\\') )
        {

            //
            // If the entry is not initialized then do it now
            //

            if( (*RootFcb)->Header.NodeTypeCode == AFS_DIRECTORY_FCB &&
                !BooleanFlagOn( (*RootFcb)->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
            {

                ntStatus = AFSEnumerateDirectory( &(*RootFcb)->DirEntry->DirectoryEntry.FileId,
                                                  &(*RootFcb)->Specific.Directory.DirectoryNodeHdr,
                                                  &(*RootFcb)->Specific.Directory.DirectoryNodeListHead,
                                                  &(*RootFcb)->Specific.Directory.DirectoryNodeListTail,
                                                  &(*RootFcb)->Specific.Directory.ShortNameTree,
                                                  NULL);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                    try_return( ntStatus);
                }

                SetFlag( (*RootFcb)->Flags, AFS_FCB_DIRECTORY_ENUMERATED);
            }
            else if( (*RootFcb)->Header.NodeTypeCode == AFS_ROOT_FCB &&
                     !BooleanFlagOn( (*RootFcb)->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
            {

                ntStatus = AFSEnumerateDirectory( &(*RootFcb)->DirEntry->DirectoryEntry.FileId,
                                                  &(*RootFcb)->Specific.VolumeRoot.DirectoryNodeHdr,
                                                  &(*RootFcb)->Specific.VolumeRoot.DirectoryNodeListHead,
                                                  &(*RootFcb)->Specific.VolumeRoot.DirectoryNodeListTail,
                                                  &(*RootFcb)->Specific.VolumeRoot.ShortNameTree,
                                                  NULL);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                    try_return( ntStatus);
                }

                SetFlag( (*RootFcb)->Flags, AFS_FCB_DIRECTORY_ENUMERATED);
            }
        }

        //
        // Return the remaining portion as the file name
        //

        *FileName = uniRemainingPath;

        *FullFileName = uniRemainingPath;

        *FreeNameString = FALSE;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSCheckCellName( IN UNICODE_STRING *CellName,
                  OUT AFSDirEntryCB **ShareDirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniName;
    AFSFileID stFileID;
    AFSDirEnumEntry *pDirEnumEntry = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSDirHdr *pDirHdr = &AFSAllRoot->Specific.Directory.DirectoryNodeHdr;
    AFSDirEntryCB *pDirNode = NULL;
    UNICODE_STRING uniDirName, uniTargetName;

    __Enter
    {

        //
        // Look for some default names we will not handle
        //

        RtlInitUnicodeString( &uniName,
                              L"IPC$");

        if( RtlCompareUnicodeString( &uniName,
                                     CellName,
                                     TRUE) == 0)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        RtlInitUnicodeString( &uniName,
                              L"wkssvc");

        if( RtlCompareUnicodeString( &uniName,
                                     CellName,
                                     TRUE) == 0)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        RtlInitUnicodeString( &uniName,
                              L"srvsvc");

        if( RtlCompareUnicodeString( &uniName,
                                     CellName,
                                     TRUE) == 0)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        RtlInitUnicodeString( &uniName,
                              L"PIPE");

        if( RtlCompareUnicodeString( &uniName,
                                     CellName,
                                     TRUE) == 0)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        RtlZeroMemory( &stFileID,
                       sizeof( AFSFileID));

        //
        // OK, ask the CM about this component name
        //

        ntStatus = AFSEvaluateTargetByName( &stFileID,
                                            CellName,
                                            &pDirEnumEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // OK, we have a dir enum entry back so add it to the root node
        //

        uniDirName.Length = (USHORT)pDirEnumEntry->FileNameLength;
        uniDirName.MaximumLength = uniDirName.Length;
        uniDirName.Buffer = (WCHAR *)((char *)pDirEnumEntry + pDirEnumEntry->FileNameOffset);

        uniTargetName.Length = (USHORT)pDirEnumEntry->TargetNameLength;
        uniTargetName.MaximumLength = uniTargetName.Length;
        uniTargetName.Buffer = (WCHAR *)((char *)pDirEnumEntry + pDirEnumEntry->TargetNameOffset);

        pDirNode = AFSInitDirEntry( &stFileID,
                                    &uniDirName,
                                    &uniTargetName,
                                    pDirEnumEntry,
                                    (ULONG)InterlockedIncrement( &pDirHdr->ContentIndex));

        if( pDirNode == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Init the short name if we have one
        //

        if( pDirEnumEntry->ShortNameLength > 0)
        {

            pDirNode->DirectoryEntry.ShortNameLength = pDirEnumEntry->ShortNameLength;

            RtlCopyMemory( pDirNode->DirectoryEntry.ShortName,
                           pDirEnumEntry->ShortName,
                           pDirNode->DirectoryEntry.ShortNameLength);
        }

        //
        // Insert the node into the name tree
        //

        if( pDirHdr->CaseSensitiveTreeHead == NULL)
        {

            pDirHdr->CaseSensitiveTreeHead = pDirNode;
        }
        else
        {

            AFSInsertCaseSensitiveDirEntry( pDirHdr->CaseSensitiveTreeHead,
                                            pDirNode);
        }            

        if( pDirHdr->CaseInsensitiveTreeHead == NULL)
        {

            pDirHdr->CaseInsensitiveTreeHead = pDirNode;

            SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);
        }
        else
        {

            AFSInsertCaseInsensitiveDirEntry( pDirHdr->CaseInsensitiveTreeHead,
                                              pDirNode);
        }              

        if( AFSAllRoot->Specific.Directory.DirectoryNodeListHead == NULL)
        {

            AFSAllRoot->Specific.Directory.DirectoryNodeListHead = pDirNode;
        }
        else
        {

            AFSAllRoot->Specific.Directory.DirectoryNodeListTail->ListEntry.fLink = pDirNode;

            pDirNode->ListEntry.bLink = AFSAllRoot->Specific.Directory.DirectoryNodeListTail;
        }

        AFSAllRoot->Specific.Directory.DirectoryNodeListTail = pDirNode;

        //
        // Pass back the dir node
        //

        *ShareDirEntry = pDirNode;

try_exit:

        if( pDirEnumEntry != NULL)
        {

            ExFreePool( pDirEnumEntry);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSBuildTargetDirectory( IN AFSFcb *Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    AFSFcb *pTargetFcb = NULL, *pVcb = NULL;
    AFSFileID stFileID;
    AFSDirEnumEntry *pDirEntry = NULL;
    AFSDirEntryCB *pDirNode = NULL;
    UNICODE_STRING uniDirName, uniTargetName;
    ULONGLONG       ullIndex = 0;

    __Enter
    {

        //
        // Simple case is where we try to locate the target FID and 
        // find an Fcb
        //
    
        ntStatus = AFSRetrieveTargetFID( Fcb,
                                         &stFileID);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        AFSAcquireShared( &pDevExt->Specific.RDR.VolumeTreeLock, TRUE);

        //
        // Locate the volume node
        //

        ullIndex = AFSCreateHighIndex( &stFileID);

        ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                       ullIndex,
                                       &pVcb);

        if( pVcb != NULL)
        {

            AFSAcquireShared( pVcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                              TRUE);
        }

        AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

        if( !NT_SUCCESS( ntStatus) ||
            pVcb == NULL) 
        {
            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        //
        // Now locate the Fcb in this volume
        //

        ullIndex = AFSCreateLowIndex( &stFileID);

        ntStatus = AFSLocateHashEntry( pVcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                       ullIndex,
                                       &pTargetFcb);

        if( NT_SUCCESS( ntStatus) &&
            pTargetFcb != NULL)
        {

            Fcb->Specific.SymbolicLink.TargetFcb = pTargetFcb;
        }

        AFSReleaseResource( pVcb->Specific.VolumeRoot.FileIDTree.TreeLock);

        if( !NT_SUCCESS( ntStatus) ||
            pTargetFcb != NULL)
        {

            try_return( ntStatus);
        }

        //
        // Go retrieve a dir entry for this node
        //

        ntStatus = AFSEvaluateTargetByID( &stFileID,
                                          &pDirEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Create a dir entry for this node
        //

        uniDirName.Length = (USHORT)pDirEntry->FileNameLength;

        uniDirName.MaximumLength = uniDirName.Length;

        uniDirName.Buffer = (WCHAR *)((char *)pDirEntry + pDirEntry->FileNameOffset);

        uniTargetName.Length = (USHORT)pDirEntry->TargetNameLength;

        uniTargetName.MaximumLength = uniTargetName.Length;

        uniTargetName.Buffer = (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset);

        pDirNode = AFSInitDirEntry( NULL,
                                    &uniDirName,
                                    &uniTargetName,
                                    pDirEntry,
                                    0);

        if( pDirNode == NULL)
        {
           
            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Go initialize the Fcb
        //

        ntStatus = AFSInitFcb( Fcb->ParentFcb,
                               pDirNode,
                               &pTargetFcb);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        InterlockedIncrement( &pTargetFcb->OpenReferenceCount);

        Fcb->Specific.SymbolicLink.TargetFcb = pTargetFcb;

        //
        // Tag this Fcb as a 'stand alone Fcb since it is not referencing a parent Fcb
        //

        SetFlag( pTargetFcb->Flags, AFS_FCB_STANDALONE_NODE);

        //
        // Mark this directory node as requiring freeing since it is not in a parent list
        //

        SetFlag( pDirNode->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE);

        AFSReleaseResource( &pTargetFcb->NPFcb->Resource);
                    
try_exit:

        if( pDirEntry != NULL)
        {

            ExFreePool( pDirEntry);
        }
    }

    return ntStatus;
}