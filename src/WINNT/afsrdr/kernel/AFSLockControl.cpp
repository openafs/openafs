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
// File: AFSLockControl.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSLockControl( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulRequestType = 0;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    BOOLEAN bCompleteRequest = TRUE;
    AFSByteRangeLockRequestCB  stLockRequestCB;
    AFSByteRangeLockResultCB  stLockResultCB;
    AFSByteRangeUnlockRequestCB stUnlockRequestCB;
    AFSByteRangeUnlockResultCB stUnlockResultCB;
    ULONG           ulResultLen = 0;

    __try
    {

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        //
        // Acquire the main shared for adding the lock control to the list
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSLockControl Acquiring Fcb lock %08lX SHARED %08lX\n",
                                                          &pFcb->NPFcb->Resource,
                                                          PsGetCurrentThread());

        AFSAcquireShared( &pFcb->NPFcb->Resource,
                          TRUE);

        if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSLockControl Failing request against PIOCtl Fcb\n");

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }
        else if( pFcb->Header.NodeTypeCode == AFS_SPECIAL_SHARE_FCB)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSLockControl Failing request against SpecialShare Fcb\n");

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        //
        // Post the request to the service for checks
        //

        switch( pIrpSp->MinorFunction)
        {

            case IRP_MN_LOCK:
            {

                stLockRequestCB.Count = 1;

                stLockRequestCB.Request[ 0].LockType = AFS_BYTE_RANGE_LOCK_TYPE_EXCL;

                stLockRequestCB.Request[ 0].Offset = pIrpSp->Parameters.LockControl.ByteOffset;

                stLockRequestCB.Request[ 0].Length.QuadPart = pIrpSp->Parameters.LockControl.Length->QuadPart;

                ulResultLen = sizeof( AFSByteRangeLockResultCB);

                ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_BYTE_RANGE_LOCK,
                                              AFS_REQUEST_FLAG_SYNCHRONOUS,
                                              0,
                                              &pFcb->DirEntry->DirectoryEntry.FileName,
                                              &pFcb->DirEntry->DirectoryEntry.FileId,
                                              &stLockRequestCB,
                                              sizeof( AFSByteRangeLockRequestCB),
                                              &stLockResultCB,
                                              &ulResultLen);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLockControl (IRP_MN_LOCK) Failed to request lock for file %wZ Status %08lX\n", 
                                                                    &pFcb->DirEntry->DirectoryEntry.FileName,
                                                                    ntStatus);

                    try_return( ntStatus);
                }

                break;
            }

            case IRP_MN_UNLOCK_ALL:
            case IRP_MN_UNLOCK_ALL_BY_KEY:
            {

                ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK_ALL,
                                              AFS_REQUEST_FLAG_SYNCHRONOUS,
                                              0,
                                              &pFcb->DirEntry->DirectoryEntry.FileName,
                                              &pFcb->DirEntry->DirectoryEntry.FileId,
                                              NULL,
                                              0,
                                              NULL,
                                              NULL);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLockControl (IRP_MN_UNLOCK_ALL/BY_KEY) Failed to request lock for file %wZ Status %08lX\n", 
                                                                    &pFcb->DirEntry->DirectoryEntry.FileName,
                                                                    ntStatus);
                }

                //
                // Even on a failure we need to notify the rtl package of the unlock
                //

                break;
            }

            case IRP_MN_UNLOCK_SINGLE:
            {

                stUnlockRequestCB.Count = 1;

                stUnlockRequestCB.Request[ 0].LockType = AFS_BYTE_RANGE_LOCK_TYPE_EXCL;

                stUnlockRequestCB.Request[ 0].Offset = pIrpSp->Parameters.LockControl.ByteOffset;

                stUnlockRequestCB.Request[ 0].Length.QuadPart = pIrpSp->Parameters.LockControl.Length->QuadPart;

                ulResultLen = sizeof( AFSByteRangeUnlockResultCB);

                ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK,
                                              AFS_REQUEST_FLAG_SYNCHRONOUS,
                                              0,
                                              &pFcb->DirEntry->DirectoryEntry.FileName,
                                              &pFcb->DirEntry->DirectoryEntry.FileId,
                                              (void *)&stUnlockRequestCB,
                                              sizeof( AFSByteRangeUnlockRequestCB),
                                              (void *)&stUnlockResultCB,
                                              &ulResultLen);

                if( !NT_SUCCESS( ntStatus))
                {

                    AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLockControl (IRP_MN_UNLOCK_SINGLE) Failed to request lock for file %wZ Status %08lX\n", 
                                                                    &pFcb->DirEntry->DirectoryEntry.FileName,
                                                                    ntStatus);
                }

                break;
            }

            default:

                break;
        }

        //
        // Below here we won;t complete the request, it is handled by the lock package
        //

        bCompleteRequest = FALSE;
        
        //
        //  Now call the system package for actually processing the lock request
        //

        ntStatus = FsRtlProcessFileLock( &pFcb->Specific.File.FileLock, 
                                         Irp, 
                                         NULL);

try_exit:

        //
        // And drop it
        //

        AFSReleaseResource( &pFcb->NPFcb->Resource);

        if( bCompleteRequest)
        {

            AFSCompleteRequest( Irp,
                                ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()))
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSLockControl\n");

        //
        // Again, there is little point in failing this request but pass back some type of failure status
        //

        ntStatus = STATUS_UNSUCCESSFUL;

        AFSCompleteRequest( Irp,
                            ntStatus);
    }

    return ntStatus;
}
