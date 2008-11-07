
#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <stdio.h>

#include "AFSUserCommon.h"

int main(int argc, char* argv[])
{

    ULONG rc = 0;
    DWORD bytesReturned = 0;
    HANDLE hControlDevice = NULL;
    char *pBuffer = NULL;
    DWORD dwError = 0;

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

    pBuffer = (char *)malloc( 1024000);

    if( pBuffer != NULL)
    {

        dwError = DeviceIoControl( hControlDevice,
                                   IOCTL_AFS_GET_TRACE_BUFFER,
                                   NULL,
                                   0,
                                   pBuffer,
                                   1024000,
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

