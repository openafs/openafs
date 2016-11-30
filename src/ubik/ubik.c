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
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <sys/param.h>
#endif

#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/cellconfig.h>

#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

#include <lwp.h>   /* temporary hack by klm */

#define ERROR_EXIT(code) do { \
    error = (code); \
    goto error_exit; \
} while (0)

/*!
 * \file
 * This system is organized in a hierarchical set of related modules.  Modules
 * at one level can only call modules at the same level or below.
 *
 * At the bottom level (0) we have R, RFTP, LWP and IOMGR, i.e. the basic
 * operating system primitives.
 *
 * At the next level (1) we have
 *
 * \li VOTER--The module responsible for casting votes when asked.  It is also
 * responsible for determining whether this server should try to become
 * a synchronization site.
 * \li BEACONER--The module responsible for sending keep-alives out when a
 * server is actually the sync site, or trying to become a sync site.
 * \li DISK--The module responsible for representing atomic transactions
 * on the local disk.  It maintains a new-value only log.
 * \li LOCK--The module responsible for locking byte ranges in the database file.
 *
 * At the next level (2) we have
 *
 * \li RECOVERY--The module responsible for ensuring that all members of a quorum
 * have the same up-to-date database after a new synchronization site is
 * elected.  This module runs only on the synchronization site.
 *
 * At the next level (3) we have
 *
 * \li REMOTE--The module responsible for interpreting requests from the sync
 * site and applying them to the database, after obtaining the appropriate
 * locks.
 *
 * At the next level (4) we have
 *
 * \li UBIK--The module users call to perform operations on the database.
 */


/* some globals */
afs_int32 ubik_quorum = 0;
struct ubik_dbase *ubik_dbase = 0;
struct ubik_stats ubik_stats;
afs_uint32 ubik_host[UBIK_MAX_INTERFACE_ADDR];
afs_int32 ubik_epochTime = 0;
afs_int32 urecovery_state = 0;
int (*ubik_SRXSecurityProc) (void *, struct rx_securityClass **, afs_int32 *);
void *ubik_SRXSecurityRock;
int (*ubik_SyncWriterCacheProc) (void);
struct ubik_server *ubik_servers;
short ubik_callPortal;

static int BeginTrans(struct ubik_dbase *dbase, afs_int32 transMode,
	   	      struct ubik_trans **transPtr, int readAny);

struct rx_securityClass *ubik_sc[3];

#define	CStampVersion	    1	/* meaning set ts->version */

/*!
 * \brief Perform an operation at a quorum, handling error conditions.
 * \return 0 if all worked and a quorum was contacted successfully
 * \return otherwise mark failing server as down and return #UERROR
 *
 * \note If any server misses an update, we must wait #BIGTIME seconds before
 * allowing the transaction to commit, to ensure that the missing and
 * possibly still functioning server times out and stops handing out old
 * data.  This is done in the commit code, where we wait for a server marked
 * down to have stayed down for #BIGTIME seconds before we allow a transaction
 * to commit.  A server that fails but comes back up won't give out old data
 * because it is sent the sync count along with the beacon message that
 * marks it as \b really up (\p beaconSinceDown).
 */
afs_int32
ContactQuorum_NoArguments(afs_int32 (*proc)(struct rx_connection *, ubik_tid *),
	       		  struct ubik_trans *atrans, int aflags)
{
    struct ubik_server *ts;
    afs_int32 code;
    afs_int32 rcode, okcalls;

    rcode = 0;
    okcalls = 0;
    for (ts = ubik_servers; ts; ts = ts->next) {
	/* for each server */
	if (!ts->up || !ts->currentDB) {
	    ts->currentDB = 0;	/* db is no longer current; we just missed an update */
	    continue;		/* not up-to-date, don't bother */
	}
	code = (*proc)(ts->disk_rxcid, &atrans->tid);
	if (code) {		/* failure */
	    rcode = code;
	    ts->up = 0;		/* mark as down now; beacons will no longer be sent */
	    ts->currentDB = 0;
	    ts->beaconSinceDown = 0;
	    urecovery_LostServer();	/* tell recovery to try to resend dbase later */
	} else {		/* success */
	    if (!ts->isClone)
		okcalls++;	/* count up how many worked */
	    if (aflags & CStampVersion) {
		ts->version = atrans->dbase->version;
	    }
	}
    }
    /* return 0 if we successfully contacted a quorum, otherwise return error code.  We don't have to contact ourselves (that was done locally) */
    if (okcalls + 1 >= ubik_quorum)
	return 0;
    else
	return (rcode != 0) ? rcode : UNOQUORUM;
}

afs_int32
ContactQuorum_DISK_Lock(struct ubik_trans *atrans, int aflags,afs_int32 file,
			afs_int32 position, afs_int32 length, afs_int32 type)
{
    struct ubik_server *ts;
    afs_int32 code;
    afs_int32 rcode, okcalls;

    rcode = 0;
    okcalls = 0;
    for (ts = ubik_servers; ts; ts = ts->next) {
	/* for each server */
	if (!ts->up || !ts->currentDB) {
	    ts->currentDB = 0;	/* db is no longer current; we just missed an update */
	    continue;		/* not up-to-date, don't bother */
	}
	code = DISK_Lock(ts->disk_rxcid, &atrans->tid, file, position, length,
			   type);
	if (code) {		/* failure */
	    rcode = code;
	    ts->up = 0;		/* mark as down now; beacons will no longer be sent */
	    ts->currentDB = 0;
	    ts->beaconSinceDown = 0;
	    urecovery_LostServer();	/* tell recovery to try to resend dbase later */
	} else {		/* success */
	    if (!ts->isClone)
		okcalls++;	/* count up how many worked */
	    if (aflags & CStampVersion) {
		ts->version = atrans->dbase->version;
	    }
	}
    }
    /* return 0 if we successfully contacted a quorum, otherwise return error code.  We don't have to contact ourselves (that was done locally) */
    if (okcalls + 1 >= ubik_quorum)
	return 0;
    else
	return (rcode != 0) ? rcode : UNOQUORUM;
}

afs_int32
ContactQuorum_DISK_Write(struct ubik_trans *atrans, int aflags,
			 afs_int32 file, afs_int32 position, bulkdata *data)
{
    struct ubik_server *ts;
    afs_int32 code;
    afs_int32 rcode, okcalls;

    rcode = 0;
    okcalls = 0;
    for (ts = ubik_servers; ts; ts = ts->next) {
	/* for each server */
	if (!ts->up || !ts->currentDB) {
	    ts->currentDB = 0;	/* db is no longer current; we just missed an update */
	    continue;		/* not up-to-date, don't bother */
	}
	code = DISK_Write(ts->disk_rxcid, &atrans->tid, file, position, data);
	if (code) {		/* failure */
	    rcode = code;
	    ts->up = 0;		/* mark as down now; beacons will no longer be sent */
	    ts->currentDB = 0;
	    ts->beaconSinceDown = 0;
	    urecovery_LostServer();	/* tell recovery to try to resend dbase later */
	} else {		/* success */
	    if (!ts->isClone)
		okcalls++;	/* count up how many worked */
	    if (aflags & CStampVersion) {
		ts->version = atrans->dbase->version;
	    }
	}
    }
    /* return 0 if we successfully contacted a quorum, otherwise return error code.  We don't have to contact ourselves (that was done locally) */
    if (okcalls + 1 >= ubik_quorum)
	return 0;
    else
	return (rcode != 0) ? rcode : UNOQUORUM;
}

afs_int32
ContactQuorum_DISK_Truncate(struct ubik_trans *atrans, int aflags,
			    afs_int32 file, afs_int32 length)
{
    struct ubik_server *ts;
    afs_int32 code;
    afs_int32 rcode, okcalls;

    rcode = 0;
    okcalls = 0;
    for (ts = ubik_servers; ts; ts = ts->next) {
	/* for each server */
	if (!ts->up || !ts->currentDB) {
	    ts->currentDB = 0;	/* db is no longer current; we just missed an update */
	    continue;		/* not up-to-date, don't bother */
	}
	code = DISK_Truncate(ts->disk_rxcid, &atrans->tid, file, length);
	if (code) {		/* failure */
	    rcode = code;
	    ts->up = 0;		/* mark as down now; beacons will no longer be sent */
	    ts->currentDB = 0;
	    ts->beaconSinceDown = 0;
	    urecovery_LostServer();	/* tell recovery to try to resend dbase later */
	} else {		/* success */
	    if (!ts->isClone)
		okcalls++;	/* count up how many worked */
	    if (aflags & CStampVersion) {
		ts->version = atrans->dbase->version;
	    }
	}
    }
    /* return 0 if we successfully contacted a quorum, otherwise return error code.  We don't have to contact ourselves (that was done locally) */
    if (okcalls + 1 >= ubik_quorum)
	return 0;
    else
	return (rcode != 0) ? rcode : UNOQUORUM;
}

afs_int32
ContactQuorum_DISK_WriteV(struct ubik_trans *atrans, int aflags,
			  iovec_wrt * io_vector, iovec_buf *io_buffer)
{
    struct ubik_server *ts;
    afs_int32 code;
    afs_int32 rcode, okcalls;

    rcode = 0;
    okcalls = 0;
    for (ts = ubik_servers; ts; ts = ts->next) {
	/* for each server */
	if (!ts->up || !ts->currentDB) {
	    ts->currentDB = 0;	/* db is no longer current; we just missed an update */
	    continue;		/* not up-to-date, don't bother */
	}

	code = DISK_WriteV(ts->disk_rxcid, &atrans->tid, io_vector, io_buffer);

	if ((code <= -450) && (code > -500)) {
	    /* An RPC interface mismatch (as defined in comerr/error_msg.c).
	     * Un-bulk the entries and do individual DISK_Write calls
	     * instead of DISK_WriteV.
	     */
	    struct ubik_iovec *iovec =
		(struct ubik_iovec *)io_vector->iovec_wrt_val;
	    char *iobuf = (char *)io_buffer->iovec_buf_val;
	    bulkdata tcbs;
	    afs_int32 i, offset;

	    for (i = 0, offset = 0; i < io_vector->iovec_wrt_len; i++) {
		/* Sanity check for going off end of buffer */
		if ((offset + iovec[i].length) > io_buffer->iovec_buf_len) {
		    code = UINTERNAL;
		    break;
		}
		tcbs.bulkdata_len = iovec[i].length;
		tcbs.bulkdata_val = &iobuf[offset];
		code =
		    DISK_Write(ts->disk_rxcid, &atrans->tid, iovec[i].file,
			       iovec[i].position, &tcbs);
		if (code)
		    break;

		offset += iovec[i].length;
	    }
	}

	if (code) {		/* failure */
	    rcode = code;
	    ts->up = 0;		/* mark as down now; beacons will no longer be sent */
	    ts->currentDB = 0;
	    ts->beaconSinceDown = 0;
	    urecovery_LostServer();	/* tell recovery to try to resend dbase later */
	} else {		/* success */
	    if (!ts->isClone)
		okcalls++;	/* count up how many worked */
	    if (aflags & CStampVersion) {
		ts->version = atrans->dbase->version;
	    }
	}
    }
    /* return 0 if we successfully contacted a quorum, otherwise return error code.  We don't have to contact ourselves (that was done locally) */
    if (okcalls + 1 >= ubik_quorum)
	return 0;
    else
	return (rcode != 0) ? rcode : UNOQUORUM;
}

afs_int32
ContactQuorum_DISK_SetVersion(struct ubik_trans *atrans, int aflags,
			      ubik_version *OldVersion,
			      ubik_version *NewVersion)
{
    struct ubik_server *ts;
    afs_int32 code;
    afs_int32 rcode, okcalls;

    rcode = 0;
    okcalls = 0;
    for (ts = ubik_servers; ts; ts = ts->next) {
	/* for each server */
	if (!ts->up || !ts->currentDB) {
	    ts->currentDB = 0;	/* db is no longer current; we just missed an update */
	    continue;		/* not up-to-date, don't bother */
	}
	code = DISK_SetVersion(ts->disk_rxcid, &atrans->tid, OldVersion,
			       NewVersion);
	if (code) {		/* failure */
	    rcode = code;
	    ts->up = 0;		/* mark as down now; beacons will no longer be sent */
	    ts->currentDB = 0;
	    ts->beaconSinceDown = 0;
	    urecovery_LostServer();	/* tell recovery to try to resend dbase later */
	} else {		/* success */
	    if (!ts->isClone)
		okcalls++;	/* count up how many worked */
	    if (aflags & CStampVersion) {
		ts->version = atrans->dbase->version;
	    }
	}
    }
    /* return 0 if we successfully contacted a quorum, otherwise return error code.  We don't have to contact ourselves (that was done locally) */
    if (okcalls + 1 >= ubik_quorum)
	return 0;
    else
	return (rcode != 0) ? rcode : UNOQUORUM;
}

/*!
 * \brief This routine initializes the ubik system for a set of servers.
 * \return 0 for success, or an error code on failure.
 * \param serverList set of servers specified; nServers gives the number of entries in this array.
 * \param pathName provides an initial prefix used for naming storage files used by this system.
 * \param dbase the returned structure representing this instance of an ubik; it is passed to various calls below.
 *
 * \todo This routine should perhaps be generalized to a low-level disk interface providing read, write, file enumeration and sync operations.
 *
 * \warning The host named by myHost should not also be listed in serverList.
 *
 * \see ubik_ServerInit(), ubik_ServerInitByInfo()
 */
int
ubik_ServerInitCommon(afs_uint32 myHost, short myPort,
		      struct afsconf_cell *info, char clones[],
		      afs_uint32 serverList[], const char *pathName,
		      struct ubik_dbase **dbase)
{
    struct ubik_dbase *tdb;
    afs_int32 code;
#ifdef AFS_PTHREAD_ENV
    pthread_t rxServerThread;        /* pthread variables */
    pthread_t ubeacon_InteractThread;
    pthread_t urecovery_InteractThread;
    pthread_attr_t rxServer_tattr;
    pthread_attr_t ubeacon_Interact_tattr;
    pthread_attr_t urecovery_Interact_tattr;
#else
    PROCESS junk;
    extern int rx_stackSize;
#endif

    afs_int32 secIndex;
    struct rx_securityClass *secClass;

    struct rx_service *tservice;

    initialize_U_error_table();

    tdb = (struct ubik_dbase *)malloc(sizeof(struct ubik_dbase));
    tdb->pathName = (char *)malloc(strlen(pathName) + 1);
    strcpy(tdb->pathName, pathName);
    tdb->activeTrans = (struct ubik_trans *)0;
    memset(&tdb->version, 0, sizeof(struct ubik_version));
    memset(&tdb->cachedVersion, 0, sizeof(struct ubik_version));
#ifdef AFS_PTHREAD_ENV
    MUTEX_INIT(&tdb->versionLock, "version lock", MUTEX_DEFAULT, 0);
#else
    Lock_Init(&tdb->versionLock);
#endif
    Lock_Init(&tdb->cache_lock);
    tdb->flags = 0;
    tdb->read = uphys_read;
    tdb->write = uphys_write;
    tdb->truncate = uphys_truncate;
    tdb->open = uphys_invalidate;	/* this function isn't used any more */
    tdb->sync = uphys_sync;
    tdb->stat = uphys_stat;
    tdb->getlabel = uphys_getlabel;
    tdb->setlabel = uphys_setlabel;
    tdb->getnfiles = uphys_getnfiles;
    tdb->readers = 0;
    tdb->tidCounter = tdb->writeTidCounter = 0;
    *dbase = tdb;
    ubik_dbase = tdb;		/* for now, only one db per server; can fix later when we have names for the other dbases */

#ifdef AFS_PTHREAD_ENV
    CV_INIT(&tdb->version_cond, "version", CV_DEFAULT, 0);
    CV_INIT(&tdb->flags_cond, "flags", CV_DEFAULT, 0);
#endif /* AFS_PTHREAD_ENV */

    /* initialize RX */

    /* the following call is idempotent so when/if it got called earlier,
     * by whatever called us, it doesn't really matter -- klm */
    code = rx_Init(myPort);
    if (code < 0)
	return code;

    ubik_callPortal = myPort;
    /* try to get an additional security object */
    ubik_sc[0] = rxnull_NewServerSecurityObject();
    ubik_sc[1] = 0;
    ubik_sc[2] = 0;
    if (ubik_SRXSecurityProc) {
	code =
	    (*ubik_SRXSecurityProc) (ubik_SRXSecurityRock, &secClass,
				     &secIndex);
	if (code == 0) {
	    ubik_sc[secIndex] = secClass;
	}
    }
    /* for backwards compat this should keep working as it does now
       and not host bind */
#if 0
    /* This really needs to be up above, where I have put it.  It works
     * here when we're non-pthreaded, but the code above, when using
     * pthreads may (and almost certainly does) end up calling on a
     * pthread resource which gets initialized by rx_Init.  The end
     * result is that an assert fails and the program dies. -- klm
     */
    code = rx_Init(myPort);
    if (code < 0)
	return code;
#endif

    tservice =
	rx_NewService(0, VOTE_SERVICE_ID, "VOTE", ubik_sc, 3,
		      VOTE_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	ubik_dprint("Could not create VOTE rx service!\n");
	return -1;
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 3);

    tservice =
	rx_NewService(0, DISK_SERVICE_ID, "DISK", ubik_sc, 3,
		      DISK_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	ubik_dprint("Could not create DISK rx service!\n");
	return -1;
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 3);

    /* start an rx_ServerProc to handle incoming RPC's in particular the
     * UpdateInterfaceAddr RPC that occurs in ubeacon_InitServerList. This avoids
     * the "steplock" problem in ubik initialization. Defect 11037.
     */
#ifdef AFS_PTHREAD_ENV
/* do assert stuff */
    osi_Assert(pthread_attr_init(&rxServer_tattr) == 0);
    osi_Assert(pthread_attr_setdetachstate(&rxServer_tattr, PTHREAD_CREATE_DETACHED) == 0);
/*    osi_Assert(pthread_attr_setstacksize(&rxServer_tattr, rx_stackSize) == 0); */

    osi_Assert(pthread_create(&rxServerThread, &rxServer_tattr, (void *)rx_ServerProc, NULL) == 0);
#else
    LWP_CreateProcess(rx_ServerProc, rx_stackSize, RX_PROCESS_PRIORITY,
              NULL, "rx_ServerProc", &junk);
#endif

    /* do basic initialization */
    code = uvote_Init();
    if (code)
	return code;
    code = urecovery_Initialize(tdb);
    if (code)
	return code;
    if (info)
	code = ubeacon_InitServerListByInfo(myHost, info, clones);
    else
	code = ubeacon_InitServerList(myHost, serverList);
    if (code)
	return code;

    /* now start up async processes */
#ifdef AFS_PTHREAD_ENV
/* do assert stuff */
    osi_Assert(pthread_attr_init(&ubeacon_Interact_tattr) == 0);
    osi_Assert(pthread_attr_setdetachstate(&ubeacon_Interact_tattr, PTHREAD_CREATE_DETACHED) == 0);
/*    osi_Assert(pthread_attr_setstacksize(&ubeacon_Interact_tattr, 16384) == 0); */
    /*  need another attr set here for priority???  - klm */

    osi_Assert(pthread_create(&ubeacon_InteractThread, &ubeacon_Interact_tattr,
           (void *)ubeacon_Interact, NULL) == 0);
#else
    code = LWP_CreateProcess(ubeacon_Interact, 16384 /*8192 */ ,
			     LWP_MAX_PRIORITY - 1, (void *)0, "beacon",
			     &junk);
    if (code)
	return code;
#endif

#ifdef AFS_PTHREAD_ENV
/* do assert stuff */
    osi_Assert(pthread_attr_init(&urecovery_Interact_tattr) == 0);
    osi_Assert(pthread_attr_setdetachstate(&urecovery_Interact_tattr, PTHREAD_CREATE_DETACHED) == 0);
/*    osi_Assert(pthread_attr_setstacksize(&urecovery_Interact_tattr, 16384) == 0); */
    /*  need another attr set here for priority???  - klm */

    osi_Assert(pthread_create(&urecovery_InteractThread, &urecovery_Interact_tattr,
           (void *)urecovery_Interact, NULL) == 0);

    return 0;  /* is this correct?  - klm */
#else
    code = LWP_CreateProcess(urecovery_Interact, 16384 /*8192 */ ,
			     LWP_MAX_PRIORITY - 1, (void *)0, "recovery",
			     &junk);
    return code;
#endif

}

/*!
 * \see ubik_ServerInitCommon()
 */
int
ubik_ServerInitByInfo(afs_uint32 myHost, short myPort,
		      struct afsconf_cell *info, char clones[],
		      const char *pathName, struct ubik_dbase **dbase)
{
    afs_int32 code;

    code =
	ubik_ServerInitCommon(myHost, myPort, info, clones, 0, pathName,
			      dbase);
    return code;
}

/*!
 * \see ubik_ServerInitCommon()
 */
int
ubik_ServerInit(afs_uint32 myHost, short myPort, afs_uint32 serverList[],
		const char *pathName, struct ubik_dbase **dbase)
{
    afs_int32 code;

    code =
	ubik_ServerInitCommon(myHost, myPort, (struct afsconf_cell *)0, 0,
			      serverList, pathName, dbase);
    return code;
}

/*!
 * \brief This routine begins a read or write transaction on the transaction
 * identified by transPtr, in the dbase named by dbase.
 *
 * An open mode of ubik_READTRANS identifies this as a read transaction,
 * while a mode of ubik_WRITETRANS identifies this as a write transaction.
 * transPtr is set to the returned transaction control block.
 * The readAny flag is set to 0 or 1 or 2 by the wrapper functions
 * ubik_BeginTrans() or ubik_BeginTransReadAny() or
 * ubik_BeginTransReadAnyWrite() below.
 *
 * \note We can only begin transaction when we have an up-to-date database.
 */
static int
BeginTrans(struct ubik_dbase *dbase, afs_int32 transMode,
	   struct ubik_trans **transPtr, int readAny)
{
    struct ubik_trans *jt;
    struct ubik_trans *tt;
    afs_int32 code;
#if defined(UBIK_PAUSE)
    int count;
#endif /* UBIK_PAUSE */

    if (readAny > 1 && ubik_SyncWriterCacheProc == NULL) {
	/* it's not safe to use ubik_BeginTransReadAnyWrite without a
	 * cache-syncing function; fall back to ubik_BeginTransReadAny,
	 * which is safe but slower */
	ubik_print("ubik_BeginTransReadAnyWrite called, but "
	           "ubik_SyncWriterCacheProc not set; pretending "
	           "ubik_BeginTransReadAny was called instead\n");
	readAny = 1;
    }

    if ((transMode != UBIK_READTRANS) && readAny)
	return UBADTYPE;
    DBHOLD(dbase);
#if defined(UBIK_PAUSE)
    /* if we're polling the slave sites, wait until the returns
     *  are all in.  Otherwise, the urecovery_CheckTid call may
     *  glitch us.
     */
    if (transMode == UBIK_WRITETRANS)
	for (count = 75; dbase->flags & DBVOTING; --count) {
	    DBRELE(dbase);
#ifdef GRAND_PAUSE_DEBUGGING
	    if (count == 75)
		fprintf(stderr,
			"%ld: myport=%d: BeginTrans is waiting 'cause of voting conflict\n",
			time(0), ntohs(ubik_callPortal));
	    else
#endif
	    if (count <= 0) {
#if 1
		fprintf(stderr,
			"%ld: myport=%d: BeginTrans failed because of voting conflict\n",
			time(0), ntohs(ubik_callPortal));
#endif
		return UNOQUORUM;	/* a white lie */
	    }
#ifdef AFS_PTHREAD_ENV
	    sleep(2);
#else
	    IOMGR_Sleep(2);
#endif
	    DBHOLD(dbase);
	}
#endif /* UBIK_PAUSE */
    if (urecovery_AllBetter(dbase, readAny) == 0) {
	DBRELE(dbase);
	return UNOQUORUM;
    }
    /* otherwise we have a quorum, use it */

    /* make sure that at most one write transaction occurs at any one time.  This
     * has nothing to do with transaction locking; that's enforced by the lock package.  However,
     * we can't even handle two non-conflicting writes, since our log and recovery modules
     * don't know how to restore one without possibly picking up some data from the other. */
    if (transMode == UBIK_WRITETRANS) {
	/* if we're writing already, wait */
	while (dbase->flags & DBWRITING) {
#ifdef AFS_PTHREAD_ENV
	    CV_WAIT(&dbase->flags_cond, &dbase->versionLock);
#else
	    DBRELE(dbase);
	    LWP_WaitProcess(&dbase->flags);
	    DBHOLD(dbase);
#endif
	}

	if (!ubeacon_AmSyncSite()) {
	    DBRELE(dbase);
	    return UNOTSYNC;
	}
    }

    /* create the transaction */
    code = udisk_begin(dbase, transMode, &jt);	/* can't take address of register var */
    tt = jt;			/* move to a register */
    if (code || tt == (struct ubik_trans *)NULL) {
	DBRELE(dbase);
	return code;
    }
    if (readAny) {
	tt->flags |= TRREADANY;
	if (readAny > 1) {
	    tt->flags |= TRREADWRITE;
	}
    }
    /* label trans and dbase with new tid */
    tt->tid.epoch = ubik_epochTime;
    /* bump by two, since tidCounter+1 means trans id'd by tidCounter has finished */
    tt->tid.counter = (dbase->tidCounter += 2);

    if (transMode == UBIK_WRITETRANS) {
	/* for a write trans, we have to keep track of the write tid counter too */
	dbase->writeTidCounter = tt->tid.counter;

	/* next try to start transaction on appropriate number of machines */
	code = ContactQuorum_NoArguments(DISK_Begin, tt, 0);
	if (code) {
	    /* we must abort the operation */
	    udisk_abort(tt);
	    ContactQuorum_NoArguments(DISK_Abort, tt, 0); /* force aborts to the others */
	    udisk_end(tt);
	    DBRELE(dbase);
	    return code;
	}
    }

    *transPtr = tt;
    DBRELE(dbase);
    return 0;
}

/*!
 * \see BeginTrans()
 */
int
ubik_BeginTrans(struct ubik_dbase *dbase, afs_int32 transMode,
		struct ubik_trans **transPtr)
{
    return BeginTrans(dbase, transMode, transPtr, 0);
}

/*!
 * \see BeginTrans()
 */
int
ubik_BeginTransReadAny(struct ubik_dbase *dbase, afs_int32 transMode,
		       struct ubik_trans **transPtr)
{
    return BeginTrans(dbase, transMode, transPtr, 1);
}

/*!
 * \see BeginTrans()
 */
int
ubik_BeginTransReadAnyWrite(struct ubik_dbase *dbase, afs_int32 transMode,
                            struct ubik_trans **transPtr)
{
    return BeginTrans(dbase, transMode, transPtr, 2);
}

/*!
 * \brief This routine ends a read or write transaction by aborting it.
 */
int
ubik_AbortTrans(struct ubik_trans *transPtr)
{
    afs_int32 code;
    afs_int32 code2;
    struct ubik_dbase *dbase;

    dbase = transPtr->dbase;

    if (transPtr->flags & TRCACHELOCKED) {
	ReleaseReadLock(&dbase->cache_lock);
	transPtr->flags &= ~TRCACHELOCKED;
    }

    ObtainWriteLock(&dbase->cache_lock);

    DBHOLD(dbase);
    memset(&dbase->cachedVersion, 0, sizeof(struct ubik_version));

    ReleaseWriteLock(&dbase->cache_lock);

    /* see if we're still up-to-date */
    if (!urecovery_AllBetter(dbase, transPtr->flags & TRREADANY)) {
	udisk_abort(transPtr);
	udisk_end(transPtr);
	DBRELE(dbase);
	return UNOQUORUM;
    }

    if (transPtr->type == UBIK_READTRANS) {
	code = udisk_abort(transPtr);
	udisk_end(transPtr);
	DBRELE(dbase);
	return code;
    }

    /* below here, we know we're doing a write transaction */
    if (!ubeacon_AmSyncSite()) {
	udisk_abort(transPtr);
	udisk_end(transPtr);
	DBRELE(dbase);
	return UNOTSYNC;
    }

    /* now it is safe to try remote abort */
    code = ContactQuorum_NoArguments(DISK_Abort, transPtr, 0);
    code2 = udisk_abort(transPtr);
    udisk_end(transPtr);
    DBRELE(dbase);
    return (code ? code : code2);
}

static void
WritebackApplicationCache(struct ubik_dbase *dbase)
{
    int code = 0;
    if (ubik_SyncWriterCacheProc) {
	code = ubik_SyncWriterCacheProc();
    }
    if (code) {
	/* we failed to sync the local cache, so just invalidate the cache;
	 * we'll try to read the cache in again on the next read */
	memset(&dbase->cachedVersion, 0, sizeof(dbase->cachedVersion));
    } else {
	memcpy(&dbase->cachedVersion, &dbase->version,
	       sizeof(dbase->cachedVersion));
    }
}

/*!
 * \brief This routine ends a read or write transaction on the open transaction identified by transPtr.
 * \return an error code.
 */
int
ubik_EndTrans(struct ubik_trans *transPtr)
{
    afs_int32 code;
    struct timeval tv;
    afs_int32 realStart;
    struct ubik_server *ts;
    afs_int32 now;
    int cachelocked = 0;
    struct ubik_dbase *dbase;

    if (transPtr->type == UBIK_WRITETRANS) {
	code = ubik_Flush(transPtr);
	if (code) {
	    ubik_AbortTrans(transPtr);
	    return (code);
	}
    }

    dbase = transPtr->dbase;

    if (transPtr->flags & TRCACHELOCKED) {
	ReleaseReadLock(&dbase->cache_lock);
	transPtr->flags &= ~TRCACHELOCKED;
    }

    if (transPtr->type != UBIK_READTRANS) {
	/* must hold cache_lock before DBHOLD'ing */
	ObtainWriteLock(&dbase->cache_lock);
	cachelocked = 1;
    }

    DBHOLD(dbase);

    /* give up if no longer current */
    if (!urecovery_AllBetter(dbase, transPtr->flags & TRREADANY)) {
	udisk_abort(transPtr);
	udisk_end(transPtr);
	DBRELE(dbase);
	code = UNOQUORUM;
	goto error;
    }

    if (transPtr->type == UBIK_READTRANS) {	/* reads are easy */
	code = udisk_commit(transPtr);
	if (code == 0)
	    goto success;	/* update cachedVersion correctly */
	udisk_end(transPtr);
	DBRELE(dbase);
	goto error;
    }

    if (!ubeacon_AmSyncSite()) {	/* no longer sync site */
	udisk_abort(transPtr);
	udisk_end(transPtr);
	DBRELE(dbase);
	code = UNOTSYNC;
	goto error;
    }

    /* now it is safe to do commit */
    code = udisk_commit(transPtr);
    if (code == 0) {
	/* db data has been committed locally; update the local cache so
	 * readers can get at it */
	WritebackApplicationCache(dbase);

	ReleaseWriteLock(&dbase->cache_lock);

	code = ContactQuorum_NoArguments(DISK_Commit, transPtr, CStampVersion);

    } else {
	memset(&dbase->cachedVersion, 0, sizeof(struct ubik_version));
	ReleaseWriteLock(&dbase->cache_lock);
    }
    cachelocked = 0;
    if (code) {
	/* failed to commit, so must return failure.  Try to clear locks first, just for fun
	 * Note that we don't know if this transaction will eventually commit at this point.
	 * If it made it to a site that will be present in the next quorum, we win, otherwise
	 * we lose.  If we contact a majority of sites, then we won't be here: contacting
	 * a majority guarantees commit, since it guarantees that one dude will be a
	 * member of the next quorum. */
	ContactQuorum_NoArguments(DISK_ReleaseLocks, transPtr, 0);
	udisk_end(transPtr);
	DBRELE(dbase);
	goto error;
    }
    /* before we can start sending unlock messages, we must wait until all servers
     * that are possibly still functioning on the other side of a network partition
     * have timed out.  Check the server structures, compute how long to wait, then
     * start the unlocks */
    realStart = FT_ApproxTime();
    while (1) {
	/* wait for all servers to time out */
	code = 0;
	now = FT_ApproxTime();
	/* check if we're still sync site, the guy should either come up
	 * to us, or timeout.  Put safety check in anyway */
	if (now - realStart > 10 * BIGTIME) {
	    ubik_stats.escapes++;
	    ubik_print("ubik escaping from commit wait\n");
	    break;
	}
	for (ts = ubik_servers; ts; ts = ts->next) {
	    if (!ts->beaconSinceDown && now <= ts->lastBeaconSent + BIGTIME) {
		/* this guy could have some damaged data, wait for him */
		code = 1;
		tv.tv_sec = 1;	/* try again after a while (ha ha) */
		tv.tv_usec = 0;
#ifdef AFS_PTHREAD_ENV
		select(0, 0, 0, 0, &tv);
#else
		IOMGR_Select(0, 0, 0, 0, &tv);	/* poll, should we wait on something? */
#endif
		break;
	    }
	}
	if (code == 0)
	    break;		/* no down ones still pseudo-active */
    }

    /* finally, unlock all the dudes.  We can return success independent of the number of servers
     * that really unlock the dbase; the others will do it if/when they elect a new sync site.
     * The transaction is committed anyway, since we succeeded in contacting a quorum
     * at the start (when invoking the DiskCommit function).
     */
    ContactQuorum_NoArguments(DISK_ReleaseLocks, transPtr, 0);

  success:
    udisk_end(transPtr);
    /* don't update cachedVersion here; it should have been updated way back
     * in ubik_CheckCache, and earlier in this function for writes */
    DBRELE(dbase);
    if (cachelocked) {
	ReleaseWriteLock(&dbase->cache_lock);
    }
    return 0;

  error:
    if (!cachelocked) {
	ObtainWriteLock(&dbase->cache_lock);
    }
    memset(&dbase->cachedVersion, 0, sizeof(struct ubik_version));
    ReleaseWriteLock(&dbase->cache_lock);
    return code;
}

/*!
 * \brief This routine reads length bytes into buffer from the current position in the database.
 *
 * The file pointer is updated appropriately (by adding the number of bytes actually transferred), and the length actually transferred is stored in the long integer pointed to by length.  A short read returns zero for an error code.
 *
 * \note *length is an INOUT parameter: at the start it represents the size of the buffer, and when done, it contains the number of bytes actually transferred.
 */
int
ubik_Read(struct ubik_trans *transPtr, void *buffer,
	  afs_int32 length)
{
    afs_int32 code;

    /* reads are easy to do: handle locally */
    DBHOLD(transPtr->dbase);
    if (!urecovery_AllBetter(transPtr->dbase, transPtr->flags & TRREADANY)) {
	DBRELE(transPtr->dbase);
	return UNOQUORUM;
    }

    code =
	udisk_read(transPtr, transPtr->seekFile, buffer, transPtr->seekPos,
		   length);
    if (code == 0) {
	transPtr->seekPos += length;
    }
    DBRELE(transPtr->dbase);
    return code;
}

/*!
 * \brief This routine will flush the io data in the iovec structures.
 *
 * It first flushes to the local disk and then uses ContactQuorum to write it
 * to the other servers.
 */
int
ubik_Flush(struct ubik_trans *transPtr)
{
    afs_int32 code, error = 0;

    if (transPtr->type != UBIK_WRITETRANS)
	return UBADTYPE;
    if (!transPtr->iovec_info.iovec_wrt_len
	|| !transPtr->iovec_info.iovec_wrt_val)
	return 0;

    DBHOLD(transPtr->dbase);
    if (!urecovery_AllBetter(transPtr->dbase, transPtr->flags & TRREADANY))
	ERROR_EXIT(UNOQUORUM);
    if (!ubeacon_AmSyncSite())	/* only sync site can write */
	ERROR_EXIT(UNOTSYNC);

    /* Update the rest of the servers in the quorum */
    code =
	ContactQuorum_DISK_WriteV(transPtr, 0, &transPtr->iovec_info,
				  &transPtr->iovec_data);
    if (code) {
	udisk_abort(transPtr);
	ContactQuorum_NoArguments(DISK_Abort, transPtr, 0); /* force aborts to the others */
	transPtr->iovec_info.iovec_wrt_len = 0;
	transPtr->iovec_data.iovec_buf_len = 0;
	ERROR_EXIT(code);
    }

    /* Wrote the buffers out, so start at scratch again */
    transPtr->iovec_info.iovec_wrt_len = 0;
    transPtr->iovec_data.iovec_buf_len = 0;

  error_exit:
    DBRELE(transPtr->dbase);
    return error;
}

int
ubik_Write(struct ubik_trans *transPtr, void *vbuffer,
	   afs_int32 length)
{
    struct ubik_iovec *iovec;
    afs_int32 code, error = 0;
    afs_int32 pos, len, size;
    char * buffer = (char *)vbuffer;

    if (transPtr->type != UBIK_WRITETRANS)
	return UBADTYPE;
    if (!length)
	return 0;

    if (length > IOVEC_MAXBUF) {
	for (pos = 0, len = length; len > 0; len -= size, pos += size) {
	    size = ((len < IOVEC_MAXBUF) ? len : IOVEC_MAXBUF);
	    code = ubik_Write(transPtr, buffer+pos, size);
	    if (code)
		return (code);
	}
	return 0;
    }

    if (!transPtr->iovec_info.iovec_wrt_val) {
	transPtr->iovec_info.iovec_wrt_len = 0;
	transPtr->iovec_info.iovec_wrt_val =
	    (struct ubik_iovec *)malloc(IOVEC_MAXWRT *
					sizeof(struct ubik_iovec));
	transPtr->iovec_data.iovec_buf_len = 0;
	transPtr->iovec_data.iovec_buf_val = (char *)malloc(IOVEC_MAXBUF);
	if (!transPtr->iovec_info.iovec_wrt_val
	    || !transPtr->iovec_data.iovec_buf_val) {
	    if (transPtr->iovec_info.iovec_wrt_val)
		free(transPtr->iovec_info.iovec_wrt_val);
	    transPtr->iovec_info.iovec_wrt_val = 0;
	    if (transPtr->iovec_data.iovec_buf_val)
		free(transPtr->iovec_data.iovec_buf_val);
	    transPtr->iovec_data.iovec_buf_val = 0;
	    return UNOMEM;
	}
    }

    /* If this write won't fit in the structure, then flush it out and start anew */
    if ((transPtr->iovec_info.iovec_wrt_len >= IOVEC_MAXWRT)
	|| ((length + transPtr->iovec_data.iovec_buf_len) > IOVEC_MAXBUF)) {
	code = ubik_Flush(transPtr);
	if (code)
	    return (code);
    }

    DBHOLD(transPtr->dbase);
    if (!urecovery_AllBetter(transPtr->dbase, transPtr->flags & TRREADANY))
	ERROR_EXIT(UNOQUORUM);
    if (!ubeacon_AmSyncSite())	/* only sync site can write */
	ERROR_EXIT(UNOTSYNC);

    /* Write to the local disk */
    code =
	udisk_write(transPtr, transPtr->seekFile, buffer, transPtr->seekPos,
		    length);
    if (code) {
	udisk_abort(transPtr);
	transPtr->iovec_info.iovec_wrt_len = 0;
	transPtr->iovec_data.iovec_buf_len = 0;
	DBRELE(transPtr->dbase);
	return (code);
    }

    /* Collect writes for the other ubik servers (to be done in bulk) */
    iovec = (struct ubik_iovec *)transPtr->iovec_info.iovec_wrt_val;
    iovec[transPtr->iovec_info.iovec_wrt_len].file = transPtr->seekFile;
    iovec[transPtr->iovec_info.iovec_wrt_len].position = transPtr->seekPos;
    iovec[transPtr->iovec_info.iovec_wrt_len].length = length;

    memcpy(&transPtr->iovec_data.
	   iovec_buf_val[transPtr->iovec_data.iovec_buf_len], buffer, length);

    transPtr->iovec_info.iovec_wrt_len++;
    transPtr->iovec_data.iovec_buf_len += length;
    transPtr->seekPos += length;

  error_exit:
    DBRELE(transPtr->dbase);
    return error;
}

/*!
 * \brief This sets the file pointer associated with the current transaction
 * to the appropriate file and byte position.
 *
 * Unlike Unix files, a transaction is labelled by both a file number \p fileid
 * and a byte position relative to the specified file \p position.
 */
int
ubik_Seek(struct ubik_trans *transPtr, afs_int32 fileid,
	  afs_int32 position)
{
    afs_int32 code;

    DBHOLD(transPtr->dbase);
    if (!urecovery_AllBetter(transPtr->dbase, transPtr->flags & TRREADANY)) {
	code = UNOQUORUM;
    } else {
	transPtr->seekFile = fileid;
	transPtr->seekPos = position;
	code = 0;
    }
    DBRELE(transPtr->dbase);
    return code;
}

/*!
 * \brief This call returns the file pointer associated with the specified
 * transaction in \p fileid and \p position.
 */
int
ubik_Tell(struct ubik_trans *transPtr, afs_int32 * fileid,
	  afs_int32 * position)
{
    DBHOLD(transPtr->dbase);
    *fileid = transPtr->seekFile;
    *position = transPtr->seekPos;
    DBRELE(transPtr->dbase);
    return 0;
}

/*!
 * \brief This sets the file size for the currently-selected file to \p length
 * bytes, if length is less than the file's current size.
 */
int
ubik_Truncate(struct ubik_trans *transPtr, afs_int32 length)
{
    afs_int32 code, error = 0;

    /* Will also catch if not UBIK_WRITETRANS */
    code = ubik_Flush(transPtr);
    if (code)
	return (code);

    DBHOLD(transPtr->dbase);
    /* first, check that quorum is still good, and that dbase is up-to-date */
    if (!urecovery_AllBetter(transPtr->dbase, transPtr->flags & TRREADANY))
	ERROR_EXIT(UNOQUORUM);
    if (!ubeacon_AmSyncSite())
	ERROR_EXIT(UNOTSYNC);

    /* now do the operation locally, and propagate it out */
    code = udisk_truncate(transPtr, transPtr->seekFile, length);
    if (!code) {
	code =
	    ContactQuorum_DISK_Truncate(transPtr, 0, transPtr->seekFile,
			  		length);
    }
    if (code) {
	/* we must abort the operation */
	udisk_abort(transPtr);
	ContactQuorum_NoArguments(DISK_Abort, transPtr, 0); /* force aborts to the others */
	ERROR_EXIT(code);
    }

  error_exit:
    DBRELE(transPtr->dbase);
    return error;
}

/*!
 * \brief set a lock; all locks are released on transaction end (commit/abort)
 */
int
ubik_SetLock(struct ubik_trans *atrans, afs_int32 apos, afs_int32 alen,
	     int atype)
{
    afs_int32 code = 0, error = 0;

    if (atype == LOCKWRITE) {
	if (atrans->type == UBIK_READTRANS)
	    return UBADTYPE;
	code = ubik_Flush(atrans);
	if (code)
	    return (code);
    }

    DBHOLD(atrans->dbase);
    if (atype == LOCKREAD) {
	code = ulock_getLock(atrans, atype, 1);
	if (code)
	    ERROR_EXIT(code);
    } else {
	/* first, check that quorum is still good, and that dbase is up-to-date */
	if (!urecovery_AllBetter(atrans->dbase, atrans->flags & TRREADANY))
	    ERROR_EXIT(UNOQUORUM);
	if (!ubeacon_AmSyncSite())
	    ERROR_EXIT(UNOTSYNC);

	/* now do the operation locally, and propagate it out */
	code = ulock_getLock(atrans, atype, 1);
	if (code == 0) {
	    code = ContactQuorum_DISK_Lock(atrans, 0, 0, 1 /*unused */ ,
				 	   1 /*unused */ , LOCKWRITE);
	}
	if (code) {
	    /* we must abort the operation */
	    udisk_abort(atrans);
	    ContactQuorum_NoArguments(DISK_Abort, atrans, 0); /* force aborts to the others */
	    ERROR_EXIT(code);
	}
    }

  error_exit:
    DBRELE(atrans->dbase);
    return error;
}

/*!
 * \brief utility to wait for a version # to change
 */
int
ubik_WaitVersion(struct ubik_dbase *adatabase,
		 struct ubik_version *aversion)
{
    DBHOLD(adatabase);
    while (1) {
	/* wait until version # changes, and then return */
	if (vcmp(*aversion, adatabase->version) != 0) {
	    DBRELE(adatabase);
	    return 0;
	}
#ifdef AFS_PTHREAD_ENV
	CV_WAIT(&adatabase->version_cond, &adatabase->versionLock);
#else
	DBRELE(adatabase);
	LWP_WaitProcess(&adatabase->version);	/* same vers, just wait */
	DBHOLD(adatabase);
#endif
    }
}

/*!
 * \brief utility to get the version of the dbase a transaction is dealing with
 */
int
ubik_GetVersion(struct ubik_trans *atrans,
		struct ubik_version *avers)
{
    *avers = atrans->dbase->version;
    return 0;
}

/*!
 * \brief Facility to simplify database caching.
 * \return zero if last trans was done on the local server and was successful.
 * \return -1 means bad (NULL) argument.
 *
 * If return value is non-zero and the caller is a server caching part of the
 * Ubik database, it should invalidate that cache.
 */
static int
ubik_CacheUpdate(struct ubik_trans *atrans)
{
    if (!(atrans && atrans->dbase))
	return -1;
    return vcmp(atrans->dbase->cachedVersion, atrans->dbase->version) != 0;
}

/**
 * check and possibly update cache of ubik db.
 *
 * If the version of the cached db data is out of date, this calls (*check) to
 * update the cache. If (*check) returns success, we update the version of the
 * cached db data.
 *
 * Checking the version of the cached db data is done under a read lock;
 * updating the cache (and thus calling (*check)) is done under a write lock
 * so is guaranteed not to interfere with another thread's (*check). On
 * successful return, a read lock on the cached db data is obtained, which
 * will be released by ubik_EndTrans or ubik_AbortTrans.
 *
 * @param[in] atrans ubik transaction
 * @param[in] check  function to call to check/update cache
 * @param[in] rock   rock to pass to *check
 *
 * @return operation status
 *   @retval 0       success
 *   @retval nonzero error; cachedVersion not updated
 *
 * @post On success, application cache is read-locked, and cache data is
 *       up-to-date
 */
int
ubik_CheckCache(struct ubik_trans *atrans, ubik_updatecache_func cbf, void *rock)
{
    int ret = 0;

    if (!(atrans && atrans->dbase))
	return -1;

    ObtainReadLock(&atrans->dbase->cache_lock);

    while (ubik_CacheUpdate(atrans) != 0) {

	ReleaseReadLock(&atrans->dbase->cache_lock);
	ObtainSharedLock(&atrans->dbase->cache_lock);

	if (ubik_CacheUpdate(atrans) != 0) {

	    BoostSharedLock(&atrans->dbase->cache_lock);

	    ret = (*cbf) (atrans, rock);
	    if (ret == 0) {
		memcpy(&atrans->dbase->cachedVersion, &atrans->dbase->version,
		       sizeof(atrans->dbase->cachedVersion));
	    }
	}

	/* It would be nice if we could convert from a shared lock to a read
	 * lock... instead, just release the shared and acquire the read */
	ReleaseSharedLock(&atrans->dbase->cache_lock);

	if (ret) {
	    /* if we have an error, don't retry, and don't hold any locks */
	    return ret;
	}

	ObtainReadLock(&atrans->dbase->cache_lock);
    }

    atrans->flags |= TRCACHELOCKED;

    return 0;
}

/*!
 * "Who said anything about panicking?" snapped Arthur.
 * "This is still just the culture shock. You wait till I've settled down
 * into the situation and found my bearings. \em Then I'll start panicking!"
 * --Authur Dent
 *
 * \returns There is no return from panic.
 */
void
panic(char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    ubik_print("Ubik PANIC:\n");
    ubik_vprint(format, ap);
    va_end(ap);

    abort();
    ubik_print("BACK FROM ABORT\n");	/* shouldn't come back */
    exit(1);			/* never know, though  */
}

/*!
 * This function takes an IP addresses as its parameter. It returns the
 * the primary IP address that is on the host passed in, or 0 if not found.
 */
afs_uint32
ubikGetPrimaryInterfaceAddr(afs_uint32 addr)
{
    struct ubik_server *ts;
    int j;

    for (ts = ubik_servers; ts; ts = ts->next)
	for (j = 0; j < UBIK_MAX_INTERFACE_ADDR; j++)
	    if (ts->addr[j] == addr)
		return ts->addr[0];	/* net byte order */
    return 0;			/* if not in server database, return error */
}
