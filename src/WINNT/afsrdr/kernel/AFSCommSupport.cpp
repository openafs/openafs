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
// File: AFSCommSupport.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSEnumerateDirectory( IN AFSFcb *Dcb,
                       IN OUT AFSDirHdr      *DirectoryHdr,
                       IN OUT AFSDirEntryCB **DirListHead,
                       IN OUT AFSDirEntryCB **DirListTail,
                       IN OUT AFSDirEntryCB **ShortNameTree,
                       IN BOOLEAN             FastQuery)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    void *pBuffer = NULL;
    ULONG ulResultLen = 0;
    AFSDirQueryCB *pDirQueryCB;
    AFSDirEnumEntry *pCurrentDirEntry = NULL;
    AFSDirEntryCB *pDirNode = NULL;
    ULONG  ulEntryLength = 0;
    AFSDirEnumResp *pDirEnumResponse = NULL;
    UNICODE_STRING uniDirName, uniTargetName;
    ULONG   ulRequestFlags = AFS_REQUEST_FLAG_SYNCHRONOUS;

    __Enter
    {
                                       
        //
        // Initialize the directory enumeration buffer for the directory
        //

        pBuffer = ExAllocatePoolWithTag( PagedPool,
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
                                          0,
                                          NULL,
                                          &Dcb->DirEntry->DirectoryEntry.FileId,
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
                                  "AFSEnumerateDirectory Failed to enumerate directory %wZ Status %08lX\n",
                                                                            &Dcb->DirEntry->DirectoryEntry.FileName,
                                                                            ntStatus);

                    ntStatus = STATUS_ACCESS_DENIED;
                }

                break;
            }

            //
            // If the node has become invalid during the request handling then bail now
            //

            if( BooleanFlagOn( Dcb->Flags, AFS_FCB_INVALID))
            {

                ntStatus = STATUS_ACCESS_DENIED;

                break;
            }

            pDirEnumResponse = (AFSDirEnumResp *)pBuffer;

            pCurrentDirEntry = (AFSDirEnumEntry *)pDirEnumResponse->Entry;

            //
            // Remvoe the leading header from the processed length
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

                pDirNode = AFSInitDirEntry( &Dcb->DirEntry->DirectoryEntry.FileId,
                                            &uniDirName,
                                            &uniTargetName,
                                            pCurrentDirEntry,
                                            (ULONG)InterlockedIncrement( &DirectoryHdr->ContentIndex));

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

                    pDirNode->DirectoryEntry.ShortNameLength = pCurrentDirEntry->ShortNameLength;

                    RtlCopyMemory( pDirNode->DirectoryEntry.ShortName,
                                   pCurrentDirEntry->ShortName,
                                   pDirNode->DirectoryEntry.ShortNameLength);

                    //
                    // Only perform the short work if given a short name tree
                    //

                    if( ShortNameTree != NULL)
                    {

                        //
                        // Generate the short name index
                        //

                        uniShortName.Length = pDirNode->DirectoryEntry.ShortNameLength;
                        uniShortName.Buffer = pDirNode->DirectoryEntry.ShortName;

                        pDirNode->Type.Data.ShortNameTreeEntry.HashIndex = AFSGenerateCRC( &uniShortName,
                                                                                           TRUE);
                    }
                }

                //
                // Insert the node into the name tree
                //

                if( DirectoryHdr->CaseSensitiveTreeHead == NULL)
                {

                    DirectoryHdr->CaseSensitiveTreeHead = pDirNode;
                }
                else
                {

                    AFSInsertCaseSensitiveDirEntry( DirectoryHdr->CaseSensitiveTreeHead,
                                                    pDirNode);
                }      

                if( DirectoryHdr->CaseInsensitiveTreeHead == NULL)
                {

                    DirectoryHdr->CaseInsensitiveTreeHead = pDirNode;

                    SetFlag( pDirNode->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);
                }
                else
                {

                    AFSInsertCaseInsensitiveDirEntry( DirectoryHdr->CaseInsensitiveTreeHead,
                                                      pDirNode);
                }              

                //
                // If we are given a list to inser the item into, do it here
                //

                if( DirListHead != NULL)
                {

                    if( *DirListHead == NULL)
                    {

                        *DirListHead = pDirNode;
                    }
                    else
                    {

                        (*DirListTail)->ListEntry.fLink = pDirNode;

                        pDirNode->ListEntry.bLink = *DirListTail;
                    }

                    *DirListTail = pDirNode;
                }

                if( ShortNameTree != NULL &&
                    pDirNode->Type.Data.ShortNameTreeEntry.HashIndex != 0)
                {

                    //
                    // Insert the short name entry if we have a valid short name
                    //

                    if( *ShortNameTree == NULL)
                    {

                        *ShortNameTree = pDirNode;
                    }
                    else
                    {

                        AFSInsertShortNameDirEntry( *ShortNameTree,
                                                    pDirNode);
                    }
                }

                //
                // Next dir entry
                //

                pCurrentDirEntry = (AFSDirEnumEntry *)((char *)pCurrentDirEntry + ulEntryLength);

                ASSERT( ulResultLen >= ulEntryLength);

                ulResultLen -= ulEntryLength;
            }

            ulResultLen = AFS_DIR_ENUM_BUFFER_LEN;

            //
            // Reset the information in the request buffer since it got trampled
            // above
            //

            pDirQueryCB->EnumHandle = pDirEnumResponse->EnumHandle;

            //
            // Something deeply wrong with enumeration.  Break now
            //
            //            break;

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
        // If we are successful then clear the enumeration event for the directory
        //

        if( NT_SUCCESS( ntStatus))
        {

            if( Dcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
            {

                KeClearEvent( &Dcb->Specific.Directory.EnumerationEvent);
            }
            else
            {

                KeClearEvent( &Dcb->Specific.VolumeRoot.EnumerationEvent);
            }
        }

        //
        // Cleanup
        //

        if( pBuffer != NULL)
        {

            ExFreePool( pBuffer);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSEnumerateDirectoryNoResponse( IN AFSFcb *Dcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirQueryCB stDirQueryCB;
    ULONG   ulRequestFlags = AFS_REQUEST_FLAG_SYNCHRONOUS;

    __Enter
    {
                                       
        //
        // Use the payload buffer for information we will pass to the service
        //

        stDirQueryCB.EnumHandle = 0;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_DIR_ENUM,
                                      ulRequestFlags,
                                      0,
                                      NULL,
                                      &Dcb->DirEntry->DirectoryEntry.FileId,
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
                              "AFSEnumerateDirectoryNoResponse Failed to enumerate directory %wZ Status %08lX\n",
                                                                            &Dcb->DirEntry->DirectoryEntry.FileName,
                                                                            ntStatus);

                ntStatus = STATUS_ACCESS_DENIED;
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyFileCreate( IN AFSFcb *ParentDcb,
                     IN PLARGE_INTEGER FileSize,
                     IN ULONG FileAttributes,
                     IN UNICODE_STRING *FileName,
                     OUT AFSDirEntryCB **DirNode)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFileCreateCB stCreateCB;
    AFSFileCreateResultCB *pResultCB = NULL;
    ULONG ulResultLen = 0;
    UNICODE_STRING uniTargetName;
    AFSDirEntryCB *pDirNode = NULL;

    __Enter
    {

        //
        // Init the control block for the request
        //

        RtlZeroMemory( &stCreateCB,
                       sizeof( AFSFileCreateCB));

        ASSERT( ParentDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY);

        stCreateCB.ParentId = ParentDcb->DirEntry->DirectoryEntry.FileId;

        stCreateCB.AllocationSize = *FileSize;

        stCreateCB.FileAttributes = FileAttributes;

        stCreateCB.EaSize = 0;

        //
        // Allocate our return buffer
        //

        pResultCB = (AFSFileCreateResultCB *)ExAllocatePoolWithTag( PagedPool,
                                                                    PAGE_SIZE,
                                                                    AFS_GENERIC_MEMORY_TAG);

        if( pResultCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pResultCB,
                       PAGE_SIZE);

        ulResultLen = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_CREATE_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
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
        // Initialize the directory entry
        //

        ASSERT( (USHORT)pResultCB->DirEnum.FileNameLength == FileName->Length);

        uniTargetName.Length = (USHORT)pResultCB->DirEnum.TargetNameLength;

        uniTargetName.MaximumLength = uniTargetName.Length;

        uniTargetName.Buffer = (WCHAR *)((char *)&pResultCB->DirEnum + pResultCB->DirEnum.TargetNameOffset);

        pDirNode = AFSInitDirEntry( &(ParentDcb->DirEntry->DirectoryEntry.FileId),
                                    FileName,
                                    &uniTargetName,
                                    &pResultCB->DirEnum,
                                    (ULONG)InterlockedIncrement( &(ParentDcb->Specific.Directory.DirectoryNodeHdr.ContentIndex)));

        if( pDirNode == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Update the parent data version
        //

        ParentDcb->DirEntry->DirectoryEntry.DataVersion = pResultCB->ParentDataVersion;

        //
        // Init the short name if we have one
        //

        if( pResultCB->DirEnum.ShortNameLength > 0)
        {

            UNICODE_STRING uniShortName;

            pDirNode->DirectoryEntry.ShortNameLength = pResultCB->DirEnum.ShortNameLength;

            RtlCopyMemory( pDirNode->DirectoryEntry.ShortName,
                           pResultCB->DirEnum.ShortName,
                           pDirNode->DirectoryEntry.ShortNameLength);

            //
            // Generate the short name index
            //

            uniShortName.Length = pDirNode->DirectoryEntry.ShortNameLength;
            uniShortName.Buffer = pDirNode->DirectoryEntry.ShortName;

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

            ExFreePool( pResultCB);
        }

        if( !NT_SUCCESS( ntStatus))
        {

        }
    }

    return ntStatus;
}

NTSTATUS
AFSUpdateFileInformation( IN PDEVICE_OBJECT DeviceObject,
                          IN AFSFcb *Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
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

        stUpdateCB.AllocationSize = Fcb->DirEntry->DirectoryEntry.EndOfFile;

        stUpdateCB.FileAttributes = Fcb->DirEntry->DirectoryEntry.FileAttributes;

        stUpdateCB.EaSize = Fcb->DirEntry->DirectoryEntry.EaSize;

        ASSERT( Fcb->ParentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY);

        stUpdateCB.ParentId = Fcb->ParentFcb->DirEntry->DirectoryEntry.FileId;

        pUpdateResultCB = (AFSFileUpdateResultCB *)ExAllocatePoolWithTag( PagedPool,
                                                                          PAGE_SIZE,
                                                                          AFS_UPDATE_RESULT_TAG);

        if( pUpdateResultCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        ulResultLen = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_UPDATE_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      &Fcb->DirEntry->DirectoryEntry.FileName,
                                      &Fcb->DirEntry->DirectoryEntry.FileId,
                                      &stUpdateCB,
                                      sizeof( AFSFileUpdateCB),
                                      pUpdateResultCB,
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
        // Update the data version
        //

        Fcb->DirEntry->DirectoryEntry.DataVersion = pUpdateResultCB->DirEnum.DataVersion;

try_exit:

        if( pUpdateResultCB != NULL)
        {

            ExFreePool( pUpdateResultCB);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyDelete( IN AFSFcb *Fcb)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulResultLen = 0;
    AFSFileDeleteCB stDelete;
    AFSFileDeleteResultCB stDeleteResult;

    __Enter
    {

        ASSERT( Fcb->ParentFcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY);

        stDelete.ParentId = Fcb->ParentFcb->DirEntry->DirectoryEntry.FileId;

        ulResultLen = sizeof( AFSFileDeleteResultCB);

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_DELETE_FILE,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      &Fcb->DirEntry->DirectoryEntry.FileName,
                                      &Fcb->DirEntry->DirectoryEntry.FileId,
                                      &stDelete,
                                      sizeof( AFSFileDeleteCB),
                                      &stDeleteResult,
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
        // Update the parent data version
        //

        Fcb->ParentFcb->DirEntry->DirectoryEntry.DataVersion = stDeleteResult.ParentDataVersion;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSNotifyRename( IN AFSFcb *Fcb,
                 IN AFSFcb *ParentDcb,
                 IN AFSFcb *TargetDcb,
                 IN UNICODE_STRING *TargetName)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFileRenameCB *pRenameCB = NULL;
    AFSFileRenameResultCB *pRenameResultCB = NULL;
    ULONG ulResultLen = 0;

    __Enter
    {

        //
        // Init the control block for the request
        //

        pRenameCB = (AFSFileRenameCB *)ExAllocatePoolWithTag( PagedPool,
                                                              PAGE_SIZE,
                                                              AFS_RENAME_REQUEST_TAG);

        if( pRenameCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pRenameCB,
                       PAGE_SIZE);

        ASSERT( ParentDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY);

        pRenameCB->SourceParentId = ParentDcb->DirEntry->DirectoryEntry.FileId;

        ASSERT( TargetDcb->DirEntry->DirectoryEntry.FileType == AFS_FILE_TYPE_DIRECTORY);

        pRenameCB->TargetParentId = TargetDcb->DirEntry->DirectoryEntry.FileId;

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
                                      0,
                                      &Fcb->DirEntry->DirectoryEntry.FileName,
                                      &Fcb->DirEntry->DirectoryEntry.FileId,
                                      pRenameCB,
                                      sizeof( AFSFileRenameCB) + TargetName->Length,
                                      pRenameResultCB,
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
        // Update the information from the returned data
        //

        ParentDcb->DirEntry->DirectoryEntry.DataVersion = pRenameResultCB->SourceParentDataVersion;

        TargetDcb->DirEntry->DirectoryEntry.DataVersion = pRenameResultCB->TargetParentDataVersion;

        //
        // Move over the FID information and the short name
        //

        Fcb->DirEntry->DirectoryEntry.FileId = pRenameResultCB->DirEnum.FileId;

        Fcb->DirEntry->DirectoryEntry.ShortNameLength = pRenameResultCB->DirEnum.ShortNameLength;

        if( Fcb->DirEntry->DirectoryEntry.ShortNameLength > 0)
        {

            RtlCopyMemory( Fcb->DirEntry->DirectoryEntry.ShortName,
                           pRenameResultCB->DirEnum.ShortName,
                           Fcb->DirEntry->DirectoryEntry.ShortNameLength);
        }

try_exit:

        if( pRenameCB != NULL)
        {

            ExFreePool( pRenameCB);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSEvaluateTargetByID( IN AFSFileID *SourceFileId,
                       IN ULONGLONG ProcessID,
                       IN BOOLEAN FastCall,
                       OUT AFSDirEnumEntry **DirEnumEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSEvalTargetCB stTargetID;
    ULONG ulResultBufferLength;
    AFSDirEnumEntry *pDirEnumCB = NULL;
    ULONGLONG ullProcessID = ProcessID;
    ULONG ulRequestFlags = AFS_REQUEST_FLAG_SYNCHRONOUS;

    __Enter
    {

        RtlZeroMemory( &stTargetID,
                       sizeof( AFSEvalTargetCB));

        if( ullProcessID == 0)
        {

            ullProcessID = (ULONGLONG)PsGetCurrentProcessId();
        }

        //
        // Allocate our response buffer
        //

        pDirEnumCB = (AFSDirEnumEntry *)ExAllocatePoolWithTag( PagedPool,
                                                               PAGE_SIZE,
                                                               AFS_GENERIC_MEMORY_TAG);

        if( pDirEnumCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Call to the service to evaulate the fid
        //

        ulResultBufferLength = PAGE_SIZE;

        if( FastCall)
        {

            ulRequestFlags |= AFS_REQUEST_FLAG_FAST_REQUEST;
        }

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_EVAL_TARGET_BY_ID,
                                      ulRequestFlags,
                                      ullProcessID,
                                      NULL,
                                      SourceFileId,
                                      &stTargetID,
                                      sizeof( AFSEvalTargetCB),
                                      pDirEnumCB,
                                      &ulResultBufferLength);

        if( ntStatus != STATUS_SUCCESS)
        {

            if( NT_SUCCESS( ntStatus))
            {

                ntStatus = STATUS_DEVICE_NOT_READY;
            }

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

                ExFreePool( pDirEnumCB);
            }

            *DirEnumEntry = NULL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSEvaluateTargetByName( IN AFSFileID *ParentFileId,
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
        // Allocate our response buffe
        //

        pDirEnumCB = (AFSDirEnumEntry *)ExAllocatePoolWithTag( PagedPool,
                                                               PAGE_SIZE,
                                                               AFS_GENERIC_MEMORY_TAG);

        if( pDirEnumCB == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Call to the service to evaulate the fid
        //

        ulResultBufferLength = PAGE_SIZE;

        ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_EVAL_TARGET_BY_NAME,
                                      AFS_REQUEST_FLAG_SYNCHRONOUS,
                                      0,
                                      SourceName,
                                      NULL,
                                      &stTargetID,
                                      sizeof( AFSEvalTargetCB),
                                      pDirEnumCB,
                                      &ulResultBufferLength);

        if( ntStatus != STATUS_SUCCESS)
        {

            if( NT_SUCCESS( ntStatus))
            {

                ntStatus = STATUS_DEVICE_NOT_READY;
            }

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

                ExFreePool( pDirEnumCB);
            }

            *DirEnumEntry = NULL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSProcessRequest( IN ULONG RequestType,
                   IN ULONG RequestFlags,
                   IN ULONGLONG CallerProcess,
                   IN PUNICODE_STRING FileName,
                   IN AFSFileID *FileId,
                   IN void  *Data,
                   IN ULONG DataLength,
                   IN OUT void *ResultBuffer,
                   IN OUT PULONG ResultBufferLength)
{

    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSPoolEntry     stPoolEntry, *pPoolEntry = NULL;
    AFSCommSrvcCB   *pCommSrvc = NULL;
    BOOLEAN          bReleasePool = FALSE;
    AFSDeviceExt    *pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    BOOLEAN          bWait = BooleanFlagOn( RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS);
    ULONG            ulPoolEntryLength = 0;
    __try
    {

        pCommSrvc = &pControlDevExt->Specific.Control.CommServiceCB;

        //
        // Grab the pool resource and check the state
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessRequest Acquiring IrpPoolLock lock %08lX EXCL %08lX\n",
                                                         &pCommSrvc->IrpPoolLock,
                                                         PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                        TRUE);

        bReleasePool = TRUE;

        if( pCommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
        {

            //
            // Pool not running so bail. 
            //

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        //
        // If this is an async request we need to allocate a pool entry for the request
        //

        pPoolEntry = &stPoolEntry;

        if( !bWait)
        {

            ASSERT( ResultBuffer == NULL);

            ulPoolEntryLength = sizeof( AFSPoolEntry) + QuadAlign( DataLength);

            if( FileName != NULL)
            {

                ulPoolEntryLength += FileName->Length;
            }

            pPoolEntry = (AFSPoolEntry *)ExAllocatePoolWithTag( NonPagedPool,
                                                                ulPoolEntryLength,
                                                                AFS_POOL_ENTRY_TAG);

            if( pPoolEntry == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlZeroMemory( pPoolEntry,
                           ulPoolEntryLength);

            pPoolEntry->Data = (void *)((char *)pPoolEntry + sizeof( AFSPoolEntry));

            pPoolEntry->FileName.Buffer = (WCHAR *)((char *)pPoolEntry->Data + DataLength); 
        }
        else
        {

            RtlZeroMemory( pPoolEntry,
                           sizeof( AFSPoolEntry));

            KeInitializeEvent( &pPoolEntry->Event,
                               NotificationEvent,
                               FALSE);
        }

        pPoolEntry->RequestType = RequestType;

        pPoolEntry->RequestIndex = pCommSrvc->IrpPoolRequestIndex++;

        pPoolEntry->RequestFlags = RequestFlags;

        pPoolEntry->ResultBufferLength = 0;

        if( FileId != NULL)
        {

            pPoolEntry->FileId = *FileId;
        }

        pPoolEntry->FileName.Length = 0;

        if( FileName != NULL)
        {

            if( bWait)
            {

                pPoolEntry->FileName = *FileName;
            }
            else
            {

                pPoolEntry->FileName.Length = FileName->Length;

                pPoolEntry->FileName.MaximumLength = pPoolEntry->FileName.Length;

                RtlCopyMemory( pPoolEntry->FileName.Buffer,
                               FileName->Buffer,
                               pPoolEntry->FileName.Length);
            }
        }

        //
        // Move in the data if there is some
        //

        pPoolEntry->DataLength = DataLength;

        if( Data != NULL &&
            DataLength > 0)
        {

            if( bWait)
            {

                pPoolEntry->Data = Data;
            }
            else
            {

                RtlCopyMemory( pPoolEntry->Data,
                               Data,
                               DataLength);
            }
        }

        pPoolEntry->ResultBuffer = ResultBuffer;

        pPoolEntry->ResultBufferLength = ResultBufferLength;

        //
        // Store off the process id
        //

        if( ( RequestType == AFS_REQUEST_TYPE_REQUEST_FILE_EXTENTS ||
              RequestType == AFS_REQUEST_TYPE_RELEASE_FILE_EXTENTS) &&
            ( CallerProcess == 0 ||
              CallerProcess == (ULONGLONG)AFSSysProcess))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSProcessRequest Extent request/release in system context\n");

            ASSERT( FALSE);
        }

        if( CallerProcess != 0)
        {

            pPoolEntry->ProcessID = CallerProcess;
        }
        else
        {

            pPoolEntry->ProcessID = (ULONGLONG)PsGetCurrentProcessId();
        }

        //
        // Indicate the type of process
        //

        if( AFSIs64BitProcess( pPoolEntry->ProcessID))
        {

            SetFlag( pPoolEntry->RequestFlags, AFS_REQUEST_FLAG_WOW64);
        }

        //
        // Insert the entry into the request pool
        //

        ntStatus = AFSInsertRequest( pCommSrvc,
                                     pPoolEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            if( !bWait)
            {

                ExFreePool( pPoolEntry);
            }

            try_return( ntStatus);
        }

        //
        // Drop the lock on the pool prior to waiting
        //

        AFSReleaseResource( &pCommSrvc->IrpPoolLock);

        bReleasePool = FALSE;

        //
        // Wait for the result if this is NOT an asynchronous request
        //

        if( bWait)
        {

            //
            // Wait for the result of the request. We specify no timeout ...
            //

            ntStatus = KeWaitForSingleObject( &pPoolEntry->Event,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              NULL);

            //
            // Process the result of the request
            //

            if( ntStatus == STATUS_SUCCESS)
            {

                ntStatus = pPoolEntry->ResultStatus;
            }
            else
            {

                ntStatus = STATUS_DEVICE_NOT_READY;
            }
        }

try_exit:

        if( bReleasePool)
        {

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()))
    {

        ntStatus = STATUS_UNSUCCESSFUL;
    }

    return ntStatus;
}

NTSTATUS
AFSProcessControlRequest( IN PIRP Irp)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION  pIrpSp;    
    ULONG               ulIoControlCode;
    BOOLEAN             bCompleteRequest = TRUE;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    ULONG               ulBytesProcessed = 0;

    __try
    {
        
        pIrpSp = IoGetCurrentIrpStackLocation( Irp);

        ulIoControlCode = pIrpSp->Parameters.DeviceIoControl.IoControlCode;

        switch( ulIoControlCode)
        {

            case IOCTL_AFS_INITIALIZE_CONTROL_DEVICE:
            {

                //
                // Go intialize the pool
                //

                ntStatus = AFSInitIrpPool();

                if( !NT_SUCCESS( ntStatus))
                {

                    //
                    // Don't initialize
                    //

                    break;
                }

                //
                // Tag this instance as the one to close the irp pool when it is closed
                //

                pIrpSp->FileObject->FsContext = (void *)((ULONG_PTR)pIrpSp->FileObject->FsContext | AFS_CONTROL_INSTANCE);

                break;
            }

            case IOCTL_AFS_INITIALIZE_REDIRECTOR_DEVICE:
            {

                AFSCacheFileInfo *pCacheFileInfo = (AFSCacheFileInfo *)Irp->AssociatedIrp.SystemBuffer;

                //
                // Extract off the passed in information which contains the
                // cache file parameters
                //

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSCacheFileInfo) ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < (ULONG)FIELD_OFFSET( AFSCacheFileInfo, CacheFileName) +
                                                                                                    pCacheFileInfo->CacheFileNameLength)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                //
                // Initialize the Redirector device
                //

                ntStatus = AFSInitializeRedirector( pCacheFileInfo);

                if( !NT_SUCCESS( ntStatus))
                {

                    break;
                }

                //
                // Stash away context so we know the instance used to initialize the redirector
                //

                pIrpSp->FileObject->FsContext = (void *)((ULONG_PTR)pIrpSp->FileObject->FsContext | AFS_REDIRECTOR_INSTANCE);

                break;
            }

            case IOCTL_AFS_PROCESS_IRP_REQUEST:
            {

                ntStatus = AFSProcessIrpRequest( Irp);

                break;
            }

            case IOCTL_AFS_PROCESS_IRP_RESULT:
            {

                ntStatus = AFSProcessIrpResult( Irp);

                break;
            }

            case IOCTL_AFS_ADD_CONNECTION:
            {

                AFSNetworkProviderConnectionCB *pConnectCB = (AFSNetworkProviderConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSNetworkProviderConnectionCB) ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < (ULONG)FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                                                                            pConnectCB->RemoteNameLength ||
                    pIrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof( ULONG))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSAddConnection( pConnectCB,
                                             (PULONG)Irp->AssociatedIrp.SystemBuffer,
                                             &Irp->IoStatus.Information);

                break;                                                  
            }

            case IOCTL_AFS_CANCEL_CONNECTION:
            {

                AFSNetworkProviderConnectionCB *pConnectCB = (AFSNetworkProviderConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSNetworkProviderConnectionCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSCancelConnection( pConnectCB,
                                                (PULONG)Irp->AssociatedIrp.SystemBuffer,
                                                &Irp->IoStatus.Information);
                      
                break;
            }

            case IOCTL_AFS_GET_CONNECTION:
            {

                AFSNetworkProviderConnectionCB *pConnectCB = (AFSNetworkProviderConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSNetworkProviderConnectionCB) ||
                    pIrpSp->Parameters.DeviceIoControl.OutputBufferLength == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSGetConnection( pConnectCB,
                                             (WCHAR *)Irp->AssociatedIrp.SystemBuffer,
                                             pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                             &Irp->IoStatus.Information);
                      
                break;
            }

            case IOCTL_AFS_LIST_CONNECTIONS:
            {

                if( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSListConnections( (AFSNetworkProviderConnectionCB *)Irp->AssociatedIrp.SystemBuffer,
                                                pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                                &Irp->IoStatus.Information);
                      
                break;
            }

            case IOCTL_AFS_GET_CONNECTION_INFORMATION:
            {

                AFSNetworkProviderConnectionCB *pConnectCB = (AFSNetworkProviderConnectionCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSNetworkProviderConnectionCB) ||
                    pIrpSp->Parameters.DeviceIoControl.OutputBufferLength == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSGetConnectionInfo( pConnectCB,
                                                 pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                                 &Irp->IoStatus.Information);

                break;
            }

            case IOCTL_AFS_SET_FILE_EXTENTS:
            {
                AFSSetFileExtentsCB *pExtents = (AFSSetFileExtentsCB*) Irp->AssociatedIrp.SystemBuffer;
                //
                // Check lengths twice so that if the buffer makes the
                // count invalid we will not Accvio
                //

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < 
                    ( FIELD_OFFSET( AFSSetFileExtentsCB, ExtentCount) + sizeof(ULONG)) ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength <
                    ( FIELD_OFFSET( AFSSetFileExtentsCB, ExtentCount) + sizeof(ULONG) +
                      sizeof (AFSFileExtentCB) * pExtents->ExtentCount))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSProcessSetFileExtents( pExtents );

                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = ntStatus;
                      
                break;
            }

            case IOCTL_AFS_RELEASE_FILE_EXTENTS:
            {
                ntStatus = AFSProcessReleaseFileExtents( Irp, FALSE, &bCompleteRequest);
                break;
            }

            case IOCTL_AFS_RELEASE_FILE_EXTENTS_DONE:
            {
                ntStatus = AFSProcessReleaseFileExtentsDone( Irp );
                break;
            }

            case IOCTL_AFS_INVALIDATE_CACHE:
            {

                AFSInvalidateCacheCB *pInvalidate = (AFSInvalidateCacheCB*)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSInvalidateCacheCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSInvalidateCache( pInvalidate);

                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = ntStatus;

                break;
            }

            case IOCTL_AFS_NETWORK_STATUS:
            {

                AFSNetworkStatusCB *pNetworkStatus = (AFSNetworkStatusCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSNetworkStatusCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                //
                // Set the network status
                //

                ntStatus = AFSSetNetworkState( pNetworkStatus);

                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = ntStatus;

                break;
            }

            case IOCTL_AFS_VOLUME_STATUS:
            {

                AFSVolumeStatusCB *pVolumeStatus = (AFSVolumeStatusCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSVolumeStatusCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSSetVolumeState( pVolumeStatus);

                Irp->IoStatus.Information = 0;
                Irp->IoStatus.Status = ntStatus;

                break;
            }

            case IOCTL_AFS_STATUS_REQUEST:
            {

                if( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength < sizeof( AFSDriverStatusRespCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSGetDriverStatus( (AFSDriverStatusRespCB *)Irp->AssociatedIrp.SystemBuffer);

                Irp->IoStatus.Information = sizeof( AFSDriverStatusRespCB);

                break;
            }

            case IOCTL_AFS_SYSNAME_NOTIFICATION:
            {

                AFSSysNameNotificationCB *pSysNameInfo = (AFSSysNameNotificationCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pSysNameInfo == NULL ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSSysNameNotificationCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSSetSysNameInformation( pSysNameInfo,
                                                     pIrpSp->Parameters.DeviceIoControl.InputBufferLength);                                                     

                break;
            }

            case IOCTL_AFS_CONFIGURE_DEBUG_TRACE:
            {

                AFSTraceConfigCB *pTraceInfo = (AFSTraceConfigCB *)Irp->AssociatedIrp.SystemBuffer;

                if( pTraceInfo == NULL ||
                    pIrpSp->Parameters.DeviceIoControl.InputBufferLength < sizeof( AFSTraceConfigCB))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSConfigureTrace( pTraceInfo);

                break;
            }

            case IOCTL_AFS_GET_TRACE_BUFFER:
            {

                if( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength == 0)
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                ntStatus = AFSGetTraceBuffer( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength,
                                              Irp->AssociatedIrp.SystemBuffer,
                                              &Irp->IoStatus.Information);

                break;
            }

            case IOCTL_AFS_FORCE_CRASH:
            {

                KeBugCheck( (ULONG)-1);

                break;
            }

            default:
            {

                ntStatus = STATUS_NOT_IMPLEMENTED;

                break;
            }
        }

//try_exit:

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()))
    {

        ntStatus = STATUS_UNSUCCESSFUL;
    }

    if( bCompleteRequest)
    {

        Irp->IoStatus.Status = ntStatus;

        AFSCompleteRequest( Irp,
                              ntStatus);
    }

    return ntStatus;
}

NTSTATUS
AFSInitIrpPool()
{

    NTSTATUS       ntStatus = STATUS_SUCCESS;    
    AFSCommSrvcCB *pCommSrvc = NULL;
    BOOLEAN        bReleasePools = FALSE;        
    AFSDeviceExt  *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        pCommSrvc = &pDevExt->Specific.Control.CommServiceCB;

        //
        // WHenever we change state we must grab both pool locks. On the checking of the state
        // within the processing routines for these respective pools, we only grab one lock to
        // minimize serialization. The ordering is always the Irp pool then the result pool
        // locks. We also do this in the tear down of the pool
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitIrpPool Acquiring IrpPoolLock lock %08lX EXCL %08lX\n",
                                                         &pCommSrvc->IrpPoolLock,
                                                         PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                          TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitIrpPool Acquiring ResultPoolLock lock %08lX EXCL %08lX\n",
                                                         &pCommSrvc->ResultPoolLock,
                                                         PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                          TRUE);

        bReleasePools = TRUE;

        //
        // The pool can be either ACTIVE or INACTIVE. If the pool state is INACTIVE and we 
        // are receiving the INIT request, then activate it. If the pool is ACTIVE, then we
        // shouldn't be getting this request ...
        //

        if( pCommSrvc->IrpPoolControlFlag == POOL_ACTIVE)
        {

            //
            // We have already been activated so just fail this request
            //

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }
        else if( pCommSrvc->IrpPoolControlFlag == POOL_INACTIVE)
        {

            //
            // The pool is currently INACTIVE so start it up and ready it to 
            // receive irp requests
            //

            pCommSrvc->IrpPoolControlFlag = POOL_ACTIVE;

            pDevExt->Specific.Control.ServiceProcess = (PKPROCESS)PsGetCurrentProcess();

            try_return( ntStatus = STATUS_SUCCESS);
        }
        else
        {

            //
            // The pool is in some mixed state, fail the request.
            //

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

try_exit:

        if( bReleasePools)
        {

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);

            AFSReleaseResource( &pCommSrvc->ResultPoolLock);
        }
    }

    return ntStatus;
}

void
AFSCleanupIrpPool()
{

    NTSTATUS        ntStatus = STATUS_SUCCESS;
    AFSPoolEntry   *pEntry = NULL, *pNextEntry = NULL;
    AFSDeviceExt   *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSCommSrvcCB  *pCommSrvc = (AFSCommSrvcCB *)&pDevExt->Specific.Control.CommServiceCB;

    __Enter
    {

        //
        // When we change the state, grab both pool locks exclusive. The order is always the 
        // Irp pool then the result pool lock
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSCleanupIrpPool Acquiring IrpPoolLock lock %08lX EXCL %08lX\n",
                                                         &pCommSrvc->IrpPoolLock,
                                                         PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                        TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSCleanupIrpPool Acquiring ResultPoolLock lock %08lX EXCL %08lX\n",
                                                         &pCommSrvc->ResultPoolLock,
                                                         PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                        TRUE);

        //
        // Indicate we are pending stop
        //

        pCommSrvc->IrpPoolControlFlag = POOL_INACTIVE;

        //
        // Set the event to release any waiting workers
        //

        KeSetEvent( &pCommSrvc->IrpPoolHasEntries,
                    0,
                    FALSE);

        //
        // Go through the pool entries and free up the structures.
        //

        pEntry = pCommSrvc->RequestPoolHead;

        while( pEntry != NULL)
        {

            pNextEntry = pEntry->fLink;

            if( BooleanFlagOn( pEntry->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS))
            {

                //
                // Here we need to complete the irp, cancelled, and delete the data block
                //

                pEntry->ResultStatus = STATUS_CANCELLED;

                KeSetEvent( &pEntry->Event,
                            0,
                            FALSE);
            }
            else
            {

                ExFreePool( pEntry);
            }

            pEntry = pNextEntry;
        }

        //
        // Cleanup the control structure for the request pool
        //

        pCommSrvc->RequestPoolHead = NULL;

        pCommSrvc->RequestPoolTail = NULL;

        pCommSrvc->IrpPoolRequestIndex = 1;

        KeClearEvent( &pCommSrvc->IrpPoolHasEntries);

        //
        // Release the irp pool lock. 
        //

        AFSReleaseResource( &pCommSrvc->IrpPoolLock);

        //
        // Go through the result pool entries and free up the structures.
        //

        pEntry = pCommSrvc->ResultPoolHead;

        while( pEntry != NULL)
        {

            pNextEntry = pEntry->fLink;

            pEntry->ResultStatus = STATUS_CANCELLED;

            //
            // Here we will set the event of the requestor and let the blocked thread
            // free the data block
            //

            KeSetEvent( &pEntry->Event,
                        0,
                        FALSE);

            //
            // Go onto the next entry
            //

            pEntry = pNextEntry;
        }

        //
        // Cleanup the control structure for the result pool
        //

        pCommSrvc->ResultPoolHead = NULL;

        pCommSrvc->ResultPoolTail = NULL;

        //
        // Release the result pool lock. 
        //

        AFSReleaseResource( &pCommSrvc->ResultPoolLock);
    }

    return;
}

NTSTATUS
AFSInsertRequest( IN AFSCommSrvcCB *CommSrvc,
                  IN AFSPoolEntry *Entry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInsertRequest Acquiring IrpPoolLock lock %08lX EXCL %08lX\n",
                                                         &CommSrvc->IrpPoolLock,
                                                         PsGetCurrentThread());

        AFSAcquireExcl( &CommSrvc->IrpPoolLock,
                        TRUE);

        if( CommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
        {

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        if( CommSrvc->RequestPoolHead == NULL)
        {

            CommSrvc->RequestPoolHead = Entry;

            KeSetEvent( &CommSrvc->IrpPoolHasEntries,
                        0,
                        FALSE);
        }
        else
        {

            CommSrvc->RequestPoolTail->fLink = Entry;

            Entry->bLink = CommSrvc->RequestPoolTail;        
        }

        CommSrvc->RequestPoolTail = Entry;

try_exit:

        AFSReleaseResource( &CommSrvc->IrpPoolLock);
    }

    return ntStatus;
}

NTSTATUS 
AFSProcessIrpRequest( IN PIRP Irp)
{

    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSDeviceExt    *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSCommSrvcCB   *pCommSrvc = NULL;
    AFSPoolEntry    *pEntry = NULL;
    AFSCommRequest  *pRequest = NULL;

    __Enter
    {

        pCommSrvc = &pDevExt->Specific.Control.CommServiceCB;

        pRequest = (AFSCommRequest *)Irp->AssociatedIrp.SystemBuffer;

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessIrpRequest Acquiring IrpPoolLock lock %08lX EXCL %08lX\n",
                                                         &pCommSrvc->IrpPoolLock,
                                                         PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                        TRUE);

        if( pCommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
        {

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        AFSReleaseResource( &pCommSrvc->IrpPoolLock);

        //
        // Wait on the 'have items' event until we can retrieve an item
        //

        while( TRUE)
        {

            ntStatus = KeWaitForSingleObject( &pCommSrvc->IrpPoolHasEntries,
                                              UserRequest,
                                              UserMode,
                                              TRUE,
                                              NULL);

            if( ntStatus != STATUS_SUCCESS)
            {
                
                ntStatus = STATUS_DEVICE_NOT_READY;

                break;
            }

            //
            // Grab the lock on the request pool
            //

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSProcessIrpRequest Acquiring IrpPoolLock (WAIT) lock %08lX EXCL %08lX\n",
                                                             &pCommSrvc->IrpPoolLock,
                                                             PsGetCurrentThread());

            AFSAcquireExcl( &pCommSrvc->IrpPoolLock,
                            TRUE);

            if( pCommSrvc->IrpPoolControlFlag != POOL_ACTIVE)
            {

                AFSReleaseResource( &pCommSrvc->IrpPoolLock);

                try_return( ntStatus = STATUS_DEVICE_NOT_READY);
            }

            pEntry = pCommSrvc->RequestPoolHead;

            if( pEntry != NULL)
            {

                pCommSrvc->RequestPoolHead = pEntry->fLink;

                pEntry->bLink = NULL;

                if( pCommSrvc->RequestPoolHead == NULL)
                {

                    pCommSrvc->RequestPoolTail = NULL;
                }
            }
            else
            {

                KeClearEvent( &pCommSrvc->IrpPoolHasEntries);
            }

            //
            // And release the request pool lock
            //

            AFSReleaseResource( &pCommSrvc->IrpPoolLock);

            //
            // Insert the entry into the result pool, if we have one
            //

            if( pEntry != NULL)
            {
                
                //
                // Move the request data into the passed in buffer
                //

                ASSERT( sizeof( AFSCommRequest) + 
                                    pEntry->FileName.Length +
                                    pEntry->DataLength <= pIrpSp->Parameters.DeviceIoControl.OutputBufferLength);

                pRequest->ProcessId.QuadPart = pEntry->ProcessID;

                pRequest->FileId = pEntry->FileId;

                pRequest->RequestType = pEntry->RequestType;

                pRequest->RequestIndex = pEntry->RequestIndex;

                pRequest->RequestFlags = pEntry->RequestFlags;

                pRequest->NameLength = pEntry->FileName.Length;

                if( pRequest->NameLength > 0)
                {

                    RtlCopyMemory( pRequest->Name,
                                   pEntry->FileName.Buffer,
                                   pRequest->NameLength);
                }

                pRequest->DataOffset = 0;

                pRequest->DataLength = pEntry->DataLength;

                if( pRequest->DataLength > 0)
                {

                    pRequest->DataOffset = pEntry->FileName.Length;

                    RtlCopyMemory( (void *)((char *)pRequest->Name + pRequest->DataOffset),
                                   pEntry->Data,
                                   pRequest->DataLength);
                }

                pRequest->SIDOffset = 0;

                pRequest->SIDLength = 0;

                pRequest->ResultBufferLength = 0;

                if( pEntry->ResultBufferLength != NULL)
                {

                    pRequest->ResultBufferLength = *(pEntry->ResultBufferLength);
                }

                Irp->IoStatus.Information = sizeof( AFSCommRequest) + 
                                                        pEntry->FileName.Length +
                                                        pEntry->DataLength;

                //
                // If this is a synchronous request then move the request into the 
                // result pool
                //

                if( BooleanFlagOn( pEntry->RequestFlags, AFS_REQUEST_FLAG_SYNCHRONOUS))
                {

                    pEntry->fLink = NULL;
                    pEntry->bLink = NULL;

                    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSProcessIrpRequest Acquiring ResultPoolLock lock %08lX EXCL %08lX\n",
                                                                     &pCommSrvc->ResultPoolLock,
                                                                     PsGetCurrentThread());

                    AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                                    TRUE);

                    if( pCommSrvc->ResultPoolHead == NULL)
                    {

                        pCommSrvc->ResultPoolHead = pEntry;
                    }
                    else
                    {

                        pCommSrvc->ResultPoolTail->fLink = pEntry;

                        pEntry->bLink = pCommSrvc->ResultPoolTail;
                    }

                    pCommSrvc->ResultPoolTail = pEntry;

                    AFSReleaseResource( &pCommSrvc->ResultPoolLock);
                }
                else
                {

                    //
                    // Free up the pool entry
                    //

                    ExFreePool( pEntry);
                }

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS 
AFSProcessIrpResult( IN PIRP Irp)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    AFSDeviceExt       *pDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    IO_STACK_LOCATION  *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSCommSrvcCB      *pCommSrvc = NULL;
    AFSPoolEntry       *pCurrentEntry = NULL;
    AFSCommResult      *pResult = NULL;

    __Enter
    {

        pCommSrvc = &pDevExt->Specific.Control.CommServiceCB;

        //
        // Get the request for the incoming result
        //

        pResult = (AFSCommResult *)Irp->AssociatedIrp.SystemBuffer;

        if( pResult == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // Go look for our entry
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessIrpResult Acquiring ResultPoolLock lock %08lX EXCL %08lX\n",
                                                                     &pCommSrvc->ResultPoolLock,
                                                                     PsGetCurrentThread());

        AFSAcquireExcl( &pCommSrvc->ResultPoolLock,
                          TRUE);

        pCurrentEntry = pCommSrvc->ResultPoolHead;

        while( pCurrentEntry != NULL)
        {

            if( pCurrentEntry->RequestIndex == pResult->RequestIndex)
            {

                //
                // Found the entry so remove it from the queue
                //

                if( pCurrentEntry->bLink == NULL)
                {

                    //
                    // At the head of the list
                    //

                    pCommSrvc->ResultPoolHead = pCurrentEntry->fLink;

                    if( pCommSrvc->ResultPoolHead != NULL)
                    {

                        pCommSrvc->ResultPoolHead->bLink = NULL;
                    }
                }
                else
                {

                    pCurrentEntry->bLink->fLink = pCurrentEntry->fLink;
                }

                if( pCurrentEntry->fLink == NULL)
                {

                    pCommSrvc->ResultPoolTail = pCurrentEntry->bLink;

                    if( pCommSrvc->ResultPoolTail != NULL)
                    {

                        pCommSrvc->ResultPoolTail->fLink = NULL;
                    }
                }
                else
                {

                    pCurrentEntry->fLink->bLink = pCurrentEntry->bLink;
                }

                break;
            }

            pCurrentEntry = pCurrentEntry->fLink;
        }

        AFSReleaseResource( &pCommSrvc->ResultPoolLock);

        if( pCurrentEntry == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // OK, move in the result information
        //

        pCurrentEntry->ResultStatus = pResult->ResultStatus;

        if( pCurrentEntry->ResultStatus == STATUS_SUCCESS &&
            pCurrentEntry->ResultBufferLength != NULL &&
            pCurrentEntry->ResultBuffer != NULL)
        {

            ASSERT( pResult->ResultBufferLength <= *(pCurrentEntry->ResultBufferLength));                       

            *(pCurrentEntry->ResultBufferLength) = pResult->ResultBufferLength;

            if( pResult->ResultBufferLength > 0)
            {

                RtlCopyMemory( pCurrentEntry->ResultBuffer,
                               pResult->ResultData,
                               pResult->ResultBufferLength);
            }
        }

        KeSetEvent( &pCurrentEntry->Event,
                    0,
                    FALSE);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

        }
    }

    return ntStatus;
}
