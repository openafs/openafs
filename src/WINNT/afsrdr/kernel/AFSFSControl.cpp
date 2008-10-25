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
// File: AFSFSControl.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSFSControl( IN PDEVICE_OBJECT DeviceObject,
              IN PIRP Irp)
{

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

                AFSPrint("AFSFSControl Processing mount volume request\n");

                break;

            case IRP_MN_VERIFY_VOLUME:

                AFSPrint("AFSFSControl Processing verify volume request\n");

                break;

            default:

                AFSPrint("AFSFSControl Processing unknown request %08lX\n", pIrpSp->MinorFunction);

                break;
        }

        AFSCompleteRequest( Irp,
                              ntStatus);

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSFSControl\n");
    }

    return ntStatus;
}

NTSTATUS
AFSProcessUserFsRequest( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulFsControlCode;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp );

    ulFsControlCode = pIrpSp->Parameters.FileSystemControl.FsControlCode;

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

            //
            // Note that implementing this call will probably need us
            // to call the server as well as adding code in read and
            // write and caching.  Also that it is unlikely that
            // anyone will ever call us at this point - RDR doesn't
            // allow it
            //

            ntStatus = STATUS_NOT_IMPLEMENTED;

            AFSPrint("AFSProcessUserFsRequest Processing oplock request\n");
        
            break;

        case FSCTL_LOCK_VOLUME:

            AFSPrint("AFSProcessUserFsRequest Processing lock volume request\n");            
                
            break;

        case FSCTL_UNLOCK_VOLUME:

            AFSPrint("AFSProcessUserFsRequest Processing unlock volume request\n");

            break;

        case FSCTL_DISMOUNT_VOLUME:

            AFSPrint("AFSProcessUserFsRequest Processing dismount volume request\n");

            break;

        case FSCTL_MARK_VOLUME_DIRTY:

            AFSPrint("AFSProcessUserFsRequest Processing mark volume dirty request\n");

            break;

        case FSCTL_IS_VOLUME_DIRTY:

            AFSPrint("AFSProcessUserFsRequest Processing IsVolumeDirty request\n");

            break;

        case FSCTL_IS_VOLUME_MOUNTED:

            AFSPrint("AFSProcessUserFsRequest Processing IsVolumeMounted request\n");

            break;

        case FSCTL_IS_PATHNAME_VALID:
            
            AFSPrint("AFSProcessUserFsRequest Processing IsPathNameValid request\n");

            break;

        case FSCTL_QUERY_RETRIEVAL_POINTERS:
            
            AFSPrint("AFSProcessUserFsRequest Processing QueryRetrievalPntrs request\n");

            break;

        case FSCTL_FILESYSTEM_GET_STATISTICS:
            
            AFSPrint("AFSProcessUserFsRequest Processing GetFSStats request\n");

            break;

        case FSCTL_GET_VOLUME_BITMAP:
            
            AFSPrint("AFSProcessUserFsRequest Processing GetVolumeBitmap request\n");

            break;

        case FSCTL_GET_RETRIEVAL_POINTERS:
            
            AFSPrint("AFSProcessUserFsRequest Processing GetRetrievalPntrs request\n");

            break;

        case FSCTL_MOVE_FILE:

            AFSPrint("AFSProcessUserFsRequest Processing MoveFile request\n");

            break;

        case FSCTL_ALLOW_EXTENDED_DASD_IO:

            AFSPrint("AFSProcessUserFsRequest Processing AllowDASD IO request\n");

            break;

        case FSCTL_GET_REPARSE_POINT:
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
            break;

        case FSCTL_SET_REPARSE_POINT:
            AFSPrint("AFSProcessUserFsRequest Get reparse data buffer for %wZ\n", &pIrpSp->FileObject->FileName);
            break;

        default :

            AFSPrint("AFSProcessUserFsRequest Processing Default handler %08lX\n", ulFsControlCode);

            break;
    }

    return ntStatus;
}
