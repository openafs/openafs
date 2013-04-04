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
AFSQueryFileInfo( IN PDEVICE_OBJECT LibDeviceObject,
                  IN PIRP Irp)
{

    UNREFERENCED_PARAMETER(LibDeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    BOOLEAN bReleaseMain = FALSE;
    LONG lLength = 0;
    FILE_INFORMATION_CLASS stFileInformationClass;
    GUID stAuthGroup;
    PVOID pBuffer;

    __try
    {

        //
        // Determine the type of request this request is
        //

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        if( pFcb == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueryFileInfo Attempted access (%p) when pFcb == NULL\n",
                          Irp));

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        lLength = (LONG)pIrpSp->Parameters.QueryFile.Length;
        stFileInformationClass = pIrpSp->Parameters.QueryFile.FileInformationClass;
        pBuffer = Irp->AssociatedIrp.SystemBuffer;

        if ( BooleanFlagOn( pFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY))
        {

            RtlZeroMemory( &stAuthGroup,
                           sizeof( GUID));

            AFSRetrieveAuthGroupFnc( (ULONGLONG)PsGetCurrentProcessId(),
                                     (ULONGLONG)PsGetCurrentThreadId(),
                                     &stAuthGroup);

            ntStatus = AFSVerifyEntry( &stAuthGroup,
                                       pCcb->DirectoryCB);

            if ( NT_SUCCESS( ntStatus))
            {

                ClearFlag( pFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY);
            }
            else
            {

                ntStatus = STATUS_SUCCESS;
            }
        }

        //
        // Grab the main shared right off the bat
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQueryFileInfo Acquiring Fcb lock %p SHARED %08lX\n",
                      &pFcb->NPFcb->Resource,
                      PsGetCurrentThread()));

        AFSAcquireShared( &pFcb->NPFcb->Resource,
                          TRUE);

        bReleaseMain = TRUE;

        //
        // Don't allow requests against IOCtl nodes
        //

        if( pFcb->Header.NodeTypeCode == AFS_SPECIAL_SHARE_FCB)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryFileInfo Processing request against SpecialShare Fcb\n"));

            ntStatus = AFSProcessShareQueryInfo( Irp,
                                                 pFcb,
                                                 pCcb);

            try_return( ntStatus);
        }
        else if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {
            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryFileInfo request against PIOCtl Fcb\n"));

            ntStatus = AFSProcessPIOCtlQueryInfo( Irp,
                                                  pFcb,
                                                  pCcb,
                                                  &lLength);

            try_return( ntStatus);
        }

        else if( pFcb->Header.NodeTypeCode == AFS_INVALID_FCB)
        {
            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQueryFileInfo request against Invalid Fcb\n"));

            try_return( ntStatus = STATUS_ACCESS_DENIED);
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
                                              pCcb->DirectoryCB,
                                              &pAllInfo->BasicInformation,
                                              &lLength);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                ntStatus = AFSQueryStandardInfo( Irp,
                                                 pCcb->DirectoryCB,
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
                                           pCcb->DirectoryCB,
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
                                             pCcb->DirectoryCB,
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

                ntStatus = AFSQueryBasicInfo( Irp,
                                              pCcb->DirectoryCB,
                                              (PFILE_BASIC_INFORMATION)pBuffer,
                                              &lLength);

                break;
            }

            case FileStandardInformation:
            {

                ntStatus = AFSQueryStandardInfo( Irp,
                                                 pCcb->DirectoryCB,
                                                 (PFILE_STANDARD_INFORMATION)pBuffer,
                                                 &lLength);

                break;
            }

            case FileInternalInformation:
            {

                ntStatus = AFSQueryInternalInfo( Irp,
                                                 pFcb,
                                                 (PFILE_INTERNAL_INFORMATION)pBuffer,
                                                 &lLength);

                break;
            }

            case FileEaInformation:
            {

                ntStatus = AFSQueryEaInfo( Irp,
                                           pCcb->DirectoryCB,
                                           (PFILE_EA_INFORMATION)pBuffer,
                                           &lLength);

                break;
            }

            case FilePositionInformation:
            {

                ntStatus = AFSQueryPositionInfo( Irp,
                                      pFcb,
                                      (PFILE_POSITION_INFORMATION)pBuffer,
                                      &lLength);

                break;
            }

            case FileNormalizedNameInformation:
            case FileNameInformation:
            {

                ntStatus = AFSQueryNameInfo( Irp,
                                  pCcb->DirectoryCB,
                                  (PFILE_NAME_INFORMATION)pBuffer,
                                  &lLength);

                break;
            }

            case FileAlternateNameInformation:
            {

                ntStatus = AFSQueryShortNameInfo( Irp,
                                       pCcb->DirectoryCB,
                                       (PFILE_NAME_INFORMATION)pBuffer,
                                       &lLength);

                break;
            }

            case FileNetworkOpenInformation:
            {

                ntStatus = AFSQueryNetworkInfo( Irp,
                                     pCcb->DirectoryCB,
                                     (PFILE_NETWORK_OPEN_INFORMATION)pBuffer,
                                     &lLength);

                break;
            }

            case FileStreamInformation:
            {

                ntStatus = AFSQueryStreamInfo( Irp,
                                               pCcb->DirectoryCB,
                                               (FILE_STREAM_INFORMATION *)pBuffer,
                                               &lLength);

                break;
            }


            case FileAttributeTagInformation:
            {

                ntStatus = AFSQueryAttribTagInfo( Irp,
                                                  pCcb->DirectoryCB,
                                                  (FILE_ATTRIBUTE_TAG_INFORMATION *)pBuffer,
                                                  &lLength);

                break;
            }

            case FileRemoteProtocolInformation:
            {

                    ntStatus = AFSQueryRemoteProtocolInfo( Irp,
                                                           pCcb->DirectoryCB,
                                                           (FILE_REMOTE_PROTOCOL_INFORMATION *)pBuffer,
                                                           &lLength);

                break;
            }

            case FileNetworkPhysicalNameInformation:
            {

                ntStatus = AFSQueryPhysicalNameInfo( Irp,
                                                     pCcb->DirectoryCB,
                                                     (FILE_NETWORK_PHYSICAL_NAME_INFORMATION *)pBuffer,
                                                     &lLength);

                break;
            }

            default:
            {
                ntStatus = STATUS_INVALID_PARAMETER;
                break;
            }
        }

try_exit:

        Irp->IoStatus.Information = pIrpSp->Parameters.QueryFile.Length - lLength;

        if( bReleaseMain)
        {

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }

        if( !NT_SUCCESS( ntStatus) &&
            ntStatus != STATUS_INVALID_PARAMETER &&
            ntStatus != STATUS_BUFFER_OVERFLOW)
        {

            if( pCcb != NULL &&
                pCcb->DirectoryCB != NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSQueryFileInfo Failed to process request for %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                              &pCcb->DirectoryCB->NameInformation.FileName,
                              pFcb->ObjectInformation->FileId.Cell,
                              pFcb->ObjectInformation->FileId.Volume,
                              pFcb->ObjectInformation->FileId.Vnode,
                              pFcb->ObjectInformation->FileId.Unique,
                              ntStatus));
            }
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSQueryFileInfo\n"));

        AFSDumpTraceFilesFnc();

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
AFSSetFileInfo( IN PDEVICE_OBJECT LibDeviceObject,
                IN PIRP Irp)
{

    UNREFERENCED_PARAMETER(LibDeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    FILE_INFORMATION_CLASS FileInformationClass;
    BOOLEAN bCanQueueRequest = FALSE;
    PFILE_OBJECT pFileObject = NULL;
    BOOLEAN bReleaseMain = FALSE;
    BOOLEAN bUpdateFileInfo = FALSE;
    AFSFileID stParentFileId;

    __try
    {

        pFileObject = pIrpSp->FileObject;

        pFcb = (AFSFcb *)pFileObject->FsContext;
        pCcb = (AFSCcb *)pFileObject->FsContext2;

        if( pFcb == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetFileInfo Attempted access (%p) when pFcb == NULL\n",
                          Irp));

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        bCanQueueRequest = !(IoIsOperationSynchronous( Irp) | (KeGetCurrentIrql() != PASSIVE_LEVEL));
        FileInformationClass = pIrpSp->Parameters.SetFile.FileInformationClass;

        //
        // Grab the Fcb EXCL
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetFileInfo Acquiring Fcb lock %p EXCL %08lX\n",
                      &pFcb->NPFcb->Resource,
                      PsGetCurrentThread()));

        AFSAcquireExcl( &pFcb->NPFcb->Resource,
                        TRUE);

        bReleaseMain = TRUE;

        //
        // Don't allow requests against IOCtl nodes
        //

        if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetFileInfo Failing request against PIOCtl Fcb\n"));

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }
        else if( pFcb->Header.NodeTypeCode == AFS_SPECIAL_SHARE_FCB)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetFileInfo Processing request against SpecialShare Fcb\n"));

            ntStatus = AFSProcessShareSetInfo( Irp,
                                               pFcb,
                                               pCcb);

            try_return( ntStatus);
        }

        if( BooleanFlagOn( pFcb->ObjectInformation->VolumeCB->VolumeInformation.FileSystemAttributes, FILE_READ_ONLY_VOLUME))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetFileInfo Request failed due to read only volume\n",
                          Irp));

            try_return( ntStatus = STATUS_MEDIA_WRITE_PROTECTED);
        }

        if( pFcb->Header.NodeTypeCode == AFS_INVALID_FCB &&
            FileInformationClass != FileDispositionInformation)
        {
            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetFileInfo request against Invalid Fcb\n"));

            try_return( ntStatus = STATUS_ACCESS_DENIED);
        }

        //
        // Ensure rename operations are synchronous
        //

        if( FileInformationClass == FileRenameInformation)
        {

            bCanQueueRequest = FALSE;
        }

        //
        // Store away the parent fid
        //

        RtlZeroMemory( &stParentFileId,
                       sizeof( AFSFileID));

        if( BooleanFlagOn( pFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_PARENT_FID))
        {

            stParentFileId = pFcb->ObjectInformation->ParentFileId;
        }

        //
        // Process the request
        //

        switch( FileInformationClass)
        {

            case FileBasicInformation:
            {

                bUpdateFileInfo = TRUE;

                ntStatus = AFSSetBasicInfo( Irp,
                                            pCcb->DirectoryCB);

                break;
            }

            case FileDispositionInformation:
            {

                ntStatus = AFSSetDispositionInfo( Irp,
                                                  pCcb->DirectoryCB);

                break;
            }

            case FileRenameInformation:
            {

                ntStatus = AFSSetRenameInfo( Irp);

                break;
            }

            case FilePositionInformation:
            {

                ntStatus = AFSSetPositionInfo( Irp,
                                               pCcb->DirectoryCB);

                break;
            }

            case FileLinkInformation:
            {

                ntStatus = AFSSetFileLinkInfo( Irp);

                break;
            }

            case FileAllocationInformation:
            {

                ntStatus = AFSSetAllocationInfo( Irp,
                                                 pCcb->DirectoryCB);

                break;
            }

            case FileEndOfFileInformation:
            {

                ntStatus = AFSSetEndOfFileInfo( Irp,
                                                pCcb->DirectoryCB);

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

        if( NT_SUCCESS( ntStatus) &&
            bUpdateFileInfo)
        {

            ntStatus = AFSUpdateFileInformation( &stParentFileId,
                                                 pFcb->ObjectInformation,
                                                 &pCcb->AuthGroup);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSAcquireExcl( &pFcb->NPFcb->Resource,
                                TRUE);

                //
                // Unwind the update and fail the request
                //

                AFSUnwindFileInfo( pFcb,
                                   pCcb);

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSSetFileInfo Failed to send file info update to service request for %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                              &pCcb->DirectoryCB->NameInformation.FileName,
                              pFcb->ObjectInformation->FileId.Cell,
                              pFcb->ObjectInformation->FileId.Volume,
                              pFcb->ObjectInformation->FileId.Vnode,
                              pFcb->ObjectInformation->FileId.Unique,
                              ntStatus));

                AFSReleaseResource( &pFcb->NPFcb->Resource);
            }
        }

        if( !NT_SUCCESS( ntStatus))
        {

            if( pCcb != NULL &&
                pCcb->DirectoryCB != NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSSetFileInfo Failed to process request for %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                              &pCcb->DirectoryCB->NameInformation.FileName,
                              pFcb->ObjectInformation->FileId.Cell,
                              pFcb->ObjectInformation->FileId.Volume,
                              pFcb->ObjectInformation->FileId.Vnode,
                              pFcb->ObjectInformation->FileId.Unique,
                              ntStatus));
            }
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSSetFileInfo\n"));

        AFSDumpTraceFilesFnc();

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
                   IN AFSDirectoryCB *DirectoryCB,
                   IN OUT PFILE_BASIC_INFORMATION Buffer,
                   IN OUT PLONG Length)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    ULONG ulFileAttribs = 0;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFileInfoCB stFileInfo;
    AFSDirectoryCB *pParentDirectoryCB = NULL;
    UNICODE_STRING uniParentPath;

    if( *Length >= sizeof( FILE_BASIC_INFORMATION))
    {

        RtlZeroMemory( Buffer,
                       *Length);

        ulFileAttribs = DirectoryCB->ObjectInformation->FileAttributes;

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;
        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        if( DirectoryCB->ObjectInformation->FileType == AFS_FILE_TYPE_SYMLINK)
        {

            pParentDirectoryCB = AFSGetParentEntry( pCcb->NameArray);

            AFSRetrieveParentPath( &pCcb->FullFileName,
                                   &uniParentPath);

            RtlZeroMemory( &stFileInfo,
                           sizeof( AFSFileInfoCB));

            //
            // Can't hold the Fcb while evaluating the path, leads to lock inversion
            //

            AFSReleaseResource( &pFcb->NPFcb->Resource);

            //
            // Its a reparse point regardless of whether the file attributes
            // can be retrieved for the target.
            //

            if ( ulFileAttribs == FILE_ATTRIBUTE_NORMAL)
            {

                ulFileAttribs = FILE_ATTRIBUTE_REPARSE_POINT;
            }
            else
            {

                ulFileAttribs |= FILE_ATTRIBUTE_REPARSE_POINT;
            }

            if( NT_SUCCESS( AFSRetrieveFileAttributes( pParentDirectoryCB,
                                                       DirectoryCB,
                                                       &uniParentPath,
                                                       pCcb->NameArray,
                                                       &pCcb->AuthGroup,
                                                       &stFileInfo)))
            {

                if ( stFileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {

                    ulFileAttribs |= FILE_ATTRIBUTE_DIRECTORY;
                }
            }

            AFSAcquireShared( &pFcb->NPFcb->Resource,
                              TRUE);
        }


        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSQueryBasicInfo %wZ Type 0x%x Attrib 0x%x -> 0x%x\n",
                      &pCcb->DirectoryCB->NameInformation.FileName,
                      pFcb->ObjectInformation->FileType,
                      pFcb->ObjectInformation->FileAttributes,
                      ulFileAttribs));

        Buffer->CreationTime = DirectoryCB->ObjectInformation->CreationTime;
        Buffer->LastAccessTime = DirectoryCB->ObjectInformation->LastAccessTime;
        Buffer->LastWriteTime = DirectoryCB->ObjectInformation->LastWriteTime;
        Buffer->ChangeTime = DirectoryCB->ObjectInformation->ChangeTime;
        Buffer->FileAttributes = ulFileAttribs;

        if( DirectoryCB->NameInformation.FileName.Buffer[ 0] == L'.' &&
            BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_HIDE_DOT_NAMES))
        {

            if ( Buffer->FileAttributes != FILE_ATTRIBUTE_NORMAL)
            {
                Buffer->FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
            }
            else
            {
                Buffer->FileAttributes = FILE_ATTRIBUTE_HIDDEN;
            }
        }

        *Length -= sizeof( FILE_BASIC_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryStandardInfo( IN PIRP Irp,
                      IN AFSDirectoryCB *DirectoryCB,
                      IN OUT PFILE_STANDARD_INFORMATION Buffer,
                      IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFileInfoCB stFileInfo;
    AFSDirectoryCB *pParentDirectoryCB = NULL;
    UNICODE_STRING uniParentPath;
    ULONG ulFileAttribs = 0;

    if( *Length >= sizeof( FILE_STANDARD_INFORMATION))
    {

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;
        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        RtlZeroMemory( Buffer,
                       *Length);

        Buffer->NumberOfLinks = 1;
        Buffer->DeletePending = BooleanFlagOn( pCcb->DirectoryCB->Flags, AFS_DIR_ENTRY_PENDING_DELETE);

        Buffer->AllocationSize.QuadPart = (ULONGLONG)((DirectoryCB->ObjectInformation->AllocationSize.QuadPart/PAGE_SIZE) + 1) * PAGE_SIZE;

        Buffer->EndOfFile = DirectoryCB->ObjectInformation->EndOfFile;

        ulFileAttribs = DirectoryCB->ObjectInformation->FileAttributes;

        if( DirectoryCB->ObjectInformation->FileType == AFS_FILE_TYPE_SYMLINK)
        {

            pParentDirectoryCB = AFSGetParentEntry( pCcb->NameArray);

            AFSRetrieveParentPath( &pCcb->FullFileName,
                                   &uniParentPath);

            RtlZeroMemory( &stFileInfo,
                           sizeof( AFSFileInfoCB));

            //
            // Can't hold the Fcb while evaluating the path, leads to lock inversion
            //

            AFSReleaseResource( &pFcb->NPFcb->Resource);

            //
            // Its a reparse point regardless of whether or not the
            // file attributes can be retrieved.
            //

            if ( ulFileAttribs == FILE_ATTRIBUTE_NORMAL)
            {

                ulFileAttribs = FILE_ATTRIBUTE_REPARSE_POINT;
            }
            else
            {

                ulFileAttribs |= FILE_ATTRIBUTE_REPARSE_POINT;
            }

            if( NT_SUCCESS( AFSRetrieveFileAttributes( pParentDirectoryCB,
                                                       DirectoryCB,
                                                       &uniParentPath,
                                                       pCcb->NameArray,
                                                       &pCcb->AuthGroup,
                                                       &stFileInfo)))
            {

                if ( stFileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {

                    ulFileAttribs |= FILE_ATTRIBUTE_DIRECTORY;
                }
            }

            AFSAcquireShared( &pFcb->NPFcb->Resource,
                              TRUE);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSQueryStandardInfo %wZ Type 0x%x Attrib 0x%x -> 0x%x\n",
                      &pCcb->DirectoryCB->NameInformation.FileName,
                      pFcb->ObjectInformation->FileType,
                      pFcb->ObjectInformation->FileAttributes,
                      ulFileAttribs));

        Buffer->Directory = BooleanFlagOn( ulFileAttribs, FILE_ATTRIBUTE_DIRECTORY);

        *Length -= sizeof( FILE_STANDARD_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryInternalInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb,
                      IN OUT PFILE_INTERNAL_INFORMATION Buffer,
                      IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(Irp);
    NTSTATUS ntStatus = STATUS_SUCCESS;

    if( *Length >= sizeof( FILE_INTERNAL_INFORMATION))
    {

        Buffer->IndexNumber.HighPart = Fcb->ObjectInformation->FileId.Vnode;

        Buffer->IndexNumber.LowPart = Fcb->ObjectInformation->FileId.Unique;

        *Length -= sizeof( FILE_INTERNAL_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryEaInfo( IN PIRP Irp,
                IN AFSDirectoryCB *DirectoryCB,
                IN OUT PFILE_EA_INFORMATION Buffer,
                IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(DirectoryCB);
    NTSTATUS ntStatus = STATUS_SUCCESS;

    RtlZeroMemory( Buffer,
                   *Length);

    if( *Length >= sizeof( FILE_EA_INFORMATION))
    {

        Buffer->EaSize = 0;

        *Length -= sizeof( FILE_EA_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryPositionInfo( IN PIRP Irp,
                      IN AFSFcb *Fcb,
                      IN OUT PFILE_POSITION_INFORMATION Buffer,
                      IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(Fcb);
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

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryAccess( IN PIRP Irp,
                IN AFSFcb *Fcb,
                IN OUT PFILE_ACCESS_INFORMATION Buffer,
                IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(Fcb);
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

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryMode( IN PIRP Irp,
              IN AFSFcb *Fcb,
              IN OUT PFILE_MODE_INFORMATION Buffer,
              IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(Fcb);
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

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryAlignment( IN PIRP Irp,
                   IN AFSFcb *Fcb,
                   IN OUT PFILE_ALIGNMENT_INFORMATION Buffer,
                   IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(Fcb);
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

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryNameInfo( IN PIRP Irp,
                  IN AFSDirectoryCB *DirectoryCB,
                  IN OUT PFILE_NAME_INFORMATION Buffer,
                  IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(DirectoryCB);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulCopyLength = 0;
    ULONG cchCopied = 0;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    BOOLEAN bAddLeadingSlash = FALSE;
    BOOLEAN bAddTrailingSlash = FALSE;
    USHORT usFullNameLength = 0;

    pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

    pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

    if( *Length >= FIELD_OFFSET( FILE_NAME_INFORMATION, FileName))
    {

        RtlZeroMemory( Buffer,
                       *Length);

        if( pCcb->FullFileName.Length == 0 ||
            pCcb->FullFileName.Buffer[ 0] != L'\\')
        {
            bAddLeadingSlash = TRUE;
        }

        if( pFcb->ObjectInformation->FileType == AFS_FILE_TYPE_DIRECTORY &&
            pCcb->FullFileName.Length > 0 &&
            pCcb->FullFileName.Buffer[ (pCcb->FullFileName.Length/sizeof( WCHAR)) - 1] != L'\\')
        {
            bAddTrailingSlash = TRUE;
        }

        usFullNameLength = sizeof( WCHAR) +
                                    AFSServerName.Length +
                                    pCcb->FullFileName.Length;

        if( bAddLeadingSlash)
        {
            usFullNameLength += sizeof( WCHAR);
        }

        if( bAddTrailingSlash)
        {
            usFullNameLength += sizeof( WCHAR);
        }

        if( *Length >= (LONG)(FIELD_OFFSET( FILE_NAME_INFORMATION, FileName) + (LONG)usFullNameLength))
        {

            ulCopyLength = (LONG)usFullNameLength;
        }
        else
        {

            ulCopyLength = *Length - FIELD_OFFSET( FILE_NAME_INFORMATION, FileName);

            ntStatus = STATUS_BUFFER_OVERFLOW;
        }

        Buffer->FileNameLength = (ULONG)usFullNameLength;

        *Length -= FIELD_OFFSET( FILE_NAME_INFORMATION, FileName);

        if( ulCopyLength > 0)
        {

            Buffer->FileName[ 0] = L'\\';
            ulCopyLength -= sizeof( WCHAR);

            *Length -= sizeof( WCHAR);
            cchCopied += 1;

            if( ulCopyLength >= AFSServerName.Length)
            {

                RtlCopyMemory( &Buffer->FileName[ 1],
                               AFSServerName.Buffer,
                               AFSServerName.Length);

                ulCopyLength -= AFSServerName.Length;
                *Length -= AFSServerName.Length;
                cchCopied += AFSServerName.Length/sizeof( WCHAR);

                if ( ulCopyLength > 0 &&
                     bAddLeadingSlash)
                {

                    Buffer->FileName[ cchCopied] = L'\\';

                    ulCopyLength -= sizeof( WCHAR);
                    *Length -= sizeof( WCHAR);
                    cchCopied++;
                }

                if( ulCopyLength >= pCcb->FullFileName.Length)
                {

                    RtlCopyMemory( &Buffer->FileName[ cchCopied],
                                   pCcb->FullFileName.Buffer,
                                   pCcb->FullFileName.Length);

                    ulCopyLength -= pCcb->FullFileName.Length;
                    *Length -= pCcb->FullFileName.Length;
                    cchCopied += pCcb->FullFileName.Length/sizeof( WCHAR);

                    if( ulCopyLength > 0 &&
                        bAddTrailingSlash)
                    {
                        Buffer->FileName[ cchCopied] = L'\\';

                        *Length -= sizeof( WCHAR);
                    }
                }
                else
                {

                    RtlCopyMemory( &Buffer->FileName[ cchCopied],
                                   pCcb->FullFileName.Buffer,
                                   ulCopyLength);

                    *Length -= ulCopyLength;
                }
            }
        }
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryShortNameInfo( IN PIRP Irp,
                       IN AFSDirectoryCB *DirectoryCB,
                       IN OUT PFILE_NAME_INFORMATION Buffer,
                       IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(Irp);
    NTSTATUS ntStatus = STATUS_BUFFER_TOO_SMALL;
    ULONG ulCopyLength = 0;

    RtlZeroMemory( Buffer,
                   *Length);

    if( DirectoryCB->NameInformation.ShortNameLength == 0)
    {

        //
        // The short name IS the long name
        //

        if( *Length >= (LONG)FIELD_OFFSET( FILE_NAME_INFORMATION, FileName))
        {

            if( *Length >= (LONG)(FIELD_OFFSET( FILE_NAME_INFORMATION, FileName) + (LONG)DirectoryCB->NameInformation.FileName.Length))
            {

                ulCopyLength = (LONG)DirectoryCB->NameInformation.FileName.Length;

                ntStatus = STATUS_SUCCESS;
            }
            else
            {

                ulCopyLength = *Length - FIELD_OFFSET( FILE_NAME_INFORMATION, FileName);

                ntStatus = STATUS_BUFFER_OVERFLOW;
            }

            Buffer->FileNameLength = DirectoryCB->NameInformation.FileName.Length;

            *Length -= FIELD_OFFSET( FILE_NAME_INFORMATION, FileName);

            if( ulCopyLength > 0)
            {

                RtlCopyMemory( Buffer->FileName,
                               DirectoryCB->NameInformation.FileName.Buffer,
                               ulCopyLength);

                *Length -= ulCopyLength;
            }
        }
    }
    else
    {

        if( *Length >= (LONG)FIELD_OFFSET( FILE_NAME_INFORMATION, FileName))
        {

            if( *Length >= (LONG)(FIELD_OFFSET( FILE_NAME_INFORMATION, FileName) + (LONG)DirectoryCB->NameInformation.FileName.Length))
            {

                ulCopyLength = (LONG)DirectoryCB->NameInformation.ShortNameLength;

                ntStatus = STATUS_SUCCESS;
            }
            else
            {

                ulCopyLength = *Length - FIELD_OFFSET( FILE_NAME_INFORMATION, FileName);

                ntStatus = STATUS_BUFFER_OVERFLOW;
            }

            Buffer->FileNameLength = DirectoryCB->NameInformation.ShortNameLength;

            *Length -= FIELD_OFFSET( FILE_NAME_INFORMATION, FileName);

            if( ulCopyLength > 0)
            {

                RtlCopyMemory( Buffer->FileName,
                               DirectoryCB->NameInformation.ShortName,
                               Buffer->FileNameLength);

                *Length -= ulCopyLength;
            }
        }
    }

    return ntStatus;
}

NTSTATUS
AFSQueryNetworkInfo( IN PIRP Irp,
                     IN AFSDirectoryCB *DirectoryCB,
                     IN OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
                     IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFileInfoCB stFileInfo;
    AFSDirectoryCB *pParentDirectoryCB = NULL;
    UNICODE_STRING uniParentPath;
    ULONG ulFileAttribs = 0;

    RtlZeroMemory( Buffer,
                   *Length);

    if( *Length >= sizeof( FILE_NETWORK_OPEN_INFORMATION))
    {

        ulFileAttribs = DirectoryCB->ObjectInformation->FileAttributes;

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;
        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        if( DirectoryCB->ObjectInformation->FileType == AFS_FILE_TYPE_SYMLINK)
        {

            pParentDirectoryCB = AFSGetParentEntry( pCcb->NameArray);

            AFSRetrieveParentPath( &pCcb->FullFileName,
                                   &uniParentPath);

            RtlZeroMemory( &stFileInfo,
                           sizeof( AFSFileInfoCB));

            //
            // Can't hold the Fcb while evaluating the path, leads to lock inversion
            //

            AFSReleaseResource( &pFcb->NPFcb->Resource);

            //
            // Its a reparse point regardless of whether the file attributes
            // can be retrieved for the target.
            //

            if ( ulFileAttribs == FILE_ATTRIBUTE_NORMAL)
            {

                ulFileAttribs = FILE_ATTRIBUTE_REPARSE_POINT;
            }
            else
            {

                ulFileAttribs |= FILE_ATTRIBUTE_REPARSE_POINT;
            }

            if( NT_SUCCESS( AFSRetrieveFileAttributes( pParentDirectoryCB,
                                                       DirectoryCB,
                                                       &uniParentPath,
                                                       pCcb->NameArray,
                                                       &pCcb->AuthGroup,
                                                       &stFileInfo)))
            {

                if ( stFileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {

                    ulFileAttribs |= FILE_ATTRIBUTE_DIRECTORY;
                }
            }

            AFSAcquireShared( &pFcb->NPFcb->Resource,
                              TRUE);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSQueryNetworkInfo %wZ Type 0x%x Attrib 0x%x -> 0x%x\n",
                      &pCcb->DirectoryCB->NameInformation.FileName,
                      pFcb->ObjectInformation->FileType,
                      pFcb->ObjectInformation->FileAttributes,
                      ulFileAttribs));

        Buffer->CreationTime.QuadPart = DirectoryCB->ObjectInformation->CreationTime.QuadPart;
        Buffer->LastAccessTime.QuadPart = DirectoryCB->ObjectInformation->LastAccessTime.QuadPart;
        Buffer->LastWriteTime.QuadPart = DirectoryCB->ObjectInformation->LastWriteTime.QuadPart;
        Buffer->ChangeTime.QuadPart = DirectoryCB->ObjectInformation->ChangeTime.QuadPart;

        Buffer->AllocationSize.QuadPart = DirectoryCB->ObjectInformation->AllocationSize.QuadPart;
        Buffer->EndOfFile.QuadPart = DirectoryCB->ObjectInformation->EndOfFile.QuadPart;

        Buffer->FileAttributes = ulFileAttribs;

        if( DirectoryCB->NameInformation.FileName.Buffer[ 0] == L'.' &&
            BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_HIDE_DOT_NAMES))
        {

            if ( Buffer->FileAttributes != FILE_ATTRIBUTE_NORMAL)
            {

                Buffer->FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
            }
            else
            {

                Buffer->FileAttributes = FILE_ATTRIBUTE_HIDDEN;
            }
        }

        *Length -= sizeof( FILE_NETWORK_OPEN_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryStreamInfo( IN PIRP Irp,
                    IN AFSDirectoryCB *DirectoryCB,
                    IN OUT FILE_STREAM_INFORMATION *Buffer,
                    IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(Irp);
    NTSTATUS ntStatus = STATUS_BUFFER_TOO_SMALL;
    ULONG ulCopyLength = 0;

    if( *Length >= FIELD_OFFSET( FILE_STREAM_INFORMATION, StreamName))
    {

        RtlZeroMemory( Buffer,
                       *Length);

        Buffer->NextEntryOffset = 0;


        if( !BooleanFlagOn( DirectoryCB->ObjectInformation->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
        {

            if( *Length >= (LONG)(FIELD_OFFSET( FILE_STREAM_INFORMATION, StreamName) + 14))  // ::$DATA
            {

                ulCopyLength = 14;

                ntStatus = STATUS_SUCCESS;
            }
            else
            {

                ulCopyLength = *Length - FIELD_OFFSET( FILE_STREAM_INFORMATION, StreamName);

                ntStatus = STATUS_BUFFER_OVERFLOW;
            }

            Buffer->StreamNameLength = 14; // ::$DATA

            Buffer->StreamSize.QuadPart = DirectoryCB->ObjectInformation->EndOfFile.QuadPart;

            Buffer->StreamAllocationSize.QuadPart = DirectoryCB->ObjectInformation->AllocationSize.QuadPart;

            *Length -= FIELD_OFFSET( FILE_STREAM_INFORMATION, StreamName);

            if( ulCopyLength > 0)
            {

                RtlCopyMemory( Buffer->StreamName,
                               L"::$DATA",
                               ulCopyLength);

                *Length -= ulCopyLength;
            }
        }
        else
        {

            Buffer->StreamNameLength = 0;       // No stream for a directory

            // The response size is zero

            ntStatus = STATUS_SUCCESS;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSQueryAttribTagInfo( IN PIRP Irp,
                       IN AFSDirectoryCB *DirectoryCB,
                       IN OUT FILE_ATTRIBUTE_TAG_INFORMATION *Buffer,
                       IN OUT PLONG Length)
{

    NTSTATUS ntStatus = STATUS_BUFFER_TOO_SMALL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFileInfoCB stFileInfo;
    AFSDirectoryCB *pParentDirectoryCB = NULL;
    UNICODE_STRING uniParentPath;
    ULONG ulFileAttribs = 0;

    if( *Length >= sizeof( FILE_ATTRIBUTE_TAG_INFORMATION))
    {

        RtlZeroMemory( Buffer,
                       *Length);

        ulFileAttribs = DirectoryCB->ObjectInformation->FileAttributes;

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;
        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        if( DirectoryCB->ObjectInformation->FileType == AFS_FILE_TYPE_SYMLINK)
        {

            pParentDirectoryCB = AFSGetParentEntry( pCcb->NameArray);

            AFSRetrieveParentPath( &pCcb->FullFileName,
                                   &uniParentPath);

            RtlZeroMemory( &stFileInfo,
                           sizeof( AFSFileInfoCB));

            //
            // Can't hold the Fcb while evaluating the path, leads to lock inversion
            //

            AFSReleaseResource( &pFcb->NPFcb->Resource);

            //
            // Its a reparse point regardless of whether the file attributes
            // can be retrieved for the target.
            //

            if ( ulFileAttribs == FILE_ATTRIBUTE_NORMAL)
            {

                ulFileAttribs = FILE_ATTRIBUTE_REPARSE_POINT;
            }
            else
            {

                ulFileAttribs |= FILE_ATTRIBUTE_REPARSE_POINT;
            }

            if( NT_SUCCESS( AFSRetrieveFileAttributes( pParentDirectoryCB,
                                                       DirectoryCB,
                                                       &uniParentPath,
                                                       pCcb->NameArray,
                                                       &pCcb->AuthGroup,
                                                       &stFileInfo)))
            {

                if ( stFileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {

                    ulFileAttribs |= FILE_ATTRIBUTE_DIRECTORY;
                }
            }

            AFSAcquireShared( &pFcb->NPFcb->Resource,
                              TRUE);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSAttribTagInfo %wZ Type 0x%x Attrib 0x%x -> 0x%x\n",
                      &pCcb->DirectoryCB->NameInformation.FileName,
                      pFcb->ObjectInformation->FileType,
                      pFcb->ObjectInformation->FileAttributes,
                      ulFileAttribs));

        Buffer->FileAttributes = ulFileAttribs;

        if( DirectoryCB->NameInformation.FileName.Buffer[ 0] == L'.' &&
            BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_HIDE_DOT_NAMES))
        {

            if ( Buffer->FileAttributes != FILE_ATTRIBUTE_NORMAL)
            {

                Buffer->FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
            }
            else
            {

                Buffer->FileAttributes = FILE_ATTRIBUTE_HIDDEN;
            }
        }

        if ( DirectoryCB->ObjectInformation->FileType == AFS_FILE_TYPE_MOUNTPOINT)
        {

            Buffer->ReparseTag = IO_REPARSE_TAG_SURROGATE|IO_REPARSE_TAG_OPENAFS_DFS;
        }
        else if( BooleanFlagOn( DirectoryCB->ObjectInformation->FileAttributes, FILE_ATTRIBUTE_REPARSE_POINT))
        {

            Buffer->ReparseTag = IO_REPARSE_TAG_SYMLINK;
        }

        *Length -= sizeof( FILE_ATTRIBUTE_TAG_INFORMATION);

        ntStatus = STATUS_SUCCESS;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryRemoteProtocolInfo( IN PIRP Irp,
                            IN AFSDirectoryCB *DirectoryCB,
                            IN OUT FILE_REMOTE_PROTOCOL_INFORMATION *Buffer,
                            IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(DirectoryCB);
    NTSTATUS ntStatus = STATUS_BUFFER_TOO_SMALL;

    if( *Length >= sizeof( FILE_REMOTE_PROTOCOL_INFORMATION))
    {

        RtlZeroMemory( Buffer,
                       *Length);

        Buffer->StructureVersion = 1;

        Buffer->StructureSize = sizeof(FILE_REMOTE_PROTOCOL_INFORMATION);

        Buffer->Protocol = WNNC_NET_OPENAFS;

        Buffer->ProtocolMajorVersion = 3;

        Buffer->ProtocolMinorVersion = 0;

        Buffer->ProtocolRevision = 0;

        *Length -= sizeof( FILE_REMOTE_PROTOCOL_INFORMATION);

        ntStatus = STATUS_SUCCESS;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryPhysicalNameInfo( IN PIRP Irp,
                          IN AFSDirectoryCB *DirectoryCB,
                          IN OUT PFILE_NETWORK_PHYSICAL_NAME_INFORMATION Buffer,
                          IN OUT PLONG Length)
{

    UNREFERENCED_PARAMETER(DirectoryCB);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulCopyLength = 0;
    ULONG cchCopied = 0;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    BOOLEAN bAddLeadingSlash = FALSE;
    USHORT usFullNameLength = 0;

    pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

    pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

    if( *Length >= FIELD_OFFSET( FILE_NETWORK_PHYSICAL_NAME_INFORMATION, FileName))
    {

        RtlZeroMemory( Buffer,
                       *Length);

        if( pCcb->FullFileName.Length == 0 ||
            pCcb->FullFileName.Buffer[ 0] != L'\\')
        {
            bAddLeadingSlash = TRUE;
        }

        usFullNameLength = pCcb->FullFileName.Length;

        if( bAddLeadingSlash)
        {
            usFullNameLength += sizeof( WCHAR);
        }

        if( *Length >= (LONG)(FIELD_OFFSET( FILE_NETWORK_PHYSICAL_NAME_INFORMATION, FileName) + (LONG)usFullNameLength))
        {
            ulCopyLength = (LONG)usFullNameLength;
        }
        else
        {

            ulCopyLength = *Length - FIELD_OFFSET( FILE_NETWORK_PHYSICAL_NAME_INFORMATION, FileName);

            ntStatus = STATUS_BUFFER_OVERFLOW;
        }

        Buffer->FileNameLength = (ULONG)usFullNameLength;

        *Length -= FIELD_OFFSET( FILE_NETWORK_PHYSICAL_NAME_INFORMATION, FileName);

        if( ulCopyLength > 0)
        {

            if( bAddLeadingSlash)
            {

                Buffer->FileName[ cchCopied] = L'\\';

                ulCopyLength -= sizeof( WCHAR);
                *Length -= sizeof( WCHAR);
                cchCopied++;
            }

            if( ulCopyLength >= pCcb->FullFileName.Length)
            {

                RtlCopyMemory( &Buffer->FileName[ cchCopied],
                               pCcb->FullFileName.Buffer,
                               pCcb->FullFileName.Length);

                ulCopyLength -= pCcb->FullFileName.Length;
                *Length -= pCcb->FullFileName.Length;
                cchCopied += pCcb->FullFileName.Length/sizeof( WCHAR);
            }
            else
            {

                RtlCopyMemory( &Buffer->FileName[ cchCopied],
                               pCcb->FullFileName.Buffer,
                               ulCopyLength);

                *Length -= ulCopyLength;
            }
        }
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSSetBasicInfo( IN PIRP Irp,
                 IN AFSDirectoryCB *DirectoryCB)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_BASIC_INFORMATION pBuffer;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    ULONG ulNotifyFilter = 0;
    AFSCcb *pCcb = NULL;

    __Enter
    {

        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        pBuffer = (PFILE_BASIC_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

        pCcb->FileUnwindInfo.FileAttributes = (ULONG)-1;

        if( pBuffer->FileAttributes != (ULONGLONG)0)
        {

            if( DirectoryCB->ObjectInformation->Fcb->Header.NodeTypeCode == AFS_FILE_FCB &&
                BooleanFlagOn( pBuffer->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
            {

                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }

            if( DirectoryCB->ObjectInformation->Fcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
            {

                pBuffer->FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
            }

            pCcb->FileUnwindInfo.FileAttributes = DirectoryCB->ObjectInformation->FileAttributes;

            DirectoryCB->ObjectInformation->FileAttributes = pBuffer->FileAttributes;

            ulNotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;

            SetFlag( DirectoryCB->ObjectInformation->Fcb->Flags, AFS_FCB_FLAG_FILE_MODIFIED);
        }

        pCcb->FileUnwindInfo.CreationTime.QuadPart = (ULONGLONG)-1;

        if( pBuffer->CreationTime.QuadPart != (ULONGLONG)-1 &&
            pBuffer->CreationTime.QuadPart != (ULONGLONG)0)
        {

            pCcb->FileUnwindInfo.CreationTime.QuadPart = DirectoryCB->ObjectInformation->CreationTime.QuadPart;

            DirectoryCB->ObjectInformation->CreationTime.QuadPart = pBuffer->CreationTime.QuadPart;

            ulNotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;

            SetFlag( DirectoryCB->ObjectInformation->Fcb->Flags, AFS_FCB_FLAG_FILE_MODIFIED | AFS_FCB_FLAG_UPDATE_CREATE_TIME);
        }

        pCcb->FileUnwindInfo.LastAccessTime.QuadPart = (ULONGLONG)-1;

        if( pBuffer->LastAccessTime.QuadPart != (ULONGLONG)-1 &&
            pBuffer->LastAccessTime.QuadPart != (ULONGLONG)0)
        {

            pCcb->FileUnwindInfo.LastAccessTime.QuadPart = DirectoryCB->ObjectInformation->LastAccessTime.QuadPart;

            DirectoryCB->ObjectInformation->LastAccessTime.QuadPart = pBuffer->LastAccessTime.QuadPart;

            ulNotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;

            SetFlag( DirectoryCB->ObjectInformation->Fcb->Flags, AFS_FCB_FLAG_FILE_MODIFIED | AFS_FCB_FLAG_UPDATE_ACCESS_TIME);
        }

        pCcb->FileUnwindInfo.LastWriteTime.QuadPart = (ULONGLONG)-1;

        if( pBuffer->LastWriteTime.QuadPart != (ULONGLONG)-1 &&
            pBuffer->LastWriteTime.QuadPart != (ULONGLONG)0)
        {

            pCcb->FileUnwindInfo.LastWriteTime.QuadPart = DirectoryCB->ObjectInformation->LastWriteTime.QuadPart;

            DirectoryCB->ObjectInformation->LastWriteTime.QuadPart = pBuffer->LastWriteTime.QuadPart;

            ulNotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;

            SetFlag( DirectoryCB->ObjectInformation->Fcb->Flags, AFS_FCB_FLAG_FILE_MODIFIED | AFS_FCB_FLAG_UPDATE_LAST_WRITE_TIME);
        }

        pCcb->FileUnwindInfo.ChangeTime.QuadPart = (ULONGLONG)-1;

        if( pBuffer->ChangeTime.QuadPart != (ULONGLONG)-1 &&
            pBuffer->ChangeTime.QuadPart != (ULONGLONG)0)
        {

            pCcb->FileUnwindInfo.ChangeTime.QuadPart = DirectoryCB->ObjectInformation->ChangeTime.QuadPart;

            DirectoryCB->ObjectInformation->ChangeTime.QuadPart = pBuffer->ChangeTime.QuadPart;

            ulNotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;

            SetFlag( DirectoryCB->ObjectInformation->Fcb->Flags, AFS_FCB_FLAG_FILE_MODIFIED | AFS_FCB_FLAG_UPDATE_CHANGE_TIME);
        }

        if( ulNotifyFilter > 0)
        {

            if( BooleanFlagOn( DirectoryCB->ObjectInformation->Flags, AFS_OBJECT_FLAGS_PARENT_FID))
            {

                AFSObjectInfoCB * pParentObjectInfo = AFSFindObjectInfo( DirectoryCB->ObjectInformation->VolumeCB,
                                                                         &DirectoryCB->ObjectInformation->ParentFileId);

                if ( pParentObjectInfo != NULL)
                {
                    AFSFsRtlNotifyFullReportChange( pParentObjectInfo,
                                                    pCcb,
                                                    (ULONG)ulNotifyFilter,
                                                    (ULONG)FILE_ACTION_MODIFIED);

                    AFSReleaseObjectInfo( &pParentObjectInfo);
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
                       IN AFSDirectoryCB *DirectoryCB)
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

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetDispositionInfo Attempt to delete root entry\n"));

            try_return( ntStatus = STATUS_CANNOT_DELETE);
        }

        //
        // If the file is read only then do not allow the delete
        //

        if( BooleanFlagOn( DirectoryCB->ObjectInformation->FileAttributes, FILE_ATTRIBUTE_READONLY))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetDispositionInfo Attempt to delete read only entry %wZ\n",
                          &DirectoryCB->NameInformation.FileName));

            try_return( ntStatus = STATUS_CANNOT_DELETE);
        }

        if( pBuffer->DeleteFile)
        {

            //
            // Check if the caller can delete the file
            //

            ntStatus = AFSNotifyDelete( DirectoryCB,
                                        &pCcb->AuthGroup,
                                        TRUE);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSSetDispositionInfo Cannot delete entry %wZ Status %08lX\n",
                              &DirectoryCB->NameInformation.FileName,
                              ntStatus));

                try_return( ntStatus);
            }

            if( pFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
            {

                //
                // Reduce the Link count in the object information block
                // to correspond with the deletion of the directory entry.
                //

                pFcb->ObjectInformation->Links--;

                //
                // Check if this is a directory that there are not currently other opens
                //

                if( pFcb->ObjectInformation->Specific.Directory.ChildOpenHandleCount > 0)
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSSetDispositionInfo Attempt to delete directory %wZ with open %u handles\n",
                                  &DirectoryCB->NameInformation.FileName,
                                  pFcb->ObjectInformation->Specific.Directory.ChildOpenHandleCount));

                    try_return( ntStatus = STATUS_DIRECTORY_NOT_EMPTY);
                }

                if( !AFSIsDirectoryEmptyForDelete( pFcb))
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSSetDispositionInfo Attempt to delete non-empty directory %wZ\n",
                                  &DirectoryCB->NameInformation.FileName));

                    try_return( ntStatus = STATUS_DIRECTORY_NOT_EMPTY);
                }

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetDispositionInfo Setting PENDING_DELETE on DirEntry  %p Name %wZ\n",
                              DirectoryCB,
                              &DirectoryCB->NameInformation.FileName));

                SetFlag( pCcb->DirectoryCB->Flags, AFS_DIR_ENTRY_PENDING_DELETE);
            }
            else if( pFcb->Header.NodeTypeCode == AFS_FILE_FCB)
            {
                BOOLEAN bMmFlushed;

                AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetDispositionInfo Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
                              &pFcb->NPFcb->SectionObjectResource,
                              PsGetCurrentThread()));

                AFSAcquireExcl( &pFcb->NPFcb->SectionObjectResource,
                                TRUE);

                //
                // Attempt to flush any outstanding data
                //

                bMmFlushed = MmFlushImageSection( &pFcb->NPFcb->SectionObjectPointers,
                                                  MmFlushForDelete);

                if ( bMmFlushed)
                {

                    //
                    // Set PENDING_DELETE before CcPurgeCacheSection to avoid a
                    // deadlock with Trend Micro's Enterprise anti-virus product
                    // which attempts to open the file which is being deleted.
                    //

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSSetDispositionInfo Setting PENDING_DELETE on DirEntry %p Name %wZ\n",
                                  DirectoryCB,
                                  &DirectoryCB->NameInformation.FileName));

                    SetFlag( pCcb->DirectoryCB->Flags, AFS_DIR_ENTRY_PENDING_DELETE);

                    //
                    // Purge the cache as well
                    //

                    if( pFcb->NPFcb->SectionObjectPointers.DataSectionObject != NULL)
                    {

                        if ( !CcPurgeCacheSection( &pFcb->NPFcb->SectionObjectPointers,
                                                   NULL,
                                                   0,
                                                   TRUE))
                        {

                            SetFlag( pFcb->Flags, AFS_FCB_FLAG_PURGE_ON_CLOSE);
                        }
                    }
                }

                AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetDispositionInfo Releasing Fcb SectionObject lock %p EXCL %08lX\n",
                              &pFcb->NPFcb->SectionObjectResource,
                              PsGetCurrentThread()));

                AFSReleaseResource( &pFcb->NPFcb->SectionObjectResource);

                if ( !bMmFlushed)
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSSetDispositionInfo Failed to flush image section for delete Entry %wZ\n",
                                  &DirectoryCB->NameInformation.FileName));

                    try_return( ntStatus = STATUS_CANNOT_DELETE);
                }
            }
            else if( pFcb->Header.NodeTypeCode == AFS_SYMBOLIC_LINK_FCB ||
                     pFcb->Header.NodeTypeCode == AFS_MOUNT_POINT_FCB ||
                     pFcb->Header.NodeTypeCode == AFS_DFS_LINK_FCB ||
                     pFcb->Header.NodeTypeCode == AFS_INVALID_FCB)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetDispositionInfo Setting PENDING_DELETE on DirEntry %p Name %wZ\n",
                              DirectoryCB,
                              &DirectoryCB->NameInformation.FileName));

                SetFlag( pCcb->DirectoryCB->Flags, AFS_DIR_ENTRY_PENDING_DELETE);
            }
        }
        else
        {

            ClearFlag( pCcb->DirectoryCB->Flags, AFS_DIR_ENTRY_PENDING_DELETE);
        }

        //
        // OK, should be good to go, set the flag in the file object
        //

        pIrpSp->FileObject->DeletePending = pBuffer->DeleteFile;

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSSetFileLinkInfo( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_LINK_INFORMATION pFileLinkInfo = NULL;
    PFILE_OBJECT pSrcFileObj = NULL;
    PFILE_OBJECT pTargetFileObj = pIrpSp->Parameters.SetFile.FileObject;
    AFSFcb *pSrcFcb = NULL, *pTargetDcb = NULL;
    AFSCcb *pSrcCcb = NULL, *pTargetDirCcb = NULL;
    AFSObjectInfoCB *pSrcObject = NULL;
    AFSObjectInfoCB *pSrcParentObject = NULL, *pTargetParentObject = NULL;
    UNICODE_STRING uniSourceName, uniTargetName;
    UNICODE_STRING uniFullTargetName, uniTargetParentName;
    BOOLEAN bCommonParent = FALSE;
    AFSDirectoryCB *pTargetDirEntry = NULL;
    AFSDirectoryCB *pNewTargetDirEntry = NULL;
    ULONG ulTargetCRC;
    BOOLEAN bTargetEntryExists = FALSE;
    LONG lCount;
    BOOLEAN bReleaseTargetDirLock = FALSE;
    ULONG ulNotificationAction = 0, ulNotifyFilter = 0;

    __Enter
    {

        pSrcFileObj = pIrpSp->FileObject;

        pSrcFcb = (AFSFcb *)pSrcFileObj->FsContext;
        pSrcCcb = (AFSCcb *)pSrcFileObj->FsContext2;

        pSrcObject = pSrcFcb->ObjectInformation;

        if ( BooleanFlagOn( pSrcFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_PARENT_FID))
        {

            pSrcParentObject = AFSFindObjectInfo( pSrcFcb->ObjectInformation->VolumeCB,
                                                  &pSrcFcb->ObjectInformation->ParentFileId);
        }

        pFileLinkInfo = (PFILE_LINK_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

        //
        // Perform some basic checks to ensure FS integrity
        //

        if( pSrcFcb->Header.NodeTypeCode != AFS_FILE_FCB)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetFileLinkInfo Attempt to non-file (INVALID_PARAMETER)\n"));

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        if( pTargetFileObj == NULL)
        {

            if ( pFileLinkInfo->RootDirectory)
            {

                //
                // The target directory is provided by HANDLE
                // RootDirectory is only set when the target directory is not the same
                // as the source directory.
                //
                // AFS only supports hard links within a single directory.
                //
                // The IOManager should translate any Handle to a FileObject for us.
                // However, the failure to receive a FileObject is treated as a fatal
                // error.
                //

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSSetFileLinkInfo Attempt to link %wZ to alternate directory by handle INVALID_PARAMETER\n",
                              &pSrcCcb->DirectoryCB->NameInformation.FileName));

                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }
            else
            {

                uniFullTargetName.Length = (USHORT)pFileLinkInfo->FileNameLength;

                uniFullTargetName.Buffer = (PWSTR)&pFileLinkInfo->FileName;

                AFSRetrieveFinalComponent( &uniFullTargetName,
                                           &uniTargetName);

                AFSRetrieveParentPath( &uniFullTargetName,
                                       &uniTargetParentName);

                if ( uniTargetParentName.Length == 0)
                {

                    //
                    // This is a simple rename. Here the target directory is the same as the source parent directory
                    // and the name is retrieved from the system buffer information
                    //

                    pTargetParentObject = pSrcParentObject;
                }
                else
                {
                    //
                    // uniTargetParentName contains the directory the renamed object
                    // will be moved to.  Must obtain the TargetParentObject.
                    //

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSSetFileLinkInfo Attempt to link  %wZ to alternate directory %wZ (NOT_SAME_DEVICE)\n",
                                  &pSrcCcb->DirectoryCB->NameInformation.FileName,
                                  &uniFullTargetName));

                    try_return( ntStatus = STATUS_NOT_SAME_DEVICE);
                }
            }

            pTargetDcb = pTargetParentObject->Fcb;
        }
        else
        {

            //
            // So here we have the target directory taken from the targetfile object
            //

            pTargetDcb = (AFSFcb *)pTargetFileObj->FsContext;

            pTargetDirCcb = (AFSCcb *)pTargetFileObj->FsContext2;

            pTargetParentObject = (AFSObjectInfoCB *)pTargetDcb->ObjectInformation;

            //
            // Grab the target name which we setup in the IRP_MJ_CREATE handler. By how we set this up
            // it is only the target component of the rename operation
            //

            uniTargetName = *((PUNICODE_STRING)&pTargetFileObj->FileName);
        }

        //
        // The quick check to see if they are self linking.
        // Do the names match? Only do this where the parent directories are
        // the same
        //

        if( pTargetParentObject == pSrcParentObject)
        {

            if( FsRtlAreNamesEqual( &uniTargetName,
                                    &uniSourceName,
                                    FALSE,
                                    NULL))
            {
                try_return( ntStatus = STATUS_SUCCESS);
            }

            bCommonParent = TRUE;
        }
        else
        {

            //
            // We do not allow cross-volume hard links
            //

            if( pTargetParentObject->VolumeCB != pSrcObject->VolumeCB)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSSetFileLinkInfo Attempt to link to different volume %wZ\n",
                              &pSrcCcb->DirectoryCB->NameInformation.FileName));

                try_return( ntStatus = STATUS_NOT_SAME_DEVICE);
            }
        }

        ulTargetCRC = AFSGenerateCRC( &uniTargetName,
                                      FALSE);

        AFSAcquireExcl( pTargetParentObject->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        bReleaseTargetDirLock = TRUE;

        AFSLocateCaseSensitiveDirEntry( pTargetParentObject->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                        ulTargetCRC,
                                        &pTargetDirEntry);

        if( pTargetDirEntry == NULL)
        {

            //
            // Missed so perform a case insensitive lookup
            //

            ulTargetCRC = AFSGenerateCRC( &uniTargetName,
                                          TRUE);

            AFSLocateCaseInsensitiveDirEntry( pTargetParentObject->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                              ulTargetCRC,
                                              &pTargetDirEntry);
        }

        if ( !BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
             pTargetDirEntry == NULL && RtlIsNameLegalDOS8Dot3( &uniTargetName,
                                                                NULL,
                                                                NULL))
        {
            //
            // Try the short name
            //
            AFSLocateShortNameDirEntry( pTargetParentObject->Specific.Directory.ShortNameTree,
                                        ulTargetCRC,
                                        &pTargetDirEntry);
        }

        //
        // Increment our ref count on the dir entry
        //

        if( pTargetDirEntry != NULL)
        {

            ASSERT( BooleanFlagOn( pTargetDirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_PARENT_FID) &&
                    AFSIsEqualFID( &pTargetParentObject->FileId, &pTargetDirEntry->ObjectInformation->ParentFileId));

            lCount = InterlockedIncrement( &pTargetDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetFileLinkInfo Increment count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pTargetDirEntry->NameInformation.FileName,
                          pTargetDirEntry,
                          pSrcCcb,
                          lCount));

            ASSERT( lCount >= 0);

            if( !pFileLinkInfo->ReplaceIfExists)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSSetFileLinkInfo Attempt to link with target collision %wZ Target %wZ\n",
                              &pSrcCcb->DirectoryCB->NameInformation.FileName,
                              &pTargetDirEntry->NameInformation.FileName));

                try_return( ntStatus = STATUS_OBJECT_NAME_COLLISION);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING | AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetFileLinkInfo Target %wZ exists DE %p Count %d, performing delete of target\n",
                          &pTargetDirEntry->NameInformation.FileName,
                          pTargetDirEntry,
                          lCount));

            //
            // Pull the directory entry from the parent
            //

            AFSRemoveDirNodeFromParent( pTargetParentObject,
                                        pTargetDirEntry,
                                        FALSE);

            bTargetEntryExists = TRUE;
        }
        else
        {
            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetFileLinkInfo Target does NOT exist, normal linking\n"));
        }

        //
        // OK, this is a simple rename. Issue the rename
        // request to the service.
        //

        ntStatus = AFSNotifyHardLink( pSrcFcb->ObjectInformation,
                                      &pSrcCcb->AuthGroup,
                                      pSrcParentObject,
                                      pTargetDcb->ObjectInformation,
                                      pSrcCcb->DirectoryCB,
                                      &uniTargetName,
                                      pFileLinkInfo->ReplaceIfExists,
                                      &pNewTargetDirEntry);

        if( ntStatus != STATUS_REPARSE &&
            !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetFileLinkInfo Failed link of %wZ to target %wZ Status %08lX\n",
                          &pSrcCcb->DirectoryCB->NameInformation.FileName,
                          &uniTargetName,
                          ntStatus));

            try_return( ntStatus);
        }

        if ( ntStatus != STATUS_REPARSE)
        {

            AFSInsertDirectoryNode( pTargetDcb->ObjectInformation,
                                    pNewTargetDirEntry,
                                    TRUE);
        }

        //
        // Send notification for the target link file
        //

        if( bTargetEntryExists || pNewTargetDirEntry)
        {

            ulNotificationAction = FILE_ACTION_MODIFIED;
        }
        else
        {

            ulNotificationAction = FILE_ACTION_ADDED;
        }

        AFSFsRtlNotifyFullReportChange( pTargetParentObject,
                                        pSrcCcb,
                                        (ULONG)ulNotifyFilter,
                                        (ULONG)ulNotificationAction);

      try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( bTargetEntryExists)
            {

                AFSInsertDirectoryNode( pTargetParentObject,
                                        pTargetDirEntry,
                                        FALSE);
            }
        }

        if( pTargetDirEntry != NULL)
        {

            //
            // Release DirOpenReferenceCount obtained above
            //

            lCount = InterlockedDecrement( &pTargetDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetFileLinkInfo Decrement count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pTargetDirEntry->NameInformation.FileName,
                          pTargetDirEntry,
                          pSrcCcb,
                          lCount));

            ASSERT( lCount >= 0);
        }

        if( pNewTargetDirEntry != NULL)
        {

            //
            // Release DirOpenReferenceCount obtained from AFSNotifyHardLink
            //

            lCount = InterlockedDecrement( &pNewTargetDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetFileLinkInfo Decrement count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pNewTargetDirEntry->NameInformation.FileName,
                          pNewTargetDirEntry,
                          pSrcCcb,
                          lCount));

            ASSERT( lCount >= 0);
        }

        if( bReleaseTargetDirLock)
        {

            AFSReleaseResource( pTargetParentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);
        }

        if ( pSrcParentObject != NULL)
        {

            AFSReleaseObjectInfo( &pSrcParentObject);
        }

        //
        // No need to release pTargetParentObject as it is either a copy of pSrcParentObject
        // or (AFSFcb *)pTargetFileObj->FsContext->ObjectInformation
        //

        pTargetParentObject = NULL;
    }

    return ntStatus;
}

NTSTATUS
AFSSetRenameInfo( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pSrcFcb = NULL, *pTargetDcb = NULL, *pTargetFcb = NULL;
    AFSCcb *pSrcCcb = NULL, *pTargetDirCcb = NULL;
    PFILE_OBJECT pSrcFileObj = pIrpSp->FileObject;
    PFILE_OBJECT pTargetFileObj = pIrpSp->Parameters.SetFile.FileObject;
    PFILE_OBJECT pTargetParentFileObj = NULL;
    PFILE_RENAME_INFORMATION pRenameInfo = NULL;
    UNICODE_STRING uniTargetName, uniSourceName, uniTargetParentName;
    BOOLEAN bReplaceIfExists = FALSE;
    UNICODE_STRING uniShortName;
    AFSDirectoryCB *pTargetDirEntry = NULL;
    ULONG ulTargetCRC = 0;
    BOOLEAN bTargetEntryExists = FALSE;
    AFSObjectInfoCB *pSrcObject = NULL;
    AFSObjectInfoCB *pSrcParentObject = NULL, *pTargetParentObject = NULL;
    AFSFileID stNewFid;
    ULONG ulNotificationAction = 0, ulNotifyFilter = 0;
    UNICODE_STRING uniFullTargetName;
    BOOLEAN bCommonParent = FALSE;
    BOOLEAN bReleaseTargetDirLock = FALSE;
    BOOLEAN bReleaseSourceDirLock = FALSE;
    BOOLEAN bDereferenceTargetParentObject = FALSE;
    PERESOURCE  pSourceDirLock = NULL;
    LONG lCount;

    __Enter
    {

        bReplaceIfExists = pIrpSp->Parameters.SetFile.ReplaceIfExists;

        pRenameInfo = (PFILE_RENAME_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

        pSrcFcb = (AFSFcb *)pSrcFileObj->FsContext;
        pSrcCcb = (AFSCcb *)pSrcFileObj->FsContext2;

        pSrcObject = pSrcFcb->ObjectInformation;

        if ( BooleanFlagOn( pSrcFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_PARENT_FID))
        {

            pSrcParentObject = AFSFindObjectInfo( pSrcFcb->ObjectInformation->VolumeCB,
                                                  &pSrcFcb->ObjectInformation->ParentFileId);
        }

        //
        // Perform some basic checks to ensure FS integrity
        //

        if( pSrcFcb->Header.NodeTypeCode == AFS_ROOT_FCB)
        {

            //
            // Can't rename the root directory
            //

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetRenameInfo Attempt to rename root entry\n"));

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        if( pSrcFcb->Header.NodeTypeCode == AFS_DIRECTORY_FCB)
        {

            //
            // If there are any open children then fail the rename
            //

            if( pSrcFcb->ObjectInformation->Specific.Directory.ChildOpenHandleCount > 0)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSSetRenameInfo Attempt to rename directory with open children %wZ\n",
                              &pSrcCcb->DirectoryCB->NameInformation.FileName));

                try_return( ntStatus = STATUS_ACCESS_DENIED);
            }
        }


        //
        // Extract off the final component name from the Fcb
        //

        uniSourceName.Length = (USHORT)pSrcCcb->DirectoryCB->NameInformation.FileName.Length;
        uniSourceName.MaximumLength = uniSourceName.Length;

        uniSourceName.Buffer = pSrcCcb->DirectoryCB->NameInformation.FileName.Buffer;

        //
        // Resolve the target fileobject
        //

        if( pTargetFileObj == NULL)
        {

            if ( pRenameInfo->RootDirectory)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSSetRenameInfo Handle provided but no FileObject ntStatus INVALID_PARAMETER\n"));

                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }
            else
            {

                uniFullTargetName.Length = (USHORT)pRenameInfo->FileNameLength;

                uniFullTargetName.Buffer = (PWSTR)&pRenameInfo->FileName;

                AFSRetrieveFinalComponent( &uniFullTargetName,
                                           &uniTargetName);

                AFSRetrieveParentPath( &uniFullTargetName,
                                       &uniTargetParentName);

                if ( uniTargetParentName.Length == 0)
                {

                    //
                    // This is a simple rename. Here the target directory is the same as the source parent directory
                    // and the name is retrieved from the system buffer information
                    //

                    pTargetParentObject = pSrcParentObject;
                }
                else
                {
                    //
                    // uniTargetParentName contains the directory the renamed object
                    // will be moved to.  Must obtain the TargetParentObject.
                    //

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSSetRenameInfo Attempt to move %wZ to %wZ -- not yet supported (NOT_SAME_DEVICE)\n",
                                  &pSrcCcb->DirectoryCB->NameInformation.FileName,
                                  &uniFullTargetName));

                    try_return( ntStatus = STATUS_NOT_SAME_DEVICE);
                }
            }

            pTargetDcb = pTargetParentObject->Fcb;
        }
        else
        {

            //
            // So here we have the target directory taken from the targetfile object
            //

            pTargetDcb = (AFSFcb *)pTargetFileObj->FsContext;

            pTargetDirCcb = (AFSCcb *)pTargetFileObj->FsContext2;

            pTargetParentObject = (AFSObjectInfoCB *)pTargetDcb->ObjectInformation;

            //
            // Grab the target name which we setup in the IRP_MJ_CREATE handler. By how we set this up
            // it is only the target component of the rename operation
            //

            uniTargetName = *((PUNICODE_STRING)&pTargetFileObj->FileName);
        }

        //
        // The quick check to see if they are not really performing a rename
        // Do the names match? Only do this where the parent directories are
        // the same
        //

        if( pTargetParentObject == pSrcParentObject)
        {

            if( FsRtlAreNamesEqual( &uniTargetName,
                                    &uniSourceName,
                                    FALSE,
                                    NULL))
            {
                try_return( ntStatus = STATUS_SUCCESS);
            }

            bCommonParent = TRUE;
        }
        else
        {

            bCommonParent = FALSE;
        }

        //
        // We do not allow cross-volume renames to occur
        //

        if( pTargetParentObject->VolumeCB != pSrcObject->VolumeCB)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetRenameInfo Attempt to rename directory to different volume %wZ\n",
                          &pSrcCcb->DirectoryCB->NameInformation.FileName));

            try_return( ntStatus = STATUS_NOT_SAME_DEVICE);
        }

        ulTargetCRC = AFSGenerateCRC( &uniTargetName,
                                      FALSE);

        AFSAcquireExcl( pTargetParentObject->Specific.Directory.DirectoryNodeHdr.TreeLock,
                        TRUE);

        bReleaseTargetDirLock = TRUE;

        if( pTargetParentObject != pSrcParentObject)
        {
            AFSAcquireExcl( pSrcParentObject->Specific.Directory.DirectoryNodeHdr.TreeLock,
                            TRUE);

            bReleaseSourceDirLock = TRUE;

            pSourceDirLock = pSrcParentObject->Specific.Directory.DirectoryNodeHdr.TreeLock;
        }

        AFSLocateCaseSensitiveDirEntry( pTargetParentObject->Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                        ulTargetCRC,
                                        &pTargetDirEntry);

        if( pTargetDirEntry == NULL)
        {

            //
            // Missed so perform a case insensitive lookup
            //

            ulTargetCRC = AFSGenerateCRC( &uniTargetName,
                                          TRUE);

            AFSLocateCaseInsensitiveDirEntry( pTargetParentObject->Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                              ulTargetCRC,
                                              &pTargetDirEntry);
        }

        if ( !BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
             pTargetDirEntry == NULL && RtlIsNameLegalDOS8Dot3( &uniTargetName,
                                                               NULL,
                                                               NULL))
        {
            //
            // Try the short name
            //
            AFSLocateShortNameDirEntry( pTargetParentObject->Specific.Directory.ShortNameTree,
                                        ulTargetCRC,
                                        &pTargetDirEntry);
        }

        //
        // Increment our ref count on the dir entry
        //

        if( pTargetDirEntry != NULL)
        {

            ASSERT( BooleanFlagOn( pTargetDirEntry->ObjectInformation->Flags, AFS_OBJECT_FLAGS_PARENT_FID) &&
                    AFSIsEqualFID( &pTargetParentObject->FileId, &pTargetDirEntry->ObjectInformation->ParentFileId));

            lCount = InterlockedIncrement( &pTargetDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetRenameInfo Increment count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pTargetDirEntry->NameInformation.FileName,
                          pTargetDirEntry,
                          pSrcCcb,
                          lCount));

            ASSERT( lCount >= 0);

            if( !bReplaceIfExists)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSSetRenameInfo Attempt to rename directory with target collision %wZ Target %wZ\n",
                              &pSrcCcb->DirectoryCB->NameInformation.FileName,
                              &pTargetDirEntry->NameInformation.FileName));

                try_return( ntStatus = STATUS_OBJECT_NAME_COLLISION);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING | AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetRenameInfo Target %wZ exists DE %p Count %d, performing delete of target\n",
                          &pTargetDirEntry->NameInformation.FileName,
                          pTargetDirEntry,
                          lCount));

            //
            // Pull the directory entry from the parent
            //

            AFSRemoveDirNodeFromParent( pTargetParentObject,
                                        pTargetDirEntry,
                                        FALSE);

            bTargetEntryExists = TRUE;
        }
        else
        {
            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetRenameInfo Target does NOT exist, normal rename\n"));
        }

        //
        // We need to remove the DirEntry from the parent node, update the index
        // and reinsert it into the parent tree. Note that for entries with the
        // same parent we do not pull the node from the enumeration list
        //

        AFSRemoveDirNodeFromParent( pSrcParentObject,
                                    pSrcCcb->DirectoryCB,
                                    !bCommonParent);

        //
        // OK, this is a simple rename. Issue the rename
        // request to the service.
        //

        ntStatus = AFSNotifyRename( pSrcFcb->ObjectInformation,
                                    &pSrcCcb->AuthGroup,
                                    pSrcParentObject,
                                    pTargetDcb->ObjectInformation,
                                    pSrcCcb->DirectoryCB,
                                    &uniTargetName,
                                    &stNewFid);

        if( !NT_SUCCESS( ntStatus))
        {

            //
            // Attempt to re-insert the directory entry
            //

            AFSInsertDirectoryNode( pSrcParentObject,
                                    pSrcCcb->DirectoryCB,
                                    !bCommonParent);

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetRenameInfo Failed rename of %wZ to target %wZ Status %08lX\n",
                          &pSrcCcb->DirectoryCB->NameInformation.FileName,
                          &uniTargetName,
                          ntStatus));

            try_return( ntStatus);
        }

        //
        // Set the notification up for the source file
        //

        if( pSrcParentObject == pTargetParentObject &&
            !bTargetEntryExists)
        {

            ulNotificationAction = FILE_ACTION_RENAMED_OLD_NAME;
        }
        else
        {

            ulNotificationAction = FILE_ACTION_REMOVED;
        }

        if( pSrcFcb->ObjectInformation->FileType == AFS_FILE_TYPE_DIRECTORY)
        {

            ulNotifyFilter = FILE_NOTIFY_CHANGE_DIR_NAME;
        }
        else
        {

            ulNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME;
        }

        AFSFsRtlNotifyFullReportChange( pSrcParentObject,
                                        pSrcCcb,
                                        (ULONG)ulNotifyFilter,
                                        (ULONG)ulNotificationAction);

        //
        // Update the name in the dir entry.
        //

        ntStatus = AFSUpdateDirEntryName( pSrcCcb->DirectoryCB,
                                          &uniTargetName);

        if( !NT_SUCCESS( ntStatus))
        {

            //
            // Attempt to re-insert the directory entry
            //

            AFSInsertDirectoryNode( pSrcParentObject,
                                    pSrcCcb->DirectoryCB,
                                    !bCommonParent);

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSSetRenameInfo Failed update of dir entry %wZ to target %wZ Status %08lX\n",
                          &pSrcCcb->DirectoryCB->NameInformation.FileName,
                          &uniTargetName,
                          ntStatus));

            try_return( ntStatus);
        }

        //
        // Update the object information block, if needed
        //

        if( !AFSIsEqualFID( &pSrcObject->FileId,
                            &stNewFid))
        {

            AFSAcquireExcl( pSrcObject->VolumeCB->ObjectInfoTree.TreeLock,
                            TRUE);

            //
            // Remove the old information entry
            //

            AFSRemoveHashEntry( &pSrcObject->VolumeCB->ObjectInfoTree.TreeHead,
                                &pSrcObject->TreeEntry);

            RtlCopyMemory( &pSrcObject->FileId,
                           &stNewFid,
                           sizeof( AFSFileID));

            //
            // Insert the entry into the new object table.
            //

            pSrcObject->TreeEntry.HashIndex = AFSCreateLowIndex( &pSrcObject->FileId);

            if( pSrcObject->VolumeCB->ObjectInfoTree.TreeHead == NULL)
            {

                pSrcObject->VolumeCB->ObjectInfoTree.TreeHead = &pSrcObject->TreeEntry;
            }
            else
            {

                if ( !NT_SUCCESS( AFSInsertHashEntry( pSrcObject->VolumeCB->ObjectInfoTree.TreeHead,
                                                     &pSrcObject->TreeEntry)))
                {

                    //
                    // Lost a race, an ObjectInfo object already exists for this FID.
                    // Let this copy be garbage collected.
                    //

                    ClearFlag( pSrcObject->Flags, AFS_OBJECT_INSERTED_HASH_TREE);
                }
            }

            AFSReleaseResource( pSrcObject->VolumeCB->ObjectInfoTree.TreeLock);
        }

        //
        // Update the hash values for the name trees.
        //

        pSrcCcb->DirectoryCB->CaseSensitiveTreeEntry.HashIndex = AFSGenerateCRC( &pSrcCcb->DirectoryCB->NameInformation.FileName,
                                                                                 FALSE);

        pSrcCcb->DirectoryCB->CaseInsensitiveTreeEntry.HashIndex = AFSGenerateCRC( &pSrcCcb->DirectoryCB->NameInformation.FileName,
                                                                                   TRUE);

        if( !BooleanFlagOn( pDeviceExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES) &&
            pSrcCcb->DirectoryCB->NameInformation.ShortNameLength > 0 &&
            !RtlIsNameLegalDOS8Dot3( &pSrcCcb->DirectoryCB->NameInformation.FileName,
                                     NULL,
                                     NULL))
        {

            uniShortName.Length = pSrcCcb->DirectoryCB->NameInformation.ShortNameLength;
            uniShortName.MaximumLength = uniShortName.Length;
            uniShortName.Buffer = pSrcCcb->DirectoryCB->NameInformation.ShortName;

            pSrcCcb->DirectoryCB->Type.Data.ShortNameTreeEntry.HashIndex = AFSGenerateCRC( &uniShortName,
                                                                                           TRUE);

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetRenameInfo Initialized short name hash for %wZ longname %wZ\n",
                          &uniShortName,
                          &pSrcCcb->DirectoryCB->NameInformation.FileName));
        }
        else
        {

            pSrcCcb->DirectoryCB->Type.Data.ShortNameTreeEntry.HashIndex = 0;
        }

        if( !bCommonParent)
        {

            //
            // Update the file index for the object in the new parent
            //

            pSrcCcb->DirectoryCB->FileIndex = (ULONG)InterlockedIncrement( &pTargetParentObject->Specific.Directory.DirectoryNodeHdr.ContentIndex);
        }

        //
        // Re-insert the directory entry
        //

        AFSInsertDirectoryNode( pTargetParentObject,
                                pSrcCcb->DirectoryCB,
                                !bCommonParent);

        //
        // Update the parent pointer in the source object if they are different
        //

        if( pSrcParentObject != pTargetParentObject)
        {

            lCount = InterlockedDecrement( &pSrcParentObject->Specific.Directory.ChildOpenHandleCount);

            lCount = InterlockedDecrement( &pSrcParentObject->Specific.Directory.ChildOpenReferenceCount);

            lCount = InterlockedIncrement( &pTargetParentObject->Specific.Directory.ChildOpenHandleCount);

            lCount = InterlockedIncrement( &pTargetParentObject->Specific.Directory.ChildOpenReferenceCount);

            lCount = AFSObjectInfoIncrement( pTargetParentObject,
                                             AFS_OBJECT_REFERENCE_CHILD);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetRenameInfo Increment count on parent object %p Cnt %d\n",
                          pTargetParentObject,
                          lCount));

            lCount = AFSObjectInfoDecrement( pSrcParentObject,
                                             AFS_OBJECT_REFERENCE_CHILD);

            AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetRenameInfo Decrement count on parent object %p Cnt %d\n",
                          pSrcParentObject,
                          lCount));

            pSrcFcb->ObjectInformation->ParentFileId = pTargetParentObject->FileId;

            SetFlag( pSrcFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_PARENT_FID);

            pSrcParentObject = pTargetParentObject;

            ulNotificationAction = FILE_ACTION_ADDED;
        }
        else
        {

            ulNotificationAction = FILE_ACTION_RENAMED_NEW_NAME;
        }

        //
        // Now update the notification for the target file
        //

        AFSFsRtlNotifyFullReportChange( pTargetParentObject,
                                        pSrcCcb,
                                        (ULONG)ulNotifyFilter,
                                        (ULONG)ulNotificationAction);

        //
        // If we performed the rename of the target because it existed, we now need to
        // delete the tmp target we created above
        //

        if( bTargetEntryExists)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetRenameInfo Setting DELETE flag in dir entry %p name %wZ\n",
                          pTargetDirEntry,
                          &pTargetDirEntry->NameInformation.FileName));

            SetFlag( pTargetDirEntry->Flags, AFS_DIR_ENTRY_DELETED);

            //
            // Try and purge the cache map if this is a file
            //

            if( pTargetDirEntry->ObjectInformation->FileType == AFS_FILE_TYPE_FILE &&
                pTargetDirEntry->ObjectInformation->Fcb != NULL &&
                pTargetDirEntry->DirOpenReferenceCount > 1)
            {

                pTargetFcb = pTargetDirEntry->ObjectInformation->Fcb;
            }

            ASSERT( pTargetDirEntry->DirOpenReferenceCount > 0);

            lCount = InterlockedDecrement( &pTargetDirEntry->DirOpenReferenceCount); // The count we added above

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetRenameInfo Decrement count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pTargetDirEntry->NameInformation.FileName,
                          pTargetDirEntry,
                          pSrcCcb,
                          lCount));

            ASSERT( lCount >= 0);

            if( lCount == 0 &&
                pTargetDirEntry->NameArrayReferenceCount <= 0)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetRenameInfo Deleting dir entry %p name %wZ\n",
                              pTargetDirEntry,
                              &pTargetDirEntry->NameInformation.FileName));

                AFSDeleteDirEntry( pTargetParentObject,
                                   &pTargetDirEntry);
            }

            pTargetDirEntry = NULL;

            if ( pTargetFcb != NULL)
            {

                //
                // Do not hold TreeLocks across the MmForceSectionClosed() call as
                // it can deadlock with Trend Micro's TmPreFlt!TmpQueryFullName
                //

                if( bReleaseTargetDirLock)
                {
                    AFSReleaseResource( pTargetParentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);

                    bReleaseTargetDirLock = FALSE;
                }

                if( bReleaseSourceDirLock)
                {

                    AFSReleaseResource( pSourceDirLock);

                    bReleaseSourceDirLock = FALSE;
                }

                //
                // MmForceSectionClosed() can eventually call back into AFSCleanup
                // which will need to acquire Fcb->Resource exclusively.  Failure
                // to obtain it here before holding the SectionObjectResource will
                // permit the locks to be obtained out of order risking a deadlock.
                //

                AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetRenameInfo Acquiring Fcb lock %p EXCL %08lX\n",
                              &pTargetFcb->NPFcb->Resource,
                              PsGetCurrentThread()));

                AFSAcquireExcl( &pTargetFcb->NPFcb->Resource,
                                TRUE);

                AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetRenameInfo Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
                              &pTargetFcb->NPFcb->SectionObjectResource,
                              PsGetCurrentThread()));

                AFSAcquireExcl( &pTargetFcb->NPFcb->SectionObjectResource,
                                TRUE);

                //
                // Close the section in the event it was mapped
                //

                if( !MmForceSectionClosed( &pTargetFcb->NPFcb->SectionObjectPointers,
                                           TRUE))
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSSetRenameInfo Failed to delete section for target file %wZ\n",
                                  &uniTargetName));
                }

                AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetRenameInfo Releasing Fcb SectionObject lock %p EXCL %08lX\n",
                              &pTargetFcb->NPFcb->SectionObjectResource,
                              PsGetCurrentThread()));

                AFSReleaseResource( &pTargetFcb->NPFcb->SectionObjectResource);

                AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetRenameInfo Releasing Fcb lock %p EXCL %08lX\n",
                              &pTargetFcb->NPFcb->Resource,
                              PsGetCurrentThread()));

                AFSReleaseResource( &pTargetFcb->NPFcb->Resource);
            }
        }

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( bTargetEntryExists)
            {

                ASSERT( pTargetParentObject != NULL);

                AFSInsertDirectoryNode( pTargetParentObject,
                                        pTargetDirEntry,
                                        FALSE);
            }
        }

        if( pTargetDirEntry != NULL)
        {

            lCount = InterlockedDecrement( &pTargetDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetRenameInfo Decrement2 count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pTargetDirEntry->NameInformation.FileName,
                          pTargetDirEntry,
                          pSrcCcb,
                          lCount));

            ASSERT( lCount >= 0);
        }

        if( bReleaseTargetDirLock)
        {

            AFSReleaseResource( pTargetParentObject->Specific.Directory.DirectoryNodeHdr.TreeLock);
        }

        if( bReleaseSourceDirLock)
        {

            AFSReleaseResource( pSourceDirLock);
        }

        if ( bDereferenceTargetParentObject)
        {

            ObDereferenceObject( pTargetParentFileObj);
        }

        if ( pSrcParentObject != NULL)
        {

            AFSReleaseObjectInfo( &pSrcParentObject);
        }

        //
        // No need to release pTargetParentObject as it is either a copy of pSrcParentObject
        // or (AFSFcb *)pTargetFileObj->FsContext->ObjectInformation
        //

        pTargetParentObject = NULL;
    }

    return ntStatus;
}

NTSTATUS
AFSSetPositionInfo( IN PIRP Irp,
                    IN AFSDirectoryCB *DirectoryCB)
{
    UNREFERENCED_PARAMETER(DirectoryCB);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_POSITION_INFORMATION pBuffer;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    pBuffer = (PFILE_POSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

    pIrpSp->FileObject->CurrentByteOffset.QuadPart = pBuffer->CurrentByteOffset.QuadPart;

    return ntStatus;
}

NTSTATUS
AFSSetAllocationInfo( IN PIRP Irp,
                      IN AFSDirectoryCB *DirectoryCB)
{
    UNREFERENCED_PARAMETER(DirectoryCB);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_ALLOCATION_INFORMATION pBuffer;
    BOOLEAN bReleasePaging = FALSE;
    BOOLEAN bTellCc = FALSE;
    BOOLEAN bTellService = FALSE;
    BOOLEAN bUserMapped = FALSE;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT pFileObject = pIrpSp->FileObject;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    LARGE_INTEGER liSaveAlloc;
    LARGE_INTEGER liSaveFileSize;
    LARGE_INTEGER liSaveVDL;

    pBuffer = (PFILE_ALLOCATION_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

    pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

    pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

    //
    // save values to put back
    //
    liSaveAlloc = pFcb->Header.AllocationSize;
    liSaveFileSize = pFcb->Header.FileSize;
    liSaveVDL = pFcb->Header.ValidDataLength;

    if( pFcb->Header.AllocationSize.QuadPart == pBuffer->AllocationSize.QuadPart ||
        pIrpSp->Parameters.SetFile.AdvanceOnly)
    {
        return STATUS_SUCCESS ;
    }

    if( pFcb->Header.AllocationSize.QuadPart > pBuffer->AllocationSize.QuadPart)
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetAllocationInfo Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
                      &pFcb->NPFcb->SectionObjectResource,
                      PsGetCurrentThread()));

        AFSAcquireExcl( &pFcb->NPFcb->SectionObjectResource,
                        TRUE);

        bUserMapped = !MmCanFileBeTruncated( pFileObject->SectionObjectPointer,
                                             &pBuffer->AllocationSize);

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetAllocationInfo Releasing Fcb SectionObject lock %p EXCL %08lX\n",
                      &pFcb->NPFcb->SectionObjectResource,
                      PsGetCurrentThread()));

        AFSReleaseResource( &pFcb->NPFcb->SectionObjectResource);

        //
        // Truncating the file
        //
        if ( bUserMapped)
        {

            ntStatus = STATUS_USER_MAPPED_FILE ;
        }
        else
        {

            //
            // If this is a truncation we need to grab the paging IO resource.
            //

            AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetAllocationInfo Acquiring Fcb PagingIo lock %p EXCL %08lX\n",
                          &pFcb->NPFcb->PagingResource,
                          PsGetCurrentThread()));

            AFSAcquireExcl( &pFcb->NPFcb->PagingResource,
                            TRUE);

            bReleasePaging = TRUE;

            //
            // Must drop the Fcb Resource.  When changing the file size
            // a deadlock can occur with Trend Micro's filter if the file
            // size is set to zero.
            //

            AFSReleaseResource( &pFcb->NPFcb->Resource);

            pFcb->Header.AllocationSize = pBuffer->AllocationSize;

            pFcb->ObjectInformation->AllocationSize = pBuffer->AllocationSize;

            //
            // Tell Cc that Allocation is moved.
            //
            bTellCc = TRUE;

            if( pFcb->Header.FileSize.QuadPart > pBuffer->AllocationSize.QuadPart)
            {
                //
                // We are pulling the EOF back as well so we need to tell
                // the service.
                //
                bTellService = TRUE;

                pFcb->Header.FileSize = pBuffer->AllocationSize;

                pFcb->ObjectInformation->EndOfFile = pBuffer->AllocationSize;
            }

        }
    }
    else
    {
        //
        // Tell Cc if allocation is increased.
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetAllocationInfo Acquiring Fcb PagingIo lock %p EXCL %08lX\n",
                      &pFcb->NPFcb->PagingResource,
                      PsGetCurrentThread()));

        AFSAcquireExcl( &pFcb->NPFcb->PagingResource,
                        TRUE);

        bReleasePaging = TRUE;

        //
        // Must drop the Fcb Resource.  When changing the file size
        // a deadlock can occur with Trend Micro's filter if the file
        // size is set to zero.
        //

        AFSReleaseResource( &pFcb->NPFcb->Resource);

        bTellCc = pBuffer->AllocationSize.QuadPart > pFcb->Header.AllocationSize.QuadPart;

        pFcb->Header.AllocationSize = pBuffer->AllocationSize;

        pFcb->ObjectInformation->AllocationSize = pBuffer->AllocationSize;
    }

    //
    // Now Tell the server if we have to
    //
    if (bTellService)
    {

        ASSERT( BooleanFlagOn( pFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_PARENT_FID));

        ntStatus = AFSUpdateFileInformation( &pFcb->ObjectInformation->ParentFileId,
                                             pFcb->ObjectInformation,
                                             &pCcb->AuthGroup);
    }

    if (NT_SUCCESS(ntStatus))
    {
        //
        // Trim extents if we told the service - the update has done an implicit
        // trim at the service.
        //
        if (bTellService)
        {
            AFSTrimExtents( pFcb,
                            &pFcb->Header.FileSize);
        }

        KeQuerySystemTime( &pFcb->ObjectInformation->ChangeTime);

        SetFlag( pFcb->Flags, AFS_FCB_FLAG_FILE_MODIFIED | AFS_FCB_FLAG_UPDATE_CHANGE_TIME);

        if (bTellCc &&
            CcIsFileCached( pFileObject))
        {
            CcSetFileSizes( pFileObject,
                            (PCC_FILE_SIZES)&pFcb->Header.AllocationSize);
        }
    }
    else
    {
        //
        // Put the saved values back
        //
        pFcb->Header.ValidDataLength = liSaveVDL;
        pFcb->Header.FileSize = liSaveFileSize;
        pFcb->Header.AllocationSize = liSaveAlloc;
        pFcb->ObjectInformation->EndOfFile = liSaveFileSize;
        pFcb->ObjectInformation->AllocationSize = liSaveAlloc;
    }

    if( bReleasePaging)
    {

        AFSReleaseResource( &pFcb->NPFcb->PagingResource);

        AFSAcquireExcl( &pFcb->NPFcb->Resource,
                        TRUE);
    }

    return ntStatus;
}

NTSTATUS
AFSSetEndOfFileInfo( IN PIRP Irp,
                     IN AFSDirectoryCB *DirectoryCB)
{
    UNREFERENCED_PARAMETER(DirectoryCB);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PFILE_END_OF_FILE_INFORMATION pBuffer;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT pFileObject = pIrpSp->FileObject;
    LARGE_INTEGER liSaveSize;
    LARGE_INTEGER liSaveVDL;
    LARGE_INTEGER liSaveAlloc;
    BOOLEAN bModified = FALSE;
    BOOLEAN bReleasePaging = FALSE;
    BOOLEAN bTruncated = FALSE;
    BOOLEAN bUserMapped = FALSE;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;

    pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

    pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

    pBuffer = (PFILE_END_OF_FILE_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

    liSaveSize = pFcb->Header.FileSize;
    liSaveAlloc = pFcb->Header.AllocationSize;
    liSaveVDL = pFcb->Header.ValidDataLength;

    if( pFcb->Header.FileSize.QuadPart != pBuffer->EndOfFile.QuadPart &&
        !pIrpSp->Parameters.SetFile.AdvanceOnly)
    {

        if( pBuffer->EndOfFile.QuadPart < pFcb->Header.FileSize.QuadPart)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetEndOfFileInfo Acquiring Fcb SectionObject lock %p EXCL %08lX\n",
                          &pFcb->NPFcb->SectionObjectResource,
                          PsGetCurrentThread()));

            AFSAcquireExcl( &pFcb->NPFcb->SectionObjectResource,
                            TRUE);

            bUserMapped = !MmCanFileBeTruncated( pFileObject->SectionObjectPointer,
                                                 &pBuffer->EndOfFile);

            AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetEndOfFileInfo Releasing Fcb SectionObject lock %p EXCL %08lX\n",
                          &pFcb->NPFcb->SectionObjectResource,
                          PsGetCurrentThread()));

            AFSReleaseResource( &pFcb->NPFcb->SectionObjectResource);

            // Truncating the file
            if ( bUserMapped)
            {

                ntStatus = STATUS_USER_MAPPED_FILE;
            }
            else
            {

                //
                // If this is a truncation we need to grab the paging
                // IO resource.
                //
                AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSSetAllocationInfo Acquiring Fcb PagingIo lock %p EXCL %08lX\n",
                              &pFcb->NPFcb->PagingResource,
                              PsGetCurrentThread()));

                AFSAcquireExcl( &pFcb->NPFcb->PagingResource,
                                TRUE);

                bReleasePaging = TRUE;

                //
                // Must drop the Fcb Resource.  When changing the file size
                // a deadlock can occur with Trend Micro's filter if the file
                // size is set to zero.
                //

                AFSReleaseResource( &pFcb->NPFcb->Resource);

                pFcb->Header.AllocationSize = pBuffer->EndOfFile;

                pFcb->Header.FileSize = pBuffer->EndOfFile;

                pFcb->ObjectInformation->EndOfFile = pBuffer->EndOfFile;

                pFcb->ObjectInformation->AllocationSize = pBuffer->EndOfFile;

                if( pFcb->Header.ValidDataLength.QuadPart > pFcb->Header.FileSize.QuadPart)
                {

                    pFcb->Header.ValidDataLength = pFcb->Header.FileSize;
                }

                bTruncated = TRUE;

                bModified = TRUE;
            }
        }
        else
        {

            //
            // extending the file, move EOF
            //

            //
            // If this is a truncation we need to grab the paging
            // IO resource.
            //
            AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSSetAllocationInfo Acquiring Fcb PagingIo lock %p EXCL %08lX\n",
                          &pFcb->NPFcb->PagingResource,
                          PsGetCurrentThread()));

            AFSAcquireExcl( &pFcb->NPFcb->PagingResource,
                            TRUE);

            bReleasePaging = TRUE;

            //
            // Must drop the Fcb Resource.  When changing the file size
            // a deadlock can occur with Trend Micro's filter if the file
            // size is set to zero.
            //

            AFSReleaseResource( &pFcb->NPFcb->Resource);

            pFcb->Header.FileSize = pBuffer->EndOfFile;

            pFcb->ObjectInformation->EndOfFile = pBuffer->EndOfFile;

            if (pFcb->Header.FileSize.QuadPart > pFcb->Header.AllocationSize.QuadPart)
            {
                //
                // And Allocation as needed.
                //
                pFcb->Header.AllocationSize = pBuffer->EndOfFile;

                pFcb->ObjectInformation->AllocationSize = pBuffer->EndOfFile;
            }

            bModified = TRUE;
        }
    }

    if (bModified)
    {

        KeQuerySystemTime( &pFcb->ObjectInformation->ChangeTime);

        SetFlag( pFcb->Flags, AFS_FCB_FLAG_FILE_MODIFIED | AFS_FCB_FLAG_UPDATE_CHANGE_TIME);

        //
        // Tell the server
        //

        ASSERT( BooleanFlagOn( pFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_PARENT_FID));

        ntStatus = AFSUpdateFileInformation( &pFcb->ObjectInformation->ParentFileId,
                                             pFcb->ObjectInformation,
                                             &pCcb->AuthGroup);

        if( NT_SUCCESS(ntStatus))
        {
            //
            // We are now good to go so tell CC.
            //
            CcSetFileSizes( pFileObject,
                            (PCC_FILE_SIZES)&pFcb->Header.AllocationSize);

            //
            // And give up those extents
            //
            if( bTruncated)
            {

                AFSTrimExtents( pFcb,
                                &pFcb->Header.FileSize);
            }
        }
        else
        {
            pFcb->Header.ValidDataLength = liSaveVDL;
            pFcb->Header.FileSize = liSaveSize;
            pFcb->Header.AllocationSize = liSaveAlloc;
            pFcb->ObjectInformation->EndOfFile = liSaveSize;
            pFcb->ObjectInformation->AllocationSize = liSaveAlloc;
        }
    }

    if( bReleasePaging)
    {

        AFSReleaseResource( &pFcb->NPFcb->PagingResource);

        AFSAcquireExcl( &pFcb->NPFcb->Resource,
                        TRUE);
    }

    return ntStatus;
}

NTSTATUS
AFSProcessShareSetInfo( IN IRP *Irp,
                        IN AFSFcb *Fcb,
                        IN AFSCcb *Ccb)
{

    UNREFERENCED_PARAMETER(Fcb);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    FILE_INFORMATION_CLASS ulFileInformationClass;
    void *pPipeInfo = NULL;

    __Enter
    {
        ulFileInformationClass = pIrpSp->Parameters.SetFile.FileInformationClass;

        AFSDbgTrace(( AFS_SUBSYSTEM_PIPE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessShareSetInfo On pipe %wZ Class %08lX\n",
                      &Ccb->DirectoryCB->NameInformation.FileName,
                      ulFileInformationClass));

        pPipeInfo = AFSLockSystemBuffer( Irp,
                                         pIrpSp->Parameters.SetFile.Length);

        if( pPipeInfo == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_PIPE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSProcessShareSetInfo Failed to lock buffer on pipe %wZ\n",
                          &Ccb->DirectoryCB->NameInformation.FileName));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Send the request to the service
        //

        ntStatus = AFSNotifySetPipeInfo( Ccb,
                                         (ULONG)ulFileInformationClass,
                                         pIrpSp->Parameters.SetFile.Length,
                                         pPipeInfo);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_PIPE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSProcessShareSetInfo Failed to send request to service on pipe %wZ Status %08lX\n",
                          &Ccb->DirectoryCB->NameInformation.FileName,
                          ntStatus));

            try_return( ntStatus);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_PIPE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessShareSetInfo Completed request on pipe %wZ Class %08lX\n",
                      &Ccb->DirectoryCB->NameInformation.FileName,
                      ulFileInformationClass));

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSProcessShareQueryInfo( IN IRP *Irp,
                          IN AFSFcb *Fcb,
                          IN AFSCcb *Ccb)
{

    UNREFERENCED_PARAMETER(Fcb);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    FILE_INFORMATION_CLASS ulFileInformationClass;
    void *pPipeInfo = NULL;

    __Enter
    {

        ulFileInformationClass = pIrpSp->Parameters.QueryFile.FileInformationClass;

        AFSDbgTrace(( AFS_SUBSYSTEM_PIPE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessShareQueryInfo On pipe %wZ Class %08lX\n",
                      &Ccb->DirectoryCB->NameInformation.FileName,
                      ulFileInformationClass));

        pPipeInfo = AFSLockSystemBuffer( Irp,
                                         pIrpSp->Parameters.QueryFile.Length);

        if( pPipeInfo == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_PIPE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSProcessShareQueryInfo Failed to lock buffer on pipe %wZ\n",
                          &Ccb->DirectoryCB->NameInformation.FileName));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Send the request to the service
        //

        ntStatus = AFSNotifyQueryPipeInfo( Ccb,
                                           (ULONG)ulFileInformationClass,
                                           pIrpSp->Parameters.QueryFile.Length,
                                           pPipeInfo,
                                           (ULONG *)&Irp->IoStatus.Information);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_PIPE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSProcessShareQueryInfo Failed to send request to service on pipe %wZ Status %08lX\n",
                          &Ccb->DirectoryCB->NameInformation.FileName,
                          ntStatus));

            try_return( ntStatus);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_PIPE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessShareQueryInfo Completed request on pipe %wZ Class %08lX\n",
                      &Ccb->DirectoryCB->NameInformation.FileName,
                      ulFileInformationClass));

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSProcessPIOCtlQueryInfo( IN IRP *Irp,
                           IN AFSFcb *Fcb,
                           IN AFSCcb *Ccb,
                           IN OUT LONG *Length)
{

    UNREFERENCED_PARAMETER(Fcb);
    UNREFERENCED_PARAMETER(Ccb);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    FILE_INFORMATION_CLASS ulFileInformationClass;

    __Enter
    {

        ulFileInformationClass = pIrpSp->Parameters.QueryFile.FileInformationClass;

        switch( ulFileInformationClass)
        {

            case FileBasicInformation:
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSProcessPIOCtlQueryInfo (FileBasicInformation)\n"));

                if ( *Length >= sizeof( FILE_BASIC_INFORMATION))
                {
                    PFILE_BASIC_INFORMATION pBasic = (PFILE_BASIC_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

                    pBasic->CreationTime.QuadPart = 0;
                    pBasic->LastAccessTime.QuadPart = 0;
                    pBasic->ChangeTime.QuadPart = 0;
                    pBasic->LastWriteTime.QuadPart = 0;
                    pBasic->FileAttributes = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;

                    *Length -= sizeof( FILE_BASIC_INFORMATION);
                }
                else
                {
                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                }

                break;
            }

            case FileStandardInformation:
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSProcessPIOCtlQueryInfo (FileStandardInformation)\n"));

                if ( *Length >= sizeof( FILE_STANDARD_INFORMATION))
                {
                    PFILE_STANDARD_INFORMATION pStandard = (PFILE_STANDARD_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

                    pStandard->NumberOfLinks = 1;
                    pStandard->DeletePending = 0;
                    pStandard->AllocationSize.QuadPart = 0;
                    pStandard->EndOfFile.QuadPart = 0;
                    pStandard->Directory = 0;

                    *Length -= sizeof( FILE_STANDARD_INFORMATION);
                }
                else
                {
                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                }

                break;
            }

            case FileNormalizedNameInformation:
            case FileNameInformation:
            {

                ULONG ulCopyLength = 0;
                AFSFcb *pFcb = NULL;
                AFSCcb *pCcb = NULL;
                USHORT usFullNameLength = 0;
                PFILE_NAME_INFORMATION pNameInfo = (PFILE_NAME_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
                UNICODE_STRING uniName;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSProcessPIOCtlQueryInfo (FileNameInformation)\n"));

                pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;
                pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

                if( *Length < FIELD_OFFSET( FILE_NAME_INFORMATION, FileName))
                {
                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                RtlZeroMemory( pNameInfo,
                               *Length);

                usFullNameLength = sizeof( WCHAR) +
                                            AFSServerName.Length +
                                            pCcb->FullFileName.Length;

                if( *Length >= (LONG)(FIELD_OFFSET( FILE_NAME_INFORMATION, FileName) + (LONG)usFullNameLength))
                {
                    ulCopyLength = (LONG)usFullNameLength;
                }
                else
                {
                    ulCopyLength = *Length - FIELD_OFFSET( FILE_NAME_INFORMATION, FileName);
                    ntStatus = STATUS_BUFFER_OVERFLOW;
                }

                pNameInfo->FileNameLength = (ULONG)usFullNameLength;

                *Length -= FIELD_OFFSET( FILE_NAME_INFORMATION, FileName);

                if( ulCopyLength > 0)
                {

                    pNameInfo->FileName[ 0] = L'\\';
                    ulCopyLength -= sizeof( WCHAR);

                    *Length -= sizeof( WCHAR);

                    if( ulCopyLength >= AFSServerName.Length)
                    {

                        RtlCopyMemory( &pNameInfo->FileName[ 1],
                                       AFSServerName.Buffer,
                                       AFSServerName.Length);

                        ulCopyLength -= AFSServerName.Length;
                        *Length -= AFSServerName.Length;

                        if( ulCopyLength >= pCcb->FullFileName.Length)
                        {

                            RtlCopyMemory( &pNameInfo->FileName[ 1 + (AFSServerName.Length/sizeof( WCHAR))],
                                           pCcb->FullFileName.Buffer,
                                           pCcb->FullFileName.Length);

                            ulCopyLength -= pCcb->FullFileName.Length;
                            *Length -= pCcb->FullFileName.Length;

                            uniName.Length = (USHORT)pNameInfo->FileNameLength;
                            uniName.MaximumLength = uniName.Length;
                            uniName.Buffer = pNameInfo->FileName;
                        }
                        else
                        {

                            RtlCopyMemory( &pNameInfo->FileName[ 1 + (AFSServerName.Length/sizeof( WCHAR))],
                                           pCcb->FullFileName.Buffer,
                                           ulCopyLength);

                            *Length -= ulCopyLength;

                            uniName.Length = (USHORT)(sizeof( WCHAR) + AFSServerName.Length + ulCopyLength);
                            uniName.MaximumLength = uniName.Length;
                            uniName.Buffer = pNameInfo->FileName;
                        }

                        AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSProcessPIOCtlQueryInfo (FileNameInformation) Returning %wZ\n",
                                      &uniName));
                    }
                }

                break;
            }

            case FileInternalInformation:
            {

                PFILE_INTERNAL_INFORMATION pInternalInfo = (PFILE_INTERNAL_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSProcessPIOCtlQueryInfo (FileInternalInformation)\n"));

                if( *Length >= sizeof( FILE_INTERNAL_INFORMATION))
                {

                    pInternalInfo->IndexNumber.HighPart = 0;

                    pInternalInfo->IndexNumber.LowPart = 0;

                    *Length -= sizeof( FILE_INTERNAL_INFORMATION);
                }
                else
                {

                    ntStatus = STATUS_BUFFER_TOO_SMALL;
                }

                break;
            }

            case FileAllInformation:
            {
                ntStatus = STATUS_INVALID_PARAMETER;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSProcessPIOCtlQueryInfo (FileAllInformation) Not Implemented\n"));

                break;
            }

            case FileEaInformation:
            {
                ntStatus = STATUS_INVALID_PARAMETER;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSProcessPIOCtlQueryInfo (FileEaInformation) Not Implemented\n"));

                break;
            }

            case FilePositionInformation:
            {
                ntStatus = STATUS_INVALID_PARAMETER;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSProcessPIOCtlQueryInfo (FilePositionInformation) Not Implemented\n"));

                break;
            }

            case FileAlternateNameInformation:
            {
                ntStatus = STATUS_INVALID_PARAMETER;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSProcessPIOCtlQueryInfo (FileAlternateNameInformation) Not Implemented\n"));

                break;
            }

            case FileNetworkOpenInformation:
            {
                ntStatus = STATUS_INVALID_PARAMETER;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSProcessPIOCtlQueryInfo (FileNetworkOpenInformation) Not Implemented\n"));

                break;
            }

            case FileStreamInformation:
            {
                ntStatus = STATUS_INVALID_PARAMETER;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSProcessPIOCtlQueryInfo (FileStreamInformation) Not Implemented\n"));

                break;
            }

            case FileAttributeTagInformation:
            {
                ntStatus = STATUS_INVALID_PARAMETER;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSProcessPIOCtlQueryInfo (FileAttributeTagInformation) Not Implemented\n"));

                break;
            }

            case FileRemoteProtocolInformation:
            {
                ntStatus = STATUS_INVALID_PARAMETER;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSProcessPIOCtlQueryInfo (FileRemoteProtocolInformation) Not Implemented\n"));

                break;
            }

            case FileNetworkPhysicalNameInformation:
            {
                ntStatus = STATUS_INVALID_PARAMETER;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSProcessPIOCtlQueryInfo (FileNetworkPhysicalNameInformation) Not Implemented\n"));

                break;
            }

            default:
            {
                ntStatus = STATUS_INVALID_PARAMETER;

                AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSProcessPIOCtlQueryInfo Not handling request %08lX\n",
                              ulFileInformationClass));

                break;
            }
        }
    }

    AFSDbgTrace(( AFS_SUBSYSTEM_PIOCTL_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSProcessPIOCtlQueryInfo ntStatus %08lX\n",
                  ntStatus));

    return ntStatus;
}

