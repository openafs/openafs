
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
    AFSTraceConfigCB stConfig;
    DWORD dwLevel = -1, dwSystem = -1, dwBufferLen = -1, dwFlags = -1;
    int dwIndex;

    if( argc < 3)
    {

        printf("SetTrace /l <Logging level> | /s <Subsystem> | /b <Buffer size in KB>\n");

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

        printf( "SetTrace: Failed to open control device error: %d\n", GetLastError());

        return 0;
    }

    dwIndex = 1;

    while( dwIndex < argc)
    {

        if( strcmp(argv[ dwIndex], "/l") == 0)
        {

            dwLevel = atoi( argv[ ++dwIndex]);
        }
        else if( strcmp(argv[ dwIndex], "/s") == 0)
        {

            dwSystem = atoi( argv[ ++dwIndex]);
        }
        else if( strcmp(argv[ dwIndex], "/b") == 0)
        {

            dwBufferLen = atoi( argv[ ++dwIndex]);
        }
        else if( strcmp(argv[ dwIndex], "/d") == 0)
        {

            dwFlags = atoi( argv[ ++dwIndex]);
        }

        dwIndex++;
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
    }
    else
    {

        printf( "SetTrace: Successfully set parameters\n");
    }

    CloseHandle( hControlDevice);

	return 0;
}

