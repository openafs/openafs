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
// File: AFSCommSupport.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSEnumerateDirectory( IN GUID *AuthGroup,
                       IN AFSObjectInfoCB *ObjectInfoCB,
                       IN BOOLEAN   FastQuery)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    void *pBuffer = NULL;
    ULONG ulResultLen = 0;
    AFSDirQueryCB *pDirQueryCB;
    AFSDirEnumEntry *pCurrentDirEntry = NULL;
    AFSDirectoryCB *pDirNode = NULL;
    ULONG  ulEntryLength = 0;
    AFSDirEnumResp *pDirEnumResponse = NULL;
    UNICODE_STRING uniDirName, uniTargetName;
    ULONG   ulRequestFlags = AFS_REQUEST_FLAG_SYNCHRONOUS;
    ULONG ulCRC = 0;
    UNICODE_STRING uniGUID;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        ASSERT( ExIsResourceAcquiredExclusiveLite( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock));

        uniGUID.Length = 0;
        uniGUID.MaximumLength = 0;
        uniGUID.Buffer = NULL;

        if( AuthGroup != NULL)
        {
            RtlStringFromGUID( *AuthGroup,
                               &uniGUID);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSEnumerateDirectory Enumerating FID %08lX-%08lX-%08lX-%08lX AuthGroup %wZ\n",
                      ObjectInfoCB->FileId.Cell,
                      ObjectInfoCB->FileId.Volume,
                      ObjectInfoCB->FileId.Vnode,
                      ObjectInfoCB->FileId.Unique,
                      &uniGUID);

        if( AuthGroup != NULL)
        {
            RtlFreeUnicodeString( &uniGUID);
        }

        //
        // Initialize the directory enumeration buffer for the directory
        //

        pBuffer = AFSExAllocatePoolWithTag( PagedPool,
                                            AFS_DIR_ENUM_BUFFER_LEN,
                                            AFS_DIR_BUFFER_TAG);

        if( pBuffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pBuffer,
                       AFS_DIR_ENUM_BUFFER_LEN);

        ulResultLen = AFS_DIR_ENUM_BUFFER_LEN;

        //
        // Use the payload buffer for information we will pass to the service
        //

        pDirQueryCB = (AFSDirQueryCB *)pBuffer;

        pDirQueryCB->EnumHandle = 0;

        if( FastQuery)
        {

            ulRequestFlags |= AFS_REQUEST_FLAG_FAST_REQUEST;
        }

        //
        // Loop on the information
        //

        while( TRUE)
        {

            //
            // If the enumeration handle is -1 then we are done
            //

            if( ((ULONG)-1) == pDirQueryCB->EnumHandle )
            {

                ntStatus = STATUS_NO_MORE_ENTRIES;
            }
            else
            {

                //
                // Go and retrieve the directory contents
                //

                ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_DIR_ENUM,
                                              ulRequestFlags,
                                              AuthGroup,
                                              NULL,
                                              &ObjectInfoCB->FileId,
                                              (void *)pDirQueryCB,
                                              sizeof( AFSDirQueryCB),
                                              pBuffer,
                                              &ulResultLen);
            }

            if( ntStatus != STATUS_SUCCESS ||
                ulResultLen == 0)
            {

                if( ntStatus == STATUS_NO_MORE_FILES ||
                    ntStatus == STATUS_NO_MORE_ENTRIES)
                {

                    ntStatus = STATUS_SUCCESS;

                    pDirEnumResponse = (AFSDirEnumResp *)pBuffer;

                    AFSAcquireExcl( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                    TRUE);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSEnumerateDirectory Directory Complete FID %08lX-%08lX-%08lX-%08lX Snapshot-DV %08lX:%08lX Current-DV %08lX:%08lX Status %08lX\n",
                                  ObjectInfoCB->FileId.Cell,
                                  ObjectInfoCB->FileId.Volume,
                                  ObjectInfoCB->FileId.Vnode,
                                  ObjectInfoCB->FileId.Unique,
                                  pDirEnumResponse->SnapshotDataVersion.HighPart,
                                  pDirEnumResponse->SnapshotDataVersion.LowPart,
                                  pDirEnumResponse->CurrentDataVersion.HighPart,
                                  pDirEnumResponse->CurrentDataVersion.LowPart,
                                  ntStatus);

                    ObjectInfoCB->DataVersion = pDirEnumResponse->SnapshotDataVersion;

                    if ( pDirEnumResponse->SnapshotDataVersion.QuadPart != pDirEnumResponse->CurrentDataVersion.QuadPart )
                    {

                        SetFlag( ObjectInfoCB->Flags, AFS_OBJECT_FLAGS_VERIFY);

                        ObjectInfoCB->DataVersion.QuadPart = (ULONGLONG)-1;

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSEnumerateDirectory Force Verify due to DV change during enumeration FID %08lX-%08lX-%08lX-%08lX\n",
                                      ObjectInfoCB->FileId.Cell,
                                      ObjectInfoCB->FileId.Volume,
                                      ObjectInfoCB->FileId.Vnode,
                                      ObjectInfoCB->FileId.Unique);
                    }

                    AFSReleaseResource( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock);
                }
                else
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSEnumerateDirectory Failed to enumerate directory FID %08lX-%08lX-%08lX-%08lX AuthGroup %wZ Status %08lX\n",
                                  ObjectInfoCB->FileId.Cell,
                                  ObjectInfoCB->FileId.Volume,
                                  ObjectInfoCB->FileId.Vnode,
                                  ObjectInfoCB->FileId.Unique,
                                  &uniGUID,
                                  ntStatus);
                }

                break;
            }

            pDirEnumResponse = (AFSDirEnumResp *)pBuffer;

            pCurrentDirEntry = (AFSDirEnumEntry *)pDirEnumResponse->Entry;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSEnumerateDirectory Enumerating FID %08lX-%08lX-%08lX-%08lX Snapshot-DV %08lX:%08lX Current-DV %08lX:%08lX\n",
                          ObjectInfoCB->FileId.Cell,
                          ObjectInfoCB->FileId.Volume,
                          ObjectInfoCB->FileId.Vnode,
                          ObjectInfoCB->FileId.Unique,
                          pDirEnumResponse->SnapshotDataVersion.HighPart,
                          pDirEnumResponse->SnapshotDataVersion.LowPart,
                          pDirEnumResponse->CurrentDataVersion.HighPart,
                          pDirEnumResponse->CurrentDataVersion.LowPart);

            //
            // Remove the leading header from the processed length
            //

            ulResultLen -= FIELD_OFFSET( AFSDirEnumResp, Entry);

            while( ulResultLen > 0)
            {

                uniDirName.Length = (USHORT)pCurrentDirEntry->FileNameLength;

                uniDirName.MaximumLength = uniDirName.Length;

                uniDirName.Buffer = (WCHAR *)((char *)pCurrentDirEntry + pCurrentDirEntry->FileNameOffset);

                uniTargetName.Length = (USHORT)pCurrentDirEntry->TargetNameLength;

                uniTargetName.MaximumLength = uniTargetName.Length;

                uniTargetName.Buffer = (WCHAR *)((char *)pCurrentDirEntry + pCurrentDirEntry->TargetNameOffset);

                //
                // Be sure we don't have this entry in the case sensitive tree
                //

                ulCRC = AFSGenerateCRC( &uniDirName,
                                        FALSE);

                AFSLocateCaseSensitiveDirEntry( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                ulCRC,
                                                &pDirNode);

                if( pDirNode != NULL)
                {

                    //
                    // Check that the FIDs are the same
                    //

                    if( AFSIsEqualFID( &pCurrentDirEntry->FileId,
                                       &pDirNode->ObjectInformation->FileId))
                    {

                        //
                        // Duplicate entry, skip it
                        //

                        ulEntryLength = QuadAlign( sizeof( AFSDirEnumEntry) +
                                                   uniDirName.Length +
                                                   uniTargetName.Length);

                        pCurrentDirEntry = (AFSDirEnumEntry *)((char *)pCurrentDirEntry + ulEntryLength);

                        if( ulResultLen >= ulEntryLength)
                        {
                            ulResultLen -= ulEntryLength;
                        }
                        else
                        {
                            ulResultLen = 0;
                        }

                        //
                        // Update the metadata for the entry
                        //

                        if( pDirNode->ObjectInformation->DataVersion.QuadPart != pCurrentDirEntry->DataVersion.QuadPart)
                        {

                            LONG lCount;
                            AFSObjectInfoCB *pObjectInfo = pDirNode->ObjectInformation;

                            //
                            // The ObjectReferenceCount will be freed by AFSPerformObjectInvalidate
                            //

                            lCount = AFSObjectInfoIncrement( pObjectInfo);

                            AFSPerformObjectInvalidate( pObjectInfo,
                                                        AFS_INVALIDATE_DATA_VERSION);
                        }
                        else
                        {

                            AFSUpdateMetaData( pDirNode,
                                               pCurrentDirEntry);
                        }

                        continue;
                    }
                    else
                    {

                        //
                        // Need to tear down this entry and rebuild it below
                        //

                        if( pDirNode->DirOpenReferenceCount <= 0)
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSEnumerateDirectory Different FIDs - Deleting DE %p for %wZ Old FID %08lX-%08lX-%08lX-%08lX New FID %08lX-%08lX-%08lX-%08lX\n",
                                          pDirNode,
                                          &pDirNode->NameInformation.FileName,
                                          pDirNode->ObjectInformation->FileId.Cell,
                                          pDirNode->ObjectInformation->FileId.Volume,
                                          pDirNode->ObjectInformation->FileId.Vnode,
                                          pDirNode->ObjectInformation->FileId.Unique,
                                          pCurrentDirEntry->FileId.Cell,
                                          pCurrentDirEntry->FileId.Volume,
                                          pCurrentDirEntry->FileId.Vnode,
                                          pCurrentDirEntry->FileId.Unique);

                            AFSDeleteDirEntry( ObjectInfoCB,
                                               pDirNode);
                        }
                        else
                        {

                            SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_DELETED);

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSEnumerateDirectory Different FIDs - Removing DE %p for %wZ Old FID %08lX-%08lX-%08lX-%08lX New FID %08lX-%08lX-%08lX-%08lX\n",
                                          pDirNode,
                                          &pDirNode->NameInformation.FileName,
                                          pDirNode->ObjectInformation->FileId.Cell,
                                          pDirNode->ObjectInformation->FileId.Volume,
                                          pDirNode->ObjectInformation->FileId.Vnode,
                                          pDirNode->ObjectInformation->FileId.Unique,
                                          pCurrentDirEntry->FileId.Cell,
                                          pCurrentDirEntry->FileId.Volume,
                                          pCurrentDirEntry->FileId.Vnode,
                                          pCurrentDirEntry->FileId.Unique);

                            AFSRemoveNameEntry( ObjectInfoCB,
                                                pDirNode);
                        }

                        pDirNode = NULL;
                    }
                }

                pDirNode = AFSInitDirEntry( ObjectInfoCB,
                                            &uniDirName,
                                            &uniTargetName,
                                            pCurrentDirEntry,
                                            (ULONG)InterlockedIncrement( &ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.ContentIndex));

                if( pDirNode == NULL)
                {

                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                    break;
                }

                AFSUpdateMetaData( pDirNode,
                                   pCurrentDirEntry);

                if( pDirNode->ObjectInformation->FileType == AFS_FILE_TYPE_DIRECTORY)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSEnumerateDirectory Setting VERIFY on entry %wZ for FID %08lX-%08lX-%08lX-%08lX\n",
                                  &uniDirName,
                                  pDirNode->ObjectInformation->FileId.Cell,
                                  pDirNode->ObjectInformation->FileId.Volume,
                                  pDirNode->ObjectInformation->FileId.Vnode,
                                  pDirNode->ObjectInformation->FileId.Unique);

                    AFSAcquireExcl( pDirNode->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                    TRUE);

                    SetFlag( pDirNode->ObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY);

                    pDirNode->ObjectInformation->DataVersion.QuadPart = (ULONGLONG)-1;

                    AFSReleaseResource( pDirNode->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);
                }

                //
                // Set up the entry length
                //

                ulEntryLength = QuadAlign( sizeof( AFSDirEnumEntry) +
                                           pCurrentDirEntry->FileNameLength +
                                           pCurrentDirEntry->TargetNameLength);

                //
                // Init the short name if we have one
                //

                if( !BooleanFlagOn( pDevExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
                    pCurrentDirEntry->ShortNameLength > 0)
                {

                    UNICODE_STRING uniShortName;

                    pDirNode->NameInformation.ShortNameLength = pCurrentDirEntry->ShortNameLength;

                    RtlCopyMemory( pDirNode->NameInformation.ShortName,
                                   pCurrentDirEntry->ShortName,
                                   pDirNode->NameInformation.ShortNameLength);

                    //
                    // Generate the short name index
                    //

                    uniShortName.Length = pDirNode->NameInformation.ShortNameLength;
                    uniShortName.MaximumLength = uniShortName.Length;
                    uniShortName.Buffer = pDirNode->NameInformation.ShortName;

                    if( !RtlIsNameLegalDOS8Dot3( &pDirNode->NameInformation.FileName,
                                                 NULL,
                                                 NULL))
                    {

                        pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = AFSGenerateCRC( &uniShortName,
                                                                                           TRUE);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSEnumerateDirectory Initialized short name %wZ for DE %p for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      &uniShortName,
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName,
                                      pCurrentDirEntry->FileId.Cell,
                                      pCurrentDirEntry->FileId.Volume,
                                      pCurrentDirEntry->FileId.Vnode,
                                      pCurrentDirEntry->FileId.Unique);
                    }
                    else
                    {
                        pDirNode->NameInformation.ShortNameLength = 0;

                        RtlZeroMemory( pDirNode->NameInformation.ShortName,
                                       (12 * sizeof( WCHAR)));
                    }
                }
                else
                {

                    //
                    // No short name or short names are disabled
                    //

                    pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = 0;
                }

                //
                // Insert the node into the name tree
                //

                if( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead == NULL)
                {

                    ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead = pDirNode;

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSEnumerateDirectory Insert DE %p to head of case sensitive tree for %wZ\n",
                                  pDirNode,
                                  &pDirNode->NameInformation.FileName);
                }
                else
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSEnumerateDirectory Insert DE %p to case sensitive tree for %wZ\n",
                                  pDirNode,
                                  &pDirNode->NameInformation.FileName);

                    if( !NT_SUCCESS( AFSInsertCaseSensitiveDirEntry( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                                     pDirNode)))
                    {

                        //
                        // Delete this dir entry and continue on
                        //

                        AFSDeleteDirEntry( ObjectInfoCB,
                                           pDirNode);

                        pCurrentDirEntry = (AFSDirEnumEntry *)((char *)pCurrentDirEntry + ulEntryLength);

                        if( ulResultLen >= ulEntryLength)
                        {
                            ulResultLen -= ulEntryLength;
                        }
                        else
                        {
                            ulResultLen = 0;
                        }

                        continue;
                    }
                }

                ClearFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE);

                if( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead == NULL)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSEnumerateDirectory Insert DE %p to head of case insensitive tree for %wZ\n",
                                  pDirNode,
                                  &pDirNode->NameInformation.FileName);

                    ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead = pDirNode;

                    SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);
                }
                else
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSEnumerateDirectory Insert DE %p to case insensitive tree for %wZ\n",
                                  pDirNode,
                                  &pDirNode->NameInformation.FileName);

                    AFSInsertCaseInsensitiveDirEntry( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                                      pDirNode);
                }

                if( ObjectInfoCB->Specific.Directory.DirectoryNodeListHead == NULL)
                {

                    ObjectInfoCB->Specific.Directory.DirectoryNodeListHead = pDirNode;
                }
                else
                {

                    ObjectInfoCB->Specific.Directory.DirectoryNodeListTail->ListEntry.fLink = pDirNode;

                    pDirNode->ListEntry.bLink = ObjectInfoCB->Specific.Directory.DirectoryNodeListTail;
                }

                ObjectInfoCB->Specific.Directory.DirectoryNodeListTail = pDirNode;

                SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_INSERTED_ENUM_LIST);

                InterlockedIncrement( &ObjectInfoCB->Specific.Directory.DirectoryNodeCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIR_NODE_COUNT,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSEnumerateDirectory Adding entry %wZ Inc Count %d to parent FID %08lX-%08lX-%08lX-%08lX\n",
                              &pDirNode->NameInformation.FileName,
                              ObjectInfoCB->Specific.Directory.DirectoryNodeCount,
                              ObjectInfoCB->FileId.Cell,
                              ObjectInfoCB->FileId.Volume,
                              ObjectInfoCB->FileId.Vnode,
                              ObjectInfoCB->FileId.Unique);

                if( pDirNode->Type.Data.ShortNameTreeEntry.HashIndex != 0)
                {

                    //
                    // Insert the short name entry if we have a valid short name
                    //

                    if( ObjectInfoCB->Specific.Directory.ShortNameTree == NULL)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSEnumerateDirectory Insert DE %p to head of shortname tree for %wZ\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName);

                        ObjectInfoCB->Specific.Directory.ShortNameTree = pDirNode;

                        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME);
                    }
                    else
                    {

                        if( NT_SUCCESS( AFSInsertShortNameDirEntry( ObjectInfoCB->Specific.Directory.ShortNameTree,
                                                                    pDirNode)))
                        {
                            SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME);

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSEnumerateDirectory Insert DE %p to shortname tree for %wZ\n",
                                          pDirNode,
                                          &pDirNode->NameInformation.FileName);
                        }
                    }
                }

                //
                // Next dir entry
                //

                pCurrentDirEntry = (AFSDirEnumEntry *)((char *)pCurrentDirEntry + ulEntryLength);

                if( ulResultLen >= ulEntryLength)
                {
                    ulResultLen -= ulEntryLength;
                }
                else
                {
                    ulResultLen = 0;
                }
            }

            ulResultLen = AFS_DIR_ENUM_BUFFER_LEN;

            //
            // Reset the information in the request buffer since it got trampled
            // above
            //

            pDirQueryCB->EnumHandle = pDirEnumResponse->EnumHandle;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSEnumerateDirectory EnumHandle %08lX\n",
                          pDirQueryCB->EnumHandle);
        }

try_exit:

        //
        // Cleanup
        //

        if( pBuffer != NULL)
        {

            AFSExFreePoolWithTag( pBuffer, AFS_DIR_BUFFER_TAG);
        }

        //
        // If the processing failed then we should reset the directory content in the event
        // it is re-enumerated
        //

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSEnumerateDirectory Resetting content for FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          ObjectInfoCB->FileId.Cell,
                          ObjectInfoCB->FileId.Volume,
                          ObjectInfoCB->FileId.Vnode,
                          ObjectInfoCB->FileId.Unique,
                          ntStatus);

            AFSResetDirectoryContent( ObjectInfoCB);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSEnumerateDirectoryNoResponse( IN GUID *AuthGroup,
                                 IN AFSFileID *FileId)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirQueryCB stDirQueryCB;
    ULONG   ulRequestFlags = 0;

    __Enter
    {

        //
        // Use the payload buffer for information we will pass to the service
        //

        stDirQueryCB.EnumHandle = 0;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_DIR_ENUM,
                                      ulRequestFlags,
                                      AuthGroup,
                                      NULL,
                                      FileId,
                                      (void *)&stDirQueryCB,
                                      sizeof( AFSDirQueryCB),
                                      NULL,
                                      NULL);

        if( ntStatus != STATUS_SUCCESS)
        {

            if( ntStatus == STATUS_NO_MORE_FILES ||
                ntStatus == STATUS_NO_MORE_ENTRIES)
            {

                ntStatus = STATUS_SUCCESS;
            }
            else
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSEnumerateDirectoryNoResponse Failed to enumerate directory Status %08lX\n",
                              ntStatus);
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSVerifyDirectoryContent( IN AFSObjectInfoCB *ObjectInfoCB,
                           IN GUID *AuthGroup)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    void *pBuffer = NULL;
    ULONG ulResultLen = 0;
    AFSDirQueryCB *pDirQueryCB;
    AFSDirEnumEntry *pCurrentDirEntry = NULL;
    AFSDirectoryCB *pDirNode = NULL;
    ULONG  ulEntryLength = 0;
    AFSDirEnumResp *pDirEnumResponse = NULL;
    UNICODE_STRING uniDirName, uniTargetName;
    ULONG   ulRequestFlags = AFS_REQUEST_FLAG_SYNCHRONOUS | AFS_REQUEST_FLAG_FAST_REQUEST;
    ULONG ulCRC = 0;
    AFSObjectInfoCB *pObjectInfo = NULL;
    ULONGLONG ullIndex = 0;
    UNICODE_STRING uniGUID;
    LONG lCount;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        ASSERT( ExIsResourceAcquiredExclusiveLite( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock));

        uniGUID.Length = 0;
        uniGUID.MaximumLength = 0;
        uniGUID.Buffer = NULL;

        if( AuthGroup != NULL)
        {
            RtlStringFromGUID( *AuthGroup,
                               &uniGUID);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSVerifyDirectoryContent Verifying content for FID %08lX-%08lX-%08lX-%08lX AuthGroup %wZ\n",
                      ObjectInfoCB->FileId.Cell,
                      ObjectInfoCB->FileId.Volume,
                      ObjectInfoCB->FileId.Vnode,
                      ObjectInfoCB->FileId.Unique,
                      &uniGUID);

        if( AuthGroup != NULL)
        {
            RtlFreeUnicodeString( &uniGUID);
        }

        //
        // Initialize the directory enumeration buffer for the directory
        //

        pBuffer = AFSExAllocatePoolWithTag( PagedPool,
                                            AFS_DIR_ENUM_BUFFER_LEN,
                                            AFS_DIR_BUFFER_TAG);

        if( pBuffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pBuffer,
                       AFS_DIR_ENUM_BUFFER_LEN);

        ulResultLen = AFS_DIR_ENUM_BUFFER_LEN;

        //
        // Use the payload buffer for information we will pass to the service
        //

        pDirQueryCB = (AFSDirQueryCB *)pBuffer;

        pDirQueryCB->EnumHandle = 0;

        //
        // Loop on the information
        //

        while( TRUE)
        {

            //
            // If the enumeration handle is -1 then we are done
            //

            if( ((ULONG)-1) == pDirQueryCB->EnumHandle )
            {

                ntStatus = STATUS_NO_MORE_ENTRIES;
            }
            else
            {

                //
                // Go and retrieve the directory contents
                //

                ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_DIR_ENUM,
                                              ulRequestFlags,
                                              AuthGroup,
                                              NULL,
                                              &ObjectInfoCB->FileId,
                                              (void *)pDirQueryCB,
                                              sizeof( AFSDirQueryCB),
                                              pBuffer,
                                              &ulResultLen);
            }

            if( ntStatus != STATUS_SUCCESS ||
                ulResultLen == 0)
            {

                if( ntStatus == STATUS_NO_MORE_FILES ||
                    ntStatus == STATUS_NO_MORE_ENTRIES)
                {

                    pDirEnumResponse = (AFSDirEnumResp *)pBuffer;

                    AFSAcquireExcl( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                    TRUE);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVerifyDirectoryContent Directory Complete FID %08lX-%08lX-%08lX-%08lX Snapshot-DV %08lX:%08lX Current-DV %08lX:%08lX Status %08lX\n",
                                  ObjectInfoCB->FileId.Cell,
                                  ObjectInfoCB->FileId.Volume,
                                  ObjectInfoCB->FileId.Vnode,
                                  ObjectInfoCB->FileId.Unique,
                                  pDirEnumResponse->SnapshotDataVersion.HighPart,
                                  pDirEnumResponse->SnapshotDataVersion.LowPart,
                                  pDirEnumResponse->CurrentDataVersion.HighPart,
                                  pDirEnumResponse->CurrentDataVersion.LowPart,
                                  ntStatus);

                    ntStatus = STATUS_SUCCESS;

                    if ( pDirEnumResponse->SnapshotDataVersion.QuadPart != pDirEnumResponse->CurrentDataVersion.QuadPart )
                    {

                        SetFlag( ObjectInfoCB->Flags, AFS_OBJECT_FLAGS_VERIFY);

                        ObjectInfoCB->DataVersion.QuadPart = (ULONGLONG)-1;

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSVerifyDirectoryContent Force Verify due to DV change during enumeration FID %08lX-%08lX-%08lX-%08lX\n",
                                      ObjectInfoCB->FileId.Cell,
                                      ObjectInfoCB->FileId.Volume,
                                      ObjectInfoCB->FileId.Vnode,
                                      ObjectInfoCB->FileId.Unique);
                    }
                    else
                    {

                        ObjectInfoCB->DataVersion = pDirEnumResponse->SnapshotDataVersion;
                    }

                    AFSReleaseResource( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock);
                }
                else
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSVerifyDirectoryContent Failed to enumerate directory FID %08lX-%08lX-%08lX-%08lX AuthGroup %wZ Status %08lX\n",
                                  ObjectInfoCB->FileId.Cell,
                                  ObjectInfoCB->FileId.Volume,
                                  ObjectInfoCB->FileId.Vnode,
                                  ObjectInfoCB->FileId.Unique,
                                  &uniGUID,
                                  ntStatus);
                }

                break;
            }

            pDirEnumResponse = (AFSDirEnumResp *)pBuffer;

            pCurrentDirEntry = (AFSDirEnumEntry *)pDirEnumResponse->Entry;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSVerifyDirectoryContent EnumResponse FID %08lX-%08lX-%08lX-%08lX Snapshot-DV %08lX:%08lX Current-DV %08lX:%08lX\n",
                          ObjectInfoCB->FileId.Cell,
                          ObjectInfoCB->FileId.Volume,
                          ObjectInfoCB->FileId.Vnode,
                          ObjectInfoCB->FileId.Unique,
                          pDirEnumResponse->SnapshotDataVersion.HighPart,
                          pDirEnumResponse->SnapshotDataVersion.LowPart,
                          pDirEnumResponse->CurrentDataVersion.HighPart,
                          pDirEnumResponse->CurrentDataVersion.LowPart);

            //
            // Remove the leading header from the processed length
            //

            ulResultLen -= FIELD_OFFSET( AFSDirEnumResp, Entry);

            while( ulResultLen > 0)
            {

                uniDirName.Length = (USHORT)pCurrentDirEntry->FileNameLength;

                uniDirName.MaximumLength = uniDirName.Length;

                uniDirName.Buffer = (WCHAR *)((char *)pCurrentDirEntry + pCurrentDirEntry->FileNameOffset);

                uniTargetName.Length = (USHORT)pCurrentDirEntry->TargetNameLength;

                uniTargetName.MaximumLength = uniTargetName.Length;

                uniTargetName.Buffer = (WCHAR *)((char *)pCurrentDirEntry + pCurrentDirEntry->TargetNameOffset);

                //
                // Does this entry already exist in the directory?
                //

                ulCRC = AFSGenerateCRC( &uniDirName,
                                        FALSE);

                AFSLocateCaseSensitiveDirEntry( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                ulCRC,
                                                &pDirNode);

                //
                //
                // Set up the entry length
                //

                ulEntryLength = QuadAlign( sizeof( AFSDirEnumEntry) +
                                           pCurrentDirEntry->FileNameLength +
                                           pCurrentDirEntry->TargetNameLength);

                if( pDirNode &&
                    AFSIsEqualFID( &pCurrentDirEntry->FileId,
                                   &pDirNode->ObjectInformation->FileId))
                {

                    //
                    // Found matching directory entry by name and FileID
                    //

                    AFSAcquireShared( ObjectInfoCB->VolumeCB->ObjectInfoTree.TreeLock,
                                      TRUE);

                    ullIndex = AFSCreateLowIndex( &pCurrentDirEntry->FileId);

                    ntStatus = AFSLocateHashEntry( ObjectInfoCB->VolumeCB->ObjectInfoTree.TreeHead,
                                                   ullIndex,
                                                   (AFSBTreeEntry **)&pObjectInfo);

                    AFSReleaseResource( ObjectInfoCB->VolumeCB->ObjectInfoTree.TreeLock);

                    if( NT_SUCCESS( ntStatus) &&
                        pObjectInfo != NULL)
                    {

                        //
                        // Indicate this is a valid entry
                        //

                        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_VALID);

                        if( pCurrentDirEntry->ShortNameLength > 0 &&
                            pDirNode->NameInformation.ShortNameLength > 0)
                        {
                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSVerifyDirectoryContent Verified entry %wZ (%wZ) parent FID %08lX-%08lX-%08lX-%08lX old short name %S New short name %S\n",
                                          &uniDirName,
                                          &pDirNode->NameInformation.FileName,
                                          ObjectInfoCB->FileId.Cell,
                                          ObjectInfoCB->FileId.Volume,
                                          ObjectInfoCB->FileId.Vnode,
                                          ObjectInfoCB->FileId.Unique,
                                          pDirNode->NameInformation.ShortName,
                                          pCurrentDirEntry->ShortName);
                        }
                        else if( pCurrentDirEntry->ShortNameLength == 0 &&
                                 pDirNode->NameInformation.ShortNameLength > 0)
                        {

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSVerifyDirectoryContent Verified entry %wZ (%wZ) parent FID %08lX-%08lX-%08lX-%08lX old short name %S New short name NULL\n",
                                          &uniDirName,
                                          &pDirNode->NameInformation.FileName,
                                          ObjectInfoCB->FileId.Cell,
                                          ObjectInfoCB->FileId.Volume,
                                          ObjectInfoCB->FileId.Vnode,
                                          ObjectInfoCB->FileId.Unique,
                                          pDirNode->NameInformation.ShortName);
                        }
                        else if( pCurrentDirEntry->ShortNameLength > 0 &&
                                 pDirNode->NameInformation.ShortNameLength == 0)
                        {
                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSVerifyDirectoryContent Verified entry %wZ (%wZ) parent FID %08lX-%08lX-%08lX-%08lX old short name NULL New short name %S\n",
                                          &uniDirName,
                                          &pDirNode->NameInformation.FileName,
                                          ObjectInfoCB->FileId.Cell,
                                          ObjectInfoCB->FileId.Volume,
                                          ObjectInfoCB->FileId.Vnode,
                                          ObjectInfoCB->FileId.Unique,
                                          pCurrentDirEntry->ShortName);
                        }
                        else
                        {
                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSVerifyDirectoryContent Verified entry %wZ (%wZ) parent FID %08lX-%08lX-%08lX-%08lX old short name NULL New short name NULL\n",
                                          &uniDirName,
                                          &pDirNode->NameInformation.FileName,
                                          ObjectInfoCB->FileId.Cell,
                                          ObjectInfoCB->FileId.Volume,
                                          ObjectInfoCB->FileId.Vnode,
                                          ObjectInfoCB->FileId.Unique);
                        }

                        //
                        // Update the metadata for the entry
                        //

                        if( pObjectInfo->DataVersion.QuadPart != pCurrentDirEntry->DataVersion.QuadPart)
                        {

                            //
                            // The ObjectReferenceCount will be freed by AFSPerformObjectInvalidate
                            // if successfully queued.  Cannot call AFSPerformObjectInvalidate directly
                            // because ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock is
                            // held during the sequence AFSVerifyEntry->AFSValidateDirectoryCache->
                            // AFSVerifyDirectoryContent and AFSPerformObjectInvalidate requires the
                            // Fcb->NPFcb->Resource which must be held prior to the TreeLock in the
                            // lock hierarchy.
                            //

                            lCount = AFSObjectInfoIncrement( pObjectInfo);

                            if ( !NT_SUCCESS( AFSQueueInvalidateObject( pObjectInfo,
                                                                        AFS_INVALIDATE_DATA_VERSION)))
                            {

                                lCount = AFSObjectInfoDecrement( pObjectInfo);
                            }
                        }
                        else
                        {

                            AFSUpdateMetaData( pDirNode,
                                               pCurrentDirEntry);
                        }

                        //
                        // Next dir entry
                        //

                        pCurrentDirEntry = (AFSDirEnumEntry *)((char *)pCurrentDirEntry + ulEntryLength);

                        if( ulResultLen >= ulEntryLength)
                        {
                            ulResultLen -= ulEntryLength;
                        }
                        else
                        {
                            ulResultLen = 0;
                        }

                        continue;
                    }
                }
                else if ( pDirNode)
                {

                    //
                    // File name matches but FileID does not.
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVerifyDirectoryContent Processing dir entry %p %wZ with different FID, same name in parent FID %08lX-%08lX-%08lX-%08lX\n",
                                  pDirNode,
                                  &pDirNode->NameInformation.FileName,
                                  ObjectInfoCB->FileId.Cell,
                                  ObjectInfoCB->FileId.Volume,
                                  ObjectInfoCB->FileId.Vnode,
                                  ObjectInfoCB->FileId.Unique);

                    //
                    // Need to tear down this entry and rebuild it below
                    //

                    if( pDirNode->DirOpenReferenceCount <= 0)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSVerifyDirectoryContent Different FIDs - Deleting DE %p for %wZ Old FID %08lX-%08lX-%08lX-%08lX New FID %08lX-%08lX-%08lX-%08lX\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName,
                                      pDirNode->ObjectInformation->FileId.Cell,
                                      pDirNode->ObjectInformation->FileId.Volume,
                                      pDirNode->ObjectInformation->FileId.Vnode,
                                      pDirNode->ObjectInformation->FileId.Unique,
                                      pCurrentDirEntry->FileId.Cell,
                                      pCurrentDirEntry->FileId.Volume,
                                      pCurrentDirEntry->FileId.Vnode,
                                      pCurrentDirEntry->FileId.Unique);

                        AFSDeleteDirEntry( ObjectInfoCB,
                                           pDirNode);
                    }
                    else
                    {

                        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_DELETED);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_WARNING,
                                      "AFSVerifyDirectoryContent Different FIDs - removing DE %p for %wZ Old FID %08lX-%08lX-%08lX-%08lX New FID %08lX-%08lX-%08lX-%08lX\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName,
                                      pDirNode->ObjectInformation->FileId.Cell,
                                      pDirNode->ObjectInformation->FileId.Volume,
                                      pDirNode->ObjectInformation->FileId.Vnode,
                                      pDirNode->ObjectInformation->FileId.Unique,
                                      pCurrentDirEntry->FileId.Cell,
                                      pCurrentDirEntry->FileId.Volume,
                                      pCurrentDirEntry->FileId.Vnode,
                                      pCurrentDirEntry->FileId.Unique);

                        AFSRemoveNameEntry( ObjectInfoCB,
                                            pDirNode);
                    }
                }
                else
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVerifyDirectoryContent New entry %wZ for parent FID %08lX-%08lX-%08lX-%08lX\n",
                                  &uniDirName,
                                  ObjectInfoCB->FileId.Cell,
                                  ObjectInfoCB->FileId.Volume,
                                  ObjectInfoCB->FileId.Vnode,
                                  ObjectInfoCB->FileId.Unique);
                }

                pDirNode = AFSInitDirEntry( ObjectInfoCB,
                                            &uniDirName,
                                            &uniTargetName,
                                            pCurrentDirEntry,
                                            (ULONG)InterlockedIncrement( &ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.ContentIndex));

                if( pDirNode == NULL)
                {

                    ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                    break;
                }

                AFSUpdateMetaData( pDirNode,
                                   pCurrentDirEntry);

                if( pDirNode->ObjectInformation->FileType == AFS_FILE_TYPE_DIRECTORY)
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVerifyDirectoryContent Setting VERIFY on entry %wZ for FID %08lX-%08lX-%08lX-%08lX\n",
                                  &uniDirName,
                                  pDirNode->ObjectInformation->FileId.Cell,
                                  pDirNode->ObjectInformation->FileId.Volume,
                                  pDirNode->ObjectInformation->FileId.Vnode,
                                  pDirNode->ObjectInformation->FileId.Unique);

                    AFSAcquireExcl( pDirNode->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                    TRUE);

                    SetFlag( pDirNode->ObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY);

                    pDirNode->ObjectInformation->DataVersion.QuadPart = (ULONGLONG)-1;

                    AFSReleaseResource( pDirNode->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);
                }

                //
                // Init the short name if we have one
                //

                if( !BooleanFlagOn( pDevExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
                    pCurrentDirEntry->ShortNameLength > 0)
                {

                    UNICODE_STRING uniShortName;

                    pDirNode->NameInformation.ShortNameLength = pCurrentDirEntry->ShortNameLength;

                    RtlCopyMemory( pDirNode->NameInformation.ShortName,
                                   pCurrentDirEntry->ShortName,
                                   pDirNode->NameInformation.ShortNameLength);

                    //
                    // Generate the short name index
                    //

                    uniShortName.Length = pDirNode->NameInformation.ShortNameLength;
                    uniShortName.MaximumLength = uniShortName.Length;
                    uniShortName.Buffer = pDirNode->NameInformation.ShortName;

                    if( !RtlIsNameLegalDOS8Dot3( &pDirNode->NameInformation.FileName,
                                                 NULL,
                                                 NULL))
                    {

                        pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = AFSGenerateCRC( &uniShortName,
                                                                                           TRUE);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSVerifyDirectoryContent Initialized short name %wZ for DE %p for %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                      &uniShortName,
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName,
                                      pCurrentDirEntry->FileId.Cell,
                                      pCurrentDirEntry->FileId.Volume,
                                      pCurrentDirEntry->FileId.Vnode,
                                      pCurrentDirEntry->FileId.Unique);
                    }
                    else
                    {
                        pDirNode->NameInformation.ShortNameLength = 0;

                        RtlZeroMemory( pDirNode->NameInformation.ShortName,
                                       (12 * sizeof( WCHAR)));
                    }
                }
                else
                {

                    //
                    // No short name or short names have been disabled
                    //

                    pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = 0;
                }

                //
                // Insert the node into the name tree
                //

                ASSERT( ExIsResourceAcquiredExclusiveLite( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock));

                if( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead == NULL)
                {

                    ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead = pDirNode;

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVerifyDirectoryContent Insert DE %p to head of case sensitive tree for %wZ\n",
                                  pDirNode,
                                  &pDirNode->NameInformation.FileName);
                }
                else
                {

                    if( !NT_SUCCESS( AFSInsertCaseSensitiveDirEntry( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                                     pDirNode)))
                    {
                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSVerifyDirectoryContent Failed to insert DE %p to case sensitive tree for %wZ\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName);

                        //
                        // Delete this dir entry and continue on
                        //

                        AFSDeleteDirEntry( ObjectInfoCB,
                                           pDirNode);

                        pCurrentDirEntry = (AFSDirEnumEntry *)((char *)pCurrentDirEntry + ulEntryLength);

                        if( ulResultLen >= ulEntryLength)
                        {
                            ulResultLen -= ulEntryLength;
                        }
                        else
                        {
                            ulResultLen = 0;
                        }

                        continue;
                    }
                    else
                    {
                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSVerifyDirectoryContent Insert DE %p to case sensitive tree for %wZ\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName);
                    }
                }

                ClearFlag( pDirNode->Flags, AFS_DIR_ENTRY_NOT_IN_PARENT_TREE);

                if( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead == NULL)
                {

                    ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead = pDirNode;

                    SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVerifyDirectoryContent Insert DE %p to head of case insensitive tree for %wZ\n",
                                  pDirNode,
                                  &pDirNode->NameInformation.FileName);
                }
                else
                {

                    AFSInsertCaseInsensitiveDirEntry( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                                      pDirNode);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVerifyDirectoryContent Insert DE %p to case insensitive tree for %wZ\n",
                                  pDirNode,
                                  &pDirNode->NameInformation.FileName);
                }

                if( ObjectInfoCB->Specific.Directory.DirectoryNodeListHead == NULL)
                {

                    ObjectInfoCB->Specific.Directory.DirectoryNodeListHead = pDirNode;
                }
                else
                {

                    (ObjectInfoCB->Specific.Directory.DirectoryNodeListTail)->ListEntry.fLink = pDirNode;

                    pDirNode->ListEntry.bLink = ObjectInfoCB->Specific.Directory.DirectoryNodeListTail;
                }

                ObjectInfoCB->Specific.Directory.DirectoryNodeListTail = pDirNode;

                SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_INSERTED_ENUM_LIST);

                InterlockedIncrement( &ObjectInfoCB->Specific.Directory.DirectoryNodeCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIR_NODE_COUNT,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSVerifyDirectoryContent Adding entry %wZ Inc Count %d to parent FID %08lX-%08lX-%08lX-%08lX\n",
                              &pDirNode->NameInformation.FileName,
                              ObjectInfoCB->Specific.Directory.DirectoryNodeCount,
                              ObjectInfoCB->FileId.Cell,
                              ObjectInfoCB->FileId.Volume,
                              ObjectInfoCB->FileId.Vnode,
                              ObjectInfoCB->FileId.Unique);

                if( pDirNode->Type.Data.ShortNameTreeEntry.HashIndex != 0)
                {

                    //
                    // Insert the short name entry if we have a valid short name
                    //

                    if( ObjectInfoCB->Specific.Directory.ShortNameTree == NULL)
                    {

                        ObjectInfoCB->Specific.Directory.ShortNameTree = pDirNode;

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSVerifyDirectoryContent Insert DE %p to head of shortname tree for %wZ\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName);

                        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME);
                    }
                    else
                    {

                        if( !NT_SUCCESS( AFSInsertShortNameDirEntry( ObjectInfoCB->Specific.Directory.ShortNameTree,
                                                                     pDirNode)))
                        {
                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSVerifyDirectoryContent Failed to insert DE %p (%08lX) to shortname tree for %wZ\n",
                                          pDirNode,
                                          pDirNode->Type.Data.ShortNameTreeEntry.HashIndex,
                                          &pDirNode->NameInformation.FileName);
                        }
                        else
                        {
                            SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_INSERTED_SHORT_NAME);

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSVerifyDirectoryContent Insert DE %p to shortname tree for %wZ\n",
                                          pDirNode,
                                          &pDirNode->NameInformation.FileName);
                        }
                    }
                }

                SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_VALID);

                //
                // Next dir entry
                //

                pCurrentDirEntry = (AFSDirEnumEntry *)((char *)pCurrentDirEntry + ulEntryLength);

                if( ulResultLen >= ulEntryLength)
                {
                    ulResultLen -= ulEntryLength;
                }
                else
                {
                    ulResultLen = 0;
                }
            }

            ulResultLen = AFS_DIR_ENUM_BUFFER_LEN;

            //
            // Reset the information in the request buffer since it got trampled
            // above
            //

            pDirQueryCB->EnumHandle = pDirEnumResponse->EnumHandle;
        }

try_exit:

        //
        // Cleanup
        //

        if( pBuffer != NULL)
        {

            AFSExFreePoolWithTag( pBuffer, AFS_DIR_BUFFER_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyFileCreate( IN GUID            *AuthGroup,
                     IN AFSObjectInfoCB *ParentObjectInfo,
                     IN PLARGE_INTEGER FileSize,
                     IN ULONG FileAttributes,
                     IN UNICODE_STRING *FileName,
                     OUT AFSDirectoryCB **DirNode)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFileCreateCB stCreateCB;
    AFSFileCreateResultCB *pResultCB = NULL;
    ULONG ulResultLen = 0;
    UNICODE_STRING uniTargetName;
    AFSDirectoryCB *pDirNode = NULL;
    ULONG     ulCRC = 0;
    LONG       lCount;
    LARGE_INTEGER liOldDataVersion;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    BOOLEAN bReleaseParentTreeLock = FALSE;

    __Enter
    {

        *DirNode = NULL;

        //
        // Init the control block for the request
        //

        RtlZeroMemory( &stCreateCB,
                       sizeof( AFSFileCreateCB));

        stCreateCB.ParentId = ParentObjectInfo->FileId;

        stCreateCB.AllocationSize = *FileSize;

        stCreateCB.FileAttributes = FileAttributes;

        stCreateCB.EaSize = 0;

        liOldDataVersion = ParentObjectInfo->DataVersion;

        //
        // Allocate our return buffer
        //

        pResultCB = (AFSFileCreateResultCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                       PAGE_SIZE,
                                                                       AFS_GENERIC_MEMORY_1_TAG);

        if( pResultCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pResultCB,
                       PAGE_SIZE);

        ulResultLen = PAGE_SIZE;

        //
        // Send the call to the service
        //

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_CREATE_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS | AFS_REQUEST_FLAG_HOLD_FID,
                                      AuthGroup,
                                      FileName,
                                      NULL,
                                      &stCreateCB,
                                      sizeof( AFSFileCreateCB),
                                      pResultCB,
                                      &ulResultLen);

        if( ntStatus != STATUS_SUCCESS)
        {

            if( NT_SUCCESS( ntStatus))
            {

                ntStatus = STATUS_DEVICE_NOT_READY;
            }

            try_return( ntStatus);
        }

        //
        // We may have raced with an invalidation call and a subsequent re-enumeration of this parent
        // and though we created the node, it is already in our list. If this is the case then
        // look up the entry rather than create a new entry
        // The check is to ensure the DV has been modified
        //

        AFSAcquireExcl( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        bReleaseParentTreeLock = TRUE;

        if( ParentObjectInfo->DataVersion.QuadPart != pResultCB->ParentDataVersion.QuadPart - 1)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSNotifyFileCreate Raced with an invalidate call and a re-enumeration for entry %wZ ParentFID %08lX-%08lX-%08lX-%08lX Version (%08lX:%08lX != %08lX:%08lX - 1)\n",
                          FileName,
                          ParentObjectInfo->FileId.Cell,
                          ParentObjectInfo->FileId.Volume,
                          ParentObjectInfo->FileId.Vnode,
                          ParentObjectInfo->FileId.Unique,
                          ParentObjectInfo->DataVersion.HighPart,
                          ParentObjectInfo->DataVersion.LowPart,
                          pResultCB->ParentDataVersion.HighPart,
                          pResultCB->ParentDataVersion.LowPart);

            //
            // We raced so go and lookup the directory entry in the parent
            //

            ulCRC = AFSGenerateCRC( FileName,
                                    FALSE);

            AFSLocateCaseSensitiveDirEntry( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                            ulCRC,
                                            &pDirNode);

            if( pDirNode != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSNotifyFileCreate Located dir entry %p for file %wZ\n",
                              pDirNode,
                              FileName);

                if ( AFSIsEqualFID( &pDirNode->ObjectInformation->FileId,
                                    &pResultCB->DirEnum.FileId))
                {

                    *DirNode = pDirNode;

                    try_return( ntStatus = STATUS_REPARSE);
                }
                else
                {

                    //
                    // We found an entry that matches the desired name but it is not the
                    // same as the one that was created for us by the file server.
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSNotifyFileCreate Found matching name entry %wZ DE %p FID %08lX-%08lX-%08lX-%08lX != FID %08lX-%08lX-%08lX-%08lX\n",
                                  FileName,
                                  pDirNode,
                                  pDirNode->ObjectInformation->FileId.Cell,
                                  pDirNode->ObjectInformation->FileId.Volume,
                                  pDirNode->ObjectInformation->FileId.Vnode,
                                  pDirNode->ObjectInformation->FileId.Unique,
                                  pResultCB->DirEnum.FileId.Cell,
                                  pResultCB->DirEnum.FileId.Volume,
                                  pResultCB->DirEnum.FileId.Vnode,
                                  pResultCB->DirEnum.FileId.Unique);

                    if( pDirNode->DirOpenReferenceCount <= 0)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSNotifyFileCreate Different FIDs - Deleting DE %p for %wZ Old FID %08lX-%08lX-%08lX-%08lX New FID %08lX-%08lX-%08lX-%08lX\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName,
                                      pDirNode->ObjectInformation->FileId.Cell,
                                      pDirNode->ObjectInformation->FileId.Volume,
                                      pDirNode->ObjectInformation->FileId.Vnode,
                                      pDirNode->ObjectInformation->FileId.Unique,
                                      pResultCB->DirEnum.FileId.Cell,
                                      pResultCB->DirEnum.FileId.Volume,
                                      pResultCB->DirEnum.FileId.Vnode,
                                      pResultCB->DirEnum.FileId.Unique);

                        AFSDeleteDirEntry( ParentObjectInfo,
                                           pDirNode);
                    }
                    else
                    {

                        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_DELETED);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSNotifyFileCreate Different FIDs - Removing DE %p for %wZ Old FID %08lX-%08lX-%08lX-%08lX New FID %08lX-%08lX-%08lX-%08lX\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName,
                                      pDirNode->ObjectInformation->FileId.Cell,
                                      pDirNode->ObjectInformation->FileId.Volume,
                                      pDirNode->ObjectInformation->FileId.Vnode,
                                      pDirNode->ObjectInformation->FileId.Unique,
                                      pResultCB->DirEnum.FileId.Cell,
                                      pResultCB->DirEnum.FileId.Volume,
                                      pResultCB->DirEnum.FileId.Vnode,
                                      pResultCB->DirEnum.FileId.Unique);

                        AFSRemoveNameEntry( ParentObjectInfo,
                                            pDirNode);
                    }

                    pDirNode = NULL;
                }
            }

            //
            // We are unsure of our current data so set the verify flag. It may already be set
            // but no big deal to reset it
            //

            SetFlag( ParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

            ParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNotifyFileCreate Creating new entry %wZ\n",
                      FileName);

        //
        // Initialize the directory entry
        //

        uniTargetName.Length = (USHORT)pResultCB->DirEnum.TargetNameLength;

        uniTargetName.MaximumLength = uniTargetName.Length;

        uniTargetName.Buffer = (WCHAR *)((char *)&pResultCB->DirEnum + pResultCB->DirEnum.TargetNameOffset);

        pDirNode = AFSInitDirEntry( ParentObjectInfo,
                                    FileName,
                                    &uniTargetName,
                                    &pResultCB->DirEnum,
                                    (ULONG)InterlockedIncrement( &ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.ContentIndex));

        if( pDirNode == NULL)
        {

            SetFlag( ParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

            ParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Init the short name if we have one
        //

        if( !BooleanFlagOn( pDevExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
            pResultCB->DirEnum.ShortNameLength > 0)
        {

            UNICODE_STRING uniShortName;

            pDirNode->NameInformation.ShortNameLength = pResultCB->DirEnum.ShortNameLength;

            RtlCopyMemory( pDirNode->NameInformation.ShortName,
                           pResultCB->DirEnum.ShortName,
                           pDirNode->NameInformation.ShortNameLength);

            //
            // Generate the short name index
            //

            uniShortName.Length = pDirNode->NameInformation.ShortNameLength;
            uniShortName.Buffer = pDirNode->NameInformation.ShortName;

            pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = AFSGenerateCRC( &uniShortName,
                                                                               TRUE);

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNotifyFileCreate Initialized short name %wZ for DE %p for %wZ\n",
                          &uniShortName,
                          pDirNode,
                          &pDirNode->NameInformation.FileName);
        }
        else
        {
            //
            // No short name or short names are disabled
            //

            pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = 0;
        }

        if ( !BooleanFlagOn( ParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY))
        {

            //
            // Update the parent data version
            //

            ParentObjectInfo->DataVersion = pResultCB->ParentDataVersion;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNotifyFileCreate entry %wZ ParentFID %08lX-%08lX-%08lX-%08lX Version %08lX:%08lX\n",
                          FileName,
                          ParentObjectInfo->FileId.Cell,
                          ParentObjectInfo->FileId.Volume,
                          ParentObjectInfo->FileId.Vnode,
                          ParentObjectInfo->FileId.Unique,
                          ParentObjectInfo->DataVersion.QuadPart);
        }

        //
        // Return the directory node
        //

        *DirNode = pDirNode;

try_exit:

        if ( *DirNode != NULL)
        {

            lCount = InterlockedIncrement( &(*DirNode)->DirOpenReferenceCount);

            AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNotifyFileCreate Increment count on %wZ DE %p Cnt %d\n",
                          &(*DirNode)->NameInformation.FileName,
                          *DirNode,
                          lCount);

            ASSERT( lCount >= 0);
        }

        if ( bReleaseParentTreeLock)
        {

            AFSReleaseResource( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);
        }

        if( pResultCB != NULL)
        {

            AFSExFreePoolWithTag( pResultCB, AFS_GENERIC_MEMORY_1_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSUpdateFileInformation( IN AFSFileID *ParentFid,
                          IN AFSObjectInfoCB *ObjectInfo,
                          IN GUID *AuthGroup)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFileUpdateCB stUpdateCB;
    ULONG ulResultLen = 0;
    AFSFileUpdateResultCB *pUpdateResultCB = NULL;

    __Enter
    {

        //
        // Init the control block for the request
        //

        RtlZeroMemory( &stUpdateCB,
                       sizeof( AFSFileUpdateCB));

        stUpdateCB.AllocationSize = ObjectInfo->EndOfFile;

        stUpdateCB.FileAttributes = ObjectInfo->FileAttributes;

        stUpdateCB.EaSize = ObjectInfo->EaSize;

        stUpdateCB.ParentId = *ParentFid;

        stUpdateCB.LastAccessTime = ObjectInfo->LastAccessTime;

        stUpdateCB.CreateTime = ObjectInfo->CreationTime;

        stUpdateCB.ChangeTime = ObjectInfo->ChangeTime;

        stUpdateCB.LastWriteTime = ObjectInfo->LastWriteTime;

        pUpdateResultCB = (AFSFileUpdateResultCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                             PAGE_SIZE,
                                                                             AFS_UPDATE_RESULT_TAG);

        if( pUpdateResultCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        ulResultLen = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_UPDATE_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      AuthGroup,
                                      NULL,
                                      &ObjectInfo->FileId,
                                      &stUpdateCB,
                                      sizeof( AFSFileUpdateCB),
                                      pUpdateResultCB,
                                      &ulResultLen);

        if( ntStatus != STATUS_SUCCESS)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSUpdateFileInformation failed FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          ObjectInfo->FileId.Cell,
                          ObjectInfo->FileId.Volume,
                          ObjectInfo->FileId.Vnode,
                          ObjectInfo->FileId.Unique,
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // Update the data version
        //

        AFSAcquireExcl( ObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        if ( !BooleanFlagOn( ObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY))
        {

            ObjectInfo->DataVersion = pUpdateResultCB->DirEnum.DataVersion;
        }

        AFSReleaseResource( ObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);

try_exit:

        if( pUpdateResultCB != NULL)
        {

            AFSExFreePoolWithTag( pUpdateResultCB, AFS_UPDATE_RESULT_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyDelete( IN AFSDirectoryCB *DirectoryCB,
                 IN GUID           *AuthGroup,
                 IN BOOLEAN         CheckOnly)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulResultLen = 0;
    AFSFileDeleteCB stDelete;
    AFSFileDeleteResultCB stDeleteResult;
    ULONG ulRequestFlags = AFS_REQUEST_FLAG_SYNCHRONOUS;

    __Enter
    {

        stDelete.ParentId = DirectoryCB->ObjectInformation->ParentObjectInformation->FileId;

        stDelete.ProcessId = (ULONGLONG)PsGetCurrentProcessId();

        ulResultLen = sizeof( AFSFileDeleteResultCB);

        if( CheckOnly)
        {
            ulRequestFlags |= AFS_REQUEST_FLAG_CHECK_ONLY;
        }

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_DELETE_FILE,
                                      ulRequestFlags,
                                      AuthGroup,
                                      &DirectoryCB->NameInformation.FileName,
                                      &DirectoryCB->ObjectInformation->FileId,
                                      &stDelete,
                                      sizeof( AFSFileDeleteCB),
                                      &stDeleteResult,
                                      &ulResultLen);

        if( ntStatus != STATUS_SUCCESS)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNotifyDelete failed ParentFID %08lX-%08lX-%08lX-%08lX %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          stDelete.ParentId.Cell,
                          stDelete.ParentId.Volume,
                          stDelete.ParentId.Vnode,
                          stDelete.ParentId.Unique,
                          &DirectoryCB->NameInformation.FileName,
                          DirectoryCB->ObjectInformation->FileId.Cell,
                          DirectoryCB->ObjectInformation->FileId.Volume,
                          DirectoryCB->ObjectInformation->FileId.Vnode,
                          DirectoryCB->ObjectInformation->FileId.Unique,
                          ntStatus);

            try_return( ntStatus);
        }

        AFSAcquireExcl( DirectoryCB->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        if( CheckOnly)
        {

            //
            // Validate the parent data version
            //

            if( DirectoryCB->ObjectInformation->ParentObjectInformation->DataVersion.QuadPart != stDeleteResult.ParentDataVersion.QuadPart)
            {

                SetFlag( DirectoryCB->ObjectInformation->ParentObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY);

                DirectoryCB->ObjectInformation->ParentObjectInformation->DataVersion.QuadPart = (ULONGLONG)-1;
            }
        }
        else
        {

            //
            // Update the parent data version
            //

            if( DirectoryCB->ObjectInformation->ParentObjectInformation->DataVersion.QuadPart != stDeleteResult.ParentDataVersion.QuadPart - 1)
            {

                SetFlag( DirectoryCB->ObjectInformation->ParentObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY);

                DirectoryCB->ObjectInformation->ParentObjectInformation->DataVersion.QuadPart = (ULONGLONG)-1;
            }
            else
            {

                //
                // TODO -- The entry must be removed from the directory at which point the
                // Directory data version number can be updated.  Until then we must force
                // a verification.
                //
                // DirectoryCB->ObjectInformation->ParentObjectInformation->DataVersion.QuadPart = stDeleteResult.ParentDataVersion.QuadPart;
                //

                SetFlag( DirectoryCB->ObjectInformation->ParentObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY);

                DirectoryCB->ObjectInformation->ParentObjectInformation->DataVersion.QuadPart = (ULONGLONG)-1;
            }
        }

        AFSReleaseResource( DirectoryCB->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

try_exit:

        NOTHING;
    }

    return ntStatus;
}


NTSTATUS
AFSNotifyHardLink( IN AFSObjectInfoCB *ObjectInfo,
                   IN GUID            *AuthGroup,
                   IN AFSObjectInfoCB *ParentObjectInfo,
                   IN AFSObjectInfoCB *TargetParentObjectInfo,
                   IN AFSDirectoryCB  *SourceDirectoryCB,
                   IN UNICODE_STRING  *TargetName,
                   IN BOOLEAN          bReplaceIfExists,
                   OUT AFSDirectoryCB **TargetDirectoryCB)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFileHardLinkCB *pHardLinkCB = NULL;
    AFSFileHardLinkResultCB *pResultCB = NULL;
    ULONG ulResultLen = 0;
    AFSDirectoryCB *pDirNode = NULL;
    ULONG     ulCRC = 0;
    BOOLEAN bReleaseParentLock = FALSE, bReleaseTargetParentLock = FALSE;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;
    LONG lCount;

    __Enter
    {

        //
        // Init the control block for the request
        //

        pHardLinkCB = (AFSFileHardLinkCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                     PAGE_SIZE,
                                                                     AFS_HARDLINK_REQUEST_TAG);

        if( pHardLinkCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pHardLinkCB,
                       PAGE_SIZE);

        pHardLinkCB->SourceParentId = ParentObjectInfo->FileId;

        pHardLinkCB->TargetParentId = TargetParentObjectInfo->FileId;

        pHardLinkCB->TargetNameLength = TargetName->Length;

        RtlCopyMemory( pHardLinkCB->TargetName,
                       TargetName->Buffer,
                       TargetName->Length);

        pHardLinkCB->bReplaceIfExists = bReplaceIfExists;

        //
        // Use the same buffer for the result control block
        //

        pResultCB = (AFSFileHardLinkResultCB *)pHardLinkCB;

        ulResultLen = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_HARDLINK_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      AuthGroup,
                                      &SourceDirectoryCB->NameInformation.FileName,
                                      &ObjectInfo->FileId,
                                      pHardLinkCB,
                                      sizeof( AFSFileHardLinkCB) + TargetName->Length,
                                      pResultCB,
                                      &ulResultLen);

        if( ntStatus != STATUS_SUCCESS)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNotifyHardLink failed FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          ObjectInfo->FileId.Cell,
                          ObjectInfo->FileId.Volume,
                          ObjectInfo->FileId.Vnode,
                          ObjectInfo->FileId.Unique,
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // Update the information from the returned data
        //

        if ( ParentObjectInfo != TargetParentObjectInfo)
        {

            AFSAcquireExcl( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                            TRUE);

            bReleaseParentLock = TRUE;

            if ( ParentObjectInfo->DataVersion.QuadPart == pResultCB->SourceParentDataVersion.QuadPart - 1)
            {

                ParentObjectInfo->DataVersion = pResultCB->SourceParentDataVersion;
            }
            else
            {

                SetFlag( ParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

                ParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;
            }
        }

        AFSAcquireExcl( TargetParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        bReleaseTargetParentLock = TRUE;

        if ( TargetParentObjectInfo->DataVersion.QuadPart == pResultCB->TargetParentDataVersion.QuadPart - 1)
        {

            TargetParentObjectInfo->DataVersion = pResultCB->TargetParentDataVersion;
        }
        else
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSNotifyHardLink Raced with an invalidate call and a re-enumeration for entry %wZ ParentFID %08lX-%08lX-%08lX-%08lX Version (%08lX:%08lX != %08lX:%08lX - 1)\n",
                          TargetName,
                          TargetParentObjectInfo->FileId.Cell,
                          TargetParentObjectInfo->FileId.Volume,
                          TargetParentObjectInfo->FileId.Vnode,
                          TargetParentObjectInfo->FileId.Unique,
                          TargetParentObjectInfo->DataVersion.HighPart,
                          TargetParentObjectInfo->DataVersion.LowPart,
                          pResultCB->TargetParentDataVersion.HighPart,
                          pResultCB->TargetParentDataVersion.LowPart);

            //
            // We raced so go and lookup the directory entry in the parent
            //

            ulCRC = AFSGenerateCRC( TargetName,
                                    FALSE);

            AFSLocateCaseSensitiveDirEntry( TargetParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                            ulCRC,
                                            &pDirNode);

            if( pDirNode != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSNotifyHardLink Located dir entry %p for file %wZ\n",
                              pDirNode,
                              TargetName);

                if ( AFSIsEqualFID( &pDirNode->ObjectInformation->FileId,
                                    &pResultCB->DirEnum.FileId))
                {

                    try_return( ntStatus = STATUS_REPARSE);
                }
                else
                {

                    //
                    // We found an entry that matches the desired name but it is not the
                    // same as the one that was created for us by the file server.
                    //

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSNotifyHardLink Found matching name entry %wZ DE %p FID %08lX-%08lX-%08lX-%08lX != FID %08lX-%08lX-%08lX-%08lX\n",
                                  TargetName,
                                  pDirNode,
                                  pDirNode->ObjectInformation->FileId.Cell,
                                  pDirNode->ObjectInformation->FileId.Volume,
                                  pDirNode->ObjectInformation->FileId.Vnode,
                                  pDirNode->ObjectInformation->FileId.Unique,
                                  pResultCB->DirEnum.FileId.Cell,
                                  pResultCB->DirEnum.FileId.Volume,
                                  pResultCB->DirEnum.FileId.Vnode,
                                  pResultCB->DirEnum.FileId.Unique);

                    if( pDirNode->DirOpenReferenceCount <= 0)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSNotifyHardLink Different FIDs - Deleting DE %p for %wZ Old FID %08lX-%08lX-%08lX-%08lX New FID %08lX-%08lX-%08lX-%08lX\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName,
                                      pDirNode->ObjectInformation->FileId.Cell,
                                      pDirNode->ObjectInformation->FileId.Volume,
                                      pDirNode->ObjectInformation->FileId.Vnode,
                                      pDirNode->ObjectInformation->FileId.Unique,
                                      pResultCB->DirEnum.FileId.Cell,
                                      pResultCB->DirEnum.FileId.Volume,
                                      pResultCB->DirEnum.FileId.Vnode,
                                      pResultCB->DirEnum.FileId.Unique);

                        AFSDeleteDirEntry( TargetParentObjectInfo,
                                           pDirNode);
                    }
                    else
                    {

                        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_DELETED);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSNotifyHardLink Different FIDs - Removing DE %p for %wZ Old FID %08lX-%08lX-%08lX-%08lX New FID %08lX-%08lX-%08lX-%08lX\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName,
                                      pDirNode->ObjectInformation->FileId.Cell,
                                      pDirNode->ObjectInformation->FileId.Volume,
                                      pDirNode->ObjectInformation->FileId.Vnode,
                                      pDirNode->ObjectInformation->FileId.Unique,
                                      pResultCB->DirEnum.FileId.Cell,
                                      pResultCB->DirEnum.FileId.Volume,
                                      pResultCB->DirEnum.FileId.Vnode,
                                      pResultCB->DirEnum.FileId.Unique);

                        AFSRemoveNameEntry( TargetParentObjectInfo,
                                            pDirNode);
                    }

                    pDirNode = NULL;
                }
            }

            //
            // We are unsure of our current data so set the verify flag. It may already be set
            // but no big deal to reset it
            //

            SetFlag( TargetParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

            TargetParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;
        }

        //
        // Create the hard link entry
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNotifyHardLink Creating new entry %wZ\n",
                      TargetName);

        //
        // Initialize the directory entry
        //

        pDirNode = AFSInitDirEntry( TargetParentObjectInfo,
                                    TargetName,
                                    NULL,
                                    &pResultCB->DirEnum,
                                    (ULONG)InterlockedIncrement( &TargetParentObjectInfo->Specific.Directory.DirectoryNodeHdr.ContentIndex));

        if( pDirNode == NULL)
        {

            SetFlag( TargetParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

            TargetParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Init the short name if we have one
        //

        if( !BooleanFlagOn( pDevExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
            pResultCB->DirEnum.ShortNameLength > 0)
        {

            UNICODE_STRING uniShortName;

            pDirNode->NameInformation.ShortNameLength = pResultCB->DirEnum.ShortNameLength;

            RtlCopyMemory( pDirNode->NameInformation.ShortName,
                           pResultCB->DirEnum.ShortName,
                           pDirNode->NameInformation.ShortNameLength);

            //
            // Generate the short name index
            //

            uniShortName.Length = pDirNode->NameInformation.ShortNameLength;
            uniShortName.Buffer = pDirNode->NameInformation.ShortName;

            pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = AFSGenerateCRC( &uniShortName,
                                                                               TRUE);

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNotifyHardLink Initialized short name %wZ for DE %p for %wZ\n",
                          &uniShortName,
                          pDirNode,
                          &pDirNode->NameInformation.FileName);
        }
        else
        {
            //
            // No short name or short names are disabled
            //

            pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = 0;
        }

        if ( !BooleanFlagOn( TargetParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY))
        {

            //
            // Update the target parent data version
            //

            TargetParentObjectInfo->DataVersion = pResultCB->TargetParentDataVersion;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNotifyHardLink entry %wZ ParentFID %08lX-%08lX-%08lX-%08lX Version %08lX:%08lX\n",
                          TargetName,
                          TargetParentObjectInfo->FileId.Cell,
                          TargetParentObjectInfo->FileId.Volume,
                          TargetParentObjectInfo->FileId.Vnode,
                          TargetParentObjectInfo->FileId.Unique,
                          TargetParentObjectInfo->DataVersion.QuadPart);
        }

try_exit:

        if ( TargetDirectoryCB != NULL)
        {

            lCount = InterlockedIncrement( &pDirNode->DirOpenReferenceCount);

            AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNotifyHardLink Increment count on %wZ DE %p Cnt %d\n",
                          &pDirNode->NameInformation.FileName,
                          pDirNode,
                          lCount);

            ASSERT( lCount >= 0);

            *TargetDirectoryCB = pDirNode;
        }

        if ( bReleaseTargetParentLock)
        {

            AFSReleaseResource( TargetParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);
        }

        if ( bReleaseParentLock)
        {

            AFSReleaseResource( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);
        }

        if( pHardLinkCB != NULL)
        {

            AFSExFreePoolWithTag( pHardLinkCB, AFS_HARDLINK_REQUEST_TAG);
        }
    }

    return ntStatus;
}



NTSTATUS
AFSNotifyRename( IN AFSObjectInfoCB *ObjectInfo,
                 IN GUID            *AuthGroup,
                 IN AFSObjectInfoCB *ParentObjectInfo,
                 IN AFSObjectInfoCB *TargetParentObjectInfo,
                 IN AFSDirectoryCB *DirectoryCB,
                 IN UNICODE_STRING *TargetName,
                 OUT AFSFileID  *UpdatedFID)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFileRenameCB *pRenameCB = NULL;
    AFSFileRenameResultCB *pRenameResultCB = NULL;
    ULONG ulResultLen = 0;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *) AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        //
        // Init the control block for the request
        //

        pRenameCB = (AFSFileRenameCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                 PAGE_SIZE,
                                                                 AFS_RENAME_REQUEST_TAG);

        if( pRenameCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pRenameCB,
                       PAGE_SIZE);

        pRenameCB->SourceParentId = ParentObjectInfo->FileId;

        pRenameCB->TargetParentId = TargetParentObjectInfo->FileId;

        pRenameCB->TargetNameLength = TargetName->Length;

        RtlCopyMemory( pRenameCB->TargetName,
                       TargetName->Buffer,
                       TargetName->Length);

        //
        // Use the same buffer for the result control block
        //

        pRenameResultCB = (AFSFileRenameResultCB *)pRenameCB;

        ulResultLen = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_RENAME_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      AuthGroup,
                                      &DirectoryCB->NameInformation.FileName,
                                      &ObjectInfo->FileId,
                                      pRenameCB,
                                      sizeof( AFSFileRenameCB) + TargetName->Length,
                                      pRenameResultCB,
                                      &ulResultLen);

        if( ntStatus != STATUS_SUCCESS)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNotifyRename failed FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          ObjectInfo->FileId.Cell,
                          ObjectInfo->FileId.Volume,
                          ObjectInfo->FileId.Vnode,
                          ObjectInfo->FileId.Unique,
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // Update the information from the returned data
        //

        AFSAcquireExcl( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        if ( ParentObjectInfo->DataVersion.QuadPart == pRenameResultCB->SourceParentDataVersion.QuadPart - 1)
        {

            ParentObjectInfo->DataVersion = pRenameResultCB->SourceParentDataVersion;
        }
        else
        {

            SetFlag( ParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

            ParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;
        }

        if ( ParentObjectInfo != TargetParentObjectInfo)
        {

            AFSAcquireExcl( TargetParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                            TRUE);

            if ( TargetParentObjectInfo->DataVersion.QuadPart == pRenameResultCB->TargetParentDataVersion.QuadPart - 1)
            {

                TargetParentObjectInfo->DataVersion = pRenameResultCB->TargetParentDataVersion;
            }
            else
            {

                SetFlag( TargetParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

                TargetParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;
            }
        }

        //
        // Move over the short name
        //

        DirectoryCB->NameInformation.ShortNameLength = pRenameResultCB->DirEnum.ShortNameLength;

        if( !BooleanFlagOn( pDevExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
            DirectoryCB->NameInformation.ShortNameLength > 0)
        {

            UNICODE_STRING uniShortName;

            uniShortName.Length = DirectoryCB->NameInformation.ShortNameLength;
            uniShortName.MaximumLength = uniShortName.Length;
            uniShortName.Buffer = DirectoryCB->NameInformation.ShortName;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNotifyRename Update old short name %wZ for DE %p for %wZ\n",
                          &uniShortName,
                          DirectoryCB,
                          &DirectoryCB->NameInformation.FileName);

            DirectoryCB->NameInformation.ShortNameLength = pRenameResultCB->DirEnum.ShortNameLength;

            RtlCopyMemory( DirectoryCB->NameInformation.ShortName,
                           pRenameResultCB->DirEnum.ShortName,
                           DirectoryCB->NameInformation.ShortNameLength);

            uniShortName.Length = DirectoryCB->NameInformation.ShortNameLength;
            uniShortName.MaximumLength = uniShortName.Length;
            uniShortName.Buffer = DirectoryCB->NameInformation.ShortName;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNotifyRename Initialized short name %wZ for DE %p for %wZ\n",
                          &uniShortName,
                          DirectoryCB,
                          &DirectoryCB->NameInformation.FileName);
        }
        else
        {

            UNICODE_STRING uniShortName;

            uniShortName.Length = DirectoryCB->NameInformation.ShortNameLength;
            uniShortName.MaximumLength = uniShortName.Length;
            uniShortName.Buffer = DirectoryCB->NameInformation.ShortName;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNotifyRename Removing old short name %wZ for DE %p for %wZ\n",
                          &uniShortName,
                          DirectoryCB,
                          &DirectoryCB->NameInformation.FileName);

            DirectoryCB->NameInformation.ShortNameLength = 0;

            DirectoryCB->Type.Data.ShortNameTreeEntry.HashIndex = 0;
        }

        if ( ParentObjectInfo != TargetParentObjectInfo)
        {

            AFSReleaseResource( TargetParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);
        }

        AFSReleaseResource( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);

        if( UpdatedFID != NULL)
        {
            *UpdatedFID = pRenameResultCB->DirEnum.FileId;
        }

try_exit:

        if( pRenameCB != NULL)
        {

            AFSExFreePoolWithTag( pRenameCB, AFS_RENAME_REQUEST_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSEvaluateTargetByID( IN AFSObjectInfoCB *ObjectInfo,
                       IN GUID *AuthGroup,
                       IN BOOLEAN FastCall,
                       OUT AFSDirEnumEntry **DirEnumEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSEvalTargetCB stTargetID;
    ULONG ulResultBufferLength;
    AFSFileEvalResultCB *pEvalResultCB = NULL;
    AFSDirEnumEntry *pDirEnumCB = NULL;
    ULONG ulRequestFlags = AFS_REQUEST_FLAG_SYNCHRONOUS;
    AFSObjectInfoCB *pParentInfo = NULL;

    __Enter
    {

        RtlZeroMemory( &stTargetID,
                       sizeof( AFSEvalTargetCB));

        pParentInfo = ObjectInfo->ParentObjectInformation;

        if( pParentInfo != NULL)
        {

            stTargetID.ParentId = pParentInfo->FileId;
        }

        //
        // Allocate our response buffer
        //

        pEvalResultCB = (AFSFileEvalResultCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                         PAGE_SIZE,
                                                                         AFS_GENERIC_MEMORY_30_TAG);

        if( pEvalResultCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Call to the service to evaluate the fid
        //

        ulResultBufferLength = PAGE_SIZE;

        if( FastCall)
        {

            ulRequestFlags |= AFS_REQUEST_FLAG_FAST_REQUEST;
        }

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_EVAL_TARGET_BY_ID,
                                      ulRequestFlags,
                                      AuthGroup,
                                      NULL,
                                      &ObjectInfo->FileId,
                                      &stTargetID,
                                      sizeof( AFSEvalTargetCB),
                                      pEvalResultCB,
                                      &ulResultBufferLength);

        if( ntStatus != STATUS_SUCCESS)
        {

            //
            // If we received back a STATUS_INVALID_HANDLE then mark the parent as requiring
            // verification
            //

            if( ntStatus == STATUS_OBJECT_PATH_INVALID)
            {

                if( pParentInfo != NULL)
                {

                    AFSAcquireExcl( pParentInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                    TRUE);

                    SetFlag( pParentInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

                    pParentInfo->DataVersion.QuadPart = (ULONGLONG)-1;

                    AFSReleaseResource( pParentInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);
                }
            }

            try_return( ntStatus);
        }

        //
        // Validate the parent data version
        //

        if ( pParentInfo != NULL)
        {

            AFSAcquireExcl( pParentInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                            TRUE);

            if ( pParentInfo->DataVersion.QuadPart != pEvalResultCB->ParentDataVersion.QuadPart)
            {

                SetFlag( pParentInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

                pParentInfo->DataVersion.QuadPart = (ULONGLONG)-1;
            }

            AFSReleaseResource( pParentInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);
        }

        //
        // Pass back the dir enum entry
        //

        if( DirEnumEntry != NULL)
        {

            pDirEnumCB = (AFSDirEnumEntry *)AFSExAllocatePoolWithTag( PagedPool,
                                                                      PAGE_SIZE,
                                                                      AFS_GENERIC_MEMORY_2_TAG);

            if( pDirEnumCB == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlCopyMemory( pDirEnumCB, &pEvalResultCB->DirEnum,
                           ulResultBufferLength - sizeof( AFSFileEvalResultCB) + sizeof( AFSDirEnumEntry));

            *DirEnumEntry = pDirEnumCB;
        }

try_exit:

        if( pEvalResultCB != NULL)
        {

            AFSExFreePoolWithTag( pEvalResultCB, AFS_GENERIC_MEMORY_30_TAG);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            if( pDirEnumCB != NULL)
            {

                AFSExFreePoolWithTag( pDirEnumCB, AFS_GENERIC_MEMORY_2_TAG);
            }

            *DirEnumEntry = NULL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSEvaluateTargetByName( IN GUID *AuthGroup,
                         IN AFSObjectInfoCB *ParentObjectInfo,
                         IN PUNICODE_STRING SourceName,
                         OUT AFSDirEnumEntry **DirEnumEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSEvalTargetCB stTargetID;
    ULONG ulResultBufferLength;
    AFSFileEvalResultCB *pEvalResultCB = NULL;
    AFSDirEnumEntry *pDirEnumCB = NULL;

    __Enter
    {

        stTargetID.ParentId = ParentObjectInfo->FileId;

        //
        // Allocate our response buffer
        //

        pEvalResultCB = (AFSFileEvalResultCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                         PAGE_SIZE,
                                                                         AFS_GENERIC_MEMORY_31_TAG);

        if( pEvalResultCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Call to the service to evaluate the fid
        //

        ulResultBufferLength = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_EVAL_TARGET_BY_NAME,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      AuthGroup,
                                      SourceName,
                                      NULL,
                                      &stTargetID,
                                      sizeof( AFSEvalTargetCB),
                                      pEvalResultCB,
                                      &ulResultBufferLength);

        if( ntStatus != STATUS_SUCCESS)
        {

            if( ntStatus == STATUS_OBJECT_PATH_INVALID)
            {

                AFSAcquireExcl( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                TRUE);

                SetFlag( ParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

                ParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;

                AFSReleaseResource( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);
            }

            try_return( ntStatus);
        }

        //
        // Validate the parent data version
        //

        AFSAcquireExcl( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        if ( ParentObjectInfo->DataVersion.QuadPart != pEvalResultCB->ParentDataVersion.QuadPart)
        {

            SetFlag( ParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

            ParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;
        }

        AFSReleaseResource( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);

        //
        // Pass back the dir enum entry
        //

        if( DirEnumEntry != NULL)
        {

            pDirEnumCB = (AFSDirEnumEntry *)AFSExAllocatePoolWithTag( PagedPool,
                                                                      PAGE_SIZE,
                                                                      AFS_GENERIC_MEMORY_3_TAG);

            if( pDirEnumCB == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlCopyMemory( pDirEnumCB, &pEvalResultCB->DirEnum,
                           ulResultBufferLength - sizeof( AFSFileEvalResultCB) + sizeof( AFSDirEnumEntry));

            *DirEnumEntry = pDirEnumCB;
        }

try_exit:

        if( pEvalResultCB != NULL)
        {

            AFSExFreePoolWithTag( pEvalResultCB, AFS_GENERIC_MEMORY_31_TAG);
        }

        if( !NT_SUCCESS( ntStatus))
        {

            if( pDirEnumCB != NULL)
            {

                AFSExFreePoolWithTag( pDirEnumCB, AFS_GENERIC_MEMORY_3_TAG);
            }

            *DirEnumEntry = NULL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSRetrieveVolumeInformation( IN GUID *AuthGroup,
                              IN AFSFileID *FileID,
                              OUT AFSVolumeInfoCB *VolumeInformation)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulResultLen = 0;

    __Enter
    {

        ulResultLen = sizeof( AFSVolumeInfoCB);

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_GET_VOLUME_INFO,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      AuthGroup,
                                      NULL,
                                      FileID,
                                      NULL,
                                      0,
                                      VolumeInformation,
                                      &ulResultLen);

        if( ntStatus != STATUS_SUCCESS)
        {

            try_return( ntStatus);
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSRetrieveVolumeSizeInformation( IN GUID *AuthGroup,
                                  IN AFSFileID *FileID,
                                  OUT AFSVolumeSizeInfoCB *VolumeSizeInformation)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulResultLen = 0;

    __Enter
    {

        ulResultLen = sizeof( AFSVolumeSizeInfoCB);

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_GET_VOLUME_SIZE_INFO,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      AuthGroup,
                                      NULL,
                                      FileID,
                                      NULL,
                                      0,
                                      VolumeSizeInformation,
                                      &ulResultLen);

        if( ntStatus != STATUS_SUCCESS)
        {

            try_return( ntStatus);
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyPipeTransceive( IN AFSCcb *Ccb,
                         IN ULONG InputLength,
                         IN ULONG OutputLength,
                         IN void *InputDataBuffer,
                         OUT void *OutputDataBuffer,
                         OUT ULONG *BytesReturned)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulResultLen = 0;
    MDL *pInputMdl = NULL, *pOutputMdl = NULL;
    void *pInputSystemBuffer = NULL, *pOutputSystemBuffer = NULL;
    AFSPipeIORequestCB *pIoRequest = NULL;

    __Enter
    {

        //
        // Map the user buffer to a system address
        //

        pInputSystemBuffer = AFSLockUserBuffer( InputDataBuffer,
                                                InputLength,
                                                &pInputMdl);

        if( pInputSystemBuffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        pIoRequest = (AFSPipeIORequestCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                     sizeof( AFSPipeIORequestCB) +
                                                                                InputLength,
                                                                     AFS_GENERIC_MEMORY_4_TAG);

        if( pIoRequest == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pIoRequest,
                       sizeof( AFSPipeIORequestCB) + InputLength);

        pIoRequest->RequestId = Ccb->RequestID;

        pIoRequest->RootId = Ccb->DirectoryCB->ObjectInformation->VolumeCB->ObjectInformation.FileId;

        pIoRequest->BufferLength = InputLength;

        RtlCopyMemory( (void *)((char *)pIoRequest + sizeof( AFSPipeIORequestCB)),
                       pInputSystemBuffer,
                       InputLength);

        pOutputSystemBuffer = AFSLockUserBuffer( OutputDataBuffer,
                                                 OutputLength,
                                                 &pOutputMdl);

        if( pOutputSystemBuffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Send the call to the service
        //

        ulResultLen = OutputLength;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_PIPE_TRANSCEIVE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      &Ccb->AuthGroup,
                                      &Ccb->DirectoryCB->NameInformation.FileName,
                                      NULL,
                                      pIoRequest,
                                      sizeof( AFSPipeIORequestCB) + InputLength,
                                      pOutputSystemBuffer,
                                      &ulResultLen);

        if( ntStatus != STATUS_SUCCESS &&
            ntStatus != STATUS_BUFFER_OVERFLOW)
        {

            if( NT_SUCCESS( ntStatus))
            {

                ntStatus = STATUS_DEVICE_NOT_READY;
            }

            try_return( ntStatus);
        }

        //
        // Return the bytes processed
        //

        *BytesReturned = ulResultLen;

try_exit:

        if( pInputMdl != NULL)
        {

            MmUnlockPages( pInputMdl);

            IoFreeMdl( pInputMdl);
        }

        if( pOutputMdl != NULL)
        {

            MmUnlockPages( pOutputMdl);

            IoFreeMdl( pOutputMdl);
        }

        if( pIoRequest != NULL)
        {

            AFSExFreePoolWithTag( pIoRequest, AFS_GENERIC_MEMORY_4_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSNotifySetPipeInfo( IN AFSCcb *Ccb,
                      IN ULONG InformationClass,
                      IN ULONG InputLength,
                      IN void *DataBuffer)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSPipeInfoRequestCB *pInfoRequest = NULL;

    __Enter
    {

        pInfoRequest = (AFSPipeInfoRequestCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                         sizeof( AFSPipeInfoRequestCB) +
                                                                                InputLength,
                                                                         AFS_GENERIC_MEMORY_5_TAG);

        if( pInfoRequest == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pInfoRequest,
                       sizeof( AFSPipeInfoRequestCB) + InputLength);

        pInfoRequest->RequestId = Ccb->RequestID;

        pInfoRequest->RootId = Ccb->DirectoryCB->ObjectInformation->VolumeCB->ObjectInformation.FileId;

        pInfoRequest->BufferLength = InputLength;

        pInfoRequest->InformationClass = InformationClass;

        RtlCopyMemory( (void *)((char *)pInfoRequest + sizeof( AFSPipeInfoRequestCB)),
                       DataBuffer,
                       InputLength);

        //
        // Send the call to the service
        //

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_PIPE_SET_INFO,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      &Ccb->AuthGroup,
                                      &Ccb->DirectoryCB->NameInformation.FileName,
                                      NULL,
                                      pInfoRequest,
                                      sizeof( AFSPipeInfoRequestCB) + InputLength,
                                      NULL,
                                      NULL);

        if( ntStatus != STATUS_SUCCESS)
        {

            if( NT_SUCCESS( ntStatus))
            {

                ntStatus = STATUS_DEVICE_NOT_READY;
            }

            try_return( ntStatus);
        }

try_exit:

        if( pInfoRequest != NULL)
        {

            AFSExFreePoolWithTag( pInfoRequest, AFS_GENERIC_MEMORY_5_TAG);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyQueryPipeInfo( IN AFSCcb *Ccb,
                        IN ULONG InformationClass,
                        IN ULONG OutputLength,
                        IN void *DataBuffer,
                        OUT ULONG *BytesReturned)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSPipeInfoRequestCB stInfoRequest;
    ULONG ulBytesProcessed = 0;

    __Enter
    {

        RtlZeroMemory( &stInfoRequest,
                       sizeof( AFSPipeInfoRequestCB));

        stInfoRequest.RequestId = Ccb->RequestID;

        stInfoRequest.RootId = Ccb->DirectoryCB->ObjectInformation->VolumeCB->ObjectInformation.FileId;

        stInfoRequest.BufferLength = OutputLength;

        stInfoRequest.InformationClass = InformationClass;

        ulBytesProcessed = OutputLength;

        //
        // Send the call to the service
        //

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_PIPE_QUERY_INFO,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      &Ccb->AuthGroup,
                                      &Ccb->DirectoryCB->NameInformation.FileName,
                                      NULL,
                                      &stInfoRequest,
                                      sizeof( AFSPipeInfoRequestCB),
                                      DataBuffer,
                                      &ulBytesProcessed);

        if( ntStatus != STATUS_SUCCESS)
        {

            if( NT_SUCCESS( ntStatus))
            {

                ntStatus = STATUS_DEVICE_NOT_READY;
            }

            try_return( ntStatus);
        }

        *BytesReturned = ulBytesProcessed;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSReleaseFid( IN AFSFileID *FileId)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_RELEASE_FID,
                                      0,
                                      NULL,
                                      NULL,
                                      FileId,
                                      NULL,
                                      0,
                                      NULL,
                                      NULL);
    }

    return ntStatus;
}

BOOLEAN
AFSIsExtentRequestQueued( IN AFSFileID *FileID,
                          IN LARGE_INTEGER *ExtentOffset,
                          IN ULONG Length)
{

    BOOLEAN bRequestQueued = FALSE;
    AFSDeviceExt    *pControlDevExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;
    AFSCommSrvcCB   *pCommSrvc = NULL;
    AFSPoolEntry    *pPoolEntry = NULL;
    AFSRequestExtentsCB *pRequestExtents = NULL;

    __Enter
    {


        pCommSrvc = &pControlDevExt->Specific.Control.CommServiceCB;

        AFSAcquireShared( &pCommSrvc->IrpPoolLock,
                          TRUE);

        pPoolEntry = pCommSrvc->RequestPoolHead;

        while( pPoolEntry != NULL)
        {

            if( pPoolEntry->RequestType == AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS)
            {

                if( AFSIsEqualFID( &pPoolEntry->FileId, FileID))
                {

                    pRequestExtents = (AFSRequestExtentsCB *)pPoolEntry->Data;

                    if( pRequestExtents->ByteOffset.QuadPart == ExtentOffset->QuadPart &&
                        pRequestExtents->Length == Length)
                    {

                        bRequestQueued = TRUE;
                    }
                }
            }

            pPoolEntry = pPoolEntry->fLink;
        }

        AFSReleaseResource( &pCommSrvc->IrpPoolLock);
    }

    return bRequestQueued;
}
