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
// File: AFSNetworkProviderSupport.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSAddConnection( IN AFSNetworkProviderConnectionCB *ConnectCB,
                  IN OUT PULONG ResultStatus,
                  IN OUT ULONG_PTR *ReturnOutputBufferLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProviderConnectionCB *pConnection = NULL, *pLastConnection = NULL;
    UNICODE_STRING uniRemoteName;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSAddConnection Acquiring AFSProviderListLock lock %p EXCL %08lX\n",
                      &pRDRDevExt->Specific.RDR.ProviderListLock,
                      PsGetCurrentThread()));

        if( ConnectCB->AuthenticationId.QuadPart == 0)
        {

            ConnectCB->AuthenticationId = AFSGetAuthenticationId();

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSAddConnection Retrieved authentication id %I64X\n",
                          ConnectCB->AuthenticationId.QuadPart));
        }

        AFSAcquireExcl( &pRDRDevExt->Specific.RDR.ProviderListLock,
                        TRUE);

        //
        // Look for the connection
        //

        uniRemoteName.Length = (USHORT)ConnectCB->RemoteNameLength;
        uniRemoteName.MaximumLength = uniRemoteName.Length;

        uniRemoteName.Buffer = ConnectCB->RemoteName;

        //
        // Strip off any trailing slashes
        //

        if( uniRemoteName.Buffer[ (uniRemoteName.Length/sizeof( WCHAR)) - 1] == L'\\')
        {

            uniRemoteName.Length -= sizeof( WCHAR);
        }

        pConnection = pRDRDevExt->Specific.RDR.ProviderConnectionList;

        while( pConnection != NULL)
        {

            if( pConnection->LocalName == ConnectCB->LocalName &&
                pConnection->AuthenticationId.QuadPart == ConnectCB->AuthenticationId.QuadPart &&
                RtlCompareUnicodeString( &uniRemoteName,
                                         &pConnection->RemoteName,
                                         TRUE) == 0)
            {

                break;
            }

            pConnection = pConnection->fLink;
        }

        if( pConnection != NULL)
        {

            if( ConnectCB->LocalName != L'\0')
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSAddConnection ALREADY_CONNECTED remote name %wZ Local %C authentication id %I64X\n",
                              &uniRemoteName,
                              ConnectCB->LocalName,
                              ConnectCB->AuthenticationId.QuadPart));
            }
            else
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSAddConnection ALREADY_CONNECTED remote name %wZ Local (NULL) authentication id %I64X\n",
                              &uniRemoteName,
                              ConnectCB->AuthenticationId.QuadPart));
            }

            *ResultStatus = WN_ALREADY_CONNECTED;

            *ReturnOutputBufferLength = sizeof( ULONG);

            try_return( ntStatus);
        }

        //
        // Validate the remote name
        //

        if( uniRemoteName.Length > 2 * sizeof( WCHAR) &&
            uniRemoteName.Buffer[ 0] == L'\\' &&
            uniRemoteName.Buffer[ 1] == L'\\')
        {

            uniRemoteName.Buffer = &uniRemoteName.Buffer[ 2];

            uniRemoteName.Length -= (2 * sizeof( WCHAR));
        }

        if( uniRemoteName.Length >= AFSServerName.Length)
        {

            USHORT usLength = uniRemoteName.Length;

            if (uniRemoteName.Buffer[AFSServerName.Length/sizeof( WCHAR)] != L'\\')
            {

                if( ConnectCB->LocalName != L'\0')
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSAddConnection BAD_NETNAME 1 remote name %wZ Local %C authentication id %I64X\n",
                                  &uniRemoteName,
                                  ConnectCB->LocalName,
                                  ConnectCB->AuthenticationId.QuadPart));
                }
                else
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSAddConnection BAD_NETNAME 1 remote name %wZ Local (NULL) authentication id %I64X\n",
                                  &uniRemoteName,
                                  ConnectCB->AuthenticationId.QuadPart));
                }

                *ResultStatus = WN_BAD_NETNAME;

                *ReturnOutputBufferLength = sizeof( ULONG);

                try_return( ntStatus = STATUS_SUCCESS);
            }

            uniRemoteName.Length = AFSServerName.Length;

            if( RtlCompareUnicodeString( &AFSServerName,
                                         &uniRemoteName,
                                         TRUE) != 0)
            {

                if( ConnectCB->LocalName != L'\0')
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSAddConnection BAD_NETNAME 2 remote name %wZ Local %C authentication id %I64X\n",
                                  &uniRemoteName,
                                  ConnectCB->LocalName,
                                  ConnectCB->AuthenticationId.QuadPart));
                }
                else
                {

                    AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                                  AFS_TRACE_LEVEL_VERBOSE,
                                  "AFSAddConnection BAD_NETNAME 2 remote name %wZ Local (NULL) authentication id %I64X\n",
                                  &uniRemoteName,
                                  ConnectCB->AuthenticationId.QuadPart));
                }

                *ResultStatus = WN_BAD_NETNAME;

                *ReturnOutputBufferLength = sizeof( ULONG);

                try_return( ntStatus = STATUS_SUCCESS);
            }

            uniRemoteName.Length = usLength;
        }
        else
        {

            if( ConnectCB->LocalName != L'\0')
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSAddConnection BAD_NETNAME 3 remote name %wZ Local %C authentication id %I64X\n",
                              &uniRemoteName,
                              ConnectCB->LocalName,
                              ConnectCB->AuthenticationId.QuadPart));
            }
            else
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSAddConnection BAD_NETNAME 3 remote name %wZ Local (NULL) authentication id %I64X\n",
                              &uniRemoteName,
                              ConnectCB->AuthenticationId.QuadPart));
            }

            *ResultStatus = WN_BAD_NETNAME;

            *ReturnOutputBufferLength = sizeof( ULONG);

            try_return( ntStatus = STATUS_SUCCESS);
        }

        uniRemoteName.Length = (USHORT)ConnectCB->RemoteNameLength;
        uniRemoteName.MaximumLength = uniRemoteName.Length;

        uniRemoteName.Buffer = ConnectCB->RemoteName;

        //
        // Strip off any trailing slashes
        //

        if( uniRemoteName.Buffer[ (uniRemoteName.Length/sizeof( WCHAR)) - 1] == L'\\')
        {

            uniRemoteName.Length -= sizeof( WCHAR);
        }

        //
        // Allocate a new node and add it to our list
        //

        pConnection = (AFSProviderConnectionCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                           sizeof( AFSProviderConnectionCB) +
                                                                                            uniRemoteName.Length,
                                                                           AFS_PROVIDER_CB);

        if( pConnection == NULL)
        {

            *ResultStatus = WN_OUT_OF_MEMORY;

            *ReturnOutputBufferLength = sizeof( ULONG);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pConnection,
                       sizeof( AFSProviderConnectionCB) + uniRemoteName.Length);

        pConnection->LocalName = ConnectCB->LocalName;

        pConnection->RemoteName.Length = uniRemoteName.Length;
        pConnection->RemoteName.MaximumLength = pConnection->RemoteName.Length;

        pConnection->RemoteName.Buffer = (WCHAR *)((char *)pConnection + sizeof( AFSProviderConnectionCB));

        RtlCopyMemory( pConnection->RemoteName.Buffer,
                       uniRemoteName.Buffer,
                       pConnection->RemoteName.Length);

        pConnection->Type = ConnectCB->Type;

        pConnection->AuthenticationId = ConnectCB->AuthenticationId;

        if( ConnectCB->LocalName != L'\0')
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSAddConnection Adding connection remote name %wZ Local %C authentication id %I64X\n",
                          &uniRemoteName,
                          ConnectCB->LocalName,
                          ConnectCB->AuthenticationId.QuadPart));
        }
        else
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSAddConnection Adding connection remote name %wZ Local (NULL) authentication id %I64X\n",
                          &uniRemoteName,
                          ConnectCB->AuthenticationId.QuadPart));
        }

        //
        // Point to the component portion of the name
        //

        pConnection->ComponentName.Length = 0;
        pConnection->ComponentName.MaximumLength = 0;

        pConnection->ComponentName.Buffer = &pConnection->RemoteName.Buffer[ (pConnection->RemoteName.Length/sizeof( WCHAR)) - 1];

        while( pConnection->ComponentName.Length <= pConnection->RemoteName.Length)
        {

            if( pConnection->ComponentName.Buffer[ 0] == L'\\')
            {

                pConnection->ComponentName.Buffer++;

                break;
            }

            pConnection->ComponentName.Length += sizeof( WCHAR);
            pConnection->ComponentName.MaximumLength += sizeof( WCHAR);

            pConnection->ComponentName.Buffer--;
        }

        //
        // Go initialize the information about the connection
        //

        AFSInitializeConnectionInfo( pConnection,
                                     (ULONG)-1);

        //
        // Insert the entry into our list
        //

        if( pRDRDevExt->Specific.RDR.ProviderConnectionList == NULL)
        {

            pRDRDevExt->Specific.RDR.ProviderConnectionList = pConnection;
        }
        else
        {

            //
            // Get the end of the list
            //

            pLastConnection = pRDRDevExt->Specific.RDR.ProviderConnectionList;

            while( pLastConnection->fLink != NULL)
            {

                pLastConnection = pLastConnection->fLink;
            }

            pLastConnection->fLink = pConnection;
        }

        *ResultStatus = WN_SUCCESS;

        *ReturnOutputBufferLength = sizeof( ULONG);

try_exit:

        AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);

    }

    return ntStatus;
}

NTSTATUS
AFSCancelConnection( IN AFSNetworkProviderConnectionCB *ConnectCB,
                     IN OUT AFSCancelConnectionResultCB *ConnectionResult,
                     IN OUT ULONG_PTR *ReturnOutputBufferLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProviderConnectionCB *pConnection = NULL, *pLastConnection = NULL;
    UNICODE_STRING uniRemoteName;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        ConnectionResult->Version = AFS_NETWORKPROVIDER_INTERFACE_VERSION_1;

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSCancelConnection Acquiring AFSProviderListLock lock %p EXCL %08lX\n",
                      &pRDRDevExt->Specific.RDR.ProviderListLock,
                      PsGetCurrentThread()));

        if( ConnectCB->AuthenticationId.QuadPart == 0)
        {

            ConnectCB->AuthenticationId = AFSGetAuthenticationId();

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSCancelConnection Retrieved authentication id %I64X\n",
                          ConnectCB->AuthenticationId.QuadPart));
        }

        AFSAcquireExcl( &pRDRDevExt->Specific.RDR.ProviderListLock,
                        TRUE);

        //
        // Look for the connection
        //

        uniRemoteName.Length = (USHORT)ConnectCB->RemoteNameLength;
        uniRemoteName.MaximumLength = uniRemoteName.Length;

        uniRemoteName.Buffer = NULL;

        if( uniRemoteName.Length > 0)
        {

            uniRemoteName.Buffer = ConnectCB->RemoteName;
        }

        pConnection = pRDRDevExt->Specific.RDR.ProviderConnectionList;

        while( pConnection != NULL)
        {

            if( ( ConnectCB->LocalName != L'\0' &&
                  pConnection->LocalName == ConnectCB->LocalName)
                ||
                ( ConnectCB->LocalName == L'\0' &&
                  pConnection->LocalName == L'\0' &&
                  RtlCompareUnicodeString( &uniRemoteName,
                                           &pConnection->RemoteName,
                                           TRUE) == 0))
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSCancelConnection Checking remote name %wZ to stored %wZ authentication id %I64X - %I64X\n",
                              &uniRemoteName,
                              &pConnection->RemoteName,
                              ConnectCB->AuthenticationId.QuadPart,
                              pConnection->AuthenticationId.QuadPart));

                if( ConnectCB->AuthenticationId.QuadPart == pConnection->AuthenticationId.QuadPart &&
                    ( ConnectCB->LocalName == L'\0' ||
                      RtlCompareUnicodeString( &uniRemoteName,
                                               &pConnection->RemoteName,
                                               TRUE) == 0))
                {

                    break;
                }
            }

            pLastConnection = pConnection;

            pConnection = pConnection->fLink;
        }

        if( pConnection == NULL)
        {

            ConnectionResult->Status = WN_NOT_CONNECTED;

            *ReturnOutputBufferLength = sizeof( AFSCancelConnectionResultCB);

            try_return( ntStatus);
        }

        if( pLastConnection == NULL)
        {

            pRDRDevExt->Specific.RDR.ProviderConnectionList = pConnection->fLink;
        }
        else
        {

            pLastConnection->fLink = pConnection->fLink;
        }

        if( pConnection->Comment.Buffer != NULL)
        {

            AFSExFreePoolWithTag( pConnection->Comment.Buffer, 0);
        }

        ConnectionResult->LocalName = pConnection->LocalName;

        AFSExFreePoolWithTag( pConnection, AFS_PROVIDER_CB);

        ConnectionResult->Status = WN_SUCCESS;

        *ReturnOutputBufferLength = sizeof( AFSCancelConnectionResultCB);

try_exit:

        AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);
    }

    return ntStatus;
}

NTSTATUS
AFSGetConnection( IN AFSNetworkProviderConnectionCB *ConnectCB,
                  IN OUT WCHAR *RemoteName,
                  IN ULONG RemoteNameBufferLength,
                  IN OUT ULONG_PTR *ReturnOutputBufferLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProviderConnectionCB *pConnection = NULL;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        if( ConnectCB->LocalName != L'\0')
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnection Local %C authentication id %I64X\n",
                          ConnectCB->LocalName,
                          ConnectCB->AuthenticationId.QuadPart));
        }
        else
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnection Local (NULL) authentication id %I64X\n",
                          ConnectCB->AuthenticationId.QuadPart));
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSGetConnection Acquiring AFSProviderListLock lock %p SHARED %08lX\n",
                      &pRDRDevExt->Specific.RDR.ProviderListLock,
                      PsGetCurrentThread()));

        if( ConnectCB->AuthenticationId.QuadPart == 0)
        {

            ConnectCB->AuthenticationId = AFSGetAuthenticationId();

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnection Retrieved authentication id %I64X\n",
                          ConnectCB->AuthenticationId.QuadPart));
        }

        AFSAcquireShared( &pRDRDevExt->Specific.RDR.ProviderListLock,
                          TRUE);

        //
        // Look for the connection
        //

        pConnection = pRDRDevExt->Specific.RDR.ProviderConnectionList;

        while( pConnection != NULL)
        {

            if( pConnection->LocalName != L'\0')
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSGetConnection Comparing passed in %C to %C authentication id %I64X - %I64X\n",
                              ConnectCB->LocalName,
                              pConnection->LocalName,
                              ConnectCB->AuthenticationId.QuadPart,
                              pConnection->AuthenticationId.QuadPart));
            }
            else
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSGetConnection Comparing passed in %C to (NULL) authentication id %I64X - %I64X\n",
                              ConnectCB->LocalName,
                              ConnectCB->AuthenticationId.QuadPart,
                              pConnection->AuthenticationId.QuadPart));
            }

            if( pConnection->LocalName == ConnectCB->LocalName &&
                pConnection->AuthenticationId.QuadPart == ConnectCB->AuthenticationId.QuadPart)
            {

                break;
            }

            pConnection = pConnection->fLink;
        }

        if( pConnection == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnection INVALID_PARAMETER\n"));

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        if( RemoteNameBufferLength < pConnection->RemoteName.Length)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnection INSUFFICIENT_RESOURCES\n"));

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlCopyMemory( RemoteName,
                       pConnection->RemoteName.Buffer,
                       pConnection->RemoteName.Length);

        *ReturnOutputBufferLength = pConnection->RemoteName.Length;

try_exit:

        AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);
    }

    return ntStatus;
}

NTSTATUS
AFSListConnections( IN OUT AFSNetworkProviderConnectionCB *ConnectCB,
                    IN ULONG ConnectionBufferLength,
                    IN OUT ULONG_PTR *ReturnOutputBufferLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProviderConnectionCB *pConnection = NULL, *pRootConnection = NULL;
    ULONG ulCopiedLength = 0, ulRemainingLength = ConnectionBufferLength;
    ULONG ulScope, ulType;
    UNICODE_STRING uniRemoteName, uniServerName, uniShareName, uniRemainingPath;
    BOOLEAN bGlobalEnumeration = FALSE;
    ULONG       ulIndex = 0;
    LARGE_INTEGER liAuthenticationID = {0,0};
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        //
        // Save off some data before moving on
        //

        ulScope = ConnectCB->Scope;

        ulType = ConnectCB->Type;

        if( ConnectCB->AuthenticationId.QuadPart == 0)
        {

            ConnectCB->AuthenticationId = AFSGetAuthenticationId();

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSListConnections Retrieved authentication id %I64X\n",
                          ConnectCB->AuthenticationId.QuadPart));
        }

        liAuthenticationID.QuadPart = ConnectCB->AuthenticationId.QuadPart;

        uniRemoteName.Length = 0;
        uniRemoteName.MaximumLength = 0;
        uniRemoteName.Buffer = NULL;

        uniServerName.Length = 0;
        uniServerName.MaximumLength = 0;
        uniServerName.Buffer = NULL;

        uniShareName.Length = 0;
        uniShareName.MaximumLength = 0;
        uniShareName.Buffer = NULL;

        if( ConnectCB->RemoteNameLength > 0)
        {

            uniRemoteName.Length = (USHORT)ConnectCB->RemoteNameLength;
            uniRemoteName.MaximumLength = uniRemoteName.Length + sizeof( WCHAR);

            uniRemoteName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                      uniRemoteName.MaximumLength,
                                                                      AFS_NETWORK_PROVIDER_1_TAG);

            if( uniRemoteName.Buffer == NULL)
            {

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            RtlCopyMemory( uniRemoteName.Buffer,
                           ConnectCB->RemoteName,
                           uniRemoteName.Length);

            if( uniRemoteName.Buffer[ 0] == L'\\' &&
                uniRemoteName.Buffer[ 1] == L'\\')
            {

                uniRemoteName.Buffer = &uniRemoteName.Buffer[ 1];

                uniRemoteName.Length -= sizeof( WCHAR);
            }

            if( uniRemoteName.Buffer[ (uniRemoteName.Length/sizeof( WCHAR)) - 1] == L'\\')
            {

                uniRemoteName.Length -= sizeof( WCHAR);
            }

            FsRtlDissectName( uniRemoteName,
                              &uniServerName,
                              &uniRemainingPath);

            uniRemoteName = uniRemainingPath;

            if( uniRemoteName.Length > 0)
            {

                FsRtlDissectName( uniRemoteName,
                                  &uniShareName,
                                  &uniRemainingPath);
            }

            //
            // If this is an enumeration of the global share name then
            // adjust it to be the server name itself
            //

            if( uniShareName.Length == 0 ||
                RtlCompareUnicodeString( &uniShareName,
                                         &AFSGlobalRootName,
                                         TRUE) == 0)
            {

                bGlobalEnumeration = TRUE;
            }
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSListConnections Acquiring AFSProviderListLock lock %p SHARED %08lX\n",
                      &pRDRDevExt->Specific.RDR.ProviderListLock,
                      PsGetCurrentThread()));

        AFSAcquireShared( &pRDRDevExt->Specific.RDR.ProviderListLock,
                          TRUE);

        //
        // If this is a globalnet enumeration with no name then enumerate the server list
        //

        if( ulScope == RESOURCE_GLOBALNET)
        {

            if( uniServerName.Buffer == NULL)
            {

                pConnection = pRDRDevExt->Specific.RDR.ProviderEnumerationList;
            }
            else
            {

                //
                // Go locate the root entry for the name passed in
                //

                if( bGlobalEnumeration)
                {

                    if( pRDRDevExt->Specific.RDR.ProviderEnumerationList == NULL)
                    {

                        AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);

                        try_return( ntStatus);
                    }

                    pConnection = pRDRDevExt->Specific.RDR.ProviderEnumerationList->EnumerationList;
                }
                else
                {

                    pRootConnection = AFSLocateEnumRootEntry( &uniShareName);

                    if( pRootConnection == NULL)
                    {

                        AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);

                        try_return( ntStatus);
                    }

                    //
                    // Need to handle these enumerations from the directory listing
                    //

                    ntStatus = AFSEnumerateConnection( ConnectCB,
                                                       pRootConnection,
                                                       ConnectionBufferLength,
                                                       &ulCopiedLength);
                }
            }
        }
        else
        {

            pConnection = pRDRDevExt->Specific.RDR.ProviderConnectionList;
        }

        ulIndex = ConnectCB->CurrentIndex;

        while( pConnection != NULL)
        {

            if( bGlobalEnumeration &&
                BooleanFlagOn( pConnection->Flags, AFS_CONNECTION_FLAG_GLOBAL_SHARE))
            {

                pConnection = pConnection->fLink;

                continue;
            }

            if( ulScope != RESOURCE_GLOBALNET &&
                !BooleanFlagOn( pConnection->Usage, RESOURCEUSAGE_ATTACHED))
            {

                pConnection = pConnection->fLink;

                continue;
            }

            if( pConnection->LocalName != L'\0')
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSListConnections Processing entry %wZ %C authentication id %I64X - %I64X Scope %08lX\n",
                              &pConnection->RemoteName,
                              pConnection->LocalName,
                              liAuthenticationID.QuadPart,
                              pConnection->AuthenticationId.QuadPart,
                              pConnection->Scope));
            }
            else
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSListConnections Processing entry %wZ NULL LocalName authentication id %I64X - %I64X Scope %08lX\n",
                              &pConnection->RemoteName,
                              liAuthenticationID.QuadPart,
                              pConnection->AuthenticationId.QuadPart,
                              pConnection->Scope));
            }

            if( ulScope != RESOURCE_GLOBALNET &&
                pConnection->AuthenticationId.QuadPart != liAuthenticationID.QuadPart)
            {

                pConnection = pConnection->fLink;

                continue;
            }

            if( ulIndex > 0)
            {

                ulIndex--;

                pConnection = pConnection->fLink;

                continue;
            }

            if( ulRemainingLength < (ULONG)FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                         pConnection->RemoteName.Length +
                                                         pConnection->Comment.Length)
            {

                break;
            }

            ConnectCB->RemoteNameLength = pConnection->RemoteName.Length;

            RtlCopyMemory( ConnectCB->RemoteName,
                           pConnection->RemoteName.Buffer,
                           pConnection->RemoteName.Length);

            ConnectCB->LocalName = pConnection->LocalName;

            ConnectCB->Type = pConnection->Type;

            ConnectCB->Scope = pConnection->Scope;

            if( !bGlobalEnumeration)
            {

                ConnectCB->Scope = RESOURCE_CONNECTED;
            }

            ConnectCB->DisplayType = pConnection->DisplayType;

            ConnectCB->Usage = pConnection->Usage;

            ConnectCB->CommentLength = pConnection->Comment.Length;

            if( pConnection->Comment.Length > 0)
            {

                ConnectCB->CommentOffset = (ULONG)(FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                                        ConnectCB->RemoteNameLength);

                RtlCopyMemory( (void *)((char *)ConnectCB + ConnectCB->CommentOffset),
                               pConnection->Comment.Buffer,
                               ConnectCB->CommentLength);
            }

            ulCopiedLength += FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                    ConnectCB->RemoteNameLength +
                                                    ConnectCB->CommentLength;

            ulRemainingLength -= FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                    ConnectCB->RemoteNameLength +
                                                    ConnectCB->CommentLength;

            ConnectCB = (AFSNetworkProviderConnectionCB *)((char *)ConnectCB +
                                                            FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                            ConnectCB->RemoteNameLength +
                                                            ConnectCB->CommentLength);

            pConnection = pConnection->fLink;
        }

        if( NT_SUCCESS( ntStatus))
        {

            *ReturnOutputBufferLength = ulCopiedLength;
        }

        AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);

try_exit:

        if( uniRemoteName.Buffer != NULL)
        {

            AFSExFreePoolWithTag( uniRemoteName.Buffer, 0);
        }
    }

    return ntStatus;
}

void
AFSInitializeConnectionInfo( IN AFSProviderConnectionCB *Connection,
                             IN ULONG DisplayType)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    UNICODE_STRING uniName, uniComponentName, uniRemainingName;

    __Enter
    {

        uniName = Connection->RemoteName;

        //
        // Strip of the double leading slash if there is one
        //

        if( uniName.Buffer[ 0] == L'\\' &&
            uniName.Buffer[ 1] == L'\\')
        {

            uniName.Buffer = &uniName.Buffer[ 1];

            uniName.Length -= sizeof( WCHAR);
        }


        FsRtlDissectName( uniName,
                          &uniComponentName,
                          &uniRemainingName);

        //
        // Initialize the information for the connection
        // First, if this is the server only then mark it accordingly
        //

        if( uniRemainingName.Length == 0 ||
            DisplayType == RESOURCEDISPLAYTYPE_SERVER)
        {

            Connection->Type = RESOURCETYPE_DISK;

            Connection->Scope = 0;

            Connection->DisplayType = RESOURCEDISPLAYTYPE_SERVER;

            Connection->Usage = RESOURCEUSAGE_CONTAINER;

            Connection->Comment.Length = 20;
            Connection->Comment.MaximumLength = 22;

            Connection->Comment.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                            Connection->Comment.MaximumLength,
                                                                            AFS_NETWORK_PROVIDER_2_TAG);

            if( Connection->Comment.Buffer != NULL)
            {

                RtlZeroMemory( Connection->Comment.Buffer,
                               Connection->Comment.MaximumLength);

                RtlCopyMemory( Connection->Comment.Buffer,
                               L"AFS Root",
                               16);
            }
            else
            {

                Connection->Comment.Length = 0;
                Connection->Comment.MaximumLength = 0;
            }

            try_return( ntStatus);
        }

        uniName = uniRemainingName;

        FsRtlDissectName( uniName,
                          &uniComponentName,
                          &uniRemainingName);

        if( uniRemainingName.Length == 0 ||
            uniRemainingName.Buffer == NULL ||
            DisplayType == RESOURCEDISPLAYTYPE_SHARE)
        {

            Connection->Type = RESOURCETYPE_DISK;

            Connection->DisplayType = RESOURCEDISPLAYTYPE_SHARE;

            Connection->Usage = RESOURCEUSAGE_CONNECTABLE;

            if( Connection->LocalName != L'\0')
            {

                Connection->Usage |= RESOURCEUSAGE_ATTACHED;

                Connection->Scope = RESOURCE_CONNECTED;
            }
            else
            {

                Connection->Usage |= RESOURCEUSAGE_NOLOCALDEVICE;

                Connection->Scope = RESOURCE_GLOBALNET;
            }

            Connection->Comment.Length = 18;
            Connection->Comment.MaximumLength = 20;

            Connection->Comment.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                            Connection->Comment.MaximumLength,
                                                                            AFS_NETWORK_PROVIDER_3_TAG);

            if( Connection->Comment.Buffer != NULL)
            {

                RtlZeroMemory( Connection->Comment.Buffer,
                               Connection->Comment.MaximumLength);

                RtlCopyMemory( Connection->Comment.Buffer,
                               L"AFS Share",
                               18);
            }
            else
            {

                Connection->Comment.Length = 0;
                Connection->Comment.MaximumLength = 0;
            }

            try_return( ntStatus);
        }

        //
        // This is a sub directory within a share
        //

        Connection->Type = RESOURCETYPE_DISK;

        Connection->DisplayType = RESOURCEDISPLAYTYPE_DIRECTORY;

        Connection->Usage = RESOURCEUSAGE_CONNECTABLE;

        if( Connection->LocalName != L'\0')
        {

            Connection->Usage |= RESOURCEUSAGE_ATTACHED;
        }
        else
        {

            Connection->Usage |= RESOURCEUSAGE_NOLOCALDEVICE;
        }

        Connection->Scope = RESOURCE_CONNECTED;

        Connection->Comment.Length = 26;
        Connection->Comment.MaximumLength = 28;

        Connection->Comment.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                        Connection->Comment.MaximumLength,
                                                                        AFS_NETWORK_PROVIDER_4_TAG);

        if( Connection->Comment.Buffer != NULL)
        {

            RtlZeroMemory( Connection->Comment.Buffer,
                           Connection->Comment.MaximumLength);

            RtlCopyMemory( Connection->Comment.Buffer,
                           L"AFS Directory",
                           26);
        }
        else
        {

            Connection->Comment.Length = 0;
            Connection->Comment.MaximumLength = 0;
        }

try_exit:

        NOTHING;
    }

    return;
}

AFSProviderConnectionCB *
AFSLocateEnumRootEntry( IN UNICODE_STRING *RemoteName)
{

    AFSProviderConnectionCB *pConnection = NULL;
    UNICODE_STRING uniRemoteName = *RemoteName;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        if( pRDRDevExt->Specific.RDR.ProviderEnumerationList == NULL)
        {

            try_return( pConnection);
        }

        pConnection = pRDRDevExt->Specific.RDR.ProviderEnumerationList->EnumerationList;

        while( pConnection != NULL)
        {

            if( RtlCompareUnicodeString( &uniRemoteName,
                                         &pConnection->ComponentName,
                                         TRUE) == 0)
            {

                break;
            }

            pConnection = pConnection->fLink;
        }

try_exit:

        NOTHING;
    }

    return pConnection;
}

NTSTATUS
AFSEnumerateConnection( IN OUT AFSNetworkProviderConnectionCB *ConnectCB,
                        IN AFSProviderConnectionCB *RootConnection,
                        IN ULONG BufferLength,
                        OUT PULONG CopiedLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG ulCRC = 0, ulCopiedLength = 0;
    AFSDirectoryCB *pShareDirEntry = NULL;
    AFSDirectoryCB *pDirEntry = NULL, *pTargetDirEntry = NULL;
    ULONG ulIndex = 0;
    LONG lCount;

    __Enter
    {

        if( AFSGlobalRoot == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSEnumerateConnection Global root not yet initialized\n"));

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        ulCRC = AFSGenerateCRC( &RootConnection->ComponentName,
                                FALSE);

        //
        // Grab our tree lock shared
        //

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSEnumerateConnection Acquiring GlobalRoot DirectoryNodeHdr.TreeLock lock %p SHARED %08lX\n",
                      AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock,
                      PsGetCurrentThread()));

        AFSAcquireShared( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock,
                          TRUE);

        //
        // Locate the dir entry for this node
        //

        ntStatus = AFSLocateCaseSensitiveDirEntry( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.CaseSensitiveTreeHead,
                                                   ulCRC,
                                                   &pShareDirEntry);

        if( pShareDirEntry == NULL ||
            !NT_SUCCESS( ntStatus))
        {

            //
            // Perform a case insensitive search
            //

            ulCRC = AFSGenerateCRC( &RootConnection->ComponentName,
                                    TRUE);

            ntStatus = AFSLocateCaseInsensitiveDirEntry( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.CaseInsensitiveTreeHead,
                                                         ulCRC,
                                                         &pShareDirEntry);

            if( pShareDirEntry == NULL ||
                !NT_SUCCESS( ntStatus))
            {

                AFSReleaseResource( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock);

                try_return( ntStatus);
            }
        }

        lCount = InterlockedIncrement( &pShareDirEntry->DirOpenReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSEnumerateConnection1 Increment count on %wZ DE %p Ccb %p Cnt %d\n",
                      &pShareDirEntry->NameInformation.FileName,
                      pShareDirEntry,
                      NULL,
                      lCount));

        AFSReleaseResource( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock);

        //
        // Setup the request to evaluate the entry
        // On success, pTargetDirEntry has the DirOpenReferenceCount held
        //

        ntStatus = AFSEvaluateRootEntry( pShareDirEntry,
                                         &pTargetDirEntry);

        if( !NT_SUCCESS( ntStatus))
        {

            try_return( ntStatus);
        }

        AFSAcquireShared( pTargetDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock,
                          TRUE);

        //
        // Enumerate the content
        //

        pDirEntry = pTargetDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeListHead;

        ulIndex = ConnectCB->CurrentIndex;

        while( pDirEntry != NULL)
        {

            if( ulIndex > 0)
            {

                ulIndex--;

                pDirEntry = (AFSDirectoryCB *)pDirEntry->ListEntry.fLink;

                continue;
            }

            if( BufferLength < (ULONG)FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                               pDirEntry->NameInformation.FileName.Length)
            {

                break;
            }

            ConnectCB->LocalName = L'\0';

            ConnectCB->RemoteNameLength = pDirEntry->NameInformation.FileName.Length;

            RtlCopyMemory( ConnectCB->RemoteName,
                           pDirEntry->NameInformation.FileName.Buffer,
                           pDirEntry->NameInformation.FileName.Length);

            ConnectCB->Type = RESOURCETYPE_DISK;

            ConnectCB->Scope = RESOURCE_CONNECTED;

            if( BooleanFlagOn( pDirEntry->ObjectInformation->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
            {

                ConnectCB->DisplayType = RESOURCEDISPLAYTYPE_DIRECTORY;
            }
            else
            {

                ConnectCB->DisplayType = RESOURCEDISPLAYTYPE_FILE;
            }

            ConnectCB->Usage = 0;

            ConnectCB->CommentLength = 0;

            ulCopiedLength += FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                    pDirEntry->NameInformation.FileName.Length;

            BufferLength -= FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                    pDirEntry->NameInformation.FileName.Length;

            ConnectCB = (AFSNetworkProviderConnectionCB *)((char *)ConnectCB +
                                                            FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                            ConnectCB->RemoteNameLength);

            pDirEntry = (AFSDirectoryCB *)pDirEntry->ListEntry.fLink;
        }

        //
        // Release the DirOpenReferenceCount obtained from AFSEvaluateRootEntry
        //

        lCount = InterlockedDecrement( &pTargetDirEntry->DirOpenReferenceCount);

        AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSEnumerateConnection Decrement count on %wZ DE %p Ccb %p Cnt %d\n",
                      &pTargetDirEntry->NameInformation.FileName,
                      pTargetDirEntry,
                      NULL,
                      lCount));

        ASSERT( lCount >= 0);

        *CopiedLength = ulCopiedLength;

        AFSReleaseResource( pTargetDirEntry->ObjectInformation->Specific.Directory.DirectoryNodeHdr.TreeLock);

try_exit:

        if( pShareDirEntry != NULL)
        {
            lCount = InterlockedDecrement( &pShareDirEntry->DirOpenReferenceCount);

            AFSDbgTrace(( AFS_SUBSYSTEM_DIRENTRY_REF_COUNTING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSEnumerateConnection1 Decrement count on %wZ DE %p Ccb %p Cnt %d\n",
                          &pShareDirEntry->NameInformation.FileName,
                          pShareDirEntry,
                          NULL,
                          lCount));

            ASSERT( lCount >= 0);
        }
    }

    return ntStatus;
}

NTSTATUS
AFSGetConnectionInfo( IN AFSNetworkProviderConnectionCB *ConnectCB,
                      IN ULONG BufferLength,
                      IN OUT ULONG_PTR *ReturnOutputBufferLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProviderConnectionCB *pConnection = NULL, *pBestMatch = NULL;
    UNICODE_STRING uniRemoteName, uniServerName, uniShareName, uniRemainingPath, uniFullName, uniRemainingPathLocal;
    USHORT usNameLen = 0, usMaxNameLen = 0;
    BOOLEAN bEnumerationEntry = FALSE;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        uniServerName.Length = 0;
        uniServerName.MaximumLength = 0;
        uniServerName.Buffer = NULL;

        uniShareName.Length = 0;
        uniShareName.MaximumLength = 0;
        uniShareName.Buffer = NULL;

        uniRemainingPathLocal.Length = 0;
        uniRemainingPathLocal.MaximumLength = 0;
        uniRemainingPathLocal.Buffer = NULL;

        uniRemoteName.Length = (USHORT)ConnectCB->RemoteNameLength;
        uniRemoteName.MaximumLength = uniRemoteName.Length + sizeof( WCHAR);
        uniRemoteName.Buffer = (WCHAR *)ConnectCB->RemoteName;

        if( ConnectCB->LocalName != L'\0')
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnectionInfo remote name %wZ Local %C authentication id %I64X\n",
                          &uniRemoteName,
                          ConnectCB->LocalName,
                          ConnectCB->AuthenticationId.QuadPart));
        }
        else
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnectionInfo remote name %wZ Local (NULL) authentication id %I64X\n",
                          &uniRemoteName,
                          ConnectCB->AuthenticationId.QuadPart));
        }

        if( AFSGlobalRoot == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSGetConnectionInfo Global root not yet initialized\n"));

            try_return( ntStatus = STATUS_DEVICE_NOT_READY);
        }

        uniFullName = uniRemoteName;

        if( uniRemoteName.Buffer[ 0] == L'\\' &&
            uniRemoteName.Buffer[ 1] == L'\\')
        {

            uniRemoteName.Buffer = &uniRemoteName.Buffer[ 1];

            uniRemoteName.Length -= sizeof( WCHAR);
        }

        if( uniRemoteName.Buffer[ (uniRemoteName.Length/sizeof( WCHAR)) - 1] == L'\\')
        {

            uniRemoteName.Length -= sizeof( WCHAR);

            uniFullName.Length -= sizeof( WCHAR);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSGetConnectionInfo Processing name %wZ\n",
                      &uniFullName));

        FsRtlDissectName( uniRemoteName,
                          &uniServerName,
                          &uniRemainingPath);

        uniRemoteName = uniRemainingPath;

        if( uniRemoteName.Length > 0)
        {

            FsRtlDissectName( uniRemoteName,
                              &uniShareName,
                              &uniRemainingPath);
        }

        if( RtlCompareUnicodeString( &uniServerName,
                                     &AFSServerName,
                                     TRUE) != 0)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        if ( uniRemainingPath.Length > 0 )
        {

            //
            // Add back in the leading \ since it was stripped off above
            //

            uniRemainingPath.Length += sizeof( WCHAR);
            if( uniRemainingPath.Length > uniRemainingPath.MaximumLength)
            {
                uniRemainingPath.MaximumLength += sizeof( WCHAR);
            }

            uniRemainingPath.Buffer--;

            uniRemainingPathLocal.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                              uniRemainingPath.Length,
                                                                              AFS_NETWORK_PROVIDER_5_TAG);

            if( uniRemainingPathLocal.Buffer == NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSGetConnectionInfo INSUFFICIENT_RESOURCES\n"));

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            uniRemainingPathLocal.MaximumLength = uniRemainingPath.Length;
            uniRemainingPathLocal.Length = uniRemainingPath.Length;

            RtlCopyMemory( uniRemainingPathLocal.Buffer,
                           uniRemainingPath.Buffer,
                           uniRemainingPath.Length);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSGetConnectionInfo Acquiring AFSProviderListLock lock %p SHARED %08lX\n",
                      &pRDRDevExt->Specific.RDR.ProviderListLock,
                      PsGetCurrentThread()));

        AFSAcquireShared( &pRDRDevExt->Specific.RDR.ProviderListLock,
                          TRUE);

        //
        // If this is the server then return information about the
        // server entry
        //

        if( uniShareName.Length == 0 &&
            RtlCompareUnicodeString( &uniServerName,
                                     &AFSServerName,
                                     TRUE) == 0)
        {

            pConnection = pRDRDevExt->Specific.RDR.ProviderEnumerationList;
        }
        else
        {

            //
            // See if we can locate it on our connection list
            //

            pConnection = pRDRDevExt->Specific.RDR.ProviderConnectionList;

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnectionInfo Searching for full name %wZ\n",
                          &uniFullName));

            while( pConnection != NULL)
            {

                if( usMaxNameLen < pConnection->RemoteName.Length &&
                    pConnection->RemoteName.Length <= uniFullName.Length)
                {

                    usNameLen = uniFullName.Length;

                    uniFullName.Length = pConnection->RemoteName.Length;

                    if( pConnection->AuthenticationId.QuadPart == ConnectCB->AuthenticationId.QuadPart &&
                        RtlCompareUnicodeString( &uniFullName,
                                                 &pConnection->RemoteName,
                                                 TRUE) == 0)
                    {

                        usMaxNameLen = pConnection->RemoteName.Length;

                        pBestMatch = pConnection;

                        AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                                      AFS_TRACE_LEVEL_VERBOSE,
                                      "AFSGetConnectionInfo Found match for %wZ\n",
                                      &pConnection->RemoteName));
                    }

                    uniFullName.Length = usNameLen;
                }

                pConnection = pConnection->fLink;
            }

            pConnection = pBestMatch;

            if( pConnection != NULL)
            {

                bEnumerationEntry = TRUE;

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSGetConnectionInfo Using best match for %wZ\n",
                              &pConnection->RemoteName));
            }

            if( pConnection == NULL)
            {

                //
                // Locate the entry for the share
                //

                pConnection = AFSLocateEnumRootEntry( &uniShareName);
            }
        }

        if( pConnection == NULL)
        {
            UNICODE_STRING uniFullName;
            AFSDirEnumEntry *pDirEnumEntry = NULL;

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnectionInfo No connection for full name %wZ\n",
                          &uniFullName));

            //
            // Drop the lock, we will pick it up again later
            //

            AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);

            //
            // OK, ask the CM about this component name
            //

            ntStatus = AFSEvaluateTargetByName( NULL,
                                                &AFSGlobalRoot->ObjectInformation,
                                                &uniShareName,
                                                0,
                                                &pDirEnumEntry);

            if( !NT_SUCCESS( ntStatus))
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSGetConnectionInfo Evaluation Failed share name %wZ\n",
                              uniShareName));

                try_return( ntStatus = STATUS_INVALID_PARAMETER);

            }

            //
            // Don't need this
            //

            AFSExFreePoolWithTag( pDirEnumEntry, AFS_GENERIC_MEMORY_3_TAG);

            //
            // The share name is valid
            // Allocate a new node and add it to our list
            //
            uniFullName.MaximumLength = PAGE_SIZE;
            uniFullName.Length = 0;

            uniFullName.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                    uniFullName.MaximumLength,
                                                                    AFS_NETWORK_PROVIDER_6_TAG);

            if( uniFullName.Buffer == NULL)
            {

                AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSGetConnectionInfo INSUFFICIENT_RESOURCES\n"));

                try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
            }

            uniFullName.Buffer[ 0] = L'\\';
            uniFullName.Buffer[ 1] = L'\\';

            uniFullName.Length = 2 * sizeof( WCHAR);

            RtlCopyMemory( &uniFullName.Buffer[ 2],
                           AFSServerName.Buffer,
                           AFSServerName.Length);

            uniFullName.Length += AFSServerName.Length;

            uniFullName.Buffer[ uniFullName.Length/sizeof( WCHAR)] = L'\\';

            uniFullName.Length += sizeof( WCHAR);

            RtlCopyMemory( &uniFullName.Buffer[ uniFullName.Length/sizeof( WCHAR)],
                           uniShareName.Buffer,
                           uniShareName.Length);

            uniFullName.Length += uniShareName.Length;

            AFSAcquireExcl( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock,
                            TRUE);

            ntStatus = AFSAddConnectionEx( &uniFullName,
                                           RESOURCEDISPLAYTYPE_SHARE,
                                           0);

            AFSReleaseResource( AFSGlobalRoot->ObjectInformation.Specific.Directory.DirectoryNodeHdr.TreeLock);

            AFSExFreePoolWithTag( uniFullName.Buffer, 0);

            AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnectionInfo Acquiring AFSProviderListLock lock %p SHARED %08lX\n",
                          &pRDRDevExt->Specific.RDR.ProviderListLock,
                          PsGetCurrentThread()));

            AFSAcquireShared( &pRDRDevExt->Specific.RDR.ProviderListLock,
                              TRUE);

            if ( NT_SUCCESS( ntStatus) )
            {
                //
                // Once again, locate the entry for the share we just created
                //

                pConnection = AFSLocateEnumRootEntry( &uniShareName);
            }

        }

        if( pConnection == NULL)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_ERROR,
                          "AFSGetConnectionInfo Failed to locate entry for full name %wZ\n",
                          &uniFullName));

            AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // Fill in the returned connection info block
        //

        if( BufferLength < (ULONG)FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                pConnection->RemoteName.Length +
                                                pConnection->Comment.Length +
                                                uniRemainingPath.Length)
        {

            AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                          AFS_TRACE_LEVEL_VERBOSE,
                          "AFSGetConnectionInfo Buffer too small for full name %wZ\n",
                          &uniFullName));

            AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);

            try_return( ntStatus = STATUS_BUFFER_OVERFLOW);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSGetConnectionInfo Returning entry Scope %08lX partial name %wZ full name %wZ\n",
                      pConnection->Scope, &pConnection->RemoteName, &uniFullName));

        ConnectCB->RemoteNameLength = pConnection->RemoteName.Length;

        if( !bEnumerationEntry)
        {

            RtlCopyMemory( ConnectCB->RemoteName,
                           uniFullName.Buffer,
                           pConnection->RemoteName.Length);
        }
        else
        {

            RtlCopyMemory( ConnectCB->RemoteName,
                           pConnection->RemoteName.Buffer,
                           pConnection->RemoteName.Length);
        }

        ConnectCB->LocalName = pConnection->LocalName;

        ConnectCB->Type = pConnection->Type;

        ConnectCB->Scope = pConnection->Scope;

        ConnectCB->DisplayType = pConnection->DisplayType;

        ConnectCB->Usage = pConnection->Usage;

        ConnectCB->CommentLength = pConnection->Comment.Length;

        ConnectCB->RemainingPathLength = uniRemainingPathLocal.Length;

        AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSGetConnectionInfo Returning lengths: remote %u comment %u remaining %u\n",
                      ConnectCB->RemoteNameLength,
                      ConnectCB->CommentLength,
                      ConnectCB->RemainingPathLength));

        if( ConnectCB->CommentLength > 0)
        {

            ConnectCB->CommentOffset = (ULONG)(FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                             ConnectCB->RemoteNameLength);

            RtlCopyMemory( (void *)((char *)ConnectCB + ConnectCB->CommentOffset),
                           pConnection->Comment.Buffer,
                           ConnectCB->CommentLength);
        }

        if ( ConnectCB->RemainingPathLength > 0 )
        {

            ConnectCB->RemainingPathOffset = (ULONG)(FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                                   ConnectCB->RemoteNameLength +
                                                                   ConnectCB->CommentLength);

            RtlCopyMemory( (void *)((char *)ConnectCB + ConnectCB->RemainingPathOffset),
                           uniRemainingPathLocal.Buffer,
                           uniRemainingPathLocal.Length);
        }

        *ReturnOutputBufferLength = FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                  ConnectCB->RemoteNameLength +
                                                  ConnectCB->CommentLength +
                                                  ConnectCB->RemainingPathLength;

        AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);

try_exit:

        if ( uniRemainingPathLocal.Buffer )
        {

            AFSExFreePoolWithTag( uniRemainingPathLocal.Buffer, 0);
        }
    }

    return ntStatus;
}

BOOLEAN
AFSIsDriveMapped( IN WCHAR DriveMapping)
{

    BOOLEAN bDriveMapped = FALSE;
    AFSProviderConnectionCB *pConnection = NULL;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        AFSAcquireShared( &pRDRDevExt->Specific.RDR.ProviderListLock,
                          TRUE);

        pConnection = pRDRDevExt->Specific.RDR.ProviderConnectionList;

        while( pConnection != NULL)
        {

            if( pConnection->LocalName != L'\0' &&
                RtlUpcaseUnicodeChar( pConnection->LocalName) == RtlUpcaseUnicodeChar( DriveMapping))
            {

                bDriveMapped = TRUE;

                break;
            }

            pConnection = pConnection->fLink;
        }

        AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);
    }

    return bDriveMapped;
}
