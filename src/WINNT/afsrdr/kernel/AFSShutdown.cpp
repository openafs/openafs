//
// File: AFSShutdown.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSShutdown( IN PDEVICE_OBJECT DeviceObject,
             IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        ntStatus = AFSShutdownFilesystem();

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSShutdown Failed to shutdown filesystem Status %08lX\n", ntStatus);

            ntStatus = STATUS_SUCCESS;
        }

        AFSCompleteRequest( Irp,
                            ntStatus);
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSShutdown\n");
    }

    return ntStatus;
}

NTSTATUS
AFSShutdownFilesystem()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;


    return ntStatus;
}

