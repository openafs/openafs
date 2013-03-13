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
// File: AFSAuthGroupSupport.cpp
//

#include "AFSCommon.h"

void
AFSRetrieveAuthGroup( IN ULONGLONG ProcessId,
                      IN ULONGLONG ThreadId,
                      OUT GUID *AuthGroup)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProcessCB *pProcessCB = NULL;
    AFSThreadCB *pThreadCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    GUID *pAuthGroup = NULL;
    UNICODE_STRING uniGUIDString;
    ULONG ulSessionId = 0;
    BOOLEAN bImpersonation = FALSE;

    __Enter
    {

        ulSessionId = AFSGetSessionId( (HANDLE)ProcessId, &bImpersonation);

        if( ulSessionId == (ULONG)-1)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to retrieve session ID for PID %I64X\n",
                          __FUNCTION__,
                          ProcessId));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Entry for Session %08lX PID %I64X TID %I64X\n",
                      __FUNCTION__,
                      ulSessionId,
                      ProcessId,
                      ThreadId));

        ntStatus = AFSCheckThreadDacl( AuthGroup);

        if( NT_SUCCESS( ntStatus))
        {

            uniGUIDString.Buffer = NULL;
            uniGUIDString.Length = 0;
            uniGUIDString.MaximumLength = 0;

            RtlStringFromGUID( *AuthGroup,
                               &uniGUIDString);

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Located AuthGroup %wZ via DACL for Session %08lX PID %I64X TID %I64X\n",
                          __FUNCTION__,
                          &uniGUIDString,
                          ulSessionId,
                          ProcessId,
                          ThreadId));

            if( uniGUIDString.Buffer != NULL)
            {
                RtlFreeUnicodeString( &uniGUIDString);
            }

            try_return( ntStatus = STATUS_SUCCESS);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSRetrieveAuthGroup Acquiring Control ProcessTree.TreeLock lock %p SHARED %08lX\n",
                      pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                      PsGetCurrentThread()));

        ntStatus = STATUS_SUCCESS;

        AFSAcquireShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          TRUE);

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       (ULONGLONG)ProcessId,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( !NT_SUCCESS( ntStatus) ||
            pProcessCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to locate process entry for Session %08lX PID %I64X TID %I64X\n",
                          __FUNCTION__,
                          ulSessionId,
                          ProcessId,
                          ThreadId));

            AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

            try_return( ntStatus);
        }

        for ( pThreadCB = pProcessCB->ThreadList;
              pThreadCB != NULL;
              pThreadCB = pThreadCB->Next)
        {

            if( pThreadCB->ThreadId == ThreadId)
            {
                break;
            }
        }

        if( pThreadCB != NULL &&
            pThreadCB->ActiveAuthGroup != NULL)
        {
            pAuthGroup = pThreadCB->ActiveAuthGroup;

            RtlCopyMemory( AuthGroup,
                           pAuthGroup,
                           sizeof( GUID));

            uniGUIDString.Buffer = NULL;
            uniGUIDString.Length = 0;
            uniGUIDString.MaximumLength = 0;

            RtlStringFromGUID( *AuthGroup,
                               &uniGUIDString);

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Located AuthGroup %wZ in thread Session %08lX PID %I64X TID %I64X\n",
                          __FUNCTION__,
                          &uniGUIDString,
                          ulSessionId,
                          ProcessId,
                          ThreadId));

            if( uniGUIDString.Buffer != NULL)
            {
                RtlFreeUnicodeString( &uniGUIDString);
            }
        }
        else if( pProcessCB->ActiveAuthGroup != NULL)
        {

            pAuthGroup = pProcessCB->ActiveAuthGroup;

            RtlCopyMemory( AuthGroup,
                           pAuthGroup,
                           sizeof( GUID));

            uniGUIDString.Buffer = NULL;
            uniGUIDString.Length = 0;
            uniGUIDString.MaximumLength = 0;

            RtlStringFromGUID( *AuthGroup,
                               &uniGUIDString);

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Located AuthGroup %wZ in process Session %08lX PID %I64X TID %I64X\n",
                          __FUNCTION__,
                          &uniGUIDString,
                          ulSessionId,
                          ProcessId,
                          ThreadId));

            if( uniGUIDString.Buffer != NULL)
            {
                RtlFreeUnicodeString( &uniGUIDString);
            }
        }

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

        if( pAuthGroup == NULL ||
            AFSIsNoPAGAuthGroup( pAuthGroup))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s No AuthGroup located, validating process for Session %08lX PID %I64X TID %I64X\n",
                          __FUNCTION__,
                          ulSessionId,
                          ProcessId,
                          ThreadId));

            pAuthGroup = AFSValidateProcessEntry( PsGetCurrentProcessId(),
                                                  FALSE);

            if( pAuthGroup != NULL)
            {
                RtlCopyMemory( AuthGroup,
                               pAuthGroup,
                               sizeof( GUID));

                uniGUIDString.Buffer = NULL;
                uniGUIDString.Length = 0;
                uniGUIDString.MaximumLength = 0;

                RtlStringFromGUID( *AuthGroup,
                                   &uniGUIDString);

                AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Located AuthGroup %wZ after validation Session %08lX PID %I64X TID %I64X\n",
                              __FUNCTION__,
                              &uniGUIDString,
                              ulSessionId,
                              ProcessId,
                              ThreadId));

                if( uniGUIDString.Buffer != NULL)
                {
                    RtlFreeUnicodeString( &uniGUIDString);
                }
            }
            else
            {
                AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "%s Failed to locate AuthGroup for Session %08lX PID %I64X TID %I64X\n",
                              __FUNCTION__,
                              ulSessionId,
                              ProcessId,
                              ThreadId));
            }
        }

try_exit:

        NOTHING;
    }

    return;
}

//
// AFSIsLocalSystemAuthGroup returns TRUE if the AuthGroup matches
// the AuthGroup associated with the first process that communicates
// with the redirector which will always be "System" (PID 4).
//

BOOLEAN
AFSIsLocalSystemAuthGroup( IN GUID *AuthGroup)
{

    BOOLEAN bIsLocalSys = FALSE;
    AFSProcessCB *pProcessCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    UNICODE_STRING uniGUIDString;

    __Enter
    {

        uniGUIDString.Length = 0;
        uniGUIDString.MaximumLength = 0;
        uniGUIDString.Buffer = NULL;

        RtlStringFromGUID( *AuthGroup,
                           &uniGUIDString);

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "%s Checking AuthGroup %wZ\n",
                      __FUNCTION__,
                      &uniGUIDString));

        AFSAcquireShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          TRUE);

        pProcessCB = (AFSProcessCB *)pDeviceExt->Specific.Control.ProcessTree.TreeHead;

        if( pProcessCB->ActiveAuthGroup != NULL &&
            RtlCompareMemory( pProcessCB->ActiveAuthGroup,
                              AuthGroup,
                              sizeof( GUID)) == sizeof( GUID))
        {
            bIsLocalSys = TRUE;

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s AuthGroup %wZ is LOCAL SYSTEM\n",
                          __FUNCTION__,
                          &uniGUIDString));
        }

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

        if( uniGUIDString.Buffer != NULL)
        {
            RtlFreeUnicodeString( &uniGUIDString);
        }
    }

    return bIsLocalSys;
}

BOOLEAN
AFSIsLocalSystemSID( IN UNICODE_STRING *SIDString)
{

    BOOLEAN bIsLocalSys = FALSE;
    UNICODE_STRING uniSysLocal;

    __Enter
    {

        RtlInitUnicodeString( &uniSysLocal,
                              L"S-1-5-18");

        if( RtlCompareUnicodeString( &uniSysLocal,
                                     SIDString,
                                     TRUE) == 0)
        {
            bIsLocalSys = TRUE;
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "%s AuthGroup SID %wZ is %sLOCAL SYSTEM\n",
                      __FUNCTION__,
                      SIDString,
                      bIsLocalSys ? "" : "not "));
    }

    return bIsLocalSys;
}

BOOLEAN
AFSIsNoPAGAuthGroup( IN GUID *AuthGroup)
{

    BOOLEAN bIsNoPAG = FALSE;
    UNICODE_STRING uniGUIDString;

    __Enter
    {

        uniGUIDString.Length = 0;
        uniGUIDString.MaximumLength = 0;
        uniGUIDString.Buffer = NULL;

        RtlStringFromGUID( *AuthGroup,
                           &uniGUIDString);

        if( RtlCompareMemory( AuthGroup,
                              &AFSNoPAGAuthGroup,
                              sizeof( GUID)) == sizeof( GUID))
        {
            bIsNoPAG = TRUE;
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE_2,
                      "%s AuthGroup %wZ is %sNoPAG\n",
                      __FUNCTION__,
                      &uniGUIDString,
                      bIsNoPAG ? "" : "not "));

        if( uniGUIDString.Buffer != NULL)
        {
            RtlFreeUnicodeString( &uniGUIDString);
        }
    }

    return bIsNoPAG;
}

//
// Creates a new AuthGroup and either activates it for
// the process or the current thread.  If set as the
// new process AuthGroup, the prior AuthGroup list is
// cleared.
//

NTSTATUS
AFSCreateSetProcessAuthGroup( AFSAuthGroupRequestCB *CreateSetAuthGroup)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProcessCB *pProcessCB = NULL;
    AFSThreadCB *pThreadCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    ULONGLONG ullProcessID = (ULONGLONG)PsGetCurrentProcessId();
    ULONGLONG ullThreadId = (ULONGLONG)PsGetCurrentThreadId();
    UNICODE_STRING uniSIDString, uniPassedSIDString;
    ULONG ulSIDHash = 0;
    AFSProcessAuthGroupCB *pAuthGroup = NULL, *pLastAuthGroup = NULL;
    ULONG ulSessionId = 0;
    ULONGLONG ullTableHash = 0;
    UNICODE_STRING uniCallerSID;
    BOOLEAN bImpersonation = FALSE;

    __Enter
    {

        uniCallerSID.Length = 0;
        uniCallerSID.MaximumLength = 0;
        uniCallerSID.Buffer = NULL;

        AFSAcquireShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          TRUE);

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       (ULONGLONG)ullProcessID,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( !NT_SUCCESS( ntStatus) ||
            pProcessCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to locate process CB for PID %I64X\n",
                          __FUNCTION__,
                          ullProcessID));

            AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        AFSAcquireExcl( &pProcessCB->Lock,
                        TRUE);

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

        ntStatus = AFSGetCallerSID( &uniCallerSID, &bImpersonation);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to locate caller SID for PID %I64X Status %08lX\n",
                          __FUNCTION__,
                          ullProcessID,
                          ntStatus));

            try_return( ntStatus);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Retrieved caller SID %wZ for PID %I64X\n",
                      __FUNCTION__,
                      &uniCallerSID,
                      ullProcessID));


        if( CreateSetAuthGroup->SIDLength != 0)
        {

            uniPassedSIDString.Length = CreateSetAuthGroup->SIDLength;
            uniPassedSIDString.MaximumLength = uniPassedSIDString.Length;

            uniPassedSIDString.Buffer = CreateSetAuthGroup->SIDString;

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Validating passed SID %wZ for PID %I64X\n",
                          __FUNCTION__,
                          &uniPassedSIDString,
                          ullProcessID));

            if( RtlCompareUnicodeString( &uniCallerSID,
                                         &uniPassedSIDString,
                                         TRUE) != 0)
            {

                if( !BooleanFlagOn( pProcessCB->Flags, AFS_PROCESS_LOCAL_SYSTEM_AUTH))
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "%s Caller specified SID %wZ for PID %I64X but caller is not LOCAL SYSTEM AUTHORITY\n",
                                  __FUNCTION__,
                                  &uniPassedSIDString,
                                  ullProcessID));

                    try_return( ntStatus = STATUS_ACCESS_DENIED);
                }

                uniSIDString = uniPassedSIDString;

                AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Using passed SID %wZ for PID %I64X\n",
                              __FUNCTION__,
                              &uniSIDString,
                              ullProcessID));
            }
            else
            {
                uniSIDString = uniCallerSID;

                AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Caller and passed SID are equal SID %wZ for PID %I64X\n",
                              __FUNCTION__,
                              &uniSIDString,
                              ullProcessID));
            }
        }
        else
        {
            uniSIDString = uniCallerSID;

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s No SID passed, using callers SID %wZ for PID %I64X\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ullProcessID));
        }

        ntStatus = RtlHashUnicodeString( &uniSIDString,
                                         TRUE,
                                         HASH_STRING_ALGORITHM_DEFAULT,
                                         &ulSIDHash);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to hash SID %wZ for PID %I64X Status %08lX\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ullProcessID,
                          ntStatus));

            try_return( ntStatus);
        }

        ulSessionId = AFSGetSessionId( (HANDLE)ullProcessID, &bImpersonation);

        if( ulSessionId == (ULONG)-1)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to retrieve SessionID PID %I64X Status %08lX\n",
                          __FUNCTION__,
                          ullProcessID,
                          ntStatus));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        if( CreateSetAuthGroup->SessionId != (ULONG)-1)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Checking passed SessionID %08lX for PID %I64X\n",
                          __FUNCTION__,
                          CreateSetAuthGroup->SessionId,
                          ullProcessID));

            if( ulSessionId != CreateSetAuthGroup->SessionId)
            {

                if( !BooleanFlagOn( pProcessCB->Flags, AFS_PROCESS_LOCAL_SYSTEM_AUTH))
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "%s Passed SessionID %08lX for PID %I64X, failed because caller is not LOCAL SYSTEM AUTHORITY\n",
                                  __FUNCTION__,
                                  CreateSetAuthGroup->SessionId,
                                  ullProcessID));

                    try_return( ntStatus = STATUS_ACCESS_DENIED);
                }

                ulSessionId = CreateSetAuthGroup->SessionId;

                AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Using passed SessionID %08lX for PID %I64X\n",
                              __FUNCTION__,
                              ulSessionId,
                              ullProcessID));
            }
        }
        else
        {
            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Using callers SessionID %08lX for PID %I64X\n",
                          __FUNCTION__,
                          ulSessionId,
                          ullProcessID));
        }

        ullTableHash = ( ((ULONGLONG)ulSessionId << 32) | ulSIDHash);

        pAuthGroup = pProcessCB->AuthGroupList;

        while( pAuthGroup != NULL)
        {

            if( pAuthGroup->AuthGroupHash == ullTableHash)
            {
                break;
            }

            pLastAuthGroup = pAuthGroup;

            pAuthGroup = pAuthGroup->Next;
        }

        if( pAuthGroup != NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Located AuthGroup for SID %wZ SessionID %08lX for PID %I64X, failing request\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ulSessionId,
                          ullProcessID));

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        pAuthGroup = (AFSProcessAuthGroupCB *)AFSExAllocatePoolWithTag( NonPagedPool,
                                                                        sizeof( AFSProcessAuthGroupCB),
                                                                        AFS_AG_ENTRY_CB_TAG);

        if( pAuthGroup == NULL)
        {
            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pAuthGroup,
                       sizeof( AFSProcessAuthGroupCB));

        pAuthGroup->AuthGroupHash = (ULONGLONG)ullTableHash;

        while( ExUuidCreate( &pAuthGroup->AuthGroup) == STATUS_RETRY);

        if( pLastAuthGroup == NULL)
        {
            pProcessCB->AuthGroupList = pAuthGroup;
        }
        else
        {
            pLastAuthGroup->Next = pAuthGroup;
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Allocated new AuthGroup for SID %wZ SessionID %08lX for PID %I64X\n",
                      __FUNCTION__,
                      &uniSIDString,
                      ulSessionId,
                      ullProcessID));

        if( BooleanFlagOn( CreateSetAuthGroup->Flags, AFS_PAG_FLAGS_THREAD_AUTH_GROUP))
        {

            pThreadCB = pProcessCB->ThreadList;

            while( pThreadCB != NULL)
            {

                if( pThreadCB->ThreadId == ullThreadId)
                {
                    pThreadCB->ActiveAuthGroup = &pAuthGroup->AuthGroup;
                    break;
                }

                pThreadCB = pThreadCB->Next;
            }

            if( pThreadCB == NULL)
            {

                pThreadCB = AFSInitializeThreadCB( pProcessCB,
                                                   ullThreadId);

                if( pThreadCB == NULL)
                {
                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                pThreadCB->ActiveAuthGroup = &pAuthGroup->AuthGroup;
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Set new AuthGroup for SID %wZ SessionID %08lX for PID %I64X on thread ID %I64X\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ulSessionId,
                          ullProcessID,
                          ullThreadId));
        }
        else if( BooleanFlagOn( CreateSetAuthGroup->Flags, AFS_PAG_FLAGS_SET_AS_ACTIVE))
        {
            pProcessCB->ActiveAuthGroup = &pAuthGroup->AuthGroup;

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Set new AuthGroup for SID %wZ SessionID %08lX for PID %I64X on process\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ulSessionId,
                          ullProcessID));
        }

try_exit:

        if( pProcessCB != NULL)
        {
            AFSReleaseResource( &pProcessCB->Lock);
        }

        if( uniCallerSID.Length > 0)
        {
            RtlFreeUnicodeString( &uniCallerSID);
        }
    }

    return ntStatus;
}

//
// Returns a list of the AuthGroup GUIDS associated
// with the current process, the current process GUID,
// and the current thread GUID.
//

NTSTATUS
AFSQueryProcessAuthGroupList( IN GUID *GUIDList,
                              IN ULONG BufferLength,
                              OUT ULONG_PTR *ReturnLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProcessCB *pProcessCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    ULONGLONG ullProcessID = (ULONGLONG)PsGetCurrentProcessId();
    ULONG ulRequiredLength = 0;
    AFSProcessAuthGroupCB *pAuthGroup = NULL;
    GUID *pCurrentGUID = GUIDList;
    UNICODE_STRING uniGUIDString;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Entry for PID %I64X\n",
                      __FUNCTION__,
                      ullProcessID));

        AFSAcquireShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          TRUE);

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       (ULONGLONG)ullProcessID,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( !NT_SUCCESS( ntStatus) ||
            pProcessCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to locate process entry PID %I64X\n",
                          __FUNCTION__,
                          ullProcessID));

            AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        AFSAcquireShared( &pProcessCB->Lock,
                          TRUE);

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

        pAuthGroup = pProcessCB->AuthGroupList;

        ulRequiredLength = 0;

        while( pAuthGroup != NULL)
        {
            ulRequiredLength += sizeof( GUID);
            pAuthGroup = pAuthGroup->Next;
        }

        if( BufferLength == 0 ||
            BufferLength < ulRequiredLength ||
            GUIDList == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Buffer too small for query, required %08lX for PID %I64X\n",
                          __FUNCTION__,
                          ulRequiredLength,
                          ullProcessID));

            *ReturnLength = ulRequiredLength;
            try_return( ntStatus = STATUS_BUFFER_OVERFLOW);
        }

        pAuthGroup = pProcessCB->AuthGroupList;

        *ReturnLength = 0;

        while( pAuthGroup != NULL)
        {
            RtlCopyMemory( pCurrentGUID,
                           &pAuthGroup->AuthGroup,
                           sizeof( GUID));

            uniGUIDString.Buffer = NULL;
            uniGUIDString.Length = 0;
            uniGUIDString.MaximumLength = 0;

            RtlStringFromGUID( pAuthGroup->AuthGroup,
                               &uniGUIDString);

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Adding AuthGroup %wZ for PID %I64X\n",
                          __FUNCTION__,
                          &uniGUIDString,
                          ullProcessID));

            if( uniGUIDString.Buffer != NULL)
            {
                RtlFreeUnicodeString( &uniGUIDString);
            }

            pCurrentGUID = (GUID *)((char *)pCurrentGUID + sizeof( GUID));

            *ReturnLength += sizeof( GUID);

            pAuthGroup = pAuthGroup->Next;
        }

try_exit:

        if( pProcessCB != NULL)
        {
            AFSReleaseResource( &pProcessCB->Lock);
        }
    }

    return ntStatus;
}

//
// Permits the current AuthGroup for the process or
// thread to be set to the specified GUID.  The GUID
// must be in the list of current values for the process.
//

NTSTATUS
AFSSetActiveProcessAuthGroup( IN AFSAuthGroupRequestCB *ActiveAuthGroup)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProcessCB *pProcessCB = NULL;
    AFSThreadCB *pThreadCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    ULONGLONG ullProcessID = (ULONGLONG)PsGetCurrentProcessId();
    ULONGLONG ullThreadId = (ULONGLONG)PsGetCurrentThreadId();
    AFSProcessAuthGroupCB *pAuthGroup = NULL;
    UNICODE_STRING uniGUIDString;

    __Enter
    {

        uniGUIDString.Length = 0;
        uniGUIDString.MaximumLength = 0;
        uniGUIDString.Buffer = NULL;

        RtlStringFromGUID( ActiveAuthGroup->AuthGroup,
                           &uniGUIDString);

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Entry for ProcessID %I64X AuthGroup GUID %wZ\n",
                      __FUNCTION__,
                      ullProcessID,
                      &uniGUIDString));

        AFSAcquireShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          TRUE);

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       (ULONGLONG)ullProcessID,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( !NT_SUCCESS( ntStatus) ||
            pProcessCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to locate process entry for ProcessID %I64X\n",
                          __FUNCTION__,
                          ullProcessID));

            AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }


        AFSAcquireExcl( &pProcessCB->Lock,
                        TRUE);

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

        pAuthGroup = pProcessCB->AuthGroupList;

        while( pAuthGroup != NULL)
        {

            if( RtlCompareMemory( &ActiveAuthGroup->AuthGroup,
                                  &pAuthGroup->AuthGroup,
                                  sizeof( GUID)) == sizeof( GUID))
            {
                break;
            }
            pAuthGroup = pAuthGroup->Next;
        }

        if( pAuthGroup == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Failed to locate AuthGroup for ProcessID %I64X AuthGroup GUID %wZ\n",
                          __FUNCTION__,
                          ullProcessID,
                          &uniGUIDString));

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        if( BooleanFlagOn( ActiveAuthGroup->Flags, AFS_PAG_FLAGS_THREAD_AUTH_GROUP))
        {

            pThreadCB = pProcessCB->ThreadList;

            while( pThreadCB != NULL)
            {

                if( pThreadCB->ThreadId == ullThreadId)
                {
                    pThreadCB->ActiveAuthGroup = &pAuthGroup->AuthGroup;
                    break;
                }

                pThreadCB = pThreadCB->Next;
            }

            if( pThreadCB == NULL)
            {

                pThreadCB = AFSInitializeThreadCB( pProcessCB,
                                                   ullThreadId);

                if( pThreadCB == NULL)
                {
                    try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
                }

                pThreadCB->ActiveAuthGroup = &pAuthGroup->AuthGroup;
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Set active AuthGroup for ProcessID %I64X AuthGroup GUID %wZ on thread %I64X\n",
                          __FUNCTION__,
                          ullProcessID,
                          &uniGUIDString,
                          ullThreadId));
        }
        else
        {
            pProcessCB->ActiveAuthGroup = &pAuthGroup->AuthGroup;

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Set active AuthGroup for ProcessID %I64X AuthGroup GUID %wZ on process\n",
                          __FUNCTION__,
                          ullProcessID,
                          &uniGUIDString));
        }

try_exit:

        if( pProcessCB != NULL)
        {
            AFSReleaseResource( &pProcessCB->Lock);
        }

        if( uniGUIDString.Buffer != NULL)
        {
            RtlFreeUnicodeString( &uniGUIDString);
        }
    }

    return ntStatus;
}

//
// Resets the current AuthGroup for the process or
// thread to the SID-AuthGroup
//

NTSTATUS
AFSResetActiveProcessAuthGroup( IN IN AFSAuthGroupRequestCB *AuthGroup)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProcessCB *pProcessCB = NULL;
    AFSThreadCB *pThreadCB = NULL;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    ULONGLONG ullProcessID = (ULONGLONG)PsGetCurrentProcessId();
    ULONGLONG ullThreadId = (ULONGLONG)PsGetCurrentThreadId();

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Entry for ProcessID %I64X\n",
                      __FUNCTION__,
                      ullProcessID));

        AFSAcquireShared( pDeviceExt->Specific.Control.ProcessTree.TreeLock,
                          TRUE);

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.ProcessTree.TreeHead,
                                       (ULONGLONG)ullProcessID,
                                       (AFSBTreeEntry **)&pProcessCB);

        if( !NT_SUCCESS( ntStatus) ||
            pProcessCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to locate AuthGroup for ProcessID %I64X\n",
                          __FUNCTION__,
                          ullProcessID));

            AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);
            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        AFSAcquireExcl( &pProcessCB->Lock,
                        TRUE);

        AFSReleaseResource( pDeviceExt->Specific.Control.ProcessTree.TreeLock);

        if( BooleanFlagOn( AuthGroup->Flags, AFS_PAG_FLAGS_THREAD_AUTH_GROUP))
        {

            pThreadCB = pProcessCB->ThreadList;

            while( pThreadCB != NULL)
            {

                if( pThreadCB->ThreadId == ullThreadId)
                {
                    pThreadCB->ActiveAuthGroup = NULL;
                    break;
                }

                pThreadCB = pThreadCB->Next;
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Reset AuthGroup list on thread %I64X for ProcessID %I64X\n",
                          __FUNCTION__,
                          ullThreadId,
                          ullProcessID));
        }
        else
        {
            pProcessCB->ActiveAuthGroup = NULL;

            pThreadCB = pProcessCB->ThreadList;

            while( pThreadCB != NULL)
            {
                pThreadCB->ActiveAuthGroup = NULL;
                pThreadCB = pThreadCB->Next;
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Reset AuthGroup list on process for ProcessID %I64X\n",
                          __FUNCTION__,
                          ullProcessID));
        }

        AFSReleaseResource( &pProcessCB->Lock);

try_exit:

        NOTHING;
    }

    return ntStatus;
}

//
// When bLogonSession == FALSE, the SID must not be specified
// and the SessionId must be -1.  A new AuthGroup GUID is
// assigned to the SID and SessionId of the calling Process.
//
// When bLogonSession == TRUE, the SID must be specified and
// the SessionId must not be -1.  The SID of the calling process
// must be LOCAL_SYSTEM and a new AuthGroup GUID is assigned to
// the specified SID and logon session.
//

NTSTATUS
AFSCreateAuthGroupForSIDorLogonSession( IN AFSAuthGroupRequestCB *AuthGroupRequestCB,
                                        IN BOOLEAN bLogonSession)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    ULONGLONG ullProcessID = (ULONGLONG)PsGetCurrentProcessId();
    ULONGLONG ullThreadId = (ULONGLONG)PsGetCurrentThreadId();
    UNICODE_STRING uniSIDString, uniPassedSIDString;
    ULONG ulSIDHash = 0;
    AFSSIDEntryCB *pSIDEntryCB = NULL;
    ULONG ulSessionId = 0;
    ULONGLONG ullTableHash = 0;
    UNICODE_STRING uniCallerSID;
    UNICODE_STRING uniGUID;
    BOOLEAN bLocalSystem = FALSE;
    BOOLEAN bImpersonation = FALSE;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Entry for ProcessID %I64X ThreadID %I64X\n",
                      __FUNCTION__,
                      ullProcessID,
                      ullThreadId));

        ntStatus = AFSGetCallerSID( &uniCallerSID, &bImpersonation);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to retrieve callers SID for ProcessID %I64X ThreadID %I64X Status %08lX\n",
                          __FUNCTION__,
                          ullProcessID,
                          ullThreadId,
                          ntStatus));

            try_return( ntStatus);
        }

        bLocalSystem = AFSIsLocalSystemSID( &uniCallerSID);

        if( bLogonSession == TRUE &&
            bLocalSystem == FALSE)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s caller is %wZ and LOCAL SYSTEM AUTHORITY required\n",
                          __FUNCTION__,
                          uniCallerSID));

            try_return( ntStatus = STATUS_ACCESS_DENIED);
        }

        if ( bLogonSession == TRUE &&
             ( AuthGroupRequestCB == NULL ||
               AuthGroupRequestCB->SIDLength == 0 ||
               AuthGroupRequestCB->SessionId == (ULONG)-1))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s SID and SessionId are mandatory\n",
                          __FUNCTION__));

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        if ( bLogonSession == FALSE &&
             AuthGroupRequestCB != NULL &&
             ( AuthGroupRequestCB->SIDLength > 0 ||
               AuthGroupRequestCB->SessionId != (ULONG)-1))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s SID and SessionId must not be specified\n",
                          __FUNCTION__));

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }


        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Retrieved callers SID %wZ for ProcessID %I64X ThreadID %I64X\n",
                      __FUNCTION__,
                      &uniCallerSID,
                      ullProcessID,
                      ullThreadId));

        if( AuthGroupRequestCB != NULL &&
            AuthGroupRequestCB->SIDLength != 0)
        {

            uniPassedSIDString.Length = AuthGroupRequestCB->SIDLength;
            uniPassedSIDString.MaximumLength = uniPassedSIDString.Length;

            uniPassedSIDString.Buffer = AuthGroupRequestCB->SIDString;

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Checking passed SID %wZ for ProcessID %I64X ThreadID %I64X\n",
                          __FUNCTION__,
                          &uniPassedSIDString,
                          ullProcessID,
                          ullThreadId));

            if( RtlCompareUnicodeString( &uniCallerSID,
                                         &uniPassedSIDString,
                                         TRUE) != 0)
            {

                if( !bLocalSystem)
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                                  AFS_TRACE_LEVEL_ERROR,
                                  "%s Not using passed SID %wZ for ProcessID %I64X ThreadID %I64X caller is not LOCAL SYSTEM AUTHORITY\n",
                                  __FUNCTION__,
                                  &uniPassedSIDString,
                                  ullProcessID,
                                  ullThreadId));

                    try_return( ntStatus = STATUS_ACCESS_DENIED);
                }

                uniSIDString = uniPassedSIDString;

                AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Using passed SID %wZ for ProcessID %I64X ThreadID %I64X\n",
                              __FUNCTION__,
                              &uniSIDString,
                              ullProcessID,
                              ullThreadId));
            }
            else
            {
                uniSIDString = uniCallerSID;

                AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Both SIDs are equal, using callers SID %wZ for ProcessID %I64X ThreadID %I64X\n",
                              __FUNCTION__,
                              &uniSIDString,
                              ullProcessID,
                              ullThreadId));
            }
        }
        else
        {
            uniSIDString = uniCallerSID;

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Using callers SID %wZ for ProcessID %I64X ThreadID %I64X\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ullProcessID,
                          ullThreadId));
        }

        ntStatus = RtlHashUnicodeString( &uniSIDString,
                                         TRUE,
                                         HASH_STRING_ALGORITHM_DEFAULT,
                                         &ulSIDHash);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to hash SID %wZ for ProcessID %I64X ThreadID %I64X Status %08lX\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ullProcessID,
                          ullThreadId,
                          ntStatus));

            try_return( ntStatus);
        }

        ulSessionId = AFSGetSessionId( (HANDLE)ullProcessID, &bImpersonation);

        if( ulSessionId == (ULONG)-1)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to retrieve session ID for ProcessID %I64X ThreadID %I64X\n",
                          __FUNCTION__,
                          ullProcessID,
                          ullThreadId));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        if( bLogonSession == TRUE &&
            AuthGroupRequestCB != NULL &&
            AuthGroupRequestCB->SessionId != (ULONG)-1)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Checking passed SessionID %08lX for ProcessID %I64X ThreadID %I64X\n",
                          __FUNCTION__,
                          AuthGroupRequestCB->SessionId,
                          ullProcessID,
                          ullThreadId));

            if( ulSessionId != AuthGroupRequestCB->SessionId)
            {

                ulSessionId = AuthGroupRequestCB->SessionId;

                AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "%s Using passed SessionID %08lX for ProcessID %I64X ThreadID %I64X\n",
                              __FUNCTION__,
                              AuthGroupRequestCB->SessionId,
                              ullProcessID,
                              ullThreadId));
            }
        }
        else
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Using callers SessionID %08lX for ProcessID %I64X ThreadID %I64X\n",
                          __FUNCTION__,
                          ulSessionId,
                          ullProcessID,
                          ullThreadId));
        }

        ullTableHash = ( ((ULONGLONG)ulSessionId << 32) | ulSIDHash);

        AFSAcquireExcl( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock,
                        TRUE);

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.AuthGroupTree.TreeHead,
                                       (ULONGLONG)ullTableHash,
                                       (AFSBTreeEntry **)&pSIDEntryCB);

        if( NT_SUCCESS( ntStatus) &&
            pSIDEntryCB != NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Located SID entry for SID %wZ SessionID %08lX ProcessID %I64X ThreadID %I64X, updating GUID\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ulSessionId,
                          ullProcessID,
                          ullThreadId));

            uniGUID.Buffer = NULL;

            RtlStringFromGUID( pSIDEntryCB->AuthGroup,
                               &uniGUID);

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Updating existing AuthGroup GUID %wZ\n",
                          __FUNCTION__,
                          &uniGUID));

            if( uniGUID.Buffer != NULL)
            {
                RtlFreeUnicodeString( &uniGUID);
            }

            while( ExUuidCreate( &pSIDEntryCB->AuthGroup) == STATUS_RETRY);

            uniGUID.Buffer = NULL;

            RtlStringFromGUID( pSIDEntryCB->AuthGroup,
                               &uniGUID);

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Updated existing AuthGroup GUID %wZ\n",
                          __FUNCTION__,
                          &uniGUID));

            if( uniGUID.Buffer != NULL)
            {
                RtlFreeUnicodeString( &uniGUID);
            }

            AFSReleaseResource( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock);
            try_return( ntStatus);
        }

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

        if( pDeviceExt->Specific.Control.AuthGroupTree.TreeHead == NULL)
        {
            pDeviceExt->Specific.Control.AuthGroupTree.TreeHead = (AFSBTreeEntry *)pSIDEntryCB;
        }
        else
        {
            AFSInsertHashEntry( pDeviceExt->Specific.Control.AuthGroupTree.TreeHead,
                                &pSIDEntryCB->TreeEntry);
        }

        AFSReleaseResource( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock);

        uniGUID.Buffer = NULL;

        RtlStringFromGUID( pSIDEntryCB->AuthGroup,
                           &uniGUID);

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Created new AuthGroup GUID %wZ SID %wZ Session %08lX\n",
                      __FUNCTION__,
                      &uniGUID,
                      &uniSIDString,
                      ulSessionId));

        if( uniGUID.Buffer != NULL)
        {
            RtlFreeUnicodeString( &uniGUID);
        }

try_exit:

        if( uniCallerSID.Length > 0)
        {
            RtlFreeUnicodeString( &uniCallerSID);
        }
    }

    return ntStatus;
}

//
// Given a SID and SessionId as input, returns the associated AuthGroup GUID.
// If SID or SessionId are not specified, the current process values are used.
//

NTSTATUS
AFSQueryAuthGroup( IN AFSAuthGroupRequestCB *AuthGroup,
                   OUT GUID *AuthGroupGUID,
                   OUT ULONG_PTR *ReturnLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDeviceExt *pDeviceExt = (AFSDeviceExt *)AFSDeviceObject->DeviceExtension;
    ULONGLONG ullProcessID = (ULONGLONG)PsGetCurrentProcessId();
    UNICODE_STRING uniSIDString;
    ULONG ulSIDHash = 0;
    AFSSIDEntryCB *pSIDEntryCB = NULL;
    ULONG ulSessionId = 0;
    ULONGLONG ullTableHash = 0;
    BOOLEAN bReleaseSID = FALSE;
    UNICODE_STRING uniGUID;
    BOOLEAN bImpersonation = FALSE;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Entry for ProcessID %I64X\n",
                      __FUNCTION__,
                      ullProcessID));

        if( AuthGroup == NULL ||
            AuthGroup->SIDLength == 0)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s No SID specified, retrieving callers SID for ProcessID %I64X\n",
                          __FUNCTION__,
                          ullProcessID));

            ntStatus = AFSGetCallerSID( &uniSIDString, &bImpersonation);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "%s Failed to retrieve callers SID for ProcessID %I64X Status %08lX\n",
                              __FUNCTION__,
                              ullProcessID,
                              ntStatus));

                try_return( ntStatus);
            }

            bReleaseSID = TRUE;

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Retrieved callers SID %wZ for ProcessID %I64X\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ullProcessID));
        }
        else
        {

            uniSIDString.Length = AuthGroup->SIDLength;
            uniSIDString.MaximumLength = uniSIDString.Length;

            uniSIDString.Buffer = AuthGroup->SIDString;

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Using passed SID %wZ for ProcessID %I64X\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ullProcessID));
        }

        ntStatus = RtlHashUnicodeString( &uniSIDString,
                                         TRUE,
                                         HASH_STRING_ALGORITHM_DEFAULT,
                                         &ulSIDHash);

        if( !NT_SUCCESS( ntStatus))
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to hash SID %wZ for ProcessID %I64X Status %08lX\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ullProcessID,
                          ntStatus));

            try_return( ntStatus);
        }

        if( AuthGroup == NULL ||
            AuthGroup->SessionId == -1)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s No SessionID specified, retrieving callers for ProcessID %I64X\n",
                          __FUNCTION__,
                          ullProcessID));

            ulSessionId = AFSGetSessionId( (HANDLE)ullProcessID, &bImpersonation);

            if( ulSessionId == (ULONG)-1)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                              AFS_TRACE_LEVEL_ERROR,
                              "%s Failed to retrieve callers Session ID for ProcessID %I64X\n",
                              __FUNCTION__,
                              ullProcessID));

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Retrieved callers SessionID %08lX for ProcessID %I64X\n",
                          __FUNCTION__,
                          ulSessionId,
                          ullProcessID));
        }
        else
        {
            ulSessionId = AuthGroup->SessionId;

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "%s Using passed SessionID %08lX for ProcessID %I64X\n",
                          __FUNCTION__,
                          ulSessionId,
                          ullProcessID));
        }

        ullTableHash = ( ((ULONGLONG)ulSessionId << 32) | ulSIDHash);

        AFSAcquireShared( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock,
                          TRUE);

        ntStatus = AFSLocateHashEntry( pDeviceExt->Specific.Control.AuthGroupTree.TreeHead,
                                       (ULONGLONG)ullTableHash,
                                       (AFSBTreeEntry **)&pSIDEntryCB);

        if( pSIDEntryCB == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                          AFS_TRACE_LEVEL_ERROR,
                          "%s Failed to locate SID entry for SID %wZ SessionID %08lX ProcessID %I64X\n",
                          __FUNCTION__,
                          &uniSIDString,
                          ulSessionId,
                          ullProcessID));

            AFSReleaseResource( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock);
            try_return( ntStatus = STATUS_NOT_FOUND);
        }

        RtlCopyMemory( AuthGroupGUID,
                       &pSIDEntryCB->AuthGroup,
                       sizeof( GUID));

        *ReturnLength = sizeof( GUID);

        uniGUID.Buffer = NULL;

        RtlStringFromGUID( pSIDEntryCB->AuthGroup,
                           &uniGUID);

        AFSDbgTrace(( AFS_SUBSYSTEM_AUTHGROUP_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "%s Retrieved AuthGroup GUID %wZ for ProcessID %I64X\n",
                      __FUNCTION__,
                      &uniGUID,
                      ullProcessID));

        if( uniGUID.Buffer != NULL)
        {
            RtlFreeUnicodeString( &uniGUID);
        }

        AFSReleaseResource( pDeviceExt->Specific.Control.AuthGroupTree.TreeLock);

try_exit:

        if( bReleaseSID &&
            uniSIDString.Length > 0)
        {
            RtlFreeUnicodeString( &uniSIDString);
        }
    }

    return ntStatus;
}
