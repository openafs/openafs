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
    ULONG ulTimeIncrement = 0;
    UNICODE_STRING uniSymLinkName;
    UNICODE_STRING uniDeviceName;
    ULONG ulIndex = 0;

    BOOLEAN bExit = FALSE;

    __try
    {

        AFSPrint("AFS DriverEntry Initialization build %s:%s\n", __DATE__, __TIME__);
        AFSBreakPoint();

        //
        // Initialize some local variables for easier processing
        //

        uniSymLinkName.Buffer = NULL;

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
        // Initialize the debug log
        //

#ifdef AFS_DEBUG_LOG

        AFSInitializeDbgLog();

        AFSDbgLogMsg( "AFSRedirector Driver Entry build %s:%s\n", __DATE__, __TIME__);

#endif

        //
        // Perform some initialization
        //

        AFSDriverObject = DriverObject;

        ntStatus = AFSReadRegistry( RegistryPath);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFS DriverEntry: Failed to read registry Status %08lX\n", ntStatus);

            ntStatus = STATUS_SUCCESS;
        }

        if( BooleanFlagOn( AFSDebugFlags, AFS_DBG_FLAG_BREAK_ON_ENTRY))
        {

            AFSPrint("AFS DriverEntrty - Break on entry\n");

            AFSBreakPoint();

            if ( bExit) 
            {
                //
                // Just as above
                //
                try_return( ntStatus = STATUS_UNSUCCESSFUL);
            }
        }

        //
        // Initialize some server name based strings
        //

        AFSInitServerStrings();

        //
        // Setup the registry string
        //

        AFSRegistryPath.MaximumLength = RegistryPath->MaximumLength;
        AFSRegistryPath.Length        = RegistryPath->Length;

        AFSRegistryPath.Buffer = (PWSTR)ExAllocatePoolWithTag( PagedPool,
                                                               AFSRegistryPath.Length,
                                                               AFS_GENERIC_MEMORY_TAG);

        if( AFSRegistryPath.Buffer == NULL)
        {

            AFSPrint("AFS DriverEntry Failed to allocate registry path buffer\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlCopyMemory( AFSRegistryPath.Buffer,
                       RegistryPath->Buffer,
                       RegistryPath->Length);

        RtlInitUnicodeString( &uniDeviceName, 
                              AFS_CONTROL_DEVICE_NAME);

#ifdef NO_SECURE_CREATE

        ntStatus = IoCreateDevice( DriverObject,
                                   sizeof( AFSDeviceExt),
                                   &uniDeviceName,
                                   FILE_DEVICE_NETWORK_FILE_SYSTEM,
                                   0,
                                   FALSE,
                                   &AFSDeviceObject);
        
#endif

//#ifdef SECURE_CREATE

        ntStatus = IoCreateDeviceSecure( DriverObject,
                                         sizeof( AFSDeviceExt),
                                         &uniDeviceName,
                                         FILE_DEVICE_NETWORK_FILE_SYSTEM,
                                         0,
                                         FALSE, 
                                         &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX,
                                         (LPCGUID)&GUID_SD_AFS_REDIRECTOR_CONTROL_OBJECT,
                                         &AFSDeviceObject);
//#endif

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFS DriverEntry - Failed to allocate device control object Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        //
        // Allocate our symbolic link for service communication
        //

        RtlInitUnicodeString( &uniSymLinkName, 
                              AFS_SYMLINK_NAME);

        ntStatus = IoCreateSymbolicLink( &uniSymLinkName, 
                                         &uniDeviceName);

        if( !NT_SUCCESS( ntStatus)) 
        {

            AFSPrint("AFS DriverEntry - Failed to create symbolic link Status %08lX\n", ntStatus);

            //
            // OK, no one can communicate with us so fail
            //

            try_return( ntStatus);
        }

        //
        // Setup the device extension
        //

        pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

        //
        // Now initialize the control device
        //

        ntStatus = AFSInitializeControlFilter();

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Initialize the worker thread pool
        //

        ntStatus = AFSInitializeWorkerPool();

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFS DriverEntry Failed to initialize worker pool Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        //
        // Calculate the time count lifetime and delay time for Fcbs
        //

        ulTimeIncrement = KeQueryTimeIncrement();
        
        pDeviceExt->Specific.Control.FcbLifeTimeCount.QuadPart = (ULONGLONG)((ULONGLONG)AFS_FCB_LIFETIME / (ULONGLONG)ulTimeIncrement);
        pDeviceExt->Specific.Control.FcbPurgeTimeCount.QuadPart = AFS_ONE_SECOND;
        pDeviceExt->Specific.Control.FcbPurgeTimeCount.QuadPart *= AFS_SERVER_PURGE_DELAY;
        pDeviceExt->Specific.Control.FcbPurgeTimeCount.QuadPart /= ulTimeIncrement;
        pDeviceExt->Specific.Control.FcbFlushTimeCount.QuadPart = (ULONGLONG)((ULONGLONG)(AFS_ONE_SECOND * AFS_SERVER_FLUSH_DELAY) / (ULONGLONG)ulTimeIncrement);

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

        //
        // Since we are not a true FSD then we are not controlling a device and hence these will not be needed
        //

#ifdef FSD_NOT_USED

        DriverObject->MajorFunction[IRP_MJ_POWER] =                     AFSPower;
        DriverObject->MajorFunction[IRP_MJ_PNP] =                       AFSPnP;

#endif

        //
        // For debugging purposes, have an unload routine
        //

        DriverObject->DriverUnload = AFSUnload;

        //
        // Fast IO Dispatch table
        //

        DriverObject->FastIoDispatch = &AFSFastIoDispatch;

        RtlZeroMemory( &AFSFastIoDispatch, 
                       sizeof( AFSFastIoDispatch));

        //
        // Again, since we are not a registered FSD many of these are not going to be called. They are here
        // for completeness.
        //

        AFSFastIoDispatch.SizeOfFastIoDispatch         = sizeof(FAST_IO_DISPATCH);
        AFSFastIoDispatch.FastIoCheckIfPossible        = AFSFastIoCheckIfPossible;  //  CheckForFastIo
        AFSFastIoDispatch.FastIoRead                   = AFSFastIoRead;             //  Read
        AFSFastIoDispatch.FastIoWrite                  = AFSFastIoWrite;            //  Write
        AFSFastIoDispatch.FastIoQueryBasicInfo         = AFSFastIoQueryBasicInfo;     //  QueryBasicInfo
        AFSFastIoDispatch.FastIoQueryStandardInfo      = AFSFastIoQueryStandardInfo;       //  QueryStandardInfo
        AFSFastIoDispatch.FastIoLock                   = AFSFastIoLock;               //  Lock
        AFSFastIoDispatch.FastIoUnlockSingle           = AFSFastIoUnlockSingle;       //  UnlockSingle
        AFSFastIoDispatch.FastIoUnlockAll              = AFSFastIoUnlockAll;          //  UnlockAll
        AFSFastIoDispatch.FastIoUnlockAllByKey         = AFSFastIoUnlockAllByKey;     //  UnlockAllByKey
        AFSFastIoDispatch.FastIoQueryNetworkOpenInfo   = AFSFastIoQueryNetworkOpenInfo;
        AFSFastIoDispatch.AcquireForCcFlush            = AFSFastIoAcquireForCCFlush;
        AFSFastIoDispatch.ReleaseForCcFlush            = AFSFastIoReleaseForCCFlush;
        AFSFastIoDispatch.FastIoDeviceControl          = AFSFastIoDevCtrl;
        AFSFastIoDispatch.AcquireFileForNtCreateSection = AFSFastIoAcquireFile;
        AFSFastIoDispatch.ReleaseFileForNtCreateSection = AFSFastIoReleaseFile;
        AFSFastIoDispatch.FastIoDetachDevice           = AFSFastIoDetachDevice;
        AFSFastIoDispatch.AcquireForModWrite           = AFSFastIoAcquireForModWrite;
        AFSFastIoDispatch.ReleaseForModWrite           = AFSFastIoReleaseForModWrite;
        AFSFastIoDispatch.MdlRead                      = AFSFastIoMdlRead;
        AFSFastIoDispatch.MdlReadComplete              = AFSFastIoMdlReadComplete;
        AFSFastIoDispatch.PrepareMdlWrite              = AFSFastIoPrepareMdlWrite;
        AFSFastIoDispatch.MdlWriteComplete             = AFSFastIoMdlWriteComplete;
        AFSFastIoDispatch.FastIoReadCompressed         = AFSFastIoReadCompressed;
        AFSFastIoDispatch.FastIoWriteCompressed        = AFSFastIoWriteCompressed;
        AFSFastIoDispatch.MdlReadCompleteCompressed    = AFSFastIoMdlReadCompleteCompressed;
        AFSFastIoDispatch.MdlWriteCompleteCompressed   = AFSFastIoMdlWriteCompleteCompressed;
        AFSFastIoDispatch.FastIoQueryOpen              = AFSFastIoQueryOpen;

        //
        //  Cache manager callback routines.
        //

        AFSCacheManagerCallbacks.AcquireForLazyWrite  = &AFSAcquireFcbForLazyWrite;
        AFSCacheManagerCallbacks.ReleaseFromLazyWrite = &AFSReleaseFcbFromLazyWrite;
        AFSCacheManagerCallbacks.AcquireForReadAhead  = &AFSAcquireFcbForReadAhead;
        AFSCacheManagerCallbacks.ReleaseFromReadAhead = &AFSReleaseFcbFromReadAhead;

        //
        //  System process.
        //

        AFSSysProcess = PsGetCurrentProcess();

        //
        // Register for shutdown notification
        //

        IoRegisterShutdownNotification( AFSDeviceObject);

        //
        // Initialize the redirector device
        //

        ntStatus = AFSInitRDRDevice();

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFS DriverEntry Failed to initialize redirector device Status %08lX\n");

            try_return( ntStatus);
        }

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            AFSPrint("AFS Driver Entry failed to initialize %08lX\n", ntStatus);

#ifdef AFS_DEBUG_LOG

            AFSTearDownDbgLog();

#endif

            if( AFSDeviceObject != NULL)
            {

                AFSRemoveWorkerPool();
            }

            if( AFSRegistryPath.Buffer != NULL)
            {

                ExFreePool( AFSRegistryPath.Buffer);
            }

            if( &uniSymLinkName.Buffer != NULL)
            {

                IoDeleteSymbolicLink( &uniSymLinkName);
            }

            if( AFSDeviceObject != NULL)
            {
                
                ExDeleteResourceLite( &AFSProviderListLock);

                IoUnregisterShutdownNotification( AFSDeviceObject);

                IoDeleteDevice( AFSDeviceObject);
            }
        }
    }
    __except( AFSExceptionFilter( GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSPrint("EXCEPTION - AFS DriverEntry\n");
    }

    return ntStatus;
}

void
AFSUnload( IN PDRIVER_OBJECT DriverObject)
{

    AFSPrint("AFSUnload Entry\n");

    //
    // Call the shutdown routine
    //

    AFSShutdownFilesystem();

    return;
}
