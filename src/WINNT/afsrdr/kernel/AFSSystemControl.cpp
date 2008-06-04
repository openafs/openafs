//
// File: AFSSystemControl.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSSystemControl( IN PDEVICE_OBJECT DeviceObject,
                   IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        AFSPrint("AFSSystemControl Entry for FO %08lX\n", pIrpSp->FileObject);





        AFSCompleteRequest( Irp,
                            ntStatus);

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSSystemControl\n");
    }

    return ntStatus;
}
