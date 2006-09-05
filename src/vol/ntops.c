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

RCSID
    ("$Header$");

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
#include <afs/assert.h>
#include <afs/errmap_nt.h>

#define BASEFILEATTRIBUTE FILE_ATTRIBUTE_NORMAL

int Testing = 0;

static void AddToZLCDeleteList(char dir, char *name);

/* nt_unlink - unlink a case sensitive name.
 *
 * nt_unlink supports the nt_dec call.
 *
 * This nt_unlink has the delete on last close semantics of the Unix unlink
 * with a minor twist. Subsequent CreateFile calls on this file can succeed
 * if they open for delete. It's also unclear what happens if a CreateFile
 * call tries to create a new file with the same name. Fortunately, neither
 * case should occur as part of nt_dec.
 */
int
nt_unlink(char *name)
{
    HANDLE fh;

    fh = CreateFile(name, GENERIC_READ | GENERIC_WRITE,
		    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		    NULL, OPEN_EXISTING,
		    BASEFILEATTRIBUTE | FILE_FLAG_DELETE_ON_CLOSE |
		    FILE_FLAG_POSIX_SEMANTICS, NULL);
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
nt_open(char *name, int flags, int mode)
{
    HANDLE fh;
    DWORD nt_access = 0;
    DWORD nt_share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD nt_create = 0;
    /* Really use the sequential one for data files, random for meta data. */
    DWORD FandA = BASEFILEATTRIBUTE | FILE_FLAG_SEQUENTIAL_SCAN;

    /* set access */
    if ((flags & O_RDWR) || (flags & O_WRONLY))
	nt_access |= GENERIC_WRITE;
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
	fh = INVALID_FD;
	errno = nterr_nt2unix(GetLastError(), EBADF);
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
nt_write(FD_t fd, char *buf, size_t size)
{
    BOOL code;
    DWORD nbytes;

    code = WriteFile((HANDLE) fd, (void *)buf, (DWORD) size, &nbytes, NULL);

    if (!code) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	return -1;
    }
    return (int)nbytes;
}

int
nt_read(FD_t fd, char *buf, size_t size)
{
    BOOL code;
    DWORD nbytes;

    code = ReadFile((HANDLE) fd, (void *)buf, (DWORD) size, &nbytes, NULL);

    if (!code) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	return -1;
    }
    return (int)nbytes;
}

int
nt_iread(IHandle_t * h, int offset, char *buf, int size)
{
    int nBytes;
    FdHandle_t *fdP;

    fdP = IH_OPEN(h);
    if (fdP == NULL)
	return -1;

    if (FDH_SEEK(fdP, offset, SEEK_SET) < 0) {
	FDH_REALLYCLOSE(fdP);
	return -1;
    }

    nBytes = FDH_READ(fdP, buf, size);
    FDH_CLOSE(fdP);
    return nBytes;
}

int
nt_iwrite(IHandle_t * h, int offset, char *buf, int size)
{
    int nBytes;
    FdHandle_t *fdP;

    fdP = IH_OPEN(h);
    if (fdP == NULL)
	return -1;

    if (FDH_SEEK(fdP, offset, SEEK_SET) < 0) {
	FDH_REALLYCLOSE(fdP);
	return -1;
    }
    nBytes = FDH_WRITE(fdP, buf, size);
    FDH_CLOSE(fdP);
    return nBytes;
}


int
nt_size(FD_t fd)
{
    BY_HANDLE_FILE_INFORMATION finfo;

    if (!GetFileInformationByHandle(fd, &finfo))
	return -1;

    return finfo.nFileSizeLow;
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
nt_ftruncate(FD_t fd, int len)
{
    if (SetFilePointer(fd, (LONG) len, NULL, FILE_BEGIN)
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
nt_seek(FD_t fd, int off, int where)
{
    int code = SetFilePointer(fd, off, NULL, where);
    return code;
}


/* Inode number format:
 * low 32 bits - if a regular file or directory, the vnode. Else the type.
 * 32-36 - unquifier tag and index into counts array for this vnode. Only
 *         two of the available bits are currently used. The rest are
 *         present in case we ever increase the number of types of volumes
 *         in the volume grou.
 * bit 37 : 1  == special, 0 == regular
 */
#define NT_VNODEMASK    0x00ffffffff
/* While the TAGMASK is 7, note that we are leaving 1 more bit available. */
#define NT_TAGMASK      0x7
#define NT_TAGSHIFT     32
#define NT_INODESPECIAL 0x2000000000

#define NT_MAXVOLS 5		/* Maximum supported number of volumes per volume
				 * group, not counting temporary (move) volumes.
				 * This is the number of separate files, all having
				 * the same vnode number, which can occur in a volume
				 * group at once.
				 */


int nt_SetLinkCount(FdHandle_t * h, Inode ino, int count, int locked);


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
char *
nt_DevToDrive(char *drive, int dev)
{
    if (dev < 2 || dev > 25) {
	errno = EINVAL;
	return NULL;		/* Invalid drive */
    }
    drive[0] = (char)('A' + dev);
    drive[1] = ':';
    drive[2] = '\0';

    return drive + 2;

}

/* Returns pointer to end of name if successful, else NULL. */
char *
nt_HandleToVolDir(char *name, IHandle_t * h)
{
    b32_string_t str1;

    if (!(name = nt_DevToDrive(name, h->ih_dev)))
	return NULL;

    (void)memcpy(name, "\\Vol_", 5);
    name += 5;
    (void)strcpy(name, int_to_base32(str1, h->ih_vid));
    name += strlen(name);
    memcpy(name, ".data", 5);
    name += 5;
    *name = '\0';

    return name;
}

/* nt_HandleToName
 *
 * Constructs a file name for the fully qualified handle.
 */
int
nt_HandleToName(char *name, IHandle_t * h)
{
    b32_string_t str1;
    int tag = (int)((h->ih_ino >> NT_TAGSHIFT) & NT_TAGMASK);
    int vno = (int)(h->ih_ino & NT_VNODEMASK);

    if (!(name = nt_HandleToVolDir(name, h)))
	return -1;

    str1[0] = '\\';
    if (h->ih_ino & NT_INODESPECIAL)
	str1[1] = 'R';
    else {
	if (vno & 0x1)
	    str1[1] = 'Q';
	else
	    str1[1] = ((vno & 0x1f) >> 1) + 'A';
    }

    memcpy(name, str1, 2);
    name += 2;
    (void)memcpy(name, "\\V_", 3);
    name += 3;
    (void)strcpy(name, int_to_base32(str1, vno));
    name += strlen(name);
    *(name++) = '.';
    (void)strcpy(name, int_to_base32(str1, tag));
    name += strlen(name);
    *name = '\0';

    return 0;
}

/* nt_CreateDataDirectories
 *
 * The data for each volume is in a separate directory. The name of the
 * volume is of the form: Vol_NNNNNN.data, where NNNNNN is a base 32
 * representation of the RW volume ID (even where the RO is the only volume
 * on the partition). Below that are separate subdirectories for the 
 * AFS directories and special files. There are also 16 directories for files,
 * hashed on the low 5 bits (recall bit0 is always 0) of the vnode number.
 * These directories are named:
 * A - P - 16 file directories.
 * Q ----- data directory
 * R ----- special files directory
 */
static int
nt_CreateDataDirectories(IHandle_t * h, int *created)
{
    char name[128];
    char *s;
    int i;

    if (!(s = nt_HandleToVolDir(name, h)))
	return -1;

    if (mkdir(name) < 0) {
	if (errno != EEXIST)
	    return -1;
    } else
	*created = 1;

    *s++ = '\\';
    *(s + 1) = '\0';
    for (i = 'A'; i <= 'R'; i++) {
	*s = (char)i;
	if (mkdir(name) < 0 && errno != EEXIST)
	    return -1;
    }
    return 0;
}

/* nt_RemoveDataDirectories
 *
 * Returns -1 on error. Typically, callers ignore this error bcause we
 * can continue running if the removes fail. The salvage process will
 * finish tidying up for us.
 */
static int
nt_RemoveDataDirectories(IHandle_t * h)
{
    char name[128];
    char *s;
    int i;

    if (!(s = nt_HandleToVolDir(name, h)))
	return -1;

    *s++ = '\\';
    *(s + 1) = '\0';
    for (i = 'A'; i <= 'R'; i++) {
	*s = (char)i;
	if (rmdir(name) < 0 && errno != ENOENT)
	    return -1;
    }

    /* Delete the Vol_NNNNNN.data directory. */
    s--;
    *s = '\0';
    if (rmdir(name) < 0 && errno != ENOENT) {
	return -1;
    }

    return 0;
}


/* Create the file in the name space.
 *
 * Parameters stored as follows:
 * Regular files:
 * p1 - volid - implied in containing directory.
 * p2 - vnode - name is <vnode>.<tag> where tag is a file name unqiquifier.
 * p3 - uniq -- creation time - dwHighDateTime
 * p4 - dv ---- creation time - dwLowDateTime
 * Special files:
 * p1 - volid - creation time - dwHighDateTime
 * p2 - vnode - -1 means special, file goes in "S" subdirectory.
 * p3 - type -- name is <type>.<tag> where tag is a file name unqiquifier.
 * p4 - parid - parent volume id - implied in containing directory.
 *
 * Return value is the inode number or (Inode)-1 if error.
 * We "know" there is only one link table, so return EEXIST if there already
 * is a link table. It's up to the calling code to test errno and increment
 * the link count.
 */

/* nt_MakeSpecIno
 *
 * This function is called by VCreateVolume to hide the implementation
 * details of the inode numbers.
 */
Inode
nt_MakeSpecIno(int type)
{
    return ((Inode) type | (Inode) NT_INODESPECIAL);
}

Inode
nt_icreate(IHandle_t * h, char *part, int p1, int p2, int p3, int p4)
{
    char filename[128];
    b32_string_t str1;
    char *p;
    int i;
    FD_t fd;
    int created_dir = 0;
    int code = 0;
    FILETIME ftime;
    IHandle_t tmp;
    FdHandle_t *fdP;
    FdHandle_t tfd;
    int save_errno;

    memset((void *)&tmp, 0, sizeof(IHandle_t));


    tmp.ih_dev = tolower(*part) - 'a';

    if (p2 == -1) {
	tmp.ih_vid = p4;	/* Use parent volume id, where this file will be. */

	if (nt_CreateDataDirectories(&tmp, &created_dir) < 0)
	    goto bad;

	tmp.ih_ino = nt_MakeSpecIno(p3);
	ftime.dwHighDateTime = p1;
	ftime.dwLowDateTime = p2;
    } else {
	/* Regular file or directory.
	 * Encoding: p1 -> dir,  p2 -> name, p3,p4 -> Create time
	 */
	tmp.ih_ino = (Inode) p2;
	tmp.ih_vid = p1;

	ftime.dwHighDateTime = p3;
	ftime.dwLowDateTime = p4;
    }

    /* Now create file. */
    if ((code = nt_HandleToName(filename, &tmp)) < 0)
	goto bad;

    p = filename + strlen(filename);
    p--;
    for (i = 0; i < NT_MAXVOLS; i++) {
	*p = *int_to_base32(str1, i);
	fd = nt_open(filename, O_CREAT | O_RDWR | O_TRUNC | O_EXCL, 0666);
	if (fd != INVALID_FD)
	    break;
	if (p2 == -1 && p3 == VI_LINKTABLE)
	    break;
    }
    if (fd == INVALID_FD) {
	code = -1;
	goto bad;
    }

    tmp.ih_ino &= ~((Inode) NT_TAGMASK << NT_TAGSHIFT);
    tmp.ih_ino |= ((Inode) i << NT_TAGSHIFT);

    if (!code) {
	if (!SetFileTime((HANDLE) fd, &ftime, NULL, NULL)) {
	    errno = EBADF;
	    code = -1;
	}
    }

    if (!code) {
	if (p2 != -1) {
	    if (fd == INVALID_FD) {
		errno = ENOENT;
		code = nt_unlink(filename);
		if (code == -1) {
		}
		code = -1;
		goto bad;
	    }
	    fdP = IH_OPEN(h);
	    if (fdP == NULL) {
		code = -1;
		goto bad;
	    }
	    code = nt_SetLinkCount(fdP, tmp.ih_ino, 1, 0);
	    FDH_CLOSE(fdP);
	} else if (p2 == -1 && p3 == VI_LINKTABLE) {
	    if (fd == INVALID_FD)
		goto bad;
	    /* hack at tmp to setup for set link count call. */
	    tfd.fd_fd = fd;
	    code = nt_SetLinkCount(&tfd, (Inode) 0, 1, 0);
	}
    }

  bad:
    if (fd != INVALID_FD)
	nt_close(fd);

    if (code && created_dir) {
	save_errno = errno;
	nt_RemoveDataDirectories(&tmp);
	errno = save_errno;
    }
    return code ? (Inode) - 1 : tmp.ih_ino;
}


FD_t
nt_iopen(IHandle_t * h)
{
    FD_t fd;
    char name[128];

    /* Convert handle to file name. */
    if (nt_HandleToName(name, h) < 0)
	return INVALID_FD;

    fd = nt_open(name, O_RDWR, 0666);
    return fd;
}

/* Need to detect vol special file and just unlink. In those cases, the
 * handle passed in _is_ for the inode. We only check p1 for the special
 * files.
 */
int
nt_dec(IHandle_t * h, Inode ino, int p1)
{
    int count = 0;
    char name[128];
    int code = 0;
    FdHandle_t *fdP;

    if (ino & NT_INODESPECIAL) {
	IHandle_t *tmp;
	int was_closed = 0;
	WIN32_FIND_DATA info;
	HANDLE dirH;

	/* Verify this is the right file. */
	IH_INIT(tmp, h->ih_dev, h->ih_vid, ino);

	if (nt_HandleToName(name, tmp) < 0) {
	    IH_RELEASE(tmp);
	    errno = EINVAL;
	    return -1;
	}

	dirH =
	    FindFirstFileEx(name, FindExInfoStandard, &info,
			    FindExSearchNameMatch, NULL,
			    FIND_FIRST_EX_CASE_SENSITIVE);
	if (!dirH) {
	    IH_RELEASE(tmp);
	    errno = ENOENT;
	    return -1;		/* Can't get info, leave alone */
	}

	FindClose(dirH);
	if (info.ftCreationTime.dwHighDateTime != (unsigned int)p1) {
	    IH_RELEASE(tmp);
	    return -1;
	}

	/* If it's the link table itself, decrement the link count. */
	if ((ino & NT_VNODEMASK) == VI_LINKTABLE) {
	    fdP = IH_OPEN(tmp);
	    if (fdP == NULL) {
		IH_RELEASE(tmp);
		return -1;
	    }

	    if ((count = nt_GetLinkCount(fdP, (Inode) 0, 1)) < 0) {
		FDH_REALLYCLOSE(fdP);
		IH_RELEASE(tmp);
		return -1;
	    }

	    count--;
	    if (nt_SetLinkCount(fdP, (Inode) 0, count < 0 ? 0 : count, 1) < 0) {
		FDH_REALLYCLOSE(fdP);
		IH_RELEASE(tmp);
		return -1;
	    }

	    FDH_REALLYCLOSE(fdP);
	    if (count > 0) {
		IH_RELEASE(tmp);
		return 0;
	    }
	}

	if ((code = nt_unlink(name)) == 0) {
	    if ((ino & NT_VNODEMASK) == VI_LINKTABLE) {
		/* Try to remove directory. If it fails, that's ok.
		 * Salvage will clean up.
		 */
		(void)nt_RemoveDataDirectories(tmp);
	    }
	}

	IH_RELEASE(tmp);
    } else {
	/* Get a file descriptor handle for this Inode */
	fdP = IH_OPEN(h);
	if (fdP == NULL) {
	    return -1;
	}

	if ((count = nt_GetLinkCount(fdP, ino, 1)) < 0) {
	    FDH_REALLYCLOSE(fdP);
	    return -1;
	}

	count--;
	if (count >= 0) {
	    if (nt_SetLinkCount(fdP, ino, count, 1) < 0) {
		FDH_REALLYCLOSE(fdP);
		return -1;
	    }
	}
	if (count == 0) {
	    IHandle_t th = *h;
	    th.ih_ino = ino;
	    nt_HandleToName(name, &th);
	    code = nt_unlink(name);
	}
	FDH_CLOSE(fdP);
    }

    return code;
}

int
nt_inc(IHandle_t * h, Inode ino, int p1)
{
    int count;
    int code = 0;
    FdHandle_t *fdP;

    if (ino & NT_INODESPECIAL) {
	if ((ino & NT_VNODEMASK) != VI_LINKTABLE)
	    return 0;
	ino = (Inode) 0;
    }

    /* Get a file descriptor handle for this Inode */
    fdP = IH_OPEN(h);
    if (fdP == NULL) {
	return -1;
    }

    if ((count = nt_GetLinkCount(fdP, ino, 1)) < 0)
	code = -1;
    else {
	count++;
	if (count > 7) {
	    errno = EINVAL;
	    code = -1;
	    count = 7;
	}
	if (nt_SetLinkCount(fdP, ino, count, 1) < 0)
	    code = -1;
    }
    if (code) {
	FDH_REALLYCLOSE(fdP);
    } else {
	FDH_CLOSE(fdP);
    }
    return code;
}



/************************************************************************
 *  Link Table Organization
 ************************************************************************
 *
 * The link table volume special file is used to hold the link counts that
 * are held in the inodes in inode based AFS vice filesystems. Since NTFS
 * doesn't provide us that access, the link counts are being kept in a separate
 * volume special file. The file begins with the usual version stamp
 * information and is then followed by one row per vnode number. vnode 0
 * is used to hold the link count of the link table itself. That is because
 * the same link table is shared among all the volumes of the volume group
 * and is deleted only when the last volume of a volume group is deleted.
 *
 * Within each row, the columns are 3 bits wide. They can each hold a 0 based
 * link count from 0 through 7. Each colume represents a unique instance of
 * that vnode. Say we have a file shared between the RW and a RO and a
 * different version of the file (or a different uniquifer) for the BU volume.
 * Then one column would be holding the link count of 2 for the RW and RO
 * and a different column would hold the link count of 1 for the BU volume.
 * The column used is determined for NT by the uiquifier tag applied to
 * generate a unique file name in the NTFS namespace. The file name is
 * of the form "V_<vno>.<tag>" . And the <tag> is also the column number
 * in the link table.
 */
#define LINKTABLE_WIDTH 2
#define LINKTABLE_SHIFT 1	/* log 2 = 1 */

static void
nt_GetLCOffsetAndIndexFromIno(Inode ino, int *offset, int *index)
{
    int toff = (int)(ino & NT_VNODEMASK);
    int tindex = (int)((ino >> NT_TAGSHIFT) & NT_TAGMASK);

    *offset = (toff << LINKTABLE_SHIFT) + 8;	/* *2 + sizeof stamp */
    *index = (tindex << 1) + tindex;
}


/* nt_GetLinkCount
 * If lockit is set, lock the file and leave it locked upon a successful
 * return.
 */
static int
nt_GetLinkCountInternal(FdHandle_t * h, Inode ino, int lockit, int fixup)
{
    unsigned short row = 0;
    DWORD bytesRead, bytesWritten;
    int offset, index;

    /* there's no linktable yet. the salvager will create one later */
    if (h->fd_fd == INVALID_HANDLE_VALUE && fixup)
       return 1;

    nt_GetLCOffsetAndIndexFromIno(ino, &offset, &index);

    if (lockit) {
	if (!LockFile(h->fd_fd, offset, 0, 2, 0))
	    return -1;
    }

    if (!SetFilePointer(h->fd_fd, (LONG) offset, NULL, FILE_BEGIN))
	goto bad_getLinkByte;

    if (!ReadFile(h->fd_fd, (void *)&row, 2, &bytesRead, NULL)) 
	goto bad_getLinkByte;
    
    if (bytesRead == 0 && fixup) { 
	LARGE_INTEGER size;

	if (!GetFileSizeEx(h->fd_fd, &size) || size.QuadPart >= offset+sizeof(row))
	    goto bad_getLinkByte;
	FDH_TRUNC(h, offset+sizeof(row));
	row = 1 << index;
      rewrite:
	WriteFile(h->fd_fd, (char *)&row, sizeof(row), &bytesWritten, NULL);
    }

    if (fixup && !((row >> index) & NT_TAGMASK)) {
        row |= 1<<index;
        goto rewrite;
    }
 
    return (int)((row >> index) & NT_TAGMASK);

  bad_getLinkByte:
    if (lockit)
	UnlockFile(h->fd_fd, offset, 0, 2, 0);
    return -1;
}

int
nt_GetLinkCount(FdHandle_t * h, Inode ino, int lockit)
{
    return nt_GetLinkCountInternal(h, ino, lockit, 0);
}

void
nt_SetNonZLC(FdHandle_t * h, Inode ino) 
{
    (void)nt_GetLinkCountInternal(h, ino, 0, 1);
}


/* nt_SetLinkCount
 * If locked is set, assume file is locked. Otherwise, lock file before
 * proceeding to modify it.
 */
int
nt_SetLinkCount(FdHandle_t * h, Inode ino, int count, int locked)
{
    int offset, index;
    unsigned short row;
    DWORD bytesRead, bytesWritten;
    int code = -1;

    nt_GetLCOffsetAndIndexFromIno(ino, &offset, &index);


    if (!locked) {
	if (!LockFile(h->fd_fd, offset, 0, 2, 0)) {
	    errno = nterr_nt2unix(GetLastError(), EBADF);
	    return -1;
	}
    }
    if (!SetFilePointer(h->fd_fd, (LONG) offset, NULL, FILE_BEGIN)) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	goto bad_SetLinkCount;
    }


    if (!ReadFile(h->fd_fd, (void *)&row, 2, &bytesRead, NULL)) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	goto bad_SetLinkCount;
    }
    if (bytesRead == 0)
	row = 0;

    bytesRead = 7 << index;
    count <<= index;
    row &= (unsigned short)~bytesRead;
    row |= (unsigned short)count;

    if (!SetFilePointer(h->fd_fd, (LONG) offset, NULL, FILE_BEGIN)) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	goto bad_SetLinkCount;
    }

    if (!WriteFile(h->fd_fd, (void *)&row, 2, &bytesWritten, NULL)) {
	errno = nterr_nt2unix(GetLastError(), EBADF);
	goto bad_SetLinkCount;
    }

    code = 0;


  bad_SetLinkCount:
    UnlockFile(h->fd_fd, offset, 0, 2, 0);

    return code;
}


/* ListViceInodes - write inode data to a results file. */
static int DecodeInodeName(char *name, int *p1, int *p2);
static int DecodeVolumeName(char *name, int *vid);
static int nt_ListAFSSubDirs(IHandle_t * dirIH,
			     int (*write_fun) (FILE *, struct ViceInodeInfo *,
					       char *, char *), FILE * fp,
			     int (*judgeFun) (struct ViceInodeInfo *,
					      int vid, void *rock),
			     int singleVolumeNumber, void *rock);


/* WriteInodeInfo
 *
 * Write the inode data to the results file. 
 *
 * Returns -2 on error, 0 on success.
 *
 * This is written as a callback simply so that other listing routines
 * can use the same inode reading code.
 */
static int
WriteInodeInfo(FILE * fp, struct ViceInodeInfo *info, char *dir, char *name)
{
    int n;
    n = fwrite(info, sizeof(*info), 1, fp);
    return (n == 1) ? 0 : -2;
}


/* ListViceInodes
 * Fill the results file with the requested inode information.
 *
 * Return values:
 *  0 - success
 * -1 - complete failure, salvage should terminate.
 * -2 - not enough space on partition, salvager has error message for this.
 *
 * This code optimizes single volume salvages by just looking at that one
 * volume's directory. 
 *
 * If the resultFile is NULL, then don't call the write routine.
 */
int
ListViceInodes(char *devname, char *mountedOn, char *resultFile,
	       int (*judgeInode) (struct ViceInodeInfo * info, int vid, void *rock),
	       int singleVolumeNumber, int *forcep, int forceR, char *wpath, 
	       void *rock)
{
    FILE *fp = (FILE *) - 1;
    int ninodes;
    struct stat status;

    if (resultFile) {
	fp = fopen(resultFile, "w");
	if (!fp) {
	    Log("Unable to create inode description file %s\n", resultFile);
	    return -1;
	}
    }
    ninodes =
	nt_ListAFSFiles(wpath, WriteInodeInfo, fp, judgeInode,
			singleVolumeNumber, rock);

    if (!resultFile)
	return ninodes;

    if (ninodes < 0) {
	fclose(fp);
	return ninodes;
    }

    if (fflush(fp) == EOF) {
	Log("Unable to successfully flush inode file for %s\n", mountedOn);
	fclose(fp);
	return -2;
    }
    if (fsync(fileno(fp)) == -1) {
	Log("Unable to successfully fsync inode file for %s\n", mountedOn);
	fclose(fp);
	return -2;
    }
    if (fclose(fp) == EOF) {
	Log("Unable to successfully close inode file for %s\n", mountedOn);
	return -2;
    }

    /*
     * Paranoia:  check that the file is really the right size
     */
    if (stat(resultFile, &status) == -1) {
	Log("Unable to successfully stat inode file for %s\n", mountedOn);
	return -2;
    }
    if (status.st_size != ninodes * sizeof(struct ViceInodeInfo)) {
	Log("Wrong size (%d instead of %d) in inode file for %s\n",
	    status.st_size, ninodes * sizeof(struct ViceInodeInfo),
	    mountedOn);
	return -2;
    }
    return 0;
}


/* nt_ListAFSFiles
 *
 * Collect all the matching AFS files on the drive.
 * If singleVolumeNumber is non-zero, just return files for that volume.
 *
 * Returns <0 on error, else number of files found to match.
 */
int
nt_ListAFSFiles(char *dev,
		int (*writeFun) (FILE *, struct ViceInodeInfo *, char *,
				 char *), FILE * fp,
		int (*judgeFun) (struct ViceInodeInfo *, int, void *),
		int singleVolumeNumber, void *rock)
{
    IHandle_t h;
    char name[MAX_PATH];
    int ninodes = 0;
    DIR *dirp;
    struct dirent *dp;
    static void FreeZLCList(void);

    memset((void *)&h, 0, sizeof(IHandle_t));
    h.ih_dev = toupper(*dev) - 'A';

    if (singleVolumeNumber) {
	h.ih_vid = singleVolumeNumber;
	if (!nt_HandleToVolDir(name, &h))
	    return -1;
	ninodes =
	    nt_ListAFSSubDirs(&h, writeFun, fp, judgeFun, singleVolumeNumber, rock);
	if (ninodes < 0)
	    return ninodes;
    } else {
	/* Find all Vol_*.data directories and descend through them. */
	if (!nt_DevToDrive(name, h.ih_dev))
	    return -1;
	ninodes = 0;
	dirp = opendir(name);
	if (!dirp)
	    return -1;
	while (dp = readdir(dirp)) {
	    if (!DecodeVolumeName(dp->d_name, &h.ih_vid)) {
		ninodes += nt_ListAFSSubDirs(&h, writeFun, fp, judgeFun, 0, rock);
	    }
	}
    }
    FreeZLCList();
    return ninodes;
}



/* nt_ListAFSSubDirs
 *
 * List the S, F, and D subdirectories of this volume's directory.
 *
 * Return values:
 * < 0 - an error
 * > = 0 - number of AFS files found.
 */
static int
nt_ListAFSSubDirs(IHandle_t * dirIH,
		  int (*writeFun) (FILE *, struct ViceInodeInfo *, char *,
				   char *), FILE * fp,
		  int (*judgeFun) (struct ViceInodeInfo *, int, void *),
		  int singleVolumeNumber, void *rock)
{
    int i;
    IHandle_t myIH = *dirIH;
    HANDLE dirH;
    WIN32_FIND_DATA data;
    char path[1024];
    char basePath[1024];
    char *s;
    char findPath[1024];
    struct ViceInodeInfo info;
    int tag, vno;
    FdHandle_t linkHandle;
    int ninodes = 0;
    static void DeleteZLCFiles(char *path);

    s = nt_HandleToVolDir(path, &myIH);
    strcpy(basePath, path);
    if (!s)
	return -1;
    *s = '\\';
    s++;
    *(s + 1) = '\0';

    /* Do the directory containing the special files first to pick up link
     * counts.
     */
    for (i = 'R'; i >= 'A'; i--) {
	*s = (char)i;
	(void)strcpy(findPath, path);
	(void)strcat(findPath, "\\*");
	dirH =
	    FindFirstFileEx(findPath, FindExInfoStandard, &data,
			    FindExSearchNameMatch, NULL,
			    FIND_FIRST_EX_CASE_SENSITIVE);
	if (dirH == INVALID_HANDLE_VALUE)
	    continue;
	while (1) {
	    /* Store the vice info. */
	    memset((void *)&info, 0, sizeof(info));
	    if (*data.cFileName == '.')
		goto next_file;
	    if (DecodeInodeName(data.cFileName, &vno, &tag) < 0) {
		Log("Error parsing %s\\%s\n", path, data.cFileName);
	    } else {
		info.inodeNumber = (Inode) tag << NT_TAGSHIFT;
		info.inodeNumber |= (Inode) vno;
		info.byteCount = data.nFileSizeLow;

		if (i == 'R') {	/* Special inode. */
		    info.inodeNumber |= NT_INODESPECIAL;
		    info.u.param[0] = data.ftCreationTime.dwHighDateTime;
		    info.u.param[1] = data.ftCreationTime.dwLowDateTime;
		    info.u.param[2] = vno;
		    info.u.param[3] = dirIH->ih_vid;
		    if (info.u.param[2] != VI_LINKTABLE) {
			info.linkCount = 1;
		    } else {
			/* Open this handle */
			char lpath[1024];
			(void)sprintf(lpath, "%s\\%s", path, data.cFileName);
			linkHandle.fd_fd = nt_open(lpath, O_RDONLY, 0666);
			info.linkCount =
			    nt_GetLinkCount(&linkHandle, (Inode) 0, 0);
		    }
		} else {	/* regular file. */
		    info.linkCount =
			nt_GetLinkCount(&linkHandle, info.inodeNumber, 0);
		    if (info.linkCount == 0) {
#ifdef notdef
			Log("Found 0 link count file %s\\%s, deleting it.\n",
			    path, data.cFileName);
			AddToZLCDeleteList((char)i, data.cFileName);
#else
			Log("Found 0 link count file %s\\%s.\n", path,
			    data.cFileName);
#endif
			goto next_file;
		    }
		    info.u.param[0] = dirIH->ih_vid;
		    info.u.param[1] = vno;
		    info.u.param[2] = data.ftCreationTime.dwHighDateTime;
		    info.u.param[3] = data.ftCreationTime.dwLowDateTime;
		}
		if (judgeFun && !(*judgeFun) (&info, singleVolumeNumber, rock))
		    goto next_file;
		if ((*writeFun) (fp, &info, path, data.cFileName) < 0) {
		    nt_close(linkHandle.fd_fd);
		    FindClose(dirH);
		    return -1;
		}
		ninodes++;
	    }
	  next_file:
	    if (!FindNextFile(dirH, &data)) {
		break;
	    }
	}
	FindClose(dirH);
    }
    nt_close(linkHandle.fd_fd);
    DeleteZLCFiles(basePath);
    if (!ninodes) {
	/* Then why does this directory exist? Blow it away. */
	nt_RemoveDataDirectories(dirIH);
    }

    return ninodes;
}

/* The name begins with "Vol_" and ends with .data.  See nt_HandleToVolDir() */
static int
DecodeVolumeName(char *name, int *vid)
{
    char stmp[32];
    int len;

    len = strlen(name);
    if (len <= 9)
	return -1;
    if (strncmp(name, "Vol_", 4))
	return -1;
    if (strcmp(name + len - 5, ".data"))
	return -1;
    strcpy(stmp, name);
    stmp[len - 5] = '\0';
    *vid = base32_to_int(stmp + 4);
    return 0;
}

/* Recall that the name beings with a "V_" */
static int
DecodeInodeName(char *name, int *p1, int *p2)
{
    char *s, *t;
    char stmp[16];

    (void)strcpy(stmp, name);
    s = strrchr(stmp, '_');
    if (!s)
	return -1;
    s++;
    t = strrchr(s, '.');
    if (!t)
	return -1;

    *t = '\0';
    *p1 = base32_to_int(s);
    *p2 = base32_to_int(t + 1);
    return 0;
}


/* PrintInode
 *
 * returns a static string used to print either 32 or 64 bit inode numbers.
 */
char *
PrintInode(char *s, Inode ino)
{
    static afs_ino_str_t result;
    if (!s)
	s = result;

    (void)sprintf((char *)s, "%I64u", ino);

    return (char *)s;
}


/* Routines to facilitate removing zero link count files. */
#define MAX_ZLC_NAMES 32
#define MAX_ZLC_NAMELEN 16
typedef struct zlcList_s {
    struct zlcList_s *zlc_next;
    int zlc_n;
    char zlc_names[MAX_ZLC_NAMES][MAX_ZLC_NAMELEN];
} zlcList_t;

static zlcList_t *zlcAnchor = NULL;
static zlcList_t *zlcCur = NULL;

static void
AddToZLCDeleteList(char dir, char *name)
{
    assert(strlen(name) <= MAX_ZLC_NAMELEN - 3);

    if (!zlcCur || zlcCur->zlc_n >= MAX_ZLC_NAMES) {
	if (zlcCur && zlcCur->zlc_next)
	    zlcCur = zlcCur->zlc_next;
	else {
	    zlcList_t *tmp = (zlcList_t *) malloc(sizeof(zlcList_t));
	    if (!tmp)
		return;
	    if (!zlcAnchor) {
		zlcAnchor = tmp;
	    } else {
		zlcCur->zlc_next = tmp;
	    }
	    zlcCur = tmp;
	    zlcCur->zlc_n = 0;
	    zlcCur->zlc_next = NULL;
	}
    }

    (void)sprintf(zlcCur->zlc_names[zlcCur->zlc_n], "%c\\%s", dir, name);
    zlcCur->zlc_n++;
}

static void
DeleteZLCFiles(char *path)
{
    zlcList_t *z;
    int i;
    char fname[1024];

    for (z = zlcAnchor; z; z = z->zlc_next) {
	for (i = 0; i < z->zlc_n; i++) {
	    (void)sprintf(fname, "%s\\%s", path, z->zlc_names[i]);
	    if (nt_unlink(fname) < 0) {
		Log("ZLC: Can't unlink %s, dos error = %d\n", fname,
		    GetLastError());
	    }
	}
	z->zlc_n = 0;		/* Can reuse space. */
    }
    zlcCur = zlcAnchor;
}

static void
FreeZLCList(void)
{
    zlcList_t *tnext;
    zlcList_t *i;

    i = zlcAnchor;
    while (i) {
	tnext = i->zlc_next;
	free(i);
	i = tnext;
    }
    zlcCur = zlcAnchor = NULL;
}

#endif /* AFS_NT40_ENV */
