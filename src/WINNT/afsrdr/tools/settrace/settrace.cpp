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
#include <stdio.h>
#include <stdlib.h>
#include <shlwapi.h>
#include <devioctl.h>

#include "AFSUserDefines.h"
#include "AFSUserIoctl.h"
#include "AFSUserStructs.h"

void
usage( void)
{

    printf("SetTrace /l <Logging level> | /s <Subsystem> | /b <Buffer size in KB> | /f <debug Flags>\n");
}

int main(int argc, char* argv[])
{

    ULONG rc = 0;
    DWORD bytesReturned = 0;
    HANDLE hControlDevice = NULL;
    DWORD dwError = 0;
    AFSTraceConfigCB stConfig;
    int dwLevel = -1, dwSystem = -1, dwBufferLen = -1, dwFlags = -1;
    int dwIndex;

    if( argc < 3)
    {

        usage();

        return 1;
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

        printf( "SetTrace: Failed to open control device error: %d\n", GetLastError());

        return 2;
    }

    dwIndex = 1;

    while( dwIndex < argc)
    {

        if( _strcmpi(argv[ dwIndex], "/l") == 0)
        {

            if ( !StrToIntExA( argv[ ++dwIndex], STIF_SUPPORT_HEX, &dwLevel))
            {
                printf("SetTrace Failed to parse parameter %s\n", argv[ dwIndex]);

                dwError = 1;

                break;
            }
        }
        else if( _strcmpi(argv[ dwIndex], "/s") == 0)
        {

            if ( !StrToIntExA( argv[ ++dwIndex], STIF_SUPPORT_HEX, &dwSystem))
            {
                printf("SetTrace Failed to parse parameter %s\n", argv[ dwIndex]);

                dwError = 1;

                break;
            }
        }
        else if( _strcmpi(argv[ dwIndex], "/b") == 0)
        {

            if ( !StrToIntExA( argv[ ++dwIndex], STIF_SUPPORT_HEX, &dwBufferLen))
            {
                printf("SetTrace Failed to parse parameter %s\n", argv[ dwIndex]);

                dwError = 1;

                break;
            }
        }
        else if( _strcmpi(argv[ dwIndex], "/d") == 0 ||
                 _strcmpi(argv[ dwIndex], "/f") == 0)
        {

            if ( !StrToIntExA( argv[ ++dwIndex], STIF_SUPPORT_HEX, &dwFlags))
            {
                printf("SetTrace Failed to parse parameter %s\n", argv[ dwIndex]);

                dwError = 1;

                break;
            }
        }
        else
        {

            usage();

            dwError = 1;

            break;
        }

        dwIndex++;
    }

    if ( dwError)
    {

        rc = 4;

        goto cleanup;
    }

    stConfig.Subsystem = dwSystem;

    stConfig.TraceLevel = dwLevel;

    stConfig.TraceBufferLength = dwBufferLen;

    stConfig.DebugFlags = dwFlags;

    dwError = DeviceIoControl( hControlDevice,
                               IOCTL_AFS_CONFIGURE_DEBUG_TRACE,
                               &stConfig,
                               sizeof( AFSTraceConfigCB),
                               NULL,
                               0,
                               &bytesReturned,
                               NULL);

    if( !dwError)
    {

        printf( "SetTrace Failed to set attributes %d\n", GetLastError());

        rc = 3;
    }
    else
    {

        printf( "SetTrace: Successfully set parameters\n");
    }

  cleanup:

    CloseHandle( hControlDevice);

    return rc;
}

