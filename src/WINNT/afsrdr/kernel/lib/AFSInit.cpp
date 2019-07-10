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
// File: AFSInit.cpp
//

#include "AFSCommon.h"

//
// DriverEntry
//
// This is the initial entry point for the driver.
//
// Inputs:
//  DriverObject        Pointer to Driver Object created by I/O manager
//  RegistryPath        Pointer to registry path representing this Driver
//
// Returns:
//  Success             To indicate Driver's inituaialization processing
//                      was successful
//  NT ERROR STATUS     Otherwise -- Driver does not remain loaded
//

NTSTATUS
DriverEntry( PDRIVER_OBJECT DriverObject,
             PUNICODE_STRING RegistryPath)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt    *pDeviceExt;
    UNICODE_STRING uniDeviceName;
    ULONG ulIndex = 0;
    UNICODE_STRING uniRoutine;

    BOOLEAN bExit = FALSE;

    __try
    {

        AFSPrint("AFSLibrary DriverEntry Initialization build %s:%s\n", __DATE__, __TIME__);

        //
        // Our backdoor to not let the driver load
        //

        if( bExit)
        {

            //
            // Return a failure so we can update the binary and manually start it without
            // having to do a reboot
            //

            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        //
        // Perform some initialization
        //

        AFSLibraryDriverObject = DriverObject;

        //
        // Setup the registry string
        //

        AFSRegistryPath.MaximumLength = RegistryPath->MaximumLength;
        AFSRegistryPath.Length        = RegistryPath->Length;

        AFSRegistryPath.Buffer = (PWSTR)AFSLibExAllocatePoolWithTag( PagedPool,
                                                                     AFSRegistryPath.Length,
                                                                     AFS_GENERIC_MEMORY_13_TAG);

        if( AFSRegistryPath.Buffer == NULL)
        {

            AFSPrint("AFS DriverEntry Failed to allocate registry path buffer\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlCopyMemory( AFSRegistryPath.Buffer,
                       RegistryPath->Buffer,
                       RegistryPath->Length);

	RtlZeroMemory( &AFSRtlSysVersion,
                       sizeof( RTL_OSVERSIONINFOW));

	AFSRtlSysVersion.dwOSVersionInfoSize = sizeof( RTL_OSVERSIONINFOW);

	RtlGetVersion( &AFSRtlSysVersion);

        RtlInitUnicodeString( &uniRoutine,
                              L"RtlSetGroupSecurityDescriptor");

        AFSRtlSetGroupSecurityDescriptor = (PAFSRtlSetGroupSecurityDescriptor)MmGetSystemRoutineAddress( &uniRoutine);

        ntStatus = AFSCreateDefaultSecurityDescriptor();

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFS DriverEntry  AFSCreateDefaultSecurityDescriptor failed Status %08lX\n", ntStatus);

            ntStatus = STATUS_SUCCESS;
        }

        //
        // Initialize the control device
        //

        RtlInitUnicodeString( &uniDeviceName,
                              AFS_LIBRARY_CONTROL_DEVICE_NAME);

        ntStatus = IoCreateDevice( DriverObject,
                                   sizeof( AFSDeviceExt),
                                   &uniDeviceName,
                                   FILE_DEVICE_NETWORK_FILE_SYSTEM,
                                   0,
                                   FALSE,
                                   &AFSLibraryDeviceObject);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFS DriverEntry - Failed to allocate device control object Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        //
        // Setup the device extension
        //

        pDeviceExt = (AFSDeviceExt *)AFSLibraryDeviceObject->DeviceExtension;

        //
        // Now initialize the control device
        //

        ntStatus = AFSInitializeLibraryDevice();

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Initialize the worker thread pool counts.
        //

        pDeviceExt->Specific.Library.WorkerCount = 0;

        pDeviceExt->Specific.Library.IOWorkerCount = 0;

        //
        // Fill in the dispatch table
        //

        for( ulIndex = 0; ulIndex <= IRP_MJ_MAXIMUM_FUNCTION; ulIndex++)
        {

            DriverObject->MajorFunction[ ulIndex] = AFSDefaultDispatch;
        }

        DriverObject->MajorFunction[IRP_MJ_CREATE] =                    AFSCreate;
        DriverObject->MajorFunction[IRP_MJ_CLOSE] =                     AFSClose;
        DriverObject->MajorFunction[IRP_MJ_READ] =                      AFSRead;
        DriverObject->MajorFunction[IRP_MJ_WRITE] =                     AFSWrite;
        DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] =         AFSQueryFileInfo;
        DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] =           AFSSetFileInfo;
        DriverObject->MajorFunction[IRP_MJ_QUERY_EA] =                  AFSQueryEA;
        DriverObject->MajorFunction[IRP_MJ_SET_EA] =                    AFSSetEA;
        DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] =             AFSFlushBuffers;
        DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] =  AFSQueryVolumeInfo;
        DriverObject->MajorFunction[IRP_MJ_SET_VOLUME_INFORMATION] =    AFSSetVolumeInfo;
        DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL] =         AFSDirControl;
        DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] =       AFSFSControl;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =            AFSDevControl;
        DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] =   AFSInternalDevControl;
        DriverObject->MajorFunction[IRP_MJ_SHUTDOWN] =                  AFSShutdown;
        DriverObject->MajorFunction[IRP_MJ_LOCK_CONTROL] =              AFSLockControl;
        DriverObject->MajorFunction[IRP_MJ_CLEANUP] =                   AFSCleanup;
        DriverObject->MajorFunction[IRP_MJ_QUERY_SECURITY] =            AFSQuerySecurity;
        DriverObject->MajorFunction[IRP_MJ_SET_SECURITY] =              AFSSetSecurity;
        DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] =            AFSSystemControl;
        //DriverObject->MajorFunction[IRP_MJ_QUERY_QUOTA] =               AFSQueryQuota;
        //DriverObject->MajorFunction[IRP_MJ_SET_QUOTA] =                 AFSSetQuota;

        DriverObject->DriverUnload = AFSUnload;

        AFSSysProcess = PsGetCurrentProcessId();

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFSLibrary DriverEntry failed to initialize %08lX\n", ntStatus);

            if( AFSRegistryPath.Buffer != NULL)
            {

		AFSLibExFreePoolWithTag( AFSRegistryPath.Buffer,
					 AFS_GENERIC_MEMORY_13_TAG);
            }

            if( AFSLibraryDeviceObject != NULL)
            {

                AFSRemoveLibraryDevice();

                IoDeleteDevice( AFSLibraryDeviceObject);
            }
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint( "EXCEPTION - AFS DriverEntry\n");

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}

void
AFSUnload( IN PDRIVER_OBJECT DriverObject)
{

    UNREFERENCED_PARAMETER(DriverObject);
    if( AFSGlobalRoot != NULL)
    {

        AFSInvalidateVolume( AFSGlobalRoot,
                             AFS_INVALIDATE_CALLBACK);

        ClearFlag( AFSGlobalRoot->Flags, AFS_VOLUME_ACTIVE_GLOBAL_ROOT);

        AFSShutdownVolumeWorker( AFSGlobalRoot);
    }

    if( AFSLibraryDeviceObject != NULL)
    {

        AFSRemoveWorkerPool();
    }

    if( AFSRegistryPath.Buffer != NULL)
    {

	AFSLibExFreePoolWithTag( AFSRegistryPath.Buffer,
				 AFS_GENERIC_MEMORY_13_TAG);
    }

    AFSCloseLibrary();

    if( AFSDefaultSD != NULL)
    {

	AFSLibExFreePoolWithTag( AFSDefaultSD,
				 AFS_GENERIC_MEMORY_27_TAG);
    }

    if( AFSLibraryDeviceObject != NULL)
    {

        AFSRemoveLibraryDevice();

        IoDeleteDevice( AFSLibraryDeviceObject);
    }

    return;
}
