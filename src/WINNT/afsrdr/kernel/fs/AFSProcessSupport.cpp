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
// File: AFSProcessSupport.cpp
//

#include "AFSCommon.h"

void
AFSProcessNotify( IN HANDLE  ParentId,
                  IN HANDLE  ProcessId,
                  IN BOOLEAN  Create)
{

    //
    // If this is a create notification then update our tree, otherwise remove the
    // entry
    //

    if( Create)
    {

        AFSProcessCreate( ParentId,
                          ProcessId,
                          PsGetCurrentProcessId(),
                          PsGetCurrentThreadId());
    }
    else
    {

        AFSProcessDestroy( ProcessId);
    }

    return;
}

void
AFSProcessNotifyEx( IN OUT PEPROCESS Process,
                    IN     HANDLE ProcessId,
                    IN OUT PPS_CREATE_NOTIFY_INFO CreateInfo)
{

    if ( CreateInfo)
    {

        AFSProcessCreate( CreateInfo->ParentProcessId,
                          ProcessId,
                          CreateInfo->CreatingThreadId.UniqueProcess,
                          CreateInfo->CreatingThreadId.UniqueThread);
    }
    else
    {

        AFSProcessDestroy( ProcessId);
    }
}


void
AFSProcessCreate( IN HANDLE ParentId,
                  IN HANDLE ProcessId,
                  IN HANDLE CreatingProcessId,
                  IN HANDLE CreatingThreadId)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSProcessCB *pProcessCB = NULL;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessCreate Acquiring Control ProcessTree.TreeLock lock %08lX EXCL %08lX\n",
                      pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                      PsGetCurrentThread());

        AFSAcquireExcl( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                        TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_PROCESS_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessCreate Parent %08lX Process %08lX %08lX\n",
                      ParentId,
                      ProcessId,
                      PsGetCurrentThread());

        pProcessCB = AFSInitializeProcessCB( (ULONGLONG)ParentId,
                                             (ULONGLONG)ProcessId);

        if( pProcessCB != NULL)
        {

            pProcessCB->CreatingProcessId = (ULONGLONG)CreatingProcessId;

            pProcessCB->CreatingThreadId = (ULONGLONG)CreatingThreadId;

            //
            // Now assign the AuthGroup ACE
            //

            AFSValidateProcessEntry( ProcessId);
        }
        else
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_PROCESS_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSProcessCreate Initialization failure for Parent %08lX Process %08lX %08lX\n",
                          ParentId,
                          ProcessId,
                          PsGetCurrentThread());
        }

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
    }

    return;
}

void
AFSProcessDestroy( IN HANDLE ProcessId)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    AFSProcessCB *pProcessCB = NULL, *pParentProcessCB = NULL;
    AFSProcessAuthGroupCB *pProcessAuthGroup = NULL, *pLastAuthGroup = NULL;
    AFSThreadCB *pThreadCB = NULL, *pNextThreadCB = NULL;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessDestroy Acquiring Control ProcessTree.TreeLock lock %08lX EXCL %08lX\n",
                      pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                      PsGetCurrentThreadId());

        AFSAcquireExcl( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                        TRUE);
        //
        // It's a remove so pull the entry
        //

        AFSDbgLogMsg( AFS_SUBSYSTEM_PROCESS_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSProcessDestroy Process %08lX %08lX\n",
                      ProcessId,
                      PsGetCurrentThread());

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       (ULONGLONG)ProcessId,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( NT_SUCCESS( ntStatus) &&
            pProcessCB != NULL)
        {

            AFSRemoveHashEntry( &pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                (AFSBTreeEntry *)pProcessCB);

            pProcessAuthGroup = pProcessCB->AuthGroupList;

            while( pProcessAuthGroup != NULL)
            {

                pLastAuthGroup = pProcessAuthGroup->Next;

                ExFreePool( pProcessAuthGroup);

                pProcessAuthGroup = pLastAuthGroup;
            }

            pThreadCB = pProcessCB->ThreadList;

            while( pThreadCB != NULL)
            {

                pNextThreadCB = pThreadCB->Next;

                ExFreePool( pThreadCB);

                pThreadCB = pNextThreadCB;
            }

            ExDeleteResourceLite( &pProcessCB->Lock);

            ExFreePool( pProcessCB);
        }
        else
        {
            AFSDbgLogMsg( AFS_SUBSYSTEM_PROCESS_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSProcessDestroy Process %08lX not found in ProcessTree Status %08lX %08lX\n",
                          ProcessId,
                          ntStatus,
                          PsGetCurrentThread());
        }

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
    }

    return;
}

//
// AFSValidateProcessEntry verifies the consistency of the current process
// entry which includes assigning an authentication group ACE if one is not
// present.  A reference to the active authentication group GUID is returned.
//

GUID *
AFSValidateProcessEntry( IN HANDLE ProcessId)
{

    GUID *pAuthGroup = NULL;
    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProcessCB *pProcessCB = NULL, *pParentProcessCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    ULONGLONG ullProcessID = (ULONGLONG)ProcessId;
    UNICODE_STRING uniSIDString;
    ULONG ulSIDHash = 0;
    AFSSIDEntryCB *pSIDEntryCB = NULL;
    ULONG ulSessionId = 0;
    ULONGLONG ullTableHash = 0;
    AFSThreadCB *pParentThreadCB = NULL;
    UNICODE_STRING uniGUID;
    BOOLEAN bImpersonation = FALSE;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSValidateProcessEntry Acquiring Control ProcessTree.TreeLock lock %08lX SHARED %08lX\n",
                      pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                      PsGetCurrentThread());

        uniSIDString.Length = 0;
        uniSIDString.MaximumLength = 0;
        uniSIDString.Buffer = NULL;

        AFSAcquireShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          TRUE);

        AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Entry for ProcessID %I64X\n",
                      __FUNCTION__,
                      ullProcessID);

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       ullProcessID,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( !NT_SUCCESS( ntStatus) ||
            pProcessCB == NULL)
        {

            AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

            AFSAcquireExcl( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                            TRUE);

            ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                           ullProcessID,
                                           (AFSBTreeEntry **)&pProcessCB);

            if( !NT_SUCCESS( ntStatus) ||
                pProcessCB == NULL)
            {

                AFSProcessCreate( 0,
                                  ProcessId,
                                  0,
                                  0);
            }

            if( !NT_SUCCESS( ntStatus) ||
                pProcessCB == NULL)
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "%s Failed to locate process entry for ProcessID %I64X\n",
                              __FUNCTION__,
                              ullProcessID);

                AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

                try_return( ntStatus = STATUS_UNSUCCESSFUL);
            }

            AFSConvertToShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
        }

        //
        // Locate and lock the ParentProcessCB if we have one
        //

        if( pProcessCB->ParentProcessId != 0)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Locating process entry for Parent ProcessID %I64X\n",
                          __FUNCTION__,
                          pProcessCB->ParentProcessId);

            ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                           (ULONGLONG)pProcessCB->ParentProcessId,
                                           (AFSBTreeEntry **)&pParentProcessCB);

            if( NT_SUCCESS( ntStatus) &&
                pParentProcessCB != NULL)
            {
                AFSAcquireExcl( &pParentProcessCB->Lock,
                                TRUE);

                AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Located process entry for Parent ProcessID %I64X\n",
                              __FUNCTION__,
                              pProcessCB->ParentProcessId);
            }
        }
        else
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s No parent ID for ProcessID %I64X\n",
                          __FUNCTION__,
                          ullProcessID);
        }

        AFSAcquireExcl( &pProcessCB->Lock,
                        TRUE);

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

        //
        // Locate the SID for the caller
        //

        ntStatus = AFSGetCallerSID( &uniSIDString, &bImpersonation);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to locate callers SID for ProcessID %I64X\n",
                          __FUNCTION__,
                          ullProcessID);

            try_return( ntStatus);
        }

        ulSessionId = AFSGetSessionId( (HANDLE)ullProcessID, &bImpersonation);

        if( ulSessionId == (ULONG)-1)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to retrieve session ID for ProcessID %I64X\n",
                          __FUNCTION__,
                          ullProcessID);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Retrieved callers SID %wZ for ProcessID %I64X Session %08lX\n",
                      __FUNCTION__,
                      &uniSIDString,
                      ullProcessID,
                      ulSessionId);

        //
        // If there is an Auth Group for the current process,
        // our job is finished.
        //

        if ( bImpersonation == FALSE)
        {
            pAuthGroup = pProcessCB->ActiveAuthGroup;

            if( pAuthGroup != NULL &&
                !AFSIsNoPAGAuthGroup( pAuthGroup))
            {

                uniGUID.Buffer = NULL;

                RtlStringFromGUID( *pAuthGroup,
                                   &uniGUID);

                AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Located valid AuthGroup GUID %wZ for SID %wZ ProcessID %I64X Session %08lX\n",
                              __FUNCTION__,
                              &uniGUID,
                              &uniSIDString,
                              ullProcessID,
                              ulSessionId);

                if( uniGUID.Buffer != NULL)
                {
                    RtlFreeUnicodeString( &uniGUID);
                }

                try_return( ntStatus = STATUS_SUCCESS);
            }

            //
            // The current process does not yet have an Auth Group.  Try to inherit
            // one from the parent process thread that created this process.
            //

            if( pParentProcessCB != NULL)
            {

                for ( pParentThreadCB = pParentProcessCB->ThreadList;
                      pParentThreadCB != NULL;
                      pParentThreadCB = pParentThreadCB->Next)
                {

                    if( pParentThreadCB->ThreadId == pProcessCB->CreatingThreadId)
                    {
                        break;
                    }
                }

                //
                // If the creating thread was found and it has a thread specific
                // Auth Group, use that even if it is the No PAG
                //

                if( pParentThreadCB != NULL &&
                    pParentThreadCB->ActiveAuthGroup != NULL &&
                    !AFSIsNoPAGAuthGroup( pParentThreadCB->ActiveAuthGroup))
                {
                    pProcessCB->ActiveAuthGroup = pParentThreadCB->ActiveAuthGroup;

                    uniGUID.Buffer = NULL;

                    RtlStringFromGUID( *(pProcessCB->ActiveAuthGroup),
                                       &uniGUID);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "%s PID %I64X Session %08lX inherited Active AuthGroup %wZ from thread %I64X\n",
                                  __FUNCTION__,
                                  ullProcessID,
                                  ulSessionId,
                                  &uniGUID,
                                  pParentThreadCB->ThreadId);

                    if( uniGUID.Buffer != NULL)
                    {
                        RtlFreeUnicodeString( &uniGUID);
                    }
                }

                //
                // If the parent thread was not found or does not have an auth group
                //

                else if( pParentProcessCB->ActiveAuthGroup != NULL &&
                         !AFSIsNoPAGAuthGroup( pParentProcessCB->ActiveAuthGroup))
                {
                    pProcessCB->ActiveAuthGroup = pParentProcessCB->ActiveAuthGroup;

                    uniGUID.Buffer = NULL;

                    RtlStringFromGUID( *(pProcessCB->ActiveAuthGroup),
                                       &uniGUID);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "%s PID %I64X Session %08lX inherited Active AuthGroup %wZ from parent PID %I64X\n",
                                  __FUNCTION__,
                                  ullProcessID,
                                  ulSessionId,
                                  &uniGUID,
                                  pParentProcessCB->TreeEntry.HashIndex);

                    if( uniGUID.Buffer != NULL)
                    {
                        RtlFreeUnicodeString( &uniGUID);
                    }
                }

                //
                // If an Auth Group was inherited, set it to be the active group
                //

                if( pProcessCB->ActiveAuthGroup != NULL &&
                    !AFSIsNoPAGAuthGroup( pParentProcessCB->ActiveAuthGroup))
                {
                    pAuthGroup = pProcessCB->ActiveAuthGroup;

                    uniGUID.Buffer = NULL;

                    RtlStringFromGUID( *(pProcessCB->ActiveAuthGroup),
                                       &uniGUID);

                    AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "%s Returning(1) Active AuthGroup %wZ for SID %wZ PID %I64X Session %08lX\n",
                                  __FUNCTION__,
                                  &uniGUID,
                                  &uniSIDString,
                                  ullProcessID,
                                  ulSessionId);

                    if( uniGUID.Buffer != NULL)
                    {
                        RtlFreeUnicodeString( &uniGUID);
                    }

                    try_return( ntStatus);
                }
            }
        }

        //
        // If no Auth Group was inherited, assign one based upon the Session and SID
        //

        ntStatus = RtlHashUnicodeString( &uniSIDString,
                                         TRUE,
                                         HASH_STRING_ALGORITHM_DEFAULT,
                                         &ulSIDHash);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to hash SID %wZ for PID %I64X Session %08lX Status %08lX\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ullProcessID,
                          ulSessionId,
                          ntStatus);

            try_return( ntStatus);
        }

        ullTableHash = ( ((ULONGLONG)ulSessionId << 32) | ulSIDHash);

        AFSAcquireShared( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock,
                          TRUE);

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.AuthGroupTree.TreeHead,
                                       (ULONGLONG)ullTableHash,
                                       (AFSBTreeEntry **)&pSIDEntryCB);

        if( !NT_SUCCESS( ntStatus) ||
            pSIDEntryCB == NULL)
        {

            AFSReleaseResource( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock);

            AFSAcquireExcl( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock,
                            TRUE);

            ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.AuthGroupTree.TreeHead,
                                           (ULONGLONG)ullTableHash,
                                           (AFSBTreeEntry **)&pSIDEntryCB);

            if( !NT_SUCCESS( ntStatus) ||
                pSIDEntryCB == NULL)
            {

                pSIDEntryCB = (AFSSIDEntryCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                                         sizeof( AFSSIDEntryCB),
                                                                         AFS_AG_ENTRY_CB_TAG);

                if( pSIDEntryCB == NULL)
                {

                    AFSReleaseResource( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock);

                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                RtlZeroMemory( pSIDEntryCB,
                               sizeof( AFSSIDEntryCB));

                pSIDEntryCB->TreeEntry.HashIndex = (ULONGLONG)ullTableHash;

                while( ExUuidCreate( &pSIDEntryCB->AuthGroup) == STATUS_RETRY);

                uniGUID.Buffer = NULL;

                RtlStringFromGUID( pSIDEntryCB->AuthGroup,
                                   &uniGUID);

                AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s  SID %wZ PID %I64X Session %08lX generated NEW AG %wZ\n",
                              __FUNCTION__,
                              &uniSIDString,
                              ullProcessID,
                              ulSessionId,
                              &uniGUID);

                if( uniGUID.Buffer != NULL)
                {
                    RtlFreeUnicodeString( &uniGUID);
                }

                if( pDeviceExt->Specific.Control.AuthGroupTree.TreeHead == NULL)
                {
                    pDeviceExt->Specific.Control.AuthGroupTree.TreeHead = (AFSBTreeEntry *)pSIDEntryCB;
                }
                else
                {
                    AFSInsertHashEntry( pDeviceExt->Specific.Control.AuthGroupTree.TreeHead,
                                        &pSIDEntryCB->TreeEntry);
                }
            }

            AFSConvertToShared( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock);
        }


        AFSReleaseResource( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock);

        //
        // Store the auth group into the process cb
        //

        pProcessCB->ActiveAuthGroup = &pSIDEntryCB->AuthGroup;

        uniGUID.Buffer = NULL;

        RtlStringFromGUID( pSIDEntryCB->AuthGroup,
                           &uniGUID);

        AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s SID %wZ PID %I64X Session %08lX assigned AG %wZ\n",
                      __FUNCTION__,
                      &uniSIDString,
                      ullProcessID,
                      ulSessionId,
                      &uniGUID);

        if( uniGUID.Buffer != NULL)
        {
            RtlFreeUnicodeString( &uniGUID);
        }

        //
        // Set the AFS_PROCESS_LOCAL_SYSTEM_AUTH flag if the process SID
        // is LOCAL_SYSTEM
        //

        if( AFSIsLocalSystemSID( &uniSIDString))
        {
            SetFlag( pProcessCB->Flags, AFS_PROCESS_LOCAL_SYSTEM_AUTH);

            AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Setting PID %I64X Session %08lX with LOCAL SYSTEM AUTHORITY\n",
                          __FUNCTION__,
                          ullProcessID,
                          ulSessionId);
        }

        //
        // Return the auth group
        //

        pAuthGroup = pProcessCB->ActiveAuthGroup;

        uniGUID.Buffer = NULL;

        RtlStringFromGUID( *(pProcessCB->ActiveAuthGroup),
                           &uniGUID);

        AFSDbgLogMsg( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Returning(2) Active AuthGroup %wZ for SID %wZ PID %I64X Session %08lX\n",
                      __FUNCTION__,
                      &uniGUID,
                      &uniSIDString,
                      ullProcessID,
                      ulSessionId);

        if( uniGUID.Buffer != NULL)
        {
            RtlFreeUnicodeString( &uniGUID);
        }

try_exit:

        if( pProcessCB != NULL)
        {

            if( bImpersonation == FALSE &&
                !BooleanFlagOn( pProcessCB->Flags, AFS_PROCESS_FLAG_ACE_SET) &&
                NT_SUCCESS( ntStatus))
            {
                ntStatus = AFSProcessSetProcessDacl( pProcessCB);

                if( !NT_SUCCESS( ntStatus))
                {
                    pAuthGroup = NULL;
                }
                else
                {
                    SetFlag( pProcessCB->Flags, AFS_PROCESS_FLAG_ACE_SET);
                }
            }

            AFSReleaseResource( &pProcessCB->Lock);
        }

        if( pParentProcessCB != NULL)
        {
            AFSReleaseResource( &pParentProcessCB->Lock);
        }

        if( uniSIDString.Length > 0)
        {
            RtlFreeUnicodeString( &uniSIDString);
        }
    }

    return pAuthGroup;
}

BOOLEAN
AFSIs64BitProcess( IN ULONGLONG ProcessId)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    BOOLEAN bIs64Bit = FALSE;
    AFSProcessCB *pProcessCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        AFSDbgLogMsg( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSIs64BitProcess Acquiring Control ProcessTree.TreeLock lock %08lX SHARED %08lX\n",
                      pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                      PsGetCurrentThread());

        AFSAcquireShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          TRUE);

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       (ULONGLONG)ProcessId,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( pProcessCB != NULL)
        {
            bIs64Bit = BooleanFlagOn( pProcessCB->Flags, AFS_PROCESS_FLAG_IS_64BIT);
        }

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
    }

    return bIs64Bit;
}

AFSProcessCB *
AFSInitializeProcessCB( IN ULONGLONG ParentProcessId,
                        IN ULONGLONG ProcessId)
{

    AFSProcessCB *pProcessCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;

    __Enter
    {

        pProcessCB = (AFSProcessCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                               sizeof( AFSProcessCB),
                                                               AFS_PROCESS_CB_TAG);

        if( pProcessCB == NULL)
        {
            try_return( pProcessCB);
        }

        RtlZeroMemory( pProcessCB,
                       sizeof( AFSProcessCB));

        pProcessCB->TreeEntry.HashIndex = (ULONGLONG)ProcessId;

        pProcessCB->ParentProcessId = (ULONGLONG)ParentProcessId;

#if defined(_WIN64)

        if( !IoIs32bitProcess( NULL))
        {
            SetFlag( pProcessCB->Flags, AFS_PROCESS_FLAG_IS_64BIT);
        }

#endif

        if( pDeviceExt->Specific.Control.ProcessTree.TreeHead == NULL)
        {
            pDeviceExt->Specific.Control.ProcessTree.TreeHead = (AFSBTreeEntry *)pProcessCB;
        }
        else
        {
            AFSInsertHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                &pProcessCB->TreeEntry);
        }

        ExInitializeResourceLite( &pProcessCB->Lock);

        pProcessCB->ActiveAuthGroup = &AFSNoPAGAuthGroup;

try_exit:

        NOTHING;
    }

    return pProcessCB;
}

AFSThreadCB *
AFSInitializeThreadCB( IN AFSProcessCB *ProcessCB,
                       IN ULONGLONG ThreadId)
{

    AFSThreadCB *pThreadCB = NULL, *pCurrentThreadCB = NULL;

    __Enter
    {

        pThreadCB = (AFSThreadCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                             sizeof( AFSThreadCB),
                                                             AFS_PROCESS_CB_TAG);

        if( pThreadCB == NULL)
        {
            try_return( pThreadCB);
        }

        RtlZeroMemory( pThreadCB,
                       sizeof( AFSThreadCB));

        pThreadCB->ThreadId = ThreadId;

        if( ProcessCB->ThreadList == NULL)
        {
            ProcessCB->ThreadList = pThreadCB;
        }
        else
        {

            pCurrentThreadCB = ProcessCB->ThreadList;

            while( pCurrentThreadCB != NULL)
            {

                if( pCurrentThreadCB->Next == NULL)
                {
                    pCurrentThreadCB->Next = pThreadCB;
                    break;
                }

                pCurrentThreadCB = pCurrentThreadCB->Next;
            }
        }

try_exit:

        NOTHING;
    }

    return pThreadCB;
}
