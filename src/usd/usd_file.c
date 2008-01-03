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

RCSID
    ("$Header$");

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#if defined(AFS_AIX_ENV)
#include <sys/tape.h>
#include <sys/statfs.h>
#else
#ifdef AFS_DARWIN_ENV
#include <sys/ioccom.h>
#endif
#if defined(AFS_DUX40_ENV) || defined(AFS_OBSD_ENV) || defined(AFS_NBSD_ENV)
#include <sys/ioctl.h>
#endif
#include <sys/mtio.h>
#endif /* AFS_AIX_ENV */

#include <string.h>
#include <stdlib.h>

#include <afs/debug.h>
#include "usd.h"

#ifdef O_LARGEFILE
typedef off64_t osi_lloff_t;
#define osi_llseek      lseek64
#else /* O_LARGEFILE */
#ifdef AFS_HAVE_LLSEEK
typedef offset_t osi_lloff_t;
#define osi_llseek      llseek
#else /* AFS_HAVE_LLSEEK */
typedef off_t osi_lloff_t;
#define osi_llseek      lseek
#endif /* AFS_HAVE_LLSEEK */
#endif /* O_LARGEFILE */

/*
 * This macro should be used inside assertions wherever offset/hyper
 * conversion occur.  A good compiler in a 64 bit environment will
 * elide the entire statement if the offset type is 64 bits wide.
 */
#define osi_hFitsInOff(ahyper, noff) \
    ((sizeof(noff) == 4) ? hfitsin32(ahyper) : 1)

#define osi_h2off(ahyper, noff)               \
    ((sizeof(noff) == 4)                      \
    ? ((noff) = (osi_lloff_t)hgetlo(ahyper))\
     : ((noff) = ((osi_lloff_t)hgethi(ahyper)<<32) | (osi_lloff_t)hgetlo(ahyper)))

#define osi_off2h(noff, ahyper)            \
     ((sizeof(noff) == 4)                   \
     ? (hset32(ahyper, (int)noff)) \
     : (hset64(ahyper, (int)((noff>>32)&0xffffffff), ((int)noff&0xffffffff))))


/************ End of osi wrappers ***********************************/

/* Implementation of user space device I/O for regular POSIX files. */

static int
usd_FileRead(usd_handle_t usd, char *buf, afs_uint32 nbytes,
	     afs_uint32 * xferdP)
{
    int fd = (int)(usd->handle);
    int got;

    got = read(fd, buf, nbytes);
    if (got == -1) {
	if (xferdP)
	    *xferdP = 0;
	return errno;
    }
    if (xferdP)
	*xferdP = got;
    return 0;
}

static int
usd_FileWrite(usd_handle_t usd, char *buf, afs_uint32 nbytes,
	      afs_uint32 * xferdP)
{
    int fd = (int)(usd->handle);
    int sent;

    sent = write(fd, buf, nbytes);
    if (sent == -1) {
	if (xferdP)
	    *xferdP = 0;
	return errno;
    }
    if (xferdP)
	*xferdP = sent;
    return 0;
}

extern osi_lloff_t osi_llseek(int, osi_lloff_t, int);

static int
usd_FileSeek(usd_handle_t usd, afs_hyper_t reqOff, int whence,
	     afs_hyper_t * curOffP)
{
    int fd = (int)(usd->handle);
    osi_lloff_t lloff;

    if (!osi_hFitsInOff(reqOff, lloff))
	return EINVAL;

    osi_h2off(reqOff, lloff);
    lloff = osi_llseek(fd, lloff, whence);
    if (lloff == (((osi_lloff_t) 0) - 1))
	return errno;
    if (curOffP)
	osi_off2h(lloff, *curOffP);

    return 0;
}

static int
usd_FileIoctl(usd_handle_t usd, int req, void *arg)
{
    int fd = (int)(usd->handle);
#ifdef O_LARGEFILE
    struct stat64 info;
#else /* O_LARGEFILE */
    struct stat info;
#endif /* O_LARGEFILE */
#ifdef AFS_AIX_ENV
    struct statfs fsinfo;	/* AIX stat structure doesn't have st_blksize */
#endif /* AFS_AIX_ENV */
    afs_hyper_t size;
    osi_lloff_t off;
    int code = 0;

    switch (req) {
    case USD_IOCTL_GETBLKSIZE:
#ifdef AFS_AIX_ENV
	code = fstatfs(fd, &fsinfo);
	if (code) {
	    *((long *)arg) = (long)4096;
	    return 0;
	}
	break;
#endif /* AFS_AIX_ENV */
    case USD_IOCTL_GETTYPE:
    case USD_IOCTL_GETDEV:
    case USD_IOCTL_GETSIZE:
#ifdef O_LARGEFILE
	code = fstat64(fd, &info);
#else /* O_LARGEFILE */
	code = fstat(fd, &info);
#endif /* O_LARGEFILE */
	if (code)
	    return errno;
	break;
    }

    switch (req) {
    case USD_IOCTL_GETTYPE:
	*(int *)arg = info.st_mode & S_IFMT;
	break;
    case USD_IOCTL_GETDEV:
	if (!(S_ISCHR(info.st_mode) || S_ISBLK(info.st_mode)))
	    return ENODEV;	/* not a device */
	*(dev_t *) arg = info.st_rdev;
	break;
    case USD_IOCTL_GETSIZE:
	if (S_ISCHR(info.st_mode) || S_ISBLK(info.st_mode))
	    return ENOTTY;	/* shouldn't be a device */
	osi_off2h(info.st_size, *(afs_hyper_t *) arg);
	break;
    case USD_IOCTL_GETFULLNAME:
	*(char **)arg = usd->fullPathName;
	break;

    case USD_IOCTL_SETSIZE:

	/* We could just use ftruncate in all cases.  (This even works on AIX;
	 * I tried it). -blake 931118 */

	/* However, I'm pretty sure this doesn't work on Ultrix so I am
	 * unsure about OSF/1 and HP/UX. 931118 */

	size = *(afs_hyper_t *) arg;
	if (!osi_hFitsInOff(size, off))
	    return EFBIG;
	osi_h2off(size, off);
#ifdef O_LARGEFILE
	code = ftruncate64(fd, off);
#else /* O_LARGEFILE */
	code = ftruncate(fd, off);
#endif /* O_LARGEFILE */
	if (code == -1)
	    code = errno;
	return code;

    case USD_IOCTL_TAPEOPERATION:
	{
	    usd_tapeop_t *tapeOpp = (usd_tapeop_t *) arg;
#if defined(AFS_AIX_ENV)
	    struct stop os_tapeop;

	    if (tapeOpp->tp_op == USDTAPE_WEOF) {
		os_tapeop.st_op = STWEOF;
	    } else if (tapeOpp->tp_op == USDTAPE_REW) {
		os_tapeop.st_op = STREW;
	    } else if (tapeOpp->tp_op == USDTAPE_FSF) {
		os_tapeop.st_op = STFSF;
	    } else if (tapeOpp->tp_op == USDTAPE_BSF) {
		os_tapeop.st_op = STRSF;
	    } else if (tapeOpp->tp_op == USDTAPE_PREPARE) {
		return 0;
	    } else if (tapeOpp->tp_op == USDTAPE_SHUTDOWN) {
		return 0;
	    } else {
		/* unsupported tape operation */
		return EINVAL;
	    }
	    os_tapeop.st_count = tapeOpp->tp_count;

	    code = ioctl(fd, STIOCTOP, &os_tapeop);
#else
	    struct mtop os_tapeop;

	    if (tapeOpp->tp_op == USDTAPE_WEOF) {
		os_tapeop.mt_op = MTWEOF;
	    } else if (tapeOpp->tp_op == USDTAPE_REW) {
		os_tapeop.mt_op = MTREW;
	    } else if (tapeOpp->tp_op == USDTAPE_FSF) {
		os_tapeop.mt_op = MTFSF;
	    } else if (tapeOpp->tp_op == USDTAPE_BSF) {
		os_tapeop.mt_op = MTBSF;
	    } else if (tapeOpp->tp_op == USDTAPE_PREPARE) {
		return 0;
	    } else if (tapeOpp->tp_op == USDTAPE_SHUTDOWN) {
		return 0;
	    } else {
		/* unsupported tape operation */
		return EINVAL;
	    }
	    os_tapeop.mt_count = tapeOpp->tp_count;

	    code = ioctl(fd, MTIOCTOP, &os_tapeop);
#endif /* AFS_AIX_ENV */

	    if (code == -1) {
		code = errno;
	    } else {
		code = 0;
	    }
	    return code;
	}

    case USD_IOCTL_GETBLKSIZE:
	if (S_ISCHR(info.st_mode) || S_ISBLK(info.st_mode)) {
	    *((long *)arg) = (long)4096;
	    return 0;
	}
#ifdef AFS_AIX_ENV
	*((long *)arg) = (long)fsinfo.f_bsize;
#else /* AFS_AIX_ENV */
	*((long *)arg) = (long)info.st_blksize;
#endif /* AFS_AIX_ENV */
	break;

    default:
	return EINVAL;
    }
    return code;
}

static int
usd_FileClose(usd_handle_t usd)
{
    int fd = (int)(usd->handle);
    int code = 0;
    int ccode;

    /* I can't really believe this is necessary.  On the one hand the user
     * space code always uses character devices, which aren't supposed to do
     * any buffering.  On the other, I very much doubt fsyncing a regular file
     * being salvaged is ever necessary.  But the salvager used to do this
     * before returning, so... */

    if (usd->openFlags & (O_WRONLY | O_RDWR)) {
	int mode;
	code = usd_FileIoctl(usd, USD_IOCTL_GETTYPE, &mode);
	if (code == 0 && S_ISBLK(mode)) {
	    if (fsync(fd) < 0)
		code = errno;
	}
    }

    ccode = close(fd);
    if (!code && ccode)
	code = errno;

    if (usd->fullPathName)
	free(usd->fullPathName);
    free(usd);

    return code;
}

static int
usd_FileOpen(const char *path, int flags, int mode, usd_handle_t * usdP)
{
    int fd;
    int oflags;
    usd_handle_t usd;
    int code;

    if (usdP)
	*usdP = NULL;

    oflags = (flags & USD_OPEN_RDWR) ? O_RDWR : O_RDONLY;

#ifdef O_SYNC			/* AFS_DARWIN_ENV XXX */
    if (flags & USD_OPEN_SYNC)
	oflags |= O_SYNC;
#endif

    if (flags & USD_OPEN_CREATE)
	oflags |= O_CREAT;

#ifdef O_LARGEFILE
    fd = open64(path, oflags | O_LARGEFILE, mode);
#else /* O_LARGEFILE */
    fd = open(path, oflags, mode);
#endif /* O_LARGEFILE */
    if (fd == -1)
	return errno;

    usd = (usd_handle_t) malloc(sizeof(*usd));
    memset(usd, 0, sizeof(*usd));
    usd->handle = (void *)fd;
    usd->read = usd_FileRead;
    usd->write = usd_FileWrite;
    usd->seek = usd_FileSeek;
    usd->ioctl = usd_FileIoctl;
    usd->close = usd_FileClose;
    usd->fullPathName = (char *)malloc(strlen(path) + 1);
    strcpy(usd->fullPathName, path);
    usd->openFlags = flags;

    code = 0;
    if (flags & (USD_OPEN_RLOCK | USD_OPEN_WLOCK)) {
#ifdef O_LARGEFILE
	struct flock64 fl;
#else /* O_LARGEFILE */
	struct flock fl;
#endif /* O_LARGEFILE */

	/* make sure both lock bits aren't set */
	assert(~flags & (USD_OPEN_RLOCK | USD_OPEN_WLOCK));

	fl.l_type = ((flags & USD_OPEN_RLOCK) ? F_RDLCK : F_WRLCK);
	fl.l_whence = SEEK_SET;
	fl.l_start = (osi_lloff_t) 0;
	fl.l_len = (osi_lloff_t) 0;	/* whole file */
#ifdef O_LARGEFILE
	code = fcntl(fd, F_SETLK64, &fl);
#else /* O_LARGEFILE */
	code = fcntl(fd, F_SETLK, &fl);
#endif /* O_LARGEFILE */
	if (code == -1)
	    code = errno;

	/* If we're trying to obtain a write lock on a real disk, then the
	 * aggregate must not be attached by the kernel.  If so, unlock it
	 * and fail. 
	 * WARNING: The code to check for the above has been removed when this
	 * file was ported from DFS src. It should be put back if
	 * this library is used to access hard disks 
	 */
    }

    if (code == 0 && usdP)
	*usdP = usd;
    else
	usd_FileClose(usd);
    return code;
}

static int
usd_FileDummyClose(usd_handle_t usd)
{
    free(usd);
    return 0;
}

int
usd_Open(const char *path, int oflag, int mode, usd_handle_t * usdP)
{
    return usd_FileOpen(path, oflag, mode, usdP);
}

static int
usd_FileStandardInput(usd_handle_t * usdP)
{
    usd_handle_t usd;

    if (usdP)
	*usdP = NULL;

    usd = (usd_handle_t) malloc(sizeof(*usd));
    memset(usd, 0, sizeof(*usd));
    usd->handle = (void *)((unsigned long)0);
    usd->read = usd_FileRead;
    usd->write = usd_FileWrite;
    usd->seek = usd_FileSeek;
    usd->ioctl = usd_FileIoctl;
    usd->close = usd_FileDummyClose;
    usd->fullPathName = "STDIN";
    usd->openFlags = 0;
    *usdP = usd;

    return 0;
}

int
usd_StandardInput(usd_handle_t * usdP)
{
    return usd_FileStandardInput(usdP);
}

static int
usd_FileStandardOutput(usd_handle_t * usdP)
{
    usd_handle_t usd;

    if (usdP)
	*usdP = NULL;

    usd = (usd_handle_t) malloc(sizeof(*usd));
    memset(usd, 0, sizeof(*usd));
    usd->handle = (void *)((unsigned long)1);
    usd->read = usd_FileRead;
    usd->write = usd_FileWrite;
    usd->seek = usd_FileSeek;
    usd->ioctl = usd_FileIoctl;
    usd->close = usd_FileDummyClose;
    usd->fullPathName = "STDOUT";
    usd->openFlags = 0;
    *usdP = usd;

    return 0;
}

int
usd_StandardOutput(usd_handle_t * usdP)
{
    return usd_FileStandardOutput(usdP);
}
