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
// File: AFSNameSupport.cpp
//

#include "AFSCommon.h"

//
// AFSLocateNameEntry
//
// On entry, *VolumeCB must have a held ReferenceCount provided by
// the caller which will be released.  On successful exit, *VolumeCB
// will be assigned the new current volume with a held ReferenceCount.
//
// On entry, *ParentDirectoryCB must have a held DirOpenReferenceCount
// provided by the caller.
//

NTSTATUS
AFSLocateNameEntry( IN GUID *AuthGroup,
                    IN PFILE_OBJECT FileObject,
                    IN UNICODE_STRING *RootPathName,
                    IN UNICODE_STRING *ParsedPathName,
                    IN AFSNameArrayHdr *NameArray,
                    IN ULONG Flags,
                    IN OUT AFSVolumeCB **VolumeCB,
                    IN OUT LONG *pVolumeReferenceReason,
                    IN OUT AFSDirectoryCB **ParentDirectoryCB,
                    OUT AFSDirectoryCB **DirectoryCB,
                    OUT PUNICODE_STRING ComponentName)
{

    NTSTATUS          ntStatus = STATUS_SUCCESS;
    UNICODE_STRING    uniPathName, uniComponentName, uniRemainingPath, uniSearchName, uniFullPathName;
    ULONG             ulCRC = 0;
    AFSDirectoryCB   *pDirEntry = NULL, *pParentDirEntry = NULL;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    UNICODE_STRING    uniSysName;
    ULONG             ulSubstituteIndex = 0;
    BOOLEAN           bSubstituteName = FALSE;
    AFSNameArrayHdr  *pNameArray = NameArray;
    BOOLEAN           bAllocatedSymLinkBuffer = FALSE;
    UNICODE_STRING    uniRelativeName, uniNoOpName;
    AFSObjectInfoCB  *pCurrentObject = NULL;
    AFSObjectInfoCB  *pParentObjectInfo = NULL;
    AFSVolumeCB      *pCurrentVolume = *VolumeCB;
    AFSVolumeCB      *pTargetVolume = NULL;
    BOOLEAN           bReleaseCurrentVolume = TRUE;
    LONG              VolumeReferenceReason = *pVolumeReferenceReason;
    BOOLEAN           bSubstitutedName = FALSE;
    LONG              lCount;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSLocateNameEntry (FO: %p) Processing full name %wZ\n",
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

        pParentDirEntry = NULL;

        pDirEntry = *ParentDirectoryCB;

        uniPathName = *ParsedPathName;

        uniFullPathName = *RootPathName;

        uniComponentName.Length = uniComponentName.MaximumLength = 0;
        uniComponentName.Buffer = NULL;

        uniRemainingPath.Length = uniRemainingPath.MaximumLength = 0;
        uniRemainingPath.Buffer = NULL;

        uniSearchName.Length = uniSearchName.MaximumLength = 0;
        uniSearchName.Buffer = NULL;

        while( TRUE)
        {

            ASSERT( pCurrentVolume->VolumeReferenceCount > 0);

            ASSERT( pDirEntry->DirOpenReferenceCount > 0);

            //
            // Check our total link count for this name array
            //

            if( pNameArray->LinkCount >= (LONG)pDevExt->Specific.RDR.MaxLinkCount)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            pCurrentObject = pDirEntry->ObjectInformation;

            KeQueryTickCount( &pCurrentObject->LastAccessCount);

            //
            // Check that the directory entry is not deleted or pending delete
            //

            if( BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_DELETED))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSLocateNameEntry (FO: %p) Deleted parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                              FileObject,
                              &pDirEntry->NameInformation.FileName,
                              pCurrentObject->FileId.Cell,
                              pCurrentObject->FileId.Volume,
                              pCurrentObject->FileId.Vnode,
                              pCurrentObject->FileId.Unique,
                              STATUS_FILE_DELETED);

                try_return( ntStatus = STATUS_FILE_DELETED);
            }

            if( BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_PENDING_DELETE))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSLocateNameEntry (FO: %p) Delete pending on %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                              FileObject,
                              &pDirEntry->NameInformation.FileName,
                              pCurrentObject->FileId.Cell,
                              pCurrentObject->FileId.Volume,
                              pCurrentObject->FileId.Vnode,
                              pCurrentObject->FileId.Unique,
                              STATUS_DELETE_PENDING);

                try_return( ntStatus = STATUS_DELETE_PENDING);
            }

            //
            // Check if the directory requires verification
            //

            if( BooleanFlagOn( pCurrentObject->Flags, AFS_OBJECT_FLAGS_VERIFY) &&
                ( pCurrentObject->FileType != AFS_FILE_TYPE_DIRECTORY ||
                  !AFSIsEnumerationInProcess( pCurrentObject)))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %p) Verifying parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              FileObject,
                              &pDirEntry->NameInformation.FileName,
                              pCurrentObject->FileId.Cell,
                              pCurrentObject->FileId.Volume,
                              pCurrentObject->FileId.Vnode,
                              pCurrentObject->FileId.Unique);

                //
                // Directory TreeLock should be exclusively held
                //

                AFSAcquireExcl( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                TRUE);

                ntStatus = AFSVerifyEntry( AuthGroup,
                                           pDirEntry);

                AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %p) Failed to verify parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                  FileObject,
                                  &pDirEntry->NameInformation.FileName,
                                  pCurrentObject->FileId.Cell,
                                  pCurrentObject->FileId.Volume,
                                  pCurrentObject->FileId.Vnode,
                                  pCurrentObject->FileId.Unique,
                                  ntStatus);

                    try_return( ntStatus);
                }
            }

            //
            // Ensure the parent node has been evaluated, if not then go do it now
            //

            if( BooleanFlagOn( pDirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_NOT_EVALUATED) ||
                pCurrentObject->FileType == AFS_FILE_TYPE_UNKNOWN)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %p) Evaluating parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              FileObject,
                              &pDirEntry->NameInformation.FileName,
                              pCurrentObject->FileId.Cell,
                              pCurrentObject->FileId.Volume,
                              pCurrentObject->FileId.Vnode,
                              pCurrentObject->FileId.Unique);

                ntStatus = AFSEvaluateNode( AuthGroup,
                                            pDirEntry);

                if( !NT_SUCCESS( ntStatus))
                {

                    if ( ntStatus == STATUS_NOT_A_DIRECTORY)
                    {

                        if ( !BooleanFlagOn( pCurrentObject->Flags, AFS_OBJECT_FLAGS_PARENT_FID))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry (FO: %p) Failed to evaluate object %wZ FID %08lX-%08lX-%08lX-%08lX PARENT NULL Status %08lX\n",
                                          FileObject,
                                          &pDirEntry->NameInformation.FileName,
                                          pCurrentObject->FileId.Cell,
                                          pCurrentObject->FileId.Volume,
                                          pCurrentObject->FileId.Vnode,
                                          pCurrentObject->FileId.Unique,
                                          ntStatus);
                        }
                        else
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry (FO: %p) Failed to evaluate object %wZ FID %08lX-%08lX-%08lX-%08lX PARENT %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          FileObject,
                                          &pDirEntry->NameInformation.FileName,
                                          pCurrentObject->FileId.Cell,
                                          pCurrentObject->FileId.Volume,
                                          pCurrentObject->FileId.Vnode,
                                          pCurrentObject->FileId.Unique,
                                          pCurrentObject->ParentFileId.Cell,
                                          pCurrentObject->ParentFileId.Volume,
                                          pCurrentObject->ParentFileId.Vnode,
                                          pCurrentObject->ParentFileId.Unique,
                                          ntStatus);
                        }
                    }
                    else
                    {
                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %p) Failed to evaluate parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      FileObject,
                                      &pDirEntry->NameInformation.FileName,
                                      pCurrentObject->FileId.Cell,
                                      pCurrentObject->FileId.Volume,
                                      pCurrentObject->FileId.Vnode,
                                      pCurrentObject->FileId.Unique,
                                      ntStatus);
                    }

                    try_return( ntStatus);
                }

                ClearFlag( pCurrentObject->Flags, AFS_OBJECT_FLAGS_NOT_EVALUATED);
            }

            //
            // If this is a mount point or symlink then go get the real directory node
            //

            switch( pCurrentObject->FileType)
            {

                case AFS_FILE_TYPE_SYMLINK:
                {

                    UNICODE_STRING uniTempName;
                    WCHAR *pTmpBuffer = NULL;
                    LONG lLinkCount = 0;

                    //
                    // Check if the flag is set to NOT evaluate a symlink
                    // and we are done with the parsing
                    //

                    if( BooleanFlagOn( Flags, AFS_LOCATE_FLAGS_NO_SL_TARGET_EVAL) &&
                        uniRemainingPath.Length == 0)
                    {

                        //
                        // Pass back the directory entries
                        //

                        *ParentDirectoryCB = pParentDirEntry;

                        *DirectoryCB = pDirEntry;

                        *VolumeCB = pCurrentVolume;

                        *RootPathName = uniFullPathName;

                        try_return( ntStatus);
                    }

                    AFSAcquireExcl( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                    TRUE);

                    AFSAcquireExcl( &pDirEntry->NonPaged->Lock,
                                    TRUE);

                    if( pDirEntry->NameInformation.TargetName.Length == 0)
                    {

                        //
                        // We'll reset the DV to ensure we validate the metadata content
                        //

                        pCurrentObject->DataVersion.QuadPart = (ULONGLONG)-1;

                        SetFlag( pCurrentObject->Flags, AFS_OBJECT_FLAGS_VERIFY);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry (FO: %p) Verifying symlink parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pDirEntry->NameInformation.FileName,
                                      pCurrentObject->FileId.Cell,
                                      pCurrentObject->FileId.Volume,
                                      pCurrentObject->FileId.Vnode,
                                      pCurrentObject->FileId.Unique);

                        //
                        // Directory TreeLock should be exclusively held
                        //

                        ntStatus = AFSVerifyEntry( AuthGroup,
                                                   pDirEntry);

                        AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry (FO: %p) Failed to verify symlink parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          FileObject,
                                          &pDirEntry->NameInformation.FileName,
                                          pCurrentObject->FileId.Cell,
                                          pCurrentObject->FileId.Volume,
                                          pCurrentObject->FileId.Vnode,
                                          pCurrentObject->FileId.Unique,
                                          ntStatus);

                            AFSReleaseResource( &pDirEntry->NonPaged->Lock);

                            try_return( ntStatus);
                        }

                        //
                        // If the type changed then reprocess this entry
                        //

                        if( pCurrentObject->FileType != AFS_FILE_TYPE_SYMLINK)
                        {

                            AFSReleaseResource( &pDirEntry->NonPaged->Lock);

                            continue;
                        }
                    }
                    else
                    {

                        AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);
                    }

                    //
                    // If we were given a zero length target name then deny access to the entry
                    //

                    if( pDirEntry->NameInformation.TargetName.Length == 0)
                    {

                        ntStatus = STATUS_REPARSE_POINT_NOT_RESOLVED;

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %p) Failed to retrieve target name for symlink %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      FileObject,
                                      &pDirEntry->NameInformation.FileName,
                                      pCurrentObject->FileId.Cell,
                                      pCurrentObject->FileId.Volume,
                                      pCurrentObject->FileId.Vnode,
                                      pCurrentObject->FileId.Unique,
                                      ntStatus);

                        AFSReleaseResource( &pDirEntry->NonPaged->Lock);

                        try_return( ntStatus);
                    }

                    if( AFSIsRelativeName( &pDirEntry->NameInformation.TargetName))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry (FO: %p) Processing relative symlink target %wZ for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pDirEntry->NameInformation.TargetName,
                                      &pDirEntry->NameInformation.FileName,
                                      pCurrentObject->FileId.Cell,
                                      pCurrentObject->FileId.Volume,
                                      pCurrentObject->FileId.Vnode,
                                      pCurrentObject->FileId.Unique);

                        //
                        // We'll substitute this name into the current process name
                        // starting at where we sit in the path
                        //

                        uniTempName.Length = 0;
                        uniTempName.MaximumLength = (USHORT)((char *)uniComponentName.Buffer - (char *)uniFullPathName.Buffer) +
                                                                    pDirEntry->NameInformation.TargetName.Length +
                                                                    sizeof( WCHAR) +
                                                                    uniRemainingPath.Length;

                        uniTempName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                                uniTempName.MaximumLength,
                                                                                AFS_NAME_BUFFER_ONE_TAG);

                        if( uniTempName.Buffer == NULL)
                        {

                            AFSReleaseResource( &pDirEntry->NonPaged->Lock);

                            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                        }

                        //
                        // We have so first copy in the portion up to the component
                        // name
                        //

                        RtlCopyMemory( uniTempName.Buffer,
                                       uniFullPathName.Buffer,
                                       (ULONG)((char *)uniComponentName.Buffer - (char *)uniFullPathName.Buffer));

                        uniTempName.Length = (USHORT)((char *)uniComponentName.Buffer - (char *)uniFullPathName.Buffer);

                        if( bAllocatedSymLinkBuffer ||
                            bSubstitutedName)
                        {

                            pTmpBuffer = uniFullPathName.Buffer;
                        }

                        bAllocatedSymLinkBuffer = TRUE;

                        //
                        // Have we parsed this name yet? Better have at least once ...
                        //

                        if( uniComponentName.Length == 0)
                        {
                            ASSERT( FALSE);
                        }

                        //
                        // Copy in the target name ...
                        //

                        RtlCopyMemory( &uniTempName.Buffer[ uniTempName.Length/sizeof( WCHAR)],
                                       pDirEntry->NameInformation.TargetName.Buffer,
                                       pDirEntry->NameInformation.TargetName.Length);

                        uniPathName.Buffer = &uniTempName.Buffer[ uniTempName.Length/sizeof( WCHAR)];

                        uniPathName.Length += pDirEntry->NameInformation.TargetName.Length;
                        uniPathName.MaximumLength = uniTempName.MaximumLength;

                        uniTempName.Length += pDirEntry->NameInformation.TargetName.Length;

                        //
                        // And now any remaining portion of the name
                        //

                        if( uniRemainingPath.Length > 0)
                        {

                            if( uniRemainingPath.Buffer[ 0] != L'\\')
                            {

                                uniRemainingPath.Buffer--;
                                uniRemainingPath.Length += sizeof( WCHAR);

                                uniPathName.Length += sizeof( WCHAR);
                            }

                            RtlCopyMemory( &uniTempName.Buffer[ uniTempName.Length/sizeof( WCHAR)],
                                           uniRemainingPath.Buffer,
                                           uniRemainingPath.Length);

                            uniTempName.Length += uniRemainingPath.Length;
                        }

                        uniFullPathName = uniTempName;

                        if( pTmpBuffer != NULL)
                        {

                            AFSExFreePoolWithTag( pTmpBuffer, 0);
                        }

                        AFSReleaseResource( &pDirEntry->NonPaged->Lock);

                        //
                        // Dereference the current entry ..
                        //

                        lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry Decrement1 count on %wZ DE %p Ccb %p Cnt %d\n",
                                      &pDirEntry->NameInformation.FileName,
                                      pDirEntry,
                                      NULL,
                                      lCount);

                        ASSERT( lCount >= 0);

                        //
                        // OK, need to back up one entry for the correct parent since the current
                        // entry we are on is the symlink itself
                        //

                        pDirEntry = AFSBackupEntry( pNameArray);

                        //
                        // Increment our reference on this dir entry
                        //

                        lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry Increment1 count on %wZ DE %p Ccb %p Cnt %d\n",
                                      &pDirEntry->NameInformation.FileName,
                                      pDirEntry,
                                      NULL,
                                      lCount);

                        if( BooleanFlagOn( pDirEntry->ObjectInformation->Flags, AFS_OBJECT_ROOT_VOLUME))
                        {

                            pParentDirEntry = NULL;
                        }
                        else
                        {

                            pParentDirEntry = AFSGetParentEntry( pNameArray);

                            ASSERT( pParentDirEntry != pDirEntry);
                        }
                    }
                    else
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry (FO: %p) Processing absolute symlink target %wZ for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pDirEntry->NameInformation.TargetName,
                                      &pDirEntry->NameInformation.FileName,
                                      pCurrentObject->FileId.Cell,
                                      pCurrentObject->FileId.Volume,
                                      pCurrentObject->FileId.Vnode,
                                      pCurrentObject->FileId.Unique);

                        if ( !AFSIsAbsoluteAFSName( &pDirEntry->NameInformation.TargetName))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry Name %wZ contains invalid server name\n",
                                          &pDirEntry->NameInformation.TargetName);

                            //
                            // The correct response would be STATUS_OBJECT_PATH_INVALID
                            // but that prevents cmd.exe from performing a recursive
                            // directory enumeration when opening a directory entry
                            // that represents a symlink to an invalid path is discovered.
                            //

                            AFSReleaseResource( &pDirEntry->NonPaged->Lock);

                            try_return( ntStatus = STATUS_OBJECT_PATH_NOT_FOUND);
                        }

                        //
                        // We'll substitute this name into the current process name
                        // starting at where we sit in the path
                        //

                        uniTempName.Length = 0;
                        uniTempName.MaximumLength = pDirEntry->NameInformation.TargetName.Length +
                                                                    sizeof( WCHAR) +
                                                                    uniRemainingPath.Length;

                        uniTempName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                                uniTempName.MaximumLength,
                                                                                AFS_NAME_BUFFER_TWO_TAG);

                        if( uniTempName.Buffer == NULL)
                        {

                            AFSReleaseResource( &pDirEntry->NonPaged->Lock);

                            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                        }

                        if( bAllocatedSymLinkBuffer ||
                            bSubstitutedName)
                        {

                            pTmpBuffer = uniFullPathName.Buffer;
                        }

                        bAllocatedSymLinkBuffer = TRUE;

                        //
                        // Have we parsed this name yet? Better have at least once ...
                        //

                        if( uniComponentName.Length == 0)
                        {
                            ASSERT( FALSE);
                        }

                        //
                        // Copy in the target name ...
                        //

                        RtlCopyMemory( uniTempName.Buffer,
                                       &pDirEntry->NameInformation.TargetName.Buffer[ AFSMountRootName.Length/sizeof( WCHAR)],
                                       pDirEntry->NameInformation.TargetName.Length - AFSMountRootName.Length);

                        uniTempName.Length = pDirEntry->NameInformation.TargetName.Length - AFSMountRootName.Length;

                        //
                        // And now any remaining portion of the name
                        //

                        if( uniRemainingPath.Length > 0)
                        {

                            if( uniRemainingPath.Buffer[ 0] != L'\\')
                            {

                                uniRemainingPath.Buffer--;
                                uniRemainingPath.Length += sizeof( WCHAR);
                            }

                            RtlCopyMemory( &uniTempName.Buffer[ uniTempName.Length/sizeof( WCHAR)],
                                           uniRemainingPath.Buffer,
                                           uniRemainingPath.Length);

                            uniTempName.Length += uniRemainingPath.Length;
                        }

                        uniFullPathName = uniTempName;

                        uniPathName = uniTempName;

                        if( pTmpBuffer != NULL)
                        {

                            AFSExFreePoolWithTag( pTmpBuffer, 0);
                        }

                        AFSReleaseResource( &pDirEntry->NonPaged->Lock);

                        //
                        // If our current volume is not the global root then make it so ...
                        //

                        if( pCurrentVolume != AFSGlobalRoot)
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSLocateNameEntry (FO: %p) Current volume not global, resetting for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                          FileObject,
                                          &pDirEntry->NameInformation.FileName,
                                          pCurrentObject->FileId.Cell,
                                          pCurrentObject->FileId.Volume,
                                          pCurrentObject->FileId.Vnode,
                                          pCurrentObject->FileId.Unique);

                            lCount = AFSVolumeDecrement( pCurrentVolume,
                                                         VolumeReferenceReason);

                            AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSLocateNameEntry Decrement count on volume %p Reason %u Cnt %d\n",
                                          pCurrentVolume,
                                          VolumeReferenceReason,
                                          lCount);

                            pCurrentVolume = AFSGlobalRoot;

                            VolumeReferenceReason = AFS_VOLUME_REFERENCE_LOCATE_NAME;

                            lCount = AFSVolumeIncrement( pCurrentVolume,
                                                         VolumeReferenceReason);

                            AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSLocateNameEntry Increment count on volume %p Reason %u Cnt %d\n",
                                          pCurrentVolume,
                                          VolumeReferenceReason,
                                          lCount);
                        }

                        //
                        // Dereference our current dir entry
                        //

                        lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry Decrement2 count on %wZ DE %p Ccb %p Cnt %d\n",
                                      &pDirEntry->NameInformation.FileName,
                                      pDirEntry,
                                      NULL,
                                      lCount);

                        ASSERT( lCount >= 0);

                        pDirEntry = pCurrentVolume->DirectoryCB;

                        //
                        // Reference the new dir entry
                        //

                        lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry Increment2 count on %wZ DE %p Ccb %p Cnt %d\n",
                                      &pDirEntry->NameInformation.FileName,
                                      pDirEntry,
                                      NULL,
                                      lCount);

                        //
                        // Reset the name array
                        // Persist the link count in the name array
                        //

                        lLinkCount = pNameArray->LinkCount;

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry (FO: %p) Resetting name array for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pDirEntry->NameInformation.FileName,
                                      pCurrentObject->FileId.Cell,
                                      pCurrentObject->FileId.Volume,
                                      pCurrentObject->FileId.Vnode,
                                      pCurrentObject->FileId.Unique);

                        AFSResetNameArray( pNameArray,
                                           pDirEntry);

                        pNameArray->LinkCount = lLinkCount;

                        pParentDirEntry = NULL;
                    }

                    //
                    // Increment our link count
                    //

                    lCount = InterlockedIncrement( &pNameArray->LinkCount);

                    continue;
                }

                case AFS_FILE_TYPE_MOUNTPOINT:
                {

                    //
                    // Check if the flag is set to NOT evaluate a mount point
                    // and we are done with the parsing
                    //

                    if( BooleanFlagOn( Flags, AFS_LOCATE_FLAGS_NO_MP_TARGET_EVAL) &&
                        uniRemainingPath.Length == 0)
                    {

                        //
                        // Pass back the directory entries
                        //

                        *ParentDirectoryCB = pParentDirEntry;

                        *DirectoryCB = pDirEntry;

                        *VolumeCB = pCurrentVolume;

                        *RootPathName = uniFullPathName;

                        try_return( ntStatus);
                    }

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %p) Building MP target for parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  FileObject,
                                  &pDirEntry->NameInformation.FileName,
                                  pCurrentObject->FileId.Cell,
                                  pCurrentObject->FileId.Volume,
                                  pCurrentObject->FileId.Vnode,
                                  pCurrentObject->FileId.Unique);

                    //
                    // Go retrieve the target entry for this node
                    // Release the current volume cb entry since we would
                    // have lock inversion in the following call
                    // Also decrement the ref count on the volume
                    //

                    ntStatus = AFSBuildMountPointTarget( AuthGroup,
                                                         pDirEntry,
                                                         &pTargetVolume);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %p) Failed to build MP target for parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      FileObject,
                                      &pDirEntry->NameInformation.FileName,
                                      pCurrentObject->FileId.Cell,
                                      pCurrentObject->FileId.Volume,
                                      pCurrentObject->FileId.Vnode,
                                      pCurrentObject->FileId.Unique,
                                      ntStatus);

                        try_return( ntStatus);
                    }

                    ASSERT( pCurrentVolume->VolumeReferenceCount > 0);

                    lCount = AFSVolumeDecrement( pCurrentVolume,
                                                 VolumeReferenceReason);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry Decrement2 count on volume %p Reason %u Cnt %d\n",
                                  pCurrentVolume,
                                  VolumeReferenceReason,
                                  lCount);

                    pCurrentVolume = pTargetVolume;

                    pTargetVolume = NULL;

                    ASSERT( pCurrentVolume->VolumeReferenceCount > 0);

                    VolumeReferenceReason = AFS_VOLUME_REFERENCE_MOUNTPT;

                    //
                    // We want to restart processing here on the new parent ...
                    // Deref and ref count the entries
                    //

                    lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry Decrement3 count on %wZ DE %p Ccb %p Cnt %d\n",
                                  &pDirEntry->NameInformation.FileName,
                                  pDirEntry,
                                  NULL,
                                  lCount);

                    ASSERT( lCount >= 0);

                    pDirEntry = pCurrentVolume->DirectoryCB;

                    lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry Increment3 count on %wZ DE %p Ccb %p Cnt %d\n",
                                  &pDirEntry->NameInformation.FileName,
                                  pDirEntry,
                                  NULL,
                                  lCount);

                    //
                    // The name array stores both the mount point and the target.
                    // Insert the target.
                    //

                    AFSInsertNextElement( pNameArray,
                                          pDirEntry);

                    pParentDirEntry = NULL;

                    //
                    // Increment our link count
                    //

                    lCount = InterlockedIncrement( &pNameArray->LinkCount);

                    continue;
                }

                case AFS_FILE_TYPE_DFSLINK:
                {

                    if( BooleanFlagOn( Flags, AFS_LOCATE_FLAGS_NO_DFS_LINK_EVAL))
                    {

                        //
                        // Pass back the directory entries
                        //

                        *ParentDirectoryCB = pParentDirEntry;

                        *DirectoryCB = pDirEntry;

                        *VolumeCB = pCurrentVolume;

                        *RootPathName = uniFullPathName;

                        try_return( ntStatus);
                    }

                    //
                    // This is a DFS link so we need to update the file name and return STATUS_REPARSE to the
                    // system for it to reevaluate it
                    //

                    if( FileObject != NULL)
                    {

                        ntStatus = AFSProcessDFSLink( pDirEntry,
                                                      FileObject,
                                                      &uniRemainingPath,
                                                      AuthGroup);
                    }
                    else
                    {

                        //
                        // This is where we have been re-entered from an NP evaluation call via the BuildBranch()
                        // routine.
                        //

                        ntStatus = STATUS_INVALID_PARAMETER;
                    }

                    if( ntStatus != STATUS_SUCCESS &&
                        ntStatus != STATUS_REPARSE)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %p) Failed to process DFSLink parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      FileObject,
                                      &pDirEntry->NameInformation.FileName,
                                      pCurrentObject->FileId.Cell,
                                      pCurrentObject->FileId.Volume,
                                      pCurrentObject->FileId.Vnode,
                                      pCurrentObject->FileId.Unique,
                                      ntStatus);
                    }

                    try_return( ntStatus);
                }

                case AFS_FILE_TYPE_UNKNOWN:
                case AFS_FILE_TYPE_INVALID:
                {

                    //
                    // Something was not processed ...
                    //

                    try_return( ntStatus = STATUS_ACCESS_DENIED);
                }

            }   /* end of switch */

            //
            // If the parent is not initialized then do it now
            //

            if( pCurrentObject->FileType == AFS_FILE_TYPE_DIRECTORY &&
                !BooleanFlagOn( pCurrentObject->Flags, AFS_OBJECT_FLAGS_DIRECTORY_ENUMERATED))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %p) Enumerating parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              FileObject,
                              &pDirEntry->NameInformation.FileName,
                              pCurrentObject->FileId.Cell,
                              pCurrentObject->FileId.Volume,
                              pCurrentObject->FileId.Vnode,
                              pCurrentObject->FileId.Unique);

                AFSAcquireExcl( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                TRUE);

                if( !BooleanFlagOn( pCurrentObject->Flags, AFS_OBJECT_FLAGS_DIRECTORY_ENUMERATED))
                {

                    ntStatus = AFSEnumerateDirectory( AuthGroup,
                                                      pCurrentObject,
                                                      TRUE);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %p) Failed to enumerate parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      FileObject,
                                      &pDirEntry->NameInformation.FileName,
                                      pCurrentObject->FileId.Cell,
                                      pCurrentObject->FileId.Volume,
                                      pCurrentObject->FileId.Vnode,
                                      pCurrentObject->FileId.Unique,
                                      ntStatus);

                        try_return( ntStatus);
                    }

                    SetFlag( pDirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_DIRECTORY_ENUMERATED);
                }

                AFSReleaseResource( pCurrentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);
            }
            else if( pCurrentObject->FileType == AFS_FILE_TYPE_FILE)
            {

                if( uniPathName.Length > 0)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %p) Encountered file node %wZ FID %08lX-%08lX-%08lX-%08lX in path processing\n",
                                  FileObject,
                                  &pDirEntry->NameInformation.FileName,
                                  pCurrentObject->FileId.Cell,
                                  pCurrentObject->FileId.Volume,
                                  pCurrentObject->FileId.Vnode,
                                  pCurrentObject->FileId.Unique);

                    // The proper error code to return would be STATUS_OBJECT_PATH_INVALID because
                    // one of the components of the path is not a directory.  However, returning
                    // that error prevents IIS 7 and 7.5 from being able to serve data out of AFS.
                    // Instead IIS insists on treating the target file as if it is a directory containing
                    // a potential web.config file.  NTFS and LanMan return STATUS_OBJECT_PATH_NOT_FOUND.
                    // AFS will follow suit.

                    ntStatus = STATUS_OBJECT_PATH_NOT_FOUND;
                }
                else
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %p) Returning file %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  FileObject,
                                  &pDirEntry->NameInformation.FileName,
                                  pCurrentObject->FileId.Cell,
                                  pCurrentObject->FileId.Volume,
                                  pCurrentObject->FileId.Vnode,
                                  pCurrentObject->FileId.Unique);

                    //
                    // Pass back the directory entries
                    //

                    *ParentDirectoryCB = pParentDirEntry;

                    *DirectoryCB = pDirEntry;

                    *VolumeCB = pCurrentVolume;

                    *RootPathName = uniFullPathName;
                }

                try_return( ntStatus);
            }

            //
            // If we are at the end of the processing, set our returned information and get out
            //

            if( uniPathName.Length == 0)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %p) Completed processing returning %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              FileObject,
                              &pDirEntry->NameInformation.FileName,
                              pCurrentObject->FileId.Cell,
                              pCurrentObject->FileId.Volume,
                              pCurrentObject->FileId.Vnode,
                              pCurrentObject->FileId.Unique);

                //
                // Pass back the directory entries
                //

                *ParentDirectoryCB = pParentDirEntry;

                *DirectoryCB = pDirEntry;

                *VolumeCB = pCurrentVolume;

                *RootPathName = uniFullPathName;

                try_return( ntStatus);
            }

            //
            // We may have returned to the top of the while( TRUE)
            //
            if( bSubstituteName &&
                uniSearchName.Buffer != NULL)
            {

                AFSExFreePoolWithTag( uniSearchName.Buffer, 0);

                bSubstituteName = FALSE;

                uniSearchName.Length = uniSearchName.MaximumLength = 0;
                uniSearchName.Buffer = NULL;
            }

            ulSubstituteIndex = 1;

            ntStatus = STATUS_SUCCESS;

            //
            // Get the next component name
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

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry (FO: %p) Backing up entry from %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              FileObject,
                              &pDirEntry->NameInformation.FileName,
                              pCurrentObject->FileId.Cell,
                              pCurrentObject->FileId.Volume,
                              pCurrentObject->FileId.Vnode,
                              pCurrentObject->FileId.Unique);

                //
                // Need to back up one entry in the name array
                //
                lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry Decrement4 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &pDirEntry->NameInformation.FileName,
                              pDirEntry,
                              NULL,
                              lCount);

                ASSERT( lCount >= 0);

                pDirEntry = AFSBackupEntry( NameArray);

                if( pDirEntry == NULL)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry AFSBackupEntry failed\n");

                    try_return(ntStatus = STATUS_OBJECT_PATH_INVALID);
                }

                lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry Increment4 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &pDirEntry->NameInformation.FileName,
                              pDirEntry,
                              NULL,
                              lCount);

                if( BooleanFlagOn( pDirEntry->ObjectInformation->Flags, AFS_OBJECT_ROOT_VOLUME))
                {

                    pParentDirEntry = NULL;
                }
                else
                {

                    pParentDirEntry = AFSGetParentEntry( pNameArray);

                    ASSERT( pParentDirEntry != pDirEntry);
                }

                uniPathName = uniRemainingPath;

                continue;
            }

            //
            // Update our pointers
            //

            pParentDirEntry = pDirEntry;

            pDirEntry = NULL;

            uniSearchName = uniComponentName;

            while( pDirEntry == NULL)
            {

                //
                // If the SearchName contains @SYS then we perform the substitution.
                // If there is no substitution we give up.
                //

                if( !bSubstituteName &&
                    FsRtlIsNameInExpression( &uniSysName,
                                             &uniSearchName,
                                             TRUE,
                                             NULL))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSLocateNameEntry (FO: %p) Processing @SYS substitution for %wZ Index %08lX\n",
                                  FileObject,
                                  &uniComponentName,
                                  ulSubstituteIndex);

                    ntStatus = AFSSubstituteSysName( &uniComponentName,
                                                     &uniSearchName,
                                                     ulSubstituteIndex);

                    if ( NT_SUCCESS( ntStatus))
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE_2,
                                      "AFSLocateNameEntry (FO: %p) Located substitution %wZ for %wZ Index %08lX\n",
                                      FileObject,
                                      &uniSearchName,
                                      &uniComponentName,
                                      ulSubstituteIndex);

                        //
                        // Go reparse the name again
                        //

                        bSubstituteName = TRUE;

                        ulSubstituteIndex++; // For the next entry, if needed

                        continue;   // while( pDirEntry == NULL)
                    }
                    else
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSLocateNameEntry (FO: %p) Failed to locate substitute string for %wZ Index %08lX Status %08lX\n",
                                      FileObject,
                                      &uniComponentName,
                                      ulSubstituteIndex,
                                      ntStatus);

                        if( ntStatus == STATUS_OBJECT_NAME_NOT_FOUND)
                        {

                            //
                            // Pass back the directory entries
                            //

                            *ParentDirectoryCB = pParentDirEntry;

                            *DirectoryCB = NULL;

                            *VolumeCB = pCurrentVolume;

                            if( ComponentName != NULL)
                            {

                                *ComponentName = uniComponentName;
                            }

                            *RootPathName = uniFullPathName;
                        }

                        //
                        // We can't possibly have a pDirEntry since the lookup failed
                        //
                        try_return( ntStatus);
                    }
                }

                //
                // Generate the CRC on the node and perform a case sensitive lookup
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSLocateNameEntry (FO: %p) Searching for entry %wZ case sensitive\n",
                              FileObject,
                              &uniSearchName);

                ulCRC = AFSGenerateCRC( &uniSearchName,
                                        FALSE);

                AFSAcquireShared( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                  TRUE);

                AFSLocateCaseSensitiveDirEntry( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                ulCRC,
                                                &pDirEntry);

                if( pDirEntry == NULL)
                {

                    //
                    // Missed so perform a case insensitive lookup
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSLocateNameEntry (FO: %p) Searching for entry %wZ case insensitive\n",
                                  FileObject,
                                  &uniSearchName);

                    ulCRC = AFSGenerateCRC( &uniSearchName,
                                            TRUE);

                    AFSLocateCaseInsensitiveDirEntry( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                                      ulCRC,
                                                      &pDirEntry);

                    if( pDirEntry == NULL)
                    {

                        //
                        // OK, if this component is a valid short name then try
                        // a lookup in the short name tree
                        //

                        if( !BooleanFlagOn( pDevExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
                            RtlIsNameLegalDOS8Dot3( &uniSearchName,
                                                    NULL,
                                                    NULL))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE_2,
                                          "AFSLocateNameEntry (FO: %p) Searching for entry %wZ short name\n",
                                          FileObject,
                                          &uniSearchName);

                            AFSLocateShortNameDirEntry( pParentDirEntry->ObjectInformation->Specific.Directory.ShortNameTree,
                                                        ulCRC,
                                                        &pDirEntry);
                        }

                        if ( pDirEntry == NULL &&
                             pParentDirEntry->ObjectInformation->VolumeCB == AFSGlobalRoot)
                        {

                            //
                            // Check with the service to see if this is a valid cell name
                            // that can be automatically resolved.  Drop the shared TreeLock
                            // since AFSCheckCellName must acquire it exclusively.
                            //

                            AFSReleaseResource( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

                            ntStatus = AFSCheckCellName( AuthGroup,
                                                         &uniSearchName,
                                                         &pDirEntry);

                            AFSAcquireShared( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                              TRUE);
                        }

                        if( pDirEntry == NULL)
                        {

                            //
                            // If we substituted a name then reset our search name and try again
                            //

                            if( bSubstituteName)
                            {

                                AFSExFreePoolWithTag( uniSearchName.Buffer, 0);

                                uniSearchName = uniComponentName;

                                bSubstituteName = FALSE;

                                AFSReleaseResource( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

                                continue;       // while( pDirEntry == NULL)
                            }

                            if( uniRemainingPath.Length > 0)
                            {

                                ntStatus = STATUS_OBJECT_PATH_NOT_FOUND;

                                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                              AFS_TRACE_LEVEL_VERBOSE,
                                              "AFSLocateNameEntry (FO: %p) Returning path not found for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                              FileObject,
                                              &uniSearchName,
                                              pCurrentObject->FileId.Cell,
                                              pCurrentObject->FileId.Volume,
                                              pCurrentObject->FileId.Vnode,
                                              pCurrentObject->FileId.Unique);
                            }
                            else
                            {

                                ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;

                                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                              AFS_TRACE_LEVEL_VERBOSE,
                                              "AFSLocateNameEntry (FO: %p) Returning name not found for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                              FileObject,
                                              &uniSearchName,
                                              pCurrentObject->FileId.Cell,
                                              pCurrentObject->FileId.Volume,
                                              pCurrentObject->FileId.Vnode,
                                              pCurrentObject->FileId.Unique);

                                //
                                // Pass back the directory entries
                                //

                                *ParentDirectoryCB = pParentDirEntry;

                                *DirectoryCB = NULL;

                                *VolumeCB = pCurrentVolume;

                                if( ComponentName != NULL)
                                {

                                    *ComponentName = uniComponentName;
                                }

                                *RootPathName = uniFullPathName;
                            }

                            AFSReleaseResource( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

                            //
                            // Node name not found so get out
                            //

                            try_return( ntStatus);  // while( pDirEntry == NULL)
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

                            //
                            // Increment our dir entry ref count since we will decrement it on exit
                            //

                            lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

                            AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSLocateNameEntry Increment5 count on %wZ DE %p Ccb %p Cnt %d\n",
                                          &pDirEntry->NameInformation.FileName,
                                          pDirEntry,
                                          NULL,
                                          lCount);

                            AFSReleaseResource( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

                            try_return(ntStatus = STATUS_OBJECT_NAME_COLLISION);
                        }
                    }
                }

                if( pDirEntry != NULL)
                {

                    //
                    // If the verify flag is set on the parent and the current entry is deleted
                    // revalidate the parent and search again.
                    //

                    if( BooleanFlagOn( pDirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_DELETED) &&
                        BooleanFlagOn( pParentDirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY))
                    {

                        AFSReleaseResource( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry (FO: %p) Verifying(2) parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      FileObject,
                                      &pParentDirEntry->NameInformation.FileName,
                                      pParentDirEntry->ObjectInformation->FileId.Cell,
                                      pParentDirEntry->ObjectInformation->FileId.Volume,
                                      pParentDirEntry->ObjectInformation->FileId.Vnode,
                                      pParentDirEntry->ObjectInformation->FileId.Unique);

                        //
                        // Directory TreeLock should be exclusively held
                        //

                        AFSAcquireExcl( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                        TRUE);

                        ntStatus = AFSVerifyEntry( AuthGroup,
                                                   pParentDirEntry);

                        AFSReleaseResource( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

                        if( !NT_SUCCESS( ntStatus))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_ERROR,
                                          "AFSLocateNameEntry (FO: %p) Failed to verify(2) parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                          FileObject,
                                          &pParentDirEntry->NameInformation.FileName,
                                          pParentDirEntry->ObjectInformation->FileId.Cell,
                                          pParentDirEntry->ObjectInformation->FileId.Volume,
                                          pParentDirEntry->ObjectInformation->FileId.Vnode,
                                          pParentDirEntry->ObjectInformation->FileId.Unique,
                                          ntStatus);

                            try_return( ntStatus);
                        }

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNameEntry (FO: %p) Reprocessing component %wZ in parent %wZ\n",
                                      FileObject,
                                      &uniSearchName,
                                      &pParentDirEntry->NameInformation.FileName);


                        pDirEntry = NULL;

                        continue;
                    }

                    //
                    // Increment our dir entry ref count
                    //

                    lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry Increment5 count on %wZ DE %p Ccb %p Cnt %d\n",
                                  &pDirEntry->NameInformation.FileName,
                                  pDirEntry,
                                  NULL,
                                  lCount);
                }

                AFSReleaseResource( pParentDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

            } // End while( pDirEntry == NULL)

            //
            // If we have a dirEntry for this component, perform some basic validation on it
            //

            if( pDirEntry != NULL &&
                BooleanFlagOn( pDirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_DELETED))
            {

                pCurrentObject = pDirEntry->ObjectInformation;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSLocateNameEntry (FO: %p) Deleted entry %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              FileObject,
                              &pDirEntry->NameInformation.FileName,
                              pCurrentObject->FileId.Cell,
                              pCurrentObject->FileId.Volume,
                              pCurrentObject->FileId.Vnode,
                              pCurrentObject->FileId.Unique);

                //
                // This entry was deleted through the invalidation call back so perform cleanup
                // on the entry
                //

                if( BooleanFlagOn( pCurrentObject->Flags, AFS_OBJECT_FLAGS_PARENT_FID))
                {

                    pParentObjectInfo = AFSFindObjectInfo( pCurrentObject->VolumeCB,
                                                           &pCurrentObject->ParentFileId);
                }

                ASSERT( pParentObjectInfo != NULL);

                AFSAcquireExcl( pParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                TRUE);

                AFSAcquireExcl( pCurrentObject->VolumeCB->ObjectInfoTree.TreeLock,
                                TRUE);

                lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry Decrement count on %wZ DE %p Ccb %p Cnt %d\n",
                              &pDirEntry->NameInformation.FileName,
                              pDirEntry,
                              NULL,
                              lCount);

                ASSERT( lCount >= 0);

                if( lCount <= 0)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING|AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry Deleting dir entry %p (%p) for %wZ\n",
                                  pDirEntry,
                                  pCurrentObject,
                                  &pDirEntry->NameInformation.FileName);

                    //
                    // Remove and delete the directory entry from the parent list
                    //

                    AFSDeleteDirEntry( pParentObjectInfo,
                                       pDirEntry);

                    AFSAcquireShared( &pCurrentObject->NonPagedInfo->ObjectInfoLock,
                                      TRUE);

                    if( pCurrentObject->ObjectReferenceCount <= 0)
                    {

                        if( BooleanFlagOn( pCurrentObject->Flags, AFS_OBJECT_INSERTED_HASH_TREE))
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_CLEANUP_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSLocateNameEntry Removing object %p from volume tree\n",
                                          pCurrentObject);

                            AFSRemoveHashEntry( &pCurrentObject->VolumeCB->ObjectInfoTree.TreeHead,
                                                &pCurrentObject->TreeEntry);

                            ClearFlag( pCurrentObject->Flags, AFS_OBJECT_INSERTED_HASH_TREE);
                        }
                    }

                    AFSReleaseResource( &pCurrentObject->NonPagedInfo->ObjectInfoLock);
                }
                else
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry Setting DELETE flag in dir entry %p for %wZ\n",
                                  pDirEntry,
                                  &pDirEntry->NameInformation.FileName);

                    SetFlag( pDirEntry->Flags, AFS_DIR_ENTRY_DELETED);

                    AFSRemoveNameEntry( pParentObjectInfo,
                                        pDirEntry);
                }

                AFSReleaseResource( pParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);

                AFSReleaseResource( pCurrentObject->VolumeCB->ObjectInfoTree.TreeLock);

                AFSReleaseObjectInfo( &pParentObjectInfo);

                //
                // We deleted the dir entry so check if there is any remaining portion
                // of the name to process.
                //

                if( uniRemainingPath.Length > 0)
                {

                    ntStatus = STATUS_OBJECT_PATH_NOT_FOUND;

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %p) Returning path not found(2) for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  FileObject,
                                  &uniComponentName,
                                  pCurrentObject->FileId.Cell,
                                  pCurrentObject->FileId.Volume,
                                  pCurrentObject->FileId.Vnode,
                                  pCurrentObject->FileId.Unique);
                }
                else
                {

                    ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNameEntry (FO: %p) Returning name not found(2) for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  FileObject,
                                  &uniComponentName,
                                  pCurrentObject->FileId.Cell,
                                  pCurrentObject->FileId.Volume,
                                  pCurrentObject->FileId.Vnode,
                                  pCurrentObject->FileId.Unique);

                    //
                    // Pass back the directory entries
                    //

                    *ParentDirectoryCB = pParentDirEntry;

                    *DirectoryCB = NULL;

                    *VolumeCB = pCurrentVolume;

                    if( ComponentName != NULL)
                    {

                        *ComponentName = uniComponentName;
                    }

                    *RootPathName = uniFullPathName;
                }
            }

            if( ntStatus != STATUS_SUCCESS)
            {

                try_return( ntStatus);
            }

            //
            // Decrement the previous parent
            //

            lCount = InterlockedDecrement( &pParentDirEntry->DirOpenReferenceCount);

            AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSLocateNameEntry Decrement5 count on Parent %wZ DE %p Ccb %p Cnt %d\n",
                          &pParentDirEntry->NameInformation.FileName,
                          pParentDirEntry,
                          NULL,
                          lCount);

            ASSERT( lCount >= 0);

            //
            // If we ended up substituting a name in the component then update
            // the full path and update the pointers
            //

            if( bSubstituteName)
            {

                BOOLEAN bRelativeOpen = FALSE;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSLocateNameEntry (FO: %p) Substituting %wZ into %wZ Index %08lX\n",
                              FileObject,
                              &uniSearchName,
                              &uniComponentName,
                              ulSubstituteIndex);

                if( FileObject != NULL &&
                    FileObject->RelatedFileObject != NULL)
                {

                    bRelativeOpen = TRUE;
                }

                //
                // AFSSubstituteNameInPath will replace the uniFullPathName.Buffer
                // and free the prior Buffer contents but only if the fourth
                // parameter is TRUE.
                //

                ntStatus = AFSSubstituteNameInPath( &uniFullPathName,
                                                    &uniComponentName,
                                                    &uniSearchName,
                                                    &uniRemainingPath,
                                                    bRelativeOpen ||
                                                            bAllocatedSymLinkBuffer ||
                                                            bSubstitutedName);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %p) Failure to substitute %wZ into %wZ Index %08lX Status %08lX\n",
                                  FileObject,
                                  &uniSearchName,
                                  &uniComponentName,
                                  ulSubstituteIndex,
                                  ntStatus);

                    try_return( ntStatus);
                }

                //
                // We have substituted a name into the buffer so if we do this again for this
                // path, we need to free up the buffer we allocated.
                //

                bSubstitutedName = TRUE;
            }

            //
            // Update the search parameters
            //

            uniPathName = uniRemainingPath;

            //
            // Check if the is a SymLink entry but has no Target FileID or Name. In this
            // case it might be a DFS Link so let's go and evaluate it to be sure
            //

            if( pCurrentObject->FileType == AFS_FILE_TYPE_SYMLINK &&
                ( pCurrentObject->TargetFileId.Vnode == 0 ||
                  pDirEntry->NameInformation.TargetName.Length == 0))
            {

                ntStatus = AFSValidateSymLink( AuthGroup,
                                               pDirEntry);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLocateNameEntry (FO: %p) Failed to evaluate possible DFS Link %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                  FileObject,
                                  &pDirEntry->NameInformation.FileName,
                                  pCurrentObject->FileId.Cell,
                                  pCurrentObject->FileId.Volume,
                                  pCurrentObject->FileId.Vnode,
                                  pCurrentObject->FileId.Unique,
                                  ntStatus);

                    try_return( ntStatus);
                }
            }

            //
            // Update the name array
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSLocateNameEntry (FO: %p) Inserting name array entry %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                          FileObject,
                          &pDirEntry->NameInformation.FileName,
                          pCurrentObject->FileId.Cell,
                          pCurrentObject->FileId.Volume,
                          pCurrentObject->FileId.Vnode,
                          pCurrentObject->FileId.Unique);

            ntStatus = AFSInsertNextElement( pNameArray,
                                             pDirEntry);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }
        }       // while (TRUE)

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSLocateNameEntry (FO: %p) Completed processing %wZ Status %08lX\n",
                      FileObject,
                      RootPathName,
                      ntStatus);

        if( ( !NT_SUCCESS( ntStatus) &&
              ntStatus != STATUS_OBJECT_NAME_NOT_FOUND) ||
            ntStatus == STATUS_REPARSE)
        {

            if( pDirEntry != NULL)
            {

                lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry Decrement6 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &pDirEntry->NameInformation.FileName,
                              pDirEntry,
                              NULL,
                              lCount);

                ASSERT( lCount >= 0);
            }
            else if( pParentDirEntry != NULL)
            {

                lCount = InterlockedDecrement( &pParentDirEntry->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry Decrement7 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &pParentDirEntry->NameInformation.FileName,
                              pParentDirEntry,
                              NULL,
                              lCount);

                ASSERT( lCount >= 0);
            }

            if( bReleaseCurrentVolume)
            {

                ASSERT( pCurrentVolume->VolumeReferenceCount > 0);

                lCount = AFSVolumeDecrement( pCurrentVolume,
                                             VolumeReferenceReason);

                AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry Decrement3 count on volume %p Reason %u Cnt %d\n",
                              pCurrentVolume,
                              VolumeReferenceReason,
                              lCount);

                bReleaseCurrentVolume = FALSE;
            }

            if( RootPathName->Buffer != uniFullPathName.Buffer)
            {

                AFSExFreePoolWithTag( uniFullPathName.Buffer, 0);
            }
        }
        else
        {

            if( *ParentDirectoryCB != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry Count on Parent %wZ DE %p Ccb %p Cnt %d\n",
                              &(*ParentDirectoryCB)->NameInformation.FileName,
                              *ParentDirectoryCB,
                              NULL,
                              (*ParentDirectoryCB)->DirOpenReferenceCount);
            }

            if( *DirectoryCB != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNameEntry Count on %wZ DE %p Ccb %p Cnt %d\n",
                              &(*DirectoryCB)->NameInformation.FileName,
                              *DirectoryCB,
                              NULL,
                              (*DirectoryCB)->DirOpenReferenceCount);
            }
        }

        if( bSubstituteName &&
            uniSearchName.Buffer != NULL)
        {

            AFSExFreePoolWithTag( uniSearchName.Buffer, 0);
        }

        if ( bReleaseCurrentVolume) {

            *pVolumeReferenceReason = VolumeReferenceReason;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSCreateDirEntry( IN GUID            *AuthGroup,
                   IN AFSObjectInfoCB *ParentObjectInfo,
                   IN AFSDirectoryCB *ParentDirCB,
                   IN PUNICODE_STRING FileName,
                   IN PUNICODE_STRING ComponentName,
                   IN ULONG Attributes,
                   IN OUT AFSDirectoryCB **DirEntry)
{

    UNREFERENCED_PARAMETER(FileName);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB *pDirNode = NULL, *pExistingDirNode = NULL;
    LARGE_INTEGER liFileSize = {0,0};
    LONG lCount;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSCreateDirEntry Creating dir entry in parent %wZ FID %08lX-%08lX-%08lX-%08lX Component %wZ Attribs %08lX\n",
                      &ParentDirCB->NameInformation.FileName,
                      ParentObjectInfo->FileId.Cell,
                      ParentObjectInfo->FileId.Volume,
                      ParentObjectInfo->FileId.Vnode,
                      ParentObjectInfo->FileId.Unique,
                      ComponentName,
                      Attributes);

        //
        // OK, before inserting the node into the parent tree, issue
        // the request to the service for node creation
        // We will need to drop the lock on the parent node since the create
        // could cause a callback into the file system to invalidate it's cache
        //

        ntStatus = AFSNotifyFileCreate( AuthGroup,
                                        ParentObjectInfo,
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
                          &ParentDirCB->NameInformation.FileName,
                          ParentObjectInfo->FileId.Cell,
                          ParentObjectInfo->FileId.Volume,
                          ParentObjectInfo->FileId.Vnode,
                          ParentObjectInfo->FileId.Unique,
                          ComponentName,
                          Attributes,
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // If AFSNotifyFileCreate returns pDirNode != NULL, then its
        // DirOpenReferenceCount is held.
        //

        AFSAcquireExcl( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        //
        // Before attempting to insert the new entry, check if we need to validate the parent
        //

        if( BooleanFlagOn( ParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCreateDirEntry Verifying parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                          &ParentDirCB->NameInformation.FileName,
                          ParentObjectInfo->FileId.Cell,
                          ParentObjectInfo->FileId.Volume,
                          ParentObjectInfo->FileId.Vnode,
                          ParentObjectInfo->FileId.Unique);

            ntStatus = AFSVerifyEntry( AuthGroup,
                                       ParentDirCB);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSCreateDirEntry Failed to verify parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                              &ParentDirCB->NameInformation.FileName,
                              ParentObjectInfo->FileId.Cell,
                              ParentObjectInfo->FileId.Volume,
                              ParentObjectInfo->FileId.Vnode,
                              ParentObjectInfo->FileId.Unique,
                              ntStatus);

                AFSReleaseResource( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);

                try_return( ntStatus);
            }
        }

        //
        // Check for the entry in the event we raced with some other thread
        //

        AFSLocateCaseSensitiveDirEntry( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                        (ULONG)pDirNode->CaseSensitiveTreeEntry.HashIndex,
                                        &pExistingDirNode);

        if( pExistingDirNode != NULL)
        {
            if (AFSIsEqualFID( &pDirNode->ObjectInformation->FileId,
                               &pExistingDirNode->ObjectInformation->FileId))
            {

                if ( pExistingDirNode != pDirNode)
                {

                    lCount = InterlockedDecrement( &pDirNode->DirOpenReferenceCount);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCreateDirEntry Decrement count on %wZ DE %p Cnt %d\n",
                                  &pDirNode->NameInformation.FileName,
                                  pDirNode,
                                  lCount);

                    AFSDeleteDirEntry( ParentObjectInfo,
                                       pDirNode);

                    lCount = InterlockedIncrement( &pExistingDirNode->DirOpenReferenceCount);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCreateDirEntry Increment count on %wZ DE %p Cnt %d\n",
                                  &pExistingDirNode->NameInformation.FileName,
                                  pExistingDirNode,
                                  lCount);

                    *DirEntry = pExistingDirNode;
                }

                AFSReleaseResource( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);

                try_return( ntStatus = STATUS_SUCCESS);
            }
            else
            {

                //
                // Need to tear down this entry and rebuild it below
                //

                if( pExistingDirNode->DirOpenReferenceCount <= 0)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCreateDirEntry Different FIDs - Deleting DE %p for %wZ Old FID %08lX-%08lX-%08lX-%08lX New FID %08lX-%08lX-%08lX-%08lX\n",
                                  pExistingDirNode,
                                  &pExistingDirNode->NameInformation.FileName,
                                  pExistingDirNode->ObjectInformation->FileId.Cell,
                                  pExistingDirNode->ObjectInformation->FileId.Volume,
                                  pExistingDirNode->ObjectInformation->FileId.Vnode,
                                  pExistingDirNode->ObjectInformation->FileId.Unique,
                                  pDirNode->ObjectInformation->FileId.Cell,
                                  pDirNode->ObjectInformation->FileId.Volume,
                                  pDirNode->ObjectInformation->FileId.Vnode,
                                  pDirNode->ObjectInformation->FileId.Unique);

                    AFSDeleteDirEntry( ParentObjectInfo,
                                       pExistingDirNode);
                }
                else
                {

                    SetFlag( pExistingDirNode->Flags, AFS_DIR_ENTRY_DELETED);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSCreateDirEntry Different FIDs - Removing DE %p for %wZ Old FID %08lX-%08lX-%08lX-%08lX New FID %08lX-%08lX-%08lX-%08lX\n",
                                  pExistingDirNode,
                                  &pExistingDirNode->NameInformation.FileName,
                                  pExistingDirNode->ObjectInformation->FileId.Cell,
                                  pExistingDirNode->ObjectInformation->FileId.Volume,
                                  pExistingDirNode->ObjectInformation->FileId.Vnode,
                                  pExistingDirNode->ObjectInformation->FileId.Unique,
                                  pDirNode->ObjectInformation->FileId.Cell,
                                  pDirNode->ObjectInformation->FileId.Volume,
                                  pDirNode->ObjectInformation->FileId.Vnode,
                                  pDirNode->ObjectInformation->FileId.Unique);

                    AFSRemoveNameEntry( ParentObjectInfo,
                                        pExistingDirNode);
                }
            }
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSCreateDirEntry Inserting dir entry in parent %wZ FID %08lX-%08lX-%08lX-%08lX Component %wZ\n",
                      &ParentDirCB->NameInformation.FileName,
                      ParentObjectInfo->FileId.Cell,
                      ParentObjectInfo->FileId.Volume,
                      ParentObjectInfo->FileId.Vnode,
                      ParentObjectInfo->FileId.Unique,
                      ComponentName);

        //
        // Insert the directory node
        //

        AFSInsertDirectoryNode( ParentObjectInfo,
                                pDirNode,
                                TRUE);

        //
        // Pass back the dir entry
        //

        *DirEntry = pDirNode;

        AFSReleaseResource( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);

try_exit:

        NOTHING;
    }

    return ntStatus;
}

void
AFSInsertDirectoryNode( IN AFSObjectInfoCB *ParentObjectInfo,
                        IN AFSDirectoryCB *DirEntry,
                        IN BOOLEAN InsertInEnumList)
{

    LONG lCount;

    __Enter
    {

        ASSERT( ExIsResourceAcquiredExclusiveLite( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock));

        //
        // Insert the node into the directory node tree
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInsertDirectoryNode Insert DE %p for %wZ Clearing NOT_IN flag\n",
                      DirEntry,
                      &DirEntry->NameInformation.FileName);

        ClearFlag( DirEntry->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE);

        if( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead == NULL)
        {

            ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead = DirEntry;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInsertDirectoryNode Insert DE %p to head of case sensitive tree for %wZ\n",
                          DirEntry,
                          &DirEntry->NameInformation.FileName);
        }
        else
        {

            AFSInsertCaseSensitiveDirEntry( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                            DirEntry);

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInsertDirectoryNode Insert DE %p to case sensitive tree for %wZ\n",
                          DirEntry,
                          &DirEntry->NameInformation.FileName);
        }

        if( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead == NULL)
        {

            ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead = DirEntry;

            SetFlag( DirEntry->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInsertDirectoryNode Insert DE %p to head of case insensitive tree for %wZ\n",
                          DirEntry,
                          &DirEntry->NameInformation.FileName);
        }
        else
        {

            AFSInsertCaseInsensitiveDirEntry( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                              DirEntry);

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInsertDirectoryNode Insert DE %p to case insensitive tree for %wZ\n",
                          DirEntry,
                          &DirEntry->NameInformation.FileName);
        }

        //
        // Into the shortname tree
        //

        if( DirEntry->Type.Data.ShortNameTreeEntry.HashIndex != 0)
        {

            if( ParentObjectInfo->Specific.Directory.ShortNameTree == NULL)
            {

                ParentObjectInfo->Specific.Directory.ShortNameTree = DirEntry;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInsertDirectoryNode Insert DE %p to head of shortname tree for %wZ\n",
                              DirEntry,
                              &DirEntry->NameInformation.FileName);

                SetFlag( DirEntry->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME);
            }
            else
            {

                if( !NT_SUCCESS( AFSInsertShortNameDirEntry( ParentObjectInfo->Specific.Directory.ShortNameTree,
                                                             DirEntry)))
                {
                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSInsertDirectoryNode Failed to insert DE %p to shortname tree for %wZ\n",
                                  DirEntry,
                                  &DirEntry->NameInformation.FileName);
                }
                else
                {
                    SetFlag( DirEntry->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSInsertDirectoryNode Insert DE %p to shortname tree for %wZ\n",
                                  DirEntry,
                                  &DirEntry->NameInformation.FileName);
                }
            }
        }

        if( InsertInEnumList)
        {

            //
            // And insert the node into the directory list
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInsertDirectoryNode Inserting entry %p %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                          DirEntry,
                          &DirEntry->NameInformation.FileName,
                          DirEntry->ObjectInformation->FileId.Cell,
                          DirEntry->ObjectInformation->FileId.Volume,
                          DirEntry->ObjectInformation->FileId.Vnode,
                          DirEntry->ObjectInformation->FileId.Unique);

            if( ParentObjectInfo->Specific.Directory.DirectoryNodeListHead == NULL)
            {

                ParentObjectInfo->Specific.Directory.DirectoryNodeListHead = DirEntry;
            }
            else
            {

                ParentObjectInfo->Specific.Directory.DirectoryNodeListTail->ListEntry.fLink = (void *)DirEntry;

                DirEntry->ListEntry.bLink = (void *)ParentObjectInfo->Specific.Directory.DirectoryNodeListTail;
            }

            ParentObjectInfo->Specific.Directory.DirectoryNodeListTail = DirEntry;

            SetFlag( DirEntry->Flags, AFS_DIR_ENTRY_INSERTED_ENUM_LIST);

            lCount = InterlockedIncrement( &ParentObjectInfo->Specific.Directory.DirectoryNodeCount);

            AFSDbgLogMsg( AFS_SUBSYSTEM_DIR_NODE_COUNT,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSInsertDirectoryNode Adding entry %wZ Inc Count %d to parent FID %08lX-%08lX-%08lX-%08lX\n",
                          &DirEntry->NameInformation.FileName,
                          lCount,
                          ParentObjectInfo->FileId.Cell,
                          ParentObjectInfo->FileId.Volume,
                          ParentObjectInfo->FileId.Vnode,
                          ParentObjectInfo->FileId.Unique);
        }
    }

    return;
}

void
AFSDeleteDirEntry( IN AFSObjectInfoCB *ParentObjectInfo,
                   IN AFSDirectoryCB *DirEntry)
{

    LONG lCount;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING | AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSDeleteDirEntry Deleting dir entry in parent %p Entry %p %wZ FID %08lX-%08lX-%08lX-%08lX RefCount %d\n",
                      ParentObjectInfo,
                      DirEntry,
                      &DirEntry->NameInformation.FileName,
                      DirEntry->ObjectInformation->FileId.Cell,
                      DirEntry->ObjectInformation->FileId.Volume,
                      DirEntry->ObjectInformation->FileId.Vnode,
                      DirEntry->ObjectInformation->FileId.Unique,
                      DirEntry->DirOpenReferenceCount);

        ASSERT( DirEntry->DirOpenReferenceCount == 0);

        AFSRemoveDirNodeFromParent( ParentObjectInfo,
                                    DirEntry,
                                    TRUE);

        //
        // Free up the name buffer if it was reallocated
        //

        if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_RELEASE_NAME_BUFFER))
        {

            AFSExFreePoolWithTag( DirEntry->NameInformation.FileName.Buffer, 0);
        }

        if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_RELEASE_TARGET_NAME_BUFFER))
        {

            AFSExFreePoolWithTag( DirEntry->NameInformation.TargetName.Buffer, 0);
        }

        //
        // Dereference the object for this dir entry
        //

        lCount = AFSObjectInfoDecrement( DirEntry->ObjectInformation,
                                         AFS_OBJECT_REFERENCE_DIRENTRY);

        AFSDbgLogMsg( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSDeleteDirEntry Decrement count on object %p Cnt %d\n",
                      DirEntry->ObjectInformation,
                      lCount);

        if( BooleanFlagOn( DirEntry->Flags, AFS_DIR_ENTRY_DELETED) &&
            DirEntry->ObjectInformation->Links == 0)
        {

            SetFlag( DirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_DELETED);
        }

        ExDeleteResourceLite( &DirEntry->NonPaged->Lock);

        AFSExFreePoolWithTag( DirEntry->NonPaged, AFS_DIR_ENTRY_NP_TAG);

        //
        // Free up the dir entry
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_ALLOCATION,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSDeleteDirEntry AFS_DIR_ENTRY_TAG deallocating %p\n",
                      DirEntry);

        AFSExFreePoolWithTag( DirEntry, AFS_DIR_ENTRY_TAG);
    }
}

NTSTATUS
AFSRemoveDirNodeFromParent( IN AFSObjectInfoCB *ParentObjectInfo,
                            IN AFSDirectoryCB *DirEntry,
                            IN BOOLEAN RemoveFromEnumList)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    LONG lCount;

    __Enter
    {


        ASSERT( ExIsResourceAcquiredExclusiveLite( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock));

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRemoveDirNodeFromParent Removing DirEntry %p %wZ FID %08lX-%08lX-%08lX-%08lX from Parent %p\n",
                      DirEntry,
                      &DirEntry->NameInformation.FileName,
                      DirEntry->ObjectInformation->FileId.Cell,
                      DirEntry->ObjectInformation->FileId.Volume,
                      DirEntry->ObjectInformation->FileId.Vnode,
                      DirEntry->ObjectInformation->FileId.Unique,
                      ParentObjectInfo);

        if( !BooleanFlagOn( DirEntry->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRemoveDirNodeFromParent Removing DirEntry %p name %wZ\n",
                          DirEntry,
                          &DirEntry->NameInformation.FileName);

            AFSRemoveNameEntry( ParentObjectInfo,
                                DirEntry);
        }
        else
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRemoveDirNodeFromParent DE %p for %wZ NOT removing entry due to flag set\n",
                          DirEntry,
                          &DirEntry->NameInformation.FileName);

        }

        if( RemoveFromEnumList &&
            BooleanFlagOn( DirEntry->Flags, AFS_DIR_ENTRY_INSERTED_ENUM_LIST))
        {

            //
            // And remove the entry from the enumeration list
            //

            if( DirEntry->ListEntry.fLink == NULL)
            {

                ParentObjectInfo->Specific.Directory.DirectoryNodeListTail = (AFSDirectoryCB *)DirEntry->ListEntry.bLink;
            }
            else
            {

                ((AFSDirectoryCB *)DirEntry->ListEntry.fLink)->ListEntry.bLink = DirEntry->ListEntry.bLink;
            }

            if( DirEntry->ListEntry.bLink == NULL)
            {

                ParentObjectInfo->Specific.Directory.DirectoryNodeListHead = (AFSDirectoryCB *)DirEntry->ListEntry.fLink;
            }
            else
            {

                ((AFSDirectoryCB *)DirEntry->ListEntry.bLink)->ListEntry.fLink = DirEntry->ListEntry.fLink;
            }

            ASSERT( ParentObjectInfo->Specific.Directory.DirectoryNodeCount > 0);

            lCount = InterlockedDecrement( &ParentObjectInfo->Specific.Directory.DirectoryNodeCount);

            ClearFlag( DirEntry->Flags, AFS_DIR_ENTRY_INSERTED_ENUM_LIST);

            AFSDbgLogMsg( AFS_SUBSYSTEM_DIR_NODE_COUNT,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSRemoveDirNodeFromParent Removing entry %wZ Dec Count %d to parent FID %08lX-%08lX-%08lX-%08lX\n",
                          &DirEntry->NameInformation.FileName,
                          lCount,
                          ParentObjectInfo->FileId.Cell,
                          ParentObjectInfo->FileId.Volume,
                          ParentObjectInfo->FileId.Vnode,
                          ParentObjectInfo->FileId.Unique);

            DirEntry->ListEntry.fLink = NULL;
            DirEntry->ListEntry.bLink = NULL;
        }
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
              IN GUID *AuthGroup,
              OUT PUNICODE_STRING FileName,
              OUT PUNICODE_STRING ParsedFileName,
              OUT PUNICODE_STRING RootFileName,
              OUT ULONG *ParseFlags,
              OUT AFSVolumeCB   **VolumeCB,
              OUT AFSDirectoryCB **ParentDirectoryCB,
              OUT AFSNameArrayHdr **NameArray)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION  pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSDeviceExt       *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    UNICODE_STRING      uniFullName, uniComponentName, uniRemainingPath;
    ULONG               ulCRC = 0;
    AFSDirectoryCB     *pDirEntry = NULL;
    USHORT              usIndex = 0, usDriveIndex = 0;
    AFSCcb             *pRelatedCcb = NULL;
    AFSNameArrayHdr    *pNameArray = NULL, *pRelatedNameArray = NULL;
    USHORT              usComponentIndex = 0;
    USHORT              usComponentLength = 0;
    AFSVolumeCB        *pVolumeCB = NULL;
    AFSFcb             *pRelatedFcb = NULL;
    BOOLEAN             bReleaseTreeLock = FALSE;
    BOOLEAN             bIsAllShare = FALSE;
    LONG                lCount;

    __Enter
    {

        //
        // Indicate we are opening a root ...
        //

        *ParseFlags = AFS_PARSE_FLAG_ROOT_ACCESS;

        *ParentDirectoryCB = NULL;

        if( pIrpSp->FileObject->RelatedFileObject != NULL)
        {

            pRelatedFcb = (AFSFcb *)pIrpSp->FileObject->RelatedFileObject->FsContext;

            pRelatedCcb = (AFSCcb *)pIrpSp->FileObject->RelatedFileObject->FsContext2;

            pRelatedNameArray = pRelatedCcb->NameArray;

            uniFullName = pIrpSp->FileObject->FileName;

            ASSERT( pRelatedFcb != NULL);

            //
            // No wild cards in the name
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSParseName (%p) Relative open for %wZ FID %08lX-%08lX-%08lX-%08lX component %wZ\n",
                          Irp,
                          &pRelatedCcb->DirectoryCB->NameInformation.FileName,
                          pRelatedCcb->DirectoryCB->ObjectInformation->FileId.Cell,
                          pRelatedCcb->DirectoryCB->ObjectInformation->FileId.Volume,
                          pRelatedCcb->DirectoryCB->ObjectInformation->FileId.Vnode,
                          pRelatedCcb->DirectoryCB->ObjectInformation->FileId.Unique,
                          &uniFullName);

            if( FsRtlDoesNameContainWildCards( &uniFullName))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%p) Component %wZ contains wild cards\n",
                              Irp,
                              &uniFullName);

                try_return( ntStatus = STATUS_OBJECT_NAME_INVALID);
            }

            pVolumeCB = pRelatedFcb->ObjectInformation->VolumeCB;

            pDirEntry = pRelatedCcb->DirectoryCB;

            *FileName = pIrpSp->FileObject->FileName;

            //
            // Grab the root node while checking state
            //

            AFSAcquireShared( pVolumeCB->VolumeLock,
                              TRUE);

            if( BooleanFlagOn( pVolumeCB->ObjectInformation.Flags, AFS_OBJECT_FLAGS_OBJECT_INVALID) ||
                BooleanFlagOn( pVolumeCB->Flags, AFS_VOLUME_FLAGS_OFFLINE))
            {

                //
                // The volume has been taken off line so fail the access
                //

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%p) Volume %08lX:%08lX OFFLINE/INVALID\n",
                              Irp,
                              pVolumeCB->ObjectInformation.FileId.Cell,
                              pVolumeCB->ObjectInformation.FileId.Volume);

                AFSReleaseResource( pVolumeCB->VolumeLock);

                try_return( ntStatus = STATUS_DEVICE_NOT_READY);
            }

            if( BooleanFlagOn( pVolumeCB->ObjectInformation.Flags, AFS_OBJECT_FLAGS_VERIFY))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%p) Verifying root of volume %08lX:%08lX\n",
                              Irp,
                              pVolumeCB->ObjectInformation.FileId.Cell,
                              pVolumeCB->ObjectInformation.FileId.Volume);

                ntStatus = AFSVerifyVolume( (ULONGLONG)PsGetCurrentProcessId(),
                                            pVolumeCB);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSParseName (%p) Failed verification of root Status %08lX\n",
                                  Irp,
                                  ntStatus);

                    AFSReleaseResource( pVolumeCB->VolumeLock);

                    try_return( ntStatus);
                }
            }

            AFSReleaseResource( pVolumeCB->VolumeLock);

            if( BooleanFlagOn( pDirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%p) Verifying parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              Irp,
                              &pDirEntry->NameInformation.FileName,
                              pDirEntry->ObjectInformation->FileId.Cell,
                              pDirEntry->ObjectInformation->FileId.Volume,
                              pDirEntry->ObjectInformation->FileId.Vnode,
                              pDirEntry->ObjectInformation->FileId.Unique);

                //
                // Directory TreeLock should be exclusively held
                //

                AFSAcquireExcl( pDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                TRUE);

                ntStatus = AFSVerifyEntry( AuthGroup,
                                           pDirEntry);

                AFSReleaseResource( pDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSParseName (%p) Failed verification of parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                  Irp,
                                  &pDirEntry->NameInformation.FileName,
                                  pDirEntry->ObjectInformation->FileId.Cell,
                                  pDirEntry->ObjectInformation->FileId.Volume,
                                  pDirEntry->ObjectInformation->FileId.Vnode,
                                  pDirEntry->ObjectInformation->FileId.Unique,
                                  ntStatus);

                    try_return( ntStatus);
                }
            }

            //
            // Create our full path name buffer
            //

            uniFullName.MaximumLength = pRelatedCcb->FullFileName.Length +
                                                    sizeof( WCHAR) +
                                                    pIrpSp->FileObject->FileName.Length +
                                                    sizeof( WCHAR);

            uniFullName.Length = 0;

            uniFullName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                    uniFullName.MaximumLength,
                                                                    AFS_NAME_BUFFER_THREE_TAG);

            if( uniFullName.Buffer == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%p) Failed to allocate full name buffer\n",
                              Irp);

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlZeroMemory( uniFullName.Buffer,
                           uniFullName.MaximumLength);

            RtlCopyMemory( uniFullName.Buffer,
                           pRelatedCcb->FullFileName.Buffer,
                           pRelatedCcb->FullFileName.Length);

            uniFullName.Length = pRelatedCcb->FullFileName.Length;

            usComponentIndex = (USHORT)(uniFullName.Length/sizeof( WCHAR));

            usComponentLength = pIrpSp->FileObject->FileName.Length;

            if( uniFullName.Buffer[ (uniFullName.Length/sizeof( WCHAR)) - 1] != L'\\' &&
                pIrpSp->FileObject->FileName.Length > 0 &&
                pIrpSp->FileObject->FileName.Buffer[ 0] != L'\\' &&
                pIrpSp->FileObject->FileName.Buffer[ 0] != L':')
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

            *RootFileName = uniFullName;

            //
            // We populate up to the current parent
            //

            if( pRelatedNameArray == NULL)
            {

                //
                // Init and populate our name array
                //

                pNameArray = AFSInitNameArray( NULL,
                                               0);

                if( pNameArray == NULL)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSParseName (%p) Failed to initialize name array\n",
                                  Irp);

                    AFSExFreePoolWithTag( uniFullName.Buffer, 0);

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                ntStatus = AFSPopulateNameArray( pNameArray,
                                                 NULL,
                                                 pRelatedCcb->DirectoryCB);
            }
            else
            {

                //
                // Init and populate our name array
                //

                pNameArray = AFSInitNameArray( NULL,
                                               pRelatedNameArray->MaxElementCount);

                if( pNameArray == NULL)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSParseName (%p) Failed to initialize name array\n",
                                  Irp);

                    AFSExFreePoolWithTag( uniFullName.Buffer, 0);

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                ntStatus = AFSPopulateNameArrayFromRelatedArray( pNameArray,
                                                                 pRelatedNameArray,
                                                                 pRelatedCcb->DirectoryCB);
            }

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%p) Failed to populate name array\n",
                              Irp);

                AFSExFreePoolWithTag( uniFullName.Buffer, 0);

                try_return( ntStatus);
            }

            ParsedFileName->Length = usComponentLength;
            ParsedFileName->MaximumLength = uniFullName.MaximumLength;

            ParsedFileName->Buffer = &uniFullName.Buffer[ usComponentIndex];

            //
            // Indicate to caller that RootFileName must be freed
            //

            SetFlag( *ParseFlags, AFS_PARSE_FLAG_FREE_FILE_BUFFER);

            *NameArray = pNameArray;

            //
            // Increment our volume reference count
            //

            lCount = AFSVolumeIncrement( pVolumeCB,
                                         AFS_VOLUME_REFERENCE_PARSE_NAME);

            AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSParseName Increment count on volume %p Cnt %d\n",
                          pVolumeCB,
                          lCount);

            *VolumeCB = pVolumeCB;

            *ParentDirectoryCB = pDirEntry;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSParseName (%p) Returning full name %wZ\n",
                          Irp,
                          &uniFullName);

            try_return( ntStatus);
        }

        //
        // No wild cards in the name
        //

        uniFullName = pIrpSp->FileObject->FileName;

        if( FsRtlDoesNameContainWildCards( &uniFullName) ||
            uniFullName.Length < AFSServerName.Length)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSParseName (%p) Name %wZ contains wild cards or too short\n",
                          Irp,
                          &uniFullName);

            try_return( ntStatus = STATUS_OBJECT_NAME_INVALID);
        }

        //
        // The name is a fully qualified name. Parse out the server/share names and
        // point to the root qualified name
        // First thing is to locate the server name
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

            //
            // Drive letter based name?
            //

            uniFullName = pIrpSp->FileObject->FileName;

            while( usIndex < uniFullName.Length/sizeof( WCHAR))
            {

                if( uniFullName.Buffer[ usIndex] == L':')
                {

                    uniFullName.Buffer = &uniFullName.Buffer[ usIndex + 2];

                    uniFullName.Length -= (usIndex + 2) * sizeof( WCHAR);

                    usDriveIndex = usIndex - 1;

                    break;
                }

                usIndex++;
            }

            //
            // Do we have the right server name now?
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
                              "AFSParseName (%p) Name %wZ does not have server name\n",
                              Irp,
                              &pIrpSp->FileObject->FileName);

                try_return( ntStatus = STATUS_BAD_NETWORK_NAME);
            }

            //
            // Validate this drive letter is actively mapped
            //

            if( usDriveIndex > 0 &&
                !AFSIsDriveMapped( pIrpSp->FileObject->FileName.Buffer[ usDriveIndex]))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%p) Name %wZ contains invalid drive mapping\n",
                              Irp,
                              &pIrpSp->FileObject->FileName);

                try_return( ntStatus = STATUS_BAD_NETWORK_NAME);
            }
        }

        if( FsRtlDoesNameContainWildCards( &uniFullName))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSParseName (%p) Component %wZ contains wild cards\n",
                          Irp,
                          &uniFullName);

            try_return( ntStatus = STATUS_OBJECT_NAME_INVALID);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSParseName (%p) Processing full name %wZ\n",
                      Irp,
                      &uniFullName);

        if( uniFullName.Length > 0 &&
            uniFullName.Buffer[ (uniFullName.Length/sizeof( WCHAR)) - 1] == L'\\')
        {

            uniFullName.Length -= sizeof( WCHAR);
        }

        //
        // Be sure we are online and ready to go
        //

        AFSAcquireShared( AFSGlobalRoot->VolumeLock,
                          TRUE);

        if( BooleanFlagOn( AFSGlobalRoot->ObjectInformation.Flags, AFS_OBJECT_FLAGS_OBJECT_INVALID) ||
            BooleanFlagOn( AFSGlobalRoot->Flags, AFS_VOLUME_FLAGS_OFFLINE))
        {

            //
            // The volume has been taken off line so fail the access
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSParseName (%p) Volume %08lX:%08lX OFFLINE/INVALID\n",
                          Irp,
                          AFSGlobalRoot->ObjectInformation.FileId.Cell,
                          AFSGlobalRoot->ObjectInformation.FileId.Volume);

            AFSReleaseResource( AFSGlobalRoot->VolumeLock);

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        if( BooleanFlagOn( AFSGlobalRoot->ObjectInformation.Flags, AFS_OBJECT_FLAGS_VERIFY))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSParseName (%p) Verifying root of volume %08lX:%08lX\n",
                          Irp,
                          AFSGlobalRoot->ObjectInformation.FileId.Cell,
                          AFSGlobalRoot->ObjectInformation.FileId.Volume);

            ntStatus = AFSVerifyVolume( (ULONGLONG)PsGetCurrentProcessId(),
                                        AFSGlobalRoot);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%p) Failed verification of root Status %08lX\n",
                              Irp,
                              ntStatus);

                AFSReleaseResource( AFSGlobalRoot->VolumeLock);

                try_return( ntStatus);
            }
        }

        AFSReleaseResource( AFSGlobalRoot->VolumeLock);

        if( !BooleanFlagOn( AFSGlobalRoot->ObjectInformation.Flags, AFS_OBJECT_FLAGS_DIRECTORY_ENUMERATED))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSParseName (%p) Enumerating global root of volume %08lX:%08lX\n",
                          Irp,
                          AFSGlobalRoot->ObjectInformation.FileId.Cell,
                          AFSGlobalRoot->ObjectInformation.FileId.Volume);

            ntStatus = AFSEnumerateGlobalRoot( AuthGroup);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSParseName (%p) Failed enumeraiton of root Status %08lX\n",
                              Irp,
                              ntStatus);

                try_return( ntStatus);
            }
        }

        //
        // Check for the \\Server access and return it as though it were \\Server\Globalroot
        //

        if( uniRemainingPath.Buffer == NULL ||
            ( uniRemainingPath.Length == sizeof( WCHAR) &&
              uniRemainingPath.Buffer[ 0] == L'\\'))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSParseName (%p) Returning global root access\n",
                          Irp);

            lCount = InterlockedIncrement( &AFSGlobalRoot->DirectoryCB->DirOpenReferenceCount);

            AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSParseName Increment2 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &AFSGlobalRoot->DirectoryCB->NameInformation.FileName,
                          AFSGlobalRoot->DirectoryCB,
                          NULL,
                          lCount);

            *VolumeCB = NULL;

            FileName->Length = 0;
            FileName->MaximumLength = 0;
            FileName->Buffer = NULL;

            try_return( ntStatus = STATUS_SUCCESS);
        }

        *RootFileName = uniFullName;

        //
        // Include the starting \ in the root name
        //

        if( RootFileName->Buffer[ 0] != L'\\')
        {
            RootFileName->Buffer--;
            RootFileName->Length += sizeof( WCHAR);
            RootFileName->MaximumLength += sizeof( WCHAR);
        }

        //
        // Get the 'share' name
        //

        FsRtlDissectName( uniFullName,
                          &uniComponentName,
                          &uniRemainingPath);

        if( FsRtlDoesNameContainWildCards( &uniFullName))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSParseName (%p) Component %wZ contains wild cards\n",
                          Irp,
                          &uniComponentName);

            try_return( ntStatus = STATUS_OBJECT_NAME_INVALID);
        }

        //
        // If this is the ALL access then perform some additional processing
        //

        if( uniComponentName.Length == 0 ||
            RtlCompareUnicodeString( &uniComponentName,
                                     &AFSGlobalRootName,
                                     TRUE) == 0)
        {

            bIsAllShare = TRUE;

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
                              "AFSParseName (%p) Returning global root access\n",
                              Irp);

                lCount = InterlockedIncrement( &AFSGlobalRoot->DirectoryCB->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName Increment3 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &AFSGlobalRoot->DirectoryCB->NameInformation.FileName,
                              AFSGlobalRoot->DirectoryCB,
                              NULL,
                              lCount);

                *VolumeCB = NULL;

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
                              "AFSParseName (%p) Returning root PIOCtl access\n",
                              Irp);

                lCount = InterlockedIncrement( &AFSGlobalRoot->DirectoryCB->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName Increment4 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &AFSGlobalRoot->DirectoryCB->NameInformation.FileName,
                              AFSGlobalRoot->DirectoryCB,
                              NULL,
                              lCount);

                ClearFlag( *ParseFlags, AFS_PARSE_FLAG_ROOT_ACCESS);

                *VolumeCB = NULL;

                *FileName = AFSPIOCtlName;

                try_return( ntStatus = STATUS_SUCCESS);
            }
        }
        else if( (pDirEntry = AFSGetSpecialShareNameEntry( &uniComponentName,
                                                           &uniRemainingPath)) != NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSParseName (%p) Returning root share name %wZ access\n",
                          Irp,
                          &uniComponentName);

            //
            // Add in the full share name to pass back
            //

            if( uniRemainingPath.Buffer != NULL)
            {

                //
                // This routine strips off the leading slash so add it back in
                //

                uniRemainingPath.Buffer--;
                uniRemainingPath.Length += sizeof( WCHAR);
                uniRemainingPath.MaximumLength += sizeof( WCHAR);

                //
                // And the cell name
                //

                uniRemainingPath.Buffer -= (uniComponentName.Length/sizeof( WCHAR));
                uniRemainingPath.Length += uniComponentName.Length;
                uniRemainingPath.MaximumLength += uniComponentName.Length;

                uniComponentName = uniRemainingPath;
            }

            *VolumeCB = NULL;

            *FileName = uniComponentName;

            ClearFlag( *ParseFlags, AFS_PARSE_FLAG_ROOT_ACCESS);

            *ParentDirectoryCB = pDirEntry;

            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // Determine the 'share' we are accessing
        //

        ulCRC = AFSGenerateCRC( &uniComponentName,
                                FALSE);

        AFSAcquireShared( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock,
                          TRUE);

        bReleaseTreeLock = TRUE;

        AFSLocateCaseSensitiveDirEntry( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                        ulCRC,
                                        &pDirEntry);

        if( pDirEntry == NULL)
        {

            ulCRC = AFSGenerateCRC( &uniComponentName,
                                    TRUE);

            AFSLocateCaseInsensitiveDirEntry( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                              ulCRC,
                                              &pDirEntry);

            if( pDirEntry == NULL)
            {

                //
                // OK, if this component is a valid short name then try
                // a lookup in the short name tree
                //

                if( !BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
                    RtlIsNameLegalDOS8Dot3( &uniComponentName,
                                            NULL,
                                            NULL))
                {

                    AFSLocateShortNameDirEntry( AFSGlobalRoot->ObjectInformation.Specific.Directory.ShortNameTree,
                                                ulCRC,
                                                &pDirEntry);
                }

                if( pDirEntry == NULL)
                {

                    //
                    // Check with the service whether it is a valid cell name
                    //

                    AFSReleaseResource( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock);

                    bReleaseTreeLock = FALSE;

                    ntStatus = AFSCheckCellName( AuthGroup,
                                                 &uniComponentName,
                                                 &pDirEntry);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        if ( bIsAllShare &&
                             uniRemainingPath.Length == 0 &&
                             ntStatus == STATUS_OBJECT_PATH_NOT_FOUND)
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSParseName (%p) AFSCheckCellName %wZ returned path not found; ntStatus %08X\n",
                                          Irp,
                                          &uniComponentName,
                                          STATUS_OBJECT_NAME_NOT_FOUND);

                            ntStatus = STATUS_OBJECT_NAME_NOT_FOUND;
                        }

                        try_return( ntStatus);
                    }
                }
            }
        }

        if( bReleaseTreeLock)
        {
            AFSReleaseResource( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock);
        }


        //
        // Be sure we are starting from the correct volume
        //

        if( pDirEntry->ObjectInformation->VolumeCB != AFSGlobalRoot)
        {

            //
            // We dropped the global root in the CheckCellName routine which is the
            // only way we can be here
            //

            pVolumeCB = pDirEntry->ObjectInformation->VolumeCB;

            //
            // Init our name array
            //

            pNameArray = AFSInitNameArray( AFSGlobalRoot->DirectoryCB,
                                           0);

            if( pNameArray == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%p) Failed to initialize name array\n",
                              Irp);

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            ntStatus = AFSInsertNextElement( pNameArray,
                                             pVolumeCB->DirectoryCB);

            if ( ntStatus)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%p) Failed to insert name array element\n",
                              Irp);

                try_return( ntStatus);
            }

            //
            // In this case don't add back in the 'share' name since that is where we are
            // starting. Just put the leading slash back in
            //

            if( uniRemainingPath.Buffer != NULL)
            {

                uniRemainingPath.Buffer--;
                uniRemainingPath.Length += sizeof( WCHAR);
                uniRemainingPath.MaximumLength += sizeof( WCHAR);

                if( uniRemainingPath.Length > sizeof( WCHAR))
                {

                    ClearFlag( *ParseFlags, AFS_PARSE_FLAG_ROOT_ACCESS);
                }

                //
                // Pass back the parent being the root of the volume
                //

                *ParentDirectoryCB = pVolumeCB->DirectoryCB;
            }
            else
            {

                //
                // Pass back a root slash
                //

                uniRemainingPath = uniComponentName;

                uniRemainingPath.Buffer--;
                uniRemainingPath.Length = sizeof( WCHAR);
                uniRemainingPath.MaximumLength = sizeof( WCHAR);

                //
                // This is a root open so pass back no parent
                //
            }
        }
        else
        {

            pVolumeCB = AFSGlobalRoot;

            //
            // Init our name array
            //

            pNameArray = AFSInitNameArray( AFSGlobalRoot->DirectoryCB,
                                           0);
            if( pNameArray == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName (%p) Failed to initialize name array\n",
                              Irp);

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            //
            // Add back in the 'share' portion of the name since we will parse it out on return
            //

            if( uniRemainingPath.Buffer != NULL)
            {

                //
                // This routine strips off the leading slash so add it back in
                //

                uniRemainingPath.Buffer--;
                uniRemainingPath.Length += sizeof( WCHAR);
                uniRemainingPath.MaximumLength += sizeof( WCHAR);

                if( uniRemainingPath.Length > sizeof( WCHAR))
                {

                    ClearFlag( *ParseFlags, AFS_PARSE_FLAG_ROOT_ACCESS);
                }

                //
                // And the cell name
                //

                uniRemainingPath.Buffer -= (uniComponentName.Length/sizeof( WCHAR));
                uniRemainingPath.Length += uniComponentName.Length;
                uniRemainingPath.MaximumLength += uniComponentName.Length;
            }
            else
            {

                uniRemainingPath = uniComponentName;
            }

            //
            // And the leading slash again ...
            //

            uniRemainingPath.Buffer--;
            uniRemainingPath.Length += sizeof( WCHAR);
            uniRemainingPath.MaximumLength += sizeof( WCHAR);

            //
            // Pass back the parent being the volume root
            //

            *ParentDirectoryCB = pVolumeCB->DirectoryCB;
        }

        //
        // Return the remaining portion as the file name
        //

        *FileName = uniRemainingPath;

        *ParsedFileName = uniRemainingPath;

        *NameArray = pNameArray;

        *VolumeCB = pVolumeCB;

        //
        // Increment our reference on the volume
        //

        lCount = AFSVolumeIncrement( pVolumeCB,
                                     AFS_VOLUME_REFERENCE_PARSE_NAME);

        AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSParseName Increment2 count on global volume %p Cnt %d\n",
                      pVolumeCB,
                      lCount);

try_exit:

        if( NT_SUCCESS( ntStatus))
        {

            if( *ParentDirectoryCB != NULL)
            {

                lCount = InterlockedIncrement( &(*ParentDirectoryCB)->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSParseName Increment1 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &(*ParentDirectoryCB)->NameInformation.FileName,
                              (*ParentDirectoryCB),
                              NULL,
                              lCount);
            }
        }

        if( *VolumeCB != NULL)
        {
            ASSERT( (*VolumeCB)->VolumeReferenceCount > 0);
        }

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
AFSCheckCellName( IN GUID *AuthGroup,
                  IN UNICODE_STRING *CellName,
                  OUT AFSDirectoryCB **ShareDirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniName;
    AFSDirEnumEntry *pDirEnumEntry = NULL;
    AFSDirHdr *pDirHdr = &AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr;
    AFSDirectoryCB *pDirNode = NULL;
    UNICODE_STRING uniDirName, uniTargetName;
    AFSVolumeCB *pVolumeCB = NULL;
    LONG lCount;

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

        //
        // OK, ask the CM about this component name
        //

        ntStatus = AFSEvaluateTargetByName( AuthGroup,
                                            &AFSGlobalRoot->ObjectInformation,
                                            CellName,
                                            &pDirEnumEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSCheckCellName entry %wZ does not exist parent FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          CellName,
                          AFSGlobalRoot->ObjectInformation.FileId.Cell,
                          AFSGlobalRoot->ObjectInformation.FileId.Volume,
                          AFSGlobalRoot->ObjectInformation.FileId.Vnode,
                          AFSGlobalRoot->ObjectInformation.FileId.Unique,
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // OK, we have a dir enum entry back so add it to the root node
        //

        uniDirName = *CellName;

        uniTargetName.Length = (USHORT)pDirEnumEntry->TargetNameLength;
        uniTargetName.MaximumLength = uniTargetName.Length;
        uniTargetName.Buffer = (WCHAR *)((char *)pDirEnumEntry + pDirEnumEntry->TargetNameOffset);

        //
        // Is this entry a root volume entry?
        //

        if( pDirEnumEntry->FileId.Cell != AFSGlobalRoot->ObjectInformation.FileId.Cell ||
            pDirEnumEntry->FileId.Volume != AFSGlobalRoot->ObjectInformation.FileId.Volume)
        {

            //
            // Build the root volume entry
            //

            ntStatus = AFSBuildRootVolume( AuthGroup,
                                           &pDirEnumEntry->FileId,
                                           &pVolumeCB);

            //
            // On success returns with a volume reference count held
            //

            if( !NT_SUCCESS( ntStatus))
            {
                try_return( ntStatus);
            }

            *ShareDirEntry = pVolumeCB->DirectoryCB;

            lCount = InterlockedIncrement( &pVolumeCB->DirectoryCB->DirOpenReferenceCount);

            AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCheckCellName Increment1 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pVolumeCB->DirectoryCB->NameInformation.FileName,
                          pVolumeCB->DirectoryCB,
                          NULL,
                          lCount);

            lCount = AFSVolumeDecrement( pVolumeCB,
                                         AFS_VOLUME_REFERENCE_BUILD_ROOT);

            AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCheckCellName Decrement count on volume %p Cnt %d\n",
                          pVolumeCB,
                          lCount);
        }
        else
        {

            lCount = InterlockedIncrement( &pDirHdr->ContentIndex);

            pDirNode = AFSInitDirEntry( &AFSGlobalRoot->ObjectInformation,
                                        &uniDirName,
                                        &uniTargetName,
                                        pDirEnumEntry,
                                        (ULONG)lCount);

            if( pDirNode == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            //
            // Init the short name if we have one
            //

            if( pDirEnumEntry->ShortNameLength > 0)
            {

                pDirNode->NameInformation.ShortNameLength = pDirEnumEntry->ShortNameLength;

                RtlCopyMemory( pDirNode->NameInformation.ShortName,
                               pDirEnumEntry->ShortName,
                               pDirNode->NameInformation.ShortNameLength);
            }

            AFSAcquireExcl( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock,
                            TRUE);

            //
            // Insert the node into the name tree
            //

            ASSERT( ExIsResourceAcquiredExclusiveLite( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock));

            if( pDirHdr->CaseSensitiveTreeHead == NULL)
            {

                pDirHdr->CaseSensitiveTreeHead = pDirNode;
            }
            else
            {

                if( !NT_SUCCESS( AFSInsertCaseSensitiveDirEntry( pDirHdr->CaseSensitiveTreeHead,
                                                                 pDirNode)))
                {

                    AFSDeleteDirEntry( &AFSGlobalRoot->ObjectInformation,
                                       pDirNode);

                    AFSReleaseResource( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock);

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }
            }

            ClearFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE);

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

            if( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeListHead == NULL)
            {

                AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeListHead = pDirNode;
            }
            else
            {

                AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeListTail->ListEntry.fLink = pDirNode;

                pDirNode->ListEntry.bLink = AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeListTail;
            }

            AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeListTail = pDirNode;

            SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_INSERTED_ENUM_LIST);

            lCount = InterlockedIncrement( &AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeCount);

            AFSDbgLogMsg( AFS_SUBSYSTEM_DIR_NODE_COUNT,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCheckCellName Adding entry %wZ Inc Count %d to parent FID %08lX-%08lX-%08lX-%08lX\n",
                          &pDirNode->NameInformation.FileName,
                          lCount,
                          AFSGlobalRoot->ObjectInformation.FileId.Cell,
                          AFSGlobalRoot->ObjectInformation.FileId.Volume,
                          AFSGlobalRoot->ObjectInformation.FileId.Vnode,
                          AFSGlobalRoot->ObjectInformation.FileId.Unique);

            lCount = InterlockedIncrement( &pDirNode->DirOpenReferenceCount);

            AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCheckCellName Increment2 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pDirNode->NameInformation.FileName,
                          pDirNode,
                          NULL,
                          lCount);

            AFSReleaseResource( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock);

            //
            // Pass back the dir node
            //

            *ShareDirEntry = pDirNode;
        }

try_exit:

        if( pDirEnumEntry != NULL)
        {

            AFSExFreePoolWithTag( pDirEnumEntry, AFS_GENERIC_MEMORY_31_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSBuildMountPointTarget( IN GUID *AuthGroup,
                          IN AFSDirectoryCB  *DirectoryCB,
                          OUT AFSVolumeCB **TargetVolumeCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    AFSDirEnumEntry *pDirEntry = NULL;
    ULONGLONG       ullIndex = 0;
    AFSVolumeCB *pVolumeCB = NULL;
    AFSFileID stTargetFileID;
    LONG lCount;
    BOOLEAN bReleaseVolumeLock = FALSE;

    __Enter
    {

        //
        // Loop on each entry, building the chain until we encounter the final target
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSBuildMountPointTarget Building target directory for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                      &DirectoryCB->NameInformation.FileName,
                      DirectoryCB->ObjectInformation->FileId.Cell,
                      DirectoryCB->ObjectInformation->FileId.Volume,
                      DirectoryCB->ObjectInformation->FileId.Vnode,
                      DirectoryCB->ObjectInformation->FileId.Unique);

        //
        // Do we need to evaluate the node?
        //

        //if( DirectoryCB->ObjectInformation->TargetFileId.Vnode == 0 &&
        //    DirectoryCB->ObjectInformation->TargetFileId.Unique == 0)
        {

            //
            // Go evaluate the current target to get the target fid
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSBuildMountPointTarget Evaluating target %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                              &DirectoryCB->NameInformation.FileName,
                              DirectoryCB->ObjectInformation->FileId.Cell,
                              DirectoryCB->ObjectInformation->FileId.Volume,
                              DirectoryCB->ObjectInformation->FileId.Vnode,
                              DirectoryCB->ObjectInformation->FileId.Unique);

            ntStatus = AFSEvaluateTargetByID( DirectoryCB->ObjectInformation,
                                              AuthGroup,
                                              FALSE,
                                              &pDirEntry);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSBuildMountPointTarget Failed to evaluate target %wZ Status %08lX\n",
                              &DirectoryCB->NameInformation.FileName,
                              ntStatus);
                try_return( ntStatus);
            }

            if( pDirEntry->TargetFileId.Vnode == 0 &&
                pDirEntry->TargetFileId.Unique == 0)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSBuildMountPointTarget Target %wZ FID %08lX-%08lX-%08lX-%08lX service returned zero FID\n",
                              &DirectoryCB->NameInformation.FileName,
                              DirectoryCB->ObjectInformation->FileId.Cell,
                              DirectoryCB->ObjectInformation->FileId.Volume,
                              DirectoryCB->ObjectInformation->FileId.Vnode,
                              DirectoryCB->ObjectInformation->FileId.Unique);

                try_return( ntStatus = STATUS_REPARSE_POINT_NOT_RESOLVED);
            }

            AFSAcquireExcl( &DirectoryCB->NonPaged->Lock,
                            TRUE);

            ntStatus = AFSUpdateTargetName( &DirectoryCB->NameInformation.TargetName,
                                            &DirectoryCB->Flags,
                                            (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset),
                                            (USHORT)pDirEntry->TargetNameLength);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

                try_return( ntStatus);
            }

            AFSReleaseResource( &DirectoryCB->NonPaged->Lock);

            DirectoryCB->ObjectInformation->TargetFileId = pDirEntry->TargetFileId;
        }

        stTargetFileID = DirectoryCB->ObjectInformation->TargetFileId;

        //
        // Try to locate this FID. First the volume then the
        // entry itself
        //

        ullIndex = AFSCreateHighIndex( &stTargetFileID);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSBuildMountPointTarget Acquiring RDR VolumeTreeLock lock %p EXCL %08lX\n",
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
                                       (AFSBTreeEntry **)&pVolumeCB);

        //
        // We can be processing a request for a target that is on a volume
        // we have never seen before.
        //

        if( pVolumeCB == NULL)
        {

            //
            // Locking is held correctly in init routine
            //

            AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

            //
            // Go init the root of the volume
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSBuildMountPointTarget Initializing root for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                          &DirectoryCB->NameInformation.FileName,
                          DirectoryCB->ObjectInformation->FileId.Cell,
                          DirectoryCB->ObjectInformation->FileId.Volume,
                          DirectoryCB->ObjectInformation->FileId.Vnode,
                          DirectoryCB->ObjectInformation->FileId.Unique);

            ntStatus = AFSInitVolume( AuthGroup,
                                      &stTargetFileID,
                                      AFS_VOLUME_REFERENCE_MOUNTPT,
                                      &pVolumeCB);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }

            //
            // pVolumeCB->VolumeLock held exclusive and
            // pVolumeCB->VolumeReferenceCount has been incremented
            // pVolumeCB->RootFcb == NULL
            //

            bReleaseVolumeLock = TRUE;
        }
        else
        {

            //
            // AFSInitVolume returns with a VolumeReferenceCount
            // obtain one to match
            //

            lCount = AFSVolumeIncrement( pVolumeCB,
                                         AFS_VOLUME_REFERENCE_MOUNTPT);

            AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSBuildMountPointTarget Increment count on volume %p Cnt %d\n",
                          pVolumeCB,
                          lCount);

            AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);
        }

        if( pVolumeCB->RootFcb == NULL)
        {

            if ( bReleaseVolumeLock == FALSE)
            {

                AFSAcquireExcl( pVolumeCB->VolumeLock,
                                TRUE);

                bReleaseVolumeLock = TRUE;
            }

            //
            // Initialize the root fcb for this volume
            //

            ntStatus = AFSInitRootFcb( (ULONGLONG)PsGetCurrentProcessId(),
                                       pVolumeCB);

            if( !NT_SUCCESS( ntStatus))
            {

                lCount = AFSVolumeDecrement( pVolumeCB,
                                             AFS_VOLUME_REFERENCE_MOUNTPT);

                AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSBuildMountPoint Decrement count on volume %p Cnt %d\n",
                              pVolumeCB,
                              lCount);

                AFSReleaseResource( pVolumeCB->VolumeLock);

                try_return( ntStatus);
            }

            //
            // Drop the lock acquired above
            //

            AFSReleaseResource( &pVolumeCB->RootFcb->NPFcb->Resource);
        }

        if ( bReleaseVolumeLock == TRUE)
        {

            AFSReleaseResource( pVolumeCB->VolumeLock);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSBuildMountPointTarget Evaluated target of %wZ FID %08lX-%08lX-%08lX-%08lX as root volume\n",
                      &pVolumeCB->DirectoryCB->NameInformation.FileName,
                      pVolumeCB->ObjectInformation.FileId.Cell,
                      pVolumeCB->ObjectInformation.FileId.Volume,
                      pVolumeCB->ObjectInformation.FileId.Vnode,
                      pVolumeCB->ObjectInformation.FileId.Unique);

        *TargetVolumeCB = pVolumeCB;

try_exit:

        if( pDirEntry)
        {

            AFSExFreePoolWithTag( pDirEntry, AFS_GENERIC_MEMORY_2_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSBuildRootVolume( IN GUID *AuthGroup,
                    IN AFSFileID *FileId,
                    OUT AFSVolumeCB **TargetVolumeCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    ULONGLONG       ullIndex = 0;
    AFSVolumeCB *pVolumeCB = NULL;
    LONG lCount;
    BOOLEAN bReleaseVolumeLock = FALSE;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSBuildRootVolume Building target volume for FID %08lX-%08lX-%08lX-%08lX\n",
                      FileId->Cell,
                      FileId->Volume,
                      FileId->Vnode,
                      FileId->Unique);

        ullIndex = AFSCreateHighIndex( FileId);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSBuildRootVolume Acquiring RDR VolumeTreeLock lock %p EXCL %08lX\n",
                      &pDevExt->Specific.RDR.VolumeTreeLock,
                      PsGetCurrentThread());

        AFSAcquireShared( &pDevExt->Specific.RDR.VolumeTreeLock,
                          TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSBuildRootVolume Locating volume for target %I64X\n",
                      ullIndex);

        ntStatus = AFSLocateHashEntry( pDevExt->Specific.RDR.VolumeTree.TreeHead,
                                       ullIndex,
                                       (AFSBTreeEntry **)&pVolumeCB);

        //
        // We can be processing a request for a target that is on a volume
        // we have never seen before.
        //

        if( pVolumeCB == NULL)
        {

            //
            // Locking is held correctly in init routine
            //

            AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);

            //
            // Go init the root of the volume
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                         AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSBuildRootVolume Initializing root for FID %08lX-%08lX-%08lX-%08lX\n",
                          FileId->Cell,
                          FileId->Volume,
                          FileId->Vnode,
                          FileId->Unique);

            ntStatus = AFSInitVolume( AuthGroup,
                                      FileId,
                                      AFS_VOLUME_REFERENCE_BUILD_ROOT,
                                      &pVolumeCB);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }

            //
            // pVolumeCB->VolumeLock is held exclusive
            // pVolumeCB->VolumeReferenceCount has been incremented
            // pVolumeCB->RootFcb == NULL
            //

            bReleaseVolumeLock = TRUE;
        }
        else
        {

            //
            // AFSInitVolume returns with a VolumeReferenceCount
            // obtain one to match
            //

            lCount = AFSVolumeIncrement( pVolumeCB,
                                         AFS_VOLUME_REFERENCE_BUILD_ROOT);

            AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSBuildRootVolume Increment count on volume %p Cnt %d\n",
                          pVolumeCB,
                          lCount);

            AFSReleaseResource( &pDevExt->Specific.RDR.VolumeTreeLock);
        }


        if( pVolumeCB->RootFcb == NULL)
        {

            if ( bReleaseVolumeLock == FALSE)
            {

                AFSAcquireExcl( pVolumeCB->VolumeLock,
                                TRUE);

                bReleaseVolumeLock = TRUE;
            }

            //
            // Initialize the root fcb for this volume
            //

            ntStatus = AFSInitRootFcb( (ULONGLONG)PsGetCurrentProcessId(),
                                       pVolumeCB);

            if( !NT_SUCCESS( ntStatus))
            {

                lCount = AFSVolumeDecrement( pVolumeCB,
                                             AFS_VOLUME_REFERENCE_BUILD_ROOT);

                AFSDbgLogMsg( AFS_SUBSYSTEM_VOLUME_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSBuildRootVolume Decrement count on volume %p Cnt %d\n",
                              pVolumeCB,
                              lCount);

                AFSReleaseResource( pVolumeCB->VolumeLock);

                try_return( ntStatus);
            }

            //
            // Drop the lock acquired above
            //

            AFSReleaseResource( &pVolumeCB->RootFcb->NPFcb->Resource);
        }

        if ( bReleaseVolumeLock == TRUE)
        {

            AFSReleaseResource( pVolumeCB->VolumeLock);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSBuildRootVolume Evaluated target of %wZ FID %08lX-%08lX-%08lX-%08lX as root volume\n",
                      &pVolumeCB->DirectoryCB->NameInformation.FileName,
                      pVolumeCB->ObjectInformation.FileId.Cell,
                      pVolumeCB->ObjectInformation.FileId.Volume,
                      pVolumeCB->ObjectInformation.FileId.Vnode,
                      pVolumeCB->ObjectInformation.FileId.Unique);

        *TargetVolumeCB = pVolumeCB;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSProcessDFSLink( IN AFSDirectoryCB *DirEntry,
                   IN PFILE_OBJECT FileObject,
                   IN UNICODE_STRING *RemainingPath,
                   IN GUID *AuthGroup)
{

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    UNICODE_STRING uniReparseName;
    UNICODE_STRING uniMUPDeviceName;
    UNICODE_STRING uniIOMgrDeviceName;
    AFSDirEnumEntry *pDirEntry = NULL;

    __Enter
    {

        //
        // Build up the name to reparse
        //

        RtlInitUnicodeString( &uniMUPDeviceName,
                              L"\\Device\\MUP");

        RtlInitUnicodeString( &uniIOMgrDeviceName,
                              L"\\??\\");

        uniReparseName.Length = 0;
        uniReparseName.Buffer = NULL;

        //
        // Be sure we have a target name
        //

        if( DirEntry->NameInformation.TargetName.Length == 0)
        {

            ntStatus = AFSEvaluateTargetByID( DirEntry->ObjectInformation,
                                              AuthGroup,
                                              FALSE,
                                              &pDirEntry);

            if( !NT_SUCCESS( ntStatus) ||
                pDirEntry->TargetNameLength == 0)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSProcessDFSLink EvaluateTargetByID failed for Fcb %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                              &DirEntry->NameInformation.FileName,
                              DirEntry->ObjectInformation->FileId.Cell,
                              DirEntry->ObjectInformation->FileId.Volume,
                              DirEntry->ObjectInformation->FileId.Vnode,
                              DirEntry->ObjectInformation->FileId.Unique,
                              ntStatus);

                if( NT_SUCCESS( ntStatus))
                {

                    ntStatus = STATUS_REPARSE_POINT_NOT_RESOLVED;
                }

                try_return( ntStatus);
            }

            //
            // Update the target name
            //

            AFSAcquireExcl( &DirEntry->NonPaged->Lock,
                            TRUE);

            ntStatus = AFSUpdateTargetName( &DirEntry->NameInformation.TargetName,
                                            &DirEntry->Flags,
                                            (WCHAR *)((char *)pDirEntry + pDirEntry->TargetNameOffset),
                                            (USHORT)pDirEntry->TargetNameLength);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSProcessDFSLink UpdateTargetName failed for Fcb %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                              &DirEntry->NameInformation.FileName,
                              DirEntry->ObjectInformation->FileId.Cell,
                              DirEntry->ObjectInformation->FileId.Volume,
                              DirEntry->ObjectInformation->FileId.Vnode,
                              DirEntry->ObjectInformation->FileId.Unique,
                              ntStatus);

                AFSReleaseResource( &DirEntry->NonPaged->Lock);

                try_return( ntStatus);
            }

            AFSConvertToShared( &DirEntry->NonPaged->Lock);
        }
        else
        {
            AFSAcquireShared( &DirEntry->NonPaged->Lock,
                              TRUE);
        }

        uniReparseName.MaximumLength = uniMUPDeviceName.Length +
                                                   sizeof( WCHAR) +
                                                   DirEntry->NameInformation.TargetName.Length +
                                                   sizeof( WCHAR);

        if( RemainingPath != NULL &&
            RemainingPath->Length > 0)
        {

            uniReparseName.MaximumLength += RemainingPath->Length;
        }

        //
        // Allocate the reparse buffer
        //

        uniReparseName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                   uniReparseName.MaximumLength,
                                                                   AFS_REPARSE_NAME_TAG);

        if( uniReparseName.Buffer == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSProcessDFSLink uniReparseName.Buffer == NULL Fcb %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          &DirEntry->NameInformation.FileName,
                          DirEntry->ObjectInformation->FileId.Cell,
                          DirEntry->ObjectInformation->FileId.Volume,
                          DirEntry->ObjectInformation->FileId.Vnode,
                          DirEntry->ObjectInformation->FileId.Unique,
                          STATUS_INSUFFICIENT_RESOURCES);

            AFSReleaseResource( &DirEntry->NonPaged->Lock);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Start building the name
        //

        if ( DirEntry->NameInformation.TargetName.Buffer[ 0] != L'\\' &&
             DirEntry->NameInformation.TargetName.Buffer[ 1] == L':')
        {

            RtlCopyMemory( uniReparseName.Buffer,
                           uniIOMgrDeviceName.Buffer,
                           uniIOMgrDeviceName.Length);

            uniReparseName.Length = uniIOMgrDeviceName.Length;
        }
        else
        {

            RtlCopyMemory( uniReparseName.Buffer,
                           uniMUPDeviceName.Buffer,
                           uniMUPDeviceName.Length);

            uniReparseName.Length = uniMUPDeviceName.Length;

            if( DirEntry->NameInformation.TargetName.Buffer[ 0] != L'\\')
            {

                uniReparseName.Buffer[ uniReparseName.Length/sizeof( WCHAR)] = L'\\';

                uniReparseName.Length += sizeof( WCHAR);
            }
        }

        RtlCopyMemory( &uniReparseName.Buffer[ uniReparseName.Length/sizeof( WCHAR)],
                       DirEntry->NameInformation.TargetName.Buffer,
                       DirEntry->NameInformation.TargetName.Length);

        uniReparseName.Length += DirEntry->NameInformation.TargetName.Length;

        AFSReleaseResource( &DirEntry->NonPaged->Lock);

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

            AFSExFreePoolWithTag( FileObject->FileName.Buffer, 0);
        }

        FileObject->FileName = uniReparseName;

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessDFSLink Reparsing access to Fcb %wZ FID %08lX-%08lX-%08lX-%08lX to %wZ\n",
                      &DirEntry->NameInformation.FileName,
                      DirEntry->ObjectInformation->FileId.Cell,
                      DirEntry->ObjectInformation->FileId.Volume,
                      DirEntry->ObjectInformation->FileId.Vnode,
                      DirEntry->ObjectInformation->FileId.Unique,
                      &uniReparseName);

        //
        // Return status reparse ...
        //

        ntStatus = STATUS_REPARSE;

try_exit:

        if ( pDirEntry)
        {

            AFSExFreePoolWithTag( pDirEntry, AFS_GENERIC_MEMORY_2_TAG);
        }
    }

    return ntStatus;
}
