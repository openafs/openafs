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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <shlwapi.h>
#include <winioctl.h>

#include <rpc.h>

#include "AFSUserDefines.h"
#include "AFSUserIoctl.h"
#include "AFSUserStructs.h"

void
Usage()
{

    printf("Usage: AFSAuthGroup < /sid <SID to use> | /ag <Auth Group GUID> /session <Session ID> | /thread <Thread Specific> | /active <Set Active> | \
            /q <Query Active AuthGroup> | /l <Query AuthGroup List> | /c <Create new AuthGroup on process or thread> | /s <Set AuthGroup on process or thread> | \
            /r <Reset AuthGroup list on process or thread> | /n <Create new AuthGroup>\n");

    return;
}

int main(int argc, char* argv[])
{

    ULONG rc = 0;
    DWORD bytesReturned = 0;
    HANDLE hControlDevice = NULL;
    char *pBuffer = NULL;
    DWORD dwError = 0, dwIndex = 0;
    BOOLEAN bQueryActiveAuthGroup = FALSE;
    BOOLEAN bQueryProcessAuthGroupList = FALSE;
    GUID stAuthGroup;
    unsigned char *pchGUID = NULL;
    WCHAR wchUserSID[ 256];
    DWORD dwSessionId = (DWORD)-1;
    BOOLEAN bThreadSpecific = FALSE;
    BOOLEAN bSetActive = FALSE;
    BOOLEAN bCreateSetAuthGroup = FALSE;
    BOOLEAN bSetAuthGroup = FALSE;
    BOOLEAN bResetAuthGroup = FALSE;
    BOOLEAN bCreateAuthGroup = FALSE;
    AFSAuthGroupRequestCB *pAuthGroupRequest = NULL;
    DWORD dwAuthGroupRequestLen = 0;
    char chGUID[ 256];

    if( argc < 2)
    {
        Usage();
        return 0;
    }

    dwIndex = 1;

    dwError = 1;

    memset( wchUserSID, '\0', 256 * sizeof( WCHAR));

    memset( chGUID, '\0', 256);

    while( dwIndex < (DWORD)argc)
    {

        if( _stricmp(argv[ dwIndex], "/q") == 0)
        {
            bQueryActiveAuthGroup = TRUE;
        }
        else if( _stricmp(argv[ dwIndex], "/l") == 0)
        {
            bQueryProcessAuthGroupList = TRUE;
        }
        else if( _stricmp(argv[ dwIndex], "/c") == 0)
        {
            bCreateSetAuthGroup = TRUE;
        }
        else if( _stricmp(argv[ dwIndex], "/s") == 0)
        {
            bSetAuthGroup = TRUE;
        }
        else if( _stricmp(argv[ dwIndex], "/r") == 0)
        {
            bResetAuthGroup = TRUE;
        }
        else if( _stricmp(argv[ dwIndex], "/n") == 0)
        {
            bCreateAuthGroup = TRUE;
        }
        else if( _stricmp( argv[ dwIndex], "/sid") == 0)
        {

            dwIndex++;

            if( MultiByteToWideChar( CP_ACP,
                                    MB_PRECOMPOSED,
                                    argv[dwIndex],
                                    -1,
                                    wchUserSID,
                                    (int)strlen( argv[dwIndex]) + 1) == 0)
            {
                dwError = -1;
                break;
            }
        }
        else if( _stricmp( argv[ dwIndex], "/ag") == 0)
        {

            dwIndex++;

            strcpy( chGUID,
                    argv[dwIndex]);
        }
        else if( _stricmp( argv[ dwIndex], "/session") == 0)
        {

            dwIndex++;

            if( !StrToIntExA( argv[ dwIndex],
                              STIF_SUPPORT_HEX,
                              (int *)&dwSessionId))
            {

                dwError = -1;
                break;
            }
        }
        else if( _stricmp( argv[ dwIndex], "/thread") == 0)
        {

            dwIndex++;

            bThreadSpecific = TRUE;
        }
        else if( _stricmp( argv[ dwIndex], "/active") == 0)
        {

            dwIndex++;

            bSetActive = TRUE;
        }
        else
        {
            Usage();
            dwError = -1;
            break;
        }

        dwIndex++;
    }

    if( dwError == -1)
    {
        return 0;
    }

    hControlDevice = CreateFile( AFS_SYMLINK,
                                 GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL,
                                 OPEN_EXISTING,
                                 0,
                                 NULL );

    if( hControlDevice == INVALID_HANDLE_VALUE)
    {

        printf( "AFSAuthGroup: Failed to open control device error: %d\n", GetLastError());

        return 0;
    }

    if( bQueryActiveAuthGroup)
    {
        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_AUTHGROUP_SID_QUERY,
                                   NULL,
                                   0,
                                   &stAuthGroup,
                                   sizeof( GUID),
                                   &bytesReturned,
                                   NULL);

        if( !dwError)
        {
            printf( "AFSAuthGroup Failed to query auth group error %d\n", GetLastError());
        }
        else
        {

            if( UuidToString( (UUID *)&stAuthGroup,
                              &pchGUID) == RPC_S_OK)
            {
                printf("AFSAuthGroup Successfully retrieved auth group %s\n", pchGUID);
                RpcStringFree( &pchGUID);
            }
            else
            {
                printf("AFSAuthGroup Failed to convert GUID to string\n");
            }
        }
    }
    else if( bQueryProcessAuthGroupList)
    {

        pBuffer = (char *)malloc( 0x1000);

        if( pBuffer == NULL)
        {
            printf("AFSAuthGroup Failed to allocate query buffer\n");
            goto cleanup;
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_AUTHGROUP_QUERY,
                                   NULL,
                                   0,
                                   pBuffer,
                                   0x1000,
                                   &bytesReturned,
                                   NULL);

        if( !dwError)
        {
            printf( "AFSAuthGroup Failed to query auth group list error %d\n", GetLastError());
        }
        else
        {

            GUID *pCurrentGUID = (GUID *)pBuffer;

            if( bytesReturned == 0)
            {
                printf("AFSAuthGroup No custom auth groups assigned to process\n");
            }
            else
            {
                while( bytesReturned > 0)
                {
                    if( UuidToString( (UUID *)pCurrentGUID,
                                      &pchGUID) == RPC_S_OK)
                    {
                        printf("AFSAuthGroup Successfully retrieved auth group list entry %s\n", pchGUID);
                        RpcStringFree( &pchGUID);
                    }
                    else
                    {
                        printf("AFSAuthGroup Failed to convert GUID to string\n");
                    }

                    pCurrentGUID++;

                    bytesReturned -= sizeof( GUID);
                }
            }
        }
    }
    else if( bCreateSetAuthGroup)
    {

        dwAuthGroupRequestLen = (DWORD)(sizeof( AFSAuthGroupRequestCB) +
                                        (wcslen( wchUserSID) * sizeof( WCHAR)));

        pAuthGroupRequest = (AFSAuthGroupRequestCB *)malloc( dwAuthGroupRequestLen);

        if( pAuthGroupRequest == NULL)
        {
            printf("AFSAuthGroup Failed to allocate request block\n");
            goto cleanup;
        }

        memset( pAuthGroupRequest,
                '\0',
                dwAuthGroupRequestLen);

        pAuthGroupRequest->SIDLength = (USHORT)(wcslen( wchUserSID) * sizeof( WCHAR));

        if( pAuthGroupRequest->SIDLength > 0)
        {
            wcscpy( &pAuthGroupRequest->SIDString[ 0], wchUserSID);
        }

        pAuthGroupRequest->SessionId = dwSessionId;

        if( bThreadSpecific)
        {
            pAuthGroupRequest->Flags |= AFS_PAG_FLAGS_THREAD_AUTH_GROUP;
        }

        if( bSetActive)
        {
            pAuthGroupRequest->Flags |= AFS_PAG_FLAGS_SET_AS_ACTIVE;
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_AUTHGROUP_CREATE_AND_SET,
                                   pAuthGroupRequest,
                                   dwAuthGroupRequestLen,
                                   NULL,
                                   0,
                                   &bytesReturned,
                                   NULL);

        if( !dwError)
        {
            printf( "AFSAuthGroup Failed to create and set auth group error %d\n", GetLastError());
        }
        else
        {
            printf( "AFSAuthGroup Successfully create and set auth group\n");
        }

        free( pAuthGroupRequest);
    }
    else if( bSetAuthGroup)
    {

        if( strlen( chGUID) == 0)
        {
            printf("AFSAuthGroup Failed to specify AuthGroup GUID when setting\n");
            goto cleanup;
        }

        dwAuthGroupRequestLen = sizeof( AFSAuthGroupRequestCB);

        pAuthGroupRequest = (AFSAuthGroupRequestCB *)malloc( dwAuthGroupRequestLen);

        if( pAuthGroupRequest == NULL)
        {
            printf("AFSAuthGroup Failed to allocate request block\n");
            goto cleanup;
        }

        memset( pAuthGroupRequest,
                '\0',
                dwAuthGroupRequestLen);

        if( bThreadSpecific)
        {
            pAuthGroupRequest->Flags |= AFS_PAG_FLAGS_THREAD_AUTH_GROUP;
        }

        if( bSetActive)
        {
            pAuthGroupRequest->Flags |= AFS_PAG_FLAGS_SET_AS_ACTIVE;
        }

        if( UuidFromString( (unsigned char *)chGUID,
                            &pAuthGroupRequest->AuthGroup) != RPC_S_OK)
        {
            printf("AFSAuthGroup Failed to convert string to GUID\n");
            free( pAuthGroupRequest);
            goto cleanup;
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_AUTHGROUP_SET,
                                   pAuthGroupRequest,
                                   dwAuthGroupRequestLen,
                                   NULL,
                                   0,
                                   &bytesReturned,
                                   NULL);

        if( !dwError)
        {
            printf( "AFSAuthGroup Failed to set auth group error %d\n", GetLastError());
        }
        else
        {
            printf( "AFSAuthGroup Successfully set auth group\n");
        }

        free( pAuthGroupRequest);
    }
    else if( bResetAuthGroup)
    {

        dwAuthGroupRequestLen = sizeof( AFSAuthGroupRequestCB);

        pAuthGroupRequest = (AFSAuthGroupRequestCB *)malloc( dwAuthGroupRequestLen);

        if( pAuthGroupRequest == NULL)
        {
            printf("AFSAuthGroup Failed to allocate request block\n");
            goto cleanup;
        }

        memset( pAuthGroupRequest,
                '\0',
                dwAuthGroupRequestLen);

        if( bThreadSpecific)
        {
            pAuthGroupRequest->Flags |= AFS_PAG_FLAGS_THREAD_AUTH_GROUP;
        }

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_AUTHGROUP_RESET,
                                   pAuthGroupRequest,
                                   dwAuthGroupRequestLen,
                                   NULL,
                                   0,
                                   &bytesReturned,
                                   NULL);

        if( !dwError)
        {
            printf( "AFSAuthGroup Failed to reset auth group error %d\n", GetLastError());
        }
        else
        {
            printf( "AFSAuthGroup Successfully reset auth group\n");
        }

        free( pAuthGroupRequest);
    }
    else if( bCreateAuthGroup)
    {

        dwAuthGroupRequestLen = (DWORD)(sizeof( AFSAuthGroupRequestCB) +
                                        (wcslen( wchUserSID) * sizeof( WCHAR)));

        pAuthGroupRequest = (AFSAuthGroupRequestCB *)malloc( dwAuthGroupRequestLen);

        if( pAuthGroupRequest == NULL)
        {
            printf("AFSAuthGroup Failed to allocate request block\n");
            goto cleanup;
        }

        memset( pAuthGroupRequest,
                '\0',
                dwAuthGroupRequestLen);

        pAuthGroupRequest->SIDLength = (USHORT)((wcslen( wchUserSID) * sizeof( WCHAR)));

        if( pAuthGroupRequest->SIDLength > 0)
        {
            wcscpy( &pAuthGroupRequest->SIDString[ 0], wchUserSID);
        }

        pAuthGroupRequest->SessionId = dwSessionId;

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_AUTHGROUP_SID_CREATE,
                                   pAuthGroupRequest,
                                   dwAuthGroupRequestLen,
                                   NULL,
                                   0,
                                   &bytesReturned,
                                   NULL);

        if( !dwError)
        {
            printf( "AFSAuthGroup Failed to create auth group error %d\n", GetLastError());
        }
        else
        {
            printf( "AFSAuthGroup Successfully create auth group\n");
        }

        free( pAuthGroupRequest);
    }
    else
    {
        printf("AFSAuthGroup Invalid request parameters\n");
        Usage();
    }

cleanup:

    if( pBuffer != NULL)
    {
        free( pBuffer);
    }

    if( hControlDevice != NULL)
    {
        CloseHandle( hControlDevice);
    }

	return 0;
}
