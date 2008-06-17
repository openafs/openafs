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

//try_exit:

        if( ntStatus != STATUS_PENDING)
        {

            AFSCompleteRequest( Irp,
                                  ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSDirControl\n");
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

    AFSDirEntryCB *pDirEntry = NULL;

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

            AFSAcquireExcl( &pFcb->NPFcb->Resource,
                            TRUE);
        }
        else
        {

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

            ClearFlag( pCcb->DirFlags, CCB_FLAG_DIR_OF_DIRS_ONLY);

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

                SetFlag( pCcb->DirFlags, CCB_FLAG_FULL_DIRECTORY_QUERY);
            }
            else
            {

                if( (( puniArgFileName->Length == sizeof(WCHAR)) &&
                     ( puniArgFileName->Buffer[0] == L'<')) ||
                    (( puniArgFileName->Length == 2*sizeof(WCHAR)) &&
                    ( RtlEqualMemory( puniArgFileName->Buffer, L"*.", 2*sizeof(WCHAR) ))))
                {

                    SetFlag( pCcb->DirFlags, CCB_FLAG_DIR_OF_DIRS_ONLY);
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

                RtlUpcaseUnicodeString( &pCcb->MaskName,
                                        puniArgFileName,
                                        FALSE);

                if( FsRtlDoesNameContainWildCards( puniArgFileName))
                {

                    SetFlag( pCcb->DirFlags, CCB_FLAG_MASK_CONTAINS_WILD_CARDS);
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
                
            pDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;

            while( TRUE)
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

                if( pDirEntry->ListEntry.fLink == NULL)
                {

                    pDirEntry = NULL;

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

            pDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;

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

                pDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;

                if( pDirEntry == NULL)
                {

                    try_return( ntStatus = STATUS_NO_SUCH_FILE);
                }
            }
            else
            {

                pDirEntry = pFcb->Specific.Directory.DirectoryNodeListHead;

                while( TRUE)
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

                    if( pDirEntry->ListEntry.fLink == NULL)
                    {

                        pDirEntry = NULL;

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
                !BooleanFlagOn( pCcb->DirFlags, CCB_FLAG_FULL_DIRECTORY_QUERY))
            {

                //
                // Only returning directories?
                //

                if( BooleanFlagOn( pCcb->DirFlags, CCB_FLAG_DIR_OF_DIRS_ONLY))
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

                    if( BooleanFlagOn( pCcb->DirFlags, CCB_FLAG_MASK_CONTAINS_WILD_CARDS))
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
                                                     TRUE))
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
            else if( pDirEntry->Type.Data.Fcb != NULL &&
                     ( BooleanFlagOn( pDirEntry->Type.Data.Fcb->Flags, AFS_FCB_PENDING_DELETE) ||
                       BooleanFlagOn( pDirEntry->Type.Data.Fcb->Flags, AFS_FCB_DELETED)))
            {
    
                pCcb->CurrentDirIndex = pDirEntry->DirectoryEntry.FileIndex;

                pDirEntry = (AFSDirEntryCB *)pDirEntry->ListEntry.fLink;
                                
                continue;
            }

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
                case FileBothDirectoryInformation:

                    pBothDirInfo = (PFILE_BOTH_DIR_INFORMATION)&pBuffer[ ulNextEntry];

                    pBothDirInfo->ShortNameLength = (CHAR)pDirEntry->DirectoryEntry.ShortNameLength;

                    if( pDirEntry->DirectoryEntry.ShortNameLength > 0)
                    {
                        RtlCopyMemory( &pBothDirInfo->ShortName[ 0],
                                       &pDirEntry->DirectoryEntry.ShortName[ 0],
                                       pBothDirInfo->ShortNameLength);
                    }

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
                    pDirInfo->FileAttributes = pDirEntry->DirectoryEntry.FileAttributes != 0 ?
                                                                           pDirEntry->DirectoryEntry.FileAttributes :
                                                                        FILE_ATTRIBUTE_NORMAL;

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
    UNICODE_STRING uniFullFileName;

    __Enter
    {

        //  Get the current Stack location
        pIrpSp = IoGetCurrentIrpStackLocation( Irp );

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;
        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        uniFullFileName.Buffer = NULL;
        uniFullFileName.Length = 0;

        if( pFcb->Header.NodeTypeCode != AFS_DIRECTORY_FCB ||
            pFcb->Header.NodeTypeCode != AFS_ROOT_FCB)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //  Reference our input parameter to make things easier
        ulCompletionFilter = pIrpSp->Parameters.NotifyDirectory.CompletionFilter;
        bWatchTree = BooleanFlagOn( pIrpSp->Flags, SL_WATCH_TREE);

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

        //
        // Grab the full name for the directory
        //

        ntStatus = AFSGetFullName( pFcb,
                                   &uniFullFileName);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //  Call the Fsrtl package to process the request.
        FsRtlNotifyFullChangeDirectory( pFcb->NPFcb->NotifySync,
								        &pFcb->NPFcb->DirNotifyList,
								        pCcb,
							            (PSTRING)&uniFullFileName,
								        bWatchTree,
								        TRUE,
								        ulCompletionFilter,
								        Irp,
								        NULL,
								        NULL);

        if( uniFullFileName.Buffer != NULL &&
            uniFullFileName.Length > sizeof( WCHAR))
        {

            ExFreePool( uniFullFileName.Buffer);
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