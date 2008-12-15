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

                break;

            case IRP_MN_VERIFY_VOLUME:

                break;

            default:

                break;
        }

        AFSCompleteRequest( Irp,
                              ntStatus);

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSFSControl\n");
    }

    return ntStatus;
}

NTSTATUS
AFSProcessUserFsRequest( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulFsControlCode;
    AFSFcb *pFcb = NULL;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp );

    __Enter
    {

        ulFsControlCode = pIrpSp->Parameters.FileSystemControl.FsControlCode;

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        if( pFcb == NULL ||
            pFcb->DirEntry == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE_2,
                          "AFSProcessUserFsRequest Invalid Fcb\n");

            try_return( ntStatus);
        }

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
            
                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing OpLock request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_LOCK_VOLUME:
                    
                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_LOCK_VOLUME request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_UNLOCK_VOLUME:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_UNLOCK_VOLUME request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_DISMOUNT_VOLUME:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_DISMOUNT_VOLUME request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_MARK_VOLUME_DIRTY:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_MARK_VOLUME_DIRTY request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_IS_VOLUME_DIRTY:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_IS_VOLUME_DIRTY request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_IS_VOLUME_MOUNTED:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_IS_VOLUME_MOUNTED request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_IS_PATHNAME_VALID:
                
                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_IS_PATHNAME_VALID request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_QUERY_RETRIEVAL_POINTERS:
                
                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_QUERY_RETRIEVAL_POINTERS request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_FILESYSTEM_GET_STATISTICS:
                
                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_FILESYSTEM_GET_STATISTICS request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_GET_VOLUME_BITMAP:
                
                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_GET_VOLUME_BITMAP request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_GET_RETRIEVAL_POINTERS:
                
                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_GET_RETRIEVAL_POINTERS request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_MOVE_FILE:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_MOVE_FILE request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_ALLOW_EXTENDED_DASD_IO:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_ALLOW_EXTENDED_DASD_IO request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_GET_REPARSE_POINT:

                ntStatus = STATUS_INVALID_DEVICE_REQUEST;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_GET_REPARSE_POINT request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            case FSCTL_SET_REPARSE_POINT:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing FSCTL_SET_REPARSE_POINT request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;

            default :

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE_2,
                              "AFSProcessUserFsRequest Processing default (%08lX) request on %wZ FID %08lX-%08lX-%08lX-%08lX\n", 
                              ulFsControlCode,
                              &pFcb->DirEntry->DirectoryEntry.FileName,
                              pFcb->DirEntry->DirectoryEntry.FileId.Cell,
                              pFcb->DirEntry->DirectoryEntry.FileId.Volume,
                              pFcb->DirEntry->DirectoryEntry.FileId.Vnode,
                              pFcb->DirEntry->DirectoryEntry.FileId.Unique);

                break;
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}
