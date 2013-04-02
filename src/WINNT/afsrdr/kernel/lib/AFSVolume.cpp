/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011, 2012, 2013 Your File System, Inc.
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
// File: AFSVolume.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSInitVolume( IN GUID *AuthGroup,
               IN AFSFileID *RootFid,
               IN LONG VolumeReferenceReason,
               OUT AFSVolumeCB **VolumeCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSNonPagedVolumeCB *pNonPagedVcb = NULL;
    AFSVolumeCB *pVolumeCB = NULL;
    AFSNonPagedObjectInfoCB *pNonPagedObject = NULL;
    ULONGLONG ullIndex = 0;
    BOOLEAN bReleaseLocks = FALSE;
    AFSVolumeInfoCB stVolumeInformation = {0};
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

                AFSDbgTrace(( AFS_SUBSYSTEM_LOAD_LIBRARY | AFS_SUBSYSTEM_INIT_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitVolume AFSRetrieveVolumeInformation(RootFid) failure %08lX\n",
                              ntStatus));

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

                lCount = AFSVolumeIncrement( pVolumeCB,
                                             VolumeReferenceReason);

                AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInitVolume Increment count on volume %p Reason %u Cnt %d\n",
                              pVolumeCB,
                              VolumeReferenceReason,
                              lCount));

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

        pVolumeCB = (AFSVolumeCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                             sizeof( AFSVolumeCB),
                                                             AFS_VCB_ALLOCATION_TAG);

        if( pVolumeCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitVolume Failed to allocate the root volume cb\n"));

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

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitVolume Failed to allocate the root non paged volume cb\n"));

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

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitVolume Failed to allocate the root non paged object cb\n"));

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

        lCount = AFSVolumeIncrement( pVolumeCB,
                                     VolumeReferenceReason);

        AFSDbgTrace(( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitVolume Initializing volume %p Reason %u count %d\n",
                      pVolumeCB,
                      VolumeReferenceReason,
                      lCount));

        AFSAcquireExcl( pVolumeCB->VolumeLock,
                        TRUE);

        pVolumeCB->DirectoryCB = (AFSDirectoryCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                             sizeof( AFSDirectoryCB) + sizeof( WCHAR),
                                                                             AFS_DIR_ENTRY_TAG);

        if( pVolumeCB->DirectoryCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitVolume AFS_DIR_ENTRY_TAG allocated %p\n",
                      pVolumeCB->DirectoryCB));

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

        //
        // The ObjectInformation VolumeCB pointer does not obtain
        // a reference count.
        //

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

                    AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSInitVolume AFS_DIR_ENTRY_TAG deallocating %p\n",
                                  pVolumeCB->DirectoryCB));

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
    LONG lCount;

    __Enter
    {

        ASSERT( VolumeCB->VolumeReferenceCount == 0);

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

            lCount = AFSObjectInfoDecrement( VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->ObjectInformation,
                                             AFS_OBJECT_REFERENCE_PIOCTL);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRemoveVolume Decrement count on object %p Cnt %d\n",
                          VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->ObjectInformation,
                          lCount));

            ASSERT( lCount == 0);

            if ( lCount == 0)
            {

                AFSDeleteObjectInfo( &VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->ObjectInformation);
            }

            ExDeleteResourceLite( &VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->NonPaged->Lock);

            AFSExFreePoolWithTag( VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB->NonPaged, AFS_DIR_ENTRY_NP_TAG);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRemoveVolume (pioctl) AFS_DIR_ENTRY_TAG deallocating %p\n",
                          VolumeCB->ObjectInformation.Specific.Directory.PIOCtlDirectoryCB));

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

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRemoveVolume AFS_DIR_ENTRY_TAG deallocating %p\n",
                          VolumeCB->DirectoryCB));

            AFSExFreePoolWithTag( VolumeCB->DirectoryCB, AFS_DIR_ENTRY_TAG);
        }

        AFSExFreePoolWithTag( VolumeCB, AFS_VCB_ALLOCATION_TAG);
    }

    return ntStatus;
}

LONG
AFSVolumeIncrement( IN AFSVolumeCB *VolumeCB,
                    IN LONG Reason)
{

    LONG lCount;

    lCount = InterlockedIncrement( &VolumeCB->VolumeReferenceCount);

    InterlockedIncrement( &VolumeCB->VolumeReferences[ Reason]);

    return lCount;
}

LONG
AFSVolumeDecrement( IN AFSVolumeCB *VolumeCB,
                    IN LONG Reason)
{

    LONG lCount;

    lCount = InterlockedDecrement( &VolumeCB->VolumeReferences[ Reason]);

    ASSERT( lCount >= 0);

    lCount = InterlockedDecrement( &VolumeCB->VolumeReferenceCount);

    ASSERT( lCount >= 0);

    return lCount;
}
