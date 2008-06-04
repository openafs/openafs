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

    __try
    {

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        //
        // Acquire the main shared for adding the lock control to the list
        //

        AFSAcquireShared( &pFcb->NPFcb->Resource,
                          TRUE);
        
        //
        //  Now call the system package for actually processing the lock request
        //

        ntStatus = FsRtlProcessFileLock( &pFcb->Specific.File.FileLock, 
                                         Irp, 
                                         NULL);

        //
        // And drop it
        //

        AFSReleaseResource( &pFcb->NPFcb->Resource);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSLockControl Failed to process lock request Status %08lX\n", ntStatus);
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()))
    {

        AFSPrint("EXCEPTION - AFSLockControl\n");

        //
        // Again, there is little point in failing this request but pass back some type of failure status
        //

        ntStatus = STATUS_UNSUCCESSFUL;
    }

    return ntStatus;
}
