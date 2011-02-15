/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* I/O operations for the Windows NT platforms. */

#include <afsconfig.h>
#include <afs/param.h>


#ifdef AFS_NT40_ENV
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <windows.h>
#include <winnt.h>
#include <winbase.h>
#include <lock.h>
#include <afs/afsutil.h>
#include "nfs.h"
#include <afs/afsint.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "viceinode.h"
#include <dirent.h>
#include <afs/afs_assert.h>
#include <afs/errmap_nt.h>

#define BASEFILEATTRIBUTE FILE_ATTRIBUTE_NORMAL

/* nt_unlink - unlink a case sensitive name.
 *
 * nt_unlink supports the nt_dec call.
 *
 * This nt_unlink has the delete on last close semantics of the Unix unlink
 * with a minor twist. Subsequent CreateFile calls on this file can succeed
 * if they open for delete. If a CreateFile call tries to create a new file
 * with the same name it will fail. Fortunately, neither case should occur
 * as part of nt_dec.
 */
int
nt_unlink(char *name)
{
    HANDLE fh;

    fh = CreateFile(name,
                    GENERIC_READ | GENERIC_WRITE | DELETE,
		    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		    NULL, OPEN_EXISTING,
		    BASEFILEATTRIBUTE | FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_POSIX_SEMANTICS,
                    NULL);
    if (fh != INVALID_HANDLE_VALUE)
	CloseHandle(fh);
    else {
	errno = nterr_nt2unix(GetLastError(), ENOENT);
	return -1;
    }
    return 0;
}

/* nt_open - open an NT handle for a file.
 *
 * Return Value:
 *	the handle or -1 on error.
 */
FD_t
nt_open(const char *name, int flags, int mode)
{
    HANDLE fh;
    DWORD nt_access = 0;
    DWORD nt_share = FILE_SHARE_READ;
    DWORD nt_create = 0;
    /* Really use the sequential one for data files, random for meta data. */
    DWORD FandA = BASEFILEATTRIBUTE | FILE_FLAG_SEQUENTIAL_SCAN;

    /* set access */
    if ((flags & O_RDWR) || (flags & O_WRONLY)) {
	nt_access |= GENERIC_WRITE;
        nt_share  |= FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    }
    if ((flags & O_RDWR) || (flags == O_RDONLY))
	nt_access |= GENERIC_READ;

    /* set creation */
    switch (flags & (O_CREAT | O_EXCL | O_TRUNC)) {
    case 0:
	nt_create = OPEN_EXISTING;
	break;
    case O_CREAT:
	nt_create = OPEN_ALWAYS;
	break;
    case O_CREAT | O_TRUNC:
	nt_create = CREATE_ALWAYS;
	break;
    case O_CREAT | O_EXCL:
    case O_CREAT | O_EXCL | O_TRUNC:
	nt_create = CREATE_NEW;
	break;
    case O_TRUNC:
	nt_create = TRUNCATE_EXISTING;
	break;
    case O_TRUNC | O_EXCL:
    case O_EXCL:
    default:
	errno = EINVAL;
	return INVALID_FD;
	break;
    }

    fh = CreateFile(name, nt_access, nt_share, NULL, nt_create, FandA, NULL);
    if (fh == INVALID_HANDLE_VALUE) {
        DWORD gle = GetLastError();
        errno = nterr_nt2unix(gle, EBADF);
    }
    return fh;
}

int
nt_close(FD_t fd)
{
    BOOL code;

    code = CloseHandle(fd);
    if (!code) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	return -1;
    }
    return 0;
}

int
nt_write(FD_t fd, void *buf, afs_sfsize_t size)
{
    BOOL code;
    DWORD nbytes;

    code = WriteFile((HANDLE) fd, buf, (DWORD) size, &nbytes, NULL);

    if (!code) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	return -1;
    }
    return (int)nbytes;
}

int
nt_pwrite(FD_t fd, const void * buf, afs_sfsize_t count, afs_foff_t offset)
{
    /*
     * same comment as read
     */

    DWORD nbytes;
    BOOL code;
    OVERLAPPED overlap = {0};
    LARGE_INTEGER liOffset;

    liOffset.QuadPart = offset;
    overlap.Offset = liOffset.LowPart;
    overlap.OffsetHigh = liOffset.HighPart;

    code = WriteFile((HANDLE) fd, (void *)buf, (DWORD) count, &nbytes, &overlap);

    if (!code) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	return -1;
    }
    return (ssize_t)nbytes;
}

int
nt_read(FD_t fd, void *buf, afs_sfsize_t size)
{
    BOOL code;
    DWORD nbytes;

    code = ReadFile((HANDLE) fd, buf, (DWORD) size, &nbytes, NULL);

    if (!code) {
        DWORD gle = GetLastError();
        if (gle != ERROR_HANDLE_EOF) {
	        errno = nterr_nt2unix(GetLastError(), EBADF);
	        return -1;
        }
    }
    return (int)nbytes;
}

int
nt_pread(FD_t fd, void * buf, afs_sfsize_t count, afs_foff_t offset)
{
    /*
     * This really ought to call NtReadFile
     */
    DWORD nbytes;
    BOOL code;
    OVERLAPPED overlap = {0};
    LARGE_INTEGER liOffset;
    /*
     * Cast through a LARGE_INTEGER - make no assumption about
     * byte ordering and leave that to the compiler..
     */
    liOffset.QuadPart = offset;
    overlap.Offset = liOffset.LowPart;
    overlap.OffsetHigh = liOffset.HighPart;

    code = ReadFile((HANDLE) fd, (void *)buf, (DWORD) count, &nbytes, &overlap);

    if (!code) {
        DWORD gle = GetLastError();
        if (gle != ERROR_HANDLE_EOF) {
	        errno = nterr_nt2unix(GetLastError(), EBADF);
	        return -1;
        }
    }
    return (ssize_t)nbytes;
}

afs_sfsize_t
nt_size(FD_t fd)
{
    BY_HANDLE_FILE_INFORMATION finfo;
    LARGE_INTEGER FileSize;

    if (!GetFileInformationByHandle(fd, &finfo))
	return -1;

    FileSize.HighPart = finfo.nFileSizeHigh;
    FileSize.LowPart = finfo.nFileSizeLow;
    return FileSize.QuadPart;
}


int
nt_getFileCreationTime(FD_t fd, FILETIME * ftime)
{
    BY_HANDLE_FILE_INFORMATION finfo;

    if (!GetFileInformationByHandle(fd, &finfo))
	return -1;

    *ftime = finfo.ftCreationTime;

    return 0;
}

int
nt_setFileCreationTime(FD_t fd, FILETIME * ftime)
{
    return !SetFileTime(fd, ftime, NULL, NULL);
}

int
nt_sync(int cdrive)
{
    FD_t drive_fd;
    char sdrive[32];
    int n;

    n = cdrive;
    if (n <= 26) {
	cdrive = 'A' + (n - 1);
    }

    cdrive = _toupper(cdrive);

    (void)sprintf(sdrive, "\\\\.\\%c:", cdrive);
    drive_fd = nt_open(sdrive, O_RDWR, 0666);
    if (drive_fd == INVALID_FD) {
	return -1;
    }

    if (!FlushFileBuffers((HANDLE) drive_fd)) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	nt_close(drive_fd);
	return -1;
    }
    nt_close(drive_fd);
    return 0;
}


/* Currently nt_ftruncate only tested to shrink a file. */
int
nt_ftruncate(FD_t fd, afs_foff_t len)
{
    LARGE_INTEGER length;

    length.QuadPart = len;

    if (SetFilePointerEx(fd, length, NULL, FILE_BEGIN)
        == 0xffffffff) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	return -1;
    }
    if (!SetEndOfFile(fd)) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	return -1;
    }
    return 0;
}


int
nt_fsync(FD_t fd)
{
    int code = FlushFileBuffers(fd);
    return code == 0 ? -1 : 0;
}


int
nt_seek(FD_t fd, afs_foff_t off, int whence)
{
    int code;
    LARGE_INTEGER offset;
    int where;

    if (SEEK_SET == whence) {
	where = FILE_BEGIN;
    } else if (SEEK_END == whence) {
	where = FILE_END;
    } else if (SEEK_CUR == whence) {
	where = FILE_CURRENT;
    } else {
	errno = EINVAL;
	return -1;
    }
    offset.QuadPart = off;

    code = SetFilePointerEx(fd, offset, NULL, where);
    if (0 == code) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	return -1;
    }
    return 0;
}

/* nt_DevToDrive
 * converts a device number (2-25) into a drive letter name.
 *
 * Arguments:
 * drive - assumes drive is a pointer to a string at least 3 bytes long.
 * dev   - drive number 2-25, since A-C already in use.
 *
 * Return Value:
 * Returns pointer to end of drive if successful, else NULL.
 *
 */
void
nt_DevToDrive(char *drive, int dev)
{
    if (dev < 2 || dev > 25) {
	errno = EINVAL;
	return;		/* Invalid drive */
    }
    drive[0] = (char)('A' + dev);
    drive[1] = ':';
    drive[2] = '\0';

    return;

}

/* nt_DriveToDev
 * converts a drive letter to a device number (2-25)
 *
 * Arguments:
 * drive - assumes drive is a pointer to a string at least 3 bytes long.
 *
 * Return Value:
 * dev   - drive number 2-25 if successful (A-C already in use), else -1
 *
 */
int
nt_DriveToDev(char *drive)
{
    int dev = -1;

    if (drive)
	dev = toupper(*drive) - 'A';
    if ((dev < 2) || (dev > 25))
	return -1;
}
#endif
