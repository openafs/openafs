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

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/afsutil.h>

#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"
int (*ubik_CheckRXSecurityProc) ();
char *ubik_CheckRXSecurityRock;
void printServerInfo();

/* routines for handling requests remotely-submitted by the sync site.  These are
    only write transactions (we don't propagate read trans), and there is at most one
    write transaction extant at any one time.
*/

struct ubik_trans *ubik_currentTrans = 0;


ubik_CheckAuth(acall)
     register struct rx_call *acall;
{
    register afs_int32 code;
    if (ubik_CheckRXSecurityProc) {
	code = (*ubik_CheckRXSecurityProc) (ubik_CheckRXSecurityRock, acall);
	return code;
    } else
	return 0;
}

/* the rest of these guys handle remote execution of write
 * transactions: this is the code executed on the other servers when a
 * sync site is executing a write transaction.
 */
afs_int32
SDISK_Begin(rxcall, atid)
     register struct rx_call *rxcall;
     struct ubik_tid *atid;
{
    register afs_int32 code;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    DBHOLD(ubik_dbase);
    urecovery_CheckTid(atid);
    if (ubik_currentTrans) {
	/* If the thread is not waiting for lock - ok to end it */
#if !defined(UBIK_PAUSE)
	if (ubik_currentTrans->locktype != LOCKWAIT) {
#endif /* UBIK_PAUSE */
	    udisk_end(ubik_currentTrans);
#if !defined(UBIK_PAUSE)
	}
#endif /* UBIK_PAUSE */
	ubik_currentTrans = (struct ubik_trans *)0;
    }
    code = udisk_begin(ubik_dbase, UBIK_WRITETRANS, &ubik_currentTrans);
    if (!code && ubik_currentTrans) {
	/* label this trans with the right trans id */
	ubik_currentTrans->tid.epoch = atid->epoch;
	ubik_currentTrans->tid.counter = atid->counter;
    }
    DBRELE(ubik_dbase);
    return code;
}


afs_int32
SDISK_Commit(rxcall, atid)
     register struct rx_call *rxcall;
     struct ubik_tid *atid;
{
    register afs_int32 code;
    register struct ubik_dbase *dbase;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }

    if (!ubik_currentTrans) {
	return USYNC;
    }
    /*
     * sanity check to make sure only write trans appear here
     */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	return UBADTYPE;
    }

    dbase = ubik_currentTrans->dbase;
    DBHOLD(dbase);
    urecovery_CheckTid(atid);
    if (!ubik_currentTrans) {
	DBRELE(dbase);
	return USYNC;
    }

    code = udisk_commit(ubik_currentTrans);
    if (code == 0) {
	/* sync site should now match */
	ubik_dbVersion = ubik_dbase->version;
    }
    DBRELE(dbase);
    return code;
}

afs_int32
SDISK_ReleaseLocks(rxcall, atid)
     register struct rx_call *rxcall;
     struct ubik_tid *atid;
{
    register struct ubik_dbase *dbase;
    register afs_int32 code;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }

    if (!ubik_currentTrans) {
	return USYNC;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	return UBADTYPE;
    }

    dbase = ubik_currentTrans->dbase;
    DBHOLD(dbase);
    urecovery_CheckTid(atid);
    if (!ubik_currentTrans) {
	DBRELE(dbase);
	return USYNC;
    }

    /* If the thread is not waiting for lock - ok to end it */
#if !defined(UBIK_PAUSE)
    if (ubik_currentTrans->locktype != LOCKWAIT) {
#endif /* UBIK_PAUSE */
	udisk_end(ubik_currentTrans);
#if !defined(UBIK_PAUSE)
    }
#endif /* UBIK_PAUSE */
    ubik_currentTrans = (struct ubik_trans *)0;
    DBRELE(dbase);
    return 0;
}

afs_int32
SDISK_Abort(rxcall, atid)
     register struct rx_call *rxcall;
     struct ubik_tid *atid;
{
    register afs_int32 code;
    register struct ubik_dbase *dbase;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }

    if (!ubik_currentTrans) {
	return USYNC;
    }
    /* sanity check to make sure only write trans appear here  */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	return UBADTYPE;
    }

    dbase = ubik_currentTrans->dbase;
    DBHOLD(dbase);
    urecovery_CheckTid(atid);
    if (!ubik_currentTrans) {
	DBRELE(dbase);
	return USYNC;
    }

    code = udisk_abort(ubik_currentTrans);
    /* If the thread is not waiting for lock - ok to end it */
#if !defined(UBIK_PAUSE)
    if (ubik_currentTrans->locktype != LOCKWAIT) {
#endif /* UBIK_PAUSE */
	udisk_end(ubik_currentTrans);
#if !defined(UBIK_PAUSE)
    }
#endif /* UBIK_PAUSE */
    ubik_currentTrans = (struct ubik_trans *)0;
    DBRELE(dbase);
    return code;
}

afs_int32
SDISK_Lock(rxcall, atid, afile, apos, alen, atype)
     register struct rx_call *rxcall;
     struct ubik_tid *atid;
     afs_int32 afile, apos, alen, atype;	/* apos and alen are not used */
{
    register afs_int32 code;
    register struct ubik_dbase *dbase;
    struct ubik_trans *ubik_thisTrans;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    if (!ubik_currentTrans) {
	return USYNC;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	return UBADTYPE;
    }
    if (alen != 1) {
	return UBADLOCK;
    }
    dbase = ubik_currentTrans->dbase;
    DBHOLD(dbase);
    urecovery_CheckTid(atid);
    if (!ubik_currentTrans) {
	DBRELE(dbase);
	return USYNC;
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

    DBRELE(dbase);
    return code;
}

/* Write a vector of data */
afs_int32
SDISK_WriteV(rxcall, atid, io_vector, io_buffer)
     register struct rx_call *rxcall;
     struct ubik_tid *atid;
     iovec_wrt *io_vector;
     iovec_buf *io_buffer;
{
    afs_int32 code, i, offset;
    struct ubik_dbase *dbase;
    struct ubik_iovec *iovec;
    char *iobuf;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    if (!ubik_currentTrans) {
	return USYNC;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	return UBADTYPE;
    }

    dbase = ubik_currentTrans->dbase;
    DBHOLD(dbase);
    urecovery_CheckTid(atid);
    if (!ubik_currentTrans) {
	DBRELE(dbase);
	return USYNC;
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

    DBRELE(dbase);
    return code;
}

afs_int32
SDISK_Write(rxcall, atid, afile, apos, adata)
     register struct rx_call *rxcall;
     struct ubik_tid *atid;
     afs_int32 afile, apos;
     register bulkdata *adata;
{
    register afs_int32 code;
    register struct ubik_dbase *dbase;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    if (!ubik_currentTrans) {
	return USYNC;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	return UBADTYPE;
    }

    dbase = ubik_currentTrans->dbase;
    DBHOLD(dbase);
    urecovery_CheckTid(atid);
    if (!ubik_currentTrans) {
	DBRELE(dbase);
	return USYNC;
    }
    code =
	udisk_write(ubik_currentTrans, afile, adata->bulkdata_val, apos,
		    adata->bulkdata_len);
    DBRELE(dbase);
    return code;
}

afs_int32
SDISK_Truncate(rxcall, atid, afile, alen)
     register struct rx_call *rxcall;
     struct ubik_tid *atid;
     afs_int32 afile;
     afs_int32 alen;
{
    register afs_int32 code;
    register struct ubik_dbase *dbase;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
    if (!ubik_currentTrans) {
	return USYNC;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	return UBADTYPE;
    }

    dbase = ubik_currentTrans->dbase;
    DBHOLD(dbase);
    urecovery_CheckTid(atid);
    if (!ubik_currentTrans) {
	DBRELE(dbase);
	return USYNC;
    }
    code = udisk_truncate(ubik_currentTrans, afile, alen);
    DBRELE(dbase);
    return code;
}

afs_int32
SDISK_GetVersion(rxcall, aversion)
     register struct rx_call *rxcall;
     register struct ubik_version *aversion;
{
    register afs_int32 code;

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
    if (ubeacon_AmSyncSite()) {
	return UDEADLOCK;
    }

    DBHOLD(ubik_dbase);
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
SDISK_GetFile(rxcall, file, version)
     register struct rx_call *rxcall;
     register afs_int32 file;
     struct ubik_version *version;
{
    register afs_int32 code;
    register struct ubik_dbase *dbase;
    register afs_int32 offset;
    struct ubik_stat ubikstat;
    char tbuffer[256];
    afs_int32 tlen;
    afs_int32 length;

    if ((code = ubik_CheckAuth(rxcall))) {
	return code;
    }
/* temporarily disabled because it causes problems for migration tool.  Hey, it's just
 * a sanity check, anyway. 
    if (ubeacon_AmSyncSite()) {
      return UDEADLOCK;
    }
*/
    dbase = ubik_dbase;
    DBHOLD(dbase);
    code = (*dbase->stat) (dbase, file, &ubikstat);
    if (code < 0) {
	DBRELE(dbase);
	return code;
    }
    length = ubikstat.size;
    tlen = htonl(length);
    code = rx_Write(rxcall, (char *)&tlen, sizeof(afs_int32));
    if (code != sizeof(afs_int32)) {
	DBRELE(dbase);
	ubik_dprint("Rx-write length error=%d\n", code);
	return BULK_ERROR;
    }
    offset = 0;
    while (length > 0) {
	tlen = (length > sizeof(tbuffer) ? sizeof(tbuffer) : length);
	code = (*dbase->read) (dbase, file, tbuffer, offset, tlen);
	if (code != tlen) {
	    DBRELE(dbase);
	    ubik_dprint("read failed error=%d\n", code);
	    return UIOERROR;
	}
	code = rx_Write(rxcall, tbuffer, tlen);
	if (code != tlen) {
	    DBRELE(dbase);
	    ubik_dprint("Rx-write length error=%d\n", code);
	    return BULK_ERROR;
	}
	length -= tlen;
	offset += tlen;
    }
    code = (*dbase->getlabel) (dbase, file, version);	/* return the dbase, too */
    DBRELE(dbase);
    return code;
}

afs_int32
SDISK_SendFile(rxcall, file, length, avers)
     register struct rx_call *rxcall;
     afs_int32 file;
     afs_int32 length;
     struct ubik_version *avers;
{
    register afs_int32 code;
    register struct ubik_dbase *dbase;
    char tbuffer[256];
    afs_int32 offset;
    struct ubik_version tversion;
    register int tlen;
    struct rx_peer *tpeer;
    struct rx_connection *tconn;
    afs_uint32 otherHost;

    /* send the file back to the requester */

    if ((code = ubik_CheckAuth(rxcall))) {
	goto failed;
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
    offset = uvote_GetSyncSite();
    tconn = rx_ConnectionOf(rxcall);
    tpeer = rx_PeerOf(tconn);
    otherHost = ubikGetPrimaryInterfaceAddr(rx_HostOf(tpeer));
    if (offset && offset != otherHost) {
	/* we *know* this is the wrong guy */
	code = USYNC;
	goto failed;
    }

    dbase = ubik_dbase;
    DBHOLD(dbase);

    /* abort any active trans that may scribble over the database */
    urecovery_AbortAll(dbase);

    ubik_print("Ubik: Synchronize database with server %s\n",
	       afs_inet_ntoa(otherHost));

    offset = 0;
    (*dbase->truncate) (dbase, file, 0);	/* truncate first */
    tversion.epoch = 0;		/* start off by labelling in-transit db as invalid */
    tversion.counter = 0;
    (*dbase->setlabel) (dbase, file, &tversion);	/* setlabel does sync */
    memcpy(&ubik_dbase->version, &tversion, sizeof(struct ubik_version));
    while (length > 0) {
	tlen = (length > sizeof(tbuffer) ? sizeof(tbuffer) : length);
	code = rx_Read(rxcall, tbuffer, tlen);
	if (code != tlen) {
	    DBRELE(dbase);
	    ubik_dprint("Rx-read length error=%d\n", code);
	    code = BULK_ERROR;
	    goto failed;
	}
	code = (*dbase->write) (dbase, file, tbuffer, offset, tlen);
	if (code != tlen) {
	    DBRELE(dbase);
	    ubik_dprint("write failed error=%d\n", code);
	    code = UIOERROR;
	    goto failed;
	}
	offset += tlen;
	length -= tlen;
    }

    /* sync data first, then write label and resync (resync done by setlabel call).
     * This way, good label is only on good database. */
    (*ubik_dbase->sync) (dbase, file);
    code = (*ubik_dbase->setlabel) (dbase, file, avers);
    memcpy(&ubik_dbase->version, avers, sizeof(struct ubik_version));
    udisk_Invalidate(dbase, file);	/* new dbase, flush disk buffers */
    LWP_NoYieldSignal(&dbase->version);
    DBRELE(dbase);
  failed:
    if (code) {
	ubik_print
	    ("Ubik: Synchronize database with server %s failed (error = %d)\n",
	     afs_inet_ntoa(otherHost), code);
    } else {
	ubik_print("Ubik: Synchronize database completed\n");
    }
    return code;
}


afs_int32
SDISK_Probe(rxcall)
     register struct rx_call *rxcall;
{
    return 0;
}

/*
* Update remote machines addresses in my server list
* Send back my addresses to caller of this RPC
* Returns zero on success, else 1.
*/
afs_int32
SDISK_UpdateInterfaceAddr(rxcall, inAddr, outAddr)
     register struct rx_call *rxcall;
     UbikInterfaceAddr *inAddr, *outAddr;
{
    struct ubik_server *ts, *tmp;
    afs_uint32 remoteAddr;	/* in net byte order */
    int i, j, found = 0, probableMatch = 0;

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
	ubik_print("Inconsistent Cell Info from server: ");
	for (i = 0; i < UBIK_MAX_INTERFACE_ADDR && inAddr->hostAddr[i]; i++)
	    ubik_print("%s ", afs_inet_ntoa(htonl(inAddr->hostAddr[i])));
	ubik_print("\n");
	fflush(stdout);
	fflush(stderr);
	printServerInfo();
	return UBADHOST;
    }

    /* update our data structures */
    for (i = 1; i < UBIK_MAX_INTERFACE_ADDR; i++)
	ts->addr[i] = htonl(inAddr->hostAddr[i]);

    ubik_print("ubik: A Remote Server has addresses: ");
    for (i = 0; i < UBIK_MAX_INTERFACE_ADDR && ts->addr[i]; i++)
	ubik_print("%s ", afs_inet_ntoa(ts->addr[i]));
    ubik_print("\n");

    return 0;
}

void
printServerInfo()
{
    struct ubik_server *ts;
    int i, j = 1;

    ubik_print("Local CellServDB:");
    for (ts = ubik_servers; ts; ts = ts->next, j++) {
	ubik_print("Server %d: ", j);
	for (i = 0; (i < UBIK_MAX_INTERFACE_ADDR) && ts->addr[i]; i++)
	    ubik_print("%s ", afs_inet_ntoa(ts->addr[i]));
    }
    ubik_print("\n");
}

afs_int32
SDISK_SetVersion(rxcall, atid, oldversionp, newversionp)
     struct rx_call *rxcall;
     struct ubik_tid *atid;
     struct ubik_version *oldversionp;
     struct ubik_version *newversionp;
{
    afs_int32 code = 0;
    struct ubik_dbase *dbase;

    if ((code = ubik_CheckAuth(rxcall))) {
	return (code);
    }

    if (!ubik_currentTrans) {
	return USYNC;
    }
    /* sanity check to make sure only write trans appear here */
    if (ubik_currentTrans->type != UBIK_WRITETRANS) {
	return UBADTYPE;
    }

    /* Should not get this for the sync site */
    if (ubeacon_AmSyncSite()) {
	return UDEADLOCK;
    }

    dbase = ubik_currentTrans->dbase;
    DBHOLD(dbase);
    urecovery_CheckTid(atid);
    if (!ubik_currentTrans) {
	DBRELE(dbase);
	return USYNC;
    }

    /* Set the label if its version matches the sync-site's */
    if ((oldversionp->epoch == ubik_dbVersion.epoch)
	&& (oldversionp->counter == ubik_dbVersion.counter)) {
	code = (*dbase->setlabel) (ubik_dbase, 0, newversionp);
	if (!code) {
	    ubik_dbase->version = *newversionp;
	    ubik_dbVersion = *newversionp;
	}
    } else {
	code = USYNC;
    }

    DBRELE(dbase);
    return code;
}
