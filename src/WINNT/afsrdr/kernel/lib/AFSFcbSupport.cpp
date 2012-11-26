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
//      Return Fcb->NPFcb->Resource held exclusive
//

NTSTATUS
AFSInitFcb( IN AFSDirectoryCB  *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSFcb *pFcb = NULL;
    AFSNonPagedFcb *pNPFcb = NULL;
    IO_STATUS_BLOCK stIoSb = {0,0};
    USHORT  usFcbLength = 0;
    ULONGLONG   ullIndex = 0;
    AFSDirEnumEntry *pDirEnumCB = NULL;
    AFSObjectInfoCB *pObjectInfo = NULL, *pParentObjectInfo = NULL;
    AFSVolumeCB *pVolumeCB = NULL;

    __Enter
    {

        pObjectInfo = DirEntry->ObjectInformation;

        pParentObjectInfo = pObjectInfo->ParentObjectInformation;

        pVolumeCB = pObjectInfo->VolumeCB;

        if ( pObjectInfo->Fcb != NULL)
        {

            AFSAcquireExcl( &pObjectInfo->Fcb->NPFcb->Resource,
                            TRUE);

            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // Allocate the Fcb and the nonpaged portion of the Fcb.
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSInitFcb Initializing fcb for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      &DirEntry->NameInformation.FileName,
                      pObjectInfo->FileId.Cell,
                      pObjectInfo->FileId.Volume,
                      pObjectInfo->FileId.Vnode,
                      pObjectInfo->FileId.Unique);

        usFcbLength = sizeof( AFSFcb);

        pFcb = (AFSFcb *)AFSExAllocatePoolWithTag( PagedPool,
                                                   usFcbLength,
                                                   AFS_FCB_ALLOCATION_TAG);

        if( pFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitFcb Failed to allocate fcb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pFcb,
                       usFcbLength);

        pFcb->Header.NodeByteSize = usFcbLength;

        pNPFcb = (AFSNonPagedFcb *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                             sizeof( AFSNonPagedFcb),
                                                             AFS_FCB_NP_ALLOCATION_TAG);

        if( pNPFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitFcb Failed to allocate non-paged fcb\n");

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

        ExInitializeResourceLite( &pNPFcb->CcbListLock);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Header.PagingIoResource = &pNPFcb->PagingResource;

        //
        // Grab the Fcb for processing
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitFcb Acquiring Fcb lock %08lX EXCL %08lX\n",
                      &pNPFcb->Resource,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pNPFcb->Resource,
                        TRUE);

        pFcb->NPFcb = pNPFcb;

        //
        // Set type specific information
        //

        if( pObjectInfo->FileType == AFS_FILE_TYPE_DIRECTORY)
        {

            //
            // Reset the type to a directory type
            //

            pFcb->Header.NodeTypeCode = AFS_DIRECTORY_FCB;
        }
        else if( pObjectInfo->FileType == AFS_FILE_TYPE_FILE)
        {

            pFcb->Header.NodeTypeCode = AFS_FILE_FCB;

            //
            // Initialize the file specific information
            //

            FsRtlInitializeFileLock( &pFcb->Specific.File.FileLock,
                                     NULL,
                                     NULL);

            //
            // Initialize the header file sizes to our dir entry information
            //

            pFcb->Header.AllocationSize.QuadPart = pObjectInfo->AllocationSize.QuadPart;
            pFcb->Header.FileSize.QuadPart = pObjectInfo->EndOfFile.QuadPart;
            pFcb->Header.ValidDataLength.QuadPart = pObjectInfo->EndOfFile.QuadPart;

            //
            // Initialize the Extents resources and so forth.  The
            // quiescent state is that no one has the extents for
            // IO (do the extents are not busy) and there is no
            // extents request outstanding (and hence the "last
            // one" is complete).
            //
            ExInitializeResourceLite( &pNPFcb->Specific.File.ExtentsResource );

            KeInitializeEvent( &pNPFcb->Specific.File.ExtentsRequestComplete,
                               NotificationEvent,
                               TRUE );

            for (ULONG i = 0; i < AFS_NUM_EXTENT_LISTS; i++)
            {
                InitializeListHead(&pFcb->Specific.File.ExtentsLists[i]);
            }

            pNPFcb->Specific.File.DirtyListHead = NULL;
            pNPFcb->Specific.File.DirtyListTail = NULL;

            ExInitializeResourceLite( &pNPFcb->Specific.File.DirtyExtentsListLock);

            KeInitializeEvent( &pNPFcb->Specific.File.FlushEvent,
                               SynchronizationEvent,
                               TRUE);

            KeInitializeEvent( &pNPFcb->Specific.File.QueuedFlushEvent,
                               NotificationEvent,
                               TRUE);
        }
        else if( pObjectInfo->FileType == AFS_FILE_TYPE_SPECIAL_SHARE_NAME)
        {

            pFcb->Header.NodeTypeCode = AFS_SPECIAL_SHARE_FCB;
        }
        else if( pObjectInfo->FileType == AFS_FILE_TYPE_PIOCTL)
        {

            pFcb->Header.NodeTypeCode = AFS_IOCTL_FCB;
        }
        else if( pObjectInfo->FileType == AFS_FILE_TYPE_SYMLINK)
        {

            pFcb->Header.NodeTypeCode = AFS_SYMBOLIC_LINK_FCB;
        }
        else if( pObjectInfo->FileType == AFS_FILE_TYPE_MOUNTPOINT)
        {

            pFcb->Header.NodeTypeCode = AFS_MOUNT_POINT_FCB;
        }
        else if( pObjectInfo->FileType == AFS_FILE_TYPE_DFSLINK)
        {
            pFcb->Header.NodeTypeCode = AFS_DFS_LINK_FCB;
        }
        else
        {
            pFcb->Header.NodeTypeCode = AFS_INVALID_FCB;
        }

        pFcb->ObjectInformation = pObjectInfo;

        AFSAcquireShared( &pObjectInfo->NonPagedInfo->ObjectInfoLock,
                          TRUE);
        //
        // Swap the allocated FCB into the ObjectInformation structure if it
        // does not already have one.
        //

        if ( InterlockedCompareExchangePointer( (PVOID *)&pObjectInfo->Fcb, pFcb, NULL) != NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSInitFcb Raced Fcb %08lX pFcb %08lX Name %wZ\n",
                          pObjectInfo->Fcb,
                          pFcb,
                          &DirEntry->NameInformation.FileName);

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitFcb Acquiring Fcb lock %08lX EXCL %08lX\n",
                          &pObjectInfo->Fcb->NPFcb->Resource,
                          PsGetCurrentThread());

            AFSReleaseResource( &pObjectInfo->NonPagedInfo->ObjectInfoLock);

            AFSAcquireExcl( &pObjectInfo->Fcb->NPFcb->Resource,
                            TRUE);

            try_return( ntStatus = STATUS_REPARSE);
        }

        AFSReleaseResource( &pObjectInfo->NonPagedInfo->ObjectInfoLock);

        AFSDbgLogMsg( AFS_SUBSYSTEM_FCB_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitFcb Initialized Fcb %08lX Name %wZ\n",
                      &pObjectInfo->Fcb,
                      &DirEntry->NameInformation.FileName);

try_exit:

        if( ntStatus != STATUS_SUCCESS)
        {

            if ( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitFcb Failed to initialize fcb Status %08lX\n",
                              ntStatus);
            }

            if( pFcb != NULL)
            {

                if( pNPFcb != NULL)
                {

                    AFSReleaseResource( &pNPFcb->Resource);

                    FsRtlTeardownPerStreamContexts( &pFcb->Header);

                    if ( pObjectInfo->FileType == AFS_FILE_TYPE_FILE)
                    {

                        FsRtlUninitializeFileLock( &pFcb->Specific.File.FileLock);

                        ExDeleteResourceLite( &pNPFcb->Specific.File.ExtentsResource);

                        ExDeleteResourceLite( &pNPFcb->Specific.File.DirtyExtentsListLock);
                    }

                    ExDeleteResourceLite( &pNPFcb->PagingResource);

                    ExDeleteResourceLite( &pNPFcb->CcbListLock);

                    ExDeleteResourceLite( &pNPFcb->Resource);

                    AFSExFreePoolWithTag( pNPFcb, AFS_FCB_NP_ALLOCATION_TAG);
                }

                AFSExFreePoolWithTag( pFcb, AFS_FCB_ALLOCATION_TAG);
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSInitVolume( IN GUID *AuthGroup,
               IN AFSFileID *RootFid,
               OUT AFSVolumeCB **VolumeCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STATUS_BLOCK stIoStatus = {0,0};
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSNonPagedVolumeCB *pNonPagedVcb = NULL;
    AFSVolumeCB *pVolumeCB = NULL;
    AFSNonPagedObjectInfoCB *pNonPagedObject = NULL;
    ULONGLONG ullIndex = 0;
    BOOLEAN bReleaseLocks = FALSE;
    AFSVolumeInfoCB stVolumeInformation;
    AFSNonPagedDirectoryCB *pNonPagedDirEntry = NULL;
    LONG lCount;

    __Enter
    {

        //
        // Before grabbing any locks ask the service for the volume information
        // This may be a waste but we need to get this information prior to
        // taking any volume tree locks. Don't do this for any 'reserved' cell entries
        //

        if( RootFid->Cell != 0)
        {

            RtlZeroMemory( &stVolumeInformation,
                           sizeof( AFSVolumeInfoCB));

            ntStatus = AFSRetrieveVolumeInformation( AuthGroup,
                                                     RootFid,
                                                     &stVolumeInformation);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitVolume AFSRetrieveVolumeInformation(RootFid) failure %08lX\n",
                              ntStatus);

                try_return( ntStatus);
            }

            //
            // Grab our tree locks and see if we raced with someone else
            //

            AFSAcquireExcl( pDeviceExt->Specific.RDR.VolumeTree.TreeLock,
                            TRUE);

            AFSAcquireExcl( &pDeviceExt->Specific.RDR.VolumeListLock,
                            TRUE);

            bReleaseLocks = TRUE;

            ullIndex = AFSCreateHighIndex( RootFid);

            ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.RDR.VolumeTree.TreeHead,
                                           ullIndex,
                                           (AFSBTreeEntry **)&pVolumeCB);

            if( NT_SUCCESS( ntStatus) &&
                pVolumeCB != NULL)
            {

                //
                // So we don't lock with an invalidation call ...
                //

                lCount = InterlockedIncrement( &pVolumeCB->VolumeReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInitVolume Increment count on volume %08lX Cnt %d\n",
                              pVolumeCB,
                              lCount);

                AFSReleaseResource( pDeviceExt->Specific.RDR.VolumeTree.TreeLock);

                AFSReleaseResource( &pDeviceExt->Specific.RDR.VolumeListLock);

                bReleaseLocks = FALSE;

                AFSAcquireExcl( pVolumeCB->VolumeLock,
                                TRUE);

                *VolumeCB = pVolumeCB;

                try_return( ntStatus);
            }

            //
            // Revert our status from the above call back to success.
            //

            ntStatus = STATUS_SUCCESS;
        }

        //
        // For the global root we allocate out volume node and insert it
        // into the volume tree ...
        //

        pVolumeCB = (AFSVolumeCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                             sizeof( AFSVolumeCB),
                                                             AFS_VCB_ALLOCATION_TAG);

        if( pVolumeCB == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitVolume Failed to allocate the root volume cb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pVolumeCB,
                       sizeof( AFSVolumeCB));

        //
        // The non paged portion
        //

        pNonPagedVcb = (AFSNonPagedVolumeCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                                        sizeof( AFSNonPagedVolumeCB),
                                                                        AFS_VCB_NP_ALLOCATION_TAG);

        if( pNonPagedVcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitVolume Failed to allocate the root non paged volume cb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pNonPagedVcb,
                       sizeof( AFSNonPagedVolumeCB));

        ExInitializeResourceLite( &pNonPagedVcb->VolumeLock);

        ExInitializeResourceLite( &pNonPagedVcb->ObjectInfoTreeLock);

        pNonPagedObject = (AFSNonPagedObjectInfoCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                                               sizeof( AFSNonPagedObjectInfoCB),
                                                                               AFS_NP_OBJECT_INFO_TAG);

        if( pNonPagedObject == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitVolume Failed to allocate the root non paged object cb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pNonPagedObject,
                       sizeof( AFSNonPagedObjectInfoCB));

        ExInitializeResourceLite( &pNonPagedObject->ObjectInfoLock);

        ExInitializeResourceLite( &pNonPagedObject->DirectoryNodeHdrLock);

        pVolumeCB->NonPagedVcb = pNonPagedVcb;

        pVolumeCB->ObjectInformation.NonPagedInfo = pNonPagedObject;

        pVolumeCB->VolumeLock = &pNonPagedVcb->VolumeLock;

        pVolumeCB->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock = &pNonPagedObject->DirectoryNodeHdrLock;

        pVolumeCB->ObjectInfoTree.TreeLock = &pNonPagedVcb->ObjectInfoTreeLock;

        //
        // Bias our reference by 1
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitVolume Initializing count (2) on volume %08lX\n",
                      pVolumeCB);

        pVolumeCB->VolumeReferenceCount = 2;

        AFSAcquireExcl( pVolumeCB->VolumeLock,
                        TRUE);

        pVolumeCB->DirectoryCB = (AFSDirectoryCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                             sizeof( AFSDirectoryCB) + sizeof( WCHAR),
                                                                             AFS_DIR_ENTRY_TAG);

        if( pVolumeCB->DirectoryCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        pNonPagedDirEntry = (AFSNonPagedDirectoryCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                                                sizeof( AFSNonPagedDirectoryCB),
                                                                                AFS_DIR_ENTRY_NP_TAG);

        if( pNonPagedDirEntry == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pVolumeCB->DirectoryCB,
                       sizeof( AFSDirectoryCB) + sizeof( WCHAR));

        RtlZeroMemory( pNonPagedDirEntry,
                       sizeof( AFSNonPagedDirectoryCB));

        ExInitializeResourceLite( &pNonPagedDirEntry->Lock);

        pVolumeCB->DirectoryCB->NonPaged = pNonPagedDirEntry;

        //
        // Initialize the non-paged portion of the directory entry
        //

        KeQuerySystemTime( &pVolumeCB->ObjectInformation.CreationTime);
        KeQuerySystemTime( &pVolumeCB->ObjectInformation.LastWriteTime);
        KeQuerySystemTime( &pVolumeCB->ObjectInformation.LastAccessTime);

        pVolumeCB->ObjectInformation.FileType = AFS_FILE_TYPE_DIRECTORY;

        SetFlag( pVolumeCB->ObjectInformation.Flags, AFS_OBJECT_ROOT_VOLUME);

        pVolumeCB->ObjectInformation.FileId.Cell = RootFid->Cell;
        pVolumeCB->ObjectInformation.FileId.Volume = RootFid->Volume;
        pVolumeCB->ObjectInformation.FileId.Vnode = RootFid->Vnode;
        pVolumeCB->ObjectInformation.FileId.Unique = RootFid->Unique;
        pVolumeCB->ObjectInformation.FileId.Hash = RootFid->Hash;

        pVolumeCB->ObjectInformation.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

        pVolumeCB->DirectoryCB->NameInformation.FileName.Length = sizeof( WCHAR);

        pVolumeCB->DirectoryCB->NameInformation.FileName.MaximumLength = pVolumeCB->DirectoryCB->NameInformation.FileName.Length;

        pVolumeCB->DirectoryCB->NameInformation.FileName.Buffer = (WCHAR *)((char *)pVolumeCB->DirectoryCB + sizeof( AFSDirectoryCB));

        RtlCopyMemory( pVolumeCB->DirectoryCB->NameInformation.FileName.Buffer,
                       L"\\",
                       sizeof( WCHAR));

        //
        // Copy in the volume information retrieved above
        //

        RtlCopyMemory( &pVolumeCB->VolumeInformation,
                       &stVolumeInformation,
                       sizeof( AFSVolumeInfoCB));

        //
        // Setup pointers
        //

        pVolumeCB->DirectoryCB->ObjectInformation = &pVolumeCB->ObjectInformation;

        pVolumeCB->DirectoryCB->ObjectInformation->VolumeCB = pVolumeCB;

        //
        // Insert the volume into our volume tree. Don't insert any reserved entries
        //

        if( RootFid->Cell != 0)
        {

            pVolumeCB->TreeEntry.HashIndex = ullIndex;

            if( pDeviceExt->Specific.RDR.VolumeTree.TreeHead == NULL)
            {

                pDeviceExt->Specific.RDR.VolumeTree.TreeHead = &pVolumeCB->TreeEntry;

                SetFlag( pVolumeCB->Flags, AFS_VOLUME_INSERTED_HASH_TREE);
            }
            else
            {

                if ( NT_SUCCESS( AFSInsertHashEntry( pDeviceExt->Specific.RDR.VolumeTree.TreeHead,
                                                     &pVolumeCB->TreeEntry)))
                {

                    SetFlag( pVolumeCB->Flags, AFS_VOLUME_INSERTED_HASH_TREE);
                }
            }

            if( pDeviceExt->Specific.RDR.VolumeListHead == NULL)
            {

                pDeviceExt->Specific.RDR.VolumeListHead = pVolumeCB;
            }
            else
            {

                pDeviceExt->Specific.RDR.VolumeListTail->ListEntry.fLink = (void *)pVolumeCB;

                pVolumeCB->ListEntry.bLink = pDeviceExt->Specific.RDR.VolumeListTail;
            }

            pDeviceExt->Specific.RDR.VolumeListTail = pVolumeCB;
        }

        *VolumeCB = pVolumeCB;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pNonPagedVcb != NULL)
            {

                AFSReleaseResource( pVolumeCB->VolumeLock);

                ExDeleteResourceLite( &pNonPagedVcb->VolumeLock);

                ExDeleteResourceLite( &pNonPagedVcb->ObjectInfoTreeLock);

                AFSExFreePoolWithTag( pNonPagedVcb, AFS_VCB_NP_ALLOCATION_TAG);
            }

            if( pNonPagedObject != NULL)
            {

                ExDeleteResourceLite( &pNonPagedObject->ObjectInfoLock);

                AFSExFreePoolWithTag( pNonPagedObject, AFS_NP_OBJECT_INFO_TAG);
            }

            if( pVolumeCB != NULL)
            {

                if( pVolumeCB->DirectoryCB != NULL)
                {

                    AFSExFreePoolWithTag( pVolumeCB->DirectoryCB, AFS_DIR_ENTRY_TAG);
                }

                AFSExFreePoolWithTag( pVolumeCB, AFS_VCB_ALLOCATION_TAG);
            }

            if( pNonPagedDirEntry != NULL)
            {

                ExDeleteResourceLite( &pNonPagedDirEntry->Lock);

                AFSExFreePoolWithTag( pNonPagedDirEntry, AFS_DIR_ENTRY_NP_TAG);
            }
        }

        if( bReleaseLocks)
        {

            AFSReleaseResource( pDeviceExt->Specific.RDR.VolumeTree.TreeLock);

            AFSReleaseResource( &pDeviceExt->Specific.RDR.VolumeListLock);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSRemoveVolume( IN AFSVolumeCB *VolumeCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        //
        // Remove the volume from the tree and list
        // Don't process the list information for reserved entries
        //

        if( VolumeCB->ObjectInformation.FileId.Cell != 0)
        {

            if( BooleanFlagOn( VolumeCB->Flags, AFS_VOLUME_INSERTED_HASH_TREE))
            {

                AFSRemoveHashEntry( &pDeviceExt->Specific.RDR.VolumeTree.TreeHead,
                                    &VolumeCB->TreeEntry);
            }

            if( VolumeCB->ListEntry.fLink == NULL)
            {

                pDeviceExt->Specific.RDR.VolumeListTail = (AFSVolumeCB *)VolumeCB->ListEntry.bLink;

                if( pDeviceExt->Specific.RDR.VolumeListTail != NULL)
                {

                    pDeviceExt->Specific.RDR.VolumeListTail->ListEntry.fLink = NULL;
                }
            }
            else
            {

                ((AFSVolumeCB *)(VolumeCB->ListEntry.fLink))->ListEntry.bLink = VolumeCB->ListEntry.bLink;
            }

            if( VolumeCB->ListEntry.bLink == NULL)
            {

                pDeviceExt->Specific.RDR.VolumeListHead = (AFSVolumeCB *)VolumeCB->ListEntry.fLink;

                if( pDeviceExt->Specific.RDR.VolumeListHead != NULL)
                {

                    pDeviceExt->Specific.RDR.VolumeListHead->ListEntry.bLink = NULL;
                }
            }
            else
            {

                ((AFSVolumeCB *)(VolumeCB->ListEntry.bLink))->ListEntry.fLink = VolumeCB->ListEntry.fLink;
            }
        }

        //
        // Remove any PIOctl objects we have
        //

        if( VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB != NULL)
        {

            if( VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->ObjectInformation->Fcb != NULL)
            {

                AFSAcquireExcl( &VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->ObjectInformation->NonPagedInfo->ObjectInfoLock,
                                TRUE);

                AFSRemoveFcb( &VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->ObjectInformation->Fcb);

                AFSReleaseResource( &VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->ObjectInformation->NonPagedInfo->ObjectInfoLock);
            }

            AFSDeleteObjectInfo( VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->ObjectInformation);

            ExDeleteResourceLite( &VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->NonPaged->Lock);

            AFSExFreePoolWithTag( VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->NonPaged, AFS_DIR_ENTRY_NP_TAG);

            AFSExFreePoolWithTag( VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB, AFS_DIR_ENTRY_TAG);
        }

        if( BooleanFlagOn( VolumeCB->ObjectInformation.Flags, AFS_OBJECT_HELD_IN_SERVICE))
        {

            //
            // Release the fid in the service
            //

            AFSReleaseFid( &VolumeCB->ObjectInformation.FileId);
        }

        //
        // Free up the memory
        //

        if( VolumeCB->NonPagedVcb != NULL)
        {

            if( ExIsResourceAcquiredLite( VolumeCB->VolumeLock))
            {
                AFSReleaseResource( VolumeCB->VolumeLock);
            }

            ExDeleteResourceLite( &VolumeCB->NonPagedVcb->VolumeLock);

            ExDeleteResourceLite( &VolumeCB->NonPagedVcb->ObjectInfoTreeLock);

            AFSExFreePoolWithTag( VolumeCB->NonPagedVcb, AFS_VCB_NP_ALLOCATION_TAG);
        }

        if( VolumeCB->ObjectInformation.NonPagedInfo != NULL)
        {

            ExDeleteResourceLite( &VolumeCB->ObjectInformation.NonPagedInfo->ObjectInfoLock);

            ExDeleteResourceLite( &VolumeCB->ObjectInformation.NonPagedInfo->DirectoryNodeHdrLock);

            AFSExFreePoolWithTag( VolumeCB->ObjectInformation.NonPagedInfo, AFS_NP_OBJECT_INFO_TAG);
        }

        if( VolumeCB->DirectoryCB != NULL)
        {

            if( VolumeCB->DirectoryCB->NonPaged != NULL)
            {

                ExDeleteResourceLite( &VolumeCB->DirectoryCB->NonPaged->Lock);

                AFSExFreePoolWithTag( VolumeCB->DirectoryCB->NonPaged, AFS_DIR_ENTRY_NP_TAG);
            }

            AFSExFreePoolWithTag( VolumeCB->DirectoryCB, AFS_DIR_ENTRY_TAG);
        }

        AFSExFreePoolWithTag( VolumeCB, AFS_VCB_ALLOCATION_TAG);
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
AFSInitRootFcb( IN ULONGLONG ProcessID,
                IN AFSVolumeCB *VolumeCB)
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

        pFcb = (AFSFcb *)AFSExAllocatePoolWithTag( PagedPool,
                                                   sizeof( AFSFcb),
                                                   AFS_FCB_ALLOCATION_TAG);

        if( pFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRootFcb Failed to allocate the root fcb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pFcb,
                       sizeof( AFSFcb));

        pFcb->Header.NodeByteSize = sizeof( AFSFcb);
        pFcb->Header.NodeTypeCode = AFS_ROOT_FCB;

        pNPFcb = (AFSNonPagedFcb *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                             sizeof( AFSNonPagedFcb),
                                                             AFS_FCB_NP_ALLOCATION_TAG);

        if( pNPFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRootFcb Failed to allocate the non-paged fcb\n");

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

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitRootFcb Acquiring Fcb lock %08lX EXCL %08lX\n",
                      &pNPFcb->Resource,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pNPFcb->Resource,
                        TRUE);

        ExInitializeResourceLite( &pNPFcb->PagingResource);

        ExInitializeResourceLite( &pNPFcb->CcbListLock);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Header.PagingIoResource = &pNPFcb->PagingResource;

        pFcb->NPFcb = pNPFcb;

        //
        // Save the root Fcb in the VolumeCB
        //

        VolumeCB->ObjectInformation.Fcb = pFcb;

        VolumeCB->ObjectInformation.VolumeCB = VolumeCB;

        VolumeCB->RootFcb = pFcb;

        pFcb->ObjectInformation = &VolumeCB->ObjectInformation;

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

    AFSDirectoryCB *pCurrentDirEntry = NULL;
    AFSVolumeCB *pVolumeCB = RootFcb->ObjectInformation->VolumeCB;

    if( RootFcb->NPFcb != NULL)
    {

        //
        // Now the resource
        //

        ExDeleteResourceLite( &RootFcb->NPFcb->Resource);

        ExDeleteResourceLite( &RootFcb->NPFcb->PagingResource);

        ExDeleteResourceLite( &RootFcb->NPFcb->CcbListLock);

        //
        // The non paged region
        //

        AFSExFreePoolWithTag( RootFcb->NPFcb, AFS_FCB_NP_ALLOCATION_TAG);
    }

    //
    // And the Fcb itself
    //

    AFSExFreePoolWithTag( RootFcb, AFS_FCB_ALLOCATION_TAG);

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
AFSRemoveFcb( IN AFSFcb **ppFcb)
{

    AFSFcb * pFcb;

    pFcb = (AFSFcb *) InterlockedCompareExchangePointer( (PVOID *)ppFcb, NULL, (PVOID)(*ppFcb));

    if ( pFcb == NULL)
    {

        return;
    }

    //
    // Uninitialize the file lock if it is a file
    //

    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSRemoveFcb Removing Fcb %08lX\n",
                  pFcb);

    if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
    {

        FsRtlUninitializeFileLock( &pFcb->Specific.File.FileLock);

        //
        // The resource we allocated
        //

        ExDeleteResourceLite( &pFcb->NPFcb->Specific.File.ExtentsResource );

        ExDeleteResourceLite( &pFcb->NPFcb->Specific.File.DirtyExtentsListLock);

    }
    else if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
    {


    }

    //
    // Tear down the FM specific contexts
    //

    FsRtlTeardownPerStreamContexts( &pFcb->Header);

    //
    // Delete the resources
    //

    ExDeleteResourceLite( &pFcb->NPFcb->Resource);

    ExDeleteResourceLite( &pFcb->NPFcb->PagingResource);

    ExDeleteResourceLite( &pFcb->NPFcb->CcbListLock);

    //
    // The non paged region
    //

    AFSExFreePoolWithTag( pFcb->NPFcb, AFS_FCB_NP_ALLOCATION_TAG);

    //
    // And the Fcb itself, which includes the name
    //

    AFSExFreePoolWithTag( pFcb, AFS_FCB_ALLOCATION_TAG);

    return;
}

NTSTATUS
AFSInitCcb( IN OUT AFSCcb **Ccb)
{

    NTSTATUS Status = STATUS_SUCCESS;
    AFSCcb *pCcb = NULL;

    __Enter
    {

        //
        // Allocate our context control block
        //

        pCcb = (AFSCcb *)AFSExAllocatePoolWithTag( PagedPool,
                                                   sizeof( AFSCcb),
                                                   AFS_CCB_ALLOCATION_TAG);

        if( pCcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitCcb Failed to allocate Ccb\n");

            try_return( Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pCcb,
                       sizeof( AFSCcb));

        pCcb->Size = sizeof( AFSCcb);

        pCcb->Type = AFS_CCB;

        pCcb->NPCcb = (AFSNonPagedCcb *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                     sizeof( AFSNonPagedCcb),
                                                     AFS_CCB_NP_ALLOCATION_TAG);

        if( pCcb->NPCcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitCcb Failed to allocate NPCcb\n");

            try_return( Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        ExInitializeResourceLite( &pCcb->NPCcb->CcbLock);

        //
        // Return the Ccb
        //

        *Ccb = pCcb;

try_exit:

        if( !NT_SUCCESS( Status))
        {

            if( pCcb != NULL)
            {

                if ( pCcb->NPCcb != NULL)
                {

                    AFSExFreePoolWithTag( pCcb->NPCcb, AFS_CCB_NP_ALLOCATION_TAG);
                }

                AFSExFreePoolWithTag( pCcb, AFS_CCB_ALLOCATION_TAG);
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

    AFSAcquireExcl( &Ccb->NPCcb->CcbLock,
                    TRUE);

    if( Fcb != NULL &&
        BooleanFlagOn( Ccb->Flags, CCB_FLAG_INSERTED_CCB_LIST))
    {

        AFSAcquireExcl( &Fcb->NPFcb->CcbListLock,
                        TRUE);

        if( Ccb->ListEntry.fLink == NULL)
        {

            Fcb->CcbListTail = (AFSCcb *)Ccb->ListEntry.bLink;

            if( Fcb->CcbListTail != NULL)
            {
                Fcb->CcbListTail->ListEntry.fLink = NULL;
            }
        }
        else
        {
            ((AFSCcb *)(Ccb->ListEntry.fLink))->ListEntry.bLink = Ccb->ListEntry.bLink;
        }

        if( Ccb->ListEntry.bLink == NULL)
        {

            Fcb->CcbListHead = (AFSCcb *)Ccb->ListEntry.fLink;

            if( Fcb->CcbListHead != NULL)
            {
                Fcb->CcbListHead->ListEntry.bLink = NULL;
            }
        }
        else
        {
            ((AFSCcb *)(Ccb->ListEntry.bLink))->ListEntry.fLink = Ccb->ListEntry.fLink;
        }

        AFSReleaseResource( &Fcb->NPFcb->CcbListLock);
    }

    if( Ccb->MaskName.Buffer != NULL)
    {

        AFSExFreePoolWithTag( Ccb->MaskName.Buffer, AFS_GENERIC_MEMORY_6_TAG);
    }

    if( BooleanFlagOn( Ccb->Flags, CCB_FLAG_FREE_FULL_PATHNAME))
    {

        AFSExFreePoolWithTag( Ccb->FullFileName.Buffer, 0);
    }

    //
    // If we have a name array then delete it
    //

    if( Ccb->NameArray != NULL)
    {

        AFSFreeNameArray( Ccb->NameArray);

        Ccb->NameArray = NULL;
    }

    if( Ccb->DirectorySnapshot != NULL)
    {

        AFSExFreePoolWithTag( Ccb->DirectorySnapshot, AFS_DIR_SNAPSHOT_TAG);

        Ccb->DirectorySnapshot = NULL;
    }

    if( Ccb->NotifyMask.Buffer != NULL)
    {

        AFSExFreePoolWithTag( Ccb->NotifyMask.Buffer, AFS_GENERIC_MEMORY_7_TAG);
    }

    AFSReleaseResource( &Ccb->NPCcb->CcbLock);

    //
    // Free up the Ccb
    //

    ExDeleteResourceLite( &Ccb->NPCcb->CcbLock);

    AFSExFreePoolWithTag( Ccb->NPCcb, AFS_CCB_NP_ALLOCATION_TAG);

    AFSExFreePoolWithTag( Ccb, AFS_CCB_ALLOCATION_TAG);

    return ntStatus;
}

NTSTATUS
AFSInsertCcb( IN AFSFcb *Fcb,
              IN AFSCcb *Ccb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    AFSAcquireExcl( &Fcb->NPFcb->CcbListLock,
                    TRUE);

    AFSAcquireExcl( &Ccb->NPCcb->CcbLock,
                    TRUE);

    if( Fcb->CcbListHead == NULL)
    {
        Fcb->CcbListHead = Ccb;
    }
    else
    {
        Fcb->CcbListTail->ListEntry.fLink = (void *)Ccb;

        Ccb->ListEntry.bLink = (void *)Fcb->CcbListTail;
    }

    Fcb->CcbListTail = Ccb;

    SetFlag( Ccb->Flags, CCB_FLAG_INSERTED_CCB_LIST);

    AFSReleaseResource( &Ccb->NPCcb->CcbLock);

    AFSReleaseResource( &Fcb->NPFcb->CcbListLock);

    return ntStatus;
}
