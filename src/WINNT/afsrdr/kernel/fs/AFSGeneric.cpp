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
// File: AFSGeneric.cpp
//

#include "AFSCommon.h"

//
// Function: AFSExceptionFilter
//
// Description:
//
//      This function is the exception handler
//
// Return:
//
//      A status is returned for the function
//

ULONG
AFSExceptionFilter( IN CHAR *FunctionString,
                    IN ULONG Code,
                    IN PEXCEPTION_POINTERS ExceptPtrs)
{
    UNREFERENCED_PARAMETER(Code);

    PEXCEPTION_RECORD ExceptRec;
    PCONTEXT Context;

    __try
    {

        ExceptRec = ExceptPtrs->ExceptionRecord;

        Context = ExceptPtrs->ContextRecord;

        AFSDbgLogMsg( 0,
                      0,
                      "AFSExceptionFilter (Framework) - EXR %p CXR %p Function %s Code %08lX Address %p Routine %p\n",
                      ExceptRec,
                      Context,
                      FunctionString,
                      ExceptRec->ExceptionCode,
                      ExceptRec->ExceptionAddress,
                      (void *)AFSExceptionFilter);

        DbgPrint("**** Exception Caught in AFS Redirector ****\n");

        DbgPrint("\n\nPerform the following WnDbg Cmds:\n");
        DbgPrint("\n\t.exr %p ;  .cxr %p\n\n", ExceptRec, Context);

        DbgPrint("**** Exception Complete from AFS Redirector ****\n");

        if( BooleanFlagOn( AFSDebugFlags, AFS_DBG_BUGCHECK_EXCEPTION))
        {

            KeBugCheck( (ULONG)-2);
        }
        else
        {

            AFSBreakPoint();
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER)
    {

        NOTHING;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

//
// Function: AFSAcquireExcl()
//
// Purpose: Called to acquire a resource exclusive with optional wait
//
// Parameters:
//                PERESOURCE Resource - Resource to acquire
//                BOOLEAN Wait - Whether to block
//
// Return:
//                BOOLEAN - Whether the mask was acquired
//

BOOLEAN
AFSAcquireExcl( IN PERESOURCE Resource,
                IN BOOLEAN wait)
{

    BOOLEAN bStatus = FALSE;

    //
    // Normal kernel APCs must be disabled before calling
    // ExAcquireResourceExclusiveLite. Otherwise a bugcheck occurs.
    //

    KeEnterCriticalRegion();

    bStatus = ExAcquireResourceExclusiveLite( Resource,
                                              wait);

    if( !bStatus)
    {

        KeLeaveCriticalRegion();
    }

    return bStatus;
}

BOOLEAN
AFSAcquireSharedStarveExclusive( IN PERESOURCE Resource,
                                 IN BOOLEAN Wait)
{

    BOOLEAN bStatus = FALSE;

    KeEnterCriticalRegion();

    bStatus = ExAcquireSharedStarveExclusive( Resource,
                                              Wait);

    if( !bStatus)
    {

        KeLeaveCriticalRegion();
    }

    return bStatus;
}

//
// Function: AFSAcquireShared()
//
// Purpose: Called to acquire a resource shared with optional wait
//
// Parameters:
//                PERESOURCE Resource - Resource to acquire
//                BOOLEAN Wait - Whether to block
//
// Return:
//                BOOLEAN - Whether the mask was acquired
//

BOOLEAN
AFSAcquireShared( IN PERESOURCE Resource,
                  IN BOOLEAN wait)
{

    BOOLEAN bStatus = FALSE;

    KeEnterCriticalRegion();

    bStatus = ExAcquireResourceSharedLite( Resource,
                                           wait);

    if( !bStatus)
    {

        KeLeaveCriticalRegion();
    }

    return bStatus;
}

//
// Function: AFSReleaseResource()
//
// Purpose: Called to release a resource
//
// Parameters:
//                PERESOURCE Resource - Resource to release
//
// Return:
//                None
//

void
AFSReleaseResource( IN PERESOURCE Resource)
{

    if( Resource != &AFSDbgLogLock)
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSReleaseResource Releasing lock %p Thread %08lX\n",
                      Resource,
                      PsGetCurrentThread());
    }

    ExReleaseResourceLite( Resource);

    KeLeaveCriticalRegion();

    return;
}

void
AFSConvertToShared( IN PERESOURCE Resource)
{

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSConvertToShared Converting lock %p Thread %08lX\n",
                  Resource,
                  PsGetCurrentThread());

    ExConvertExclusiveToSharedLite( Resource);

    return;
}

//
// Function: AFSCompleteRequest
//
// Description:
//
//      This function completes irps
//
// Return:
//
//      A status is returned for the function
//

void
AFSCompleteRequest( IN PIRP Irp,
                    IN ULONG Status)
{

    Irp->IoStatus.Status = Status;

    IoCompleteRequest( Irp,
                       IO_NO_INCREMENT);

    return;
}

NTSTATUS
AFSReadRegistry( IN PUNICODE_STRING RegistryPath)
{

    NTSTATUS ntStatus        = STATUS_SUCCESS;
    ULONG Default            = 0;
    UNICODE_STRING paramPath;
    ULONG Value                = 0;
    RTL_QUERY_REGISTRY_TABLE paramTable[2];
    UNICODE_STRING defaultUnicodeName;
    WCHAR SubKeyString[]    = L"\\Parameters";

    //
    // Setup the paramPath buffer.
    //

    paramPath.MaximumLength = RegistryPath->Length + sizeof( SubKeyString);
    paramPath.Buffer = (PWSTR)AFSExAllocatePoolWithTag( PagedPool,
                                                        paramPath.MaximumLength,
                                                        AFS_GENERIC_MEMORY_15_TAG);

    RtlInitUnicodeString( &defaultUnicodeName,
                          L"NO NAME");

    //
    // If it exists, setup the path.
    //

    if( paramPath.Buffer != NULL)
    {

        //
        // Move in the paths
        //

        RtlCopyMemory( &paramPath.Buffer[ 0],
                       &RegistryPath->Buffer[ 0],
                       RegistryPath->Length);

        RtlCopyMemory( &paramPath.Buffer[ RegistryPath->Length / 2],
                       SubKeyString,
                       sizeof( SubKeyString));

        paramPath.Length = paramPath.MaximumLength;

        RtlZeroMemory( paramTable,
                       sizeof( paramTable));

        Value = 0;

        //
        // Setup the table to query the registry for the needed value
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = AFS_REG_DEBUG_FLAGS;
        paramTable[0].EntryContext = &Value;

        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &Default;
        paramTable[0].DefaultLength = sizeof (ULONG) ;

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                           paramPath.Buffer,
                                           paramTable,
                                           NULL,
                                           NULL);

        if( NT_SUCCESS( ntStatus))
        {

            AFSDebugFlags = Value;
        }

        RtlZeroMemory( paramTable,
                       sizeof( paramTable));

        Value = 0;

        //
        // Setup the table to query the registry for the needed value
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = AFS_REG_TRACE_SUBSYSTEM;
        paramTable[0].EntryContext = &Value;

        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &Default;
        paramTable[0].DefaultLength = sizeof (ULONG) ;

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                           paramPath.Buffer,
                                           paramTable,
                                           NULL,
                                           NULL);

        if( NT_SUCCESS( ntStatus))
        {

            AFSTraceComponent = Value;
        }

        RtlZeroMemory( paramTable,
                       sizeof( paramTable));

        Value = 0;

        //
        // Setup the table to query the registry for the needed value
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = AFS_REG_TRACE_BUFFER_LENGTH;
        paramTable[0].EntryContext = &Value;

        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &Default;
        paramTable[0].DefaultLength = sizeof (ULONG);

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                           paramPath.Buffer,
                                           paramTable,
                                           NULL,
                                           NULL);

        if( NT_SUCCESS( ntStatus) &&
            Value > 0)
        {

            AFSDbgBufferLength = Value;

            //
            // Let's limit things a bit ...
            //

            if( AFSDbgBufferLength > 10240)
            {

                AFSDbgBufferLength = 1024;
            }
        }
        else
        {

            AFSDbgBufferLength = 0;
        }

        //
        // Make it bytes
        //

        AFSDbgBufferLength *= 1024;

        //
        // Now get ready to set up for MaxServerDirty
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = AFS_REG_MAX_DIRTY;
        paramTable[0].EntryContext = &Value;

        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &Default;
        paramTable[0].DefaultLength = sizeof (ULONG) ;

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                           paramPath.Buffer,
                                           paramTable,
                                           NULL,
                                           NULL);

        if( NT_SUCCESS( ntStatus))
        {

            AFSMaxDirtyFile = Value;
        }

        RtlZeroMemory( paramTable,
                       sizeof( paramTable));

        Value = 0;

        //
        // Setup the table to query the registry for the needed value
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = AFS_REG_TRACE_LEVEL;
        paramTable[0].EntryContext = &Value;

        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &Default;
        paramTable[0].DefaultLength = sizeof (ULONG) ;

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                           paramPath.Buffer,
                                           paramTable,
                                           NULL,
                                           NULL);

        if( NT_SUCCESS( ntStatus))
        {

            AFSTraceLevel = Value;
        }

        //
        // MaxIO
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = AFS_REG_MAX_IO;
        paramTable[0].EntryContext = &Value;

        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &Default;
        paramTable[0].DefaultLength = sizeof (ULONG) ;

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                           paramPath.Buffer,
                                           paramTable,
                                           NULL,
                                           NULL);

        if( NT_SUCCESS( ntStatus))
        {

            AFSMaxDirectIo = Value;
        }

        //
        // Now set up for ShutdownStatus query
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = AFS_REG_SHUTDOWN_STATUS;
        paramTable[0].EntryContext = &Value;

        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &Default;
        paramTable[0].DefaultLength = sizeof (ULONG) ;

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                           paramPath.Buffer,
                                           paramTable,
                                           NULL,
                                           NULL);

        if( !NT_SUCCESS( ntStatus) ||
            Value != (ULONG)-1)
        {

            SetFlag( AFSDebugFlags, AFS_DBG_CLEAN_SHUTDOWN);
        }

        //
        // Now set up for RequireCleanShutdown query
        //

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = AFS_REG_REQUIRE_CLEAN_SHUTDOWN;
        paramTable[0].EntryContext = &Value;

        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &Default;
        paramTable[0].DefaultLength = sizeof (ULONG) ;

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                           paramPath.Buffer,
                                           paramTable,
                                           NULL,
                                           NULL);

        if( !NT_SUCCESS( ntStatus) ||
            Value != 0L)
        {

            SetFlag( AFSDebugFlags, AFS_DBG_REQUIRE_CLEAN_SHUTDOWN);
        }

        //
        // Free up the buffer
        //

        ExFreePool( paramPath.Buffer);

        ntStatus = STATUS_SUCCESS;
    }
    else
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
}

NTSTATUS
AFSUpdateRegistryParameter( IN PUNICODE_STRING ValueName,
                            IN ULONG ValueType,
                            IN void *ValueData,
                            IN ULONG ValueDataLength)
{

    NTSTATUS ntStatus        = STATUS_SUCCESS;
    UNICODE_STRING paramPath, uniParamKey;
    HANDLE hParameters = 0;
    OBJECT_ATTRIBUTES stObjectAttributes;

    __Enter
    {

        RtlInitUnicodeString( &uniParamKey,
                              L"\\Parameters");

        //
        // Setup the paramPath buffer.
        //

        paramPath.MaximumLength = AFSRegistryPath.Length + uniParamKey.Length;
        paramPath.Buffer = (PWSTR)AFSExAllocatePoolWithTag( PagedPool,
                                                            paramPath.MaximumLength,
                                                            AFS_GENERIC_MEMORY_16_TAG);

        if( paramPath.Buffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Move in the paths
        //

        RtlCopyMemory( paramPath.Buffer,
                       AFSRegistryPath.Buffer,
                       AFSRegistryPath.Length);

        paramPath.Length = AFSRegistryPath.Length;

        RtlCopyMemory( &paramPath.Buffer[ paramPath.Length / 2],
                       uniParamKey.Buffer,
                       uniParamKey.Length);

        paramPath.Length += uniParamKey.Length;

        InitializeObjectAttributes( &stObjectAttributes,
                                    &paramPath,
                                    OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                                    NULL,
                                    NULL);

        ntStatus = ZwOpenKey( &hParameters,
                              KEY_ALL_ACCESS,
                              &stObjectAttributes);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        //
        // Set the value
        //

        ntStatus = ZwSetValueKey( hParameters,
                                  ValueName,
                                  0,
                                  ValueType,
                                  ValueData,
                                  ValueDataLength);

        ZwClose( hParameters);

try_exit:

        if( paramPath.Buffer != NULL)
        {

            //
            // Free up the buffer
            //

            ExFreePool( paramPath.Buffer);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSInitializeControlDevice()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        //
        // Initialize the comm pool resources
        //

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.CommServiceCB.IrpPoolLock);

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.CommServiceCB.ResultPoolLock);

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.ExtentReleaseResource);

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.SysName32ListLock);

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.SysName64ListLock);

        //
        // And the events
        //

        KeInitializeEvent( &pDeviceExt->Specific.Control.CommServiceCB.IrpPoolHasEntries,
                           SynchronizationEvent,
                           FALSE);

        KeInitializeEvent( &pDeviceExt->Specific.Control.CommServiceCB.IrpPoolHasReleaseEntries,
                           SynchronizationEvent,
                           FALSE);

        KeInitializeEvent( &pDeviceExt->Specific.Control.ExtentReleaseEvent,
                           NotificationEvent,
                           FALSE);

        pDeviceExt->Specific.Control.ExtentReleaseSequence = 0;

        KeInitializeEvent( &pDeviceExt->Specific.Control.VolumeWorkerCloseEvent,
                           NotificationEvent,
                           TRUE);

        //
        // Library support information
        //

        KeInitializeEvent( &pDeviceExt->Specific.Control.LoadLibraryEvent,
                           SynchronizationEvent,
                           TRUE);

        //
        // Initialize the library queued as cancelled
        //

        pDeviceExt->Specific.Control.LibraryState = AFS_LIBRARY_QUEUE_CANCELLED;

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.LibraryStateLock);

        pDeviceExt->Specific.Control.InflightLibraryRequests = 0;

        KeInitializeEvent( &pDeviceExt->Specific.Control.InflightLibraryEvent,
                           NotificationEvent,
                           FALSE);

        pDeviceExt->Specific.Control.ExtentCount = 0;
        pDeviceExt->Specific.Control.ExtentsHeldLength = 0;

        KeInitializeEvent( &pDeviceExt->Specific.Control.ExtentsHeldEvent,
                           NotificationEvent,
                           TRUE);

        pDeviceExt->Specific.Control.OutstandingServiceRequestCount = 0;

        KeInitializeEvent( &pDeviceExt->Specific.Control.OutstandingServiceRequestEvent,
                           NotificationEvent,
                           TRUE);

        pDeviceExt->Specific.Control.WaitingForMemoryCount = 0;

        KeInitializeEvent( &pDeviceExt->Specific.Control.MemoryAvailableEvent,
                           NotificationEvent,
                           TRUE);

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.LibraryQueueLock);

        pDeviceExt->Specific.Control.LibraryQueueHead = NULL;

        pDeviceExt->Specific.Control.LibraryQueueTail = NULL;

        //
        // Set the initial state of the irp pool
        //

        pDeviceExt->Specific.Control.CommServiceCB.IrpPoolControlFlag = POOL_INACTIVE;

        //
        // Initialize our process and sid tree information
        //

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.ProcessTreeLock);

        pDeviceExt->Specific.Control.ProcessTree.TreeLock = &pDeviceExt->Specific.Control.ProcessTreeLock;

        pDeviceExt->Specific.Control.ProcessTree.TreeHead = NULL;

        ExInitializeResourceLite( &pDeviceExt->Specific.Control.AuthGroupTreeLock);

        pDeviceExt->Specific.Control.AuthGroupTree.TreeLock = &pDeviceExt->Specific.Control.AuthGroupTreeLock;

        pDeviceExt->Specific.Control.AuthGroupTree.TreeHead = NULL;

        //
        // Increase the StackSize to support the extra stack frame required
        // for use of IoCompletion routines.
        //

        AFSDeviceObject->StackSize++;

    }

    return ntStatus;
}

NTSTATUS
AFSRemoveControlDevice()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        //
        // Initialize the comm pool resources
        //

        ExDeleteResourceLite( &pDeviceExt->Specific.Control.CommServiceCB.IrpPoolLock);

        ExDeleteResourceLite( &pDeviceExt->Specific.Control.CommServiceCB.ResultPoolLock);

        ExDeleteResourceLite( &pDeviceExt->Specific.Control.ExtentReleaseResource);

        ExDeleteResourceLite( &pDeviceExt->Specific.Control.SysName32ListLock);

        ExDeleteResourceLite( &pDeviceExt->Specific.Control.SysName64ListLock);

        ExDeleteResourceLite( &pDeviceExt->Specific.Control.ProcessTreeLock);

        if( pDeviceExt->Specific.Control.ProcessTree.TreeHead != NULL)
        {
            ExFreePool( pDeviceExt->Specific.Control.ProcessTree.TreeHead);
        }

        ExDeleteResourceLite( &pDeviceExt->Specific.Control.AuthGroupTreeLock);

        ExDeleteResourceLite( &pDeviceExt->Specific.Control.LibraryStateLock);

        ExDeleteResourceLite( &pDeviceExt->Specific.Control.LibraryQueueLock);
    }

    return ntStatus;
}

void
AFSInitServerStrings()
{

    UNICODE_STRING uniFullName;
    WCHAR wchBuffer[ 50];

    //
    // Add the server name into the list of resources
    //

    uniFullName.Length = (2 * sizeof( WCHAR)) + AFSServerName.Length;
    uniFullName.MaximumLength = uniFullName.Length + sizeof( WCHAR);

    uniFullName.Buffer = wchBuffer;

    wchBuffer[ 0] = L'\\';
    wchBuffer[ 1] = L'\\';

    RtlCopyMemory( &wchBuffer[ 2],
                   AFSServerName.Buffer,
                   AFSServerName.Length);

    AFSAddConnectionEx( &uniFullName,
                        RESOURCEDISPLAYTYPE_SERVER,
                        0);

    //
    // Add in the global share name
    //

    wchBuffer[ uniFullName.Length/sizeof( WCHAR)] = L'\\';

    uniFullName.Length += sizeof( WCHAR);

    RtlCopyMemory( &wchBuffer[ uniFullName.Length/sizeof( WCHAR)],
                   AFSGlobalRootName.Buffer,
                   AFSGlobalRootName.Length);

    uniFullName.Length += AFSGlobalRootName.Length;

    AFSAddConnectionEx( &uniFullName,
                        RESOURCEDISPLAYTYPE_SHARE,
                        AFS_CONNECTION_FLAG_GLOBAL_SHARE);

    return;
}

NTSTATUS
AFSReadServerName()
{

    NTSTATUS ntStatus        = STATUS_SUCCESS;
    UNICODE_STRING paramPath;
    RTL_QUERY_REGISTRY_TABLE paramTable[2];

    __Enter
    {

        //
        // Setup the paramPath buffer.
        //

        paramPath.MaximumLength = PAGE_SIZE;
        paramPath.Buffer = (PWSTR)AFSExAllocatePoolWithTag( PagedPool,
                                                            paramPath.MaximumLength,
                                                            AFS_GENERIC_MEMORY_17_TAG);

        //
        // If it exists, setup the path.
        //

        if( paramPath.Buffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Move in the paths
        //

        RtlZeroMemory( paramPath.Buffer,
                       paramPath.MaximumLength);

        RtlCopyMemory( &paramPath.Buffer[ 0],
                       L"\\TransarcAFSDaemon\\Parameters",
                       58);

        paramPath.Length = 58;

        RtlZeroMemory( paramTable,
                       sizeof( paramTable));

        //
        // Setup the table to query the registry for the needed value
        //

        AFSServerName.Length = 0;
        AFSServerName.MaximumLength = 0;
        AFSServerName.Buffer = NULL;

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = AFS_REG_NETBIOS_NAME;
        paramTable[0].EntryContext = &AFSServerName;

        paramTable[0].DefaultType = REG_NONE;
        paramTable[0].DefaultData = NULL;
        paramTable[0].DefaultLength = 0;

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_SERVICES,
                                           paramPath.Buffer,
                                           paramTable,
                                           NULL,
                                           NULL);

        //
        // Free up the buffer
        //

        ExFreePool( paramPath.Buffer);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            RtlInitUnicodeString( &AFSServerName,
                                  L"AFS");
        }
    }

    return ntStatus;
}

NTSTATUS
AFSReadMountRootName()
{

    NTSTATUS ntStatus        = STATUS_SUCCESS;
    UNICODE_STRING paramPath;
    RTL_QUERY_REGISTRY_TABLE paramTable[2];

    __Enter
    {

        //
        // Setup the paramPath buffer.
        //

        paramPath.MaximumLength = PAGE_SIZE;
        paramPath.Buffer = (PWSTR)AFSExAllocatePoolWithTag( PagedPool,
                                                            paramPath.MaximumLength,
                                                            AFS_GENERIC_MEMORY_17_TAG);

        //
        // If it exists, setup the path.
        //

        if( paramPath.Buffer == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Move in the paths
        //

        RtlZeroMemory( paramPath.Buffer,
                       paramPath.MaximumLength);

        RtlCopyMemory( &paramPath.Buffer[ 0],
                       L"\\TransarcAFSDaemon\\Parameters",
                       58);

        paramPath.Length = 58;

        RtlZeroMemory( paramTable,
                       sizeof( paramTable));

        //
        // Setup the table to query the registry for the needed value
        //

        AFSMountRootName.Length = 0;
        AFSMountRootName.MaximumLength = 0;
        AFSMountRootName.Buffer = NULL;

        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = AFS_REG_MOUNT_ROOT;
        paramTable[0].EntryContext = &AFSMountRootName;

        paramTable[0].DefaultType = REG_NONE;
        paramTable[0].DefaultData = NULL;
        paramTable[0].DefaultLength = 0;

        //
        // Query the registry
        //

        ntStatus = RtlQueryRegistryValues( RTL_REGISTRY_SERVICES,
                                           paramPath.Buffer,
                                           paramTable,
                                           NULL,
                                           NULL);

        if ( NT_SUCCESS( ntStatus))
        {
            if ( AFSMountRootName.Buffer[0] == L'/')
            {

                AFSMountRootName.Buffer[0] = L'\\';
            }
        }

        //
        // Free up the buffer
        //

        ExFreePool( paramPath.Buffer);

try_exit:

        if( !NT_SUCCESS( ntStatus))
        {

            RtlInitUnicodeString( &AFSMountRootName,
                                  L"\\afs");
        }
    }

    return ntStatus;
}

NTSTATUS
AFSSetSysNameInformation( IN AFSSysNameNotificationCB *SysNameInfo,
                          IN ULONG SysNameInfoBufferLength)
{
    UNREFERENCED_PARAMETER(SysNameInfoBufferLength);

    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSDeviceExt    *pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSSysNameCB    *pSysName = NULL;
    ERESOURCE       *pSysNameLock = NULL;
    AFSSysNameCB   **pSysNameListHead = NULL, **pSysNameListTail = NULL;
    ULONG            ulIndex = 0;
    __Enter
    {

        //
        // Depending on the architecture of the information, set up the lsit
        //

        if( SysNameInfo->Architecture == AFS_SYSNAME_ARCH_32BIT)
        {

            pSysNameLock = &pControlDevExt->Specific.Control.SysName32ListLock;

            pSysNameListHead = &pControlDevExt->Specific.Control.SysName32ListHead;

            pSysNameListTail = &pControlDevExt->Specific.Control.SysName32ListTail;
        }
        else
        {

#if defined(_WIN64)

            pSysNameLock = &pControlDevExt->Specific.Control.SysName64ListLock;

            pSysNameListHead = &pControlDevExt->Specific.Control.SysName64ListHead;

            pSysNameListTail = &pControlDevExt->Specific.Control.SysName64ListTail;

#else

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
#endif
        }

        //
        // Process the request
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSSetSysNameInformation Acquiring SysName lock %p EXCL %08lX\n",
                      pSysNameLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( pSysNameLock,
                        TRUE);

        //
        // If we already have a list, then tear it down
        //

        if( *pSysNameListHead != NULL)
        {

            AFSResetSysNameList( *pSysNameListHead);

            *pSysNameListHead = NULL;
        }

        //
        // Loop through the entries adding in a node for each
        //

        while( ulIndex < SysNameInfo->NumberOfNames)
        {

            pSysName = (AFSSysNameCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                 sizeof( AFSSysNameCB) +
                                                                 SysNameInfo->SysNames[ ulIndex].Length +
                                                                 sizeof( WCHAR),
                                                                 AFS_SYS_NAME_NODE_TAG);

            if( pSysName == NULL)
            {

                //
                // Reset the current list
                //

                AFSResetSysNameList( *pSysNameListHead);

                *pSysNameListHead = NULL;

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlZeroMemory( pSysName,
                           sizeof( AFSSysNameCB) +
                                   SysNameInfo->SysNames[ ulIndex].Length +
                                   sizeof( WCHAR));

            pSysName->SysName.Length = (USHORT)SysNameInfo->SysNames[ ulIndex].Length;

            pSysName->SysName.MaximumLength = pSysName->SysName.Length + sizeof( WCHAR);

            pSysName->SysName.Buffer = (WCHAR *)((char *)pSysName + sizeof( AFSSysNameCB));

            RtlCopyMemory( pSysName->SysName.Buffer,
                           SysNameInfo->SysNames[ ulIndex].String,
                           pSysName->SysName.Length);

            if( *pSysNameListHead == NULL)
            {

                *pSysNameListHead = pSysName;
            }
            else
            {

                (*pSysNameListTail)->fLink = pSysName;
            }

            *pSysNameListTail = pSysName;

            ulIndex++;
        }

try_exit:

        AFSReleaseResource( pSysNameLock);
    }

    return ntStatus;
}

void
AFSResetSysNameList( IN AFSSysNameCB *SysNameList)
{

    AFSSysNameCB *pNextEntry = NULL, *pCurrentEntry = SysNameList;

    while( pCurrentEntry != NULL)
    {

        pNextEntry = pCurrentEntry->fLink;

        ExFreePool( pCurrentEntry);

        pCurrentEntry = pNextEntry;
    }

    return;
}

NTSTATUS
AFSDefaultDispatch( IN PDEVICE_OBJECT DeviceObject,
                    IN PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    AFSCompleteRequest( Irp,
                        ntStatus);

    return ntStatus;
}

NTSTATUS
AFSSendDeviceIoControl( IN DEVICE_OBJECT *TargetDeviceObject,
                        IN ULONG IOControl,
                        IN void *InputBuffer,
                        IN ULONG InputBufferLength,
                        IN OUT void *OutputBuffer,
                        IN ULONG OutputBufferLength,
                        OUT ULONG *ResultLength)
{
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    NTSTATUS            ntStatus = STATUS_SUCCESS;
    PIRP                pIrp = NULL;
    KEVENT              kEvent;
    PIO_STACK_LOCATION  pIoStackLocation = NULL;

    __Enter
    {

        //
        // Initialize the event
        //

        KeInitializeEvent( &kEvent,
                           SynchronizationEvent,
                           FALSE);

        //
        // Allocate an irp for this request.  This could also come from a
        // private pool, for instance.
        //

        pIrp = IoAllocateIrp( TargetDeviceObject->StackSize,
                              FALSE);

        if( pIrp == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        //
        // Build the IRP's main body
        //

        pIrp->RequestorMode = KernelMode;

        //
        // Set up the I/O stack location.
        //

        pIoStackLocation = IoGetNextIrpStackLocation( pIrp);
        pIoStackLocation->MajorFunction = IRP_MJ_DEVICE_CONTROL;
        pIoStackLocation->DeviceObject = TargetDeviceObject;

        pIoStackLocation->Parameters.DeviceIoControl.IoControlCode = IOControl;

        pIrp->AssociatedIrp.SystemBuffer = (void *)InputBuffer;
        pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength = InputBufferLength;

        //
        // Set the completion routine.
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "Setting AFSIrpComplete as IoCompletion Routine Irp %p\n",
                      pIrp);

        IoSetCompletionRoutine( pIrp,
                                AFSIrpComplete,
                                &kEvent,
                                TRUE,
                                TRUE,
                                TRUE);

        //
        // Send it to the FSD
        //

        ntStatus = IoCallDriver( TargetDeviceObject,
                                 pIrp);

        if( NT_SUCCESS( ntStatus))
        {

            //
            // Wait for the I/O
            //

            ntStatus = KeWaitForSingleObject( &kEvent,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              0);

            if( NT_SUCCESS( ntStatus))
            {

                ntStatus = pIrp->IoStatus.Status;

                if( ResultLength != NULL)
                {
                    *ResultLength = (ULONG)pIrp->IoStatus.Information;
                }
            }
        }

try_exit:

        if( pIrp != NULL)
        {

            if( pIrp->MdlAddress != NULL)
            {

                if( FlagOn( pIrp->MdlAddress->MdlFlags, MDL_PAGES_LOCKED))
                {

                    MmUnlockPages( pIrp->MdlAddress);
                }

                IoFreeMdl( pIrp->MdlAddress);
            }

            pIrp->MdlAddress = NULL;

            //
            // Free the Irp
            //

            IoFreeIrp( pIrp);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSIrpComplete( IN PDEVICE_OBJECT DeviceObject,
                IN PIRP           Irp,
                IN PVOID          Context)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);

    KEVENT *pEvent = (KEVENT *)Context;

    KeSetEvent( pEvent,
                0,
                FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

void *
AFSExAllocatePoolWithTag( IN POOL_TYPE  PoolType,
                          IN SIZE_T  NumberOfBytes,
                          IN ULONG  Tag)
{

    AFSDeviceExt *pControlDevExt = NULL;
    void *pBuffer = NULL;
    BOOLEAN bTimeout = FALSE;
    LARGE_INTEGER liTimeout;
    NTSTATUS ntStatus;

    //
    // Attempt to allocation memory from the system.  If the allocation fails
    // wait up to 30 seconds for the AFS redirector to free some memory.  As
    // long as the wait does not timeout, continue to retry the allocation.
    // If the wait does timeout, attempt to allocate one more time in case
    // memory was freed by another driver.  Otherwise, fail the request.
    //

    if ( AFSDeviceObject)
    {

        pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    }

    while( pBuffer == NULL)
    {

        pBuffer = ExAllocatePoolWithTag( PoolType,
                                         NumberOfBytes,
                                         Tag);

        if( pBuffer == NULL)
        {

            if ( bTimeout || pControlDevExt == NULL)
            {

                AFSDbgLogMsg( 0,
                              0,
                              "AFSExAllocatePoolWithTag failure Type %08lX Size %08lX Tag %08lX %08lX\n",
                              PoolType,
                              NumberOfBytes,
                              Tag,
                              PsGetCurrentThread());

                switch ( Tag ) {

                case AFS_GENERIC_MEMORY_21_TAG:
                case AFS_GENERIC_MEMORY_22_TAG:
                    // AFSDumpTraceFiles -- do nothing;
                    break;

                default:
                    AFSBreakPoint();
                }

                break;
            }


            //
            // Wait up to 30 seconds for a memory deallocation
            //

            liTimeout.QuadPart = -(30 *AFS_ONE_SECOND);

            if( InterlockedIncrement( &pControlDevExt->Specific.Control.WaitingForMemoryCount) == 1)
            {
                KeClearEvent( &pControlDevExt->Specific.Control.MemoryAvailableEvent);
            }

            ntStatus = KeWaitForSingleObject( &pControlDevExt->Specific.Control.MemoryAvailableEvent,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              &liTimeout);

            if( ntStatus == STATUS_TIMEOUT)
            {

                bTimeout = TRUE;
            }

            InterlockedDecrement( &pControlDevExt->Specific.Control.WaitingForMemoryCount);
        }
    }

    return pBuffer;
}

void
AFSExFreePoolWithTag( IN void *Buffer, IN ULONG Tag)
{

    AFSDeviceExt *pControlDevExt = NULL;

    if ( AFSDeviceObject)
    {

        pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    }

    if ( Tag)
    {

        ExFreePoolWithTag( Buffer, Tag);
    }
    else
    {

        ExFreePool( Buffer);
    }

    if ( pControlDevExt)
    {

        KeSetEvent( &pControlDevExt->Specific.Control.MemoryAvailableEvent,
                    0,
                    FALSE);
    }
    return;
}

NTSTATUS
AFSShutdownRedirector()
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;
    AFSDeviceExt *pControlDevExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    LARGE_INTEGER liTimeout;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Shutting down redirector Extent count %08lX Request count %08lX\n",
                      __FUNCTION__,
                      pControlDevExt->Specific.Control.ExtentCount,
                      pControlDevExt->Specific.Control.OutstandingServiceRequestCount);

        //
        // Set the shutdown flag so the worker is more agressive in tearing down extents
        //

        SetFlag( pRDRDevExt->DeviceFlags, AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN);

        //
        // Wait on any outstanding service requests
        //

        liTimeout.QuadPart = -(30 *AFS_ONE_SECOND);

        ntStatus = KeWaitForSingleObject( &pControlDevExt->Specific.Control.OutstandingServiceRequestEvent,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          &liTimeout);

        if( ntStatus == STATUS_TIMEOUT)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSShutdownRedirector Failed to complete all service requests Remaining count %08lX\n",
                          pControlDevExt->Specific.Control.OutstandingServiceRequestCount);

            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        AFSProcessQueuedResults( TRUE);

        //
        // Wait for all extents to be released
        //

        liTimeout.QuadPart = -(30 *AFS_ONE_SECOND);

        ntStatus = KeWaitForSingleObject( &pControlDevExt->Specific.Control.ExtentsHeldEvent,
                                          Executive,
                                          KernelMode,
                                          FALSE,
                                          &liTimeout);

        if( ntStatus == STATUS_TIMEOUT)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSShutdownRedirector Failed to purge all extents Remaining count %08lX\n",
                          pControlDevExt->Specific.Control.ExtentCount);

            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        ntStatus = AFSUnloadLibrary( TRUE);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

try_exit:

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Completed shut down of redirector Extent count %08lX Request count %08lX Status %08lX\n",
                      __FUNCTION__,
                      pControlDevExt->Specific.Control.ExtentCount,
                      pControlDevExt->Specific.Control.OutstandingServiceRequestCount,
                      ntStatus);
    }

    return ntStatus;
}

//
// Cache manager callback routines
//

BOOLEAN
AFSAcquireFcbForLazyWrite( IN PVOID Fcb,
                           IN BOOLEAN Wait)
{

    BOOLEAN bStatus = FALSE;
    AFSFcb *pFcb = (AFSFcb *)Fcb;
    BOOLEAN bReleaseSectionObject = FALSE, bReleasePaging = FALSE;

    //
    // Try and acquire the Fcb resource
    //

    AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSAcquireFcbForLazyWrite Acquiring Fcb %p\n",
                  Fcb);

    //
    // Try and grab the paging
    //

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSAcquireFcbForLazyWrite Attempt to acquire Fcb PagingIo lock %p SHARED %08lX\n",
                  &pFcb->NPFcb->PagingResource,
                  PsGetCurrentThread());

    if( AFSAcquireShared( &pFcb->NPFcb->PagingResource,
                          Wait))
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSAcquireFcbForLazyWrite Acquired Fcb PagingIo lock %p SHARED %08lX\n",
                      &pFcb->NPFcb->PagingResource,
                      PsGetCurrentThread());

        bReleasePaging = TRUE;

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSAcquireFcbForLazyWrite Attempt to acquire Fcb SectionObject lock %p SHARED %08lX\n",
                      &pFcb->NPFcb->SectionObjectResource,
                      PsGetCurrentThread());

        if( AFSAcquireShared( &pFcb->NPFcb->SectionObjectResource,
                              Wait))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSAcquireFcbForLazyWrite Acquired Fcb SectionObject lock %p SHARED %08lX\n",
                          &pFcb->NPFcb->SectionObjectResource,
                          PsGetCurrentThread());

            bReleaseSectionObject = TRUE;

            //
            // All is well ...
            //

            bStatus = TRUE;

            IoSetTopLevelIrp( (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
        }
    }

    if( !bStatus)
    {

        if( bReleaseSectionObject)
        {

            AFSReleaseResource( &pFcb->NPFcb->SectionObjectResource);
        }

        if( bReleasePaging)
        {

            AFSReleaseResource( &pFcb->NPFcb->PagingResource);
        }
    }

    return bStatus;
}

VOID
AFSReleaseFcbFromLazyWrite( IN PVOID Fcb)
{

    AFSFcb *pFcb = (AFSFcb *)Fcb;

    AFSDbgLogMsg( AFS_SUBSYSTEM_IO_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSReleaseFcbFromLazyWrite Releasing Fcb %p\n",
                  Fcb);

    IoSetTopLevelIrp( NULL);

    AFSReleaseResource( &pFcb->NPFcb->SectionObjectResource);

    AFSReleaseResource( &pFcb->NPFcb->PagingResource);

    return;
}

BOOLEAN
AFSAcquireFcbForReadAhead( IN PVOID Fcb,
                           IN BOOLEAN Wait)
{

    BOOLEAN bStatus = FALSE;
    AFSFcb *pFcb = (AFSFcb *)Fcb;

    AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                  AFS_TRACE_LEVEL_VERBOSE,
                  "AFSAcquireFcbForReadAhead Attempt to acquire Fcb SectionObject lock %p SHARED %08lX\n",
                  &pFcb->NPFcb->SectionObjectResource,
                  PsGetCurrentThread());

    if( AFSAcquireShared( &pFcb->NPFcb->SectionObjectResource,
                          Wait))
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSAcquireFcbForReadAhead Acquired Fcb SectionObject lock %p SHARED %08lX\n",
                      &pFcb->NPFcb->SectionObjectResource,
                      PsGetCurrentThread());

        bStatus = TRUE;

        IoSetTopLevelIrp( (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
    }

    return bStatus;
}

VOID
AFSReleaseFcbFromReadAhead( IN PVOID Fcb)
{

    AFSFcb *pFcb = (AFSFcb *)Fcb;

    IoSetTopLevelIrp( NULL);

    AFSReleaseResource( &pFcb->NPFcb->SectionObjectResource);

    return;
}

NTSTATUS
AFSGetCallerSID( OUT UNICODE_STRING *SIDString, OUT BOOLEAN *pbImpersonation)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PACCESS_TOKEN hToken = NULL;
    TOKEN_USER *pTokenInfo = NULL;
    BOOLEAN bCopyOnOpen = FALSE;
    BOOLEAN bEffectiveOnly = FALSE;
    BOOLEAN bPrimaryToken = FALSE;
    SECURITY_IMPERSONATION_LEVEL stImpersonationLevel;
    UNICODE_STRING uniSIDString;

    __Enter
    {

        hToken = PsReferenceImpersonationToken( PsGetCurrentThread(),
                                                &bCopyOnOpen,
                                                &bEffectiveOnly,
                                                &stImpersonationLevel);

        if( hToken == NULL)
        {

            hToken = PsReferencePrimaryToken( PsGetCurrentProcess());

            if( hToken == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSGetCallerSID Failed to retrieve impersonation or primary token\n");

                try_return( ntStatus);
            }

            bPrimaryToken = TRUE;
        }

        ntStatus = SeQueryInformationToken( hToken,
                                            TokenUser,
                                            (PVOID *)&pTokenInfo);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSGetCallerSID Failed to retrieve information Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        uniSIDString.Length = 0;
        uniSIDString.MaximumLength = 0;
        uniSIDString.Buffer = NULL;

        ntStatus = RtlConvertSidToUnicodeString( &uniSIDString,
                                                 pTokenInfo->User.Sid,
                                                 TRUE);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSGetCallerSID Failed to convert sid to string Status %08lX\n", ntStatus);

            try_return( ntStatus);
        }

        *SIDString = uniSIDString;

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING | AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSGetCallerSID Successfully retrieved SID %wZ\n",
                      SIDString);

        if ( bPrimaryToken == FALSE &&
             pbImpersonation)
        {
            *pbImpersonation = TRUE;
        }

try_exit:

        if( hToken != NULL)
        {
            if( bPrimaryToken)
            {
                PsDereferencePrimaryToken( hToken);
            }
            else
            {
                PsDereferenceImpersonationToken( hToken);
            }
        }

        if( pTokenInfo != NULL)
        {
            ExFreePool( pTokenInfo);    // Allocated by SeQueryInformationToken
        }
    }

    return ntStatus;
}

ULONG
AFSGetSessionId( IN HANDLE ProcessId, OUT BOOLEAN *pbImpersonation)
{
    UNREFERENCED_PARAMETER(ProcessId);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    PACCESS_TOKEN hToken = NULL;
    ULONG ulSessionId = (ULONG)-1;
    BOOLEAN bCopyOnOpen = FALSE;
    BOOLEAN bEffectiveOnly = FALSE;
    BOOLEAN bPrimaryToken = FALSE;
    SECURITY_IMPERSONATION_LEVEL stImpersonationLevel;

    __Enter
    {

        hToken = PsReferenceImpersonationToken( PsGetCurrentThread(),
                                                &bCopyOnOpen,
                                                &bEffectiveOnly,
                                                &stImpersonationLevel);

        if( hToken == NULL)
        {

            hToken = PsReferencePrimaryToken( PsGetCurrentProcess());

            if( hToken == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "AFSGetSessionId Failed to retrieve impersonation or primary token\n");

                try_return( ntStatus);
            }

            bPrimaryToken = TRUE;
        }

        ntStatus = SeQueryInformationToken( hToken,
                                            TokenSessionId,
                                            (PVOID *)&ulSessionId);

        if( !NT_SUCCESS( ntStatus))
        {
            ulSessionId = (ULONG)-1;

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSGetSessionId Failed to retrieve session id Status %08lX\n",
                          ntStatus);

            try_return( ntStatus);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING | AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "AFSGetSessionId found %08lX\n",
                      ulSessionId);

        if ( bPrimaryToken == FALSE &&
             pbImpersonation)
        {
            *pbImpersonation = TRUE;
        }

try_exit:

        if( hToken != NULL)
        {
            if( bPrimaryToken)
            {
                PsDereferencePrimaryToken( hToken);
            }
            else
            {
                PsDereferenceImpersonationToken( hToken);
            }
        }
    }

    return ulSessionId;
}

NTSTATUS
AFSCheckThreadDacl( OUT GUID *AuthGroup)
{

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    ULONG idx;
    PACCESS_TOKEN token = NULL;
    PTOKEN_DEFAULT_DACL defDacl = NULL;
    PACE_HEADER ace;
    PACCESS_ALLOWED_ACE adace;
    BOOLEAN bCopyOnOpen = FALSE, bEffectiveOnly = FALSE;
    SECURITY_IMPERSONATION_LEVEL stImpersonationLevel;
    BOOLEAN bLocatedACE = FALSE;

    __Enter
    {

        token = PsReferenceImpersonationToken( PsGetCurrentThread(),
                                               &bCopyOnOpen,
                                               &bEffectiveOnly,
                                               &stImpersonationLevel);

        if( token == NULL)
        {
           try_return( ntStatus);
        }

        ntStatus = SeQueryInformationToken( token,
                                            TokenDefaultDacl,
                                            (PVOID *)&defDacl);

        if( ntStatus != STATUS_SUCCESS)
        {
           try_return( ntStatus);
        }

        // scan through all ACEs in the DACL
        for (idx = 0, ace = (PACE_HEADER)((char *)defDacl->DefaultDacl + sizeof(ACL)); idx < defDacl->DefaultDacl->AceCount; idx++)
        {
           if (ace->AceType == ACCESS_ALLOWED_ACE_TYPE)
           {
              adace = (PACCESS_ALLOWED_ACE)ace;

              if (adace->Header.AceSize == (FIELD_OFFSET(ACCESS_ALLOWED_ACE, SidStart) + AFS_DACL_SID_LENGTH))
              {
                 if (RtlCompareMemory( RtlSubAuthoritySid((PSID)&adace->SidStart, 0), &AFSSidGuid, sizeof(GUID)) == sizeof(GUID))
                 {

                    RtlCopyMemory( AuthGroup,
                                   RtlSubAuthoritySid((PSID)&adace->SidStart, 4),
                                   sizeof( GUID));

                    bLocatedACE = TRUE;

                    break;
                 }
              }
           }

           // go to next ace
           ace = (PACE_HEADER)((char *)ace + ace->AceSize);
        }

try_exit:

        if( token != NULL)
        {
            PsDereferenceImpersonationToken( token);
        }

        if (defDacl != NULL)
        {
           ExFreePool(defDacl);
        }

        if( !bLocatedACE)
        {
            ntStatus = STATUS_UNSUCCESSFUL;
        }
    }

    return ntStatus;
}

NTSTATUS
AFSProcessSetProcessDacl( IN AFSProcessCB *ProcessCB)
{

    PTOKEN_DEFAULT_DACL defDacl = NULL;
    HANDLE hToken = NULL;
    PACE_HEADER ace = NULL;
    SID_IDENTIFIER_AUTHORITY sia = SECURITY_NT_AUTHORITY;
    PACCESS_ALLOWED_ACE aaace;
    ULONG bytesNeeded;
    ULONG bytesReturned;
    ULONG idx;
    PSID psid;
    NTSTATUS ntStatus = STATUS_SUCCESS;

    __Enter
    {

        ntStatus = ZwOpenProcessTokenEx( NtCurrentProcess(),
                                         GENERIC_ALL,
                                         OBJ_KERNEL_HANDLE,
                                         &hToken);

        if( !NT_SUCCESS( ntStatus))
        {
            try_return( ntStatus);
        }

        // get the size of the current DACL
        ntStatus = ZwQueryInformationToken( hToken,
                                            TokenDefaultDacl,
                                            NULL,
                                            0,
                                            &bytesNeeded);

        // if we failed to get the buffer size needed
        if ((ntStatus != STATUS_SUCCESS) && (ntStatus != STATUS_BUFFER_TOO_SMALL))
        {
            try_return( ntStatus);
        }

        // tack on enough space for our ACE if we need to add it...
        bytesNeeded += FIELD_OFFSET(ACCESS_ALLOWED_ACE, SidStart) + AFS_DACL_SID_LENGTH;

        // allocate space for the DACL
        defDacl = (PTOKEN_DEFAULT_DACL)ExAllocatePoolWithTag( PagedPool, bytesNeeded, AFS_GENERIC_MEMORY_26_TAG);

        if (defDacl == NULL)
        {
           try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        // get the DACL
        ntStatus = ZwQueryInformationToken( hToken,
                                            TokenDefaultDacl,
                                            defDacl,
                                            bytesNeeded,
                                            &bytesReturned);

        if( ntStatus != STATUS_SUCCESS)
        {
            try_return( ntStatus);
        }

        // scan through DACL to see if we have the SID set already...
        ace = (PACE_HEADER)((char *)defDacl->DefaultDacl + sizeof(ACL));
        for (idx = 0; idx < defDacl->DefaultDacl->AceCount; idx++)
        {
            if (ace->AceType == ACCESS_ALLOWED_ACE_TYPE)
            {
                aaace = (PACCESS_ALLOWED_ACE)ace;

                if (aaace->Header.AceSize == (FIELD_OFFSET(ACCESS_ALLOWED_ACE, SidStart) + AFS_DACL_SID_LENGTH))
                {
                    // if the GUID part matches
                    if( RtlCompareMemory( RtlSubAuthoritySid((PSID)&aaace->SidStart, 0),
                                          &AFSSidGuid,
                                          sizeof(GUID)) == sizeof(GUID))
                    {

                        if ( RtlCompareMemory( RtlSubAuthoritySid((PSID)&aaace->SidStart, 4),
                                               ProcessCB->ActiveAuthGroup,
                                               sizeof( GUID)) != sizeof( GUID))
                        {

                            RtlCopyMemory( RtlSubAuthoritySid((PSID)&aaace->SidStart, 4),
                                           ProcessCB->ActiveAuthGroup,
                                           sizeof( GUID));

                            if( AFSSetInformationToken != NULL)
                            {
                                ntStatus = AFSSetInformationToken( hToken,
                                                                   TokenDefaultDacl,
                                                                   defDacl,
                                                                   bytesReturned);
                            }
                        }

                        try_return( ntStatus);
                    }
                }
            }

            // go to next ace
            ace = (PACE_HEADER)((char *)ace + ace->AceSize);
        }

        //
        // if we made it here we need to add a new ACE to the DACL
        //

        aaace = (ACCESS_ALLOWED_ACE *)ace;
        aaace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
        aaace->Header.AceFlags = 0;
        aaace->Mask = GENERIC_ALL;
        psid = (PSID)&aaace->SidStart;
        RtlInitializeSid( psid, &sia, 8);

        RtlCopyMemory( RtlSubAuthoritySid(psid, 0),
                       &AFSSidGuid,
                       sizeof(GUID));

        RtlCopyMemory( RtlSubAuthoritySid(psid, 4),
                       ProcessCB->ActiveAuthGroup,
                       sizeof( GUID));

        aaace->Header.AceSize = (USHORT)(FIELD_OFFSET(ACCESS_ALLOWED_ACE, SidStart) + RtlLengthSid( psid));

        defDacl->DefaultDacl->AclSize += aaace->Header.AceSize;
        defDacl->DefaultDacl->AceCount++;

        if( AFSSetInformationToken != NULL)
        {
            ntStatus = AFSSetInformationToken( hToken,
                                              TokenDefaultDacl,
                                              defDacl,
                                              defDacl->DefaultDacl->AclSize + sizeof(PTOKEN_DEFAULT_DACL));
        }

try_exit:

        if( hToken != NULL)
        {
            ZwClose( hToken);
        }

        if (defDacl != NULL)
        {
           ExFreePool( defDacl);
        }
    }

    return ntStatus;
}


