/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file is a reimplementation of volser/vsproc.s.  Every attempt
 * has been made to keep it as similar as possible to aid comprehension
 * of the code.  For most functions, two parameters have been added
 * the cell handle, and a status variable.  For those functions that
 * require one, a server handle may also be added.
 *
 * Other changes were made to provide thread safe functions and
 * eliminate the practice of reporting errors to STDOUT.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "vsprocs.h"
#include "vosutils.h"
#include "lockprocs.h"
#include "../adminutil/afs_AdminInternal.h"
#include <afs/afs_AdminErrors.h>
#include "afs_vosAdmin.h"
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef AFS_NT40_ENV
#include <io.h>
#endif

static afs_int32 GroupEntries();

struct release {
    afs_int32 time;
    afs_int32 vldbEntryIndex;
};

static struct rx_connection *
UV_Bind(afs_cell_handle_p cellHandle, afs_int32 aserver, afs_int32 port)
{
    return rx_GetCachedConnection(htonl(aserver), htons(port), VOLSERVICE_ID,
				  cellHandle->tokens->afs_sc,
				  cellHandle->tokens->sc_index);
}


/* if <okvol> is allright(indicated by beibg able to
 * start a transaction, delete the <delvol> */
static afs_int32
CheckAndDeleteVolume(struct rx_connection *aconn, afs_int32 apart,
		     afs_int32 okvol, afs_int32 delvol)
{
    afs_int32 error, code, tid, rcode;

    error = 0;
    code = 0;

    if (okvol == 0) {
	code = AFSVolTransCreate(aconn, delvol, apart, ITOffline, &tid);
	if (!error && code)
	    error = code;
	code = AFSVolDeleteVolume(aconn, tid);
	if (!error && code)
	    error = code;
	code = AFSVolEndTrans(aconn, tid, &rcode);
	if (!code)
	    code = rcode;
	if (!error && code)
	    error = code;
	return error;
    } else {
	code = AFSVolTransCreate(aconn, okvol, apart, ITOffline, &tid);
	if (!code) {
	    code = AFSVolEndTrans(aconn, tid, &rcode);
	    if (!code)
		code = rcode;
	    if (!error && code)
		error = code;
	    code = AFSVolTransCreate(aconn, delvol, apart, ITOffline, &tid);
	    if (!error && code)
		error = code;
	    code = AFSVolDeleteVolume(aconn, tid);
	    if (!error && code)
		error = code;
	    code = AFSVolEndTrans(aconn, tid, &rcode);
	    if (!code)
		code = rcode;
	    if (!error && code)
		error = code;
	} else
	    error = code;
	return error;
    }
}

/* forcibly remove a volume.  Very dangerous call */
int
UV_NukeVolume(afs_cell_handle_p cellHandle, struct rx_connection *server,
	      unsigned int partition, unsigned int volumeId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    tst = AFSVolNukeVolume(server, partition, volumeId);

    if (!tst) {
	rc = 1;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/* create a volume, given a server, partition number, volume name --> sends
* back new vol id in <anewid>*/
int
UV_CreateVolume(afs_cell_handle_p cellHandle, struct rx_connection *server,
		unsigned int partition, const char *volumeName,
		unsigned int quota, unsigned int *volumeId, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_int32 tid = 0;
    afs_int32 rcode;
    struct nvldbentry entry;
    struct volintInfo tstatus;

    memset(&tstatus, 0, sizeof(tstatus));
    tstatus.dayUse = -1;
    tstatus.maxquota = quota;

    /* next the next 3 available ids from the VLDB */
    tst = ubik_VL_GetNewVolumeId(cellHandle->vos, 0, 3, volumeId);
    if (tst) {
	goto fail_UV_CreateVolume;
    }

    tst =
	AFSVolCreateVolume(server, partition, volumeName, volser_RW, 0,
			   volumeId, &tid);
    if (tst) {
	goto fail_UV_CreateVolume;
    }

    AFSVolSetInfo(server, tid, &tstatus);

    tst = AFSVolSetFlags(server, tid, 0);
    if (tst) {
	goto fail_UV_CreateVolume;
    }

    /* set up the vldb entry for this volume */
    strncpy(entry.name, volumeName, VOLSER_OLDMAXVOLNAME);
    entry.nServers = 1;
    entry.serverNumber[0] = ntohl(rx_HostOf(rx_PeerOf(server)));
    entry.serverPartition[0] = partition;
    entry.flags = RW_EXISTS;
    entry.serverFlags[0] = ITSRWVOL;
    entry.volumeId[RWVOL] = *volumeId;
    entry.volumeId[ROVOL] = *volumeId + 1;
    entry.volumeId[BACKVOL] = *volumeId + 2;
    entry.cloneId = 0;

    if (!VLDB_CreateEntry(cellHandle, &entry, &tst)) {
	AFSVolDeleteVolume(server, tid);
	goto fail_UV_CreateVolume;
    }

    tst = AFSVolEndTrans(server, tid, &rcode);
    tid = 0;
    if (tst) {
	goto fail_UV_CreateVolume;
    }
    rc = 1;

  fail_UV_CreateVolume:

    if (tid != 0) {
	AFSVolEndTrans(server, tid, &rcode);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/* Delete the volume <volid>on <aserver> <apart>
 * the physical entry gets removed from the vldb only if the ref count 
 * becomes zero
 */
int
UV_DeleteVolume(afs_cell_handle_p cellHandle, struct rx_connection *server,
		unsigned int partition, unsigned int volumeId,
		afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_status_t temp = 0;
    int serverAddr = ntohl(rx_HostOf(rx_PeerOf(server)));

    afs_int32 ttid = 0;
    afs_int32 rcode;
    struct nvldbentry entry;
    int islocked = 0;
    afs_int32 avoltype = -1, vtype;
    int notondisk = 0, notinvldb = 0;

    /* Find and read the VLDB entry for this volume */
    tst =
	ubik_VL_SetLock(cellHandle->vos, 0, volumeId, avoltype,
		  VLOP_DELETE);
    if (tst) {
	if (tst != VL_NOENT) {
	    goto fail_UV_DeleteVolume;
	}
	notinvldb = 1;
    } else {
	islocked = 1;

	if (!aVLDB_GetEntryByID(cellHandle, volumeId, avoltype, &entry, &tst)) {
	    goto fail_UV_DeleteVolume;
	}

    }

    /* Whether volume is in the VLDB or not. Delete the volume on disk */
    tst = AFSVolTransCreate(server, volumeId, partition, ITOffline, &ttid);
    if (tst) {
	if (tst == VNOVOL) {
	    notondisk = 1;
	} else {
	    goto fail_UV_DeleteVolume;
	}
    } else {
	tst = AFSVolDeleteVolume(server, ttid);
	if (tst) {
	    goto fail_UV_DeleteVolume;
	}
	tst = AFSVolEndTrans(server, ttid, &rcode);
	tst = (tst ? tst : rcode);
	ttid = 0;
	if (tst) {
	    goto fail_UV_DeleteVolume;
	}
    }

    if (notinvldb) {
	goto fail_UV_DeleteVolume;
    }

    if (volumeId == entry.volumeId[BACKVOL]) {
	if (!(entry.flags & BACK_EXISTS)
	    || !Lp_Match(cellHandle, &entry, serverAddr, partition, &tst)) {
	    notinvldb = 2;
	    goto fail_UV_DeleteVolume;
	}

	entry.flags &= ~BACK_EXISTS;
	vtype = BACKVOL;
    }

    else if (volumeId == entry.volumeId[ROVOL]) {
	if (!Lp_ROMatch(cellHandle, &entry, serverAddr, partition, &tst)) {
	    notinvldb = 2;
	    goto fail_UV_DeleteVolume;
	}

	Lp_SetROValue(cellHandle, &entry, serverAddr, partition, 0, 0);
	entry.nServers--;
	if (!Lp_ROMatch(cellHandle, &entry, 0, 0, &tst)) {
	    entry.flags &= ~RO_EXISTS;
	}
	vtype = ROVOL;
    }

    else if (volumeId == entry.volumeId[RWVOL]) {
	if (!(entry.flags & RW_EXISTS)
	    || !Lp_Match(cellHandle, &entry, serverAddr, partition, &tst)) {
	    notinvldb = 2;
	    goto fail_UV_DeleteVolume;
	}

	/* Delete backup if it exists */
	tst =
	    AFSVolTransCreate(server, entry.volumeId[BACKVOL], partition,
			      ITOffline, &ttid);
	if (!tst) {
	    tst = AFSVolDeleteVolume(server, ttid);
	    if (tst) {
		goto fail_UV_DeleteVolume;
	    }
	    tst = AFSVolEndTrans(server, ttid, &rcode);
	    ttid = 0;
	    tst = (tst ? tst : rcode);
	    if (tst) {
		goto fail_UV_DeleteVolume;
	    }
	}

	Lp_SetRWValue(cellHandle, &entry, serverAddr, partition, 0L, 0L);
	entry.nServers--;
	entry.flags &= ~(BACK_EXISTS | RW_EXISTS);
	vtype = RWVOL;

    }

    else {
	notinvldb = 2;		/* Not found on this server and partition */
	goto fail_UV_DeleteVolume;
    }

    if ((entry.nServers <= 0) || !(entry.flags & (RO_EXISTS | RW_EXISTS))) {
	tst = ubik_VL_DeleteEntry(cellHandle->vos, 0, volumeId, vtype);
	if (tst) {
	    goto fail_UV_DeleteVolume;
	}
    } else {
	if (!VLDB_ReplaceEntry
	    (cellHandle, volumeId, vtype, &entry,
	     (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP), &tst)) {
	    goto fail_UV_DeleteVolume;
	}
    }
    islocked = 0;
    rc = 1;

  fail_UV_DeleteVolume:

    if (notondisk && notinvldb) {
	if (!tst)
	    tst = ADMVOSVOLUMENOEXIST;
    }

    if (ttid) {
	temp = AFSVolEndTrans(server, ttid, &rcode);
	temp = (temp ? temp : rcode);
	if (temp) {
	    if (!tst)
		tst = temp;
	}
    }

    if (islocked) {
	temp =
	    ubik_VL_ReleaseLock(cellHandle->vos, 0, volumeId, -1,
		      (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	if (temp) {
	    if (!tst)
		tst = temp;
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

#define ONERR(ec, es, ep) if (ec) { fprintf(STDERR, (es), (ep)); error = (ec); goto mfail; }

/* Move volume <afromvol> on <afromserver> <afrompart> to <atoserver>
 * <atopart>. The operation is almost idempotent 
 */

int
UV_MoveVolume(afs_cell_handle_p cellHandle, afs_int32 afromvol,
	      afs_int32 afromserver, afs_int32 afrompart, afs_int32 atoserver,
	      afs_int32 atopart, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_status_t etst = 0;
    struct rx_connection *toconn, *fromconn;
    afs_int32 fromtid, totid, clonetid;
    char vname[64];
    char *volName = 0;
    char tmpName[VOLSER_MAXVOLNAME + 1];
    afs_int32 rcode;
    afs_int32 fromDate;
    struct restoreCookie cookie;
    afs_int32 newVol, volid, backupId;
    struct volser_status tstatus;
    struct destServer destination;

    struct nvldbentry entry;
    int islocked, pntg;
    afs_int32 error;
    int same;
    afs_int32 store_flags;

#ifdef	ENABLE_BUGFIX_1165
    volEntries volumeInfo;
    struct volintInfo *infop = 0;
#endif

    islocked = 0;
    fromconn = (struct rx_connection *)0;
    toconn = (struct rx_connection *)0;
    fromtid = 0;
    totid = 0;
    clonetid = 0;
    error = 0;
    volid = 0;
    pntg = 0;
    backupId = 0;
    newVol = 0;

    if (!aVLDB_GetEntryByID(cellHandle, afromvol, -1, &entry, &tst)) {
	goto fail_UV_MoveVolume;
    }

    if (entry.volumeId[RWVOL] != afromvol) {
	tst = ADMVOSVOLUMEMOVERWONLY;
	goto fail_UV_MoveVolume;
    }

    tst =
	ubik_VL_SetLock(cellHandle->vos, 0, afromvol, RWVOL, VLOP_MOVE);
    if (tst) {
	goto fail_UV_MoveVolume;
    }
    islocked = 1;

    if (!aVLDB_GetEntryByID(cellHandle, afromvol, RWVOL, &entry, &tst)) {
	goto fail_UV_MoveVolume;
    }

    backupId = entry.volumeId[BACKVOL];

    if (!Lp_Match(cellHandle, &entry, afromserver, afrompart, &tst)) {
	/* the from server and partition do not exist in the vldb entry corresponding to volid */
	if (!Lp_Match(cellHandle, &entry, atoserver, atopart, &tst)) {
	    /* the to server and partition do not exist in the vldb entry corresponding to volid */
	    tst =
		ubik_VL_ReleaseLock(cellHandle->vos, 0, afromvol, -1,
			  (LOCKREL_OPCODE | LOCKREL_AFSID |
			   LOCKREL_TIMESTAMP));
	    if (tst) {
		goto fail_UV_MoveVolume;
	    }
	    tst = VOLSERVOLMOVED;
	    goto fail_UV_MoveVolume;
	}

	/* delete the volume afromvol on src_server */
	/* from-info does not exist but to-info does =>
	 * we have already done the move, but the volume
	 * may still be existing physically on from fileserver
	 */
	fromconn = UV_Bind(cellHandle, afromserver, AFSCONF_VOLUMEPORT);
	fromtid = 0;
	pntg = 1;

	tst =
	    AFSVolTransCreate(fromconn, afromvol, afrompart, ITOffline,
			      &fromtid);
	if (!tst) {		/* volume exists - delete it */
	    tst =
		AFSVolSetFlags(fromconn, fromtid,
			       VTDeleteOnSalvage | VTOutOfService);
	    if (tst) {
		goto fail_UV_MoveVolume;
	    }

	    tst = AFSVolDeleteVolume(fromconn, fromtid);
	    if (tst) {
		goto fail_UV_MoveVolume;
	    }

	    tst = AFSVolEndTrans(fromconn, fromtid, &rcode);
	    fromtid = 0;
	    if (!tst)
		tst = rcode;
	    if (tst) {
		goto fail_UV_MoveVolume;
	    }
	}

	/*delete the backup volume now */
	fromtid = 0;
	tst =
	    AFSVolTransCreate(fromconn, backupId, afrompart, ITOffline,
			      &fromtid);
	if (!tst) {		/* backup volume exists - delete it */
	    tst =
		AFSVolSetFlags(fromconn, fromtid,
			       VTDeleteOnSalvage | VTOutOfService);
	    if (tst) {
		goto fail_UV_MoveVolume;
	    }

	    tst = AFSVolDeleteVolume(fromconn, fromtid);
	    if (tst) {
		goto fail_UV_MoveVolume;
	    }

	    tst = AFSVolEndTrans(fromconn, fromtid, &rcode);
	    fromtid = 0;
	    if (!tst)
		tst = rcode;
	    if (tst) {
		goto fail_UV_MoveVolume;
	    }
	}

	fromtid = 0;
	error = 0;
	goto fail_UV_MoveVolume;
    }

    /* From-info matches the vldb info about volid,
     * its ok start the move operation, the backup volume 
     * on the old site is deleted in the process 
     */
    if (afrompart == atopart) {
	if (!VLDB_IsSameAddrs
	    (cellHandle, afromserver, atoserver, &same, &tst)) {
	    goto fail_UV_MoveVolume;
	}
	if (same) {
	    tst = VOLSERVOLMOVED;
	    goto fail_UV_MoveVolume;
	}
    }

    pntg = 1;
    toconn = UV_Bind(cellHandle, atoserver, AFSCONF_VOLUMEPORT);	/* get connections to the servers */
    fromconn = UV_Bind(cellHandle, afromserver, AFSCONF_VOLUMEPORT);
    fromtid = totid = 0;	/* initialize to uncreated */

    /* ***
     * clone the read/write volume locally.
     * ***/

    tst = AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy, &fromtid);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* Get a clone id */
    newVol = 0;
    tst = ubik_VL_GetNewVolumeId(cellHandle->vos, 0, 1, &newVol);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* Do the clone. Default flags on clone are set to delete on salvage and out of service */
    strcpy(vname, "move-clone-temp");
    tst = AFSVolClone(fromconn, fromtid, 0, readonlyVolume, vname, &newVol);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* lookup the name of the volume we just cloned */
    volid = afromvol;
    tst = AFSVolGetName(fromconn, fromtid, &volName);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    rcode = 0;
    tst = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;
    if (!tst)
	tst = rcode;
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* ***
     * Create the destination volume
     * ***/

    tst =
	AFSVolTransCreate(fromconn, newVol, afrompart, ITOffline, &clonetid);
    if (tst) {
	goto fail_UV_MoveVolume;
    }
    tst = AFSVolSetFlags(fromconn, clonetid, VTDeleteOnSalvage | VTOutOfService);	/*redundant */
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* remember time from which we've dumped the volume */
    tst = AFSVolGetStatus(fromconn, clonetid, &tstatus);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    fromDate = tstatus.creationDate - CLOCKSKEW;

#ifdef	ENABLE_BUGFIX_1165
    /*
     * Get the internal volume state from the source volume. We'll use such info (i.e. dayUse)
     * to copy it to the new volume (via AFSSetInfo later on) so that when we move volumes we
     * don't use this information...
     */
    volumeInfo.volEntries_val = (volintInfo *) 0;	/*this hints the stub to allocate space */
    volumeInfo.volEntries_len = 0;
    tst = AFSVolListOneVolume(fromconn, afrompart, afromvol, &volumeInfo);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    infop = (volintInfo *) volumeInfo.volEntries_val;
    infop->maxquota = -1;	/* Else it will replace the default quota */
#endif

    /* create a volume on the target machine */
    volid = afromvol;
    tst = AFSVolTransCreate(toconn, volid, atopart, ITOffline, &totid);
    if (!tst) {			/*delete the existing volume */

	tst = AFSVolDeleteVolume(toconn, totid);
	if (tst) {
	    goto fail_UV_MoveVolume;
	}

	tst = AFSVolEndTrans(toconn, totid, &rcode);
	totid = 0;
	if (!tst)
	    tst = rcode;
	if (tst) {
	    goto fail_UV_MoveVolume;
	}

    }

    tst =
	AFSVolCreateVolume(toconn, atopart, volName, volser_RW, volid, &volid,
			   &totid);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    strncpy(tmpName, volName, VOLSER_OLDMAXVOLNAME);
    free(volName);
    volName = NULL;

    tst = AFSVolSetFlags(toconn, totid, (VTDeleteOnSalvage | VTOutOfService));
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /***
     * Now dump the clone to the new volume
     ***/

    destination.destHost = atoserver;
    destination.destPort = AFSCONF_VOLUMEPORT;
    destination.destSSID = 1;

    /* Copy the clone to the new volume */
    strncpy(cookie.name, tmpName, VOLSER_OLDMAXVOLNAME);
    cookie.type = RWVOL;
    cookie.parent = entry.volumeId[RWVOL];
    cookie.clone = 0;
    tst = AFSVolForward(fromconn, clonetid, 0, &destination, totid, &cookie);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    tst = AFSVolEndTrans(fromconn, clonetid, &rcode);
    if (!tst)
	tst = rcode;
    clonetid = 0;
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* ***
     * reattach to the main-line volume, and incrementally dump it.
     * ***/

    tst = AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy, &fromtid);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* now do the incremental */
    tst =
	AFSVolForward(fromconn, fromtid, fromDate, &destination, totid,
		      &cookie);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* now adjust the flags so that the new volume becomes official */
    tst = AFSVolSetFlags(fromconn, fromtid, VTOutOfService);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    tst = AFSVolSetFlags(toconn, totid, 0);
    if (tst) {
	goto fail_UV_MoveVolume;
    }
#ifdef	ENABLE_BUGFIX_1165
    tst = AFSVolSetInfo(toconn, totid, infop);
    if (tst) {
	goto fail_UV_MoveVolume;
    }
#endif

    /* put new volume online */
    tst = AFSVolEndTrans(toconn, totid, &rcode);
    totid = 0;
    if (!tst)
	tst = rcode;
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    Lp_SetRWValue(cellHandle, &entry, afromserver, afrompart, atoserver,
		  atopart);
    store_flags = entry.flags;
    entry.flags &= ~BACK_EXISTS;

    if (!VLDB_ReplaceEntry
	(cellHandle, afromvol, -1, &entry,
	 (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP), &tst)) {
	goto fail_UV_MoveVolume;
    }
    entry.flags = store_flags;
    islocked = 0;

    if (atoserver != afromserver) {
	/* set forwarding pointer for moved volumes */
	tst = AFSVolSetForwarding(fromconn, fromtid, htonl(atoserver));
	if (tst) {
	    goto fail_UV_MoveVolume;
	}
    }

    tst = AFSVolDeleteVolume(fromconn, fromtid);	/* zap original volume */
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    tst = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;
    if (!tst)
	tst = rcode;
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* Delete the backup volume on the original site */
    tst =
	AFSVolTransCreate(fromconn, backupId, afrompart, ITOffline, &fromtid);
    if (!tst) {
	tst =
	    AFSVolSetFlags(fromconn, fromtid,
			   VTDeleteOnSalvage | VTOutOfService);
	if (tst) {
	    goto fail_UV_MoveVolume;
	}

	tst = AFSVolDeleteVolume(fromconn, fromtid);
	if (tst) {
	    goto fail_UV_MoveVolume;
	}

	tst = AFSVolEndTrans(fromconn, fromtid, &rcode);
	fromtid = 0;
	if (!tst)
	    tst = rcode;
	if (tst) {
	    goto fail_UV_MoveVolume;
	}

    } else
	tst = 0;		/* no backup volume? that's okay */

    fromtid = 0;

    tst =
	AFSVolTransCreate(fromconn, newVol, afrompart, ITOffline, &clonetid);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* now delete the clone */

    tst = AFSVolDeleteVolume(fromconn, clonetid);
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    tst = AFSVolEndTrans(fromconn, clonetid, &rcode);
    if (!tst)
	tst = rcode;
    clonetid = 0;
    if (tst) {
	goto fail_UV_MoveVolume;
    }

    /* fall through */
    /* END OF MOVE */

    /* normal cleanup code */

    if (islocked) {
	etst =
	    ubik_VL_ReleaseLock(cellHandle->vos, 0, afromvol, -1,
		      (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	if (etst) {
	    if (!tst)
		tst = etst;
	}
    }

    if (fromtid) {
	etst = AFSVolEndTrans(fromconn, fromtid, &rcode);
	if (etst || rcode) {
	    if (!tst)
		tst = (etst ? etst : rcode);
	}
    }

    if (clonetid) {
	etst = AFSVolEndTrans(fromconn, clonetid, &rcode);
	if (etst || rcode) {
	    if (!tst)
		tst = (etst ? etst : rcode);
	}
    }

    if (totid) {
	etst = AFSVolEndTrans(toconn, totid, &rcode);
	if (etst) {
	    if (!tst)
		tst = (etst ? etst : rcode);
	}
    }
    if (volName)
	free(volName);
#ifdef	ENABLE_BUGFIX_1165
    if (infop)
	free(infop);
#endif
    if (fromconn)
	rx_ReleaseCachedConnection(fromconn);
    if (toconn)
	rx_ReleaseCachedConnection(toconn);

    rc = 1;
    if (st != NULL) {
	*st = tst;
    }
    return rc;

    /* come here only when the sky falls */

  fail_UV_MoveVolume:

    /* unlock VLDB entry */
    if (islocked)
	ubik_VL_ReleaseLock(cellHandle->vos, 0, afromvol, -1,
		  (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));

    if (clonetid)
	AFSVolEndTrans(fromconn, clonetid, &rcode);
    if (totid)
	AFSVolEndTrans(toconn, totid, &rcode);
    if (fromtid) {		/* put it on-line */
	AFSVolSetFlags(fromconn, fromtid, 0);
	AFSVolEndTrans(fromconn, fromtid, &rcode);
    }

    if (!aVLDB_GetEntryByID(cellHandle, afromvol, -1, &entry, &tst)) {
	goto done;
    }

    /* Delete either the volume on the source location or the target location. 
     * If the vldb entry still points to the source location, then we know the
     * volume move didn't finish so we remove the volume from the target 
     * location. Otherwise, we remove the volume from the source location.
     */
    if (Lp_Match(cellHandle, &entry, afromserver, afrompart, &tst)) {	/* didn't move - delete target volume */

	if (volid && toconn) {
	    tst =
		AFSVolTransCreate(toconn, volid, atopart, ITOffline, &totid);
	    if (!tst) {
		AFSVolSetFlags(toconn, totid,
			       VTDeleteOnSalvage | VTOutOfService);
		AFSVolDeleteVolume(toconn, totid);
		AFSVolEndTrans(toconn, totid, &rcode);
	    }
	}

	/* put source volume on-line */
	if (fromconn) {
	    tst =
		AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy,
				  &fromtid);
	    if (!tst) {
		AFSVolSetFlags(fromconn, fromtid, 0);
		AFSVolEndTrans(fromconn, fromtid, &rcode);
	    }
	}
    } else {			/* yep, move complete */
	/* delete backup volume */
	if (fromconn) {
	    tst =
		AFSVolTransCreate(fromconn, backupId, afrompart, ITOffline,
				  &fromtid);
	    if (!tst) {
		AFSVolSetFlags(fromconn, fromtid,
			       VTDeleteOnSalvage | VTOutOfService);
		AFSVolDeleteVolume(fromconn, fromtid);
		AFSVolEndTrans(fromconn, fromtid, &rcode);
	    }

	    /* delete source volume */
	    tst =
		AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy,
				  &fromtid);
	    if (!tst) {
		AFSVolSetFlags(fromconn, fromtid,
			       VTDeleteOnSalvage | VTOutOfService);
		if (atoserver != afromserver)
		    AFSVolSetForwarding(fromconn, fromtid, htonl(atoserver));
		AFSVolDeleteVolume(fromconn, fromtid);
		AFSVolEndTrans(fromconn, fromtid, &rcode);
	    }
	}
    }

    /* common cleanup - delete local clone */
    if (newVol) {
	tst =
	    AFSVolTransCreate(fromconn, newVol, afrompart, ITOffline,
			      &clonetid);
	if (!tst) {
	    AFSVolDeleteVolume(fromconn, clonetid);
	    AFSVolEndTrans(fromconn, clonetid, &rcode);
	}
    }

    /* unlock VLDB entry */
    ubik_VL_ReleaseLock(cellHandle->vos, 0, afromvol, -1,
	      (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));

  done:			/* routine cleanup */
    if (volName)
	free(volName);
#ifdef	ENABLE_BUGFIX_1165
    if (infop)
	free(infop);
#endif
    if (fromconn)
	rx_ReleaseCachedConnection(fromconn);
    if (toconn)
	rx_ReleaseCachedConnection(toconn);

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/* Make a new backup of volume <avolid> on <aserver> and <apart> 
 * if one already exists, update it 
 */

int
UV_BackupVolume(afs_cell_handle_p cellHandle, afs_int32 aserver,
		afs_int32 apart, afs_int32 avolid, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0, temp = 0;
    afs_int32 ttid = 0, btid = 0;
    afs_int32 backupID;
    afs_int32 rcode = 0;
    char vname[VOLSER_MAXVOLNAME + 1];
    struct nvldbentry entry;
    int vldblocked = 0, vldbmod = 0, backexists = 1;
    struct rx_connection *aconn = UV_Bind(cellHandle, aserver,
					  AFSCONF_VOLUMEPORT);


    /* the calls to VLDB will succeed only if avolid is a RW volume,
     * since we are following the RW hash chain for searching */
    if (!aVLDB_GetEntryByID(cellHandle, avolid, RWVOL, &entry, &tst)) {
	goto fail_UV_BackupVolume;
    }

    /* These operations require the VLDB be locked since it means the VLDB
     * will change or the vldb is already locked.
     */
    if (!(entry.flags & BACK_EXISTS) ||	/* backup volume doesnt exist */
	(entry.flags & VLOP_ALLOPERS) ||	/* vldb lock already held */
	(entry.volumeId[BACKVOL] == INVALID_BID)) {
	/* no assigned backup volume id */

	tst =
	    ubik_VL_SetLock(cellHandle->vos, 0, avolid, RWVOL,
		      VLOP_BACKUP);
	if (tst) {
	    goto fail_UV_BackupVolume;
	}
	vldblocked = 1;

	/* Reread the vldb entry */
	if (!aVLDB_GetEntryByID(cellHandle, avolid, RWVOL, &entry, &tst)) {
	    goto fail_UV_BackupVolume;
	}
    }

    if (!ISNAMEVALID(entry.name)) {
	tst = VOLSERBADNAME;
	goto fail_UV_BackupVolume;
    }

    backupID = entry.volumeId[BACKVOL];
    if (backupID == INVALID_BID) {
	/* Get a backup volume id from the VLDB and update the vldb
	 * entry with it. 
	 */
	tst = ubik_VL_GetNewVolumeId(cellHandle->vos, 0, 1, &backupID);
	if (tst) {
	    goto fail_UV_BackupVolume;
	}
	entry.volumeId[BACKVOL] = backupID;
	vldbmod = 1;
    }

    /* Test to see if the backup volume exists by trying to create
     * a transaction on the backup volume. We've assumed the backup exists.
     */
    tst = AFSVolTransCreate(aconn, backupID, apart, ITOffline, &btid);
    if (tst) {
	if (tst != VNOVOL) {
	    goto fail_UV_BackupVolume;
	}
	backexists = 0;		/* backup volume does not exist */
    }
    if (btid) {
	tst = AFSVolEndTrans(aconn, btid, &rcode);
	btid = 0;
	if (tst || rcode) {
	    tst = (tst ? tst : rcode);
	    goto fail_UV_BackupVolume;
	}
    }

    /* Now go ahead and try to clone the RW volume.
     * First start a transaction on the RW volume 
     */
    tst = AFSVolTransCreate(aconn, avolid, apart, ITBusy, &ttid);
    if (tst) {
	goto fail_UV_BackupVolume;
    }

    /* Clone or reclone the volume, depending on whether the backup 
     * volume exists or not
     */
    if (backexists) {
	tst = AFSVolReClone(aconn, ttid, backupID);
	if (tst) {
	    goto fail_UV_BackupVolume;
	}
    } else {
	strcpy(vname, entry.name);
	strcat(vname, ".backup");

	tst = AFSVolClone(aconn, ttid, 0, backupVolume, vname, &backupID);
	if (tst) {
	    goto fail_UV_BackupVolume;
	}
    }

    /* End transaction on the RW volume */
    tst = AFSVolEndTrans(aconn, ttid, &rcode);
    ttid = 0;
    if (tst || rcode) {
	tst = (tst ? tst : rcode);
	goto fail_UV_BackupVolume;
    }

    /* Mork vldb as backup exists */
    if (!(entry.flags & BACK_EXISTS)) {
	entry.flags |= BACK_EXISTS;
	vldbmod = 1;
    }

    /* Now go back to the backup volume and bring it on line */
    tst = AFSVolTransCreate(aconn, backupID, apart, ITOffline, &btid);
    if (tst) {
	goto fail_UV_BackupVolume;
    }

    tst = AFSVolSetFlags(aconn, btid, 0);
    if (tst) {
	goto fail_UV_BackupVolume;
    }

    tst = AFSVolEndTrans(aconn, btid, &rcode);
    btid = 0;
    if (tst || rcode) {
	tst = (tst ? tst : rcode);
	goto fail_UV_BackupVolume;
    }
    rc = 1;

    /* Will update the vldb below */

  fail_UV_BackupVolume:

    if (ttid) {
	temp = AFSVolEndTrans(aconn, ttid, &rcode);
	if (temp || rcode) {
	    if (!tst)
		tst = (temp ? temp : rcode);
	}
    }

    if (btid) {
	temp = AFSVolEndTrans(aconn, btid, &rcode);
	if (temp || rcode) {
	    if (!tst)
		tst = (temp ? temp : rcode);
	}
    }

    /* Now update the vldb - if modified */
    if (vldblocked) {
	if (vldbmod) {
	    if (!VLDB_ReplaceEntry
		(cellHandle, avolid, RWVOL, &entry,
		 (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP),
		 &temp)) {
		if (!tst) {
		    tst = temp;
		}
	    }
	} else {
	    temp =
		ubik_VL_ReleaseLock(cellHandle->vos, 0, avolid, RWVOL,
			  (LOCKREL_OPCODE | LOCKREL_AFSID |
			   LOCKREL_TIMESTAMP));
	    if (temp) {
		if (!tst) {
		    tst = temp;
		}
	    }
	}
    }

    if (aconn) {
	rx_ReleaseCachedConnection(aconn);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
DelVol(struct rx_connection *conn, afs_int32 vid, afs_int32 part,
       afs_int32 flags)
{
    afs_int32 acode, ccode, rcode, tid;
    ccode = rcode = tid = 0;

    acode = AFSVolTransCreate(conn, vid, part, flags, &tid);
    if (!acode) {		/* It really was there */
	acode = AFSVolDeleteVolume(conn, tid);
	ccode = AFSVolEndTrans(conn, tid, &rcode);
	if (!ccode)
	    ccode = rcode;
    }

    return acode;
}

#define ONERROR(ec, ep, es) if (ec) { fprintf(STDERR, (es), (ep)); error = (ec); goto rfail; }
#define ERROREXIT(ec) { error = (ec); goto rfail; }

#if 0				/* doesn't appear to be used, why compile it */
static int
CloneVol(afs_cell_handle_p cellHandle, struct rx_connection *conn,
	 afs_int32 rwvid, afs_int32 part, afs_int32 * rovidp, int nottemp,
	 struct nvldbentry *entry, afs_int32 * vidCreateDate, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0, etst = 0;
    afs_int32 rcode = 0, tid = 0;
    struct volser_status volstatus;
    char vname[64];

    /* Begin transaction on RW volume marking it busy (clients will wait) */
    tst = AFSVolTransCreate(conn, rwvid, part, ITBusy, &tid);
    if (tst) {
	goto fail_CloneVol;
    }

    /* Get the RO volume id. Allocate a new one if need to */
    *rovidp = entry->volumeId[ROVOL];
    if (*rovidp == INVALID_BID) {
	tst = ubik_VL_GetNewVolumeId(cellHandle->vos, 0, 1, rovidp);
	if (tst) {
	    goto fail_CloneVol;
	}

	entry->volumeId[ROVOL] = *rovidp;
    }

    /* If we are creating the ro clone, what are we calling it.
     * Depends on whether its a temporary clone or not.
     */
    if (nottemp) {
	strcpy(vname, entry->name);
	strcat(vname, ".readonly");
    } else {
	strcpy(vname, "readonly-clone-temp");	/* Should be unique? */
    }

    /* Create the new clone. If it exists, then reclone it */
    tst = AFSVolClone(conn, tid, 0, readonlyVolume, vname, rovidp);
    if (tst == VVOLEXISTS) {
	tst = AFSVolReClone(conn, tid, *rovidp);
	if (tst) {
	    goto fail_CloneVol;
	}
    }
    if (tst) {
	goto fail_CloneVol;
    }

    /* Bring the volume back on-line as soon as possible */
    if (nottemp) {
	afs_int32 fromtid = 0;

	/* Now bring the RO clone on-line */
	tst = AFSVolTransCreate(conn, *rovidp, part, ITOffline, &fromtid);
	if (tst) {
	    goto fail_CloneVol;
	}

	tst = AFSVolSetFlags(conn, fromtid, 0);
	if (tst) {
	    goto fail_CloneVol;
	}

	tst = AFSVolEndTrans(conn, fromtid, &rcode);
	fromtid = 0;
	if (!tst)
	    tst = rcode;
	if (tst) {
	    goto fail_CloneVol;
	}
    }

    /* Get the time the RW was created for return information */
    tst = AFSVolGetStatus(conn, tid, &volstatus);
    if (tst) {
	goto fail_CloneVol;
    }
    *vidCreateDate = volstatus.creationDate;
    rc = 1;

  fail_CloneVol:

    if (tid) {
	tst = AFSVolEndTrans(conn, tid, &rcode);
	tid = 0;
	if (!tst)
	    tst = rcode;
	if (tst) {
	    rc = 0;
	    goto fail_CloneVol;
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}
#endif

/* Get a "transaction" on this replica.  Create the volume 
 * if necessary.  Return the time from which a dump should
 * be made (0 if it's a new volume)
 */
static int
GetTrans(afs_cell_handle_p cellHandle, struct nvldbentry *vldbEntryPtr,
	 afs_int32 index, struct rx_connection **connPtr,
	 afs_int32 * transPtr, afs_int32 * timePtr, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0, etst = 0;
    afs_int32 volid;
    struct volser_status tstatus;
    int rcode;

    *connPtr = (struct rx_connection *)0;
    *timePtr = 0;
    *transPtr = 0;

    /* get connection to the replication site */
    *connPtr =
	UV_Bind(cellHandle, vldbEntryPtr->serverNumber[index],
		AFSCONF_VOLUMEPORT);
    if (!*connPtr) {
	/* server is down */
	tst = -1;
	goto fail_GetTrans;
    }

    volid = vldbEntryPtr->volumeId[ROVOL];
    if (volid) {
	tst =
	    AFSVolTransCreate(*connPtr, volid,
			      vldbEntryPtr->serverPartition[index], ITOffline,
			      transPtr);
    }

    /* If the volume does not exist, create it */
    if (!volid || tst) {
	char volname[64];

	if (volid && (tst != VNOVOL)) {
	    goto fail_GetTrans;
	}

	strcpy(volname, vldbEntryPtr->name);
	strcat(volname, ".readonly");

	tst =
	    AFSVolCreateVolume(*connPtr, vldbEntryPtr->serverPartition[index],
			       volname, volser_RO,
			       vldbEntryPtr->volumeId[RWVOL], &volid,
			       transPtr);
	if (tst) {
	    goto fail_GetTrans;
	}
	vldbEntryPtr->volumeId[ROVOL] = volid;

	/* The following is a bit redundant, since create sets these flags by default */
	tst =
	    AFSVolSetFlags(*connPtr, *transPtr,
			   VTDeleteOnSalvage | VTOutOfService);
	if (tst) {
	    goto fail_GetTrans;
	}
    }

    /* Otherwise, the transaction did succeed, so get the creation date of the
     * latest RO volume on the replication site 
     */
    else {
	tst = AFSVolGetStatus(*connPtr, *transPtr, &tstatus);
	if (tst) {
	    goto fail_GetTrans;
	}
	*timePtr = tstatus.creationDate - CLOCKSKEW;
    }
    rc = 1;

  fail_GetTrans:

    if ((rc == 0) && (*transPtr)) {
	etst = AFSVolEndTrans(*connPtr, *transPtr, &rcode);
	*transPtr = 0;
	if (!etst)
	    etst = rcode;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
SimulateForwardMultiple(struct rx_connection *fromconn, afs_int32 fromtid,
			afs_int32 fromdate, manyDests * tr, afs_int32 flags,
			void *cookie, manyResults * results)
{
    int i;

    for (i = 0; i < tr->manyDests_len; i++) {
	results->manyResults_val[i] =
	    AFSVolForward(fromconn, fromtid, fromdate,
			  &(tr->manyDests_val[i].server),
			  tr->manyDests_val[i].trans, cookie);
    }
    return 0;
}


/* VolumeExists()
 *      Determine if a volume exists on a server and partition.
 *      Try creating a transaction on the volume. If we can,
 *      the volume exists, if not, then return the error code.
 *      Some error codes mean the volume is unavailable but
 *      still exists - so we catch these error codes.
 */
static afs_int32
VolumeExists(afs_cell_handle_p cellHandle, afs_int32 server,
	     afs_int32 partition, afs_int32 volumeid, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct rx_connection *conn = (struct rx_connection *)0;
    volEntries volumeInfo;

    conn = UV_Bind(cellHandle, server, AFSCONF_VOLUMEPORT);
    if (conn) {
	volumeInfo.volEntries_val = (volintInfo *) 0;
	volumeInfo.volEntries_len = 0;
	tst = AFSVolListOneVolume(conn, partition, volumeid, &volumeInfo);
	if (volumeInfo.volEntries_val)
	    free(volumeInfo.volEntries_val);
	if (tst == VOLSERILLEGAL_PARTITION) {
	    tst = ENODEV;
	}
	rx_ReleaseCachedConnection(conn);
    }
    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/* release volume <afromvol> on <afromserver> <afrompart> to all the
 * sites if forceflag is 1.If its 0 complete the release if the previous
 * release aborted else start a new release */
int
UV_ReleaseVolume(afs_cell_handle_p cellHandle, afs_int32 afromvol,
		 afs_int32 afromserver, afs_int32 afrompart, int forceflag,
		 afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0, etst = 0;

    char vname[64];
    afs_int32 rcode;
    afs_int32 cloneVolId, roVolId;
    struct replica *replicas = 0;
    struct nvldbentry entry;
    int i, volcount, m, fullrelease, vldbindex;
    int failure;
    struct restoreCookie cookie;
    struct rx_connection **toconns = 0;
    struct release *times = 0;
    int nservers = 0;
    struct rx_connection *fromconn = (struct rx_connection *)0;
    int islocked = 0;
    afs_int32 clonetid = 0, onlinetid;
    afs_int32 fromtid = 0;
    afs_uint32 fromdate = 0, thisdate;
    int s;
    manyDests tr;
    manyResults results;
    int rwindex, roindex, roclone, roexists;
    afs_int32 rwcrdate = 0;
    struct rtime {
	int validtime;
	afs_uint32 time;
    } remembertime[NMAXNSERVERS];
    int releasecount = 0;
    struct volser_status volstatus;

    memset((char *)remembertime, 0, sizeof(remembertime));
    memset((char *)&results, 0, sizeof(results));

    tst =
	ubik_VL_SetLock(cellHandle->vos, 0, afromvol, RWVOL,
		  VLOP_RELEASE);
    if ((tst) && (tst != VL_RERELEASE)) {
	goto fail_UV_ReleaseVolume;
    }
    islocked = 1;

    /* Get the vldb entry in readable format */
    if (!aVLDB_GetEntryByID(cellHandle, afromvol, RWVOL, &entry, &tst)) {
	goto fail_UV_ReleaseVolume;
    }

    if (!ISNAMEVALID(entry.name)) {
	tst = VOLSERBADNAME;
	goto fail_UV_ReleaseVolume;
    }

    if (entry.volumeId[RWVOL] != afromvol) {
	tst = ADMVOSVOLUMERELEASERWONLY;
	goto fail_UV_ReleaseVolume;
    }

    if (entry.nServers <= 1) {
	tst = ADMVOSVOLUMENOREPLICAS;
	goto fail_UV_ReleaseVolume;
    }

    if (strlen(entry.name) > (VOLSER_OLDMAXVOLNAME - 10)) {
	tst = VOLSERBADNAME;
	goto fail_UV_ReleaseVolume;
    }

    /* roclone is true if one of the RO volumes is on the same
     * partition as the RW volume. In this case, we make the RO volume
     * on the same partition a clone instead of a complete copy.
     */

    roindex =
	Lp_ROMatch(cellHandle, &entry, afromserver, afrompart, &tst) - 1;
    roclone = ((roindex == -1) ? 0 : 1);
    rwindex = Lp_GetRwIndex(cellHandle, &entry, 0);
    if (rwindex < 0) {
	tst = VOLSERNOVOL;
	goto fail_UV_ReleaseVolume;
    }

    /* Make sure we have a RO volume id to work with */
    if (entry.volumeId[ROVOL] == INVALID_BID) {
	/* need to get a new RO volume id */
	tst = ubik_VL_GetNewVolumeId(cellHandle->vos, 0, 1, &roVolId);
	if (tst) {
	    goto fail_UV_ReleaseVolume;
	}

	entry.volumeId[ROVOL] = roVolId;
	if (!VLDB_ReplaceEntry(cellHandle, afromvol, RWVOL, &entry, 0, &tst)) {
	    goto fail_UV_ReleaseVolume;
	}
    }

    /* Will we be completing a previously unfinished release. -force overrides */
    for (fullrelease = 1, i = 0; (fullrelease && (i < entry.nServers)); i++) {
	if (entry.serverFlags[i] & NEW_REPSITE)
	    fullrelease = 0;
    }
    if (forceflag && !fullrelease)
	fullrelease = 1;

    /* Determine which volume id to use and see if it exists */
    cloneVolId =
	((fullrelease
	  || (entry.cloneId == 0)) ? entry.volumeId[ROVOL] : entry.cloneId);
    VolumeExists(cellHandle, afromserver, afrompart, cloneVolId, &tst);
    roexists = ((tst == ENODEV) ? 0 : 1);
    if (!roexists && !fullrelease)
	fullrelease = 1;	/* Do a full release if RO clone does not exist */

    fromconn = UV_Bind(cellHandle, afromserver, AFSCONF_VOLUMEPORT);
    if (!fromconn) {
	tst = -1;
	goto fail_UV_ReleaseVolume;
    }

    if (fullrelease) {
	/* If the RO clone exists, then if the clone is a temporary
	 * clone, delete it. Or if the RO clone is marked RO_DONTUSE
	 * (it was recently added), then also delete it. We do not
	 * want to "reclone" a temporary RO clone.
	 */
	if (roexists
	    && (!roclone || (entry.serverFlags[roindex] & RO_DONTUSE))) {
	    tst = DelVol(fromconn, cloneVolId, afrompart, ITOffline);
	    if (tst && (tst != VNOVOL)) {
		goto fail_UV_ReleaseVolume;
	    }
	    roexists = 0;
	}

	/* Mark all the ROs in the VLDB entry as RO_DONTUSE. We don't
	 * write this entry out to the vlserver until after the first
	 * RO volume is released (temp RO clones don't count).
	 */
	for (i = 0; i < entry.nServers; i++) {
	    entry.serverFlags[i] &= ~NEW_REPSITE;
	    entry.serverFlags[i] |= RO_DONTUSE;
	}
	entry.serverFlags[rwindex] |= NEW_REPSITE;
	entry.serverFlags[rwindex] &= ~RO_DONTUSE;

	/* Begin transaction on RW and mark it busy while we clone it */
	tst =
	    AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy,
			      &clonetid);
	if (tst) {
	    goto fail_UV_ReleaseVolume;
	}

	/* Clone or reclone the volume */
	if (roexists) {
	    tst = AFSVolReClone(fromconn, clonetid, cloneVolId);
	    if (tst) {
		goto fail_UV_ReleaseVolume;
	    }
	} else {
	    if (roclone) {
		strcpy(vname, entry.name);
		strcat(vname, ".readonly");
	    } else {
		strcpy(vname, "readonly-clone-temp");
	    }
	    tst =
		AFSVolClone(fromconn, clonetid, 0, readonlyVolume, vname,
			    &cloneVolId);
	    if (tst) {
		goto fail_UV_ReleaseVolume;
	    }
	}

	/* Get the time the RW was created for future information */
	tst = AFSVolGetStatus(fromconn, clonetid, &volstatus);
	if (tst) {
	    goto fail_UV_ReleaseVolume;
	}
	rwcrdate = volstatus.creationDate;

	/* End the transaction on the RW volume */
	tst = AFSVolEndTrans(fromconn, clonetid, &rcode);
	clonetid = 0;
	tst = (tst ? tst : rcode);
	if (tst) {
	    goto fail_UV_ReleaseVolume;
	}

	/* Remember clone volume ID in case we fail or are interrupted */
	entry.cloneId = cloneVolId;

	if (roclone) {
	    /* Bring the RO clone online - though not if it's a temporary clone */
	    tst =
		AFSVolTransCreate(fromconn, cloneVolId, afrompart, ITOffline,
				  &onlinetid);
	    if (tst) {
		goto fail_UV_ReleaseVolume;
	    }

	    etst = AFSVolSetFlags(fromconn, onlinetid, 0);

	    tst = AFSVolEndTrans(fromconn, onlinetid, &rcode);
	    tst = (tst ? tst : rcode);
	    if (tst) {
		goto fail_UV_ReleaseVolume;
	    }
	    if (etst) {
		tst = etst;
		goto fail_UV_ReleaseVolume;
	    }

	    /* Sleep so that a client searching for an online volume won't
	     * find the clone offline and then the next RO offline while the 
	     * release brings the clone online and the next RO offline (race).
	     * There is a fix in the 3.4 client that does not need this sleep
	     * anymore, but we don't know what clients we have.
	     */
	    if (entry.nServers > 2)
		sleep(5);

	    /* Mark the RO clone in the VLDB as a good site (already released) */
	    entry.serverFlags[roindex] |= NEW_REPSITE;
	    entry.serverFlags[roindex] &= ~RO_DONTUSE;
	    entry.flags |= RO_EXISTS;

	    releasecount++;

	    /* Write out the VLDB entry only if the clone is not a temporary
	     * clone. If we did this to a temporary clone then we would end
	     * up marking all the ROs as "old release" making the ROs
	     * temporarily unavailable.
	     */
	    if (!VLDB_ReplaceEntry
		(cellHandle, afromvol, RWVOL, &entry, 0, &tst)) {
		goto fail_UV_ReleaseVolume;
	    }
	}
    }

    /* Now we will release from the clone to the remaining RO replicas.
     * The first 2 ROs (counting the non-temporary RO clone) are released
     * individually: releasecount. This is to reduce the race condition
     * of clients trying to find an on-line RO volume. The remaining ROs
     * are released in parallel but no more than half the number of ROs
     * (rounded up) at a time: nservers.
     */

    strcpy(vname, entry.name);
    strcat(vname, ".readonly");
    memset(&cookie, 0, sizeof(cookie));
    strncpy(cookie.name, vname, VOLSER_OLDMAXVOLNAME);
    cookie.type = ROVOL;
    cookie.parent = entry.volumeId[RWVOL];
    cookie.clone = 0;

    nservers = entry.nServers / 2;	/* how many to do at once, excluding clone */
    replicas =
	(struct replica *)malloc(sizeof(struct replica) * nservers + 1);
    times = (struct release *)malloc(sizeof(struct release) * nservers + 1);
    toconns =
	(struct rx_connection **)malloc(sizeof(struct rx_connection *) *
					nservers + 1);
    results.manyResults_val =
	(afs_int32 *) malloc(sizeof(afs_int32) * nservers + 1);
    if (!replicas || !times || !!!results.manyResults_val || !toconns) {
	tst = ADMNOMEM;
	goto fail_UV_ReleaseVolume;
    }

    memset(replicas, 0, (sizeof(struct replica) * nservers + 1));
    memset(times, 0, (sizeof(struct release) * nservers + 1));
    memset(toconns, 0, (sizeof(struct rx_connection *) * nservers + 1));
    memset(results.manyResults_val, 0, (sizeof(afs_int32) * nservers + 1));

    /* Create a transaction on the cloned volume */
    tst =
	AFSVolTransCreate(fromconn, cloneVolId, afrompart, ITBusy, &fromtid);
    if (tst) {
	goto fail_UV_ReleaseVolume;
    }

    /* For each index in the VLDB */
    for (vldbindex = 0; vldbindex < entry.nServers;) {

	/* Get a transaction on the replicas. Pick replacas which have an old release. */
	for (volcount = 0;
	     ((volcount < nservers) && (vldbindex < entry.nServers));
	     vldbindex++) {
	    /* The first two RO volumes will be released individually.
	     * The rest are then released in parallel. This is a hack
	     * for clients not recognizing right away when a RO volume
	     * comes back on-line.
	     */
	    if ((volcount == 1) && (releasecount < 2))
		break;

	    if (vldbindex == roindex)
		continue;	/* the clone    */
	    if ((entry.serverFlags[vldbindex] & NEW_REPSITE)
		&& !(entry.serverFlags[vldbindex] & RO_DONTUSE))
		continue;
	    if (!(entry.serverFlags[vldbindex] & ITSROVOL))
		continue;	/* not a RO vol */


	    /* Get a Transaction on this replica. Get a new connection if
	     * necessary.  Create the volume if necessary.  Return the
	     * time from which the dump should be made (0 if it's a new
	     * volume).  Each volume might have a different time. 
	     */
	    replicas[volcount].server.destHost =
		entry.serverNumber[vldbindex];
	    replicas[volcount].server.destPort = AFSCONF_VOLUMEPORT;
	    replicas[volcount].server.destSSID = 1;
	    times[volcount].vldbEntryIndex = vldbindex;

	    if (!GetTrans
		(cellHandle, &entry, vldbindex, &(toconns[volcount]),
		 &(replicas[volcount].trans), &(times[volcount].time),
		 &tst)) {
		continue;
	    }

	    /* Thisdate is the date from which we want to pick up all changes */
	    if (forceflag || !fullrelease
		|| (rwcrdate > times[volcount].time)) {
		/* If the forceflag is set, then we want to do a full dump.
		 * If it's not a full release, we can't be sure that the creation
		 *  date is good (so we also do a full dump).
		 * If the RW volume was replaced (its creation date is newer than
		 *  the last release), then we can't be sure what has changed (so
		 *  we do a full dump).
		 */
		thisdate = 0;
	    } else if (remembertime[vldbindex].validtime) {
		/* Trans was prev ended. Use the time from the prev trans
		 * because, prev trans may have created the volume. In which
		 * case time[volcount].time would be now instead of 0.
		 */
		thisdate =
		    (remembertime[vldbindex].time <
		     times[volcount].time) ? remembertime[vldbindex].
		    time : times[volcount].time;
	    } else {
		thisdate = times[volcount].time;
	    }
	    remembertime[vldbindex].validtime = 1;
	    remembertime[vldbindex].time = thisdate;

	    if (volcount == 0) {
		fromdate = thisdate;
	    } else {
		/* Include this volume if it is within 15 minutes of the earliest */
		if (((fromdate >
		      thisdate) ? (fromdate - thisdate) : (thisdate -
							   fromdate)) > 900) {
		    AFSVolEndTrans(toconns[volcount],
				   replicas[volcount].trans, &rcode);
		    replicas[volcount].trans = 0;
		    break;
		}
		if (thisdate < fromdate)
		    fromdate = thisdate;
	    }
	    volcount++;
	}
	if (!volcount)
	    continue;

	/* Release the ones we have collected */
	tr.manyDests_val = &(replicas[0]);
	tr.manyDests_len = results.manyResults_len = volcount;
	tst =
	    AFSVolForwardMultiple(fromconn, fromtid, fromdate, &tr,
				  0 /*spare */ , &cookie, &results);
	if (tst == RXGEN_OPCODE) {	/* RPC Interface Mismatch */
	    tst =
		SimulateForwardMultiple(fromconn, fromtid, fromdate, &tr,
					0 /*spare */ , &cookie, &results);
	    nservers = 1;
	}

	if (tst) {
	    goto fail_UV_ReleaseVolume;
	} else {
	    for (m = 0; m < volcount; m++) {
		if (results.manyResults_val[m]) {
		    continue;
		}

		tst =
		    AFSVolSetIdsTypes(toconns[m], replicas[m].trans, vname,
				      ROVOL, entry.volumeId[RWVOL], 0, 0);
		if (tst) {
		    continue;
		}

		/* have to clear dest. flags to ensure new vol goes online:
		 * because the restore (forwarded) operation copied
		 * the V_inService(=0) flag over to the destination. 
		 */
		tst = AFSVolSetFlags(toconns[m], replicas[m].trans, 0);
		if (tst) {
		    continue;
		}

		entry.serverFlags[times[m].vldbEntryIndex] |= NEW_REPSITE;
		entry.serverFlags[times[m].vldbEntryIndex] &= ~RO_DONTUSE;
		entry.flags |= RO_EXISTS;
		releasecount++;
	    }
	}

	/* End the transactions and destroy the connections */
	for (s = 0; s < volcount; s++) {
	    if (replicas[s].trans)
		tst = AFSVolEndTrans(toconns[s], replicas[s].trans, &rcode);
	    replicas[s].trans = 0;
	    if (!tst)
		tst = rcode;
	    if (tst) {
		if ((s == 0) || (tst != ENOENT)) {
		} else {
		    if (times[s].vldbEntryIndex < vldbindex)
			vldbindex = times[s].vldbEntryIndex;
		}
	    }

	    if (toconns[s])
		rx_ReleaseCachedConnection(toconns[s]);
	    toconns[s] = 0;
	}

	if (!VLDB_ReplaceEntry(cellHandle, afromvol, RWVOL, &entry, 0, &tst)) {
	    goto fail_UV_ReleaseVolume;
	}
    }				/* for each index in the vldb */

    /* End the transaction on the cloned volume */
    tst = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;

    /* Figure out if any volume were not released and say so */
    for (failure = 0, i = 0; i < entry.nServers; i++) {
	if (!(entry.serverFlags[i] & NEW_REPSITE))
	    failure++;
    }
    if (failure) {
	if (!VLDB_ReplaceEntry
	    (cellHandle, afromvol, RWVOL, &entry, LOCKREL_TIMESTAMP, &tst)) {
	    goto fail_UV_ReleaseVolume;
	}

	tst = VOLSERBADRELEASE;
	goto fail_UV_ReleaseVolume;
    }

    /* All the ROs were release successfully. Remove the temporary clone */
    if (!roclone) {
	tst = DelVol(fromconn, cloneVolId, afrompart, ITOffline);
	if (tst) {
	    goto fail_UV_ReleaseVolume;
	}
    }
    entry.cloneId = 0;

    for (i = 0; i < entry.nServers; i++)
	entry.serverFlags[i] &= ~NEW_REPSITE;

    /* Update the VLDB */
    if (!VLDB_ReplaceEntry
	(cellHandle, afromvol, RWVOL, &entry,
	 LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP, &tst)) {
	goto fail_UV_ReleaseVolume;
    }
    rc = 1;

  fail_UV_ReleaseVolume:

    if (clonetid) {
	tst = AFSVolEndTrans(fromconn, clonetid, &rcode);
	clonetid = 0;
	if (tst) {
	    rc = 0;
	}
    }
    if (fromtid) {
	tst = AFSVolEndTrans(fromconn, fromtid, &rcode);
	fromtid = 0;
	if (tst) {
	    rc = 0;
	}
    }
    for (i = 0; i < nservers; i++) {
	if (replicas && replicas[i].trans) {
	    tst = AFSVolEndTrans(toconns[i], replicas[i].trans, &rcode);
	    replicas[i].trans = 0;
	    if (tst) {
		rc = 0;
	    }
	}
	if (toconns && toconns[i]) {
	    rx_ReleaseCachedConnection(toconns[i]);
	    toconns[i] = 0;
	}
    }
    if (islocked) {
	tst =
	    ubik_VL_ReleaseLock(cellHandle->vos, 0, afromvol, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (tst) {
	    rc = 0;
	}
    }

    if (fromconn)
	rx_ReleaseCachedConnection(fromconn);
    if (results.manyResults_val)
	free(results.manyResults_val);
    if (replicas)
	free(replicas);
    if (toconns)
	free(toconns);
    if (times)
	free(times);

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static int
ReceiveFile(register int fd, register struct rx_call *call,
	    register struct stat *status)
{
    register char *buffer = (char *)0;
    register int blockSize;
    afs_int32 bytesread, nbytes, bytesleft, w;
    fd_set out;
    afs_int32 error = 0;

#ifdef AFS_NT40_ENV
    blockSize = 4096;
#else
    if (fd != 1) {
#ifdef  AFS_AIX_ENV
	struct statfs tstatfs;

/* Unfortunately in AIX valuable fields such as st_blksize are gone from the sta
t structure!! */
	fstatfs(fd, &tstatfs);
	blockSize = tstatfs.f_bsize;
#else
	blockSize = status->st_blksize;
#endif
    } else {
	blockSize = 4096;
    }
#endif
    nbytes = blockSize;
    buffer = (char *)malloc(blockSize);
    if (!buffer) {
	return ADMNOMEM;
    }
    bytesread = 1;
    while (!error && (bytesread > 0)) {
	bytesread = rx_Read(call, buffer, nbytes);
	bytesleft = bytesread;
	while (!error && (bytesleft > 0)) {
	    FD_ZERO(&out);
	    FD_SET(fd, &out);
#ifndef AFS_NT40_ENV		/* NT csn't select on non-socket fd's */
	    select(fd + 1, 0, &out, 0, 0);	/* don't timeout if write bl
						 * ocks */
#endif
	    w = write(fd, &buffer[bytesread - bytesleft], bytesleft);
	    if (w < 0) {
		error = ADMVOSDUMPFILEWRITEFAIL;
	    } else {
		bytesleft -= w;
	    }
	}
    }
    if (buffer)
	free(buffer);
    if (fd != 1)
	if (!error)
	    fstat(fd, status);
    return error;
}


static afs_int32
DumpFunction(struct rx_call *call, const char *filename)
{
    int fd;
    struct stat status;
    afs_int32 error, code;

    error = 0;
    fd = -1;

    fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd < 0 || fstat(fd, &status) < 0) {
	error = VOLSERBADOP;
	goto dffail;
    }
    code = ReceiveFile(fd, call, &status);
    if (code) {
	error = code;
	goto dffail;
    }
  dffail:
    if (fd >= 0)
	code = close(fd);
    else
	code = 0;
    if (code) {
	if (!error)
	    error = code;
    }
    return error;
}


/*dump the volume <afromvol> on <afromserver> and
* <afrompart> to <afilename> starting from <fromdate>.
* DumpFunction does the real work behind the scenes after
* extracting parameters from the rock  */
int
UV_DumpVolume(afs_cell_handle_p cellHandle, afs_int32 afromvol,
	      afs_int32 afromserver, afs_int32 afrompart, afs_int32 fromdate,
	      const char *filename, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_status_t etst = 0;
    struct rx_connection *fromconn;
    struct rx_call *fromcall;
    afs_int32 fromtid;
    afs_int32 rxError;
    afs_int32 rcode;

    struct nvldbentry entry;
    afs_int32 error;
    int islocked;

    islocked = 0;
    error = 0;
    rxError = 0;
    fromcall = (struct rx_call *)0;
    fromconn = (struct rx_connection *)0;
    fromtid = 0;
    fromcall = (struct rx_call *)0;

    islocked = 0;
    if (!aVLDB_GetEntryByID(cellHandle, afromvol, -1, &entry, &tst)) {
	goto fail_UV_DumpVolume;
    }

    /* get connections to the servers */
    fromconn = UV_Bind(cellHandle, afromserver, AFSCONF_VOLUMEPORT);
    tst = AFSVolTransCreate(fromconn, afromvol, afrompart, ITBusy, &fromtid);
    if (tst) {
	goto fail_UV_DumpVolume;
    }
    fromcall = rx_NewCall(fromconn);
    tst = StartAFSVolDump(fromcall, fromtid, fromdate);
    if (tst) {
	goto fail_UV_DumpVolume;
    }
    if ((tst = DumpFunction(fromcall, filename))) {
	goto fail_UV_DumpVolume;
    }
    tst = rx_EndCall(fromcall, rxError);
    fromcall = (struct rx_call *)0;
    if (tst) {
	goto fail_UV_DumpVolume;
    }
    tst = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;
    if (!tst)
	tst = rcode;
    if (tst) {
	goto fail_UV_DumpVolume;
    }
    rc = 1;

  fail_UV_DumpVolume:

    if (islocked) {
	etst =
	    ubik_VL_ReleaseLock(cellHandle->vos, 0, afromvol, -1,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (etst) {
	    if (!tst)
		tst = etst;
	}
    }

    if (fromcall) {
	etst = rx_EndCall(fromcall, rxError);
	if (etst) {
	    if (!tst)
		tst = etst;
	}
    }

    if (fromtid) {
	etst = AFSVolEndTrans(fromconn, fromtid, &rcode);
	if (!tst)
	    tst = etst;
	if (rcode) {
	    if (!tst)
		tst = rcode;
	}
    }

    if (fromconn) {
	rx_ReleaseCachedConnection(fromconn);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

int
SendFile(register int fd, register struct rx_call *call,
	 register struct stat *status)
{
    char *buffer = (char *)0;
    int blockSize;
    fd_set in;
    afs_int32 error = 0;
    int done = 0;
    int nbytes;

#ifdef AFS_NT40_ENV
    blockSize = 4096;
#else
    if (fd != 0) {
#ifdef  AFS_AIX_ENV
	struct statfs tstatfs;

/* Unfortunately in AIX valuable fields such as st_blksize are gone from the sta
t structure!! */
	fstatfs(fd, &tstatfs);
	blockSize = tstatfs.f_bsize;
#else
	blockSize = status->st_blksize;
#endif
    } else {
	blockSize = 4096;
    }
#endif
    buffer = (char *)malloc(blockSize);
    if (!buffer) {
	return ADMNOMEM;
    }

    while (!error && !done) {
	FD_ZERO(&in);
	FD_SET(fd, &in);
#ifndef AFS_NT40_ENV		/* NT csn't select on non-socket fd's */
	select(fd + 1, &in, 0, 0, 0);	/* don't timeout if read blocks */
#endif
	nbytes = read(fd, buffer, blockSize);
	if (nbytes < 0) {
	    error = ADMVOSRESTOREFILEREADFAIL;
	    break;
	}
	if (nbytes == 0) {
	    done = 1;
	    break;
	}
	if (rx_Write(call, buffer, nbytes) != nbytes) {
	    error = ADMVOSRESTOREFILEWRITEFAIL;
	    break;
	}
    }
    if (buffer)
	free(buffer);
    return error;
}

static afs_int32
WriteData(struct rx_call *call, const char *filename)
{
    int fd;
    struct stat status;
    afs_int32 error, code;

    error = 0;
    fd = -1;

    fd = open(filename, 0);
    if (fd < 0 || fstat(fd, &status) < 0) {
	fprintf(STDERR, "Could access file '%s'\n", filename);
	error = ADMVOSRESTOREFILEOPENFAIL;
	goto fail_WriteData;
    }
    code = SendFile(fd, call, &status);
    if (code) {
	error = code;
	goto fail_WriteData;
    }

  fail_WriteData:

    if (fd >= 0)
	code = close(fd);
    else
	code = 0;
    if (code) {
	if (!error)
	    error = ADMVOSRESTOREFILECLOSEFAIL;
    }
    return error;
}

/*
 * Restore a volume <tovolid> <tovolname> on <toserver> <topart> from
 * the dump file <afilename>. WriteData does all the real work
 * after extracting params from the rock 
 */
int
UV_RestoreVolume(afs_cell_handle_p cellHandle, afs_int32 toserver,
		 afs_int32 topart, afs_int32 tovolid, const char *tovolname,
		 int flags, const char *dumpFile, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_status_t etst = 0;
    struct rx_connection *toconn, *tempconn;
    struct rx_call *tocall;
    afs_int32 totid, rcode;
    afs_int32 rxError = 0;
    struct volser_status tstatus;
    char partName[10];
    afs_int32 pvolid;
    afs_int32 temptid;
    int success;
    struct nvldbentry entry;
    afs_int32 error;
    int islocked;
    struct restoreCookie cookie;
    int reuseID;
    afs_int32 newDate, volflag;
    int index, same;


    memset(&cookie, 0, sizeof(cookie));
    islocked = 0;
    success = 0;
    error = 0;
    reuseID = 1;
    tocall = (struct rx_call *)0;
    toconn = (struct rx_connection *)0;
    tempconn = (struct rx_connection *)0;
    totid = 0;
    temptid = 0;

    pvolid = tovolid;
    toconn = UV_Bind(cellHandle, toserver, AFSCONF_VOLUMEPORT);
    if (pvolid == 0) {		/*alot a new id if needed */
	aVLDB_GetEntryByName(cellHandle, tovolname, &entry, &tst);
	if (tst == VL_NOENT) {
	    tst =
		ubik_VL_GetNewVolumeId(cellHandle->vos, 0, 1, &pvolid);
	    if (tst) {
		goto fail_UV_RestoreVolume;
	    }
	    reuseID = 0;
	} else {
	    pvolid = entry.volumeId[RWVOL];
	}
    }

    /* 
     * at this point we have a volume id to use/reuse for the
     * volume to be restored
     */
    if (strlen(tovolname) > (VOLSER_OLDMAXVOLNAME - 1)) {
	tst = ADMVOSRESTOREVOLUMENAMETOOBIG;
	goto fail_UV_RestoreVolume;
    }

    if (!vos_PartitionIdToName(topart, partName, &tst)) {
	goto fail_UV_RestoreVolume;
    }
    /*what should the volume be restored as ? rw or ro or bk ?
     * right now the default is rw always */
    tst =
	AFSVolCreateVolume(toconn, topart, tovolname, volser_RW, 0, &pvolid,
			   &totid);
    if (tst) {
	if (flags & RV_FULLRST) {	/* full restore: delete then create anew */
	    tst =
		AFSVolTransCreate(toconn, pvolid, topart, ITOffline, &totid);
	    if (tst) {
		goto fail_UV_RestoreVolume;
	    }
	    tst =
		AFSVolSetFlags(toconn, totid,
			       VTDeleteOnSalvage | VTOutOfService);
	    if (tst) {
		goto fail_UV_RestoreVolume;
	    }
	    tst = AFSVolDeleteVolume(toconn, totid);
	    if (tst) {
		goto fail_UV_RestoreVolume;
	    }
	    tst = AFSVolEndTrans(toconn, totid, &rcode);
	    totid = 0;
	    if (!tst)
		tst = rcode;
	    if (tst) {
		goto fail_UV_RestoreVolume;
	    }
	    tst =
		AFSVolCreateVolume(toconn, topart, tovolname, volser_RW, 0,
				   &pvolid, &totid);
	    if (tst) {
		goto fail_UV_RestoreVolume;
	    }
	} else {
	    tst =
		AFSVolTransCreate(toconn, pvolid, topart, ITOffline, &totid);
	    if (tst) {
		goto fail_UV_RestoreVolume;
	    }
	}
    }
    cookie.parent = pvolid;
    cookie.type = RWVOL;
    cookie.clone = 0;
    strncpy(cookie.name, tovolname, VOLSER_OLDMAXVOLNAME);

    tocall = rx_NewCall(toconn);
    tst = StartAFSVolRestore(tocall, totid, 1, &cookie);
    if (tst) {
	goto fail_UV_RestoreVolume;
    }
    tst = WriteData(tocall, dumpFile);
    if (tst) {
	goto fail_UV_RestoreVolume;
    }
    tst = rx_EndCall(tocall, rxError);
    tocall = (struct rx_call *)0;
    if (tst) {
	goto fail_UV_RestoreVolume;
    }
    tst = AFSVolGetStatus(toconn, totid, &tstatus);
    if (tst) {
	goto fail_UV_RestoreVolume;
    }
    tst = AFSVolSetIdsTypes(toconn, totid, tovolname, RWVOL, pvolid, 0, 0);
    if (tst) {
	goto fail_UV_RestoreVolume;
    }
    newDate = time(0);
    tst = AFSVolSetDate(toconn, totid, newDate);
    if (tst) {
	goto fail_UV_RestoreVolume;
    }

    volflag = ((flags & RV_OFFLINE) ? VTOutOfService : 0);	/* off or on-line */
    tst = AFSVolSetFlags(toconn, totid, volflag);
    if (tst) {
	goto fail_UV_RestoreVolume;
    }

/* It isn't handled right in fail_UV_RestoreVolume */
    tst = AFSVolEndTrans(toconn, totid, &rcode);
    totid = 0;
    if (!tst)
	tst = rcode;
    if (tst) {
	goto fail_UV_RestoreVolume;
    }

    success = 1;
    if (success && (!reuseID || (flags & RV_FULLRST))) {
	/* Volume was restored on the file server, update the 
	 * VLDB to reflect the change.
	 */
	aVLDB_GetEntryByID(cellHandle, pvolid, RWVOL, &entry, &tst);
	if (tst && tst != VL_NOENT && tst != VL_ENTDELETED) {
	    goto fail_UV_RestoreVolume;
	}
	if (tst == VL_NOENT) {	/* it doesnot exist already */
	    /*make the vldb return this indication specifically */
	    strcpy(entry.name, tovolname);
	    entry.nServers = 1;
	    entry.serverNumber[0] = toserver;	/*should be indirect */
	    entry.serverPartition[0] = topart;
	    entry.serverFlags[0] = ITSRWVOL;
	    entry.flags = RW_EXISTS;
	    if (tstatus.cloneID != 0) {
		entry.volumeId[ROVOL] = tstatus.cloneID;	/*this should come from status info on the volume if non zero */
	    } else
		entry.volumeId[ROVOL] = INVALID_BID;
	    entry.volumeId[RWVOL] = pvolid;
	    entry.cloneId = 0;
	    if (tstatus.backupID != 0) {
		entry.volumeId[BACKVOL] = tstatus.backupID;
		/*this should come from status info on the volume if non zero */
	    } else
		entry.volumeId[BACKVOL] = INVALID_BID;
	    if (!VLDB_CreateEntry(cellHandle, &entry, &tst)) {
		goto fail_UV_RestoreVolume;
	    }
	    islocked = 0;
	} else {		/*update the existing entry */
	    tst =
		ubik_VL_SetLock(cellHandle->vos, 0, pvolid, RWVOL,
			  VLOP_RESTORE);
	    if (tst) {
		goto fail_UV_RestoreVolume;
	    }
	    islocked = 1;
	    strcpy(entry.name, tovolname);

	    /* Update the vlentry with the new information */
	    index = Lp_GetRwIndex(cellHandle, &entry, 0);
	    if (index == -1) {
		/* Add the rw site for the volume being restored */
		entry.serverNumber[entry.nServers] = toserver;
		entry.serverPartition[entry.nServers] = topart;
		entry.serverFlags[entry.nServers] = ITSRWVOL;
		entry.nServers++;
	    } else {
		/* This volume should be deleted on the old site
		 * if its different from new site.
		 */
		VLDB_IsSameAddrs(cellHandle, toserver,
				 entry.serverNumber[index], &same, &tst);
		if ((!tst && !same)
		    || (entry.serverPartition[index] != topart)) {
		    tempconn =
			UV_Bind(cellHandle, entry.serverNumber[index],
				AFSCONF_VOLUMEPORT);
		    tst =
			AFSVolTransCreate(tempconn, pvolid,
					  entry.serverPartition[index],
					  ITOffline, &temptid);
		    if (!tst) {
			tst =
			    AFSVolSetFlags(tempconn, temptid,
					   VTDeleteOnSalvage |
					   VTOutOfService);
			if (tst) {
			    goto fail_UV_RestoreVolume;
			}
			tst = AFSVolDeleteVolume(tempconn, temptid);
			if (tst) {
			    goto fail_UV_RestoreVolume;
			}
			tst = AFSVolEndTrans(tempconn, temptid, &rcode);
			temptid = 0;
			if (!tst)
			    tst = rcode;
			if (tst) {
			    goto fail_UV_RestoreVolume;
			}
			vos_PartitionIdToName(entry.serverPartition[index],
					      partName, &tst);
		    }
		}
		entry.serverNumber[index] = toserver;
		entry.serverPartition[index] = topart;
	    }

	    entry.flags |= RW_EXISTS;
	    if (!VLDB_ReplaceEntry
		(cellHandle, pvolid, RWVOL, &entry,
		 LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP, &tst)) {
		goto fail_UV_RestoreVolume;
	    }
	    islocked = 0;
	}
    }
    rc = 1;

  fail_UV_RestoreVolume:

    if (tocall) {
	etst = rx_EndCall(tocall, rxError);
	if (!tst)
	    tst = etst;
    }
    if (islocked) {
	etst =
	    ubik_VL_ReleaseLock(cellHandle->vos, 0, pvolid, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (etst) {
	    if (!tst)
		tst = etst;
	}
    }
    if (totid) {
	etst = AFSVolEndTrans(toconn, totid, &rcode);
	if (!etst)
	    etst = rcode;
	if (etst) {
	    if (!tst)
		tst = etst;
	}
    }
    if (temptid) {
	etst = AFSVolEndTrans(toconn, temptid, &rcode);
	if (!etst)
	    etst = rcode;
	if (etst) {
	    if (!tst)
		tst = etst;
	}
    }

    if (tempconn)
	rx_ReleaseCachedConnection(tempconn);
    if (toconn)
	rx_ReleaseCachedConnection(toconn);

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*adds <server> and <part> as a readonly replication site for <volid>
*in vldb */
int
UV_AddSite(afs_cell_handle_p cellHandle, afs_int32 server, afs_int32 part,
	   afs_int32 volid, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int j, nro = 0, islocked = 0;
    struct nvldbentry entry;
    int same = 0;

    tst =
	ubik_VL_SetLock(cellHandle->vos, 0, volid, RWVOL, VLOP_ADDSITE);
    if (tst) {
	goto fail_UV_AddSite;
    }
    islocked = 1;

    if (!aVLDB_GetEntryByID(cellHandle, volid, RWVOL, &entry, &tst)) {
	goto fail_UV_AddSite;
    }
    if (!ISNAMEVALID(entry.name)) {
	tst = VOLSERBADOP;
	goto fail_UV_AddSite;
    }

    /* See if it's too many entries */
    if (entry.nServers >= NMAXNSERVERS) {
	tst = VOLSERBADOP;
	goto fail_UV_AddSite;
    }

    /* See if it's on the same server */
    for (j = 0; j < entry.nServers; j++) {
	if (entry.serverFlags[j] & ITSROVOL) {
	    nro++;
	    if (!VLDB_IsSameAddrs
		(cellHandle, server, entry.serverNumber[j], &same, &tst)) {
		goto fail_UV_AddSite;
	    }
	    if (same) {
		tst = VOLSERBADOP;
		goto fail_UV_AddSite;
	    }
	}
    }

    /* See if it's too many RO sites - leave one for the RW */
    if (nro >= NMAXNSERVERS - 1) {
	tst = VOLSERBADOP;
	goto fail_UV_AddSite;
    }

    entry.serverNumber[entry.nServers] = server;
    entry.serverPartition[entry.nServers] = part;
    entry.serverFlags[entry.nServers] = (ITSROVOL | RO_DONTUSE);
    entry.nServers++;

    if (!VLDB_ReplaceEntry
	(cellHandle, volid, RWVOL, &entry,
	 LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP, &tst)) {
	goto fail_UV_AddSite;
    }
    islocked = 0;
    rc = 1;

  fail_UV_AddSite:

    if (islocked) {
	tst =
	    ubik_VL_ReleaseLock(cellHandle->vos, 0, volid, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*removes <server> <part> as read only site for <volid> from the vldb */
int
UV_RemoveSite(afs_cell_handle_p cellHandle, afs_int32 server, afs_int32 part,
	      afs_int32 volid, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct nvldbentry entry;
    int islocked = 0;

    tst =
	ubik_VL_SetLock(cellHandle->vos, 0, volid, RWVOL, VLOP_ADDSITE);
    if (tst) {
	goto fail_UV_RemoveSite;
    }
    islocked = 1;

    if (!aVLDB_GetEntryByID(cellHandle, volid, RWVOL, &entry, &tst)) {
	goto fail_UV_RemoveSite;
    }
    if (!Lp_ROMatch(cellHandle, &entry, server, part, &tst)) {
	/*this site doesnot exist  */
	goto fail_UV_RemoveSite;
    } else {			/*remove the rep site */
	Lp_SetROValue(cellHandle, &entry, server, part, 0, 0);
	entry.nServers--;
	if ((entry.nServers == 1) && (entry.flags & RW_EXISTS))
	    entry.flags &= ~RO_EXISTS;
	if (entry.nServers < 1) {	/*this is the last ref */
	    tst = ubik_VL_DeleteEntry(cellHandle->vos, 0, volid, ROVOL);
	    if (tst) {
		goto fail_UV_RemoveSite;
	    }
	}
	if (!VLDB_ReplaceEntry
	    (cellHandle, volid, RWVOL, &entry,
	     (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP), &tst)) {
	    goto fail_UV_RemoveSite;
	}
    }
    rc = 1;

  fail_UV_RemoveSite:

    if (islocked) {
	afs_status_t t;
	t = ubik_VL_ReleaseLock(cellHandle->vos, 0, volid, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (tst == 0) {
	    tst = t;
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*list all the partitions on <aserver> */
int
UV_ListPartitions(struct rx_connection *server, struct partList *ptrPartList,
		  afs_int32 * cntp, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    struct pIDs partIds;
    struct partEntries partEnts;
    register int i, j = 0;

    *cntp = 0;

    partEnts.partEntries_len = 0;
    partEnts.partEntries_val = NULL;
    /* this is available only on new servers */
    tst = AFSVolXListPartitions(server, &partEnts);

    /* next, try old interface */
    if (tst == RXGEN_OPCODE) {
	for (i = 0; i < 26; i++)
	    partIds.partIds[i] = -1;
	tst = AFSVolListPartitions(server, &partIds);
	if (!tst) {
	    for (i = 0; i < 26; i++) {
		if ((partIds.partIds[i]) != -1) {
		    ptrPartList->partId[j] = partIds.partIds[i];
		    ptrPartList->partFlags[j] = PARTVALID;
		    j++;
		} else
		    ptrPartList->partFlags[i] = 0;
	    }
	    *cntp = j;
	} else {
	    goto fail_UV_ListPartitions;
	}
    } else if (!tst) {
	*cntp = partEnts.partEntries_len;
	if (*cntp > VOLMAXPARTS) {
	    *cntp = VOLMAXPARTS;
	}
	for (i = 0; i < *cntp; i++) {
	    ptrPartList->partId[i] = partEnts.partEntries_val[i];
	    ptrPartList->partFlags[i] = PARTVALID;
	}
	free(partEnts.partEntries_val);
    } else {
	goto fail_UV_ListPartitions;
    }
    rc = 1;

  fail_UV_ListPartitions:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*------------------------------------------------------------------------
 * EXPORTED UV_XListVolumes
 *
 * Description:
 *	List the extended information for all the volumes on a particular
 *	File Server and partition.  We may either return the volume's ID
 *	or all of its extended information.
 *
 * Arguments:
 *	a_serverID	   : Address of the File Server for which we want
 *				extended volume info.
 *	a_partID	   : Partition for which we want the extended
 *				volume info.
 *	a_all		   : If non-zero, fetch ALL the volume info,
 *				otherwise just the volume ID.
 *	a_resultPP	   : Ptr to the address of the area containing
 *				the returned volume info.
 *	a_numEntsInResultP : Ptr for the value we set for the number of
 *				entries returned.
 *
 * Returns:
 *	0 on success,
 *	Otherise, the return value of AFSVolXListVolumes.
 *
 * Environment:
 *	This routine is closely related to UV_ListVolumes, which returns
 *	only the standard level of detail on AFS volumes. It is a
 *	heavyweight operation, zipping through all the volume entries for
 *	a given server/partition.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
UV_XListVolumes(struct rx_connection *server, afs_int32 a_partID, int a_all,
		struct volintXInfo **a_resultPP,
		afs_int32 * a_numEntsInResultP, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    volXEntries volumeXInfo;	/*Area for returned extended vol info */

    /*
     * Set up our error code and the area for returned extended volume info.
     * We set the val field to a null pointer as a hint for the stub to
     * allocate space.
     */
    *a_numEntsInResultP = 0;
    *a_resultPP = (volintXInfo *) 0;
    volumeXInfo.volXEntries_val = (volintXInfo *) 0;
    volumeXInfo.volXEntries_len = 0;

    /*
     * Bind to the Volume Server port on the File Server machine in question,
     * then go for it.
     */
    tst = AFSVolXListVolumes(server, a_partID, a_all, &volumeXInfo);
    if (tst) {
	goto fail_UV_XListVolumes;
    } else {
	/*
	 * We got the info; pull out the pointer to where the results lie
	 * and how many entries are there.
	 */
	*a_resultPP = volumeXInfo.volXEntries_val;
	*a_numEntsInResultP = volumeXInfo.volXEntries_len;
    }
    rc = 1;

  fail_UV_XListVolumes:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*------------------------------------------------------------------------
 * EXPORTED UV_XListOneVolume
 *
 * Description:
 *	List the extended information for a volume on a particular File
 *	Server and partition.
 *
 * Arguments:
 *	server	   : a handle to the server where the volume resides.
 *	a_partID	   : Partition for which we want the extended
 *				volume info.
 *	a_volID		   : Volume ID for which we want the info.
 *	a_resultPP	   : Ptr to the address of the area containing
 *				the returned volume info.
 *
 * Returns:
 *	0 on success,
 *	Otherise, the return value of AFSVolXListOneVolume.
 *
 * Environment:
 *	This routine is closely related to UV_ListOneVolume, which returns
 *	only the standard level of detail on the chosen AFS volume.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
UV_XListOneVolume(struct rx_connection *server, afs_int32 a_partID,
		  afs_int32 a_volID, struct volintXInfo **a_resultPP,
		  afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    volXEntries volumeXInfo;	/*Area for returned info */

    /*
     * Set the area we're in which we are returning
     * the info.  Setting the val field to a null pointer tells the stub
     * to allocate space for us.
     */
    *a_resultPP = (volintXInfo *) 0;
    volumeXInfo.volXEntries_val = (volintXInfo *) 0;
    volumeXInfo.volXEntries_len = 0;

    tst = AFSVolXListOneVolume(server, a_partID, a_volID, &volumeXInfo);

    if (tst) {
	goto fail_UV_XListOneVolume;
    } else {
	/*
	 * We got the info; pull out the pointer to where the results lie.
	 */
	*a_resultPP = volumeXInfo.volXEntries_val;
    }
    rc = 1;

  fail_UV_XListOneVolume:

    if (st != NULL) {
	*st = tst;
    }
    return rc;

}				/*UV_XListOneVolume */

/*------------------------------------------------------------------------
 * EXPORTED UV_ListOneVolume
 *
 * Description:
 *	List the volume information for a volume on a particular File
 *	Server and partition.
 *
 * Arguments:
 *	server	   : a handle to the server where the volume resides.
 *	a_partID	   : Partition for which we want the extended
 *				volume info.
 *	a_volID		   : Volume ID for which we want the info.
 *	a_resultPP	   : Ptr to the address of the area containing
 *				the returned volume info.
 *
 * Returns:
 *	0 on success,
 *	Otherise, the return value of AFSVolXListOneVolume.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int UV_ListOneVolume(struct rx_connection *server, afs_int32 a_partID,
		  afs_int32 a_volID, struct volintInfo **a_resultPP,
		  afs_status_p st)
{
	int rc = 0;
    afs_status_t tst = 0;
    volEntries volumeInfo;	/*Area for returned info */

    /*
     * Set the area we're in which we are returning
     * the info.  Setting the val field to a null pointer tells the stub
     * to allocate space for us.
     */
    *a_resultPP = (volintInfo *) 0;
    volumeInfo.volEntries_val = (volintInfo *) 0;
    volumeInfo.volEntries_len = 0;

    tst = AFSVolListOneVolume(server, a_partID, a_volID, &volumeInfo);

    if (tst) {
	goto fail_UV_ListOneVolume;
    } else {
	/*
	 * We got the info; pull out the pointer to where the results lie.
	 */
	*a_resultPP = volumeInfo.volEntries_val;
    }
    rc = 1;

  fail_UV_ListOneVolume:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}/*UV_ListOneVolume*/

/*sync vldb with all the entries on <myQueue> on <aserver> and <apart>*/
static afs_int32
ProcessEntries(afs_cell_handle_p cellHandle, struct qHead *myQueue,
	       struct rx_connection *server, afs_int32 apart, afs_int32 force)
{
    struct aqueue elem;
    int success, temp, temp1, temp2;
    afs_int32 vcode, maxVolid = 0;
    struct nvldbentry entry;
    int noError = 1, error, same;
    int totalC, totalU, totalCE, totalUE, totalG;
    int counter;
    int aserver = ntohl(rx_HostOf(rx_PeerOf(server)));
    afs_status_t tst;

    totalC = totalU = totalCE = totalUE = totalG = 0;
    counter = 0;

    /* get the next  available id's from the vldb server */
    vcode = ubik_VL_GetNewVolumeId(cellHandle->vos, 0, 0, &maxVolid);
    if (vcode) {
	return (vcode);
    }
    totalG = myQueue->count;
    if (totalG == 0)
	return 0;
    while (1) {
	Lp_QEnumerate(myQueue, &success, &elem, 0);
	if (!success)
	    break;
	counter++;

	if (!elem.isValid[RWVOL] && !elem.isValid[ROVOL] && !elem.isValid[BACKVOL]) {	/*something is wrong with elem */
	    noError = 0;
	    continue;
	}
	if (maxVolid <= elem.ids[RWVOL]) {
	    temp1 = maxVolid;
	    temp2 = elem.ids[RWVOL] - maxVolid + 1;
	    maxVolid = 0;
	    vcode =
		ubik_VL_GetNewVolumeId(cellHandle->vos, 0, temp2,
			  &maxVolid);
	    maxVolid += temp2;


	}
	if (maxVolid <= elem.ids[ROVOL]) {

	    temp1 = maxVolid;
	    temp2 = elem.ids[ROVOL] - maxVolid + 1;
	    maxVolid = 0;
	    vcode =
		ubik_VL_GetNewVolumeId(cellHandle->vos, 0, temp2,
			  &maxVolid);
	    maxVolid += temp2;

	}
	if (maxVolid <= elem.ids[BACKVOL]) {
	    temp1 = maxVolid;
	    temp2 = elem.ids[BACKVOL] - temp1 + 1;
	    maxVolid = 0;
	    vcode =
		ubik_VL_GetNewVolumeId(cellHandle->vos, 0, temp2,
			  &maxVolid);
	    maxVolid += temp2;

	}
	aVLDB_GetEntryByID(cellHandle, elem.ids[RWVOL], RWVOL, &entry, &tst);
	if (tst && (tst != VL_NOENT)) {
	    noError = 0;
	    totalCE++;
	} else if (tst && (tst == VL_NOENT)) {	/*entry doesnot exist */
	    /*set up a vldb entry for elem */
	    memset(&entry, 0, sizeof(entry));
	    strncpy(entry.name, elem.name, VOLSER_OLDMAXVOLNAME);
	    if (elem.isValid[RWVOL]) {	/*rw exists */
		entry.flags |= RW_EXISTS;
		entry.serverFlags[entry.nServers] = ITSRWVOL;
		entry.serverNumber[entry.nServers] = aserver;
		entry.serverPartition[entry.nServers] = apart;
		entry.nServers += 1;
		entry.volumeId[RWVOL] = elem.ids[RWVOL];
		entry.volumeId[ROVOL] = elem.ids[ROVOL];
		entry.volumeId[BACKVOL] = elem.ids[BACKVOL];
	    }
	    if (elem.isValid[ROVOL]) {	/*ro volume exists */
		entry.flags |= RO_EXISTS;
		entry.serverFlags[entry.nServers] = ITSROVOL;
		entry.serverNumber[entry.nServers] = aserver;
		entry.serverPartition[entry.nServers] = apart;
		entry.nServers += 1;
		entry.volumeId[RWVOL] = elem.ids[RWVOL];
		entry.volumeId[ROVOL] = elem.ids[ROVOL];

	    }
	    if (elem.isValid[BACKVOL]) {	/*backup volume exists */
		entry.flags |= BACK_EXISTS;
		if (!(entry.flags & RW_EXISTS)) {	/*this helps to check for a stray backup if parent moves */
		    entry.serverFlags[entry.nServers] = ITSRWVOL;
		    entry.serverNumber[entry.nServers] = aserver;
		    entry.serverPartition[entry.nServers] = apart;
		    entry.nServers += 1;
		}

		entry.volumeId[RWVOL] = elem.ids[RWVOL];
		entry.volumeId[BACKVOL] = elem.ids[BACKVOL];

	    }
	    VLDB_CreateEntry(cellHandle, &entry, &tst);
	    if (tst) {
		noError = 0;
		totalCE++;
	    } else
		totalC++;
	} else {		/* Update the existing entry */
	    strncpy(entry.name, elem.name, VOLSER_OLDMAXVOLNAME);	/*the name Could have changed */

	    if (elem.isValid[RWVOL]) {	/* A RW volume */
		temp = Lp_GetRwIndex(cellHandle, &entry, 0);
		if (temp == -1) {
		    /* A RW index is not found in the VLDB entry - will add it */

		    entry.flags |= RW_EXISTS;
		    entry.serverNumber[entry.nServers] = aserver;
		    entry.serverPartition[entry.nServers] = apart;
		    entry.serverFlags[entry.nServers] = ITSRWVOL;
		    entry.nServers++;
		} else {
		    /* A RW index is found in the VLDB entry.
		     * Verify that the volume location matches the VLDB location.
		     * Fix the VLDB entry if it is not correct.
		     */

		    error =
			VLDB_IsSameAddrs(cellHandle, aserver,
					 entry.serverNumber[temp], &same,
					 &tst);
		    if (!error) {
			continue;
		    }
		    if (!same || (apart != entry.serverPartition[temp])) {
			/* VLDB says volume is in another place. Fix the VLDB entry */
			entry.serverNumber[temp] = aserver;
			entry.serverPartition[temp] = apart;

		    }
		    entry.flags |= RW_EXISTS;
		}
		if ((elem.ids[BACKVOL] != 0) && elem.isValid[BACKVOL])
		    entry.volumeId[BACKVOL] = elem.ids[BACKVOL];
		if ((elem.ids[ROVOL] != 0) && elem.isValid[ROVOL])
		    entry.volumeId[ROVOL] = elem.ids[ROVOL];
	    }

	    if (elem.isValid[ROVOL]) {
		/*tackle a ro volume */

		if (!Lp_ROMatch(cellHandle, &entry, aserver, apart, 0)) {
		    /*add this site */
		    if (elem.ids[ROVOL] > entry.volumeId[ROVOL]) {
			/*there is a conflict of ids, keep the later volume */
			/*delete all the ro volumes listed in vldb entry since they 
			 * are older */

			int j, count, rwsite;


			count = entry.nServers;
			rwsite = -1;
			for (j = 0; j < count; j++) {
			    if (entry.serverFlags[j] & ITSROVOL) {

				/*delete the site */
				entry.serverNumber[j] = 0;
				entry.serverPartition[j] = 0;
				entry.serverFlags[j] = 0;

			    } else if (entry.serverFlags[j] & ITSRWVOL)
				rwsite = j;
			}
			entry.nServers = 0;
			if (rwsite != -1) {
			    entry.serverNumber[entry.nServers] =
				entry.serverNumber[rwsite];
			    entry.serverPartition[entry.nServers] =
				entry.serverPartition[rwsite];
			    entry.serverFlags[entry.nServers] =
				entry.serverFlags[rwsite];
			    entry.nServers++;
			}
			entry.serverNumber[entry.nServers] = aserver;
			entry.serverPartition[entry.nServers] = apart;
			entry.serverFlags[entry.nServers] = ITSROVOL;
			entry.nServers++;
			entry.volumeId[ROVOL] = elem.ids[ROVOL];
			entry.flags |= RO_EXISTS;

		    } else if (elem.ids[ROVOL] < entry.volumeId[ROVOL]) {
			if (!(entry.flags & RO_EXISTS)) {
			    entry.volumeId[ROVOL] = elem.ids[ROVOL];
			    entry.serverNumber[entry.nServers] = aserver;
			    entry.serverPartition[entry.nServers] = apart;
			    entry.serverFlags[entry.nServers] = ITSROVOL;
			    entry.nServers++;
			    entry.flags |= RO_EXISTS;
			}

		    }

		    else if (elem.ids[ROVOL] == entry.volumeId[ROVOL]) {
			entry.serverNumber[entry.nServers] = aserver;
			entry.serverPartition[entry.nServers] = apart;
			entry.serverFlags[entry.nServers] = ITSROVOL;
			entry.nServers++;
			entry.flags |= RO_EXISTS;
			entry.volumeId[ROVOL] = elem.ids[ROVOL];
		    }
		}
		if (entry.volumeId[ROVOL] == INVALID_BID)
		    entry.volumeId[ROVOL] = elem.ids[ROVOL];
	    }

	    if (elem.isValid[BACKVOL]) {
		temp = Lp_GetRwIndex(cellHandle, &entry, 0);
		if (temp != -1) {	/*check if existing backup site matches with the given arguments */
		    error =
			VLDB_IsSameAddrs(cellHandle, aserver,
					 entry.serverNumber[temp], &same,
					 &tst);
		    if (!error) {
			continue;
		    }
		} else {
		    /*tackle the backup volume */
		    entry.volumeId[BACKVOL] = elem.ids[BACKVOL];
		    entry.flags |= BACK_EXISTS;
		}
		if (entry.volumeId[BACKVOL] == INVALID_BID)
		    entry.volumeId[BACKVOL] = elem.ids[BACKVOL];
	    }

	    VLDB_ReplaceEntry(cellHandle, elem.ids[RWVOL], RWVOL, &entry,
			      LOCKREL_OPCODE | LOCKREL_AFSID |
			      LOCKREL_TIMESTAMP, &tst);
	    if (tst) {
		noError = 0;
		totalUE++;

		vcode =
		    ubik_VL_ReleaseLock(cellHandle->vos, 0,
			      elem.ids[RWVOL], RWVOL,
			      LOCKREL_OPCODE | LOCKREL_AFSID |
			      LOCKREL_TIMESTAMP);
		if (vcode) {
		    noError = 0;
		}
	    }
	}			/* else update the existing entry */

    }				/* End of while(1) */

    if (noError)
	return 0;
    else
	return VOLSERBADOP;
}

/*synchronise vldb with the file server <aserver> and <apart>(if flags=1).
*else synchronise with all the valid partitions on <aserver>
*/
int
UV_SyncVldb(afs_cell_handle_p cellHandle, struct rx_connection *server,
	    afs_int32 apart, int flags, int force, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_int32 count;
    int i;
    volEntries volumeInfo;
    volintInfo *pntr;
    struct qHead myQueue;
    struct partList PartList;
    int noError = 1;
    afs_int32 cnt;
    char pname[10];

    /*this hints the stub to allocate space */
    volumeInfo.volEntries_val = (volintInfo *) 0;
    volumeInfo.volEntries_len = 0;

    if (!flags) {		/*generate all the valid partitions */
	UV_ListPartitions(server, &PartList, &cnt, &tst);
	if (tst) {
	    goto fail_UV_SyncVldb;
	}
    } else {
	PartList.partId[0] = apart;
	cnt = 1;
    }

    for (i = 0; i < cnt; i++) {
	apart = PartList.partId[i];
	/*this hints the stub to allocate space */
	volumeInfo.volEntries_val = (volintInfo *) 0;
	volumeInfo.volEntries_len = 0;
	tst = AFSVolListVolumes(server, apart, 1, &volumeInfo);
	if (tst) {
	    goto fail_UV_SyncVldb;
	}
	count = volumeInfo.volEntries_len;
	pntr = volumeInfo.volEntries_val;

	if (!vos_PartitionIdToName(apart, pname, &tst)) {
	    goto fail_UV_SyncVldb;
	}
	/*collect all vol entries by their parentid */
	tst = GroupEntries(server, pntr, count, &myQueue, apart);
	if (tst) {
	    noError = 0;
	    if (volumeInfo.volEntries_val) {
		/*free the space allocated by the stub */
		free(volumeInfo.volEntries_val);
		volumeInfo.volEntries_val = 0;
	    }
	    continue;
	}
	tst = ProcessEntries(cellHandle, &myQueue, server, apart, force);
	if (tst) {
	    tst = VOLSERFAILEDOP;
	    if (volumeInfo.volEntries_val) {
		/*free the space allocated by the stub */
		free(volumeInfo.volEntries_val);
		volumeInfo.volEntries_val = 0;
	    }
	    continue;
	}
	if (noError)
	    tst = 0;
	else
	    tst = VOLSERFAILEDOP;
    }				/* thru all partitions */
    rc = 1;

  fail_UV_SyncVldb:

    if (volumeInfo.volEntries_val)
	free(volumeInfo.volEntries_val);

    if (tst != 0) {
	rc = 0;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

static afs_int32
CheckVldbRWBK(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
	      afs_int32 * modified, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int modentry = 0;
    int idx;

    if (modified)
	*modified = 0;
    idx = Lp_GetRwIndex(cellHandle, entry, 0);

    /* Check to see if the RW volume exists and set the RW_EXISTS
     * flag accordingly.
     */
    if (idx == -1) {		/* Did not find a RW entry */
	if (entry->flags & RW_EXISTS) {	/* ... yet entry says RW exists */
	    entry->flags &= ~RW_EXISTS;	/* ... so say RW does not exist */
	    modentry++;
	}
    } else {
	if (VolumeExists
	    (cellHandle, entry->serverNumber[idx],
	     entry->serverPartition[idx], entry->volumeId[RWVOL], &tst)) {
	    if (!(entry->flags & RW_EXISTS)) {	/* ... yet entry says RW does no
						 * t exist */
		entry->flags |= RW_EXISTS;	/* ... so say RW does exist */
		modentry++;
	    }
	} else if (tst == ENODEV) {	/* RW volume does not exist */
	    if (entry->flags & RW_EXISTS) {	/* ... yet entry says RW exists
						 */
		entry->flags &= ~RW_EXISTS;	/* ... so say RW does not exist
						 */
		modentry++;
	    }
	} else {
	    /* If VLDB says it didn't exist, then ignore error */
	    if (entry->flags & RW_EXISTS) {
		goto fail_CheckVldbRWBK;
	    }
	}
    }

    /* Check to see if the BK volume exists and set the BACK_EXISTS
     * flag accordingly. idx already ponts to the RW entry.
     */
    if (idx == -1) {		/* Did not find a RW entry */
	if (entry->flags & BACK_EXISTS) {	/* ... yet entry says BK exists */
	    entry->flags &= ~BACK_EXISTS;	/* ... so say BK does not exist */
	    modentry++;
	}
    } else {			/* Found a RW entry */
	if (VolumeExists
	    (cellHandle, entry->serverNumber[idx],
	     entry->serverPartition[idx], entry->volumeId[BACKVOL], &tst)) {
	    if (!(entry->flags & BACK_EXISTS)) {	/* ... yet entry says BK does n
							 * ot exist */
		entry->flags |= BACK_EXISTS;	/* ... so say BK does exist */
		modentry++;
	    }
	} else if (tst == ENODEV) {	/* BK volume does not exist */
	    if (entry->flags & BACK_EXISTS) {	/* ... yet entry says BK exists
						 */
		entry->flags &= ~BACK_EXISTS;	/* ... so say BK does not exist
						 */
		modentry++;
	    }
	} else {
	    /* If VLDB says it didn't exist, then ignore error */
	    if (entry->flags & BACK_EXISTS) {
		goto fail_CheckVldbRWBK;
	    }
	}
    }

    /* If there is an idx but the BK and RW volumes no
     * longer exist, then remove the RW entry.
     */
    if ((idx != -1) && !(entry->flags & RW_EXISTS)
	&& !(entry->flags & BACK_EXISTS)) {
	Lp_SetRWValue(cellHandle, entry, entry->serverNumber[idx],
		      entry->serverPartition[idx], 0L, 0L);
	entry->nServers--;
	modentry++;
    }
    rc = 1;

  fail_CheckVldbRWBK:

    if (modified)
	*modified = modentry;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


static int
CheckVldbRO(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
	    afs_int32 * modified, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    int idx;
    int foundro = 0, modentry = 0;

    if (modified)
	*modified = 0;

    /* Check to see if the RO volumes exist and set the RO_EXISTS
     * flag accordingly.
     */
    for (idx = 0; idx < entry->nServers; idx++) {
	if (!(entry->serverFlags[idx] & ITSROVOL)) {
	    continue;		/* not a RO */
	}

	if (VolumeExists
	    (cellHandle, entry->serverNumber[idx],
	     entry->serverPartition[idx], entry->volumeId[ROVOL], &tst)) {
	    foundro++;
	} else if (tst == ENODEV) {	/* RW volume does not exist */
	    Lp_SetROValue(cellHandle, entry, entry->serverNumber[idx],
			  entry->serverPartition[idx], 0L, 0L);
	    entry->nServers--;
	    idx--;
	    modentry++;
	} else {
	    goto fail_CheckVldbRO;
	}
    }

    if (foundro) {		/* A RO volume exists */
	if (!(entry->flags & RO_EXISTS)) {	/* ... yet entry says RW does not e
						 * xist */
	    entry->flags |= RO_EXISTS;	/* ... so say RW does exist */
	    modentry++;
	}
    } else {			/* A RO volume does not exist */
	if (entry->flags & RO_EXISTS) {	/* ... yet entry says RO exists */
	    entry->flags &= ~RO_EXISTS;	/* ... so say RO does not exist */
	    modentry++;
	}
    }
    rc = 1;

  fail_CheckVldbRO:

    if (modified)
	*modified = modentry;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*ensure that <entry> matches with the info on file servers */
int
CheckVldb(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
	  afs_int32 * modified, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_int32 vcode;
    int islocked = 0;
    int pass = 0;
    afs_int32 modentry = 0;
    afs_int32 delentry = 0;

    if (modified) {
	*modified = 0;
    }

    if (strlen(entry->name) > (VOLSER_OLDMAXVOLNAME - 10)) {
	tst = VOLSERBADOP;
	goto fail_CheckVldb;
    }

  retry:

    /* Check to see if the VLDB is ok without locking it (pass 1).
     * If it will change, then lock the VLDB entry, read it again,
     * then make the changes to it (pass 2).
     */
    if (++pass == 2) {
	tst =
	    ubik_VL_SetLock(cellHandle->vos, 0, entry->volumeId[RWVOL],
		      RWVOL, VLOP_DELETE);
	if (tst) {
	    goto fail_CheckVldb;
	}
	islocked = 1;

	if (!aVLDB_GetEntryByID
	    (cellHandle, entry->volumeId[RWVOL], RWVOL, entry, &tst)) {
	    goto fail_CheckVldb;
	}
    }

    modentry = 0;

    /* Check if the RW and BK entries are ok */
    if (!CheckVldbRWBK(cellHandle, entry, &modentry, &tst)) {
	goto fail_CheckVldb;
    }
    if (modentry && (pass == 1))
	goto retry;

    /* Check if the RO volumes entries are ok */
    if (!CheckVldbRO(cellHandle, entry, &modentry, &tst)) {
	goto fail_CheckVldb;
    }
    if (modentry && (pass == 1))
	goto retry;

    /* The VLDB entry has been updated. If it as been modified, then
     * write the entry back out the the VLDB.
     */
    if (modentry) {
	if (pass == 1)
	    goto retry;

	if (!(entry->flags & RW_EXISTS) && !(entry->flags & BACK_EXISTS)
	    && !(entry->flags & RO_EXISTS)) {
	    /* The RW, BK, nor RO volumes do not exist. Delete the VLDB entry */
	    tst =
		ubik_VL_DeleteEntry(cellHandle->vos, 0,
			  entry->volumeId[RWVOL], RWVOL);
	    if (tst) {
		goto fail_CheckVldb;
	    }
	    delentry = 1;
	} else {
	    /* Replace old entry with our new one */
	    if (!VLDB_ReplaceEntry
		(cellHandle, entry->volumeId[RWVOL], RWVOL, entry,
		 (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP),
		 &tst)) {
		goto fail_CheckVldb;
	    }
	}
	if (modified)
	    *modified = 1;
	islocked = 0;
    }
    rc = 1;

  fail_CheckVldb:

    if (islocked) {
	vcode =
	    ubik_VL_ReleaseLock(cellHandle->vos, 0,
				entry->volumeId[RWVOL], RWVOL,
				(LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	if (vcode) {
	    if (!tst)
		tst = vcode;
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*synchronise <aserver> <apart>(if flags = 1) with the vldb .
*if flags = 0, synchronise all the valid partitions on <aserver>*/
int
UV_SyncServer(afs_cell_handle_p cellHandle, struct rx_connection *server,
	      afs_int32 apart, int flags, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_int32 code, vcode;
    int noError;
    afs_int32 nentries, tentries = 0;
    struct VldbListByAttributes attributes;
    nbulkentries arrayEntries;
    int totalF;
    register struct nvldbentry *vllist;
    register int j;
    afs_int32 si, nsi;
    afs_int32 modified = 0;

    code = 0;
    vcode = 0;
    noError = 1;
    arrayEntries.nbulkentries_val = 0;

    /* Set up attributes to search VLDB  */
    attributes.server = ntohl(rx_HostOf(rx_PeerOf(server)));
    attributes.Mask = VLLIST_SERVER;
    if (flags) {
	attributes.partition = apart;
	attributes.Mask |= VLLIST_PARTITION;
    }

    for (si = 0; si != -1; si = nsi) {
	/*initialize to hint the stub  to alloc space */
	memset(&arrayEntries, 0, sizeof(arrayEntries));
	if (!VLDB_ListAttributes
	    (cellHandle, &attributes, &nentries, &arrayEntries, &tst)) {
	    goto fail_UV_SyncServer;
	}
	nsi = -1;
	tentries += nentries;
	totalF = 0;
	for (j = 0; j < nentries; j++) {	/* process each entry */
	    vllist = &arrayEntries.nbulkentries_val[j];
	    if (!CheckVldb(cellHandle, vllist, &modified, &tst)) {
		noError = 0;
		totalF++;
	    }
	}
	if (arrayEntries.nbulkentries_val) {
	    free(arrayEntries.nbulkentries_val);
	    arrayEntries.nbulkentries_val = 0;
	}
    }
    rc = 1;

  fail_UV_SyncServer:

    if (arrayEntries.nbulkentries_val) {
	free(arrayEntries.nbulkentries_val);
    }
    if (!noError)
	tst = VOLSERFAILEDOP;
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*rename volume <oldname> to <newname>, changing the names of the related 
 *readonly and backup volumes. This operation is also idempotent.
 *salvager is capable of recovering from rename operation stopping halfway.
 *to recover run syncserver on the affected machines,it will force renaming to completion. name clashes should have been detected before calling this proc */
int
UV_RenameVolume(afs_cell_handle_p cellHandle, struct nvldbentry *entry,
		const char *newname, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_status_t etst = 0;
    afs_int32 rcode, error;
    int i, index;
    char nameBuffer[256];
    afs_int32 tid;
    struct rx_connection *aconn;
    int islocked;

    error = 0;
    aconn = (struct rx_connection *)0;
    tid = 0;
    islocked = 0;

    tst = ubik_VL_SetLock(cellHandle->vos, 0, entry->volumeId[RWVOL], RWVOL, VLOP_ADDSITE);	/*last param is dummy */
    if (tst) {
	goto fail_UV_RenameVolume;
    }
    islocked = 1;

    strncpy(entry->name, newname, VOLSER_OLDMAXVOLNAME);

    if (!VLDB_ReplaceEntry
	(cellHandle, entry->volumeId[RWVOL], RWVOL, entry, 0, &tst)) {
	goto fail_UV_RenameVolume;
    }
    /*at this stage the intent to rename is recorded in the vldb, as far
     * as the vldb 
     * is concerned, oldname is lost */
    if (entry->flags & RW_EXISTS) {
	index = Lp_GetRwIndex(cellHandle, entry, 0);
	if (index == -1) {	/* there is a serious discrepancy */
	    tst = VOLSERVLDB_ERROR;
	    goto fail_UV_RenameVolume;
	}
	aconn =
	    UV_Bind(cellHandle, entry->serverNumber[index],
		    AFSCONF_VOLUMEPORT);
	tst =
	    AFSVolTransCreate(aconn, entry->volumeId[RWVOL],
			      entry->serverPartition[index], ITOffline, &tid);
	if (tst) {		/*volume doesnot exist */
	    goto fail_UV_RenameVolume;
	} else {		/*volume exists, process it */

	    tst =
		AFSVolSetIdsTypes(aconn, tid, newname, RWVOL,
				  entry->volumeId[RWVOL],
				  entry->volumeId[ROVOL],
				  entry->volumeId[BACKVOL]);
	    if (!tst) {
		tst = AFSVolEndTrans(aconn, tid, &rcode);
		tid = 0;
		if (tst) {
		    goto fail_UV_RenameVolume;
		}
	    } else {
		goto fail_UV_RenameVolume;
	    }
	}
	if (aconn)
	    rx_ReleaseCachedConnection(aconn);
	aconn = (struct rx_connection *)0;
    }
    /*end rw volume processing */
    if (entry->flags & BACK_EXISTS) {	/*process the backup volume */
	index = Lp_GetRwIndex(cellHandle, entry, 0);
	if (index == -1) {	/* there is a serious discrepancy */
	    tst = VOLSERVLDB_ERROR;
	    goto fail_UV_RenameVolume;
	}
	aconn =
	    UV_Bind(cellHandle, entry->serverNumber[index],
		    AFSCONF_VOLUMEPORT);
	tst =
	    AFSVolTransCreate(aconn, entry->volumeId[BACKVOL],
			      entry->serverPartition[index], ITOffline, &tid);
	if (tst) {		/*volume doesnot exist */
	    goto fail_UV_RenameVolume;
	} else {		/*volume exists, process it */
	    if (strlen(newname) > (VOLSER_OLDMAXVOLNAME - 8)) {
		goto fail_UV_RenameVolume;
	    }
	    strcpy(nameBuffer, newname);
	    strcat(nameBuffer, ".backup");

	    tst =
		AFSVolSetIdsTypes(aconn, tid, nameBuffer, BACKVOL,
				  entry->volumeId[RWVOL], 0, 0);
	    if (!tst) {
		tst = AFSVolEndTrans(aconn, tid, &rcode);
		tid = 0;
		if (tst) {
		    goto fail_UV_RenameVolume;
		}
	    } else {
		goto fail_UV_RenameVolume;
	    }
	}
    }				/* end backup processing */
    if (aconn)
	rx_ReleaseCachedConnection(aconn);
    aconn = (struct rx_connection *)0;
    if (entry->flags & RO_EXISTS) {	/*process the ro volumes */
	for (i = 0; i < entry->nServers; i++) {
	    if (entry->serverFlags[i] & ITSROVOL) {
		aconn =
		    UV_Bind(cellHandle, entry->serverNumber[i],
			    AFSCONF_VOLUMEPORT);
		tst =
		    AFSVolTransCreate(aconn, entry->volumeId[ROVOL],
				      entry->serverPartition[i], ITOffline,
				      &tid);
		if (tst) {	/*volume doesnot exist */
		    goto fail_UV_RenameVolume;
		} else {	/*volume exists, process it */
		    strcpy(nameBuffer, newname);
		    strcat(nameBuffer, ".readonly");
		    if (strlen(nameBuffer) > (VOLSER_OLDMAXVOLNAME - 1)) {
			goto fail_UV_RenameVolume;
		    }
		    tst =
			AFSVolSetIdsTypes(aconn, tid, nameBuffer, ROVOL,
					  entry->volumeId[RWVOL], 0, 0);
		    if (!tst) {
			tst = AFSVolEndTrans(aconn, tid, &rcode);
			tid = 0;
			if (tst) {
			    goto fail_UV_RenameVolume;
			}
		    } else {
			goto fail_UV_RenameVolume;
		    }
		}
		if (aconn)
		    rx_ReleaseCachedConnection(aconn);
		aconn = (struct rx_connection *)0;
	    }
	}
    }
    rc = 1;

  fail_UV_RenameVolume:

    if (islocked) {
	etst =
	    ubik_VL_ReleaseLock(cellHandle->vos, 0,
		      entry->volumeId[RWVOL], RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (etst) {
	    if (!tst)
		tst = etst;
	}
    }
    if (tid) {
	etst = AFSVolEndTrans(aconn, tid, &rcode);
	if (etst) {
	    if (!tst)
		tst = etst;
	}
    }
    if (aconn)
	rx_ReleaseCachedConnection(aconn);

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*group all the volume entries on< apart >by their parentid or by their ids'
*if the volume is rw. <count> is the number of entries to be processesd.
*<pntr> points to the first entry.< myQueue> is the queue with entries 
*grouped */
static afs_int32
GroupEntries(struct rx_connection *server, volintInfo * pntr, afs_int32 count,
	     struct qHead *myQueue, afs_int32 apart)
{
    struct aqueue *qPtr;
    int success;
    afs_int32 curId, code;
    int i;
    afs_int32 error = 0;


    Lp_QInit(myQueue);
    if (count == 0)
	return 0;
    for (i = 0; i < count; i++) {	/*process each entry */
	if (pntr->status) {	/* entry is valid */
	    if (pntr->type == RWVOL)
		curId = pntr->volid;
	    else
		curId = pntr->parentID;
	    Lp_QScan(myQueue, curId, &success, &qPtr, 0);
	    if (success) {	/*entry exists in the queue */
		if (pntr->type == RWVOL) {
		    /*check if the rw exists already, if so hang on the
		     * later version if the present version is ok */
		    if (qPtr->isValid[RWVOL]) {
			/*this should not happen, there is a serious error here */
			if (!error)
			    error = VOLSERMULTIRWVOL;
		    } else {
			qPtr->isValid[RWVOL] = 1;
			qPtr->copyDate[RWVOL] = pntr->copyDate;
			if (!qPtr->isValid[BACKVOL])
			    qPtr->ids[BACKVOL] = pntr->backupID;
			if (!qPtr->isValid[ROVOL])
			    qPtr->ids[ROVOL] = pntr->cloneID;
		    }
		} else if (pntr->type == BACKVOL) {
		    if (qPtr->isValid[BACKVOL]) {
			/*do different checks, delete superflous volume */
			if (qPtr->copyDate[BACKVOL] > pntr->copyDate) {
			    /*delete the present volume . */
			    code =
				CheckAndDeleteVolume(server, apart, 0,
						     pntr->volid);
			    if (code) {
				if (!error)
				    error = code;
			    }

			} else {
			    /*delete the older volume after making sure, current one is ok */
			    code =
				CheckAndDeleteVolume(server, apart,
						     pntr->volid,
						     qPtr->ids[BACKVOL]);
			    if (code) {
				if (!error)
				    error = code;
			    }

			    qPtr->copyDate[BACKVOL] = pntr->copyDate;
			    qPtr->ids[BACKVOL] = pntr->volid;

			}
		    } else {
			qPtr->isValid[BACKVOL] = 1;
			qPtr->ids[BACKVOL] = pntr->volid;
			qPtr->copyDate[BACKVOL] = pntr->copyDate;
		    }
		} else if (pntr->type == ROVOL) {
		    if (qPtr->isValid[ROVOL]) {
			/*do different checks, delete superflous volume */
			if (qPtr->copyDate[ROVOL] > pntr->copyDate) {
			    /*delete the present volume . */
			    /*a hack */
			    code =
				CheckAndDeleteVolume(server, apart, 0,
						     pntr->volid);
			    if (code) {
				if (!error)
				    error = code;
			    }
			} else {
			    /*delete the older volume after making sure, current one is ok */
			    code =
				CheckAndDeleteVolume(server, apart,
						     pntr->volid,
						     qPtr->ids[ROVOL]);
			    if (code) {
				if (!error)
				    error = code;
			    }

			    qPtr->copyDate[ROVOL] = pntr->copyDate;
			    qPtr->ids[ROVOL] = pntr->volid;

			}
		    } else {
			qPtr->isValid[ROVOL] = 1;
			qPtr->ids[ROVOL] = pntr->volid;
			qPtr->copyDate[ROVOL] = pntr->copyDate;
		    }
		} else {
		    if (!error)
			error = VOLSERBADOP;
		}
	    } else {		/*create a fresh entry */
		qPtr = (struct aqueue *)malloc(sizeof(struct aqueue));
		if (pntr->type == RWVOL) {
		    qPtr->isValid[RWVOL] = 1;
		    qPtr->isValid[BACKVOL] = 0;
		    qPtr->isValid[ROVOL] = 0;
		    qPtr->ids[RWVOL] = pntr->volid;
		    qPtr->ids[BACKVOL] = pntr->backupID;
		    qPtr->ids[ROVOL] = pntr->cloneID;
		    qPtr->copyDate[RWVOL] = pntr->copyDate;
		    strncpy(qPtr->name, pntr->name, VOLSER_OLDMAXVOLNAME);
		    qPtr->next = NULL;
		} else if (pntr->type == BACKVOL) {
		    qPtr->isValid[RWVOL] = 0;
		    qPtr->isValid[BACKVOL] = 1;
		    qPtr->isValid[ROVOL] = 0;
		    qPtr->ids[RWVOL] = pntr->parentID;
		    qPtr->ids[BACKVOL] = pntr->volid;
		    qPtr->ids[ROVOL] = 0;
		    qPtr->copyDate[BACKVOL] = pntr->copyDate;
		    vsu_ExtractName(qPtr->name, pntr->name);
		    qPtr->next = NULL;
		} else if (pntr->type == ROVOL) {
		    qPtr->isValid[RWVOL] = 0;
		    qPtr->isValid[BACKVOL] = 0;
		    qPtr->isValid[ROVOL] = 1;
		    qPtr->ids[RWVOL] = pntr->parentID;
		    qPtr->ids[BACKVOL] = 0;
		    qPtr->ids[ROVOL] = pntr->volid;
		    qPtr->copyDate[ROVOL] = pntr->copyDate;
		    vsu_ExtractName(qPtr->name, pntr->name);
		    qPtr->next = NULL;

		}
		Lp_QAdd(myQueue, qPtr);
	    }
	    pntr++;		/*get next entry */
	} else {
	    pntr++;
	    continue;
	}
    }				/* for loop */

    return error;
}

/*report on all the active transactions on volser */
int
UV_VolserStatus(struct rx_connection *server, transDebugInfo ** rpntr,
		afs_int32 * rcount, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    transDebugEntries transInfo;

    transInfo.transDebugEntries_val = (transDebugInfo *) 0;
    transInfo.transDebugEntries_len = 0;
    tst = AFSVolMonitor(server, &transInfo);
    if (tst) {
	goto fail_UV_VolserStatus;
    }

    *rcount = transInfo.transDebugEntries_len;
    *rpntr = transInfo.transDebugEntries_val;
    rc = 1;

  fail_UV_VolserStatus:

    if (rc == 0) {
	if (transInfo.transDebugEntries_val) {
	    free(transInfo.transDebugEntries_val);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*delete the volume without interacting with the vldb */
int
UV_VolumeZap(afs_cell_handle_p cellHandle, struct rx_connection *server,
	     unsigned int partition, unsigned int volumeId, afs_status_p st)
{
    afs_int32 rcode, ttid, error, code;
    int rc = 0;
    afs_status_t tst = 0;

    code = 0;
    error = 0;
    ttid = 0;

    tst = AFSVolTransCreate(server, volumeId, partition, ITOffline, &ttid);
    if (!tst) {
	tst = AFSVolDeleteVolume(server, ttid);
	if (!tst) {
	    tst = AFSVolEndTrans(server, ttid, &rcode);
	    if (!tst) {
		if (rcode) {
		    tst = rcode;
		}
	    }
	} else {
	    /*
	     * We failed to delete the volume, but we still need
	     * to end the transaction.
	     */
	    AFSVolEndTrans(server, ttid, &rcode);
	}
	rc = 1;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

int
UV_SetVolume(struct rx_connection *server, afs_int32 partition,
	     afs_int32 volid, afs_int32 transflag, afs_int32 setflag,
	     unsigned int sleepTime, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    afs_status_t etst = 0;
    afs_int32 tid = 0;
    afs_int32 rcode;

    tst = AFSVolTransCreate(server, volid, partition, transflag, &tid);
    if (tst) {
	goto fail_UV_SetVolume;
    }

    tst = AFSVolSetFlags(server, tid, setflag);
    if (tst) {
	goto fail_UV_SetVolume;
    }

    if (sleepTime) {
	sleep(sleepTime);
    }
    rc = 1;

  fail_UV_SetVolume:

    if (tid) {
	etst = AFSVolEndTrans(server, tid, &rcode);
	/* FIXME: this looks like a typo */
	if (etst || etst) {
	    if (!tst)
		tst = (etst ? etst : rcode);
	}
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}
