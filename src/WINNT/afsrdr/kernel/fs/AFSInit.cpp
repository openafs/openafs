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

#ifndef AMD64
extern "C"
{
extern void                   *KeServiceDescriptorTable;
};
#endif

typedef NTSTATUS (*PsSetCreateProcessNotifyRoutineEx_t)( PCREATE_PROCESS_NOTIFY_ROUTINE_EX NotifyRoutine, BOOLEAN Remove);

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
    AFSDeviceExt    *pDeviceExt = NULL;
    UNICODE_STRING uniSymLinkName;
    UNICODE_STRING uniDeviceName;
    ULONG ulIndex = 0;
    ULONG ulValue = 0;
    UNICODE_STRING uniValueName;
    BOOLEAN bExit = FALSE;
    UNICODE_STRING uniRoutine;
    RTL_OSVERSIONINFOW sysVersion;
    UNICODE_STRING uniPsSetCreateProcessNotifyRoutineEx;
    PsSetCreateProcessNotifyRoutineEx_t pPsSetCreateProcessNotifyRoutineEx = NULL;

    __try
    {

        DbgPrint("AFSRedirFs DriverEntry Initialization build %s:%s\n", __DATE__, __TIME__);

        //
        // Initialize some local variables for easier processing
        //

        uniSymLinkName.Buffer = NULL;

        AFSDumpFileLocation.Length = 0;
        AFSDumpFileLocation.MaximumLength = 0;
        AFSDumpFileLocation.Buffer = NULL;

        AFSDumpFileName.Length = 0;
        AFSDumpFileName.Buffer = NULL;
        AFSDumpFileName.MaximumLength = 0;

        ExInitializeResourceLite( &AFSDbgLogLock);

        //
        // Initialize the server name
        //

        AFSReadServerName();

        AFSReadMountRootName();

        RtlZeroMemory( &sysVersion,
                       sizeof( RTL_OSVERSIONINFOW));

        sysVersion.dwOSVersionInfoSize = sizeof( RTL_OSVERSIONINFOW);

        RtlGetVersion( &sysVersion);

        RtlInitUnicodeString( &uniRoutine,
                              L"ZwSetInformationToken");

        AFSSetInformationToken = (PAFSSetInformationToken)MmGetSystemRoutineAddress( &uniRoutine);

        if( AFSSetInformationToken == NULL)
        {
#ifndef AMD64
            AFSSrvcTableEntry *pServiceTable = NULL;

            pServiceTable = (AFSSrvcTableEntry *)KeServiceDescriptorTable;

            //
            // Only perform this lookup for Windows XP.
            //

            if( pServiceTable != NULL &&
                sysVersion.dwMajorVersion == 5 &&
                sysVersion.dwMinorVersion == 1)
            {
                AFSSetInformationToken = (PAFSSetInformationToken)pServiceTable->ServiceTable[ 0xE6];
            }
#endif
        }

        //
        // And the global root share name
        //

        RtlInitUnicodeString( &AFSGlobalRootName,
                              AFS_GLOBAL_ROOT_SHARE_NAME);

        RtlZeroMemory( &AFSNoPAGAuthGroup,
                       sizeof( GUID));

        //
        // Our backdoor to not let the driver load
        //

        if( bExit)
        {
            try_return( ntStatus);
        }

        //
        // Perform some initialization
        //

        AFSDriverObject = DriverObject;

        ntStatus = AFSReadRegistry( RegistryPath);

        if( !NT_SUCCESS( ntStatus))
        {

            DbgPrint("AFS DriverEntry: Failed to read registry Status %08lX\n", ntStatus);

            ntStatus = STATUS_SUCCESS;
        }

#if DBG

        if( BooleanFlagOn( AFSDebugFlags, AFS_DBG_FLAG_BREAK_ON_ENTRY))
        {

            DbgPrint("AFSRedirFs DriverEntry - Break on entry\n");

            AFSBreakPoint();

            if ( bExit)
            {
                //
                // Just as above
                //
                try_return( ntStatus = STATUS_UNSUCCESSFUL);
            }
        }
#endif

        if( BooleanFlagOn( AFSDebugFlags, AFS_DBG_REQUIRE_CLEAN_SHUTDOWN) &&
            !BooleanFlagOn( AFSDebugFlags, AFS_DBG_CLEAN_SHUTDOWN))
        {

            AFSPrint("AFS DriverEntry: Failed to shutdown clean, exiting\n");

            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        //
        // Initialize the debug log and dump file interface
        //

        AFSInitializeDbgLog();

        AFSInitializeDumpFile();

        //
        // Setup the registry string
        //

        AFSRegistryPath.MaximumLength = RegistryPath->MaximumLength;
        AFSRegistryPath.Length        = RegistryPath->Length;

        AFSRegistryPath.Buffer = (PWSTR)ExAllocatePoolWithTag( PagedPool,
                                                               AFSRegistryPath.Length,
                                                               AFS_GENERIC_MEMORY_18_TAG);

        if( AFSRegistryPath.Buffer == NULL)
        {

            DbgPrint("AFSRedirFs DriverEntry Failed to allocate registry path buffer\n");

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlCopyMemory( AFSRegistryPath.Buffer,
                       RegistryPath->Buffer,
                       RegistryPath->Length);

        if( BooleanFlagOn( AFSDebugFlags, AFS_DBG_REQUIRE_CLEAN_SHUTDOWN))
        {

            //
            // Update the shutdown flag
            //

            ulValue = (ULONG)-1;

            RtlInitUnicodeString( &uniValueName,
                                  AFS_REG_SHUTDOWN_STATUS);

            AFSUpdateRegistryParameter( &uniValueName,
                                        REG_DWORD,
                                        &ulValue,
                                        sizeof( ULONG));
        }

        RtlInitUnicodeString( &uniDeviceName,
                              AFS_CONTROL_DEVICE_NAME);

        ntStatus = IoCreateDeviceSecure( DriverObject,
                                         sizeof( AFSDeviceExt),
                                         &uniDeviceName,
                                         FILE_DEVICE_NETWORK_FILE_SYSTEM,
                                         FILE_DEVICE_SECURE_OPEN | FILE_REMOTE_DEVICE,
                                         FALSE,
                                         &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX,
                                         (LPCGUID)&GUID_SD_AFS_REDIRECTOR_CONTROL_OBJECT,
                                         &AFSDeviceObject);

        if( !NT_SUCCESS( ntStatus))
        {

            DbgPrint("AFS DriverEntry - Failed to allocate device control object Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        //
        // Setup the device extension
        //

        pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

        InitializeListHead( &pDeviceExt->Specific.Control.DirNotifyList);
        FsRtlNotifyInitializeSync( &pDeviceExt->Specific.Control.NotifySync);

        //
        // Now initialize the control device
        //

        ntStatus = AFSInitializeControlDevice();

        if( !NT_SUCCESS( ntStatus))
        {

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

            DbgPrint("AFS DriverEntry - Failed to create symbolic link Status %08lX\n", ntStatus);

            //
            // OK, no one can communicate with us so fail
            //

            try_return( ntStatus);
        }

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

        AFSSysProcess = PsGetCurrentProcessId();

        //
        // Initialize the worker Queues and their syncrhonization structures
        //

        KeInitializeEvent( &pDeviceExt->Specific.Control.WorkerQueueHasItems,
                           SynchronizationEvent,
                           FALSE);

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.QueueLock);

        KeInitializeEvent( &pDeviceExt->Specific.Control.IOWorkerQueueHasItems,
                           SynchronizationEvent,
                           FALSE);

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.IOQueueLock);


        //
        // Register for shutdown notification
        //

        IoRegisterShutdownNotification( AFSDeviceObject);

        //
        // Initialize the system process cb
        //

        AFSInitializeProcessCB( 0,
                                (ULONGLONG)AFSSysProcess);

        //
        // Initialize the redirector device
        //

        ntStatus = AFSInitRDRDevice();

        if( !NT_SUCCESS( ntStatus))
        {

            DbgPrint("AFS DriverEntry Failed to initialize redirector device Status %08lX\n");

            try_return( ntStatus);
        }

        //
        // Initialize some server name based strings
        //

        AFSInitServerStrings();

        //
        // Register the call back for process creation and tear down.
        // On Vista SP1 and above, PsSetCreateProcessNotifyRoutineEx
        // will be used.  This function returns STATUS_ACCESS_DENIED
        // if there is a signing error.  In that case, the AFSProcessNotifyEx
        // routine has not been registered and we can fallback to the
        // Windows 2000 interface and AFSProcessNotify.
        //

        RtlInitUnicodeString( &uniPsSetCreateProcessNotifyRoutineEx,
                              L"PsSetCreateProcessNotifyRoutineEx");

        pPsSetCreateProcessNotifyRoutineEx = (PsSetCreateProcessNotifyRoutineEx_t)MmGetSystemRoutineAddress(&uniPsSetCreateProcessNotifyRoutineEx);

        ntStatus = STATUS_ACCESS_DENIED;

        if ( pPsSetCreateProcessNotifyRoutineEx)
        {

            ntStatus = pPsSetCreateProcessNotifyRoutineEx( AFSProcessNotifyEx,
                                                           FALSE);
        }

        if ( ntStatus == STATUS_ACCESS_DENIED)
        {

            ntStatus = PsSetCreateProcessNotifyRoutine( AFSProcessNotify,
                                                        FALSE);
        }

        ntStatus = STATUS_SUCCESS;

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            DbgPrint("AFSRedirFs DriverEntry failed to initialize %08lX\n", ntStatus);

            if( AFSRegistryPath.Buffer != NULL)
            {

                ExFreePool( AFSRegistryPath.Buffer);
            }

            if( uniSymLinkName.Buffer != NULL)
            {

                IoDeleteSymbolicLink( &uniSymLinkName);
            }

            if( AFSDeviceObject != NULL)
            {

                AFSRemoveControlDevice();

                FsRtlNotifyUninitializeSync( &pDeviceExt->Specific.Control.NotifySync);

                IoUnregisterShutdownNotification( AFSDeviceObject);

                IoDeleteDevice( AFSDeviceObject);
            }

            AFSTearDownDbgLog();

            ExDeleteResourceLite( &AFSDbgLogLock);
        }
    }
    __except( AFSExceptionFilter( __FUNCTION__, GetExceptionCode(), GetExceptionInformation()) )
    {

        AFSDbgTrace(( 0,
                      0,
                      "EXCEPTION - AFSRedirFs DriverEntry\n"));

        AFSDumpTraceFilesFnc();
    }

    return ntStatus;
}
