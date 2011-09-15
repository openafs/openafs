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

int main(int argc, char* argv[])
{

    ULONG rc = 0;
    DWORD bytesReturned = 0;
    HANDLE hControlDevice = NULL;
    char *pBuffer = NULL;
    DWORD dwError = 0;
    int   dwBufferSize = 2001;

    if( argc > 1)
    {

        if( !strcmp(argv[ 1], "?"))
        {

            printf("Usage:GetTrace <Buffer size in KB (Default: 2000)>\n");

            return 0;
        }
        else
        {

            StrToIntExA( argv[ 1], STIF_SUPPORT_HEX, &dwBufferSize);

            if ( dwBufferSize == 0)
            {

                printf("Usage:GetTrace <Buffer size in KB (Default: 2000)>\n");

                return 1;
            }
        }
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

        printf( "GetBuffer: Failed to open control device error: %d\n", GetLastError());

        return 0;
    }

    dwBufferSize *= 1024 + 1;

    pBuffer = (char *)malloc( dwBufferSize);

    if( pBuffer != NULL)
    {

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_GET_TRACE_BUFFER,
                                   NULL,
                                   0,
                                   pBuffer,
                                   dwBufferSize,
                                   &bytesReturned,
                                   NULL);

        if( !dwError)
        {

            printf( "GetBuffer Failed to retrieve buffer %d\n", GetLastError());
        }
        else
        {

            pBuffer[bytesReturned] = '\0';

            printf( "%s", pBuffer);
        }

        free( pBuffer);
    }

    CloseHandle( hControlDevice);

    return 0;
}

