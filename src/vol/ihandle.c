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


#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
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

#include <rx/xdr.h>
#include <afs/afsint.h>
#include <errno.h>
#include <afs/afssyscalls.h>
#include "nfs.h"
#include "ihandle.h"
#include "viceinode.h"
#include "afs/afs_assert.h"
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
int ih_PkgDefaultsSet = 0;

/* Most of the servers use fopen/fdopen. Since the FILE structure
 * only has eight bits for the file descriptor, the cache size
 * has to be less than 256. The cache can be made larger as long
 * as you are sure you don't need fopen/fdopen. */

/* As noted in ihandle.h, the fileno member of FILE on most platforms
 * in 2008 is a 16- or 32-bit signed int. -Matt
 */
int fdMaxCacheSize = 0;
int fdCacheSize = 0;

/* Number of in use file descriptors */
int fdInUseCount = 0;

/* Hash table for inode handles */
IHashBucket_t ihashTable[I_HANDLE_HASH_SIZE];

void *ih_sync_thread(void *);

/* start-time configurable I/O limits */
ih_init_params vol_io_params;

void ih_PkgDefaults(void)
{
    /* once */
    ih_PkgDefaultsSet = 1;

    /* default to well-known values */
    vol_io_params.fd_handle_setaside = FD_HANDLE_SETASIDE;

    /* initial fd cachesize.  the only one that will be used if
     * the application does not call ih_UseLargeCache().  set this
     * to a value representable in fileno member of the system's
     * FILE structure (or equivalent). */
    vol_io_params.fd_initial_cachesize = FD_DEFAULT_CACHESIZE;

    /* fd cache size that will be used if/when ih_UseLargeCache()
     * is called */
    vol_io_params.fd_max_cachesize = FD_MAX_CACHESIZE;

    vol_io_params.sync_behavior = IH_SYNC_ONCLOSE;
}

int
ih_SetSyncBehavior(const char *behavior)
{
    int val;

    if (strcmp(behavior, "always") == 0) {
	val = IH_SYNC_ALWAYS;

    } else if (strcmp(behavior, "delayed") == 0) {
	val = IH_SYNC_DELAYED;

    } else if (strcmp(behavior, "onclose") == 0) {
	val = IH_SYNC_ONCLOSE;

    } else if (strcmp(behavior, "never") == 0) {
	val = IH_SYNC_NEVER;

    } else {
	/* invalid behavior name */
	return -1;
    }

    vol_io_params.sync_behavior = val;
    return 0;
}

#ifdef AFS_PTHREAD_ENV
/* Initialize the global ihandle mutex */
void
ih_glock_init(void)
{
    MUTEX_INIT(&ih_glock_mutex, "ih glock", MUTEX_DEFAULT, 0);
}
#endif /* AFS_PTHREAD_ENV */

/* Initialize the file descriptor cache */
void
ih_Initialize(void)
{
    int i;
    osi_Assert(!ih_Inited);
    ih_Inited = 1;
    DLL_INIT_LIST(ihAvailHead, ihAvailTail);
    DLL_INIT_LIST(fdAvailHead, fdAvailTail);
    DLL_INIT_LIST(fdLruHead, fdLruTail);
    for (i = 0; i < I_HANDLE_HASH_SIZE; i++) {
	DLL_INIT_LIST(ihashTable[i].ihash_head, ihashTable[i].ihash_tail);
    }
#if defined(AFS_NT40_ENV)
    fdMaxCacheSize = vol_io_params.fd_max_cachesize;
#elif defined(AFS_SUN5_ENV) || defined(AFS_NBSD_ENV)
    {
	struct rlimit rlim;
	osi_Assert(getrlimit(RLIMIT_NOFILE, &rlim) == 0);
	rlim.rlim_cur = rlim.rlim_max;
	osi_Assert(setrlimit(RLIMIT_NOFILE, &rlim) == 0);
	fdMaxCacheSize = rlim.rlim_cur - vol_io_params.fd_handle_setaside;
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
	fdMaxCacheSize = MIN(fdMaxCacheSize, vol_io_params.fd_max_cachesize);
	osi_Assert(fdMaxCacheSize > 0);
    }
#elif defined(AFS_HPUX_ENV)
    /* Avoid problems with "UFSOpen: igetinode failed" panics on HPUX 11.0 */
    fdMaxCacheSize = 0;
#else
    {
	long fdMax = MAX(sysconf(_SC_OPEN_MAX) - vol_io_params.fd_handle_setaside,
					 0);
	fdMaxCacheSize = (int)MIN(fdMax, vol_io_params.fd_max_cachesize);
    }
#endif
    fdCacheSize = MIN(fdMaxCacheSize, vol_io_params.fd_initial_cachesize);

    if (vol_io_params.sync_behavior == IH_SYNC_DELAYED) {
#ifdef AFS_PTHREAD_ENV
	pthread_t syncer;
	pthread_attr_t tattr;

	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

	pthread_create(&syncer, &tattr, ih_sync_thread, NULL);
#else /* AFS_PTHREAD_ENV */
	PROCESS syncer;
	LWP_CreateProcess(ih_sync_thread, 16*1024, LWP_MAX_PRIORITY - 2,
	    NULL, "ih_syncer", &syncer);
#endif /* AFS_PTHREAD_ENV */
    }

}

/* Make the file descriptor cache as big as possible. Don't this call
 * if the program uses fopen or fdopen, if fd_max_cachesize cannot be
 * represented in the fileno member of the system FILE structure (or
 * equivalent).
 */
void
ih_UseLargeCache(void)
{
    IH_LOCK;

    if (!ih_PkgDefaultsSet) {
        ih_PkgDefaults();
    }

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

    osi_Assert(ihAvailHead == NULL);
    ihP = (IHandle_t *) malloc(I_HANDLE_MALLOCSIZE * sizeof(IHandle_t));
    osi_Assert(ihP != NULL);
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

    if (!ih_PkgDefaultsSet) {
        ih_PkgDefaults();
    }

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
    osi_Assert(ihP->ih_refcnt == 0);
    DLL_DELETE(ihP, ihAvailHead, ihAvailTail, ih_next, ih_prev);
    ihP->ih_dev = dev;
    ihP->ih_vid = vid;
    ihP->ih_ino = ino;
    ihP->ih_flags = 0;
    ihP->ih_synced = 0;
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
    osi_Assert(ih_Inited);
    osi_Assert(ihP->ih_refcnt > 0);
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

    osi_Assert(fdAvailHead == NULL);
    fdP = (FdHandle_t *) malloc(FD_HANDLE_MALLOCSIZE * sizeof(FdHandle_t));
    osi_Assert(fdP != NULL);
    for (i = 0; i < FD_HANDLE_MALLOCSIZE; i++) {
	fdP[i].fd_status = FD_HANDLE_AVAIL;
	fdP[i].fd_refcnt = 0;
	fdP[i].fd_ih = NULL;
	fdP[i].fd_fd = INVALID_FD;
        fdP[i].fd_ihnext = NULL;
        fdP[i].fd_ihprev = NULL;
	DLL_INSERT_TAIL(&fdP[i], fdAvailHead, fdAvailTail, fd_next, fd_prev);
    }
}

/* Allocate a chunk of stream handles */
void
streamHandleAllocateChunk(void)
{
    int i;
    StreamHandle_t *streamP;

    osi_Assert(streamAvailHead == NULL);
    streamP = (StreamHandle_t *)
	malloc(STREAM_HANDLE_MALLOCSIZE * sizeof(StreamHandle_t));
    osi_Assert(streamP != NULL);
    for (i = 0; i < STREAM_HANDLE_MALLOCSIZE; i++) {
	streamP[i].str_fd = INVALID_FD;
	DLL_INSERT_TAIL(&streamP[i], streamAvailHead, streamAvailTail,
			str_next, str_prev);
    }
}

/*
 * Get a file descriptor handle given an Inode handle
 * Takes the given file descriptor, and creates a new FdHandle_t for it,
 * attached to the given IHandle_t. fd can be INVALID_FD, indicating that the
 * caller failed to open the relevant file because we had too many FDs open;
 * ih_attachfd_r will then just evict/close an existing fd in the cache, and
 * return NULL.
 */
static FdHandle_t *
ih_attachfd_r(IHandle_t *ihP, FD_t fd)
{
    FD_t closeFd;
    FdHandle_t *fdP;

    /* fdCacheSize limits the size of the descriptor cache, but
     * we permit the number of open files to exceed fdCacheSize.
     * We only recycle open file descriptors when the number
     * of open files reaches the size of the cache */
    if ((fdInUseCount > fdCacheSize || fd == INVALID_FD)  && fdLruHead != NULL) {
	fdP = fdLruHead;
	osi_Assert(fdP->fd_status == FD_HANDLE_OPEN);
	DLL_DELETE(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);
	DLL_DELETE(fdP, fdP->fd_ih->ih_fdhead, fdP->fd_ih->ih_fdtail,
		   fd_ihnext, fd_ihprev);
	closeFd = fdP->fd_fd;
	if (fd == INVALID_FD) {
	    fdCacheSize--;          /* reduce in order to not run into here too often */
	    DLL_INSERT_TAIL(fdP, fdAvailHead, fdAvailTail, fd_next, fd_prev);
	    fdP->fd_status = FD_HANDLE_AVAIL;
	    fdP->fd_ih = NULL;
	    fdP->fd_fd = INVALID_FD;
	    IH_UNLOCK;
	    OS_CLOSE(closeFd);
	    IH_LOCK;
	    fdInUseCount -= 1;
	    return NULL;
	}
    } else {
	if (fdAvailHead == NULL) {
	    fdHandleAllocateChunk();
	}
	fdP = fdAvailHead;
	osi_Assert(fdP->fd_status == FD_HANDLE_AVAIL);
	DLL_DELETE(fdP, fdAvailHead, fdAvailTail, fd_next, fd_prev);
	closeFd = INVALID_FD;
    }

    fdP->fd_status = FD_HANDLE_INUSE;
    fdP->fd_fd = fd;
    fdP->fd_ih = ihP;
    fdP->fd_refcnt++;

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

    return fdP;
}

FdHandle_t *
ih_attachfd(IHandle_t *ihP, FD_t fd)
{
    FdHandle_t *fdP;

    IH_LOCK;

    fdInUseCount += 1;

    fdP = ih_attachfd_r(ihP, fd);
    if (!fdP) {
	fdInUseCount -= 1;
    }

    IH_UNLOCK;

    return fdP;
}

/*
 * Get a file descriptor handle given an Inode handle
 */
FdHandle_t *
ih_open(IHandle_t * ihP)
{
    FdHandle_t *fdP;
    FD_t fd;

    if (!ihP)			/* XXX should log here in the fileserver */
	return NULL;

    IH_LOCK;

    /* Do we already have an open file handle for this Inode? */
    for (fdP = ihP->ih_fdtail; fdP != NULL; fdP = fdP->fd_ihprev) {
	if (fdP->fd_status == FD_HANDLE_CLOSING) {
	    /* The handle was open when an IH_REALLYCLOSE was issued, so we
	     * cannot reuse it; it will be closed soon. */
	    continue;
	}
#ifndef HAVE_PIO
	/*
	 * If we don't have positional i/o, don't try to share fds, since
	 * we can't do so in a threadsafe way.
	 */
	if (fdP->fd_status == FD_HANDLE_INUSE) {
	    continue;
	}
	osi_Assert(fdP->fd_status == FD_HANDLE_OPEN);
#else /* HAVE_PIO */
	osi_Assert(fdP->fd_status != FD_HANDLE_AVAIL);
#endif /* HAVE_PIO */

	fdP->fd_refcnt++;
	if (fdP->fd_status == FD_HANDLE_OPEN) {
	    fdP->fd_status = FD_HANDLE_INUSE;
	    DLL_DELETE(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);
	}
	ihP->ih_refcnt++;
	IH_UNLOCK;
	return fdP;
    }

    /*
     * Try to open the Inode, return NULL on error.
     */
    fdInUseCount += 1;
    IH_UNLOCK;
ih_open_retry:
    fd = OS_IOPEN(ihP);
    IH_LOCK;
    if (fd == INVALID_FD && (errno != EMFILE || fdLruHead == NULL) ) {
	fdInUseCount -= 1;
	IH_UNLOCK;
	return NULL;
    }

    fdP = ih_attachfd_r(ihP, fd);
    if (!fdP) {
	osi_Assert(fd == INVALID_FD);
	IH_UNLOCK;
	goto ih_open_retry;
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
    osi_Assert(ih_Inited);
    osi_Assert(fdInUseCount > 0);
    osi_Assert(fdP->fd_status == FD_HANDLE_INUSE ||
               fdP->fd_status == FD_HANDLE_CLOSING);

    ihP = fdP->fd_ih;

    /* Call fd_reallyclose to really close the unused file handles if
     * the previous attempt to close (ih_reallyclose()) all file handles
     * failed (this is determined by checking the ihandle for the flag
     * IH_REALLY_CLOSED) or we have too many open files.
     */
    if (fdP->fd_status == FD_HANDLE_CLOSING ||
        ihP->ih_flags & IH_REALLY_CLOSED || fdInUseCount > fdCacheSize) {
	IH_UNLOCK;
	return fd_reallyclose(fdP);
    }

    fdP->fd_refcnt--;
    if (fdP->fd_refcnt == 0) {
	/* Put this descriptor back into the cache */
	fdP->fd_status = FD_HANDLE_OPEN;
	DLL_INSERT_TAIL(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);
    }

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
    osi_Assert(ih_Inited);
    osi_Assert(fdInUseCount > 0);
    osi_Assert(fdP->fd_status == FD_HANDLE_INUSE ||
               fdP->fd_status == FD_HANDLE_CLOSING);

    ihP = fdP->fd_ih;
    closeFd = fdP->fd_fd;
    fdP->fd_refcnt--;

    if (fdP->fd_refcnt == 0) {
	DLL_DELETE(fdP, ihP->ih_fdhead, ihP->ih_fdtail, fd_ihnext, fd_ihprev);
	DLL_INSERT_TAIL(fdP, fdAvailHead, fdAvailTail, fd_next, fd_prev);

	fdP->fd_status = FD_HANDLE_AVAIL;
	fdP->fd_refcnt = 0;
	fdP->fd_ih = NULL;
	fdP->fd_fd = INVALID_FD;
    }

    /* All the file descriptor handles have been closed; reset
     * the IH_REALLY_CLOSED flag indicating that ih_reallyclose
     * has completed its job.
     */
    if (!ihP->ih_fdhead) {
	ihP->ih_flags &= ~IH_REALLY_CLOSED;
    } else {
	FdHandle_t *lfdP, *next;
	int clear = 1;
	for (lfdP = ihP->ih_fdhead; lfdP != NULL; lfdP = next) {
	    next = lfdP->fd_ihnext;
	    osi_Assert(lfdP->fd_ih == ihP);
	    if (lfdP->fd_status != FD_HANDLE_CLOSING) {
		clear = 0;
		break;
	    }
	}
	/* no *future* fd should be subjected to this */
	if (clear)
	    ihP->ih_flags &= ~IH_REALLY_CLOSED;
    }

    if (fdP->fd_refcnt == 0) {
	IH_UNLOCK;
	OS_CLOSE(closeFd);
	IH_LOCK;
	fdInUseCount -= 1;
    }

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
    streamP->str_fdoff = 0;
    streamP->str_error = 0;
    streamP->str_eof = 0;
    streamP->str_direction = STREAM_DIRECTION_NONE;
    return streamP;
}

/* Open a file for buffered I/O */
StreamHandle_t *
stream_open(const char *filename, const char *mode)
{
    FD_t fd = INVALID_FD;

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
	osi_Assert(FALSE);		/* not implemented */
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
	osi_Assert(streamP->str_direction == STREAM_DIRECTION_READ);
    }

    bytesRead = 0;
    nbytes = size * nitems;
    p = (char *)ptr;
    while (nbytes > 0 && !streamP->str_eof) {
	if (streamP->str_buflen == 0) {
	    streamP->str_bufoff = 0;
	    streamP->str_buflen =
		OS_PREAD(streamP->str_fd, streamP->str_buffer,
			STREAM_HANDLE_BUFSIZE, streamP->str_fdoff);
	    if (streamP->str_buflen < 0) {
		streamP->str_error = errno;
		streamP->str_buflen = 0;
		bytesRead = 0;
		break;
	    } else if (streamP->str_buflen == 0) {
		streamP->str_eof = 1;
		break;
	    }
	    streamP->str_fdoff += streamP->str_buflen;
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
	osi_Assert(streamP->str_direction == STREAM_DIRECTION_WRITE);
    }

    nbytes = size * nitems;
    bytesWritten = 0;
    p = (char *)ptr;
    while (nbytes > 0) {
	if (streamP->str_buflen == 0) {
	    rc = OS_PWRITE(streamP->str_fd, streamP->str_buffer,
			  STREAM_HANDLE_BUFSIZE, streamP->str_fdoff);
	    if (rc < 0) {
		streamP->str_error = errno;
		bytesWritten = 0;
		break;
	    }
	    streamP->str_fdoff += rc;
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
stream_aseek(StreamHandle_t * streamP, afs_foff_t offset)
{
    ssize_t rc;
    int retval = 0;

    if (streamP->str_direction == STREAM_DIRECTION_WRITE
	&& streamP->str_bufoff > 0) {
	rc = OS_PWRITE(streamP->str_fd, streamP->str_buffer,
		      streamP->str_bufoff, streamP->str_fdoff);
	if (rc < 0) {
	    streamP->str_error = errno;
	    retval = -1;
	}
    }
    streamP->str_fdoff = offset;
    streamP->str_bufoff = 0;
    streamP->str_buflen = 0;
    streamP->str_eof = 0;
    streamP->str_direction = STREAM_DIRECTION_NONE;
    return retval;
}

/* fflush for buffered I/O handles */
int
stream_flush(StreamHandle_t * streamP)
{
    ssize_t rc;
    int retval = 0;

    if (streamP->str_direction == STREAM_DIRECTION_WRITE
	&& streamP->str_bufoff > 0) {
	rc = OS_PWRITE(streamP->str_fd, streamP->str_buffer,
		      streamP->str_bufoff, streamP->str_fdoff);
	if (rc < 0) {
	    streamP->str_error = errno;
	    retval = -1;
	} else {
	    streamP->str_fdoff += rc;
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
    ssize_t rc;
    int retval = 0;

    osi_Assert(streamP != NULL);
    if (streamP->str_direction == STREAM_DIRECTION_WRITE
	&& streamP->str_bufoff > 0) {
	rc = OS_PWRITE(streamP->str_fd, streamP->str_buffer,
		      streamP->str_bufoff, streamP->str_fdoff);
	if (rc < 0) {
	    retval = -1;
	} else {
	    streamP->str_fdoff += rc;
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

    osi_Assert(ihP->ih_refcnt > 0);

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
	osi_Assert(fdP->fd_ih == ihP);
	osi_Assert(fdP->fd_status == FD_HANDLE_OPEN
	       || fdP->fd_status == FD_HANDLE_INUSE
	       || fdP->fd_status == FD_HANDLE_CLOSING);
	if (fdP->fd_status == FD_HANDLE_OPEN) {
	    /* Note that FdHandle_t's do not count against the parent
	     * IHandle_t ref count when they are FD_HANDLE_OPEN. So, we don't
	     * need to dec the parent IHandle_t ref count for each one we pull
	     * off here. */
	    DLL_DELETE(fdP, ihP->ih_fdhead, ihP->ih_fdtail, fd_ihnext,
		       fd_ihprev);
	    DLL_DELETE(fdP, fdLruHead, fdLruTail, fd_next, fd_prev);
	    DLL_INSERT_TAIL(fdP, head, tail, fd_next, fd_prev);
	} else {
	    closedAll = 0;
	    fdP->fd_status = FD_HANDLE_CLOSING;
	    ihP->ih_flags |= IH_REALLY_CLOSED;
	}
    }

    /* If the ihandle reference count is 1, we should have
     * closed all file descriptors.
     */
    if (ihP->ih_refcnt == 1 || closedAll) {
	osi_Assert(closedAll);
	osi_Assert(!ihP->ih_fdhead);
	osi_Assert(!ihP->ih_fdtail);
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
	fdP->fd_refcnt = 0;
	fdP->fd_fd = INVALID_FD;
	fdP->fd_ih = NULL;
	closeCount++;
    }

    IH_LOCK;
    osi_Assert(fdInUseCount >= closeCount);
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
    ihP->ih_refcnt++;   /* must not disappear over unlock */
    if (ihP->ih_synced) {
	FdHandle_t *fdP;
	osi_Assert(vol_io_params.sync_behavior != IH_SYNC_ALWAYS);
	osi_Assert(vol_io_params.sync_behavior != IH_SYNC_NEVER);
        ihP->ih_synced = 0;
	IH_UNLOCK;

	fdP = IH_OPEN(ihP);
	if (fdP) {
	    OS_SYNC(fdP->fd_fd);
	    FDH_CLOSE(fdP);
	}

	IH_LOCK;
    }

    osi_Assert(ihP->ih_refcnt > 0);

    ih_fdclose(ihP);

    if (ihP->ih_refcnt > 1) {
	ihP->ih_refcnt--;
	IH_UNLOCK;
    } else {
	IH_UNLOCK;
	ih_release(ihP);
    }
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
    osi_Assert(ihP->ih_refcnt > 0);

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
ih_sync_all(void) {

    int ihash;

    IH_LOCK;
    for (ihash = 0; ihash < I_HANDLE_HASH_SIZE; ihash++) {
	IHandle_t *ihP, *ihPnext;

	ihP = ihashTable[ihash].ihash_head;
	if (ihP)
	    ihP->ih_refcnt++;	/* must not disappear over unlock */
	for (; ihP; ihP = ihPnext) {

	    if (ihP->ih_synced) {
		FD_t fd;

		ihP->ih_synced = 0;
		IH_UNLOCK;

		fd = OS_IOPEN(ihP);
		if (fd != INVALID_FD) {
		    OS_SYNC(fd);
		    OS_CLOSE(fd);
		}

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
ih_sync_thread(void *dummy) {
    while(1) {

#ifdef AFS_PTHREAD_ENV
	sleep(10);
#else /* AFS_PTHREAD_ENV */
	IOMGR_Sleep(60);
#endif /* AFS_PTHREAD_ENV */

        ih_sync_all();
    }
    return NULL;
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

#if defined(AFS_NT40_ENV) || !defined(AFS_NAMEI_ENV)
/* Unix namei implements its own more efficient IH_CREATE_INIT; this wrapper
 * is for everyone else */
IHandle_t *
ih_icreate_init(IHandle_t *lh, int dev, char *part, Inode nearInode,
                afs_uint32 p1, afs_uint32 p2, afs_uint32 p3, afs_uint32 p4)
{
    IHandle_t *ihP;
    Inode ino = IH_CREATE(lh, dev, part, nearInode, p1, p2, p3, p4);
    if (!VALID_INO(ino)) {
        return NULL;
    }
    IH_INIT(ihP, dev, p1, ino);
    return ihP;
}
#endif

afs_sfsize_t
ih_size(FD_t fd)
{
#ifdef AFS_NT40_ENV
    LARGE_INTEGER size;
    if (!GetFileSizeEx(fd, &size))
	return -1;
    return size.QuadPart;
#else
    struct afs_stat status;
    if (afs_fstat(fd, &status) < 0)
	return -1;
    return status.st_size;
#endif
}

#ifndef HAVE_PIO
ssize_t
ih_pread(int fd, void * buf, size_t count, afs_foff_t offset)
{
	afs_foff_t code;
	code = OS_SEEK(fd, offset, 0);
	if (code < 0)
	    return code;
	return OS_READ(fd, buf, count);
}

ssize_t
ih_pwrite(int fd, const void * buf, size_t count, afs_foff_t offset)
{
	afs_foff_t code;
	code = OS_SEEK(fd, offset, 0);
	if (code < 0)
	    return code;
	return OS_WRITE(fd, buf, count);
}
#endif /* !HAVE_PIO */

#ifndef AFS_NT40_ENV
int
ih_isunlinked(int fd)
{
    struct afs_stat status;
    if (afs_fstat(fd, &status) < 0) {
	return -1;
    }
    if (status.st_nlink < 1) {
	return 1;
    }
    return 0;
}
#endif /* !AFS_NT40_ENV */

int
ih_fdsync(FdHandle_t *fdP)
{
    switch (vol_io_params.sync_behavior) {
    case IH_SYNC_ALWAYS:
	return OS_SYNC(fdP->fd_fd);
    case IH_SYNC_DELAYED:
    case IH_SYNC_ONCLOSE:
	if (fdP->fd_ih) {
	    fdP->fd_ih->ih_synced = 1;
	    return 0;
	}
	return 1;
    case IH_SYNC_NEVER:
	return 0;
    default:
	osi_Assert(0);
    }
}
