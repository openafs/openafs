/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <winioctl.h>
#include <sys/stat.h>
#include <crtdbg.h>

#include <afs/errmap_nt.h>
#include <afs/usd.h>


/* WinNT-specific implementation of user space device I/O for WinNT devices. */

/* This module uses the following usd_handle fields:
 * handle -- the Win32 (HANDLE) returned by CreateFile.
 * fullPathName -- allocated ptr (char *) to pathname used to open device.
 * openFlags -- unused
 * privateData -- Device size (afs_uint32) in 1Kb units.
 */

/* Module-specific constants */
#define TAPEOP_RETRYMAX  5

#define TRANSIENT_TAPE_ERROR(err) \
    ((err) == ERROR_BUS_RESET || (err) == ERROR_MEDIA_CHANGED)

/* Interface Functions */

static int
usd_DeviceRead(usd_handle_t usd, char *buf, afs_uint32 nbytes,
	       afs_uint32 * xferdP)
{
    HANDLE fd = usd->handle;
    DWORD bytesRead;

    if (xferdP == NULL)
	xferdP = &bytesRead;
    else
	*xferdP = 0;

    if (ReadFile(fd, buf, nbytes, xferdP, NULL)) {
	/* read was successful */
	return 0;
    } else {
	/* read failed */
	return nterr_nt2unix(GetLastError(), EIO);
    }
}

static int
usd_DeviceWrite(usd_handle_t usd, char *buf, afs_uint32 nbytes,
		afs_uint32 * xferdP)
{
    HANDLE fd = usd->handle;
    DWORD bytesWritten;
    DWORD nterr;

    if (xferdP == NULL)
	xferdP = &bytesWritten;
    else
	*xferdP = 0;

    if (WriteFile(fd, buf, nbytes, xferdP, NULL)) {
	/* write was successful */
	return 0;
    } else {
	/* write failed */
	nterr = GetLastError();
	if (nterr == ERROR_END_OF_MEDIA) {
	    *xferdP = 0;
	    return 0;		/* indicate end of tape condition */
	} else
	    return nterr_nt2unix(nterr, EIO);
    }
}

/* usd_DeviceSeek --
 *
 * TODO -- Episode tries to determine the size of a disk device
 * empirically by binary searching for the last region it can successfully
 * read from the device.  It uses only USD_SEEK and USD_READ operations, so
 * this is portable as long as the underlying device driver fails
 * gracefully when an attempt is made to access past the end of the device.
 * Unfortunately this fails on WinNT when accessing a floppy disk.  Reads
 * from floppy disks at tremendous offsets blithely succeed.  Luckily, the
 * Win32 interface provides a way to determine the disk size.
 *
 * Add a check of the offset against the disk size to fix this problem. */

static int
usd_DeviceSeek(usd_handle_t usd, afs_hyper_t reqOff, int whence,
	       afs_hyper_t * curOffP)
{
    HANDLE fd = usd->handle;
    DWORD method, result;
    DWORD error;
    long loOffset, hiOffset;

    /* determine move method based on value of whence */

    if (whence == SEEK_SET) {
	method = FILE_BEGIN;
    } else if (whence == SEEK_CUR) {
	method = FILE_CURRENT;
    } else if (whence == SEEK_END) {
	method = FILE_END;
    } else {
	/* whence is invalid */
	return EINVAL;
    }
    _ASSERT(sizeof(DWORD) == 4);

    /* attempt seek */

    loOffset = hgetlo(reqOff);
    hiOffset = hgethi(reqOff);

    if (usd->privateData) {

	/* For disk devices that know their size, check the offset against the
	 * limit provided by DeviceIoControl(). */

	DWORDLONG offset =
	    ((DWORDLONG) hgethi(reqOff) << 32 | (DWORDLONG) hgetlo(reqOff));

	DWORDLONG k = (DWORDLONG) ((afs_uint32) usd->privateData);

	/* _ASSERT(AFS_64BIT_ENV); */
	if (offset >= (k << 10))
	    return EINVAL;
    }

    result = SetFilePointer(fd, loOffset, &hiOffset, method);
    if (result == 0xffffffff && (error = GetLastError()) != NO_ERROR)
	return nterr_nt2unix(error, EIO);
    if (curOffP)
	hset64(*curOffP, hiOffset, result);

    return 0;
}

static int
usd_DeviceIoctl(usd_handle_t usd, int req, void *arg)
{
    HANDLE fd = usd->handle;
    DWORD result;
    DWORD hiPart;
    int code = 0;

    switch (req) {
    case USD_IOCTL_GETTYPE:
	{
	    int mode;

	    BY_HANDLE_FILE_INFORMATION info;
	    DISK_GEOMETRY geom;
	    DWORD nbytes;
	    DWORD fileError = 0;
	    DWORD diskError = 0;

	    if (!GetFileInformationByHandle(fd, &info))
		fileError = GetLastError();

	    if (!DeviceIoControl
		(fd, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geom,
		 sizeof(geom), &nbytes, NULL))
		diskError = GetLastError();

	    mode = 0;
	    if ((fileError == ERROR_INVALID_PARAMETER
		 || fileError == ERROR_INVALID_FUNCTION)
		&& diskError == 0) {
		mode = S_IFCHR;	/* a disk device */
		if ((afs_uint32) (usd->privateData) == 0) {

		    /* Fill in the device size from disk geometry info.  Note
		     * that this is the whole disk, not just the partition, so
		     * it will serve only as an upper bound. */

		    DWORDLONG size = geom.Cylinders.QuadPart;
		    afs_uint32 k;
		    size *= geom.TracksPerCylinder;
		    size *= geom.SectorsPerTrack;
		    size *= geom.BytesPerSector;
		    if (size == 0)
			return ENODEV;
		    size >>= 10;	/* convert to Kilobytes */
		    if (size >> 31)
			k = 0x7fffffff;
		    else
			k = (afs_uint32) size;
		    usd->privateData = (void *)k;
		}
	    } else if (diskError == ERROR_INVALID_PARAMETER && fileError == 0)
		mode = S_IFREG;	/* a regular file */
	    else {
		/* check to see if device is a tape drive */
		result = GetTapeStatus(fd);

		if (result != ERROR_INVALID_FUNCTION
		    && result != ERROR_INVALID_PARAMETER) {
		    /* looks like a tape drive */
		    mode = S_IFCHR;
		}
	    }

	    if (!mode)
		return EINVAL;
	    *(int *)arg = mode;
	    return 0;
	}

    case USD_IOCTL_GETDEV:
	return EINVAL;
	*(dev_t *) arg = 0;
	break;

    case USD_IOCTL_GETFULLNAME:
	*(char **)arg = usd->fullPathName;
	break;

    case USD_IOCTL_GETSIZE:
	result = GetFileSize(fd, &hiPart);
	if (result == 0xffffffff && (code = GetLastError()) != NO_ERROR)
	    return nterr_nt2unix(code, EIO);
	hset64(*(afs_hyper_t *) arg, hiPart, result);
	return 0;

    case USD_IOCTL_SETSIZE:
	code = usd_DeviceSeek(usd, *(afs_hyper_t *) arg, SEEK_SET, NULL);
	if (!code) {
	    if (!SetEndOfFile(fd))
		code = nterr_nt2unix(GetLastError(), EIO);
	}
	return code;

    case USD_IOCTL_TAPEOPERATION:
	{
	    TAPE_GET_MEDIA_PARAMETERS mediaParam;
	    TAPE_GET_DRIVE_PARAMETERS driveParam;
	    DWORD mediaParamSize = sizeof(TAPE_GET_MEDIA_PARAMETERS);
	    DWORD driveParamSize = sizeof(TAPE_GET_DRIVE_PARAMETERS);
	    DWORD reloffset, fmarkType;
	    int retrycount;
	    usd_tapeop_t *tapeOpp = (usd_tapeop_t *) arg;

	    /* Execute specified tape command */

	    switch (tapeOpp->tp_op) {
	    case USDTAPE_WEOF:
		/* Determine type of filemark supported by device */
		result =
		    GetTapeParameters(fd, GET_TAPE_DRIVE_INFORMATION,
				      &driveParamSize, &driveParam);

		if (result == NO_ERROR) {
		    /* drive must support either normal or long filemarks */
		    if (driveParam.FeaturesHigh & TAPE_DRIVE_WRITE_FILEMARKS) {
			fmarkType = TAPE_FILEMARKS;
		    } else if (driveParam.
			       FeaturesHigh & TAPE_DRIVE_WRITE_LONG_FMKS) {
			fmarkType = TAPE_LONG_FILEMARKS;
		    } else {
			result = ERROR_NOT_SUPPORTED;
		    }
		}

		/* Write specified number of filemarks */
		if (result == NO_ERROR) {
		    result =
			WriteTapemark(fd, fmarkType, tapeOpp->tp_count,
				      FALSE);
		}
		break;

	    case USDTAPE_REW:
		/* Rewind tape */
		retrycount = 0;
		do {
		    /* absorb non-persistant errors, e.g. ERROR_MEDIA_CHANGED. */
		    result = SetTapePosition(fd, TAPE_REWIND, 0, 0, 0, FALSE);
		} while ((result != NO_ERROR)
			 && (retrycount++ < TAPEOP_RETRYMAX));

		break;

	    case USDTAPE_FSF:
	    case USDTAPE_BSF:
		/* Space over specified number of file marks */
		if (tapeOpp->tp_count < 0) {
		    result = ERROR_INVALID_PARAMETER;
		} else {
		    if (tapeOpp->tp_op == USDTAPE_FSF) {
			reloffset = tapeOpp->tp_count;
		    } else {
			reloffset = 0 - tapeOpp->tp_count;
		    }

		    result =
			SetTapePosition(fd, TAPE_SPACE_FILEMARKS, 0,
					reloffset, 0, FALSE);
		}
		break;

	    case USDTAPE_PREPARE:
		/* Prepare tape drive for operation; do after open. */

		retrycount = 0;
		do {
		    /* absorb non-persistant errors */
		    if (retrycount > 0) {
			Sleep(2 * 1000);
		    }
		    result = PrepareTape(fd, TAPE_LOCK, FALSE);
		} while (TRANSIENT_TAPE_ERROR(result)
			 && retrycount++ < TAPEOP_RETRYMAX);

		if (result == NO_ERROR) {
		    retrycount = 0;
		    do {
			/* absorb non-persistant errors */
			if (retrycount > 0) {
			    Sleep(2 * 1000);
			}
			result = GetTapeStatus(fd);
		    } while (TRANSIENT_TAPE_ERROR(result)
			     && retrycount++ < TAPEOP_RETRYMAX);
		}

		/* Querying media/drive info seems to clear bad tape state */
		if (result == NO_ERROR) {
		    result =
			GetTapeParameters(fd, GET_TAPE_MEDIA_INFORMATION,
					  &mediaParamSize, &mediaParam);
		}

		if (result == NO_ERROR) {
		    result =
			GetTapeParameters(fd, GET_TAPE_DRIVE_INFORMATION,
					  &driveParamSize, &driveParam);
		}
		break;

	    case USDTAPE_SHUTDOWN:
		/* Decommission tape drive after operation; do before close. */
		result = PrepareTape(fd, TAPE_UNLOCK, FALSE);
		break;

	    default:
		/* Invalid command */
		result = ERROR_INVALID_PARAMETER;
		break;
	    }

	    if (result == NO_ERROR) {
		return (0);
	    } else {
		return nterr_nt2unix(result, EIO);
	    }
	}

    case USD_IOCTL_GETBLKSIZE:
	*((long *)arg) = (long)4096;
	return 0;

    default:
	return EINVAL;
    }
    return code;
}


static int
usd_DeviceClose(usd_handle_t usd)
{
    HANDLE fd = usd->handle;
    int code;

    if (CloseHandle(fd))
	code = 0;
    else
	code = nterr_nt2unix(GetLastError(), EIO);

    if (usd->fullPathName)
	free(usd->fullPathName);
    free(usd);

    return code;
}

/*
 * usd_DeviceOpen() -- open WinNT device (or regular file)
 *
 * PARAMETERS --
 *     oflag -- Various combinations of USD_OPEN_XXX defined in usd.h.
 *     pmode -- ignored; file's security descriptor set to default on create.
 *     usdP -- if NULL device is immediately closed after being opened.
 *         Otherwise, *usdP is set to the user space device handle.
 *
 * RETURN CODES -- On error a unix-style errno value is *returned*.  Else zero.
 */

static int
usd_DeviceOpen(const char *path, int oflag, int pmode, usd_handle_t * usdP)
{
    HANDLE devhandle;
    DWORD access, share, create, attr;
    usd_handle_t usd;
    int mode;			/* type of opened object */
    int code;

    if (usdP)
	*usdP = NULL;

    /* set access as specified in oflag */

    if (oflag & USD_OPEN_SYNC)
	attr = FILE_FLAG_WRITE_THROUGH;
    else
	attr = 0;

    /* should we always set:
     *     FILE_FLAG_NO_BUFFERING?
     *     FILE_FLAG_RANDOM_ACCESS?
     */

    access = GENERIC_READ;
    if (oflag & USD_OPEN_RDWR)
	access |= GENERIC_WRITE;

    /* set create as specified in oflag */

    if (oflag & USD_OPEN_CREATE) {
	/* must be opening a file; allow it to be created */
	create = OPEN_ALWAYS;
    } else {
	create = OPEN_EXISTING;
    }


    if (oflag & (USD_OPEN_RLOCK | USD_OPEN_WLOCK)) {

	/* make sure both lock bits aren't set */
	_ASSERT(~oflag & (USD_OPEN_RLOCK | USD_OPEN_WLOCK));

	share =
	    ((oflag & USD_OPEN_RLOCK) ? FILE_SHARE_READ : 0 /*no sharing */ );

    } else {
	share = FILE_SHARE_READ + FILE_SHARE_WRITE;
    }

    /* attempt to open the device/file */

    devhandle = CreateFile(path, access, share, NULL, create, attr, NULL);

    if (devhandle == INVALID_HANDLE_VALUE)
	return nterr_nt2unix(GetLastError(), EIO);

    usd = (usd_handle_t) malloc(sizeof(*usd));
    memset(usd, 0, sizeof(*usd));


    _ASSERT(sizeof(devhandle) <= sizeof(usd->handle));
    usd->handle = (void *)devhandle;

    usd->read = usd_DeviceRead;
    usd->write = usd_DeviceWrite;
    usd->seek = usd_DeviceSeek;
    usd->ioctl = usd_DeviceIoctl;
    usd->close = usd_DeviceClose;

    usd->fullPathName = (char *)malloc(strlen(path) + 1);
    strcpy(usd->fullPathName, path);
    usd->openFlags = oflag;

    /* For devices, this is the first real reference, so many errors show up
     * here.  Also this call also sets the size (stored in usd->privateData)
     * based on the results of the call to DeviceIoControl(). */
    code = USD_IOCTL(usd, USD_IOCTL_GETTYPE, &mode);


    /* If we're trying to obtain a write lock on a real disk, then the
     * aggregate must not be attached by the kernel.  If so, unlock it
     * and fail.
     * WARNING: The code to check for the above has been removed when this
     * file was ported from DFS src. It should be put back if
     * this library is used to access hard disks
     */

    if (code == 0 && usdP)
	*usdP = usd;
    else
	usd_DeviceClose(usd);
    return code;
}

int
usd_Open(const char *path, int oflag, int mode, usd_handle_t * usdP)
{
    return usd_DeviceOpen(path, oflag, mode, usdP);
}

static int
usd_DeviceDummyClose(usd_handle_t usd)
{
    free(usd);
    return 0;
}

static int
usd_DeviceStandardInput(usd_handle_t * usdP)
{
    usd_handle_t usd;

    if (usdP)
	*usdP = NULL;

    usd = (usd_handle_t) malloc(sizeof(*usd));
    memset(usd, 0, sizeof(*usd));
    usd->handle = (void *)0;
    usd->read = usd_DeviceRead;
    usd->write = usd_DeviceWrite;
    usd->seek = usd_DeviceSeek;
    usd->ioctl = usd_DeviceIoctl;
    usd->close = usd_DeviceDummyClose;
    usd->fullPathName = "STDIN";
    usd->openFlags = 0;

    return 0;
}

int
usd_StandardInput(usd_handle_t * usdP)
{
    return usd_DeviceStandardInput(usdP);
}

static int
usd_DeviceStandardOutput(usd_handle_t * usdP)
{
    usd_handle_t usd;

    if (usdP)
	*usdP = NULL;

    usd = (usd_handle_t) malloc(sizeof(*usd));
    memset(usd, 0, sizeof(*usd));
    usd->handle = (void *)1;
    usd->read = usd_DeviceRead;
    usd->write = usd_DeviceWrite;
    usd->seek = usd_DeviceSeek;
    usd->ioctl = usd_DeviceIoctl;
    usd->close = usd_DeviceDummyClose;
    usd->fullPathName = "STDOUT";
    usd->openFlags = 0;

    return 0;
}

int
usd_StandardOutput(usd_handle_t * usdP)
{
    return usd_DeviceStandardOutput(usdP);
}
