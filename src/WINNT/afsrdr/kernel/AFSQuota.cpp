//
// File: AFSQuota.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSQueryQuota( IN PDEVICE_OBJECT DeviceObject,
               IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        AFSPrint("AFSQueryQuota Entry for FO %08lX\n", pIrpSp->FileObject);





        AFSCompleteRequest( Irp,
                            ntStatus);

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSQueryQuota\n");
    }

    return ntStatus;
}

NTSTATUS
AFSSetQuota( IN PDEVICE_OBJECT DeviceObject,
             IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        AFSPrint("AFSSetQuota Entry for FO %08lX\n", pIrpSp->FileObject);





        AFSCompleteRequest( Irp,
                            ntStatus);
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSSetQuota\n");
    }

    return ntStatus;
}
