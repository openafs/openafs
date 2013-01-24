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
// File: AFSRDRSupport.cpp
//
#include "AFSCommon.h"

typedef NTSTATUS  (*FsRtlRegisterUncProviderEx_t)( PHANDLE  MupHandle, PUNICODE_STRING  RedirDevName, PDEVICE_OBJECT  DeviceObject, ULONG  Flags);

NTSTATUS
AFSInitRDRDevice()
{

    NTSTATUS       ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniDeviceName;
    AFSDeviceExt  *pDeviceExt = NULL;
    UNICODE_STRING uniFsRtlRegisterUncProviderEx;
    FsRtlRegisterUncProviderEx_t pFsRtlRegisterUncProviderEx = NULL;

    __Enter
    {

        RtlInitUnicodeString( &uniDeviceName,
                              AFS_RDR_DEVICE_NAME);

        RtlInitUnicodeString( &uniFsRtlRegisterUncProviderEx,
                              L"FsRtlRegisterUncProviderEx");

        pFsRtlRegisterUncProviderEx = (FsRtlRegisterUncProviderEx_t)MmGetSystemRoutineAddress(&uniFsRtlRegisterUncProviderEx);

        ntStatus = IoCreateDevice( AFSDriverObject,
                                   sizeof( AFSDeviceExt),
                                   pFsRtlRegisterUncProviderEx ? NULL : &uniDeviceName,
                                   FILE_DEVICE_NETWORK_FILE_SYSTEM,
                                   FILE_REMOTE_DEVICE,
                                   FALSE,
                                   &AFSRDRDeviceObject);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRDRDevice IoCreateDevice failure %08lX\n",
                          ntStatus);

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

        ExInitializeResourceLite( &pDeviceExt->Specific.RDR.RootCellTreeLock);

        pDeviceExt->Specific.RDR.RootCellTree.TreeLock = &pDeviceExt->Specific.RDR.RootCellTreeLock;

        pDeviceExt->Specific.RDR.RootCellTree.TreeHead = NULL;

        ExInitializeResourceLite( &pDeviceExt->Specific.RDR.ProviderListLock);

        ntStatus = AFSInitRdrFcb( &pDeviceExt->Fcb);

        if ( !NT_SUCCESS(ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRDRDevice AFSInitRdrFcb failure %08lX\n",
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // Clear the initializing bit
        //

        AFSRDRDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

        //
        // Register this device with MUP with FilterMgr if Vista or above
        //

        if( pFsRtlRegisterUncProviderEx)
        {

            ntStatus = pFsRtlRegisterUncProviderEx( &AFSMUPHandle,
                                                    &uniDeviceName,
                                                    AFSRDRDeviceObject,
                                                    0);
            if ( !NT_SUCCESS( ntStatus))
            {
                AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitRDRDevice FsRtlRegisterUncProvider failure %08lX\n",
                              ntStatus);
            }
        }
        else
        {

            ntStatus = FsRtlRegisterUncProvider( &AFSMUPHandle,
                                                 &uniDeviceName,
                                                 FALSE);

            if ( NT_SUCCESS( ntStatus))
            {

                IoRegisterFileSystem( AFSRDRDeviceObject);
            }
            else
            {
                AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitRDRDevice FsRtlRegisterUncProvider failure %08lX\n",
                              ntStatus);
            }
        }

        //
        // Good to go, all registered and ready to start receiving requests
        //

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            //
            // Delete our device and bail
            //

            ExDeleteResourceLite( pDeviceExt->Specific.RDR.VolumeTree.TreeLock);

            ExDeleteResourceLite( &pDeviceExt->Specific.RDR.VolumeListLock);

            ExDeleteResourceLite( &pDeviceExt->Specific.RDR.RootCellTreeLock);

            ExDeleteResourceLite( &pDeviceExt->Specific.RDR.ProviderListLock);

            IoDeleteDevice( AFSRDRDeviceObject);

            AFSRDRDeviceObject = NULL;

            try_return( ntStatus);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSRDRDeviceControl( IN PDEVICE_OBJECT DeviceObject,
                     IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS           ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation( Irp);
    BOOLEAN            bCompleteIrp = TRUE;

    __Enter
    {

        switch( pIrpSp->Parameters.DeviceIoControl.IoControlCode)
        {

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

                    USHORT usLength = uniPathName.Length;

                    uniPathName.Length = AFSServerName.Length;

                    //
                    // Skip over the first slash in the name
                    //

                    uniPathName.Buffer = &uniPathName.Buffer[ 1];


                    //
                    // Check to see if the first (or only) component
                    // of the path matches the server name
                    //

                    if( RtlCompareUnicodeString( &AFSServerName,
                                                 &uniPathName,
                                                 TRUE) == 0 &&
                        ( usLength == AFSServerName.Length + sizeof( WCHAR) ||
                          uniPathName.Buffer[ AFSServerName.Length / sizeof( WCHAR)] == '\\'))
                    {

                        ntStatus = STATUS_SUCCESS;

                        pPathResponse->LengthAccepted = AFSServerName.Length + sizeof( WCHAR);
                    }
                }

                break;
            }

            case IOCTL_REDIR_QUERY_PATH_EX:
            {

                QUERY_PATH_REQUEST_EX *pPathRequest = (QUERY_PATH_REQUEST_EX *)pIrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
                QUERY_PATH_RESPONSE *pPathResponse = (QUERY_PATH_RESPONSE *)Irp->UserBuffer;
                UNICODE_STRING uniPathName;

                ntStatus = STATUS_BAD_NETWORK_PATH;

                uniPathName.Length = pPathRequest->PathName.Length;
                uniPathName.MaximumLength = uniPathName.Length;

                uniPathName.Buffer = pPathRequest->PathName.Buffer;

                if( uniPathName.Length >= AFSServerName.Length + sizeof( WCHAR))
                {

                    USHORT usLength = uniPathName.Length;

                    uniPathName.Length = AFSServerName.Length;

                    //
                    // Skip over the first slash in the name
                    //

                    uniPathName.Buffer = &uniPathName.Buffer[ 1];


                    //
                    // Check to see if the first (or only) component
                    // of the path matches the server name
                    //

                    if( RtlCompareUnicodeString( &AFSServerName,
                                                 &uniPathName,
                                                 TRUE) == 0 &&
                        ( usLength == AFSServerName.Length + sizeof( WCHAR) ||
                          uniPathName.Buffer[ AFSServerName.Length / sizeof( WCHAR)] == '\\'))
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
    UNICODE_STRING uniServiceName;

    __Enter
    {

        //
        // First this is to load the library
        //

        RtlInitUnicodeString( &uniServiceName,
                              AFS_REDIR_LIBRARY_SERVICE_ENTRY);

        ntStatus = AFSLoadLibrary( 0,
                                   &uniServiceName);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeRedirector AFSLoadLibrary failure %08lX\n",
                          ntStatus);

            try_return( ntStatus);
        }

        //
        // Save off the cache file information
        //

        pDevExt->Specific.RDR.CacheBlockSize = RedirInitInfo->CacheBlockSize;

        pDevExt->Specific.RDR.CacheBlockCount = RedirInitInfo->ExtentCount;

        pDevExt->Specific.RDR.MaximumRPCLength = RedirInitInfo->MaximumChunkLength;

        cacheSizeBytes = RedirInitInfo->ExtentCount;
        cacheSizeBytes.QuadPart *= RedirInitInfo->CacheBlockSize;

        AFSDumpFileLocation.Length = 0;

        AFSDumpFileLocation.MaximumLength = (USHORT)RedirInitInfo->DumpFileLocationLength + (4 * sizeof( WCHAR));

        AFSDumpFileLocation.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                        AFSDumpFileLocation.MaximumLength,
                                                                        AFS_GENERIC_MEMORY_23_TAG);

        if( AFSDumpFileLocation.Buffer == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeRedirector AFS_GENERIC_MEMORY_23_TAG allocation error\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlCopyMemory( AFSDumpFileLocation.Buffer,
                       L"\\??\\",
                       4 * sizeof( WCHAR));

        AFSDumpFileLocation.Length = 4 * sizeof( WCHAR);

        RtlCopyMemory( &AFSDumpFileLocation.Buffer[ AFSDumpFileLocation.Length/sizeof( WCHAR)],
                       (void *)((char *)RedirInitInfo + RedirInitInfo->DumpFileLocationOffset),
                       RedirInitInfo->DumpFileLocationLength);

        AFSDumpFileLocation.Length += (USHORT)RedirInitInfo->DumpFileLocationLength;

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
        if( AFSMaxDirectIo)
        {
            //
            // collect what the user
            //
            pDevExt->Specific.RDR.MaxIo.QuadPart = AFSMaxDirectIo;
            pDevExt->Specific.RDR.MaxIo.QuadPart *= (1024 * 1024);

        }
        else
        {

            pDevExt->Specific.RDR.MaxIo.QuadPart = cacheSizeBytes.QuadPart / 2;
        }

        if (pDevExt->Specific.RDR.MaxIo.QuadPart < (5 * 1024 * 1204))
        {

            pDevExt->Specific.RDR.MaxIo.QuadPart = 5 * 1024 * 1204;

        }

        //
        // For small cache configurations ...
        //

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

        if( BooleanFlagOn( RedirInitInfo->Flags, AFS_REDIR_INIT_FLAG_DISABLE_SHORTNAMES))
        {

            //
            // Hide files which begin with .
            //

            SetFlag( pDevExt->DeviceFlags, AFS_DEVICE_FLAG_DISABLE_SHORTNAMES);
        }

        if( RedirInitInfo->MemoryCacheOffset.QuadPart != 0 &&
            RedirInitInfo->MemoryCacheLength.QuadPart != 0)
        {

            ntStatus = STATUS_INSUFFICIENT_RESOURCES;

#ifdef AMD64
            pDevExt->Specific.RDR.CacheMdl = MmCreateMdl( NULL,
                                                          (void *)RedirInitInfo->MemoryCacheOffset.QuadPart,
                                                          RedirInitInfo->MemoryCacheLength.QuadPart);
#else
            pDevExt->Specific.RDR.CacheMdl = MmCreateMdl( NULL,
                                                          (void *)RedirInitInfo->MemoryCacheOffset.LowPart,
                                                          RedirInitInfo->MemoryCacheLength.LowPart);
#endif

            if( pDevExt->Specific.RDR.CacheMdl != NULL)
            {

                __try
                {

                    MmProbeAndLockPages( pDevExt->Specific.RDR.CacheMdl,
                                         KernelMode,
                                         IoModifyAccess);

                    pDevExt->Specific.RDR.CacheBaseAddress = MmGetSystemAddressForMdlSafe( pDevExt->Specific.RDR.CacheMdl,
                                                                                           NormalPagePriority);
                }
                __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
                {

                    AFSDumpTraceFilesFnc();

                    IoFreeMdl( pDevExt->Specific.RDR.CacheMdl);
                    pDevExt->Specific.RDR.CacheMdl = NULL;
                }

                if( pDevExt->Specific.RDR.CacheMdl != NULL)
                {
                    pDevExt->Specific.RDR.CacheLength = RedirInitInfo->MemoryCacheLength;
                    ntStatus = STATUS_SUCCESS;
                }

            }
        }

        if( !NT_SUCCESS( ntStatus) &&
            RedirInitInfo->CacheFileNameLength == 0)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeRedirector Unable to initialize cache file %08lX\n",
                          ntStatus);

            try_return( ntStatus);
        }

        if( pDevExt->Specific.RDR.CacheMdl == NULL)
        {

            if( RedirInitInfo->CacheFileNameLength == 0)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitializeRedirector CacheMdl == NULL\n");

                try_return( ntStatus = STATUS_INVALID_PARAMETER);
            }

            //
            // Go open the cache file
            //

            pDevExt->Specific.RDR.CacheFile.Length = 0;
            pDevExt->Specific.RDR.CacheFile.MaximumLength = (USHORT)RedirInitInfo->CacheFileNameLength + (4 * sizeof( WCHAR));

            pDevExt->Specific.RDR.CacheFile.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                                        pDevExt->Specific.RDR.CacheFile.MaximumLength,
                                                                                        AFS_GENERIC_MEMORY_24_TAG);

            if( pDevExt->Specific.RDR.CacheFile.Buffer == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitializeRedirector AFS_GENERIC_MEMORY_24_TAG allocation failure\n");

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

            InitializeObjectAttributes( &stObjectAttribs,
                                        &pDevExt->Specific.RDR.CacheFile,
                                        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                                        NULL,
                                        NULL);

            ntStatus = ZwOpenFile( &pDevExt->Specific.RDR.CacheFileHandle,
                                   GENERIC_READ | GENERIC_WRITE,
                                   &stObjectAttribs,
                                   &stIoStatus,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   FILE_WRITE_THROUGH | FILE_RANDOM_ACCESS);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitializeRedirector ZwOpenFile failure %08lX\n",
                              ntStatus);

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

                AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSInitializeRedirector ObReferenceObjectByHandle failure %08lX\n",
                              ntStatus);

                try_return( ntStatus);
            }
        }

        pDevExt->Specific.RDR.MaxLinkCount = RedirInitInfo->MaxPathLinkCount;

        pDevExt->Specific.RDR.NameArrayLength = RedirInitInfo->NameArrayLength;

        //
        // Intialize the library
        //

        ntStatus = AFSInitializeLibrary( &RedirInitInfo->GlobalFileId,
                                         TRUE);

        if ( !NT_SUCCESS( ntStatus))
        {
            AFSDbgLogMsg( AFS_SUBSYSTEM_INIT_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitializeRedirector AFSInitializeLibrary failure %08lX\n",
                          ntStatus);
        }

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            if( pDevExt->Specific.RDR.CacheMdl != NULL)
            {

                MmUnmapLockedPages( pDevExt->Specific.RDR.CacheBaseAddress,
                                    pDevExt->Specific.RDR.CacheMdl);

                MmUnlockPages( pDevExt->Specific.RDR.CacheMdl);

                ExFreePool( pDevExt->Specific.RDR.CacheMdl);

                pDevExt->Specific.RDR.CacheMdl = NULL;
                pDevExt->Specific.RDR.CacheBaseAddress = NULL;
                pDevExt->Specific.RDR.CacheLength.QuadPart = 0;
            }

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

            if( AFSDumpFileLocation.Buffer != NULL)
            {
                ExFreePool( AFSDumpFileLocation.Buffer);

                AFSDumpFileLocation.Buffer = NULL;
            }

            if ( pDevExt->Fcb != NULL)
            {

                AFSRemoveRdrFcb( &pDevExt->Fcb);

                pDevExt = NULL;
            }

            AFSUnloadLibrary( TRUE);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSCloseRedirector()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        //
        // Unload the library first so we close off any accesses to the cache or underlying
        // shared memory
        //

        AFSUnloadLibrary( TRUE);

        //
        // Close off the cache file or mapping
        //

        if( pDevExt->Specific.RDR.CacheMdl != NULL)
        {

            MmUnmapLockedPages( pDevExt->Specific.RDR.CacheBaseAddress,
                                pDevExt->Specific.RDR.CacheMdl);

            MmUnlockPages( pDevExt->Specific.RDR.CacheMdl);

            ExFreePool( pDevExt->Specific.RDR.CacheMdl);

            pDevExt->Specific.RDR.CacheMdl = NULL;
            pDevExt->Specific.RDR.CacheBaseAddress = NULL;
            pDevExt->Specific.RDR.CacheLength.QuadPart = 0;
        }

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

        if( AFSDumpFileLocation.Buffer != NULL)
        {
            ExFreePool( AFSDumpFileLocation.Buffer);

            AFSDumpFileLocation.Buffer = NULL;
        }

        if ( pDevExt->Fcb != NULL)
        {

            AFSRemoveRdrFcb( &pDevExt->Fcb);

            pDevExt->Fcb = NULL;
        }

    }

    return ntStatus;
}

//
// Function: AFSInitRdrFcb
//
// Description:
//
//      This function performs Redirector Fcb initialization
//
// Return:
//
//      A status is returned for the function
//

NTSTATUS
AFSInitRdrFcb( OUT AFSFcb **RdrFcb)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSFcb *pFcb = NULL;
    AFSNonPagedFcb *pNPFcb = NULL;

    __Enter
    {

        //
        // Initialize the root fcb
        //

        pFcb = (AFSFcb *)AFSExAllocatePoolWithTag( PagedPool,
                                                   sizeof( AFSFcb),
                                                   AFS_FCB_ALLOCATION_TAG);

        if( pFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRdrFcb Failed to allocate the root fcb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pFcb,
                       sizeof( AFSFcb));

        pFcb->Header.NodeByteSize = sizeof( AFSFcb);
        pFcb->Header.NodeTypeCode = AFS_REDIRECTOR_FCB;

        pNPFcb = (AFSNonPagedFcb *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                             sizeof( AFSNonPagedFcb),
                                                             AFS_FCB_NP_ALLOCATION_TAG);

        if( pNPFcb == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSInitRdrFcb Failed to allocate the non-paged fcb\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pNPFcb,
                       sizeof( AFSNonPagedFcb));

        pNPFcb->Size = sizeof( AFSNonPagedFcb);

        pNPFcb->Type = AFS_NON_PAGED_FCB;

        //
        // OK, initialize the entry
        //

        ExInitializeFastMutex( &pNPFcb->AdvancedHdrMutex);

        FsRtlSetupAdvancedHeader( &pFcb->Header, &pNPFcb->AdvancedHdrMutex);

        ExInitializeResourceLite( &pNPFcb->Resource);

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSInitRootFcb Acquiring Fcb lock %p EXCL %08lX\n",
                      &pNPFcb->Resource,
                      PsGetCurrentThread());

        ExInitializeResourceLite( &pNPFcb->PagingResource);

        pFcb->Header.Resource = &pNPFcb->Resource;

        pFcb->Header.PagingIoResource = &pNPFcb->PagingResource;

        pFcb->NPFcb = pNPFcb;

        if ( InterlockedCompareExchangePointer( (PVOID *)RdrFcb, pFcb, NULL) != NULL)
        {

            try_return( ntStatus = STATUS_REPARSE);
        }

try_exit:

        if( ntStatus != STATUS_SUCCESS)
        {

            if( pFcb != NULL)
            {

                AFSRemoveRdrFcb( &pFcb);
            }
        }
    }

    return ntStatus;
}

//
// Function: AFSRemoveRdrFcb
//
// Description:
//
//      This function performs Redirector Fcb removal/deallocation
//
// Return:
//
//      void.
//

void
AFSRemoveRdrFcb( IN OUT AFSFcb **RdrFcb)
{
    AFSFcb *pFcb = NULL;

    pFcb = (AFSFcb *) InterlockedCompareExchangePointer( (PVOID *)RdrFcb, NULL, (PVOID)(*RdrFcb));

    if ( pFcb == NULL)
    {

        return;
    }

    if( pFcb->NPFcb != NULL)
    {

        //
        // Now the resource
        //

        ExDeleteResourceLite( &pFcb->NPFcb->Resource);

        ExDeleteResourceLite( &pFcb->NPFcb->PagingResource);

        //
        // The non paged region
        //

        AFSExFreePoolWithTag( pFcb->NPFcb, AFS_FCB_NP_ALLOCATION_TAG);
    }

    //
    // And the Fcb itself
    //

    AFSExFreePoolWithTag( pFcb, AFS_FCB_ALLOCATION_TAG);

    return;
}
