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


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <rx/xdr.h>
#include "bulk.h"

int
bulk_SendFile(fd, call, status)
     int fd;
     struct rx_call *call;
     struct stat *status;
{
    char *buffer = (char *)0;
    int blockSize;
    long length;
    XDR xdr;
    long error = 0;
    blockSize = status->st_blksize;
    length = status->st_size;
    buffer = (char *)malloc(status->st_blksize);
    if (!buffer) {
	printf("malloc failed\n");
	return BULK_ERROR;
    }
    xdrrx_create(&xdr, call, XDR_ENCODE);
    if (!xdr_long(&xdr, &length))
	error = BULK_ERROR;
    while (!error && length) {
	int nbytes = (length > blockSize ? blockSize : length);
	nbytes = read(fd, buffer, nbytes);
	if (nbytes <= 0) {
	    fprintf(stderr, "File system read failed\n");
	    break;
	}
	if (rx_Write(call, buffer, nbytes) != nbytes)
	    break;
	length -= nbytes;
    }
    if (buffer)
	free(buffer);
    if (length)
	error = BULK_ERROR;
    return error;
}

/* Copy the appropriate number of bytes from the call to fd.  The status should reflect the file's status coming into the routine and will reflect it going out of the routine, in the absence of errors */
int
bulk_ReceiveFile(fd, call, status)
     int fd;
     struct rx_call *call;
     struct stat *status;
{
    char *buffer = (char *)0;
    long length;
    XDR xdr;
    int blockSize;
    long error = 0;

    xdrrx_create(&xdr, call, XDR_DECODE);
    if (!xdr_long(&xdr, &length))
	return BULK_ERROR;
    blockSize = status->st_blksize;
    buffer = (char *)malloc(status->st_blksize);
    if (!buffer) {
	printf("malloc failed\n");
	return BULK_ERROR;
    }
    while (!error && length) {
	int nbytes = (length > blockSize ? blockSize : length);
	nbytes = rx_Read(call, buffer, nbytes);
	if (!nbytes)
	    error = BULK_ERROR;
	if (write(fd, buffer, nbytes) != nbytes) {
	    fprintf(stderr, "File system write failed!\n");
	    error = BULK_ERROR;
	}
	length -= nbytes;
    }
    if (buffer)
	free(buffer);
    if (!error)
	fstat(fd, status);
    return error;
}
