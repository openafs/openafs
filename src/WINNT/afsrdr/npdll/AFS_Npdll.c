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

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#include <windows.h>
#include <windef.h>
#include <winbase.h>
#include <winsvc.h>
#include <winnetwk.h>
#include <npapi.h>
#include <winioctl.h>

#ifndef WNNC_NET_OPENAFS
#define WNNC_NET_OPENAFS     0x00390000
#endif

#include "AFSUserCommon.h"
#include "AFSProvider.h"

#include "stdio.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define SCRATCHSZ 1024

//
// Common informaiton
//

#define DbgP(_x_) WideDbgPrint _x_

ULONG _cdecl WideDbgPrint( PWCHAR Format, ... );

#define TRACE_TAG    L"AFS NP: "

#define WNNC_DRIVER( major, minor ) ( major * 0x00010000 + minor )

#define OPENAFS_PROVIDER_NAME           L"OpenAFS Network"
#define OPENAFS_PROVIDER_NAME_LENGTH    30

DWORD
NPIsRedirectorStarted( void);

#define try_return(S) { S; goto try_exit; }

#define __Enter

#define NOTHING

//#define OPENAFS_TRACE         1

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

HANDLE
OpenRedirector( void);

//
// This is the only function which must be exported, everything else is optional
//

DWORD 
APIENTRY
NPGetCaps( DWORD nIndex )
{

    DWORD rc = 0;
    WCHAR wszOutputBuffer[ 512];

    switch( nIndex)
    {
        case WNNC_SPEC_VERSION:
        {

            rc = WNNC_SPEC_VERSION51;
            break;
        }

        case WNNC_NET_TYPE:
        {
            rc = WNNC_NET_OPENAFS;
            break;
        }

        case WNNC_DRIVER_VERSION:
        {

            rc = WNNC_DRIVER(1, 0);
            break;
        }

        case WNNC_CONNECTION:
        {

            rc = WNNC_CON_GETCONNECTIONS |
                    WNNC_CON_CANCELCONNECTION |
                    WNNC_CON_ADDCONNECTION |
                    WNNC_CON_ADDCONNECTION3;
            break;
        }

        case WNNC_ENUMERATION:
        {

            rc = WNNC_ENUM_LOCAL;
            break;
        }

        case WNNC_START:
        {

            rc = 1;

            break;
        }

        case WNNC_USER:
        case WNNC_DIALOG:
        case WNNC_ADMIN:
        default:
        {

            rc = 0;
            break;
        }
    }
    
    return rc;
}

DWORD 
APIENTRY
NPAddConnection( LPNETRESOURCE   lpNetResource,
                 LPWSTR          lpPassword,
                 LPWSTR          lpUserName )
{

    return NPAddConnection3( NULL, lpNetResource, lpPassword, lpUserName, 0 );
}

DWORD 
APIENTRY
NPAddConnection3( HWND            hwndOwner,
                  LPNETRESOURCE   lpNetResource,
                  LPWSTR          lpPassword,
                  LPWSTR          lpUserName,
                  DWORD           dwFlags )
{

    DWORD    dwStatus = WN_SUCCESS;
    WCHAR    wchRemoteName[MAX_PATH+1];
    WCHAR    wszScratch[SCRATCHSZ];
    WCHAR    wchLocalName[3];
    DWORD    dwCopyBytes = 0;
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    HANDLE   hControlDevice = NULL;

    __Enter
    {

        if ((lpNetResource->lpRemoteName == NULL) ||
            (lpNetResource->lpRemoteName[0] != L'\\') ||
            (lpNetResource->lpRemoteName[1] != L'\\') ||
            ((lpNetResource->dwType != RESOURCETYPE_DISK) &&
             (lpNetResource->dwType != RESOURCETYPE_ANY)))
        {

            return WN_BAD_NETNAME;
        }

        if( lpNetResource->lpLocalName != NULL)
        {

            wchLocalName[0] = (WCHAR) CharUpper( (PWCHAR) MAKELONG( (USHORT) lpNetResource->lpLocalName[0], 0 ) );
            wchLocalName[1] = L':';
            wchLocalName[2] = L'\0';
        }

        lstrcpy( wchRemoteName, lpNetResource->lpRemoteName);
        wchRemoteName[MAX_PATH] = L'\0';

        //
        // Allocate our buffer to pass to the redirector filter
        //

        dwBufferSize = sizeof( AFSNetworkProviderConnectionCB) + (wcslen( wchRemoteName) * sizeof( WCHAR));

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);
 
        if( pConnectCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        if( lpNetResource->lpLocalName != NULL)
        {

            pConnectCB->LocalName = wchLocalName[0];
        }
        else
        {

            pConnectCB->LocalName = L'\0';
        }

        pConnectCB->RemoteNameLength = wcslen( wchRemoteName) * sizeof( WCHAR);

        memcpy( pConnectCB->RemoteName,
                wchRemoteName,
                pConnectCB->RemoteNameLength);

        pConnectCB->ResourceType = lpNetResource->dwType;

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_ADD_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   &dwStatus,
                                   sizeof( DWORD),
                                   &dwCopyBytes,
                                   NULL);

        CloseHandle( hControlDevice);

        if( !dwError)
        {

            swprintf( wszScratch, L"NPAddConnection3 Failed to add connection to file system %d\n", GetLastError());

            OutputDebugString( wszScratch);

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        //
        // The status returned from the driver will indicate how it was handled
        //

        if( dwStatus == WN_SUCCESS &&
            lpNetResource->lpLocalName != NULL)
        {

            WCHAR TempBuf[64];

            if( !QueryDosDeviceW( wchLocalName,
                                  TempBuf,
                                  64))
            {

                if( GetLastError() != ERROR_FILE_NOT_FOUND)
                {

                    dwStatus = ERROR_ALREADY_ASSIGNED;
                }
                else
                {

                    UNICODE_STRING uniConnectionName;
                    UNICODE_STRING uniDeviceName;

                    uniDeviceName.Length = (wcslen( AFS_RDR_DEVICE_NAME) * sizeof( WCHAR));
                    uniDeviceName.MaximumLength = uniDeviceName.Length;
                    uniDeviceName.Buffer = AFS_RDR_DEVICE_NAME;

                    //
                    // Create a symbolic link object to the device we are redirecting
                    //

                    uniConnectionName.MaximumLength = (USHORT)( uniDeviceName.Length +
                                                                pConnectCB->RemoteNameLength +
                                                                8 +              // Local name and \;
                                                                sizeof(WCHAR));   //  Space for NULL-termination.

                    //
                    //  Don't include NULL-termination.
                    //

                    uniConnectionName.Length = uniConnectionName.MaximumLength - sizeof(WCHAR);

                    uniConnectionName.Buffer = LocalAlloc( LMEM_ZEROINIT,
                                                           uniConnectionName.MaximumLength);

                    if( uniConnectionName.Buffer == NULL)
                    {

                        try_return( dwStatus = GetLastError());
                    }

                    CopyMemory( uniConnectionName.Buffer,
                                uniDeviceName.Buffer,
                                uniDeviceName.Length);

                    wcscat( uniConnectionName.Buffer, L"\\;" );

                    wcscat( uniConnectionName.Buffer, wchLocalName);

                    wcscat( uniConnectionName.Buffer, wchRemoteName);

                    if( !DefineDosDeviceW( DDD_RAW_TARGET_PATH |
                                           DDD_NO_BROADCAST_SYSTEM,
                                           wchLocalName,
                                           uniConnectionName.Buffer))
                    {

                        OutputDebugString(L"NPAddConnection3 Failed to assign drive\n");

                        dwStatus = GetLastError();
                    }
                    else
                    {

                        dwStatus = WN_SUCCESS;
                    }

                    LocalFree( uniConnectionName.Buffer);
                }
            }
            else
            {

                dwStatus = ERROR_ALREADY_ASSIGNED;
            }
        }

try_exit:

        if( pConnectCB != NULL)
        {

            HeapFree( GetProcessHeap( ), 0, (PVOID) pConnectCB);
        }
    }
    
    return dwStatus;
}

DWORD 
APIENTRY
NPCancelConnection( LPWSTR  lpName,
                    BOOL    fForce)
{

    WCHAR    wchRemoteName[MAX_PATH+1];
    DWORD    dwRemoteNameLength = (MAX_PATH+1) * sizeof(WCHAR);
    DWORD    dwStatus = WN_NOT_CONNECTED;
    WCHAR    wszScratch[SCRATCHSZ];
    DWORD    dwCopyBytes = 0;
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    BOOL     bLocalName = TRUE;
    HANDLE   hControlDevice = NULL;

    WCHAR    wchOutputBuffer[ 256];

    __Enter
    {

        if( *lpName == L'\\' && 
            *(lpName + 1) == L'\\')
        {

            bLocalName = FALSE;
        }

        if( bLocalName)
        {

            //
            // Get the remote name for the connection, if we are handling it
            //

            dwStatus = NPGetConnection( lpName,
                                        wchRemoteName,
                                        &dwRemoteNameLength);

            if( dwStatus != WN_SUCCESS ||
                dwRemoteNameLength == 0)
            {

                try_return( dwStatus = WN_NOT_CONNECTED);
            }
        }
        else
        {

            wcscpy( wchRemoteName, lpName);

            dwRemoteNameLength = (wcslen( wchRemoteName) * sizeof( WCHAR));
        }

        wchRemoteName[ dwRemoteNameLength/sizeof( WCHAR)] = L'\0';

        dwBufferSize = sizeof( AFSNetworkProviderConnectionCB);

        if( !bLocalName)
        {

            dwBufferSize += dwRemoteNameLength;
        }

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);

        if( pConnectCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        if( bLocalName)
        {

            pConnectCB->LocalName = lpName[0];

            pConnectCB->RemoteNameLength = 0;
        }
        else
        {

            pConnectCB->LocalName = L'\0';

            pConnectCB->RemoteNameLength = (USHORT)dwRemoteNameLength;

            wcscpy( pConnectCB->RemoteName,
                    wchRemoteName);
        }

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_CANCEL_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   &dwStatus,
                                   sizeof( DWORD),
                                   &dwCopyBytes,
                                   NULL);

        CloseHandle( hControlDevice);

        if( !dwError)
        {

            OutputDebugString(L"NPCancelConnection Failed to cancel connection to file system\n");

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        if( dwStatus == WN_SUCCESS &&
            bLocalName)
        {

            UNICODE_STRING uniConnectionName;
            UNICODE_STRING uniDeviceName;

            uniDeviceName.Length = (wcslen( AFS_RDR_DEVICE_NAME) * sizeof( WCHAR));
            uniDeviceName.MaximumLength = uniDeviceName.Length;
            uniDeviceName.Buffer = AFS_RDR_DEVICE_NAME;

            //
            // Create a symbolic link object to the device we are redirecting
            //

            uniConnectionName.MaximumLength = (USHORT)( uniDeviceName.Length +
                                                        dwRemoteNameLength +
                                                        8 +             // Local name and \;
                                                        sizeof(WCHAR)); //  Space for NULL-termination.

            //
            //  Don't include NULL-termination.
            //

            uniConnectionName.Length = uniConnectionName.MaximumLength - sizeof(WCHAR);

            uniConnectionName.Buffer = LocalAlloc( LMEM_ZEROINIT,
                                                   uniConnectionName.MaximumLength);

            if( uniConnectionName.Buffer == NULL)
            {

                try_return( dwStatus = GetLastError());
            }

            CopyMemory( uniConnectionName.Buffer,
                        uniDeviceName.Buffer,
                        uniDeviceName.Length);

            wcscat( uniConnectionName.Buffer, L"\\;" );

            wcscat( uniConnectionName.Buffer, lpName);

            wcscat( uniConnectionName.Buffer, wchRemoteName);

            DefineDosDevice( DDD_REMOVE_DEFINITION | DDD_RAW_TARGET_PATH | DDD_EXACT_MATCH_ON_REMOVE,
                             lpName,
                             uniConnectionName.Buffer);
        }

try_exit:

        if( pConnectCB != NULL)
        {

            HeapFree( GetProcessHeap( ), 0, (PVOID) pConnectCB);
        }

    }

    return dwStatus;
}

DWORD 
APIENTRY
NPGetConnection( LPWSTR  lpLocalName,
                 LPWSTR  lpRemoteName,
                 LPDWORD lpBufferSize)
{

    DWORD    dwStatus = WN_NOT_CONNECTED;
    WCHAR    wchLocalName[3];
    WCHAR    wszScratch[ 255];
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    HANDLE   hControlDevice = NULL;
    DWORD    dwPassedLength = *lpBufferSize;

    WCHAR    wchOutputBuffer[ 256];

    __Enter
    {

        if( lstrlen( lpLocalName) == 0)
        {

            OutputDebugString(L"NPGetConnection No local name\n");

            try_return( dwStatus = WN_BAD_LOCALNAME);
        }

        wchLocalName[0] = (WCHAR) CharUpper( (PWCHAR) MAKELONG( (USHORT) lpLocalName[0], 0 ) );
        wchLocalName[1] = L':';
        wchLocalName[2] = L'\0';

        dwBufferSize = 0x1000;

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);

        if( pConnectCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        pConnectCB->LocalName = wchLocalName[0];

        pConnectCB->RemoteNameLength = 0;

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_GET_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   pConnectCB,
                                   dwBufferSize,
                                   lpBufferSize,
                                   NULL);

        CloseHandle( hControlDevice);

        if( !dwError)
        {

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        if( *lpBufferSize > dwPassedLength)
        {

            dwPassedLength = *lpBufferSize;

            *lpBufferSize = dwPassedLength;

            try_return( dwStatus = WN_MORE_DATA);
        }

        memcpy( lpRemoteName,
                (void *)pConnectCB,
                *lpBufferSize);

        lpRemoteName[ *lpBufferSize/sizeof( WCHAR)] = L'\0';
 
        dwStatus = WN_SUCCESS;

try_exit:

        if( pConnectCB != NULL)
        {

            HeapFree( GetProcessHeap( ), 0, (PVOID) pConnectCB);
        }
    }

    return dwStatus;
}

DWORD 
APIENTRY
NPGetConnection3(
  IN     LPCWSTR lpLocalName,
  IN     DWORD dwLevel,
  OUT    LPVOID lpBuffer,
  IN OUT  LPDWORD lpBufferSize)
{

    DWORD    dwStatus = WN_NOT_CONNECTED;
    WCHAR    wchLocalName[3];
    WCHAR    wszScratch[ 255];
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    HANDLE   hControlDevice = NULL;
    DWORD    dwPassedLength = 0;
    NETCONNECTINFOSTRUCT *pNetInfo = (NETCONNECTINFOSTRUCT *)lpBuffer;

    WCHAR    wchOutputBuffer[ 256];

    __Enter
    {

        if( lstrlen( lpLocalName) == 0)
        {
            
            try_return( dwStatus = WN_BAD_LOCALNAME);
        }
        
        if( *lpBufferSize < sizeof( NETCONNECTINFOSTRUCT))
        {

            *lpBufferSize = sizeof( NETCONNECTINFOSTRUCT);

            try_return( dwStatus = WN_MORE_DATA);
        }

        wchLocalName[0] = (WCHAR) CharUpper( (PWCHAR) MAKELONG( (USHORT) lpLocalName[0], 0 ) );
        wchLocalName[1] = L':';
        wchLocalName[2] = L'\0';

        dwBufferSize = 0x1000;

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);

        if( pConnectCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        pConnectCB->LocalName = wchLocalName[0];

        pConnectCB->RemoteNameLength = 0;

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_GET_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   pConnectCB,
                                   dwBufferSize,
                                   lpBufferSize,
                                   NULL);

        CloseHandle( hControlDevice);

        if( !dwError)
        {

            swprintf( wchOutputBuffer, L"NPGetConnection Failed to get connection from file system for local %s\n", wchLocalName);

            OutputDebugString( wchOutputBuffer);

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        pNetInfo->dwDelay = 0;

        pNetInfo->dwFlags = 0;

        pNetInfo->dwOptDataSize = 0;

        pNetInfo->dwSpeed = 1560;

        *lpBufferSize = sizeof( NETCONNECTINFOSTRUCT);

        dwStatus = WN_SUCCESS;
 
try_exit:

        if( pConnectCB != NULL)
        {

            HeapFree( GetProcessHeap( ), 0, (PVOID) pConnectCB);
        }
    }

    return dwStatus;
}

DWORD 
APIENTRY
NPOpenEnum( DWORD          dwScope,
            DWORD          dwType,
            DWORD          dwUsage,
            LPNETRESOURCE  lpNetResource,
            LPHANDLE       lphEnum )
{
    
    DWORD   dwStatus;

    *lphEnum = NULL;

    switch( dwScope )
    {
        case RESOURCE_CONNECTED:
        {

            *lphEnum = HeapAlloc( GetProcessHeap( ), HEAP_ZERO_MEMORY, sizeof( ULONG ) );

            if( *lphEnum )
            {
                dwStatus = WN_SUCCESS;
            }
            else
            {
                dwStatus = WN_OUT_OF_MEMORY;
            }

            break;
        }

        case RESOURCE_CONTEXT:
        default:

            dwStatus  = WN_NOT_SUPPORTED;
            break;
    }

    return dwStatus;
}

DWORD 
APIENTRY
NPEnumResource( HANDLE  hEnum,
                LPDWORD lpcCount,
                LPVOID  lpBuffer,
                LPDWORD lpBufferSize)
{

    DWORD            dwStatus = WN_NO_MORE_ENTRIES; //WN_SUCCESS;
    ULONG            dwCopyBytes;
    ULONG            EntriesCopied;
    ULONG            dwIndex;
    LPNETRESOURCE    pNetResource;
    ULONG            SpaceNeeded;
    ULONG            SpaceAvailable;
    PWCHAR           StringZone;
    AFSNetworkProviderConnectionCB *pConnectionCB = NULL;
    void            *pConnectionCBBase = NULL;
    DWORD            dwError = 0;
    UNICODE_STRING   uniLocalName, uniRemoteName;
    WCHAR            wchLocalBuffer[ 5];
    HANDLE           hControlDevice = NULL;

    WCHAR wchOutputBuffer[ 512];

    __Enter
    {

        pNetResource = (LPNETRESOURCE) lpBuffer;
        SpaceAvailable = *lpBufferSize;
        EntriesCopied = 0;
        StringZone = (PWCHAR) ((char *)lpBuffer + *lpBufferSize);

        pConnectionCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 0x1000);

        if( pConnectionCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        pConnectionCBBase = (void *)pConnectionCB;

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_LIST_CONNECTIONS,
                                   NULL,
                                   0,
                                   pConnectionCB,
                                   0x1000,
                                   &dwCopyBytes,
                                   NULL);

        CloseHandle( hControlDevice);

        if( !dwError)
        {

            swprintf( wchOutputBuffer, L"NPEnumResource Failed to get connection list from file system %d\n", GetLastError());

            OutputDebugString( wchOutputBuffer);

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        if( dwCopyBytes == 0)
        {

            try_return( dwStatus = WN_NO_MORE_ENTRIES);
        }

        //
        // Get to where we need to be in the list
        //

        dwIndex = *((PULONG)hEnum);

        if( dwIndex > 0)
        {

            while( dwCopyBytes > 0)
            {

                dwCopyBytes -= (FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) + pConnectionCB->RemoteNameLength);

                if( pConnectionCB->LocalName != 0)
                {

                    dwIndex--;
                }

                pConnectionCB = (AFSNetworkProviderConnectionCB *)((char *)pConnectionCB + 
                                                                FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                                pConnectionCB->RemoteNameLength);

                if( dwIndex == 0)
                {

                    break;
                }
            }
        }

        if( dwCopyBytes == 0)
        {

            try_return( dwStatus = WN_NO_MORE_ENTRIES);
        }

        dwIndex = *((PULONG)hEnum);

        while( TRUE)
        {

            uniLocalName.Length = 0;
            uniLocalName.MaximumLength = 0;
            uniLocalName.Buffer = NULL;

            if( pConnectionCB->LocalName != 0)
            {

                uniLocalName.Buffer = wchLocalBuffer;

                uniLocalName.Length = 2;
                uniLocalName.MaximumLength = 2;
                uniLocalName.Buffer[ 0] = pConnectionCB->LocalName;
                uniLocalName.Buffer[ 1] = L'\0';
            }

            uniRemoteName.Length = pConnectionCB->RemoteNameLength;
            uniRemoteName.MaximumLength = uniRemoteName.Length;
            uniRemoteName.Buffer = pConnectionCB->RemoteName;

            // Determine the space needed for this entry...

            SpaceNeeded  = sizeof( NETRESOURCE );            // resource struct
            
            if( uniLocalName.Length > 0)
            {

                SpaceNeeded += 3 * sizeof(WCHAR);                // local name
            }

            SpaceNeeded += pConnectionCB->RemoteNameLength + sizeof( WCHAR);        // remote name
            SpaceNeeded += 0;                                                       // comment
            SpaceNeeded += OPENAFS_PROVIDER_NAME_LENGTH + sizeof( WCHAR);         // provider name

            if( SpaceNeeded > SpaceAvailable)
            {

                dwStatus = WN_MORE_DATA;
            
                *lpBufferSize = SpaceNeeded;
                
                break;
            }

            SpaceAvailable -= SpaceNeeded;

            pNetResource->dwScope       = RESOURCE_CONNECTED;
            pNetResource->dwType        = pConnectionCB->ResourceType;

            if( uniRemoteName.Length == 10)
            {

                pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SERVER;
            }
            else
            {

                pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
            }

            pNetResource->dwUsage       = 0; //RESOURCEUSAGE_CONNECTABLE | RESOURCEUSAGE_ATTACHED;

            // setup string area at opposite end of buffer
            StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded );
                
            // copy local name
            if( uniLocalName.Length > 0)
            {

                pNetResource->lpLocalName = StringZone;
                *StringZone++ = pConnectionCB->LocalName;
                *StringZone++ = L':';
                *StringZone++ = L'\0';
            }
            else
            {

                pNetResource->lpLocalName = NULL;
            }

            // copy remote name                    
            pNetResource->lpRemoteName = StringZone;
                    
            CopyMemory( StringZone, 
                        pConnectionCB->RemoteName, 
                        pConnectionCB->RemoteNameLength);

            StringZone += pConnectionCB->RemoteNameLength / sizeof(WCHAR);

            *StringZone++ = L'\0';

            // copy comment
            pNetResource->lpComment = NULL;

            // copy provider name
            pNetResource->lpProvider = StringZone;
            wcscpy( StringZone, OPENAFS_PROVIDER_NAME);         /* safe */

            StringZone += OPENAFS_PROVIDER_NAME_LENGTH / sizeof( WCHAR);

            *StringZone = L'\0';

            // setup the new end of buffer
            StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded );

            EntriesCopied++;

            pNetResource++;

            dwIndex++;

            dwCopyBytes -= FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) + pConnectionCB->RemoteNameLength;

            if( dwCopyBytes == 0)
            {

                dwStatus = WN_SUCCESS;

                break;
            }

            pConnectionCB = (AFSNetworkProviderConnectionCB *)((char *)pConnectionCB + 
                                                                FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                                pConnectionCB->RemoteNameLength);
        }

        *lpcCount = EntriesCopied;

        // update entry index
        *(PULONG)hEnum = dwIndex;

try_exit:

        if( pConnectionCBBase != NULL)
        {

            HeapFree( GetProcessHeap( ), 0, (PVOID) pConnectionCBBase);
        }
    }

    return dwStatus;
}

/*++

Routine Description:

    This routine closes the handle for enumeration of resources.

Arguments:

    hEnum  - the enumeration handle

Return Value:

    WN_SUCCESS if successful, otherwise the appropriate error

Notes:

    The sample only supports the notion of enumerating connected shares

--*/

DWORD APIENTRY
NPCloseEnum( HANDLE hEnum )
{

    HeapFree( GetProcessHeap( ), 0, (PVOID) hEnum );

    return WN_SUCCESS;
}

ULONG
_cdecl
WideDbgPrint(
    PWCHAR Format,
    ...
    )
{   
    ULONG rc = 0;
    WCHAR wszbuffer[255];

    va_list marker;
    va_start( marker, Format );
    {
         rc = wvsprintf( wszbuffer, Format, marker );
         OutputDebugString( TRACE_TAG );
         OutputDebugString( wszbuffer );
    }

    return rc;
}

/************************************************************
/       Unsupported entry points
/************************************************************/

DWORD APIENTRY
NPLogonNotify(
    PLUID   lpLogonId,
    LPCWSTR lpAuthentInfoType,
    LPVOID  lpAuthentInfo,
    LPCWSTR lpPreviousAuthentInfoType,
    LPVOID  lpPreviousAuthentInfo,
    LPWSTR  lpStationName,
    LPVOID  StationHandle,
    LPWSTR  *lpLogonScript)
{
    *lpLogonScript = NULL;
    return WN_SUCCESS;
}

DWORD APIENTRY
NPPasswordChangeNotify (
    LPCWSTR    lpAuthentInfoType,
    LPVOID    lpAuthentInfo,
    LPCWSTR    lpPreviousAuthentInfoType,
    LPVOID    lpPreviousAuthentInfo,
    LPWSTR    lpStationName,
    LPVOID    StationHandle,
    DWORD    dwChangeInfo )
{

    SetLastError( WN_NOT_SUPPORTED );
    return WN_NOT_SUPPORTED;
}

DWORD APIENTRY
NPGetResourceParent( LPNETRESOURCE   lpNetResource,
                     LPVOID  lpBuffer,
                     LPDWORD lpBufferSize )
{

    return WN_NOT_SUPPORTED;
}

DWORD APIENTRY
NPGetResourceInformation( LPNETRESOURCE   lpNetResource,
                          LPVOID  lpBuffer,
                          LPDWORD lpBufferSize,
                          LPWSTR  *lplpSystem )
{

    return WN_NOT_SUPPORTED;
}

DWORD APIENTRY
NPGetUniversalName( LPCWSTR lpLocalPath,
                    DWORD   dwInfoLevel,
                    LPVOID  lpBuffer,
                    LPDWORD lpBufferSize )
{

    return WN_NOT_SUPPORTED;
}

/************************************************************
/       END Unsupported entry points
/************************************************************/


HANDLE
OpenRedirector()
{

    HANDLE hControlDevice = NULL;
    WCHAR wchError[ 256];

    hControlDevice = CreateFile( AFS_SYMLINK_W,
	 			 		         GENERIC_READ | GENERIC_WRITE,
							     FILE_SHARE_READ | FILE_SHARE_WRITE,
							     NULL,
							     OPEN_EXISTING,
							     0,
							     NULL );

    if( hControlDevice == INVALID_HANDLE_VALUE)
    {

        hControlDevice = NULL;

        swprintf( wchError, L"Failed to open control device error: %d\n", GetLastError());

        OutputDebugString( wchError);
    }

    return hControlDevice;
}

