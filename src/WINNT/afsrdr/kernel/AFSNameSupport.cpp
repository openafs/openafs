/*
 * Copyright (c) 2008, 2009 Kernel Drivers, LLC.
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
AFSLocateNameEntry( IN ULONGLONG ProcessID,
                    IN PFILE_OBJECT FileObject,
                    IN UNICODE_STRING *RootPathName,
                    IN UNICODE_STRING *ParsedPathName,
                    IN AFSNameArrayHdr *NameArray,
                    IN ULONG Flags,
                    IN OUT AFSFcb **ParentFcb,
                    OUT AFSFcb **Fcb,
                    OUT PUNICODE_STRING ComponentName)
{

    NTSTATUS          ntStatus = STATUS_SUCCESS;
    AFSFcb           *pParentFcb = NULL, *pCurrentFcb = NULL, *pTargetFcb = NULL;
    UNICODE_STRING    uniPathName, uniComponentName, uniRemainingPath, uniSearchName;
    ULONG             ulCRC = 0;
    AFSDirEntryCB    *pDirEntry = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    ULONGLONG         ullIndex = 0;
    UNICODE_STRING    uniSysName;
    ULONG             ulSubstituteIndex = 0;
    BOOLEAN           bFoundComponent = FALSE;
    BOOLEAN           bSubstituteName = FALSE;
    BOOLEAN           bContinueProcessing = TRUE;
    AFSNameArrayHdr  *pNameArray = NameArray;
    AFSNameArrayCB   *pCurrentElement = NULL;
    BOOLEAN           bAllocatedNameArray = FALSE;
    UNICODE_STRING    uniRelativeName, uniNoOpName;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSLocateNameEntry (FO: %08lX) Processing full name %wZ\n",
                      FileObject,
                      RootPathName);

        RtlInitUnicodeString( &uniSysName,
                              L"*@SYS");

        RtlInitUnicodeString( &uniRelativeName,
                              L"..");

        RtlInitUnicodeString( &uniNoOpName,
                              L".");

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
        // We will parse through the filename, locating the directory nodes until we encounter a cache miss
        // Starting at the root node
        //

        pParentFcb = *ParentFcb;

        uniPathName = *ParsedPathName;

        //
        // Increment the call count, this is the count of 're-entrancy' we are currently at
        //

        if( InterlockedIncrement( &pNameArray->RecursionCount) > AFS_MAX_RECURSION_COUNT)
        {

            AFSReleaseResource( &pParentFcb->NPFcb->Resource);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        while( TRUE)
        {

            //
            // Check our total component count for this name array
            //

            InterlockedIncrement( &pNameArray->ComponentCount);

            if( pNameArray->ComponentCount >= AFS_MAX_NAME_ARRAY_COUNT)
            {

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            //
            // Check if the directory requires verification
            //

            if( BooleanFlagOn( pParentFcb->Flags, AFS_FCB_VERIFY) &&
                !AFSIsEnumerationInProcess( pParentFcb))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %08lX) Verifying parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              FileObject,
                              &pParentFcb->DirEntry->DirectoryEntry.FileName,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                ntStatus = AFSVerifyEntry( ProcessID,
                                           pParentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failed to verify parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                  FileObject,
                                  &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Unique,
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
                              "AFSLocateNameEntry (FO: %08lX) Evaluating parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              FileObject,
                              &pParentFcb->DirEntry->DirectoryEntry.FileName,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                ntStatus = AFSEvaluateNode( ProcessID,
                                            pParentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failed to evaluate parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                  FileObject,
                                  &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                  ntStatus);

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    try_return( ntStatus = STATUS_OBJECT_PATH_INVALID);
                }

                ClearFlag( pParentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
            }

            //
            // If this is a mount point or symlink then go get the real directory node
            //

            switch ( pParentFcb->DirEntry->DirectoryEntry.FileType )
            {

                case AFS_FILE_TYPE_SYMLINK:
                {
                
                    //
                    // Build the symlink target
                    //
                    // If this is not the first pass, pParentFcb will have been
                    // been added to the NameArray because it might have been a
                    // directory, mount point, etc.  Since we now know it is a 
                    // symlink we must remove it from the NameArray before we 
                    // evaluate the target.
                    //

                    if ( pNameArray->CurrentEntry->Fcb == pParentFcb )
                    {

                        AFSBackupEntry( pNameArray);
                    }

                    ntStatus = AFSBuildSymLinkTarget( ProcessID,
                                                      pParentFcb,
                                                      FileObject,
                                                      pNameArray,
                                                      Flags,
                                                      NULL,
                                                      &pTargetFcb);

                    if( !NT_SUCCESS( ntStatus) ||
                        ntStatus == STATUS_REPARSE)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to build SL target for parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      FileObject,
                                      &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                      ntStatus);

                        try_return( ntStatus);
                    }

                    ASSERT( pTargetFcb != NULL);

                    ntStatus = AFSInsertNextElement( pNameArray,
                                                     pTargetFcb);

                    pParentFcb = pTargetFcb;

                    //
                    // We actually want to restart processing here on the new parent  ...
                    //

                    continue;
                }

                case AFS_FILE_TYPE_MOUNTPOINT:
                {

                    ASSERT( pParentFcb->Specific.Directory.DirectoryNodeListHead == NULL);

                    //
                    // Check if we have a target Fcb for this node
                    //

                    if( pParentFcb->Specific.MountPoint.VolumeTargetFcb == NULL)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry (FO: %08lX) Building MP target for parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                        //
                        // Go retrieve the target entry for this node
                        //

                        ntStatus = AFSBuildMountPointTarget( ProcessID,
                                                             pParentFcb);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry (FO: %08lX) Failed to build MP target for parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          FileObject,
                                          &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                          pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                          pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                          pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                          pParentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                          ntStatus);

                            AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                            try_return( ntStatus);
                        }
                    }

                    //
                    // Swap out where we are in the chain
                    //

                    pTargetFcb = pParentFcb->Specific.MountPoint.VolumeTargetFcb;

                    ASSERT( pTargetFcb != NULL);

                    AFSAcquireExcl( &pTargetFcb->NPFcb->Resource,
                                    TRUE);

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    pParentFcb = pTargetFcb;

                    //
                    // Again, we want to restart processing here on the new parent ...
                    //

                    continue;
                }

                case AFS_FILE_TYPE_DFSLINK:
                {

                    //
                    // This is a DFS link so we need to update the file name and return STATUS_REPARSE to the
                    // system for it to reevaluate it
                    //

                    if( FileObject != NULL)
                    {

                        ntStatus = AFSProcessDFSLink( pParentFcb,
                                                      FileObject,
                                                      &uniRemainingPath);
                    }
                    else
                    {

                        //
                        // This is where we have been re-entered from an NP evaluation call via the BuildBranch()
                        // routine.
                        //

                        ntStatus = STATUS_INVALID_PARAMETER;
                    }

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    try_return( ntStatus);
                }

            }   /* end of switch */

            //
            // If the parent is not initialized then do it now
            //

            if( pParentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB &&
                !BooleanFlagOn( pParentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %08lX) Enumerating parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              FileObject,
                              &pParentFcb->DirEntry->DirectoryEntry.FileName,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                ntStatus = AFSEnumerateDirectory( ProcessID,
                                                  pParentFcb,
                                                  &pParentFcb->Specific.Directory.DirectoryNodeHdr,
                                                  &pParentFcb->Specific.Directory.DirectoryNodeListHead,
                                                  &pParentFcb->Specific.Directory.DirectoryNodeListTail,
                                                  &pParentFcb->Specific.Directory.ShortNameTree,
                                                  TRUE);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failed to enumerate parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                  FileObject,
                                  &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Unique,
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
                                  "AFSLocateNameEntry (FO: %08lX) Enumerating root %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  FileObject,
                                  &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                    ntStatus = AFSEnumerateDirectory( ProcessID,
                                                      pParentFcb,
                                                      &pParentFcb->Specific.VolumeRoot.DirectoryNodeHdr,
                                                      &pParentFcb->Specific.VolumeRoot.DirectoryNodeListHead,
                                                      &pParentFcb->Specific.VolumeRoot.DirectoryNodeListTail,
                                                      &pParentFcb->Specific.VolumeRoot.ShortNameTree,
                                                      TRUE);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to enumerate root %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      FileObject,
                                      &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pParentFcb->DirEntry->DirectoryEntry.FileId.Unique,
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
                                  "AFSLocateNameEntry (FO: %08lX) Initializing worker for root %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  FileObject,
                                  &pParentFcb->DirEntry->DirectoryEntry.FileName,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pParentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                    AFSInitVolumeWorker( pParentFcb);
                }
            }
            else if( pParentFcb->Header.NodeTypeCode == AFS_FILE_FCB)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSLocateNameEntry (FO: %08lX) Encountered file node %wZ FID %08lX-%08lX-%08lX-%08lX in path processing\n",
                              FileObject,
                              &pParentFcb->DirEntry->DirectoryEntry.FileName,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pParentFcb->DirEntry->DirectoryEntry.FileId.Unique);

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

            //
            // Check for the . and .. in the path
            //

            if( RtlCompareUnicodeString( &uniComponentName,
                                         &uniNoOpName,
                                         TRUE) == 0)
            {

                uniPathName = uniRemainingPath;

                continue;
            }

            if( RtlCompareUnicodeString( &uniComponentName,
                                         &uniRelativeName,
                                         TRUE) == 0)
            {

                //
                // Drop the lock on the current parent since we will be
                // backing up one entry
                //

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                //
                // Need to back up one entry in the name array
                //

                pParentFcb = AFSBackupEntry( NameArray);

                if( pParentFcb == NULL)
                {

                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                    break;
                }

                AFSAcquireExcl( &pParentFcb->NPFcb->Resource,
                                TRUE);

                uniPathName = uniRemainingPath;

                continue;
            }

            uniSearchName = uniComponentName;

            while( !bFoundComponent)
            {

                //
                // If the SearchName contains @SYS then we skip the lookup 
                // and jump to the substitution.  If there is no substitution
                // we give up.
                //

                if( FsRtlIsNameInExpression( &uniSysName,
                                             &uniSearchName,
                                             TRUE,
                                             NULL))
                {

                    goto SysNameSubstitution;
                }

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

                              SysNameSubstitution:

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

                                        if( ComponentName != NULL)
                                        {

                                            *ComponentName = uniComponentName;
                                        }
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

                                if( ComponentName != NULL)
                                {

                                    *ComponentName = uniComponentName;
                                }
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

                BOOLEAN bRelativeOpen = FALSE;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSLocateNameEntry (FO: %08lX) Substituting %wZ into %wZ Index %08lX\n",
                              FileObject,
                              &uniSearchName,
                              &uniComponentName,
                              ulSubstituteIndex);

                if( FileObject != NULL &&
                    FileObject->RelatedFileObject != NULL)
                {

                    bRelativeOpen = TRUE;
                }

                ntStatus = AFSSubstituteNameInPath( RootPathName,
                                                    &uniComponentName,
                                                    &uniSearchName,
                                                    &uniRemainingPath,
                                                    bRelativeOpen);

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

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    ExFreePool( uniSearchName.Buffer);

                    try_return( ntStatus);
                }

                ExFreePool( uniSearchName.Buffer);
            }

            //
            // Update the search parameters
            //

            uniPathName = uniRemainingPath;
                
            if( ComponentName != NULL)
            {

                *ComponentName = uniComponentName;
            }

            //
            // Check if the is a SymLink entry but has no Target FileID or Name. In this
            // case it might be a DFS Link so let's go and evaluate it to be sure
            //

            if( pDirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK && 
                pDirEntry->DirectoryEntry.TargetFileId.Vnode == 0 &&
                pDirEntry->DirectoryEntry.TargetFileId.Unique == 0 &&
                pDirEntry->DirectoryEntry.TargetName.Length == 0)
            {

                ntStatus = AFSValidateSymLink( ProcessID,
                                               pDirEntry);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failed to evaluate possible DFS Link %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                  FileObject,
                                  &pDirEntry->DirectoryEntry.FileName,
                                  pDirEntry->DirectoryEntry.FileId.Cell,
                                  pDirEntry->DirectoryEntry.FileId.Volume,
                                  pDirEntry->DirectoryEntry.FileId.Vnode,
                                  pDirEntry->DirectoryEntry.FileId.Unique,
                                  ntStatus);

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    //
                    // Update the failure status to a general failure
                    // 
    
                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }
            }

            //
            // Locate/create the Fcb for the current portion.
            //

            if( pDirEntry->Fcb == NULL)
            {

                pCurrentFcb = NULL;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %08lX) Initializing Fcb for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  FileObject,
                                  &pDirEntry->DirectoryEntry.FileName,
                                  pDirEntry->DirectoryEntry.FileId.Cell,
                                  pDirEntry->DirectoryEntry.FileId.Volume,
                                  pDirEntry->DirectoryEntry.FileId.Vnode,
                                  pDirEntry->DirectoryEntry.FileId.Unique);

                ntStatus = AFSInitFcb( pParentFcb,
                                       pDirEntry,
                                       &pCurrentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %08lX) Failed to initialize Fcb for %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      FileObject,
                                      &pDirEntry->DirectoryEntry.FileName,
                                      pDirEntry->DirectoryEntry.FileId.Cell,
                                      pDirEntry->DirectoryEntry.FileId.Volume,
                                      pDirEntry->DirectoryEntry.FileId.Vnode,
                                      pDirEntry->DirectoryEntry.FileId.Unique,
                                      ntStatus);

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    break;
                }

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %08lX) Initialized Fcb %08lX for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  FileObject,
                                  pCurrentFcb,
                                  &pDirEntry->DirectoryEntry.FileName,
                                  pDirEntry->DirectoryEntry.FileId.Cell,
                                  pDirEntry->DirectoryEntry.FileId.Volume,
                                  pDirEntry->DirectoryEntry.FileId.Vnode,
                                  pDirEntry->DirectoryEntry.FileId.Unique);
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

            if( uniRemainingPath.Length > 0)
            {

                //
                // Update the name array
                //

                ntStatus = AFSInsertNextElement( pNameArray,
                                                 pCurrentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    try_return( ntStatus);
                }

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
                // As above, we need to walk the chain of potential links to the end node
                //

                bContinueProcessing = TRUE;

                while( bContinueProcessing)
                {

                    //
                    // Ensure the node has been evaluated, if not then go do it now
                    //

                    if( BooleanFlagOn( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE_2,
                                      "AFSLocateNameEntry (FO: %08lX) Evaluating fcb for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                        ntStatus = AFSEvaluateNode( ProcessID,
                                                    pCurrentFcb);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry (FO: %08lX) Failed to evaluate fcb for %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          FileObject,
                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                          ntStatus);

                            if( pParentFcb != pCurrentFcb) 
                            {

                                AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                            }

                            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                            try_return( ntStatus = STATUS_OBJECT_PATH_INVALID);
                        }

                        ClearFlag( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
                    }

                    if( BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_VERIFY) &&
                        !AFSIsEnumerationInProcess( pCurrentFcb))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry (FO: %08lX) Verifying parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                        ntStatus = AFSVerifyEntry( ProcessID,
                                                   pCurrentFcb);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry (FO: %08lX) Failed to verify parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          FileObject,
                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                          ntStatus);

                            if( pParentFcb != pCurrentFcb) 
                            {

                                AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                            }

                            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                            try_return( ntStatus);
                        }
                    }

                    //
                    // If this is a mount point or symlink then go get the real directory node
                    //

                    switch ( pCurrentFcb->DirEntry->DirectoryEntry.FileType)
                    {

                        case AFS_FILE_TYPE_SYMLINK:
                        {
                        
                            //
                            // Build the symlink target
                            //

                            ntStatus = AFSBuildSymLinkTarget( ProcessID,
                                                              pCurrentFcb,
                                                              FileObject,
                                                              pNameArray,
                                                              Flags,
                                                              NULL,
                                                              &pTargetFcb);

                            if( !NT_SUCCESS( ntStatus) ||
                                ntStatus == STATUS_REPARSE)
                            {

                                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                              AFS_TRACE_LEVEL_ERROR,
                                              "AFSLocateNameEntry (FO: %08lX) Failed to build SL target for parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                              FileObject,
                                              &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                              ntStatus);

                                //
                                // We may have already released this resource ...
                                //

                                if( ExIsResourceAcquiredLite( &pParentFcb->NPFcb->Resource))
                                {

                                    AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                                }

                                try_return( ntStatus);
                            }

                            ASSERT( pTargetFcb != NULL);                    

                            pCurrentFcb = pTargetFcb;

                            continue;
                        }

                        case AFS_FILE_TYPE_MOUNTPOINT:
                        {

                            ASSERT( pCurrentFcb->Specific.Directory.DirectoryNodeListHead == NULL);

                            //
                            // If we are not evaluating the target of this node then
                            // get out now
                            //

                            if( BooleanFlagOn( Flags, AFS_LOCATE_FLAGS_NO_MP_TARGET_EVAL))
                            {

                                bContinueProcessing = FALSE;

                                break;
                            }

                            //
                            // Check if we have a target Fcb for this node
                            //

                            if( pCurrentFcb->Specific.MountPoint.VolumeTargetFcb == NULL)
                            {

                                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                              AFS_TRACE_LEVEL_VERBOSE_2,
                                              "AFSLocateNameEntry (FO: %08lX) Building target for MP %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                              FileObject,
                                              &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                                //
                                // Go retrieve the target entry for this node
                                //

                                ntStatus = AFSBuildMountPointTarget( ProcessID,
                                                                     pCurrentFcb);

                                if( !NT_SUCCESS( ntStatus))
                                {

                                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                                  AFS_TRACE_LEVEL_ERROR,
                                                  "AFSLocateNameEntry (FO: %08lX) Failed to build target for MP %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                                  FileObject,
                                                  &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                                  ntStatus);

                                    if( pParentFcb != pCurrentFcb) 
                                    {

                                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                                    }

                                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                                    try_return( ntStatus);
                                }
                            }

                            //
                            // Swap out where we are in the chain
                            //

                            pTargetFcb = pCurrentFcb->Specific.MountPoint.VolumeTargetFcb;

                            ASSERT( pTargetFcb != NULL);

                            AFSAcquireExcl( &pTargetFcb->NPFcb->Resource,
                                            TRUE);

                            if( pParentFcb != pCurrentFcb) 
                            {

                                AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
                            }

                            pCurrentFcb = pTargetFcb;

                            //
                            // If this is a root then the parent and current will be the same
                            //

                            if( pCurrentFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
                            {

                                AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                                pParentFcb = pCurrentFcb;
                            }

                            continue;
                        }

                        case AFS_FILE_TYPE_DFSLINK:
                        {

                            //
                            // This is a DFS link so we need to update the file name and return STATUS_REPARSE to the
                            // system for it to reevaluate it
                            //

                            if( FileObject != NULL)
                            {

                                ntStatus = AFSProcessDFSLink( pCurrentFcb,
                                                              FileObject,
                                                              &uniRemainingPath);
                            }
                            else
                            {

                                ntStatus = STATUS_INVALID_PARAMETER;
                            }

                            if( pCurrentFcb != pParentFcb)
                            {

                                AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                            }

                            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                            try_return( ntStatus);
                        }

                        default: 
                        {

                            //
                            // Roots have already been inserted into the table ...
                            //

                            if( pCurrentFcb->ParentFcb != NULL)
                            {

                                ntStatus = AFSInsertNextElement( pNameArray,
                                                                 pCurrentFcb);

                                if( !NT_SUCCESS( ntStatus))
                                {

                                    if( pCurrentFcb != pParentFcb)
                                    {

                                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                                    }

                                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                                    try_return( ntStatus);
                                }
                            }

                            //
                            // We're done ...
                            //

                            bContinueProcessing = FALSE;

                            break;
                        }
                    } /* end of switch */
                } // End of While()

                //
                // If the entry is not initialized then do it now
                //

                if( pCurrentFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB &&
                    !BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSLocateNameEntry (FO: %08lX) Enumerating directory %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  FileObject,
                                  &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                    ntStatus = AFSEnumerateDirectory( ProcessID,
                                                      pCurrentFcb,
                                                      &pCurrentFcb->Specific.Directory.DirectoryNodeHdr,
                                                      &pCurrentFcb->Specific.Directory.DirectoryNodeListHead,
                                                      &pCurrentFcb->Specific.Directory.DirectoryNodeListTail,
                                                      &pCurrentFcb->Specific.Directory.ShortNameTree,
                                                      TRUE);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %08lX) Failed to enumerate directory %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      FileObject,
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                      ntStatus);

                        if( pCurrentFcb != pParentFcb)
                        {

                            AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                        }

                        AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);
 
                        break;
                    }

                    SetFlag( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED);
                }
                else if( pCurrentFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
                {
                    
                    //
                    // Check if the directory requires verification
                    //

                    if( BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_VERIFY) &&
                        !AFSIsEnumerationInProcess( pCurrentFcb))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry (FO: %08lX) Verifying root %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                        ntStatus = AFSVerifyEntry( ProcessID,
                                                   pCurrentFcb);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry (FO: %08lX) Failed to verify root %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          FileObject,
                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                          ntStatus);

                            if( pCurrentFcb != pParentFcb)
                            {

                                AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                            }

                            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                            try_return( ntStatus);
                        }
                    }

                    if( !BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE_2,
                                      "AFSLocateNameEntry (FO: %08lX) Enuemrating root %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                        ntStatus = AFSEnumerateDirectory( ProcessID,
                                                          pCurrentFcb,
                                                          &pCurrentFcb->Specific.VolumeRoot.DirectoryNodeHdr,
                                                          &pCurrentFcb->Specific.VolumeRoot.DirectoryNodeListHead,
                                                          &pCurrentFcb->Specific.VolumeRoot.DirectoryNodeListTail,
                                                          &pCurrentFcb->Specific.VolumeRoot.ShortNameTree,
                                                          TRUE);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry (FO: %08lX) Failed to enumerate root %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                                  FileObject,
                                                  &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                                  ntStatus);

                            if( pCurrentFcb != pParentFcb)
                            {

                                AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                            }

                            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

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
                                      "AFSLocateNameEntry (FO: %08lX) Initializing worker for root %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                        AFSInitVolumeWorker( pCurrentFcb);
                    }
                }

                //
                // Pass back the remaining portion of the name
                //

                if( ComponentName != NULL)
                {

                    *ComponentName = uniRemainingPath;
                }

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

        if( pNameArray != NULL)
        {

            InterlockedDecrement( &pNameArray->RecursionCount);
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
                      "AFSCreateDirEntry Creating dir entry in parent %wZ FID %08lX-%08lX-%08lX-%08lX Component %wZ Attribs %08lX\n",
                      &ParentDcb->DirEntry->DirectoryEntry.FileName,
                      ParentDcb->DirEntry->DirectoryEntry.FileId.Cell,
                      ParentDcb->DirEntry->DirectoryEntry.FileId.Volume,
                      ParentDcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      ParentDcb->DirEntry->DirectoryEntry.FileId.Unique,
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
                          "AFSCreateDirEntry Failed to create dir entry in parent %wZ FID %08lX-%08lX-%08lX-%08lX Component %wZ Attribs %08lX Status %08lX\n",
                          &ParentDcb->DirEntry->DirectoryEntry.FileName,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Cell,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Volume,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Vnode,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Unique,
                          ComponentName,
                          Attributes,
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // If we still have a valid parent then insert it into the tree
        // otherwise we will re-evaluate the node on next access
        //

        if( BooleanFlagOn( ParentDcb->Flags, AFS_FCB_DIRECTORY_ENUMERATED))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSCreateDirEntry Inserting dir entry in parent %wZ FID %08lX-%08lX-%08lX-%08lX Component %wZ\n",
                          &ParentDcb->DirEntry->DirectoryEntry.FileName,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Cell,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Volume,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Vnode,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Unique,
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
                          "AFSCreateDirEntry Created stand alone dir entry in parent %wZ FID %08lX-%08lX-%08lX-%08lX Component %wZ\n",
                          &ParentDcb->DirEntry->DirectoryEntry.FileName,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Cell,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Volume,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Vnode,
                          ParentDcb->DirEntry->DirectoryEntry.FileId.Unique,
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

        ASSERT( DirEntry->DirectoryEntry.FileType != AFS_FILE_TYPE_DIRECTORY ||
                DirEntry->DirectoryEntry.FileId.Vnode != 1);

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
                      "AFSInsertDirectoryNode Inserting entry %08lX %wZ FID %08lX-%08lX-%08lX-%08lX into parent %08lX Status %08lX\n",
                      DirEntry,
                      &DirEntry->DirectoryEntry.FileName,
                      DirEntry->DirectoryEntry.FileId.Cell,
                      DirEntry->DirectoryEntry.FileId.Volume,
                      DirEntry->DirectoryEntry.FileId.Vnode,
                      DirEntry->DirectoryEntry.FileId.Unique,
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
                      "AFSDeleteDirEntry Deleting dir entry in parent %08lX Entry %08lX %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      ParentDcb,
                      DirEntry,
                      &DirEntry->DirectoryEntry.FileName,
                      DirEntry->DirectoryEntry.FileId.Cell,
                      DirEntry->DirectoryEntry.FileId.Volume,
                      DirEntry->DirectoryEntry.FileId.Vnode,
                      DirEntry->DirectoryEntry.FileId.Unique);

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

        //ASSERT( DirEntry->DirectoryEntry.FileType != AFS_FILE_TYPE_DIRECTORY ||
        //        DirEntry->DirectoryEntry.FileId.Vnode != 1);


        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveDirNodeFromParent Removing DirEntry %08lX %wZ FID %08lX-%08lX-%08lX-%08lX from Parent %08lX\n",
                      DirEntry,
                      &DirEntry->DirectoryEntry.FileName,
                      DirEntry->DirectoryEntry.FileId.Cell,
                      DirEntry->DirectoryEntry.FileId.Volume,
                      DirEntry->DirectoryEntry.FileId.Vnode,
                      DirEntry->DirectoryEntry.FileId.Unique,
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

            if( ParentDcb->Specific.Directory.ShortNameTree &&
                DirEntry->Type.Data.ShortNameTreeEntry.HashIndex != 0)
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
              OUT PUNICODE_STRING ParsedFileName,
              OUT PUNICODE_STRING RootFileName,
              OUT BOOLEAN *FreeNameString,
              OUT AFSFcb **RootFcb,
              OUT AFSFcb **ParentFcb,
              OUT AFSNameArrayHdr **NameArray)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION  pIrpSp = IoGetCurrentIrpStackLocation( Irp); 
    AFSDeviceExt       *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSFcb             *pParentFcb = NULL, *pCurrentFcb = NULL, *pRelatedFcb = NULL, *pTargetFcb = NULL;
    UNICODE_STRING      uniFullName, uniComponentName, uniRemainingPath;
    ULONG               ulCRC = 0;
    AFSDirEntryCB      *pDirEntry = NULL, *pShareDirEntry = NULL;
    BOOLEAN             bAddTrailingSlash = FALSE;
    USHORT              usIndex = 0;
    UNICODE_STRING      uniRelatedFullName;
    BOOLEAN             bFreeRelatedName = FALSE;
    AFSCcb             *pCcb = NULL;
    ULONGLONG           ullIndex = 0;
    BOOLEAN             bContinueProcessing = TRUE;
    AFSNameArrayHdr    *pNameArray = NULL, *pRelatedNameArray = NULL;
    USHORT              usComponentIndex = 0;
    USHORT              usComponentLength = 0;

    __Enter
    {

        *FreeNameString = FALSE;

        if( pIrpSp->FileObject->RelatedFileObject != NULL)
        {

            pRelatedFcb = (AFSFcb *)pIrpSp->FileObject->RelatedFileObject->FsContext;

            pCcb = (AFSCcb *)pIrpSp->FileObject->RelatedFileObject->FsContext2;

            pRelatedNameArray = pCcb->NameArray;

            uniFullName = pIrpSp->FileObject->FileName;

            ASSERT( pRelatedFcb != NULL);

            //
            // No wild cards in the name
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSParseName (%08lX) Relative open for %wZ FID %08lX-%08lX-%08lX-%08lX component %wZ\n",
                          Irp,
                          &pRelatedFcb->DirEntry->DirectoryEntry.FileName,
                          pRelatedFcb->DirEntry->DirectoryEntry.FileId.Cell,
                          pRelatedFcb->DirEntry->DirectoryEntry.FileId.Volume,
                          pRelatedFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                          pRelatedFcb->DirEntry->DirectoryEntry.FileId.Unique,
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

                ntStatus = AFSVerifyEntry( (ULONGLONG)PsGetCurrentProcessId(),
                                           *RootFcb);

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
                                  "AFSParseName (%08lX) Verifying parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  Irp,
                                  &(*ParentFcb)->DirEntry->DirectoryEntry.FileName,
                                  (*ParentFcb)->DirEntry->DirectoryEntry.FileId.Cell,
                                  (*ParentFcb)->DirEntry->DirectoryEntry.FileId.Volume,
                                  (*ParentFcb)->DirEntry->DirectoryEntry.FileId.Vnode,
                                  (*ParentFcb)->DirEntry->DirectoryEntry.FileId.Unique);

                    ntStatus = AFSVerifyEntry( (ULONGLONG)PsGetCurrentProcessId(),
                                               *ParentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSParseName (%08lX) Failed verification of parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      Irp,
                                      &(*ParentFcb)->DirEntry->DirectoryEntry.FileName,
                                      (*ParentFcb)->DirEntry->DirectoryEntry.FileId.Cell,
                                      (*ParentFcb)->DirEntry->DirectoryEntry.FileId.Volume,
                                      (*ParentFcb)->DirEntry->DirectoryEntry.FileId.Vnode,
                                      (*ParentFcb)->DirEntry->DirectoryEntry.FileId.Unique,
                                      ntStatus);

                        AFSReleaseResource( &(*ParentFcb)->NPFcb->Resource);

                        AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                        *RootFcb = NULL;

                        *ParentFcb = NULL;

                        try_return( ntStatus);
                    }
                }
            }

            //
            // Create our full path name buffer
            //

            uniFullName.MaximumLength = pCcb->FullFileName.Length + 
                                                    sizeof( WCHAR) + 
                                                    pIrpSp->FileObject->FileName.Length +
                                                    sizeof( WCHAR);

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

            RtlZeroMemory( uniFullName.Buffer,
                           uniFullName.MaximumLength);

            RtlCopyMemory( uniFullName.Buffer,
                           pCcb->FullFileName.Buffer,
                           pCcb->FullFileName.Length);

            uniFullName.Length = pCcb->FullFileName.Length;

            usComponentIndex = (USHORT)(uniFullName.Length/sizeof( WCHAR));

            usComponentLength = pIrpSp->FileObject->FileName.Length;

            if( uniFullName.Buffer[ (uniFullName.Length/sizeof( WCHAR)) - 1] != L'\\' &&
                pIrpSp->FileObject->FileName.Length > 0 &&
                pIrpSp->FileObject->FileName.Buffer[ 0] != L'\\')
            {

                uniFullName.Buffer[ (uniFullName.Length/sizeof( WCHAR))] = L'\\';

                uniFullName.Length += sizeof( WCHAR);

                usComponentLength += sizeof( WCHAR);
            }

            if( pIrpSp->FileObject->FileName.Length > 0)
            {

                RtlCopyMemory( &uniFullName.Buffer[ uniFullName.Length/sizeof( WCHAR)],
                               pIrpSp->FileObject->FileName.Buffer,
                               pIrpSp->FileObject->FileName.Length);

                uniFullName.Length += pIrpSp->FileObject->FileName.Length;
            }

            //
            // Init and populate our name array
            //

            pNameArray = AFSInitNameArray();

            if( pNameArray == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%08lX) Failed to initialize name array\n",
                                              Irp);

                ExFreePool( uniFullName.Buffer);

                AFSReleaseResource( &(*ParentFcb)->NPFcb->Resource);

                AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                *RootFcb = NULL;

                *ParentFcb = NULL;

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            *RootFileName = uniFullName;

            //
            // We populate up to the current parent
            //

            if( pRelatedNameArray == NULL)
            {

                ntStatus = AFSPopulateNameArray( pNameArray,
                                                 NULL,
                                                 *ParentFcb);
            }
            else
            {

                ntStatus = AFSPopulateNameArrayFromRelatedArray( pNameArray,
                                                                 pRelatedNameArray,
                                                                 *ParentFcb);
            }

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%08lX) Failed to populate name array\n",
                                              Irp);

                ExFreePool( uniFullName.Buffer);

                AFSReleaseResource( &(*ParentFcb)->NPFcb->Resource);

                AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                *RootFcb = NULL;

                *ParentFcb = NULL;

                try_return( ntStatus);
            }

            ParsedFileName->Length = usComponentLength;
            ParsedFileName->MaximumLength = uniFullName.MaximumLength;

            ParsedFileName->Buffer = &uniFullName.Buffer[ usComponentIndex];

            *FreeNameString = TRUE;

            *NameArray = pNameArray;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSParseName (%08lX) Returning full name %wZ\n",
                          Irp,
                          &uniFullName);

            try_return( ntStatus);
        }
       
        //
        // The name is a fully qualified name. Parse out the server/share names and 
        // point to the root qualified name
        // First thing is to locate the server name
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
        // Check if the global root needs verification
        //

#ifdef NOT_YET

        if( BooleanFlagOn( AFSGlobalRoot->Flags, AFS_FCB_VERIFY))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSParseName (%08lX) Verifying global root volume\n",
                                      Irp);

            ntStatus = AFSVerifyEntry( (ULONGLONG)PsGetCurrentProcessId(),
                                       AFSGlobalRoot);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%08lX) Failed to verify global root volume Status %08lX\n",
                                          Irp,
                                          ntStatus);

                AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

                try_return( ntStatus);
            }
        }

#endif

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

        //
        // Init and populate our name array
        //

        pNameArray = AFSInitNameArray();

        if( pNameArray == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSParseName (%08lX) Failed to initialize name array\n",
                                              Irp);

            AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // We populate up to the current parent, if they are different
        //

        ntStatus = AFSPopulateNameArray( pNameArray,
                                         &uniFullName,
                                         AFSGlobalRoot);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSParseName (%08lX) Failed to populate name array\n",
                                              Irp);

            AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

            try_return( ntStatus);
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

                        ntStatus = AFSCheckCellName( (ULONGLONG)PsGetCurrentProcessId(),
                                                     &uniComponentName,
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

                    ntStatus = AFSInitRootFcb( (ULONGLONG)PsGetCurrentProcessId(),
                                               pShareDirEntry,
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

            AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);
        }
        else
        {

            AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);

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
        // At this point we want to process the current node. But we do this and keep going until
        // we actually reach a root node for the cell
        //

        pCurrentFcb = pShareDirEntry->Fcb;

        while( bContinueProcessing)
        {

            //
            // If this node has become invalid then fail the request now
            //

            if( BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_INVALID))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%08lX) Fcb for cell name %wZ invalid\n",
                              Irp,
                              &uniComponentName);

                //
                // The symlink or share entry has been deleted
                //

                AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                try_return( ntStatus = STATUS_OBJECT_PATH_INVALID);
            }

            //
            // Ensure the root node has been evaluated, if not then go do it now
            //

            if( BooleanFlagOn( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%08lX) Evaluating cell %wZ\n",
                              Irp,
                              &uniComponentName);

                ntStatus = AFSEvaluateNode( (ULONGLONG)PsGetCurrentProcessId(),
                                            pCurrentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSParseName (%08lX) Failed to evaluate cell %wZ Status %08lX\n",
                                  Irp,
                                  &uniComponentName,
                                  ntStatus);

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    try_return( ntStatus);
                }

                ClearFlag( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
            }

            //
            // Be sure we return a root or a directory
            //

            switch ( pCurrentFcb->DirEntry->DirectoryEntry.FileType) 
            {

                case AFS_FILE_TYPE_SYMLINK:
                {
                
                    //
                    // Build the symlink target
                    //

                    ntStatus = AFSBuildSymLinkTarget( (ULONGLONG)PsGetCurrentProcessId(),
                                                      pCurrentFcb,
                                                      pIrpSp->FileObject,
                                                      pNameArray,
                                                      0,
                                                      NULL,
                                                      &pTargetFcb);

                    if( !NT_SUCCESS( ntStatus) ||
                        ntStatus == STATUS_REPARSE)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSParseName (%08lX) Failed to build SL target for parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      Irp,
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                      ntStatus);

                        try_return( ntStatus);
                    }

                    ASSERT( pTargetFcb != NULL);                    

                    pCurrentFcb = pTargetFcb;

                    pTargetFcb = NULL;

                    continue;
                }

                case AFS_FILE_TYPE_MOUNTPOINT:
                {

                    //
                    // Check if we have a target Fcb for this node
                    //

                    if( pCurrentFcb->Specific.MountPoint.VolumeTargetFcb == NULL)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSParseName (%08lX) Building target for MP %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      Irp,
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                        //
                        // Go retrieve the target entry for this node
                        //

                        ntStatus = AFSBuildMountPointTarget( (ULONGLONG)PsGetCurrentProcessId(),
                                                             pCurrentFcb);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSParseName (%08lX) Failed to build target for MP %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          Irp,
                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                          ntStatus);

                            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                            try_return( ntStatus);
                        }
                    }

                    //
                    // Swap out where we are in the chain
                    //

                    pTargetFcb = pCurrentFcb->Specific.MountPoint.VolumeTargetFcb;

                    AFSAcquireExcl( &pTargetFcb->NPFcb->Resource,
                                    TRUE);

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    pCurrentFcb = pTargetFcb;

                    pTargetFcb = NULL;

                    continue;
                }

                case AFS_FILE_TYPE_DFSLINK:
                {

                    //
                    // This is a DFS link so we need to update the file name and return STATUS_REPARSE to the
                    // system for it to reevaluate the link
                    //

                    ntStatus = AFSProcessDFSLink( pCurrentFcb,
                                                  pIrpSp->FileObject,
                                                  &uniRemainingPath);

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    try_return( ntStatus);
                }
        
                default:
                {

                    *RootFcb = pCurrentFcb;

                    *ParentFcb = pCurrentFcb;
               
                    if( BooleanFlagOn( (*RootFcb)->Flags, AFS_FCB_VOLUME_OFFLINE))
                    {

                        //
                        // The volume has been taken off line so fail the access
                        //

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSParseName (%08lX) Root volume OFFLINE for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      Irp,
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);


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
                                      "AFSParseName (%08lX) Verifying root volume %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      Irp,
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                        ntStatus = AFSVerifyEntry( (ULONGLONG)PsGetCurrentProcessId(),
                                                   *RootFcb);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSParseName (%08lX) Failed to verify root volume %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          Irp,
                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                          ntStatus);

                            AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                            *RootFcb = NULL;

                            *ParentFcb = NULL;

                            try_return( ntStatus);
                        }
                    }

                    //
                    // Insert the node into the name array
                    //

                    ntStatus = AFSInsertNextElement( pNameArray,
                                                     pCurrentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSReleaseResource( &(*RootFcb)->NPFcb->Resource);

                        *RootFcb = NULL;

                        *ParentFcb = NULL;

                        try_return( ntStatus);
                    }

                    //
                    // We'll break out on this loop
                    //

                    bContinueProcessing = FALSE;

                    break;
                }
            }
        }

        //
        // Return the remaining portion as the file name
        //

        *FileName = uniRemainingPath;

        *RootFileName = uniRemainingPath;

        *ParsedFileName = uniRemainingPath;

        *FreeNameString = FALSE;

        *NameArray = pNameArray;

try_exit:

        if( ntStatus != STATUS_SUCCESS)
        {

            if( pNameArray != NULL)
            {

                AFSFreeNameArray( pNameArray);
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSCheckCellName( IN ULONGLONG ProcessID,
                  IN UNICODE_STRING *CellName,
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

        ntStatus = AFSEvaluateTargetByName( ProcessID,
                                            &stFileID,
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
AFSBuildMountPointTarget( IN ULONGLONG ProcessID,
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
                      "AFSBuildMountPointTarget Building target directory for %wZ FID %08lX-%08lX-%08lX-%08lX PID %I64X\n",
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                      ProcessID);

        pCurrentFcb = Fcb;

        //
        // Do we need to evaluate the node?
        //

        if( pCurrentFcb->DirEntry->DirectoryEntry.TargetFileId.Vnode == 0 &&
            pCurrentFcb->DirEntry->DirectoryEntry.TargetFileId.Unique == 0)
        {

            //
            // Go evaluate the current target to get the target fid
            //

            stTargetFileID = pCurrentFcb->DirEntry->DirectoryEntry.FileId;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSBuildMountPointTarget Evaluating target %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

            ntStatus = AFSEvaluateTargetByID( &stTargetFileID,
                                              ProcessID,
                                              FALSE,
                                              &pDirEntry);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSBuildMountPointTarget Failed to evaluate target %wZ Status %08lX\n",
                                                                    &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                                                    ntStatus);
                try_return( ntStatus);
            }

            if( pDirEntry->TargetFileId.Vnode == 0 &&
                pDirEntry->TargetFileId.Unique == 0)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSBuildMountPointTarget Target %wZ FID %08lX-%08lX-%08lX-%08lX service returned zero FID\n",
                                  &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                try_return( ntStatus = STATUS_ACCESS_DENIED);
            }

            pCurrentFcb->DirEntry->DirectoryEntry.TargetFileId = pDirEntry->TargetFileId;
        }

        stTargetFileID = pCurrentFcb->DirEntry->DirectoryEntry.TargetFileId;

        ASSERT( stTargetFileID.Vnode != 0 &&
                stTargetFileID.Unique != 0);

        //
        // Try to locate this FID. First the volume then the 
        // entry itself
        //

        ullIndex = AFSCreateHighIndex( &stTargetFileID);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSBuildMountPointTarget Acquiring RDR VolumeTreeLock lock %08lX EXCL %08lX\n",
                          &pDevExt->Specific.RDR.VolumeTreeLock,
                          PsGetCurrentThread());

        AFSAcquireShared( &pDevExt->Specific.RDR.VolumeTreeLock,
                          TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSBuildMountPointTarget Locating volume for target %I64X\n",
                          ullIndex);

        ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                       ullIndex,
                                       &pVcb);

        //
        // We can be processing a request for a target that is on a volume
        // we have never seen before.
        //

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

            ntStatus = AFSRetrieveVolumeInformation( ProcessID,
                                                     &stTargetFileID,
                                                     &stVolumeInfo);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSBuildMountPointTarget Failed to retrieve volume information on VolumeFID %08lX-%08lX\n",
                                  stTargetFileID.Cell,
                                  stTargetFileID.Volume);

                try_return( ntStatus);
            }

            AFSAcquireExcl( &pDevExt->Specific.RDR.VolumeTreeLock,
                            TRUE);

            ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                           ullIndex,
                                           &pVcb);

            //
            // At this point if we do not have a volume root Fcb
            // we can only create a volume root for the target of 
            // a MountPoint
            //

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
                              "AFSBuildMountPointTarget Initializing root for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                ntStatus = AFSInitRootForMountPoint( &stDirEnumEntry,
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

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSBuildMountPointTarget Evaluated target of %wZ FID %08lX-%08lX-%08lX-%08lX as root volume\n",
                                  &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                  pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

            InterlockedIncrement( &pVcb->OpenReferenceCount);

            pCurrentFcb->Specific.MountPoint.VolumeTargetFcb = pVcb;
        }

        AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

        if( pVcb == NULL) 
        {

            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }
                    
try_exit:

        if ( pDirEntry)
        {

            ExFreePool( pDirEntry);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSBuildSymLinkTarget( IN ULONGLONG ProcessID,
                       IN AFSFcb *Fcb,
                       IN PFILE_OBJECT FileObject,
                       IN AFSNameArrayHdr *NameArray,
                       IN ULONG Flags,
                       OUT AFSFileInfoCB *FileInfo,
                       OUT AFSFcb **TargetFcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    AFSFcb *pTargetFcb = NULL, *pVcb = NULL, *pCurrentFcb = NULL;
    AFSFileID stTargetFileID;
    AFSDirEnumEntry *pDirEntry = NULL;
    AFSDirEntryCB *pDirNode = NULL;
    UNICODE_STRING uniTargetName;
    ULONGLONG       ullIndex = 0;
    BOOLEAN     bFirstEntry = TRUE;
    AFSFcb *pParentFcb = NULL, *pParentFcbSaved;
    BOOLEAN bReleaseParent = FALSE;

    __Enter
    {

        //
        // Loop on each entry, building the chain until we encounter the final target
        //

        pCurrentFcb = Fcb;

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSBuildSymLinkTarget Building target for %wZ FID %08lX-%08lX-%08lX-%08lX PID %I64X\n",
                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                      ProcessID);

        if ( FileInfo != NULL) 
        {

            RtlZeroMemory( FileInfo,
                           sizeof( AFSFileInfoCB));
        }
        
        if( TargetFcb != NULL)
        {
            *TargetFcb = NULL;
        }

        uniTargetName.Length = 0;
        
        uniTargetName.MaximumLength = ( pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Length < PAGE_SIZE ? PAGE_SIZE : 
                                        pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Length + PAGE_SIZE);

        uniTargetName.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                               uniTargetName.MaximumLength,
                                                               AFS_NAME_BUFFER_TAG);

        while( TRUE)
        {
    
            ASSERT( pCurrentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_SYMLINK);

            if ( pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Length > uniTargetName.MaximumLength)
            {

                ExFreePool( uniTargetName.Buffer);

                uniTargetName.MaximumLength = ( pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Length < PAGE_SIZE ? PAGE_SIZE : 
                                                pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Length + PAGE_SIZE);

                uniTargetName.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                                       uniTargetName.MaximumLength,
                                                                       AFS_NAME_BUFFER_TAG);
            }

            uniTargetName.Length = pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Length;

            RtlCopyMemory( uniTargetName.Buffer,
                           pCurrentFcb->DirEntry->DirectoryEntry.TargetName.Buffer,
                           uniTargetName.Length);

            //
            // If the target name is relative then we need to process
            // it differently to obtain the target ...
            //

            if( AFSIsRelativeName( &uniTargetName))
            {

                if( NameArray == NULL)
                {

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    try_return( ntStatus = STATUS_INVALID_PARAMETER);
                }

                pParentFcb = pCurrentFcb->ParentFcb;

                ASSERT( pParentFcb != NULL);

                InterlockedIncrement( &pCurrentFcb->OpenReferenceCount);

                AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                //
                // If we don't already have the parent then acquire it here
                //

                if( !ExIsResourceAcquiredLite( &pParentFcb->NPFcb->Resource))
                {

                    AFSAcquireExcl( &pParentFcb->NPFcb->Resource,
                                    TRUE);

                    bReleaseParent = TRUE;
                }

                pParentFcbSaved = pParentFcb;

                ntStatus = AFSLocateNameEntry( ProcessID,
                                               FileObject,
                                               &uniTargetName,
                                               &uniTargetName,
                                               NameArray,
                                               Flags,
                                               &pParentFcb,
                                               &pTargetFcb,
                                               NULL);

                InterlockedDecrement( &pCurrentFcb->OpenReferenceCount);

                if ( pParentFcb != pParentFcbSaved )
                {

                    // AFSLocateNameEntry() released the prior ParentFcb and gave us a 
                    // new one.  We are responsible for releasing this new parent.

                    bReleaseParent = TRUE;
                }

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSBuildSymLinkTarget Failed to build relative target %wZ Status %08lX\n",
                                  &pCurrentFcb->DirEntry->DirectoryEntry.TargetName,
                                  ntStatus);

                    break;
                }



                if( pTargetFcb != NULL)
                {

                    //
                    // If we have acquired the parent then release it now
                    //

                    if( bReleaseParent &&
                        ExIsResourceAcquiredLite( &pParentFcb->NPFcb->Resource))
                    {

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);
                    }
                }
                else
                {

                    pTargetFcb = pParentFcb;  // The parent was updated in the locate routine ...

                    pParentFcb = pTargetFcb->ParentFcb; // Save off the parent in case we need to release it ...
                }

                // 
                // We have either released the parent, or it was never held, or it was assigned to pTargetFcb.
                //

                bReleaseParent = FALSE;
            }
            else
            {

                //
                // Go build the branch leading to this target node
                // Need to drop locks while performing this work
                // but also don't want things to go away so ref count the
                // node
                //

                if ( pParentFcb) 
                {
                    InterlockedIncrement( &pParentFcb->OpenReferenceCount);

                    if ( bReleaseParent)
                    {

                        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

                        bReleaseParent = FALSE;
                    }
                }

                InterlockedIncrement( &pCurrentFcb->OpenReferenceCount);

                AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                ntStatus = AFSBuildTargetBranch( ProcessID,
                                                 &uniTargetName,
                                                 FileObject,
                                                 NameArray,
                                                 Flags,
                                                 &pTargetFcb);

                InterlockedDecrement( &pCurrentFcb->OpenReferenceCount);

                if ( pParentFcb) 
                {
                    
                    InterlockedDecrement( &pParentFcb->OpenReferenceCount);
                }

                if( !NT_SUCCESS( ntStatus) ||
                    ntStatus == STATUS_REPARSE)
                {    

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSBuildSymLinkTarget Failed to evaluate target of %I64X Status %08lX\n",
                                  ullIndex,
                                  ntStatus);

                    //
                    // Dropped locks above ...
                    //

                    break;
                }

            }

            if ( FileInfo != NULL )
            {

                //
                // Check for the case we are given back a mount point. In this case
                // we will set some attributes explicitly.
                //

                if( pTargetFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_MOUNTPOINT)
                {

                    FileInfo->FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                }
                else
                {

                    FileInfo->FileAttributes = pTargetFcb->DirEntry->DirectoryEntry.FileAttributes;
                }

                FileInfo->AllocationSize = pTargetFcb->DirEntry->DirectoryEntry.AllocationSize;

                FileInfo->EndOfFile = pTargetFcb->DirEntry->DirectoryEntry.EndOfFile;

                FileInfo->CreationTime = pTargetFcb->DirEntry->DirectoryEntry.CreationTime;

                FileInfo->LastAccessTime = pTargetFcb->DirEntry->DirectoryEntry.LastAccessTime;

                FileInfo->LastWriteTime = pTargetFcb->DirEntry->DirectoryEntry.LastWriteTime;

                FileInfo->ChangeTime = pTargetFcb->DirEntry->DirectoryEntry.ChangeTime;
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSBuildSymLinkTarget Located final target %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                          &pTargetFcb->DirEntry->DirectoryEntry.FileName,
                          pTargetFcb->DirEntry->DirectoryEntry.FileId.Cell,
                          pTargetFcb->DirEntry->DirectoryEntry.FileId.Volume,
                          pTargetFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                          pTargetFcb->DirEntry->DirectoryEntry.FileId.Unique);

            if( TargetFcb != NULL)
            {

                *TargetFcb = pTargetFcb;
            }
            else 
            {

                AFSReleaseResource( &pTargetFcb->NPFcb->Resource);
            }

            ntStatus = STATUS_SUCCESS;

            break;
        }
                    
try_exit:

        if( bReleaseParent &&
            ExIsResourceAcquiredLite( &pParentFcb->NPFcb->Resource))
        {

            AFSReleaseResource( &pParentFcb->NPFcb->Resource);

        }

        ExFreePool( uniTargetName.Buffer);

        if ( pDirEntry)
        {
            ExFreePool( pDirEntry);
        }
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
    AFSDirEnumEntry *pDirEntry = NULL;

    __Enter
    {

        //
        // Build up the name to reparse
        // 

        RtlInitUnicodeString( &uniMUPDeviceName,
                              L"\\Device\\MUP");

        uniReparseName.Length = 0;
        uniReparseName.Buffer = NULL;

        //
        // Be sure we have a target name
        //

        if( Fcb->DirEntry->DirectoryEntry.TargetName.Length == 0)
        {

            //
            // Try to get one
            //

            ntStatus = AFSEvaluateTargetByID( &Fcb->DirEntry->DirectoryEntry.FileId,
                                              0,
                                              FALSE,
                                              &pDirEntry);

            if( !NT_SUCCESS( ntStatus) ||
                pDirEntry->TargetNameLength == 0)
            {

                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }

            //
            // Update the target name
            //

            ntStatus = AFSUpdateTargetName( &Fcb->DirEntry->DirectoryEntry.TargetName,
                                            &Fcb->DirEntry->Flags,
                                            (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset),
                                            (USHORT)pDirEntry->TargetNameLength);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }
        }

        uniReparseName.MaximumLength = uniMUPDeviceName.Length +
                                       sizeof( WCHAR) +
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

        if( Fcb->DirEntry->DirectoryEntry.TargetName.Buffer[ 0] != L'\\')
        {

            uniReparseName.Buffer[ uniReparseName.Length/sizeof( WCHAR)] = L'\\';

            uniReparseName.Length += sizeof( WCHAR);
        }
                       
        RtlCopyMemory( &uniReparseName.Buffer[ uniReparseName.Length/sizeof( WCHAR)],
                       Fcb->DirEntry->DirectoryEntry.TargetName.Buffer,
                       Fcb->DirEntry->DirectoryEntry.TargetName.Length);

        uniReparseName.Length += Fcb->DirEntry->DirectoryEntry.TargetName.Length;

        if( RemainingPath != NULL &&
            RemainingPath->Length > 0)
        {

            if( uniReparseName.Buffer[ (uniReparseName.Length/sizeof( WCHAR)) - 1] != L'\\' &&
                RemainingPath->Buffer[ 0] != L'\\')
            {

                uniReparseName.Buffer[ uniReparseName.Length/sizeof( WCHAR)] = L'\\';

                uniReparseName.Length += sizeof( WCHAR);
            }

            RtlCopyMemory( &uniReparseName.Buffer[ uniReparseName.Length/sizeof( WCHAR)],
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
                      "AFSProcessDFSLink Reparsing access to Fcb %wZ FID %08lX-%08lX-%08lX-%08lX to %wZ\n",
                      &Fcb->DirEntry->DirectoryEntry.FileName,
                      Fcb->DirEntry->DirectoryEntry.FileId.Cell,
                      Fcb->DirEntry->DirectoryEntry.FileId.Volume,
                      Fcb->DirEntry->DirectoryEntry.FileId.Vnode,
                      Fcb->DirEntry->DirectoryEntry.FileId.Unique,
                      &uniReparseName);

        //
        // Return status reparse ...
        //

        ntStatus = STATUS_REPARSE;

try_exit:

        if ( pDirEntry)
        {

            ExFreePool( pDirEntry);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSBuildTargetBranch( IN ULONGLONG ProcessID,
                      IN UNICODE_STRING *TargetName,
                      IN PFILE_OBJECT FileObject,
                      IN AFSNameArrayHdr *NameArray,
                      IN ULONG Flags,
                      OUT AFSFcb **TargetFcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    UNICODE_STRING uniFullName, uniComponentName, uniRemainingPath;
    ULONG ulCRC = 0;
    AFSDirEntryCB *pDirEntry = NULL, *pShareDirEntry = NULL;
    AFSFcb *pRootVcb = NULL, *pCurrentFcb = NULL, *pTargetFcb = NULL, *pParentFcb = NULL;
    BOOLEAN bContinueProcessing = TRUE;
    ULONGLONG ullIndex = 0;
    AFSNameArrayHdr *pNameArray = NULL;

    __Enter
    {

        uniFullName = *TargetName;

        //
        // Update the name ...
        //

        AFSUpdateName( &uniFullName);

        //
        // When building a target branch we don't want to mess with the original name
        // array so build our own for processing this path
        //

        pNameArray = AFSInitNameArray();

        if( pNameArray == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSBuildTargetBranch (%08lX) Failed to populate name array\n",
                          FileObject);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        ntStatus = AFSPopulateNameArray( pNameArray,
                                         &uniFullName,
                                         AFSGlobalRoot);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // We want to get to the root node of the path so start from the beginning ...
        //

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
                          "AFSBuildTargetBranch Name %wZ does not have server name\n",
                          &uniComponentName);

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // Be sure we are online and ready to go
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSBuildTargetBranch Acquiring GlobalRoot lock %08lX SHARED %08lX\n",
                      &AFSGlobalRoot->NPFcb->Resource,
                      ProcessID);

        AFSAcquireShared( &AFSGlobalRoot->NPFcb->Resource,
                          TRUE);

        if( BooleanFlagOn( AFSGlobalRoot->Flags, AFS_FCB_VOLUME_OFFLINE))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSBuildTargetBranch Global root OFFLINE\n");

            AFSReleaseResource( &AFSGlobalRoot->NPFcb->Resource);

            try_return( ntStatus = STATUS_TOO_LATE);
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
        // Generate a CRC on the component and look it up in the name tree
        //

        ulCRC = AFSGenerateCRC( &uniComponentName,
                                FALSE);

        //
        // Grab our tree lock shared
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSBuildTargetBranch Acquiring GlobalRoot DirectoryNodeHdr.TreeLock lock %08lX SHARED %08lX\n",
                      AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                      ProcessID);

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
                              "AFSBuildTargetBranch Acquiring GlobalRoot DirectoryNodeHdr.TreeLock lock %08lX EXCL %08lX\n",
                              AFSGlobalRoot->Specific.Directory.DirectoryNodeHdr.TreeLock,
                              ProcessID);

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

                        ntStatus = AFSCheckCellName( ProcessID,
                                                     &uniComponentName,
                                                     &pShareDirEntry);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_WARNING,
                                          "AFSBuildTargetBranch Cell name %wZ not found\n",
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
                      "AFSBuildTargetBranch Acquiring ShareEntry DirNode lock %08lX EXCL %08lX\n",
                      &pShareDirEntry->NPDirNode->Lock,
                      ProcessID);

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

                    ntStatus = AFSInitRootFcb( ProcessID,
                                               pShareDirEntry,
                                               &pShareDirEntry->Fcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSBuildTargetBranch Failed to initialize root fcb for cell name %wZ\n",
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
                                      "AFSBuildTargetBranch Failed to initialize fcb for cell name %wZ\n",
                                      &uniComponentName);

                        AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);

                        try_return( ntStatus);
                    }
                }
            }

            InterlockedIncrement( &pShareDirEntry->Fcb->OpenReferenceCount);

            AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);
        }
        else
        {

            AFSReleaseResource( &pShareDirEntry->NPDirNode->Lock);

            //
            // Grab the root node exclusive before returning
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSBuildTargetBranch Acquiring ShareEntry Fcb lock %08lX EXCL %08lX\n",
                          &pShareDirEntry->Fcb->NPFcb->Resource,
                          ProcessID);

            AFSAcquireExcl( &pShareDirEntry->Fcb->NPFcb->Resource,
                            TRUE);
        }

        //
        // Loop until we get to the root node
        //

        pCurrentFcb = pShareDirEntry->Fcb;

        while( bContinueProcessing)
        {

            //
            // If this node has become invalid then fail the request now
            //

            if( BooleanFlagOn( pCurrentFcb->Flags, AFS_FCB_INVALID))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSBuildTargetBranch Fcb for cell name %wZ invalid\n",
                              &uniComponentName);

                //
                // The symlink or share entry has been deleted
                //

                AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                try_return( ntStatus = STATUS_OBJECT_PATH_INVALID);
            }

            //
            // Ensure the root node has been evaluated, if not then go do it now
            //

            if( BooleanFlagOn( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSBuildTargetBranch Evaluating cell %wZ\n",
                              &uniComponentName);

                ntStatus = AFSEvaluateNode( ProcessID,
                                            pCurrentFcb);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSBuildTargetBranch Failed to evaluate cell %wZ Status %08lX\n",
                                  &uniComponentName,
                                  ntStatus);

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    try_return( ntStatus);
                }

                ClearFlag( pCurrentFcb->DirEntry->Flags, AFS_DIR_ENTRY_NOT_EVALUATED);
            }

            //
            // Be sure we return a root or a directory
            //

            switch ( pCurrentFcb->DirEntry->DirectoryEntry.FileType) 
            {

                case AFS_FILE_TYPE_SYMLINK:
                {
                
                    //
                    // Build the symlink target
                    //

                    ntStatus = AFSBuildSymLinkTarget( ProcessID,
                                                      pCurrentFcb,
                                                      FileObject,
                                                      pNameArray,
                                                      Flags,
                                                      NULL,
                                                      &pTargetFcb);

                    if( !NT_SUCCESS( ntStatus) ||
                        ntStatus == STATUS_REPARSE)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSBuildTargetBranch Failed to build SL target for parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                      ntStatus);

                        try_return( ntStatus);
                    }

                    ASSERT( pTargetFcb != NULL);                    

                    pCurrentFcb = pTargetFcb;

                    pTargetFcb = NULL;

                    continue;
                }

                case AFS_FILE_TYPE_MOUNTPOINT:
                {

                    //
                    // Check if we have a target Fcb for this node
                    //

                    if( pCurrentFcb->Specific.MountPoint.VolumeTargetFcb == NULL)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSBuildTargetBranch Building target for MP %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                        //
                        // Go retrieve the target entry for this node
                        //

                        ntStatus = AFSBuildMountPointTarget( ProcessID,
                                                             pCurrentFcb);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSBuildTargetBranch Failed to build target for MP %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                          ntStatus);

                            AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                            try_return( ntStatus);
                        }
                    }

                    //
                    // Swap out where we are in the chain
                    //

                    pTargetFcb = pCurrentFcb->Specific.MountPoint.VolumeTargetFcb;

                    AFSAcquireExcl( &pTargetFcb->NPFcb->Resource,
                                    TRUE);

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    pCurrentFcb = pTargetFcb;

                    pTargetFcb = NULL;

                    AFSReplaceCurrentElement( pNameArray,
                                              pCurrentFcb);

                    continue;
                }

                case AFS_FILE_TYPE_DFSLINK:
                {

                    //
                    // This is a DFS link so we need to update the file name and return STATUS_REPARSE to the
                    // system for it to reevaluate the link
                    //

                    if( FileObject != NULL)
                    {

                        ntStatus = AFSProcessDFSLink( pCurrentFcb,
                                                      FileObject,
                                                      &uniRemainingPath);
                    }
                    else
                    {

                        //
                        // This is where we have an evaluation coming in from the NP dll
                        // and have encountered a DFS link in the name mapping ...
                        //

                        ntStatus = STATUS_INVALID_PARAMETER;
                    }

                    AFSReleaseResource( &pCurrentFcb->NPFcb->Resource);

                    try_return( ntStatus);
                }
        
                default:
                {

                    pRootVcb = pCurrentFcb;

                    ntStatus = AFSInsertNextElement( pNameArray,
                                                     pCurrentFcb);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSReleaseResource( &pRootVcb->NPFcb->Resource);

                        try_return( ntStatus);
                    }
               
                    if( BooleanFlagOn( pRootVcb->Flags, AFS_FCB_VOLUME_OFFLINE))
                    {

                        //
                        // The volume has been taken off line so fail the access
                        //

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSBuildTargetBranch Root volume OFFLINE for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);


                        AFSReleaseResource( &pRootVcb->NPFcb->Resource);

                        try_return( ntStatus = STATUS_DEVICE_NOT_READY);               
                    }

                    //
                    // Check if the root requires verification due to an invalidation call
                    //

                    if( BooleanFlagOn( pRootVcb->Flags, AFS_FCB_VERIFY))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSBuildTargetBranch Verifying root volume %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                      pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique);

                        ntStatus = AFSVerifyEntry( ProcessID,
                                                   pRootVcb);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSBuildTargetBranch Failed to verify root volume %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          &pCurrentFcb->DirEntry->DirectoryEntry.FileName,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Cell,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Volume,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                                          pCurrentFcb->DirEntry->DirectoryEntry.FileId.Unique,
                                          ntStatus);

                            AFSReleaseResource( &pRootVcb->NPFcb->Resource);

                            try_return( ntStatus);
                        }
                    }

                    bContinueProcessing = FALSE;

                    break;
                }
            }
        } // End While()

        //
        // At this point the remaining part of the path is in uniRemainingPath and we have the
        // root held exclusively.
        // First check of there is nothing remaining in the path, or just a \. In this case we are
        // looking for the root of the volume so we are good to go
        //

        if( uniRemainingPath.Length == 0 ||
            ( uniRemainingPath.Length == sizeof( WCHAR) &&
              uniRemainingPath.Buffer[ 0] == L'\\'))
        {

            //
            // We are good to go, return the root fcb and we're done
            //

            *TargetFcb = pRootVcb;

            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // OK, we have a remaining component so we need to build up the branch from this point.
        //

        pParentFcb = pRootVcb;

        ntStatus = AFSLocateNameEntry( ProcessID,
                                       FileObject,
                                       &uniRemainingPath,
                                       &uniRemainingPath,
                                       pNameArray,
                                       Flags,
                                       &pParentFcb,
                                       TargetFcb,
                                       NULL);

        if( !NT_SUCCESS( ntStatus) ||
            ntStatus == STATUS_REPARSE)
        {

            if( ntStatus == STATUS_OBJECT_NAME_NOT_FOUND &&
                ExIsResourceAcquiredLite( &pParentFcb->NPFcb->Resource))  // We need to check since we could be re-entered ...
            {

                //
                // Still have the parent ...
                //

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);
            }

            try_return( ntStatus);
        }

        if( *TargetFcb == NULL)
        {

            *TargetFcb = pParentFcb;
        }
        else if( ExIsResourceAcquiredLite( &pParentFcb->NPFcb->Resource))
        {

            //
            // Release locks 
            //

            AFSReleaseResource( &pParentFcb->NPFcb->Resource);
        }

try_exit:

        if( pNameArray != NULL)
        {

            AFSFreeNameArray( pNameArray);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSBuildRelativeTargetBranch( IN ULONGLONG ProcessID,
                              IN AFSNameArrayHdr *NameArray,
                              IN PFILE_OBJECT FileObject,
                              IN UNICODE_STRING *TargetName,
                              OUT AFSFcb **TargetFcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSNameArrayCB *pCurrentElement = NULL;
    UNICODE_STRING uniFullName = *TargetName;
    UNICODE_STRING uniComponentName, uniRemainingPath;
    UNICODE_STRING uniRelativeName;
    AFSFcb *pCurrentFcb = NULL, *pParentFcb = NULL;
    UNICODE_STRING uniNoOpName;

    __Enter
    {

        RtlInitUnicodeString( &uniRelativeName,
                              L"..");

        RtlInitUnicodeString( &uniNoOpName,
                              L".");

        pCurrentElement = NameArray->CurrentEntry;

        pCurrentFcb = pCurrentElement->Fcb;

        //
        // Need to move relative to where we are in the name array
        // based upon the target name
        //

        while( TRUE)
        {

            FsRtlDissectName( uniFullName,
                              &uniComponentName,
                              &uniRemainingPath);            

            //
            // If this is going up an entry move
            //

            if( RtlCompareUnicodeString( &uniComponentName,
                                         &uniRelativeName,
                                         TRUE) != 0)
            {

                break;
            }

            //
            // Check for the . in the path
            //

            if( RtlCompareUnicodeString( &uniComponentName,
                                         &uniNoOpName,
                                         TRUE) == 0)
            {

                uniFullName = uniRemainingPath;

                continue;
            }

            uniFullName = uniRemainingPath;

            //
            // Keep the initial \ in the name for easier processing lower
            //

            uniFullName.Length += sizeof( WCHAR);
            uniFullName.MaximumLength += sizeof (WCHAR);
            uniFullName.Buffer--;

            //
            // We are backing up an entry in the name array so
            // dereference the entry
            //

            InterlockedDecrement( &pCurrentElement->Fcb->OpenReferenceCount);

            pCurrentElement->Fcb = NULL;

            InterlockedDecrement( &NameArray->Count);

            if( pCurrentElement == &NameArray->TopElement[ 0])
            {

                if( uniRemainingPath.Length == 0)
                {

                    pCurrentFcb = NameArray->TopElement[ 0].Fcb;

                    break;
                }
              
                FsRtlDissectName( uniFullName,
                                  &uniComponentName,
                                  &uniRemainingPath);

                if( RtlCompareUnicodeString( &uniComponentName,
                                             &uniRelativeName,
                                             TRUE) != 0)
                {

                    pCurrentFcb = NameArray->TopElement[ 0].Fcb;

                    break;
                }

                //
                // We can't go back any further ..
                //

                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }

            pCurrentElement--;

            NameArray->CurrentEntry = pCurrentElement;

            pCurrentFcb = pCurrentElement->Fcb;

            if( uniRemainingPath.Length == 0)
            {

                break;
            }
        }

        //
        // If we have no more name to process, we are done
        //

        if( uniRemainingPath.Length == 0 ||
            ( uniRemainingPath.Length == sizeof( WCHAR) &&
              uniRemainingPath.Buffer[ 0] == L'\\'))
        {

            //
            // Acquire the lock on the Fcb before returning
            //

            AFSAcquireExcl( &pCurrentFcb->NPFcb->Resource,
                            TRUE);

            *TargetFcb = pCurrentFcb;

            try_return( ntStatus);
        }
        
        //
        // More entries to process in the name so now walk this out to the end. 
        // We do this by noting it is similar to a relative open in the normal
        // processing of a name so we can set up the root and parent accordingly
        // and pass it into the LocateNameEntry() routine ...
        //

        pParentFcb = pCurrentFcb;

        AFSAcquireExcl( &pParentFcb->NPFcb->Resource,
                        TRUE);

        ntStatus = AFSLocateNameEntry( ProcessID,
                                       FileObject,
                                       &uniFullName,
                                       &uniFullName,
                                       NameArray,
                                       0,
                                       &pParentFcb,
                                       TargetFcb,
                                       NULL);

        if( !NT_SUCCESS( ntStatus) ||
            ntStatus == STATUS_REPARSE)
        {

            if( ntStatus == STATUS_OBJECT_NAME_NOT_FOUND &&
                ExIsResourceAcquiredLite( &pParentFcb->NPFcb->Resource))  // We need to check since we could be re-entered ...)
            {

                //
                // Still have the parent ...
                //

                AFSReleaseResource( &pParentFcb->NPFcb->Resource);
            }

            try_return( ntStatus);
        }

        //
        // Release the locks ...
        //

        AFSReleaseResource( &pParentFcb->NPFcb->Resource);

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSBuildRootName( IN AFSFcb *StartFcb,
                  IN UNICODE_STRING *AdditionalComponent,
                  OUT UNICODE_STRING *CompleteName)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniFullName;
    USHORT usFullNameLength = 0, usCurrentIndex = 0;
    AFSFcb *pCurrentFcb = NULL;
    BOOLEAN bRootStart = FALSE;

    __Enter
    {

        //
        // First, determine how big the buffer needs to be to hold the full name
        //

        if( StartFcb->ParentFcb == NULL)
        {

            bRootStart = TRUE;
        }
        else
        {

            pCurrentFcb = StartFcb;

            while( TRUE)
            {

                usFullNameLength += pCurrentFcb->DirEntry->DirectoryEntry.FileName.Length;

                if( pCurrentFcb->ParentFcb == NULL)
                {

                    break;
                }

                //
                // Add in space for the slash ...
                //

                usFullNameLength += sizeof( WCHAR);

                pCurrentFcb = pCurrentFcb->ParentFcb;
            }
        }

        //
        // Add in the length of the additional component ...
        //

        if( AdditionalComponent != NULL)
        {

            usFullNameLength += AdditionalComponent->Length;

            if( AdditionalComponent->Buffer[ 0] != L'\\')
            {

                //
                // And a separator
                //

                usFullNameLength += sizeof( WCHAR);
            }
        }

        //
        // Now populate the name buffer
        //

        CompleteName->Length = 0;
        CompleteName->MaximumLength = usFullNameLength;

        CompleteName->Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                               CompleteName->MaximumLength,
                                                               AFS_GENERIC_MEMORY_TAG);

        if( CompleteName->Buffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Copy in the name going backwards
        //

        usCurrentIndex = (CompleteName->MaximumLength/sizeof( WCHAR));

        if( !bRootStart)
        {

            usCurrentIndex--;
        }

        if( AdditionalComponent != NULL)
        {

            usCurrentIndex -= AdditionalComponent->Length/sizeof(WCHAR);

            RtlCopyMemory( &CompleteName->Buffer[ usCurrentIndex],
                           AdditionalComponent->Buffer,
                           AdditionalComponent->Length);

            CompleteName->Length = AdditionalComponent->Length;

            if( AdditionalComponent->Buffer[ 0] != L'\\')
            {

                usCurrentIndex--;

                CompleteName->Buffer[ usCurrentIndex] = L'\\';

                CompleteName->Length += sizeof( WCHAR);
            }
        }

        //
        // If we are starting at the root we are done ...
        //

        if( bRootStart)
        {

            try_return( ntStatus);
        }

        pCurrentFcb = StartFcb;

        while( TRUE)
        {

            usCurrentIndex -= pCurrentFcb->DirEntry->DirectoryEntry.FileName.Length/sizeof( WCHAR);

            RtlCopyMemory( &CompleteName->Buffer[ usCurrentIndex],
                           pCurrentFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                           pCurrentFcb->DirEntry->DirectoryEntry.FileName.Length);

            CompleteName->Length += pCurrentFcb->DirEntry->DirectoryEntry.FileName.Length;
                           
            if( CompleteName->Buffer[ usCurrentIndex] != L'\\')
            {

                //
                // Add in the \
                //

                usCurrentIndex--;

                CompleteName->Buffer[ usCurrentIndex] = L'\\';

                CompleteName->Length += sizeof( WCHAR);
            }

            pCurrentFcb = pCurrentFcb->ParentFcb;

            if( pCurrentFcb == NULL ||
                pCurrentFcb->ParentFcb == NULL)
            {

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}