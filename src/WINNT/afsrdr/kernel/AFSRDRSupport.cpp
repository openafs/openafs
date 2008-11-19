/*
 * Copyright (c) 2008 Kernel Drivers, LLC.
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
 * - Neither the name of Kernel Drivers, LLC nor the names of its
 *   contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission from Kernel Drivers, LLC.
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

#include "AFSCommon.h"

NTSTATUS
AFSInitRDRDevice()
{

    NTSTATUS       ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniDeviceName;
    ULONG          ulIndex = 0;
    AFSDeviceExt  *pDeviceExt = NULL;

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

        pDeviceExt->Specific.RDR.VolumeTree.TreeLock = &pDeviceExt->Specific.RDR.VolumeTreeLock;

        ExInitializeResourceLite( pDeviceExt->Specific.RDR.VolumeTree.TreeLock);

        pDeviceExt->Specific.RDR.VolumeTree.TreeHead = NULL;

        ExInitializeResourceLite( &pDeviceExt->Specific.RDR.VolumeListLock);

        pDeviceExt->Specific.RDR.VolumeListHead = NULL;

        pDeviceExt->Specific.RDR.VolumeListTail = NULL;

        KeInitializeEvent( &pDeviceExt->Specific.RDR.QueuedReleaseExtentEvent,
                           NotificationEvent,
                           TRUE);

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

    NTSTATUS           ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    PFILE_OBJECT       pFileObject = pIrpSp->FileObject;
    AFSFcb            *pFcb = (AFSFcb *)pFileObject->FsContext;
    BOOLEAN            bCompleteIrp = TRUE;

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
                ntStatus = AFSProcessReleaseFileExtents( Irp, TRUE, &bCompleteIrp );

                Irp->IoStatus.Status = ntStatus;
                      
                break;
            }

            case IOCTL_AFS_RELEASE_FILE_EXTENTS_DONE:
            {
                ntStatus = AFSProcessReleaseFileExtentsDone( Irp );
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

                break;
            }

            default:

                ntStatus = STATUS_INVALID_DEVICE_REQUEST;

                break;
        }

        if (bCompleteIrp)
        {
            //
            // Complete the request
            //

            AFSCompleteRequest( Irp,
                                ntStatus);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSInitializeRedirector( IN AFSRedirectorInitInfo *RedirInitInfo)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    LARGE_INTEGER cacheSizeBytes;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    OBJECT_ATTRIBUTES   stObjectAttribs;
    IO_STATUS_BLOCK stIoStatus;

    __Enter
    {

        //
        // Save off the cache file information
        //

        pDevExt->Specific.RDR.CacheBlockSize = RedirInitInfo->CacheBlockSize;

        pDevExt->Specific.RDR.CacheBlockCount = RedirInitInfo->ExtentCount;

        pDevExt->Specific.RDR.MaximumRPCLength = RedirInitInfo->MaximumChunkLength;

        ASSERT( pDevExt->Specific.RDR.MaximumRPCLength != 0);

        cacheSizeBytes = RedirInitInfo->ExtentCount;
        cacheSizeBytes.QuadPart *= RedirInitInfo->CacheBlockSize;

        pDevExt->Specific.RDR.CacheFile.Length = 0;

        pDevExt->Specific.RDR.CacheFile.MaximumLength = (USHORT)RedirInitInfo->CacheFileNameLength + (4 * sizeof( WCHAR));

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
                       RedirInitInfo->CacheFileName,
                       RedirInitInfo->CacheFileNameLength);

        pDevExt->Specific.RDR.CacheFile.Length += (USHORT)RedirInitInfo->CacheFileNameLength;

        //
        // Be sure the shutdown flag is not set
        //

        ClearFlag( pDevExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN);

        //
        // Set up the Throttles.
        //
        // Max IO is 10% of the cache, or the value in the registry,
        // with a minimum of 5Mb (and a maximum of 50% cache size)
        //
        if (AFSMaxDirectIo) 
        {
            //
            // collect what the user 
            //
            pDevExt->Specific.RDR.MaxIo.QuadPart = AFSMaxDirectIo;
            pDevExt->Specific.RDR.MaxIo.QuadPart *= (1024 * 1024);

        } 
        else 
        {

            pDevExt->Specific.RDR.MaxIo.QuadPart = cacheSizeBytes.QuadPart / 10;
        }

        if (pDevExt->Specific.RDR.MaxIo.QuadPart < (5 * 1024 * 1204)) 
        {
            
            pDevExt->Specific.RDR.MaxIo.QuadPart = 5 * 1024 * 1204;

        } 
        

        if (pDevExt->Specific.RDR.MaxIo.QuadPart > cacheSizeBytes.QuadPart / 2)
        {

            pDevExt->Specific.RDR.MaxIo.QuadPart  = cacheSizeBytes.QuadPart / 2;

        }

        //
        // Maximum Dirty is 50% of the cache, or the value in the
        // registry.  No minimum, maximum of 90% of cache size.
        //
        if (AFSMaxDirtyFile) 
        {
            
            pDevExt->Specific.RDR.MaxDirty.QuadPart = AFSMaxDirtyFile;
            pDevExt->Specific.RDR.MaxDirty.QuadPart *= (1024 * 1024);

        } 
        else 
        {

            pDevExt->Specific.RDR.MaxDirty.QuadPart = cacheSizeBytes.QuadPart/2;

        }

        cacheSizeBytes.QuadPart *= 9;
        cacheSizeBytes.QuadPart  = cacheSizeBytes.QuadPart / 10;

        if (pDevExt->Specific.RDR.MaxDirty.QuadPart > cacheSizeBytes.QuadPart) 
        {
            pDevExt->Specific.RDR.MaxDirty.QuadPart = cacheSizeBytes.QuadPart;
        }

        //
        // Store off any flags for the file system
        //

        if( BooleanFlagOn( RedirInitInfo->Flags, AFS_REDIR_INIT_FLAG_HIDE_DOT_FILES))
        {

            //
            // Hide files which begin with .
            //

            SetFlag( pDevExt->DeviceFlags, AFS_DEVICE_FLAG_HIDE_DOT_NAMES);
        }

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
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSDeviceExt *pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    
    __Enter
    {

        //
        // When shutting down the redirector we first set the SHUTDOWN flag so no more opens or
        // IO can be processed. Then force the volume workers to flush all data from all files
        //

        SetFlag( pRDRDevExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN);

        //
        // Now wait for the volume workers to close, they will be attempting to flush all data for the files
        //

        ntStatus = KeWaitForSingleObject( &pControlDevExt->Specific.Control.VolumeWorkerCloseEvent,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          NULL);
    }

    return ntStatus;
}
