
#ifndef lint
#endif

/*
 * COPYRIGHT (C) Transarc Corporation 1998
 */
/************************************************************************/
/*									*/
/*  ihandle.c	- file descriptor cacheing for Inode handles.           */
/*									*/
/************************************************************************/

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#else
#include <sys/file.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef	AFS_SUN5_ENV
#include <sys/fcntl.h>
#include <sys/resource.h>
#endif
#endif
#include <rx/xdr.h>
#include <afs/afsint.h>
#include <errno.h>
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "nfs.h"
#include "viceinode.h"
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include "afs/assert.h"
#endif /* AFS_PTHREAD_ENV */
#include <limits.h>

extern afs_int32 DErrno;

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
void ih_glock_init()
{
    assert(pthread_mutex_init(&ih_glock_mutex, NULL) == 0);
}
#endif /* AFS_PTHREAD_ENV */

/* Initialize the file descriptor cache */
void ih_Initialize() {
    int i;
#ifdef AFS_SUN5_ENV
    struct rlimit rlim;
#endif /* AFS_SUN5_ENV */
    assert(!ih_Inited);
    ih_Inited = 1;
    DLL_INIT_LIST(ihAvailHead, ihAvailTail);
    DLL_INIT_LIST(fdAvailHead, fdAvailTail);
    DLL_INIT_LIST(fdLruHead, fdLruTail);
    for (i = 0 ; i < I_HANDLE_HASH_SIZE ; i++) {
	DLL_INIT_LIST(ihashTable[i].ihash_head, ihashTable[i].ihash_tail);
    }
#if defined(AFS_NT40_ENV)
    fdMaxCacheSize = FD_MAX_CACHESIZE;
#elif defined(AFS_SUN5_ENV)
    assert(getrlimit(RLIMIT_NOFILE, &rlim) == 0);
    rlim.rlim_cur = rlim.rlim_max;
    assert(setrlimit(RLIMIT_NOFILE, &rlim) == 0);
    fdMaxCacheSize = rlim.rlim_cur-FD_HANDLE_SETASIDE;
    fdMaxCacheSize = MIN(fdMaxCacheSize, FD_MAX_CACHESIZE);
    assert(fdMaxCacheSize > 0);
#elif defined(AFS_HPUX_ENV)
    /* Avoid problems with "UFSOpen: igetinode failed" panics on HPUX 11.0 */
    fdMaxCacheSize = 0;
#else
    fdMaxCacheSize = MAX(sysconf(_SC_OPEN_MAX)-FD_HANDLE_SETASIDE, 0);
    fdMaxCacheSize = MIN(fdMaxCacheSize, FD_MAX_CACHESIZE);
#endif
    fdCacheSize = MIN(fdMaxCacheSize, FD_DEFAULT_CACHESIZE);
}

/* Make the file descriptor cache as big as possible. Don't this call
 * if the program uses fopen or fdopen. */
void ih_UseLargeCache() {
    IH_LOCK

    if (!ih_Inited) {
	ih_Initialize();
    }
    fdCacheSize = fdMaxCacheSize;

    IH_UNLOCK
}

/* Allocate a chunk of inode handles */
void iHandleAllocateChunk()
{
    int i;
    IHandle_t *ihP;

    assert(ihAvailHead == NULL);
    ihP = (IHandle_t *)malloc(I_HANDLE_MALLOCSIZE * sizeof(IHandle_t));
    assert(ihP != NULL);
    for (i = 0 ; i < I_HANDLE_MALLOCSIZE ; i++) {
	ihP[i].ih_refcnt = 0;
	DLL_INSERT_TAIL(&ihP[i], ihAvailHead, ihAvailTail, ih_next, ih_prev);
    }
}

/* Initialize an inode handle */
IHandle_t *ih_init(int dev, int vid, Inode ino)
{
    int ihash = IH_HASH(dev, vid, ino);
    IHandle_t *ihP;

    IH_LOCK

    if (!ih_Inited) {
	ih_Initialize();
    }

    /* Do we already have a handle for this Inode? */
    for (ihP = ihashTable[ihash].ihash_head ; ihP ; ihP = ihP->ih_next) {
	if (ihP->ih_ino == ino && ihP->ih_vid == vid && ihP->ih_dev == dev) {
	    ihP->ih_refcnt++;
	    IH_UNLOCK
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
    IH_UNLOCK
    return ihP;
}

/* Copy an inode handle */
IHandle_t *ih_copy(IHandle_t *ihP)
{
    IH_LOCK
    assert(ih_Inited);
    assert(ihP->ih_refcnt > 0);
    ihP->ih_refcnt++;
    IH_UNLOCK
    return ihP;
}

/* Allocate a chunk of file descriptor handles */
void fdHandleAllocateChunk()
{
    int i;
    FdHandle_t *fdP;

    assert(fdAvailHead == NULL);
    fdP = (FdHandle_t *)malloc(FD_HANDLE_MALLOCSIZE * sizeof(FdHandle_t));
    assert(fdP != NULL);
    for (i = 0 ; i < FD_HANDLE_MALLOCSIZE ; i++) {
	fdP[i].fd_status = FD_HANDLE_AVAIL;
	fdP[i].fd_ih = NULL;
	fdP[i].fd_fd = INVALID_FD;
	DLL_INSERT_TAIL(&fdP[i], fdAvailHead, fdAvailTail, fd_next, fd_prev);
    }
}

/* Allocate a chunk of stream handles */
void streamHandleAllocateChunk()
{
    int i;
    StreamHandle_t *streamP;

    assert(streamAvailHead == NULL);
    streamP = (StreamHandle_t *)
	      malloc(STREAM_HANDLE_MALLOCSIZE * sizeof(StreamHandle_t));
    assert(streamP != NULL);
    for (i = 0 ; i < STREAM_HANDLE_MALLOCSIZE ; i++) {
	streamP[i].str_fd = INVALID_FD;
	DLL_INSERT_TAIL(&streamP[i], streamAvailHead, streamAvailTail,
			str_next, str_prev);
    }
}

/*
 * Get a file descriptor handle given an Inode handle
 */
FdHandle_t *ih_open(IHandle_t *ihP)
{
    FdHandle_t *fdP;
    FD_t fd;
    FD_t closeFd;

    IH_LOCK

    if (!ih_Inited) {
	ih_Initialize();
    }

    /* Do we already have an open file handle for this Inode? */
    for (fdP = ihP->ih_fdtail ; fdP != NULL ; fdP = fdP->fd_ihprev) {
	if (fdP->fd_status != FD_HANDLE_INUSE) {
	    assert(fdP->fd_status == FD_HANDLE_OPEN);
	    fdP->fd_status = FD_HANDLE_INUSE;
	    DLL_DELETE(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);
	    ihP->ih_refcnt++;
	    IH_UNLOCK
	    FDH_SEEK(fdP, 0, SEEK_SET);
	    return fdP;
	}
    }

    /*
     * Try to open the Inode, return NULL on error.
     */
    fdInUseCount += 1;
    IH_UNLOCK
    fd = OS_IOPEN(ihP);
    IH_LOCK
    if (fd == INVALID_FD) {
	fdInUseCount -= 1;
	IH_UNLOCK
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
    /* Add this handle to the Inode's list of open descriptors */
    DLL_INSERT_TAIL(fdP, ihP->ih_fdhead, ihP->ih_fdtail, fd_ihnext, fd_ihprev);

    if (closeFd != INVALID_FD) {
	IH_UNLOCK
	OS_CLOSE(closeFd);
	IH_LOCK
	fdInUseCount -= 1;
    }

    ihP->ih_refcnt++;
    IH_UNLOCK
    return fdP;
}

/*
 * Return a file descriptor handle to the cache
 */
int fd_close(FdHandle_t *fdP)
{
    FD_t closeFd;
    IHandle_t *ihP;

    if (!fdP)
	return 0;

    IH_LOCK
    assert(ih_Inited);
    assert(fdInUseCount > 0);
    assert(fdP->fd_status == FD_HANDLE_INUSE);

    ihP = fdP->fd_ih;

    /* If a previous attempt to close  ( ih_reallyclose() )
     * all fd handles failed, then the IH_REALLY_CLOSED flag is set in
     * the Inode handle so we call fd_reallyclose
     */

    if ( ihP->ih_flags & IH_REALLY_CLOSED ) {
	IH_UNLOCK
	return (fd_reallyclose(fdP));
    }

    /* If we have too many open files then close the descriptor. If we
     * hold the last reference to the Inode handle then wait and let
     * ih_release do the work.  */
    if (fdInUseCount > fdCacheSize && ihP->ih_refcnt > 1) {
	assert(fdInUseCount > 0);
	closeFd = fdP->fd_fd;
	DLL_DELETE(fdP, fdP->fd_ih->ih_fdhead, fdP->fd_ih->ih_fdtail,
		   fd_ihnext, fd_ihprev);
	DLL_INSERT_TAIL(fdP, fdAvailHead, fdAvailTail, fd_next, fd_prev);
	fdP->fd_status = FD_HANDLE_AVAIL;
	fdP->fd_ih = NULL;
	fdP->fd_fd = INVALID_FD;
	ihP->ih_refcnt--;
	IH_UNLOCK
	OS_CLOSE(closeFd);
	IH_LOCK
	fdInUseCount -= 1;
	IH_UNLOCK
	return 0;
    }

    /* Put this descriptor back into the cache */
    fdP->fd_status = FD_HANDLE_OPEN;
    DLL_INSERT_TAIL(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);

    /* If this is not the only reference to the Inode then we can decrement
     * the reference count, otherwise we need to call ih_release. */
    if (ihP->ih_refcnt > 1) {
	ihP->ih_refcnt--;
	IH_UNLOCK
    } else {
	IH_UNLOCK
	ih_release(ihP);
    }

    return 0;
}

/*
 * Return a file descriptor handle to the cache
 */
int fd_reallyclose(FdHandle_t *fdP)
{
    FD_t closeFd;
    IHandle_t *ihP;

    if (!fdP)
	return 0;

    IH_LOCK
    assert(ih_Inited);
    assert(fdInUseCount > 0);
    assert(fdP->fd_status == FD_HANDLE_INUSE);

    ihP = fdP->fd_ih;
    closeFd = fdP->fd_fd;

    DLL_DELETE(fdP, fdP->fd_ih->ih_fdhead, fdP->fd_ih->ih_fdtail,
	       fd_ihnext, fd_ihprev);
    DLL_INSERT_TAIL(fdP, fdAvailHead, fdAvailTail, fd_next, fd_prev);
    fdP->fd_status = FD_HANDLE_AVAIL;
    fdP->fd_ih = NULL;
    fdP->fd_fd = INVALID_FD;
    IH_UNLOCK
    OS_CLOSE(closeFd);
    IH_LOCK
    fdInUseCount -= 1;

    /* If this is not the only reference to the Inode then we can decrement
     * the reference count, otherwise we need to call ih_release. */
    if (ihP->ih_refcnt > 1) {
	ihP->ih_refcnt--;
	IH_UNLOCK
    } else {
	IH_UNLOCK
	ih_release(ihP);
    }
    return 0;
}

/* Enable buffered I/O on a file descriptor */
StreamHandle_t *stream_fdopen(FD_t fd)
{
    StreamHandle_t *streamP;

    IH_LOCK
    if (streamAvailHead == NULL) {
	streamHandleAllocateChunk();
    }
    streamP = streamAvailHead;
    DLL_DELETE(streamP, streamAvailHead, streamAvailTail, str_next, str_prev);
    IH_UNLOCK

    streamP->str_fd = fd;
    streamP->str_buflen = 0;
    streamP->str_bufoff = 0;
    streamP->str_error = 0;
    streamP->str_eof = 0;
    streamP->str_direction = STREAM_DIRECTION_NONE;
    return streamP;
}

/* Open a file for buffered I/O */
StreamHandle_t *stream_open(char *filename, char *mode)
{
    FD_t fd;

    if (strcmp(mode, "r") == 0) {
	fd = OS_OPEN(filename, O_RDONLY, 0);
    } else if (strcmp(mode, "r+") == 0) {
	fd = OS_OPEN(filename, O_RDWR, 0);
    } else if (strcmp(mode, "w") == 0) {
	fd = OS_OPEN(filename, O_WRONLY|O_TRUNC|O_CREAT, 0);
    } else if (strcmp(mode, "w+") == 0) {
	fd = OS_OPEN(filename, O_RDWR|O_TRUNC|O_CREAT, 0);
    } else if (strcmp(mode, "a") == 0) {
	fd = OS_OPEN(filename, O_WRONLY|O_APPEND|O_CREAT, 0);
    } else if (strcmp(mode, "a+") == 0) {
	fd = OS_OPEN(filename, O_RDWR|O_APPEND|O_CREAT, 0);
    } else {
	assert(FALSE); /* not implemented */
    }

    if (fd == INVALID_FD) {
	return NULL;
    }
    return stream_fdopen(fd);
}

/* fread for buffered I/O handles */
int stream_read(void *ptr, int size, int nitems, StreamHandle_t *streamP)
{
    int nbytes, bytesRead, bytesToRead;
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
	    streamP->str_buflen = OS_READ(streamP->str_fd, streamP->str_buffer,
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
	memcpy(p, streamP->str_buffer+streamP->str_bufoff, bytesToRead);
	p += bytesToRead;
	streamP->str_bufoff += bytesToRead;
	streamP->str_buflen -= bytesToRead;
	bytesRead += bytesToRead;
	nbytes -= bytesToRead;
    }

    return (bytesRead/size);
}

/* fwrite for buffered I/O handles */
int stream_write(void *ptr, int size, int nitems, StreamHandle_t *streamP)
{
    char *p;
    int rc, nbytes, bytesWritten, bytesToWrite;

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
	memcpy(streamP->str_buffer+streamP->str_bufoff, p, bytesToWrite);
	p += bytesToWrite;
	streamP->str_bufoff += bytesToWrite;
	streamP->str_buflen -= bytesToWrite;
	bytesWritten += bytesToWrite;
	nbytes -= bytesToWrite;
    }

    return (bytesWritten/size);
}

/* fseek for buffered I/O handles */
int stream_seek(StreamHandle_t *streamP, int offset, int whence)
{
    int rc;
    int retval = 0;

    if (streamP->str_direction == STREAM_DIRECTION_WRITE &&
	streamP->str_bufoff > 0) {
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
int stream_flush(StreamHandle_t *streamP)
{
    int rc;
    int retval = 0;

    if (streamP->str_direction == STREAM_DIRECTION_WRITE &&
	streamP->str_bufoff > 0) {
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
int stream_close(StreamHandle_t *streamP, int reallyClose)
{
    int rc;
    int retval = 0;

    assert(streamP != NULL);
    if (streamP->str_direction == STREAM_DIRECTION_WRITE &&
	streamP->str_bufoff > 0) {
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

    IH_LOCK
    DLL_INSERT_TAIL(streamP, streamAvailHead, streamAvailTail,
		    str_next, str_prev);
    IH_UNLOCK

    return retval;
}

/* Close all cached file descriptors for this inode. */
int ih_reallyclose(IHandle_t *ihP)
{
    int closeCount;
    FdHandle_t *fdP;
    FdHandle_t *head, *tail;

    if (!ihP)
	return 0;

    IH_LOCK

    assert(ihP->ih_refcnt > 0);

    /*
     * Remove the file descriptors for this Inode from the LRU queue
     * and put them on a temporary queue so we drop the lock before
     * we close the files.
     */
    DLL_INIT_LIST(head, tail);
    for (fdP = ihP->ih_fdhead ; fdP != NULL ; fdP = fdP->fd_ihnext) {
	if (fdP->fd_status == FD_HANDLE_OPEN) {
	    assert(fdP->fd_ih == ihP);
	    DLL_DELETE(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);
	    DLL_INSERT_TAIL(fdP, head, tail, fd_next, fd_prev);
	} else {
	    ihP->ih_flags |= IH_REALLY_CLOSED;
	}
    }
    
    /*
     * If we found any file descriptors in use, then we dont zero out
     * fdhead and fdtail, since ih_reallyclose() will be called again on this
     * Inode handle
     */

    if ( ! (ihP->ih_flags & IH_REALLY_CLOSED) )
	DLL_INIT_LIST(ihP->ih_fdhead, ihP->ih_fdtail);

    if (head == NULL) {
	IH_UNLOCK
	return 0;
    }

    /*
     * Close the file descriptors
     */
    closeCount = 0;
    for (fdP = head ; fdP != NULL ; fdP = fdP->fd_ihnext) {
	IH_UNLOCK
	OS_CLOSE(fdP->fd_fd);
	IH_LOCK
	assert(fdInUseCount > 0);
	fdInUseCount -= 1;
	fdP->fd_status = FD_HANDLE_AVAIL;
	fdP->fd_fd = INVALID_FD;
	fdP->fd_ih = NULL;
	closeCount++;
    }

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
    IH_UNLOCK

    return 0;
}

/* Release an Inode handle. All cached file descriptors for this
 * inode are closed when the last reference to this handle is released */
int ih_release(IHandle_t *ihP)
{
    int closeCount;
    FdHandle_t *fdP;
    FdHandle_t *head, *tail;
    int ihash;

    if (!ihP)
	return 0;

    IH_LOCK

    /**
      * If the IH_REALLY_CLOSED flag is set then clear it here before adding
      * the Inode handle to the available queue
      */
    if ( ihP->ih_flags & IH_REALLY_CLOSED )
	ihP->ih_flags &= ~IH_REALLY_CLOSED;

    ihP->ih_refcnt--;
    if (ihP->ih_refcnt > 0) {
	IH_UNLOCK
	return 0;
    }

    assert(ihP->ih_refcnt == 0);

    ihash = IH_HASH(ihP->ih_dev, ihP->ih_vid, ihP->ih_ino);
    DLL_DELETE(ihP, ihashTable[ihash].ihash_head,
	       ihashTable[ihash].ihash_tail, ih_next, ih_prev);

    /*
     * Remove the file descriptors for this Inode from the LRU queue
     * and put them on a temporary queue so we drop the lock before
     * we close the files.
     */
    DLL_INIT_LIST(head, tail);
    for (fdP = ihP->ih_fdhead ; fdP != NULL ; fdP = fdP->fd_ihnext) {
	assert(fdP->fd_status == FD_HANDLE_OPEN);
	assert(fdP->fd_ih == ihP);
	DLL_DELETE(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);
	DLL_INSERT_TAIL(fdP, head, tail, fd_next, fd_prev);
    }
    DLL_INIT_LIST(ihP->ih_fdhead, ihP->ih_fdtail);

    if (head == NULL) {
	DLL_INSERT_TAIL(ihP, ihAvailHead, ihAvailTail, ih_next, ih_prev);
	IH_UNLOCK
	return 0;
    }

    /*
     * Close the file descriptors
     */
    closeCount = 0;
    for (fdP = head ; fdP != NULL ; fdP = fdP->fd_ihnext) {
	IH_UNLOCK
	OS_CLOSE(fdP->fd_fd);
	IH_LOCK
	assert(fdInUseCount > 0);
	fdInUseCount -= 1;
	fdP->fd_status = FD_HANDLE_AVAIL;
	fdP->fd_fd = INVALID_FD;
	fdP->fd_ih = NULL;
	closeCount++;
    }

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
    DLL_INSERT_TAIL(ihP, ihAvailHead, ihAvailTail, ih_next, ih_prev);
    IH_UNLOCK

    return 0;
}

/* Sync an inode to disk if its handle isn't NULL */
int ih_condsync(IHandle_t *ihP)
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



/*************************************************************************
 * OS specific support routines.
 *************************************************************************/
#ifndef AFS_NAMEI_ENV
Inode ih_icreate(IHandle_t *ih, int dev, char *part, Inode nI, int p1, int p2,
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
int ih_size(int fd)
{
    struct stat status;
    if (fstat(fd, &status)<0)
	return -1;
    return status.st_size;
}
#endif
