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

#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <afsutil.h>
#else
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#endif
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <lock.h>
#include "ubik.h"
#include "utst_int.h"


/* useful globals */
struct ubik_dbase *dbase;
afs_int32 sleepTime;

SAMPLE_Inc(rxconn)
     struct rx_connection *rxconn;
{
    afs_int32 code, temp;
    struct ubik_trans *tt;
    struct timeval tv;

    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    printf("about to set lock\n");
    /* now set database locks.  Must do this or people may read uncommitted
     * data.  Note that we're just setting a lock at position 1, which is
     * this program's convention for locking the whole database */
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    printf("now have lock\n");
    if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    /* sleep for a little while to make it possible for us to test for some
     * race conditions */
    if (sleepTime) {
	tv.tv_sec = sleepTime;
	tv.tv_usec = 0;
	IOMGR_Select(0, 0, 0, 0, &tv);
    }
    /* read the original value */
    code = ubik_Read(tt, &temp, sizeof(afs_int32));
    if (code == UEOF) {
	/* short read */
	temp = 0;
    } else if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    temp++;			/* bump the value here */
    /* reset the file pointer back to where it was before the read */
    code = ubik_Seek(tt, 0, 0);
    if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    /* write the data back */
    code = ubik_Write(tt, &temp, sizeof(afs_int32));
    if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    /* finally, we commit the transaction */
    code = ubik_EndTrans(tt);
    temp = 0;
    return code;
}


SAMPLE_Get(rxconn, gnumber)
     struct rx_connection *rxconn;
     afs_int32 *gnumber;
{
    afs_int32 code, temp;
    struct ubik_trans *tt;
    struct timeval tv;

    /* start with a read transaction, since we're only going to do read
     * operations in this transaction. */
    code = ubik_BeginTrans(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    printf("about to set lock\n");
    /* obtain a read lock, so we don't read data the other guy is writing */
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    printf("now have lock\n");
    if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    /* sleep to allow races */
    if (sleepTime) {
	tv.tv_sec = sleepTime;
	tv.tv_usec = 0;
	IOMGR_Select(0, 0, 0, 0, &tv);
    }
    /* read the value */
    code = ubik_Read(tt, &temp, sizeof(afs_int32));
    if (code == UEOF) {
	/* premature eof, use 0 */
	temp = 0;
    } else if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    *gnumber = temp;
    /* end the transaction, automatically releasing locks */
    code = ubik_EndTrans(tt);
    return code;
}


SAMPLE_QGet(rxconn, gnumber)
     struct rx_connection *rxconn;
     afs_int32 *gnumber;
{
    afs_int32 code, temp;
    struct ubik_trans *tt;
    struct timeval tv;

    /* start with a read transaction, since we're only going to do read
     * operations in this transaction. */
    code = ubik_BeginTransReadAny(dbase, UBIK_READTRANS, &tt);
    if (code)
	return code;
    printf("about to set lock\n");
    /* obtain a read lock, so we don't read data the other guy is writing */
    code = ubik_SetLock(tt, 1, 1, LOCKREAD);
    printf("now have lock\n");
    if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    /* sleep to allow races */
    if (sleepTime) {
	tv.tv_sec = sleepTime;
	tv.tv_usec = 0;
	IOMGR_Select(0, 0, 0, 0, &tv);
    }
    /* read the value */
    code = ubik_Read(tt, &temp, sizeof(afs_int32));
    if (code == UEOF) {
	/* premature eof, use 0 */
	temp = 0;
    } else if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    *gnumber = temp;
    /* end the transaction, automatically releasing locks */
    code = ubik_EndTrans(tt);
    return code;
}


SAMPLE_Trun(rxconn)
     struct rx_connection *rxconn;
{
    afs_int32 code;
    struct ubik_trans *tt;
    struct timeval tv;

    /* truncation operation requires a write transaction, too */
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    printf("about to set lock\n");
    /* lock the database */
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    printf("now have lock\n");
    if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    if (sleepTime) {
	tv.tv_sec = sleepTime;
	tv.tv_usec = 0;
	IOMGR_Select(0, 0, 0, 0, &tv);
    }
    /* shrink the file */
    code = ubik_Truncate(tt, 0);
    if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    /* commit */
    code = ubik_EndTrans(tt);
    return code;
}


SAMPLE_Test(rxconn)
     struct rx_connection *rxconn;
{
    afs_int32 code, temp;
    struct ubik_trans *tt;
    struct timeval tv;

    /* first start a new transaction.  Must be a write transaction since
     * we're going to change some data (with ubik_Write) */
    code = ubik_BeginTrans(dbase, UBIK_WRITETRANS, &tt);
    if (code)
	return code;
    printf("about to set lock\n");
    /* now set database locks.  Must do this or people may read uncommitted
     * data.  Note that we're just setting a lock at position 1, which is
     * this program's convention for locking the whole database */
    code = ubik_SetLock(tt, 1, 1, LOCKWRITE);
    printf("now have lock\n");
    if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    /* sleep for a little while to make it possible for us to test for some
     * race conditions */
    if (sleepTime) {
	tv.tv_sec = sleepTime;
	tv.tv_usec = 0;
	IOMGR_Select(0, 0, 0, 0, &tv);
    }
    /* read the original value */
    code = ubik_Read(tt, &temp, sizeof(afs_int32));
    if (code == UEOF) {
	printf("short read, using 0\n");
	temp = 0;
    } else if (code) {
	ubik_AbortTrans(tt);
	return code;
    }
    ubik_AbortTrans(tt);	/* surprise! pretend something went wrong */
    return code;
}


#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    register afs_int32 code, i;
    afs_int32 serverList[MAXSERVERS];
    afs_int32 myHost;
    struct rx_service *tservice;
    struct rx_securityClass *sc[2];
    extern int SAMPLE_ExecuteRequest();
    char dbfileName[128];

    if (argc == 1) {
	printf("usage: userver -servers <serverlist> {-sleep <sleeptime>}\n");
	exit(0);
    }
#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit() < 0)
	return -1;
#endif
    /* parse our own local arguments */
    sleepTime = 0;
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-sleep") == 0) {
	    if (i >= argc - 1) {
		printf("missing time in -sleep argument\n");
		exit(1);
	    }
	    sleepTime = atoi(argv[i + 1]);
	    i++;
	}
    }
    /* call routine to parse command line -servers switch, filling in
     * myHost and serverList arrays appropriately */
    code = ubik_ParseServerList(argc, argv, &myHost, serverList);
    if (code) {
	printf("could not parse server list, code %d\n", code);
	exit(1);
    }
    /* call ServerInit with the values from ParseServerList.  Also specify the
     * name to use for the database files (/tmp/testdb), and the port (3000)
     * for RPC requests.  ServerInit returns a pointer to the database (in
     * dbase), which is required for creating new transactions */

    sprintf(dbfileName, "%s/testdb", gettmpdir());

    code =
	ubik_ServerInit(myHost, htons(3000), serverList, dbfileName, &dbase);

    if (code) {
	printf("ubik init failed with code %d\n", code);
	return;
    }

    sc[0] = rxnull_NewServerSecurityObject();
#if 0
    sc[1] = rxvab_NewServerSecurityObject("applexx", 0);
#endif
    tservice = rx_NewService(0, USER_SERVICE_ID, "Sample", sc, 1 /*2 */ ,
			     SAMPLE_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	printf("Could not create SAMPLE rx service\n");
	exit(3);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 3);

    rx_StartServer(1);		/* Why waste this idle process?? */
}
