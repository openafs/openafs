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
// File: AFSCreate.cpp
//

#include "AFSCommon.h"

//
// Function: AFSCreate
//
// Description:
//
//      This function is the dispatch handler for the IRP_MJ_CREATE requests. It makes the determination to
//      which interface this request is destined.
//
// Return:
//
//      A status is returned for the function. The Irp completion processing is handled in the specific
//      interface handler.
//

NTSTATUS
AFSCreate( IN PDEVICE_OBJECT DeviceObject,
           IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __try
    {

        if( DeviceObject == AFSDeviceObject)
        {

            ntStatus = AFSControlDeviceCreate( Irp);

            try_return( ntStatus);
        }

        ntStatus = AFSCommonCreate( DeviceObject,
                                    Irp);

try_exit:

        NOTHING;
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSCreate\n"));

        ntStatus = STATUS_ACCESS_DENIED;

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

NTSTATUS
AFSCommonCreate( IN PDEVICE_OBJECT DeviceObject,
                 IN PIRP Irp)
{

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    FILE_OBJECT        *pFileObject = NULL;
    IO_STACK_LOCATION  *pIrpSp;
    AFSDeviceExt       *pDeviceExt = NULL;
    AFSDeviceExt       *pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    GUID               *pAuthGroup = NULL;
    UNICODE_STRING uniGUIDString;

    __Enter
    {

        pIrpSp = IoGetCurrentIrpStackLocation( Irp);
        pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
        pFileObject = pIrpSp->FileObject;

        uniGUIDString.Buffer = NULL;
        uniGUIDString.Length = 0;
        uniGUIDString.MaximumLength = 0;

        //
        // Validate the process entry
        //

        pAuthGroup = AFSValidateProcessEntry( PsGetCurrentProcessId(),
                                              FALSE);

        if( pAuthGroup != NULL)
        {

            RtlStringFromGUID( *pAuthGroup,
                               &uniGUIDString);

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s (%p) Located AuthGroup %wZ after validation\n",
                          __FUNCTION__,
                          Irp,
                          &uniGUIDString));

        }
        else
        {
            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s (%p) Failed to locate AuthGroup\n",
                          __FUNCTION__,
                          Irp));
        }

        //
        // Root open?
        //

        if( pFileObject == NULL ||
            pFileObject->FileName.Buffer == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCommonCreate (%p) Processing volume open request\n",
                          Irp));

            ntStatus = AFSOpenRedirector( Irp);

            AFSCompleteRequest( Irp,
                                ntStatus);

            try_return( ntStatus);
        }


        //
        // Check the state of the library
        //

        ntStatus = AFSCheckLibraryState( Irp);

        if( !NT_SUCCESS( ntStatus) ||
            ntStatus == STATUS_PENDING)
        {

            if( ntStatus != STATUS_PENDING)
            {
                AFSCompleteRequest( Irp, ntStatus);
            }

            try_return( ntStatus);
        }

        IoSkipCurrentIrpStackLocation( Irp);

        ntStatus = IoCallDriver( pControlDevExt->Specific.Control.LibraryDeviceObject,
                                 Irp);

        //
        // Indicate the library is done with the request
        //

        AFSClearLibraryRequest();

try_exit:

        if ( pFileObject) {
            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s (%p) File \"%wZ\" AuthGroup '%wZ' ntStatus %08lX\n",
                          __FUNCTION__,
                          Irp,
                          &pFileObject->FileName,
                          &uniGUIDString,
                          ntStatus));
        }

        if( uniGUIDString.Buffer != NULL)
        {
            RtlFreeUnicodeString( &uniGUIDString);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSControlDeviceCreate( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        //
        // For now, jsut let the open happen
        //

        Irp->IoStatus.Information = FILE_OPENED;

        AFSCompleteRequest( Irp, ntStatus);
    }

    return ntStatus;
}

NTSTATUS
AFSOpenRedirector( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    FILE_OBJECT        *pFileObject = NULL;
    IO_STACK_LOCATION  *pIrpSp;
    AFSDeviceExt* pDeviceExt =
        (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        pIrpSp = IoGetCurrentIrpStackLocation( Irp);

        pFileObject = pIrpSp->FileObject;

        pFileObject->FsContext = (PVOID) pDeviceExt->Fcb;

        ASSERT(pFileObject->FsContext != NULL);

        //
        // Return the open result for this file
        //

        Irp->IoStatus.Information = FILE_OPENED;

        Irp->IoStatus.Status = ntStatus;
    }

    return ntStatus;
}
