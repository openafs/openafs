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

#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/vlserver.h>	/*Misc server-side Volume Location stuff */
#include <ubik.h>
#include <afs/afsint.h>
#include <afs/volser.h>

#include "bc.h"
#include <afs/volint.h>
#include <afs/volser.h>
#include <afs/volser_prototypes.h>
#include <afs/com_err.h>

extern char *whoami;

/* ********************************************************************* */
/* Volserver routines */
/* ********************************************************************* */

afs_int32
bc_GetEntryByID(struct ubik_client *uclient, afs_int32 volID,
		afs_int32 volType, struct vldbentry *vldbEntryPtr)
{
    afs_int32 code = 0;

    code =
	ubik_VL_GetEntryByID(uclient, 0, volID, volType, vldbEntryPtr);
    return (code);
}

/* volImageTime
 *	Determine the time stamp to be recorded with the backup of this
 *	volume. For backup and r/o volumes this is the clone time, for
 *	r/w volumes, this is the current time. This timestamp is stored
 *	directly into the cloneDate field of the bc_volumeDump structure
 * exit:
 *	0 - success
 *	-1 - failed to get information. Sets cloneDate to 0.
 */

afs_int32
volImageTime(afs_uint32 serv, afs_int32 part, afs_uint32 volid,
	     afs_int32 voltype, afs_int32 *clDatePtr)
{
    afs_int32 code = 0;
    struct volintInfo *viptr;
    struct rx_sockaddr sa;

    if (voltype == RWVOL) {
	*clDatePtr = time(0);
	return (0);
    }

    /* UV_ListOneVolume() takes a family-agnostic struct rx_sockaddr, not
     * a raw afs_uint32 - build one here instead of reinterpreting the
     * integer as a pointer (undefined behavior; used to compile with
     * only a -Wint-conversion warning). serv is host byte order here,
     * same as every other caller of this function - htonl() it into
     * network byte order the way the old direct call did. */
    rx_ipv4_to_sockaddr(htonl(serv), 0, 0, &sa);
    code = UV_ListOneVolume(&sa, part, volid, &viptr);
    if (code) {
	afs_com_err(whoami, code,
		"Warning: Can't get clone time of volume %u - using 0",
		volid);
	*clDatePtr = 0;
	return (0);
    }

    /* volume types from vol/voldefs.h */
    switch (viptr->type) {
    case RWVOL:
	/* For a r/w volume there may not be any foolproof way of
	 * preventing anomalies in the backups. Use the current time;
	 */
	*clDatePtr = time(0);
	break;

    case ROVOL:
    case BACKVOL:
	*clDatePtr = viptr->creationDate;	/* use the creation time */
	break;

    default:
	afs_com_err(whoami, 0,
		"Can't get clone time of volume %u - unknown volume type",
		volid);
	return (-1);
    }
    return (0);
}
