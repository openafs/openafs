//
// File: AFSDevControl.cpp
//

#include "AFSCommon.h"

//
// Function: AFSDevControl
//
// Description:
//
//      This is the dipatch handler for the IRP_MJ_DEVICE_CONTROL requests.
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSDevControl( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        if( DeviceObject == AFSDeviceObject)
        {

            ntStatus = AFSProcessControlRequest( Irp);

            try_return( ntStatus);
        }

        if( DeviceObject == AFSRDRDeviceObject)
        {

            ntStatus = AFSRDRDeviceControl( DeviceObject,
                                            Irp);

            try_return( ntStatus);
        }

        switch( pIrpSp->Parameters.DeviceIoControl.IoControlCode)
        {
            
            case 0:
            default:
            {

                AFSPrint("AFSDevControl Invalid control code\n");

                ntStatus = STATUS_INVALID_PARAMETER;

                break;
            }
        }

        AFSCompleteRequest( Irp,
                              ntStatus);

try_exit:

        NOTHING;
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSDevControl\n");
    }
    return ntStatus;
}
