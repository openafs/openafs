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
#include <afs/errmap_nt.h>

int nterr_lastNTError = 0;	/* Useful for core dumps from LWP based binaries */

/*
 * nterr_nt2unix() -- convert NT error code to a Unix-ish value.
 *
 * RETURN CODES: translated code, or 'defaultErr' if no translation available.
 */

int
nterr_nt2unix(long ntErr, int defaultErr)
{
    int ucode;

    nterr_lastNTError = ntErr;
    switch (ntErr) {
    case ERROR_SUCCESS:
	ucode = 0;
	break;
    case ERROR_INVALID_PARAMETER:
    case ERROR_BAD_COMMAND:
	ucode = EINVAL;
	break;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
	ucode = ENOENT;
	break;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
	ucode = EEXIST;
	break;
    case ERROR_ACCESS_DENIED:
	ucode = EACCES;
	break;
    case ERROR_WRITE_PROTECT:
	ucode = EROFS;
	break;
    case ERROR_NOT_SUPPORTED:
	ucode = ENOTTY;
	break;
    case ERROR_INVALID_HANDLE:
	ucode = EBADF;
	break;
    case ERROR_TOO_MANY_OPEN_FILES:
	ucode = EMFILE;
	break;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
	ucode = ENOSPC;
	break;
    case ERROR_OUTOFMEMORY:
    case ERROR_NOT_ENOUGH_MEMORY:
	ucode = ENOMEM;
	break;
    case ERROR_SHARING_VIOLATION:
    case ERROR_PIPE_BUSY:
	ucode = EBUSY;
	break;
    case ERROR_BROKEN_PIPE:
    case ERROR_BAD_PIPE:
    case ERROR_PIPE_NOT_CONNECTED:
	ucode = EPIPE;
	break;
    default:
	ucode = defaultErr;
	break;
    }

    return ucode;
}
