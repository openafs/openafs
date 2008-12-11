/*
 * Copyright (c) 2008 Kernel Drivers, LLC.
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
 * - Neither the name of Kernel Drivers, LLC nor the names of its
 *   contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission from Kernel Drivers, LLC.
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
    AFSDirEnumEntry *pDirEnumCB = NULL;
    AFSFcb      *pVcb = NULL;
    
    __Enter
    {

        //
        // Allocate the Fcb and the nonpaged portion of the Fcb.
        //

        if( DirEntry != NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSInitFcb Initializing fcb for %wZ Parent %wZ\n",
                                                  &DirEntry->DirectoryEntry.FileName,
                                                  &ParentFcb->DirEntry->DirectoryEntry.FileName);

        }
        else
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSInitFcb Initializing fcb for (PIOCtl/Share) Parent %wZ\n",
                                                  &ParentFcb->DirEntry->DirectoryEntry.FileName);
        }

        usFcbLength = sizeof( AFSFcb);

        pFcb = (AFSFcb *)ExAllocatePoolWithTag( PagedPool,
                                                usFcbLength,
                                                AFS_FCB_ALLOCATION_TAG);

        if( pFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitFcb Failed to allocate fcb\n");

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
        // Initialize some fields in the Fcb
        //

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
            else if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_FILE ||
                     DirEntry->DirectoryEntry.FileType == 0)
            {

                //
                // Initialize the file specific information
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

                InitializeListHead( &pNPFcb->Specific.File.DirtyExtentsList);

                ExInitializeResourceLite( &pNPFcb->Specific.File.DirtyExtentsListLock);

                KeInitializeEvent( &pNPFcb->Specific.File.FlushEvent, 
                                   SynchronizationEvent, 
                                   TRUE);

                KeInitializeEvent( &pNPFcb->Specific.File.QueuedFlushEvent, 
                                   NotificationEvent, 
                                   TRUE);
            }
            else if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
            {

                //
                // Reset the type to a mount point type
                //

                pFcb->Header.NodeTypeCode = AFS_MOUNT_POINT_FCB;
            }
            else if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK)
            {

                //
                // Reset the type to a symblic link type
                //

                pFcb->Header.NodeTypeCode = AFS_SYMBOLIC_LINK_FCB;
            }
            else if( DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DFSLINK)
            {

                //
                // Reset the type to a dfs link type
                //

                pFcb->Header.NodeTypeCode = AFS_DFS_LINK_FCB;
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

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitFcb Acquiring VolumeRoot FcbListLock lock %08lX EXCL %08lX\n",
                                                           pFcb->RootFcb->Specific.VolumeRoot.FcbListLock,
                                                           PsGetCurrentThread());

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

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInitFcb Acquiring VolumeRoot FileIDTree.TreeLock lock %08lX EXCL %08lX\n",
                                                           pFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                                           PsGetCurrentThread());

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

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitFcb Failed to initialize fcb Status %08lX\n",
                                        ntStatus);

            if( DirEntry != NULL)
            {

                DirEntry->Fcb = NULL;
            }

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

        if( AFSGlobalRoot == NULL)
        {

            //
            // Initialize the root fcb
            //

            pFcb = (AFSFcb *)ExAllocatePoolWithTag( PagedPool,
                                                      sizeof( AFSFcb),
                                                      AFS_FCB_ALLOCATION_TAG);

            if( pFcb == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitAFSRoot Failed to allocate the root fcb\n");

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

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitAFSRoot Failed to allocate the non-paged fcb\n");

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

            // 
            // The AFS Global Root has a cell identifier of 0.
            // It is a root volume therefore it has a Vnode/Unique of 1
            //
            pFcb->DirEntry->DirectoryEntry.FileId.Cell = 0;
            pFcb->DirEntry->DirectoryEntry.FileId.Volume = 0;
            pFcb->DirEntry->DirectoryEntry.FileId.Vnode = 1;
            pFcb->DirEntry->DirectoryEntry.FileId.Unique = 1;
            pFcb->DirEntry->DirectoryEntry.FileId.Hash = 0;

            pFcb->DirEntry->DirectoryEntry.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

            pFcb->DirEntry->DirectoryEntry.FileName.Length = sizeof( WCHAR);
        
            pFcb->DirEntry->DirectoryEntry.FileName.MaximumLength = pFcb->DirEntry->DirectoryEntry.FileName.Length;

            pFcb->DirEntry->DirectoryEntry.FileName.Buffer = (WCHAR *)((char *)pFcb->DirEntry + sizeof( AFSDirEntryCB));

            RtlCopyMemory( pFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                           L"\\",
                           sizeof( WCHAR));

            AFSGlobalRoot = pFcb;

            pFcb = NULL; // So we don't tear it down on failure below
        }
        
        //
        // Go get the volume information from the service
        //

        ASSERT( AFSGlobalRoot->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY &&
                AFSGlobalRoot->DirEntry->DirectoryEntry.FileId.Vnode == 1);

        ntStatus = AFSRetrieveVolumeInformation( &AFSGlobalRoot->DirEntry->DirectoryEntry.FileId,
                                                 &AFSGlobalRoot->DirEntry->Type.Volume.VolumeInformation);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }
      
try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pFcb != NULL)
            {

                if( pFcb->NPFcb != NULL)
                {

                    ExDeleteResourceLite( &pFcb->NPFcb->Resource);

                    ExDeleteResourceLite( &pFcb->NPFcb->Specific.VolumeRoot.DirectoryTreeLock);

                    ExDeleteResourceLite( &pFcb->NPFcb->Specific.VolumeRoot.FileIDTreeLock);

                    ExDeleteResourceLite( &pFcb->NPFcb->Specific.VolumeRoot.FcbListLock);

                    FsRtlNotifyUninitializeSync( &pFcb->NPFcb->NotifySync);

                    ExFreePool( pFcb->NPFcb);
                }

                if( pFcb->DirEntry != NULL)
                {

                    if( pFcb->DirEntry->NPDirNode != NULL)
                    {

                        ExDeleteResourceLite( &pFcb->DirEntry->NPDirNode->Lock);

                        ExFreePool( pFcb->DirEntry->NPDirNode);
                    }

                    ExFreePool( pFcb->DirEntry);
                }

                ExFreePool( pFcb);
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

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveAFSRoot Removing Global root\n");

        if( AFSGlobalRoot == NULL)
        {

            //
            // Already initialized
            //

            try_return( ntStatus);
        }

        //
        // Reset the Redirector volume tree
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveAFSRoot Acquiring RDR VolumeTreeLock lock %08lX EXCL %08lX\n",
                                                           &pDevExt->Specific.RDR.VolumeTreeLock,
                                                           PsGetCurrentThread());

        AFSAcquireExcl( &pDevExt->Specific.RDR.VolumeTreeLock, 
                        TRUE);

        pDevExt->Specific.RDR.VolumeTree.TreeHead = NULL;

        AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveAFSRoot Acquiring GlobalRoot lock %08lX EXCL %08lX\n",
                                                           &AFSGlobalRoot->NPFcb->Resource,
                                                           PsGetCurrentThread());

        AFSAcquireExcl( &AFSGlobalRoot->NPFcb->Resource,
                        TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveAFSRoot Acquiring GlobalRoot DirectoryNodeHdr.TreeLock lock %08lX EXCL %08lX\n",
                                                           AFSGlobalRoot->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock,
                                                           PsGetCurrentThread());

        AFSAcquireExcl( AFSGlobalRoot->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock,
                        TRUE);

        AFSGlobalRoot->Specific.VolumeRoot.DirectoryNodeHdr.CaseSensitiveTreeHead = NULL;

        AFSGlobalRoot->Specific.VolumeRoot.DirectoryNodeHdr.CaseInsensitiveTreeHead = NULL;

        //
        // Reset the btree and directory list information
        //

        pCurrentDirEntry = AFSGlobalRoot->Specific.VolumeRoot.DirectoryNodeListHead;

        while( pCurrentDirEntry != NULL)
        {

            pFcb = pCurrentDirEntry->Fcb;

            if( pFcb != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSRemoveAFSRoot Acquiring Fcb lock %08lX EXCL %08lX\n",
                                                                   &pFcb->NPFcb->Resource,
                                                                   PsGetCurrentThread());

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

                AFSRemoveDirNodeFromParent( AFSGlobalRoot,
                                            pCurrentDirEntry);

                ASSERT( pFcb->OpenReferenceCount != 0);

                InterlockedDecrement( &pFcb->OpenReferenceCount);
            }
            else
            {

                AFSDeleteDirEntry( AFSGlobalRoot,
                                   pCurrentDirEntry);
            }

            pCurrentDirEntry = AFSGlobalRoot->Specific.Directory.DirectoryNodeListHead;
        }

        AFSGlobalRoot->Specific.VolumeRoot.DirectoryNodeListHead = NULL;

        AFSGlobalRoot->Specific.VolumeRoot.DirectoryNodeListTail = NULL;

        //
        // Check if we still have any Fcbs
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveAFSRoot Acquiring VolumeRoot FcbListLock lock %08lX EXCL %08lX\n",
                                                                   AFSGlobalRoot->Specific.VolumeRoot.FcbListLock,
                                                                   PsGetCurrentThread());

        AFSAcquireExcl( AFSGlobalRoot->Specific.VolumeRoot.FcbListLock,
                        TRUE);

        if( AFSGlobalRoot->Specific.VolumeRoot.FcbListHead != NULL)
        {

            pFcb = AFSGlobalRoot->Specific.VolumeRoot.FcbListHead;

            while( pFcb != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSRemoveAFSRoot Acquiring Fcb(2) lock %08lX EXCL %08lX\n",
                                                                           &pFcb->NPFcb->Resource,
                                                                           PsGetCurrentThread());

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                if( pFcb->OpenReferenceCount == 0)
                {

                    if( pFcb->DirEntry != NULL)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSRemoveAFSRoot Processing Fcb (%08lX) for %wZ\n",
                                                    pFcb,
                                                    &pFcb->DirEntry->DirectoryEntry.FileName);

                        pFcb->DirEntry->Fcb = NULL;

                        if( !BooleanFlagOn( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE))
                        {

                            AFSRemoveDirNodeFromParent( AFSGlobalRoot,
                                                        pFcb->DirEntry);
                        }

                        if( BooleanFlagOn( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_NAME_BUFFER))
                        {

                           ExFreePool( pFcb->DirEntry->DirectoryEntry.FileName.Buffer);
                        }

                        if( BooleanFlagOn( pFcb->DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER))
                        {

                            ExFreePool( pFcb->DirEntry->DirectoryEntry.TargetName.Buffer);
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
                    else
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSRemoveAFSRoot Processing Fcb (%08lX)\n",
                                                    pFcb);
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

            AFSGlobalRoot->Specific.VolumeRoot.FcbListHead = NULL;

            AFSGlobalRoot->Specific.VolumeRoot.FcbListTail = NULL;

            AFSGlobalRoot->Specific.VolumeRoot.FileIDTree.TreeHead = NULL;
        }

        AFSReleaseResource( AFSGlobalRoot->Specific.VolumeRoot.FcbListLock);

        //
        // Indicate the node is NOT initialized
        //

        ClearFlag( AFSGlobalRoot->Flags, AFS_FCB_DIRECTORY_ENUMERATED);

        AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

        AFSReleaseResource( AFSGlobalRoot->Specific.VolumeRoot.DirectoryNodeHdr.TreeLock);

        //
        // Tag the Fcbs as invalid so they are torn down
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveAFSRoot Acquiring RDR VolumeListLock lock %08lX EXCL %08lX\n",
                                                                           &pDevExt->Specific.RDR.VolumeListLock,
                                                                           PsGetCurrentThread());

        AFSAcquireExcl( &pDevExt->Specific.RDR.VolumeListLock,
                        TRUE);

        pVcb = pDevExt->Specific.RDR.VolumeListHead;

        while( pVcb != NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRemoveAFSRoot Acquiring VolumeRoot FcbListLock lock %08lX EXCL %08lX\n",
                                                                               pVcb->Specific.VolumeRoot.FcbListLock,
                                                                               PsGetCurrentThread());

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
                
                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSRemoveAFSRoot Acquiring Fcb(3) lock %08lX EXCL %08lX\n",
                                                                                       &pFcb->NPFcb->Resource,
                                                                                       PsGetCurrentThread());

                    AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                    TRUE);

                    SetFlag( pFcb->Flags, AFS_FCB_INVALID);

                    pNextFcb = (AFSFcb *)pFcb->ListEntry.fLink;

                    AFSReleaseResource( &pFcb->NPFcb->Resource);

                    if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
                    {

                        //
                        // Clear out the extents
                        //

                        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSRemoveAFSRoot Acquiring Fcb extents lock %08lX EXCL %08lX\n",
                                                                                           &pFcb->NPFcb->Specific.File.ExtentsResource,
                                                                                           PsGetCurrentThread());

                        AFSAcquireExcl( &pFcb->NPFcb->Specific.File.ExtentsResource, 
                                        TRUE);

                        pFcb->NPFcb->Specific.File.ExtentsRequestStatus = STATUS_CANCELLED;

                        KeSetEvent( &pFcb->NPFcb->Specific.File.ExtentsRequestComplete,
                                    0,
                                    FALSE);

                        AFSReleaseResource( &pFcb->NPFcb->Specific.File.ExtentsResource);
                                
                        //
                        // And get rid of them (not this involves waiting
                        // for any writes or reads to the cache to complete
                        //

                        AFSDbgLogMsg( AFS_SUBSYSTEM_EXTENT_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSRemoveAFSRoot Tearing down extents for %wZ\n",
                                          &pFcb->DirEntry->DirectoryEntry.FileName);        

                        (VOID) AFSTearDownFcbExtents( pFcb);
                    }

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
AFSInitRootFcb( IN AFSDirEntryCB *RootDirEntry,
                OUT AFSFcb **RootVcb)
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

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRootFcb Failed to allocate the root fcb\n");

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

        ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.DirectoryTreeLock);

        ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.FileIDTreeLock);

        ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.FcbListLock);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Header.PagingIoResource = &pNPFcb->PagingResource;

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

        pFcb->DirEntry = RootDirEntry;

        pFcb->DirEntry->DirectoryEntry.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

        //
        // Go get the volume information from the service
        //

        ASSERT( pFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY &&
                pFcb->DirEntry->DirectoryEntry.FileId.Vnode == 1);

        ntStatus = AFSRetrieveVolumeInformation( &pFcb->DirEntry->DirectoryEntry.FileId,
                                                 &pFcb->DirEntry->Type.Volume.VolumeInformation);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Insert the root of the volume into our volume list of entries
        //

        pFcb->TreeEntry.HashIndex = AFSCreateHighIndex( &pFcb->DirEntry->DirectoryEntry.FileId);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitRootFcb Acquiring RDR VolumeTree.TreeLock lock %08lX EXCL %08lX\n",
                                                           pDeviceExt->Specific.RDR.VolumeTree.TreeLock,
                                                           PsGetCurrentThread());

        AFSAcquireExcl( pDeviceExt->Specific.RDR.VolumeTree.TreeLock,
                        TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitRootFcb Adding volume FID %08lX:%08lX:%08lX:%08lX\n",
                                      pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Unique);        

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

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitRootFcb Acquiring RDR VolumeListLock lock %08lX EXCL %08lX\n",
                                                           &pDeviceExt->Specific.RDR.VolumeListLock,
                                                           PsGetCurrentThread());

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

        if( RootVcb != NULL)
        {

            *RootVcb = pFcb;
        }

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
// AFSInitRootForMountPoint started out as a function that would create 
// a volume root Fcb based upon a MountPoint but it is also used when
// a symbolicLink points to a volume that has never been seen before.
//
// It takes advantage of the fact that the Specific union for a
// MountPoint and a SymbolicLink are the same.
// 

NTSTATUS
AFSInitRootForMountPoint( IN AFSDirEnumEntryCB *MountPointDirEntry,
                          IN AFSVolumeInfoCB *VolumeInfoCB,
                          OUT AFSFcb **RootVcb)
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

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRootForMountPoint Failed to allocate the root fcb\n");

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

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRootForMountPoint Failed to allocate the non-paged fcb\n");

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
                      "AFSInitRootForMountPoint Acquiring Fcb lock %08lX EXCL %08lX\n",
                                                           &pNPFcb->Resource,
                                                           PsGetCurrentThread());

        AFSAcquireExcl( &pNPFcb->Resource,
                        TRUE);

        ExInitializeResourceLite( &pNPFcb->PagingResource);

        ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.DirectoryTreeLock);

        ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.FileIDTreeLock);

        ExInitializeResourceLite( &pNPFcb->Specific.VolumeRoot.FcbListLock);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Header.PagingIoResource = &pNPFcb->PagingResource;

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

            AFSReleaseResource( &pNPFcb->Resource);

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

            AFSReleaseResource( &pNPFcb->Resource);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Initialize the non-paged portion of the dire entry
        //

        AFSInitNonPagedDirEntry( pFcb->DirEntry);

        pFcb->DirEntry->Fcb = pFcb;

        //
        // Populate the dir entry for this root
        //

        pFcb->DirEntry->DirectoryEntry.FileId = MountPointDirEntry->TargetFileId;

        //
        // Make this a volume FID.  Don't assume the target is a volume root
        // because we might have been passed a symbolicLink.
        //

        pFcb->DirEntry->DirectoryEntry.FileId.Hash = 0;
        pFcb->DirEntry->DirectoryEntry.FileId.Vnode = 1;
        pFcb->DirEntry->DirectoryEntry.FileId.Unique = 1;

        pFcb->DirEntry->DirectoryEntry.CreationTime.QuadPart = MountPointDirEntry->CreationTime.QuadPart;
        pFcb->DirEntry->DirectoryEntry.LastWriteTime.QuadPart = MountPointDirEntry->CreationTime.QuadPart;
        pFcb->DirEntry->DirectoryEntry.LastAccessTime.QuadPart = MountPointDirEntry->CreationTime.QuadPart;

        pFcb->DirEntry->Type.Volume.VolumeInformation.VolumeCreationTime = MountPointDirEntry->CreationTime;

        pFcb->DirEntry->DirectoryEntry.FileType = AFS_FILE_TYPE_DIRECTORY;

        pFcb->DirEntry->DirectoryEntry.FileName.Length = sizeof( WCHAR);
        
        pFcb->DirEntry->DirectoryEntry.FileName.MaximumLength = pFcb->DirEntry->DirectoryEntry.FileName.Length;

        pFcb->DirEntry->DirectoryEntry.FileName.Buffer = (WCHAR *)((char *)pFcb->DirEntry + sizeof( AFSDirEntryCB));

        RtlCopyMemory( pFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                       L"\\",
                       sizeof( WCHAR));

        //
        // Copy in the volume information
        //

        RtlCopyMemory( &pFcb->DirEntry->Type.Volume.VolumeInformation,
                       VolumeInfoCB,
                       sizeof( AFSVolumeInfoCB));

        //
        // Insert the root of the volume into our volume list of entries
        //

        pFcb->TreeEntry.HashIndex = AFSCreateHighIndex( &pFcb->DirEntry->DirectoryEntry.FileId);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitRootForMountPoint Acquiring RDR VolumeTree.TreeLock lock %08lX EXCL %08lX\n",
                                                           pDeviceExt->Specific.RDR.VolumeTree.TreeLock,
                                                           PsGetCurrentThread());

        AFSAcquireExcl( pDeviceExt->Specific.RDR.VolumeTree.TreeLock,
                        TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitRootForMountPoint Adding volume FID %08lX:%08lX:%08lX:%08lX\n",
                                      pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pFcb->DirEntry->DirectoryEntry.FileId.Unique);        

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

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitRootForMountPoint Acquiring RDR VolumeListLock lock %08lX EXCL %08lX\n",
                                                           &pDeviceExt->Specific.RDR.VolumeListLock,
                                                           PsGetCurrentThread());

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

        if( RootVcb != NULL)
        {

            *RootVcb = pFcb;
        }

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

        ExDeleteResourceLite( &RootFcb->NPFcb->PagingResource);

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

    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSRemoveFcb Removing Fcb %08lX\n",
                                         Fcb);

    if( Fcb->Header.NodeTypeCode == AFS_FILE_FCB)
    {

        FsRtlUninitializeFileLock( &Fcb->Specific.File.FileLock);

        //
        // The resource we allocated
        //

        ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.ExtentsResource );

        ExDeleteResourceLite( &Fcb->NPFcb->Specific.File.DirtyExtentsListLock);

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

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitCcb Failed to allocate Ccb\n");

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

