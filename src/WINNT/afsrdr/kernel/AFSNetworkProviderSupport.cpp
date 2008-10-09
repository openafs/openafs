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

    __Enter
    {

        AFSAcquireExcl( &AFSProviderListLock,
                        TRUE);

        //
        // Look for the connection
        //

        uniRemoteName.Length = ConnectCB->RemoteNameLength;
        uniRemoteName.MaximumLength = uniRemoteName.Length;

        uniRemoteName.Buffer = ConnectCB->RemoteName;

        pConnection = AFSProviderConnectionList;

        while( pConnection != NULL)
        {

            if( pConnection->LocalName == ConnectCB->LocalName &&
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

        if( uniRemoteName.Length >= AFSServerName.Length + sizeof( WCHAR))
        {

            USHORT usLength = uniRemoteName.Length;

            uniRemoteName.Length = AFSServerName.Length;

            if( RtlCompareUnicodeString( &AFSServerName,
                                         &uniRemoteName,
                                         TRUE) != 0)
            {

                *ResultStatus = WN_BAD_NETNAME;

                *ReturnOutputBufferLength = sizeof( ULONG);

                try_return( ntStatus = STATUS_SUCCESS);
            }

            uniRemoteName.Length = usLength;
        }

        //
        // Allocate a new node and add it to our list
        //

        pConnection = (AFSProviderConnectionCB *)ExAllocatePoolWithTag( PagedPool,
                                                                        sizeof( AFSProviderConnectionCB) +
                                                                                      ConnectCB->RemoteNameLength,
                                                                        AFS_PROVIDER_CB);

        if( pConnection == NULL)
        {

            *ResultStatus = WN_OUT_OF_MEMORY;

            *ReturnOutputBufferLength = sizeof( ULONG);

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pConnection,
                       sizeof( AFSProviderConnectionCB) + ConnectCB->RemoteNameLength);

        pConnection->LocalName = ConnectCB->LocalName;

        pConnection->RemoteName.Length = ConnectCB->RemoteNameLength;
        pConnection->RemoteName.MaximumLength = pConnection->RemoteName.Length;

        pConnection->RemoteName.Buffer = (WCHAR *)((char *)pConnection + sizeof( AFSProviderConnectionCB));

        RtlCopyMemory( pConnection->RemoteName.Buffer,
                       ConnectCB->RemoteName,
                       pConnection->RemoteName.Length);

        pConnection->ResourceType = ConnectCB->ResourceType;

        if( AFSProviderConnectionList == NULL)
        {

            AFSProviderConnectionList = pConnection;
        }
        else
        {

            //
            // Get the end of the list
            //

            pLastConnection = AFSProviderConnectionList;

            while( pLastConnection->fLink != NULL)
            {

                pLastConnection = pLastConnection->fLink;
            }

            pLastConnection->fLink = pConnection;
        }

        *ResultStatus = WN_SUCCESS;

        *ReturnOutputBufferLength = sizeof( ULONG);

try_exit:

        AFSReleaseResource( &AFSProviderListLock);

    }

    return ntStatus;
}

NTSTATUS
AFSCancelConnection( IN AFSNetworkProviderConnectionCB *ConnectCB,
                     IN OUT PULONG ResultStatus,
                     IN OUT ULONG_PTR *ReturnOutputBufferLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProviderConnectionCB *pConnection = NULL, *pLastConnection = NULL;
    UNICODE_STRING uniRemoteName;

    __Enter
    {

        AFSAcquireExcl( &AFSProviderListLock,
                        TRUE);

        //
        // Look for the connection
        //

        uniRemoteName.Length = ConnectCB->RemoteNameLength;
        uniRemoteName.MaximumLength = uniRemoteName.Length;

        uniRemoteName.Buffer = NULL;

        if( uniRemoteName.Length > 0)
        {

            uniRemoteName.Buffer = ConnectCB->RemoteName;
        }

        pConnection = AFSProviderConnectionList;

        while( pConnection != NULL)
        {

            if( ( ConnectCB->LocalName != L'\0' &&
                  pConnection->LocalName == ConnectCB->LocalName) 
                            ||
                ( RtlCompareUnicodeString( &uniRemoteName,
                                           &pConnection->RemoteName,
                                           TRUE) == 0))
            {

                break;
            }

            pLastConnection = pConnection;

            pConnection = pConnection->fLink;
        }

        if( pConnection == NULL)
        {

            *ResultStatus = WN_NOT_CONNECTED;

            *ReturnOutputBufferLength = sizeof( ULONG);

            try_return( ntStatus);
        }

        if( pLastConnection == NULL)
        {

            AFSProviderConnectionList = pConnection->fLink;
        }
        else
        {

            pLastConnection->fLink = pConnection->fLink;
        }

        ExFreePool( pConnection);

        *ResultStatus = WN_SUCCESS;

        *ReturnOutputBufferLength = sizeof( ULONG);

try_exit:

        AFSReleaseResource( &AFSProviderListLock);
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

    __Enter
    {

        AFSAcquireShared( &AFSProviderListLock,
                          TRUE);

        //
        // Look for the connection
        //

        pConnection = AFSProviderConnectionList;

        while( pConnection != NULL)
        {

            if( pConnection->LocalName == ConnectCB->LocalName)
            {

                break;
            }

            pConnection = pConnection->fLink;
        }

        if( pConnection == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        if( RemoteNameBufferLength < pConnection->RemoteName.Length)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlCopyMemory( RemoteName,
                       pConnection->RemoteName.Buffer,
                       pConnection->RemoteName.Length);

        *ReturnOutputBufferLength = pConnection->RemoteName.Length;

try_exit:

        AFSReleaseResource( &AFSProviderListLock);
    }

    return ntStatus;
}

NTSTATUS
AFSListConnections( IN OUT AFSNetworkProviderConnectionCB *ConnectCB,
                    IN ULONG ConnectionBufferLength,
                    IN OUT ULONG_PTR *ReturnOutputBufferLength)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProviderConnectionCB *pConnection = NULL;
    ULONG ulCopiedLength = 0, ulRemainingLength = ConnectionBufferLength;

    __Enter
    {

        AFSAcquireShared( &AFSProviderListLock,
                          TRUE);

        //
        // Look for the connection
        //

        pConnection = AFSProviderConnectionList;

        while( pConnection != NULL)
        {

            if( ulRemainingLength < (ULONG)FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                               pConnection->RemoteName.Length)
            {

                ntStatus = STATUS_INSUFFICIENT_RESOURCES;

                break;
            }

            ConnectCB->LocalName = pConnection->LocalName;

            ConnectCB->RemoteNameLength = pConnection->RemoteName.Length;

            RtlCopyMemory( ConnectCB->RemoteName,
                           pConnection->RemoteName.Buffer,
                           pConnection->RemoteName.Length);

            ConnectCB->ResourceType = pConnection->ResourceType;

            ulCopiedLength += FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                    pConnection->RemoteName.Length;

            ConnectCB = (AFSNetworkProviderConnectionCB *)((char *)ConnectCB + 
                                                            FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                            pConnection->RemoteName.Length);

            pConnection = pConnection->fLink;
        }

        if( NT_SUCCESS( ntStatus))
        {

            *ReturnOutputBufferLength = ulCopiedLength;
        }

        AFSReleaseResource( &AFSProviderListLock);
    }

    return ntStatus;
}

