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

    __Enter
    {

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

            if( ntStatus != STATUS_SUCCESS ||
                ulResultLen == 0)
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
                                  "AFSEnumerateDirectory Failed to enumerate directory Status %08lX\n",
                                  ntStatus);
                }

                break;
            }

            pDirEnumResponse = (AFSDirEnumResp *)pBuffer;

            pCurrentDirEntry = (AFSDirEnumEntry *)pDirEnumResponse->Entry;

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

                    continue;
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

                //
                // Set up the entry length
                //

                ulEntryLength = QuadAlign( sizeof( AFSDirEnumEntry) +
                                                    pCurrentDirEntry->FileNameLength +
                                                    pCurrentDirEntry->TargetNameLength);

                //
                // Init the short name if we have one
                //

                if( pCurrentDirEntry->ShortNameLength > 0)
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
                    uniShortName.Buffer = pDirNode->NameInformation.ShortName;

                    pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = AFSGenerateCRC( &uniShortName,
                                                                                       TRUE);
                }

                //
                // Insert the node into the name tree
                //

                ASSERT( ExIsResourceAcquiredExclusiveLite( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock));

                if( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead == NULL)
                {

                    ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead = pDirNode;
                }
                else
                {

                    AFSInsertCaseSensitiveDirEntry( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                    pDirNode);
                }

                if( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead == NULL)
                {

                    ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead = pDirNode;

                    SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);
                }
                else
                {

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

                        ObjectInfoCB->Specific.Directory.ShortNameTree = pDirNode;
                    }
                    else
                    {

                        AFSInsertShortNameDirEntry( ObjectInfoCB->Specific.Directory.ShortNameTree,
                                                    pDirNode);
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

            //
            // If the enumeration handle is -1 then we are done
            //

            if( ((ULONG)-1) == pDirQueryCB->EnumHandle )
            {

                break;
            }
        }

try_exit:

        //
        // Cleanup
        //

        if( pBuffer != NULL)
        {

            AFSExFreePool( pBuffer);
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

    __Enter
    {

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

            if( ntStatus != STATUS_SUCCESS ||
                ulResultLen == 0)
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
                                  "AFSVerifyDirectoryContent Failed to enumerate directory Status %08lX\n",
                                  ntStatus);
                }

                break;
            }

            pDirEnumResponse = (AFSDirEnumResp *)pBuffer;

            pCurrentDirEntry = (AFSDirEnumEntry *)pDirEnumResponse->Entry;

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

                ASSERT( ExIsResourceAcquiredExclusiveLite( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock));

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

                if( pDirNode != NULL)
                {

                    //
                    // Check that the FIDs are the same
                    //

                    if( AFSIsEqualFID( &pCurrentDirEntry->FileId,
                                       &pDirNode->ObjectInformation->FileId))
                    {

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

                            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSVerifyDirectoryContent Verified entry %wZ for parent FID %08lX-%08lX-%08lX-%08lX\n",
                                          &uniDirName,
                                          ObjectInfoCB->FileId.Cell,
                                          ObjectInfoCB->FileId.Volume,
                                          ObjectInfoCB->FileId.Vnode,
                                          ObjectInfoCB->FileId.Unique);


                            //
                            // Update the metadata for the entry
                            //

                            if( pObjectInfo->DataVersion.QuadPart == 0 ||
                                pObjectInfo->DataVersion.QuadPart != pCurrentDirEntry->DataVersion.QuadPart)
                            {

                                AFSUpdateMetaData( pDirNode,
                                                   pCurrentDirEntry);

                                if( pObjectInfo->FileType == AFS_FILE_TYPE_DIRECTORY)
                                {

                                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                                  AFS_TRACE_LEVEL_VERBOSE,
                                                  "AFSVerifyDirectoryContent Setting VERIFY on entry %wZ for FID %08lX-%08lX-%08lX-%08lX\n",
                                                  &uniDirName,
                                                  ObjectInfoCB->FileId.Cell,
                                                  ObjectInfoCB->FileId.Volume,
                                                  ObjectInfoCB->FileId.Vnode,
                                                  ObjectInfoCB->FileId.Unique);

                                    SetFlag( pObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);
                                    pObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;
                                    pObjectInfo->Expiration.QuadPart = 0;
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

                            continue;
                        }
                    }

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSVerifyDirectoryContent Processing dir entry %wZ with different FID, same name in parent FID %08lX-%08lX-%08lX-%08lX\n",
                                  &pDirNode->NameInformation.FileName,
                                  ObjectInfoCB->FileId.Cell,
                                  ObjectInfoCB->FileId.Volume,
                                  ObjectInfoCB->FileId.Vnode,
                                  ObjectInfoCB->FileId.Unique);

                    //
                    // Need to tear down this entry and rebuild it below
                    //

                    if( pDirNode->OpenReferenceCount == 0)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSVerifyDirectoryContent Deleting dir entry %wZ from parent FID %08lX-%08lX-%08lX-%08lX\n",
                                      &pDirNode->NameInformation.FileName,
                                      ObjectInfoCB->FileId.Cell,
                                      ObjectInfoCB->FileId.Volume,
                                      ObjectInfoCB->FileId.Vnode,
                                      ObjectInfoCB->FileId.Unique);

                        AFSDeleteDirEntry( ObjectInfoCB,
                                           pDirNode);
                    }
                    else
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSVerifyDirectoryContent Setting dir entry %p name %wZ DELETED in parent FID %08lX-%08lX-%08lX-%08lX\n",
                                      pDirNode,
                                      &pDirNode->NameInformation.FileName,
                                      ObjectInfoCB->FileId.Cell,
                                      ObjectInfoCB->FileId.Volume,
                                      ObjectInfoCB->FileId.Vnode,
                                      ObjectInfoCB->FileId.Unique);

                        SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_DELETED);

                        AFSRemoveNameEntry( ObjectInfoCB,
                                            pDirNode);
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

                //
                // Init the short name if we have one
                //

                if( pCurrentDirEntry->ShortNameLength > 0)
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
                    uniShortName.Buffer = pDirNode->NameInformation.ShortName;

                    pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = AFSGenerateCRC( &uniShortName,
                                                                                       TRUE);
                }

                //
                // Insert the node into the name tree
                //

                ASSERT( ExIsResourceAcquiredExclusiveLite( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.TreeLock));

                if( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead == NULL)
                {

                    ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead = pDirNode;
                }
                else
                {

                    AFSInsertCaseSensitiveDirEntry( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                    pDirNode);
                }

                if( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead == NULL)
                {

                    ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead = pDirNode;

                    SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);
                }
                else
                {

                    AFSInsertCaseInsensitiveDirEntry( ObjectInfoCB->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                                      pDirNode);
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
                    }
                    else
                    {

                        AFSInsertShortNameDirEntry( ObjectInfoCB->Specific.Directory.ShortNameTree,
                                                    pDirNode);
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

            //
            // If the enumeration handle is -1 then we are done
            //

            if( ((ULONG)-1) == pDirQueryCB->EnumHandle )
            {

                break;
            }
        }

try_exit:

        //
        // Cleanup
        //

        if( pBuffer != NULL)
        {

            AFSExFreePool( pBuffer);
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
    LARGE_INTEGER liOldDataVersion;

    __Enter
    {

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

        if( liOldDataVersion.QuadPart != pResultCB->ParentDataVersion.QuadPart - 1 ||
            liOldDataVersion.QuadPart != ParentObjectInfo->DataVersion.QuadPart)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSNotifyFileCreate Raced with an invalidate call and a re-enumeration for entry %wZ\n",
                          FileName);

            //
            // We raced so go and lookup the directory entry in the parent
            //

            ulCRC = AFSGenerateCRC( FileName,
                                    FALSE);

            AFSAcquireShared( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                              TRUE);

            AFSLocateCaseSensitiveDirEntry( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                            ulCRC,
                                            &pDirNode);

            AFSReleaseResource( ParentObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);

            if( pDirNode != NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSNotifyFileCreate Located dir entry for file %wZ\n",
                              FileName);

                *DirNode = pDirNode;

                try_return( ntStatus = STATUS_REPARSE);
            }

            //
            // We are unsure of our current data so set the verify flag. It may already be set
            // but no big deal to reset it
            //

            SetFlag( ParentObjectInfo->Flags, AFS_OBJECT_FLAGS_VERIFY);

            ParentObjectInfo->DataVersion.QuadPart = (ULONGLONG)-1;
        }
        else
        {

            //
            // Update the parent data version
            //

            ParentObjectInfo->DataVersion = pResultCB->ParentDataVersion;
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

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Init the short name if we have one
        //

        if( pResultCB->DirEnum.ShortNameLength > 0)
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
        }

        //
        // Return the directory node
        //

        *DirNode = pDirNode;

try_exit:

        if( pResultCB != NULL)
        {

            AFSExFreePool( pResultCB);
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
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
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

        ObjectInfo->DataVersion = pUpdateResultCB->DirEnum.DataVersion;

try_exit:

        if( pUpdateResultCB != NULL)
        {

            AFSExFreePool( pUpdateResultCB);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyDelete( IN AFSDirectoryCB *DirectoryCB,
                 IN BOOLEAN CheckOnly)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulResultLen = 0;
    AFSFileDeleteCB stDelete;
    AFSFileDeleteResultCB stDeleteResult;
    ULONG ulRequestFlags = AFS_REQUEST_FLAG_SYNCHRONOUS;
    GUID *pAuthGroup = NULL;

    __Enter
    {

        stDelete.ParentId = DirectoryCB->ObjectInformation->ParentObjectInformation->FileId;

        stDelete.ProcessId = (ULONGLONG)PsGetCurrentProcessId();

        ulResultLen = sizeof( AFSFileDeleteResultCB);

        if( CheckOnly)
        {
            ulRequestFlags |= AFS_REQUEST_FLAG_CHECK_ONLY;
        }

        if( DirectoryCB->ObjectInformation->Fcb != NULL)
        {
            pAuthGroup = &DirectoryCB->ObjectInformation->Fcb->AuthGroup;
        }

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_DELETE_FILE,
                                      ulRequestFlags,
                                      pAuthGroup,
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
                          "AFSNotifyDelete failed FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                          &DirectoryCB->ObjectInformation->FileId.Cell,
                          &DirectoryCB->ObjectInformation->FileId.Volume,
                          &DirectoryCB->ObjectInformation->FileId.Vnode,
                          &DirectoryCB->ObjectInformation->FileId.Unique,
                          ntStatus);

            try_return( ntStatus);
        }

        if( !CheckOnly)
        {

            //
            // Update the parent data version
            //

            if( DirectoryCB->ObjectInformation->ParentObjectInformation->DataVersion.QuadPart != stDeleteResult.ParentDataVersion.QuadPart)
            {

                DirectoryCB->ObjectInformation->ParentObjectInformation->Flags |= AFS_OBJECT_FLAGS_VERIFY;

                DirectoryCB->ObjectInformation->ParentObjectInformation->DataVersion.QuadPart = (ULONGLONG)-1;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyRename( IN AFSObjectInfoCB *ObjectInfo,
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
    GUID *pAuthGroup = NULL;

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

        if( ObjectInfo->Fcb != NULL)
        {
            pAuthGroup = &ObjectInfo->Fcb->AuthGroup;
        }

        //
        // Use the same buffer for the result control block
        //

        pRenameResultCB = (AFSFileRenameResultCB *)pRenameCB;

        ulResultLen = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_RENAME_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      pAuthGroup,
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

        ParentObjectInfo->DataVersion = pRenameResultCB->SourceParentDataVersion;

        TargetParentObjectInfo->DataVersion = pRenameResultCB->TargetParentDataVersion;

        //
        // Move over the short name
        //

        DirectoryCB->NameInformation.ShortNameLength = pRenameResultCB->DirEnum.ShortNameLength;

        if( DirectoryCB->NameInformation.ShortNameLength > 0)
        {

            RtlCopyMemory( DirectoryCB->NameInformation.ShortName,
                           pRenameResultCB->DirEnum.ShortName,
                           DirectoryCB->NameInformation.ShortNameLength);
        }

        if( UpdatedFID != NULL)
        {

            *UpdatedFID = pRenameResultCB->DirEnum.FileId;
        }

try_exit:

        if( pRenameCB != NULL)
        {

            AFSExFreePool( pRenameCB);
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
    AFSDirEnumEntry *pDirEnumCB = NULL;
    ULONG ulRequestFlags = AFS_REQUEST_FLAG_SYNCHRONOUS;

    __Enter
    {

        RtlZeroMemory( &stTargetID,
                       sizeof( AFSEvalTargetCB));

        if( ObjectInfo->ParentObjectInformation != NULL)
        {

            stTargetID.ParentId = ObjectInfo->ParentObjectInformation->FileId;
        }

        //
        // Allocate our response buffer
        //

        pDirEnumCB = (AFSDirEnumEntry *)AFSExAllocatePoolWithTag( PagedPool,
                                                                  PAGE_SIZE,
                                                                  AFS_GENERIC_MEMORY_2_TAG);

        if( pDirEnumCB == NULL)
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
                                      pDirEnumCB,
                                      &ulResultBufferLength);

        if( ntStatus != STATUS_SUCCESS)
        {

            //
            // If we received back a STATUS_INVALID_HANDLE then mark the parent as requiring
            // verification
            //

            if( ntStatus == STATUS_INVALID_HANDLE)
            {

                if( ObjectInfo->ParentObjectInformation != NULL)
                {

                    SetFlag( ObjectInfo->ParentObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY);
                }
            }

            try_return( ntStatus);
        }

        //
        // Pass back the dir enum entry
        //

        if( DirEnumEntry != NULL)
        {

            *DirEnumEntry = pDirEnumCB;
        }
        else
        {

            AFSExFreePool( pDirEnumCB);
        }

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pDirEnumCB != NULL)
            {

                AFSExFreePool( pDirEnumCB);
            }

            *DirEnumEntry = NULL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSEvaluateTargetByName( IN GUID *AuthGroup,
                         IN AFSFileID *ParentFileId,
                         IN PUNICODE_STRING SourceName,
                         OUT AFSDirEnumEntry **DirEnumEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSEvalTargetCB stTargetID;
    ULONG ulResultBufferLength;
    AFSDirEnumEntry *pDirEnumCB = NULL;

    __Enter
    {

        stTargetID.ParentId = *ParentFileId;

        //
        // Allocate our response buffer
        //

        pDirEnumCB = (AFSDirEnumEntry *)AFSExAllocatePoolWithTag( PagedPool,
                                                                  PAGE_SIZE,
                                                                  AFS_GENERIC_MEMORY_3_TAG);

        if( pDirEnumCB == NULL)
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
                                      pDirEnumCB,
                                      &ulResultBufferLength);

        if( ntStatus != STATUS_SUCCESS)
        {

            try_return( ntStatus);
        }

        //
        // Pass back the dir enum entry
        //

        *DirEnumEntry = pDirEnumCB;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pDirEnumCB != NULL)
            {

                AFSExFreePool( pDirEnumCB);
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
    ULONG ulBufferLength = OutputLength;
    AFSPipeIORequestCB *pIoRequest = NULL;
    GUID *pAuthGroup = NULL;

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

        if( Ccb->DirectoryCB->ObjectInformation->Fcb != NULL)
        {
            pAuthGroup = &Ccb->DirectoryCB->ObjectInformation->Fcb->AuthGroup;
        }

        //
        // Send the call to the service
        //

        ulResultLen = OutputLength;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_PIPE_TRANSCEIVE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      pAuthGroup,
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

            AFSExFreePool( pIoRequest);
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
    GUID *pAuthGroup = NULL;

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

        if( Ccb->DirectoryCB->ObjectInformation->Fcb != NULL)
        {
            pAuthGroup = &Ccb->DirectoryCB->ObjectInformation->Fcb->AuthGroup;
        }

        //
        // Send the call to the service
        //

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_PIPE_SET_INFO,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      pAuthGroup,
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

            AFSExFreePool( pInfoRequest);
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
    GUID *pAuthGroup = NULL;

    __Enter
    {

        RtlZeroMemory( &stInfoRequest,
                       sizeof( AFSPipeInfoRequestCB));

        stInfoRequest.RequestId = Ccb->RequestID;

        stInfoRequest.RootId = Ccb->DirectoryCB->ObjectInformation->VolumeCB->ObjectInformation.FileId;

        stInfoRequest.BufferLength = OutputLength;

        stInfoRequest.InformationClass = InformationClass;

        ulBytesProcessed = OutputLength;

        if( Ccb->DirectoryCB->ObjectInformation->Fcb != NULL)
        {
            pAuthGroup = &Ccb->DirectoryCB->ObjectInformation->Fcb->AuthGroup;
        }

        //
        // Send the call to the service
        //

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_PIPE_QUERY_INFO,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      pAuthGroup,
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
    NTSTATUS ntStatus = STATUS_SUCCESS;
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
