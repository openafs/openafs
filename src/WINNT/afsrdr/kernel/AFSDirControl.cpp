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
// File: AFSDirControl.cpp
//

#include "AFSCommon.h"

//
// Function: AFSDirControl
//
// Description: 
//
//      This function is the IRP_MJ_DIRECTORY_CONTROL dispatch handler
//
// Return:
//
//       A status is returned for the handling of this request
//

NTSTATUS
AFSDirControl( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulRequestType = 0;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;

    __try
    {

        switch( pIrpSp->MinorFunction ) 
        {

            case IRP_MN_QUERY_DIRECTORY:
            {

                ntStatus = AFSQueryDirectory( Irp);

                break;
            }

            case IRP_MN_NOTIFY_CHANGE_DIRECTORY:
            {

                ntStatus = AFSNotifyChangeDirectory( Irp);

                break;
            }

            default:

                ntStatus = STATUS_INVALID_DEVICE_REQUEST;
                
                break;
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSDirControl\n");
    }

    if( ntStatus != STATUS_PENDING)
    {

        AFSCompleteRequest( Irp,
                            ntStatus);
    }

    return ntStatus;
}

NTSTATUS 
AFSQueryDirectory( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    NTSTATUS dStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp;

    AFSFcb *pFcb = NULL, *pParentDcb = NULL;

    AFSCcb *pCcb = NULL;
    BOOLEAN bInitialQuery = FALSE;

    ULONG ulIndex;
    PUCHAR pBuffer;
    ULONG ulUserBufferLength;

    PUNICODE_STRING puniArgFileName = NULL;

    UNICODE_STRING uniTmpMaskName;
    UNICODE_STRING uniDirUniBuf;
    
    WCHAR wchMaskBuffer[ 4];

    FILE_INFORMATION_CLASS FileInformationClass;
    
    ULONG ulFileIndex, ulDOSFileIndex;
    BOOLEAN bRestartScan;
    BOOLEAN bReturnSingleEntry;
    BOOLEAN bIndexSpecified;

    ULONG ulNextEntry = 0;
    ULONG ulLastEntry = 0;
    BOOLEAN bDoCase;

    PFILE_DIRECTORY_INFORMATION pDirInfo;
    PFILE_FULL_DIR_INFORMATION pFullDirInfo;
    PFILE_BOTH_DIR_INFORMATION pBothDirInfo;
    PFILE_NAMES_INFORMATION pNamesInfo;

    ULONG ulBaseLength;
    ULONG ulBytesConverted;

    AFSDirEntryCB *pDirEntry = NULL, *pBestMatchDirEntry = NULL;

    BOOLEAN bReleaseMain = FALSE;

    __Enter
    {

        //  Get the current Stack location
        pIrpSp = IoGetCurrentIrpStackLocation( Irp);

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;
        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        if( pFcb->Header.NodeTypeCode != AFS_DIRECTORY_FCB &&
            pFcb->Header.NodeTypeCode != AFS_ROOT_FCB &&
            pFcb->Header.NodeTypeCode != AFS_ROOT_ALL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //  Reference our input parameters to make things easier
        ulUserBufferLength = pIrpSp->Parameters.QueryDirectory.Length;

        FileInformationClass = pIrpSp->Parameters.QueryDirectory.FileInformationClass;
        ulFileIndex = pIrpSp->Parameters.QueryDirectory.FileIndex;

        puniArgFileName = (PUNICODE_STRING)pIrpSp->Parameters.QueryDirectory.FileName;

        bRestartScan       = BooleanFlagOn( pIrpSp->Flags, SL_RESTART_SCAN);
        bReturnSingleEntry = BooleanFlagOn( pIrpSp->Flags, SL_RETURN_SINGLE_ENTRY);
        bIndexSpecified    = BooleanFlagOn( pIrpSp->Flags, SL_INDEX_SPECIFIED);

        bInitialQuery = (BOOLEAN)(!pCcb->DirQueryMapped);

        if( bInitialQuery)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryDirectory Acquiring Dcb lock %08lX EXCL %08lX\n",
                                                          &pFcb->NPFcb->Resource,
                                                          PsGetCurrentThread());

            AFSAcquireExcl( &pFcb->NPFcb->Resource,
                            TRUE);

            //
            // Tell the service to prime the cache of the directory content
            //

            ntStatus = AFSEnumerateDirectoryNoResponse( pFcb);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }
        }
        else
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryDirectory Acquiring Dcb lock %08lX SHARED %08lX\n",
                                                          &pFcb->NPFcb->Resource,
                                                          PsGetCurrentThread());

            AFSAcquireShared( &pFcb->NPFcb->Resource,
                              TRUE);
        }

        bReleaseMain = TRUE;

        //
        // Start processing the data
        //

        pBuffer = (PUCHAR)AFSLockSystemBuffer( Irp,
                                               ulUserBufferLength);

        if( pBuffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        // Check if initial on this map
        if( bInitialQuery)
        {

            pCcb->DirQueryMapped = TRUE;

            ClearFlag( pCcb->Flags, CCB_FLAG_DIR_OF_DIRS_ONLY);

            // build mask if none
            if( puniArgFileName == NULL)
            {
                puniArgFileName = &uniTmpMaskName;
                puniArgFileName->Length = 0;
                puniArgFileName->Buffer = NULL;
            }

            if( puniArgFileName->Length == 0)
            {

                puniArgFileName->Length = sizeof(WCHAR);
                puniArgFileName->MaximumLength = (USHORT)4;
            }

            if( puniArgFileName->Buffer == NULL)
            {

                puniArgFileName->Buffer = wchMaskBuffer;
                
                RtlZeroMemory( wchMaskBuffer, 
                               4);
                
                RtlCopyMemory( &puniArgFileName->Buffer[ 0], 
                               L"*", 
                               sizeof(WCHAR));
            }

            if( (( puniArgFileName->Length == sizeof(WCHAR)) &&
                 ( puniArgFileName->Buffer[0] == L'*'))) 
            {

                SetFlag( pCcb->Flags, CCB_FLAG_FULL_DIRECTORY_QUERY);
            }
            else
            {

                if( (( puniArgFileName->Length == sizeof(WCHAR)) &&
                     ( puniArgFileName->Buffer[0] == L'<')) ||
                    (( puniArgFileName->Length == 2*sizeof(WCHAR)) &&
                    ( RtlEqualMemory( puniArgFileName->Buffer, L"*.", 2*sizeof(WCHAR) ))))
                {

                    SetFlag( pCcb->Flags, CCB_FLAG_DIR_OF_DIRS_ONLY);
                }

                //
                // Build the name for procesisng
                //

                pCcb->MaskName.Length = puniArgFileName->Length;
                pCcb->MaskName.MaximumLength = pCcb->MaskName.Length;

                pCcb->MaskName.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                                        pCcb->MaskName.Length,
                                                                        AFS_GENERIC_MEMORY_TAG);

                if( pCcb->MaskName.Buffer == NULL)
                {

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                if( FsRtlDoesNameContainWildCards( puniArgFileName))
                {

                    RtlUpcaseUnicodeString( &pCcb->MaskName,
                                            puniArgFileName,
                                            FALSE);

                    SetFlag( pCcb->Flags, CCB_FLAG_MASK_CONTAINS_WILD_CARDS);
                }
                else
                {

                    RtlCopyMemory( pCcb->MaskName.Buffer,
                                   puniArgFileName->Buffer,
                                   pCcb->MaskName.Length);
                }
            }

            // Drop to shared on teh Fcb 
            AFSConvertToShared( &pFcb->NPFcb->Resource);
        }

        // Check if we need to start from index
        if( bIndexSpecified)
        {

            //
            // Need to set up the initial point for the query
            //
                
            if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
            {

                pDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;
            }
            else
            {

                pDirEntry = pFcb->Specific.VolumeRoot.DirectoryNodeListHead;
            }

            while( pDirEntry != NULL)
            {

                if( pDirEntry->DirectoryEntry.FileIndex == pCcb->CurrentDirIndex)
                {

                    //
                    // Go to the next entry if there is one
                    //

                    if( pDirEntry->ListEntry.fLink == NULL)
                    {

                        pDirEntry = NULL;
                    }
                    else
                    {

                        pDirEntry = (AFSDirEntryCB *)pDirEntry->ListEntry.fLink;
                    }

                    break;
                }

                pDirEntry = (AFSDirEntryCB *)pDirEntry->ListEntry.fLink;
            }

            if( pDirEntry == NULL)
            {

                try_return( ntStatus = STATUS_NO_MORE_FILES);
            }
        }
                                                     
        // Check if we need to restart the scan
        else if( bRestartScan)
        {

            //
            // Reset the current scan processing
            //

            if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
            {

                pDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;
            }
            else
            {

                pDirEntry = pFcb->Specific.VolumeRoot.DirectoryNodeListHead;
            }

            pCcb->CurrentDirIndex = 0;

            if( pDirEntry == NULL)
            {

                try_return( ntStatus = STATUS_NO_SUCH_FILE);
            }
        }

        // Default start from where left off
        else
        {

            if( pCcb->CurrentDirIndex == 0)
            {

                if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
                {

                    pDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;
                }
                else
                {

                    pDirEntry = pFcb->Specific.VolumeRoot.DirectoryNodeListHead;
                }

                if( pDirEntry == NULL)
                {

                    try_return( ntStatus = STATUS_NO_SUCH_FILE);
                }
            }
            else
            {

                if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
                {

                    pDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;
                }
                else
                {

                    pDirEntry = pFcb->Specific.VolumeRoot.DirectoryNodeListHead;
                }

                while( pDirEntry != NULL)
                {

                    if( pDirEntry->DirectoryEntry.FileIndex == pCcb->CurrentDirIndex)
                    {

                        //
                        // Go to the next entry if there is one
                        //

                        if( pDirEntry->ListEntry.fLink == NULL)
                        {

                            pDirEntry = NULL;
                        }
                        else
                        {

                            pDirEntry = (AFSDirEntryCB *)pDirEntry->ListEntry.fLink;
                        }

                        break;
                    }

                    pDirEntry = (AFSDirEntryCB *)pDirEntry->ListEntry.fLink;
                }

                if( pDirEntry == NULL)
                {

                    try_return( ntStatus = STATUS_NO_MORE_FILES);
                }
            }
        }

        switch( FileInformationClass) 
        {

            case FileDirectoryInformation:

                ulBaseLength = FIELD_OFFSET( FILE_DIRECTORY_INFORMATION,
                                               FileName[0] );
                break;

            case FileFullDirectoryInformation:

                ulBaseLength = FIELD_OFFSET( FILE_FULL_DIR_INFORMATION,
                                             FileName[0] );
                break;

            case FileNamesInformation:

                ulBaseLength = FIELD_OFFSET( FILE_NAMES_INFORMATION,
                                             FileName[0] );
                break;

            case FileBothDirectoryInformation:

                ulBaseLength = FIELD_OFFSET( FILE_BOTH_DIR_INFORMATION,
                                               FileName[0] );
                break;

            case FileIdBothDirectoryInformation:

                ulBaseLength = FIELD_OFFSET( FILE_ID_BOTH_DIR_INFORMATION,
                                               FileName[0] );

                break;

            case FileIdFullDirectoryInformation:

                ulBaseLength = FIELD_OFFSET( FILE_ID_FULL_DIR_INFORMATION,
                                               FileName[0] );

                break;

            default:

                ntStatus = STATUS_INVALID_INFO_CLASS;
                try_return( ntStatus );
        }

        while( TRUE) 
        {

            ULONG ulBytesRemainingInBuffer;
            int rc;

            //  If the user had requested only a single match and we have
            //  returned that, then we stop at this point.
            if( bReturnSingleEntry && ulNextEntry != 0) 
            {

                try_return( ntStatus);
            }

            //
            // Apply the name filter if there is one
            //

            if( pDirEntry != NULL &&
                !BooleanFlagOn( pCcb->Flags, CCB_FLAG_FULL_DIRECTORY_QUERY))
            {

                //
                // Only returning directories?
                //

                if( BooleanFlagOn( pCcb->Flags, CCB_FLAG_DIR_OF_DIRS_ONLY))
                {

                    if( !(pDirEntry->DirectoryEntry.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    {

                        if( pDirEntry->ListEntry.fLink != NULL)
                        {

                            pCcb->CurrentDirIndex = pDirEntry->DirectoryEntry.FileIndex;

                            pDirEntry = (AFSDirEntryCB *)pDirEntry->ListEntry.fLink;

                            continue;
                        }

                        pDirEntry = NULL;
                    }
                }
                else
                {

                    //
                    // Are we doing a wild card search?
                    //

                    if( BooleanFlagOn( pCcb->Flags, CCB_FLAG_MASK_CONTAINS_WILD_CARDS))
                    {

                        if( !FsRtlIsNameInExpression( &pCcb->MaskName,
                                                      &pDirEntry->DirectoryEntry.FileName,
                                                      TRUE,
                                                      NULL))
                        {

                            if( pDirEntry->ListEntry.fLink != NULL)
                            {

                                pCcb->CurrentDirIndex = pDirEntry->DirectoryEntry.FileIndex;

                                pDirEntry = (AFSDirEntryCB *)pDirEntry->ListEntry.fLink;

                                continue;
                            }

                            pDirEntry = NULL;
                        }
                    }
                    else
                    {

                        if( RtlCompareUnicodeString( &pDirEntry->DirectoryEntry.FileName,
                                                     &pCcb->MaskName,
                                                     FALSE))
                        {

                            //
                            // See if this is a match for a case insensitive search
                            //

                            if( RtlCompareUnicodeString( &pDirEntry->DirectoryEntry.FileName,
                                                         &pCcb->MaskName,
                                                         TRUE) == 0)
                            {

                                pBestMatchDirEntry = pDirEntry;
                            }

                            if( pDirEntry->ListEntry.fLink != NULL)
                            {

                                pCcb->CurrentDirIndex = pDirEntry->DirectoryEntry.FileIndex;

                                pDirEntry = (AFSDirEntryCB *)pDirEntry->ListEntry.fLink;

                                continue;
                            }

                            //
                            // If we have reached this point then we have no matches. If we have a 
                            // best match pointer then use it, otherwise bail
                            //

                            pDirEntry = pBestMatchDirEntry;
                        }                                                    
                    }
                }
            }

            if( pDirEntry == NULL)
            {

                if( ulNextEntry == 0)
                {

                    if( bInitialQuery && 
                        pCcb->MaskName.Buffer != NULL)
                    {
                        ntStatus = STATUS_NO_SUCH_FILE;
                    }
                    else
                    {
                        ntStatus = STATUS_NO_MORE_FILES;
                    }
                }

                try_return( ntStatus);
            }
            else if( pDirEntry->Fcb != NULL &&
                     ( BooleanFlagOn( pDirEntry->Fcb->Flags, AFS_FCB_PENDING_DELETE) ||
                       BooleanFlagOn( pDirEntry->Fcb->Flags, AFS_FCB_DELETED)))
            {
    
                pCcb->CurrentDirIndex = pDirEntry->DirectoryEntry.FileIndex;

                pDirEntry = (AFSDirEntryCB *)pDirEntry->ListEntry.fLink;
                                
                continue;
            }

            //
            // Be sure the information is valid
            // We don't worry about entries while enumerating the directory
            //

            AFSValidateEntry( pDirEntry,
                              FALSE,
                              TRUE);

            //  Here are the rules concerning filling up the buffer:
            //
            //  1.  The Io system garentees that there will always be
            //      enough room for at least one base record.
            //
            //  2.  If the full first record (including file name) cannot
            //      fit, as much of the name as possible is copied and
            //      STATUS_BUFFER_OVERFLOW is returned.
            //
            //  3.  If a subsequent record cannot completely fit into the
            //      buffer, none of it (as in 0 bytes) is copied, and
            //      STATUS_SUCCESS is returned.  A subsequent query will
            //      pick up with this record.

            ulBytesRemainingInBuffer = ulUserBufferLength - ulNextEntry;

            if( ( ulNextEntry != 0) &&
                ( ( ulBaseLength + pDirEntry->DirectoryEntry.FileName.Length > ulBytesRemainingInBuffer) ||
                  ( ulUserBufferLength < ulNextEntry) ) ) 
            {

                try_return( ntStatus = STATUS_SUCCESS);
            }

            //  Zero the base part of the structure.
            RtlZeroMemory( &pBuffer[ ulNextEntry], 
                           ulBaseLength);

            switch( FileInformationClass) 
            {

                //  Now fill the base parts of the strucure that are applicable.
                case FileIdBothDirectoryInformation:
                case FileBothDirectoryInformation:

                    pBothDirInfo = (PFILE_BOTH_DIR_INFORMATION)&pBuffer[ ulNextEntry];

                    pBothDirInfo->ShortNameLength = (CHAR)pDirEntry->DirectoryEntry.ShortNameLength;

                    if( pDirEntry->DirectoryEntry.ShortNameLength > 0)
                    {
                        RtlCopyMemory( &pBothDirInfo->ShortName[ 0],
                                       &pDirEntry->DirectoryEntry.ShortName[ 0],
                                       pBothDirInfo->ShortNameLength);
                    }
                case FileIdFullDirectoryInformation:
                case FileFullDirectoryInformation:

                    pFullDirInfo = (PFILE_FULL_DIR_INFORMATION)&pBuffer[ ulNextEntry];
                    pFullDirInfo->EaSize = 0;

                case FileDirectoryInformation:

                    pDirInfo = (PFILE_DIRECTORY_INFORMATION)&pBuffer[ ulNextEntry];

                    pDirInfo->CreationTime = pDirEntry->DirectoryEntry.CreationTime;
                    pDirInfo->LastWriteTime = pDirEntry->DirectoryEntry.LastWriteTime;
                    pDirInfo->LastAccessTime = pDirEntry->DirectoryEntry.LastAccessTime;
                    pDirInfo->ChangeTime = pDirEntry->DirectoryEntry.LastWriteTime;
                    pDirInfo->EndOfFile = pDirEntry->DirectoryEntry.EndOfFile;
                    pDirInfo->AllocationSize = pDirEntry->DirectoryEntry.AllocationSize;
                    
                    if( pDirEntry->DirectoryEntry.FileType != 0)
                    {

                        pDirInfo->FileAttributes = pDirEntry->DirectoryEntry.FileAttributes != 0 ?
                                                                               pDirEntry->DirectoryEntry.FileAttributes :
                                                                            FILE_ATTRIBUTE_NORMAL;
                    }
                    else
                    {

                        if( (pDirEntry->DirectoryEntry.FileId.Vnode % 2) != 0)
                        {

                            pDirInfo->FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                        }
                        else
                        {

                            pDirInfo->FileAttributes = FILE_ATTRIBUTE_NORMAL;
                        }
                    }

                    pDirInfo->FileIndex = pDirEntry->DirectoryEntry.FileIndex; 
                    pDirInfo->FileNameLength = pDirEntry->DirectoryEntry.FileName.Length;

                    break;

                case FileNamesInformation:

                    pNamesInfo = (PFILE_NAMES_INFORMATION)&pBuffer[ ulNextEntry];
                    pNamesInfo->FileIndex = pDirEntry->DirectoryEntry.FileIndex;
                    pNamesInfo->FileNameLength = pDirEntry->DirectoryEntry.FileName.Length;

                    break;

                default:

                    try_return( ntStatus = STATUS_INVALID_PARAMETER);

                    break;
            }

            ulBytesConverted = ulBytesRemainingInBuffer - ulBaseLength >= pDirEntry->DirectoryEntry.FileName.Length ?
                                            pDirEntry->DirectoryEntry.FileName.Length :
                                            ulBytesRemainingInBuffer - ulBaseLength;

            RtlCopyMemory( &pBuffer[ ulNextEntry + ulBaseLength],
                           pDirEntry->DirectoryEntry.FileName.Buffer,
                           ulBytesConverted);

            //  Set up the previous next entry offset
            *((PULONG)(&pBuffer[ ulLastEntry])) = ulNextEntry - ulLastEntry;

            //  And indicate how much of the user buffer we have currently
            //  used up.  
            Irp->IoStatus.Information = QuadAlign( Irp->IoStatus.Information) + ulBaseLength + ulBytesConverted;

            pCcb->CurrentDirIndex = pDirEntry->DirectoryEntry.FileIndex;

            //  Check for the case that a single entry doesn't fit.
            //  This should only get this far on the first entry.
            if( ulBytesConverted < pDirEntry->DirectoryEntry.FileName.Length) 
            {

                try_return( ntStatus = STATUS_BUFFER_OVERFLOW);
            }
            
            dStatus = STATUS_SUCCESS;

            if( pDirEntry->ListEntry.fLink == NULL)
            {

                try_return( ntStatus = STATUS_SUCCESS);
            }

            //  Set ourselves up for the next iteration
            ulLastEntry = ulNextEntry;
            ulNextEntry += (ULONG)QuadAlign( ulBaseLength + ulBytesConverted);

            pDirEntry = (AFSDirEntryCB *)pDirEntry->ListEntry.fLink;
        }

try_exit:

        if( bReleaseMain)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }
    } 

    return ntStatus;
}

NTSTATUS
AFSNotifyChangeDirectory( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    PIO_STACK_LOCATION pIrpSp;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    ULONG ulCompletionFilter;
    BOOLEAN bWatchTree;
    BOOLEAN bReleaseLock = FALSE;

    __Enter
    {

        //  Get the current Stack location
        pIrpSp = IoGetCurrentIrpStackLocation( Irp );

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;
        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        if( pFcb->Header.NodeTypeCode != AFS_DIRECTORY_FCB &&
            pFcb->Header.NodeTypeCode != AFS_ROOT_FCB)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //  Reference our input parameter to make things easier
        ulCompletionFilter = pIrpSp->Parameters.NotifyDirectory.CompletionFilter;
        bWatchTree = BooleanFlagOn( pIrpSp->Flags, SL_WATCH_TREE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNotifyChangeDirectory Acquiring Dcb lock %08lX EXCL %08lX\n",
                                                          &pFcb->NPFcb->Resource,
                                                          PsGetCurrentThread());

        AFSAcquireExcl( &pFcb->NPFcb->Resource,
                          TRUE);

        bReleaseLock = TRUE;

        //
        // Check if the ndoe has already been deleted
        //

        if( BooleanFlagOn( pFcb->Flags, AFS_FCB_DELETED))
        {

            try_return( ntStatus = STATUS_FILE_DELETED);
        }
		else if( BooleanFlagOn( pFcb->Flags, AFS_FCB_PENDING_DELETE)) 
		{
			
            try_return( ntStatus = STATUS_DELETE_PENDING);
        }

        //  Call the Fsrtl package to process the request.
        FsRtlNotifyFullChangeDirectory( pFcb->NPFcb->NotifySync,
								        &pFcb->NPFcb->DirNotifyList,
								        pCcb,
							            (PSTRING)&pCcb->FullFileName,
								        bWatchTree,
								        TRUE,
								        ulCompletionFilter,
								        Irp,
								        NULL,
								        NULL);

        ntStatus = STATUS_PENDING;

try_exit:

        if( bReleaseLock)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }
    }

    return ntStatus;
}