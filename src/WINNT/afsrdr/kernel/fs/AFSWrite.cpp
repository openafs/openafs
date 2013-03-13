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
// File: AFSWrite.cpp
//

#include "AFSCommon.h"


static
NTSTATUS
AFSWriteComplete( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp,
                  IN PVOID Contxt);

//
// Function: AFSWrite
//
// Description:
//
//      This is the dispatch handler for the IRP_MJ_WRITE request.  Since we want to
//      allow the library to pend the write we need to lock the library for the
//      duration of the thread calling the library but also for the life of the IRP.
//      So this code path establishes an IO completion function.
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSWrite( IN PDEVICE_OBJECT DeviceObject,
          IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pControlDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __try
    {

        if( DeviceObject == AFSDeviceObject)
        {

            ntStatus = STATUS_INVALID_DEVICE_REQUEST;

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

        //
        // Increment the outstanding IO count again - this time for the
        // completion routine.
        //

        ntStatus = AFSCheckLibraryState( Irp);

        if( !NT_SUCCESS( ntStatus) ||
            ntStatus == STATUS_PENDING)
        {

            AFSClearLibraryRequest();

            if( ntStatus != STATUS_PENDING)
            {

                AFSCompleteRequest( Irp, ntStatus);
            }

            try_return( ntStatus);
        }

        //
        // And send it down, but arrange to capture the comletion
        // so we can free our lock against unloading.
        //

        IoCopyCurrentIrpStackLocationToNext( Irp);

        AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "Setting AFSWriteComplete as IoCompletion Routine Irp %p\n",
                      Irp));

        IoSetCompletionRoutine( Irp, AFSWriteComplete, NULL, TRUE, TRUE, TRUE);

        ntStatus = IoCallDriver( pControlDeviceExt->Specific.Control.LibraryDeviceObject,
                                 Irp);

        //
        // Indicate the library/thread pair is done with the request
        //

        AFSClearLibraryRequest();

try_exit:

        NOTHING;
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

//
// AFSWriteComplete
//
static
NTSTATUS
AFSWriteComplete( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp,
                  IN PVOID Context)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(Context);
    BOOLEAN bPending = FALSE;

    //
    // Indicate the library/IRP pair is done with the request
    //

    AFSClearLibraryRequest();

    if (Irp->PendingReturned) {

        bPending = TRUE;

        IoMarkIrpPending(Irp);
    }

    AFSDbgTrace(( AFS_SUBSYSTEM_IO_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSWriteComplete Irp %p%s\n",
                  Irp,
                  bPending ? " PENDING" : ""));

    return STATUS_CONTINUE_COMPLETION;
}
