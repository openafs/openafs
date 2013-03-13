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
// File: AFSSecurity.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSSetSecurity( IN PDEVICE_OBJECT LibDeviceObject,
                IN PIRP Irp)
{

    UNREFERENCED_PARAMETER(LibDeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_ERROR,
                      "AFSSetSecurity Entry for FO %p\n",
                      pIrpSp->FileObject));

        AFSCompleteRequest( Irp,
                            ntStatus);
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSSetSecurity\n"));

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

NTSTATUS
AFSQuerySecurity( IN PDEVICE_OBJECT LibDeviceObject,
                  IN PIRP Irp)
{

    UNREFERENCED_PARAMETER(LibDeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp;
    PMDL pUserBufferMdl = NULL;
    void *pLockedUserBuffer = NULL;
    ULONG ulSDLength = 0;
    SECURITY_INFORMATION SecurityInformation;
    PFILE_OBJECT pFileObject;
    AFSFcb *pFcb = NULL;
    AFSCcb *pCcb = NULL;

    __try
    {

        pIrpSp = IoGetCurrentIrpStackLocation( Irp);

        SecurityInformation = pIrpSp->Parameters.QuerySecurity.SecurityInformation;

        pFileObject = pIrpSp->FileObject;

        pFcb = (AFSFcb *)pFileObject->FsContext;

        pCcb = (AFSCcb *)pFileObject->FsContext2;

        AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSQuerySecurity (%p) Entry for FO %p SI %08lX\n",
                      Irp,
                      pFileObject,
                      SecurityInformation));

        if( pFcb == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQuerySecurity Attempted access (%p) when pFcb == NULL\n",
                          Irp));

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        if ( SecurityInformation & SACL_SECURITY_INFORMATION)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQuerySecurity Attempted access (%p) SACL\n",
                          Irp));

            try_return( ntStatus = STATUS_ACCESS_DENIED);
        }

        if( AFSDefaultSD == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQuerySecurity No default SD allocated\n"));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        ulSDLength = RtlLengthSecurityDescriptor( AFSDefaultSD);

        if( pIrpSp->Parameters.QuerySecurity.Length < ulSDLength ||
            Irp->UserBuffer == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSQuerySecurity Buffer too small\n"));

            Irp->IoStatus.Information = (ULONG_PTR)ulSDLength;

            try_return( ntStatus = STATUS_BUFFER_OVERFLOW);
        }

        pLockedUserBuffer = AFSLockUserBuffer( Irp->UserBuffer,
                                               pIrpSp->Parameters.QuerySecurity.Length,
                                               &pUserBufferMdl);

        if( pLockedUserBuffer == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQuerySecurity Failed to lock user buffer\n"));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlCopyMemory( pLockedUserBuffer,
                       AFSDefaultSD,
                       ulSDLength);

        Irp->IoStatus.Information = (ULONG_PTR)ulSDLength;

try_exit:

        if( pUserBufferMdl != NULL)
        {
            MmUnlockPages( pUserBufferMdl);
            IoFreeMdl( pUserBufferMdl);
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSQuerySecurity\n"));

        AFSDumpTraceFilesFnc();
    }

    AFSCompleteRequest( Irp,
                        ntStatus);

    return ntStatus;
}
