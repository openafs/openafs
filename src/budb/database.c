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
    ("$Header: /cvs/openafs/src/budb/database.c,v 1.7.2.1 2007/07/06 11:34:00 jaltman Exp $");

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <sys/types.h>
#include <afs/stds.h>
#include <ubik.h>
#include <afs/auth.h>
#include <afs/bubasics.h>
#include "budb_errs.h"
#include "database.h"
#include "error_macros.h"
#include "afs/audit.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


int pollCount;
struct memoryDB db;		/* really allocate it here */

void
db_panic(reason)
     char *reason;
{
    LogError(0, "db_panic: %s\n", reason);
    BUDB_EXIT(-1);
}

afs_int32
InitDB()
{
    afs_int32 code;

    pollCount = 0;

    memset(&db, 0, sizeof(db));
    Lock_Init(&db.lock);
    if ((code = InitDBalloc()) || (code = InitDBhash()))
	return code;
    return 0;
}

/* package up seek and write into one procedure for ease of use */

/* dbwrite 
 * 	write a portion of the database
 * entry:
 *	pos - offset into the database (disk address). If this is in the
 *		database header, then buff must be a ptr to a portion of
 *		the in-core header
 *	buff - the information to write
 *	len - size of the write
 */

afs_int32
dbwrite(struct ubik_trans *ut, afs_int32 pos, void *buff, afs_int32 len)
{
    afs_int32 code = 0;

    if (((pos < sizeof(db.h)) && (buff != (char *)&db.h + pos))
	|| (pos >= ntohl(db.h.eofPtr))) {
	Log("dbwrite: Illegal attempt to write at location 0 or past EOF\n");
	ERROR(BUDB_IO);
    }

    code = ubik_Seek(ut, 0, pos);
    if (code) {
	LogError(code, "dbwrite: ubik_Seek to %d failed\n", pos);
	ERROR(code);
    }
    code = ubik_Write(ut, buff, len);
    if (code) {
	LogError(code, "dbwrite: ubik_Write failed\n");
	ERROR(code);
    }

  error_exit:
    if (((++pollCount) % 4) == 0) {	/* Poll every 4 reads/writes */
	IOMGR_Poll();
	pollCount = 0;
    }
    return code;
}

/* same thing for read */

afs_int32
dbread(struct ubik_trans *ut, afs_int32 pos, void *buff, afs_int32 len)
{
    afs_int32 code = 0;

    if (pos >= ntohl(db.h.eofPtr)) {
	LogError(0, "dbread: Attempt to read @%d (past EOF)\n", pos);
	ERROR(BUDB_IO);
    }

    code = ubik_Seek(ut, 0, pos);
    if (code) {
	LogError(code, "dbread: ubik_Seek to %d failed\n", pos);
	ERROR(code);
    }
    code = ubik_Read(ut, buff, len);
    if (code) {
	LogError(code, "dbread: ubik_Read pos %d, buff %d, len %d\n", pos,
		 buff, len);
	ERROR(code);
    }

  error_exit:
    if (((++pollCount) % 4) == 0) {	/* Poll every 4 reads/writes */
	IOMGR_Poll();
	pollCount = 0;
    }
    return code;
}

/* Same as dbread excepts it does checking */
afs_int32
cdbread(struct ubik_trans *ut, int type, afs_int32 pos, void *buff, afs_int32 len)
{
    afs_int32 code = 0;

    code = checkDiskAddress(pos, type, 0, 0);
    if (code) {
	LogError(code, "cdbread: Bad Address for block %d (addr 0x%x)\n",
		 type, pos);
	ERROR(code);
    }

    code = ubik_Seek(ut, 0, pos);
    if (code) {
	LogError(code, "cdbread: ubik_Seek to 0x%x failed\n", pos);
	ERROR(code);
    }
    code = ubik_Read(ut, buff, len);
    if (code) {
	LogError(code, "cdbread: ubik_Read pos 0x%x, buff %d, len %d\n", pos,
		 buff, len);
	ERROR(code);
    }

  error_exit:
    if (((++pollCount) % 4) == 0) {	/* Poll every 4 reads/writes */
	IOMGR_Poll();
	pollCount = 0;
    }
    return code;
}

/* check that the database has been initialized.  Be careful to fail in a safe
   manner, to avoid bogusly reinitializing the db.  */

afs_int32
CheckInit(ut, db_init)
     struct ubik_trans *ut;
     int (*db_init) ();		/* procedure to call if rebuilding DB */
{
    register afs_int32 code;

    /* Don't read header if not necessary */
    if (!ubik_CacheUpdate(ut))
	return 0;

    ObtainWriteLock(&db.lock);

    db.h.eofPtr = htonl(sizeof(db.h));	/* for sanity check in dbread */
    code = dbread(ut, 0, (char *)&db.h, sizeof(db.h));
    if (code)
	ERROR(code);

    if ((ntohl(db.h.version) != BUDB_VERSION)
	|| (ntohl(db.h.checkVersion) != BUDB_VERSION)) {

	if ((ntohl(db.h.version) == 0) || (ntohl(db.h.checkVersion) == 0))
	    ERROR(BUDB_EMPTY);

	LogError(0, "DB version should be %d; Initial = %d; Terminal = %d\n",
		 BUDB_VERSION, ntohl(db.h.version), ntohl(db.h.checkVersion));
	ERROR(BUDB_IO);
    }

    db.readTime = time(0);
    ht_Reset(&db.volName);
    ht_Reset(&db.tapeName);
    ht_Reset(&db.dumpName);
    ht_Reset(&db.dumpIden);

  error_exit:
    ReleaseWriteLock(&db.lock);
    if (code) {
	if ((code == UEOF) || (code == BUDB_EMPTY)) {
	    if (db_init) {
		LogDebug(0, "No data base - Building new one\n");

		/* try to write a good header */
		memset(&db.h, 0, sizeof(db.h));
		db.h.version = htonl(BUDB_VERSION);
		db.h.checkVersion = htonl(BUDB_VERSION);
		db.h.lastUpdate = db.h.lastDumpId = htonl(time(0));
		db.h.eofPtr = htonl(sizeof(db.h));

		/* text ptrs cleared by bzero */
		ht_DBInit();

		code = dbwrite(ut, 0, (char *)&db.h, sizeof(db.h));
		if (code)
		    code = BUDB_IO;	/* return the error code */
		else
		    code = db_init(ut);	/* initialize the db */
	    } else {
		LogDebug(0, "No data base\n");
		code = BUDB_EMPTY;
	    }
	} else {
	    LogDebug(0, "I/O Error\n");
	    code = BUDB_IO;
	}
    }
    return code;
}
