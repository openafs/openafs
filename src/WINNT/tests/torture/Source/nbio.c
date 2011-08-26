/*
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Andrew Tridgell 1997-1998
   Copyright (C) Richard Sharpe  2002

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "common.h"
#include <stdlib.h>

extern int verbose;

#define off_t DWORD

__declspec( thread ) extern int     ProcessNumber;
__declspec( thread ) extern int     LineCount;
__declspec( thread ) extern int     AfsTrace;
__declspec( thread ) extern int     *pThreadStatus;
__declspec( thread ) extern int     LogID;
__declspec( thread ) extern char    *IoBuffer;
__declspec( thread ) extern int     BufferSize;
__declspec( thread ) extern char    AfsLocker[256];
__declspec( thread ) extern char    OriginalAfsLocker[256];
__declspec( thread ) extern char    HostName[256];
__declspec( thread ) extern DWORD   TickCount1, TickCount2, MilliTickStart;
__declspec( thread ) extern FTABLE  ftable[MAX_FILES];
__declspec( thread ) extern struct  cmd_struct ThreadCommandInfo[CMD_MAX_CMD + 1];
__declspec( thread ) extern EXIT_STATUS *pExitStatus;
__declspec( thread ) extern DWORD   LastKnownError;
__declspec( thread ) int EnforcePathInfoErrors = 0;

extern void LogMessage(int ProcessNumber, char *HostName, char *FileName, char *message, int LogID);

HANDLE CreateObject(const char *fname, uint32 DesiredAccess,
                  uint32 FileAttributes, uint32 ShareAccess,
                  uint32 CreateDisposition, uint32 CreateOptions);
void DumpAFSLog(char * HostName, int LogID);
int  FindHandle(int handle);
int  GetFileList(char *Mask, void (*fn)(file_info *, const char *, void *), void *state);
BOOL GetFileInfo(char *FileName, HANDLE fd, uint16 *mode, size_t *size,
		        time_t *c_time, time_t *a_time, time_t *m_time,
		        time_t *w_time);
BOOL GetPathInfo(const char *fname, time_t *c_time, time_t *a_time, time_t *m_time,
		         size_t *size, uint16 *mode);
int  LeaveThread(int status, char *Reason, int cmd);
void StartFirstTimer();
void EndFirstTimer(int cmd, int Type);
void StartSecondTime(int cmd);
void EndSecondTime(int cmd);
void SubstituteString(char *s,const char *pattern,const char *insert, size_t len);
intptr_t  SystemCall(char *command);
HANDLE WinFindFirstFile(char *Mask, void **FileData, char *cFileName, int *dwFileAttributes);
int  WinFindNextFile(HANDLE hFind, void **FileData, char *cFileName, int *dwFileAttributes);

int FindHandle(int handle)
{
    int i;
    for (i=0;i<MAX_FILES;i++)
    {
        if (ftable[i].handle == handle)
            return(i);
    }
    if (verbose)
        printf("(%d) ERROR: handle %d was not found\n", LineCount, handle);
    return(LeaveThread(1, "", -1));
}

int nb_unlink(char *fname)
{
    int     rc;
    char    temp[512];
    char    FileName[128];
    pstring path;
    DWORD   gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    strcpy(path, AfsLocker);
    strcat(path, fname);

    StartFirstTimer();
    rc = DeleteFile(path);
    gle = GetLastError();
    EndFirstTimer(CMD_UNLINK, 1);
    if (!rc)
    {
        LeaveThread(gle, "", CMD_UNLINK);
        sprintf(temp, "FILE: DeleteFile %s failed GLE(0x%x)\n", path, gle);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }

    return(0);
}

int nb_SetLocker(char *Locker)
{

    StartFirstTimer();
    if (strlen(Locker) == 0)
        strcpy(AfsLocker, OriginalAfsLocker);
    else
        strcpy(AfsLocker, Locker);
    EndFirstTimer(CMD_SETLOCKER, 1);
    return(0);
}

int nb_Xrmdir(char *Directory, char *type)
{
    DWORD   rc;
    char    FileName[128];
    char    command[256];
    char    NewDirectory[256];
    char    temp[512];

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    if (strlen(Directory) == 0)
    {
        return(LeaveThread(1, "rmdir failed no path specified\n", CMD_XRMDIR));
    }
    strcpy(NewDirectory, Directory);
    memset(command, '\0', sizeof(command));
    strcpy(command,"rmdir /Q ");
    if (!stricmp(type, "all"))
    {
        strcat(command, "/S ");
    }
    strcat(command, NewDirectory);

    StartFirstTimer();
    rc = system(command);

    if ((rc) && (rc != 2) && (rc != 3))
    {
        EndFirstTimer(CMD_XRMDIR, 0);
        sprintf(temp, "rmdir failed on %s\n", command);
        LeaveThread(rc, temp, CMD_XRMDIR);
        sprintf(temp, "FAILURE: Thread %d - Xrmdir failed on \"%s\"\n", ProcessNumber, command);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_XRMDIR, 1);
    return(0);
}


int nb_Mkdir(char *Directory)
{
    DWORD   rc;
    char    FileName[128];
    char    command[256];
    char    NewDirectory[256];
    char    temp[512];

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    if (strlen(Directory) == 0)
    {
        return(LeaveThread(1, "mkdir failed on no path specified\n", CMD_MKDIR));
    }
    strcpy(NewDirectory, Directory);
    memset(command, '\0', sizeof(command));
    strcpy(command,"mkdir ");
    strcat(command, NewDirectory);

    StartFirstTimer();
    rc = system(command);

    if (rc > 1)
    {
        EndFirstTimer(CMD_MKDIR, 0);
        sprintf(temp,  "mkdir failed on %s\n", command);
        LeaveThread(rc, temp, CMD_MKDIR);
        sprintf(temp, "ERROR: Thread %d - mkdir failed on \"%s\"\n", ProcessNumber, command);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_MKDIR, 1);
    return(0);
}

int nb_Attach(char *Locker, char *Drive)
{
    DWORD   rc;
    char    FileName[128];
    char    command[512];
    char    temp[512];

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    if (strlen(Locker) == 0)
    {
        return(LeaveThread(1, "attach failed no locker specified\n", CMD_ATTACH));
    }
    memset(command, '\0', sizeof(command));
    strcpy(command,"attach -q ");
    rc = 0;
    if (strlen(Drive) != 0)
    {
        sprintf(temp, "-D %s ", Drive);
        strcat(command, temp);
    }
    strcat(command, Locker);

    StartFirstTimer();
    rc = system(command);

    if (rc)
    {
        EndFirstTimer(CMD_ATTACH, 0);
        sprintf(pExitStatus->Reason, "attach failed on %s\n", command);
        pExitStatus->ExitStatus = rc;
        sprintf(temp, "ERROR: Thread %d - attach failed on \"%s\"\n", ProcessNumber, command);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
    }
    EndFirstTimer(CMD_ATTACH, 1);
    return(0);
}

int nb_Detach(char *Name, char *type)
{
    DWORD   rc;
    char    FileName[128];
    char    command[512];
    char    temp[512];

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    memset(command, '\0', sizeof(command));
    strcpy(command,"detach -q ");
    rc = 0;
    if (!stricmp(type, "drive"))
    {
        sprintf(temp, "-D %s ", Name);
        strcat(command, temp);
    }
    else if (!stricmp(type, "locker"))
    {
        strcat(command, Name);
    }
    else
    {
        return(LeaveThread(1, "nb_Detach failed unknown type: %s\n", CMD_DETACH));
    }

    StartFirstTimer();
    rc = system(command);

    if (rc)
    {
        EndFirstTimer(CMD_DETACH, 0);
        sprintf(temp, "detach failed on %s\n", command);
        LeaveThread(rc, temp, CMD_DETACH);
        sprintf(temp, "ERROR: Thread %d - detach failed on \"%s\"\n", ProcessNumber, command);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_DETACH, 1);
    return(0);
}

int nb_CreateFile(char *path, DWORD size)
{
    char    NewPath[256];
    char    Buffer[512];
    char    temp[512];
    char    FileName[128];
    HANDLE  fHandle;
    DWORD   Moved;
    DWORD   BytesWritten;
    DWORD   BytesToWrite;
    BOOL    rc;
    DWORD   gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    if (strlen(path) == 0)
    {
        return(LeaveThread(1, "nb_CreateFile failed no path specified\n", CMD_CREATEFILE));
    }

    strcpy(NewPath, path);

    StartFirstTimer();
    fHandle = CreateFile(NewPath,
                         GENERIC_READ | GENERIC_WRITE | STANDARD_RIGHTS_ALL,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL,
                         CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);
    gle = GetLastError();
    if (fHandle == INVALID_HANDLE_VALUE)
    {
        EndFirstTimer(CMD_CREATEFILE, 0);
        sprintf(temp, "Create file failed on \"%s\" GLE(0x%x)\n", NewPath, gle);
        LeaveThread(gle, temp, CMD_CREATEFILE);
        sprintf(temp, "ERROR: Thread %d - Create file failed on \"%s\" GLE(0x%x)\n", ProcessNumber, NewPath, gle);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_CREATEFILE, 1);
    Moved = SetFilePointer(fHandle,
                           size - sizeof(Buffer) <= 0 ? 0 : size - sizeof(Buffer),
                           NULL,
                           FILE_BEGIN);
    memset(Buffer, 'A', sizeof(Buffer));
    BytesToWrite = sizeof(Buffer);
    rc = WriteFile(fHandle, Buffer, BytesToWrite, &BytesWritten, NULL);

    FlushFileBuffers(fHandle);
    CloseHandle(fHandle);

    return(0);
}

int nb_CopyFile(char *Source, char *Destination)
{
    DWORD   rc;
    char    FileName[128];
    char    temp[512];
    char    command[256];
    char    NewSource[256];
    char    NewDestination[256];

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    if ((strlen(Source) == 0) || (strlen(Destination) == 0))
    {
        return(LeaveThread(1, "nb_CopyFile failed to copy files: either source or destination path not specified\n", CMD_COPYFILES));
    }
    strcpy(NewSource, Source);
    strcpy(NewDestination, Destination);

    memset(command, '\0', sizeof(command));
    sprintf(command, "copy /V /Y /B %s %s > .\\test\\%s%d", NewSource, NewDestination, HostName, ProcessNumber);

    StartFirstTimer();
    rc = system(command);

    if (rc)
    {
        EndFirstTimer(CMD_COPYFILES, 0);
        sprintf(temp, "copy failed on \"%s\"\n", command);
        LeaveThread(rc, temp, CMD_COPYFILES);
        sprintf(temp, "FAILURE: Thread %d - copy failed on \"%s\"\n", ProcessNumber, command);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        if (verbose)
            printf("%s", temp);
        return(-1);
    }
    EndFirstTimer(CMD_COPYFILES, 1);
    return(0);
}

int nb_DeleteFile(char *path)
{
    DWORD   rc;
    char    FileName[128];
    char    command[256];
    char    NewPath[256];
    char    temp[512];

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    if (strlen(path) == 0)
    {
        return(LeaveThread(1, "nb_DeleteFile failed to delete files: no path specified\n", CMD_DELETEFILES));
    }
    strcpy(NewPath, path);

    memset(command, '\0', sizeof(command));
    sprintf(command, "del /Q %s", NewPath);

    StartFirstTimer();
    rc = system(command);

    if (rc)
    {
        EndFirstTimer(CMD_DELETEFILES, 0);
        sprintf(temp, "del failed on \"%s\"\n", NewPath);
        LeaveThread(rc, temp, CMD_DELETEFILES);
        sprintf(temp, "ERROR: Thread %d - del failed on \"%s\"\n", ProcessNumber, command);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_DELETEFILES, 1);
    return(0);
}

int nb_xcopy(char *Source, char *Destination)
{
    intptr_t   rc;
    char    FileName[128];
    char    temp[512];
    char    command[256];
    char    NewSource[256];
    char    NewDestination[256];

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    if ((strlen(Source) == 0) || (strlen(Destination) == 0))
    {
        return(LeaveThread(1, "nb_xcopy failed to xcopy: either source or destination is missing\n", CMD_XCOPY));
    }
    strcpy(NewSource, Source);
    strcpy(NewDestination, Destination);
    memset(command, '\0', sizeof(command));
    sprintf(command, "xcopy /E /I /V /Y /Q %s %s > .\\test\\%s%d", NewSource, NewDestination, HostName, ProcessNumber);

    StartFirstTimer();
    rc = SystemCall(command);

    if (rc)
    {
        EndFirstTimer(CMD_XCOPY, 0);
        sprintf(temp, "xcopy failed on %s\n", command);
        LeaveThread((int)rc, temp, CMD_XCOPY);
        sprintf(temp, "FAIURE: Thread %d - xcopy failed on \"%s\"\n", ProcessNumber, command);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_XCOPY, 1);
    return(0);
}

int nb_Move(char *Source, char *Destination)
{
    DWORD   rc;
    char    command[256];
    char    FileName[128];
    char    temp[512];
    char    NewSource[256];
    char    NewDestination[256];

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    if ((strlen(Source) == 0) || (strlen(Destination) == 0))
    {
        return(LeaveThread(1, "nb_Move failed to move: either source or destination is missing\n", CMD_MOVE));
    }
    strcpy(NewSource, Source);
    strcpy(NewDestination, Destination);
    memset(command, '\0', sizeof(command));
    sprintf(command, "move /Y %s %s > .\\test\\%s%d", NewSource, NewDestination, HostName, ProcessNumber);
    StartFirstTimer();
    rc = system(command);

    if (rc)
    {
        EndFirstTimer(CMD_MOVE, 0);
        sprintf(temp, "move failed on \"%s\"\n", command);
        LeaveThread(rc, temp, CMD_MOVE);
        sprintf(temp, "FAILURE: Thread %d - move failed on \"%s\"\n", ProcessNumber, command);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_MOVE, 1);
    return(0);
}

int nb_createx(char *fname, unsigned create_options, unsigned create_disposition, int handle)
{
    HANDLE  fd;
    int     i;
    uint32  desired_access;
    char    FileName[128];
    char    temp[512];
    pstring path;
    DWORD gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    strcpy(path, AfsLocker);
    strcat(path, fname);
    if (create_options & FILE_DIRECTORY_FILE)
    {
        desired_access = FILE_READ_DATA;
    }
    else
    {
        desired_access = FILE_READ_DATA | FILE_WRITE_DATA;
    }

    StartFirstTimer();
    fd = CreateObject(path,
                       desired_access,
                       0x0,
                       FILE_SHARE_READ|FILE_SHARE_WRITE,
                       create_disposition,
                       create_options);

    if (fd == INVALID_HANDLE_VALUE && handle != -1)
    {
        gle = GetLastError();
        if (create_options & FILE_DIRECTORY_FILE)
        {
            EndFirstTimer(CMD_NTCREATEX, 0);
            LeaveThread(gle, "", CMD_NTCREATEX);
            sprintf(temp, "Directory: unable to create directory %s\n", path);
            if (verbose)
                printf("%s", temp);
            LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
            return(-1);
        }
        else
        {
            EndFirstTimer(CMD_NTCREATEX, 0);
            LeaveThread(gle, "", CMD_NTCREATEX);
            sprintf(temp, "File: unable to create file %s\n", path);
            if (verbose)
                printf("%s", temp);
            LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
            return(-1);
        }
    }

    EndFirstTimer(CMD_NTCREATEX, 1);

    if (fd != INVALID_HANDLE_VALUE && handle == -1)
    {
        CloseHandle(fd);
        nb_unlink(fname);
        return(0);
    }

    if (fd == INVALID_HANDLE_VALUE && handle == -1)
        return(0);

    for (i = 0; i < MAX_FILES; i++)
    {
        if (ftable[i].handle == 0)
            break;
    }
    if (i == MAX_FILES)
    {
        printf("(%d) file table full for %s\n", LineCount, path);
        return(LeaveThread(1, "file table is full\n", CMD_NTCREATEX));
    }
    ftable[i].handle = handle;
    ftable[i].fd = fd;
    if (ftable[i].name)
        free(ftable[i].name);
    ftable[i].name = strdup(path);
    ftable[i].reads = ftable[i].writes = 0;
    return(0);
}

int nb_writex(int handle, int offset, int size, int ret_size)
{
    int     i;
    ssize_t status;
    char    FileName[128];
    char    temp[512];
    unsigned char magic = (unsigned char)getpid();
    DWORD gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    if (IoBuffer[0] != magic ||
        IoBuffer[1] != magic ||
        IoBuffer[2] != magic ||
        IoBuffer[3] != magic)
        memset(IoBuffer, magic, BufferSize);

    if ((i = FindHandle(handle)) == -1)
        return(-1);
    StartFirstTimer();
    status = nb_write(ftable[i].fd, IoBuffer, offset, size);
    if (status != ret_size)
    {
        gle = GetLastError();
        EndFirstTimer(CMD_WRITEX, 0);
        LeaveThread(gle, "", CMD_WRITEX);
        if (status == 0)
            sprintf(temp, "File: %s. wrote %d bytes, got %d bytes\n", ftable[i].name, size, status);
        if (status == -1)
            sprintf(temp, "File: %s. On write, cannot set file pointer\n", ftable[i].name);
        if (verbose)
            printf("%s", temp);
        nb_close(handle);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
	}
    EndFirstTimer(CMD_WRITEX, 1);

    ftable[i].writes++;
    return(0);
}

int nb_lock(int handle, int offset, int size, int timeout, unsigned char locktype, NTSTATUS exp)
{
/*
    int         i;
    NTSTATUS    ret;
    char        FileName[128];
    char        temp[512];

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    if ((i = FindHandle(handle)) == -1)
    {
        LeaveThread(0, "", CMD_LOCKINGX);
        sprintf(temp, "File unlock: Cannot find handle for %s", ftable[i].name);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }

    StartFirstTimer();
    ret = cli_locktype(c, i, offset, size, timeout, locktype);

    if (ret != exp)
    {
        EndFirstTimer(CMD_LOCKINGX, 0);
        LeaveThread(0, "", CMD_LOCKINGX);
        sprintf(temp, "(%d) ERROR: lock failed on handle %d ofs=%d size=%d timeout= %d exp=%d fd %d errno %d (%s)\n",
               LineCount, handle, offset, size, timeout, exp, ftable[i].fd, exp, "");
        sprintf(temp, "File unlock: lock failed %s", ftable[i].name);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_LOCKINGX, 1);
*/
    return(0);
}

int nb_readx(int handle, int offset, int size, int ret_size)
{
    int     i;
    ssize_t ret;
    char    FileName[128];
    char    temp[512];
    DWORD   gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    if ((i = FindHandle(handle)) == -1)
        return(-1);

    StartFirstTimer();
    ret = nb_read(ftable[i].fd, IoBuffer, offset, size);
    gle = GetLastError();
    if ((ret != size) && (ret != ret_size))
    {
        EndFirstTimer(CMD_READX, 0);
        LeaveThread(gle, "", CMD_READX);
        if (ret == 0)
            sprintf(temp, "File: read failed on index=%d, offset=%d ReadSize=%d ActualRead=%d handle=%p GLE(0x%x)\n",
                    handle, offset, size, ret, ftable[i].fd, gle);
        if (ret == -1)
            sprintf(temp, "File: %s. On read, cannot set file pointer GLE(0x%x)\n", ftable[i].name, gle);
        if (verbose)
            printf("%s", temp);
        nb_close(handle);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_READX, 1);
    ftable[i].reads++;
    return(0);
}

int nb_close(int handle)
{
    int     i;
    int     ret;
    char    FileName[128];
    char    temp[512];
    DWORD   gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    if ((i = FindHandle(handle)) == -1)
        return(0);

    StartFirstTimer();
    ret = nb_close1(ftable[i].fd);
    gle = GetLastError();
    EndFirstTimer(CMD_CLOSE, ret);
    if (!ret)
    {
        LeaveThread(gle, "", CMD_CLOSE);
        sprintf(temp, "(%d) close failed on handle %d\n", LineCount, handle);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }

    ftable[i].handle = 0;
    ftable[i].fd = 0;
    if (ftable[i].name)
        free(ftable[i].name);
    ftable[i].name = NULL;
    return(0);
}

int nb_rmdir(char *fname)
{
    int     rc;
    pstring path;
    char    FileName[128];
    char    temp[512];
    DWORD   gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    strcpy(path, AfsLocker);
    strcat(path, fname);

    StartFirstTimer();
    rc = RemoveDirectory(path);
    gle = GetLastError();
    EndFirstTimer(CMD_RMDIR, rc);

    if (!rc)
    {
        LeaveThread(gle, "", CMD_RMDIR);
        sprintf(temp, "Directory: RemoveDirectory %s failed GLE(0x%x)\n", fname, gle);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    return(0);
}

int nb_rename(char *old, char *New)
{
    int     rc;
    pstring opath;
    pstring npath;
    char    FileName[128];
    char    temp[512];
    DWORD   gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    strcpy(opath, AfsLocker);
    strcat(opath, old);
    strcpy(npath, AfsLocker);
    strcat(npath, New);

    StartFirstTimer();
    rc = MoveFileEx(opath, npath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    gle = GetLastError();
    EndFirstTimer(CMD_RENAME, rc);

    if (!rc)
    {
        LeaveThread(gle, "", CMD_RENAME);
        sprintf(temp, "File: rename %s %s failed GLE(0x%x)\n", old, New, gle);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    return(0);
}

/*
 * Type is used to determine whether the file is expected
 * to exist or not.  It is overloaded (temporarily) to control
 * Flag which indicates whether an error is treated as an error
 * or not.  The StreamFiles.txt script does not have the Type
 * parameter set correctly for all 120,000+ lines.  As a result
 * it is not possible to enforce the presence test throughout
 * the entire script.
 */
int nb_qpathinfo(char *fname, int Type)
{
    pstring path;
    int     rc;
    char    FileName[128];
    char    temp[512];
    DWORD   gle = 0;

    if (Type == 1111) {
        EnforcePathInfoErrors = 1;
        Type = 1;
    } else if (Type == 1001) {
        EnforcePathInfoErrors = 0;
        Type = 1;
    } else if (Type == 1000) {
        EnforcePathInfoErrors = 0;
        Type = 0;
    } else if (Type == 1110) {
        EnforcePathInfoErrors = 1;
        Type = 0;
    }

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    strcpy(path, AfsLocker);
    strcat(path, fname);

    StartFirstTimer();
    rc = GetPathInfo(path, NULL, NULL, NULL, NULL, NULL);
    if (rc == 0)
        gle = GetLastError();
    if (strstr(fname, "~TS"))
    {
        if (rc == 0)
            rc = 1;
        else
            rc = 0;
    }

    if (!EnforcePathInfoErrors)
    {
        if (Type)
        {
            if (rc)
                rc = 0;
            else
                rc = 1;
        }
        if (!rc)
        {
            EndFirstTimer(CMD_QUERY_PATH_INFO, 0);
            LeaveThread(gle, "", CMD_QUERY_PATH_INFO);
            sprintf(temp, "File: qpathinfo failed for %s type %d GLE(0x%x)\n", path, Type, gle);
            if (verbose)
                printf("%s", temp);
            LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
            return(-1);
        }
    }
    EndFirstTimer(CMD_QUERY_PATH_INFO, 1);
    return(0);
}

int nb_qfileinfo(int handle)
{
    int     i;
    int     rc;
    char    FileName[128];
    char    temp[512];
    DWORD   gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    if ((i = FindHandle(handle)) == -1)
        return(-1);

    StartFirstTimer();
    rc = GetFileInfo(ftable[i].name, ftable[i].fd, NULL, NULL, NULL, NULL, NULL, NULL);
    gle = GetLastError();
    if (!rc)
    {
        EndFirstTimer(CMD_QUERY_FILE_INFO, 0);
        LeaveThread(gle, "", CMD_QUERY_FILE_INFO);
        sprintf(temp, "File: qfileinfo failed for %s GLE(0x%x)\n", ftable[i].name, gle);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_QUERY_FILE_INFO, 1);

    return(0);
}

int nb_qfsinfo(int level)
{

//	int     bsize;
//    int     total;
//    int     avail;
    int     rc;
    char    FileName[128];
    char    temp[512];
    char    Path[512];
    ULARGE_INTEGER FreeBytesAvailable;
    ULARGE_INTEGER TotalNumberOfBytes;
    ULARGE_INTEGER TotalNumberOfFreeBytes;
    DWORD   gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    sprintf(Path, "%s\\%s%05d", AfsLocker, HostName, LogID);

    StartFirstTimer();
    rc = GetDiskFreeSpaceEx(Path, &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes);
//    rc = cli_dskattr(c, &bsize, &total, &avail);
    gle = GetLastError();

    if (!rc)
    {
        EndFirstTimer(CMD_QUERY_FS_INFO, 0);
        LeaveThread(gle, "", CMD_QUERY_FS_INFO);
        sprintf(temp, "File: Disk free space failed GLE(0x%x)\n", gle);
        if (verbose)
            printf("%s", temp);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_QUERY_FS_INFO, 1);

    return(0);
}

void find_fn(file_info *finfo, char *name, void *state)
{
	/* noop */
}

int nb_findfirst(char *mask)
{
    int     rc;
    char    FileName[128];
    char    NewMask[512];
    char    temp[512];

    if (strstr(mask, "<.JNK"))
        return(0);

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    strcpy(NewMask, AfsLocker);
    strcat(NewMask, mask);

    StartFirstTimer();
    rc = GetFileList(NewMask, (void *)find_fn, NULL);

    if (!rc)
    {
        EndFirstTimer(CMD_FIND_FIRST, 0);
        sprintf(temp, "File: findfirst cannot find for %s\n", mask);
        if (verbose)
            printf("%s", temp);
        LeaveThread(1, temp, CMD_FIND_FIRST);
        LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
        return(-1);
    }
    EndFirstTimer(CMD_FIND_FIRST, 1);
    return(0);
}

int nb_flush(int handle)
{
    int i;

    if ((i = FindHandle(handle)) == -1)
        return(-1);

    FlushFileBuffers(ftable[i].fd);
    return(0);
}

static int total_deleted;

void delete_fn(file_info *finfo, const char *name, void *state)
{
    int     rc;
    char    temp[512];
    char    s[1024];
    char    FileName[128];
    DWORD   gle;

    sprintf(FileName, "Thread_%05d.log", ProcessNumber);

    if (finfo->mode & aDIR)
    {
        char s2[1024];
        sprintf(s2, "%s\\*", name);
        GetFileList(s2, delete_fn, NULL);
        sprintf(s, "%s", &name[strlen(AfsLocker)]);
        nb_rmdir(s);
    }
    else
    {
        rc = DeleteFile(name);
        gle = GetLastError();
        if (!rc)
        {
            LeaveThread(gle, "", CMD_UNLINK);
            sprintf(temp, "FILE: DeleteFile %s failed GLE(0x%x)\n", name, gle);
            if (verbose)
                printf("%s", temp);
            LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
            return;
        }
    }
    return;
}

int nb_deltree(char *dname)
{
    int     rc;
    char    mask[1024];
    pstring path;
    DWORD   gle;

    strcpy(path, AfsLocker);
    strcat(path, dname);
    sprintf(mask, "%s\\*", path);

    total_deleted = 0;

    StartFirstTimer();
    GetFileList(mask, delete_fn, NULL);

//    pstrcpy(path, AfsLocker);
//    pstrcat(path, dname);
    rc = RemoveDirectory(path);
    gle = GetLastError();
    EndFirstTimer(CMD_DELTREE, rc);
    if (!rc)
    {
        char FileName[256];
        char temp[512];

        if ((gle != ERROR_FILE_NOT_FOUND) && (gle != ERROR_PATH_NOT_FOUND))
        {
            LeaveThread(gle, "", CMD_DELTREE);
            sprintf(FileName, "Thread_%05d.log", ProcessNumber);
            sprintf(temp, "ERROR: Thread %d - Unable to remove %s.\n", ProcessNumber, path);
            LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
            SetLastError(gle);
            if (verbose)
                printf(temp);
            return(-1);
        }
    }
    return(0);
}


int nb_cleanup(char *cname)
{
    char temp[256];

    strcpy(temp, "\\clients\\client1");
    SubstituteString(temp, "client1", cname, sizeof(temp));
    SubstituteString(temp, "clients", HostName, sizeof(temp));
    nb_deltree(temp);
    return(0);
}

int LeaveThread(int status, char *Reason, int cmd)
{
    char    FileName[256];
    char    temp[512];
    DWORD   rc;

    if (cmd != -1)
        ++ThreadCommandInfo[cmd].ErrorCount;

    if (strlen(Reason) == 0)
    {
        if (status == 0)
            rc = GetLastError();
        else
            rc = status;
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL,
                      rc,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      pExitStatus->Reason,
                      sizeof(pExitStatus->Reason),
                      NULL);
        LastKnownError = rc;
    }
    else
        strcpy(pExitStatus->Reason, Reason);

    if (strlen(pExitStatus->Reason) == 0)
        strcpy(pExitStatus->Reason, "\n");
    if (pExitStatus->Reason[strlen(pExitStatus->Reason) - 1] != '\n')
        strcat(pExitStatus->Reason, "\n");
    sprintf(FileName, "Thread_%05d.log", ProcessNumber);
    if (strlen(Reason) == 0)
        sprintf(temp, "ERROR(%d): Thread %d - (%d) %s", LineCount, ProcessNumber, rc, pExitStatus->Reason);
    else
        sprintf(temp, "ERROR(%d): Thread %d - %s", LineCount, ProcessNumber, pExitStatus->Reason);
    LogMessage(ProcessNumber, HostName, FileName, temp, LogID);
    if (verbose)
        printf("%s", temp);
    pExitStatus->ExitStatus = status;
    if (AfsTrace)
        DumpAFSLog(HostName, LogID);
    (*pThreadStatus) = 0;
    return(-1);
}

void StartFirstTimer(void)
{

    EndSecondTime(CMD_NONAFS);
    TickCount1 = GetTickCount();
}

void EndFirstTimer(int cmd, int Type)
{
    DWORD   MilliTick;
    DWORD   cmd_time;

    ThreadCommandInfo[cmd].count++;
    cmd_time = 0;
    MilliTick = GetTickCount() - TickCount1;
    if (MilliTick <= 0)
    {
        StartSecondTime(CMD_NONAFS);
        return;
    }

    ThreadCommandInfo[cmd].MilliSeconds += MilliTick;
    while (ThreadCommandInfo[cmd].MilliSeconds > 1000)
    {
        ThreadCommandInfo[cmd].MilliSeconds -= 1000;
        cmd_time += 1;
    }

    if (cmd_time == 0)
    {
        ThreadCommandInfo[cmd].min_sec = cmd_time;
        StartSecondTime(CMD_NONAFS);
        return;
    }

    if (!Type)
        ThreadCommandInfo[cmd].ErrorTime += cmd_time;
    else
    {
        ThreadCommandInfo[cmd].total_sec += cmd_time;
        if (cmd_time < (int)ThreadCommandInfo[cmd].min_sec)
            ThreadCommandInfo[cmd].min_sec = cmd_time;
        if ((int)ThreadCommandInfo[cmd].max_sec < cmd_time)
            ThreadCommandInfo[cmd].max_sec = cmd_time;
    }

    StartSecondTime(CMD_NONAFS);
}

void StartSecondTime(int cmd)
{

    TickCount2 = GetTickCount();

}

void EndSecondTime(int cmd)
{
    DWORD   MilliTick;
    DWORD   cmd_time;

    ThreadCommandInfo[cmd].count++;
    cmd_time = 0;
    MilliTick = GetTickCount() - TickCount2;

    if (MilliTick <= 0)
        return;

    ThreadCommandInfo[cmd].MilliSeconds += MilliTick;
    while (ThreadCommandInfo[cmd].MilliSeconds > 1000)
    {
        ThreadCommandInfo[cmd].MilliSeconds -= 1000;
        cmd_time += 1;
    }
    if (cmd_time == 0)
    {
        ThreadCommandInfo[cmd].min_sec = cmd_time;
        return;
    }
    if (cmd_time < (int)ThreadCommandInfo[cmd].min_sec)
        ThreadCommandInfo[cmd].min_sec = cmd_time;
    if ((int)ThreadCommandInfo[cmd].max_sec < cmd_time)
        ThreadCommandInfo[cmd].max_sec = cmd_time;
    ThreadCommandInfo[cmd].total_sec += cmd_time;
}


intptr_t SystemCall(char *command)
{
    intptr_t rc;
    char    *argv[6];

    argv[0] = getenv("COMSPEC");
    argv[1] = "/q";
    argv[2] = "/c";
    argv[3] = (char *)command;
    argv[4] = NULL;

    rc = spawnve(_P_WAIT,argv[0],argv,NULL);
// != -1 || (errno != ENOENT && errno != EACCES))
    return(rc);
}

HANDLE CreateObject(const char *fname, uint32 DesiredAccess,
		 uint32 FileAttributes, uint32 ShareAccess,
		 uint32 CreateDisposition, uint32 CreateOptions)
{
    HANDLE   fd;
    DWORD dwCreateDisposition = 0;
    DWORD dwDesiredAccess = 0;
    DWORD dwShareAccess = 0;
    DWORD dwFlags = 0;

    if (CreateOptions & FILE_DIRECTORY_FILE)
    {
        DWORD rc;

        if (!CreateDirectory(fname, NULL) && (rc = GetLastError()) != ERROR_ALREADY_EXISTS)
        {
            SetLastError(rc);
            fd = INVALID_HANDLE_VALUE;
        }
        else
        {
            fd = CreateFile(fname, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        }
    }
    else
    {
        dwDesiredAccess = 0;
        if (DesiredAccess & FILE_READ_DATA)
            dwDesiredAccess |= GENERIC_READ;
        if (DesiredAccess & FILE_WRITE_DATA)
            dwDesiredAccess |= GENERIC_WRITE;
        dwShareAccess = ShareAccess;
        dwShareAccess |= FILE_SHARE_DELETE;
        dwCreateDisposition = OPEN_ALWAYS;
        if (CreateDisposition == 1)
            dwCreateDisposition = OPEN_EXISTING;
        fd = CreateFile(fname, dwDesiredAccess, ShareAccess, NULL, dwCreateDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    return(fd);
}

BOOL nb_close1(HANDLE fd)
{
    int dwFlags = 0;
    int rc = 1;

    if (fd != INVALID_HANDLE_VALUE)
    {
        if (rc = GetHandleInformation(fd, &dwFlags))
            CloseHandle(fd);
    }
    return(rc);
}

/****************************************************************************
  do a directory listing, calling fn on each file found
  this uses the old SMBsearch interface. It is needed for testing Samba,
  but should otherwise not be used
  ****************************************************************************/
int nb_list_old(const char *Mask, void (*fn)(file_info *, const char *, void *), void *state)
{
    int     num_received = 0;
    pstring mask;
    char    temp[512];
    char    cFileName[1024];
    int     dwFileAttributes;
    HANDLE  hFind;
    void    *FileData;

    strcpy(mask,Mask);


    if (!strcmp(&mask[strlen(mask)-2], "\"*"))
    {
        strcpy(&mask[strlen(mask)-2], "*");
    }
    FileData = NULL;
    dwFileAttributes = 0;
    memset(cFileName, '\0', sizeof(cFileName));
    hFind = WinFindFirstFile(mask, &FileData, cFileName, &dwFileAttributes);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return(0);
    }
    mask[strlen(mask) - 1] = '\0';
    while (1)
    {
        if (cFileName[0] != '.')
        {
            file_info finfo;

            memset(&finfo, '\0', sizeof(finfo));
            if (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                finfo.mode = aDIR;
            strcpy(finfo.name, cFileName);
            sprintf(temp, "%s%s", mask, cFileName);
    		fn(&finfo, temp, state);
            ++num_received;
        }
        memset(cFileName, '\0', sizeof(cFileName));
        dwFileAttributes = 0;
        if (!WinFindNextFile(hFind, &FileData, cFileName, &dwFileAttributes))
            break;
    }
    FindClose(hFind);
    return(num_received);
}


/****************************************************************************
  do a directory listing, calling fn on each file found
  this auto-switches between old and new style
  ****************************************************************************/
int GetFileList(char *Mask, void (*fn)(file_info *, const char *, void *), void *state)
{
    return(nb_list_old(Mask, fn, state));
}

HANDLE WinFindFirstFile(char *Mask, void **FileData, char *cFileName, int *dwFileAttributes)
{
    HANDLE  rc;
    static WIN32_FIND_DATAW FileDataW;
    static WIN32_FIND_DATAA FileDataA;

    memset(&FileDataA, '\0', sizeof(FileDataA));
    rc = FindFirstFile(Mask, &FileDataA);
    if (rc != INVALID_HANDLE_VALUE)
    {
        (*FileData) = (void *)&FileDataA;
        (*dwFileAttributes) = FileDataA.dwFileAttributes;
        strcpy(cFileName, FileDataA.cFileName);
    }

    return(rc);
}

int WinFindNextFile(HANDLE hFind, void **FileData, char *cFileName, int *dwFileAttributes)
{
    int     rc;
    WIN32_FIND_DATAA *FileDataA;

    FileDataA = (WIN32_FIND_DATAA *)(*FileData);
    if (!(rc = FindNextFile(hFind, FileDataA)))
    {
        return(rc);
    }
    (*dwFileAttributes) = FileDataA->dwFileAttributes;
    strcpy(cFileName, FileDataA->cFileName);

    return(rc);
}

void SubstituteString(char *s,const char *pattern,const char *insert, size_t len)
{
    char *p;
    ssize_t ls,lp,li;

    if (!insert || !pattern || !s) return;

    ls = (ssize_t)strlen(s);
    lp = (ssize_t)strlen(pattern);
    li = (ssize_t)strlen(insert);

    if (!*pattern) return;

    while (lp <= ls && (p = strstr(s,pattern)))
    {
        if (len && (ls + (li-lp) >= (int)len))
        {
            break;
        }
        if (li != lp)
        {
            memmove(p+li,p+lp,strlen(p+lp)+1);
        }
        memcpy(p, insert, li);
        s = p + li;
        ls += (li-lp);
    }
}

#define CHANGE_TIME(A,B) \
B.tm_hour = A.wHour; \
B.tm_sec = A.wSecond; \
B.tm_min = A.wMinute; \
B.tm_mon = A.wMonth - 1; \
B.tm_yday = 0; \
B.tm_year = A.wYear - 1900; \
B.tm_wday = A.wDayOfWeek - 1; \
B.tm_isdst = -1; \
B.tm_mday = A.wDay;


BOOL GetPathInfo(const char *fname,
		   time_t *c_time, time_t *a_time, time_t *m_time,
		   size_t *size, uint16 *mode)
{
    WIN32_FILE_ATTRIBUTE_DATA FileInfo;
    int         rc;
    SYSTEMTIME  SystemTime;
    struct tm   tm_time;

    rc = GetFileAttributesEx(fname, GetFileExInfoStandard, &FileInfo);
    if (rc != 0)
    {
        if (c_time)
        {
            rc = FileTimeToSystemTime(&FileInfo.ftCreationTime, &SystemTime);
            CHANGE_TIME(SystemTime, tm_time)
            (*c_time) = mktime(&tm_time);
        }
        if (a_time)
        {
            rc = FileTimeToSystemTime(&FileInfo.ftLastAccessTime, &SystemTime);
            CHANGE_TIME(SystemTime, tm_time)
            (*a_time) = mktime(&tm_time);
        }
        if (m_time)
        {
            rc = FileTimeToSystemTime(&FileInfo.ftLastWriteTime, &SystemTime);
            CHANGE_TIME(SystemTime, tm_time)
            (*m_time) = mktime(&tm_time);
        }
        if (size)
        {
            rc = 1;
            if (!(FileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                *size = FileInfo.nFileSizeLow;
        }
        if (mode)
        {
            rc = 1;
            (*mode) = 0;
        }
    }
    return(rc);
}

/****************************************************************************
send a qfileinfo call
****************************************************************************/
BOOL GetFileInfo(char *FileName, HANDLE fd,
                   uint16 *mode, size_t *size,
                   time_t *c_time, time_t *a_time, time_t *m_time,
                   time_t *w_time)
{
    WIN32_FILE_ATTRIBUTE_DATA FileInfo;
    int         rc;
    SYSTEMTIME  SystemTime;
    struct tm   tm_time;

    rc = GetFileAttributesEx(FileName, GetFileExInfoStandard, &FileInfo);
    if (rc != 0)
    {
        if (c_time)
        {
            rc = FileTimeToSystemTime(&FileInfo.ftCreationTime, &SystemTime);
            CHANGE_TIME(SystemTime, tm_time)
            (*c_time) = mktime(&tm_time);
        }
        if (a_time)
        {
            rc = FileTimeToSystemTime(&FileInfo.ftLastAccessTime, &SystemTime);
            CHANGE_TIME(SystemTime, tm_time)
            (*a_time) = mktime(&tm_time);
        }
        if (m_time)
        {
            rc = FileTimeToSystemTime(&FileInfo.ftLastWriteTime, &SystemTime);
            CHANGE_TIME(SystemTime, tm_time)
            (*m_time) = mktime(&tm_time);
        }
        if (size)
        {
            rc = 0;
            if (!(FileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
               *size = FileInfo.nFileSizeLow;
	    }
	    if (mode)
        {
            rc = 0;
            (*mode) = 0;
        }
    }
    return(rc);
}

/****************************************************************************
  Read size bytes at offset offset using SMBreadX.
****************************************************************************/

ssize_t nb_read(HANDLE fd, char *IoBuffer, off_t offset, size_t size)
{
    DWORD   total = 0;
    int     rc;
    DWORD   LowDword;

    if (size == 0)
        return(0);

    LowDword = SetFilePointer(fd, offset, 0, FILE_BEGIN);

    if (LowDword == INVALID_SET_FILE_POINTER)
        return(-1);
    rc = ReadFile(fd, IoBuffer, (DWORD)size, &total, NULL);
    if (!rc)
        return(rc);

    return(total);
}

/****************************************************************************
  write to a file
****************************************************************************/

ssize_t nb_write(HANDLE fd, char *IoBuffer, off_t offset, size_t size)
{
    DWORD   bwritten = 0;
    int     rc;
    DWORD   LowDword;

    LowDword = SetFilePointer(fd, offset, 0, FILE_BEGIN);
    if (LowDword == INVALID_SET_FILE_POINTER)
        return(-1);
    rc = WriteFile(fd, IoBuffer, (DWORD)size, &bwritten, NULL);
    if (!rc)
        return(rc);
    FlushFileBuffers(fd);
    return(bwritten);
}

