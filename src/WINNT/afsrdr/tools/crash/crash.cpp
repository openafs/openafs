
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

        printf( "Crash: Failed to open control device error: %d\n", GetLastError());

        return 0;
    }


    DeviceIoControl( hControlDevice,
                     IOCTL_AFS_FORCE_CRASH,
                     NULL,
                     0,
                     NULL,
                     0,
                     &bytesReturned,
                     NULL);

    CloseHandle( hControlDevice);

	return 0;
}

