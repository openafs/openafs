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
    AFSFcb *pFcb = NULL;
    AFSNonPagedFcb *pNPFcb = NULL;
    USHORT  usFcbLength = 0;
    AFSObjectInfoCB *pObjectInfo = NULL;
    AFSVolumeCB *pVolumeCB = NULL;

    __Enter
    {

        pObjectInfo = DirEntry->ObjectInformation;

        if ( pObjectInfo->Fcb != NULL)
        {

            AFSAcquireExcl( &pObjectInfo->Fcb->NPFcb->Resource,
                            TRUE);

            try_return( ntStatus = STATUS_SUCCESS);
        }

        pVolumeCB = pObjectInfo->VolumeCB;

        //
        // Allocate the Fcb and the nonpaged portion of the Fcb.
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSInitFcb Initializing fcb for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      &DirEntry->NameInformation.FileName,
                      pObjectInfo->FileId.Cell,
                      pObjectInfo->FileId.Volume,
                      pObjectInfo->FileId.Vnode,
                      pObjectInfo->FileId.Unique));

        usFcbLength = sizeof( AFSFcb);

        pFcb = (AFSFcb *)AFSExAllocatePoolWithTag( PagedPool,
                                                   usFcbLength,
                                                   AFS_FCB_ALLOCATION_TAG);

        if( pFcb == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitFcb Failed to allocate fcb\n"));

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

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitFcb Failed to allocate non-paged fcb\n"));

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

        ExInitializeResourceLite( &pNPFcb->SectionObjectResource);

        ExInitializeResourceLite( &pNPFcb->CcbListLock);

        //
        // Grab the Fcb for processing
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitFcb Acquiring Fcb lock %p EXCL %08lX\n",
                      &pNPFcb->Resource,
                      PsGetCurrentThread()));

        AFSAcquireExcl( &pNPFcb->Resource,
                        TRUE);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Header.PagingIoResource = &pNPFcb->PagingResource;

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

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSInitFcb Raced Fcb %p pFcb %p Name %wZ\n",
                          pObjectInfo->Fcb,
                          pFcb,
                          &DirEntry->NameInformation.FileName));

            AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitFcb Acquiring Fcb lock %p EXCL %08lX\n",
                          &pObjectInfo->Fcb->NPFcb->Resource,
                          PsGetCurrentThread()));

            AFSReleaseResource( &pObjectInfo->NonPagedInfo->ObjectInfoLock);

            AFSAcquireExcl( &pObjectInfo->Fcb->NPFcb->Resource,
                            TRUE);

            try_return( ntStatus = STATUS_REPARSE);
        }

        AFSReleaseResource( &pObjectInfo->NonPagedInfo->ObjectInfoLock);

        AFSDbgTrace(( AFS_SUBSYSTEM_FCB_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitFcb Initialized Fcb %p Name %wZ\n",
                      &pObjectInfo->Fcb,
                      &DirEntry->NameInformation.FileName));

try_exit:

        if( ntStatus != STATUS_SUCCESS)
        {

            if ( !NT_SUCCESS( ntStatus))
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitFcb Failed to initialize fcb Status %08lX\n",
                              ntStatus));
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

                    ExDeleteResourceLite( &pNPFcb->SectionObjectResource);

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

    UNREFERENCED_PARAMETER(ProcessID);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pFcb = NULL;
    AFSNonPagedFcb *pNPFcb = NULL;

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

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRootFcb Failed to allocate the root fcb\n"));

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

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRootFcb Failed to allocate the non-paged fcb\n"));

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

        ExInitializeResourceLite( &pNPFcb->PagingResource);

        ExInitializeResourceLite( &pNPFcb->SectionObjectResource);

        ExInitializeResourceLite( &pNPFcb->CcbListLock);

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitRootFcb Acquiring Fcb lock %p EXCL %08lX\n",
                      &pNPFcb->Resource,
                      PsGetCurrentThread()));

        AFSAcquireExcl( &pNPFcb->Resource,
                        TRUE);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Header.PagingIoResource = &pNPFcb->PagingResource;

        pFcb->NPFcb = pNPFcb;

        //
        // Save the root Fcb in the VolumeCB
        //

        pFcb->ObjectInformation = &VolumeCB->ObjectInformation;

        VolumeCB->ObjectInformation.VolumeCB = VolumeCB;

        AFSAcquireShared( &VolumeCB->ObjectInformation.NonPagedInfo->ObjectInfoLock,
                          TRUE);
        //
        // Swap the allocated FCB into the ObjectInformation structure if it
        // does not already have one.
        //

        if ( InterlockedCompareExchangePointer( (PVOID *)&VolumeCB->ObjectInformation.Fcb, pFcb, NULL) != NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSInitRootFcb Raced Fcb %p pFcb %p\n",
                          VolumeCB->ObjectInformation.Fcb,
                          pFcb));

            AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitRootFcb Acquiring Fcb lock %p EXCL %08lX\n",
                          &VolumeCB->ObjectInformation.Fcb->NPFcb->Resource,
                          PsGetCurrentThread()));

            AFSReleaseResource( &VolumeCB->ObjectInformation.NonPagedInfo->ObjectInfoLock);

            AFSAcquireExcl( &VolumeCB->ObjectInformation.Fcb->NPFcb->Resource,
                            TRUE);

            try_return( ntStatus = STATUS_REPARSE);
        }

        VolumeCB->RootFcb = VolumeCB->ObjectInformation.Fcb;

        AFSReleaseResource( &VolumeCB->ObjectInformation.NonPagedInfo->ObjectInfoLock);

        AFSDbgTrace(( AFS_SUBSYSTEM_FCB_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitRootFcb Initialized Fcb %p\n",
                      &VolumeCB->ObjectInformation.Fcb));

try_exit:

        if( !NT_SUCCESS( ntStatus) ||
            ntStatus == STATUS_REPARSE)
        {

            if( pFcb != NULL)
            {

                if( pNPFcb != NULL)
                {

                    AFSReleaseResource( &pNPFcb->Resource);

                    FsRtlTeardownPerStreamContexts( &pFcb->Header);

                    ExDeleteResourceLite( &pNPFcb->SectionObjectResource);

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

//
// Function: AFSRemoveRootFcb
//
// Description:
//
//      This function performs root Fcb removal/deallocation from
//      the provided VolumeCB object.
//
// Return:
//
//      None
//

void
AFSRemoveRootFcb( IN AFSVolumeCB *VolumeCB)
{
    AFSFcb * pRootFcb;

    pRootFcb = (AFSFcb *) InterlockedCompareExchangePointer( (PVOID *)&VolumeCB->ObjectInformation.Fcb,
                                                             NULL,
                                                             (PVOID *)&VolumeCB->ObjectInformation.Fcb);

    if ( pRootFcb == NULL)
    {

        return;
    }

    //
    // The Fcb has been disconnected from the ObjectInformation block.
    // Clear it from the RootFcb convenience pointer.
    //

    VolumeCB->RootFcb = NULL;

    if( pRootFcb->NPFcb != NULL)
    {

        //
        // Now the resource
        //

        ExDeleteResourceLite( &pRootFcb->NPFcb->Resource);

        ExDeleteResourceLite( &pRootFcb->NPFcb->PagingResource);

        ExDeleteResourceLite( &pRootFcb->NPFcb->SectionObjectResource);

        ExDeleteResourceLite( &pRootFcb->NPFcb->CcbListLock);

        FsRtlTeardownPerStreamContexts( &pRootFcb->Header);

        //
        // The non paged region
        //

        AFSExFreePoolWithTag( pRootFcb->NPFcb, AFS_FCB_NP_ALLOCATION_TAG);

        pRootFcb->NPFcb = NULL;
    }

    //
    // And the Fcb itself
    //

    AFSExFreePoolWithTag( pRootFcb, AFS_FCB_ALLOCATION_TAG);

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

    ASSERT( pFcb->Header.NodeTypeCode != AFS_ROOT_FCB);

    //
    // Uninitialize the file lock if it is a file
    //

    AFSDbgTrace(( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSRemoveFcb Removing Fcb %p\n",
                  pFcb));

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

    ExDeleteResourceLite( &pFcb->NPFcb->SectionObjectResource);

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
AFSInitCcb( IN OUT AFSCcb **Ccb,
            IN     AFSDirectoryCB *DirectoryCB,
            IN     ACCESS_MASK     GrantedAccess,
            IN     ULONG           FileAccess)
{

    NTSTATUS Status = STATUS_SUCCESS;
    AFSCcb *pCcb = NULL;
    LONG lCount;

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

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitCcb Failed to allocate Ccb\n"));

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

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitCcb Failed to allocate NPCcb\n"));

            try_return( Status = STATUS_INSUFFICIENT_RESOURCES);
        }

        ExInitializeResourceLite( &pCcb->NPCcb->CcbLock);

        pCcb->DirectoryCB = DirectoryCB;

        lCount = InterlockedIncrement( &pCcb->DirectoryCB->DirOpenReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitCcb Increment count on %wZ DE %p Ccb %p Cnt %d\n",
                      &pCcb->DirectoryCB->NameInformation.FileName,
                      pCcb->DirectoryCB,
                      pCcb,
                      lCount));

        pCcb->GrantedAccess = GrantedAccess;

        pCcb->FileAccess = FileAccess;

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
//      None
//

void
AFSRemoveCcb( IN AFSFcb *Fcb,
              IN AFSCcb *Ccb)
{

    LONG lCount;

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

    if ( Ccb->DirectoryCB != NULL)
    {

        lCount = InterlockedDecrement( &Ccb->DirectoryCB->DirOpenReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveCcb Decrement count on %wZ DE %p Ccb %p Cnt %d\n",
                      &Ccb->DirectoryCB->NameInformation.FileName,
                      Ccb->DirectoryCB,
                      Ccb,
                      lCount));

        ASSERT( lCount >= 0);
    }

    AFSReleaseResource( &Ccb->NPCcb->CcbLock);

    //
    // Free up the Ccb
    //

    ExDeleteResourceLite( &Ccb->NPCcb->CcbLock);

    AFSExFreePoolWithTag( Ccb->NPCcb, AFS_CCB_NP_ALLOCATION_TAG);

    AFSExFreePoolWithTag( Ccb, AFS_CCB_ALLOCATION_TAG);
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
