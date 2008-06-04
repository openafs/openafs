//
// File: AFSSecurity.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSSetSecurity( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        AFSPrint("AFSSetSecurity Entry for FO %08lX\n", pIrpSp->FileObject);





        AFSCompleteRequest( Irp,
                            ntStatus);
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSSetSecurity\n");
    }

    return ntStatus;
}

NTSTATUS
AFSQuerySecurity( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        AFSPrint("AFSQuerySecurity Entry for FO %08lX\n", pIrpSp->FileObject);





        AFSCompleteRequest( Irp,
                            ntStatus);
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSQuerySecurity\n");
    }

    return ntStatus;
}
