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
AFSAddConnectionEx( IN UNICODE_STRING *RemoteName,
                    IN ULONG DisplayType,
                    IN ULONG Flags)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSProviderConnectionCB *pConnection = NULL, *pLastConnection = NULL, *pServerConnection = NULL;
    UNICODE_STRING uniRemoteName;
    AFSDeviceExt *pRDRDevExt = (AFSDeviceExt *)AFSRDRDeviceObject->DeviceExtension;

    __Enter
    {

        AFSDbgTrace(( AFS_SUBSYSTEM_LOCK_PROCESSING,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSAddConnectionEx Acquiring AFSProviderListLock lock %p EXCL %08lX\n",
                      &pRDRDevExt->Specific.RDR.ProviderListLock,
                      PsGetCurrentThread()));

        AFSAcquireExcl( &pRDRDevExt->Specific.RDR.ProviderListLock,
                        TRUE);


        AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSAddConnectionEx remote name %wZ display type %08lX flags %08lX\n",
                      RemoteName,
                      DisplayType,
                      Flags));

        //
        // If this is a server, start in the enum list, otherwise
        // locate the server node
        //

        if( DisplayType == RESOURCEDISPLAYTYPE_SERVER)
        {

            pConnection = pRDRDevExt->Specific.RDR.ProviderEnumerationList;
        }
        else
        {

            pServerConnection = pRDRDevExt->Specific.RDR.ProviderEnumerationList; // For now we have only one server ...

            if( pServerConnection == NULL)
            {

                try_return( ntStatus);
            }

            pConnection = pServerConnection->EnumerationList;
        }

        //
        // Look for the connection
        //

        uniRemoteName.Length = RemoteName->Length;
        uniRemoteName.MaximumLength = RemoteName->Length;

        uniRemoteName.Buffer = RemoteName->Buffer;

        while( pConnection != NULL)
        {

            if( RtlCompareUnicodeString( &uniRemoteName,
                                         &pConnection->RemoteName,
                                         TRUE) == 0)
            {

                break;
            }

            pConnection = pConnection->fLink;
        }

        if( pConnection != NULL)
        {

            try_return( ntStatus);
        }

        //
        // Strip off any trailing slashes
        //

        if( uniRemoteName.Buffer[ (uniRemoteName.Length/sizeof( WCHAR)) - 1] == L'\\')
        {

            uniRemoteName.Buffer[ (uniRemoteName.Length/sizeof( WCHAR)) - 1] = L'\0';

            uniRemoteName.Length -= sizeof( WCHAR);
        }

        AFSDbgTrace(( AFS_SUBSYSTEM_NETWORK_PROVIDER,
                      AFS_TRACE_LEVEL_VERBOSE,
                      "AFSAddConnectionEx Inserting remote name %wZ\n",
                      &uniRemoteName));

        //
        // Allocate a new node and add it to our list
        //

        pConnection = (AFSProviderConnectionCB *)AFSExAllocatePoolWithTag( PagedPool,
                                                                           sizeof( AFSProviderConnectionCB) +
                                                                           uniRemoteName.Length,
                                                                           AFS_PROVIDER_CB);

        if( pConnection == NULL)
        {

            try_return( ntStatus = STATUS_INSUFFICIENT_RESOURCES);
        }

        RtlZeroMemory( pConnection,
                       sizeof( AFSProviderConnectionCB) + uniRemoteName.Length);

        pConnection->LocalName = L'\0';

        pConnection->RemoteName.Length = uniRemoteName.Length;
        pConnection->RemoteName.MaximumLength = pConnection->RemoteName.Length;

        pConnection->RemoteName.Buffer = (WCHAR *)((char *)pConnection + sizeof( AFSProviderConnectionCB));

        RtlCopyMemory( pConnection->RemoteName.Buffer,
                       uniRemoteName.Buffer,
                       pConnection->RemoteName.Length);

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
                                     DisplayType);

        //
        // Store away the flags for the connection
        //

        pConnection->Flags = Flags;

        //
        // Insert the entry into our list. If this is a server
        // connection then add it to the enumeration list, otherwise
        // find the server name for this connection
        //

        if( DisplayType == RESOURCEDISPLAYTYPE_SERVER)
        {

            if( pRDRDevExt->Specific.RDR.ProviderEnumerationList == NULL)
            {

                pRDRDevExt->Specific.RDR.ProviderEnumerationList = pConnection;
            }
            else
            {

                //
                // Get the end of the list
                //

                pLastConnection = pRDRDevExt->Specific.RDR.ProviderEnumerationList;

                while( pLastConnection->fLink != NULL)
                {

                    pLastConnection = pLastConnection->fLink;
                }

                pLastConnection->fLink = pConnection;
            }
        }
        else if( pServerConnection != NULL)
        {

            if( pServerConnection->EnumerationList == NULL)
            {

                pServerConnection->EnumerationList = pConnection;
            }
            else
            {

                //
                // Get the end of the list
                //

                pLastConnection = pServerConnection->EnumerationList;

                while( pLastConnection->fLink != NULL)
                {

                    pLastConnection = pLastConnection->fLink;
                }

                pLastConnection->fLink = pConnection;
            }
        }

try_exit:

        AFSReleaseResource( &pRDRDevExt->Specific.RDR.ProviderListLock);
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

            Connection->Scope = RESOURCE_GLOBALNET;

            Connection->DisplayType = RESOURCEDISPLAYTYPE_SERVER;

            Connection->Usage = RESOURCEUSAGE_CONTAINER;

            Connection->Comment.Length = 20;
            Connection->Comment.MaximumLength = 22;

            Connection->Comment.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                            Connection->Comment.MaximumLength,
                                                                            AFS_NETWORK_PROVIDER_7_TAG);

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

                Connection->Scope = RESOURCE_GLOBALNET;
            }

            Connection->Comment.Length = 18;
            Connection->Comment.MaximumLength = 20;

            Connection->Comment.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                            Connection->Comment.MaximumLength,
                                                                            AFS_NETWORK_PROVIDER_8_TAG);

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

        Connection->Scope = RESOURCE_CONNECTED;

        Connection->Comment.Length = 26;
        Connection->Comment.MaximumLength = 28;

        Connection->Comment.Buffer = (WCHAR *)AFSExAllocatePoolWithTag( PagedPool,
                                                                        Connection->Comment.MaximumLength,
                                                                        AFS_NETWORK_PROVIDER_9_TAG);

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
