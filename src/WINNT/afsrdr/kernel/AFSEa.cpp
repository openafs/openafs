//
// File: AFSEa.cpp
//

#include "AFSCommon.h"

//
// Function: AFSQueryEA
//
// Description:
//
//      This function is the dipatch handler for the IRP_MJ_QUERY_EA request
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSQueryEA( IN PDEVICE_OBJECT DeviceObject,
            IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_EAS_NOT_SUPPORTED;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        AFSPrint("AFSQueryEa Entry for FO %08lX\n", pIrpSp->FileObject);





        AFSCompleteRequest( Irp,
                              ntStatus);

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSQueryEA\n");
    }

    return ntStatus;
}

//
// Function: AFSSetEA
//
// Description:
//
//      This function is the dipatch handler for the IRP_MJ_SET_EA request
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSSetEA( IN PDEVICE_OBJECT DeviceObject,
          IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_EAS_NOT_SUPPORTED;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        AFSPrint("AFSSetEa Entry for FO %08lX\n", pIrpSp->FileObject);





        AFSCompleteRequest( Irp,
                              ntStatus);

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSSetEA\n");
    }

    return ntStatus;
}
