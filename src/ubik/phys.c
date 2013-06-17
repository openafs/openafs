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
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif
#include <sys/stat.h>

/* #ifdef AFS_PTHREAD_ENV */
#if 0   /* temporary hack - klm */
/* nothing */
#else
#include <lwp.h>
#endif

#include <lock.h>
#include <afs/afsutil.h>

#define	UBIK_INTERNALS 1
#include "ubik.h"

/*! \file
 * These routines are called via the proc ptr in the ubik_dbase structure.  They provide access to
 * the physical disk, by converting the file numbers being processed ( >= 0 for user data space, < 0
 * for ubik system files, such as the log) to actual pathnames to open, read, write, truncate, sync,
 * etc.
 */

#define	MAXFDCACHE  4
static struct fdcache {
    int fd;
    int fileID;
    int refCount;
} fdcache[MAXFDCACHE];

static char pbuffer[1024];

/*!
 * \warning Beware, when using this function, of the header in front of most files.
 */
static int
uphys_open(struct ubik_dbase *adbase, afs_int32 afid)
{
    int fd;
    static int initd;
    int i;
    struct fdcache *tfd;
    struct fdcache *bestfd;

    /* initialize package */
    if (!initd) {
	initd = 1;
	tfd = fdcache;
	for (i = 0; i < MAXFDCACHE; tfd++, i++) {
	    tfd->fd = -1;	/* invalid value */
	    tfd->fileID = -10000;	/* invalid value */
	    tfd->refCount = 0;
	}
    }

    /* scan file descr cache */
    for (tfd = fdcache, i = 0; i < MAXFDCACHE; i++, tfd++) {
	if (afid == tfd->fileID && tfd->refCount == 0) {	/* don't use open fd */
	    lseek(tfd->fd, 0, 0);	/* reset ptr just like open would have */
	    tfd->refCount++;
	    return tfd->fd;
	}
    }

    /* not found, open it and try to enter in cache */
    afs_snprintf(pbuffer, sizeof(pbuffer), "%s.DB%s%d", adbase->pathName,
		 (afid<0)?"SYS":"", (afid<0)?-afid:afid);
    fd = open(pbuffer, O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
	/* try opening read-only */
	fd = open(pbuffer, O_RDONLY, 0);
	if (fd < 0)
	    return fd;
    }

    /* enter it in the cache */
    tfd = fdcache;
    bestfd = NULL;
    for (i = 0; i < MAXFDCACHE; i++, tfd++) {	/* look for empty slot */
	if (tfd->fd == -1) {
	    bestfd = tfd;
	    break;
	}
    }
    if (!bestfd) {		/* look for reclaimable slot */
	tfd = fdcache;
	for (i = 0; i < MAXFDCACHE; i++, tfd++) {
	    if (tfd->refCount == 0) {
		bestfd = tfd;
		break;
	    }
	}
    }
    if (bestfd) {		/* found a usable slot */
	tfd = bestfd;
	if (tfd->fd >= 0)
	    close(tfd->fd);
	tfd->fd = fd;
	tfd->refCount = 1;	/* us */
	tfd->fileID = afid;
    }

    /* finally, we're done */
    return fd;
}

/*!
 * \brief Close the file, maintaining ref count in cache structure.
 */
int
uphys_close(int afd)
{
    int i;
    struct fdcache *tfd;

    if (afd < 0)
	return EBADF;
    tfd = fdcache;
    for (i = 0; i < MAXFDCACHE; i++, tfd++) {
	if (tfd->fd == afd) {
	    if (tfd->fileID != -10000) {
		tfd->refCount--;
		return 0;
	    } else {
		if (tfd->refCount > 0) {
		    tfd->refCount--;
		    if (tfd->refCount == 0) {
			close(tfd->fd);
			tfd->fd = -1;
		    }
		    return 0;
		}
		tfd->fd = -1;
		break;
	    }
	}
    }
    return close(afd);
}

int
uphys_stat(struct ubik_dbase *adbase, afs_int32 afid, struct ubik_stat *astat)
{
    int fd;
    struct stat tstat;
    afs_int32 code;

    fd = uphys_open(adbase, afid);
    if (fd < 0)
	return fd;
    code = fstat(fd, &tstat);
    uphys_close(fd);
    if (code < 0) {
	return code;
    }
    astat->mtime = tstat.st_mtime;
    code = tstat.st_size - HDRSIZE;
    if (code < 0)
	astat->size = 0;
    else
	astat->size = code;
    return 0;
}

int
uphys_read(struct ubik_dbase *adbase, afs_int32 afile,
	   void *abuffer, afs_int32 apos, afs_int32 alength)
{
    int fd;
    afs_int32 code;

    fd = uphys_open(adbase, afile);
    if (fd < 0)
	return -1;
    code = lseek(fd, apos + HDRSIZE, 0);
    if (code < 0) {
	uphys_close(fd);
	return -1;
    }
    code = read(fd, abuffer, alength);
    uphys_close(fd);
    return code;
}

int
uphys_write(struct ubik_dbase *adbase, afs_int32 afile,
	    void *abuffer, afs_int32 apos, afs_int32 alength)
{
    int fd;
    afs_int32 code;
    afs_int32 length;

    fd = uphys_open(adbase, afile);
    if (fd < 0)
	return -1;
    code = lseek(fd, apos + HDRSIZE, 0);
    if (code < 0) {
	uphys_close(fd);
	return -1;
    }
    length = write(fd, abuffer, alength);
    code = uphys_close(fd);
    if (code)
	return -1;
    else
	return length;
}

int
uphys_truncate(struct ubik_dbase *adbase, afs_int32 afile,
	       afs_int32 asize)
{
    afs_int32 code, fd;
    fd = uphys_open(adbase, afile);
    if (fd < 0)
	return UNOENT;
    code = ftruncate(fd, asize + HDRSIZE);
    uphys_close(fd);
    return code;
}

/*!
 * \brief Get number of dbase files.
 *
 * \todo Really should scan dir for data.
 */
int
uphys_getnfiles(struct ubik_dbase *adbase)
{
    /* really should scan dir for data */
    return 1;
}

/*!
 * \brief Get database label, with \p aversion in host order.
 */
int
uphys_getlabel(struct ubik_dbase *adbase, afs_int32 afile,
	       struct ubik_version *aversion)
{
    struct ubik_hdr thdr;
    afs_int32 code, fd;

    fd = uphys_open(adbase, afile);
    if (fd < 0)
	return UNOENT;
    code = read(fd, &thdr, sizeof(thdr));
    if (code != sizeof(thdr)) {
	uphys_close(fd);
	return EIO;
    }
    aversion->epoch = ntohl(thdr.version.epoch);
    aversion->counter = ntohl(thdr.version.counter);
    uphys_close(fd);
    return 0;
}

/*!
 * \brief Label database, with \p aversion in host order.
 */
int
uphys_setlabel(struct ubik_dbase *adbase, afs_int32 afile,
	       struct ubik_version *aversion)
{
    struct ubik_hdr thdr;
    afs_int32 code, fd;

    fd = uphys_open(adbase, afile);
    if (fd < 0)
	return UNOENT;

    memset(&thdr, 0, sizeof(thdr));

    thdr.version.epoch = htonl(aversion->epoch);
    thdr.version.counter = htonl(aversion->counter);
    thdr.magic = htonl(UBIK_MAGIC);
    thdr.size = htons(HDRSIZE);
    code = write(fd, &thdr, sizeof(thdr));
    fsync(fd);			/* preserve over crash */
    uphys_close(fd);
    if (code != sizeof(thdr)) {
	return EIO;
    }
    return 0;
}

int
uphys_sync(struct ubik_dbase *adbase, afs_int32 afile)
{
    afs_int32 code, fd;
    fd = uphys_open(adbase, afile);
    code = fsync(fd);
    uphys_close(fd);
    return code;
}

void
uphys_invalidate(struct ubik_dbase *adbase, afs_int32 afid)
{
    int i;
    struct fdcache *tfd;

    /* scan file descr cache */
    for (tfd = fdcache, i = 0; i < MAXFDCACHE; i++, tfd++) {
	if (afid == tfd->fileID) {
	    tfd->fileID = -10000;
	    if (tfd->fd >= 0 && tfd->refCount == 0) {
		close(tfd->fd);
		tfd->fd = -1;
	    }
	    return;
	}
    }
}
