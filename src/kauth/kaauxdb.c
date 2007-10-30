/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * ALL RIGHTS RESERVED
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef AFS_NT40_ENV
#include <io.h>
#else
#include <sys/file.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <string.h>
#include <rx/rxkad.h>
#include "ubik_int.h"
#include "kauth.h"
#include "kaserver.h"


static int fd = 0;

/* Open the auxiliary database file containing failed authentication
 * counters, and the times at which the last failures occurred.
 * Nothing fancy.
 */
int
kaux_opendb(char *path)
{
    char dbpathname[1024];
    static char dbname[] = "auxdb";

    if (strlen(path) < 1024 - strlen(dbname)) {	/* bullet-proofing */

	strcpy(dbpathname, path);
	strcat(dbpathname, dbname);

	fd = open(dbpathname, O_CREAT | O_RDWR, 0600);
	if (fd < 0)
	    perror(dbpathname);
    }

    return fd;
}

/* close that auxiliary database.  Unneccessary, but here for symmetry.
 */
void
kaux_closedb(void)
{

    if (fd > 0)
	close(fd);
    return;
}


/* 
 * The read and write routines take as a parameter, the offset into
 * the main database at which a particular user's entry resides.  They
 * then convert that into an offset into the auxiliary database.  This
 * makes the main code a little simpler, though it obscures a small
 * detail.
 */
int
kaux_read(afs_int32 to,		/* this is the offset of the user id in the main database. 
				 * we do the conversion here - probably a bad idea. */
	  unsigned int *nfailures, afs_uint32 * lasttime)
{
    unsigned int offset;

    *nfailures = *lasttime = 0;

    if (fd <= 0 || !to)
	return 0;

    offset =
	((to - sizeof(struct kaheader)) / ENTRYSIZE) * (sizeof(int) +
							sizeof(afs_int32));
    /* can't get there from here */
    if (offset > lseek(fd, offset, SEEK_SET))
	return 0;

    /* we should just end up with 0 for nfailures and lasttime if EOF is 
     * encountered here, I hope */
    if ((0 > read(fd, nfailures, sizeof(int)))
	|| (0 > read(fd, lasttime, sizeof(afs_int32)))) {
	*nfailures = *lasttime = 0;
	perror("kaux_read()");
    }

    return 0;
}

int
kaux_write(afs_int32 to, unsigned int nfailures, afs_uint32 lasttime)
{
    unsigned int offset;

    if (fd <= 0 || !to)
	return 0;

    offset =
	((to - sizeof(struct kaheader)) / ENTRYSIZE) * (sizeof(int) +
							sizeof(afs_int32));
    /* can't get there from here */
    if (offset > lseek(fd, offset, SEEK_SET))
	return 0;

    if ((write(fd, &nfailures, sizeof(int)) != sizeof(int))
	|| (write(fd, &lasttime, sizeof(afs_int32)) != sizeof(afs_int32)))
	perror("kaux_write()");
    return 0;
}


/* adjust this user's records to reflect a failure.
 * locktime is the value stored in the main database that specifies
 * how long a user's ID should be locked once the attempts limit has
 * been exceeded.  It also functions as the interval during which the
 * permitted N-1 authentication failures plus the forbidden Nth
 * failure must occur, in order for the ID to actually be locked.  Ie,
 * all failures which occurred more than _locktime_ seconds ago are
 * forgiven.
 * locktime == 0 signifies that the ID should be locked indefinitely
 */
void
kaux_inc(afs_int32 to, afs_uint32 locktime)
{
    int nfailures;
    afs_uint32 lasttime, now;

    now = time(0);

    kaux_read(to, &nfailures, &lasttime);

    if (locktime && lasttime + locktime < now)
	nfailures = 1;
    else
	nfailures++;

    kaux_write(to, nfailures, now);

}

/* 
 * report on whether a particular id is locked or not...
 * has to get some dirt from ubik.
 * We multiply the actual number of permitted attempts by two because
 * klog tries to authenticate twice when the password is bogus: once
 * with the ka_string_to_key, and once with des_string_to_key, for
 * Kerberos compatibility.  It's easier to frob here than to explain
 * to users/admins.
 * RETURNS: time when the ID will be unlocked, or 0 if it's not locked. 
 */
int
kaux_islocked(afs_int32 to, u_int attempts, u_int locktime)
{
    extern int ubeacon_Debug(), ubeacon_AmSyncSite();
    unsigned int nfailures, myshare;
    afs_uint32 lasttime;
    struct ubik_debug beaconinfo;

    /* if attempts is 0, that means there's no limit, so the id
     * can't ever be locked...
     */
    if (!attempts)
	return 0;

    kaux_read(to, &nfailures, &lasttime);

    ubeacon_Debug(&beaconinfo);
    attempts = attempts * 2;

    myshare = attempts / beaconinfo.nServers;
    if (ubeacon_AmSyncSite())
	myshare += attempts % beaconinfo.nServers;

    if (!myshare) {
	return -1;
    } else if ((nfailures < myshare)
	       || (locktime && lasttime + locktime < time(0))) {
	return 0;
    } else if (locktime == 0) {	/* infinite */
	return -1;
    } else {
	return (lasttime + locktime);
    }
}
