//
// File: AFSFileInfo.cpp
//

#include "AFSCommon.h"

//
// Function: AFSQueryFileInfo
//
// Description:
//
//      This function is the dispatch handler for the IRP_MJ_QUERY_FILE_INFORMATION request
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSQueryFileInfo( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    ULONG ulRequestType = 0;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    PFILE_OBJECT pFileObject;
    BOOLEAN bReleaseMain = FALSE;
    LONG lLength;
    FILE_INFORMATION_CLASS stFileInformationClass;
    PVOID pBuffer;

    __try
    {

        //
        // Determine the type of request this request is
        //

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        lLength = (LONG)pIrpSp->Parameters.QueryFile.Length;
        stFileInformationClass = pIrpSp->Parameters.QueryFile.FileInformationClass;
        pBuffer = Irp->AssociatedIrp.SystemBuffer;

        //
        // Grab the main shared right off the bat
        //

        AFSAcquireShared( &pFcb->NPFcb->Resource,
                            TRUE);

        bReleaseMain = TRUE;

        //
        // Don't allow requests against IOCtl nodes
        //

        if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        //
        // Process the request
        //

        switch( stFileInformationClass) 
        {

            case FileAllInformation:
            {

                PFILE_ALL_INFORMATION pAllInfo;

                //
                //  For the all information class we'll typecast a local
                //  pointer to the output buffer and then call the
                //  individual routines to fill in the buffer.
                //

                pAllInfo = (PFILE_ALL_INFORMATION)pBuffer;

                ntStatus = AFSQueryBasicInfo( Irp,
                                              pFcb, 
                                              &pAllInfo->BasicInformation, 
                                              &lLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                ntStatus = AFSQueryStandardInfo( Irp,
                                                 pFcb,
                                                 &pAllInfo->StandardInformation, 
                                                 &lLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                ntStatus = AFSQueryInternalInfo( Irp,
                                                 pFcb, 
                                                 &pAllInfo->InternalInformation, 
                                                 &lLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                ntStatus = AFSQueryEaInfo( Irp,
                                           pFcb,
                                           &pAllInfo->EaInformation, 
                                           &lLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                ntStatus = AFSQueryAccess( Irp,
                                           pFcb,
                                           &pAllInfo->AccessInformation,
                                           &lLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                ntStatus = AFSQueryPositionInfo( Irp,
                                                 pFcb,
                                                 &pAllInfo->PositionInformation, 
                                                 &lLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                ntStatus = AFSQueryMode( Irp,
                                         pFcb,
                                         &pAllInfo->ModeInformation,
                                         &lLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                ntStatus = AFSQueryAlignment( Irp,
                                              pFcb,
                                              &pAllInfo->AlignmentInformation,
                                              &lLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                ntStatus = AFSQueryNameInfo( Irp,
                                             pFcb, 
                                             &pAllInfo->NameInformation, 
                                             &lLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                break;
            }

            case FileBasicInformation:
            {

                AFSQueryBasicInfo( Irp,
                                   pFcb, 
                                   (PFILE_BASIC_INFORMATION)pBuffer, 
                                   &lLength);

                break;
            }

            case FileStandardInformation:
            {

                AFSQueryStandardInfo( Irp,
                                      pFcb,
                                      (PFILE_STANDARD_INFORMATION)pBuffer, 
                                      &lLength);

                break;
            }

            case FileInternalInformation:
            {

                AFSQueryInternalInfo( Irp,
                                      pFcb, 
                                      (PFILE_INTERNAL_INFORMATION)pBuffer, 
                                      &lLength);

                break;
            }

            case FileEaInformation:
            {

                AFSQueryEaInfo( Irp,
                                pFcb, 
                                (PFILE_EA_INFORMATION)pBuffer, 
                                &lLength);

                break;
            }

            case FilePositionInformation:
            {

                AFSQueryPositionInfo( Irp,
                                      pFcb,
                                      (PFILE_POSITION_INFORMATION)pBuffer, 
                                      &lLength);

                break;
            }

            case FileNameInformation:
            {

                AFSQueryNameInfo( Irp,
                                  pFcb,
                                  (PFILE_NAME_INFORMATION)pBuffer, 
                                  &lLength);

                break;
            }

            case FileAlternateNameInformation:
            {

                AFSQueryShortNameInfo( Irp,
                                       pFcb,
                                       (PFILE_NAME_INFORMATION)pBuffer, 
                                       &lLength);

                break;
            }

            case FileNetworkOpenInformation:
            {

                AFSQueryNetworkInfo( Irp,
                                     pFcb, 
                                     (PFILE_NETWORK_OPEN_INFORMATION)pBuffer, 
                                     &lLength);

                break;
            }

            default:

                ntStatus = STATUS_INVALID_PARAMETER;

                break;
        }

try_exit:

        if( NT_SUCCESS( ntStatus))
        {

            Irp->IoStatus.Information = pIrpSp->Parameters.QueryFile.Length - lLength;
        }

        if( bReleaseMain)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }

        if( !NT_SUCCESS( ntStatus) &&
            ntStatus != STATUS_INVALID_PARAMETER)
        {

            AFSPrint("AFSQueryFileInfo Failed to process request Status %08lX\n", ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSQueryFileInfo Main loop\n");

        ntStatus = STATUS_UNSUCCESSFUL;

        if( bReleaseMain)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }
    }

    AFSCompleteRequest( Irp,
                        ntStatus);

    return ntStatus;
}

//
// Function: AFSSetFileInfo
//
// Description:
//
//      This function is the dispatch handler for the IRP_MJ_SET_FILE_INFORMATION request
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSSetFileInfo( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    BOOLEAN bCompleteRequest = TRUE;
    FILE_INFORMATION_CLASS FileInformationClass;
    BOOLEAN bCanQueueRequest = FALSE;
    PFILE_OBJECT pFileObject = NULL;
    BOOLEAN bReleaseMain = FALSE;

    __try
    {

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        pFileObject = pIrpSp->FileObject;

        pFcb = (AFSFcb *)pFileObject->FsContext;
        pCcb = (AFSCcb *)pFileObject->FsContext2;

        bCanQueueRequest = !(IoIsOperationSynchronous( Irp) | (KeGetCurrentIrql() != PASSIVE_LEVEL));
        FileInformationClass = pIrpSp->Parameters.SetFile.FileInformationClass;
        
        //
        // Grab teh Fcb EXCL
        //

        AFSAcquireExcl( &pFcb->NPFcb->Resource,
                        TRUE);

        bReleaseMain = TRUE;

        //
        // Don't allow requests against IOCtl nodes
        //

        if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        //
        // Ensure rename operations are synchronous
        //

        if( FileInformationClass == FileRenameInformation)
        {

            bCanQueueRequest = FALSE;
        }

        FileInformationClass = pIrpSp->Parameters.SetFile.FileInformationClass;

        //
        // Process the request
        //

        switch( FileInformationClass) 
        {

            case FileBasicInformation:
            {

                ntStatus = AFSSetBasicInfo( Irp,
                                            pFcb);
                
                break;
            }

            case FileDispositionInformation:
            {

                ntStatus = AFSSetDispositionInfo( Irp, 
                                                  pFcb);
        
                break;
            }

            case FileRenameInformation:
            {

                ntStatus = AFSSetRenameInfo( DeviceObject,
                                             Irp,
                                             pFcb);

                break;
            }

            case FilePositionInformation:
            {
            
                ntStatus = AFSSetPositionInfo( Irp,
                                               pFcb);
           
                break;
            }

            case FileLinkInformation:
            {

                ntStatus = STATUS_INVALID_DEVICE_REQUEST;
            
                break;
            }

            case FileAllocationInformation:
            {
            
                ntStatus = AFSSetAllocationInfo( Irp, 
                                                 pFcb);
            
                break;
            }

            case FileEndOfFileInformation:
            {
            
                ntStatus = AFSSetEndOfFileInfo( Irp,
                                                pFcb);
            
                break;
            }

            default:

                ntStatus = STATUS_INVALID_PARAMETER;
                
                break;
        }

try_exit:

        if( bReleaseMain)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }


        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSSetFileInfo Failed to process request Status %08lX\n", ntStatus);
        }

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSSetFileInfo Processing loop\n");

        ntStatus = STATUS_UNSUCCESSFUL;
    }

    AFSCompleteRequest( Irp,
                        ntStatus);

    return ntStatus;
}

//
// Function: AFSQueryBasicInfo
//
// Description:
//
//      This function is the handler for the query basic information request
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSQueryBasicInfo( IN PIRP Irp,
                   IN AFSFcb *Fcb,
                   IN OUT PFILE_BASIC_INFORMATION Buffer,
                   IN OUT PLONG Length)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    ASSERT( Fcb->DirEntry != NULL);

    if( *Length >= sizeof( FILE_BASIC_INFORMATION))
    {

        RtlZeroMemory( Buffer, 
                       *Length);

        Buffer->CreationTime = Fcb->DirEntry->DirectoryEntry.CreationTime;
        Buffer->LastAccessTime = Fcb->DirEntry->DirectoryEntry.LastAccessTime;
        Buffer->LastWriteTime = Fcb->DirEntry->DirectoryEntry.LastWriteTime;
        Buffer->ChangeTime = Fcb->DirEntry->DirectoryEntry.ChangeTime;
        Buffer->FileAttributes = Fcb->DirEntry->DirectoryEntry.FileAttributes;

        *Length -= sizeof( FILE_BASIC_INFORMATION);

    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryStandardInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb,
                      IN OUT PFILE_STANDARD_INFORMATION Buffer,
                      IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if( *Length >= sizeof( FILE_STANDARD_INFORMATION))
    {

        RtlZeroMemory( Buffer, 
                       *Length);

        Buffer->AllocationSize = Fcb->DirEntry->DirectoryEntry.AllocationSize;
        Buffer->EndOfFile = Fcb->DirEntry->DirectoryEntry.EndOfFile;
        Buffer->NumberOfLinks = 0;
        Buffer->DeletePending = BooleanFlagOn( Fcb->Flags, AFS_FCB_PENDING_DELETE);
        Buffer->Directory = BooleanFlagOn( Fcb->DirEntry->DirectoryEntry.FileAttributes, FILE_ATTRIBUTE_DIRECTORY);

        *Length -= sizeof( FILE_STANDARD_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryInternalInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb,
                      IN OUT PFILE_INTERNAL_INFORMATION Buffer,
                      IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if( *Length >= sizeof( FILE_INTERNAL_INFORMATION))
    {

        //Buffer->IndexNumber.QuadPart = Fcb->DirEntry->DirectoryEntry.FileId.QuadPart;

        *Length -= sizeof( FILE_INTERNAL_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryEaInfo( IN PIRP Irp,
                IN AFSFcb *Fcb,
                IN OUT PFILE_EA_INFORMATION Buffer,
                IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if( *Length >= (LONG)Fcb->DirEntry->DirectoryEntry.EaSize)
    {

        Buffer->EaSize = Fcb->DirEntry->DirectoryEntry.EaSize;

        if( Fcb->DirEntry->DirectoryEntry.EaSize > 0)
        {

        }

        *Length -= sizeof( FILE_EA_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryPositionInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb,
                      IN OUT PFILE_POSITION_INFORMATION Buffer,
                      IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    if( *Length >= sizeof( FILE_POSITION_INFORMATION))
    {

        RtlZeroMemory( Buffer, 
                       *Length);

        Buffer->CurrentByteOffset.QuadPart = pIrpSp->FileObject->CurrentByteOffset.QuadPart;

        *Length -= sizeof( FILE_POSITION_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryAccess( IN PIRP Irp,
                IN AFSFcb *Fcb,
                IN OUT PFILE_ACCESS_INFORMATION Buffer,
                IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if( *Length >= sizeof( FILE_ACCESS_INFORMATION))
    {

        RtlZeroMemory( Buffer, 
                       *Length);

        Buffer->AccessFlags = 0;

        *Length -= sizeof( FILE_ACCESS_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryMode( IN PIRP Irp,
              IN AFSFcb *Fcb,
              IN OUT PFILE_MODE_INFORMATION Buffer,
              IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if( *Length >= sizeof( FILE_MODE_INFORMATION))
    {

        RtlZeroMemory( Buffer, 
                       *Length);

        Buffer->Mode = 0;

        *Length -= sizeof( FILE_MODE_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryAlignment( IN PIRP Irp,
                   IN AFSFcb *Fcb,
                   IN OUT PFILE_ALIGNMENT_INFORMATION Buffer,
                   IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    if( *Length >= sizeof( FILE_ALIGNMENT_INFORMATION))
    {

        RtlZeroMemory( Buffer, 
                       *Length);

        Buffer->AlignmentRequirement = 1;

        *Length -= sizeof( FILE_ALIGNMENT_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryNameInfo( IN PIRP Irp,
                  IN AFSFcb *Fcb,
                  IN OUT PFILE_NAME_INFORMATION Buffer,
                  IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulCopyLength = 0;

    if( *Length >= sizeof( FILE_NAME_INFORMATION))
    {

        RtlZeroMemory( Buffer, 
                       *Length);

        if( *Length >= (LONG)(sizeof( FILE_NAME_INFORMATION) + (LONG)Fcb->DirEntry->DirectoryEntry.FileName.Length - sizeof( WCHAR)))
        {

            ulCopyLength = (LONG)Fcb->DirEntry->DirectoryEntry.FileName.Length;
        }
        else
        {

            ulCopyLength = *Length - (sizeof( FILE_NAME_INFORMATION) - sizeof( WCHAR));
        }

        Buffer->FileNameLength = Fcb->DirEntry->DirectoryEntry.FileName.Length;

        if( ulCopyLength > 0)
        {

            RtlCopyMemory( Buffer->FileName,
                           Fcb->DirEntry->DirectoryEntry.FileName.Buffer,
                           ulCopyLength);
        }

        *Length -= sizeof( FILE_NAME_INFORMATION) + ulCopyLength - sizeof( WCHAR);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryShortNameInfo( IN PIRP Irp,
                       IN AFSFcb *Fcb,
                       IN OUT PFILE_NAME_INFORMATION Buffer,
                       IN OUT PLONG Length)
{
    
    NTSTATUS ntStatus = STATUS_SUCCESS;

    RtlZeroMemory( Buffer, 
                   *Length);

    if( Fcb->DirEntry->DirectoryEntry.ShortNameLength == 0)
    {

        //
        // The short name IS the long name
        //

        if( *Length >= (LONG)(sizeof( FILE_NAME_INFORMATION) + Fcb->DirEntry->DirectoryEntry.FileName.Length - sizeof( WCHAR)))
        {

            Buffer->FileNameLength = Fcb->DirEntry->DirectoryEntry.FileName.Length;

            RtlCopyMemory( Buffer->FileName,
                           Fcb->DirEntry->DirectoryEntry.FileName.Buffer,
                           Buffer->FileNameLength);

            *Length -= sizeof( FILE_NAME_INFORMATION) + Fcb->DirEntry->DirectoryEntry.FileName.Length - sizeof( WCHAR);
        }
        else
        {

            ntStatus = STATUS_BUFFER_OVERFLOW;
        }
    }
    else
    {

        if( *Length >= (LONG)(sizeof( FILE_NAME_INFORMATION) + Fcb->DirEntry->DirectoryEntry.ShortNameLength - sizeof( WCHAR)))
        {

            Buffer->FileNameLength = Fcb->DirEntry->DirectoryEntry.ShortNameLength;

            RtlCopyMemory( Buffer->FileName,
                           Fcb->DirEntry->DirectoryEntry.ShortName,
                           Buffer->FileNameLength);

            *Length -= sizeof( FILE_NAME_INFORMATION) + Fcb->DirEntry->DirectoryEntry.ShortNameLength - sizeof( WCHAR);
        }
        else
        {

            ntStatus = STATUS_BUFFER_OVERFLOW;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSQueryNetworkInfo( IN PIRP Irp,
                     IN AFSFcb *Fcb,
                     IN OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
                     IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    RtlZeroMemory( Buffer, 
                   *Length);

    if( *Length >= sizeof( FILE_NETWORK_OPEN_INFORMATION))
    {

        Buffer->CreationTime.QuadPart = Fcb->DirEntry->DirectoryEntry.CreationTime.QuadPart;
        Buffer->LastAccessTime.QuadPart = Fcb->DirEntry->DirectoryEntry.LastAccessTime.QuadPart;
        Buffer->LastWriteTime.QuadPart = Fcb->DirEntry->DirectoryEntry.LastWriteTime.QuadPart;
        Buffer->ChangeTime.QuadPart = Fcb->DirEntry->DirectoryEntry.ChangeTime.QuadPart;

        Buffer->AllocationSize.QuadPart = Fcb->DirEntry->DirectoryEntry.AllocationSize.QuadPart;
        Buffer->EndOfFile.QuadPart = Fcb->DirEntry->DirectoryEntry.EndOfFile.QuadPart;

        Buffer->FileAttributes = Fcb->DirEntry->DirectoryEntry.FileAttributes;

        *Length -= sizeof( FILE_NETWORK_OPEN_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSSetBasicInfo( IN PIRP Irp,
                 IN AFSFcb *Fcb)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_BASIC_INFORMATION pBuffer;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    ULONG ulNotifyFilter = 0;
    AFSFcb *pParentFcb = Fcb->ParentFcb;

    __Enter
    {

        if( pParentFcb == NULL)
        {

            pParentFcb = Fcb->RootFcb;
        }

        pBuffer = (PFILE_BASIC_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

        if( pBuffer->FileAttributes != (ULONGLONG)0)
        {

            if( Fcb->Header.NodeTypeCode == AFS_FILE_FCB &&
                BooleanFlagOn( pBuffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
            {

                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }
            else if( Fcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
            {

                pBuffer->FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
            }

            Fcb->DirEntry->DirectoryEntry.FileAttributes = pBuffer->FileAttributes;

            ulNotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;

            SetFlag( Fcb->Flags, AFS_FILE_MODIFIED);
        }

        if( pBuffer->CreationTime.QuadPart != (ULONGLONG)-1 &&
            pBuffer->CreationTime.QuadPart != (ULONGLONG)0)
        {

            Fcb->DirEntry->DirectoryEntry.CreationTime.QuadPart = pBuffer->CreationTime.QuadPart;

            ulNotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;

            SetFlag( Fcb->Flags, AFS_FILE_MODIFIED);
        }

        if( pBuffer->LastAccessTime.QuadPart != (ULONGLONG)-1 &&
            pBuffer->LastAccessTime.QuadPart != (ULONGLONG)0)
        {

            Fcb->DirEntry->DirectoryEntry.LastAccessTime.QuadPart = pBuffer->LastAccessTime.QuadPart;

            ulNotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;

            SetFlag( Fcb->Flags, AFS_FILE_MODIFIED);
        }

        if( pBuffer->LastWriteTime.QuadPart != (ULONGLONG)-1 &&
            pBuffer->LastWriteTime.QuadPart != (ULONGLONG)0)
        {

            Fcb->DirEntry->DirectoryEntry.LastWriteTime.QuadPart = pBuffer->LastWriteTime.QuadPart;

            ulNotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;

            SetFlag( Fcb->Flags, AFS_FILE_MODIFIED);
        }

        if( pBuffer->ChangeTime.QuadPart != (ULONGLONG)-1 &&
            pBuffer->ChangeTime.QuadPart != (ULONGLONG)0)
        {

            Fcb->DirEntry->DirectoryEntry.ChangeTime.QuadPart = pBuffer->ChangeTime.QuadPart;

            ulNotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;

            SetFlag( Fcb->Flags, AFS_FILE_MODIFIED);
        }

		if( ulNotifyFilter > 0)
		{

            UNICODE_STRING uniFullFileName;
            
            if( NT_SUCCESS( AFSGetFullName( Fcb,
                                              &uniFullFileName)))
            {

				FsRtlNotifyFullReportChange( pParentFcb->NPFcb->NotifySync,
											 &pParentFcb->NPFcb->DirNotifyList,
											 (PSTRING)&uniFullFileName,
											 (USHORT)(uniFullFileName.Length - Fcb->DirEntry->DirectoryEntry.FileName.Length),
											 (PSTRING)NULL,
											 (PSTRING)NULL,
											 (ULONG)ulNotifyFilter,
											 (ULONG)FILE_ACTION_MODIFIED,
											 (PVOID)NULL);

                if( uniFullFileName.Length > sizeof( WCHAR))
                {

                    ExFreePool( uniFullFileName.Buffer);
                }
			}
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSSetDispositionInfo( IN PIRP Irp,
                       IN AFSFcb *Fcb)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_DISPOSITION_INFORMATION pBuffer;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;

    __Enter
    {

        pBuffer = (PFILE_DISPOSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        //
        // Can't delete the root
        //

        if( pFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
        {

            try_return( ntStatus = STATUS_CANNOT_DELETE);
        }

        //
        // Check if this is a directory that there are not currently other opens
        //

        if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
        {

            if( pFcb->Specific.Directory.ChildOpenHandleCount > 0)
            {

                try_return( ntStatus = STATUS_DIRECTORY_NOT_EMPTY);
            }
        }

        if( pBuffer->DeleteFile)
        {

            //
            // If there is more than the open that is currently on the file then
            // don't allow it to be deleted
            //

            if( pFcb->OpenHandleCount > 1)
            {

                try_return( ntStatus = STATUS_CANNOT_DELETE);
            }

            if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
            {

                SetFlag( pFcb->Flags, AFS_FCB_PENDING_DELETE);
            }
            else
            {

                //
                // Attempt to flush any outstanding data
                //

                if( !MmFlushImageSection( &pFcb->NPFcb->SectionObjectPointers,
                                          MmFlushForDelete)) 
                {

                    AFSPrint("AFSSetDispositionInfo Failed to flush image section for delete\n");

                    ntStatus = STATUS_CANNOT_DELETE;
                }
                else
                {

                    //
                    // Purge the cache as well
                    //

                    if( pFcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL)
                    {

                        CcPurgeCacheSection( &pFcb->NPFcb->SectionObjectPointers,
                                             NULL,
                                             0,
                                             TRUE);
                    }

                    SetFlag( pFcb->Flags, AFS_FCB_PENDING_DELETE);
                }
            }

            if( !NT_SUCCESS( ntStatus))
            {

                AFSPrint("AFSSetDispositionInfo Failed to transition file to deleted state Status %08lX\n", ntStatus);
            }
        }
        else
        {

            ClearFlag( pFcb->Flags, AFS_FCB_PENDING_DELETE);
        }

        if( NT_SUCCESS( ntStatus))
        {

            //
            // OK, should be good to go, set the flag in the fo
            //

            pIrpSp->FileObject->DeletePending = pBuffer->DeleteFile;
        }

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSSetDispositionInfo Failed to process delete request Status %08lX\n", ntStatus);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSSetRenameInfo( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp,
                  IN AFSFcb *Fcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    IO_STATUS_BLOCK stIoSb = {0,0};
    AFSFcb *pSrcFcb = NULL, *pTargetDcb = NULL;
    AFSCcb *pSrcCcb = NULL, *pTargetDirCcb = NULL;
    PFILE_OBJECT pSrcFileObj = pIrpSp->FileObject;
    PFILE_OBJECT pTargetFileObj = pIrpSp->Parameters.SetFile.FileObject;
    PFILE_RENAME_INFORMATION pRenameInfo = NULL;
    UNICODE_STRING uniTargetName, uniSourceName;
    BOOLEAN bReplaceIfExists = FALSE;
    UNICODE_STRING uniShortName;

    __Enter
    {

        bReplaceIfExists = pIrpSp->Parameters.SetFile.ReplaceIfExists;

        pSrcFcb = (AFSFcb *)pSrcFileObj->FsContext;
        pSrcCcb = (AFSCcb *)pSrcFileObj->FsContext2;

        //
        // Perform some basic checks to ensure FS integrity
        //

        if( pSrcFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
        {

            //
            // Can't rename the root directory
            //

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        if( pSrcFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
        {

            //
            // If there are any open children then fail the rename
            //

            if( pSrcFcb->Specific.Directory.ChildOpenHandleCount > 0)
            {

                try_return( ntStatus = STATUS_ACCESS_DENIED);
            }
        }
        else 
        {

            if( pSrcFcb->OpenHandleCount > 1)
            {

                try_return( ntStatus = STATUS_ACCESS_DENIED);
            }
        }

        //
        // Resolve the target fileobject
        //

        if( pTargetFileObj == NULL) 
        {

            //
            // This is a simple rename. Here the target directory is the same as the source parent directory
            // and the name is retrieved from the system buffer information
            //         

            pRenameInfo = (PFILE_RENAME_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

            pTargetDcb = pSrcFcb->ParentFcb;

            uniTargetName.Length = (USHORT)pRenameInfo->FileNameLength;
            uniTargetName.Buffer = (PWSTR)&pRenameInfo->FileName;
        } 
        else 
        {

            //
            // So here we have the target directory taken from the targetfile object
            //

            pTargetDcb = (AFSFcb *)pTargetFileObj->FsContext;

            pTargetDirCcb = (AFSCcb *)pTargetFileObj->FsContext2;

            //
            // Check for 'cross-volume rename operations
            //

            if( pTargetDcb->DeviceObject != pSrcFcb->DeviceObject)
            {

                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }

            //
            // Grab the target name which we setup in the IRP_MJ_CREATE handler. By how we set this up
            // it is only the target component of the rename operation
            //

            uniTargetName = *((PUNICODE_STRING)&pTargetFileObj->FileName);
        }

        //
        // Extract off the final component name from the Fcb
        //

        uniSourceName.Length = (USHORT)pSrcFcb->DirEntry->DirectoryEntry.FileName.Length;
        uniSourceName.MaximumLength = uniSourceName.Length;

        uniSourceName.Buffer = pSrcFcb->DirEntry->DirectoryEntry.FileName.Buffer;

        //
        // The quick check to see if they are not really performing a rename
        // Do the names match?
        //

        if( FsRtlAreNamesEqual( &uniTargetName,
                                &uniSourceName,
                                TRUE,
                                NULL)) 
        {

            //
            // Check for case only rename
            //

            if( !FsRtlAreNamesEqual( &uniTargetName,
                                     &uniSourceName,
                                     FALSE,
                                     NULL)) 
            {

                //
                // Just move in the new case form of the name
                //

                RtlCopyMemory( pSrcFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                               uniTargetName.Buffer,
                               uniTargetName.Length);
            }

            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // We need to remove the DirEntry from the parent node, update the index
        // and reinsert it into the parent tree
        //

        AFSRemoveDirNodeFromParent( pSrcFcb->ParentFcb,
                                    pSrcFcb->DirEntry);

        //
        // OK, this is a simple rename. Issue the rename
        // request to the service.
        //

        ntStatus = AFSNotifyRename( pSrcFcb,
                                    pSrcFcb->ParentFcb,
                                    pTargetDcb,
                                    &uniTargetName);

        if( !NT_SUCCESS( ntStatus))
        {

            //
            // Attempt to re-insert the directory entry
            //

            AFSInsertDirectoryNode( pSrcFcb->ParentFcb,
                                    pSrcFcb->DirEntry);

            try_return( ntStatus);
        }

        //
        // If the name can fit into the current space in the
        // dir entry then just push it in, otherwise we need
        // to re-allocate the name buffer for the dir entry
        //

        if( uniTargetName.Length > pSrcFcb->DirEntry->DirectoryEntry.FileName.Length)
        {

            WCHAR *pTmpBuffer = NULL;

            if( BooleanFlagOn( pSrcFcb->DirEntry->Flags, AFS_DIR_RELEASE_NAME_BUFFER))
            {

                ExFreePool( pSrcFcb->DirEntry->DirectoryEntry.FileName.Buffer);

                ClearFlag( pSrcFcb->DirEntry->Flags, AFS_DIR_RELEASE_NAME_BUFFER);
            }

            //
            // OK, we need to allocate a new name buffer
            //

            pTmpBuffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                         uniTargetName.Length,
                                                         AFS_NAME_BUFFER_TAG);

            if( pTmpBuffer == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            pSrcFcb->DirEntry->DirectoryEntry.FileName.Buffer = pTmpBuffer;

            pSrcFcb->DirEntry->DirectoryEntry.FileName.MaximumLength = uniTargetName.Length;

            SetFlag( pSrcFcb->DirEntry->Flags, AFS_DIR_RELEASE_NAME_BUFFER);
        }

        pSrcFcb->DirEntry->DirectoryEntry.FileName.Length = uniTargetName.Length;

        RtlCopyMemory( pSrcFcb->DirEntry->DirectoryEntry.FileName.Buffer,
                       uniTargetName.Buffer,
                       uniTargetName.Length);

        //
        // Re-insert the directory entry
        // It has been updated in the above routine
        //

        AFSInsertDirectoryNode( pSrcFcb->ParentFcb,
                                pSrcFcb->DirEntry);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

        }
    }

    return ntStatus;
}

NTSTATUS
AFSSetPositionInfo( IN PIRP Irp,
                    IN AFSFcb *Fcb)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_POSITION_INFORMATION pBuffer;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    pBuffer = (PFILE_POSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

    pIrpSp->FileObject->CurrentByteOffset.QuadPart = pBuffer->CurrentByteOffset.QuadPart;

    AFSPrint("AFSSetPositionInfo Position %08lX\n", 
                                                pBuffer->CurrentByteOffset.LowPart);

    return ntStatus;
}

NTSTATUS
AFSSetAllocationInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_ALLOCATION_INFORMATION pBuffer;
    BOOLEAN bReleasePaging = FALSE;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT pFileObject = pIrpSp->FileObject;
    LARGE_INTEGER liAllocationSize;
    LARGE_INTEGER liSave = Fcb->Header.AllocationSize;

    pBuffer = (PFILE_ALLOCATION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

    //
    // If this is a truncation we need to grab the paging IO resource
    //

    if( Fcb->Header.AllocationSize.QuadPart > pBuffer->AllocationSize.QuadPart)
    {

        AFSAcquireExcl( &Fcb->NPFcb->PagingResource,
                          TRUE);

        bReleasePaging = TRUE;
    }

    if( Fcb->Header.AllocationSize.QuadPart != pBuffer->AllocationSize.QuadPart &&
        !pIrpSp->Parameters.SetFile.AdvanceOnly)
    {

        liAllocationSize.QuadPart = pBuffer->AllocationSize.QuadPart;

        if( (liAllocationSize.QuadPart % 512) > 0)
        {

            liAllocationSize.QuadPart = ((liAllocationSize.QuadPart/512) + 1) * 512;
        }

        Fcb->Header.AllocationSize = liAllocationSize;

        Fcb->DirEntry->DirectoryEntry.AllocationSize = liAllocationSize;

        ntStatus = AFSUpdateFileInformation( AFSRDRDeviceObject, Fcb);

        if (NT_SUCCESS(ntStatus)) {

            //
            // Tell the MM about the new size if we are truncating
            //

            if( Fcb->Header.AllocationSize.QuadPart > liAllocationSize.QuadPart &&
                CcIsFileCached( pFileObject)) 
            {

                CcSetFileSizes( pFileObject, 
                                (PCC_FILE_SIZES)&Fcb->Header.AllocationSize);
            }
            SetFlag( Fcb->Flags, AFS_FILE_MODIFIED);
        } 
        else 
        {
            //
            // Put things back
            //
            Fcb->Header.AllocationSize = liSave;

            Fcb->DirEntry->DirectoryEntry.AllocationSize = liSave;
        }
    }

    if( bReleasePaging)
    {

        AFSReleaseResource( &Fcb->NPFcb->PagingResource);
    }

    return ntStatus;
}

NTSTATUS
AFSSetEndOfFileInfo( IN PIRP Irp,
                     IN AFSFcb *Fcb)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_END_OF_FILE_INFORMATION pBuffer;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT pFileObject = pIrpSp->FileObject;
    LARGE_INTEGER liSaveSize = Fcb->Header.FileSize;
    LARGE_INTEGER liSaveVDL = Fcb->Header.ValidDataLength;
    BOOLEAN bModified = FALSE;

    pBuffer = (PFILE_END_OF_FILE_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

    if( Fcb->Header.FileSize.QuadPart != pBuffer->EndOfFile.QuadPart &&
        !pIrpSp->Parameters.SetFile.AdvanceOnly) 
    {

        if( pBuffer->EndOfFile.QuadPart < Fcb->Header.FileSize.QuadPart)
        {

            // Truncating the file
            if( !MmCanFileBeTruncated( pFileObject->SectionObjectPointer,
                                       &pBuffer->EndOfFile)) 
            {

                ntStatus = STATUS_USER_MAPPED_FILE;
            }
            else
            {

                Fcb->Header.FileSize = pBuffer->EndOfFile;

                Fcb->DirEntry->DirectoryEntry.EndOfFile = pBuffer->EndOfFile;

                if( Fcb->Header.ValidDataLength.QuadPart > Fcb->Header.FileSize.QuadPart) 
                {

                    Fcb->Header.ValidDataLength = Fcb->Header.FileSize;
                }

                bModified = TRUE;
            }
        }
        else
        {

            Fcb->Header.FileSize = pBuffer->EndOfFile;

            Fcb->DirEntry->DirectoryEntry.EndOfFile = pBuffer->EndOfFile;

            bModified = TRUE;
        }
    }

    if (bModified) {
        //
        // Tell the server
        //
        ntStatus = AFSUpdateFileInformation( AFSRDRDeviceObject, Fcb);

        if (NT_SUCCESS(ntStatus))
        {
        
            SetFlag( Fcb->Flags, AFS_FILE_MODIFIED);
        } 
        else 
        {
            Fcb->Header.ValidDataLength = liSaveVDL;
            Fcb->Header.FileSize = liSaveSize;
            Fcb->DirEntry->DirectoryEntry.EndOfFile = liSaveSize;
        }
    }

    return ntStatus;
}
