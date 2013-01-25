/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice,
 *   this list of conditions and the following disclaimer in the
 *   documentation
 *   and/or other materials provided with the distribution.
 * - Neither the names of Kernel Drivers, LLC and Your File System, Inc.
 *   nor the names of their contributors may be used to endorse or promote
 *   products derived from this software without specific prior written
 *   permission from Kernel Drivers, LLC and Your File System, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// File: AFSVolumeInfo.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSQueryVolumeInfo( IN PDEVICE_OBJECT LibDeviceObject,
                    IN PIRP Irp)
{

    UNREFERENCED_PARAMETER(LibDeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;
    FS_INFORMATION_CLASS FsInformationClass = FileFsMaximumInformation;
    void *pBuffer = NULL;
    ULONG ulLength = 0;
    BOOLEAN bReleaseResource = FALSE;
    PFILE_OBJECT pFileObject = NULL;
    AFSFcb *pFcb = NULL;
    AFSObjectInfoCB *pObjectInfo = NULL;
    AFSVolumeCB *pVolumeCB = NULL;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        pFileObject = pIrpSp->FileObject;

        if( pFileObject == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueryVolumeInfo Failing request with NULL FileObject\n");

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        pFcb = (AFSFcb *)pFileObject->FsContext;

        if( pFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueryVolumeInfo Failing request with NULL Fcb\n");

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        pObjectInfo = pFcb->ObjectInformation;

        if( pObjectInfo == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueryVolumeInfo Failing request with NULL ObjectInformation\n");

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        pVolumeCB = pObjectInfo->VolumeCB;

        ulLength = pIrpSp->Parameters.QueryVolume.Length;
        FsInformationClass = pIrpSp->Parameters.QueryVolume.FsInformationClass;
        pBuffer = Irp->AssociatedIrp.SystemBuffer;

        AFSAcquireShared( pVolumeCB->VolumeLock,
                          TRUE);

        bReleaseResource = TRUE;

        //
        // Don't allow requests against IOCtl nodes
        //

        if( pFcb->Header.NodeTypeCode == AFS_IOCTL_FCB)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueryVolumeInfo Failing request against PIOCtl Fcb\n");

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }
        else if( pFcb->Header.NodeTypeCode == AFS_SPECIAL_SHARE_FCB)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueryVolumeInfo Failing request against SpecialShare Fcb\n");

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }
        else if( pFcb->Header.NodeTypeCode == AFS_INVALID_FCB)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSQueryVolumeInfo Failing request against SpecialShare Fcb\n");

            try_return( ntStatus = STATUS_INVALID_DEVICE_REQUEST);
        }

        //
        // Process the request
        //

        switch( FsInformationClass)
        {

            case FileFsVolumeInformation:
            {

                ntStatus = AFSQueryFsVolumeInfo( &pVolumeCB->VolumeInformation,
                                                 (PFILE_FS_VOLUME_INFORMATION)pBuffer,
                                                 &ulLength);

                break;
            }

            case FileFsSizeInformation:
            {

                ntStatus = AFSQueryFsSizeInfo( &pVolumeCB->VolumeInformation,
                                               (PFILE_FS_SIZE_INFORMATION)pBuffer,
                                               &ulLength);

                break;
            }

            case FileFsDeviceInformation:
            {

                ntStatus = AFSQueryFsDeviceInfo( &pVolumeCB->VolumeInformation,
                                                 (PFILE_FS_DEVICE_INFORMATION)pBuffer,
                                                 &ulLength);

                break;
            }

            case FileFsAttributeInformation:
            {

                ntStatus = AFSQueryFsAttributeInfo( &pVolumeCB->VolumeInformation,
                                                    (PFILE_FS_ATTRIBUTE_INFORMATION)pBuffer,
                                                    &ulLength);

                break;
            }

            case FileFsFullSizeInformation:
            {

                ntStatus = AFSQueryFsFullSizeInfo( &pVolumeCB->VolumeInformation,
                                                   (PFILE_FS_FULL_SIZE_INFORMATION)pBuffer,
                                                   &ulLength);

                break;
            }

            default:

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSQueryVolumeInfo Invalid class %d\n",
                              FsInformationClass);

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

            AFSReleaseResource( pVolumeCB->VolumeLock);
        }

        AFSCompleteRequest( Irp,
                            ntStatus);

    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSQueryVolumeInfo FO %p InfoClass %d FCB %p ObjectInfo %p VolCB %p\n",
                      pFileObject,
                      FsInformationClass,
                      pFcb,
                      pObjectInfo,
                      pVolumeCB);

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

NTSTATUS
AFSSetVolumeInfo( IN PDEVICE_OBJECT DeviceObject,
                  IN PIRP Irp)
{

    UNREFERENCED_PARAMETER(DeviceObject);
    NTSTATUS ntStatus = STATUS_SUCCESS;
    IO_STACK_LOCATION *pIrpSp;

    pIrpSp = IoGetCurrentIrpStackLocation( Irp);

    __try
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_WARNING,
                      "AFSSetVolumeInfo Entry for FO %p\n", pIrpSp->FileObject);

        AFSCompleteRequest( Irp,
                            ntStatus);

    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgLogMsg( 0,
                      0,
                      "EXCEPTION - AFSSetVolumeInfo\n");

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

NTSTATUS
AFSQueryFsVolumeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                      IN PFILE_FS_VOLUME_INFORMATION Buffer,
                      IN OUT PULONG Length)

{

    NTSTATUS ntStatus = STATUS_BUFFER_TOO_SMALL;
    ULONG ulCopyLength;

    RtlZeroMemory( Buffer,
                   *Length);

    if( *Length >= (ULONG)sizeof( FILE_FS_VOLUME_INFORMATION))
    {

        if( *Length >= (ULONG)(FIELD_OFFSET( FILE_FS_VOLUME_INFORMATION, VolumeLabel) + (LONG)VolumeInfo->VolumeLabelLength))
        {

            ulCopyLength = (LONG)VolumeInfo->VolumeLabelLength;

            ntStatus = STATUS_SUCCESS;
        }
        else
        {

            ulCopyLength = *Length - FIELD_OFFSET( FILE_FS_VOLUME_INFORMATION, VolumeLabel);

            ntStatus = STATUS_BUFFER_OVERFLOW;
        }

        Buffer->VolumeCreationTime.QuadPart = VolumeInfo->VolumeCreationTime.QuadPart;

        Buffer->VolumeSerialNumber = VolumeInfo->VolumeID;

        Buffer->VolumeLabelLength = VolumeInfo->VolumeLabelLength;

        Buffer->SupportsObjects = FALSE;

        *Length -= FIELD_OFFSET( FILE_FS_VOLUME_INFORMATION, VolumeLabel);

        if( ulCopyLength > 0)
        {

            RtlCopyMemory( Buffer->VolumeLabel,
                           VolumeInfo->VolumeLabel,
                           ulCopyLength);

            *Length -= ulCopyLength;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSQueryFsSizeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                    IN PFILE_FS_SIZE_INFORMATION Buffer,
                    IN OUT PULONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFileID FileID;
    AFSVolumeSizeInfoCB VolumeSizeInfo;

    RtlZeroMemory( Buffer,
                   *Length);

    if( *Length >= sizeof( FILE_FS_SIZE_INFORMATION))
    {

        RtlZeroMemory( &FileID,
                       sizeof(AFSFileID));

        FileID.Cell = VolumeInfo->CellID;

        FileID.Volume = VolumeInfo->VolumeID;

        ntStatus = AFSRetrieveVolumeSizeInformation( NULL,
                                                     &FileID,
                                                     &VolumeSizeInfo);

        if ( NT_SUCCESS( ntStatus))
        {

            Buffer->TotalAllocationUnits.QuadPart = VolumeSizeInfo.TotalAllocationUnits.QuadPart;

            Buffer->AvailableAllocationUnits.QuadPart = VolumeSizeInfo.AvailableAllocationUnits.QuadPart;

            Buffer->SectorsPerAllocationUnit = VolumeSizeInfo.SectorsPerAllocationUnit;

            Buffer->BytesPerSector = VolumeSizeInfo.BytesPerSector;

            *Length -= sizeof( FILE_FS_SIZE_INFORMATION);
        }
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
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

        //
        // This value is used to determine the return type of
        // Win32 GetFileType().  Returning FILE_DEVICE_NETWORK_FILE_SYSTEM
        // results in GetFileType returning FILE_TYPE_UNKNOWN which breaks
        // msys-based applications.  They treat all files as character
        // special devices instead of files.
        //

        Buffer->DeviceType = FILE_DEVICE_DISK;

        Buffer->Characteristics = VolumeInfo->Characteristics;

        *Length -= sizeof( FILE_FS_DEVICE_INFORMATION);
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryFsAttributeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                         IN PFILE_FS_ATTRIBUTE_INFORMATION Buffer,
                         IN OUT PULONG Length)
{
    UNREFERENCED_PARAMETER(VolumeInfo);
    NTSTATUS ntStatus = STATUS_SUCCESS;

    RtlZeroMemory( Buffer,
                   *Length);

    if( *Length >= (LONG)(sizeof( FILE_FS_ATTRIBUTE_INFORMATION)))
    {

        Buffer->FileSystemAttributes = (FILE_CASE_PRESERVED_NAMES |
                                        FILE_UNICODE_ON_DISK |
                                        FILE_SUPPORTS_HARD_LINKS |
                                        FILE_SUPPORTS_REPARSE_POINTS);

        Buffer->MaximumComponentNameLength = 255;

        Buffer->FileSystemNameLength = 18;

        *Length -= FIELD_OFFSET( FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName);

        if( *Length >= 18)
        {

            RtlCopyMemory( Buffer->FileSystemName,
                           L"AFSRDRFsd",
                           18);

            *Length -= 18;
        }
        else
        {

            ntStatus = STATUS_BUFFER_OVERFLOW;
        }
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}

NTSTATUS
AFSQueryFsFullSizeInfo( IN AFSVolumeInfoCB *VolumeInfo,
                        IN PFILE_FS_FULL_SIZE_INFORMATION Buffer,
                        IN OUT PULONG Length)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFileID FileID;
    AFSVolumeSizeInfoCB VolumeSizeInfo;

    RtlZeroMemory( Buffer,
                   *Length);

    if( *Length >= sizeof( FILE_FS_FULL_SIZE_INFORMATION))
    {

        RtlZeroMemory( &FileID,
                       sizeof(AFSFileID));

        FileID.Cell = VolumeInfo->CellID;

        FileID.Volume = VolumeInfo->VolumeID;

        ntStatus = AFSRetrieveVolumeSizeInformation( NULL,
                                                     &FileID,
                                                     &VolumeSizeInfo);

        if ( NT_SUCCESS( ntStatus))
        {

            Buffer->TotalAllocationUnits.QuadPart = VolumeSizeInfo.TotalAllocationUnits.QuadPart;

            Buffer->CallerAvailableAllocationUnits.QuadPart = VolumeSizeInfo.AvailableAllocationUnits.QuadPart;

            Buffer->ActualAvailableAllocationUnits.QuadPart = VolumeSizeInfo.AvailableAllocationUnits.QuadPart;

            Buffer->SectorsPerAllocationUnit = VolumeSizeInfo.SectorsPerAllocationUnit;

            Buffer->BytesPerSector = VolumeSizeInfo.BytesPerSector;

            *Length -= sizeof( FILE_FS_FULL_SIZE_INFORMATION);
        }
    }
    else
    {

        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

    return ntStatus;
}
