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
// File: AFSCleanup.cpp
//

#include "AFSCommon.h"

//
// Function: AFSCleanup
//
// Description:
//
//      This function is the IRP_MJ_CLEANUP dispatch handler
//
// Return:
//
//       A status is returned for the handling of this request
//

NTSTATUS
AFSCleanup( IN PDEVICE_OBJECT DeviceObject,
            IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        //
        // Set some initial variables to make processing easier
        //

        if( DeviceObject == AFSDeviceObject)
        {

            if( FlagOn( (ULONG_PTR)pIrpSp->FileObject->FsContext, AFS_CONTROL_INSTANCE))
            {

                //
                // This is the process which was registered for the callback pool so cleanup the pool
                //

                AFSCleanupIrpPool();

                //
                // And reset the Service PID
                //
                AFSDeregisterService();

            }

            if( FlagOn( (ULONG_PTR)pIrpSp->FileObject->FsContext, AFS_REDIRECTOR_INSTANCE))
            {

                //
                // Close the redirector
                //

                AFSCloseRedirector();
            }

            AFSCompleteRequest( Irp,
                                ntStatus);

            try_return( ntStatus);
        }

        ntStatus = AFSCommonCleanup( DeviceObject,
                                     Irp);

try_exit:

        NOTHING;
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSCleanup\n"));

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

NTSTATUS
AFSCommonCleanup( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT pFileObject = NULL;
    AFSDeviceExt *pControlDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    BOOLEAN bCompleteRequest = TRUE;
    AFSFcb* pFcb = NULL;
    __Enter
    {

        //
        // Set some initial variables to make processing easier
        //

        pFileObject = pIrpSp->FileObject;

        pFcb = (AFSFcb*) pIrpSp->FileObject->FsContext;

        if( pFcb == NULL ||
            pFcb->Header.NodeTypeCode == AFS_REDIRECTOR_FCB)
        {

            //
            // Root open
            //

            try_return( ntStatus);
        }

        //
        // Check the state of the library
        //

        ntStatus = AFSCheckLibraryState( Irp);

        if( !NT_SUCCESS( ntStatus) ||
            ntStatus == STATUS_PENDING)
        {

            if( ntStatus == STATUS_PENDING)
            {
                bCompleteRequest = FALSE;
            }

            try_return( ntStatus);
        }

        bCompleteRequest = FALSE;

        IoSkipCurrentIrpStackLocation( Irp);

        ntStatus = IoCallDriver( pControlDeviceExt->Specific.Control.LibraryDeviceObject,
                                 Irp);

        //
        // Indicate the library is done with the request
        //

        AFSClearLibraryRequest();

try_exit:

        if( bCompleteRequest)
        {

            if( pFileObject != NULL)
            {

                //
                // Setup the fileobject flags to indicate cleanup is complete.
                //

                SetFlag( pFileObject->Flags, FO_CLEANUP_COMPLETE);
            }

            //
            // Complete the request
            //

            AFSCompleteRequest( Irp, ntStatus);
        }
    }

    return ntStatus;
}
