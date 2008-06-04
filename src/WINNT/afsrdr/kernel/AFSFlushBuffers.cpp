//
// File: AFSFlushBuffers.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSFlushBuffers( IN PDEVICE_OBJECT DeviceObject,
                 IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {





        AFSCompleteRequest( Irp,
                            ntStatus);

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSFlushBuffers\n");
    }

    return ntStatus;
}
