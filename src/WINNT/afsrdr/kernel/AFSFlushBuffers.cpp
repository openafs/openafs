//
// File: AFSFlushBuffers.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSFlushBuffers( IN PDEVICE_OBJECT DeviceObject,
                 IN PIRP Irp)
{

    NTSTATUS           ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT       pFileObject = pIrpSp->FileObject;
    AFSFcb            *pFcb = (AFSFcb *)pFileObject->FsContext;
    AFSCcb            *pCcb = (AFSCcb *)pFileObject->FsContext2;
    IO_STATUS_BLOCK    iosb = {0};

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __Enter 
    {
        if (pFcb->Header.NodeTypeCode == AFS_ROOT_FCB ||
            pFcb->Header.NodeTypeCode == AFS_ROOT_ALL ) 
        {

            //
            // Do we need to do a full flush here?
            //
            try_return( ntStatus = STATUS_SUCCESS);

        }
        else if (pFcb->Header.NodeTypeCode != AFS_FILE_FCB)
        {
            //
            // Nothing to flush Everything but files are write through
            //
            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }
        //
        // The flush consists of two parts.  We firstly flush our
        // cache (if we have one), then we tell the service to write
        // to the remote server
        //
        __try
        {
            CcFlushCache( &pFcb->NPFcb->SectionObjectPointers, NULL, 0, &iosb);
        }
        __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
        {
        
            try_return( ntStatus = GetExceptionCode());
        }
        if (!NT_SUCCESS( iosb.Status )) 
        {
            try_return( ntStatus = iosb.Status );
        }
        //
        // Now, flush to the server - if there is stuff to do
        //
        ntStatus = AFSFlushExtents( pFcb );

    try_exit:
        AFSCompleteRequest( Irp, ntStatus);
    }

    return ntStatus;
}
