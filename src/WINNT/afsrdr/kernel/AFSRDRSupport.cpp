
#include "AFSCommon.h"

NTSTATUS
AFSInitRDRDevice()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniDeviceName;
    ULONG ulIndex = 0;
    AFSDeviceExt *pDeviceExt = NULL;

    __Enter
    {

        RtlInitUnicodeString( &uniDeviceName, 
                              AFS_RDR_DEVICE_NAME);

        ntStatus = IoCreateDevice( AFSDriverObject,
                                   sizeof( AFSDeviceExt),
                                   &uniDeviceName,
                                   FILE_DEVICE_NETWORK_FILE_SYSTEM,
                                   FILE_REMOTE_DEVICE,
                                   FALSE,
                                   &AFSRDRDeviceObject);

        if( !NT_SUCCESS( ntStatus)) 
        {

            try_return( ntStatus);
        }

        pDeviceExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

        RtlZeroMemory( pDeviceExt,
                       sizeof( AFSDeviceExt));

        //
        // Initialize resources
        //

        pDeviceExt->Specific.RDR.FileIDTree.TreeLock = &pDeviceExt->Specific.RDR.FileIDTreeLock;

        ExInitializeResourceLite( pDeviceExt->Specific.RDR.FileIDTree.TreeLock);

        pDeviceExt->Specific.RDR.FileIDTree.TreeHead = NULL;

        ExInitializeResourceLite( &pDeviceExt->Specific.RDR.FcbListLock);

        pDeviceExt->Specific.RDR.FcbListHead = NULL;

        pDeviceExt->Specific.RDR.FcbListTail = NULL;

        //
        // Initialize the volume worker thread responsible for handling any volume specific
        // work such as Fcb teear down
        //

        AFSInitVolumeWorker();

        //
        // Clear the initializing bit
        //

        AFSRDRDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

        //
        // Register this device with MUP
        //

        ntStatus = FsRtlRegisterUncProvider( &AFSMUPHandle,
                                             &uniDeviceName,
                                             FALSE);

        if( !NT_SUCCESS( ntStatus))
        {

            //
            // Delete our device and bail
            //

            IoDeleteDevice( AFSRDRDeviceObject);

            AFSRDRDeviceObject = NULL;

            try_return( ntStatus);
        }

        //
        // Good to go, all registered and ready to start receiving requests
        //

try_exit:

        NOTHING;
    }

    return ntStatus;
}

void
AFSDeleteRDRDevice()
{

    //
    // Unregister with MUP if we ahve a handle
    //

    if( AFSMUPHandle != NULL)
    {

        FsRtlDeregisterUncProvider( AFSMUPHandle);

        AFSMUPHandle = NULL;
    }

    if( AFSRDRDeviceObject != NULL)
    {

        //
        // Delete our device and we're done
        //

        IoDeleteDevice( AFSRDRDeviceObject);

        AFSRDRDeviceObject = NULL;
    }

    return;
}

NTSTATUS
AFSRDRDeviceControl( IN PDEVICE_OBJECT DeviceObject,
                     IN PIRP Irp)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT       pFileObject = pIrpSp->FileObject;
    AFSFcb            *pFcb = (AFSFcb *)pFileObject->FsContext;

    __Enter
    {

        switch( pIrpSp->Parameters.DeviceIoControl.IoControlCode)
        {
#if DBG
            //
            // DEBUG only support to sent a release_file_extents to
            // a file
            //
            case IOCTL_AFS_RELEASE_FILE_EXTENTS:
            {
                ntStatus = AFSProcessReleaseFileExtents( Irp, TRUE );

                Irp->IoStatus.Status = ntStatus;
                      
                break;
            }
#endif
                
            case IOCTL_REDIR_QUERY_PATH:
            {

                QUERY_PATH_REQUEST *pPathRequest = (QUERY_PATH_REQUEST *)pIrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
                QUERY_PATH_RESPONSE *pPathResponse = (QUERY_PATH_RESPONSE *)Irp->UserBuffer;
                UNICODE_STRING uniPathName;

                ntStatus = STATUS_BAD_NETWORK_PATH;

                uniPathName.Length = (USHORT)pPathRequest->PathNameLength;
                uniPathName.MaximumLength = uniPathName.Length;

                uniPathName.Buffer = pPathRequest->FilePathName;
                
                if( uniPathName.Length >= AFSServerName.Length + sizeof( WCHAR))
                {

                    uniPathName.Length = AFSServerName.Length;

                    //
                    // Skip over the first slash in the name
                    //

                    uniPathName.Buffer = &uniPathName.Buffer[ 1];

                    if( RtlCompareUnicodeString( &AFSServerName,
                                                 &uniPathName,
                                                 TRUE) == 0)
                    {

                        ntStatus = STATUS_SUCCESS;

                        pPathResponse->LengthAccepted = AFSServerName.Length + sizeof( WCHAR);
                    }
                }

                AFSPrint("AFSRDRDeviceControl (IOCTL_REDIR_QUERY_PATH) Request for path %wZ Status %08lX (%08lX)\n",
                                                                               &uniPathName,
                                                                               ntStatus,
                                                                               pPathResponse->LengthAccepted);

                break;
            }

            default:

                ntStatus = STATUS_INVALID_DEVICE_REQUEST;

                break;
        }

        //
        // Complete the request
        //

        AFSCompleteRequest( Irp,
                            ntStatus);
    }

    return ntStatus;
}

NTSTATUS
AFSInitializeRedirector( IN AFSCacheFileInfo *CacheFileInfo)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    OBJECT_ATTRIBUTES   stObjectAttribs;
    IO_STATUS_BLOCK stIoStatus;

    __Enter
    {

        //
        // Save off the cache file information
        //

        pDevExt->Specific.RDR.CacheBlockSize = CacheFileInfo->CacheBlockSize;

        pDevExt->Specific.RDR.CacheFile.Length = 0;

        pDevExt->Specific.RDR.CacheFile.MaximumLength = (USHORT)CacheFileInfo->CacheFileNameLength + (4 * sizeof( WCHAR));

        pDevExt->Specific.RDR.CacheFile.Buffer = (WCHAR *)ExAllocatePoolWithTag( PagedPool,
                                                                                 pDevExt->Specific.RDR.CacheFile.MaximumLength,
                                                                                 AFS_GENERIC_MEMORY_TAG);

        if( pDevExt->Specific.RDR.CacheFile.Buffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlCopyMemory( pDevExt->Specific.RDR.CacheFile.Buffer,
                       L"\\??\\",
                       4 * sizeof( WCHAR));

        pDevExt->Specific.RDR.CacheFile.Length = 4 * sizeof( WCHAR);

        RtlCopyMemory( &pDevExt->Specific.RDR.CacheFile.Buffer[ pDevExt->Specific.RDR.CacheFile.Length/sizeof( WCHAR)],
                       CacheFileInfo->CacheFileName,
                       CacheFileInfo->CacheFileNameLength);

        pDevExt->Specific.RDR.CacheFile.Length += (USHORT)CacheFileInfo->CacheFileNameLength;

        //
        // Go open the cache file
        //

        InitializeObjectAttributes( &stObjectAttribs,
                                    &pDevExt->Specific.RDR.CacheFile,
                                    OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                                    NULL,
                                    NULL);

        ntStatus = ZwCreateFile( &pDevExt->Specific.RDR.CacheFileHandle,
                                 GENERIC_READ | GENERIC_WRITE,
                                 &stObjectAttribs,
                                 &stIoStatus,
                                 NULL,
                                 FILE_ATTRIBUTE_NORMAL,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 FILE_OPEN,
                                 FILE_WRITE_THROUGH,
                                 NULL,
                                 0);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Map to the fileobject
        //

        ntStatus = ObReferenceObjectByHandle( pDevExt->Specific.RDR.CacheFileHandle,
                                              SYNCHRONIZE,
                                              NULL,
                                              KernelMode,
                                              (void **)&pDevExt->Specific.RDR.CacheFileObject,
                                              NULL);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Initialize the root information
        //

        ntStatus = AFSInitAFSRoot();

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pDevExt->Specific.RDR.CacheFileHandle != NULL)
            {

                ZwClose( pDevExt->Specific.RDR.CacheFileHandle);

                pDevExt->Specific.RDR.CacheFileHandle = NULL;
            }

            if( pDevExt->Specific.RDR.CacheFileObject != NULL)
            {

                ObDereferenceObject( pDevExt->Specific.RDR.CacheFileObject);
                
                pDevExt->Specific.RDR.CacheFileObject = NULL;
            }

            if( pDevExt->Specific.RDR.CacheFile.Buffer != NULL)
            {

                ExFreePool( pDevExt->Specific.RDR.CacheFile.Buffer);

                pDevExt->Specific.RDR.CacheFile.Buffer = NULL;
            }            
        }
    }

    return ntStatus;
}

NTSTATUS
AFSCloseRedirector()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSDirEntryCB *pDirEntry = NULL;

    __Enter
    {

        //
        // Close off the cache file
        //

        if( pDevExt->Specific.RDR.CacheFileHandle != NULL)
        {

            ZwClose( pDevExt->Specific.RDR.CacheFileHandle);

            pDevExt->Specific.RDR.CacheFileHandle = NULL;
        }

        if( pDevExt->Specific.RDR.CacheFileObject != NULL)
        {

            ObDereferenceObject( pDevExt->Specific.RDR.CacheFileObject);

            pDevExt->Specific.RDR.CacheFileObject = NULL;
        }

        if( pDevExt->Specific.RDR.CacheFile.Buffer != NULL)
        {

            ExFreePool( pDevExt->Specific.RDR.CacheFile.Buffer);

            pDevExt->Specific.RDR.CacheFile.Buffer = NULL;
        }            

        //
        // Reset the ALL Root
        //

        AFSRemoveAFSRoot();
    }

    return ntStatus;
}

NTSTATUS
AFSShutdownRedirector()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        if( pDevExt->Specific.RDR.CacheFileHandle != NULL)
        {

            ZwClose( pDevExt->Specific.RDR.CacheFileHandle);

            pDevExt->Specific.RDR.CacheFileHandle = NULL;
        }

        if( pDevExt->Specific.RDR.CacheFileObject != NULL)
        {

            ObDereferenceObject( pDevExt->Specific.RDR.CacheFileObject);

            pDevExt->Specific.RDR.CacheFileObject = NULL;
        }

        if( pDevExt->Specific.RDR.CacheFile.Buffer != NULL)
        {

            ExFreePool( pDevExt->Specific.RDR.CacheFile.Buffer);

            pDevExt->Specific.RDR.CacheFile.Buffer = NULL;
        }

        //
        // Close the volume worker
        //

        AFSShutdownVolumeWorker();

        //
        // Delete resources
        //

        ExDeleteResourceLite( pDevExt->Specific.RDR.FileIDTree.TreeLock);

        ExDeleteResourceLite( &pDevExt->Specific.RDR.FcbListLock);

        //try_exit:

        NOTHING;
    }

    return ntStatus;
}
