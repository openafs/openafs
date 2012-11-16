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
#include <strsafe.h>

#define AFS_DEBUG_TRACE 1

#ifndef WNNC_NET_OPENAFS
#define WNNC_NET_OPENAFS     0x00390000
#endif

#include "AFSUserDefines.h"
#include "AFSUserIoctl.h"
#include "AFSUserStructs.h"
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

ULONG _cdecl AFSDbgPrint( PWCHAR Format, ... );

static DWORD APIENTRY
NPGetConnectionCommon( LPWSTR  lpLocalName,
                       LPWSTR  lpRemoteName,
                       LPDWORD lpBufferSize,
                       BOOL    bDriveSubstOk);

static DWORD APIENTRY
NPGetConnection3Common( LPCWSTR lpLocalName,
                        DWORD dwLevel,
                        LPVOID lpBuffer,
                        LPDWORD lpBufferSize,
                        BOOL bDriveSubstOk);

static DWORD APIENTRY
NPGetUniversalNameCommon( LPCWSTR lpLocalPath,
                          DWORD   dwInfoLevel,
                          LPVOID  lpBuffer,
                          LPDWORD lpBufferSize,
                          BOOL    bDriveSubstOk);

static BOOL
DriveSubstitution( LPCWSTR drivestr,
                   LPWSTR subststr,
                   size_t substlen,
                   DWORD * pStatus);

#define WNNC_DRIVER( major, minor ) ( major * 0x00010000 + minor )

#define OPENAFS_PROVIDER_NAME           L"OpenAFS Network"
#define OPENAFS_PROVIDER_NAME_LENGTH    30

#define MAX_PROVIDER_NAME_LENGTH        256

static ULONG cbProviderNameLength = OPENAFS_PROVIDER_NAME_LENGTH;

static wchar_t wszProviderName[MAX_PROVIDER_NAME_LENGTH+1] = OPENAFS_PROVIDER_NAME;

static BOOL bProviderNameRead = FALSE;

#define OPENAFS_SERVER_NAME             L"AFS"
#define OPENAFS_SERVER_NAME_LENGTH      6

#define OPENAFS_SERVER_COMMENT          L"AFS Root"
#define OPENAFS_SERVER_COMMENT_LENGTH   16

#define MAX_SERVER_NAME_LENGTH         30

static ULONG cbServerNameLength = 0;

static ULONG cbServerNameUNCLength = 0;

static ULONG cbServerCommentLength = OPENAFS_SERVER_COMMENT_LENGTH;

static wchar_t wszServerName[MAX_SERVER_NAME_LENGTH+1];

static wchar_t wszServerNameUNC[MAX_SERVER_NAME_LENGTH+3];

static wchar_t wszServerComment[] = OPENAFS_SERVER_COMMENT;

static BOOL bServerNameRead = FALSE;

LARGE_INTEGER
AFSRetrieveAuthId( void);

void
ReadProviderNameString( void)
{
    HKEY hk;
    DWORD code;
    DWORD dwLen = 0;

    if ( bProviderNameRead )
        return;

    code = RegOpenKeyExW( HKEY_LOCAL_MACHINE,
                         L"SYSTEM\\CurrentControlSet\\Services\\AFSRedirector\\NetworkProvider",
                         0, KEY_QUERY_VALUE, &hk);

    if ( code == ERROR_SUCCESS) {

        dwLen = sizeof(wszProviderName);

        code = RegQueryValueExW( hk, L"Name", NULL, NULL,
                                 (LPBYTE) wszProviderName, &dwLen);

        if ( code == ERROR_SUCCESS)
        {

            wszProviderName[MAX_PROVIDER_NAME_LENGTH] = '\0';

            cbProviderNameLength = wcslen( wszProviderName) * sizeof( WCHAR);
        }

        RegCloseKey( hk);
    }

    bProviderNameRead = TRUE;
}

void
ReadServerNameString( void)
{
    HKEY hk;
    DWORD code;
    DWORD dwLen = 0;

    if ( bServerNameRead )
        return;

    code = RegOpenKeyExW( HKEY_LOCAL_MACHINE,
                         L"SYSTEM\\CurrentControlSet\\Services\\TransarcAFSDaemon\\Parameters",
                         0, KEY_QUERY_VALUE, &hk);

    if ( code == ERROR_SUCCESS) {

        dwLen = sizeof(wszProviderName);

        code = RegQueryValueExW( hk, L"NetbiosName", NULL, NULL,
                                 (LPBYTE) wszServerName, &dwLen);

        if ( code == ERROR_SUCCESS)
        {

            wszServerName[MAX_SERVER_NAME_LENGTH] = '\0';

            cbServerNameLength = wcslen( wszServerName) * sizeof( WCHAR);

            wszServerNameUNC[0] = wszServerNameUNC[1] = L'\\';

            memcpy(&wszServerNameUNC[2], wszServerName, (cbServerNameLength + 1) * sizeof( WCHAR));

            cbServerNameUNCLength = cbServerNameLength + 2 * sizeof( WCHAR);
        }

        RegCloseKey( hk);
    }

    bServerNameRead = TRUE;
}



/* returns TRUE if the file system is disabled or not installed */
BOOL
NPIsFSDisabled( void)
{
    HKEY hk;
    DWORD code;
    DWORD dwLen = 0;
    DWORD dwStart = SERVICE_DISABLED;

    code = RegOpenKeyExW( HKEY_LOCAL_MACHINE,
                         L"SYSTEM\\CurrentControlSet\\Services\\AFSRedirector",
                         0, KEY_QUERY_VALUE, &hk);

    if ( code != ERROR_SUCCESS)

        return TRUE;


    dwLen = sizeof(dwStart);

    code = RegQueryValueExW( hk, L"Start", NULL, NULL,
                             (LPBYTE) &dwStart, &dwLen);

    RegCloseKey( hk);

    return ( dwStart == SERVICE_DISABLED);
}


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
// Recursively evaluate drivestr to find the final
// dos drive letter to which the source is mapped.
//
static BOOL
DriveSubstitution(LPCWSTR drivestr, LPWSTR subststr, size_t substlen, DWORD * pStatus)
{
    WCHAR drive[3];
    WCHAR device[MAX_PATH + 26];
    HRESULT hr = S_OK;

    *pStatus = WN_SUCCESS;

    memset( subststr, 0, substlen);
    drive[0] = drivestr[0];
    drive[1] = drivestr[1];
    drive[2] = 0;

    if ( substlen < 3 * sizeof( WCHAR))
    {
        //
        // Cannot represent "D:"
        //
        return FALSE;
    }

    if ( QueryDosDevice(drive, device, MAX_PATH + 26) )
    {
#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"DriveSubstitution QueryDosDevice %s [%s -> %s]\n",
                     drivestr,
                     drive,
                     device);
#endif
        if ( device[0] == L'\\' &&
             device[1] == L'?' &&
             device[2] == L'?' &&
             device[3] == L'\\' &&
             iswalpha(device[4]) &&
             device[5] == L':')
        {
            drive[0] = device[4];
            drive[1] = L':';
            drive[2] = L'\0';

            if ( !DriveSubstitution(drive, subststr, substlen, pStatus) )
            {

                subststr[0] = drive[0];
                subststr[1] = L':';
                subststr[2] = L'\0';
            }

            hr = S_OK;

            if ( device[6] )
            {
                hr = StringCbCat( subststr, substlen, &device[6]);
            }
            if ( SUCCEEDED(hr) && drivestr[2] )
            {
                hr = StringCbCat( subststr, substlen, &drivestr[2]);
            }

            if ( SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER)
            {

                if ( hr == STRSAFE_E_INSUFFICIENT_BUFFER )
                    *pStatus = WN_MORE_DATA;

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"DriveSubstitution (hr = %X) %s -> %s\n",
                                 hr,
                                 drivestr,
                                 subststr);
#endif
                return TRUE;
            }
        }
        else if ( device[0] == L'\\' &&
                  device[1] == L'?' &&
                  device[2] == L'?' &&
                  device[3] == L'\\' &&
                  device[4] == L'U' &&
                  device[5] == L'N' &&
                  device[6] == L'C' &&
                  device[7] == L'\\')
        {

            subststr[0] = L'\\';

            hr = StringCbCopyN(&subststr[1], substlen - sizeof(WCHAR), &device[7], sizeof(device) - 7 * sizeof(WCHAR));

            if ( SUCCEEDED(hr) && drivestr[2] )
            {
                hr = StringCbCat( subststr, substlen, &drivestr[2]);
            }

            if ( SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER)
            {

                if ( hr == STRSAFE_E_INSUFFICIENT_BUFFER )
                    *pStatus = WN_MORE_DATA;

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"DriveSubstitution (hr = %X) %s -> %s\n",
                                 hr,
                                 drivestr,
                                 subststr);
#endif
                return TRUE;
            }

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"DriveSubstitution StringCbCopyN 1 hr 0x%X\n",
                         hr);
#endif
        }
        else if ( _wcsnicmp( AFS_RDR_DEVICE_NAME, device, sizeof( AFS_RDR_DEVICE_NAME) / sizeof( WCHAR) - 1) == 0)
        {
            //
            // \Device\AFSRedirector\;X:\\afs\cellname
            //

            hr = StringCbCopy( subststr, substlen,
                               &device[3 + sizeof( AFS_RDR_DEVICE_NAME) / sizeof( WCHAR)]);

            if ( SUCCEEDED(hr) && drivestr[2] )
            {
                hr = StringCbCat( subststr, substlen, &drivestr[2]);
            }

            if ( SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER)
            {

                if ( hr == STRSAFE_E_INSUFFICIENT_BUFFER )
                    *pStatus = WN_MORE_DATA;

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"DriveSubstitution (hr = %X) %s -> %s\n",
                             hr,
                             drivestr,
                             subststr);
#endif
                return TRUE;
            }

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"DriveSubstitution StringCbCopyN 2 hr 0x%X\n",
                         hr);
#endif
        }

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"DriveSubstitution no substitution or match %s !! %s\n",
                     drivestr,
                     device);
#endif
    }
    else
    {
#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"DriveSubstitution QueryDosDevice failed %s gle 0x%X\n",
                     drivestr,
                     GetLastError());
#endif
    }

    return FALSE;
}


static const WCHAR *
NPGetCapsQueryString( DWORD nIndex)
{
    switch ( nIndex) {
    case WNNC_SPEC_VERSION:
        return L"WNNC_SPEC_VERSION";

    case WNNC_NET_TYPE:
        return L"WNNC_NET_TYPE";

    case WNNC_DRIVER_VERSION:
        return L"WNNC_DRIVER_VERSION";

    case WNNC_USER:
        return L"WNNC_USER";

    case WNNC_CONNECTION:
        return L"WNNC_CONNECTION";

    case WNNC_DIALOG:
        return L"WNNC_DIALOG";

    case WNNC_ADMIN:
        return L"WNNC_ADMIN";

    case WNNC_ENUMERATION:
        return L"WNNC_ENUMERATION";

    case WNNC_START:
        return L"WNNC_START";

    case WNNC_CONNECTION_FLAGS:
        return L"WNNC_CONNECTION_FLAGS";

    default:
        return L"UNKNOWN";
    }
}

//
// This is the only function which must be exported, everything else is optional
//

DWORD
APIENTRY
NPGetCaps( DWORD nIndex )
{

    DWORD rc = 0;

#ifdef AFS_DEBUG_TRACE
    AFSDbgPrint( L"NPGetCaps Index %u %s\n", nIndex,
                 NPGetCapsQueryString( nIndex));
#endif
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

            //
            // No support for:
            //   WNNC_CON_DEFER
            //

            rc = WNNC_CON_GETCONNECTIONS |
                 WNNC_CON_CANCELCONNECTION |
                 WNNC_CON_ADDCONNECTION |
                 WNNC_CON_ADDCONNECTION3 |
                 WNNC_CON_GETPERFORMANCE;

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

            rc = WNNC_WAIT_FOR_START;

            break;
        }

        case WNNC_DIALOG:
        {

            //
            // No support for:
            //    WNNC_DLG_DEVICEMODE
            //    WNNC_DLG_PROPERTYDIALOG
            //    WNNC_DLG_SEARCHDIALOG
            //    WNNC_DLG_PERMISSIONEDITOR
            //

            rc = WNNC_DLG_FORMATNETWORKNAME |
                 WNNC_DLG_GETRESOURCEINFORMATION |
                 WNNC_DLG_GETRESOURCEPARENT;

            break;
        }

        case WNNC_USER:
        {
            //
            // No support for:
            //    WNNC_USR_GETUSER
            //

            break;
        }

        case WNNC_ADMIN:
        {
            //
            // No support for:
            //    WNNC_ADM_GETDIRECTORYTYPE
            //    WNNC_ADM_DIRECTORYNOTIFY
            // used by the old File Manager
            //
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

#ifdef AFS_DEBUG_TRACE
    AFSDbgPrint( L"NPAddConnection forwarding to NPAddConnection3\n");
#endif
    return NPAddConnection3( NULL, lpNetResource, lpPassword, lpUserName, 0 );
}

static void
Add3FlagsToString( DWORD dwFlags, WCHAR *wszBuffer, size_t wch)
{
    HRESULT  hr;
    int first = 1;

    *wszBuffer = L'\0';

    if (dwFlags & CONNECT_TEMPORARY) {

        hr = StringCbCat( wszBuffer, wch, L"TEMPORARY");

        if ( FAILED(hr)) {

            return;
        }

        first = 0;
    }

    if (dwFlags & CONNECT_INTERACTIVE) {

        if (!first) {

            hr = StringCbCat( wszBuffer, wch, L"|");

            if ( FAILED(hr)) {

                return;
            }
        }

        hr = StringCbCat( wszBuffer, wch, L"INTERACTIVE");

        if ( FAILED(hr)) {

            return;
        }

        first = 0;
    }

    if (dwFlags & CONNECT_PROMPT) {

        if (!first) {

            hr = StringCbCat( wszBuffer, wch, L"|");

            if ( FAILED(hr)) {

                return;
            }
        }

        hr = StringCbCat( wszBuffer, wch, L"PROMPT");

        if ( FAILED(hr)) {

            return;
        }

        first = 0;
    }

    if (dwFlags & CONNECT_INTERACTIVE) {

        if (!first) {

            hr = StringCbCat( wszBuffer, wch, L"|");

            if ( FAILED(hr)) {

                return;
            }
        }

        hr = StringCbCat( wszBuffer, wch, L"DEFERRED");

        if ( FAILED(hr)) {

            return;
        }

        first = 0;
    }
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
    HANDLE   hToken = NULL;
    LARGE_INTEGER liAuthId = {0,0};
    HRESULT  hr;
    WCHAR    wszFlagsString[1024]=L"";

    __Enter
    {
        if ( NPIsFSDisabled())
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPAddConnection3 AFSRDFS is disabled, returning WN_BAD_NETNAME\n");
#endif

            return WN_BAD_NETNAME;
        }

        if ((lpNetResource->lpRemoteName == NULL) ||
            (lpNetResource->lpRemoteName[0] != L'\\') ||
            (lpNetResource->lpRemoteName[1] != L'\\') ||
            ((lpNetResource->dwType != RESOURCETYPE_DISK) &&
             (lpNetResource->dwType != RESOURCETYPE_ANY)))
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPAddConnection3 invalid input, returning WN_BAD_NETNAME\n");
#endif
            return WN_BAD_NETNAME;
        }

#ifdef AFS_DEBUG_TRACE
        Add3FlagsToString( dwFlags, wszFlagsString, 1024);

        AFSDbgPrint( L"NPAddConnection3 processing Remote %s User %s Pass %s Flags %s\n",
                     lpNetResource->lpRemoteName,
                     lpUserName == NULL? L"use-default": lpUserName[0] ? lpUserName : L"no-username",
                     lpPassword == NULL? L"use-default": lpPassword[0] ? L"provided" : L"no-password",
                     wszFlagsString);
#endif
        if( lpNetResource->lpLocalName != NULL)
        {

            wchLocalName[0] = towupper(lpNetResource->lpLocalName[0]);
            wchLocalName[1] = L':';
            wchLocalName[2] = L'\0';
        }

        hr = StringCbCopy(wchRemoteName, sizeof( wchRemoteName), lpNetResource->lpRemoteName);
        if ( FAILED(hr))
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPAddConnection3 lpRemoteName longer than MAX_PATH, returning WN_BAD_NETNAME\n");
#endif
            return WN_BAD_NETNAME;
        }

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

            pConnectCB->LocalName = towupper(wchLocalName[0]);

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPAddConnection3 Adding mapping for drive %s remote name %s\n",
                         wchLocalName,
                         wchRemoteName);
#endif
        }
        else
        {

            pConnectCB->LocalName = L'\0';

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPAddConnection3 Adding mapping for NO drive remote name %s\n",
                         wchRemoteName);
#endif
        }

        pConnectCB->Version = AFS_NETWORKPROVIDER_INTERFACE_VERSION_1;

        pConnectCB->RemoteNameLength = wcslen( wchRemoteName) * sizeof( WCHAR);

        memcpy( pConnectCB->RemoteName,
                wchRemoteName,
                pConnectCB->RemoteNameLength);

        pConnectCB->Type = lpNetResource->dwType;

        pConnectCB->AuthenticationId = AFSRetrieveAuthId();

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPAddConnection3 Retrieved authentication id %08lX-%08lX\n",
                     pConnectCB->AuthenticationId.HighPart,
                     pConnectCB->AuthenticationId.LowPart);
#endif

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPAddConnection3 OpenRedirector failure, returning WN_NET_ERROR\n");
#endif

            try_return( dwStatus = WN_NET_ERROR);
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
#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPAddConnection3 Failed to add connection to file system %d\n", GetLastError());
#endif
            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        //
        // The status returned from the driver will indicate how it was handled
        //

        if( dwStatus == WN_SUCCESS &&
            lpNetResource->lpLocalName != NULL)
        {

            WCHAR TempBuf[MAX_PATH+26];

            if( !QueryDosDeviceW( wchLocalName,
                                  TempBuf,
                                  MAX_PATH+26))
            {

                if( GetLastError() != ERROR_FILE_NOT_FOUND)
                {

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPAddConnection3 QueryDosDeviceW failed with file not found\n");
#endif
                    NPCancelConnection( wchLocalName, TRUE);

                    dwStatus = ERROR_ALREADY_ASSIGNED;
                }
                else
                {

                    UNICODE_STRING uniConnectionName;

                    //
                    // Create a symbolic link object to the device we are redirecting
                    //

                    uniConnectionName.MaximumLength = (USHORT)( wcslen( AFS_RDR_DEVICE_NAME) * sizeof( WCHAR) +
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

                    hr = StringCbCopyW( uniConnectionName.Buffer,
                                        uniConnectionName.MaximumLength,
                                        AFS_RDR_DEVICE_NAME);
                    if ( FAILED(hr))
                    {
#ifdef AFS_DEBUG_TRACE
                        AFSDbgPrint( L"NPAddConnection3 uniConnectionBuffer too small\n");
#endif
                        try_return( dwStatus = WN_OUT_OF_MEMORY);
                    }

                    hr = StringCbCatW( uniConnectionName.Buffer,
                                       uniConnectionName.MaximumLength,
                                       L"\\;" );
                    if ( FAILED(hr))
                    {
#ifdef AFS_DEBUG_TRACE
                        AFSDbgPrint( L"NPAddConnection3 uniConnectionBuffer too small\n");
#endif
                        try_return( dwStatus = WN_OUT_OF_MEMORY);
                    }

                    hr = StringCbCatW( uniConnectionName.Buffer,
                                       uniConnectionName.MaximumLength,
                                       wchLocalName);
                    if ( FAILED(hr))
                    {
#ifdef AFS_DEBUG_TRACE
                        AFSDbgPrint( L"NPAddConnection3 uniConnectionBuffer too small\n");
#endif
                        try_return( dwStatus = WN_OUT_OF_MEMORY);
                    }

                    hr = StringCbCatW( uniConnectionName.Buffer,
                                       uniConnectionName.MaximumLength,
                                       wchRemoteName);
                    if ( FAILED(hr))
                    {
#ifdef AFS_DEBUG_TRACE
                        AFSDbgPrint( L"NPAddConnection3 uniConnectionBuffer too small\n");
#endif
                        try_return( dwStatus = WN_OUT_OF_MEMORY);
                    }

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPAddConnection3 DefineDosDevice Local %s connection name %s\n",
                                 wchLocalName,
                                 uniConnectionName.Buffer);
#endif

                    if( !DefineDosDeviceW( DDD_RAW_TARGET_PATH |
                                           DDD_NO_BROADCAST_SYSTEM,
                                           wchLocalName,
                                           uniConnectionName.Buffer))
                    {
#ifdef AFS_DEBUG_TRACE
                        AFSDbgPrint( L"NPAddConnection3 Failed to assign drive\n");
#endif
                        dwStatus = GetLastError();
                    }
                    else
                    {

#ifdef AFS_DEBUG_TRACE
                        AFSDbgPrint( L"NPAddConnection3 QueryDosDeviceW assigned drive %s\n", wchLocalName);
#endif

                        dwStatus = WN_SUCCESS;
                    }

                    LocalFree( uniConnectionName.Buffer);
                }
            }
            else
            {

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPAddConnection3 QueryDosDeviceW %Z already existed\n", TempBuf);
#endif
                NPCancelConnection( wchLocalName, TRUE);

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
    AFSCancelConnectionResultCB stCancelConn;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    BOOL     bLocalName = TRUE;
    HANDLE   hControlDevice = NULL;
    WCHAR    wchLocalName[ 3];
    WCHAR   *pwchLocalName = NULL;
    HRESULT hr;

    __Enter
    {

        if ( NPIsFSDisabled())
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPCancelConnection AFSRDFS is disabled, returning WN_NOT_CONNECTED\n");
#endif

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        if( *lpName == L'\\' &&
            *(lpName + 1) == L'\\')
        {

            bLocalName = FALSE;

            wchLocalName[0] = L'\0';

            hr = StringCbCopyW( wchRemoteName, sizeof( wchRemoteName), lpName);

            if ( FAILED(hr))
            {
#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPCancelConnection lpName longer than MAX_PATH\n");
#endif
                try_return( dwStatus = WN_OUT_OF_MEMORY);
            }

            dwRemoteNameLength = (wcslen( wchRemoteName) * sizeof( WCHAR));
        }
        else
        {

            wchLocalName[0] = towupper(lpName[0]);
            wchLocalName[1] = L':';
            wchLocalName[2] = L'\0';

            //
            // Get the remote name for the connection, if we are handling it
            //

            dwStatus = NPGetConnectionCommon( wchLocalName,
                                              wchRemoteName,
                                              &dwRemoteNameLength,
                                              FALSE);

            if( dwStatus != WN_SUCCESS ||
                dwRemoteNameLength == 0)
            {

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPCancelConnection Status 0x%x NameLength %u, returning WN_NOT_CONNECTED\n",
                             dwStatus, dwRemoteNameLength);
#endif
                try_return( dwStatus = WN_NOT_CONNECTED);
            }

            //
            // NPGetConnection returns the buffer size not the length without NUL
            //
            dwRemoteNameLength -= sizeof( WCHAR);
        }

        wchRemoteName[ dwRemoteNameLength/sizeof( WCHAR)] = L'\0';

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPCancelConnection Attempting to cancel '%s' -> '%s'\n",
                     wchLocalName, wchRemoteName);
#endif

        dwBufferSize = sizeof( AFSNetworkProviderConnectionCB) + dwRemoteNameLength;

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);

        if( pConnectCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        if( bLocalName)
        {

            pConnectCB->LocalName = wchLocalName[0];
        }
        else
        {

            pConnectCB->LocalName = L'\0';
        }

        pConnectCB->RemoteNameLength = (USHORT)dwRemoteNameLength;

        StringCbCopyW( pConnectCB->RemoteName,
                       dwRemoteNameLength + sizeof( WCHAR),
                       wchRemoteName);

        pConnectCB->Version = AFS_NETWORKPROVIDER_INTERFACE_VERSION_1;

        pConnectCB->AuthenticationId = AFSRetrieveAuthId();

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPCancelConnection Retrieved authentication id %08lX-%08lX\n",
                     pConnectCB->AuthenticationId.HighPart,
                     pConnectCB->AuthenticationId.LowPart);
#endif

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPCancelConnection OpenRedirector failure, returning WN_NET_ERROR\n");
#endif

            try_return( dwStatus = WN_NET_ERROR);
        }

        memset( &stCancelConn,
                '\0',
                sizeof( AFSCancelConnectionResultCB));

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_CANCEL_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   &stCancelConn,
                                   sizeof( AFSCancelConnectionResultCB),
                                   &dwCopyBytes,
                                   NULL);

        if( !dwError)
        {
#ifdef AFS_DEBUG_TRACE
            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPCancelConnection DeviceIoControl failed - gle 0x%x\n", gle);
#endif
            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        dwStatus = stCancelConn.Status;

#ifdef AFS_DEBUG_TRACE
        if ( dwStatus == WN_NOT_CONNECTED )
        {

            AFSDbgPrint( L"NPCancelConnection Cancel connection to file system - Name %s Status WN_NOT_CONNECTED\n",
                         lpName);
        }
        else
        {

            AFSDbgPrint( L"NPCancelConnection Cancel connection to file system - Name %s Status %08lX\n",
                         lpName,
                         dwStatus);
        }
#endif

        if( dwStatus == WN_SUCCESS &&
            ( bLocalName ||
              stCancelConn.LocalName != L'\0'))
        {

            UNICODE_STRING uniConnectionName;

            //
            // Create a symbolic link object to the device we are redirecting
            //

            uniConnectionName.MaximumLength = (USHORT)( wcslen( AFS_RDR_DEVICE_NAME) * sizeof( WCHAR) +
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

            hr = StringCbCopyW( uniConnectionName.Buffer,
                                uniConnectionName.MaximumLength,
                                AFS_RDR_DEVICE_NAME);

            if ( FAILED(hr))
            {
#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPCancelConnection uniConnectionName buffer too small (1)\n");
#endif
                try_return( dwStatus = WN_OUT_OF_MEMORY);
            }

            hr = StringCbCatW( uniConnectionName.Buffer,
                               uniConnectionName.MaximumLength,
                               L"\\;" );

            if ( FAILED(hr))
            {
#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPCancelConnection uniConnectionName buffer too small (2)\n");
#endif
                try_return( dwStatus = WN_OUT_OF_MEMORY);
            }

            if( !bLocalName)
            {

                wchLocalName[ 0] = stCancelConn.LocalName;

                wchLocalName[ 1] = L':';

                wchLocalName[ 2] = L'\0';

                hr = StringCbCatW( uniConnectionName.Buffer,
                                   uniConnectionName.MaximumLength,
                                   wchLocalName);

                if ( FAILED(hr))
                {
#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPCancelConnection uniConnectionName buffer too small (3)\n");
#endif
                    try_return( dwStatus = WN_OUT_OF_MEMORY);
                }

                pwchLocalName = wchLocalName;
            }
            else
            {

                hr = StringCbCatW( uniConnectionName.Buffer,
                                   uniConnectionName.MaximumLength,
                                   lpName);

                if ( FAILED(hr))
                {
#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPCancelConnection uniConnectionName buffer too small (4)\n");
#endif
                    try_return( dwStatus = WN_OUT_OF_MEMORY);
                }

                pwchLocalName = lpName;
            }

            hr = StringCbCatW( uniConnectionName.Buffer,
                               uniConnectionName.MaximumLength,
                               wchRemoteName);

            if ( FAILED(hr))
            {
#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPCancelConnection uniConnectionName buffer too small (5)\n");
#endif
                try_return( dwStatus = WN_OUT_OF_MEMORY);
            }

            if( !DefineDosDevice( DDD_REMOVE_DEFINITION | DDD_RAW_TARGET_PATH | DDD_EXACT_MATCH_ON_REMOVE,
                                  pwchLocalName,
                                  uniConnectionName.Buffer))
            {

#ifdef AFS_DEBUG_TRACE
                DWORD gle = GetLastError();

                AFSDbgPrint( L"NPCancelConnection Failed to cancel connection to system - gle 0x%x Name %s connection %wZ\n",
                             gle,
                             pwchLocalName,
                             &uniConnectionName);
#endif
            }
            else
            {
#ifdef AFS_DEBUG_TRACE

                AFSDbgPrint( L"NPCancelConnection Canceled connection to system - Name %s connection %wZ\n",
                             pwchLocalName,
                             &uniConnectionName);
#endif
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
NPGetConnection( LPWSTR  lpLocalName,
                 LPWSTR  lpRemoteName,
                 LPDWORD lpBufferSize)
{

    DWORD dwBufferSize = *lpBufferSize;
    DWORD dwStatus;

    dwStatus = NPGetConnectionCommon( lpLocalName,
                                      lpRemoteName,
                                      &dwBufferSize,
                                      FALSE);

    if ( dwStatus == WN_NOT_CONNECTED)
    {

        dwStatus = NPGetConnectionCommon( lpLocalName,
                                          lpRemoteName,
                                          lpBufferSize,
                                          TRUE);
    }
    else
    {

        *lpBufferSize = dwBufferSize;
    }

    return dwStatus;
}

DWORD
APIENTRY
NPGetConnectionCommon( LPWSTR  lpLocalName,
                       LPWSTR  lpRemoteName,
                       LPDWORD lpBufferSize,
                       BOOL    bDriveSubstOk)
{

    DWORD    dwStatus = WN_NOT_CONNECTED;
    WCHAR    wchLocalName[3];
    WCHAR    wchSubstName[1024 + 26];
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    HANDLE   hControlDevice = NULL;
    DWORD    dwPassedSize;

    __Enter
    {

        if ( NPIsFSDisabled())
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection AFSRDFS is disabled, returning WN_NOT_CONNECTED\n");
#endif

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        if( lstrlen( lpLocalName) == 0)
        {
#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection No local name, returning WN_BAD_LOCALNAME\n");
#endif
            try_return( dwStatus = WN_BAD_LOCALNAME);
        }

        if ( lpBufferSize == NULL)
        {
#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection No output size, returning WN_BAD_LOCALNAME\n");
#endif
            try_return( dwStatus = WN_BAD_VALUE);
        }

        dwPassedSize = *lpBufferSize;

        if ( !bDriveSubstOk ||
             !DriveSubstitution( lpLocalName, wchSubstName, sizeof( wchSubstName), &dwStatus))
        {
            wchLocalName[0] = towupper(lpLocalName[0]);
            wchLocalName[1] = L':';
            wchLocalName[2] = L'\0';

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection Requesting connection for %s\n",
                         wchLocalName);
#endif
        }
        else
        {
            ReadServerNameString();

            if ( wchSubstName[0] != L'\\' &&
                 wchSubstName[1] == L':')
            {

                wchLocalName[0] = towupper(wchSubstName[0]);
                wchLocalName[1] = L':';
                wchLocalName[2] = L'\0';

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetConnection Requesting connection for drive substitution %s -> %s\n",
                             wchSubstName,
                             wchLocalName);
#endif
            }
            else if ( _wcsnicmp( wchSubstName, wszServerNameUNC, cbServerNameUNCLength / sizeof( WCHAR)) == 0 &&
                      ( wchSubstName[cbServerNameUNCLength / sizeof( WCHAR)] == L'\\' ||
                        wchSubstName[cbServerNameUNCLength / sizeof( WCHAR)] == 0))
            {
                HRESULT hr;
                WCHAR  *pwch;
                DWORD   dwCount = 0;
                DWORD   dwRequiredSize;

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetConnection drive substitution %s is AFS\n",
                             wchSubstName);
#endif

                dwRequiredSize = wcslen( wchSubstName) * sizeof( WCHAR) + sizeof( WCHAR);

                if ( lpRemoteName == NULL ||
                     dwPassedSize == 0 ||
                     dwRequiredSize > *lpBufferSize)
                {

                    *lpBufferSize = dwRequiredSize;

                    try_return( dwStatus = WN_MORE_DATA);

                }

                hr = StringCbCopyN(lpRemoteName, *lpBufferSize, wchSubstName, sizeof( wchSubstName));

                if ( SUCCEEDED(hr))
                {

                    for ( dwCount = 0, pwch = lpRemoteName; *pwch && pwch < lpRemoteName + (*lpBufferSize); pwch++ )
                    {
                        if ( *pwch == L'\\' )
                        {
                            dwCount++;

                            if ( dwCount == 4)
                            {
                                *pwch = L'\0';

                                break;
                            }
                        }

                    }

                    *lpBufferSize = wcslen( lpRemoteName) * sizeof( WCHAR) + sizeof( WCHAR);

                    try_return( dwStatus = WN_SUCCESS);
                }
                else if ( hr == STRSAFE_E_INSUFFICIENT_BUFFER)
                {

                    *lpBufferSize = wcslen( wchSubstName) * sizeof( WCHAR) + sizeof( WCHAR);

                    for ( dwCount = 0, pwch = lpRemoteName; *pwch; pwch++ )
                    {
                        if ( *pwch == L'\\' )
                        {
                            dwCount++;

                            if ( dwCount == 4)
                            {
                                *pwch = L'\0';

                                *lpBufferSize = wcslen( lpRemoteName) * sizeof( WCHAR) + sizeof( WCHAR);

                                try_return( dwStatus = WN_SUCCESS);
                            }
                        }

                    }

                    try_return( dwStatus = WN_MORE_DATA);
                }
                else
                {

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPGetConnection StringCbCopyN failure 0x%X\n",
                                 hr);
#endif
                    try_return( dwStatus = WN_NET_ERROR);
                }
            }
            else
            {

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetConnection drive substitution %s is not AFS\n",
                             wchSubstName);
#endif
                try_return( dwStatus = WN_NOT_CONNECTED);
            }
        }

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetConnection Requesting connection for %s\n",
                     wchLocalName);
#endif

        dwBufferSize = 0x1000;

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);

        if( pConnectCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        pConnectCB->LocalName = towupper(wchLocalName[0]);

        pConnectCB->RemoteNameLength = 0;

        pConnectCB->Version = AFS_NETWORKPROVIDER_INTERFACE_VERSION_1;

        pConnectCB->AuthenticationId = AFSRetrieveAuthId();

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetConnection Retrieved authentication id %08lX-%08lX\n",
                     pConnectCB->AuthenticationId.HighPart,
                     pConnectCB->AuthenticationId.LowPart);
#endif

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection OpenRedirector failure, returning WN_NET_ERROR\n");
#endif

            try_return( dwStatus = WN_NET_ERROR);
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
#ifdef AFS_DEBUG_TRACE
            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPGetConnection Failed to get connection from file system for local %s gle 0x%x\n",
                         wchLocalName, gle);
#endif
            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        //
        // IOCTL_AFS_GET_CONNECTION returns a counted string
        //

        if( lpRemoteName == NULL ||
            *lpBufferSize + sizeof( WCHAR) > dwPassedSize)
        {

            *lpBufferSize += sizeof( WCHAR);

            try_return( dwStatus = WN_MORE_DATA);
        }

        memcpy( lpRemoteName,
                (void *)pConnectCB,
                *lpBufferSize);

        lpRemoteName[ *lpBufferSize/sizeof( WCHAR)] = L'\0';

        *lpBufferSize += sizeof( WCHAR);

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetConnection local %s remote %s\n",
                     wchLocalName,
                     lpRemoteName);
#endif
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
NPGetConnection3( IN     LPCWSTR lpLocalName,
                  IN     DWORD dwLevel,
                  OUT    LPVOID lpBuffer,
                  IN OUT LPDWORD lpBufferSize)
{

    DWORD dwBufferSize = *lpBufferSize;
    DWORD dwStatus;

    dwStatus = NPGetConnection3Common( lpLocalName,
                                       dwLevel,
                                       lpBuffer,
                                       &dwBufferSize,
                                       FALSE);

    if ( dwStatus == WN_NOT_CONNECTED)
    {

        dwStatus = NPGetConnection3Common( lpLocalName,
                                           dwLevel,
                                           lpBuffer,
                                           lpBufferSize,
                                           TRUE);
    }
    else
    {

        *lpBufferSize = dwBufferSize;
    }

    return dwStatus;
}


static DWORD APIENTRY
NPGetConnection3Common( IN     LPCWSTR lpLocalName,
                        IN     DWORD dwLevel,
                        OUT    LPVOID lpBuffer,
                        IN OUT LPDWORD lpBufferSize,
                        IN     BOOL bDriveSubstOk)
{

    DWORD    dwStatus = WN_NOT_CONNECTED;
    WCHAR    wchLocalName[3];
    WCHAR    wchSubstName[1024 + 26];
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    HANDLE   hControlDevice = NULL;
    DWORD    dwPassedSize;
    DWORD   *pConnectState =(DWORD *)lpBuffer;

    __Enter
    {

        if ( NPIsFSDisabled())
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection3 AFSRDFS is disabled, returning WN_NOT_CONNECTED\n");
#endif

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        if( lstrlen( lpLocalName) == 0)
        {
#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection3 No local name, returning WN_BAD_LOCALNAME\n");
#endif
            try_return( dwStatus = WN_BAD_LOCALNAME);
        }

        //
        // LanMan NPGetConnection3 only responds to level 1
        //

        if ( dwLevel != 0x1)
        {
#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection3 Level 0x%X returning WN_BAD_LEVEL\n", dwLevel);
#endif
            try_return( dwStatus = WN_BAD_LEVEL);
        }

        if ( lpBufferSize == NULL)
        {
#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection3 No output size, returning WN_BAD_VALUE\n");
#endif
            try_return( dwStatus = WN_BAD_VALUE);
        }

        dwPassedSize = *lpBufferSize;

        if ( dwPassedSize == 0 ||
             lpBuffer == NULL)
        {

            *lpBufferSize = sizeof( DWORD);

            try_return( dwStatus = WN_MORE_DATA);
        }

        if ( !bDriveSubstOk ||
             !DriveSubstitution( lpLocalName, wchSubstName, sizeof( wchSubstName), &dwStatus))
        {
            wchLocalName[0] = towupper(lpLocalName[0]);
            wchLocalName[1] = L':';
            wchLocalName[2] = L'\0';

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection3 Requesting connection for %s level 0x%X\n",
                         wchLocalName,
                         dwLevel);
#endif
        }
        else
        {

            ReadServerNameString();

            if ( wchSubstName[0] != L'\\' &&
                 wchSubstName[1] == L':')
            {

                wchLocalName[0] = towupper(wchSubstName[0]);
                wchLocalName[1] = L':';
                wchLocalName[2] = L'\0';

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetConnection3 Requesting connection for drive substitution %s -> %s level 0x%x\n",
                             wchSubstName,
                             wchLocalName,
                             dwLevel);
#endif
            }
            else if ( _wcsnicmp( wchSubstName, wszServerNameUNC, cbServerNameUNCLength / sizeof( WCHAR)) == 0 &&
                      ( wchSubstName[cbServerNameUNCLength / sizeof( WCHAR)] == L'\\' ||
                        wchSubstName[cbServerNameUNCLength / sizeof( WCHAR)] == 0))
            {

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetConnection3 drive substitution %s is AFS return connected\n",
                             wchSubstName);
#endif
                *pConnectState = WNGETCON_CONNECTED;

                *lpBufferSize = sizeof( DWORD);

                try_return( dwStatus = WN_SUCCESS);
            }
            else
            {

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetConnection3 drive substitution %s is not AFS return not connected\n",
                             wchSubstName);
#endif
                try_return( dwStatus = WN_NOT_CONNECTED);
            }
        }

        dwBufferSize = 0x1000;

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);

        if( pConnectCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        pConnectCB->LocalName = towupper(wchLocalName[0]);

        pConnectCB->RemoteNameLength = 0;

        pConnectCB->Version = AFS_NETWORKPROVIDER_INTERFACE_VERSION_1;

        pConnectCB->AuthenticationId = AFSRetrieveAuthId();

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetConnection3 Retrieved authentication id %08lX-%08lX\n",
                     pConnectCB->AuthenticationId.HighPart,
                     pConnectCB->AuthenticationId.LowPart);
#endif

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnection3 OpenRedirector failure, returning WN_NET_ERROR\n");
#endif

            try_return( dwStatus = WN_NET_ERROR);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_GET_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   pConnectCB,
                                   dwBufferSize,
                                   &dwBufferSize,
                                   NULL);

        if( !dwError)
        {
#ifdef AFS_DEBUG_TRACE
            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPGetConnection3 Failed to get connection from file system for local %s gle 0x%x\n",
                         wchLocalName, gle);
#endif
            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        *lpBufferSize = sizeof( DWORD);

        if( sizeof( DWORD) > dwPassedSize)
        {

            try_return( dwStatus = WN_MORE_DATA);
        }

        *pConnectState = WNGETCON_CONNECTED;

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetConnection3 local %s connect-state 0x%x\n",
                     wchLocalName,
                     *pConnectState);
#endif
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
NPGetConnectionPerformance( LPCWSTR lpRemoteName,
                            LPNETCONNECTINFOSTRUCT lpNetConnectInfo)
{

    DWORD dwReturn = WN_SUCCESS;
    AFSNetworkProviderConnectionCB *pConnectCB = NULL;
    DWORD dwBufferSize = 0;
    HANDLE hControlDevice = NULL;
    DWORD dwError = 0;

    __Enter
    {

        if ( NPIsFSDisabled())
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetConnectionPerformance AFSRDFS is disabled, returning WN_NET_ERROR\n");
#endif

            return WN_NET_ERROR;
        }

        AFSDbgPrint( L"NPGetConnectionPerformance Entry for remote connection %S\n",
                     lpRemoteName);

        dwBufferSize = 0x1000;

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);

        if( pConnectCB == NULL)
        {
            try_return( dwReturn = WN_OUT_OF_MEMORY);
        }

        pConnectCB->RemoteNameLength = wcslen( lpRemoteName) * sizeof( WCHAR);

        StringCbCopy( pConnectCB->RemoteName,
                      dwBufferSize - sizeof(AFSNetworkProviderConnectionCB),
                      lpRemoteName);

        pConnectCB->Version = AFS_NETWORKPROVIDER_INTERFACE_VERSION_1;

        pConnectCB->AuthenticationId = AFSRetrieveAuthId();

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {
            AFSDbgPrint( L"NPGetConnectionPerformance OpenRedirector failure, returning WN_NET_ERROR\n");

            try_return( dwReturn = WN_NET_ERROR);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_GET_CONNECTION_INFORMATION,
                                   pConnectCB,
                                   dwBufferSize,
                                   pConnectCB,
                                   dwBufferSize,
                                   &dwBufferSize,
                                   NULL);

        if( !dwError)
        {
#ifdef AFS_DEBUG_TRACE
            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPGetConnectionPerformance Failed to get connection info from file system for remote %S gle 0x%x\n",
                         lpRemoteName,
                         gle);
#endif
            try_return( dwReturn = WN_NOT_CONNECTED);
        }

        lpNetConnectInfo->dwFlags = 0;

        lpNetConnectInfo->dwSpeed = 0;

        lpNetConnectInfo->dwDelay = 0;

        lpNetConnectInfo->dwOptDataSize = 0x10000;

        AFSDbgPrint( L"NPGetConnectionPerformance Successfully returned information for remote connection %S\n",
                     lpRemoteName);

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

    return dwReturn;
}

static LPCWSTR
GetUsageString( DWORD dwUsage)
{
    static WCHAR Buffer[128] = L"";
    //
    // RESOURCEUSAGE_CONNECTABLE   0x00000001
    // RESOURCEUSAGE_CONTAINER     0x00000002
    // RESOURCEUSAGE_NOLOCALDEVICE 0x00000004
    // RESOURCEUSAGE_SIBLING       0x00000008
    // RESOURCEUSAGE_ATTACHED      0x00000010
    // RESOURCEUSAGE_ALL           (RESOURCEUSAGE_CONNECTABLE | RESOURCEUSAGE_CONTAINER | RESOURCEUSAGE_ATTACHED)
    // RESOURCEUSAGE_RESERVED      0x80000000
    //

    Buffer[0] = L'\0';

    if ( dwUsage == RESOURCEUSAGE_ALL )
    {
        return L"ALL";
    }

    if ( dwUsage == 0 )
    {
        return L"NONE";
    }

    if ( dwUsage & RESOURCEUSAGE_CONNECTABLE )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"CONNECTABLE|");
    }

    if ( dwUsage & RESOURCEUSAGE_CONTAINER )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"CONTAINER|");
    }

    if ( dwUsage & RESOURCEUSAGE_NOLOCALDEVICE )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"NOLOCALDEVICE|");
    }

    if ( dwUsage & RESOURCEUSAGE_SIBLING )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"SIBLING|");
    }

    if ( dwUsage & RESOURCEUSAGE_ATTACHED )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"ATTACHED|");
    }

    if ( dwUsage & RESOURCEUSAGE_RESERVED )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"RESERVED|");
    }

    if ( dwUsage & ~(RESOURCEUSAGE_ALL|RESOURCEUSAGE_NOLOCALDEVICE|RESOURCEUSAGE_SIBLING|RESOURCEUSAGE_RESERVED) )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"UNKNOWN|");
    }

    Buffer[lstrlen(Buffer)-1] = L'\0';

    return Buffer;
}

static LPCWSTR
GetTypeString( DWORD dwType)
{
    static WCHAR Buffer[128] = L"";

    //
    // RESOURCETYPE_ANY        0x00000000
    // RESOURCETYPE_DISK       0x00000001
    // RESOURCETYPE_PRINT      0x00000002
    // RESOURCETYPE_RESERVED   0x00000008
    // RESOURCETYPE_UNKNOWN    0xFFFFFFFF
    //

    Buffer[0] = L'\0';

    if ( dwType == RESOURCETYPE_ANY )
    {
        return L"ANY";
    }

    if ( dwType == RESOURCETYPE_UNKNOWN )
    {
        return L"UNKNOWN";
    }

    if ( dwType & RESOURCETYPE_DISK )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"DISK|");
    }

    if ( dwType & RESOURCETYPE_PRINT )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"PRINT|");
    }

    if ( dwType & RESOURCETYPE_RESERVED )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"RESERVED|");
    }

    if ( dwType & ~(RESOURCETYPE_DISK|RESOURCETYPE_PRINT|RESOURCETYPE_RESERVED) )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"UNKNOWN|");
    }

    Buffer[lstrlen(Buffer)-1] = L'\0';

    return Buffer;
}

static LPCWSTR
GetScopeString( DWORD dwScope)
{
    static WCHAR Buffer[128] = L"";

    //
    // RESOURCE_CONNECTED      0x00000001
    // RESOURCE_GLOBALNET      0x00000002
    // RESOURCE_REMEMBERED     0x00000003
    // RESOURCE_RECENT         0x00000004
    // RESOURCE_CONTEXT        0x00000005
    //

    Buffer[0] = L'\0';

    if ( dwScope == RESOURCE_CONNECTED )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"CONNECTED|");
    }

    if ( dwScope == RESOURCE_GLOBALNET )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"GLOBALNET|");
    }

    if ( dwScope == RESOURCE_REMEMBERED )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"REMEMBERED|");
    }

    if ( dwScope == RESOURCE_RECENT )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"RECENT|");
    }

    if ( dwScope == RESOURCE_CONTEXT )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"CONTEXT|");
    }

    if ( dwScope & ~(RESOURCE_CONNECTED|RESOURCE_GLOBALNET|RESOURCE_REMEMBERED|RESOURCE_RECENT|RESOURCE_CONTEXT) )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"UNKNOWN|");
    }

    Buffer[lstrlen(Buffer)-1] = L'\0';

    return Buffer;
}

static LPCWSTR
GetDisplayString( DWORD dwDisplay)
{
    //
    // RESOURCEDISPLAYTYPE_GENERIC        0x00000000
    // RESOURCEDISPLAYTYPE_DOMAIN         0x00000001
    // RESOURCEDISPLAYTYPE_SERVER         0x00000002
    // RESOURCEDISPLAYTYPE_SHARE          0x00000003
    // RESOURCEDISPLAYTYPE_FILE           0x00000004
    // RESOURCEDISPLAYTYPE_GROUP          0x00000005
    // RESOURCEDISPLAYTYPE_NETWORK        0x00000006
    // RESOURCEDISPLAYTYPE_ROOT           0x00000007
    // RESOURCEDISPLAYTYPE_SHAREADMIN     0x00000008
    // RESOURCEDISPLAYTYPE_DIRECTORY      0x00000009
    // RESOURCEDISPLAYTYPE_TREE           0x0000000A
    // RESOURCEDISPLAYTYPE_NDSCONTAINER   0x0000000B
    //

    switch ( dwDisplay ) {
    case RESOURCEDISPLAYTYPE_GENERIC:
        return L"GENERIC";
    case RESOURCEDISPLAYTYPE_DOMAIN:
        return L"DOMAIN";
    case RESOURCEDISPLAYTYPE_SERVER:
        return L"SERVER";
    case RESOURCEDISPLAYTYPE_SHARE:
        return L"SHARE";
    case RESOURCEDISPLAYTYPE_FILE:
        return L"FILE";
    case RESOURCEDISPLAYTYPE_GROUP:
        return L"GROUP";
    case RESOURCEDISPLAYTYPE_NETWORK:
        return L"NETWORK";
    case RESOURCEDISPLAYTYPE_ROOT:
        return L"ROOT";
    case RESOURCEDISPLAYTYPE_SHAREADMIN:
        return L"SHAREADMIN";
    case RESOURCEDISPLAYTYPE_DIRECTORY:
        return L"DIRECTORY";
    case RESOURCEDISPLAYTYPE_TREE:
        return L"TREE";
    case RESOURCEDISPLAYTYPE_NDSCONTAINER:
        return L"NDSCONTAINER";
    default:
        return L"UNKNOWN";
    }
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

#ifdef AFS_DEBUG_TRACE
    if ( lpNetResource == NULL)
    {
        AFSDbgPrint( L"NPOpenEnum Scope %s Type %s Usage %s NetResource: (Null)\n",
                     GetScopeString(dwScope), GetTypeString(dwType), GetUsageString(dwUsage));
    }
    else
    {
        AFSDbgPrint( L"NPOpenEnum Scope %s Type %s Usage %s NetResource (0x%p): Scope %s Type %s Display %s Usage %s Local %s Remote \"%s\" Comment \"%s\"\n",
                     GetScopeString(dwScope),
                     GetTypeString(dwType),
                     GetUsageString(dwUsage),
                     lpNetResource,
                     GetScopeString(lpNetResource->dwScope),
                     GetTypeString(lpNetResource->dwType),
                     GetDisplayString(lpNetResource->dwDisplayType),
                     GetUsageString(lpNetResource->dwUsage),
                     lpNetResource->lpLocalName,
                     lpNetResource->lpRemoteName,
                     lpNetResource->lpComment);
    }
#endif

    if ( dwUsage == 0 )
    {
        dwUsage = RESOURCEUSAGE_ALL;
    }

#if 0
    if ( dwType == 0 || dwType == RESOURCEUSAGE_ATTACHED)
    {
        dwType |= RESOURCETYPE_DISK | RESOURCETYPE_PRINT;
    }
#endif

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

            break;
        }

        case RESOURCE_CONTEXT:
        {

            pEnumCB->Scope = RESOURCE_CONTEXT;

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

                    StringCbCopy( pEnumCB->RemoteName,
                                  0x1000,
                                  lpNetResource->lpRemoteName);

                }
            }

            pEnumCB->Scope = RESOURCE_GLOBALNET;

            break;
        }

        default:

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPOpenEnum Processing (Scope %s 0x%x) Type %s Usage %s, returning WN_NOT_SUPPORTED\n",
                         GetScopeString(dwScope), dwScope, GetTypeString(dwType), GetUsageString(dwUsage));
#endif

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

        if ( lpBufferSize == NULL)
        {
#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPEnumResource No output size, returning WN_BAD_VALUE\n");
#endif
            try_return( dwStatus = WN_BAD_VALUE);
        }

        ReadProviderNameString();

        pNetResource = (LPNETRESOURCE) lpBuffer;
        SpaceAvailable = *lpBufferSize;
        EntriesRequested = *lpcCount;
        *lpcCount = EntriesCopied = 0;
        StringZone = (PWCHAR) ((char *)lpBuffer + *lpBufferSize);

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPEnumResource Processing Remote name %s Scope %s Type %s Usage %s Index %d SpaceAvailable 0x%lX RequestedEntries %lu\n",
                     pEnumCB->RemoteName ? pEnumCB->RemoteName : L"(Null)",
                     GetScopeString(pEnumCB->Scope),
                     GetTypeString(pEnumCB->Type),
                     GetUsageString(pEnumCB->Type),
                     pEnumCB->CurrentIndex,
                     SpaceAvailable,
                     EntriesRequested);
#endif

        if ( NPIsFSDisabled())
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPEnumResource AFSRDFS is disabled, returning WN_NO_MORE_ENTRIES\n");
#endif

            try_return( dwStatus = WN_NO_MORE_ENTRIES);
        }

        pConnectionCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 0x1000);

        if( pConnectionCB == NULL)
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPEnumResource Out of Memory\n");
#endif

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        pConnectionCBBase = (void *)pConnectionCB;

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPEnumResource OpenRedirector failure, returning WN_NET_ERROR\n");
#endif

            try_return( dwStatus = WN_NET_ERROR);
        }

        if( pEnumCB->Type != RESOURCETYPE_ANY && pEnumCB->Type != RESOURCETYPE_DISK)
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPEnumResource Non-DISK queries are not supported, returning WN_NO_MORE_ENTRIES\n");
#endif
            try_return( dwStatus = WN_NO_MORE_ENTRIES);
        }

        //
        // Handle the special cases here
        //   0. Provider Network Root
        //   1. Server Root
        //

#if 0
        if ( pEnumCB->Scope == RESOURCE_GLOBALNET)
        {

            ReadServerNameString();

            if ( pEnumCB->CurrentIndex == 0 &&
                 pEnumCB->RemoteName == NULL)
            {

                // Determine the space needed for this entry...

                SpaceNeeded = 2 * ( cbProviderNameLength + sizeof( WCHAR));

                uniRemoteName.Length = (USHORT)cbProviderNameLength;
                uniRemoteName.MaximumLength = uniRemoteName.Length;
                uniRemoteName.Buffer = wszProviderName;

                if( SpaceNeeded + sizeof( NETRESOURCE) > SpaceAvailable)
                {

                    *lpBufferSize = SpaceNeeded + sizeof( NETRESOURCE);

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPEnumResource Request MORE_DATA for entry %s Len %d\n",
                                 &uniRemoteName,
                                 *lpBufferSize);
#endif
                    try_return( dwStatus = WN_MORE_DATA);
                }

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPEnumResource Processing Entry %wZ\n",
                             &uniRemoteName);
#endif

                SpaceAvailable -= (SpaceNeeded + sizeof( NETRESOURCE));

                pNetResource->dwScope       = RESOURCE_GLOBALNET;
                pNetResource->dwType        = RESOURCETYPE_ANY;
                pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_NETWORK;
                pNetResource->dwUsage       = RESOURCEUSAGE_CONTAINER | RESOURCEUSAGE_RESERVED;

                // setup string area at opposite end of buffer
                StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded);

                pNetResource->lpLocalName = NULL;

                // copy remote name
                pNetResource->lpRemoteName = StringZone;

                StringCbCopy( StringZone,
                              cbProviderNameLength + sizeof( WCHAR),
                              wszProviderName);

                StringZone += cbProviderNameLength / sizeof(WCHAR) + 1;

                pNetResource->lpComment = NULL;

                // copy provider name
                pNetResource->lpProvider = StringZone;
                StringCbCopy( StringZone,
                              cbProviderNameLength + sizeof( WCHAR),
                              wszProviderName);

                StringZone += cbProviderNameLength / sizeof( WCHAR) + 1;

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPEnumResource Entry (0x%p) Scope %s Type %s Display %s Usage %s Local %s Remote \"%s\" Comment \"%s\"\n",
                             pNetResource,
                             GetScopeString(pNetResource->dwScope),
                             GetTypeString(pNetResource->dwType),
                             GetDisplayString(pNetResource->dwDisplayType),
                             GetUsageString(pNetResource->dwUsage),
                             pNetResource->lpLocalName,
                             pNetResource->lpRemoteName,
                             pNetResource->lpComment);
#endif

                // setup the new end of buffer
                StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded);

                EntriesCopied++;

                pNetResource++;

                // do not change the index since we did not query the redirector
                pEnumCB->CurrentIndex = 0;

                // remember that we returned the provider name
                pEnumCB->RemoteName = (WCHAR *)HeapAlloc( GetProcessHeap( ), HEAP_ZERO_MEMORY, 0x1000);

                if( pEnumCB->RemoteName == NULL)
                {

                    try_return( dwStatus = WN_OUT_OF_MEMORY);
                }
                else
                {

                    StringCbCopy( pEnumCB->RemoteName,
                                   0x1000,
                                   wszProviderName);
                }
            }

            if ( pEnumCB->CurrentIndex == 0 &&
                 lstrlen( pEnumCB->RemoteName) == cbProviderNameLength / sizeof( WCHAR) &&
                 _wcsnicmp( pEnumCB->RemoteName, wszProviderName, cbProviderNameLength / sizeof( WCHAR)) == 0 &&
                 EntriesCopied < EntriesRequested)
            {

                //
                // After the network provider entry comes the server entry
                //

                // Determine the space needed for this entry...

                SpaceNeeded = cbProviderNameLength + cbServerNameUNCLength + cbServerCommentLength + 3 * sizeof( WCHAR);

                uniRemoteName.Length = (USHORT)cbServerNameUNCLength;
                uniRemoteName.MaximumLength = uniRemoteName.Length;
                uniRemoteName.Buffer = wszServerNameUNC;

                if( SpaceNeeded + sizeof( NETRESOURCE) > SpaceAvailable)
                {

                    *lpBufferSize = SpaceNeeded + sizeof( NETRESOURCE);

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPEnumResource Request MORE_DATA for entry %s Len %d\n",
                                 &uniRemoteName,
                                 *lpBufferSize);
#endif
                    try_return( dwStatus = WN_MORE_DATA);
                }

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPEnumResource Processing Entry %wZ\n",
                             &uniRemoteName);
#endif

                SpaceAvailable -= (SpaceNeeded + sizeof( NETRESOURCE));

                pNetResource->dwScope       = 0;
                pNetResource->dwType        = RESOURCETYPE_ANY;
                pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SERVER;
                pNetResource->dwUsage       = RESOURCEUSAGE_CONTAINER;

                // setup string area at opposite end of buffer
                StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded);

                pNetResource->lpLocalName = NULL;

                // copy remote name
                pNetResource->lpRemoteName = StringZone;

                StringCbCopy( StringZone,
                              cbServerNameUNCLength + sizeof( WCHAR),
                              wszServerNameUNC);

                StringZone += cbServerNameUNCLength / sizeof(WCHAR) + 1;

                // copy comment
                pNetResource->lpComment = StringZone;

                StringCbCopy( StringZone,
                              cbServerCommentLength + sizeof( WCHAR),
                              wszServerComment);

                StringZone += cbServerCommentLength / sizeof( WCHAR) + 1;

                // copy provider name
                pNetResource->lpProvider = StringZone;
                StringCbCopy( StringZone,
                              cbProviderNameLength + sizeof( WCHAR),
                              wszProviderName);

                StringZone += cbProviderNameLength / sizeof( WCHAR) + 1;

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPEnumResource Entry (0x%p) Scope %s Type %s Display %s Usage %s Local %s Remote \"%s\" Comment \"%s\"\n",
                             pNetResource,
                             GetScopeString(pNetResource->dwScope),
                             GetTypeString(pNetResource->dwType),
                             GetDisplayString(pNetResource->dwDisplayType),
                             GetUsageString(pNetResource->dwUsage),
                             pNetResource->lpLocalName,
                             pNetResource->lpRemoteName,
                             pNetResource->lpComment);
#endif

                // setup the new end of buffer
                StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded);

                EntriesCopied++;

                pNetResource++;

                // do not update the index because we did not query the redirector
                pEnumCB->CurrentIndex = 0;

                // remember that we returned the server
                StringCbCopy( pEnumCB->RemoteName,
                              0x1000,
                              wszServerNameUNC);
            }
        }
#endif

        //
        // Setup what we are going to ask for
        //

        pConnectionCB->Scope = pEnumCB->Scope;

        pConnectionCB->Type = pEnumCB->Type;

        pConnectionCB->CurrentIndex = pEnumCB->CurrentIndex;

        pConnectionCB->Version = AFS_NETWORKPROVIDER_INTERFACE_VERSION_1;

        //
        // If this is a RESOURCE_GLOBALNET enumeration then pass down the remote name if
        // there is one
        //

        pConnectionCB->RemoteNameLength = 0;

        if( pEnumCB->Scope == RESOURCE_GLOBALNET &&
            pEnumCB->RemoteName != NULL)
        {

            pConnectionCB->RemoteNameLength = wcslen( pEnumCB->RemoteName) * sizeof( WCHAR);

            StringCbCopy( pConnectionCB->RemoteName,
                          (0x1000 - sizeof(AFSNetworkProviderConnectionCB)) + sizeof(WCHAR),
                          pEnumCB->RemoteName);
        }

        pConnectionCB->AuthenticationId = AFSRetrieveAuthId();

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPEnumResource Retrieved authentication id %08lX-%08lX\n",
                     pConnectionCB->AuthenticationId.HighPart,
                     pConnectionCB->AuthenticationId.LowPart);
#endif

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
#ifdef AFS_DEBUG_TRACE
            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPEnumResource Failed to list connections from file system - gle 0x%x\n",
                         gle);
#endif
            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        if( dwCopyBytes == 0)
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPEnumResource No More Entries\n");
#endif
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

            pNetResource->dwScope       = pConnectionCB->Scope;
            pNetResource->dwType        = pConnectionCB->Type;

            pNetResource->dwDisplayType = pConnectionCB->DisplayType;

            if ( pNetResource->dwType == RESOURCETYPE_ANY &&
                 pNetResource->dwDisplayType == RESOURCEDISPLAYTYPE_SHARE)
            {

                pNetResource->dwType = RESOURCETYPE_DISK;
            }

            if ( pEnumCB->Scope == RESOURCE_CONNECTED)
            {

                pNetResource->dwUsage       = 0;
            }
            else
            {

                pNetResource->dwUsage       = pConnectionCB->Usage;
            }

            // setup string area at opposite end of buffer
            StringZone = (PWCHAR)( (PBYTE) StringZone - SpaceNeeded);

            // copy local name
            if( pConnectionCB->LocalName != 0)
            {

                pNetResource->lpLocalName = StringZone;
                *StringZone++ = towupper(pConnectionCB->LocalName);
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
            StringCbCopy( StringZone,
                          cbProviderNameLength + sizeof( WCHAR),
                          wszProviderName);

            StringZone += (cbProviderNameLength / sizeof( WCHAR) + 1);

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPEnumResource Entry (0x%p) Scope %s Type %s Display %s Usage %s Local %s Remote \"%s\" Comment \"%s\"\n",
                         pNetResource,
                         GetScopeString(pNetResource->dwScope),
                         GetTypeString(pNetResource->dwType),
                         GetDisplayString(pNetResource->dwDisplayType),
                         GetUsageString(pNetResource->dwUsage),
                         pNetResource->lpLocalName,
                         pNetResource->lpRemoteName,
                         pNetResource->lpComment);
#endif

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

#ifdef AFS_DEBUG_TRACE
    AFSDbgPrint( L"NPCloseEnum\n");
#endif

    if( pEnumCB->RemoteName != NULL)
    {

        HeapFree( GetProcessHeap( ), 0, (PVOID) pEnumCB->RemoteName);
    }

    HeapFree( GetProcessHeap( ), 0, (PVOID) hEnum );

    return WN_SUCCESS;
}

DWORD APIENTRY
NPGetResourceParent( LPNETRESOURCE   lpNetResource,
                     LPVOID  lpBuffer,
                     LPDWORD lpBufferSize )
{

    DWORD    dwStatus = WN_ACCESS_DENIED;
    WCHAR   *pwchRemoteName = NULL, *pwchSearch = NULL, *pwchSystem = NULL;
    LPNETRESOURCE lpOutResource = (LPNETRESOURCE) lpBuffer;

    if ( lpNetResource == NULL)
    {
#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetResourceParent NULL NETRESOURCE\n");
#endif
        return WN_MORE_DATA;
    }

    if( lpNetResource->lpRemoteName == NULL)
    {
#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetResourceParent NULL NETRESOURCE\n");
#endif
        return WN_BAD_NETNAME;
    }

    if ( lpNetResource->dwType != 0 &&
         lpNetResource->dwType != RESOURCETYPE_DISK)
    {
#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetResourceParent Bad dwType\n");
#endif
        return WN_BAD_VALUE;
    }

    if ( lpBufferSize == NULL )
    {

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetResourceParent Null lpBufferSize\n");
#endif
        return WN_BAD_VALUE;
    }

#ifdef AFS_DEBUG_TRACE
    AFSDbgPrint( L"NPGetResourceParent For remote name %s\n",
                 lpNetResource->lpRemoteName);
#endif

    pwchRemoteName = lpNetResource->lpRemoteName;

    pwchSearch = pwchRemoteName + (wcslen( pwchRemoteName) - 1);

    while( pwchSearch != pwchRemoteName)
    {

        if( *pwchSearch == L'\\')
        {

            *pwchSearch = L'\0';

            break;
        }

        pwchSearch--;
    }

    if( pwchSearch != pwchRemoteName)
    {

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetResourceParent Processing parent %s\n",
                     lpNetResource->lpRemoteName);
#endif

        dwStatus = NPGetResourceInformation( lpNetResource,
                                             lpBuffer,
                                             lpBufferSize,
                                             &pwchSystem);
    }
    else
    {
        if ( lpOutResource == NULL ||
             *lpBufferSize < sizeof( NETRESOURCE) )
        {
            *lpBufferSize = sizeof( NETRESOURCE);

            return WN_MORE_DATA;
        }

        memset( lpOutResource, 0, sizeof( NETRESOURCE));

        return WN_SUCCESS;

    }

    return dwStatus;
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
    DWORD    dwPassedSize;


    __Enter
    {
        if ( lplpSystem)
        {
            *lplpSystem = NULL;
        }

        ReadProviderNameString();

        if ( NPIsFSDisabled())
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetResourceInformation AFSRDFS is disabled, returning WN_BAD_NETNAME\n");
#endif

            try_return( dwStatus = WN_BAD_NETNAME);
        }

        if ( lpNetResource == NULL ||
             lpBufferSize == NULL )
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetResourceInformaton Null lpNetResource or lpBufferSize\n");
#endif
            return WN_BAD_VALUE;
        }

        if( lpNetResource->lpRemoteName == NULL)
        {
#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetResourceInformation No resource name\n");
#endif

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        dwPassedSize = *lpBufferSize;

        dwBufferSize = 0x1000;

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);

        if( pConnectCB == NULL)
        {

            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        pConnectCB->RemoteNameLength = wcslen( lpNetResource->lpRemoteName) * sizeof( WCHAR);

        StringCbCopy( pConnectCB->RemoteName,
                      dwBufferSize - sizeof(AFSNetworkProviderConnectionCB),
                      lpNetResource->lpRemoteName);

        pConnectCB->Version = AFS_NETWORKPROVIDER_INTERFACE_VERSION_1;

        pConnectCB->AuthenticationId = AFSRetrieveAuthId();

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetResourceInformation Retrieved authentication id %08lX-%08lX\n",
                     pConnectCB->AuthenticationId.HighPart,
                     pConnectCB->AuthenticationId.LowPart);
#endif

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetResourceInformation OpenRedirector failure, returning WN_NET_ERROR\n");
#endif

            try_return( dwStatus = WN_NET_ERROR);
        }

        uniRemoteName.Length = (USHORT)pConnectCB->RemoteNameLength;
        uniRemoteName.MaximumLength = uniRemoteName.Length;
        uniRemoteName.Buffer = pConnectCB->RemoteName;

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetResourceInformation For remote name %wZ Scope %08lX Type %08lX Usage %08lX\n",
                     &uniRemoteName,
                     pConnectCB->Scope,
                     pConnectCB->Type,
                     pConnectCB->Usage);
#endif

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
#ifdef AFS_DEBUG_TRACE
            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPGetResourceInformation Failed to get connection info from file system for local %s gle 0x%x\n",
                         lpNetResource->lpRemoteName, gle);
#endif
            try_return( dwStatus = WN_BAD_NETNAME);
        }

        // Determine the space needed for this entry...

        ulRequiredLen = sizeof( NETRESOURCE);

        ulRequiredLen += pConnectCB->RemoteNameLength + sizeof( WCHAR);

        ulRequiredLen += pConnectCB->CommentLength + sizeof( WCHAR);

        ulRequiredLen += cbProviderNameLength + sizeof( WCHAR);

        ulRequiredLen += pConnectCB->RemainingPathLength + sizeof( WCHAR);

        if( pNetResource == NULL ||
            ulRequiredLen > dwPassedSize)
        {

            *lpBufferSize = ulRequiredLen;

            try_return( dwStatus = WN_MORE_DATA);
        }

        pStringZone = (PWCHAR) ((char *)lpBuffer + sizeof( NETRESOURCE));

        pNetResource->dwScope       = 0 /* pConnectCB->Scope*/;
        pNetResource->dwType        = 0 /* pConnectCB->Type */;

        pNetResource->dwDisplayType = pConnectCB->DisplayType;

        pNetResource->dwUsage       = pConnectCB->Usage;

        pNetResource->lpLocalName = NULL;

        // copy remote name
        pNetResource->lpRemoteName = pStringZone;

        CopyMemory( pStringZone,
                    pConnectCB->RemoteName,
                    pConnectCB->RemoteNameLength);

        pStringZone += (pConnectCB->RemoteNameLength / sizeof(WCHAR));

        *pStringZone++ = L'\0';

        // copy comment
        pNetResource->lpComment = pStringZone;

        CopyMemory( pStringZone,
                    (void *)((char *)pConnectCB + pConnectCB->CommentOffset),
                    pConnectCB->CommentLength);

        pStringZone += (pConnectCB->CommentLength / sizeof(WCHAR));

        *pStringZone++ = L'\0';

        // copy remaining path
        if (pConnectCB->RemainingPathLength > 0)
        {
            *lplpSystem = pStringZone;

            CopyMemory( pStringZone,
                        (void *)((char *)pConnectCB + pConnectCB->RemainingPathOffset),
                        pConnectCB->RemainingPathLength);

            pStringZone += (pConnectCB->RemainingPathLength / sizeof(WCHAR));

            *pStringZone++ = L'\0';

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetResourceInformation For remote name %s returning remaining path %s\n",
                         pNetResource->lpRemoteName,
                         *lplpSystem);
#endif
        }

        // copy provider name
        pNetResource->lpProvider = pStringZone;

        StringCbCopy( pStringZone,
                      cbProviderNameLength + sizeof( WCHAR),
                      wszProviderName);

        pStringZone += (cbProviderNameLength / sizeof( WCHAR) + 1);

        *lpBufferSize = ulRequiredLen;

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

static VOID
SeparateRemainingPath( WCHAR * lpConnectionName, WCHAR **lppRemainingPath)
{
    WCHAR *pwch;
    WCHAR wch1, wch2;
    DWORD  dwCount;

    //
    // at this point the lpConnectionName contains the full name.  We need to
    // truncate it to \\server\share and move the remaining path back one position.
    //

    for ( pwch = lpConnectionName, dwCount = 0; *pwch; pwch++)
    {
        if ( *pwch == L'\\')
        {
            dwCount++;
        }

        if ( dwCount == 4)
        {
            break;
        }
    }

    if (*pwch == L'\\')
    {
        //
        // Found the remaining path that must be moved
        //

        *lppRemainingPath = pwch + 1;

        *pwch++ = 0;

        //
        // Find the end
        //
        for ( ; *pwch; pwch++);

        //
        // and work backwards moving the string
        // and then make sure that there is at least
        // a path separator.
        //

        *(pwch + 1) = 0;

        for ( ;pwch > *lppRemainingPath; pwch--)
        {
            *pwch = *(pwch - 1);
        }

        *pwch = L'\\';
    }
}

DWORD APIENTRY
NPGetUniversalName( LPCWSTR lpLocalPath,
                    DWORD   dwInfoLevel,
                    LPVOID  lpBuffer,
                    LPDWORD lpBufferSize)
{

    DWORD dwBufferSize = *lpBufferSize;
    DWORD dwStatus;

    dwStatus = NPGetUniversalNameCommon( lpLocalPath,
                                         dwInfoLevel,
                                         lpBuffer,
                                         &dwBufferSize,
                                         FALSE);

    if ( dwStatus == WN_NOT_CONNECTED)
    {

        dwStatus = NPGetUniversalNameCommon( lpLocalPath,
                                             dwInfoLevel,
                                             lpBuffer,
                                             lpBufferSize,
                                             TRUE);
    }
    else
    {

        *lpBufferSize = dwBufferSize;
    }

    return dwStatus;
}

static DWORD APIENTRY
NPGetUniversalNameCommon( LPCWSTR lpLocalPath,
                          DWORD   dwInfoLevel,
                          LPVOID  lpBuffer,
                          LPDWORD lpBufferSize,
                          BOOL    bDriveSubstOk)
{
    DWORD    dwStatus = WN_NOT_CONNECTED;
    WCHAR    wchLocalName[3];
    WCHAR   *pwchSubstName = NULL;
    DWORD    dwSubstNameLength = 0;
    AFSNetworkProviderConnectionCB   *pConnectCB = NULL;
    DWORD    dwError = 0;
    DWORD    dwBufferSize = 0;
    DWORD    dwPassedSize = *lpBufferSize;
    DWORD    dwRemainingLength = *lpBufferSize;
    HANDLE   hControlDevice = NULL;
    DWORD    dwLocalPathLength = 0;
    DWORD    dwRemainingPathLength = 0;
    CHAR    *pch;

    __Enter
    {

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetUniversalName local path %s level 0x%X\n",
                     lpLocalPath ? lpLocalPath : L"(Null)",
                     dwInfoLevel);
#endif

        if ( NPIsFSDisabled())
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetUniversalName AFSRDFS is disabled, returning WN_NOT_CONNECTED\n");
#endif

            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        dwLocalPathLength = lstrlen( lpLocalPath);

        dwRemainingPathLength = dwLocalPathLength - 2;          // no drive letter

        if( dwLocalPathLength == 0)
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetUniversalName AFSRDFS is disabled, returning WN_BAD_LOCALNAME\n");
#endif

            try_return( dwStatus = WN_BAD_LOCALNAME);
        }

        if( lpBuffer == NULL ||
            lpBufferSize == NULL)
        {
#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetUniversalName No output buffer or size\n");
#endif
            try_return( dwStatus = WN_BAD_VALUE);
        }

        dwSubstNameLength = 4096;

        pwchSubstName = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwSubstNameLength);

        if ( pwchSubstName == NULL)
        {
#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetUniversalName unable to allocate substitution name buffer.\n");
#endif
            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        memset(lpBuffer, 0, dwPassedSize);

        if ( !bDriveSubstOk ||
             !DriveSubstitution( lpLocalPath, pwchSubstName, dwSubstNameLength, &dwStatus))
        {
            wchLocalName[0] = towupper(lpLocalPath[0]);
            wchLocalName[1] = L':';
            wchLocalName[2] = L'\0';

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"NPGetUniversalName Requesting UNC for %s level 0x%X\n",
                         wchLocalName,
                         dwInfoLevel);
#endif
        }
        else
        {

            ReadServerNameString();

            if ( pwchSubstName[0] != L'\\' &&
                 pwchSubstName[1] == L':')
            {

                wchLocalName[0] = towupper(pwchSubstName[0]);
                wchLocalName[1] = L':';
                wchLocalName[2] = L'\0';

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetUniversalName Requesting UNC for drive substitution %s -> %s\n",
                             pwchSubstName,
                             wchLocalName);
#endif
            }
            else if ( _wcsnicmp( pwchSubstName, wszServerNameUNC, cbServerNameUNCLength / sizeof( WCHAR)) == 0 &&
                      ( pwchSubstName[cbServerNameUNCLength / sizeof( WCHAR)] == L'\\' ||
                        pwchSubstName[cbServerNameUNCLength / sizeof( WCHAR)] == 0))
            {
                HRESULT hr;

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetUniversalName drive substitution %s is AFS; Level 0x%x BufferSize 0x%x\n",
                             pwchSubstName,
                             dwInfoLevel,
                             dwPassedSize);
#endif

                dwBufferSize = (wcslen( pwchSubstName) + 1) * sizeof( WCHAR);

                switch( dwInfoLevel)
                {

                case UNIVERSAL_NAME_INFO_LEVEL:
                {

                    UNIVERSAL_NAME_INFO *pUniversalInfo = (UNIVERSAL_NAME_INFO *)lpBuffer;

                    *lpBufferSize = sizeof( UNIVERSAL_NAME_INFO) + dwBufferSize;

                    if( dwPassedSize <= sizeof( UNIVERSAL_NAME_INFO))
                    {

#ifdef AFS_DEBUG_TRACE
                        AFSDbgPrint( L"NPGetUniversalName (UNIVERSAL_NAME_INFO) WN_MORE_DATA\n");
#endif

                        try_return( dwStatus = WN_MORE_DATA);
                    }

                    dwRemainingLength -= sizeof( UNIVERSAL_NAME_INFO);

                    pUniversalInfo->lpUniversalName = (LPTSTR)((char *)lpBuffer + sizeof( UNIVERSAL_NAME_INFO));

                    memcpy( pUniversalInfo->lpUniversalName,
                            pwchSubstName,
                            min( dwBufferSize, dwRemainingLength));

                    dwRemainingLength -= min( dwBufferSize, dwRemainingLength);

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPGetUniversalName (UNIVERSAL_NAME_INFO_LEVEL) lpBuffer: %p Name: (%p) \"%s\"\n",
                                 lpBuffer,
                                 pUniversalInfo->lpUniversalName,
                                 pUniversalInfo->lpUniversalName);
#endif

                    if ( dwPassedSize < *lpBufferSize)
                    {

                        try_return( dwStatus = WN_MORE_DATA);
                    }

                    try_return( dwStatus = WN_SUCCESS);
                }

                case REMOTE_NAME_INFO_LEVEL:
                {

                    REMOTE_NAME_INFO *pRemoteInfo = (REMOTE_NAME_INFO *)lpBuffer;

                    *lpBufferSize = sizeof( REMOTE_NAME_INFO) + 2 * dwBufferSize + sizeof( WCHAR);

                    if( dwPassedSize <= sizeof( REMOTE_NAME_INFO))
                    {

#ifdef AFS_DEBUG_TRACE
                        AFSDbgPrint( L"NPGetUniversalName (REMOTE_NAME_INFO) WN_MORE_DATA\n");
#endif

                        try_return( dwStatus = WN_MORE_DATA);
                    }

                    dwRemainingLength -= sizeof( REMOTE_NAME_INFO);

                    pRemoteInfo->lpUniversalName = (LPTSTR)((char *)lpBuffer + sizeof( REMOTE_NAME_INFO));

                    memcpy( pRemoteInfo->lpUniversalName,
                            pwchSubstName,
                            min( dwRemainingLength, dwBufferSize));

                    dwRemainingLength -= min( dwRemainingLength, dwBufferSize);

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPGetUniversalName (REMOTE_NAME_INFO_LEVEL) UNI lpBuffer: %p Name: (%p) \"%s\"\n",
                                 lpBuffer,
                                 pRemoteInfo->lpUniversalName,
                                 pRemoteInfo->lpUniversalName);
#endif

                    if ( dwRemainingLength > dwBufferSize + sizeof( WCHAR))
                    {
                        pRemoteInfo->lpConnectionName = (LPTSTR)((char *)pRemoteInfo->lpUniversalName + dwBufferSize);

                        memcpy( pRemoteInfo->lpConnectionName,
                                pwchSubstName,
                                min( dwRemainingLength, dwBufferSize));

                        dwRemainingLength -= min( dwRemainingLength, dwBufferSize) - sizeof( WCHAR);

                        SeparateRemainingPath( pRemoteInfo->lpConnectionName,
                                               &pRemoteInfo->lpRemainingPath);
                    }

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPGetUniversalName (REMOTE_NAME_INFO_LEVEL) CONN lpBuffer: %p Name: (%p) \"%s\"\n",
                                 lpBuffer,
                                 pRemoteInfo->lpConnectionName,
                                 pRemoteInfo->lpConnectionName ? pRemoteInfo->lpConnectionName : L"(null)");

                    AFSDbgPrint( L"NPGetUniversalName (REMOTE_NAME_INFO_LEVEL) REMAIN lpBuffer: %p Name: (%p) \"%s\"\n",
                                 lpBuffer,
                                 pRemoteInfo->lpRemainingPath,
                                 pRemoteInfo->lpRemainingPath ? pRemoteInfo->lpRemainingPath : L"(null)");
#endif

                    if ( dwPassedSize < *lpBufferSize)
                    {

                        try_return( dwStatus = WN_MORE_DATA);
                    }

                    try_return( dwStatus = WN_SUCCESS);
                }

                default:
#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPGetUniversalName (UNKNOWN: 0x%X) WN_BAD_VALUE\n",
                                 dwInfoLevel);
#endif
                    try_return( dwStatus = WN_BAD_VALUE);
                }
            }
            else
            {

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetUniversalName drive substitution %s is not AFS\n",
                             pwchSubstName);
#endif
                try_return( dwStatus = WN_NOT_CONNECTED);
            }
        }

        dwBufferSize = 0x1000;

        pConnectCB = (AFSNetworkProviderConnectionCB *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufferSize);

        if( pConnectCB == NULL)
        {
            try_return( dwStatus = WN_OUT_OF_MEMORY);
        }

        pConnectCB->LocalName = towupper(wchLocalName[0]);

        pConnectCB->RemoteNameLength = 0;

        pConnectCB->Version = AFS_NETWORKPROVIDER_INTERFACE_VERSION_1;

        pConnectCB->AuthenticationId = AFSRetrieveAuthId();

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetUniversalName Retrieved authentication id %08lX-%08lX\n",
                     pConnectCB->AuthenticationId.HighPart,
                     pConnectCB->AuthenticationId.LowPart);
#endif

        hControlDevice = OpenRedirector();

        if( hControlDevice == NULL)
        {

            try_return( dwStatus = WN_NET_ERROR);
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_GET_CONNECTION,
                                   pConnectCB,
                                   dwBufferSize,
                                   pConnectCB,
                                   dwBufferSize,
                                   &dwBufferSize,
                                   NULL);

        if( !dwError)
        {
#ifdef AFS_DEBUG_TRACE
            DWORD gle = GetLastError();

            AFSDbgPrint( L"NPGetUniversalName Failed to get connection from file system for local %s gle 0x%x\n",
                         wchLocalName, gle);
#endif
            try_return( dwStatus = WN_NOT_CONNECTED);
        }

        switch( dwInfoLevel)
        {

            case UNIVERSAL_NAME_INFO_LEVEL:
            {

                UNIVERSAL_NAME_INFO *pUniversalInfo = (UNIVERSAL_NAME_INFO *)lpBuffer;

                *lpBufferSize = sizeof( UNIVERSAL_NAME_INFO) + dwBufferSize + sizeof( WCHAR);

                *lpBufferSize += dwRemainingPathLength * sizeof( WCHAR);

                if( dwPassedSize <= sizeof( UNIVERSAL_NAME_INFO))
                {

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPGetUniversalName (UNIVERSAL_NAME_INFO) WN_MORE_DATA\n");
#endif

                    try_return( dwStatus = WN_MORE_DATA);
                }

                dwRemainingLength -= sizeof( UNIVERSAL_NAME_INFO);

                pUniversalInfo->lpUniversalName = (LPTSTR)((char *)lpBuffer + sizeof( UNIVERSAL_NAME_INFO));

                pch = (char *)pUniversalInfo->lpUniversalName;

                memcpy( pch,
                        pConnectCB,
                        min( dwBufferSize, dwRemainingLength));

                pch += min( dwBufferSize, dwRemainingLength);

                dwRemainingLength -= min( dwBufferSize + sizeof(WCHAR), dwRemainingLength);

                memcpy( pch,
                        &lpLocalPath[2],
                        min(dwRemainingPathLength * sizeof( WCHAR), dwRemainingLength));

                pch += min(dwRemainingPathLength * sizeof( WCHAR), dwRemainingLength);

                dwRemainingLength -= min(dwRemainingPathLength * sizeof( WCHAR), dwRemainingLength);

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetUniversalName (UNIVERSAL_NAME_INFO_LEVEL) lpBuffer: %p Name: (%p) \"%s\"\n",
                             lpBuffer,
                             pUniversalInfo->lpUniversalName,
                             pUniversalInfo->lpUniversalName);
#endif

                if ( dwPassedSize < *lpBufferSize)
                {

                    try_return( dwStatus = WN_MORE_DATA);
                }

                try_return( dwStatus = WN_SUCCESS);
            }

            case REMOTE_NAME_INFO_LEVEL:
            {

                REMOTE_NAME_INFO *pRemoteInfo = (REMOTE_NAME_INFO *)lpBuffer;

                *lpBufferSize = sizeof( REMOTE_NAME_INFO) + (2 * dwBufferSize + sizeof( WCHAR)) + 2 * sizeof( WCHAR);

                *lpBufferSize += 2 * dwRemainingPathLength * sizeof( WCHAR);

                if( dwPassedSize <= sizeof( REMOTE_NAME_INFO))
                {

#ifdef AFS_DEBUG_TRACE
                    AFSDbgPrint( L"NPGetUniversalName (REMOTE_NAME_INFO) WN_MORE_DATA\n");
#endif

                    try_return( dwStatus = WN_MORE_DATA);
                }

                dwRemainingLength -= sizeof( REMOTE_NAME_INFO);

                pRemoteInfo->lpUniversalName = (LPTSTR)((char *)lpBuffer + sizeof( REMOTE_NAME_INFO));

                pch = (char *)pRemoteInfo->lpUniversalName;

                memcpy( pch,
                        pConnectCB,
                        min( dwBufferSize, dwRemainingLength));

                pch += min( dwBufferSize, dwRemainingLength);

                dwRemainingLength -= min( dwBufferSize + sizeof( WCHAR), dwRemainingLength);

                memcpy( pch,
                        &lpLocalPath[2],
                        min(dwRemainingPathLength * sizeof( WCHAR), dwRemainingLength));

                pch += min((dwRemainingPathLength + 1) * sizeof( WCHAR), dwRemainingLength);

                dwRemainingLength -= min((dwRemainingPathLength + 1) * sizeof( WCHAR), dwRemainingLength);

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetUniversalName (REMOTE_NAME_INFO_LEVEL) UNI lpBuffer: %p Name: (%p) \"%s\"\n",
                             lpBuffer,
                             pRemoteInfo->lpUniversalName,
                             pRemoteInfo->lpUniversalName);
#endif

                if ( dwRemainingLength > dwBufferSize + sizeof( WCHAR))
                {
                    pRemoteInfo->lpConnectionName = (LPWSTR)pch;

                    memcpy( pch,
                            pConnectCB,
                            min( dwBufferSize, dwRemainingLength));

                    pch += min( dwBufferSize + sizeof( WCHAR), dwRemainingLength);

                    dwRemainingLength -= min( dwBufferSize + sizeof( WCHAR), dwRemainingLength);
                }


                if ( dwRemainingLength > dwRemainingPathLength + sizeof( WCHAR))
                {
                    pRemoteInfo->lpRemainingPath = (LPWSTR)pch;

                    memcpy( pch,
                            &lpLocalPath[2],
                            min((dwRemainingPathLength + 1) * sizeof( WCHAR), dwRemainingLength));

                    pch += min((dwRemainingPathLength + 1) * sizeof( WCHAR), dwRemainingLength);

                    dwRemainingLength -= min((dwLocalPathLength + 1) * sizeof( WCHAR), dwRemainingLength);
                }

#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetUniversalName (REMOTE_NAME_INFO_LEVEL) CONN lpBuffer: %p Name: (%p) \"%s\"\n",
                             lpBuffer,
                             pRemoteInfo->lpConnectionName,
                             pRemoteInfo->lpConnectionName ? pRemoteInfo->lpConnectionName : L"(null)");

                AFSDbgPrint( L"NPGetUniversalName (REMOTE_NAME_INFO_LEVEL) REMAIN lpBuffer: %p Name: (%p) \"%s\"\n",
                             lpBuffer,
                             pRemoteInfo->lpRemainingPath,
                             pRemoteInfo->lpRemainingPath ? pRemoteInfo->lpRemainingPath : L"(null)");
#endif

                if ( dwPassedSize < *lpBufferSize)
                {

                    try_return( dwStatus = WN_MORE_DATA);
                }

                try_return( dwStatus = WN_SUCCESS);
            }

            default:
#ifdef AFS_DEBUG_TRACE
                AFSDbgPrint( L"NPGetUniversalName (UNKNOWN: 0x%X) WN_BAD_VALUE\n",
                             dwInfoLevel);
#endif
                try_return( dwStatus = WN_BAD_VALUE);
        }

try_exit:

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPGetUniversalName BufferSize 0x%X\n",
                     *lpBufferSize);
#endif
        if ( hControlDevice != NULL)
        {

            CloseHandle( hControlDevice);
        }

        if ( pwchSubstName)
        {

            HeapFree( GetProcessHeap(), 0, (PVOID) pwchSubstName);
        }

        if( pConnectCB != NULL)
        {

            HeapFree( GetProcessHeap( ), 0, (PVOID) pConnectCB);
        }
    }

    return dwStatus;
}


static LPCWSTR
GetFormatFlags( DWORD dwFlags)
{
    static WCHAR Buffer[128] = L"";

    //
    // WNFMT_MULTILINE         0x01
    // WNFMT_ABBREVIATED       0x02
    // WNFMT_INENUM            0x10
    // WNFMT_CONNECTION        0x20
    //

    Buffer[0] = L'\0';

    if ( dwFlags == 0)
    {
        return L"NONE";
    }

    if ( dwFlags & WNFMT_MULTILINE )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"MULTILINE|");
    }

    if ( dwFlags & WNFMT_INENUM )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"ABBREVIATED|");
    }

    if ( dwFlags & WNFMT_INENUM )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"INENUM|");
    }

    if ( dwFlags & WNFMT_CONNECTION )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"CONNECTION|");
    }

    if ( dwFlags & ~(WNFMT_MULTILINE|WNFMT_ABBREVIATED|WNFMT_INENUM|WNFMT_CONNECTION) )
    {
        StringCbCat( Buffer, sizeof(Buffer), L"UNKNOWN|");
    }

    Buffer[lstrlen(Buffer)-1] = L'\0';

    return Buffer;
}

DWORD
NPFormatNetworkName( LPTSTR  lpRemoteName,
                     LPTSTR  lpFormattedName,
                     LPDWORD lpnLength,
                     DWORD dwFlags,
                     DWORD dwAveCharPerLine)
{

    DWORD dwLen = 0, dwCurrentLen = 0;
    LPTSTR pCurrentName = lpRemoteName;

#ifdef AFS_DEBUG_TRACE
    AFSDbgPrint( L"NPFormatNetworkName Remote %s Flags %s (0x%x) CharsPerLine %u\n",
                 lpRemoteName,
                 GetFormatFlags( dwFlags),
                 dwFlags,
                 dwAveCharPerLine);
#endif


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

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"NPFormatNetworkName remote name %s WN_MORE_DATA\n",
                     lpRemoteName);
#endif

        return WN_MORE_DATA;
    }

    StringCbCopy( lpFormattedName,
                  *lpnLength,
                  pCurrentName);

    *lpnLength = dwCurrentLen * sizeof( WCHAR);

#ifdef AFS_DEBUG_TRACE
    AFSDbgPrint( L"NPFormatNetworkName remote name %s as %s\n",
                 lpRemoteName,
                 lpFormattedName);
#endif

    return WN_SUCCESS;
}

/************************************************************
/       Unsupported entry points
/************************************************************/

//
// AuthGroup processing is implemented in src/WINNT/afsd/afslogon.c
//
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

#ifdef AFS_DEBUG_TRACE
    AFSDbgPrint( L"NPLogonNotify, returning WN_NOT_SUPPORTED\n");
#endif

    return WN_NOT_SUPPORTED;
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

#ifdef AFS_DEBUG_TRACE
    AFSDbgPrint( L"NPPasswordChangeNotify, returning WN_NOT_SUPPORTED\n");
#endif

    SetLastError( WN_NOT_SUPPORTED );

    return WN_NOT_SUPPORTED;
}

DWORD APIENTRY
NPGetUser( LPTSTR lpName,
           LPTSTR lpUserName,
           LPDWORD lpBufferSize)
{

    DWORD rc = WN_NOT_SUPPORTED;

    AFSDbgPrint( L"NPGetUser Entry Name %s\n", lpName);

    return rc;
}


DWORD
APIENTRY
NPGetReconnectFlags( LPWSTR  lpRemoteName,
                     unsigned char *Parameter2)
{

    DWORD    dwStatus = WN_NOT_SUPPORTED;

    AFSDbgPrint( L"NPGetReconnectFlags RemoteName %s\n",
                 lpRemoteName);

    return dwStatus;
}


DWORD
APIENTRY
I_SystemFocusDialog( VOID)
{

    DWORD    dwStatus = WN_NOT_SUPPORTED;

    AFSDbgPrint( L"I_SystemFocusDialog\n");

    return dwStatus;
}

/************************************************************
/       END Unsupported entry points
/************************************************************/


HANDLE
OpenRedirector()
{

    HANDLE hControlDevice = NULL;

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
#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"Failed to open control device error: %d\n",
                     GetLastError());
#endif
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

LARGE_INTEGER
AFSRetrieveAuthId()
{

    LARGE_INTEGER liAuthId = {0,0};
    HANDLE hToken = NULL;
    TOKEN_STATISTICS stTokenInfo;
    DWORD dwCopyBytes = 0;

    if ( !OpenThreadToken( GetCurrentThread(),
                           TOKEN_QUERY,
                           FALSE,       // Impersonation
                           &hToken))
    {
        if( !OpenProcessToken( GetCurrentProcess(),
                               TOKEN_QUERY,
                               &hToken))
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"AFSRetrieveAuthId Failed to retrieve Thread and Process tokens 0x%X\n",
                         GetLastError());
#endif
        }
        else
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"AFSRetrieveAuthId Retrieved Process Token\n");
#endif
        }
    }
    else
    {

#ifdef AFS_DEBUG_TRACE
        AFSDbgPrint( L"AFSRetrieveAuthId Retrieved Thread Token\n");
#endif
    }

    if ( hToken != NULL)
    {

        if( !GetTokenInformation( hToken,
                                  TokenStatistics,
                                  &stTokenInfo,
                                  sizeof( TOKEN_STATISTICS),
                                  &dwCopyBytes))
        {

#ifdef AFS_DEBUG_TRACE
            AFSDbgPrint( L"AFSRetrieveAuthId Failed to retrieve token information 0x%X\n",
                         GetLastError());
#endif
        }
        else
        {

            liAuthId.HighPart = stTokenInfo.AuthenticationId.HighPart;
            liAuthId.LowPart = stTokenInfo.AuthenticationId.LowPart;
        }

        CloseHandle( hToken);
    }

    return liAuthId;
}

static DWORD
Debug(void)
{
    static int init = 0;
    static DWORD debug = 0;

    if ( !init ) {
        HKEY hk;

        if (RegOpenKey (HKEY_LOCAL_MACHINE,
                         TEXT("SYSTEM\\CurrentControlSet\\Services\\AFSRedirector\\NetworkProvider"), &hk) == 0)
        {
            DWORD dwSize = sizeof(BOOL);
            DWORD dwType = REG_DWORD;
            RegQueryValueEx (hk, TEXT("Debug"), NULL, &dwType, (PBYTE)&debug, &dwSize);
            RegCloseKey (hk);
        }
        init = 1;
    }

    return debug;
}

static char *
cm_Utf16ToUtf8Alloc(const WCHAR * s, int cch_src, int *pcch_dest)
{
    int cch_dest;
    char * dest;

    if (s == NULL || cch_src == 0 || *s == L'\0') {
        if (pcch_dest)
            *pcch_dest = ((cch_src != 0)?1:0);
        return NULL;
    }

    cch_dest = WideCharToMultiByte(CP_UTF8, 0, s, cch_src, NULL, 0, NULL, FALSE);

    if (cch_dest == 0) {
        if (pcch_dest)
            *pcch_dest = cch_dest;
        return NULL;
    }

    dest = HeapAlloc( GetProcessHeap(), 0, (cch_dest + 1) * sizeof(char));

    WideCharToMultiByte(CP_UTF8, 0, s, cch_src, dest, cch_dest, NULL, FALSE);
    dest[cch_dest] = 0;

    if (pcch_dest)
        *pcch_dest = cch_dest;

    return dest;
}

static void
AppendDebugStringToLogFile(WCHAR *wszbuffer)
{
    HANDLE hFile;
    int len;
    char * buffer;
    DWORD dwWritten;
    BOOL bRet;

    if ( !wszbuffer || !wszbuffer[0] )
        return;

    len = (int)wcslen(wszbuffer);

    buffer = cm_Utf16ToUtf8Alloc(wszbuffer, len, &len);

    if (!buffer)
        return;

    hFile = CreateFileW( L"C:\\TEMP\\AFSRDFSProvider.log",
                         FILE_APPEND_DATA,
                         FILE_SHARE_WRITE,
                         NULL,
                         OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);

    if ( hFile == INVALID_HANDLE_VALUE ) {
        OutputDebugString(L"C:\\AFSRDFSProvider.log cannot be opened.\n");
        return;
    }

    bRet = WriteFile( hFile, buffer, len, &dwWritten, NULL);

    bRet = CloseHandle(hFile);

    HeapFree(GetProcessHeap(), 0, buffer);
}

ULONG
_cdecl
AFSDbgPrint(
    PWCHAR Format,
    ...
    )
{
    HRESULT rc = S_OK;
    WCHAR wszbuffer[512];
    va_list marker;
    DWORD debug = Debug();

    if (debug == 0)
        return 0;

    va_start( marker, Format );
    {
        StringCbPrintf( wszbuffer, sizeof(wszbuffer), L"[%d-%08X] ",
#ifdef AMD64
                           64,
#else
                           32,
#endif
                           GetCurrentThreadId());

        rc = StringCbVPrintfW( &wszbuffer[ 14], sizeof(wszbuffer) - 14 * sizeof(WCHAR), Format, marker);

        if (SUCCEEDED(rc)) {
            if (debug & 1)
                OutputDebugString( wszbuffer );
            if (debug & 2)
                AppendDebugStringToLogFile(wszbuffer);
        } else {
            OutputDebugString(L"AFSDbgPrint Failed to create string\n");
        }
    }
    return SUCCEEDED(rc) ? 1 : 0;
}
