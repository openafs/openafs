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
    ("$Header: /cvs/openafs/src/ubik/ubik.c,v 1.15.2.1 2006/06/12 21:53:44 shadow Exp $");

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <sys/param.h>
#endif
#include <time.h>
#include <lock.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/cellconfig.h>

#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

#define ERROR_EXIT(code) {error=(code); goto error_exit;}

/*  This system is organized in a hierarchical set of related modules.  Modules
    at one level can only call modules at the same level or below.
    
    At the bottom level (0) we have R, RFTP, LWP and IOMGR, i.e. the basic
    operating system primitives.
    
    At the next level (1) we have

	VOTER--The module responsible for casting votes when asked.  It is also
	responsible for determining whether this server should try to become
	a synchronization site.
	
	BEACONER--The module responsible for sending keep-alives out when a
	server is actually the sync site, or trying to become a sync site.
	
	DISK--The module responsible for representing atomic transactions
	on the local disk.  It maintains a new-value only log.
	
	LOCK--The module responsible for locking byte ranges in the database file.
	
    At the next level (2) we have
	  
	RECOVERY--The module responsible for ensuring that all members of a quorum
	have the same up-to-date database after a new synchronization site is
	elected.  This module runs only on the synchronization site.
	
    At the next level (3) we have
    
	REMOTE--The module responsible for interpreting requests from the sync
	site and applying them to the database, after obtaining the appropriate
	locks.
	
    At the next level (4) we have
    
	UBIK--The module users call to perform operations on the database.
*/


/* some globals */
afs_int32 ubik_quorum = 0;
struct ubik_dbase *ubik_dbase = 0;
struct ubik_stats ubik_stats;
afs_uint32 ubik_host[UBIK_MAX_INTERFACE_ADDR];
afs_int32 ubik_epochTime = 0;
afs_int32 urecovery_state = 0;
int (*ubik_SRXSecurityProc) ();
char *ubik_SRXSecurityRock;
struct ubik_server *ubik_servers;
short ubik_callPortal;

static int BeginTrans();

struct rx_securityClass *ubik_sc[3];

/* perform an operation at a quorum, handling error conditions.  return 0 if
    all worked, otherwise mark failing server as down and return UERROR

    Note that if any server misses an update, we must wait BIGTIME seconds before
    allowing the transaction to commit, to ensure that the missing and possibly still
    functioning server times out and stop handing out old data.  This is done in the commit
    code, where we wait for a server marked down to have stayed down for BIGTIME seconds
    before we allow a transaction to commit.  A server that fails but comes back up won't give
    out old data because it is sent the sync count along with the beacon message that
    marks it as *really* up (beaconSinceDown).
*/
#define	CStampVersion	    1	/* meaning set ts->version */
afs_int32
ContactQuorum(aproc, atrans, aflags, aparm0, aparm1, aparm2, aparm3, aparm4,
	      aparm5)
     int (*aproc) ();
     int aflags;
     register struct ubik_trans *atrans;
     long aparm0, aparm1, aparm2, aparm3, aparm4, aparm5;
{
    register struct ubik_server *ts;
    register afs_int32 code;
    afs_int32 rcode, okcalls;

    rcode = 0;
    okcalls = 0;
    for (ts = ubik_servers; ts; ts = ts->next) {
	/* for each server */
	if (!ts->up || !ts->currentDB) {
	    ts->currentDB = 0;	/* db is no longer current; we just missed an update */
	    continue;		/* not up-to-date, don't bother */
	}
	code =
	    (*aproc) (ts->disk_rxcid, &atrans->tid, aparm0, aparm1, aparm2,
		      aparm3, aparm4, aparm5);
	if ((aproc == DISK_WriteV) && (code <= -450) && (code > -500)) {
	    /* An RPC interface mismatch (as defined in comerr/error_msg.c).
	     * Un-bulk the entries and do individual DISK_Write calls
	     * instead of DISK_WriteV.
	     */
	    iovec_wrt *iovec_infoP = (iovec_wrt *) aparm0;
	    iovec_buf *iovec_dataP = (iovec_buf *) aparm1;
	    struct ubik_iovec *iovec =
		(struct ubik_iovec *)iovec_infoP->iovec_wrt_val;
	    char *iobuf = (char *)iovec_dataP->iovec_buf_val;
	    bulkdata tcbs;
	    afs_int32 i, offset;

	    for (i = 0, offset = 0; i < iovec_infoP->iovec_wrt_len; i++) {
		/* Sanity check for going off end of buffer */
		if ((offset + iovec[i].length) > iovec_dataP->iovec_buf_len) {
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
	return rcode;
}

/* This routine initializes the ubik system for a set of servers.  It returns 0 for success, or an error code on failure.  The set of servers is specified by serverList; nServers gives the number of entries in this array.  Finally, dbase is the returned structure representing this instance of a ubik; it is passed to various calls below.  The variable pathName provides an initial prefix used for naming storage files used by this system.  It should perhaps be generalized to a low-level disk interface providing read, write, file enumeration and sync operations.

    Note that the host named by myHost should not also be listed in serverList.
*/

int
ubik_ServerInitCommon(afs_int32 myHost, short myPort,
		      struct afsconf_cell *info, char clones[],
		      afs_int32 serverList[], char *pathName,
		      struct ubik_dbase **dbase)
{
    register struct ubik_dbase *tdb;
    register afs_int32 code;
    PROCESS junk;
    afs_int32 secIndex;
    struct rx_securityClass *secClass;

    struct rx_service *tservice;
    extern int VOTE_ExecuteRequest(), DISK_ExecuteRequest();
    extern void rx_ServerProc();
    extern int rx_stackSize;

    initialize_U_error_table();

    tdb = (struct ubik_dbase *)malloc(sizeof(struct ubik_dbase));
    tdb->pathName = (char *)malloc(strlen(pathName) + 1);
    strcpy(tdb->pathName, pathName);
    tdb->activeTrans = (struct ubik_trans *)0;
    memset(&tdb->version, 0, sizeof(struct ubik_version));
    memset(&tdb->cachedVersion, 0, sizeof(struct ubik_version));
    Lock_Init(&tdb->versionLock);
    tdb->flags = 0;
    tdb->read = uphys_read;
    tdb->write = uphys_write;
    tdb->truncate = uphys_truncate;
    tdb->open = 0;		/* this function isn't used any more */
    tdb->sync = uphys_sync;
    tdb->stat = uphys_stat;
    tdb->getlabel = uphys_getlabel;
    tdb->setlabel = uphys_setlabel;
    tdb->getnfiles = uphys_getnfiles;
    tdb->readers = 0;
    tdb->tidCounter = tdb->writeTidCounter = 0;
    *dbase = tdb;
    ubik_dbase = tdb;		/* for now, only one db per server; can fix later when we have names for the other dbases */

    /* initialize RX */
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
    code = rx_Init(myPort);
    if (code < 0)
	return code;
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
    LWP_CreateProcess(rx_ServerProc, rx_stackSize, RX_PROCESS_PRIORITY,
		      (void *)0, "rx_ServerProc", &junk);

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
    code = LWP_CreateProcess(ubeacon_Interact, 16384 /*8192 */ ,
			     LWP_MAX_PRIORITY - 1, (void *)0, "beacon",
			     &junk);
    if (code)
	return code;
    code = LWP_CreateProcess(urecovery_Interact, 16384 /*8192 */ ,
			     LWP_MAX_PRIORITY - 1, (void *)0, "recovery",
			     &junk);
    return code;
}

int
ubik_ServerInitByInfo(afs_int32 myHost, short myPort,
		      struct afsconf_cell *info, char clones[],
		      char *pathName, struct ubik_dbase **dbase)
{
    afs_int32 code;

    code =
	ubik_ServerInitCommon(myHost, myPort, info, clones, 0, pathName,
			      dbase);
    return code;
}

int
ubik_ServerInit(afs_int32 myHost, short myPort, afs_int32 serverList[],
		char *pathName, struct ubik_dbase **dbase)
{
    afs_int32 code;

    code =
	ubik_ServerInitCommon(myHost, myPort, (struct afsconf_cell *)0, 0,
			      serverList, pathName, dbase);
    return code;
}

/*  This routine begins a read or write transaction on the transaction
    identified by transPtr, in the dbase named by dbase.  An open mode of
    ubik_READTRANS identifies this as a read transaction, while a mode of
    ubik_WRITETRANS identifies this as a write transaction.  transPtr 
    is set to the returned transaction control block. The readAny flag is
    set to 0 or 1 by the wrapper functions ubik_BeginTrans() or 
    ubik_BeginTransReadAny() below.

    We can only begin transaction when we have an up-to-date database.
*/

static int
BeginTrans(register struct ubik_dbase *dbase, afs_int32 transMode,
	   struct ubik_trans **transPtr, int readAny)
{
    struct ubik_trans *jt;
    register struct ubik_trans *tt;
    register afs_int32 code;
#if defined(UBIK_PAUSE)
    int count;
#endif /* UBIK_PAUSE */

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
	    IOMGR_Sleep(2);
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
	    DBRELE(dbase);
	    LWP_WaitProcess(&dbase->flags);
	    DBHOLD(dbase);
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
    if (readAny)
	tt->flags |= TRREADANY;
    /* label trans and dbase with new tid */
    tt->tid.epoch = ubik_epochTime;
    /* bump by two, since tidCounter+1 means trans id'd by tidCounter has finished */
    tt->tid.counter = (dbase->tidCounter += 2);

    if (transMode == UBIK_WRITETRANS) {
	/* for a write trans, we have to keep track of the write tid counter too */
#if defined(UBIK_PAUSE)
	dbase->writeTidCounter = tt->tid.counter;
#else
	dbase->writeTidCounter += 2;
#endif /* UBIK_PAUSE */

	/* next try to start transaction on appropriate number of machines */
	code = ContactQuorum(DISK_Begin, tt, 0);
	if (code) {
	    /* we must abort the operation */
	    udisk_abort(tt);
	    ContactQuorum(DISK_Abort, tt, 0);	/* force aborts to the others */
	    udisk_end(tt);
	    DBRELE(dbase);
	    return code;
	}
    }

    *transPtr = tt;
    DBRELE(dbase);
    return 0;
}

int
ubik_BeginTrans(register struct ubik_dbase *dbase, afs_int32 transMode,
		struct ubik_trans **transPtr)
{
    return BeginTrans(dbase, transMode, transPtr, 0);
}

int
ubik_BeginTransReadAny(register struct ubik_dbase *dbase, afs_int32 transMode,
		       struct ubik_trans **transPtr)
{
    return BeginTrans(dbase, transMode, transPtr, 1);
}

/* this routine ends a read or write transaction by aborting it */
int
ubik_AbortTrans(register struct ubik_trans *transPtr)
{
    register afs_int32 code;
    afs_int32 code2;
    register struct ubik_dbase *dbase;

    dbase = transPtr->dbase;
    DBHOLD(dbase);
    memset(&dbase->cachedVersion, 0, sizeof(struct ubik_version));
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
    code = ContactQuorum(DISK_Abort, transPtr, 0);
    code2 = udisk_abort(transPtr);
    udisk_end(transPtr);
    DBRELE(dbase);
    return (code ? code : code2);
}

/* This routine ends a read or write transaction on the open transaction identified by transPtr.  It returns an error code. */
int
ubik_EndTrans(register struct ubik_trans *transPtr)
{
    register afs_int32 code;
    struct timeval tv;
    afs_int32 realStart;
    register struct ubik_server *ts;
    afs_int32 now;
    register struct ubik_dbase *dbase;

    if (transPtr->type == UBIK_WRITETRANS) {
	code = ubik_Flush(transPtr);
	if (code) {
	    ubik_AbortTrans(transPtr);
	    return (code);
	}
    }

    dbase = transPtr->dbase;
    DBHOLD(dbase);
    memset(&dbase->cachedVersion, 0, sizeof(struct ubik_version));

    /* give up if no longer current */
    if (!urecovery_AllBetter(dbase, transPtr->flags & TRREADANY)) {
	udisk_abort(transPtr);
	udisk_end(transPtr);
	DBRELE(dbase);
	return UNOQUORUM;
    }

    if (transPtr->type == UBIK_READTRANS) {	/* reads are easy */
	code = udisk_commit(transPtr);
	if (code == 0)
	    goto success;	/* update cachedVersion correctly */
	udisk_end(transPtr);
	DBRELE(dbase);
	return code;
    }

    if (!ubeacon_AmSyncSite()) {	/* no longer sync site */
	udisk_abort(transPtr);
	udisk_end(transPtr);
	DBRELE(dbase);
	return UNOTSYNC;
    }

    /* now it is safe to do commit */
    code = udisk_commit(transPtr);
    if (code == 0)
	code = ContactQuorum(DISK_Commit, transPtr, CStampVersion);
    if (code) {
	/* failed to commit, so must return failure.  Try to clear locks first, just for fun
	 * Note that we don't know if this transaction will eventually commit at this point.
	 * If it made it to a site that will be present in the next quorum, we win, otherwise
	 * we lose.  If we contact a majority of sites, then we won't be here: contacting
	 * a majority guarantees commit, since it guarantees that one dude will be a
	 * member of the next quorum. */
	ContactQuorum(DISK_ReleaseLocks, transPtr, 0);
	udisk_end(transPtr);
	DBRELE(dbase);
	return code;
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
		IOMGR_Select(0, 0, 0, 0, &tv);	/* poll, should we wait on something? */
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
    ContactQuorum(DISK_ReleaseLocks, transPtr, 0);

  success:
    udisk_end(transPtr);
    /* update version on successful EndTrans */
    memcpy(&dbase->cachedVersion, &dbase->version,
	   sizeof(struct ubik_version));

    DBRELE(dbase);
    return 0;
}

/* This routine reads length bytes into buffer from the current position in the database.  The file pointer is updated appropriately (by adding the number of bytes actually transferred), and the length actually transferred is stored in the long integer pointed to by length.  Note that *length is an INOUT parameter: at the start it represents the size of the buffer, and when done, it contains the number of bytes actually transferred.  A short read returns zero for an error code. */

int
ubik_Read(register struct ubik_trans *transPtr, char *buffer,
	  afs_int32 length)
{
    register afs_int32 code;

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

/* This routine will flush the io data in the iovec structures. It first
 * flushes to the local disk and then uses ContactQuorum to write it to 
 * the other servers.
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
	ContactQuorum(DISK_WriteV, transPtr, 0, &transPtr->iovec_info,
		      &transPtr->iovec_data);
    if (code) {
	udisk_abort(transPtr);
	ContactQuorum(DISK_Abort, transPtr, 0);	/* force aborts to the others */
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
ubik_Write(register struct ubik_trans *transPtr, char *buffer,
	   afs_int32 length)
{
    struct ubik_iovec *iovec;
    afs_int32 code, error = 0;
    afs_int32 pos, len, size;

    if (transPtr->type != UBIK_WRITETRANS)
	return UBADTYPE;
    if (!length)
	return 0;

    if (length > IOVEC_MAXBUF) {
	for (pos = 0, len = length; len > 0; len -= size, pos += size) {
	    size = ((len < IOVEC_MAXBUF) ? len : IOVEC_MAXBUF);
	    code = ubik_Write(transPtr, &buffer[pos], size);
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

/* This sets the file pointer associated with the current transaction to the appropriate file and byte position.  Unlike Unix files, a transaction is labelled by both a file number (fileid) and a byte position relative to the specified file (position). */

int
ubik_Seek(register struct ubik_trans *transPtr, afs_int32 fileid,
	  afs_int32 position)
{
    register afs_int32 code;

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

/* This call returns the file pointer associated with the specified transaction in fileid and position. */

int
ubik_Tell(register struct ubik_trans *transPtr, afs_int32 * fileid,
	  afs_int32 * position)
{
    DBHOLD(transPtr->dbase);
    *fileid = transPtr->seekFile;
    *position = transPtr->seekPos;
    DBRELE(transPtr->dbase);
    return 0;
}

/* This sets the file size for the currently-selected file to length bytes, if length is less than the file's current size. */

int
ubik_Truncate(register struct ubik_trans *transPtr, afs_int32 length)
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
	    ContactQuorum(DISK_Truncate, transPtr, 0, transPtr->seekFile,
			  length);
    }
    if (code) {
	/* we must abort the operation */
	udisk_abort(transPtr);
	ContactQuorum(DISK_Abort, transPtr, 0);	/* force aborts to the others */
	ERROR_EXIT(code);
    }

  error_exit:
    DBRELE(transPtr->dbase);
    return error;
}

/* set a lock; all locks are released on transaction end (commit/abort) */
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
	    code = ContactQuorum(DISK_Lock, atrans, 0, 0, 1 /*unused */ ,
				 1 /*unused */ , LOCKWRITE);
	}
	if (code) {
	    /* we must abort the operation */
	    udisk_abort(atrans);
	    ContactQuorum(DISK_Abort, atrans, 0);	/* force aborts to the others */
	    ERROR_EXIT(code);
	}
    }

  error_exit:
    DBRELE(atrans->dbase);
    return error;
}

/* utility to wait for a version # to change */
int
ubik_WaitVersion(register struct ubik_dbase *adatabase,
		 register struct ubik_version *aversion)
{
    while (1) {
	/* wait until version # changes, and then return */
	if (vcmp(*aversion, adatabase->version) != 0)
	    return 0;
	LWP_WaitProcess(&adatabase->version);	/* same vers, just wait */
    }
}

/* utility to get the version of the dbase a transaction is dealing with */
int
ubik_GetVersion(register struct ubik_trans *atrans,
		register struct ubik_version *avers)
{
    *avers = atrans->dbase->version;
    return 0;
}

/* Facility to simplify database caching.  Returns zero if last trans was done
   on the local server and was successful.  If return value is non-zero and the
   caller is a server caching part of the Ubik database, it should invalidate
   that cache.  A return value of -1 means bad (NULL) argument. */

int
ubik_CacheUpdate(register struct ubik_trans *atrans)
{
    if (!(atrans && atrans->dbase))
	return -1;
    return vcmp(atrans->dbase->cachedVersion, atrans->dbase->version) != 0;
}

int
panic(char *a, char *b, char *c, char *d)
{
    ubik_print("Ubik PANIC: ");
    ubik_print(a, b, c, d);
    abort();
    ubik_print("BACK FROM ABORT\n");	/* shouldn't come back */
    exit(1);			/* never know, though  */
}

/*
** This functions takes an IP addresses as its parameter. It returns the
** the primary IP address that is on the host passed in.
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
