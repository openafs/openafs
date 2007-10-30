/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* File-oriented utility functions */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef AFS_NT40_ENV
#include <windows.h>
#include <io.h>
#include "errmap_nt.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "fileutil.h"


/*
 * renamefile() -- rename a file (Unix rename() semantics)
 */
int
renamefile(const char *oldname, const char *newname)
{
    int rc = 0;

#ifdef AFS_NT40_ENV
    if (!MoveFileEx(oldname, newname, MOVEFILE_REPLACE_EXISTING)) {
	/* rename failed */
	errno = nterr_nt2unix(GetLastError(), EIO);
	rc = -1;
    }
#else
    rc = rename(oldname, newname);
#endif
    return rc;
}


/*
 * FilepathNormalizeEx() -- normalize file path; i.e., use only forward (or only 
 *     backward) slashes, remove multiple and trailing slashes.
 */
void
FilepathNormalizeEx(char *path, int slashType)
{
    short bWasSlash = 0;
    char *pP, *pCopyFrom;
    char slash = '/';		/* Default to forward slashes */

    if (slashType == FPN_BACK_SLASHES)
	slash = '\\';

    if (path != NULL) {
	/* use only forward slashes; remove multiple slashes */
	for (pP = pCopyFrom = path; *pCopyFrom != '\0'; pCopyFrom++) {
	    if ((*pCopyFrom == '/') || (*pCopyFrom == '\\')) {
		if (!bWasSlash) {
		    *pP++ = slash;
		    bWasSlash = 1;
		}
	    } else {
		*pP++ = *pCopyFrom;
		bWasSlash = 0;
	    }
	}
	*pP = '\0';

	/* strip off trailing slash (unless specifies root) */
	pP--;
	if ((*pP == slash) && (pP != path)) {
#ifdef AFS_NT40_ENV
	    /* check for "X:/" */
	    if (*(pP - 1) != ':') {
		*pP = '\0';
	    }
#else
	    *pP = '\0';
#endif
	}
    }
}


void
FilepathNormalize(char *path)
{
    FilepathNormalizeEx(path, FPN_FORWARD_SLASHES);
}

/* Open a file for buffered I/O */
bufio_p
BufioOpen(char *path, int oflag, int mode)
{
    bufio_p bp;

    bp = (bufio_p) malloc(sizeof(bufio_t));
    if (bp == NULL) {
	return NULL;
    }
#ifdef AFS_NT40_ENV
    bp->fd = _open(path, oflag, mode);
#else
    bp->fd = open(path, oflag, mode);
#endif
    if (bp->fd == BUFIO_INVALID_FD) {
	free(bp);
	return NULL;
    }

    bp->pos = 0;
    bp->len = 0;
    bp->eof = 0;

    return bp;
}

/* Read the next line of a file up to len-1 bytes into buf,
 * and strip off the carriage return. buf is null terminated.
 * Returns -1 on EOF or error, length of string on success.
 */
int
BufioGets(bufio_p bp, char *buf, int buflen)
{
    int rc;
    char c;
    int tlen, pos, len;

    if (!buf || buflen <= 1 || !bp || bp->eof) {
	return -1;
    }

    tlen = 0;
    pos = bp->pos;
    len = bp->len;
    while (1) {
	if (pos >= len) {
#ifdef AFS_NT40_ENV
	    rc = _read(bp->fd, bp->buf, BUFIO_BUFSIZE);
#else
	    rc = read(bp->fd, bp->buf, BUFIO_BUFSIZE);
#endif
	    if (rc < 0) {
		bp->eof = 1;
		return -1;
	    } else if (rc == 0) {
		bp->eof = 1;
		if (tlen == 0) {
		    return -1;
		} else {
		    return tlen;
		}
	    }
	    pos = bp->pos = 0;
	    len = bp->len = rc;
	}
	while (pos < len) {
	    c = bp->buf[pos++];
	    if (c == '\n') {
		buf[tlen] = '\0';
		bp->pos = pos;
		bp->len = len;
		return tlen;
	    } else {
		buf[tlen++] = c;
		if (tlen >= buflen - 1) {
		    buf[tlen] = '\0';
		    bp->pos = pos;
		    bp->len = len;
		    return tlen;
		}
	    }
	}
    }
}

/* Close a buffered I/O handle */
int
BufioClose(bufio_p bp)
{
    BUFIO_FD fd;
    int rc;

    if (!bp) {
	return -1;
    }
    fd = bp->fd;
    free(bp);
#ifdef AFS_NT40_ENV
    rc = _close(fd);
#else
    rc = close(fd);
#endif

    return rc;
}
