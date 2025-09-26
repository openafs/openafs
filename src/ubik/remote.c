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

#include <roken.h>

#include <assert.h>

#include <afs/opr.h>
#ifdef AFS_PTHREAD_ENV
# include <opr/lock.h>
#else
# include <opr/lockstub.h>
#endif

#include <afs/afs_lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/afsutil.h>

#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

static void printServerInfo(void);

/*! \file
 * routines for handling requests remotely-submitted by the sync site.  These are
 * only write transactions (we don't propagate read trans), and there is at most one
 * write transaction extant at any one time.
 */

struct ubik_trans *ubik_currentTrans = 0;



/* the rest of these guys handle remote execution of write
 * transactions: this is the code executed on the other servers when a
 * sync site is executing a write transaction.
 */
afs_int32
SDISK_Begin(struct rx_call *rxcall, struct ubik_tid *atid)
{
    afs_int32 code;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    DBHOLD(ubik_dbase);
    if (urecovery_AllBetter(ubik_dbase, 0) == 0) {
	code = UNOQUORUM;
	goto out;
    }
    urecovery_CheckTid(atid, 1);
    code = udisk_begin(ubik_dbase, UBIK_WRITETRANS, &ubik_currentTrans);
    if (!code && ubik_currentTrans) {
	/* label this trans with the right trans id */
	ubik_currentTrans->tid.epoch = atid->epoch;
	ubik_currentTrans->tid.counter = atid->counter;
    }
  out:
    DBRELE(ubik_dbase);
    return code;
}


afs_int32
SDISK_Commit(struct rx_call *rxcall, struct ubik_tid *atid)
{
    afs_int32 code;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    ObtainWriteLock(&ubik_dbase->cache_lock);
    DBHOLD(ubik_dbase);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }
    /*
     * sanity check to make sure only write trans appear here
     */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	code = UBADTYPE;
	goto done;
    }

    urecovery_CheckTid(atid, 0);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }

    code = udisk_commit(ubik_currentTrans);
    if (code == 0) {
	/* sync site should now match */
	uvote_set_dbVersion(ubik_dbase->version);
    }
done:
    DBRELE(ubik_dbase);
    ReleaseWriteLock(&ubik_dbase->cache_lock);
    return code;
}

afs_int32
SDISK_ReleaseLocks(struct rx_call *rxcall, struct ubik_tid *atid)
{
    afs_int32 code;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }

    DBHOLD(ubik_dbase);

    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	code = UBADTYPE;
	goto done;
    }

    urecovery_CheckTid(atid, 0);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }

    /* If the thread is not waiting for lock - ok to end it */
    if (ubik_currentTrans->locktype != LOCKWAIT) {
	udisk_end(ubik_currentTrans);
    }
    ubik_currentTrans = (struct ubik_trans *)0;
done:
    DBRELE(ubik_dbase);
    return code;
}

afs_int32
SDISK_Abort(struct rx_call *rxcall, struct ubik_tid *atid)
{
    afs_int32 code;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    DBHOLD(ubik_dbase);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }
    /* sanity check to make sure only write trans appear here  */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	code = UBADTYPE;
	goto done;
    }

    urecovery_CheckTid(atid, 0);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }

    code = udisk_abort(ubik_currentTrans);
    /* If the thread is not waiting for lock - ok to end it */
    if (ubik_currentTrans->locktype != LOCKWAIT) {
	udisk_end(ubik_currentTrans);
    }
    ubik_currentTrans = (struct ubik_trans *)0;
done:
    DBRELE(ubik_dbase);
    return code;
}

/* apos and alen are not used */
afs_int32
SDISK_Lock(struct rx_call *rxcall, struct ubik_tid *atid,
	   afs_int32 afile, afs_int32 apos, afs_int32 alen, afs_int32 atype)
{
    afs_int32 code;
    struct ubik_trans *ubik_thisTrans;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    DBHOLD(ubik_dbase);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	code = UBADTYPE;
	goto done;
    }
    if (alen != 1) {
	code = UBADLOCK;
	goto done;
    }
    urecovery_CheckTid(atid, 0);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }

    ubik_thisTrans = ubik_currentTrans;
    code = ulock_getLock(ubik_currentTrans, atype, 1);

    /* While waiting, the transaction may have been ended/
     * aborted from under us (urecovery_CheckTid). In that
     * case, end the transaction here.
     */
    if (!code && (ubik_currentTrans != ubik_thisTrans)) {
	udisk_end(ubik_thisTrans);
	code = USYNC;
    }
done:
    DBRELE(ubik_dbase);
    return code;
}

/*!
 * \brief Write a vector of data
 */
afs_int32
SDISK_WriteV(struct rx_call *rxcall, struct ubik_tid *atid,
	     iovec_wrt *io_vector, iovec_buf *io_buffer)
{
    afs_int32 code, i, offset;
    struct ubik_iovec *iovec;
    char *iobuf;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    DBHOLD(ubik_dbase);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	code = UBADTYPE;
	goto done;
    }

    urecovery_CheckTid(atid, 0);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }

    iovec = (struct ubik_iovec *)io_vector->iovec_wrt_val;
    iobuf = (char *)io_buffer->iovec_buf_val;
    for (i = 0, offset = 0; i < io_vector->iovec_wrt_len; i++) {
	/* Sanity check for going off end of buffer */
	if ((offset + iovec[i].length) > io_buffer->iovec_buf_len) {
	    code = UINTERNAL;
	} else {
	    code =
		udisk_write(ubik_currentTrans, iovec[i].file, &iobuf[offset],
			    iovec[i].position, iovec[i].length);
	}
	if (code)
	    break;

	offset += iovec[i].length;
    }
done:
    DBRELE(ubik_dbase);
    return code;
}

afs_int32
SDISK_Write(struct rx_call *rxcall, struct ubik_tid *atid,
	    afs_int32 afile, afs_int32 apos, bulkdata *adata)
{
    afs_int32 code;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    DBHOLD(ubik_dbase);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	code = UBADTYPE;
	goto done;
    }

    urecovery_CheckTid(atid, 0);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }
    code =
	udisk_write(ubik_currentTrans, afile, adata->bulkdata_val, apos,
		    adata->bulkdata_len);
done:
    DBRELE(ubik_dbase);
    return code;
}

afs_int32
SDISK_Truncate(struct rx_call *rxcall, struct ubik_tid *atid,
	       afs_int32 afile, afs_int32 alen)
{
    afs_int32 code;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    DBHOLD(ubik_dbase);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	code = UBADTYPE;
	goto done;
    }

    urecovery_CheckTid(atid, 0);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }
    code = udisk_truncate(ubik_currentTrans, afile, alen);
done:
    DBRELE(ubik_dbase);
    return code;
}

afs_int32
SDISK_GetVersion(struct rx_call *rxcall,
		 struct ubik_version *aversion)
{
    afs_int32 code;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }

    /*
     * If we are the sync site, recovery shouldn't be running on any
     * other site. We shouldn't be getting this RPC as long as we are
     * the sync site.  To prevent any unforseen activity, we should
     * reject this RPC until we have recognized that we are not the
     * sync site anymore, and/or if we have any pending WRITE
     * transactions that have to complete. This way we can be assured
     * that this RPC would not block any pending transactions that
     * should either fail or pass. If we have recognized the fact that
     * we are not the sync site any more, all write transactions would
     * fail with UNOQUORUM anyway.
     */
    DBHOLD(ubik_dbase);
    if (ubeacon_AmSyncSite()) {
	DBRELE(ubik_dbase);
	return UDEADLOCK;
    }

    code = (*ubik_dbase->getlabel) (ubik_dbase, 0, aversion);
    DBRELE(ubik_dbase);
    if (code) {
	/* tell other side there's no dbase */
	aversion->epoch = 0;
	aversion->counter = 0;
    }
    return 0;
}

afs_int32
SDISK_GetFile(struct rx_call *rxcall, afs_int32 file,
	      struct ubik_version *version)
{
    afs_int32 code;
    struct ubik_dbase *dbase;
    afs_int32 offset;
    struct ubik_stat ubikstat;
    char tbuffer[256];
    afs_int32 tlen;
    afs_int32 length;
    struct rx_peer *tpeer;
    struct rx_connection *tconn;
    afs_uint32 otherHost = 0;
    char hoststr[16];

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }

    tconn = rx_ConnectionOf(rxcall);
    tpeer = rx_PeerOf(tconn);
    otherHost = ubikGetPrimaryInterfaceAddr(rx_HostOf(tpeer));
    ViceLog(0, ("Ubik: Synchronize database: send (via GetFile) "
		"to server %s begin\n",
	       afs_inet_ntoa_r(otherHost, hoststr)));

    dbase = ubik_dbase;
    DBHOLD(dbase);
    code = (*dbase->stat) (dbase, file, &ubikstat);
    if (code < 0) {
	ViceLog(0, ("database stat() error:%d\n", code));
	goto failed;
    }
    length = ubikstat.size;
    tlen = htonl(length);
    code = rx_Write(rxcall, (char *)&tlen, sizeof(afs_int32));
    if (code != sizeof(afs_int32)) {
	ViceLog(0, ("Rx-write length error=%d\n", code));
	code = BULK_ERROR;
	goto failed;
    }
    offset = 0;
    while (length > 0) {
	tlen = (length > sizeof(tbuffer) ? sizeof(tbuffer) : length);
	code = (*dbase->read) (dbase, file, tbuffer, offset, tlen);
	if (code != tlen) {
	    ViceLog(0, ("read failed error=%d\n", code));
	    code = UIOERROR;
	    goto failed;
	}
	code = rx_Write(rxcall, tbuffer, tlen);
	if (code != tlen) {
	    ViceLog(0, ("Rx-write data error=%d\n", code));
	    code = BULK_ERROR;
	    goto failed;
	}
	length -= tlen;
	offset += tlen;
    }
    code = (*dbase->getlabel) (dbase, file, version);	/* return the dbase, too */
    if (code)
	ViceLog(0, ("getlabel error=%d\n", code));

 failed:
    DBRELE(dbase);
    if (code) {
	ViceLog(0,
	    ("Ubik: Synchronize database: send (via GetFile) to "
	     "server %s failed (error = %d)\n",
	     afs_inet_ntoa_r(otherHost, hoststr), code));
    } else {
	ViceLog(0,
	    ("Ubik: Synchronize database: send (via GetFile) to "
	     "server %s complete, version: %d.%d\n",
	    afs_inet_ntoa_r(otherHost, hoststr), version->epoch, version->counter));
    }
    return code;
}

afs_int32
SDISK_SendFile(struct rx_call *rxcall, afs_int32 file,
	       afs_int32 length, struct ubik_version *avers)
{
    afs_int32 code;
    struct ubik_dbase *dbase = NULL;
    char tbuffer[1024];
    struct ubik_version tversion;
    int tlen;
    struct rx_peer *tpeer;
    struct rx_connection *tconn;
    afs_uint32 syncHost = 0;
    afs_uint32 otherHost = 0;
    char hoststr[16];
    char pbuffer[1028];
    int fd = -1;
    afs_int32 epoch = 0;
#if !defined(AFS_PTHREAD_ENV)
    afs_int32 pass = 0;
#endif
    /* send the file back to the requester */

    dbase = ubik_dbase;
    pbuffer[0] = '\0';

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }

    /* next, we do a sanity check to see if the guy sending us the database is
     * the guy we think is the sync site.  It turns out that we might not have
     * decided yet that someone's the sync site, but they could have enough
     * votes from others to be sync site anyway, and could send us the database
     * in advance of getting our votes.  This is fine, what we're really trying
     * to check is that some authenticated bogon isn't sending a random database
     * into another configuration.  This could happen on a bad configuration
     * screwup.  Thus, we only object if we're sure we know who the sync site
     * is, and it ain't the guy talking to us.
     */
    syncHost = uvote_GetSyncSite();
    tconn = rx_ConnectionOf(rxcall);
    tpeer = rx_PeerOf(tconn);
    otherHost = ubikGetPrimaryInterfaceAddr(rx_HostOf(tpeer));
    if (syncHost && syncHost != otherHost) {
	/* we *know* this is the wrong guy */
        char sync_hoststr[16];
	ViceLog(0,
	    ("Ubik: Refusing synchronization with server %s since it is not the sync-site (%s).\n",
	     afs_inet_ntoa_r(otherHost, hoststr),
	     afs_inet_ntoa_r(syncHost, sync_hoststr)));
	return USYNC;
    }

    DBHOLD(dbase);

    /* abort any active trans that may scribble over the database */
    urecovery_AbortAll(dbase);

    ViceLog(0, ("Ubik: Synchronize database: receive (via SendFile) from server %s begin\n",
	       afs_inet_ntoa_r(otherHost, hoststr)));

    UBIK_VERSION_LOCK;
    epoch = tversion.epoch = 0;		/* start off by labelling in-transit db as invalid */
    (*dbase->setlabel) (dbase, file, &tversion);	/* setlabel does sync */
    snprintf(pbuffer, sizeof(pbuffer), "%s.DB%s%d.TMP",
	     ubik_dbase->pathName, (file<0)?"SYS":"",
	     (file<0)?-file:file);
    fd = open(pbuffer, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (fd < 0) {
	code = errno;
	ViceLog(0, ("Open error=%d\n", code));
	goto failed_locked;
    }
    code = lseek(fd, HDRSIZE, 0);
    if (code != HDRSIZE) {
	ViceLog(0, ("lseek error=%d\n", code));
	close(fd);
	goto failed_locked;
    }
    memcpy(&ubik_dbase->version, &tversion, sizeof(struct ubik_version));
    UBIK_VERSION_UNLOCK;
    while (length > 0) {
	tlen = (length > sizeof(tbuffer) ? sizeof(tbuffer) : length);
#if !defined(AFS_PTHREAD_ENV)
	if (pass % 4 == 0)
	    IOMGR_Poll();
	pass++;
#endif
	code = rx_Read(rxcall, tbuffer, tlen);
	if (code != tlen) {
	    ViceLog(0, ("Rx-read length error=%d\n", code));
	    code = BULK_ERROR;
	    close(fd);
	    goto failed;
	}
	code = write(fd, tbuffer, tlen);
	if (code != tlen) {
	    ViceLog(0, ("write failed tlen=%d, error=%d\n", tlen, code));
	    code = UIOERROR;
	    close(fd);
	    goto failed;
	}
	length -= tlen;
    }
    code = close(fd);
    if (code) {
	ViceLog(0, ("close failed error=%d\n", code));
	goto failed;
    }

    /* sync data first, then write label and resync (resync done by setlabel call).
     * This way, good label is only on good database. */
    snprintf(tbuffer, sizeof(tbuffer), "%s.DB%s%d",
	     ubik_dbase->pathName, (file<0)?"SYS":"", (file<0)?-file:file);
#ifdef AFS_NT40_ENV
    snprintf(pbuffer, sizeof(pbuffer), "%s.DB%s%d.OLD",
	     ubik_dbase->pathName, (file<0)?"SYS":"", (file<0)?-file:file);
    code = unlink(pbuffer);
    if (!code)
	code = rename(tbuffer, pbuffer);
    snprintf(pbuffer, sizeof(pbuffer), "%s.DB%s%d.TMP",
	     ubik_dbase->pathName, (file<0)?"SYS":"", (file<0)?-file:file);
#endif
    if (!code)
	code = rename(pbuffer, tbuffer);
    UBIK_VERSION_LOCK;
    if (!code) {
	(*ubik_dbase->open) (ubik_dbase, file);
	code = (*ubik_dbase->setlabel) (dbase, file, avers);
    }
#ifdef AFS_NT40_ENV
    snprintf(pbuffer, sizeof(pbuffer), "%s.DB%s%d.OLD",
	     ubik_dbase->pathName, (file<0)?"SYS":"", (file<0)?-file:file);
    unlink(pbuffer);
#endif
    memcpy(&ubik_dbase->version, avers, sizeof(struct ubik_version));
    udisk_Invalidate(dbase, file);	/* new dbase, flush disk buffers */

failed_locked:
    UBIK_VERSION_UNLOCK;

failed:
    if (code) {
	if (pbuffer[0] != '\0')
	    unlink(pbuffer);

	/* Failed to sync. Allow reads again for now. */
	if (dbase != NULL) {
	    UBIK_VERSION_LOCK;
	    tversion.epoch = epoch;
	    (*dbase->setlabel) (dbase, file, &tversion);
	    UBIK_VERSION_UNLOCK;
	}
	ViceLog(0,
	    ("Ubik: Synchronize database: receive (via SendFile) from "
	     "server %s failed (error = %d)\n",
	    afs_inet_ntoa_r(otherHost, hoststr), code));
    } else {
	uvote_set_dbVersion(*avers);
	ViceLog(0,
	    ("Ubik: Synchronize database: receive (via SendFile) from "
	     "server %s complete, version: %d.%d\n",
	    afs_inet_ntoa_r(otherHost, hoststr), avers->epoch, avers->counter));
    }
    DBRELE(dbase);
    return code;
}


afs_int32
SDISK_Probe(struct rx_call *rxcall)
{
    return 0;
}

/*!
 * \brief Update remote machines addresses in my server list
 *
 * Send back my addresses to caller of this RPC
 * \return zero on success, else 1.
 */
afs_int32
SDISK_UpdateInterfaceAddr(struct rx_call *rxcall,
			  UbikInterfaceAddr *inAddr,
			  UbikInterfaceAddr *outAddr)
{
    struct ubik_server *ts, *tmp;
    afs_uint32 remoteAddr;	/* in net byte order */
    int i, j, found = 0, probableMatch = 0;
    char hoststr[16];

    UBIK_ADDR_LOCK;
    /* copy the output parameters */
    for (i = 0; i < UBIK_MAX_INTERFACE_ADDR; i++)
	outAddr->hostAddr[i] = ntohl(ubik_host[i]);

    remoteAddr = htonl(inAddr->hostAddr[0]);
    for (ts = ubik_servers; ts; ts = ts->next)
	if (ts->addr[0] == remoteAddr) {	/* both in net byte order */
	    probableMatch = 1;
	    break;
	}

    if (probableMatch) {
	/* verify that all addresses in the incoming RPC are
	 ** not part of other server entries in my CellServDB
	 */
	for (i = 0; !found && (i < UBIK_MAX_INTERFACE_ADDR)
	     && inAddr->hostAddr[i]; i++) {
	    remoteAddr = htonl(inAddr->hostAddr[i]);
	    for (tmp = ubik_servers; (!found && tmp); tmp = tmp->next) {
		if (ts == tmp)	/* this is my server */
		    continue;
		for (j = 0; (j < UBIK_MAX_INTERFACE_ADDR) && tmp->addr[j];
		     j++)
		    if (remoteAddr == tmp->addr[j]) {
			found = 1;
			break;
		    }
	    }
	}
    }

    /* if (probableMatch) */
    /* inconsistent addresses in CellServDB */
    if (!probableMatch || found) {
	ViceLog(0, ("Inconsistent Cell Info from server:\n"));
	for (i = 0; i < UBIK_MAX_INTERFACE_ADDR && inAddr->hostAddr[i]; i++)
	    ViceLog(0, ("... %s\n", afs_inet_ntoa_r(htonl(inAddr->hostAddr[i]), hoststr)));
	fflush(stdout);
	fflush(stderr);
	printServerInfo();
	UBIK_ADDR_UNLOCK;
	return UBADHOST;
    }

    /* update our data structures */
    for (i = 1; i < UBIK_MAX_INTERFACE_ADDR; i++)
	ts->addr[i] = htonl(inAddr->hostAddr[i]);

    ViceLog(0, ("ubik: A Remote Server has addresses:\n"));
    for (i = 0; i < UBIK_MAX_INTERFACE_ADDR && ts->addr[i]; i++)
	ViceLog(0, ("... %s\n", afs_inet_ntoa_r(ts->addr[i], hoststr)));

    UBIK_ADDR_UNLOCK;

    /*
     * The most likely cause of a DISK_UpdateInterfaceAddr RPC
     * is because the server was restarted.  Reset its state
     * so that no DISK_Begin RPCs will be issued until the
     * known database version is current.
     */
    UBIK_BEACON_LOCK;
    ts->beaconSinceDown = 0;
    ts->currentDB = 0;
    urecovery_LostServer(ts);
    UBIK_BEACON_UNLOCK;
    return 0;
}

static void
printServerInfo(void)
{
    struct ubik_server *ts;
    int i, j = 1;
    char hoststr[16];

    ViceLog(0, ("Local CellServDB:\n"));
    for (ts = ubik_servers; ts; ts = ts->next, j++) {
	ViceLog(0, ("  Server %d:\n", j));
	for (i = 0; (i < UBIK_MAX_INTERFACE_ADDR) && ts->addr[i]; i++)
	    ViceLog(0, ("  ... %s\n", afs_inet_ntoa_r(ts->addr[i], hoststr)));
    }
}

afs_int32
SDISK_SetVersion(struct rx_call *rxcall, struct ubik_tid *atid,
		 struct ubik_version *oldversionp,
		 struct ubik_version *newversionp)
{
    afs_int32 code = 0;

    if ((code = ubik_CheckAuth(rxcall))) {
	return (code);
    }
    DBHOLD(ubik_dbase);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	code = UBADTYPE;
	goto done;
    }

    /* Should not get this for the sync site */
    if (ubeacon_AmSyncSite()) {
	code = UDEADLOCK;
	goto done;
    }

    urecovery_CheckTid(atid, 0);
    if (!ubik_currentTrans) {
	code = USYNC;
	goto done;
    }

    /* Set the label if our version matches the sync-site's. Also set the label
     * if our on-disk version matches the old version, and our view of the
     * sync-site's version matches the new version. This suggests that
     * ubik_dbVersion was updated while the sync-site was setting the new
     * version, and it already told us via VOTE_Beacon. */
    if (uvote_eq_dbVersion(*oldversionp)
	|| (uvote_eq_dbVersion(*newversionp)
	    && vcmp(ubik_dbase->version, *oldversionp) == 0)) {
	UBIK_VERSION_LOCK;
	code = (*ubik_dbase->setlabel) (ubik_dbase, 0, newversionp);
	if (!code) {
	    ubik_dbase->version = *newversionp;
	    uvote_set_dbVersion(*newversionp);
	}
	UBIK_VERSION_UNLOCK;
    } else {
	code = USYNC;
    }
done:
    DBRELE(ubik_dbase);
    return code;
}
