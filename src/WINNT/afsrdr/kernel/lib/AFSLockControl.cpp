/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011, 2014 Your File System, Inc.
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
// File: AFSLockControl.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSLockControl( IN PDEVICE_OBJECT LibDeviceObject,
                  IN PIRP Irp)
{

    UNREFERENCED_PARAMETER(LibDeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;
    BOOLEAN bCompleteRequest = TRUE;
    AFSByteRangeLockRequestCB  stLockRequestCB;
    AFSByteRangeLockResultCB  stLockResultCB;
    AFSByteRangeUnlockRequestCB stUnlockRequestCB;
    AFSByteRangeUnlockResultCB stUnlockResultCB;
    ULONG           ulResultLen = 0;
    BOOLEAN         bReleaseResource = FALSE;
    IO_STATUS_BLOCK stIoStatus;

    __try
    {

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        pCcb = (AFSCcb *)pIrpSp->FileObject->FsContext2;

        if( pFcb == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSLockControl Attempted access (%p) when pFcb == NULL\n",
                          Irp));

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        //
        // Acquire the main shared for adding the lock control to the list
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSLockControl Acquiring Fcb lock %p SHARED %08lX\n",
                      &pFcb->NPFcb->Resource,
                      PsGetCurrentThread()));

        AFSAcquireShared( &pFcb->NPFcb->Resource,
                          TRUE);

        bReleaseResource = TRUE;

        if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSLockControl Failing request against PIOCtl Fcb\n"));

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }
        else if( pFcb->Header.NodeTypeCode == AFS_SPECIAL_SHARE_FCB)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSLockControl Failing request against SpecialShare Fcb\n"));

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }
        else if( pFcb->Header.NodeTypeCode == AFS_INVALID_FCB)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSLockControl Failing request against Invalid Fcb\n"));

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

                stLockRequestCB.ProcessId = (ULONGLONG)PsGetCurrentProcessId();

                stLockRequestCB.Request[ 0].LockType = AFS_BYTE_RANGE_LOCK_TYPE_EXCL;

                stLockRequestCB.Request[ 0].Offset = pIrpSp->Parameters.LockControl.ByteOffset;

                stLockRequestCB.Request[ 0].Length.QuadPart = pIrpSp->Parameters.LockControl.Length->QuadPart;

                ulResultLen = sizeof( AFSByteRangeLockResultCB);

                ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_BYTE_RANGE_LOCK,
                                              AFS_REQUEST_FLAG_SYNCHRONOUS,
                                              &pCcb->AuthGroup,
                                              &pCcb->DirectoryCB->NameInformation.FileName,
                                              &pFcb->ObjectInformation->FileId,
                                              pFcb->ObjectInformation->VolumeCB->VolumeInformation.Cell,
                                              pFcb->ObjectInformation->VolumeCB->VolumeInformation.CellLength,
                                              &stLockRequestCB,
                                              sizeof( AFSByteRangeLockRequestCB),
                                              &stLockResultCB,
                                              &ulResultLen);

                if( !NT_SUCCESS( ntStatus))
                {

                    try_return( ntStatus);
                }

                break;
            }

            case IRP_MN_UNLOCK_ALL:
            case IRP_MN_UNLOCK_ALL_BY_KEY:
            {
                //
                // Flush data and then release the locks on the file server
                //

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLockControl Acquiring Fcb SectionObject lock %p SHARED %08lX\n",
                              &pFcb->NPFcb->SectionObjectResource,
                              PsGetCurrentThread()));

                AFSAcquireShared( &pFcb->NPFcb->SectionObjectResource,
                                  TRUE);

		__try
		{

		    CcFlushCache( &pFcb->NPFcb->SectionObjectPointers,
				  NULL,
				  0,
				  &stIoStatus);
		}
		__except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()))
		{

		    ntStatus = GetExceptionCode();

                    stIoStatus.Status = ntStatus;

		    AFSDbgTrace(( 0,
				  0,
				  "EXCEPTION - AFSLockControl CcFlushCache failed FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
				  pFcb->ObjectInformation->FileId.Cell,
				  pFcb->ObjectInformation->FileId.Volume,
				  pFcb->ObjectInformation->FileId.Vnode,
				  pFcb->ObjectInformation->FileId.Unique,
				  ntStatus));
		}

                if( !NT_SUCCESS( stIoStatus.Status))
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLockControl CcFlushCache [1] failure FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX Bytes 0x%08lX\n",
                                  pFcb->ObjectInformation->FileId.Cell,
                                  pFcb->ObjectInformation->FileId.Volume,
                                  pFcb->ObjectInformation->FileId.Vnode,
                                  pFcb->ObjectInformation->FileId.Unique,
                                  stIoStatus.Status,
                                  stIoStatus.Information));

                    ntStatus = stIoStatus.Status;
                }

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLockControl Releasing Fcb SectionObject lock %p SHARED %08lX\n",
                              &pFcb->NPFcb->SectionObjectResource,
                              PsGetCurrentThread()));

                AFSReleaseResource( &pFcb->NPFcb->SectionObjectResource);

                RtlZeroMemory( &stUnlockRequestCB,
                               sizeof( AFSByteRangeUnlockRequestCB));

                stUnlockRequestCB.ProcessId = (ULONGLONG)PsGetCurrentProcessId();

                ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK_ALL,
                                              AFS_REQUEST_FLAG_SYNCHRONOUS,
                                              &pCcb->AuthGroup,
                                              &pCcb->DirectoryCB->NameInformation.FileName,
                                              &pFcb->ObjectInformation->FileId,
                                              pFcb->ObjectInformation->VolumeCB->VolumeInformation.Cell,
                                              pFcb->ObjectInformation->VolumeCB->VolumeInformation.CellLength,
                                              (void *)&stUnlockRequestCB,
                                              sizeof( AFSByteRangeUnlockRequestCB),
                                              NULL,
                                              NULL);

                //
                // Even on a failure we need to notify the rtl package of the unlock
                //

                break;
            }

            case IRP_MN_UNLOCK_SINGLE:
            {
                //
                // Flush the data and then release the file server locks
                //

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLockControl Acquiring Fcb SectionObject lock %p SHARED %08lX\n",
                              &pFcb->NPFcb->SectionObjectResource,
                              PsGetCurrentThread()));

                AFSAcquireShared( &pFcb->NPFcb->SectionObjectResource,
                                  TRUE);

		__try
		{

		    CcFlushCache( &pFcb->NPFcb->SectionObjectPointers,
				  &pIrpSp->Parameters.LockControl.ByteOffset,
				  pIrpSp->Parameters.LockControl.Length->LowPart,
				  &stIoStatus);
		}
		__except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()))
		{

		    ntStatus = GetExceptionCode();

                    stIoStatus.Status = ntStatus;

		    AFSDbgTrace(( 0,
				  0,
				  "EXCEPTION - AFSLockControl CcFlushCache failed FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX\n",
				  pFcb->ObjectInformation->FileId.Cell,
				  pFcb->ObjectInformation->FileId.Volume,
				  pFcb->ObjectInformation->FileId.Vnode,
				  pFcb->ObjectInformation->FileId.Unique,
				  ntStatus));
		}

		AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING|AFS_SUBSYSTEM_SECTION_OBJECT,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSLockControl Releasing Fcb SectionObject lock %p SHARED %08lX\n",
                              &pFcb->NPFcb->SectionObjectResource,
                              PsGetCurrentThread()));

                AFSReleaseResource( &pFcb->NPFcb->SectionObjectResource);

                if( !NT_SUCCESS( stIoStatus.Status))
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "AFSLockControl CcFlushCache [2] failure FID %08lX-%08lX-%08lX-%08lX Status 0x%08lX Bytes 0x%08lX\n",
                                  pFcb->ObjectInformation->FileId.Cell,
                                  pFcb->ObjectInformation->FileId.Volume,
                                  pFcb->ObjectInformation->FileId.Vnode,
                                  pFcb->ObjectInformation->FileId.Unique,
                                  stIoStatus.Status,
                                  stIoStatus.Information));

                    ntStatus = stIoStatus.Status;
                }

                stUnlockRequestCB.Count = 1;

                stUnlockRequestCB.ProcessId = (ULONGLONG)PsGetCurrentProcessId();

                stUnlockRequestCB.Request[ 0].LockType = AFS_BYTE_RANGE_LOCK_TYPE_EXCL;

                stUnlockRequestCB.Request[ 0].Offset = pIrpSp->Parameters.LockControl.ByteOffset;

                stUnlockRequestCB.Request[ 0].Length.QuadPart = pIrpSp->Parameters.LockControl.Length->QuadPart;

                ulResultLen = sizeof( AFSByteRangeUnlockResultCB);

                ntStatus = AFSProcessRequest( AFS_REQUEST_TYPE_BYTE_RANGE_UNLOCK,
                                              AFS_REQUEST_FLAG_SYNCHRONOUS,
                                              &pCcb->AuthGroup,
                                              &pCcb->DirectoryCB->NameInformation.FileName,
                                              &pFcb->ObjectInformation->FileId,
                                              pFcb->ObjectInformation->VolumeCB->VolumeInformation.Cell,
                                              pFcb->ObjectInformation->VolumeCB->VolumeInformation.CellLength,
                                              (void *)&stUnlockRequestCB,
                                              sizeof( AFSByteRangeUnlockRequestCB),
                                              (void *)&stUnlockResultCB,
                                              &ulResultLen);

                break;
            }

            default:

                break;
        }

        //
        // Below here we won't complete the request, it is handled by the lock package
        //

        bCompleteRequest = FALSE;

        //
        //  Now call the system package for actually processing the lock request
        //

        ntStatus = FsRtlProcessFileLock( &pFcb->Specific.File.FileLock,
                                         Irp,
                                         NULL);

try_exit:

        if( bReleaseResource)
        {

            //
            // And drop it
            //

            AFSReleaseResource( &pFcb->NPFcb->Resource);
        }

        if( bCompleteRequest)
        {

            AFSCompleteRequest( Irp,
                                ntStatus);
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()))
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSLockControl\n"));

        AFSDumpTraceFilesFnc();

        //
        // Again, there is little point in failing this request but pass back some type of failure status
        //

        ntStatus = STATUS_UNSUCCESSFUL;

        AFSCompleteRequest( Irp,
                            ntStatus);
    }

    return ntStatus;
}
