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
#include <winioctl.h>
#include <stdio.h>
#include <shlwapi.h>

#include "AFSUserDefines.h"
#include "AFSUserIoctl.h"
#include "AFSUserStructs.h"

bool
ParseFID( char *FidName,
          AFSFileID *FID);

char *
GetAFSFileType( IN DWORD FileType);

void
Usage()
{

    printf("Usage: AFSObjectStatus </f FID (Cell.Volume.VNode.Unique)> | </n Full file name> /i <Invalidate entry by FID>\n");

    return;
}

int
main(int argc, char* argv[])
{

    ULONG rc = 0;
    DWORD bytesReturned = 0;
    HANDLE hControlDevice = NULL;
    char *pBuffer = NULL;
    DWORD dwError = 0, dwIndex = 0, dwBufferSize = 0;
    AFSGetStatusInfoCB *pGetStatusInfo = NULL;
    AFSStatusInfoCB *pStatusInfo = NULL;
    AFSFileID stFID;
    BOOLEAN bUseFID = false;
    WCHAR wchFileName[ 256];
    AFSInvalidateCacheCB *pInvalidate = NULL;
    bool bInvalidate = false;
    DWORD dwIOControl = 0;

    if( argc < 2)
    {

        Usage();

        return 0;
    }

    dwIndex = 1;

    dwError = 1;

    while( dwIndex < (DWORD)argc)
    {

        if( _stricmp(argv[ dwIndex], "/f") == 0)
        {

            if( !ParseFID( argv[ ++dwIndex],
                           &stFID))
            {

                printf("AFSObjectStatus Failed to parse fid %s\n", argv[ dwIndex]);

                dwError = -1;

                break;
            }

            bUseFID = true;
        }
        else if( _stricmp(argv[ dwIndex], "/n") == 0)
        {

            dwIndex++;

            if( MultiByteToWideChar( CP_ACP,
                                    MB_PRECOMPOSED,
                                    argv[ dwIndex],
                                    -1,
                                    wchFileName,
                                    (int)strlen( argv[ dwIndex]) + 1) == 0)
            {

                printf("AFSObjectStatus Failed to map string %d\n", GetLastError());

                dwError = -1;

                break;
            }
        }
        else if( _stricmp(argv[ dwIndex], "/i") == 0)
        {

            bInvalidate = true;
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

    if( bInvalidate &&
        !bUseFID)
    {

        printf("AFSObjectStatus Must specify FID when performing invalidation\n");

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

        printf( "AFSObjectStatus: Failed to open control device error: %d\n", GetLastError());

        return 0;
    }

    dwBufferSize = 1024;

    pBuffer = (char *)malloc( dwBufferSize);

    if( pBuffer != NULL)
    {

        if( bInvalidate)
        {

            pInvalidate = (AFSInvalidateCacheCB *)pBuffer;

            pInvalidate->FileID = stFID;

            pInvalidate->FileType = 0;

            pInvalidate->Reason = AFS_INVALIDATE_FLUSHED;

            pInvalidate->WholeVolume = false;

            dwError = DeviceIoControl( hControlDevice,
                                       IOCTL_AFS_INVALIDATE_CACHE,
                                       pBuffer,
                                       sizeof( AFSInvalidateCacheCB),
                                       NULL,
                                       0,
                                       &bytesReturned,
                                       NULL);

            if( !dwError)
            {
                printf( "AFSObjectStatus Failed to invalidate object error %d\n", GetLastError());
            }
            else
            {
                printf("AFSObjectStatus Successfully invalidated object\n");
            }
        }
        else
        {

            pGetStatusInfo = (AFSGetStatusInfoCB *)pBuffer;

            memset( pGetStatusInfo, '\0', sizeof( AFSGetStatusInfoCB));

            if( bUseFID)
            {
                pGetStatusInfo->FileID = stFID;
            }
            else
            {
                pGetStatusInfo->FileNameLength = (USHORT)(wcslen( wchFileName) * sizeof( WCHAR));

                dwIndex = 0;

                if( wchFileName[ 0] == L'\\' &&
                    wchFileName[ 1] == L'\\')
                {
                    dwIndex = 1;

                    pGetStatusInfo->FileNameLength -= sizeof( WCHAR);
                }

                memcpy( pGetStatusInfo->FileName,
                        &wchFileName[ dwIndex],
                        pGetStatusInfo->FileNameLength);
            }

            dwError = DeviceIoControl( hControlDevice,
                                       IOCTL_AFS_GET_OBJECT_INFORMATION,
                                       pBuffer,
                                       sizeof( AFSGetStatusInfoCB) + pGetStatusInfo->FileNameLength,
                                       pBuffer,
                                       dwBufferSize,
                                       &bytesReturned,
                                       NULL);

            if( !dwError)
            {
                printf( "AFSObjectStatus Failed to retrieve buffer %d\n", GetLastError());
            }
            else
            {

                pStatusInfo = (AFSStatusInfoCB *)pBuffer;

                if( bUseFID)
                {
                    printf("AFS ObjectStatus Results: FID (%08lX.%08lX.%08lX.%08lX)\n", stFID.Cell, stFID.Volume, stFID.Vnode, stFID.Unique);
                }
                else
                {
                    printf("AFS ObjectStatus Results: Name (%S)\n", wchFileName);
                }

                printf("\n");
                printf("\t\t FID: %08lX.%08lX.%08lX.%08lX\n",
                                        pStatusInfo->FileId.Cell,
                                        pStatusInfo->FileId.Volume,
                                        pStatusInfo->FileId.Vnode,
                                        pStatusInfo->FileId.Unique);

                printf("\t\t TargetFID: %08lX.%08lX.%08lX.%08lX\n",
                                        pStatusInfo->TargetFileId.Cell,
                                        pStatusInfo->TargetFileId.Volume,
                                        pStatusInfo->TargetFileId.Vnode,
                                        pStatusInfo->TargetFileId.Unique);

                printf("\t\t Expiration: %08lX-%08lX\n", pStatusInfo->Expiration.HighPart, pStatusInfo->Expiration.LowPart);

                printf("\t\t Data Version: %08lX-%08lX\n", pStatusInfo->DataVersion.HighPart, pStatusInfo->DataVersion.LowPart);

                printf("\t\t FileType: %s - %08lX\n", GetAFSFileType( pStatusInfo->FileType), pStatusInfo->FileType);

                printf("\t\t Object Flags: %08lX\n", pStatusInfo->ObjectFlags);

                printf("\t\t Create Time: %08lX-%08lX\n", pStatusInfo->CreationTime.HighPart, pStatusInfo->CreationTime.LowPart);

                printf("\t\t Last Access Time: %08lX-%08lX\n", pStatusInfo->LastAccessTime.HighPart, pStatusInfo->LastAccessTime.LowPart);

                printf("\t\t Last Write Time: %08lX-%08lX\n", pStatusInfo->LastWriteTime.HighPart, pStatusInfo->LastWriteTime.LowPart);

                printf("\t\t Change Time: %08lX-%08lX\n", pStatusInfo->ChangeTime.HighPart, pStatusInfo->ChangeTime.LowPart);

                printf("\t\t File Attributes: %08lX\n", pStatusInfo->FileAttributes);

                printf("\t\t EOF: %08lX-%08lX\n", pStatusInfo->EndOfFile.HighPart, pStatusInfo->EndOfFile.LowPart);

                printf("\t\t Alloc Size: %08lX-%08lX\n", pStatusInfo->AllocationSize.HighPart, pStatusInfo->AllocationSize.LowPart);

                printf("\t\t EA Size: %08lX\n", pStatusInfo->EaSize);

                printf("\t\t Links: %08lX\n", pStatusInfo->Links);
            }
        }

        free( pBuffer);
    }

    CloseHandle( hControlDevice);

	return 0;
}

bool
ParseFID( char *FidName,
          AFSFileID *FID)
{

    char *pchCell = NULL, *pchVolume = NULL, *pchVnode = NULL, *pchUnique = NULL;
    char *pchCurrentPos = FidName;
    char *pLocation = NULL;
    char chBuffer[ 50];

    pchCell = pchCurrentPos;

    pLocation = strchr( pchCell, '.');

    if( pLocation == NULL)
    {
        return false;
    }

    pLocation++;

    if( *pLocation == NULL)
    {
        return false;
    }

    pLocation--;

    *pLocation = NULL;

    pLocation++;

    pchVolume = pLocation;

    pLocation = strchr( pchVolume, '.');

    if( pLocation == NULL)
    {
        return false;
    }

    pLocation++;

    if( *pLocation == NULL)
    {
        return false;
    }

    pLocation--;

    *pLocation = NULL;

    pLocation++;

    pchVnode = pLocation;

    pLocation = strchr( pchVnode, '.');

    if( pLocation == NULL)
    {
        return false;
    }

    pLocation++;

    if( *pLocation == NULL)
    {
        return false;
    }

    pLocation--;

    *pLocation = NULL;

    pLocation++;

    pchUnique = pLocation;

    strcpy_s( chBuffer,
              50,
              "0x");

    strcat_s( &chBuffer[ 2],
              48,
              pchCell);

    if( !StrToIntEx( chBuffer,
                     STIF_SUPPORT_HEX,
                     (int *)&FID->Cell))
    {

        printf("AFSObjectStatus Failed to parse cell %s\n", chBuffer);

        return false;
    }

    strcpy_s( chBuffer,
              50,
              "0x");

    strcat_s( &chBuffer[ 2],
              48,
              pchVolume);

    if( !StrToIntEx( chBuffer,
                     STIF_SUPPORT_HEX,
                     (int *)&FID->Volume))
    {

        printf("AFSObjectStatus Failed to parse volume %s\n", chBuffer);

        return false;
    }

    strcpy_s( chBuffer,
              50,
              "0x");

    strcat_s( &chBuffer[ 2],
              48,
              pchVnode);

    if( !StrToIntEx( chBuffer,
                     STIF_SUPPORT_HEX,
                     (int *)&FID->Vnode))
    {

        printf("AFSObjectStatus Failed to parse vnode %s\n", chBuffer);

        return false;
    }

    strcpy_s( chBuffer,
              50,
              "0x");

    strcat_s( &chBuffer[ 2],
              48,
              pchUnique);

    if( !StrToIntEx( chBuffer,
                     STIF_SUPPORT_HEX,
                     (int *)&FID->Unique))
    {

        printf("AFSObjectStatus Failed to parse Unique %s\n", chBuffer);

        return false;
    }

    return true;
}

char *
GetAFSFileType( IN DWORD FileType)
{

    char *pchType = NULL;

    switch( FileType)
    {

        case AFS_FILE_TYPE_FILE:
        {
            pchType = "File";
            break;
        }

        case AFS_FILE_TYPE_DIRECTORY:
        {
            pchType = "Directory";
            break;
        }

        case AFS_FILE_TYPE_SYMLINK:
        {
            pchType = "Symbolic link";
            break;
        }

        case AFS_FILE_TYPE_MOUNTPOINT:
        {
            pchType = "Mount point";
            break;
        }

        case AFS_FILE_TYPE_DFSLINK:
        {
            pchType = "DFS link";
            break;
        }

        default:
        {
            pchType = "Unknown";

            break;
        }
    }

    return pchType;
}
