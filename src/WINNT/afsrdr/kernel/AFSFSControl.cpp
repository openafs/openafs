//
// File: AFSFSControl.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSFSControl( IN PDEVICE_OBJECT DeviceObject,
              IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        switch( pIrpSp->MinorFunction) 
        {

            case IRP_MN_USER_FS_REQUEST:

                ntStatus = AFSProcessUserFsRequest( Irp);

                break;

            case IRP_MN_MOUNT_VOLUME:

                AFSPrint("AFSFSControl Processing mount volume request\n");

                break;

            case IRP_MN_VERIFY_VOLUME:

                AFSPrint("AFSFSControl Processing verify volume request\n");

                break;

            default:

                AFSPrint("AFSFSControl Processing unknown request %08lX\n", pIrpSp->MinorFunction);

                break;
        }

        AFSCompleteRequest( Irp,
                              ntStatus);

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSFSControl\n");
    }

    return ntStatus;
}

NTSTATUS
AFSProcessUserFsRequest( IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulFsControlCode;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp );

    ulFsControlCode = pIrpSp->Parameters.FileSystemControl.FsControlCode;

    //
    // Process the request
    //

    switch( ulFsControlCode ) 
    {

        case FSCTL_REQUEST_OPLOCK_LEVEL_1:
        case FSCTL_REQUEST_OPLOCK_LEVEL_2:
        case FSCTL_REQUEST_BATCH_OPLOCK:
        case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
        case FSCTL_OPBATCH_ACK_CLOSE_PENDING:
        case FSCTL_OPLOCK_BREAK_NOTIFY:
        case FSCTL_OPLOCK_BREAK_ACK_NO_2:
        case FSCTL_REQUEST_FILTER_OPLOCK :

            AFSPrint("AFSProcessUserFsRequest Processing oplock request\n");
        
            break;

        case FSCTL_LOCK_VOLUME:

            AFSPrint("AFSProcessUserFsRequest Processing lock volume request\n");            
                
            break;

        case FSCTL_UNLOCK_VOLUME:

            AFSPrint("AFSProcessUserFsRequest Processing unlock volume request\n");

            break;

        case FSCTL_DISMOUNT_VOLUME:

            AFSPrint("AFSProcessUserFsRequest Processing dismount volume request\n");

            break;

        case FSCTL_MARK_VOLUME_DIRTY:

            AFSPrint("AFSProcessUserFsRequest Processing mark volume dirty request\n");

            break;

        case FSCTL_IS_VOLUME_DIRTY:

            AFSPrint("AFSProcessUserFsRequest Processing IsVolumeDirty request\n");

            break;

        case FSCTL_IS_VOLUME_MOUNTED:

            AFSPrint("AFSProcessUserFsRequest Processing IsVolumeMounted request\n");

            break;

        case FSCTL_IS_PATHNAME_VALID:
            
            AFSPrint("AFSProcessUserFsRequest Processing IsPathNameValid request\n");

            break;

        case FSCTL_QUERY_RETRIEVAL_POINTERS:
            
            AFSPrint("AFSProcessUserFsRequest Processing QueryRetrievalPntrs request\n");

            break;

        case FSCTL_FILESYSTEM_GET_STATISTICS:
            
            AFSPrint("AFSProcessUserFsRequest Processing GetFSStats request\n");

            break;

        case FSCTL_GET_VOLUME_BITMAP:
            
            AFSPrint("AFSProcessUserFsRequest Processing GetVolumeBitmap request\n");

            break;

        case FSCTL_GET_RETRIEVAL_POINTERS:
            
            AFSPrint("AFSProcessUserFsRequest Processing GetRetrievalPntrs request\n");

            break;

        case FSCTL_MOVE_FILE:

            AFSPrint("AFSProcessUserFsRequest Processing MoveFile request\n");

            break;

        case FSCTL_ALLOW_EXTENDED_DASD_IO:

            AFSPrint("AFSProcessUserFsRequest Processing AllowDASD IO request\n");

            break;

        case FSCTL_GET_REPARSE_POINT:
            AFSPrint("AFSProcessUserFsRequest Get reparse data buffer for %wZ\n", &pIrpSp->FileObject->FileName);
            break;

        case FSCTL_SET_REPARSE_POINT:
            AFSPrint("AFSProcessUserFsRequest Get reparse data buffer for %wZ\n", &pIrpSp->FileObject->FileName);
            break;

        default :

            AFSPrint("AFSProcessUserFsRequest Processing Default handler %08lX\n", ulFsControlCode);

            break;
    }

    return ntStatus;
}
