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
    UNICODE_STRING    uniPathName, uniComponentName, uniRemainingPath, uniSearchName;
    ULONG             ulCRC = 0;
    AFSDirEntryCB    *pDirEntry = NULL;
    UNICODE_STRING    uniCurrentPath;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    BOOLEAN           bReleaseRoot = FALSE;
    ULONGLONG         ullIndex = 0;
    UNICODE_STRING    uniSysName;
    ULONG             ulSubstituteIndex = 0;
    BOOLEAN           bFoundComponent = FALSE;
    BOOLEAN           bSubstituteName = FALSE;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSLocateNameEntry (FO: %08lX) Processing full name %wZ\n",
                                                             FileObject,
                                                             FullPathName);

        RtlInitUnicodeString( &uniSysName,
                              L"*@SYS*");

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

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSLocateNameEntry Acquiring ParentFcb lock %08lX EXCL %08lX\n",
                                                              &pParentFcb->NPFcb->Resource,
                                                              PsGetCurrentThread());

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

            //
            // Check if the directory requires verification
            //

            if( BooleanFlagOn( pParentFcb->Flags, AFS_FCB_VERIFY))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %08lX) Verifying parent %wZ\n",
                                                                     FileObject,
                                                                     &pParentFcb->DirEntry->DirectoryEntry.FileName);

                ntStatus = AFSVerifyEntry( pParentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failed to verify parent %wZ Status %08lX\n",
                                                                         FileObject,
                                                                         &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                                                         ntStatus);

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    try_return( ntStatus);
                }
            }

            //
            // Ensure the parent node has been evaluated, if not then go do it now
            //

            if( BooleanFlagOn( pParentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %08lX) Evaluating parent %wZ\n",
                                                                     FileObject,
                                                                     &pParentFcb->DirEntry->DirectoryEntry.FileName);

                ntStatus = AFSEvaluateNode( pParentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failed to evaluate parent %wZ Status %08lX\n",
                                                                         FileObject,
                                                                         &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                                                         ntStatus);

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    try_return( ntStatus = STATUS_OBJECT_PATH_INVALID);
                }

                ClearFlag( pParentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
            }

            //
            // If this is a mount point or sym link then go get the real directory node
            //

            if( pParentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK ||
                pParentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
            {

                ASSERT( pParentFcb->Specific.Directory.DirectoryNodeListHead == NULL);

                //
                // Check if we have a target Fcb for this node
                //

                if( pParentFcb->Specific.SymbolicLink.TargetFcb == NULL)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %08lX) Building link target for parent %wZ\n",
                                                                         FileObject,
                                                                         &pParentFcb->DirEntry->DirectoryEntry.FileName);

                    //
                    // Go retrieve the target entry for this node
                    //

                    ntStatus = AFSQueueBuildTargetDirectory( pParentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to build link target for parent %wZ Status %08lX\n",
                                                                             FileObject,
                                                                             &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                                                             ntStatus);

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        break;
                    }
                }

                //
                // Swap out where we are in the chain
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSLocateNameEntry (FO: %08lX) Walking link chain for parent %wZ\n",
                                                                     FileObject,
                                                                     &pParentFcb->DirEntry->DirectoryEntry.FileName);

                ntStatus = AFSWalkTargetChain( pParentFcb,
                                               &pParentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failed to walk link chain for parent %wZ Status %08lX\n",
                                                                         FileObject,
                                                                         &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                                                         ntStatus);

                    //
                    // Had a failure while walking the chain
                    //

                    try_return( ntStatus);
                }

                ASSERT( pParentFcb != NULL);
            }
            else if( pParentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DFSLINK)
            {

                //
                // This is a DFS link so we need to update the file name and return STATUS_REPARSE to the
                // system for it to reevaluate it
                //

                ntStatus = AFSProcessDFSLink( pParentFcb,
                                              FileObject,
                                              &uniRemainingPath);

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                try_return( ntStatus);
            }

            //
            // If the parent is not initialized then do it now
            //

            if( pParentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB &&
                !BooleanFlagOn( pParentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %08lX) Enumerating parent %wZ\n",
                                                                     FileObject,
                                                                     &pParentFcb->DirEntry->DirectoryEntry.FileName);

                ntStatus = AFSEnumerateDirectory( pParentFcb,
                                                  &pParentFcb->Specific.Directory.DirectoryNodeHdr,
                                                  &pParentFcb->Specific.Directory.DirectoryNodeListHead,
                                                  &pParentFcb->Specific.Directory.DirectoryNodeListTail,
                                                  &pParentFcb->Specific.Directory.ShortNameTree,
                                                  TRUE);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failed to enumerate parent %wZ Status %08lX\n",
                                                                         FileObject,
                                                                         &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                                                         ntStatus);

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    break;
                }

                SetFlag( pParentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED);
            }
            else if( pParentFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
            {
                
                if( !BooleanFlagOn( pParentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %08lX) Enumerating root %wZ\n",
                                                                         FileObject,
                                                                         &pParentFcb->DirEntry->DirectoryEntry.FileName);

                    ntStatus = AFSEnumerateDirectory( pParentFcb,
                                                      &pParentFcb->Specific.VolumeRoot.DirectoryNodeHdr,
                                                      &pParentFcb->Specific.VolumeRoot.DirectoryNodeListHead,
                                                      &pParentFcb->Specific.VolumeRoot.DirectoryNodeListTail,
                                                      &pParentFcb->Specific.VolumeRoot.ShortNameTree,
                                                      TRUE);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to enumerate root %wZ Status %08lX\n",
                                                                             FileObject,
                                                                             &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                                                             ntStatus);

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

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %08lX) Initializing worker for root %wZ\n",
                                                                         FileObject,
                                                                         &pParentFcb->DirEntry->DirectoryEntry.FileName);

                    AFSInitVolumeWorker( pParentFcb);
                }
            }
            else if( pParentFcb->Header.NodeTypeCode == AFS_FILE_FCB)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSLocateNameEntry (FO: %08lX) Encountered file node %wZ in path processing\n",
                                                                     FileObject,
                                                                     &pParentFcb->DirEntry->DirectoryEntry.FileName);

                ntStatus = STATUS_OBJECT_NAME_INVALID;

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                break;
            }

            bFoundComponent = FALSE;

            bSubstituteName = FALSE;

            uniSearchName.Buffer = NULL;

            ulSubstituteIndex = 1;

            ntStatus = STATUS_SUCCESS;

            //
            // Get the next componet name
            //

            FsRtlDissectName( uniPathName,
                              &uniComponentName,
                              &uniRemainingPath);

            uniSearchName = uniComponentName;

            while( !bFoundComponent)
            {

                //
                // Generate the CRC on the node and perform a case sensitive lookup
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSLocateNameEntry (FO: %08lX) Searching for entry %wZ case sensitive\n",
                                                                     FileObject,
                                                                     &uniSearchName);

                ulCRC = AFSGenerateCRC( &uniSearchName,
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

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSLocateNameEntry (FO: %08lX) Searching for entry %wZ case insensitive\n",
                                                                         FileObject,
                                                                         &uniSearchName);

                    ulCRC = AFSGenerateCRC( &uniSearchName,
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

                        if( RtlIsNameLegalDOS8Dot3( &uniSearchName,
                                                    NULL,
                                                    NULL))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE_2,
                                          "AFSLocateNameEntry (FO: %08lX) Searching for entry %wZ short name\n",
                                                                                 FileObject,
                                                                                 &uniSearchName);

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

                            //
                            // If the component has a @SYS in it then we need to substitute
                            // in the name
                            //

                            if( FsRtlIsNameInExpression( &uniSysName,
                                                         &uniComponentName,
                                                         TRUE,
                                                         NULL))
                            {

                                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                              AFS_TRACE_LEVEL_VERBOSE_2,
                                              "AFSLocateNameEntry (FO: %08lX) Processing @SYS substitution for %wZ Index %08lX\n",
                                                                                     FileObject,
                                                                                     &uniComponentName,
                                                                                     ulSubstituteIndex);

                                //
                                // If we already substituted a name then free up the buffer before
                                // doing it again
                                //

                                if( bSubstituteName &&
                                    uniSearchName.Buffer != NULL)
                                {

                                    ExFreePool( uniSearchName.Buffer);
                                }

                                ntStatus = AFSSubstituteSysName( &uniComponentName,
                                                                 &uniSearchName,
                                                                 ulSubstituteIndex);

                                if( !NT_SUCCESS( ntStatus))
                                {

                                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                                  AFS_TRACE_LEVEL_ERROR,
                                                  "AFSLocateNameEntry (FO: %08lX) Failed to locate substitute string for %wZ Index %08lX Status %08lX\n",
                                                                                         FileObject,
                                                                                         &uniComponentName,
                                                                                         ulSubstituteIndex,
                                                                                         ntStatus);

                                    bSubstituteName = FALSE;

                                    if( ntStatus == STATUS_OBJECT_NAME_NOT_FOUND)
                                    {

                                        //
                                        // Pass back the parent node and the component name
                                        //

                                        *ParentFcb = pParentFcb;

                                        *Fcb = NULL;

                                        *ComponentName = uniComponentName;
                                    }

                                    break;
                                }

                                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                              AFS_TRACE_LEVEL_VERBOSE_2,
                                              "AFSLocateNameEntry (FO: %08lX) Located substitution %wZ for %wZ Index %08lX\n",
                                                                                     FileObject,
                                                                                     &uniSearchName,
                                                                                     &uniComponentName,
                                                                                     ulSubstituteIndex);

                                //
                                // Go reparse the name again
                                //

                                bSubstituteName = TRUE;

                                ulSubstituteIndex++; // For the next entry, if needed

                                continue;
                            }

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
                        }

                        //
                        // Node name not found so get out
                        //

                        break;
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
                        }

                        break;
                    }
                }
                else
                {

                    break;
                }
            }

            if( ntStatus != STATUS_SUCCESS)
            {

                //
                // If we substituted a name then free up the allocated buffer
                //

                if( bSubstituteName &&
                    uniSearchName.Buffer != NULL)
                {

                    ExFreePool( uniSearchName.Buffer);
                }

                break;
            }

            //
            // If we ended up substituting a name in the component then update
            // the full path and update the pointers
            //

            if( bSubstituteName)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSLocateNameEntry (FO: %08lX) Substituting %wZ into %wZ Index %08lX\n",
                                                                                     FileObject,
                                                                                     &uniSearchName,
                                                                                     &uniComponentName,
                                                                                     ulSubstituteIndex);

                ntStatus = AFSSubstituteNameInPath( FullPathName,
                                                    &uniComponentName,
                                                    &uniSearchName,
                                                    (BOOLEAN)(FileObject->RelatedFileObject != NULL));

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failure to substitute %wZ into %wZ Index %08lX Status %08lX\n",
                                                                                     FileObject,
                                                                                     &uniSearchName,
                                                                                     &uniComponentName,
                                                                                     ulSubstituteIndex,
                                                                                     ntStatus);

                    try_return( ntStatus);
                }
            }

            //
            // Update the search parameters
            //

            uniPathName = uniRemainingPath;
                
            *ComponentName = uniComponentName;

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

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %08lX) Locating Fcb for %wZ\n",
                                                              FileObject,
                                                              &pDirEntry->DirectoryEntry.FileName);

                //
                // See if we can first locate the Fcb since this could be a stand alone node
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry Acquiring VolumeRoot FileIDTree.TreeLock lock %08lX SHARED %08lX\n",
                                                              pParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                                              PsGetCurrentThread());

                AFSAcquireShared( pParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock, 
                                  TRUE);

                ullIndex = AFSCreateLowIndex( &pDirEntry->DirectoryEntry.FileId);

                AFSLocateHashEntry( pParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                    ullIndex,
                                    &pCurrentFcb);

                AFSReleaseResource( pParentFcb->RootFcb->Specific.VolumeRoot.FileIDTree.TreeLock);

                if( pCurrentFcb != NULL)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry Acquiring Fcb lock %08lX EXCL %08lX\n",
                                                              &pCurrentFcb->NPFcb->Resource,
                                                              PsGetCurrentThread());

                    AFSAcquireExcl( &pCurrentFcb->NPFcb->Resource,
                                    TRUE);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %08lX) Located stand alone Fcb %08lX for %wZ\n",
                                                                  FileObject,
                                                                  pCurrentFcb,
                                                                  &pDirEntry->DirectoryEntry.FileName);

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

                        if( BooleanFlagOn( pCurrentFcb->DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER))
                        {

                            ExFreePool( pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Buffer);
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

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %08lX) Initializing Fcb for %wZ\n",
                                                                  FileObject,
                                                                  &pDirEntry->DirectoryEntry.FileName);

                    ntStatus = AFSInitFcb( pParentFcb,
                                           pDirEntry,
                                           &pCurrentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to initialize Fcb for %wZ Status %08lX\n",
                                                                      FileObject,
                                                                      &pDirEntry->DirectoryEntry.FileName,
                                                                      ntStatus);

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        break;
                    }

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %08lX) Initialized Fcb %08lX for %wZ\n",
                                                                  FileObject,
                                                                  pCurrentFcb,
                                                                  &pDirEntry->DirectoryEntry.FileName);
                }
            }
            else
            {

                pCurrentFcb = pDirEntry->Fcb;
            
                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry Acquiring Fcb lock %08lX EXCL %08lX\n",
                                                              &pCurrentFcb->NPFcb->Resource,
                                                              PsGetCurrentThread());

                AFSAcquireExcl( &pCurrentFcb->NPFcb->Resource,
                                TRUE);
            }

            //
            // Ensure the node has been evaluated, if not then go do it now
            //

            if( BooleanFlagOn( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSLocateNameEntry (FO: %08lX) Evaluating fcb for %wZ\n",
                                                                  FileObject,
                                                                  &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                ntStatus = AFSEvaluateNode( pCurrentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failed to evaluate fcb for %wZ Status %08lX\n",
                                                                      FileObject,
                                                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                                      ntStatus);

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    try_return( ntStatus = STATUS_OBJECT_PATH_INVALID);
                }

                ClearFlag( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
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

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_WARNING,
                                  "AFSLocateNameEntry (FO: %08lX) Have remaining component %wZ\n",
                                                                  FileObject,
                                                                  &uniRemainingPath);
                }

                //
                // Ensure the node has been evaluated, if not then go do it now
                //

                if( BooleanFlagOn( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSLocateNameEntry (FO: %08lX) Evaluating fcb for %wZ\n",
                                                                  FileObject,
                                                                  &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                    ntStatus = AFSEvaluateNode( pCurrentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to evaluate fcb for %wZ Status %08lX\n",
                                                                      FileObject,
                                                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                                      ntStatus);

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                        try_return( ntStatus = STATUS_OBJECT_PATH_INVALID);
                    }

                    ClearFlag( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
                }

                if( BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_VERIFY))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %08lX) Verifying parent %wZ\n",
                                                                         FileObject,
                                                                         &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                    ntStatus = AFSVerifyEntry( pCurrentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to verify parent %wZ Status %08lX\n",
                                                                             FileObject,
                                                                             &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                                             ntStatus);

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                        try_return( ntStatus);
                    }
                }

                //
                // If this is a mount point or sym link then go get the real directory node
                //

                if( pCurrentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK ||
                    pCurrentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
                {

                    ASSERT( pCurrentFcb->Specific.Directory.DirectoryNodeListHead == NULL);

                    //
                    // Check if we have a target Fcb for this node
                    //

                    if( pCurrentFcb->Specific.SymbolicLink.TargetFcb == NULL)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE_2,
                                      "AFSLocateNameEntry (FO: %08lX) Building target for link %wZ\n",
                                                                      FileObject,
                                                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                        //
                        // Go retrieve the target entry for this node
                        //

                        ntStatus = AFSQueueBuildTargetDirectory( pCurrentFcb);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry (FO: %08lX) Failed to build target for link %wZ Status %08lX\n",
                                                                          FileObject,
                                                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                                          ntStatus);

                            AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                            break;
                        }
                    }

                    //
                    // Swap out where we are in the chain
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSLocateNameEntry (FO: %08lX) Walking link chain for %wZ\n",
                                                                      FileObject,
                                                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                    ntStatus = AFSWalkTargetChain( pCurrentFcb,
                                                   &pCurrentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE_2,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to walk link chain for %wZ Status %08lX\n",
                                                                          FileObject,
                                                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                                          ntStatus);

                        //
                        // Had a failure while walking the chain
                        //

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        try_return( ntStatus);
                    }

                    ASSERT( pCurrentFcb != NULL);

                    //
                    // If this is a root then the parent and current will be the same
                    //

                    if( pCurrentFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
                    {

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        pParentFcb = pCurrentFcb;
                    }
                }
                else if( pCurrentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DFSLINK)
                {

                    //
                    // This is a DFS link so we need to update the file name and return STATUS_REPARSE to the
                    // system for it to reevaluate it
                    //

                    ntStatus = AFSProcessDFSLink( pCurrentFcb,
                                                  FileObject,
                                                  &uniRemainingPath);

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    if( pCurrentFcb != pParentFcb)
                    {

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                    }

                    try_return( ntStatus);
                }

                //
                // If the entry is not initialized then do it now
                //

                if( pCurrentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB &&
                    !BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSLocateNameEntry (FO: %08lX) Enumerating directory %wZ\n",
                                                                          FileObject,
                                                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                    ntStatus = AFSEnumerateDirectory( pCurrentFcb,
                                                      &pCurrentFcb->Specific.Directory.DirectoryNodeHdr,
                                                      &pCurrentFcb->Specific.Directory.DirectoryNodeListHead,
                                                      &pCurrentFcb->Specific.Directory.DirectoryNodeListTail,
                                                      &pCurrentFcb->Specific.Directory.ShortNameTree,
                                                      TRUE);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to enumerate directory %wZ Status %08lX\n",
                                                                          FileObject,
                                                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                                          ntStatus);

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        if( pCurrentFcb != pParentFcb)
                        {

                            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                        }

                        break;
                    }

                    SetFlag( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED);
                }
                else if( pCurrentFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
                {
                    
                    if( !BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE_2,
                                      "AFSLocateNameEntry (FO: %08lX) Enuemrating root %wZ\n",
                                                                          FileObject,
                                                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                        ntStatus = AFSEnumerateDirectory( pCurrentFcb,
                                                          &pCurrentFcb->Specific.VolumeRoot.DirectoryNodeHdr,
                                                          &pCurrentFcb->Specific.VolumeRoot.DirectoryNodeListHead,
                                                          &pCurrentFcb->Specific.VolumeRoot.DirectoryNodeListTail,
                                                          &pCurrentFcb->Specific.VolumeRoot.ShortNameTree,
                                                          TRUE);

                        if( !NT_SUCCESS( ntStatus))
                        {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to enumerate root %wZ Status %08lX\n",
                                                                          FileObject,
                                                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                                          ntStatus);

                            AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                            if( pCurrentFcb != pParentFcb)
                            {

                                AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                            }

                            break;
                        }

                        SetFlag( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED);
                    }

                    //
                    // Initialize the volume worker if it has not already
                    //

                    if( pCurrentFcb->Specific.VolumeRoot.VolumeWorkerContext.WorkerThreadObject == NULL &&
                        !BooleanFlagOn( pCurrentFcb->Specific.VolumeRoot.VolumeWorkerContext.State, AFS_WORKER_INITIALIZED))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE_2,
                                      "AFSLocateNameEntry (FO: %08lX) Initializing worker for root %wZ\n",
                                                                          FileObject,
                                                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                        AFSInitVolumeWorker( pCurrentFcb);
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

            if( *ParentFcb != RootFcb)
            {

                AFSReleaseResource( &RootFcb->NPFcb->Resource);
            }
            else
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSLocateNameEntry Hit parse name condition\n");
            }
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

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSCreateDirEntry Creating dir entry in parent %wZ Component %wZ Attribs %08lX\n",
                                                                          &ParentDcb->DirEntry->DirectoryEntry.FileName,
                                                                          ComponentName,
                                                                          Attributes);

        //
        // OK, before inserting the node into the parent tree, issue
        // the request to the service for node creation
        // We will need to drop the lock on the parent node since the create
        // could cause a callback into the file system to invalidate it's cache
        //

        ntStatus = AFSNotifyFileCreate( ParentDcb,
                                        &liFileSize,
                                        Attributes,
                                        ComponentName,
                                        &pDirNode);

        //
        // If the returned status is STATUS_REPARSE then the entry exists
        // and we raced, get out.

        if( ntStatus == STATUS_REPARSE)
        {

            *DirEntry = pDirNode;

            try_return( ntStatus = STATUS_SUCCESS);
        }
        
        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSCreateDirEntry Failed to create dir entry in parent %wZ Component %wZ Attribs %08lX Status %08lX\n",
                                                                          &ParentDcb->DirEntry->DirectoryEntry.FileName,
                                                                          ComponentName,
                                                                          Attributes,
                                                                          ntStatus);

            try_return( ntStatus);
        }

        //
        // If we still ahve a valid parent then insert it into the tree
        // otherwise we will re-evaluate the node on next access
        //

        if( BooleanFlagOn( ParentDcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSCreateDirEntry Inserting dir entry in parent %wZ Component %wZ\n",
                                                                              &ParentDcb->DirEntry->DirectoryEntry.FileName,
                                                                              ComponentName);

            //
            // Insert the directory node
            //

            AFSInsertDirectoryNode( ParentDcb,
                                    pDirNode);
        }
        else
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSCreateDirEntry Created stand alone dir entry in parent %wZ Component %wZ\n",
                                                                              &ParentDcb->DirEntry->DirectoryEntry.FileName,
                                                                              ComponentName);

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

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInsertDirectoryNode Acquiring DirectoryNodeHdr.TreeLock lock %08lX EXCL %08lX\n",
                                                              ParentDcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                                              PsGetCurrentThread());

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

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInsertDirectoryNode Inserting entry %08lX %wZ into parent %08lX Status %08lX\n",
                                                                            DirEntry,
                                                                            &DirEntry->DirectoryEntry.FileName,
                                                                            ParentDcb);

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

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSDeleteDirEntry Deleting dir entry in parent %08lX Entry %08lX %wZ\n",
                                                                              ParentDcb,
                                                                              DirEntry,
                                                                              &DirEntry->DirectoryEntry.FileName);

        if( ParentDcb != NULL)
        {

            AFSRemoveDirNodeFromParent( ParentDcb,
                                        DirEntry);
        }

        //
        // Free up the name buffer if it was reallocated
        //

        if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_RELEASE_NAME_BUFFER))
        {

            ExFreePool( DirEntry->DirectoryEntry.FileName.Buffer);
        }

        if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER))
        {

            ExFreePool( DirEntry->DirectoryEntry.TargetName.Buffer);
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

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveDirNodeFromParent Removing DirEntry %08lX %wZ from Parent %08lX\n",
                                                              DirEntry,
                                                              &DirEntry->DirectoryEntry.FileName,
                                                              ParentDcb);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveDirNodeFromParent Acquiring DirectoryNodeHdr.TreeLock lock %08lX EXCL %08lX\n",
                                                              ParentDcb->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                                              PsGetCurrentThread());

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
        // And remove the entry from the enumeration list
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
    AFSFcb             *pParentFcb = NULL, *pCurrentFcb = NULL, *pRelatedFcb = NULL, *pTargetFcb = NULL;
    UNICODE_STRING      uniFullName, uniComponentName, uniRemainingPath;
    ULONG               ulCRC = 0;
    AFSDirEntryCB      *pDirEntry = NULL, *pShareDirEntry = NULL;
    UNICODE_STRING      uniCurrentPath;
    BOOLEAN             bAddTrailingSlash = FALSE;
    USHORT              usIndex = 0;
    UNICODE_STRING      uniRelatedFullName;
    BOOLEAN             bFreeRelatedName = FALSE;
    AFSCcb             *pCcb = NULL;
    ULONGLONG           ullIndex = 0;

    __Enter
    {

        *FreeNameString = FALSE;

        if( pIrpSp->FileObject->RelatedFileObject != NULL)
        {

            pRelatedFcb = (AFSFcb *)pIrpSp->FileObject->RelatedFileObject->FsContext;

            pCcb = (AFSCcb *)pIrpSp->FileObject->RelatedFileObject->FsContext2;

            uniFullName = pIrpSp->FileObject->FileName;

            ASSERT( pRelatedFcb != NULL);

            //
            // No wild cards in the name
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSParseName (%08lX) Relative open for %wZ component %wZ\n",
                                                           Irp,
                                                           &pRelatedFcb->DirEntry->DirectoryEntry.FileName,
                                                           &uniFullName);

            if( FsRtlDoesNameContainWildCards( &uniFullName))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%08lX) Component %wZ contains wild cards\n",
                                                               Irp,
                                                               &uniFullName);

                try_return( ntStatus = STATUS_OBJECT_NAME_INVALID);
            }

            *RootFcb = pRelatedFcb->RootFcb;

            *ParentFcb = pRelatedFcb;

            *FileName = pIrpSp->FileObject->FileName;

            //
            // Grab the root node exclusive before returning
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSParseName Acquiring RootFcb lock %08lX EXCL %08lX\n",
                                                              &(*RootFcb)->NPFcb->Resource,
                                                              PsGetCurrentThread());

            AFSAcquireExcl( &(*RootFcb)->NPFcb->Resource,
                            TRUE);

            if( BooleanFlagOn( (*RootFcb)->Flags, AFS_FCB_VOLUME_OFFLINE))
            {

                //
                // The volume has been taken off line so fail the access
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%08lX) Volume %08lX:%08lX OFFLINE\n",
                                                               Irp,
                                                               (*RootFcb)->DirEntry->DirectoryEntry.FileId.Cell,
                                                               (*RootFcb)->DirEntry->DirectoryEntry.FileId.Volume);


                AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                *RootFcb = NULL;

                *ParentFcb = NULL;

                try_return( ntStatus = STATUS_DEVICE_NOT_READY);
            }

            if( BooleanFlagOn( (*RootFcb)->Flags, AFS_FCB_VERIFY))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%08lX) Verifying root of volume %08lX:%08lX\n",
                                                               Irp,
                                                               (*RootFcb)->DirEntry->DirectoryEntry.FileId.Cell,
                                                               (*RootFcb)->DirEntry->DirectoryEntry.FileId.Volume);

                ntStatus = AFSVerifyEntry( *RootFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSParseName (%08lX) Failed verification of root Status %08lX\n",
                                                                   Irp,
                                                                   ntStatus);

                    AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                    *RootFcb = NULL;

                    *ParentFcb = NULL;

                    try_return( ntStatus);
                }
            }

            if( *ParentFcb != *RootFcb)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName Acquiring ParentFcb lock %08lX EXCL %08lX\n",
                                                              &(*ParentFcb)->NPFcb->Resource,
                                                              PsGetCurrentThread());

                AFSAcquireExcl( &(*ParentFcb)->NPFcb->Resource,
                                TRUE);

                //
                // Check if the root requires verification due to an invalidation call
                //

                if( BooleanFlagOn( (*ParentFcb)->Flags, AFS_FCB_VERIFY))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSParseName (%08lX) Verifying parent %wZ\n",
                                                                   Irp,
                                                                   &(*ParentFcb)->DirEntry->DirectoryEntry.FileName);

                    ntStatus = AFSVerifyEntry( *ParentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSParseName (%08lX) Failed verification of parent %wZ Status %08lX\n",
                                                                       Irp,
                                                                       &(*ParentFcb)->DirEntry->DirectoryEntry.FileName,
                                                                       ntStatus);

                        AFSReleaseResource( &(*ParentFcb)->NPFcb->Resource);

                        AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                        *RootFcb = NULL;

                        *ParentFcb = NULL;
                    }
                }

                AFSReleaseResource( &(*ParentFcb)->NPFcb->Resource);
            }

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

            if( uniFullName.Buffer == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%08lX) Failed to allocate full name buffer\n",
                                                                       Irp);

                AFSReleaseResource( &(*ParentFcb)->NPFcb->Resource);

                AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                *RootFcb = NULL;

                *ParentFcb = NULL;

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

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

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSParseName (%08lX) Returning full name %wZ\n",
                                                                   Irp,
                                                                   &uniFullName);

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

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSParseName (%08lX) Processing full name %wZ\n",
                                                 Irp,
                                                 &uniFullName);

        //
        // No wild cards in the name
        //

        if( FsRtlDoesNameContainWildCards( &uniFullName) ||
            uniFullName.Length < AFSServerName.Length)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSParseName (%08lX) Name %wZ contains wild cards or too short\n",
                                                 Irp,
                                                 &uniFullName);

            try_return( ntStatus = STATUS_OBJECT_NAME_INVALID);
        }

        if( uniFullName.Buffer[ (uniFullName.Length/sizeof( WCHAR)) - 1] == L'\\')
        {

            uniFullName.Length -= sizeof( WCHAR);
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

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSParseName (%08lX) Name %wZ does not have server name\n",
                                                 Irp,
                                                 &uniComponentName);

            try_return( ntStatus = STATUS_BAD_NETWORK_NAME);
        }

        //
        // Be sure we are online and ready to go
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSParseName Acquiring GlobalRoot lock %08lX SHARED %08lX\n",
                                                              &AFSGlobalRoot->NPFcb->Resource,
                                                              PsGetCurrentThread());

        AFSAcquireShared( &AFSGlobalRoot->NPFcb->Resource,
                          TRUE);

        if( BooleanFlagOn( AFSGlobalRoot->Flags, AFS_FCB_VOLUME_OFFLINE))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSParseName (%08lX) Global root OFFLINE\n",
                                                 Irp);

            AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

            try_return( ntStatus = STATUS_TOO_LATE);
        }

        //
        // Check for the \\Server access and return it as though it where \\Server\Globalroot
        //

        if( uniRemainingPath.Buffer == NULL ||
            ( uniRemainingPath.Length == sizeof( WCHAR) &&
              uniRemainingPath.Buffer[ 0] == L'\\'))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSParseName (%08lX) Returning global root access\n",
                                                 Irp);

            AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

            *RootFcb = NULL;

            FileName->Length = 0;
            FileName->MaximumLength = 0;
            FileName->Buffer = NULL;

            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // Get the 'share' name
        //

        FsRtlDissectName( uniFullName,
                          &uniComponentName,
                          &uniRemainingPath);

        //
        // If this is the ALL access then perform some additional processing
        //

        if( RtlCompareUnicodeString( &uniComponentName,
                                     &AFSGlobalRootName,
                                     TRUE) == 0)
        {

            //
            // If there is nothing else then get out
            //

            if( uniRemainingPath.Buffer == NULL ||
                uniRemainingPath.Length == 0 ||
                ( uniRemainingPath.Length == sizeof( WCHAR) &&
                  uniRemainingPath.Buffer[ 0] == L'\\'))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSParseName (%08lX) Returning global root access\n",
                                                     Irp);

                AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

                *RootFcb = NULL;

                FileName->Length = 0;
                FileName->MaximumLength = 0;
                FileName->Buffer = NULL;

                try_return( ntStatus = STATUS_SUCCESS);
            }

            //
            // Process the name again to strip off the ALL portion
            //

            uniFullName = uniRemainingPath;

            FsRtlDissectName( uniFullName,
                              &uniComponentName,
                              &uniRemainingPath);

            //
            // Check for the PIOCtl name
            //

            if( RtlCompareUnicodeString( &uniComponentName,
                                         &AFSPIOCtlName,
                                         TRUE) == 0)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSParseName (%08lX) Returning root PIOCtl access\n",
                                                     Irp);

                AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

                //
                // Return NULL for all the information but with a success status
                //

                *RootFcb = NULL;

                *FileName = AFSPIOCtlName;

                try_return( ntStatus = STATUS_SUCCESS);
            }
        }
        else if( AFSIsSpecialShareName( &uniComponentName)) 
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSParseName (%08lX) Returning root share name %wZ access\n",
                                                     Irp,
                                                     &uniComponentName);

            AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

            //
            // Return NULL for all the information but with a success status
            //

            *RootFcb = NULL;

            *FileName = uniComponentName;

            try_return( ntStatus = STATUS_SUCCESS);
        }

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
        // Generate a CRC on the component and look it up in the name tree
        //

        ulCRC = AFSGenerateCRC( &uniComponentName,
                                FALSE);

        //
        // Grab our tree lock shared
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSParseName Acquiring GlobalRoot DirectoryNodeHdr.TreeLock lock %08lX SHARED %08lX\n",
                                                              AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                                              PsGetCurrentThread());

        AFSAcquireShared( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                          TRUE);

        AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

        //
        // Locate the dir entry for this node
        //

        ntStatus = AFSLocateCaseSensitiveDirEntry( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
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

            ntStatus = AFSLocateCaseInsensitiveDirEntry( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                                         ulCRC,
                                                         &pShareDirEntry);

            if( pShareDirEntry == NULL ||
                !NT_SUCCESS( ntStatus))
            {

                //
                // We need to drop the lock on the global root and re-acquire it excl since we may insert a new
                // entry
                //

                AFSReleaseResource( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock);

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName Acquiring GlobalRoot DirectoryNodeHdr.TreeLock lock %08lX EXCL %08lX\n",
                                                                      AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                                                      PsGetCurrentThread());

                AFSAcquireExcl( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                TRUE);

                //
                // Since we dropped the lock, check again of we raced with someone else
                // who inserted the entry
                //

                ulCRC = AFSGenerateCRC( &uniComponentName,
                                        FALSE);

                ntStatus = AFSLocateCaseSensitiveDirEntry( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
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

                    ntStatus = AFSLocateCaseInsensitiveDirEntry( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
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

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_WARNING,
                                          "AFSParseName (%08lX) Cell name %wZ not found\n",
                                                                 Irp,
                                                                 &uniComponentName);

                            AFSReleaseResource( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock);

                            try_return( ntStatus);
                        }
                    }
                }
            }
        }

        //
        // Grab the dir node exclusive while we determine the state
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSParseName Acquiring ShareEntry DirNode lock %08lX EXCL %08lX\n",
                                                              &pShareDirEntry->NPDirNode->Lock,
                                                              PsGetCurrentThread());

        AFSAcquireExcl( &pShareDirEntry->NPDirNode->Lock,
                        TRUE);

        AFSReleaseResource( AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock);

        //
        // If the root node for this entry is NULL, then we need to initialize 
        // the volume node information
        //

        if( pShareDirEntry->Fcb == NULL)
        {

            //
            // If this is a volume FID then lookup the fcb in the volume
            // tree otherwise look it up in the fcb tree
            //

            if( AFSIsVolumeFID( &pShareDirEntry->DirectoryEntry.FileId))
            {

                AFSAcquireShared( &pDeviceExt->Specific.RDR.VolumeTreeLock, TRUE);

                ullIndex = AFSCreateHighIndex( &pShareDirEntry->DirectoryEntry.FileId);

                ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.RDR.VolumeTree.TreeHead,
                                               ullIndex,
                                               &pShareDirEntry->Fcb);

                if( pShareDirEntry->Fcb != NULL)
                {

                    AFSAcquireExcl( &pShareDirEntry->Fcb->NPFcb->Resource,
                                    TRUE);
                }

                AFSReleaseResource( &pDeviceExt->Specific.RDR.VolumeTreeLock);

                if( pShareDirEntry->Fcb == NULL)
                {

                    ntStatus = AFSInitRootFcb( pShareDirEntry,
                                               &pShareDirEntry->Fcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSParseName (%08lX) Failed to initialize root fcb for cell name %wZ\n",
                                                                 Irp,
                                                                 &uniComponentName);

                        AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);

                        try_return( ntStatus);
                    }
                }
            }
            else
            {

                //
                // Check to see if the Fcb already exists for this entry
                //

                AFSAcquireShared( AFSGlobalRoot->Specific.VolumeRoot.FileIDTree.TreeLock, 
                                  TRUE);

                ullIndex = AFSCreateLowIndex( &pShareDirEntry->DirectoryEntry.FileId);

                AFSLocateHashEntry( AFSGlobalRoot->Specific.VolumeRoot.FileIDTree.TreeHead,
                                    ullIndex,
                                    &pShareDirEntry->Fcb);

                if( pShareDirEntry->Fcb != NULL)
                {

                    AFSAcquireExcl( &pShareDirEntry->Fcb->NPFcb->Resource,
                                    TRUE);
                }

                AFSReleaseResource( AFSGlobalRoot->Specific.VolumeRoot.FileIDTree.TreeLock);

                if( pShareDirEntry->Fcb == NULL)
                {

                    ntStatus = AFSInitFcb( AFSGlobalRoot,
                                           pShareDirEntry,
                                           NULL);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSParseName (%08lX) Failed to initialize fcb for cell name %wZ\n",
                                                                 Irp,
                                                                 &uniComponentName);

                        AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);

                        try_return( ntStatus);
                    }
                }
            }

            InterlockedIncrement( &pShareDirEntry->Fcb->OpenReferenceCount);
        }
        else
        {

            //
            // Grab the root node exclusive before returning
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSParseName Acquiring ShareEntry Fcb lock %08lX EXCL %08lX\n",
                                                              &pShareDirEntry->Fcb->NPFcb->Resource,
                                                              PsGetCurrentThread());

            AFSAcquireExcl( &pShareDirEntry->Fcb->NPFcb->Resource,
                            TRUE);
        }

        //
        // Drop the volume lock
        //

        AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);

        //
        // If this node has become invalid then fail the request now
        //

        if( BooleanFlagOn( pShareDirEntry->Fcb->Flags, AFS_FCB_INVALID))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSParseName (%08lX) Fcb for cell name %wZ invalid\n",
                                                         Irp,
                                                         &uniComponentName);

            //
            // The symlink or share entry has been deleted
            //

            AFSReleaseResource( &pShareDirEntry->Fcb->NPFcb->Resource);

            try_return( ntStatus = STATUS_OBJECT_PATH_INVALID);
        }

        //
        // Ensure the root node has been evaluated, if not then go do it now
        //

        if( BooleanFlagOn( pShareDirEntry->Fcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSParseName (%08lX) Evaluating cell %wZ\n",
                                                         Irp,
                                                         &uniComponentName);

            ntStatus = AFSEvaluateNode( pShareDirEntry->Fcb);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%08lX) Failed to evaluate cell %wZ Status %08lX\n",
                                                             Irp,
                                                             &uniComponentName,
                                                             ntStatus);

                AFSReleaseResource( &pShareDirEntry->Fcb->NPFcb->Resource);

                try_return( ntStatus);
            }

            ClearFlag( pShareDirEntry->Fcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
        }

        //
        // Be sure we return a root or a directory
        //

        if( pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK ||
            pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
        {

            //
            // Check if we have a target Fcb for this node
            //

            if( pShareDirEntry->Fcb->Specific.SymbolicLink.TargetFcb == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%08lX) Building target for link %wZ\n",
                                                             Irp,
                                                             &pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileName);

                //
                // Go retrieve the target entry for this node
                //

                ntStatus = AFSQueueBuildTargetDirectory( pShareDirEntry->Fcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSParseName (%08lX) Failed to build target for link %wZ Status %08lX\n",
                                                                 Irp,
                                                                 &pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileName,
                                                                 ntStatus);

                    AFSReleaseResource( &pShareDirEntry->Fcb->NPFcb->Resource);

                    try_return( ntStatus);
                }
            }

            //
            // Swap out where we are in the chain
            //

            ntStatus = AFSWalkTargetChain( pShareDirEntry->Fcb,
                                           &pTargetFcb);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%08lX) Failed to walk target for link %wZ\n",
                                                             Irp,
                                                             &pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileName);

                //
                // Had a failure while walking the chain
                //

                try_return( ntStatus);
            }

            //
            // Resoufces are acquired above
            //

            *RootFcb = pTargetFcb;

            *ParentFcb = pTargetFcb;
        }
        else if( pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DFSLINK)
        {

            //
            // This is a DFS link so we need to update the file name and return STATUS_REPARSE to the
            // system for it to reevaluate the link
            //

            ntStatus = AFSProcessDFSLink( pShareDirEntry->Fcb,
                                          pIrpSp->FileObject,
                                          &uniRemainingPath);

            AFSReleaseResource( &pShareDirEntry->Fcb->NPFcb->Resource);

            try_return( ntStatus);
        }
        else
        {

            *RootFcb = pShareDirEntry->Fcb;

            *ParentFcb = pShareDirEntry->Fcb;
           
            if( BooleanFlagOn( (*RootFcb)->Flags, AFS_FCB_VOLUME_OFFLINE))
            {

                //
                // The volume has been taken off line so fail the access
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%08lX) Root volume OFFLINE for %wZ\n",
                                                             Irp,
                                                             &pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileName);

                AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                *RootFcb = NULL;

                *ParentFcb = NULL;

                try_return( ntStatus = STATUS_DEVICE_NOT_READY);
            }

            //
            // Check if the root requires verification due to an invalidation call
            //

            if( BooleanFlagOn( (*RootFcb)->Flags, AFS_FCB_VERIFY))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%08lX) Verifying root volume %wZ\n",
                                                             Irp,
                                                             &pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileName);

                ntStatus = AFSVerifyEntry( *RootFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSParseName (%08lX) Failed to verify root volume %wZ Status %08lX\n",
                                                                 Irp,
                                                                 &pShareDirEntry->Fcb->DirEntry->DirectoryEntry.FileName,
                                                                 ntStatus);

                    AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                    *RootFcb = NULL;

                    *ParentFcb = NULL;

                    try_return( ntStatus);
                }
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
    AFSDirHdr *pDirHdr = &AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr;
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

            try_return( ntStatus = STATUS_NO_SUCH_FILE);
        }

        RtlInitUnicodeString( &uniName,
                              L"wkssvc");

        if( RtlCompareUnicodeString( &uniName,
                                     CellName,
                                     TRUE) == 0)
        {

            try_return( ntStatus = STATUS_NO_SUCH_FILE);
        }

        RtlInitUnicodeString( &uniName,
                              L"srvsvc");

        if( RtlCompareUnicodeString( &uniName,
                                     CellName,
                                     TRUE) == 0)
        {

            try_return( ntStatus = STATUS_NO_SUCH_FILE);
        }

        RtlInitUnicodeString( &uniName,
                              L"PIPE");

        if( RtlCompareUnicodeString( &uniName,
                                     CellName,
                                     TRUE) == 0)
        {

            try_return( ntStatus = STATUS_NO_SUCH_FILE);
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

        uniDirName = *CellName;

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

        if( AFSGlobalRoot->Specific.Directory.DirectoryNodeListHead == NULL)
        {

            AFSGlobalRoot->Specific.Directory.DirectoryNodeListHead = pDirNode;
        }
        else
        {

            AFSGlobalRoot->Specific.Directory.DirectoryNodeListTail->ListEntry.fLink = pDirNode;

            pDirNode->ListEntry.bLink = AFSGlobalRoot->Specific.Directory.DirectoryNodeListTail;
        }

        AFSGlobalRoot->Specific.Directory.DirectoryNodeListTail = pDirNode;

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
AFSBuildTargetDirectory( IN ULONGLONG ProcessID,
                         IN AFSFcb *Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    AFSFcb *pTargetFcb = NULL, *pVcb = NULL, *pCurrentFcb = NULL;
    AFSFileID stTargetFileID;
    AFSDirEnumEntry *pDirEntry = NULL;
    AFSDirEntryCB *pDirNode = NULL;
    UNICODE_STRING uniDirName, uniTargetName;
    ULONGLONG       ullIndex = 0;

    __Enter
    {

        //
        // Loop on each entry, building the chain until we encounter the final target
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSBuildTargetDirectory Building target directory for %wZ PID %I64X\n",
                                                    &Fcb->DirEntry->DirectoryEntry.FileName,
                                                    ProcessID);

        pCurrentFcb = Fcb;

        while( TRUE)
        {
    
            //
            // If there is already a target Fcb then process it
            //

            if( pCurrentFcb->Specific.SymbolicLink.TargetFcb != NULL)
            {

                pTargetFcb = pCurrentFcb->Specific.SymbolicLink.TargetFcb;

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSBuildTargetDirectory Acquiring Fcb lock %08lX EXCL %08lX\n",
                                                              &pTargetFcb->NPFcb->Resource,
                                                              PsGetCurrentThread());

                AFSAcquireExcl( &pTargetFcb->NPFcb->Resource,
                                TRUE);

                if( pCurrentFcb != Fcb)
                {

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                }

                //
                // If this is a final node then get out
                //

                if( AFSIsFinalNode( pTargetFcb))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSBuildTargetDirectory Have final target for %wZ Target %wZ\n",
                                                                &Fcb->DirEntry->DirectoryEntry.FileName,
                                                                &pTargetFcb->DirEntry->DirectoryEntry.FileName);

                    AFSReleaseResource( &pTargetFcb->NPFcb->Resource);

                    break;
                }

                pCurrentFcb = pTargetFcb;

                continue;
            }

            if( pCurrentFcb->DirEntry->DirectoryEntry.TargetFileId.Hash == 0)
            {

                //
                // Go evaluate the current target to get the target fid
                //

                stTargetFileID = pCurrentFcb->DirEntry->DirectoryEntry.FileId;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSBuildTargetDirectory Evaluating target %wZ\n",
                                                                &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                ntStatus = AFSEvaluateTargetByID( &stTargetFileID,
                                                  ProcessID,
                                                  FALSE,
                                                  &pDirEntry);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSBuildTargetDirectory Failed to evaluate target %wZ Status %08lX\n",
                                                                    &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                                    ntStatus);

                    if( pCurrentFcb != Fcb)
                    {

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                    }

                    break;
                }

                if( pDirEntry->TargetFileId.Hash == 0)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSBuildTargetDirectory Target %wZ service returned zero FID\n",
                                                                    &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                    if( pCurrentFcb != Fcb)
                    {

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                    }

                    ntStatus = STATUS_ACCESS_DENIED;

                    break;
                }

                pCurrentFcb->DirEntry->DirectoryEntry.TargetFileId = pDirEntry->TargetFileId;

                ExFreePool( pDirEntry);
            }

            stTargetFileID = pCurrentFcb->DirEntry->DirectoryEntry.TargetFileId;

            ASSERT( stTargetFileID.Hash != 0);

            //
            // Try to locate this FID. First the volume then the 
            // entry itself
            //

            ullIndex = AFSCreateHighIndex( &stTargetFileID);

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSBuildTargetDirectory Acquiring RDR VolumeTreeLock lock %08lX EXCL %08lX\n",
                                                              &pDevExt->Specific.RDR.VolumeTreeLock,
                                                              PsGetCurrentThread());

            AFSAcquireShared( &pDevExt->Specific.RDR.VolumeTreeLock,
                              TRUE);

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSBuildTargetDirectory Locating volume for target %I64X\n",
                                                                    ullIndex);

            ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                           ullIndex,
                                           &pVcb);

            if( pVcb == NULL)
            {

                AFSDirEnumEntryCB   stDirEnumEntry;
                AFSVolumeInfoCB     stVolumeInfo;

                AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

                //
                // Go get the volume information while we no longer have the lock. Trying to
                // call the service while holding the tree lock can produce a deadlock due to
                // invalidation
                //

                ntStatus = AFSRetrieveVolumeInformation( &stTargetFileID,
                                                         &stVolumeInfo);

                if( !NT_SUCCESS( ntStatus))
                {

                    if( pCurrentFcb != Fcb)
                    {

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                    }

                    break;
                }

                AFSAcquireExcl( &pDevExt->Specific.RDR.VolumeTreeLock,
                                TRUE);

                ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                               ullIndex,
                                               &pVcb);

                if( pVcb == NULL)
                {

                    RtlCopyMemory( &stDirEnumEntry,
                                   pCurrentFcb->DirEntry,
                                   sizeof( AFSDirEnumEntryCB));
                                    
                    stDirEnumEntry.TargetFileId = stTargetFileID;

                    //
                    // Go init the root of the volume
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSBuildTargetDirectory Initializing root for %wZ\n",
                                                                        &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                    AFSInitRootForMountPoint( &stDirEnumEntry,
                                              &stVolumeInfo,
                                              &pVcb);

                    if( pVcb != NULL)
                    {

                        if( pVcb->Specific.VolumeRoot.VolumeWorkerContext.WorkerThreadObject == NULL &&
                            !BooleanFlagOn( pVcb->Specific.VolumeRoot.VolumeWorkerContext.State, AFS_WORKER_INITIALIZED))
                        {

                            AFSInitVolumeWorker( pVcb);
                        }

                        AFSReleaseResource( &pVcb->NPFcb->Resource);
                    }
                }
            }

            if( pVcb != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSBuildTargetDirectory Acquiring VolumeRoot FileIDTree.TreeLock lock %08lX EXCL %08lX\n",
                                                              pVcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                                              PsGetCurrentThread());

                AFSAcquireShared( pVcb->Specific.VolumeRoot.FileIDTree.TreeLock,
                                  TRUE);
            }

            AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

            if( pVcb == NULL) 
            {

                if( pCurrentFcb != Fcb)
                {

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                }

                ntStatus = STATUS_UNSUCCESSFUL;

                break;
            }

            //
            // Check if this is a volume FID which could be the target of a 
            // mount point. If so, then we are done
            //

            if( AFSIsVolumeFID( &stTargetFileID))
            {

                //
                // It is pointing at a volume root so set the target and get out
                //

                InterlockedIncrement( &pVcb->OpenReferenceCount);

                pCurrentFcb->Specific.SymbolicLink.TargetFcb = pVcb;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSBuildTargetDirectory Evaluated target of %wZ as root volume\n",
                                                                    &pCurrentFcb->DirEntry->DirectoryEntry.FileName);

                if( pCurrentFcb != Fcb)
                {

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                }

                AFSReleaseResource( pVcb->Specific.VolumeRoot.FileIDTree.TreeLock);

                break;
            }

            //
            // We have the volume ndoe so now search for the entry itself
            //

            ullIndex = AFSCreateLowIndex( &stTargetFileID);

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSBuildTargetDirectory Locating file %I64X in volume\n",
                                                                    ullIndex);

            ntStatus = AFSLocateHashEntry( pVcb->Specific.VolumeRoot.FileIDTree.TreeHead,
                                           ullIndex,
                                           &pTargetFcb);

            if( NT_SUCCESS( ntStatus) &&
                pTargetFcb != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSBuildTargetDirectory Acquiring Fcb lock %08lX SHARED %08lX\n",
                                                              &pTargetFcb->NPFcb->Resource,
                                                              PsGetCurrentThread());

                AFSAcquireShared( &pTargetFcb->NPFcb->Resource,
                                  TRUE);                
            }

            AFSReleaseResource( pVcb->Specific.VolumeRoot.FileIDTree.TreeLock);

            //
            // If we don't have a target fcb then go build one
            //

            if( pTargetFcb == NULL)
            {

                //
                // Need to evaluate the entry to get a direnum cb
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSBuildTargetDirectory Evaluating target of %I64X\n",
                                                                        ullIndex);

                ntStatus = AFSEvaluateTargetByID( &stTargetFileID,
                                                  ProcessID,
                                                  FALSE,
                                                  &pDirEntry);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSBuildTargetDirectory Failed to evaluate target of %I64X Status %08lX\n",
                                                                            ullIndex,
                                                                            ntStatus);

                    if( pCurrentFcb != Fcb)
                    {

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                    }

                    break;
                }

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
                   
                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSBuildTargetDirectory Failed to initialize dir entry for %wZ\n",
                                                                            &uniDirName);

                    ExFreePool( pDirEntry);

                    if( pCurrentFcb != Fcb)
                    {

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                    }

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                //
                // Go initialize the Fcb
                //

                ntStatus = AFSInitFcb( pVcb,
                                       pDirNode,
                                       &pTargetFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSBuildTargetDirectory Failed to initialize fcb for dir entry %wZ Status %08lX\n",
                                                                            &uniDirName,
                                                                            ntStatus);

                    ExFreePool( pDirEntry);

                    AFSDeleteDirEntry( NULL,
                                       pDirNode);

                    if( pCurrentFcb != Fcb)
                    {

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                    }

                    try_return( ntStatus);
                }

                ExFreePool( pDirEntry);

                InterlockedIncrement( &pTargetFcb->OpenReferenceCount);

                //
                // Tag this Fcb as a 'stand alone Fcb since it is not referencing a parent Fcb
                // through the dir entry list
                //

                SetFlag( pTargetFcb->Flags, AFS_FCB_STANDALONE_NODE);

                //
                // Mark this directory node as requiring freeing since it is not in a parent list
                //

                SetFlag( pDirNode->Flags, AFS_DIR_RELEASE_DIRECTORY_NODE);
            }

            //
            // If the target Fcb is either a directory, root or a file
            // then we are done
            //

            if( AFSIsFinalNode( pTargetFcb))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSBuildTargetDirectory Located final target %wZ\n",
                                                                            &pTargetFcb->DirEntry->DirectoryEntry.FileName);

                InterlockedIncrement( &pTargetFcb->OpenReferenceCount);

                pCurrentFcb->Specific.SymbolicLink.TargetFcb = pTargetFcb;

                AFSReleaseResource( &pTargetFcb->NPFcb->Resource);

                if( pCurrentFcb != Fcb)
                {

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                }

                break;
            }

            //
            // Update the current Fcbs target
            //

            pCurrentFcb->Specific.SymbolicLink.TargetFcb = pTargetFcb;

            //
            // If this is not the initial Fcb we are currently on then
            // drop the lock
            //

            if( pCurrentFcb != Fcb)
            {

                AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
            }

            pCurrentFcb = pTargetFcb;

            pTargetFcb = NULL;
        }
                    
try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSProcessDFSLink( IN AFSFcb *Fcb,
                   IN PFILE_OBJECT FileObject,
                   IN UNICODE_STRING *RemainingPath)
{

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    UNICODE_STRING uniReparseName;
    UNICODE_STRING uniMUPDeviceName;

    __Enter
    {

        //
        // Build up the name to reparse
        // 

        RtlInitUnicodeString( &uniMUPDeviceName,
                              L"\\Device\\MUP\\");

        uniReparseName.Length = 0;
        uniReparseName.Buffer = NULL;

        //
        // Be sure we have a target name
        //

        if( Fcb->DirEntry->DirectoryEntry.TargetName.Length == 0)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        uniReparseName.MaximumLength = uniMUPDeviceName.Length +
                                       Fcb->DirEntry->DirectoryEntry.TargetName.Length +
                                       sizeof( WCHAR);

        if( RemainingPath != NULL &&
            RemainingPath->Length > 0)
        {

            uniReparseName.MaximumLength += RemainingPath->Length;
        }

        //
        // Allocate the reparse buffer
        //

        uniReparseName.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                                uniReparseName.MaximumLength,
                                                                AFS_REPARSE_NAME_TAG);

        if( uniReparseName.Buffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Start building the name
        //

        RtlCopyMemory( uniReparseName.Buffer,
                       uniMUPDeviceName.Buffer,
                       uniMUPDeviceName.Length);

        uniReparseName.Length = uniMUPDeviceName.Length;
                       
        RtlCopyMemory( &uniReparseName.Buffer[ uniReparseName.Length],
                       Fcb->DirEntry->DirectoryEntry.TargetName.Buffer,
                       Fcb->DirEntry->DirectoryEntry.TargetName.Length);

        uniReparseName.Length += Fcb->DirEntry->DirectoryEntry.TargetName.Length;

        if( RemainingPath != NULL &&
            RemainingPath->Length > 0)
        {

            if( uniReparseName.Buffer[ uniReparseName.Length] != L'\\' &&
                RemainingPath->Buffer[ 0] != L'\\')
            {

                uniReparseName.Buffer[ uniReparseName.Length] = L'\\';

                uniReparseName.Length += sizeof( WCHAR);
            }

            RtlCopyMemory( &uniReparseName.Buffer[ uniReparseName.Length],
                           RemainingPath->Buffer,
                           RemainingPath->Length);

            uniReparseName.Length += RemainingPath->Length;
        }

        //
        // Update the name in the file object
        //

        if( FileObject->FileName.Buffer != NULL)
        {

            ExFreePool( FileObject->FileName.Buffer);
        }

        FileObject->FileName = uniReparseName;

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessDFSLink Reparsing access to Fcb %wZ to %wZ\n",
                              &Fcb->DirEntry->DirectoryEntry.FileName,
                              &uniReparseName);

        //
        // Return status reparse ...
        //

        ntStatus = STATUS_REPARSE;

try_exit:

        NOTHING;
    }

    return ntStatus;
}