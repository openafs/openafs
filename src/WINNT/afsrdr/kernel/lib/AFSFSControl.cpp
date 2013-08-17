/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011, 2012, 2013 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
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
// File: AFSFSControl.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSFSControl( IN PDEVICE_OBJECT LibDeviceObject,
              IN PIRP Irp)
{

    UNREFERENCED_PARAMETER(LibDeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        switch( pIrpSp->MinorFunction)
        {

            case IRP_MN_USER_FS_REQUEST:

                ntStatus = AFSProcessUserFsRequest( Irp);

                break;

            case IRP_MN_MOUNT_VOLUME:

                break;

            case IRP_MN_VERIFY_VOLUME:

                break;

            default:

                break;
        }

        AFSCompleteRequest( Irp,
                              ntStatus);

    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSFSControl\n"));

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

static BOOLEAN
AFSParseMountPointTarget( IN  UNICODE_STRING *Target,
                          OUT USHORT         *Type,
                          OUT UNICODE_STRING *Volume,
                          OUT UNICODE_STRING *Cell)
{
    // Targets are of the form <type>[<cell>:]<volume>

    *Type = Target->Buffer[ 0];

    // Extract the cell name (if any)

    Cell->Buffer = &Target->Buffer[ 1];

    // Search for colon separator or end of counted string

    for ( Cell->Length = 0; Cell->Length < Target->Length - sizeof( WCHAR); Cell->Length += sizeof( WCHAR))
    {

        if ( Cell->Buffer[ Cell->Length / sizeof( WCHAR)] == L':')
        {
            break;
        }
    }

    // If a colon is not found, it means there is no cell

    if ( Cell->Length < Target->Length - sizeof( WCHAR) &&
         Cell->Buffer[ Cell->Length / sizeof( WCHAR)] == L':')
    {

        Cell->MaximumLength = Cell->Length;

        if ( Cell->Length > Target->Length - 2 * sizeof( WCHAR))
        {
            // Invalid target string if there is no room for
            // the volume name.

            return FALSE;
        }

        Volume->Length = Volume->MaximumLength = (Target->Length - Cell->Length - 2 * sizeof( WCHAR));

        Volume->Buffer = &Target->Buffer[ Cell->Length / sizeof( WCHAR) + 2];
    }
    else
    {
        // There is no cell

        Volume->Length = Volume->MaximumLength = Cell->Length;

        Volume->Buffer = Cell->Buffer;

        Cell->Length = Cell->MaximumLength = 0;

        Cell->Buffer = NULL;
    }

    return TRUE;
}

NTSTATUS
AFSProcessUserFsRequest( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulFsControlCode;
    AFSFcb *pFcb = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp );
    AFSCcb *pCcb = NULL;
    ULONG ulOutputBufferLen, ulInputBufferLen;

    __Enter
    {

        ulFsControlCode = pIrpSp->Parameters.FileSystemControl.FsControlCode;

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        if( pFcb == NULL ||
            pCcb == NULL ||
            pCcb->DirectoryCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSProcessUserFsRequest Invalid Fcb\n"));

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        if( pFcb->Header.NodeTypeCode == AFS_SPECIAL_SHARE_FCB)
        {

            ntStatus = AFSProcessShareFsCtrl( Irp,
                                              pFcb,
                                              pCcb);

            try_return( ntStatus);
        }

        ulOutputBufferLen = pIrpSp->Parameters.FileSystemControl.OutputBufferLength;
        ulInputBufferLen = pIrpSp->Parameters.FileSystemControl.InputBufferLength;

        //
        // Process the request
        //

        switch( ulFsControlCode )
        {

            case FSCTL_REQUEST_OPLOCK_LEVEL_1:
            case FSCTL_REQUEST_OPLOCK_LEVEL_2:
            case FSCTL_REQUEST_BATCH_OPLOCK:
            case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
            case FSCTL_OPBATCH_ACK_CLOSE_PENDING:
            case FSCTL_OPLOCK_BREAK_NOTIFY:
            case FSCTL_OPLOCK_BREAK_ACK_NO_2:
            case FSCTL_REQUEST_FILTER_OPLOCK :
            {
                //
                // Note that implementing this call will probably need us
                // to call the server as well as adding code in read and
                // write and caching.  Also that it is unlikely that
                // anyone will ever call us at this point - RDR doesn't
                // allow it
                //

                ntStatus = STATUS_NOT_IMPLEMENTED;

                break;
            }

            case FSCTL_LOCK_VOLUME:
            {
                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_LOCK_VOLUME request\n"));

                ntStatus = STATUS_NOT_IMPLEMENTED;

                break;
            }

            case FSCTL_UNLOCK_VOLUME:
            {
                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_UNLOCK_VOLUME request\n"));

                ntStatus = STATUS_NOT_IMPLEMENTED;

                break;
            }

            case FSCTL_DISMOUNT_VOLUME:
            {
                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_DISMOUNT_VOLUME request\n"));

                ntStatus = STATUS_NOT_IMPLEMENTED;

                break;
            }

            case FSCTL_MARK_VOLUME_DIRTY:
            {
                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_MARK_VOLUME_DIRTY request\n"));

                ntStatus = STATUS_NOT_IMPLEMENTED;

                break;
            }

            case FSCTL_IS_VOLUME_DIRTY:
            {
                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_IS_VOLUME_DIRTY request\n"));

                ntStatus = STATUS_NOT_IMPLEMENTED;

                break;
            }

            case FSCTL_IS_VOLUME_MOUNTED:
            {
                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_IS_VOLUME_MOUNTED request\n"));

                ntStatus = STATUS_NOT_IMPLEMENTED;

                break;
            }

            case FSCTL_IS_PATHNAME_VALID:
            {
                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_IS_PATHNAME_VALID request\n"));

                ntStatus = STATUS_SUCCESS;

                break;
            }

#ifndef FSCTL_CSC_INTERNAL
#define FSCTL_CSC_INTERNAL                  CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 107, METHOD_NEITHER, FILE_ANY_ACCESS)
#endif
            case FSCTL_CSC_INTERNAL:
            {
                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_CSC_INTERNAL request\n"));

                ntStatus = STATUS_INVALID_DEVICE_REQUEST;

                break;
            }

            case FSCTL_GET_REPARSE_POINT:
            {

                REPARSE_GUID_DATA_BUFFER *pReparseBuffer = (REPARSE_GUID_DATA_BUFFER *)Irp->AssociatedIrp.SystemBuffer;
                REPARSE_DATA_BUFFER *pMSFTReparseBuffer = (REPARSE_DATA_BUFFER *)Irp->AssociatedIrp.SystemBuffer;
                ULONG ulRemainingLen = ulOutputBufferLen;
                AFSReparseTagInfo *pReparseInfo = NULL;
                BOOLEAN bRelative = FALSE;
                BOOLEAN bDriveLetter = FALSE;
                WCHAR * PathBuffer = NULL;

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_GET_REPARSE_POINT request %wZ Type 0x%x Attrib 0x%x\n",
                              &pCcb->DirectoryCB->NameInformation.FileName,
                              pFcb->ObjectInformation->FileType,
                              pFcb->ObjectInformation->FileAttributes));

                //
                // Check if we have the reparse entry set on the entry
                //

                if( !BooleanFlagOn( pFcb->ObjectInformation->FileAttributes, FILE_ATTRIBUTE_REPARSE_POINT))
                {

                    ntStatus = STATUS_NOT_A_REPARSE_POINT;

                    break;
                }


                switch ( pFcb->ObjectInformation->FileType) {
                case AFS_FILE_TYPE_MOUNTPOINT:

                    if( ulOutputBufferLen < FIELD_OFFSET( REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer.DataBuffer))
                    {

                        ntStatus = STATUS_BUFFER_TOO_SMALL;

                        Irp->IoStatus.Information = FIELD_OFFSET( REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer.DataBuffer);

                        break;
                    }

                    ulRemainingLen -= FIELD_OFFSET( REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer.DataBuffer);

                    break;

                default:

                    if( ulOutputBufferLen < FIELD_OFFSET( REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer))
                    {

                        ntStatus = STATUS_BUFFER_TOO_SMALL;

                        Irp->IoStatus.Information = FIELD_OFFSET( REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer);

                        break;
                    }

                    ulRemainingLen -= FIELD_OFFSET( REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer);

                    break;
                }

                //
                // Populate the data in the reparse buffer
                //

                pReparseBuffer->ReparseDataLength  = 0;

                AFSAcquireExcl( &pCcb->DirectoryCB->NonPaged->Lock,
                                TRUE);

                if( pCcb->DirectoryCB->NameInformation.TargetName.Length == 0)
                {

                    //
                    // We'll reset the DV to ensure we validate the metadata content
                    //

                    pFcb->ObjectInformation->DataVersion.QuadPart = (ULONGLONG)-1;

                    SetFlag( pFcb->ObjectInformation->Flags, AFS_OBJECT_FLAGS_VERIFY);

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSProcessUserFsRequest Verifying symlink %wZ FID %08lX-%08lX-%08lX-%08lX\n",
                                  &pCcb->DirectoryCB->NameInformation.FileName,
                                  pFcb->ObjectInformation->FileId.Cell,
                                  pFcb->ObjectInformation->FileId.Volume,
                                  pFcb->ObjectInformation->FileId.Vnode,
                                  pFcb->ObjectInformation->FileId.Unique));

                    ntStatus = AFSVerifyEntry( &pCcb->AuthGroup,
					       pCcb->DirectoryCB,
					       FALSE);

                    if( !NT_SUCCESS( ntStatus))
                    {

                        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                      AFS_TRACE_LEVEL_ERROR,
                                      "AFSProcessUserFsRequest Failed to verify symlink %wZ FID %08lX-%08lX-%08lX-%08lX Status %08lX\n",
                                      &pCcb->DirectoryCB->NameInformation.FileName,
                                      pFcb->ObjectInformation->FileId.Cell,
                                      pFcb->ObjectInformation->FileId.Volume,
                                      pFcb->ObjectInformation->FileId.Vnode,
                                      pFcb->ObjectInformation->FileId.Unique,
                                      ntStatus));

                        AFSReleaseResource( &pCcb->DirectoryCB->NonPaged->Lock);

                        break;
                    }
                }

                pReparseInfo = (AFSReparseTagInfo *)&pReparseBuffer->GenericReparseBuffer.DataBuffer[ 0];

                switch( pFcb->ObjectInformation->FileType)
                {

                    case AFS_FILE_TYPE_SYMLINK:
                    {

                        if( pCcb->DirectoryCB->NameInformation.TargetName.Length == 0)
                        {

                            ntStatus = STATUS_REPARSE_POINT_NOT_RESOLVED;

                            break;
                        }

                        bRelative = AFSIsRelativeName( &pCcb->DirectoryCB->NameInformation.TargetName);

                        if ( bRelative)
                        {

                            if( ulRemainingLen < (ULONG) FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                                pCcb->DirectoryCB->NameInformation.TargetName.Length)
                            {

                                ntStatus = STATUS_BUFFER_TOO_SMALL;

                                Irp->IoStatus.Information = FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                                    pCcb->DirectoryCB->NameInformation.TargetName.Length;

                                break;
                            }

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.Flags = SYMLINK_FLAG_RELATIVE;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength =
                                pCcb->DirectoryCB->NameInformation.TargetName.Length;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength =
                                pCcb->DirectoryCB->NameInformation.TargetName.Length;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameOffset = 0;

                            PathBuffer = pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PathBuffer;

                            RtlCopyMemory( PathBuffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Buffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Length);

                            pReparseBuffer->ReparseDataLength = FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) -
                                FIELD_OFFSET( REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer) +
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength;
                        }
                        else
                        {

                            if( ulRemainingLen < (ULONG) FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                                /* Display Name */
                                pCcb->DirectoryCB->NameInformation.TargetName.Length + 1 * sizeof( WCHAR) +
                                /* Substitute Name */
                                pCcb->DirectoryCB->NameInformation.TargetName.Length + 7 * sizeof( WCHAR))
                            {

                                ntStatus = STATUS_BUFFER_TOO_SMALL;

                                Irp->IoStatus.Information = FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                                    pCcb->DirectoryCB->NameInformation.TargetName.Length + 1 * sizeof( WCHAR) +
                                    pCcb->DirectoryCB->NameInformation.TargetName.Length + 7 * sizeof( WCHAR);

                                break;
                            }

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.Flags = 0;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength =
                                pCcb->DirectoryCB->NameInformation.TargetName.Length + 7 * sizeof( WCHAR);

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength =
                                pCcb->DirectoryCB->NameInformation.TargetName.Length + 1 * sizeof( WCHAR);

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset =
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameOffset = 0;

                            PathBuffer = pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PathBuffer;

                            /* Display Name */
                            *PathBuffer++ = L'\\';

                            RtlCopyMemory( PathBuffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Buffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Length);

                            PathBuffer += pCcb->DirectoryCB->NameInformation.TargetName.Length / sizeof( WCHAR);

                            /* Substitute Name */
                            *PathBuffer++ = L'\\';
                            *PathBuffer++ = L'?';
                            *PathBuffer++ = L'?';
                            *PathBuffer++ = L'\\';
                            *PathBuffer++ = L'U';
                            *PathBuffer++ = L'N';
                            *PathBuffer++ = L'C';

                            RtlCopyMemory( PathBuffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Buffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Length);

                            pReparseBuffer->ReparseDataLength = FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) -
                                FIELD_OFFSET( REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer) +
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength +
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength;
                        }

                        break;
                    }

                    case AFS_FILE_TYPE_MOUNTPOINT:
                    {
                        UNICODE_STRING Cell, Volume;
                        USHORT Type;

                        if( pCcb->DirectoryCB->NameInformation.TargetName.Length == 0)
                        {
                            ntStatus = STATUS_REPARSE_POINT_NOT_RESOLVED;

                            break;
                        }

                        if ( !AFSParseMountPointTarget( &pCcb->DirectoryCB->NameInformation.TargetName,
                                                        &Type,
                                                        &Volume,
                                                        &Cell))
                        {
                            ntStatus = STATUS_INVALID_PARAMETER;

                            break;
                        }

                        if( ulRemainingLen < (ULONG) FIELD_OFFSET( AFSReparseTagInfo, AFSMountPoint.Buffer) + Volume.Length + Cell.Length)
                        {

                            ntStatus = STATUS_BUFFER_TOO_SMALL;

                            Irp->IoStatus.Information = FIELD_OFFSET( REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer.DataBuffer) +
                                                        FIELD_OFFSET( AFSReparseTagInfo, AFSMountPoint.Buffer) +
                                                        Volume.Length + Cell.Length;

                            break;
                        }

                        pReparseInfo->SubTag = OPENAFS_SUBTAG_MOUNTPOINT;

                        pReparseInfo->AFSMountPoint.Type = Type;

                        pReparseInfo->AFSMountPoint.MountPointCellLength = Cell.Length;

                        pReparseInfo->AFSMountPoint.MountPointVolumeLength = Volume.Length;

                        RtlCopyMemory( pReparseInfo->AFSMountPoint.Buffer,
                                       Cell.Buffer,
                                       Cell.Length);

                        RtlCopyMemory( &pReparseInfo->AFSMountPoint.Buffer[ Cell.Length / sizeof( WCHAR)],
                                       Volume.Buffer,
                                       Volume.Length);

                        pReparseBuffer->ReparseDataLength = (FIELD_OFFSET( AFSReparseTagInfo, AFSMountPoint.Buffer) + Volume.Length + Cell.Length);

                        break;
                    }

                    case AFS_FILE_TYPE_DFSLINK:
                    {

                        if( pCcb->DirectoryCB->NameInformation.TargetName.Length == 0)
                        {

                            ntStatus = STATUS_REPARSE_POINT_NOT_RESOLVED;

                            break;
                        }

                        bRelative = ( pCcb->DirectoryCB->NameInformation.TargetName.Buffer[0] == L'\\');

                        bDriveLetter = (bRelative == FALSE && pCcb->DirectoryCB->NameInformation.TargetName.Buffer[1] == L':');

                        if ( bRelative)
                        {

                            if( ulRemainingLen < (ULONG) FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                                pCcb->DirectoryCB->NameInformation.TargetName.Length)
                            {

                                ntStatus = STATUS_BUFFER_TOO_SMALL;

                                Irp->IoStatus.Information = FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                                    pCcb->DirectoryCB->NameInformation.TargetName.Length;

                                break;
                            }

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.Flags = SYMLINK_FLAG_RELATIVE;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength =
                                pCcb->DirectoryCB->NameInformation.TargetName.Length;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength =
                                pCcb->DirectoryCB->NameInformation.TargetName.Length;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameOffset = 0;

                            PathBuffer = pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PathBuffer;

                            RtlCopyMemory( PathBuffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Buffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Length);

                            pReparseBuffer->ReparseDataLength = FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) -
                                FIELD_OFFSET( REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer) +
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength;
                        }
                        else if ( bDriveLetter)
                        {

                            if( ulRemainingLen < (ULONG) FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                                /* Display Name */
                                pCcb->DirectoryCB->NameInformation.TargetName.Length +
                                /* Substitute Name */
                                pCcb->DirectoryCB->NameInformation.TargetName.Length + 4 * sizeof( WCHAR))
                            {

                                ntStatus = STATUS_BUFFER_TOO_SMALL;

                                Irp->IoStatus.Information = FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                                    pCcb->DirectoryCB->NameInformation.TargetName.Length +
                                    pCcb->DirectoryCB->NameInformation.TargetName.Length + 4 * sizeof( WCHAR);

                                break;
                            }

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.Flags = 0;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength =
                                pCcb->DirectoryCB->NameInformation.TargetName.Length + 4 * sizeof( WCHAR);

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength =
                                pCcb->DirectoryCB->NameInformation.TargetName.Length;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset =
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameOffset = 0;

                            PathBuffer = pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PathBuffer;

                            /* Display Name */
                            RtlCopyMemory( PathBuffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Buffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Length);

                            PathBuffer += pCcb->DirectoryCB->NameInformation.TargetName.Length / sizeof( WCHAR);

                            /* Substitute Name */
                            *PathBuffer++ = L'\\';
                            *PathBuffer++ = L'?';
                            *PathBuffer++ = L'?';
                            *PathBuffer++ = L'\\';

                            RtlCopyMemory( PathBuffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Buffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Length);

                            pReparseBuffer->ReparseDataLength = FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) -
                                FIELD_OFFSET( REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer) +
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength +
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength;
                        }
                        else
                        {

                            if( ulRemainingLen < (ULONG) FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                                /* Display Name */
                                pCcb->DirectoryCB->NameInformation.TargetName.Length + 1 * sizeof( WCHAR) +
                                /* Substitute Name */
                                pCcb->DirectoryCB->NameInformation.TargetName.Length + 7 * sizeof( WCHAR))
                            {

                                ntStatus = STATUS_BUFFER_TOO_SMALL;

                                Irp->IoStatus.Information = FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) +
                                    pCcb->DirectoryCB->NameInformation.TargetName.Length + 1 * sizeof( WCHAR) +
                                    pCcb->DirectoryCB->NameInformation.TargetName.Length + 7 * sizeof( WCHAR);

                                break;
                            }

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.Flags = 0;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength =
                                pCcb->DirectoryCB->NameInformation.TargetName.Length + 7 * sizeof( WCHAR);

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength =
                                pCcb->DirectoryCB->NameInformation.TargetName.Length + 1 * sizeof( WCHAR);

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset =
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength;

                            pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameOffset = 0;

                            PathBuffer = pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PathBuffer;

                            /* Display Name */
                            *PathBuffer++ = L'\\';

                            RtlCopyMemory( PathBuffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Buffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Length);

                            PathBuffer += pCcb->DirectoryCB->NameInformation.TargetName.Length / sizeof( WCHAR);

                            /* Substitute Name */
                            *PathBuffer++ = L'\\';
                            *PathBuffer++ = L'?';
                            *PathBuffer++ = L'?';
                            *PathBuffer++ = L'\\';
                            *PathBuffer++ = L'U';
                            *PathBuffer++ = L'N';
                            *PathBuffer++ = L'C';

                            RtlCopyMemory( PathBuffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Buffer,
                                           pCcb->DirectoryCB->NameInformation.TargetName.Length);

                            pReparseBuffer->ReparseDataLength = FIELD_OFFSET( REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) -
                                FIELD_OFFSET( REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer) +
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.PrintNameLength +
                                pMSFTReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength;
                        }

                        break;
                    }

                    default:

                        ntStatus = STATUS_NOT_A_REPARSE_POINT;

                        break;
                }

                if ( ntStatus == STATUS_SUCCESS)
                {

                    ulRemainingLen -= pReparseBuffer->ReparseDataLength;

                    if ( pFcb->ObjectInformation->FileType == AFS_FILE_TYPE_MOUNTPOINT)
                    {

                        pReparseBuffer->ReparseTag = IO_REPARSE_TAG_SURROGATE|IO_REPARSE_TAG_OPENAFS_DFS;

                        RtlCopyMemory( &pReparseBuffer->ReparseGuid,
                                       &GUID_AFS_REPARSE_GUID,
                                       sizeof( GUID));

                        Irp->IoStatus.Information = FIELD_OFFSET( REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer.DataBuffer)
                            + pReparseBuffer->ReparseDataLength;
                    }
                    else
                    {

                        pReparseBuffer->ReparseTag = IO_REPARSE_TAG_SYMLINK;

                        Irp->IoStatus.Information = FIELD_OFFSET( REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer)
                            + pReparseBuffer->ReparseDataLength;
                    }
                }

                AFSReleaseResource( &pCcb->DirectoryCB->NonPaged->Lock);

                break;
            }

            case FSCTL_SET_REPARSE_POINT:
            {

                REPARSE_GUID_DATA_BUFFER *pReparseGUIDBuffer = (REPARSE_GUID_DATA_BUFFER *)Irp->AssociatedIrp.SystemBuffer;
                REPARSE_DATA_BUFFER *pReparseBuffer = NULL;
                AFSReparseTagInfo *pReparseInfo = NULL;
                AFSObjectInfoCB *pParentObjectInfo = NULL;
                UNICODE_STRING uniTargetName;
                ULONGLONG ullIndex = 0;
                LONG lCount;

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSProcessUserFsRequest Processing FSCTL_SET_REPARSE_POINT request %wZ Type 0x%x Attrib 0x%x\n",
                              &pCcb->DirectoryCB->NameInformation.FileName,
                              pFcb->ObjectInformation->FileType,
                              pFcb->ObjectInformation->FileAttributes));

                if( ulInputBufferLen < FIELD_OFFSET( REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer.DataBuffer))
                {

                    ntStatus = STATUS_IO_REPARSE_DATA_INVALID;

                    break;
                }

                if( (pReparseGUIDBuffer->ReparseTag & 0x0000FFFF) == IO_REPARSE_TAG_OPENAFS_DFS)
                {

                    if( RtlCompareMemory( &pReparseGUIDBuffer->ReparseGuid,
                                          &GUID_AFS_REPARSE_GUID,
                                          sizeof( GUID)) != sizeof( GUID))
                    {

                        ntStatus = STATUS_REPARSE_ATTRIBUTE_CONFLICT;

                        break;
                    }

                    if( ulInputBufferLen < FIELD_OFFSET( REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer.DataBuffer) +
                                                                        sizeof( AFSReparseTagInfo))
                    {

                        ntStatus = STATUS_IO_REPARSE_DATA_INVALID;

                        break;
                    }

                    pReparseInfo = (AFSReparseTagInfo *)pReparseGUIDBuffer->GenericReparseBuffer.DataBuffer;

                    switch( pReparseInfo->SubTag)
                    {

                        case OPENAFS_SUBTAG_SYMLINK:
                        {

                            if( ulInputBufferLen < (ULONG)(FIELD_OFFSET( REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer.DataBuffer) +
                                                           FIELD_OFFSET( AFSReparseTagInfo, AFSSymLink.Buffer) +
                                                           pReparseInfo->AFSSymLink.SymLinkTargetLength))
                            {

                                ntStatus = STATUS_IO_REPARSE_DATA_INVALID;

                                break;
                            }

                            uniTargetName.Length = pReparseInfo->AFSSymLink.SymLinkTargetLength;
                            uniTargetName.MaximumLength = uniTargetName.Length;

                            uniTargetName.Buffer = (WCHAR *)pReparseInfo->AFSSymLink.Buffer;

                            break;
                        }

                        case OPENAFS_SUBTAG_UNC:
                        {

                            if( ulInputBufferLen < (ULONG)(FIELD_OFFSET( REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer.DataBuffer) +
                                                           FIELD_OFFSET( AFSReparseTagInfo, UNCReferral.Buffer) +
                                                           pReparseInfo->UNCReferral.UNCTargetLength))
                            {

                                ntStatus = STATUS_IO_REPARSE_DATA_INVALID;

                                break;
                            }

                            uniTargetName.Length = pReparseInfo->UNCReferral.UNCTargetLength;
                            uniTargetName.MaximumLength = uniTargetName.Length;

                            uniTargetName.Buffer = (WCHAR *)pReparseInfo->UNCReferral.Buffer;

                            break;
                        }

                        case OPENAFS_SUBTAG_MOUNTPOINT:
                            //
                            // Not yet handled
                            //
                        default:
                        {

                            ntStatus = STATUS_IO_REPARSE_DATA_INVALID;

                            break;
                        }
                    }
                }
                else
                {
                    //
                    // Handle Microsoft Reparse Tags
                    //

                    switch( pReparseGUIDBuffer->ReparseTag)
                    {

                        case IO_REPARSE_TAG_MOUNT_POINT:
                        {

                            pReparseBuffer = (REPARSE_DATA_BUFFER *)Irp->AssociatedIrp.SystemBuffer;

                            uniTargetName.Length = pReparseBuffer->MountPointReparseBuffer.PrintNameLength;
                            uniTargetName.MaximumLength = uniTargetName.Length;

                            uniTargetName.Buffer = (WCHAR *)((char *)pReparseBuffer->MountPointReparseBuffer.PathBuffer +
                                                              pReparseBuffer->MountPointReparseBuffer.PrintNameOffset);

                            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE_2,
                                          "AFSProcessUserFsRequest IO_REPARSE_TAG_MOUNT_POINT request %wZ\n",
                                          &uniTargetName));

                            ntStatus = STATUS_IO_REPARSE_DATA_INVALID;

                            break;
                        }

                        case IO_REPARSE_TAG_SYMLINK:
                        {

                            pReparseBuffer = (REPARSE_DATA_BUFFER *)Irp->AssociatedIrp.SystemBuffer;

                            uniTargetName.Length = pReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength;
                            uniTargetName.MaximumLength = uniTargetName.Length;

                            uniTargetName.Buffer = (WCHAR *)((char *)pReparseBuffer->SymbolicLinkReparseBuffer.PathBuffer +
                                                              pReparseBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset);

                            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                          AFS_TRACE_LEVEL_VERBOSE_2,
                                          "AFSProcessUserFsRequest IO_REPARSE_TAG_SYMLINK request %wZ\n",
                                          &uniTargetName));
                            break;
                        }

                        default:
                        {

                            ntStatus = STATUS_IO_REPARSE_DATA_INVALID;

                            break;
                        }
                    }
                }

                if( !NT_SUCCESS( ntStatus))
                {

                    break;
                }

                //
                // First thing is to locate/create our object information block
                // for this entry
                //

                AFSAcquireExcl( pFcb->ObjectInformation->VolumeCB->ObjectInfoTree.TreeLock,
                                TRUE);

                if ( AFSIsVolumeFID( &pFcb->ObjectInformation->ParentFileId))
                {

                    pParentObjectInfo = &pFcb->ObjectInformation->VolumeCB->ObjectInformation;
                }
                else
                {
                    ullIndex = AFSCreateLowIndex( &pFcb->ObjectInformation->ParentFileId);

                    ntStatus = AFSLocateHashEntry( pFcb->ObjectInformation->VolumeCB->ObjectInfoTree.TreeHead,
                                                   ullIndex,
                                                   (AFSBTreeEntry **)&pParentObjectInfo);
                }

                if ( NT_SUCCESS( ntStatus))
                {

                    lCount = AFSObjectInfoIncrement( pParentObjectInfo,
                                                     AFS_OBJECT_REFERENCE_FS_REQ);

                    AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSProcessUserFsRequest Increment count on object %p Cnt %d\n",
                                  pParentObjectInfo,
                                  lCount));

                    KeQueryTickCount( &pParentObjectInfo->LastAccessCount);
                }

                AFSReleaseResource( pFcb->ObjectInformation->VolumeCB->ObjectInfoTree.TreeLock);

                if ( NT_SUCCESS( ntStatus))
                {

                    //
                    // Extract out the information to the call to the service
                    //

                    ntStatus = AFSCreateSymlink( &pCcb->AuthGroup,
                                                 pParentObjectInfo,
                                                 &pCcb->DirectoryCB->NameInformation.FileName,
                                                 pFcb->ObjectInformation,
                                                 &uniTargetName);

                    AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE_2,
                                  "AFSProcessUserFsRequest Processed FSCTL_SET_REPARSE_POINT request %wZ Type 0x%x Attrib 0x%x Status %08lX\n",
                                  &pCcb->DirectoryCB->NameInformation.FileName,
                                  pFcb->ObjectInformation->FileType,
                                  pFcb->ObjectInformation->FileAttributes,
                                  ntStatus));

                    AFSAcquireShared( pFcb->ObjectInformation->VolumeCB->ObjectInfoTree.TreeLock,
                                      TRUE);

                    lCount = AFSObjectInfoDecrement( pParentObjectInfo,
                                                     AFS_OBJECT_REFERENCE_FS_REQ);

                    AFSDbgTrace(( AFS_SUBSYSTEM_OBJECT_REF_COUNTING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSProcessUserFsRequest Decrement count on object %p Cnt %d\n",
                                  pParentObjectInfo,
                                  lCount));

                    AFSReleaseResource( pFcb->ObjectInformation->VolumeCB->ObjectInfoTree.TreeLock);
                }

                break;
            }

            case FSCTL_DELETE_REPARSE_POINT:
            {

                REPARSE_GUID_DATA_BUFFER *pReparseBuffer = (REPARSE_GUID_DATA_BUFFER *)Irp->AssociatedIrp.SystemBuffer;

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_DELETE_REPARSE_POINT request %wZ Type 0x%x Attrib 0x%x\n",
                              &pCcb->DirectoryCB->NameInformation.FileName,
                              pFcb->ObjectInformation->FileType,
                              pFcb->ObjectInformation->FileAttributes));

                //
                // Check if we have the reparse entry set on the entry
                //

                if( !BooleanFlagOn( pFcb->ObjectInformation->FileAttributes, FILE_ATTRIBUTE_REPARSE_POINT))
                {

                    ntStatus = STATUS_NOT_A_REPARSE_POINT;

                    break;
                }

                if( ulInputBufferLen < FIELD_OFFSET( REPARSE_GUID_DATA_BUFFER, GenericReparseBuffer.DataBuffer))
                {

                    ntStatus = STATUS_INVALID_PARAMETER;

                    break;
                }

                if( (pReparseBuffer->ReparseTag & 0x0000FFFF) != IO_REPARSE_TAG_OPENAFS_DFS)
                {

                    ntStatus = STATUS_IO_REPARSE_TAG_MISMATCH;

                    break;
                }

                if( RtlCompareMemory( &pReparseBuffer->ReparseGuid,
                                      &GUID_AFS_REPARSE_GUID,
                                      sizeof( GUID)) != sizeof( GUID))
                {

                    ntStatus = STATUS_REPARSE_ATTRIBUTE_CONFLICT;

                    break;
                }

                //
                // Claim success.  The typical usage is setting delete on close
                // as the next operation on the reparse point before closing
                // the handle.
                //

                ntStatus = STATUS_SUCCESS;

                break;
            }

#ifndef FSCTL_SET_PURGE_FAILURE_MODE
#define FSCTL_SET_PURGE_FAILURE_MODE        CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 156, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

            case FSCTL_SET_PURGE_FAILURE_MODE:
            {

                //
                // For the time being just succeed this call
                //

                ntStatus = STATUS_SUCCESS;

                break;
            }

            default :
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing default (%08lX) request\n",
                              ulFsControlCode));

                ntStatus = STATUS_INVALID_DEVICE_REQUEST;

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSProcessShareFsCtrl( IN IRP *Irp,
                       IN AFSFcb *Fcb,
                       IN AFSCcb *Ccb)
{

    UNREFERENCED_PARAMETER(Fcb);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    ULONG ulOutputBufferLen = 0, ulInputBufferLen;
    ULONG ulFsControlCode;

    __Enter
    {

        ulFsControlCode = pIrpSp->Parameters.FileSystemControl.FsControlCode;

        ulOutputBufferLen = pIrpSp->Parameters.FileSystemControl.OutputBufferLength;
        ulInputBufferLen = pIrpSp->Parameters.FileSystemControl.InputBufferLength;

        switch( ulFsControlCode)
        {

            case FSCTL_PIPE_TRANSCEIVE:
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_PIPE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSProcessShareFsCtrl On pipe %wZ Class FSCTL_PIPE_TRANSCEIVE\n",
                              &Ccb->DirectoryCB->NameInformation.FileName));

                ntStatus = AFSNotifyPipeTransceive( Ccb,
                                                    ulInputBufferLen,
                                                    ulOutputBufferLen,
                                                    pIrpSp->Parameters.FileSystemControl.Type3InputBuffer,
                                                    Irp->UserBuffer,
                                                    (ULONG *)&Irp->IoStatus.Information);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_PIPE_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSProcessShareFsCtrl Failure on pipe %wZ Class FSCTL_PIPE_TRANSCEIVE Status %08lX\n",
                                  &Ccb->DirectoryCB->NameInformation.FileName,
                                  ntStatus));
                }

                break;
            }

            default:
            {

                AFSPrint( "AFSProcessShareFsCtrl (%08lX) For IPC$ input %08lX output %08lX\n",
                          ulFsControlCode,
                          ulInputBufferLen,
                          ulOutputBufferLen);

                break;
            }
        }
    }

    return ntStatus;
}
