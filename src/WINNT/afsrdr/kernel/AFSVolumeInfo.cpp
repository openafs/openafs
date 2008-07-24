//
// File: AFSVolumeInfo.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSQueryVolumeInfo( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)DeviceObject->DeviceExtension;
    IO_STACK_LOCATION *pIrpSp;
    FS_INFORMATION_CLASS FsInformationClass;
    void *pBuffer = NULL;
    ULONG ulLength = 0;
    BOOLEAN bReleaseResource = FALSE;
    AFSFcb *pFcb = NULL;
    AFSDirEntryCB *pVolumeEntry = NULL;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        pFcb = (AFSFcb *)pIrpSp->FileObject->FsContext;

        pVolumeEntry = pFcb->VolumeNode;

        ulLength = pIrpSp->Parameters.QueryVolume.Length;
        FsInformationClass = pIrpSp->Parameters.QueryVolume.FsInformationClass;
        pBuffer = Irp->AssociatedIrp.SystemBuffer;

        AFSAcquireExcl( &pVolumeEntry->NPDirNode->Lock,
                        TRUE);

        bReleaseResource = TRUE;

        //
        // Don't allow requests against IOCtl nodes
        //

        if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        //
        // Process the request
        //

        switch( FsInformationClass) 
        {

            case FileFsVolumeInformation:
            {
                
                ntStatus = AFSQueryFsVolumeInfo( &pVolumeEntry->Type.Volume.VolumeInformation,
                                                 (PFILE_FS_VOLUME_INFORMATION)pBuffer, 
                                                 &ulLength);

                break;
            }

            case FileFsSizeInformation:
            {
            
                ntStatus = AFSQueryFsSizeInfo( &pVolumeEntry->Type.Volume.VolumeInformation,
                                               (PFILE_FS_SIZE_INFORMATION)pBuffer, 
                                               &ulLength);
            
                break;
            }

            case FileFsDeviceInformation:
            {
            
                ntStatus = AFSQueryFsDeviceInfo( &pVolumeEntry->Type.Volume.VolumeInformation,
                                                 (PFILE_FS_DEVICE_INFORMATION)pBuffer, 
                                                 &ulLength);
            
                break;
            }

            case FileFsAttributeInformation:
            {
            
                ntStatus = AFSQueryFsAttributeInfo( &pVolumeEntry->Type.Volume.VolumeInformation,
                                                    (PFILE_FS_ATTRIBUTE_INFORMATION)pBuffer, 
                                                    &ulLength);
            
                break;
            }

            case FileFsFullSizeInformation:
            {

                ntStatus = AFSQueryFsFullSizeInfo( &pVolumeEntry->Type.Volume.VolumeInformation,
                                                   (PFILE_FS_FULL_SIZE_INFORMATION)pBuffer, 
                                                   &ulLength);
            
                break;
            }

            default:

                AFSPrint("AFSQueryVolumeInfo Invalid class %d\n", FsInformationClass);

                ntStatus = STATUS_INVALID_PARAMETER;
            
                break;
        }

try_exit:

        //
        // Setup the Irp's information field to what we actually copied in.
        //

        Irp->IoStatus.Information = pIrpSp->Parameters.QueryVolume.Length - ulLength;

        if( bReleaseResource)
        {

            AFSReleaseResource( &pVolumeEntry->NPDirNode->Lock);
        }

        AFSCompleteRequest( Irp,
                            ntStatus);

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSQueryVolumeInfo\n");
    }

    return ntStatus;
}

NTSTATUS
AFSSetVolumeInfo( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        AFSPrint("AFSSetVolumeInfo Entry for FO %08lX\n", pIrpSp->FileObject);





        AFSCompleteRequest( Irp,
                            ntStatus);

    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFSSetVolumeInfo\n");
    }

    return ntStatus;
}

NTSTATUS
AFSQueryFsVolumeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                      IN PFILE_FS_VOLUME_INFORMATION Buffer,
                      IN OUT PULONG Length)

{
    
    NTSTATUS ntStatus = STATUS_SUCCESS;

    RtlZeroMemory( Buffer, 
                   *Length);

    if( *Length >= (LONG)sizeof( FILE_FS_VOLUME_INFORMATION) + VolumeInfo->VolumeLabelLength - sizeof( WCHAR))
    {

        Buffer->VolumeCreationTime.QuadPart = VolumeInfo->VolumeCreationTime.QuadPart;

        Buffer->VolumeSerialNumber = VolumeInfo->VolumeID;

        Buffer->VolumeLabelLength = VolumeInfo->VolumeLabelLength;

        Buffer->SupportsObjects = FALSE;

        RtlCopyMemory( Buffer->VolumeLabel,
                       VolumeInfo->VolumeLabel,
                       Buffer->VolumeLabelLength);

        *Length -= sizeof( FILE_FS_VOLUME_INFORMATION) + VolumeInfo->VolumeLabelLength - sizeof( WCHAR);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryFsSizeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                    IN PFILE_FS_SIZE_INFORMATION Buffer,
                    IN OUT PULONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    RtlZeroMemory( Buffer, 
                   *Length);

    if( *Length >= sizeof( FILE_FS_SIZE_INFORMATION))
    {

        AFSPrint("AFSQueryFsSizeInfo Query size request\n");

        Buffer->TotalAllocationUnits.QuadPart = VolumeInfo->TotalAllocationUnits.QuadPart;

        Buffer->AvailableAllocationUnits.QuadPart = VolumeInfo->AvailableAllocationUnits.QuadPart;

        Buffer->SectorsPerAllocationUnit = VolumeInfo->SectorsPerAllocationUnit;

        Buffer->BytesPerSector = VolumeInfo->BytesPerSector;

        *Length -= sizeof( FILE_FS_SIZE_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryFsDeviceInfo( IN AFSVolumeInfoCB *VolumeInfo,
                      IN PFILE_FS_DEVICE_INFORMATION Buffer,
                      IN OUT PULONG Length)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    RtlZeroMemory( Buffer, 
                   *Length);

    if( *Length >= (LONG)sizeof( FILE_FS_DEVICE_INFORMATION))
    {

        Buffer->DeviceType = FILE_DEVICE_DISK;

        Buffer->Characteristics = VolumeInfo->Characteristics;

        *Length -= sizeof( FILE_FS_DEVICE_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryFsAttributeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                         IN PFILE_FS_ATTRIBUTE_INFORMATION Buffer,
                         IN OUT PULONG Length)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    RtlZeroMemory( Buffer, 
                   *Length);

    if( *Length >= (LONG)(sizeof( FILE_FS_ATTRIBUTE_INFORMATION) + 18))
    {

        Buffer->FileSystemAttributes = (FILE_CASE_PRESERVED_NAMES | 
                                                FILE_UNICODE_ON_DISK);

        Buffer->MaximumComponentNameLength = 255;

        Buffer->FileSystemNameLength = 18;

        RtlCopyMemory( Buffer->FileSystemName,
                       L"AFSRDRFsd",
                       18);

        *Length -= sizeof( FILE_FS_ATTRIBUTE_INFORMATION) + 18;
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryFsFullSizeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                        IN PFILE_FS_FULL_SIZE_INFORMATION Buffer,
                        IN OUT PULONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;

    RtlZeroMemory( Buffer, 
                   *Length);

    if( *Length >= sizeof( FILE_FS_FULL_SIZE_INFORMATION))
    {

        AFSPrint("AFSQueryFsFullSizeInfo Query size request\n");

        Buffer->TotalAllocationUnits.QuadPart = VolumeInfo->TotalAllocationUnits.QuadPart;

        Buffer->CallerAvailableAllocationUnits.QuadPart = VolumeInfo->AvailableAllocationUnits.QuadPart;

        Buffer->ActualAvailableAllocationUnits.QuadPart = VolumeInfo->AvailableAllocationUnits.QuadPart;

        Buffer->SectorsPerAllocationUnit = VolumeInfo->SectorsPerAllocationUnit;

        Buffer->BytesPerSector = VolumeInfo->BytesPerSector;

        *Length -= sizeof( FILE_FS_FULL_SIZE_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_OVERFLOW;
    }

    return ntStatus;
}
