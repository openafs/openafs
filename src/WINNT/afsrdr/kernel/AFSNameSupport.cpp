//
// File: AFSNameSupport.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSLocateNameEntry( IN AFSFcb *RootFcb,
                    IN PFILE_OBJECT FileObject,
                    IN UNICODE_STRING *FullPathName,
                    IN OUT AFSFcb **ParentFcb,
                    IN OUT AFSFcb **Fcb,
                    IN OUT PUNICODE_STRING ComponentName)
{

    NTSTATUS          ntStatus = STATUS_SUCCESS;
    AFSFcb           *pParentFcb = NULL, *pCurrentFcb = NULL;
    UNICODE_STRING    uniPathName, uniComponentName, uniRemainingPath;
    ULONG             ulCRC = 0;
    AFSDirEntryCB    *pDirEntry = NULL;
    UNICODE_STRING    uniCurrentPath;

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

        pParentFcb = RootFcb;

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
            // If the parent is not initialized then do it now
            //

            if( pParentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB &&
                !BooleanFlagOn( pParentFcb->Flags, AFS_FCB_DIRECTORY_INITIALIZED))
            {

                AFSFileID stFileID;

                if( pParentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY)
                {

                    //
                    // Just the FID of the node
                    //

                    stFileID = pParentFcb->DirEntry->DirectoryEntry.FileId;
                }
                else
                {

                    //
                    // MP or SL
                    //

                    stFileID = pParentFcb->DirEntry->DirectoryEntry.TargetFileId;

                    //
                    // If this is zero then we need to evaluate it
                    //

                    if( stFileID.Hash == 0)
                    {

                        AFSDirEnumEntry *pDirEntry = NULL;

                        if( pParentFcb->ParentFcb != NULL)
                        {

                            if( pParentFcb->ParentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY)
                            {

                                stFileID = pParentFcb->ParentFcb->DirEntry->DirectoryEntry.FileId;
                            }
                            else
                            {

                                stFileID = pParentFcb->ParentFcb->DirEntry->DirectoryEntry.TargetFileId;
                            }
                        }
                        else
                        {

                            RtlZeroMemory( &stFileID,
                                           sizeof( AFSFileID));
                        }

                        ntStatus = AFSEvaluateTargetByID( &stFileID,
                                                          &pParentFcb->DirEntry->DirectoryEntry.FileId,
                                                          &pDirEntry);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            try_return( ntStatus);
                        }

                        pParentFcb->DirEntry->DirectoryEntry.TargetFileId = pDirEntry->TargetFileId;

                        stFileID = pDirEntry->TargetFileId;

                        ExFreePool( pDirEntry);
                    }
                }

                ntStatus = AFSEnumerateDirectory( &stFileID,
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

                SetFlag( pParentFcb->Flags, AFS_FCB_DIRECTORY_INITIALIZED);
            }
            else if( pParentFcb->Header.NodeTypeCode == AFS_FILE_FCB)
            {

                ntStatus = STATUS_OBJECT_NAME_INVALID;

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                break;
            }

            //
            // Generate the CRC on the node and perform a lookup
            //

            ulCRC = AFSGenerateCRC( &uniComponentName);

            AFSLocateDirEntry( pParentFcb->Specific.Directory.DirectoryNodeHdr.TreeHead,
                               ulCRC,
                               &pDirEntry);

            if( pDirEntry == NULL)
            {

                //
                // OK, if this component is a valid short name then try
                // a lookup in the short name tree
                //

                if( RtlIsNameLegalDOS8Dot3( &uniComponentName,
                                            NULL,
                                            NULL))
                {

                    AFSLocateShortNameDirEntry( pParentFcb->Specific.Directory.ShortNameTree,
                                                ulCRC,
                                                &pDirEntry);
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

                        *ComponentName = uniComponentName;
                    }

                    //
                    // Node name not found so get out
                    //

                    break;
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

                ntStatus = AFSInitFcb( pParentFcb,
                                       &uniCurrentPath,
                                       pDirEntry,
                                       &pCurrentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    break;
                }
            }
            else
            {

                pCurrentFcb = pDirEntry->Fcb;
            
                AFSAcquireExcl( &pCurrentFcb->NPFcb->Resource,
                                TRUE);
            }

            if( uniRemainingPath.Length > 0 &&
                pCurrentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
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
                // Pass back the Parent and Fcb
                //

                *ParentFcb = pParentFcb;

                *Fcb = pCurrentFcb;

                break;
            }
        }

try_exit:

        NOTHING;
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
        //

        ntStatus = AFSNotifyFileCreate( ParentDcb,
                                        &liFileSize,
                                        Attributes,
                                        ComponentName,
                                        &pDirNode);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Inser the directory node
        //

        AFSInsertDirectoryNode( ParentDcb,
                                pDirNode);

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

            ulCRC = AFSGenerateCRC( &uniShortName);

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

        if( ParentDcb->Specific.Directory.DirectoryNodeHdr.TreeHead == NULL)
        {

            ParentDcb->Specific.Directory.DirectoryNodeHdr.TreeHead = DirEntry;
        }
        else
        {

            AFSInsertDirEntry( ParentDcb->Specific.Directory.DirectoryNodeHdr.TreeHead,
                               DirEntry);
        }               

        //
        // Into the shortname tree
        //

        if( ParentDcb->Specific.Directory.ShortNameTree == NULL)
        {

            ParentDcb->Specific.Directory.ShortNameTree = DirEntry;
        }
        else
        {

            AFSInsertShortNameDirEntry( ParentDcb->Specific.Directory.ShortNameTree,
                                        DirEntry);
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

        //
        // Remove the entry from the parent tree
        //

        AFSRemoveDirEntry( &ParentDcb->Specific.Directory.DirectoryNodeHdr.TreeHead,
                           DirEntry);

        if( DirEntry->Type.Data.ShortNameTreeEntry.Index != 0)
        {

            //
            // From the short name tree
            //

            AFSRemoveShortNameDirEntry( &ParentDcb->Specific.Directory.ShortNameTree,
                                        DirEntry);
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
AFSGetFullName( IN AFSFcb *Fcb,
                IN OUT PUNICODE_STRING FullFileName)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniFileName;
    AFSFcb *pCurrentFcb = NULL;
    ULONG ulCurrentIndex = 0;

    __Enter
    {

        uniFileName.Length = 0;
        uniFileName.MaximumLength = 0;
        uniFileName.Buffer = NULL;

        //
        // Be sure this is not the root entry
        //

        if( Fcb->Header.NodeTypeCode == AFS_ROOT_FCB)
        {

            uniFileName.Length = sizeof( WCHAR);
            uniFileName.MaximumLength = sizeof( WCHAR);

            uniFileName.Buffer = Fcb->DirEntry->DirectoryEntry.FileName.Buffer;

            try_return( ntStatus);
        }

        //
        // Get the length required
        //

        pCurrentFcb = Fcb;

        while( TRUE)
        {

            uniFileName.Length += (USHORT)pCurrentFcb->DirEntry->DirectoryEntry.FileName.Length;

            uniFileName.Length += sizeof( WCHAR);

            pCurrentFcb = pCurrentFcb->ParentFcb;

            if( pCurrentFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
            {               

                break;
            }
        }

        uniFileName.MaximumLength = uniFileName.Length;

        //
        // Allocate the buffer for the full name
        //

        uniFileName.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                             uniFileName.Length,
                                                             AFS_NAME_BUFFER_TAG);

        if( uniFileName.Buffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( uniFileName.Buffer,
                       uniFileName.Length);

        //
        // Move in the name itself
        //

        ulCurrentIndex = uniFileName.Length / sizeof( WCHAR);

        pCurrentFcb = Fcb;

        while( TRUE)
        {

            ulCurrentIndex -= pCurrentFcb->DirEntry->DirectoryEntry.FileName.Length/sizeof( WCHAR);

            RtlCopyMemory( &uniFileName.Buffer[ ulCurrentIndex],
                           pCurrentFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                           pCurrentFcb->DirEntry->DirectoryEntry.FileName.Length);

            ulCurrentIndex--;

            uniFileName.Buffer[ ulCurrentIndex] = L'\\';

            pCurrentFcb = pCurrentFcb->ParentFcb;

            if( ulCurrentIndex == 0 ||
                pCurrentFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
            {

                break;
            }
        }

        //
        // Pass back the name
        //

        FullFileName->Length = uniFileName.Length;
        FullFileName->MaximumLength = uniFileName.MaximumLength;
        FullFileName->Buffer = uniFileName.Buffer;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS 
AFSParseName( IN PIRP Irp,
              OUT PUNICODE_STRING FileName,
              OUT BOOLEAN *FreeNameString,
              OUT AFSFcb **RootFcb)
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
    UNICODE_STRING      uniAFSAllName;

    __Enter
    {

        if( pIrpSp->FileObject->RelatedFileObject != NULL)
        {

            pRelatedFcb = (AFSFcb *)pIrpSp->FileObject->RelatedFileObject->FsContext;

            ASSERT( pRelatedFcb != NULL);

            //
            // No wild cards in the name
            //

            if( FsRtlDoesNameContainWildCards( &pIrpSp->FileObject->FileName))
            {

                try_return( ntStatus = STATUS_BAD_NETWORK_PATH);
            }

            AFSAcquireShared( &pRelatedFcb->NPFcb->Resource,
                                TRUE);

            uniFullName.MaximumLength = uniFullName.Length = 0;

            uniFullName.Buffer = NULL;

            uniFullName.MaximumLength = pRelatedFcb->DirEntry->DirectoryEntry.FileName.Length;

            //
            // By definition of the names, they do not contain a trailing slash
            //

            if( pRelatedFcb->Header.NodeTypeCode != AFS_ROOT_FCB &&
                pIrpSp->FileObject->FileName.Buffer != NULL &&
                pIrpSp->FileObject->FileName.Buffer[ 0] != L'\\')
            {

                uniFullName.MaximumLength += sizeof( WCHAR);

                bAddTrailingSlash = TRUE;
            }

            uniFullName.MaximumLength += pIrpSp->FileObject->FileName.Length;

            //
            // Allocate the name buffer and build up the name
            //

            uniFullName.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                                 uniFullName.MaximumLength,
                                                                 AFS_NAME_BUFFER_TAG);

            if( uniFullName.Buffer == NULL)
            {

                AFSReleaseResource( &pRelatedFcb->NPFcb->Resource);

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            *FreeNameString = TRUE;

            if( pRelatedFcb->DirEntry->DirectoryEntry.FileName.Buffer != NULL)
            {

                RtlCopyMemory( uniFullName.Buffer,
                               pRelatedFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                               pRelatedFcb->DirEntry->DirectoryEntry.FileName.Length);

                uniFullName.Length = pRelatedFcb->DirEntry->DirectoryEntry.FileName.Length;
            }

            if( bAddTrailingSlash)
            {

                uniFullName.Buffer[ uniFullName.Length/sizeof( WCHAR)] = L'\\';

                uniFullName.Length += sizeof( WCHAR);
            }

            if( pIrpSp->FileObject->FileName.Length > 0)
            {

                RtlCopyMemory( &uniFullName.Buffer[ uniFullName.Length/sizeof( WCHAR)],
                               pIrpSp->FileObject->FileName.Buffer,
                               pIrpSp->FileObject->FileName.Length);

                uniFullName.Length += pIrpSp->FileObject->FileName.Length;
            }

            *RootFcb = pRelatedFcb->RootFcb;

            *FileName = uniFullName;

            AFSReleaseResource( &pRelatedFcb->NPFcb->Resource);

            //
            // Grab the root node exclusive before returning
            //

            AFSAcquireExcl( &(*RootFcb)->NPFcb->Resource,
                              TRUE);

            try_return( ntStatus);
        }
       
        //
        // The name is a fully qualified name. Parese out the server/share names and 
        // point to the root qualified name
        // Firs thing is to locate the server name
        //

        uniFullName = pIrpSp->FileObject->FileName;

        *FreeNameString = FALSE;

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
        // Special case the \\AFS\All request
        //

        RtlInitUnicodeString( &uniAFSAllName,
                              L"ALL");

        if( RtlCompareUnicodeString( &uniComponentName,
                                     &uniAFSAllName,
                                     TRUE) == 0)
        {

            //
            // Return NULL for all the information but with a success status
            //

            *RootFcb = NULL;

            *FileName = uniRemainingPath;  // Return any remaining portion

            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // Generate a CRC on the component and look it up in the name tree
        //

        ulCRC = AFSGenerateCRC( &uniComponentName);

        //
        // Grab our tree lock shared
        //

        AFSAcquireExcl( AFSAllRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        //
        // Locate the dir entry for this node
        //

        ntStatus = AFSLocateDirEntry( AFSAllRoot->Specific.Directory.DirectoryNodeHdr.TreeHead,
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

        //
        // Grab the dir node exclusive while we determine the state
        //

        AFSAcquireExcl( &pShareDirEntry->NPDirNode->Lock,
                        TRUE);

        AFSReleaseResource( AFSAllRoot->Specific.Directory.DirectoryNodeHdr.TreeLock);

        //
        // TODO: If this is not a volume type node then we will need to walk the
        // link to get to the volume node
        //




        //
        // If the root node for this entry is NULL, then we need to initialize 
        // the volume node information
        //

        if( pShareDirEntry->Fcb == NULL)
        {

            ntStatus = AFSInitializeVolume( pShareDirEntry);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);

                try_return( ntStatus);
            }
        }

        //
        // Grab the root node exclusive before returning
        //

        AFSAcquireExcl( &pShareDirEntry->Fcb->NPFcb->Resource,
                        TRUE);

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
        // Return the remaining portion as the file name
        //

        *FileName = uniRemainingPath;

        *RootFcb = pShareDirEntry->Fcb;

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

        if( pDirHdr->TreeHead == NULL)
        {

            pDirHdr->TreeHead = pDirNode;
        }
        else
        {

            AFSInsertDirEntry( pDirHdr->TreeHead,
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
