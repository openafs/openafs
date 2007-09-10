/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*  ihandle.c	- file descriptor cacheing for Inode handles.           */
/*									*/
/************************************************************************/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#else
#include <sys/file.h>
#include <unistd.h>
#include <sys/stat.h>
#if defined(AFS_SUN5_ENV) || defined(AFS_NBSD_ENV)
#include <sys/fcntl.h>
#include <sys/resource.h>
#endif
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <rx/xdr.h>
#include <afs/afsint.h>
#include <errno.h>
#include <afs/afssyscalls.h>
#include "nfs.h"
#include "ihandle.h"
#include "viceinode.h"
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include "afs/assert.h"
#endif /* AFS_PTHREAD_ENV */
#include <limits.h>

#ifndef AFS_NT40_ENV
#ifdef O_LARGEFILE
#define afs_stat	stat64
#define afs_fstat	fstat64
#else /* !O_LARGEFILE */
#define	afs_stat	stat
#define	afs_fstat	fstat
#endif /* !O_LARGEFILE */
#endif /* AFS_NT40_ENV */

#ifdef AFS_PTHREAD_ENV
pthread_once_t ih_glock_once = PTHREAD_ONCE_INIT;
pthread_mutex_t ih_glock_mutex;
#endif /* AFS_PTHREAD_ENV */

/* Linked list of available inode handles */
IHandle_t *ihAvailHead;
IHandle_t *ihAvailTail;

/* Linked list of available file descriptor handles */
FdHandle_t *fdAvailHead;
FdHandle_t *fdAvailTail;

/* Linked list of available stream descriptor handles */
StreamHandle_t *streamAvailHead;
StreamHandle_t *streamAvailTail;

/* LRU list for file descriptor handles */
FdHandle_t *fdLruHead;
FdHandle_t *fdLruTail;

int ih_Inited = 0;

/* Most of the servers use fopen/fdopen. Since the FILE structure
 * only has eight bits for the file descriptor, the cache size
 * has to be less than 256. The cache can be made larger as long
 * as you are sure you don't need fopen/fdopen. */
int fdMaxCacheSize = 0;
int fdCacheSize = 0;

/* Number of in use file descriptors */
int fdInUseCount = 0;

/* Hash table for inode handles */
IHashBucket_t ihashTable[I_HANDLE_HASH_SIZE];


#ifdef AFS_PTHREAD_ENV
/* Initialize the global ihandle mutex */
void
ih_glock_init()
{
    assert(pthread_mutex_init(&ih_glock_mutex, NULL) == 0);
}
#endif /* AFS_PTHREAD_ENV */

/* Initialize the file descriptor cache */
void
ih_Initialize(void)
{
    int i;
    assert(!ih_Inited);
    ih_Inited = 1;
    DLL_INIT_LIST(ihAvailHead, ihAvailTail);
    DLL_INIT_LIST(fdAvailHead, fdAvailTail);
    DLL_INIT_LIST(fdLruHead, fdLruTail);
    for (i = 0; i < I_HANDLE_HASH_SIZE; i++) {
	DLL_INIT_LIST(ihashTable[i].ihash_head, ihashTable[i].ihash_tail);
    }
#if defined(AFS_NT40_ENV)
    fdMaxCacheSize = FD_MAX_CACHESIZE;
#elif defined(AFS_SUN5_ENV) || defined(AFS_NBSD_ENV)
    {
	struct rlimit rlim;
	assert(getrlimit(RLIMIT_NOFILE, &rlim) == 0);
	rlim.rlim_cur = rlim.rlim_max;
	assert(setrlimit(RLIMIT_NOFILE, &rlim) == 0);
	fdMaxCacheSize = rlim.rlim_cur - FD_HANDLE_SETASIDE;
#ifdef AFS_NBSD_ENV
	/* XXX this is to avoid using up all system fd netbsd is
	 * somewhat broken and have set maximum fd for a root process
	 * to the same as system fd that is avaible, so if the
	 * fileserver uses all up process fds, all system fd will be
	 * used up too !
	 *
	 * Check for this better
	 */
	fdMaxCacheSize /= 4;
#endif
	fdMaxCacheSize = MIN(fdMaxCacheSize, FD_MAX_CACHESIZE);
	assert(fdMaxCacheSize > 0);
    }
#elif defined(AFS_HPUX_ENV)
    /* Avoid problems with "UFSOpen: igetinode failed" panics on HPUX 11.0 */
    fdMaxCacheSize = 0;
#else
    {
	long fdMax = MAX(sysconf(_SC_OPEN_MAX) - FD_HANDLE_SETASIDE, 0);
	fdMaxCacheSize = (int)MIN(fdMax, FD_MAX_CACHESIZE);
    }
#endif
    fdCacheSize = MIN(fdMaxCacheSize, FD_DEFAULT_CACHESIZE);

    {
	void *ih_sync_thread();
#ifdef AFS_PTHREAD_ENV
	pthread_t syncer;
	pthread_attr_t tattr;

	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);

	pthread_create(&syncer, &tattr, ih_sync_thread, NULL);
#else /* AFS_PTHREAD_ENV */
	PROCESS syncer;
	LWP_CreateProcess(ih_sync_thread, 16*1024, LWP_MAX_PRIORITY - 2,
	    NULL, "ih_syncer", &syncer);
#endif /* AFS_PTHREAD_ENV */
    }

}

/* Make the file descriptor cache as big as possible. Don't this call
 * if the program uses fopen or fdopen. */
void
ih_UseLargeCache(void)
{
    IH_LOCK;
    if (!ih_Inited) {
	ih_Initialize();
    }
    fdCacheSize = fdMaxCacheSize;

    IH_UNLOCK;
}

/* Allocate a chunk of inode handles */
void
iHandleAllocateChunk(void)
{
    int i;
    IHandle_t *ihP;

    assert(ihAvailHead == NULL);
    ihP = (IHandle_t *) malloc(I_HANDLE_MALLOCSIZE * sizeof(IHandle_t));
    assert(ihP != NULL);
    for (i = 0; i < I_HANDLE_MALLOCSIZE; i++) {
	ihP[i].ih_refcnt = 0;
	DLL_INSERT_TAIL(&ihP[i], ihAvailHead, ihAvailTail, ih_next, ih_prev);
    }
}

/* Initialize an inode handle */
IHandle_t *
ih_init(int dev, int vid, Inode ino)
{
    int ihash = IH_HASH(dev, vid, ino);
    IHandle_t *ihP;

    IH_LOCK;
    if (!ih_Inited) {
	ih_Initialize();
    }

    /* Do we already have a handle for this Inode? */
    for (ihP = ihashTable[ihash].ihash_head; ihP; ihP = ihP->ih_next) {
	if (ihP->ih_ino == ino && ihP->ih_vid == vid && ihP->ih_dev == dev) {
	    ihP->ih_refcnt++;
	    IH_UNLOCK;
	    return ihP;
	}
    }

    /* Allocate and initialize a new Inode handle */
    if (ihAvailHead == NULL) {
	iHandleAllocateChunk();
    }
    ihP = ihAvailHead;
    assert(ihP->ih_refcnt == 0);
    DLL_DELETE(ihP, ihAvailHead, ihAvailTail, ih_next, ih_prev);
    ihP->ih_dev = dev;
    ihP->ih_vid = vid;
    ihP->ih_ino = ino;
    ihP->ih_flags = 0;
    ihP->ih_refcnt = 1;
    DLL_INIT_LIST(ihP->ih_fdhead, ihP->ih_fdtail);
    DLL_INSERT_TAIL(ihP, ihashTable[ihash].ihash_head,
		    ihashTable[ihash].ihash_tail, ih_next, ih_prev);
    IH_UNLOCK;
    return ihP;
}

/* Copy an inode handle */
IHandle_t *
ih_copy(IHandle_t * ihP)
{
    IH_LOCK;
    assert(ih_Inited);
    assert(ihP->ih_refcnt > 0);
    ihP->ih_refcnt++;
    IH_UNLOCK;
    return ihP;
}

/* Allocate a chunk of file descriptor handles */
void
fdHandleAllocateChunk(void)
{
    int i;
    FdHandle_t *fdP;

    assert(fdAvailHead == NULL);
    fdP = (FdHandle_t *) malloc(FD_HANDLE_MALLOCSIZE * sizeof(FdHandle_t));
    assert(fdP != NULL);
    for (i = 0; i < FD_HANDLE_MALLOCSIZE; i++) {
	fdP[i].fd_status = FD_HANDLE_AVAIL;
	fdP[i].fd_ih = NULL;
	fdP[i].fd_fd = INVALID_FD;
	DLL_INSERT_TAIL(&fdP[i], fdAvailHead, fdAvailTail, fd_next, fd_prev);
    }
}

/* Allocate a chunk of stream handles */
void
streamHandleAllocateChunk(void)
{
    int i;
    StreamHandle_t *streamP;

    assert(streamAvailHead == NULL);
    streamP = (StreamHandle_t *)
	malloc(STREAM_HANDLE_MALLOCSIZE * sizeof(StreamHandle_t));
    assert(streamP != NULL);
    for (i = 0; i < STREAM_HANDLE_MALLOCSIZE; i++) {
	streamP[i].str_fd = INVALID_FD;
	DLL_INSERT_TAIL(&streamP[i], streamAvailHead, streamAvailTail,
			str_next, str_prev);
    }
}

/*
 * Get a file descriptor handle given an Inode handle
 */
FdHandle_t *
ih_open(IHandle_t * ihP)
{
    FdHandle_t *fdP;
    FD_t fd;
    FD_t closeFd;

    if (!ihP)			/* XXX should log here in the fileserver */
	return NULL;

    IH_LOCK;

    /* Do we already have an open file handle for this Inode? */
    for (fdP = ihP->ih_fdtail; fdP != NULL; fdP = fdP->fd_ihprev) {
	if (fdP->fd_status != FD_HANDLE_INUSE) {
	    assert(fdP->fd_status == FD_HANDLE_OPEN);
	    fdP->fd_status = FD_HANDLE_INUSE;
	    DLL_DELETE(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);
	    ihP->ih_refcnt++;
	    IH_UNLOCK;
	    (void)FDH_SEEK(fdP, 0, SEEK_SET);
	    return fdP;
	}
    }

    /*
     * Try to open the Inode, return NULL on error.
     */
    fdInUseCount += 1;
    IH_UNLOCK;
    fd = OS_IOPEN(ihP);
    IH_LOCK;
    if (fd == INVALID_FD) {
	fdInUseCount -= 1;
	IH_UNLOCK;
	return NULL;
    }

    /* fdCacheSize limits the size of the descriptor cache, but
     * we permit the number of open files to exceed fdCacheSize.
     * We only recycle open file descriptors when the number
     * of open files reaches the size of the cache */
    if (fdInUseCount > fdCacheSize && fdLruHead != NULL) {
	fdP = fdLruHead;
	assert(fdP->fd_status == FD_HANDLE_OPEN);
	DLL_DELETE(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);
	DLL_DELETE(fdP, fdP->fd_ih->ih_fdhead, fdP->fd_ih->ih_fdtail,
		   fd_ihnext, fd_ihprev);
	closeFd = fdP->fd_fd;
    } else {
	if (fdAvailHead == NULL) {
	    fdHandleAllocateChunk();
	}
	fdP = fdAvailHead;
	assert(fdP->fd_status == FD_HANDLE_AVAIL);
	DLL_DELETE(fdP, fdAvailHead, fdAvailTail, fd_next, fd_prev);
	closeFd = INVALID_FD;
    }

    fdP->fd_status = FD_HANDLE_INUSE;
    fdP->fd_fd = fd;
    fdP->fd_ih = ihP;

    ihP->ih_refcnt++;

    /* Add this handle to the Inode's list of open descriptors */
    DLL_INSERT_TAIL(fdP, ihP->ih_fdhead, ihP->ih_fdtail, fd_ihnext,
		    fd_ihprev);

    if (closeFd != INVALID_FD) {
	IH_UNLOCK;
	OS_CLOSE(closeFd);
	IH_LOCK;
	fdInUseCount -= 1;
    }

    IH_UNLOCK;
    return fdP;
}

/*
 * Return a file descriptor handle to the cache
 */
int
fd_close(FdHandle_t * fdP)
{
    IHandle_t *ihP;

    if (!fdP)
	return 0;

    IH_LOCK;
    assert(ih_Inited);
    assert(fdInUseCount > 0);
    assert(fdP->fd_status == FD_HANDLE_INUSE);

    ihP = fdP->fd_ih;

    /* Call fd_reallyclose to really close the unused file handles if
     * the previous attempt to close (ih_reallyclose()) all file handles
     * failed (this is determined by checking the ihandle for the flag
     * IH_REALLY_CLOSED) or we have too many open files.
     */
    if (ihP->ih_flags & IH_REALLY_CLOSED || fdInUseCount > fdCacheSize) {
	IH_UNLOCK;
	return fd_reallyclose(fdP);
    }

    /* Put this descriptor back into the cache */
    fdP->fd_status = FD_HANDLE_OPEN;
    DLL_INSERT_TAIL(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);

    /* If this is not the only reference to the Inode then we can decrement
     * the reference count, otherwise we need to call ih_release.
     */
    if (ihP->ih_refcnt > 1) {
	ihP->ih_refcnt--;
	IH_UNLOCK;
    } else {
	IH_UNLOCK;
	ih_release(ihP);
    }

    return 0;
}

/*
 * Actually close the file descriptor handle and return it to
 * the free list.
 */
int
fd_reallyclose(FdHandle_t * fdP)
{
    FD_t closeFd;
    IHandle_t *ihP;

    if (!fdP)
	return 0;

    IH_LOCK;
    assert(ih_Inited);
    assert(fdInUseCount > 0);
    assert(fdP->fd_status == FD_HANDLE_INUSE);

    ihP = fdP->fd_ih;
    closeFd = fdP->fd_fd;

    DLL_DELETE(fdP, ihP->ih_fdhead, ihP->ih_fdtail, fd_ihnext, fd_ihprev);
    DLL_INSERT_TAIL(fdP, fdAvailHead, fdAvailTail, fd_next, fd_prev);

    fdP->fd_status = FD_HANDLE_AVAIL;
    fdP->fd_ih = NULL;
    fdP->fd_fd = INVALID_FD;

    /* All the file descriptor handles have been closed; reset
     * the IH_REALLY_CLOSED flag indicating that ih_reallyclose
     * has completed its job.
     */
    if (!ihP->ih_fdhead) {
	ihP->ih_flags &= ~IH_REALLY_CLOSED;
    }

    IH_UNLOCK;
    OS_CLOSE(closeFd);
    IH_LOCK;
    fdInUseCount -= 1;

    /* If this is not the only reference to the Inode then we can decrement
     * the reference count, otherwise we need to call ih_release. */
    if (ihP->ih_refcnt > 1) {
	ihP->ih_refcnt--;
	IH_UNLOCK;
    } else {
	IH_UNLOCK;
	ih_release(ihP);
    }

    return 0;
}

/* Enable buffered I/O on a file descriptor */
StreamHandle_t *
stream_fdopen(FD_t fd)
{
    StreamHandle_t *streamP;

    IH_LOCK;
    if (streamAvailHead == NULL) {
	streamHandleAllocateChunk();
    }
    streamP = streamAvailHead;
    DLL_DELETE(streamP, streamAvailHead, streamAvailTail, str_next, str_prev);
    IH_UNLOCK;
    streamP->str_fd = fd;
    streamP->str_buflen = 0;
    streamP->str_bufoff = 0;
    streamP->str_error = 0;
    streamP->str_eof = 0;
    streamP->str_direction = STREAM_DIRECTION_NONE;
    return streamP;
}

/* Open a file for buffered I/O */
StreamHandle_t *
stream_open(const char *filename, const char *mode)
{
    FD_t fd;

    if (strcmp(mode, "r") == 0) {
	fd = OS_OPEN(filename, O_RDONLY, 0);
    } else if (strcmp(mode, "r+") == 0) {
	fd = OS_OPEN(filename, O_RDWR, 0);
    } else if (strcmp(mode, "w") == 0) {
	fd = OS_OPEN(filename, O_WRONLY | O_TRUNC | O_CREAT, 0);
    } else if (strcmp(mode, "w+") == 0) {
	fd = OS_OPEN(filename, O_RDWR | O_TRUNC | O_CREAT, 0);
    } else if (strcmp(mode, "a") == 0) {
	fd = OS_OPEN(filename, O_WRONLY | O_APPEND | O_CREAT, 0);
    } else if (strcmp(mode, "a+") == 0) {
	fd = OS_OPEN(filename, O_RDWR | O_APPEND | O_CREAT, 0);
    } else {
	assert(FALSE);		/* not implemented */
    }

    if (fd == INVALID_FD) {
	return NULL;
    }
    return stream_fdopen(fd);
}

/* fread for buffered I/O handles */
afs_sfsize_t
stream_read(void *ptr, afs_fsize_t size, afs_fsize_t nitems,
	    StreamHandle_t * streamP)
{
    afs_fsize_t nbytes, bytesRead, bytesToRead;
    char *p;

    /* Need to seek before changing direction */
    if (streamP->str_direction == STREAM_DIRECTION_NONE) {
	streamP->str_direction = STREAM_DIRECTION_READ;
	streamP->str_bufoff = 0;
	streamP->str_buflen = 0;
    } else {
	assert(streamP->str_direction == STREAM_DIRECTION_READ);
    }

    bytesRead = 0;
    nbytes = size * nitems;
    p = (char *)ptr;
    while (nbytes > 0 && !streamP->str_eof) {
	if (streamP->str_buflen == 0) {
	    streamP->str_bufoff = 0;
	    streamP->str_buflen =
		OS_READ(streamP->str_fd, streamP->str_buffer,
			STREAM_HANDLE_BUFSIZE);
	    if (streamP->str_buflen < 0) {
		streamP->str_error = errno;
		streamP->str_buflen = 0;
		bytesRead = 0;
		break;
	    } else if (streamP->str_buflen == 0) {
		streamP->str_eof = 1;
		break;
	    }
	}

	bytesToRead = nbytes;
	if (bytesToRead > streamP->str_buflen) {
	    bytesToRead = streamP->str_buflen;
	}
	memcpy(p, streamP->str_buffer + streamP->str_bufoff, bytesToRead);
	p += bytesToRead;
	streamP->str_bufoff += bytesToRead;
	streamP->str_buflen -= bytesToRead;
	bytesRead += bytesToRead;
	nbytes -= bytesToRead;
    }

    return (bytesRead / size);
}

/* fwrite for buffered I/O handles */
afs_sfsize_t
stream_write(void *ptr, afs_fsize_t size, afs_fsize_t nitems,
	     StreamHandle_t * streamP)
{
    char *p;
    afs_sfsize_t rc;
    afs_fsize_t nbytes, bytesWritten, bytesToWrite;

    /* Need to seek before changing direction */
    if (streamP->str_direction == STREAM_DIRECTION_NONE) {
	streamP->str_direction = STREAM_DIRECTION_WRITE;
	streamP->str_bufoff = 0;
	streamP->str_buflen = STREAM_HANDLE_BUFSIZE;
    } else {
	assert(streamP->str_direction == STREAM_DIRECTION_WRITE);
    }

    nbytes = size * nitems;
    bytesWritten = 0;
    p = (char *)ptr;
    while (nbytes > 0) {
	if (streamP->str_buflen == 0) {
	    rc = OS_WRITE(streamP->str_fd, streamP->str_buffer,
			  STREAM_HANDLE_BUFSIZE);
	    if (rc < 0) {
		streamP->str_error = errno;
		bytesWritten = 0;
		break;
	    }
	    streamP->str_bufoff = 0;
	    streamP->str_buflen = STREAM_HANDLE_BUFSIZE;
	}

	bytesToWrite = nbytes;
	if (bytesToWrite > streamP->str_buflen) {
	    bytesToWrite = streamP->str_buflen;
	}
	memcpy(streamP->str_buffer + streamP->str_bufoff, p, bytesToWrite);
	p += bytesToWrite;
	streamP->str_bufoff += bytesToWrite;
	streamP->str_buflen -= bytesToWrite;
	bytesWritten += bytesToWrite;
	nbytes -= bytesToWrite;
    }

    return (bytesWritten / size);
}

/* fseek for buffered I/O handles */
int
stream_seek(StreamHandle_t * streamP, afs_foff_t offset, int whence)
{
    int rc;
    int retval = 0;

    if (streamP->str_direction == STREAM_DIRECTION_WRITE
	&& streamP->str_bufoff > 0) {
	rc = OS_WRITE(streamP->str_fd, streamP->str_buffer,
		      streamP->str_bufoff);
	if (rc < 0) {
	    streamP->str_error = errno;
	    retval = -1;
	}
    }
    streamP->str_bufoff = 0;
    streamP->str_buflen = 0;
    streamP->str_eof = 0;
    streamP->str_direction = STREAM_DIRECTION_NONE;
    if (OS_SEEK(streamP->str_fd, offset, whence) < 0) {
	streamP->str_error = errno;
	retval = -1;
    }
    return retval;
}

/* fflush for buffered I/O handles */
int
stream_flush(StreamHandle_t * streamP)
{
    int rc;
    int retval = 0;

    if (streamP->str_direction == STREAM_DIRECTION_WRITE
	&& streamP->str_bufoff > 0) {
	rc = OS_WRITE(streamP->str_fd, streamP->str_buffer,
		      streamP->str_bufoff);
	if (rc < 0) {
	    streamP->str_error = errno;
	    retval = -1;
	}
	streamP->str_bufoff = 0;
	streamP->str_buflen = STREAM_HANDLE_BUFSIZE;
    }

    return retval;
}

/* Free a buffered I/O handle */
int
stream_close(StreamHandle_t * streamP, int reallyClose)
{
    int rc;
    int retval = 0;

    assert(streamP != NULL);
    if (streamP->str_direction == STREAM_DIRECTION_WRITE
	&& streamP->str_bufoff > 0) {
	rc = OS_WRITE(streamP->str_fd, streamP->str_buffer,
		      streamP->str_bufoff);
	if (rc < 0) {
	    retval = -1;
	}
    }
    if (reallyClose) {
	rc = OS_CLOSE(streamP->str_fd);
	if (rc < 0) {
	    retval = -1;
	}
    }
    streamP->str_fd = INVALID_FD;

    IH_LOCK;
    DLL_INSERT_TAIL(streamP, streamAvailHead, streamAvailTail,
		    str_next, str_prev);
    IH_UNLOCK;
    return retval;
}

/* Close all unused file descriptors associated with the inode
 * handle. Called with IH_LOCK held. May drop and reacquire
 * IH_LOCK. Sets the IH_REALLY_CLOSED flag in the inode handle
 * if it fails to close all file handles.
 */
static int
ih_fdclose(IHandle_t * ihP)
{
    int closeCount, closedAll;
    FdHandle_t *fdP, *head, *tail, *next;

    assert(ihP->ih_refcnt > 0);

    closedAll = 1;
    DLL_INIT_LIST(head, tail);
    ihP->ih_flags &= ~IH_REALLY_CLOSED;

    /*
     * Remove the file descriptors for this Inode from the LRU queue
     * and the IHandle queue and put them on a temporary queue so we
     * can drop the lock before we close the files.
     */
    for (fdP = ihP->ih_fdhead; fdP != NULL; fdP = next) {
	next = fdP->fd_ihnext;
	assert(fdP->fd_ih == ihP);
	assert(fdP->fd_status == FD_HANDLE_OPEN
	       || fdP->fd_status == FD_HANDLE_INUSE);
	if (fdP->fd_status == FD_HANDLE_OPEN) {
	    DLL_DELETE(fdP, ihP->ih_fdhead, ihP->ih_fdtail, fd_ihnext,
		       fd_ihprev);
	    DLL_DELETE(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);
	    DLL_INSERT_TAIL(fdP, head, tail, fd_next, fd_prev);
	} else {
	    closedAll = 0;
	    ihP->ih_flags |= IH_REALLY_CLOSED;
	}
    }

    /* If the ihandle reference count is 1, we should have
     * closed all file descriptors.
     */
    if (ihP->ih_refcnt == 1 || closedAll) {
	assert(closedAll);
	assert(!ihP->ih_fdhead);
	assert(!ihP->ih_fdtail);
    }

    if (head == NULL) {
	return 0;		/* No file descriptors closed */
    }

    IH_UNLOCK;
    /*
     * Close the file descriptors
     */
    closeCount = 0;
    for (fdP = head; fdP != NULL; fdP = fdP->fd_next) {
	OS_CLOSE(fdP->fd_fd);
	fdP->fd_status = FD_HANDLE_AVAIL;
	fdP->fd_fd = INVALID_FD;
	fdP->fd_ih = NULL;
	closeCount++;
    }

    IH_LOCK;
    assert(fdInUseCount >= closeCount);
    fdInUseCount -= closeCount;

    /*
     * Append the temporary queue to the list of available descriptors
     */
    if (fdAvailHead == NULL) {
	fdAvailHead = head;
	fdAvailTail = tail;
    } else {
	fdAvailTail->fd_next = head;
	head->fd_prev = fdAvailTail;
	fdAvailTail = tail;
    }

    return 0;
}

/* Close all cached file descriptors for this inode. */
int
ih_reallyclose(IHandle_t * ihP)
{
    if (!ihP)
	return 0;

    IH_LOCK;
    assert(ihP->ih_refcnt > 0);
    ih_fdclose(ihP);

    IH_UNLOCK;
    return 0;
}

/* Release an Inode handle. All cached file descriptors for this
 * inode are closed when the last reference to this handle is released
 */
int
ih_release(IHandle_t * ihP)
{
    int ihash;

    if (!ihP)
	return 0;

    IH_LOCK;
    assert(ihP->ih_refcnt > 0);

    if (ihP->ih_refcnt > 1) {
	ihP->ih_refcnt--;
	IH_UNLOCK;
	return 0;
    }

    ihash = IH_HASH(ihP->ih_dev, ihP->ih_vid, ihP->ih_ino);
    DLL_DELETE(ihP, ihashTable[ihash].ihash_head,
	       ihashTable[ihash].ihash_tail, ih_next, ih_prev);

    ih_fdclose(ihP);

    ihP->ih_refcnt--;

    DLL_INSERT_TAIL(ihP, ihAvailHead, ihAvailTail, ih_next, ih_prev);

    IH_UNLOCK;
    return 0;
}

/* Sync an inode to disk if its handle isn't NULL */
int
ih_condsync(IHandle_t * ihP)
{
    int code;
    FdHandle_t *fdP;

    if (!ihP)
	return 0;

    fdP = IH_OPEN(ihP);
    if (fdP == NULL)
	return -1;

    code = FDH_SYNC(fdP);
    FDH_CLOSE(fdP);

    return code;
}

void
ih_sync_all() {
    int ihash;

    IH_LOCK;
    for (ihash = 0; ihash < I_HANDLE_HASH_SIZE; ihash++) {
	IHandle_t *ihP, *ihPnext;

	ihP = ihashTable[ihash].ihash_head;
	if (ihP)
	    ihP->ih_refcnt++;	/* must not disappear over unlock */
	for (; ihP; ihP = ihPnext) {
	    
	    if (ihP->ih_synced) {
		FdHandle_t *fdP;

		ihP->ih_synced = 0;
		IH_UNLOCK;

		fdP = IH_OPEN(ihP);
		if (fdP) OS_SYNC(fdP->fd_fd);
		FDH_CLOSE(fdP);

	  	IH_LOCK;
	    }

	    /* when decrementing the refcount, the ihandle might disappear
	       and we might not even be able to proceed to the next one.
	       Hence the gymnastics putting a hold on the next one already */
	    ihPnext = ihP->ih_next;
	    if (ihPnext) ihPnext->ih_refcnt++;

	    if (ihP->ih_refcnt > 1) {
		ihP->ih_refcnt--;
	    } else {
		IH_UNLOCK;
		ih_release(ihP);
		IH_LOCK;
	    }

	}
    }
    IH_UNLOCK;
}

void *
ih_sync_thread() {
    while(1) {

#ifdef AFS_PTHREAD_ENV
	sleep(10);
#else /* AFS_PTHREAD_ENV */
	IOMGR_Sleep(60);
#endif /* AFS_PTHREAD_ENV */

#ifndef AFS_NT40_ENV
        sync();
#endif
        ih_sync_all();
    }
}


/*************************************************************************
 * OS specific support routines.
 *************************************************************************/
#ifndef AFS_NAMEI_ENV
Inode
ih_icreate(IHandle_t * ih, int dev, char *part, Inode nI, int p1, int p2,
	   int p3, int p4)
{
    Inode ino;
#ifdef	AFS_3DISPARES
    /* See viceinode.h */
    if (p2 == INODESPECIAL) {
	int tp = p3;
	p3 = p4;
	p4 = tp;
    }
#endif
    ino = ICREATE(dev, part, nI, p1, p2, p3, p4);
    return ino;
}
#endif /* AFS_NAMEI_ENV */


#ifndef AFS_NT40_ENV
afs_sfsize_t
ih_size(int fd)
{
    struct afs_stat status;
    if (afs_fstat(fd, &status) < 0)
	return -1;
    return status.st_size;
}
#endif
