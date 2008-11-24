
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <devioctl.h>

#include "AFSUserCommon.h"

int main(int argc, char* argv[])
{

    ULONG rc = 0;
    DWORD bytesReturned = 0;
    HANDLE hControlDevice = NULL;
    char *pBuffer = NULL;
    DWORD dwError = 0;
    DWORD dwBufferSize = 2000;

    if( argc > 1)
    {

        if( strcmp(argv[ 1], "?"))
        {

            printf("Usage:GetTrace <Buffer size in KB (Default: 2000)>\n");

            return 0;
        }
        else
        {

            dwBufferSize = atoi( argv[ 1]);
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

    dwBufferSize *= 1024;

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

            printf( pBuffer);
        }

        free( pBuffer);
    }

    CloseHandle( hControlDevice);

	return 0;
}

