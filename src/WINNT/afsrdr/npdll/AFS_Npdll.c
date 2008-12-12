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
#include <winreg.h>
#include <winsvc.h>
#include <winnetwk.h>
#include <npapi.h>
#include <winioctl.h>

#ifndef WNNC_NET_OPENAFS
#define WNNC_NET_OPENAFS     0x00390000
#endif

#include "AFSUserCommon.h"
#include "AFSProvider.h"
#include "AFS_Npdll.h"

#include "stdio.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define SCRATCHSZ 1024

//
// Common information
//

//#define AFS_DEBUG_TRACE         1

ULONG _cdecl AFSDbgPrint( PWCHAR Format, ... );

#define WNNC_DRIVER( major, minor ) ( major * 0x00010000 + minor )

#define OPENAFS_PROVIDER_NAME           L"OpenAFS Network"
#define OPENAFS_PROVIDER_NAME_LENGTH    30

static ULONG cbProviderNameLength;

static wchar_t *szProviderName = NULL;

static BOOL bFreeProviderName = FALSE;

void
ReadProviderNameString( void)
{
    HKEY hk;
    DWORD code;
    DWORD dwLen = 0;

    code = RegOpenKeyExW( HKEY_LOCAL_MACHINE, 
                         L"SYSTEM\\CurrentControlSet\\Services\\AFSRedirector\\NetworkProvider",
                         0, KEY_QUERY_VALUE, &hk);

    if ( code == ERROR_SUCCESS) {

        code = RegQueryValueExW( hk, L"Name", NULL, NULL, NULL, &dwLen);

        if ( code == ERROR_SUCCESS || code == ERROR_MORE_DATA) {

            szProviderName = (wchar_t *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwLen + sizeof(wchar_t));

            code = RegQueryValueExW( hk, L"Name", NULL, NULL, 
                                     (LPBYTE) szProviderName, &dwLen);

            if ( code == ERROR_SUCCESS) 
            {
                cbProviderNameLength = dwLen;
            }
        }

        RegCloseKey( hk);
    }

    if ( szProviderName == NULL) {

        szProviderName = OPENAFS_PROVIDER_NAME;

        cbProviderNameLength = OPENAFS_PROVIDER_NAME_LENGTH;

        bFreeProviderName = FALSE;
    
    } else {

        bFreeProviderName = TRUE;
    }

}

void
FreeProviderNameString( void)
{
    if ( bFreeProviderName ) {

        HeapFree( GetProcessHeap(), 0, szProviderName);

        bFreeProviderName = FALSE;
    }

    szProviderName = NULL;

    cbProviderNameLength = 0;
}


DWORD
NPIsRedirectorStarted( void);

#define try_return(S) { S; goto try_exit; }

#define __Enter

#define NOTHING

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

HANDLE
OpenRedirector( void);

typedef struct _AFS_ENUM_CB
{

    DWORD       CurrentIndex;

    DWORD       Scope;

    DWORD       Type;

    WCHAR      *RemoteName;

} AFSEnumerationCB;

//
// This is the only function which must be exported, everything else is optional
//

DWORD 
APIENTRY
NPGetCaps( DWORD nIndex )
{

    DWORD rc = 0;

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

            rc = WNNC_ENUM_LOCAL | 
                        WNNC_ENUM_CONTEXT |
                        WNNC_ENUM_GLOBAL |
                        WNNC_ENUM_SHAREABLE;
            break;
        }

        case WNNC_START:
        {

            rc = 1;

            break;
        }

        case WNNC_DIALOG:
        {

            rc = WNNC_DLG_FORMATNETWORKNAME | 
                 WNNC_DLG_GETRESOURCEINFORMATION |
                 WNNC_DLG_GETRESOURCEPARENT;

            break;
        }

        case WNNC_USER:
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

            wchLocalName[0] = towupper(lpNetResource->lpLocalName[0]);
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

        pConnectCB->Type = lpNetResource->dwType;

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

            try_return( dwStatus = WN_NO_NETWORK);
        }



        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_ADD_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   &dwStatus,
                                   sizeof( DWORD),
                                   &dwCopyBytes,
                                   NULL);

        if( !dwError)
        {

            AFSDbgPrint( L"NPAddConnection3 Failed to add connection to file system %d\n", GetLastError());

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

                        AFSDbgPrint(L"NPAddConnection3 Failed to assign drive\n");

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

        if ( hControlDevice != NULL) 
        {
        
            CloseHandle( hControlDevice);
        }

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
    DWORD    dwCopyBytes = 0;
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    BOOL     bLocalName = TRUE;
    HANDLE   hControlDevice = NULL;

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

            try_return( dwStatus = WN_NO_NETWORK);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_CANCEL_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   &dwStatus,
                                   sizeof( DWORD),
                                   &dwCopyBytes,
                                   NULL);

        if( !dwError)
        {

            DWORD gle = GetLastError();

            AFSDbgPrint(L"NPCancelConnection Failed to cancel connection to file system - gle 0x%x\n", gle);

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

        if ( hControlDevice != NULL) 
        {
        
            CloseHandle( hControlDevice);
        }


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
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    HANDLE   hControlDevice = NULL;
    DWORD    dwPassedLength = *lpBufferSize;

    __Enter
    {

        if( lstrlen( lpLocalName) == 0)
        {

            AFSDbgPrint(L"NPGetConnection No local name\n");

            try_return( dwStatus = WN_BAD_LOCALNAME);
        }

        wchLocalName[0] = towupper(lpLocalName[0]);
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

            try_return( dwStatus = WN_NO_NETWORK);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_GET_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   pConnectCB,
                                   dwBufferSize,
                                   lpBufferSize,
                                   NULL);

        if( !dwError)
        {
            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPGetConnection Failed to get connection from file system for local %s gle 0x%x\n", 
                         wchLocalName, gle);

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

        if ( hControlDevice != NULL) 
        {
        
            CloseHandle( hControlDevice);
        }

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
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    HANDLE   hControlDevice = NULL;
    DWORD    dwPassedLength = 0;
    NETCONNECTINFOSTRUCT *pNetInfo = (NETCONNECTINFOSTRUCT *)lpBuffer;

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

        wchLocalName[0] = towupper(lpLocalName[0]);
        wchLocalName[1] = L':';
        wchLocalName[2] = L'\0';

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetConnection3 Requesting connection for %s\n", wchLocalName);
#endif

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

            try_return( dwStatus = WN_NO_NETWORK);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_GET_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   pConnectCB,
                                   dwBufferSize,
                                   lpBufferSize,
                                   NULL);

        if( !dwError)
        {

            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPGetConnection3 Failed to get connection from file system for local %s gle 0x%x\n", 
                         wchLocalName, gle);

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        pNetInfo->dwDelay = 0;

        pNetInfo->dwFlags = 0;

        pNetInfo->dwOptDataSize = 0;

        pNetInfo->dwSpeed = 1560;

        *lpBufferSize = sizeof( NETCONNECTINFOSTRUCT);

        dwStatus = WN_SUCCESS;
 
try_exit:

        if ( hControlDevice != NULL) 
        {
        
            CloseHandle( hControlDevice);
        }

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
    
    DWORD   dwStatus = WN_SUCCESS;
    AFSEnumerationCB *pEnumCB = NULL;

    *lphEnum = HeapAlloc( GetProcessHeap( ), HEAP_ZERO_MEMORY, sizeof( AFSEnumerationCB));

    if( *lphEnum == NULL)
    {

        return WN_OUT_OF_MEMORY;
    }

    pEnumCB = (AFSEnumerationCB *)*lphEnum;

    pEnumCB->CurrentIndex = 0;

    pEnumCB->Type = dwType;

    switch( dwScope )
    {
        case RESOURCE_CONNECTED:
        {

            pEnumCB->Scope = RESOURCE_CONNECTED;

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint(L"NPOpenEnum Processing (RESOURCE_CONNECTED)\n");
#endif

            break;
        }

        case RESOURCE_CONTEXT:
        {

            pEnumCB->Scope = RESOURCE_CONTEXT;

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint(L"NPOpenEnum Processing (RESOURCE_CONTEXT)\n");
#endif

            break;
        }

        case RESOURCE_GLOBALNET:
        {

            if( lpNetResource != NULL &&
                lpNetResource->lpRemoteName != NULL)
            {

                pEnumCB->RemoteName = (WCHAR *)HeapAlloc( GetProcessHeap( ), HEAP_ZERO_MEMORY, 0x1000);

                if( pEnumCB->RemoteName == NULL)
                {

                    dwStatus = WN_OUT_OF_MEMORY;
                    HeapFree( GetProcessHeap( ), 0, (PVOID) *lphEnum );
                    *lphEnum = NULL;
                }
                else
                {

                    wcscpy( pEnumCB->RemoteName,
                            lpNetResource->lpRemoteName);

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPOpenEnum Processing (RESOURCE_GLOBALNET) For remote name %s\n",
                             lpNetResource->lpRemoteName);
#endif

                }
            }
#ifdef AFS_DEBUG_TRACE
            else
            {

                AFSDbgPrint(L"NPOpenEnum Processing (RESOURCE_GLOBALNET)\n");
            }
#endif

            pEnumCB->Scope = RESOURCE_GLOBALNET;            
            
            break;
        }

        default:

            dwStatus  = WN_NOT_SUPPORTED;
            HeapFree( GetProcessHeap( ), 0, (PVOID) *lphEnum );
            *lphEnum = NULL;

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
    ULONG            EntriesRequested;
    ULONG            dwIndex;
    LPNETRESOURCE    pNetResource;
    ULONG            SpaceNeeded;
    ULONG            SpaceAvailable;
    PWCHAR           StringZone;
    AFSNetworkProviderConnectionCB *pConnectionCB = NULL;
    void            *pConnectionCBBase = NULL;
    DWORD            dwError = 0;
    UNICODE_STRING   uniRemoteName;
    HANDLE           hControlDevice = NULL;
    AFSEnumerationCB *pEnumCB = (AFSEnumerationCB *)hEnum;

    __Enter
    {

        if ( szProviderName == NULL )
        {
                
            ReadProviderNameString();
        }

        pNetResource = (LPNETRESOURCE) lpBuffer;
        SpaceAvailable = *lpBufferSize;
        EntriesRequested = *lpcCount;
        *lpcCount = EntriesCopied = 0;
        StringZone = (PWCHAR) ((char *)lpBuffer + *lpBufferSize);

#ifdef AFS_DEBUG_TRACE
        if( pEnumCB->RemoteName != NULL)
        {

            AFSDbgPrint( L"NPEnumResource Processing Remote name %s Type %d Index %d\n",
                     pEnumCB->RemoteName,
                     pEnumCB->Type,
                     pEnumCB->CurrentIndex);
        }
        else
        {

            AFSDbgPrint( L"NPEnumResource Processing Remote name (NULL) Type %d Index %d\n",
                     pEnumCB->Type,
                     pEnumCB->CurrentIndex);
        }
#endif

        pConnectionCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 0x1000);

        if( pConnectionCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        pConnectionCBBase = (void *)pConnectionCB;

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

            try_return( dwStatus = WN_NO_NETWORK);
        }

        //
        // Setup what we are going to ask for
        //

        pConnectionCB->Scope = pEnumCB->Scope;

        pConnectionCB->Type = pEnumCB->Type;

        pConnectionCB->CurrentIndex = pEnumCB->CurrentIndex;

        //
        // If this is a RESOURCE_GLOBALNET enumeration then pass down the remote name if
        // there is one
        //

        pConnectionCB->RemoteNameLength = 0;

        if( pEnumCB->Scope == RESOURCE_GLOBALNET &&
            pEnumCB->RemoteName != NULL)
        {

            pConnectionCB->RemoteNameLength = wcslen( pEnumCB->RemoteName) * sizeof( WCHAR);

            wcscpy( pConnectionCB->RemoteName,
                    pEnumCB->RemoteName);
        } 
        else if ( pEnumCB->Scope == RESOURCE_CONTEXT ) 
        {

            pConnectionCB->Scope = RESOURCE_GLOBALNET;
        }
        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_LIST_CONNECTIONS,
                                   pConnectionCB,
                                   0x1000,
                                   pConnectionCB,
                                   0x1000,
                                   &dwCopyBytes,
                                   NULL);

        if( !dwError)
        {

            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPEnumResource Failed to list connections from file system - gle 0x%x\n", 
                         gle);

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        if( dwCopyBytes == 0)
        {

            try_return( dwStatus = WN_NO_MORE_ENTRIES);
        }

        dwIndex = pEnumCB->CurrentIndex;

        while( EntriesCopied < EntriesRequested)
        {

            uniRemoteName.Length = (USHORT)pConnectionCB->RemoteNameLength;
            uniRemoteName.MaximumLength = uniRemoteName.Length;
            uniRemoteName.Buffer = pConnectionCB->RemoteName;

            // Determine the space needed for this entry...

            SpaceNeeded  = 0;
            
            if( pConnectionCB->LocalName != 0)
            {

                SpaceNeeded += 3 * sizeof(WCHAR);                // local name
            }

            SpaceNeeded += pConnectionCB->RemoteNameLength + sizeof( WCHAR);        // remote name

            if( pConnectionCB->CommentLength > 0)
            {
                
                SpaceNeeded += pConnectionCB->CommentLength + sizeof( WCHAR);           // comment
            }

            SpaceNeeded += cbProviderNameLength + sizeof( WCHAR);           // provider name

            if( SpaceNeeded + sizeof( NETRESOURCE) > SpaceAvailable)
            {

                if (EntriesCopied == 0) {
                    
                    dwStatus = WN_MORE_DATA;
            
                    *lpBufferSize = SpaceNeeded + sizeof( NETRESOURCE);

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPEnumResource Request MORE_DATA for entry %s Len %d\n",
                             &uniRemoteName,
                             *lpBufferSize);
#endif

                } else {

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPEnumResource Return SUCCESS but more entries Index %d\n",
                             dwIndex);
#endif

                    dwStatus = WN_SUCCESS;
                }     
                
                break;
            }

            SpaceAvailable -= (SpaceNeeded + sizeof( NETRESOURCE));

            pNetResource->dwScope       = pEnumCB->Scope;
            pNetResource->dwType        = pConnectionCB->Type;

            pNetResource->dwDisplayType = pConnectionCB->DisplayType;

            if ( pNetResource->dwType == RESOURCETYPE_ANY &&
                 pNetResource->dwDisplayType == RESOURCEDISPLAYTYPE_SHARE)
            {
                
                pNetResource->dwType = RESOURCETYPE_DISK;
            }
            pNetResource->dwUsage       = pConnectionCB->Usage;

            // setup string area at opposite end of buffer
            StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded);
                
            // copy local name
            if( pConnectionCB->LocalName != 0)
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

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPEnumResource Processing Entry %wZ\n",
                     &uniRemoteName);
#endif

            // copy remote name                    
            pNetResource->lpRemoteName = StringZone;
                    
            CopyMemory( StringZone, 
                        pConnectionCB->RemoteName, 
                        pConnectionCB->RemoteNameLength);

            StringZone += (pConnectionCB->RemoteNameLength / sizeof(WCHAR));

            *StringZone++ = L'\0';

            // copy comment
            if( pConnectionCB->CommentLength > 0)
            {

                pNetResource->lpComment = StringZone;

                CopyMemory( StringZone, 
                            (void *)((char *)pConnectionCB + pConnectionCB->CommentOffset), 
                            pConnectionCB->CommentLength);

                StringZone += (pConnectionCB->CommentLength / sizeof(WCHAR));

                *StringZone++ = L'\0';
            }
            else
            {

                pNetResource->lpComment = NULL;
            }

            // copy provider name
            pNetResource->lpProvider = StringZone;
            wcscpy( StringZone, szProviderName);

            StringZone += (cbProviderNameLength / sizeof( WCHAR));

            *StringZone = L'\0';

            // setup the new end of buffer
            StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded);

            EntriesCopied++;

            pNetResource++;

            dwIndex++;

            dwCopyBytes -= FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) + 
                           pConnectionCB->RemoteNameLength +
                           pConnectionCB->CommentLength;

            if( dwCopyBytes == 0)
            {

                dwStatus = WN_SUCCESS;

                break;
            }

            pConnectionCB = (AFSNetworkProviderConnectionCB *)((char *)pConnectionCB + 
                            FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                            pConnectionCB->RemoteNameLength +
                            pConnectionCB->CommentLength);
        }

        *lpcCount = EntriesCopied;

        // update entry index
        pEnumCB->CurrentIndex = dwIndex;

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPEnumResource Completed Count %d Index %d\n",
                 EntriesCopied,
                 dwIndex);
#endif

try_exit:

        if ( hControlDevice != NULL) 
        {
        
            CloseHandle( hControlDevice);
        }

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

    AFSEnumerationCB *pEnumCB = (AFSEnumerationCB *)hEnum;

    if( pEnumCB->RemoteName != NULL)
    {

        HeapFree( GetProcessHeap( ), 0, (PVOID) pEnumCB->RemoteName);
    }

    HeapFree( GetProcessHeap( ), 0, (PVOID) hEnum );

    return WN_SUCCESS;
}

ULONG
_cdecl
AFSDbgPrint(
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

#ifdef AFS_DEBUG_TRACE
    if( lpNetResource != NULL &&
        lpNetResource->lpRemoteName != NULL)
    {

        AFSDbgPrint( L"NPGetResourceParent For remote name %s\n", lpNetResource->lpRemoteName);
    }
    else
    {

        AFSDbgPrint( L"NPGetResourceParent No resource\n");
    }
#endif

    return WN_NOT_SUPPORTED;
}

DWORD APIENTRY
NPGetResourceInformation( LPNETRESOURCE   lpNetResource,
                          LPVOID  lpBuffer,
                          LPDWORD lpBufferSize,
                          LPWSTR  *lplpSystem )
{

    DWORD    dwStatus = WN_NOT_CONNECTED;
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    HANDLE   hControlDevice = NULL;
    NETRESOURCE *pNetResource = (NETRESOURCE *)lpBuffer;
    PWCHAR   pStringZone = NULL;
    UNICODE_STRING uniRemoteName;
    DWORD    ulRequiredLen = 0;

    __Enter
    {

        if( lpNetResource == NULL ||
            lpNetResource->lpRemoteName == NULL)
        {

            AFSDbgPrint( L"NPGetResourceInformation No resource name\n");

            try_return( dwStatus = WN_BAD_VALUE);
        }

        dwBufferSize = 0x1000;

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);

        if( pConnectCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        pConnectCB->RemoteNameLength = wcslen( lpNetResource->lpRemoteName) * sizeof( WCHAR);

        wcscpy( pConnectCB->RemoteName,
                lpNetResource->lpRemoteName);

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

            try_return( dwStatus = WN_NO_NETWORK);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_GET_CONNECTION_INFORMATION,
                                   pConnectCB,
                                   dwBufferSize,
                                   pConnectCB,
                                   dwBufferSize,
                                   lpBufferSize,
                                   NULL);

        if( !dwError)
        {

            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPGetResourceInformation Failed to get connection info from file system for local %S gle 0x%x\n", 
                         lpNetResource->lpRemoteName, gle);

            try_return( dwStatus = WN_BAD_NETNAME);
        }        

        uniRemoteName.Length = (USHORT)pConnectCB->RemoteNameLength;
        uniRemoteName.MaximumLength = uniRemoteName.Length;
        uniRemoteName.Buffer = pConnectCB->RemoteName;

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetResourceInformation For remote name %wZ\n", 
                                             &uniRemoteName);
#endif

        // Determine the space needed for this entry...

        ulRequiredLen = sizeof( NETRESOURCE);            
        
        ulRequiredLen += pConnectCB->RemoteNameLength + sizeof( WCHAR);        // remote name

#if 0
        // Never return the comment from a GetResourceInfo request
        if ( pConnectCB->CommentLength > 0)
        {

            ulRequiredLen += pConnectCB->CommentLength + sizeof( WCHAR);       // comment
        }
#endif
        ulRequiredLen += cbProviderNameLength + sizeof( WCHAR);                // provider name

        if( ulRequiredLen > *lpBufferSize)
        {
          
            *lpBufferSize = ulRequiredLen;

            try_return( dwStatus = WN_MORE_DATA);
        }

        pStringZone = (PWCHAR) ((char *)lpBuffer + sizeof( NETRESOURCE));

        pNetResource->dwScope       = RESOURCE_GLOBALNET;
        pNetResource->dwType        = pConnectCB->Type;

        pNetResource->dwDisplayType = pConnectCB->DisplayType;

        pNetResource->dwUsage       = pConnectCB->Usage;

        pNetResource->lpLocalName = NULL;

        // copy remote name                    
        pNetResource->lpRemoteName = pStringZone;
                    
        CopyMemory( pStringZone, 
                    pConnectCB->RemoteName, 
                    pConnectCB->RemoteNameLength);

        pStringZone += (pConnectCB->RemoteNameLength / sizeof(WCHAR));

#if 0
        // Never return the comment from a GetResourceInfo request
        if( pConnectCB->CommentLength > 0)
        {

            CopyMemory( pStringZone, 
                        (void *)((char *)pConnectCB + pConnectCB->CommentOffset), 
                        pConnectCB->CommentLength);

            pStringZone += (pConnectCB->CommentLength / sizeof(WCHAR));

            *pStringZone++ = L'\0';
        }
        else
#endif
        {

            pNetResource->lpComment = NULL;
        }

        // copy provider name
        pNetResource->lpProvider = pStringZone;
        wcscpy( pStringZone, szProviderName);

        pStringZone += (cbProviderNameLength / sizeof( WCHAR));

        *pStringZone++ = L'\0';

        pNetResource->lpComment = NULL;

        *lpBufferSize = ulRequiredLen;

        //
        // Check for the name pased to us for non-processed portion
        //

        if( uniRemoteName.Length < wcslen( lpNetResource->lpRemoteName) * sizeof( WCHAR))
        {

            *lplpSystem = (LPWSTR)((char *)lpNetResource->lpRemoteName + (uniRemoteName.Length/sizeof( WCHAR)));

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetResourceInformation For remote name %s returning non-process %s\n", 
                                                                        lpNetResource->lpRemoteName,
                                                                        lplpSystem);
#endif
        }
 
        dwStatus = WN_SUCCESS;

try_exit:

        if ( hControlDevice != NULL) 
        {
        
            CloseHandle( hControlDevice);
        }

        if( pConnectCB != NULL)
        {

            HeapFree( GetProcessHeap( ), 0, (PVOID) pConnectCB);
        }
    }

    return dwStatus;
}

DWORD APIENTRY
NPGetUniversalName( LPCWSTR lpLocalPath,
                    DWORD   dwInfoLevel,
                    LPVOID  lpBuffer,
                    LPDWORD lpBufferSize )
{

    return WN_NOT_SUPPORTED;
}

DWORD 
NPFormatNetworkName(
  LPTSTR  lpRemoteName,
  LPTSTR  lpFormattedName,
  LPDWORD lpnLength,
  DWORD dwFlags,
  DWORD dwAveCharPerLine
)
{

    DWORD dwLen = 0, dwCurrentLen = 0;
    LPTSTR pCurrentName = lpRemoteName;

    //
    // Walk back in the name until we hit a \
    //

    dwLen = wcslen( lpRemoteName);

    pCurrentName += (dwLen - 1);

    if ( pCurrentName[ 0] != L'\\') 
    {

        while( dwLen > 0)
        {

            if( pCurrentName[ 0] == L'\\')
            {

                pCurrentName++;

                break;
            }

            pCurrentName--;

            dwLen--;

            dwCurrentLen++;
        }
    }

    if( *lpnLength  < dwCurrentLen * sizeof( WCHAR))
    {

        *lpnLength = dwCurrentLen * sizeof( WCHAR);

        return WN_MORE_DATA;
    }

    *lpnLength = dwCurrentLen * sizeof( WCHAR);

    wcscpy( lpFormattedName,
            pCurrentName);
    
    return WN_SUCCESS;
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

        AFSDbgPrint( L"Failed to open control device error: %d\n", GetLastError());
    } 
#if 0
    //
    // only do this if you want network shares to fail to mount
    // when the file system is not yet ready
    //
    else {

        AFSDriverStatusRespCB   respCB;
        DWORD                   dwBytes;

        memset( &respCB, '\0', sizeof( AFSDriverStatusRespCB));

        if ( !DeviceIoControl( hControlDevice,
                               IOCTL_AFS_STATUS_REQUEST,
                               NULL,
                               0,
                               (void *)&respCB,
                               sizeof( AFSDriverStatusRespCB),
                               &dwBytes,
                               NULL) ||
             dwBytes != sizeof(AFSDriverStatusRespCB) ||
             respCB.Status != AFS_DRIVER_STATUS_READY )
        {
                
            CloseHandle( hControlDevice);

            hControlDevice = NULL;
        }
    }
#endif

    return hControlDevice;
}

