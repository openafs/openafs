/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_USD_H
#define OPENAFS_USD_H

/* Define I/O functions that operate on both device and regular files.
 *
 * Essentially, this is a mechanism for dealing with systems (such as NT)
 * that do not allow devices to be accessed via POSIX-equivalent
 * open()/read()/write()/close() functions.
 */

/* The Unix common code is implemented by usd/usd_file.c and the WinNT
 * code is in usd/usd_nt.c. */

/* The device handle "usd_handle_t" is returned by the function usd_Open.
 * This object includes function pointers for all the other operations,
 * which are accessed by macros, e.g. USD_READ(usd_handle_t, ...).  In
 * general, this handle is entirely opaque to outside callers.
 *
 * Because of WinNT restrictions on dealing with devices, it is not possible
 * to lock devices independently from opening them.  A flag bit is specified
 * at open time requesting a read (shared) or write (exclusive) lock or
 * neither.  The device remains locked until the handle is closed.
 *
 * An application can not open a device multiple times.  This is because on
 * WinNT the holder of the lock is the handle not the process as on Unix.
 *
 * All the "usd_" function return an error status as an "(int)" converted
 * into the errno domain, with zero meaning no error.
 *
 * As a consequence of this method of reporting errors, output values are
 * returned in reference parameters instead of being encoded in the return
 * value.  Also, offsets and lengths use the afs_hyper_t type. */

/* NOTE -- this module is preempt safe.  It assume it is being called from a
 *     preemptive environment.  Treat all calls as if they had an "_r"
 *     suffix. */

typedef struct usd_handle *usd_handle_t;

struct usd_handle {
    int (*read) (usd_handle_t usd, char *buf, afs_uint32 nbyte,
		 afs_uint32 * xferdP);
    int (*write) (usd_handle_t usd, char *buf, afs_uint32 nbyte,
		  afs_uint32 * xferdP);
    int (*seek) (usd_handle_t usd, afs_hyper_t inOff, int whence,
		 afs_hyper_t * outOffP);
    int (*ioctl) (usd_handle_t usd, int req, void *arg);
    int (*close) (usd_handle_t usd);

    /* private members */
    void *handle;
    char *fullPathName;
    int openFlags;
    void *privateData;
};

#define USD_READ(usd, buf, nbyte, xferP) \
    ((*(usd)->read)(usd, buf, nbyte, xferP))
#define USD_WRITE(usd, buf, nbyte, xferP) \
    ((*(usd)->write)(usd, buf, nbyte, xferP))
#define USD_SEEK(usd, inOff, w, outOff) ((*(usd)->seek)(usd, inOff, w, outOff))

/* USD_IOCTL -- performs various query and control operations.
 *
 * PARAMETERS --
 *     req -- is one of the constants, defined below, starting with
 *         USD_IOCTL_... which specify the desired operation.
 *     arg -- is a (void *) pointer whose purpose depends on "req". */

#define USD_IOCTL(usd, req, arg) ((*(usd)->ioctl)(usd, req, arg))

/* USD_CLOSE -- closes and deallocates the specified device handle.
 *
 * CAUTIONS -- The handle is deallocated *even if an error occurs*. */

#define USD_CLOSE(usd) ((*(usd)->close)(usd))

extern int usd_Open(const char *path, int oflag, int mode,
		    usd_handle_t * usdP);
extern int usd_StandardInput(usd_handle_t * usdP);
extern int usd_StandardOutput(usd_handle_t * usdP);

/* Open flag bits */

#define USD_OPEN_RDONLY		0	/* read only */
#define USD_OPEN_RDWR		1	/* writable */
#define USD_OPEN_SYNC		2	/* do I/O synchronously to disk */
#define USD_OPEN_RLOCK		4	/* obtain a read lock on device */
#define USD_OPEN_WLOCK		8	/* obtain a write lock on device */
#define USD_OPEN_CREATE	     0x10	/* create file if doesn't exist */

/* I/O Control requests */

/* GetType(int *arg) -- returns an integer like the st_mode field masked with
 *     S_IFMT.  It can be decoded using the S_ISxxx macros. */

#define USD_IOCTL_GETTYPE	1

/* GetFullName(char *arg) -- returns a pointer to the fully qualified pathname
 *     to the opened device.  This string is stored with the open handle and
 *     can be used as long as the handle is open.  If the string is required
 *     longer, the string must be copied before the handle is closed. */

#define USD_IOCTL_GETFULLNAME	2

/* GetDev(dev_t *arg) -- returns a dev_t representing the device number of open
 *     device.  If the handle does not represent a device the return value is
 *     ENODEV. */

#define USD_IOCTL_GETDEV	3

/* GetSize(afs_hyper_t *sizeP) -- returns the size of the file.  Doesn't work
 *     on BLK or CHR devices. */

#define USD_IOCTL_GETSIZE	6

/* SetSize(afs_hyper_t *sizeP) -- sets the size of the file.  Doesn't work
 *     on BLK or CHR devices. */

#define USD_IOCTL_SETSIZE	7

/* TapeOperation(usd_tapeop_t *tapeOpp) -- perform tape operation specified
 *     in tapeOpp->tp_op.
 */

#define USD_IOCTL_TAPEOPERATION 8

/* GetBlkSize(long *sizeP) -- returns blocksize used by filesystem for a file.
 *     Doesn't work on BLK or CHR devices. */

#define USD_IOCTL_GETBLKSIZE	9

typedef struct {
    int tp_op;			/* tape operation */
    int tp_count;		/* tape operation count argument */
} usd_tapeop_t;

#define USDTAPE_WEOF     0	/* write specified number of tape marks */
#define USDTAPE_REW      1	/* rewind tape */
#define USDTAPE_FSF      2	/* forward-space specified number of tape marks */
#define USDTAPE_BSF      3	/* back-space specified number of tape marks */
#define USDTAPE_PREPARE  4	/* ready tape drive for operation */
#define USDTAPE_SHUTDOWN 5	/* decommission tape drive after operation */

#endif /* OPENAFS_USD_H */
