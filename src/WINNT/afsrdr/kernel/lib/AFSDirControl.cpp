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
AFSDirControl( IN PDEVICE_OBJECT LibDeviceObject,
               IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(LibDeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);

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
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSDirControl\n");

        AFSDumpTraceFilesFnc();
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
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    PIO_STACK_LOCATION pIrpSp;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    BOOLEAN bInitialQuery = FALSE;
    PUCHAR pBuffer;
    ULONG ulUserBufferLength;
    PUNICODE_STRING puniArgFileName = NULL;
    UNICODE_STRING uniTmpMaskName;
    WCHAR wchMaskBuffer[ 4];
    FILE_INFORMATION_CLASS FileInformationClass;
    ULONG ulFileIndex;
    BOOLEAN bRestartScan;
    BOOLEAN bReturnSingleEntry;
    BOOLEAN bIndexSpecified;
    ULONG ulNextEntry = 0;
    ULONG ulLastEntry = 0;
    PFILE_DIRECTORY_INFORMATION pDirInfo;
    PFILE_FULL_DIR_INFORMATION pFullDirInfo;
    PFILE_BOTH_DIR_INFORMATION pBothDirInfo;
    PFILE_NAMES_INFORMATION pNamesInfo;
    ULONG ulBaseLength;
    ULONG ulBytesConverted;
    AFSDirectoryCB *pDirEntry = NULL;
    BOOLEAN bReleaseMain = FALSE;
    BOOLEAN bReleaseFcb = FALSE;
    AFSFileInfoCB       stFileInfo;
    AFSObjectInfoCB *pObjectInfo = NULL;
    ULONG ulAdditionalAttributes = 0;
    LONG lCount;

    __Enter
    {

        //  Get the current Stack location
        pIrpSp = IoGetCurrentIrpStackLocation( Irp);

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;
        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        if( pFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueryDirectory Attempted access (%p) when pFcb == NULL\n",
                          Irp);

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        if( pFcb->Header.NodeTypeCode != AFS_DIRECTORY_FCB &&
            pFcb->Header.NodeTypeCode != AFS_ROOT_FCB &&
            pFcb->Header.NodeTypeCode != AFS_ROOT_ALL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueryDirectory Attempted access (%p) to non-directory Fcb %p NodeType %u\n",
                          Irp,
                          pFcb,
                          pFcb->Header.NodeTypeCode);

            pFcb = NULL;

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // Set the enumeration event ...
        //

        AFSSetEnumerationEvent( pFcb);

        //  Reference our input parameters to make things easier
        ulUserBufferLength = pIrpSp->Parameters.QueryDirectory.Length;

        FileInformationClass = pIrpSp->Parameters.QueryDirectory.FileInformationClass;
        ulFileIndex = pIrpSp->Parameters.QueryDirectory.FileIndex;

        puniArgFileName = (PUNICODE_STRING)pIrpSp->Parameters.QueryDirectory.FileName;

        bRestartScan       = BooleanFlagOn( pIrpSp->Flags, SL_RESTART_SCAN);
        bReturnSingleEntry = BooleanFlagOn( pIrpSp->Flags, SL_RETURN_SINGLE_ENTRY);
        bIndexSpecified    = BooleanFlagOn( pIrpSp->Flags, SL_INDEX_SPECIFIED);

        bInitialQuery = (BOOLEAN)( !BooleanFlagOn( pCcb->Flags, CCB_FLAG_DIRECTORY_QUERY_MAPPED));

        if( bInitialQuery)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryDirectory Enumerating content for parent %wZ Initial Query\n",
                          &pCcb->DirectoryCB->NameInformation.FileName);

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryDirectory Acquiring Dcb lock %p EXCL %08lX\n",
                          &pFcb->NPFcb->Resource,
                          PsGetCurrentThread());

            AFSAcquireExcl( &pFcb->NPFcb->Resource,
                            TRUE);

            bReleaseFcb = TRUE;
        }
        else
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryDirectory Enumerating content for parent %wZ Subsequent\n",
                          &pCcb->DirectoryCB->NameInformation.FileName);

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryDirectory Acquiring Dcb lock %p SHARED %08lX\n",
                          &pFcb->NPFcb->Resource,
                          PsGetCurrentThread());

            AFSAcquireShared( &pFcb->NPFcb->Resource,
                              TRUE);

            bReleaseFcb = TRUE;
        }

        //
        // Grab the directory node hdr tree lock while parsing the directory
        // contents
        //

        AFSAcquireExcl( pFcb->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        bReleaseMain = TRUE;

        //
        // Before attempting to insert the new entry, check if we need to validate the parent
        //

        if( BooleanFlagOn( pFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryDirectory Verifying parent %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                          &pCcb->DirectoryCB->NameInformation.FileName,
                          pFcb->ObjectInformation->FileId.Cell,
                          pFcb->ObjectInformation->FileId.Volume,
                          pFcb->ObjectInformation->FileId.Vnode,
                          pFcb->ObjectInformation->FileId.Unique);

            ntStatus = AFSVerifyEntry( &pCcb->AuthGroup,
                                       pCcb->DirectoryCB);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSQueryDirectory Failed to verify parent %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                              &pCcb->DirectoryCB->NameInformation.FileName,
                              pFcb->ObjectInformation->FileId.Cell,
                              pFcb->ObjectInformation->FileId.Volume,
                              pFcb->ObjectInformation->FileId.Vnode,
                              pFcb->ObjectInformation->FileId.Unique,
                              ntStatus);

                try_return( ntStatus);
            }

            //
            // Perform a new snapshot of the directory
            //

            AFSAcquireExcl( &pCcb->NPCcb->CcbLock,
                            TRUE);

            ntStatus = AFSSnapshotDirectory( pFcb,
                                             pCcb,
                                             FALSE);

            AFSReleaseResource( &pCcb->NPCcb->CcbLock);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSQueryDirectory Snapshot directory failure for parent %wZ Mask %wZ Status %08lX\n",
                              &pCcb->DirectoryCB->NameInformation.FileName,
                              &pCcb->MaskName,
                              ntStatus);

                try_return( ntStatus);
            }
        }

        AFSConvertToShared( pFcb->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

        //
        // We can now safely drop the lock on the node
        //

        AFSReleaseResource( &pFcb->NPFcb->Resource);

        bReleaseFcb = FALSE;

        //
        // Start processing the data
        //

        pBuffer = (PUCHAR)AFSLockSystemBuffer( Irp,
                                               ulUserBufferLength);

        if( pBuffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSAcquireExcl( &pCcb->NPCcb->CcbLock,
                        TRUE);

        // Check if initial on this map
        if( bInitialQuery)
        {

            ntStatus = AFSSnapshotDirectory( pFcb,
                                             pCcb,
                                             TRUE);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSReleaseResource( &pCcb->NPCcb->CcbLock);

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSQueryDirectory Snapshot directory failure for parent %wZ Mask %wZ Status %08lX\n",
                              &pCcb->DirectoryCB->NameInformation.FileName,
                              &pCcb->MaskName,
                              ntStatus);

                try_return( ntStatus);
            }

            SetFlag( pCcb->Flags, CCB_FLAG_DIRECTORY_QUERY_MAPPED);

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

                pCcb->MaskName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                           pCcb->MaskName.Length,
                                                                           AFS_GENERIC_MEMORY_6_TAG);

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

                    if( FsRtlIsNameInExpression( &pCcb->MaskName,
                                                 &AFSPIOCtlName,
                                                 TRUE,
                                                 NULL))
                    {
                        SetFlag( pCcb->Flags, CCB_FLAG_MASK_PIOCTL_QUERY);
                    }
                }
                else
                {

                    RtlCopyMemory( pCcb->MaskName.Buffer,
                                   puniArgFileName->Buffer,
                                   pCcb->MaskName.Length);

                    if( RtlCompareUnicodeString( &AFSPIOCtlName,
                                                 &pCcb->MaskName,
                                                 TRUE) == 0)
                    {
                        SetFlag( pCcb->Flags, CCB_FLAG_MASK_PIOCTL_QUERY);
                    }
                }

                if( BooleanFlagOn( pCcb->Flags, CCB_FLAG_MASK_PIOCTL_QUERY))
                {
                    if( pFcb->ObjectInformation->Specific.Directory.PIOCtlDirectoryCB == NULL)
                    {

                        AFSReleaseResource( &pCcb->NPCcb->CcbLock);

                        AFSReleaseResource( pFcb->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

                        bReleaseMain = FALSE;

                        AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                        TRUE);

                        if( pFcb->ObjectInformation->Specific.Directory.PIOCtlDirectoryCB == NULL)
                        {

                            ntStatus = AFSInitPIOCtlDirectoryCB( pFcb->ObjectInformation);

                            if( !NT_SUCCESS( ntStatus))
                            {

                                AFSReleaseResource( &pFcb->NPFcb->Resource);

                                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                              AFS_TRACE_LEVEL_ERROR,
                                              "AFSQueryDirectory Init PIOCtl directory failure for parent %wZ Mask %wZ Status %08lX\n",
                                              &pCcb->DirectoryCB->NameInformation.FileName,
                                              &pCcb->MaskName,
                                              ntStatus);

                                try_return( ntStatus);
                            }
                        }

                        AFSAcquireShared( pFcb->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                                          TRUE);

                        bReleaseMain = TRUE;

                        AFSReleaseResource( &pFcb->NPFcb->Resource);

                        AFSAcquireExcl( &pCcb->NPCcb->CcbLock,
                                        TRUE);
                    }
                }

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSQueryDirectory Enumerating content for parent %wZ Mask %wZ\n",
                              &pCcb->DirectoryCB->NameInformation.FileName,
                              &pCcb->MaskName);
            }
        }

        // Check if we need to start from index
        if( bIndexSpecified)
        {

            //
            // Need to set up the initial point for the query
            //

            pCcb->CurrentDirIndex = ulFileIndex - 1;
        }

        // Check if we need to restart the scan
        else if( bRestartScan)
        {

            //
            // Reset the current scan processing
            //

            if( BooleanFlagOn( pCcb->Flags, CCB_FLAG_RETURN_RELATIVE_ENTRIES))
            {

                pCcb->CurrentDirIndex = AFS_DIR_ENTRY_INITIAL_DIR_INDEX;
            }
            else
            {

                pCcb->CurrentDirIndex = AFS_DIR_ENTRY_INITIAL_ROOT_INDEX;
            }
        }

        AFSReleaseResource( &pCcb->NPCcb->CcbLock);

        AFSReleaseResource( pFcb->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

        bReleaseMain = FALSE;

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

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSQueryDirectory (%p) Unknown FileInformationClass %u\n",
                              Irp,
                              FileInformationClass);

                try_return( ntStatus = STATUS_INVALID_INFO_CLASS);
        }

        while( TRUE)
        {

            ULONG ulBytesRemainingInBuffer;

            ulAdditionalAttributes = 0;

            //
            //  If the user had requested only a single match and we have
            //  returned that, then we stop at this point.
            //

            if( bReturnSingleEntry && ulNextEntry != 0)
            {

                try_return( ntStatus);
            }

            pDirEntry = AFSLocateNextDirEntry( pFcb->ObjectInformation,
                                               pCcb);

            if( pDirEntry == NULL)
            {

                if( ulNextEntry == 0)
                {

                    if( ( bInitialQuery ||
                          bRestartScan) &&
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

            //
            // Skip the entry if it is pending delete or deleted
            //

            else if( BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_PENDING_DELETE) ||
                     BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_DELETED))
            {

                lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSQueryDirectory Decrement count on %wZ DE %p Ccb %p Cnt %d\n",
                              &pDirEntry->NameInformation.FileName,
                              pDirEntry,
                              pCcb,
                              lCount);

                ASSERT( lCount >= 0);

                continue;
            }

            pObjectInfo = pDirEntry->ObjectInformation;

            //
            // Apply the name filter if there is one
            //

            if( !BooleanFlagOn( pCcb->Flags, CCB_FLAG_FULL_DIRECTORY_QUERY))
            {

                //
                // Only returning directories?
                //

                if( BooleanFlagOn( pCcb->Flags, CCB_FLAG_DIR_OF_DIRS_ONLY))
                {

                    if( !FlagOn( pObjectInfo->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
                    {

                        lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSQueryDirectory Decrement2 count on %wZ DE %p Ccb %p Cnt %d\n",
                                      &pDirEntry->NameInformation.FileName,
                                      pDirEntry,
                                      pCcb,
                                      lCount);

                        ASSERT( lCount >= 0);

                        continue;
                    }
                }
                else if( !BooleanFlagOn( pCcb->Flags, CCB_FLAG_MASK_PIOCTL_QUERY))
                {

                    //
                    // Are we doing a wild card search?
                    //

                    if( BooleanFlagOn( pCcb->Flags, CCB_FLAG_MASK_CONTAINS_WILD_CARDS))
                    {

                        if( !FsRtlIsNameInExpression( &pCcb->MaskName,
                                                      &pDirEntry->NameInformation.FileName,
                                                      TRUE,
                                                      NULL))
                        {

                            lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                            AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                          AFS_TRACE_LEVEL_VERBOSE,
                                          "AFSQueryDirectory Decrement3 count on %wZ DE %p Ccb %p Cnt %d\n",
                                          &pDirEntry->NameInformation.FileName,
                                          pDirEntry,
                                          pCcb,
                                          lCount);

                            ASSERT( lCount >= 0);

                            continue;
                        }
                    }
                    else
                    {

                        if( RtlCompareUnicodeString( &pDirEntry->NameInformation.FileName,
                                                     &pCcb->MaskName,
                                                     FALSE))
                        {

                            //
                            // See if this is a match for a case insensitive search
                            //

                            if( RtlCompareUnicodeString( &pDirEntry->NameInformation.FileName,
                                                         &pCcb->MaskName,
                                                         TRUE))
                            {

                                lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                              AFS_TRACE_LEVEL_VERBOSE,
                                              "AFSQueryDirectory Decrement4 count on %wZ DE %p Ccb %p Cnt %d\n",
                                              &pDirEntry->NameInformation.FileName,
                                              pDirEntry,
                                              pCcb,
                                              lCount);

                                ASSERT( lCount >= 0);

                                continue;
                            }
                        }
                    }
                }
            }

            //
            // Be sure the information is valid
            // We don't worry about entries while enumerating the directory
            //

            if ( BooleanFlagOn( pDirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY))
            {

                ntStatus = AFSValidateEntry( pDirEntry,
                                             &pCcb->AuthGroup,
                                             FALSE,
                                             FALSE);
                if ( NT_SUCCESS( ntStatus))
                {

                    ClearFlag( pDirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY);
                }
                else
                {

                    ntStatus = STATUS_SUCCESS;
                }
            }

            pObjectInfo = pDirEntry->ObjectInformation;

            //  Here are the rules concerning filling up the buffer:
            //
            //  1.  The Io system guarantees that there will always be
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
                ( ( ulBaseLength + pDirEntry->NameInformation.FileName.Length > ulBytesRemainingInBuffer) ||
                  ( ulUserBufferLength < ulNextEntry) ) )
            {

                //
                // Back off our current index
                //

                pCcb->CurrentDirIndex--;

                lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSQueryDirectory Decrement5 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &pDirEntry->NameInformation.FileName,
                              pDirEntry,
                              pCcb,
                              lCount);

                ASSERT( lCount >= 0);

                try_return( ntStatus = STATUS_SUCCESS);
            }


            //
            // For Symlinks and Mount Points the reparse point attribute
            // must be associated with the directory entry.  In addition,
            // for Symlinks it must be determined if the target object is
            // a directory or not.  If so, the directory attribute must be
            // specified.  Mount points always refer to directories and
            // must have the directory attribute set.
            //

            switch( pObjectInfo->FileType)
            {

            case AFS_FILE_TYPE_MOUNTPOINT:
            case AFS_FILE_TYPE_DFSLINK:
            {

                ulAdditionalAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT;

                break;
            }

            case AFS_FILE_TYPE_SYMLINK:
            {

                //
                // Go grab the file information for this entry
                // No worries on failures since we will just display
                // pseudo information
                //

                RtlZeroMemory( &stFileInfo,
                               sizeof( AFSFileInfoCB));

                if( NT_SUCCESS( AFSRetrieveFileAttributes( pCcb->DirectoryCB,
                                                           pDirEntry,
                                                           &pCcb->FullFileName,
                                                           pCcb->NameArray,
                                                           &pCcb->AuthGroup,
                                                           &stFileInfo)))
                {

                    if ( stFileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {

                        ulAdditionalAttributes = FILE_ATTRIBUTE_DIRECTORY;
                    }
                }

                ulAdditionalAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;

                break;
            }
            }

            //
            // Check if the name begins with a . and we are hiding them
            //

            if( !BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_FAKE) &&
                pDirEntry->NameInformation.FileName.Buffer[ 0] == L'.' &&
                BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_HIDE_DOT_NAMES))
            {

                ulAdditionalAttributes |= FILE_ATTRIBUTE_HIDDEN;
            }


            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryDirectory Insert into parent %wZ Entry %wZ\n",
                          &pCcb->DirectoryCB->NameInformation.FileName,
                          &pDirEntry->NameInformation.FileName);

            //  Zero the base part of the structure.
            RtlZeroMemory( &pBuffer[ ulNextEntry],
                           ulBaseLength);

            switch( FileInformationClass)
            {

                //  Now fill the base parts of the structure that are applicable.
                case FileIdBothDirectoryInformation:
                case FileBothDirectoryInformation:
                {
                    pBothDirInfo = (PFILE_BOTH_DIR_INFORMATION)&pBuffer[ ulNextEntry];

                    pBothDirInfo->ShortNameLength = (CHAR)pDirEntry->NameInformation.ShortNameLength;

                    if( pDirEntry->NameInformation.ShortNameLength > 0)
                    {
                        RtlCopyMemory( &pBothDirInfo->ShortName[ 0],
                                       &pDirEntry->NameInformation.ShortName[ 0],
                                       pBothDirInfo->ShortNameLength);
                    }
                }
                case FileIdFullDirectoryInformation:
                case FileFullDirectoryInformation:
                {
                    pFullDirInfo = (PFILE_FULL_DIR_INFORMATION)&pBuffer[ ulNextEntry];
                    pFullDirInfo->EaSize = 0;
                }
                case FileDirectoryInformation:
                {
                    pDirInfo = (PFILE_DIRECTORY_INFORMATION)&pBuffer[ ulNextEntry];

                    if( BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_FAKE))
                    {

                        pDirInfo->CreationTime = pFcb->ObjectInformation->CreationTime;
                        pDirInfo->LastWriteTime = pFcb->ObjectInformation->LastWriteTime;
                        pDirInfo->LastAccessTime = pFcb->ObjectInformation->LastAccessTime;
                        pDirInfo->ChangeTime = pFcb->ObjectInformation->ChangeTime;

                        pDirInfo->EndOfFile = pFcb->ObjectInformation->EndOfFile;
                        pDirInfo->AllocationSize = pFcb->ObjectInformation->AllocationSize;

                        if( BooleanFlagOn( pCcb->Flags, CCB_FLAG_MASK_PIOCTL_QUERY))
                        {
                            pDirInfo->FileAttributes = pObjectInfo->FileAttributes;
                        }
                        else
                        {
                            pDirInfo->FileAttributes = pFcb->ObjectInformation->FileAttributes;
                        }
                    }
                    else
                    {

                        pDirInfo->CreationTime = pObjectInfo->CreationTime;
                        pDirInfo->LastWriteTime = pObjectInfo->LastWriteTime;
                        pDirInfo->LastAccessTime = pObjectInfo->LastAccessTime;
                        pDirInfo->ChangeTime = pObjectInfo->ChangeTime;

                        pDirInfo->EndOfFile = pObjectInfo->EndOfFile;
                        pDirInfo->AllocationSize = pObjectInfo->AllocationSize;

                        if ( ulAdditionalAttributes && pObjectInfo->FileAttributes == FILE_ATTRIBUTE_NORMAL)
                        {

                            pDirInfo->FileAttributes = ulAdditionalAttributes;
                        }
                        else
                        {

                            pDirInfo->FileAttributes = pObjectInfo->FileAttributes | ulAdditionalAttributes;
                        }
                    }

                    pDirInfo->FileIndex = pDirEntry->FileIndex;
                    pDirInfo->FileNameLength = pDirEntry->NameInformation.FileName.Length;

                    break;
                }

                case FileNamesInformation:
                {
                    pNamesInfo = (PFILE_NAMES_INFORMATION)&pBuffer[ ulNextEntry];
                    pNamesInfo->FileIndex = pDirEntry->FileIndex;
                    pNamesInfo->FileNameLength = pDirEntry->NameInformation.FileName.Length;

                    break;
                }
                default:
                {
                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSQueryDirectory (%p) Unknown FileInformationClass %u\n",
                                  Irp,
                                  FileInformationClass);

                    lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSQueryDirectory Decrement6 count on %wZ DE %p Ccb %p Cnt %d\n",
                                  &pDirEntry->NameInformation.FileName,
                                  pDirEntry,
                                  pCcb,
                                  lCount);

                    ASSERT( lCount >= 0);

                    try_return( ntStatus = STATUS_INVALID_INFO_CLASS);

                    break;
                }
            }

            ulBytesConverted = ulBytesRemainingInBuffer - ulBaseLength >= pDirEntry->NameInformation.FileName.Length ?
                                            pDirEntry->NameInformation.FileName.Length :
                                            ulBytesRemainingInBuffer - ulBaseLength;

            RtlCopyMemory( &pBuffer[ ulNextEntry + ulBaseLength],
                           pDirEntry->NameInformation.FileName.Buffer,
                           ulBytesConverted);

            //  Set up the previous next entry offset
            *((PULONG)(&pBuffer[ ulLastEntry])) = ulNextEntry - ulLastEntry;

            //  And indicate how much of the user buffer we have currently
            //  used up.
            Irp->IoStatus.Information = QuadAlign( Irp->IoStatus.Information) + ulBaseLength + ulBytesConverted;

            //  Check for the case that a single entry doesn't fit.
            //  This should only get this far on the first entry.
            if( ulBytesConverted < pDirEntry->NameInformation.FileName.Length)
            {

                lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSQueryDirectory Decrement7 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &pDirEntry->NameInformation.FileName,
                              pDirEntry,
                              pCcb,
                              lCount);

                ASSERT( lCount >= 0);

                try_return( ntStatus = STATUS_BUFFER_OVERFLOW);
            }

            lCount = InterlockedDecrement( &pDirEntry->DirOpenReferenceCount);

            AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryDirectory Decrement8 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pDirEntry->NameInformation.FileName,
                          pDirEntry,
                          pCcb,
                          lCount);

            ASSERT( lCount >= 0);

            dStatus = STATUS_SUCCESS;

            //  Set ourselves up for the next iteration
            ulLastEntry = ulNextEntry;
            ulNextEntry += (ULONG)QuadAlign( ulBaseLength + ulBytesConverted);
        }

try_exit:

        if( bReleaseMain)
        {

            AFSReleaseResource( pFcb->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);
        }

        if ( bReleaseFcb)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }

        if( pFcb != NULL)
        {

            AFSClearEnumerationEvent( pFcb);
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

        if( pFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNotifyChangeDirectory Attempted access (%p) when pFcb == NULL\n",
                          Irp);

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        if( pFcb->Header.NodeTypeCode != AFS_DIRECTORY_FCB &&
            pFcb->Header.NodeTypeCode != AFS_ROOT_FCB &&
            pFcb->Header.NodeTypeCode != AFS_ROOT_ALL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSNotifyChangeDirectory NodeTypeCode !AFS_DIRECTORY_FCB && !AFS_ROOT_FCB %wZ NodeTypeCode 0x%x\n",
                          &pCcb->DirectoryCB->NameInformation.FileName,
                          pFcb->Header.NodeTypeCode);

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //  Reference our input parameter to make things easier
        ulCompletionFilter = pIrpSp->Parameters.NotifyDirectory.CompletionFilter;
        bWatchTree = BooleanFlagOn( pIrpSp->Flags, SL_WATCH_TREE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSNotifyChangeDirectory Acquiring Dcb lock %p EXCL %08lX\n",
                      &pFcb->NPFcb->Resource,
                      PsGetCurrentThread());

        AFSAcquireExcl( &pFcb->NPFcb->Resource,
                          TRUE);

        bReleaseLock = TRUE;

        //
        // Check if the node has already been deleted
        //

        if( BooleanFlagOn( pFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_DELETED))
        {

            try_return( ntStatus = STATUS_FILE_DELETED);
        }
        else if( BooleanFlagOn( pCcb->DirectoryCB->Flags, AFS_DIR_ENTRY_PENDING_DELETE))
        {

            try_return( ntStatus = STATUS_DELETE_PENDING);
        }

        //  Call the Fsrtl package to process the request.
        ntStatus = AFSFsRtlNotifyFullChangeDirectory( pFcb->ObjectInformation,
                                                      pCcb,
                                                      bWatchTree,
                                                      ulCompletionFilter,
                                                      Irp);

        if( !NT_SUCCESS( ntStatus))
        {
            try_return( ntStatus);
        }

        ntStatus = STATUS_PENDING;

try_exit:

        if( bReleaseLock)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }
    }

    return ntStatus;
}

AFSDirectoryCB *
AFSLocateNextDirEntry( IN AFSObjectInfoCB *ObjectInfo,
                       IN AFSCcb *Ccb)
{

    AFSDirectoryCB *pDirEntry = NULL;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSSnapshotHdr *pSnapshotHdr = NULL;
    AFSSnapshotEntry *pSnapshotEntry = NULL;
    ULONG ulCount = 0;
    LONG lCount;

    __Enter
    {

        AFSAcquireShared( ObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock,
                          TRUE);

        AFSAcquireExcl( &Ccb->NPCcb->CcbLock,
                        TRUE);

        //
        // Is this a PIOCtl query
        //

        if( BooleanFlagOn( Ccb->Flags, CCB_FLAG_MASK_PIOCTL_QUERY))
        {

            if( Ccb->CurrentDirIndex == (ULONG)AFS_DIR_ENTRY_INITIAL_DIR_INDEX ||
                Ccb->CurrentDirIndex == (ULONG)AFS_DIR_ENTRY_INITIAL_ROOT_INDEX)
            {

                pDirEntry = ObjectInfo->Specific.Directory.PIOCtlDirectoryCB;

                if( pDirEntry != NULL)
                {

                    lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSLocateNextDirEntry Increment count on %wZ DE %p Ccb %p Cnt %d\n",
                                  &pDirEntry->NameInformation.FileName,
                                  pDirEntry,
                                  Ccb,
                                  lCount);

                    ASSERT( lCount >= 0);
                }

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNextDirEntry Returning PIOctl entry %wZ in parent FID %08lX-%08lX-%08lX-%08lX\n",
                              &pDirEntry->NameInformation.FileName,
                              ObjectInfo->FileId.Cell,
                              ObjectInfo->FileId.Volume,
                              ObjectInfo->FileId.Vnode,
                              ObjectInfo->FileId.Unique);
            }

            Ccb->CurrentDirIndex++;

            try_return( ntStatus);
        }

        Ccb->CurrentDirIndex++;

        if( Ccb->CurrentDirIndex == (ULONG)AFS_DIR_ENTRY_DOT_INDEX)
        {

            //
            // Return the .. entry
            //

            pDirEntry = AFSGlobalDotDirEntry;

            if( pDirEntry != NULL)
            {

                lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNextDirEntry Increment2 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &pDirEntry->NameInformation.FileName,
                              pDirEntry,
                              Ccb,
                              lCount);

                ASSERT( lCount >= 0);
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSLocateNextDirEntry Returning1 snapshot entry %wZ in parent FID %08lX-%08lX-%08lX-%08lX\n",
                          &pDirEntry->NameInformation.FileName,
                          ObjectInfo->FileId.Cell,
                          ObjectInfo->FileId.Volume,
                          ObjectInfo->FileId.Vnode,
                          ObjectInfo->FileId.Unique);
        }
        else if( Ccb->CurrentDirIndex == (ULONG)AFS_DIR_ENTRY_DOT_DOT_INDEX)
        {

            //
            // Return the .. entry
            //

            pDirEntry = AFSGlobalDotDotDirEntry;

            if( pDirEntry != NULL)
            {

                lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

                AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNextDirEntry Increment3 count on %wZ DE %p Ccb %p Cnt %d\n",
                              &pDirEntry->NameInformation.FileName,
                              pDirEntry,
                              Ccb,
                              lCount);

                ASSERT( lCount >= 0);
            }

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSLocateNextDirEntry Returning2 snapshot entry %wZ in parent FID %08lX-%08lX-%08lX-%08lX\n",
                          &pDirEntry->NameInformation.FileName,
                          ObjectInfo->FileId.Cell,
                          ObjectInfo->FileId.Volume,
                          ObjectInfo->FileId.Vnode,
                          ObjectInfo->FileId.Unique);
        }
        else
        {

            pSnapshotHdr = Ccb->DirectorySnapshot;

            if( pSnapshotHdr == NULL ||
                Ccb->CurrentDirIndex >= pSnapshotHdr->EntryCount)
            {

                try_return( ntStatus);
            }

            pSnapshotEntry = &pSnapshotHdr->TopEntry[ Ccb->CurrentDirIndex];

            ulCount = Ccb->CurrentDirIndex;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSLocateNextDirEntry CurrentDirIndex %08lX\n",
                          ulCount);

            //
            // Get to a valid entry
            //

            while( ulCount < pSnapshotHdr->EntryCount)
            {

                pDirEntry = NULL;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNextDirEntry Searching for hash %08lX\n",
                              pSnapshotEntry->NameHash);

                if( pSnapshotEntry->NameHash == 0)
                {

                    break;
                }

                ntStatus = AFSLocateCaseSensitiveDirEntry( ObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                           pSnapshotEntry->NameHash,
                                                           &pDirEntry);

                if( !NT_SUCCESS( ntStatus) ||
                    pDirEntry != NULL)
                {

                    if( pDirEntry != NULL)
                    {

                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNextDirEntry Returning3 snapshot entry %wZ (%08lX) in parent FID %08lX-%08lX-%08lX-%08lX\n",
                                      &pDirEntry->NameInformation.FileName,
                                      (ULONG)pDirEntry->CaseInsensitiveTreeEntry.HashIndex,
                                      ObjectInfo->FileId.Cell,
                                      ObjectInfo->FileId.Volume,
                                      ObjectInfo->FileId.Vnode,
                                      ObjectInfo->FileId.Unique);

                        lCount = InterlockedIncrement( &pDirEntry->DirOpenReferenceCount);

                        AFSDbgLogMsg( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNextDirEntry Increment4 count on %wZ DE %p Ccb %p Cnt %d\n",
                                      &pDirEntry->NameInformation.FileName,
                                      pDirEntry,
                                      Ccb,
                                      lCount);

                        ASSERT( lCount >= 0);
                    }
                    else
                    {


                        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSLocateNextDirEntry Returning3 NO snapshot entry in parent FID %08lX-%08lX-%08lX-%08lX\n",
                                      ObjectInfo->FileId.Cell,
                                      ObjectInfo->FileId.Volume,
                                      ObjectInfo->FileId.Vnode,
                                      ObjectInfo->FileId.Unique);
                    }

                    break;
                }

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLocateNextDirEntry Entry %08lX not found in parent FID %08lX-%08lX-%08lX-%08lX\n",
                              pSnapshotEntry->NameHash,
                              ObjectInfo->FileId.Cell,
                              ObjectInfo->FileId.Volume,
                              ObjectInfo->FileId.Vnode,
                              ObjectInfo->FileId.Unique);

                pSnapshotEntry++;

                ulCount++;

                Ccb->CurrentDirIndex++;
            }
        }

try_exit:

        AFSReleaseResource( &Ccb->NPCcb->CcbLock);

        AFSReleaseResource( ObjectInfo->Specific.Directory.DirectoryNodeHdr.TreeLock);
    }

    return pDirEntry;
}

AFSDirectoryCB *
AFSLocateDirEntryByIndex( IN AFSObjectInfoCB *ObjectInfo,
                          IN AFSCcb *Ccb,
                          IN ULONG DirIndex)
{

    AFSDirectoryCB *pDirEntry = NULL;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSSnapshotHdr *pSnapshotHdr = NULL;
    AFSSnapshotEntry *pSnapshotEntry = NULL;
    ULONG ulCount = 0;

    __Enter
    {

        AFSAcquireExcl( &Ccb->NPCcb->CcbLock,
                        TRUE);

        Ccb->CurrentDirIndex = DirIndex;

        if( DirIndex == (ULONG)AFS_DIR_ENTRY_DOT_INDEX)
        {

            //
            // Return the .. entry
            //

            pDirEntry = AFSGlobalDotDirEntry;
        }
        else if( DirIndex == (ULONG)AFS_DIR_ENTRY_DOT_DOT_INDEX)
        {

            //
            // Return the .. entry
            //

            pDirEntry = AFSGlobalDotDotDirEntry;
        }
        else
        {

            pSnapshotHdr = Ccb->DirectorySnapshot;

            if( pSnapshotHdr == NULL ||
                Ccb->CurrentDirIndex >= pSnapshotHdr->EntryCount)
            {

                try_return( ntStatus);
            }

            pSnapshotEntry = &pSnapshotHdr->TopEntry[ Ccb->CurrentDirIndex];

            ulCount = Ccb->CurrentDirIndex;

            //
            // Get to a valid entry
            //

            while( ulCount < pSnapshotHdr->EntryCount)
            {

                pDirEntry = NULL;

                ntStatus = AFSLocateCaseSensitiveDirEntry( ObjectInfo->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                           pSnapshotEntry->NameHash,
                                                           &pDirEntry);

                if( !NT_SUCCESS( ntStatus) ||
                    ( pDirEntry != NULL &&
                      pDirEntry->FileIndex == DirIndex))
                {

                    break;
                }

                pSnapshotEntry++;

                ulCount++;
            }

            if( pDirEntry != NULL)
            {

                Ccb->CurrentDirIndex = ulCount;
            }
        }

try_exit:

        AFSReleaseResource( &Ccb->NPCcb->CcbLock);
    }

    return pDirEntry;
}

NTSTATUS
AFSSnapshotDirectory( IN AFSFcb *Fcb,
                      IN AFSCcb *Ccb,
                      IN BOOLEAN ResetIndex)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSSnapshotHdr *pSnapshotHdr = NULL;
    AFSSnapshotEntry *pSnapshotEntry = NULL;
    AFSDirectoryCB *pDirEntry = NULL;

    __Enter
    {

        if( ResetIndex)
        {

            //
            // Set it up so we still get the . and .. entries for empty directories
            //

            if( BooleanFlagOn( Ccb->Flags, CCB_FLAG_RETURN_RELATIVE_ENTRIES))
            {

                Ccb->CurrentDirIndex = AFS_DIR_ENTRY_INITIAL_DIR_INDEX;
            }
            else
            {

                Ccb->CurrentDirIndex = AFS_DIR_ENTRY_INITIAL_ROOT_INDEX;
            }
        }

        if( Fcb->ObjectInformation->Specific.Directory.DirectoryNodeCount == 0)
        {

            //
            // If we have a snapshot then clear it out
            //

            if( Ccb->DirectorySnapshot != NULL)
            {

                AFSExFreePoolWithTag( Ccb->DirectorySnapshot, AFS_DIR_SNAPSHOT_TAG);

                Ccb->DirectorySnapshot = NULL;
            }

            try_return( ntStatus);
        }

        //
        // Allocate our snapshot buffer for this enumeration
        //

        pSnapshotHdr = (AFSSnapshotHdr *)AFSExAllocatePoolWithTag( PagedPool,
                                                                   sizeof( AFSSnapshotHdr) +
                                                                        ( Fcb->ObjectInformation->Specific.Directory.DirectoryNodeCount *
                                                                                sizeof( AFSSnapshotEntry)),
                                                                   AFS_DIR_SNAPSHOT_TAG);

        if( pSnapshotHdr == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pSnapshotHdr,
                       sizeof( AFSSnapshotHdr) +
                            ( Fcb->ObjectInformation->Specific.Directory.DirectoryNodeCount *
                                    sizeof( AFSSnapshotEntry)));

        pSnapshotHdr->EntryCount = Fcb->ObjectInformation->Specific.Directory.DirectoryNodeCount;

        pSnapshotHdr->TopEntry = (AFSSnapshotEntry *)((char *)pSnapshotHdr + sizeof( AFSSnapshotHdr));

        //
        // Populate our snapshot
        //

        pSnapshotEntry = pSnapshotHdr->TopEntry;

        pDirEntry = Fcb->ObjectInformation->Specific.Directory.DirectoryNodeListHead;

        while( pDirEntry != NULL)
        {

            if( !BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_DELETED) &&
                !BooleanFlagOn( pDirEntry->Flags, AFS_DIR_ENTRY_PENDING_DELETE) &&
                !AFSIsNameInSnapshot( pSnapshotHdr,
                                      (ULONG)pDirEntry->CaseSensitiveTreeEntry.HashIndex))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSnapshotDirectory Snapshot (%08lX) Inserting entry %wZ (%08lX) Flags %08lX in parent FID %08lX-%08lX-%08lX-%08lX\n",
                              pSnapshotHdr->EntryCount,
                              &pDirEntry->NameInformation.FileName,
                              (ULONG)pDirEntry->CaseSensitiveTreeEntry.HashIndex,
                              pDirEntry->Flags,
                              Fcb->ObjectInformation->FileId.Cell,
                              Fcb->ObjectInformation->FileId.Volume,
                              Fcb->ObjectInformation->FileId.Vnode,
                              Fcb->ObjectInformation->FileId.Unique);

                pSnapshotEntry->NameHash = (ULONG)pDirEntry->CaseSensitiveTreeEntry.HashIndex;

                pSnapshotEntry++;
            }
            else
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSnapshotDirectory Snapshot (%08lX) Skipping entry %wZ (%08lX) Flags %08lX in parent FID %08lX-%08lX-%08lX-%08lX\n",
                              pSnapshotHdr->EntryCount,
                              &pDirEntry->NameInformation.FileName,
                              (ULONG)pDirEntry->CaseSensitiveTreeEntry.HashIndex,
                              pDirEntry->Flags,
                              Fcb->ObjectInformation->FileId.Cell,
                              Fcb->ObjectInformation->FileId.Volume,
                              Fcb->ObjectInformation->FileId.Vnode,
                              Fcb->ObjectInformation->FileId.Unique);
            }

            pDirEntry = (AFSDirectoryCB *)pDirEntry->ListEntry.fLink;
        }

        if( Ccb->DirectorySnapshot != NULL)
        {

            AFSExFreePoolWithTag( Ccb->DirectorySnapshot, AFS_DIR_SNAPSHOT_TAG);

            Ccb->DirectorySnapshot = NULL;
        }

        Ccb->DirectorySnapshot = pSnapshotHdr;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSFsRtlNotifyFullChangeDirectory( IN AFSObjectInfoCB *ObjectInfo,
                                   IN AFSCcb *Ccb,
                                   IN BOOLEAN WatchTree,
                                   IN ULONG CompletionFilter,
                                   IN PIRP NotifyIrp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;
    size_t sztLength;

    __Enter
    {

        AFSAcquireExcl( &Ccb->NPCcb->CcbLock,
                        TRUE);

        //
        // Build a dir name based on the FID of the file
        //

        if( Ccb->NotifyMask.Buffer == NULL)
        {

            Ccb->NotifyMask.Length = 0;
            Ccb->NotifyMask.MaximumLength = 1024;

            Ccb->NotifyMask.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                        Ccb->NotifyMask.MaximumLength,
                                                                        AFS_GENERIC_MEMORY_7_TAG);

            if( Ccb->NotifyMask.Buffer == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            ntStatus = RtlStringCbPrintfW( Ccb->NotifyMask.Buffer,
                                           Ccb->NotifyMask.MaximumLength,
                                           L"\\%08lX.%08lX.%08lX.%08lX",
                                           ObjectInfo->FileId.Cell,
                                           ObjectInfo->FileId.Volume,
                                           ObjectInfo->FileId.Vnode,
                                           ObjectInfo->FileId.Unique);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }

            ntStatus = RtlStringCbLengthW( Ccb->NotifyMask.Buffer,
                                           (size_t)Ccb->NotifyMask.MaximumLength,
                                           &sztLength);

            if( !NT_SUCCESS( ntStatus))
            {

                try_return( ntStatus);
            }

            Ccb->NotifyMask.Length = (USHORT)sztLength;
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_DIR_NOTIF_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSFsRtlNotifyFullChangeDirectory Registering notification on %wZ Irp %p Filter %08lX Tree %02lX\n",
                      &Ccb->NotifyMask,
                      NotifyIrp,
                      CompletionFilter,
                      WatchTree);

        FsRtlNotifyFilterChangeDirectory( pDeviceExt->Specific.Control.NotifySync,
                                        &pDeviceExt->Specific.Control.DirNotifyList,
                                        (void *)Ccb,
                                        (PSTRING)&Ccb->NotifyMask,
                                        WatchTree,
                                        TRUE,
                                        CompletionFilter,
                                        NotifyIrp,
                                        NULL,
                                        NULL,
                                        NULL);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( Ccb->NotifyMask.Buffer != NULL)
            {

                AFSExFreePoolWithTag( Ccb->NotifyMask.Buffer, AFS_GENERIC_MEMORY_7_TAG);

                Ccb->NotifyMask.Buffer = NULL;
            }
        }

        AFSReleaseResource( &Ccb->NPCcb->CcbLock);
    }

    return ntStatus;
}

void
AFSFsRtlNotifyFullReportChange( IN AFSObjectInfoCB *ParentObjectInfo,
                                IN AFSCcb *Ccb,
                                IN ULONG NotifyFilter,
                                IN ULONG NotificationAction)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSControlDeviceObject->DeviceExtension;
    UNICODE_STRING uniName, uniComponentName;
    size_t sztLength;
    USHORT usNameOffset = 0;

    __Enter
    {

        uniName.Buffer = NULL;

        if( ParentObjectInfo == NULL ||
            AFSGlobalRoot == NULL)
        {

            try_return( ntStatus);
        }

        if( Ccb == NULL)
        {

            RtlInitUnicodeString( &uniComponentName,
                                  L"_AFSChange.dat");
        }
        else
        {

            uniComponentName = Ccb->DirectoryCB->NameInformation.FileName;
        }

        //
        // Build a dir name based on the FID of the file
        //

        uniName.Length = 0;
        uniName.MaximumLength = 1024 + uniComponentName.Length;

        uniName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                            uniName.MaximumLength,
                                                            AFS_GENERIC_MEMORY_8_TAG);

        if( uniName.Buffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        ntStatus = RtlStringCbPrintfW( uniName.Buffer,
                                       uniName.MaximumLength,
                                       L"\\%08lX.%08lX.%08lX.%08lX\\%wZ",
                                       ParentObjectInfo->FileId.Cell,
                                       ParentObjectInfo->FileId.Volume,
                                       ParentObjectInfo->FileId.Vnode,
                                       ParentObjectInfo->FileId.Unique,
                                       &uniComponentName);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        ntStatus = RtlStringCbLengthW( uniName.Buffer,
                                       (size_t)uniName.MaximumLength,
                                       &sztLength);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        uniName.Length = (USHORT)sztLength;

        usNameOffset = uniName.Length - uniComponentName.Length;

        AFSDbgLogMsg( AFS_SUBSYSTEM_DIR_NOTIF_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSFsRtlNotifyFullReportChange Notification call for %wZ Filter %08lX Action %08lX Offset %08lX Len %08lX CompLen %08lX\n",
                      &uniName,
                      NotifyFilter,
                      NotificationAction,
                      usNameOffset,
                      uniName.Length,
                      uniComponentName.Length);

        FsRtlNotifyFilterReportChange( pDeviceExt->Specific.Control.NotifySync,
                                       &pDeviceExt->Specific.Control.DirNotifyList,
                                       (PSTRING)&uniName,
                                       usNameOffset,
                                       NULL,
                                       NULL,
                                       NotifyFilter,
                                       NotificationAction,
                                       NULL,
                                       (void *)Ccb);

try_exit:

        if( uniName.Buffer != NULL)
        {

            AFSExFreePoolWithTag( uniName.Buffer, AFS_GENERIC_MEMORY_8_TAG);
        }
    }

    return;
}

// For use with FsRtlNotifyFilterChangeDirectory but must
// be implemented in the Framework because the library can
// be unloaded.

BOOLEAN
AFSNotifyReportChangeCallback( IN void *NotifyContext,
                               IN void *FilterContext)
{
    UNREFERENCED_PARAMETER(NotifyContext);
    UNREFERENCED_PARAMETER(FilterContext);
    BOOLEAN bReturn = TRUE;

    __Enter
    {

    }

    return bReturn;
}

BOOLEAN
AFSIsNameInSnapshot( IN AFSSnapshotHdr *SnapshotHdr,
                     IN ULONG HashIndex)
{

    BOOLEAN bIsInSnapshot = FALSE;
    AFSSnapshotEntry *pSnapshotEntry = SnapshotHdr->TopEntry;
    ULONG ulCount = 0;

    while( ulCount < SnapshotHdr->EntryCount)
    {

        if( pSnapshotEntry->NameHash == HashIndex)
        {

            bIsInSnapshot = TRUE;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSIsNameInSnapshot  Hash index %08lX already in snapshot\n",
                          HashIndex);

            break;
        }
        else if( pSnapshotEntry->NameHash == 0)
        {

            break;
        }

        pSnapshotEntry++;

        ulCount++;
    }

    return bIsInSnapshot;
}
