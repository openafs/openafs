
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

#define OPENAFS_PROVIDER_NAME   L"OpenAFS Provider"

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
            (lpNetResource->dwType != RESOURCETYPE_DISK))
        {

            OutputDebugString(L"NPAddConnection3 Bad type\n");

            return WN_BAD_NETNAME;
        }

        if( _wcsnicmp( L"\\\\AFS", lpNetResource->lpRemoteName, 5) != 0)
        {

            swprintf( wszScratch, L"NPAddConnection3 Bad remote name %s\n", lpNetResource->lpRemoteName);

            OutputDebugString( wszScratch);

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

            OutputDebugString(L"NPAddConnection3 No memory\n");

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

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

            OutputDebugString(L"NPAddConnection3 Failed to open control device\n");

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

                    OutputDebugString(L"NPAddConnection3 Already assigned (2)\n");

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

                OutputDebugString(L"NPAddConnection3 Already assigned (1)\n");

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

        swprintf( wchOutputBuffer, L"NPCancelConnection Entry for %s\n", lpName);

        OutputDebugString( wchOutputBuffer);

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

                swprintf( wszScratch, L"NPCancelConnection Failed to get remote name for %S\n", lpName);

                OutputDebugString( wszScratch);

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

            OutputDebugString(L"NPCancelConnection Failed to open control device\n");

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        swprintf( wchOutputBuffer, L"NPCancelConnection Cancelling for %s\n", wchRemoteName);

        OutputDebugString( wchOutputBuffer);

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
        else if( dwStatus != WN_SUCCESS)
        {

            swprintf( wszScratch, L"NPCancelConnection Failed Redirector call %08lX\n", dwStatus);

            OutputDebugString( wszScratch);
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

        if( lstrlen( lpLocalName) == 0 ||
            *lpBufferSize == 0)
        {

            try_return( dwStatus);
        }

        if ( lpLocalName[1] == L':' )
        {

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

                OutputDebugString(L"NPAddConnection3 Failed to open control device\n");

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

            if( *lpBufferSize > dwPassedLength)
            {

                swprintf( wchOutputBuffer, L"NPGetConnection Buffer too small for local %s\n", wchLocalName);

                OutputDebugString( wchOutputBuffer);

                try_return( dwStatus = ERROR_BUFFER_OVERFLOW);
            }

            memcpy( lpRemoteName,
                    (void *)pConnectCB,
                    *lpBufferSize);

            lpRemoteName[ *lpBufferSize/sizeof( WCHAR)] = L'\0';

            dwStatus = WN_SUCCESS;
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

                OutputDebugString(L"NPOpenEnum Failed to allocate heap buffer\n");
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

            OutputDebugString(L"NPAddConnection3 Failed to open control device\n");

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

        uniLocalName.Length = 1;
        uniLocalName.MaximumLength = 1;
        uniLocalName.Buffer = &pConnectionCB->LocalName;

        uniRemoteName.Length = pConnectionCB->RemoteNameLength;
        uniRemoteName.MaximumLength = uniRemoteName.Length;
        uniRemoteName.Buffer = pConnectionCB->RemoteName;

        //
        // Get to where we need to be in the lsit
        //

        dwIndex = *((PULONG)hEnum);

        if( dwIndex > 0)
        {

            while( dwCopyBytes > 0)
            {

                dwCopyBytes -= (FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) + pConnectionCB->RemoteNameLength);

                uniLocalName.Length = 1;
                uniLocalName.MaximumLength = 1;
                uniLocalName.Buffer = &pConnectionCB->LocalName;

                uniRemoteName.Length = pConnectionCB->RemoteNameLength;
                uniRemoteName.MaximumLength = uniRemoteName.Length;
                uniRemoteName.Buffer = pConnectionCB->RemoteName;

                pConnectionCB = (AFSNetworkProviderConnectionCB *)((char *)pConnectionCB + 
                                                                FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) +
                                                                pConnectionCB->RemoteNameLength);

                dwIndex--;

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

            if( pConnectionCB->LocalName == 0)
            {

                dwStatus = WN_SUCCESS;

                break;
            }

            uniLocalName.Length = 1;
            uniLocalName.MaximumLength = 1;
            uniLocalName.Buffer = &pConnectionCB->LocalName;

            uniRemoteName.Length = pConnectionCB->RemoteNameLength;
            uniRemoteName.MaximumLength = uniRemoteName.Length;
            uniRemoteName.Buffer = pConnectionCB->RemoteName;
                            
            // Determine the space needed for this entry...

            SpaceNeeded  = sizeof( NETRESOURCE );            // resource struct
            SpaceNeeded += 3 * sizeof(WCHAR);                // local name
            SpaceNeeded += 2 * sizeof(WCHAR) + pConnectionCB->RemoteNameLength;    // remote name
            SpaceNeeded += 5 * sizeof(WCHAR);                // comment
            SpaceNeeded += sizeof( OPENAFS_PROVIDER_NAME);    // provider name

            if( SpaceNeeded > SpaceAvailable)
            {

                swprintf( wchOutputBuffer, L"NPEnumResource No space Needed %d Available %d\n", SpaceNeeded, SpaceAvailable);

                OutputDebugString( wchOutputBuffer);

                dwStatus = WN_MORE_DATA;
            
                *lpBufferSize = SpaceNeeded;
                
                break;
            }

            SpaceAvailable -= SpaceNeeded;

            pNetResource->dwScope       = RESOURCE_CONNECTED;
            pNetResource->dwType        = RESOURCETYPE_DISK;
            pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
            pNetResource->dwUsage       = RESOURCEUSAGE_CONNECTABLE | RESOURCEUSAGE_ATTACHED;

            // setup string area at opposite end of buffer
            StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded );
                
            // copy local name
            pNetResource->lpLocalName = StringZone;
            *StringZone++ = pConnectionCB->LocalName;
            *StringZone++ = L':';
            *StringZone++ = L'\0';

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

            EntriesCopied++;

            // set new bottom of string zone
            StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded );

            pNetResource++;

            dwIndex++;

            dwCopyBytes -= (FIELD_OFFSET( AFSNetworkProviderConnectionCB, RemoteName) + pConnectionCB->RemoteNameLength);

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

