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
#include <stdio.h>
#include <string.h>
#include <lock.h>
#include <afs/afsutil.h>
#include <ubik.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <afs/keys.h>
#include "vlserver.h"
#include "afs/audit.h"
#ifndef AFS_NT40_ENV
#include <unistd.h>
#endif
#ifdef HAVE_POSIX_REGEX		/* use POSIX regexp library */
#include <regex.h>
#endif

extern int smallMem;
extern extent_mod;
extern struct afsconf_dir *vldb_confdir;
extern struct ubik_dbase *VL_dbase;
struct vlheader cheader;	/* kept in network byte order */
extern afs_uint32 vl_HostAddress[];	/* host addresses kept in host byte order */
int maxnservers;
struct extentaddr *ex_addr[VL_MAX_ADDREXTBLKS] = { 0, 0, 0, 0 };
static char rxinfo_str[128];	/* Need rxinfo string to be non-local */
#define ABORT(c) { errorcode = (c); goto abort; }
#undef END
#define END(c) { errorcode = (c); goto end; }

#define VLDBALLOCLIMIT	10000
#define VLDBALLOCINCR	2048

static int put_attributeentry();
static int put_nattributeentry();
static int RemoveEntry();
static void ReleaseEntry();
static int check_vldbentry();
static int check_nvldbentry();
static int vldbentry_to_vlentry();
static int nvldbentry_to_vlentry();
static get_vldbupdateentry();
static int repsite_exists();
static void repsite_compress();
static void vlentry_to_vldbentry();
static void vlentry_to_nvldbentry();
static void vlentry_to_uvldbentry();
static int InvalidVolname();
static int InvalidVoltype();
static int InvalidOperation();
static int InvalidReleasetype();
static int IpAddrToRelAddr();
static int ChangeIPAddr();

char *
rxinfo(rxcall)
     struct rx_call *rxcall;
{
    int code;
    register struct rx_connection *tconn;
    char tname[64];
    char tinst[64];
    char tcell[64];
    afs_uint32 exp;
    struct in_addr hostAddr;
    int scIndex;
#ifdef AFS_RXK5
    int lvl, expires, kvno, enctype;
    char *princ;
#endif

    tconn = rx_ConnectionOf(rxcall);
    hostAddr.s_addr = rx_HostOf(rx_PeerOf(tconn));
    scIndex = tconn->securityIndex;
    switch(scIndex) {
	case 2:
	    code =
		rxkad_GetServerInfo(rxcall->conn, NULL, &exp, tname, 
				    tinst, tcell, NULL);
	    if (!code)
		sprintf(rxinfo_str, "%s %s%s%s%s%s", inet_ntoa(hostAddr), 
			tname, tinst?".":"", tinst?tinst:"", tcell?"@":"", 
			tcell?tcell:"");
	    else
		sprintf(rxinfo_str, "%s noauth", inet_ntoa(hostAddr));
	    break;
	case 5:
#ifdef AFS_RXK5
	    if (code = rxk5_GetServerInfo(rxcall->conn, &lvl,
					  &expires, &princ, &kvno,
					  &enctype)) {
            break;
        } else {
	    /* TODO: review wrt cell mapping, foreign cells */
	    sprintf(rxinfo_str, "%s %s", inet_ntoa(hostAddr), princ);
	}
#endif
	    break;
    }
	    return (rxinfo_str);
}

/* This is called to initialize the database, set the appropriate locks and make sure that the vldb header is valid */
int
Init_VLdbase(trans, locktype, this_op)
     struct ubik_trans **trans;
     int locktype;		/* indicate read or write transaction */
     int this_op;
{
    int errorcode = 0, pass, wl;

    for (pass = 1; pass <= 3; pass++) {
	if (pass == 2) {	/* take write lock to rebuild the db */
	    errorcode = ubik_BeginTrans(VL_dbase, UBIK_WRITETRANS, trans);
	    wl = 1;
	} else if (locktype == LOCKREAD) {
	    errorcode =
		ubik_BeginTransReadAny(VL_dbase, UBIK_READTRANS, trans);
	    wl = 0;
	} else {
	    errorcode = ubik_BeginTrans(VL_dbase, UBIK_WRITETRANS, trans);
	    wl = 1;
	}
	if (errorcode)
	    return errorcode;

	errorcode = ubik_SetLock(*trans, 1, 1, locktype);
	if (errorcode) {
	    COUNT_ABO;
	    ubik_AbortTrans(*trans);
	    return errorcode;
	}

	/* check that dbase is initialized and setup cheader */
	/* 2nd pass we try to rebuild the header */
	errorcode = CheckInit(*trans, ((pass == 2) ? 1 : 0));
	if (!errorcode && wl && extent_mod)
	    errorcode = readExtents(*trans);	/* Fix the mh extent blocks */
	if (errorcode) {
	    COUNT_ABO;
	    ubik_AbortTrans(*trans);
	    /* Only rebuld if the database is empty */
	    /* Exit if can't rebuild */
	    if ((pass == 1) && (errorcode != VL_EMPTY))
		return errorcode;
	    if (pass == 2)
		return errorcode;
	} else {		/* No errorcode */
	    if (pass == 2) {
		ubik_EndTrans(*trans);	/* Rebuilt db. End trans, then retake original lock */
	    } else {
		break;		/* didn't rebuild and successful - exit */
	    }
	}
    }
    return errorcode;
}


/* Create a new vldb entry; both new volume id and name must be unique (non-existant in vldb). */

afs_int32
SVL_CreateEntry(rxcall, newentry)
     struct rx_call *rxcall;
     struct vldbentry *newentry;
{
    struct ubik_trans *trans;
    afs_int32 errorcode, blockindex;
    struct nvlentry tentry;

    COUNT_REQ(VLCREATEENTRY);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL)) {
	errorcode = VL_PERM;
	goto end;
    }

    /* Do some validity tests on new entry */
    if ((errorcode = check_vldbentry(newentry))
	|| (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op)))
	goto end;

    VLog(1,
	 ("OCreate Volume %d %s\n", newentry->volumeId[RWVOL],
	  rxinfo(rxcall)));
    /* XXX shouldn't we check to see if the r/o volume is duplicated? */
    if (newentry->volumeId[RWVOL]
	&& FindByID(trans, newentry->volumeId[RWVOL], RWVOL, &tentry, &errorcode)) {	/* entry already exists, we fail */
	errorcode = VL_IDEXIST;
	goto abort;
    } else if (errorcode) {
	goto abort;
    }

    /* Is this following check (by volume name) necessary?? */
    /* If entry already exists, we fail */
    if (FindByName(trans, newentry->name, &tentry, &errorcode)) {
	errorcode = VL_NAMEEXIST;
	goto abort;
    } else if (errorcode) {
	goto abort;
    }

    blockindex = AllocBlock(trans, &tentry);
    if (blockindex == 0) {
	errorcode = VL_CREATEFAIL;
	goto abort;
    }

    memset(&tentry, 0, sizeof(struct nvlentry));
    /* Convert to its internal representation; both in host byte order */
    if (errorcode = vldbentry_to_vlentry(trans, newentry, &tentry)) {
	FreeBlock(trans, blockindex);
	goto abort;
    }

    /* Actually insert the entry in vldb */
    errorcode = ThreadVLentry(trans, blockindex, &tentry);
    if (errorcode) {
	FreeBlock(trans, blockindex);
	goto abort;
    } else {
	errorcode = ubik_EndTrans(trans);
	goto end;
    }

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLCreateEntryEvent, errorcode, AUD_STR,
	       (newentry ? newentry->name : NULL), AUD_END);
    return errorcode;
}


afs_int32
SVL_CreateEntryN(rxcall, newentry)
     struct rx_call *rxcall;
     struct nvldbentry *newentry;
{
    struct ubik_trans *trans;
    afs_int32 errorcode, blockindex;
    struct nvlentry tentry;

    COUNT_REQ(VLCREATEENTRYN);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL)) {
	errorcode = VL_PERM;
	goto end;
    }

    /* Do some validity tests on new entry */
    if ((errorcode = check_nvldbentry(newentry))
	|| (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op)))
	goto end;

    VLog(1,
	 ("Create Volume %d %s\n", newentry->volumeId[RWVOL],
	  rxinfo(rxcall)));
    /* XXX shouldn't we check to see if the r/o volume is duplicated? */
    if (newentry->volumeId[RWVOL]
	&& FindByID(trans, newentry->volumeId[RWVOL], RWVOL, &tentry, &errorcode)) {	/* entry already exists, we fail */
	errorcode = VL_IDEXIST;
	goto abort;
    } else if (errorcode) {
	goto abort;
    }

    /* Is this following check (by volume name) necessary?? */
    /* If entry already exists, we fail */
    if (FindByName(trans, newentry->name, &tentry, &errorcode)) {
	errorcode = VL_NAMEEXIST;
	goto abort;
    } else if (errorcode) {
	goto abort;
    }

    blockindex = AllocBlock(trans, &tentry);
    if (blockindex == 0) {
	errorcode = VL_CREATEFAIL;
	goto abort;
    }

    memset(&tentry, 0, sizeof(struct nvlentry));
    /* Convert to its internal representation; both in host byte order */
    if (errorcode = nvldbentry_to_vlentry(trans, newentry, &tentry)) {
	FreeBlock(trans, blockindex);
	goto abort;
    }

    /* Actually insert the entry in vldb */
    errorcode = ThreadVLentry(trans, blockindex, &tentry);
    if (errorcode) {
	FreeBlock(trans, blockindex);
	goto abort;
    } else {
	errorcode = ubik_EndTrans(trans);
	goto end;
    }

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLCreateEntryEvent, errorcode, AUD_STR,
	       (newentry ? newentry->name : NULL), AUD_END);
    return errorcode;
}


afs_int32
SVL_ChangeAddr(rxcall, ip1, ip2)
     struct rx_call *rxcall;
     afs_int32 ip1, ip2;
{
    struct ubik_trans *trans;
    afs_int32 errorcode;

    COUNT_REQ(VLCHANGEADDR);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL)) {
	errorcode = VL_PERM;
	goto end;
    }

    if (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op))
	goto end;

    VLog(1, ("Change Addr %d -> %d %s\n", ip1, ip2, rxinfo(rxcall)));
    if (errorcode = ChangeIPAddr(ip1, ip2, trans))
	goto abort;
    else {
	errorcode = ubik_EndTrans(trans);
	goto end;
    }

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLChangeAddrEvent, errorcode, AUD_LONG, ip1, AUD_LONG,
	       ip2, AUD_END);
    return errorcode;
}

/* Delete a vldb entry given the volume id. */
afs_int32
SVL_DeleteEntry(rxcall, volid, voltype)
     struct rx_call *rxcall;
     afs_int32 volid;
     afs_int32 voltype;
{
    struct ubik_trans *trans;
    afs_int32 blockindex, errorcode;
    struct nvlentry tentry;

    COUNT_REQ(VLDELETEENTRY);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	END(VL_PERM);

    if ((voltype != -1) && (InvalidVoltype(voltype)))
	END(VL_BADVOLTYPE);

    if (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op))
	goto end;

    VLog(1, ("Delete Volume %d %s\n", volid, rxinfo(rxcall)));
    blockindex = FindByID(trans, volid, voltype, &tentry, &errorcode);
    if (blockindex == 0) {	/* volid not found */
	if (!errorcode)
	    errorcode = VL_NOENT;
	goto abort;
    }

    if (tentry.flags & VLDELETED) {	/* Already deleted; return */
	ABORT(VL_ENTDELETED);
    }
    if (errorcode = RemoveEntry(trans, blockindex, &tentry)) {
	goto abort;
    }
    errorcode = (ubik_EndTrans(trans));
    goto end;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLDeleteEntryEvent, errorcode, AUD_LONG, volid,
	       AUD_END);
    return errorcode;
}


/* Get a vldb entry given its volume id; make sure it's not a deleted entry. */
GetEntryByID(rxcall, volid, voltype, aentry, new, this_op)
     struct rx_call *rxcall;
     afs_int32 volid;
     afs_int32 voltype, new, this_op;
     char *aentry;		/* entry data copied here */
{
    struct ubik_trans *trans;
    afs_int32 blockindex, errorcode;
    struct nvlentry tentry;

    if ((voltype != -1) && (InvalidVoltype(voltype)))
	return VL_BADVOLTYPE;
    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;

    VLog(5, ("GetVolumeByID %d (%d) %s\n", volid, new, rxinfo(rxcall)));
    blockindex = FindByID(trans, volid, voltype, &tentry, &errorcode);
    if (blockindex == 0) {	/* entry not found */
	if (!errorcode)
	    errorcode = VL_NOENT;
	COUNT_ABO;
	ubik_AbortTrans(trans);
	return errorcode;
    }
    if (tentry.flags & VLDELETED) {	/* Entry is deleted! */
	COUNT_ABO;
	ubik_AbortTrans(trans);
	return VL_ENTDELETED;
    }
    /* Convert from the internal to external form */
    if (new == 1)
	vlentry_to_nvldbentry(&tentry, (struct nvldbentry *)aentry);
    else if (new == 2)
	vlentry_to_uvldbentry(&tentry, (struct uvldbentry *)aentry);
    else
	vlentry_to_vldbentry(&tentry, (struct vldbentry *)aentry);
    return (ubik_EndTrans(trans));
}

afs_int32
SVL_GetEntryByID(rxcall, volid, voltype, aentry)
     struct rx_call *rxcall;
     afs_int32 volid, voltype;
     vldbentry *aentry;		/* entry data copied here */
{
    COUNT_REQ(VLGETENTRYBYID);
    return (GetEntryByID(rxcall, volid, voltype, (char *)aentry, 0, this_op));
}

afs_int32
SVL_GetEntryByIDN(rxcall, volid, voltype, aentry)
     struct rx_call *rxcall;
     afs_int32 volid, voltype;
     nvldbentry *aentry;	/* entry data copied here */
{
    COUNT_REQ(VLGETENTRYBYIDN);
    return (GetEntryByID(rxcall, volid, voltype, (char *)aentry, 1, this_op));
}

afs_int32
SVL_GetEntryByIDU(rxcall, volid, voltype, aentry)
     struct rx_call *rxcall;
     afs_int32 volid, voltype;
     uvldbentry *aentry;	/* entry data copied here */
{
    COUNT_REQ(VLGETENTRYBYIDU);
    return (GetEntryByID(rxcall, volid, voltype, (char *)aentry, 2, this_op));
}



/* returns true if the id is a decimal integer, in which case we interpret it
    as an id.  make the cache manager much simpler */
static int
NameIsId(aname)
     register char *aname;
{
    register int tc;
    while (tc = *aname++) {
	if (tc > '9' || tc < '0')
	    return 0;
    }
    return 1;
}

/* Get a vldb entry given the volume's name; of course, very similar to VLGetEntryByID() above. */
GetEntryByName(rxcall, volname, aentry, new, this_op)
     struct rx_call *rxcall;
     char *volname;
     char *aentry;		/* entry data copied here */
     int new, this_op;
{
    struct ubik_trans *trans;
    afs_int32 blockindex, errorcode;
    struct nvlentry tentry;

    if (NameIsId(volname)) {
	return GetEntryByID(rxcall, atoi(volname), -1, aentry, new, this_op);
    }
    if (InvalidVolname(volname))
	return VL_BADNAME;
    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;
    VLog(5, ("GetVolumeByName %s (%d) %s\n", volname, new, rxinfo(rxcall)));
    blockindex = FindByName(trans, volname, &tentry, &errorcode);
    if (blockindex == 0) {	/* entry not found */
	if (!errorcode)
	    errorcode = VL_NOENT;
	COUNT_ABO;
	ubik_AbortTrans(trans);
	return errorcode;
    }
    if (tentry.flags & VLDELETED) {	/* Entry is deleted */
	COUNT_ABO;
	ubik_AbortTrans(trans);
	return VL_ENTDELETED;
    }
    /* Convert to external entry representation */
    if (new == 1)
	vlentry_to_nvldbentry(&tentry, (struct nvldbentry *)aentry);
    else if (new == 2)
	vlentry_to_uvldbentry(&tentry, (struct uvldbentry *)aentry);
    else
	vlentry_to_vldbentry(&tentry, (struct vldbentry *)aentry);
    return (ubik_EndTrans(trans));
}

afs_int32
SVL_GetEntryByNameO(rxcall, volname, aentry)
     struct rx_call *rxcall;
     char *volname;
     struct vldbentry *aentry;	/* entry data copied here */
{
    COUNT_REQ(VLGETENTRYBYNAME);
    return (GetEntryByName(rxcall, volname, (char *)aentry, 0, this_op));
}


afs_int32
SVL_GetEntryByNameN(rxcall, volname, aentry)
     struct rx_call *rxcall;
     char *volname;
     struct nvldbentry *aentry;	/* entry data copied here */
{
    COUNT_REQ(VLGETENTRYBYNAMEN);
    return (GetEntryByName(rxcall, volname, (char *)aentry, 1, this_op));
}

afs_int32
SVL_GetEntryByNameU(rxcall, volname, aentry)
     struct rx_call *rxcall;
     char *volname;
     struct uvldbentry *aentry;	/* entry data copied here */
{
    COUNT_REQ(VLGETENTRYBYNAMEU);
    return (GetEntryByName(rxcall, volname, (char *)aentry, 2, this_op));
}



/* Get the current value of the maximum volume id and bump the volume id counter by Maxvolidbump. */
afs_int32
SVL_GetNewVolumeId(rxcall, Maxvolidbump, newvolumeid)
     struct rx_call *rxcall;
     afs_int32 Maxvolidbump;
     afs_int32 *newvolumeid;
{
    register afs_int32 errorcode, maxvolumeid;
    struct ubik_trans *trans;

    COUNT_REQ(VLGETNEWVOLUMEID);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	END(VL_PERM);

    if (Maxvolidbump < 0 || Maxvolidbump > MAXBUMPCOUNT)
	END(VL_BADVOLIDBUMP);

    if (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op))
	goto end;

    *newvolumeid = maxvolumeid = ntohl(cheader.vital_header.MaxVolumeId);
    maxvolumeid += Maxvolidbump;
    VLog(1, ("GetNewVolid newmax=%d %s\n", maxvolumeid, rxinfo(rxcall)));
    cheader.vital_header.MaxVolumeId = htonl(maxvolumeid);
    if (write_vital_vlheader(trans)) {
	ABORT(VL_IO);
    }
    errorcode = (ubik_EndTrans(trans));
    goto end;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLGetNewVolumeIdEvent, errorcode, AUD_END);
    return errorcode;
}


/* Simple replace the contents of the vldb entry, volid, with
 * newentry. No individual checking/updating per field (alike
 * VLUpdateEntry) is done. */

afs_int32
SVL_ReplaceEntry(rxcall, volid, voltype, newentry, releasetype)
     struct rx_call *rxcall;
     afs_int32 volid;
     afs_int32 voltype;
     struct vldbentry *newentry;
     afs_int32 releasetype;
{
    struct ubik_trans *trans;
    afs_int32 blockindex, errorcode, typeindex;
    int hashnewname;
    int hashVol[MAXTYPES];
    struct nvlentry tentry;

    COUNT_REQ(VLREPLACEENTRY);
    for (typeindex = 0; typeindex < MAXTYPES; typeindex++)
	hashVol[typeindex] = 0;
    hashnewname = 0;
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	END(VL_PERM);

    if (errorcode = check_vldbentry(newentry))
	goto end;

    if (voltype != -1 && InvalidVoltype(voltype))
	END(VL_BADVOLTYPE);

    if (releasetype && InvalidReleasetype(releasetype))
	END(VL_BADRELLOCKTYPE);
    if (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op))
	goto end;

    VLog(1, ("OReplace Volume %d %s\n", volid, rxinfo(rxcall)));
    /* find vlentry we're changing */
    blockindex = FindByID(trans, volid, voltype, &tentry, &errorcode);
    if (blockindex == 0) {	/* entry not found */
	if (!errorcode)
	    errorcode = VL_NOENT;
	goto abort;
    }

    /* check that we're not trying to change the RW vol ID */
    if (newentry->volumeId[RWVOL] != tentry.volumeId[RWVOL]) {
	ABORT(VL_BADENTRY);
    }

    /* unhash volid entries if they're disappearing or changing.
     * Remember if we need to hash in the new value (we don't have to
     * rehash if volid stays same */
    for (typeindex = ROVOL; typeindex <= BACKVOL; typeindex++) {
	if (tentry.volumeId[typeindex] != newentry->volumeId[typeindex]) {
	    if (tentry.volumeId[typeindex])
		if (errorcode =
		    UnhashVolid(trans, typeindex, blockindex, &tentry)) {
		    goto abort;
		}
	    /* we must rehash new id if the id is different and the ID is nonzero */
	    hashVol[typeindex] = 1;	/* must rehash this guy if he exists */
	}
    }

    /* Rehash volname if it changes */
    if (strcmp(newentry->name, tentry.name)) {	/* Name changes; redo hashing */
	if (errorcode = UnhashVolname(trans, blockindex, &tentry)) {
	    goto abort;
	}
	hashnewname = 1;
    }

    /* after this, tentry is new entry, not old one.  vldbentry_to_vlentry
     * doesn't touch hash chains */
    if (errorcode = vldbentry_to_vlentry(trans, newentry, &tentry)) {
	goto abort;
    }

    for (typeindex = ROVOL; typeindex <= BACKVOL; typeindex++) {
	if (hashVol[typeindex] && tentry.volumeId[typeindex]) {
	    if (errorcode = HashVolid(trans, typeindex, blockindex, &tentry)) {
		goto abort;
	    }
	}
    }

    if (hashnewname)
	HashVolname(trans, blockindex, &tentry);

    if (releasetype)
	ReleaseEntry(&tentry, releasetype);	/* Unlock entry if necessary */
    if (vlentrywrite(trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }

    END(ubik_EndTrans(trans));

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLReplaceVLEntryEvent, errorcode, AUD_LONG, volid,
	       AUD_END);
    return errorcode;
}

afs_int32
SVL_ReplaceEntryN(rxcall, volid, voltype, newentry, releasetype)
     struct rx_call *rxcall;
     afs_int32 volid;
     afs_int32 voltype;
     struct nvldbentry *newentry;
     afs_int32 releasetype;
{
    struct ubik_trans *trans;
    afs_int32 blockindex, errorcode, typeindex;
    int hashnewname;
    int hashVol[MAXTYPES];
    struct nvlentry tentry;

    COUNT_REQ(VLREPLACEENTRYN);
    for (typeindex = 0; typeindex < MAXTYPES; typeindex++)
	hashVol[typeindex] = 0;
    hashnewname = 0;
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	END(VL_PERM);

    if (errorcode = check_nvldbentry(newentry))
	goto end;

    if (voltype != -1 && InvalidVoltype(voltype))
	END(VL_BADVOLTYPE);

    if (releasetype && InvalidReleasetype(releasetype))
	END(VL_BADRELLOCKTYPE);
    if (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op))
	goto end;

    VLog(1, ("Replace Volume %d %s\n", volid, rxinfo(rxcall)));
    /* find vlentry we're changing */
    blockindex = FindByID(trans, volid, voltype, &tentry, &errorcode);
    if (blockindex == 0) {	/* entry not found */
	if (!errorcode)
	    errorcode = VL_NOENT;
	goto abort;
    }

    /* check that we're not trying to change the RW vol ID */
    if (newentry->volumeId[RWVOL] != tentry.volumeId[RWVOL]) {
	ABORT(VL_BADENTRY);
    }

    /* unhash volid entries if they're disappearing or changing.
     * Remember if we need to hash in the new value (we don't have to
     * rehash if volid stays same */
    for (typeindex = ROVOL; typeindex <= BACKVOL; typeindex++) {
	if (tentry.volumeId[typeindex] != newentry->volumeId[typeindex]) {
	    if (tentry.volumeId[typeindex])
		if (errorcode =
		    UnhashVolid(trans, typeindex, blockindex, &tentry)) {
		    goto abort;
		}
	    /* we must rehash new id if the id is different and the ID is nonzero */
	    hashVol[typeindex] = 1;	/* must rehash this guy if he exists */
	}
    }

    /* Rehash volname if it changes */
    if (strcmp(newentry->name, tentry.name)) {	/* Name changes; redo hashing */
	if (errorcode = UnhashVolname(trans, blockindex, &tentry)) {
	    goto abort;
	}
	hashnewname = 1;
    }

    /* after this, tentry is new entry, not old one.  vldbentry_to_vlentry
     * doesn't touch hash chains */
    if (errorcode = nvldbentry_to_vlentry(trans, newentry, &tentry)) {
	goto abort;
    }

    for (typeindex = ROVOL; typeindex <= BACKVOL; typeindex++) {
	if (hashVol[typeindex] && tentry.volumeId[typeindex]) {
	    if (errorcode = HashVolid(trans, typeindex, blockindex, &tentry)) {
		goto abort;
	    }
	}
    }

    if (hashnewname)
	HashVolname(trans, blockindex, &tentry);

    if (releasetype)
	ReleaseEntry(&tentry, releasetype);	/* Unlock entry if necessary */
    if (vlentrywrite(trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }

    END(ubik_EndTrans(trans));

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLReplaceVLEntryEvent, errorcode, AUD_LONG, volid,
	       AUD_END);
    return errorcode;
}


/* Update a vldb entry (accessed thru its volume id). Almost all of the entry's fields can be modified in a single call by setting the appropriate bits in the Mask field in VldbUpdateentry. */
/* this routine may never have been tested; use replace entry instead unless you're brave */
afs_int32
SVL_UpdateEntry(rxcall, volid, voltype, updateentry, releasetype)
     struct rx_call *rxcall;
     afs_int32 volid;
     afs_int32 voltype;
     afs_int32 releasetype;
     struct VldbUpdateEntry *updateentry;	/* Update entry copied here */
{
    struct ubik_trans *trans;
    afs_int32 blockindex, errorcode;
    struct nvlentry tentry;

    COUNT_REQ(VLUPDATEENTRY);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	END(VL_PERM);
    if ((voltype != -1) && (InvalidVoltype(voltype)))
	END(VL_BADVOLTYPE);
    if (releasetype && InvalidReleasetype(releasetype))
	END(VL_BADRELLOCKTYPE);
    if (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op))
	goto end;

    VLog(1, ("Update Volume %d %s\n", volid, rxinfo(rxcall)));
    blockindex = FindByID(trans, volid, voltype, &tentry, &errorcode);
    if (blockindex == 0) {	/* entry not found */
	if (!errorcode)
	    errorcode = VL_NOENT;
	goto abort;
    }

    /* Do the actual updating of the entry, tentry. */
    if (errorcode =
	get_vldbupdateentry(trans, blockindex, updateentry, &tentry)) {
	goto abort;
    }
    if (releasetype)
	ReleaseEntry(&tentry, releasetype);	/* Unlock entry if necessary */
    if (vlentrywrite(trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }
    END(ubik_EndTrans(trans));

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLUpdateEntryEvent, errorcode, AUD_LONG, volid,
	       AUD_END);
    return errorcode;
}


afs_int32
SVL_UpdateEntryByName(rxcall, volname, updateentry, releasetype)
     struct rx_call *rxcall;
     char *volname;
     afs_int32 releasetype;
     struct VldbUpdateEntry *updateentry;	/* Update entry copied here */
{
    struct ubik_trans *trans;
    afs_int32 blockindex, errorcode;
    struct nvlentry tentry;

    COUNT_REQ(VLUPDATEENTRYBYNAME);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	END(VL_PERM);
    if (releasetype && InvalidReleasetype(releasetype))
	END(VL_BADRELLOCKTYPE);
    if (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op))
	goto end;

    blockindex = FindByName(trans, volname, &tentry, &errorcode);
    if (blockindex == 0) {	/* entry not found */
	if (!errorcode)
	    errorcode = VL_NOENT;
	goto abort;
    }

    /* Do the actual updating of the entry, tentry. */
    if (errorcode =
	get_vldbupdateentry(trans, blockindex, updateentry, &tentry)) {
	goto abort;
    }
    if (releasetype)
	ReleaseEntry(&tentry, releasetype);	/* Unlock entry if necessary */
    if (vlentrywrite(trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }
    END(ubik_EndTrans(trans));

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLUpdateEntryEvent, errorcode, AUD_LONG, -1, AUD_END);
    return errorcode;
}


/* Set a lock to the vldb entry for volid (of type voltype if not -1). */
afs_int32
SVL_SetLock(rxcall, volid, voltype, voloper)
     struct rx_call *rxcall;
     afs_int32 volid;
     afs_int32 voltype;
     afs_int32 voloper;
{
    afs_int32 timestamp, blockindex, errorcode;
    struct ubik_trans *trans;
    struct nvlentry tentry;

    COUNT_REQ(VLSETLOCK);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	END(VL_PERM);
    if ((voltype != -1) && (InvalidVoltype(voltype)))
	END(VL_BADVOLTYPE);
    if (InvalidOperation(voloper))
	END(VL_BADVOLOPER);
    if (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op))
	goto end;

    VLog(1, ("SetLock Volume %d %s\n", volid, rxinfo(rxcall)));
    blockindex = FindByID(trans, volid, voltype, &tentry, &errorcode);
    if (blockindex == NULLO) {
	if (!errorcode)
	    errorcode = VL_NOENT;
	goto abort;
    }
    if (tentry.flags & VLDELETED) {
	ABORT(VL_ENTDELETED);
    }
    timestamp = FT_ApproxTime();

    /* Check if entry is already locked; note that we unlock any entry
     * locked more than MAXLOCKTIME seconds */
    if ((tentry.LockTimestamp)
	&& ((timestamp - tentry.LockTimestamp) < MAXLOCKTIME)) {
	ABORT(VL_ENTRYLOCKED);
    }

    /* Consider it an unlocked entry: set current timestamp, caller
     * and active vol operation */
    tentry.LockTimestamp = timestamp;
    tentry.LockAfsId = 0;	/* Not implemented yet */
    if (tentry.flags & VLOP_RELEASE) {
	ABORT(VL_RERELEASE);
    }
    tentry.flags &= ~VLOP_ALLOPERS;	/* Clear any possible older operation bit */
    tentry.flags |= voloper;

    if (vlentrywrite(trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }
    END(ubik_EndTrans(trans));

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLSetLockEvent, errorcode, AUD_LONG, volid, AUD_END);
    return errorcode;
}


/* Release an already locked vldb entry. Releasetype determines what
 * fields (afsid and/or volume operation) will be cleared along with
 * the lock time stamp. */

afs_int32
SVL_ReleaseLock(rxcall, volid, voltype, releasetype)
     struct rx_call *rxcall;
     afs_int32 volid;
     afs_int32 voltype;
     afs_int32 releasetype;
{
    afs_int32 blockindex, errorcode;
    struct ubik_trans *trans;
    struct nvlentry tentry;

    COUNT_REQ(VLRELEASELOCK);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	END(VL_PERM);
    if ((voltype != -1) && (InvalidVoltype(voltype)))
	END(VL_BADVOLTYPE);
    if (releasetype && InvalidReleasetype(releasetype))
	END(VL_BADRELLOCKTYPE);
    if (errorcode = Init_VLdbase(&trans, LOCKWRITE, this_op))
	goto end;

    VLog(1, ("ReleaseLock Volume %d %s\n", volid, rxinfo(rxcall)));
    blockindex = FindByID(trans, volid, voltype, &tentry, &errorcode);
    if (blockindex == NULLO) {
	if (!errorcode)
	    errorcode = VL_NOENT;
	goto abort;
    }
    if (tentry.flags & VLDELETED) {
	ABORT(VL_ENTDELETED);
    }
    if (releasetype)
	ReleaseEntry(&tentry, releasetype);	/* Unlock the appropriate fields */
    if (vlentrywrite(trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }
    END(ubik_EndTrans(trans));

  abort:
    COUNT_ABO;
    ubik_AbortTrans(trans);

  end:
    osi_auditU(rxcall, VLReleaseLockEvent, errorcode, AUD_LONG, volid,
	       AUD_END);
    return errorcode;
}


/* ListEntry returns a single vldb entry, aentry, with offset previous_index; the remaining parameters (i.e. next_index) are used so that sequential calls to this routine will get the next (all) vldb entries. */
afs_int32
SVL_ListEntry(rxcall, previous_index, count, next_index, aentry)
     struct rx_call *rxcall;
     afs_int32 previous_index;
     afs_int32 *count;
     afs_int32 *next_index;
     struct vldbentry *aentry;
{
    int errorcode;
    struct ubik_trans *trans;
    struct nvlentry tentry;

    COUNT_REQ(VLLISTENTRY);
    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;
    VLog(25, ("OListEntry index=%d %s\n", previous_index, rxinfo(rxcall)));
    *next_index = NextEntry(trans, previous_index, &tentry, count);
    if (*next_index)
	vlentry_to_vldbentry(&tentry, aentry);
    return (ubik_EndTrans(trans));
}

/* ListEntry returns a single vldb entry, aentry, with offset previous_index; the remaining parameters (i.e. next_index) are used so that sequential calls to this routine will get the next (all) vldb entries. */
afs_int32
SVL_ListEntryN(rxcall, previous_index, count, next_index, aentry)
     struct rx_call *rxcall;
     afs_int32 previous_index;
     afs_int32 *count;
     afs_int32 *next_index;
     struct nvldbentry *aentry;
{
    int errorcode;
    struct ubik_trans *trans;
    struct nvlentry tentry;

    COUNT_REQ(VLLISTENTRYN);
    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;
    VLog(25, ("ListEntry index=%d %s\n", previous_index, rxinfo(rxcall)));
    *next_index = NextEntry(trans, previous_index, &tentry, count);
    if (*next_index)
	vlentry_to_nvldbentry(&tentry, aentry);
    return (ubik_EndTrans(trans));
}


/* Retrieves in vldbentries all vldb entries that match the specified attributes (by server number, partition, volume type, and flag); if volume id is specified then the associated list for that entry is returned. CAUTION: This could be a very expensive call since in most cases sequential search of all vldb entries is performed. */
afs_int32
SVL_ListAttributes(rxcall, attributes, nentries, vldbentries)
     struct rx_call *rxcall;
     struct VldbListByAttributes *attributes;
     afs_int32 *nentries;
     bulkentries *vldbentries;
{
    int errorcode, allocCount = 0;
    struct ubik_trans *trans;
    struct nvlentry tentry;
    struct vldbentry *Vldbentry = 0, *VldbentryFirst = 0, *VldbentryLast = 0;
    int pollcount = 0;

    COUNT_REQ(VLLISTATTRIBUTES);
    vldbentries->bulkentries_val = 0;
    vldbentries->bulkentries_len = *nentries = 0;
    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;
    allocCount = VLDBALLOCCOUNT;
    Vldbentry = VldbentryFirst = vldbentries->bulkentries_val =
	(vldbentry *) malloc(allocCount * sizeof(vldbentry));
    if (Vldbentry == NULL) {
	COUNT_ABO;
	ubik_AbortTrans(trans);
	return VL_NOMEM;
    }
    VldbentryLast = VldbentryFirst + allocCount;
    /* Handle the attribute by volume id totally separate of the rest (thus additional Mask values are ignored if VLLIST_VOLUMEID is set!) */
    if (attributes->Mask & VLLIST_VOLUMEID) {
	afs_int32 blockindex;

	blockindex =
	    FindByID(trans, attributes->volumeid, -1, &tentry, &errorcode);
	if (blockindex == 0) {
	    if (!errorcode)
		errorcode = VL_NOENT;
	    COUNT_ABO;
	    ubik_AbortTrans(trans);
	    if (vldbentries->bulkentries_val)
		free((char *)vldbentries->bulkentries_val);
	    vldbentries->bulkentries_val = 0;
	    vldbentries->bulkentries_len = 0;
	    return errorcode;
	}
	if (errorcode =
	    put_attributeentry(&Vldbentry, &VldbentryFirst, &VldbentryLast,
			       vldbentries, &tentry, nentries, &allocCount)) {
	    COUNT_ABO;
	    ubik_AbortTrans(trans);
	    if (vldbentries->bulkentries_val)
		free((char *)vldbentries->bulkentries_val);
	    vldbentries->bulkentries_val = 0;
	    vldbentries->bulkentries_len = 0;
	    return VL_SIZEEXCEEDED;
	}
    } else {
	afs_int32 nextblockindex = 0, count = 0, k, match = 0;
	while (nextblockindex =
	       NextEntry(trans, nextblockindex, &tentry, &count)) {
	    if (++pollcount > 50) {
		IOMGR_Poll();
		pollcount = 0;
	    }
	    match = 0;
	    if (attributes->Mask & VLLIST_SERVER) {
		int serverindex;
		if ((serverindex =
		     IpAddrToRelAddr(attributes->server,
				     (struct ubik_trans *)0)) == -1)
		    continue;
		for (k = 0; k < OMAXNSERVERS; k++) {
		    if (tentry.serverNumber[k] == BADSERVERID)
			break;
		    if (tentry.serverNumber[k] == serverindex) {
			match = 1;
			break;
		    }
		}
		if (!match)
		    continue;
	    }
	    if (attributes->Mask & VLLIST_PARTITION) {
		if (match) {
		    if (tentry.serverPartition[k] != attributes->partition)
			continue;
		} else {
		    for (k = 0; k < OMAXNSERVERS; k++) {
			if (tentry.serverNumber[k] == BADSERVERID)
			    break;
			if (tentry.serverPartition[k] ==
			    attributes->partition) {
			    match = 1;
			    break;
			}
		    }
		    if (!match)
			continue;
		}
	    }

	    if (attributes->Mask & VLLIST_FLAG) {
		if (!(tentry.flags & attributes->flag))
		    continue;
	    }
	    if (errorcode =
		put_attributeentry(&Vldbentry, &VldbentryFirst,
				   &VldbentryLast, vldbentries, &tentry,
				   nentries, &allocCount)) {
		COUNT_ABO;
		ubik_AbortTrans(trans);
		if (vldbentries->bulkentries_val)
		    free((char *)vldbentries->bulkentries_val);
		vldbentries->bulkentries_val = 0;
		vldbentries->bulkentries_len = 0;
		return errorcode;
	    }
	}
    }
    if (vldbentries->bulkentries_len
	&& (allocCount > vldbentries->bulkentries_len)) {

	vldbentries->bulkentries_val =
	    (vldbentry *) realloc(vldbentries->bulkentries_val,
				  vldbentries->bulkentries_len *
				  sizeof(vldbentry));
	if (vldbentries->bulkentries_val == NULL) {
	    COUNT_ABO;
	    ubik_AbortTrans(trans);
	    return VL_NOMEM;
	}
    }
    VLog(5,
	 ("ListAttrs nentries=%d %s\n", vldbentries->bulkentries_len,
	  rxinfo(rxcall)));
    return (ubik_EndTrans(trans));
}

afs_int32
SVL_ListAttributesN(rxcall, attributes, nentries, vldbentries)
     struct rx_call *rxcall;
     struct VldbListByAttributes *attributes;
     afs_int32 *nentries;
     nbulkentries *vldbentries;
{
    int errorcode, allocCount = 0;
    struct ubik_trans *trans;
    struct nvlentry tentry;
    struct nvldbentry *Vldbentry = 0, *VldbentryFirst = 0, *VldbentryLast = 0;
    int pollcount = 0;

    COUNT_REQ(VLLISTATTRIBUTESN);
    vldbentries->nbulkentries_val = 0;
    vldbentries->nbulkentries_len = *nentries = 0;
    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;
    allocCount = VLDBALLOCCOUNT;
    Vldbentry = VldbentryFirst = vldbentries->nbulkentries_val =
	(nvldbentry *) malloc(allocCount * sizeof(nvldbentry));
    if (Vldbentry == NULL) {
	COUNT_ABO;
	ubik_AbortTrans(trans);
	return VL_NOMEM;
    }
    VldbentryLast = VldbentryFirst + allocCount;
    /* Handle the attribute by volume id totally separate of the rest (thus additional Mask values are ignored if VLLIST_VOLUMEID is set!) */
    if (attributes->Mask & VLLIST_VOLUMEID) {
	afs_int32 blockindex;

	blockindex =
	    FindByID(trans, attributes->volumeid, -1, &tentry, &errorcode);
	if (blockindex == 0) {
	    if (!errorcode)
		errorcode = VL_NOENT;
	    COUNT_ABO;
	    ubik_AbortTrans(trans);
	    if (vldbentries->nbulkentries_val)
		free((char *)vldbentries->nbulkentries_val);
	    vldbentries->nbulkentries_val = 0;
	    vldbentries->nbulkentries_len = 0;
	    return errorcode;
	}
	if (errorcode =
	    put_nattributeentry(&Vldbentry, &VldbentryFirst, &VldbentryLast,
				vldbentries, &tentry, 0, 0, nentries,
				&allocCount)) {
	    COUNT_ABO;
	    ubik_AbortTrans(trans);
	    if (vldbentries->nbulkentries_val)
		free((char *)vldbentries->nbulkentries_val);
	    vldbentries->nbulkentries_val = 0;
	    vldbentries->nbulkentries_len = 0;
	    return VL_SIZEEXCEEDED;
	}
    } else {
	afs_int32 nextblockindex = 0, count = 0, k, match = 0;
	while (nextblockindex =
	       NextEntry(trans, nextblockindex, &tentry, &count)) {
	    if (++pollcount > 50) {
		IOMGR_Poll();
		pollcount = 0;
	    }

	    match = 0;
	    if (attributes->Mask & VLLIST_SERVER) {
		int serverindex;
		if ((serverindex =
		     IpAddrToRelAddr(attributes->server,
				     (struct ubik_trans *)0)) == -1)
		    continue;
		for (k = 0; k < NMAXNSERVERS; k++) {
		    if (tentry.serverNumber[k] == BADSERVERID)
			break;
		    if (tentry.serverNumber[k] == serverindex) {
			match = 1;
			break;
		    }
		}
		if (!match)
		    continue;
	    }
	    if (attributes->Mask & VLLIST_PARTITION) {
		if (match) {
		    if (tentry.serverPartition[k] != attributes->partition)
			continue;
		} else {
		    for (k = 0; k < NMAXNSERVERS; k++) {
			if (tentry.serverNumber[k] == BADSERVERID)
			    break;
			if (tentry.serverPartition[k] ==
			    attributes->partition) {
			    match = 1;
			    break;
			}
		    }
		    if (!match)
			continue;
		}
	    }

	    if (attributes->Mask & VLLIST_FLAG) {
		if (!(tentry.flags & attributes->flag))
		    continue;
	    }
	    if (errorcode =
		put_nattributeentry(&Vldbentry, &VldbentryFirst,
				    &VldbentryLast, vldbentries, &tentry, 0,
				    0, nentries, &allocCount)) {
		COUNT_ABO;
		ubik_AbortTrans(trans);
		if (vldbentries->nbulkentries_val)
		    free((char *)vldbentries->nbulkentries_val);
		vldbentries->nbulkentries_val = 0;
		vldbentries->nbulkentries_len = 0;
		return errorcode;
	    }
	}
    }
    if (vldbentries->nbulkentries_len
	&& (allocCount > vldbentries->nbulkentries_len)) {

	vldbentries->nbulkentries_val =
	    (nvldbentry *) realloc(vldbentries->nbulkentries_val,
				   vldbentries->nbulkentries_len *
				   sizeof(nvldbentry));
	if (vldbentries->nbulkentries_val == NULL) {
	    COUNT_ABO;
	    ubik_AbortTrans(trans);
	    return VL_NOMEM;
	}
    }
    VLog(5,
	 ("NListAttrs nentries=%d %s\n", vldbentries->nbulkentries_len,
	  rxinfo(rxcall)));
    return (ubik_EndTrans(trans));
}


afs_int32
SVL_ListAttributesN2(rxcall, attributes, name, startindex, nentries,
		     vldbentries, nextstartindex)
     struct rx_call *rxcall;
     struct VldbListByAttributes *attributes;
     char *name;		/* Wildcarded volume name */
     afs_int32 startindex;
     afs_int32 *nentries;
     nbulkentries *vldbentries;
     afs_int32 *nextstartindex;
{
    int errorcode = 0, maxCount = VLDBALLOCCOUNT;
    struct ubik_trans *trans;
    struct nvlentry tentry;
    struct nvldbentry *Vldbentry = 0, *VldbentryFirst = 0, *VldbentryLast = 0;
    afs_int32 blockindex = 0, count = 0, k, match, matchindex;
    int serverindex = -1;	/* no server found */
    int findserver = 0, findpartition = 0, findflag = 0, findname = 0;
    char *t;
    int pollcount = 0;
    int namematchRWBK, namematchRO, thismatch, matchtype;
    char volumename[VL_MAXNAMELEN];
#ifdef HAVE_POSIX_REGEX
    regex_t re;
    int need_regfree = 0;
#endif

    COUNT_REQ(VLLISTATTRIBUTESN2);
    vldbentries->nbulkentries_val = 0;
    vldbentries->nbulkentries_len = 0;
    *nentries = 0;
    *nextstartindex = -1;

    errorcode = Init_VLdbase(&trans, LOCKREAD, this_op);
    if (errorcode)
	return errorcode;

    Vldbentry = VldbentryFirst = vldbentries->nbulkentries_val =
	(nvldbentry *) malloc(maxCount * sizeof(nvldbentry));
    if (Vldbentry == NULL) {
	COUNT_ABO;
	ubik_AbortTrans(trans);
	return VL_NOMEM;
    }

    VldbentryLast = VldbentryFirst + maxCount;

    /* Handle the attribute by volume id totally separate of the rest
     * (thus additional Mask values are ignored if VLLIST_VOLUMEID is set!)
     */
    if (attributes->Mask & VLLIST_VOLUMEID) {
	blockindex =
	    FindByID(trans, attributes->volumeid, -1, &tentry, &errorcode);
	if (blockindex == 0) {
	    if (!errorcode)
		errorcode = VL_NOENT;
	} else {
	    errorcode =
		put_nattributeentry(&Vldbentry, &VldbentryFirst,
				    &VldbentryLast, vldbentries, &tentry, 0,
				    0, nentries, &maxCount);
	    if (errorcode)
		goto done;
	}
    }

    /* Search each entry in the database and return all entries
     * that match the request. It checks volumename (with
     * wildcarding), entry flags, server, and partition.
     */
    else {
	/* Get the server index for matching server address */
	if (attributes->Mask & VLLIST_SERVER) {
	    serverindex =
		IpAddrToRelAddr(attributes->server, (struct ubik_trans *)0);
	    if (serverindex == -1)
		goto done;
	    findserver = 1;
	}
	findpartition = ((attributes->Mask & VLLIST_PARTITION) ? 1 : 0);
	findflag = ((attributes->Mask & VLLIST_FLAG) ? 1 : 0);
	if (name && (strcmp(name, ".*") != 0) && (strcmp(name, "") != 0)) {
	    sprintf(volumename, "^%s$", name);
#ifdef HAVE_POSIX_REGEX
	    if (regcomp(&re, volumename, REG_NOSUB) != 0) {
		errorcode = VL_BADNAME;
		goto done;
	    }
	    need_regfree = 1;
#else
	    t = (char *)re_comp(volumename);
	    if (t) {
		errorcode = VL_BADNAME;
		goto done;
	    }
#endif
	    findname = 1;
	}

	/* Read each entry and see if it is the one we want */
	blockindex = startindex;
	while (blockindex = NextEntry(trans, blockindex, &tentry, &count)) {
	    if (++pollcount > 50) {
		IOMGR_Poll();
		pollcount = 0;
	    }

	    /* Step through each server index searching for a match.
	     * Match to an existing RW, BK, or RO volume name (preference
	     * is in this order). Remember which index we matched against.
	     */
	    namematchRWBK = namematchRO = 0;	/* 0->notTried; 1->match; 2->noMatch */
	    match = 0;
	    for (k = 0;
		 (k < NMAXNSERVERS
		  && (tentry.serverNumber[k] != BADSERVERID)); k++) {
		thismatch = 0;	/* does this index match */

		/* Match against the RW or BK volume name. Remember
		 * results in namematchRWBK. Prefer RW over BK.
		 */
		if (tentry.serverFlags[k] & VLSF_RWVOL) {
		    /* Does the name match the RW name */
		    if (tentry.flags & VLF_RWEXISTS) {
			if (findname) {
			    sprintf(volumename, "%s", tentry.name);
#ifdef HAVE_POSIX_REGEX
			    if (regexec(&re, volumename, 0, NULL, 0) == 0) {
				thismatch = VLSF_RWVOL;
			    }
#else
			    if (re_exec(volumename)) {
				thismatch = VLSF_RWVOL;
			    }
#endif
			} else {
			    thismatch = VLSF_RWVOL;
			}
		    }

		    /* Does the name match the BK name */
		    if (!thismatch && (tentry.flags & VLF_BACKEXISTS)) {
			if (findname) {
			    sprintf(volumename, "%s.backup", tentry.name);
#ifdef HAVE_POSIX_REGEX
			    if (regexec(&re, volumename, 0, NULL, 0) == 0) {
				thismatch = VLSF_BACKVOL;
			    }
#else
			    if (re_exec(volumename)) {
				thismatch = VLSF_BACKVOL;
			    }
#endif
			} else {
			    thismatch = VLSF_BACKVOL;
			}
		    }

		    namematchRWBK = (thismatch ? 1 : 2);
		}

		/* Match with the RO volume name. Compare once and 
		 * remember results in namematchRO. Note that this will
		 * pick up entries marked NEWREPSITEs and DONTUSE.
		 */
		else {
		    if (tentry.flags & VLF_ROEXISTS) {
			if (findname) {
			    if (namematchRO) {
				thismatch =
				    ((namematchRO == 1) ? VLSF_ROVOL : 0);
			    } else {
				sprintf(volumename, "%s.readonly",
					tentry.name);
#ifdef HAVE_POSIX_REGEX
			    if (regexec(&re, volumename, 0, NULL, 0) == 0) {
				thismatch = VLSF_ROVOL;
			    }
#else
				if (re_exec(volumename))
				    thismatch = VLSF_ROVOL;
#endif
			    }
			} else {
			    thismatch = VLSF_ROVOL;
			}
		    }
		    namematchRO = (thismatch ? 1 : 2);
		}

		/* Is there a server match */
		if (thismatch && findserver
		    && (tentry.serverNumber[k] != serverindex))
		    thismatch = 0;

		/* Is there a partition match */
		if (thismatch && findpartition
		    && (tentry.serverPartition[k] != attributes->partition))
		    thismatch = 0;

		/* Is there a flag match */
		if (thismatch && findflag
		    && !(tentry.flags & attributes->flag))
		    thismatch = 0;

		/* We found a match. Remember the index, and type */
		if (thismatch) {
		    match = 1;
		    matchindex = k;
		    matchtype = thismatch;
		}

		/* Since we prefer RW and BK volume matches over RO matches,
		 * if we have already checked the RWBK name, then we already
		 * found the best match and so end the search.
		 *
		 * If we tried matching against the RW, BK, and RO volume names
		 * and both failed, then we end the search (none will match).
		 */
		if ((match && namematchRWBK)
		    || ((namematchRWBK == 2) && (namematchRO == 2)))
		    break;
	    }

	    /* Passed all the tests. Take it */
	    if (match) {
		errorcode =
		    put_nattributeentry(&Vldbentry, &VldbentryFirst,
					&VldbentryLast, vldbentries, &tentry,
					matchtype, matchindex, nentries,
					&maxCount);
		if (errorcode)
		    goto done;

		if (*nentries >= maxCount)
		    break;	/* collected the max */
	    }
	}
	*nextstartindex = (blockindex ? blockindex : -1);
    }

  done:
#ifdef HAVE_POSIX_REGEX
    if (need_regfree)
	regfree(&re);
#endif

    if (errorcode) {
	COUNT_ABO;
	ubik_AbortTrans(trans);
	if (vldbentries->nbulkentries_val)
	    free((char *)vldbentries->nbulkentries_val);
	vldbentries->nbulkentries_val = 0;
	vldbentries->nbulkentries_len = 0;
	*nextstartindex = -1;
	return errorcode;
    } else {
	VLog(5,
	     ("N2ListAttrs nentries=%d %s\n", vldbentries->nbulkentries_len,
	      rxinfo(rxcall)));
	return (ubik_EndTrans(trans));
    }
}


/* Retrieves in vldbentries all vldb entries that match the specified
 * attributes (by server number, partition, volume type, and flag); if
 * volume id is specified then the associated list for that entry is
 * returned. CAUTION: This could be a very expensive call since in most
 * cases sequential search of all vldb entries is performed.
 */
afs_int32
SVL_LinkedList(rxcall, attributes, nentries, vldbentries)
     struct rx_call *rxcall;
     struct VldbListByAttributes *attributes;
     afs_int32 *nentries;
     vldb_list *vldbentries;
{
    int errorcode;
    struct ubik_trans *trans;
    struct nvlentry tentry;
    vldblist vllist, *vllistptr;
    afs_int32 blockindex, count, k, match;
    int serverindex;
    int pollcount = 0;

    COUNT_REQ(VLLINKEDLIST);
    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;

    *nentries = 0;
    vldbentries->node = NULL;
    vllistptr = &vldbentries->node;

    /* List by volumeid */
    if (attributes->Mask & VLLIST_VOLUMEID) {
	blockindex =
	    FindByID(trans, attributes->volumeid, -1, &tentry, &errorcode);
	if (!blockindex) {
	    COUNT_ABO;
	    ubik_AbortTrans(trans);
	    return (errorcode ? errorcode : VL_NOENT);
	}

	vllist = (single_vldbentry *) malloc(sizeof(single_vldbentry));
	if (vllist == NULL) {
	    COUNT_ABO;
	    ubik_AbortTrans(trans);
	    return VL_NOMEM;
	}
	vlentry_to_vldbentry(&tentry, &vllist->VldbEntry);
	vllist->next_vldb = NULL;

	*vllistptr = vllist;	/* Thread onto list */
	vllistptr = &vllist->next_vldb;
	(*nentries)++;
    }

    /* Search by server, partition, and flags */
    else {
	for (blockindex = NextEntry(trans, 0, &tentry, &count); blockindex;
	     blockindex = NextEntry(trans, blockindex, &tentry, &count)) {
	    match = 0;

	    if (++pollcount > 50) {
		IOMGR_Poll();
		pollcount = 0;
	    }

	    /* Does this volume exist on the desired server */
	    if (attributes->Mask & VLLIST_SERVER) {
		serverindex =
		    IpAddrToRelAddr(attributes->server,
				    (struct ubik_trans *)0);
		if (serverindex == -1)
		    continue;
		for (k = 0; k < OMAXNSERVERS; k++) {
		    if (tentry.serverNumber[k] == BADSERVERID)
			break;
		    if (tentry.serverNumber[k] == serverindex) {
			match = 1;
			break;
		    }
		}
		if (!match)
		    continue;
	    }

	    /* Does this volume exist on the desired partition */
	    if (attributes->Mask & VLLIST_PARTITION) {
		if (match) {
		    if (tentry.serverPartition[k] != attributes->partition)
			match = 0;
		} else {
		    for (k = 0; k < OMAXNSERVERS; k++) {
			if (tentry.serverNumber[k] == BADSERVERID)
			    break;
			if (tentry.serverPartition[k] ==
			    attributes->partition) {
			    match = 1;
			    break;
			}
		    }
		}
		if (!match)
		    continue;
	    }

	    /* Does this volume have the desired flags */
	    if (attributes->Mask & VLLIST_FLAG) {
		if (!(tentry.flags & attributes->flag))
		    continue;
	    }

	    vllist = (single_vldbentry *) malloc(sizeof(single_vldbentry));
	    if (vllist == NULL) {
		COUNT_ABO;
		ubik_AbortTrans(trans);
		return VL_NOMEM;
	    }
	    vlentry_to_vldbentry(&tentry, &vllist->VldbEntry);
	    vllist->next_vldb = NULL;

	    *vllistptr = vllist;	/* Thread onto list */
	    vllistptr = &vllist->next_vldb;
	    (*nentries)++;
	    if (smallMem && (*nentries >= VLDBALLOCCOUNT)) {
		COUNT_ABO;
		ubik_AbortTrans(trans);
		return VL_SIZEEXCEEDED;
	    }
	}
    }
    *vllistptr = NULL;
    return (ubik_EndTrans(trans));
}

afs_int32
SVL_LinkedListN(rxcall, attributes, nentries, vldbentries)
     struct rx_call *rxcall;
     struct VldbListByAttributes *attributes;
     afs_int32 *nentries;
     nvldb_list *vldbentries;
{
    int errorcode;
    struct ubik_trans *trans;
    struct nvlentry tentry;
    nvldblist vllist, *vllistptr;
    afs_int32 blockindex, count, k, match;
    int serverindex;
    int pollcount = 0;

    COUNT_REQ(VLLINKEDLISTN);
    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;

    *nentries = 0;
    vldbentries->node = NULL;
    vllistptr = &vldbentries->node;

    /* List by volumeid */
    if (attributes->Mask & VLLIST_VOLUMEID) {
	blockindex =
	    FindByID(trans, attributes->volumeid, -1, &tentry, &errorcode);
	if (!blockindex) {
	    COUNT_ABO;
	    ubik_AbortTrans(trans);
	    return (errorcode ? errorcode : VL_NOENT);
	}

	vllist = (single_nvldbentry *) malloc(sizeof(single_nvldbentry));
	if (vllist == NULL) {
	    COUNT_ABO;
	    ubik_AbortTrans(trans);
	    return VL_NOMEM;
	}
	vlentry_to_nvldbentry(&tentry, &vllist->VldbEntry);
	vllist->next_vldb = NULL;

	*vllistptr = vllist;	/* Thread onto list */
	vllistptr = &vllist->next_vldb;
	(*nentries)++;
    }

    /* Search by server, partition, and flags */
    else {
	for (blockindex = NextEntry(trans, 0, &tentry, &count); blockindex;
	     blockindex = NextEntry(trans, blockindex, &tentry, &count)) {
	    match = 0;

	    if (++pollcount > 50) {
		IOMGR_Poll();
		pollcount = 0;
	    }

	    /* Does this volume exist on the desired server */
	    if (attributes->Mask & VLLIST_SERVER) {
		serverindex =
		    IpAddrToRelAddr(attributes->server,
				    (struct ubik_trans *)0);
		if (serverindex == -1)
		    continue;
		for (k = 0; k < NMAXNSERVERS; k++) {
		    if (tentry.serverNumber[k] == BADSERVERID)
			break;
		    if (tentry.serverNumber[k] == serverindex) {
			match = 1;
			break;
		    }
		}
		if (!match)
		    continue;
	    }

	    /* Does this volume exist on the desired partition */
	    if (attributes->Mask & VLLIST_PARTITION) {
		if (match) {
		    if (tentry.serverPartition[k] != attributes->partition)
			match = 0;
		} else {
		    for (k = 0; k < NMAXNSERVERS; k++) {
			if (tentry.serverNumber[k] == BADSERVERID)
			    break;
			if (tentry.serverPartition[k] ==
			    attributes->partition) {
			    match = 1;
			    break;
			}
		    }
		}
		if (!match)
		    continue;
	    }

	    /* Does this volume have the desired flags */
	    if (attributes->Mask & VLLIST_FLAG) {
		if (!(tentry.flags & attributes->flag))
		    continue;
	    }

	    vllist = (single_nvldbentry *) malloc(sizeof(single_nvldbentry));
	    if (vllist == NULL) {
		COUNT_ABO;
		ubik_AbortTrans(trans);
		return VL_NOMEM;
	    }
	    vlentry_to_nvldbentry(&tentry, &vllist->VldbEntry);
	    vllist->next_vldb = NULL;

	    *vllistptr = vllist;	/* Thread onto list */
	    vllistptr = &vllist->next_vldb;
	    (*nentries)++;
	    if (smallMem && (*nentries >= VLDBALLOCCOUNT)) {
		COUNT_ABO;
		ubik_AbortTrans(trans);
		return VL_SIZEEXCEEDED;
	    }
	}
    }
    *vllistptr = NULL;
    return (ubik_EndTrans(trans));
}

/* Get back vldb header statistics (allocs, frees, maxvolumeid, totalentries, etc) and dynamic statistics (number of requests and/or aborts per remote procedure call, etc) */
afs_int32
SVL_GetStats(rxcall, stats, vital_header)
     struct rx_call *rxcall;
     vldstats *stats;
     vital_vlheader *vital_header;
{
    register afs_int32 errorcode;
    struct ubik_trans *trans;

    COUNT_REQ(VLGETSTATS);
#ifdef	notdef
    /* Allow users to get statistics freely */
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))	/* Must be in 'UserList' to use */
	return VL_PERM;
#endif
    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;
    VLog(5, ("GetStats %s\n", rxinfo(rxcall)));
    memcpy((char *)vital_header, (char *)&cheader.vital_header,
	   sizeof(vital_vlheader));
    memcpy((char *)stats, (char *)&dynamic_statistics, sizeof(vldstats));
    return (ubik_EndTrans(trans));
}

/* Get the list of file server addresses from the VLDB.  Currently it's pretty
 * easy to do.  In the future, it might require a little bit of grunging
 * through the VLDB, but that's life.
 */
afs_int32
SVL_GetAddrs(rxcall, Handle, spare2, spare3, nentries, addrsp)
     struct rx_call *rxcall;
     afs_int32 Handle, spare2;
     struct VLCallBack *spare3;
     afs_int32 *nentries;
     bulkaddrs *addrsp;
{
    register afs_int32 errorcode;
    struct ubik_trans *trans;
    int nservers, i;
    afs_uint32 *taddrp;

    COUNT_REQ(VLGETADDRS);
    addrsp->bulkaddrs_len = *nentries = 0;
    addrsp->bulkaddrs_val = 0;
    memset(spare3, 0, sizeof(struct VLCallBack));

    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;

    VLog(5, ("GetAddrs\n"));
    addrsp->bulkaddrs_val = taddrp =
	(afs_uint32 *) malloc(sizeof(afs_int32) * (MAXSERVERID + 1));
    nservers = *nentries = addrsp->bulkaddrs_len = 0;

    if (!taddrp) {
	COUNT_ABO;
	ubik_AbortTrans(trans);
	return VL_NOMEM;
    }

    for (i = 0; i <= MAXSERVERID; i++) {
	if (*taddrp = ntohl(cheader.IpMappedAddr[i])) {
	    taddrp++;
	    nservers++;
	}
    }

    addrsp->bulkaddrs_len = *nentries = nservers;
    return (ubik_EndTrans(trans));
}

#define PADDR(addr) VLog(0,("%d.%d.%d.%d", (addr>>24)&0xff, (addr>>16)&0xff, (addr>>8) &0xff, addr&0xff));

afs_int32
SVL_RegisterAddrs(rxcall, uuidp, spare1, addrsp)
     struct rx_call *rxcall;
     afsUUID *uuidp;
     afs_int32 spare1;
     bulkaddrs *addrsp;
{
    afs_int32 code;
    struct ubik_trans *trans;
    int cnt, h, i, j, k, m, base, index;
    struct extentaddr *exp = 0, *tex;
    afsUUID tuuid;
    afs_uint32 addrs[VL_MAXIPADDRS_PERMH];
    afs_int32 fbase;
    int count, willChangeEntry, foundUuidEntry, willReplaceCnt;
    int WillReplaceEntry, WillChange[MAXSERVERID + 1], FoundUuid,
	ReplaceEntry;
    int srvidx, mhidx;

    COUNT_REQ(VLREGADDR);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	return (VL_PERM);
    if (code = Init_VLdbase(&trans, LOCKWRITE, this_op))
	return code;

    /* Eliminate duplicates from IP address list */
    for (k = 0, cnt = 0; k < addrsp->bulkaddrs_len; k++) {
	if (addrsp->bulkaddrs_val[k] == 0)
	    continue;
	for (m = 0; m < cnt; m++) {
	    if (addrs[m] == addrsp->bulkaddrs_val[k])
		break;
	}
	if (m == cnt) {
	    if (m == VL_MAXIPADDRS_PERMH) {
		VLog(0,
		     ("Number of addresses exceeds %d. Cannot register IP addr 0x%x in VLDB\n",
		      VL_MAXIPADDRS_PERMH, addrsp->bulkaddrs_val[k]));
	    } else {
		addrs[m] = addrsp->bulkaddrs_val[k];
		cnt++;
	    }
	}
    }
    if (cnt <= 0) {
	ubik_AbortTrans(trans);
	return VL_INDEXERANGE;
    }

    count = 0;
    willReplaceCnt = 0;
    foundUuidEntry = 0;
    /* For each server registered within the VLDB */
    for (srvidx = 0; srvidx <= MAXSERVERID; srvidx++) {
	willChangeEntry = 0;
	WillReplaceEntry = 1;
	if ((vl_HostAddress[srvidx] & 0xff000000) == 0xff000000) {
	    /* The server is registered as a multihomed */
	    base = (vl_HostAddress[srvidx] >> 16) & 0xff;
	    index = vl_HostAddress[srvidx] & 0x0000ffff;
	    if (base >= VL_MAX_ADDREXTBLKS) {
		VLog(0,
		     ("Internal error: Multihome extent base is too large. Base %d index %d\n",
		      base, index));
		continue;
	    }
	    if (index >= VL_MHSRV_PERBLK) {
		VLog(0,
		     ("Internal error: Multihome extent index is too large. Base %d index %d\n",
		      base, index));
		continue;
	    }
	    if (!ex_addr[base]) {
		VLog(0,
		     ("Internal error: Multihome extent does not exist. Base %d\n",
		      base));
		continue;
	    }

	    /* See if the addresses to register will change this server entry */
	    exp = &ex_addr[base][index];
	    tuuid = exp->ex_hostuuid;
	    afs_ntohuuid(&tuuid);
	    if (afs_uuid_equal(uuidp, &tuuid)) {
		foundUuidEntry = 1;
		FoundUuid = srvidx;
	    } else {
		for (mhidx = 0; mhidx < VL_MAXIPADDRS_PERMH; mhidx++) {
		    if (!exp->ex_addrs[mhidx])
			continue;
		    for (k = 0; k < cnt; k++) {
			if (ntohl(exp->ex_addrs[mhidx]) == addrs[k]) {
			    willChangeEntry = 1;
			    WillChange[count] = srvidx;
			    break;
			}
		    }
		    if (k >= cnt)
			WillReplaceEntry = 0;
		}
	    }
	} else {
	    /* The server is not registered as a multihomed.
	     * See if the addresses to register will replace this server entry.
	     */
	    for (k = 0; k < cnt; k++) {
		if (vl_HostAddress[srvidx] == addrs[k]) {
		    willChangeEntry = 1;
		    WillChange[count] = srvidx;
		    WillReplaceEntry = 1;
		    break;
		}
	    }
	}
	if (willChangeEntry) {
	    if (WillReplaceEntry) {
		willReplaceCnt++;
		ReplaceEntry = srvidx;
	    }
	    count++;
	}
    }

    /* If we found the uuid in the VLDB and if we are replacing another
     * entire entry, then complain and fail. Also, if we did not find
     * the uuid in the VLDB and the IP addresses being registered was
     * found in more than one other entry, then we don't know which one
     * to replace and will complain and fail.
     */
    if ((foundUuidEntry && (willReplaceCnt > 0))
	|| (!foundUuidEntry && (count > 1))) {
	VLog(0,
	     ("The following fileserver is being registered in the VLDB:\n"));
	VLog(0, ("      ["));
	for (k = 0; k < cnt; k++) {
	    if (k > 0)
		VLog(0,(" "));
	    PADDR(addrs[k]);
	}
	VLog(0,("]\n"));

	if (foundUuidEntry) {
	    VLog(0,
		 ("   It would have replaced the existing VLDB server entry:\n"));
	    VLog(0, ("      entry %d: [", FoundUuid));
	    base = (vl_HostAddress[FoundUuid] >> 16) & 0xff;
	    index = vl_HostAddress[FoundUuid] & 0x0000ffff;
	    exp = &ex_addr[base][index];
	    for (mhidx = 0; mhidx < VL_MAXIPADDRS_PERMH; mhidx++) {
		if (!exp->ex_addrs[mhidx])
		    continue;
		if (mhidx > 0)
		    VLog(0,(" "));
		PADDR(ntohl(exp->ex_addrs[mhidx]));
	    }
	    VLog(0, ("]\n"));
	}

	if (count == 1)
	    VLog(0, ("   Yet another VLDB server entry exists:\n"));
	else
	    VLog(0, ("   Yet other VLDB server entries exist:\n"));
	for (j = 0; j < count; j++) {
	    srvidx = WillChange[j];
	    VLog(0, ("      entry %d: ", srvidx));
	    if ((vl_HostAddress[srvidx] & 0xff000000) == 0xff000000) {
		VLog(0, ("["));
		base = (vl_HostAddress[srvidx] >> 16) & 0xff;
		index = vl_HostAddress[srvidx] & 0x0000ffff;
		exp = &ex_addr[base][index];
		for (mhidx = 0; mhidx < VL_MAXIPADDRS_PERMH; mhidx++) {
		    if (!exp->ex_addrs[mhidx])
			continue;
		    if (mhidx > 0)
			VLog(0, (" "));
		    PADDR(ntohl(exp->ex_addrs[mhidx]));
		}
		VLog(0, ("]"));
	    } else {
		PADDR(vl_HostAddress[srvidx]);
	    }
	    VLog(0, ("\n"));
	}

	if (count == 1)
	    VLog(0, ("   You must 'vos changeaddr' this other server entry\n"));
	else
	    VLog(0, 
		("   You must 'vos changeaddr' these other server entries\n"));
	if (foundUuidEntry)
	    VLog(0, 
		("   and/or remove the sysid file from the registering fileserver\n"));
	VLog(0, ("   before the fileserver can be registered in the VLDB.\n"));

	ubik_AbortTrans(trans);
	return VL_MULTIPADDR;
    }

    /* Passed the checks. Now find and update the existing mh entry, or create
     * a new mh entry.
     */
    if (foundUuidEntry) {
	/* Found the entry with same uuid. See if we need to change it */
	int change = 0;

	fbase = (vl_HostAddress[FoundUuid] >> 16) & 0xff;
	index = vl_HostAddress[FoundUuid] & 0x0000ffff;
	exp = &ex_addr[fbase][index];

	/* Determine if the entry has changed */
	for (k = 0; ((k < cnt) && !change); k++) {
	    if (ntohl(exp->ex_addrs[k]) != addrs[k])
		change = 1;
	}
	for (; ((k < VL_MAXIPADDRS_PERMH) && !change); k++) {
	    if (exp->ex_addrs[k] != 0)
		change = 1;
	}
	if (!change) {
	    return (ubik_EndTrans(trans));
	}
    }

    VLog(0, ("The following fileserver is being registered in the VLDB:\n"));
    VLog(0, ("      ["));
    for (k = 0; k < cnt; k++) {
	if (k > 0)
	    VLog(0, (" "));
	PADDR(addrs[k]);
    }
    VLog(0, ("]\n"));

    if (foundUuidEntry) {
	VLog(0, 
	    ("   It will replace the following existing entry in the VLDB (same uuid):\n"));
	VLog(0, ("      entry %d: [", FoundUuid));
	for (k = 0; k < VL_MAXIPADDRS_PERMH; k++) {
	    if (exp->ex_addrs[k] == 0)
		continue;
	    if (k > 0)
		VLog(0, (" "));
	    PADDR(ntohl(exp->ex_addrs[k]));
	}
	VLog(0, ("]\n"));
    } else if (willReplaceCnt || (count == 1)) {
	/* If we are not replacing an entry and there is only one entry to change,
	 * then we will replace that entry.
	 */
	if (!willReplaceCnt) {
	    ReplaceEntry = WillChange[0];
	    willReplaceCnt++;
	}

	/* Have an entry that needs to be replaced */
	if ((vl_HostAddress[ReplaceEntry] & 0xff000000) == 0xff000000) {
	    fbase = (vl_HostAddress[ReplaceEntry] >> 16) & 0xff;
	    index = vl_HostAddress[ReplaceEntry] & 0x0000ffff;
	    exp = &ex_addr[fbase][index];

	    VLog(0, 
		("   It will replace the following existing entry in the VLDB (new uuid):\n"));
	    VLog(0, ("      entry %d: [", ReplaceEntry));
	    for (k = 0; k < VL_MAXIPADDRS_PERMH; k++) {
		if (exp->ex_addrs[k] == 0)
		    continue;
		if (k > 0)
		    VLog(0, (" "));
		PADDR(ntohl(exp->ex_addrs[k]));
	    }
	    VLog(0, ("]\n"));
	} else {
	    /* Not a mh entry. So we have to create a new mh entry and 
	     * put it on the ReplaceEntry slot of the vl_HostAddress array.
	     */
	    VLog(0, ("   It will replace existing entry %d, ", ReplaceEntry));
	    PADDR(vl_HostAddress[ReplaceEntry]);
	    VLog(0,(", in the VLDB (new uuid):\n"));

	    code =
		FindExtentBlock(trans, uuidp, 1, ReplaceEntry, &exp, &fbase);
	    if (code || !exp) {
		ubik_AbortTrans(trans);
		return (code ? code : VL_IO);
	    }
	}
    } else {
	/* There is no entry for this server, must create a new mh entry as
	 * well as use a new slot of the vl_HostAddress array.
	 */
	VLog(0, ("   It will create a new entry in the VLDB.\n"));
	code = FindExtentBlock(trans, uuidp, 1, -1, &exp, &fbase);
	if (code || !exp) {
	    ubik_AbortTrans(trans);
	    return (code ? code : VL_IO);
	}
    }

    /* Now we have a mh entry to fill in. Update the uuid, bump the
     * uniquifier, and fill in its IP addresses.
     */
    tuuid = *uuidp;
    afs_htonuuid(&tuuid);
    exp->ex_hostuuid = tuuid;
    exp->ex_uniquifier = htonl(ntohl(exp->ex_uniquifier) + 1);
    for (k = 0; k < cnt; k++) {
	exp->ex_addrs[k] = htonl(addrs[k]);
    }
    for (; k < VL_MAXIPADDRS_PERMH; k++) {
	exp->ex_addrs[k] = 0;
    }

    /* Write the new mh entry out */
    if (vlwrite
	(trans,
	 DOFFSET(ntohl(ex_addr[0]->ex_contaddrs[fbase]),
		 (char *)ex_addr[fbase], (char *)exp), (char *)exp,
	 sizeof(*exp))) {
	ubik_AbortTrans(trans);
	return VL_IO;
    }

    /* Remove any common addresses from other mh entres. We know these entries 
     * are being changed and not replaced so they are mh entries.
     */
    m = 0;
    for (i = 0; i < count; i++) {
	afs_int32 doff;

	/* Skip the entry we replaced */
	if (willReplaceCnt && (WillChange[i] == ReplaceEntry))
	    continue;

	base = (vl_HostAddress[WillChange[i]] >> 16) & 0xff;
	index = vl_HostAddress[WillChange[i]] & 0x0000ffff;
	tex = &ex_addr[fbase][index];

	if (++m == 1)
	    VLog(0, 
		("   The following existing entries in the VLDB will be updated:\n"));

	VLog(0, ("      entry %d: [", WillChange[i]));
	for (h = j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
	    if (tex->ex_addrs[j]) {
		if (j > 0)
		    printf(" ");
		PADDR(ntohl(tex->ex_addrs[j]));
	    }

	    for (k = 0; k < cnt; k++) {
		if (ntohl(tex->ex_addrs[j]) == addrs[k])
		    break;
	    }
	    if (k >= cnt) {
		/* Not found, so we keep it */
		tex->ex_addrs[h] = tex->ex_addrs[j];
		h++;
	    }
	}
	for (j = h; j < VL_MAXIPADDRS_PERMH; j++) {
	    tex->ex_addrs[j] = 0;	/* zero rest of mh entry */
	}
	VLog(0, ("]\n"));

	/* Write out the modified mh entry */
	tex->ex_uniquifier = htonl(ntohl(tex->ex_uniquifier) + 1);
	doff =
	    DOFFSET(ntohl(ex_addr[0]->ex_contaddrs[base]),
		    (char *)ex_addr[base], (char *)tex);
	if (vlwrite(trans, doff, (char *)tex, sizeof(*tex))) {
	    ubik_AbortTrans(trans);
	    return VL_IO;
	}
    }

    return (ubik_EndTrans(trans));
}

afs_int32
SVL_GetAddrsU(rxcall, attributes, uuidpo, uniquifier, nentries, addrsp)
     struct rx_call *rxcall;
     struct ListAddrByAttributes *attributes;
     afsUUID *uuidpo;
     afs_int32 *uniquifier, *nentries;
     bulkaddrs *addrsp;
{
    register afs_int32 errorcode, index = -1, offset;
    struct ubik_trans *trans;
    int nservers, i, j, base = 0;
    struct extentaddr *exp = 0;
    afsUUID tuuid;
    afs_uint32 *taddrp, taddr;

    COUNT_REQ(VLGETADDRSU);
    addrsp->bulkaddrs_len = *nentries = 0;
    addrsp->bulkaddrs_val = 0;
    VLog(5, ("GetAddrsU %s\n", rxinfo(rxcall)));
    if (errorcode = Init_VLdbase(&trans, LOCKREAD, this_op))
	return errorcode;

    if (attributes->Mask & VLADDR_IPADDR) {
	if (attributes->Mask & (VLADDR_INDEX | VLADDR_UUID)) {
	    ubik_AbortTrans(trans);
	    return VL_BADMASK;
	}
	for (base = 0; base < VL_MAX_ADDREXTBLKS; base++) {
	    if (!ex_addr[base])
		break;
	    for (i = 1; i < VL_MHSRV_PERBLK; i++) {
		exp = &ex_addr[base][i];
		tuuid = exp->ex_hostuuid;
		afs_ntohuuid(&tuuid);
		if (afs_uuid_is_nil(&tuuid))
		    continue;
		for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
		    if (exp->ex_addrs[j]
			&& (ntohl(exp->ex_addrs[j]) == attributes->ipaddr)) {
			break;
		    }
		}
		if (j < VL_MAXIPADDRS_PERMH)
		    break;
	    }
	    if (i < VL_MHSRV_PERBLK)
		break;
	}
	if (base >= VL_MAX_ADDREXTBLKS) {
	    ubik_AbortTrans(trans);
	    return VL_NOENT;
	}
    } else if (attributes->Mask & VLADDR_INDEX) {
	if (attributes->Mask & (VLADDR_IPADDR | VLADDR_UUID)) {
	    ubik_AbortTrans(trans);
	    return VL_BADMASK;
	}
	index = attributes->index;
	if (index < 1 || index >= (VL_MAX_ADDREXTBLKS * VL_MHSRV_PERBLK)) {
	    ubik_AbortTrans(trans);
	    return VL_INDEXERANGE;
	}
	base = index / VL_MHSRV_PERBLK;
	offset = index % VL_MHSRV_PERBLK;
	if (offset == 0) {
	    ubik_AbortTrans(trans);
	    return VL_NOENT;
	}
	if (!ex_addr[base]) {
	    ubik_AbortTrans(trans);
	    return VL_INDEXERANGE;
	}
	exp = &ex_addr[base][offset];
    } else if (attributes->Mask & VLADDR_UUID) {
	if (attributes->Mask & (VLADDR_IPADDR | VLADDR_INDEX)) {
	    ubik_AbortTrans(trans);
	    return VL_BADMASK;
	}
	if (!ex_addr[0]) {	/* mh servers probably aren't setup on this vldb */
	    ubik_AbortTrans(trans);
	    return VL_NOENT;
	}
	if (errorcode =
	    FindExtentBlock(trans, &attributes->uuid, 0, -1, &exp, &base)) {
	    ubik_AbortTrans(trans);
	    return errorcode;
	}
    } else {
	ubik_AbortTrans(trans);
	return VL_BADMASK;
    }

    if (exp == NULL) {
	ubik_AbortTrans(trans);
	return VL_NOENT;
    }
    addrsp->bulkaddrs_val = taddrp =
	(afs_uint32 *) malloc(sizeof(afs_int32) * (MAXSERVERID + 1));
    nservers = *nentries = addrsp->bulkaddrs_len = 0;
    if (!taddrp) {
	COUNT_ABO;
	ubik_AbortTrans(trans);
	return VL_NOMEM;
    }
    tuuid = exp->ex_hostuuid;
    afs_ntohuuid(&tuuid);
    if (afs_uuid_is_nil(&tuuid)) {
	ubik_AbortTrans(trans);
	return VL_NOENT;
    }
    if (uuidpo)
	*uuidpo = tuuid;
    if (uniquifier)
	*uniquifier = ntohl(exp->ex_uniquifier);
    for (i = 0; i < VL_MAXIPADDRS_PERMH; i++) {
	if (exp->ex_addrs[i]) {
	    taddr = ntohl(exp->ex_addrs[i]);
	    /* Weed out duplicates */
	    for (j = 0; j < nservers; j++) {
		if (taddrp[j] == taddr)
		    break;
	    }
	    if ((j == nservers) && (j <= MAXSERVERID)) {
		taddrp[nservers] = taddr;
		nservers++;
	    }
	}
    }
    addrsp->bulkaddrs_len = *nentries = nservers;
    return (ubik_EndTrans(trans));
}

/* ============> End of Exported vldb RPC functions <============= */


/* Routine that copies the given vldb entry to the output buffer, vldbentries. */
static int
put_attributeentry(Vldbentry, VldbentryFirst, VldbentryLast, vldbentries,
		   entry, nentries, alloccnt)
     struct vldbentry **Vldbentry, **VldbentryFirst, **VldbentryLast;
     bulkentries *vldbentries;
     struct nvlentry *entry;
     afs_int32 *nentries, *alloccnt;
{
    vldbentry *reall;
    afs_int32 allo;

    if (*Vldbentry == *VldbentryLast) {
	if (smallMem)
	    return VL_SIZEEXCEEDED;	/* no growing if smallMem defined */

	/* Allocate another set of memory; each time allocate twice as
	 * many blocks as the last time. When we reach VLDBALLOCLIMIT, 
	 * then grow in increments of VLDBALLOCINCR.
	 */
	allo = (*alloccnt > VLDBALLOCLIMIT) ? VLDBALLOCINCR : *alloccnt;
	reall =
	    (vldbentry *) realloc(*VldbentryFirst,
				  (*alloccnt + allo) * sizeof(vldbentry));
	if (reall == NULL)
	    return VL_NOMEM;

	*VldbentryFirst = vldbentries->bulkentries_val = reall;
	*Vldbentry = *VldbentryFirst + *alloccnt;
	*VldbentryLast = *Vldbentry + allo;
	*alloccnt += allo;
    }
    vlentry_to_vldbentry(entry, *Vldbentry);
    (*Vldbentry)++;
    (*nentries)++;
    vldbentries->bulkentries_len++;
    return 0;
}

static int
put_nattributeentry(Vldbentry, VldbentryFirst, VldbentryLast, vldbentries,
		    entry, matchtype, matchindex, nentries, alloccnt)
     struct nvldbentry **Vldbentry, **VldbentryFirst, **VldbentryLast;
     nbulkentries *vldbentries;
     struct nvlentry *entry;
     afs_int32 matchtype, matchindex, *nentries, *alloccnt;
{
    nvldbentry *reall;
    afs_int32 allo;

    if (*Vldbentry == *VldbentryLast) {
	if (smallMem)
	    return VL_SIZEEXCEEDED;	/* no growing if smallMem defined */

	/* Allocate another set of memory; each time allocate twice as
	 * many blocks as the last time. When we reach VLDBALLOCLIMIT, 
	 * then grow in increments of VLDBALLOCINCR.
	 */
	allo = (*alloccnt > VLDBALLOCLIMIT) ? VLDBALLOCINCR : *alloccnt;
	reall =
	    (nvldbentry *) realloc(*VldbentryFirst,
				   (*alloccnt + allo) * sizeof(nvldbentry));
	if (reall == NULL)
	    return VL_NOMEM;

	*VldbentryFirst = vldbentries->nbulkentries_val = reall;
	*Vldbentry = *VldbentryFirst + *alloccnt;
	*VldbentryLast = *Vldbentry + allo;
	*alloccnt += allo;
    }
    vlentry_to_nvldbentry(entry, *Vldbentry);
    (*Vldbentry)->matchindex = (matchtype << 16) + matchindex;
    (*Vldbentry)++;
    (*nentries)++;
    vldbentries->nbulkentries_len++;
    return 0;
}


/* Common code to actually remove a vldb entry from the database. */
static int
RemoveEntry(trans, entryptr, tentry)
     struct ubik_trans *trans;
     afs_int32 entryptr;
     struct nvlentry *tentry;
{
    register int errorcode;

    if (errorcode = UnthreadVLentry(trans, entryptr, tentry))
	return errorcode;
    if (errorcode = FreeBlock(trans, entryptr))
	return errorcode;
    return 0;
}

static void
ReleaseEntry(tentry, releasetype)
     struct nvlentry *tentry;
     afs_int32 releasetype;
{
    if (releasetype & LOCKREL_TIMESTAMP)
	tentry->LockTimestamp = 0;
    if (releasetype & LOCKREL_OPCODE)
	tentry->flags &= ~VLOP_ALLOPERS;
    if (releasetype & LOCKREL_AFSID)
	tentry->LockAfsId = 0;
}


/* Verify that the incoming vldb entry is valid; multi type of error codes are returned. */
static int
check_vldbentry(aentry)
     struct vldbentry *aentry;
{
    afs_int32 i;

    if (InvalidVolname(aentry->name))
	return VL_BADNAME;
    if (aentry->nServers <= 0 || aentry->nServers > OMAXNSERVERS)
	return VL_BADSERVER;
    for (i = 0; i < aentry->nServers; i++) {
/*	if (aentry->serverNumber[i] < 0 || aentry->serverNumber[i] > MAXSERVERID)
	    return VL_BADSERVER;	*/
	if (aentry->serverPartition[i] < 0
	    || aentry->serverPartition[i] > MAXPARTITIONID)
	    return VL_BADPARTITION;
	if (aentry->serverFlags[i] < 0
	    || aentry->serverFlags[i] > MAXSERVERFLAG)
	    return VL_BADSERVERFLAG;
    }
    return 0;
}

static int
check_nvldbentry(aentry)
     struct nvldbentry *aentry;
{
    afs_int32 i;

    if (InvalidVolname(aentry->name))
	return VL_BADNAME;
    if (aentry->nServers <= 0 || aentry->nServers > NMAXNSERVERS)
	return VL_BADSERVER;
    for (i = 0; i < aentry->nServers; i++) {
/*	if (aentry->serverNumber[i] < 0 || aentry->serverNumber[i] > MAXSERVERID)
	    return VL_BADSERVER;	*/
	if (aentry->serverPartition[i] < 0
	    || aentry->serverPartition[i] > MAXPARTITIONID)
	    return VL_BADPARTITION;
	if (aentry->serverFlags[i] < 0
	    || aentry->serverFlags[i] > MAXSERVERFLAG)
	    return VL_BADSERVERFLAG;
    }
    return 0;
}


/* Convert from the external vldb entry representation to its internal
   (more compact) form.  This call should not change the hash chains! */
static int
vldbentry_to_vlentry(atrans, VldbEntry, VlEntry)
     struct ubik_trans *atrans;
     struct vldbentry *VldbEntry;
     struct nvlentry *VlEntry;
{
    int i, serverindex;

    if (strcmp(VlEntry->name, VldbEntry->name))
	strncpy(VlEntry->name, VldbEntry->name, sizeof(VlEntry->name));
    for (i = 0; i < VldbEntry->nServers; i++) {
	serverindex = IpAddrToRelAddr(VldbEntry->serverNumber[i], atrans);
	if (serverindex == -1)
	    return VL_BADSERVER;
	VlEntry->serverNumber[i] = serverindex;
	VlEntry->serverPartition[i] = VldbEntry->serverPartition[i];
	VlEntry->serverFlags[i] = VldbEntry->serverFlags[i];
    }
    for (; i < OMAXNSERVERS; i++)
	VlEntry->serverNumber[i] = VlEntry->serverPartition[i] =
	    VlEntry->serverFlags[i] = BADSERVERID;
    for (i = 0; i < MAXTYPES; i++)
	VlEntry->volumeId[i] = VldbEntry->volumeId[i];
    VlEntry->cloneId = VldbEntry->cloneId;
    VlEntry->flags = VldbEntry->flags;
    return 0;
}

static int
nvldbentry_to_vlentry(atrans, VldbEntry, VlEntry)
     struct ubik_trans *atrans;
     struct nvldbentry *VldbEntry;
     struct nvlentry *VlEntry;
{
    int i, serverindex;

    if (strcmp(VlEntry->name, VldbEntry->name))
	strncpy(VlEntry->name, VldbEntry->name, sizeof(VlEntry->name));
    for (i = 0; i < VldbEntry->nServers; i++) {
	serverindex = IpAddrToRelAddr(VldbEntry->serverNumber[i], atrans);
	if (serverindex == -1)
	    return VL_BADSERVER;
	VlEntry->serverNumber[i] = serverindex;
	VlEntry->serverPartition[i] = VldbEntry->serverPartition[i];
	VlEntry->serverFlags[i] = VldbEntry->serverFlags[i];
    }
    for (; i < NMAXNSERVERS; i++)
	VlEntry->serverNumber[i] = VlEntry->serverPartition[i] =
	    VlEntry->serverFlags[i] = BADSERVERID;
    for (i = 0; i < MAXTYPES; i++)
	VlEntry->volumeId[i] = VldbEntry->volumeId[i];
    VlEntry->cloneId = VldbEntry->cloneId;
    VlEntry->flags = VldbEntry->flags;
    return 0;
}


/* Update the vldb entry with the new fields as indicated by the value of the Mask entry in the updateentry structure. All necessary validation checks are performed. */
static
get_vldbupdateentry(trans, blockindex, updateentry, VlEntry)
     struct ubik_trans *trans;
     afs_int32 blockindex;
     struct VldbUpdateEntry *updateentry;
     struct nvlentry *VlEntry;
{
    int i, j, errorcode, serverindex;

    if (updateentry->Mask & VLUPDATE_VOLUMENAME) {
	if (InvalidVolname(updateentry->name))
	    return VL_BADNAME;
	if (errorcode = UnhashVolname(trans, blockindex, VlEntry))
	    return errorcode;
	strncpy(VlEntry->name, updateentry->name, sizeof(VlEntry->name));
	HashVolname(trans, blockindex, VlEntry);
    }

    if (updateentry->Mask & VLUPDATE_VOLNAMEHASH) {
	if (errorcode = UnhashVolname(trans, blockindex, VlEntry)) {
	    if (errorcode != VL_NOENT)
		return errorcode;
	}
	HashVolname(trans, blockindex, VlEntry);
    }

    if (updateentry->Mask & VLUPDATE_FLAGS) {
	VlEntry->flags = updateentry->flags;
    }
    if (updateentry->Mask & VLUPDATE_CLONEID) {
	VlEntry->cloneId = updateentry->cloneId;
    }
    if (updateentry->Mask & VLUPDATE_RWID) {
	if (errorcode = UnhashVolid(trans, RWVOL, blockindex, VlEntry)) {
	    if (errorcode != VL_NOENT)
		return errorcode;
	}
	VlEntry->volumeId[RWVOL] = updateentry->spares3;	/* rw id */
	if (errorcode = HashVolid(trans, RWVOL, blockindex, VlEntry))
	    return errorcode;
    }
    if (updateentry->Mask & VLUPDATE_READONLYID) {
	if (errorcode = UnhashVolid(trans, ROVOL, blockindex, VlEntry)) {
	    if (errorcode != VL_NOENT)
		return errorcode;
	}
	VlEntry->volumeId[ROVOL] = updateentry->ReadOnlyId;
	if (errorcode = HashVolid(trans, ROVOL, blockindex, VlEntry))
	    return errorcode;
    }
    if (updateentry->Mask & VLUPDATE_BACKUPID) {
	if (errorcode = UnhashVolid(trans, BACKVOL, blockindex, VlEntry)) {
	    if (errorcode != VL_NOENT)
		return errorcode;
	}
	VlEntry->volumeId[BACKVOL] = updateentry->BackupId;
	if (errorcode = HashVolid(trans, BACKVOL, blockindex, VlEntry))
	    return errorcode;
    }
    if (updateentry->Mask & VLUPDATE_REPSITES) {
	if (updateentry->nModifiedRepsites <= 0
	    || updateentry->nModifiedRepsites > OMAXNSERVERS)
	    return VL_BADSERVER;
	for (i = 0; i < updateentry->nModifiedRepsites; i++) {
/*	    if (updateentry->RepsitesTargetServer[i] < 0 || updateentry->RepsitesTargetServer[i] > MAXSERVERID)
		return VL_BADSERVER;	*/
	    if (updateentry->RepsitesTargetPart[i] < 0
		|| updateentry->RepsitesTargetPart[i] > MAXPARTITIONID)
		return VL_BADPARTITION;
	    if (updateentry->RepsitesMask[i] & VLUPDATE_REPS_DELETE) {
		if ((j =
		     repsite_exists(VlEntry,
				    IpAddrToRelAddr(updateentry->
						    RepsitesTargetServer[i],
						    trans),
				    updateentry->RepsitesTargetPart[i])) !=
		    -1)
		    repsite_compress(VlEntry, j);
		else
		    return VL_NOREPSERVER;
	    }
	    if (updateentry->RepsitesMask[i] & VLUPDATE_REPS_ADD) {
/*		if (updateentry->RepsitesNewServer[i] < 0 || updateentry->RepsitesNewServer[i] > MAXSERVERID)
		    return VL_BADSERVER;		*/
		if (updateentry->RepsitesNewPart[i] < 0
		    || updateentry->RepsitesNewPart[i] > MAXPARTITIONID)
		    return VL_BADPARTITION;
		if (repsite_exists
		    (VlEntry,
		     IpAddrToRelAddr(updateentry->RepsitesNewServer[i],
				     trans),
		     updateentry->RepsitesNewPart[i]) != -1)
		    return VL_DUPREPSERVER;
		for (j = 0;
		     VlEntry->serverNumber[j] != BADSERVERID
		     && j < OMAXNSERVERS; j++);
		if (j >= OMAXNSERVERS)
		    return VL_REPSFULL;
		if ((serverindex =
		     IpAddrToRelAddr(updateentry->RepsitesNewServer[i],
				     trans)) == -1)
		    return VL_BADSERVER;
		VlEntry->serverNumber[j] = serverindex;
		VlEntry->serverPartition[j] = updateentry->RepsitesNewPart[i];
		if (updateentry->RepsitesNewFlags[i] < 0
		    || updateentry->RepsitesNewFlags[i] > MAXSERVERFLAG)
		    return VL_BADSERVERFLAG;
		VlEntry->serverFlags[j] = updateentry->RepsitesNewFlags[i];
	    }
	    if (updateentry->RepsitesMask[i] & VLUPDATE_REPS_MODSERV) {
/*n		if (updateentry->RepsitesNewServer[i] < 0 || updateentry->RepsitesNewServer[i] > MAXSERVERID)
		    return VL_BADSERVER;	    */
		if ((j =
		     repsite_exists(VlEntry,
				    IpAddrToRelAddr(updateentry->
						    RepsitesTargetServer[i],
						    trans),
				    updateentry->RepsitesTargetPart[i])) !=
		    -1) {
		    VlEntry->serverNumber[j] =
			IpAddrToRelAddr(updateentry->RepsitesNewServer[i],
					trans);
		} else
		    return VL_NOREPSERVER;
	    }
	    if (updateentry->RepsitesMask[i] & VLUPDATE_REPS_MODPART) {
		if (updateentry->RepsitesNewPart[i] < 0
		    || updateentry->RepsitesNewPart[i] > MAXPARTITIONID)
		    return VL_BADPARTITION;
		if ((j =
		     repsite_exists(VlEntry,
				    IpAddrToRelAddr(updateentry->
						    RepsitesTargetServer[i],
						    trans),
				    updateentry->RepsitesTargetPart[i])) !=
		    -1)
		    VlEntry->serverPartition[j] =
			updateentry->RepsitesNewPart[i];
		else
		    return VL_NOREPSERVER;
	    }
	    if (updateentry->RepsitesMask[i] & VLUPDATE_REPS_MODFLAG) {
		if ((j =
		     repsite_exists(VlEntry,
				    IpAddrToRelAddr(updateentry->
						    RepsitesTargetServer[i],
						    trans),
				    updateentry->RepsitesTargetPart[i])) !=
		    -1) {
		    if (updateentry->RepsitesNewFlags[i] < 0
			|| updateentry->RepsitesNewFlags[i] > MAXSERVERFLAG)
			return VL_BADSERVERFLAG;
		    VlEntry->serverFlags[j] =
			updateentry->RepsitesNewFlags[i];
		} else
		    return VL_NOREPSERVER;
	    }
	}
    }
    return 0;
}


/* Check if the specified [server,partition] entry is found in the vldb entry's repsite table; it's offset in the table is returned, if it's present there. */
static int
repsite_exists(VlEntry, server, partition)
     struct nvlentry *VlEntry;
     int server, partition;
{
    int i;

    for (i = 0; VlEntry->serverNumber[i] != BADSERVERID && i < OMAXNSERVERS;
	 i++) {
	if ((VlEntry->serverNumber[i] == server)
	    && (VlEntry->serverPartition[i] == partition))
	    return i;
    }
    return -1;
}



/* Repsite table compression: used when deleting a repsite entry so that all active repsite entries are on the top of the table. */
static void
repsite_compress(VlEntry, offset)
     struct nvlentry *VlEntry;
     int offset;
{
    int repsite_offset = offset;
    for (;
	 VlEntry->serverNumber[repsite_offset] != BADSERVERID
	 && repsite_offset < OMAXNSERVERS - 1; repsite_offset++) {
	VlEntry->serverNumber[repsite_offset] =
	    VlEntry->serverNumber[repsite_offset + 1];
	VlEntry->serverPartition[repsite_offset] =
	    VlEntry->serverPartition[repsite_offset + 1];
	VlEntry->serverFlags[repsite_offset] =
	    VlEntry->serverFlags[repsite_offset + 1];
    }
    VlEntry->serverNumber[repsite_offset] = BADSERVERID;
}


/* Convert from the internal (compacted) vldb entry to the external representation used by the interface. */
static void
vlentry_to_vldbentry(VlEntry, VldbEntry)
     struct nvlentry *VlEntry;
     struct vldbentry *VldbEntry;
{
    int i, j;

    memset(VldbEntry, 0, sizeof(struct vldbentry));
    strncpy(VldbEntry->name, VlEntry->name, sizeof(VldbEntry->name));
    for (i = 0; i < OMAXNSERVERS; i++) {
	if (VlEntry->serverNumber[i] == BADSERVERID)
	    break;
	if ((vl_HostAddress[j = VlEntry->serverNumber[i]] & 0xff000000) ==
	    0xff000000) {
	    struct extentaddr *exp;
	    int base, index;

	    base = (vl_HostAddress[j] >> 16) & 0xff;
	    index = vl_HostAddress[j] & 0x0000ffff;
	    exp = &ex_addr[base][index];
	    /* For now return the first ip address back */
	    for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
		if (exp->ex_addrs[j]) {
		    VldbEntry->serverNumber[i] = ntohl(exp->ex_addrs[j]);
		    break;
		}
	    }
	} else
	    VldbEntry->serverNumber[i] =
		vl_HostAddress[VlEntry->serverNumber[i]];
	VldbEntry->serverPartition[i] = VlEntry->serverPartition[i];
	VldbEntry->serverFlags[i] = VlEntry->serverFlags[i];
    }
    VldbEntry->nServers = i;
    for (i = 0; i < MAXTYPES; i++)
	VldbEntry->volumeId[i] = VlEntry->volumeId[i];
    VldbEntry->cloneId = VlEntry->cloneId;
    VldbEntry->flags = VlEntry->flags;
}


/* Convert from the internal (compacted) vldb entry to the external representation used by the interface. */
static void
vlentry_to_nvldbentry(VlEntry, VldbEntry)
     struct nvlentry *VlEntry;
     struct nvldbentry *VldbEntry;
{
    int i, j;

    memset(VldbEntry, 0, sizeof(struct vldbentry));
    strncpy(VldbEntry->name, VlEntry->name, sizeof(VldbEntry->name));
    for (i = 0; i < NMAXNSERVERS; i++) {
	if (VlEntry->serverNumber[i] == BADSERVERID)
	    break;
	if ((vl_HostAddress[j = VlEntry->serverNumber[i]] & 0xff000000) ==
	    0xff000000) {
	    struct extentaddr *exp;
	    int base, index;

	    base = (vl_HostAddress[j] >> 16) & 0xff;
	    index = vl_HostAddress[j] & 0x0000ffff;
	    exp = &ex_addr[base][index];
	    /* For now return the first ip address back */
	    for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
		if (exp->ex_addrs[j]) {
		    VldbEntry->serverNumber[i] = ntohl(exp->ex_addrs[j]);
		    break;
		}
	    }
	} else
	    VldbEntry->serverNumber[i] =
		vl_HostAddress[VlEntry->serverNumber[i]];
	VldbEntry->serverPartition[i] = VlEntry->serverPartition[i];
	VldbEntry->serverFlags[i] = VlEntry->serverFlags[i];
    }
    VldbEntry->nServers = i;
    for (i = 0; i < MAXTYPES; i++)
	VldbEntry->volumeId[i] = VlEntry->volumeId[i];
    VldbEntry->cloneId = VlEntry->cloneId;
    VldbEntry->flags = VlEntry->flags;
}

static void
vlentry_to_uvldbentry(VlEntry, VldbEntry)
     struct nvlentry *VlEntry;
     struct uvldbentry *VldbEntry;
{
    int i, j;

    memset(VldbEntry, 0, sizeof(struct vldbentry));
    strncpy(VldbEntry->name, VlEntry->name, sizeof(VldbEntry->name));
    for (i = 0; i < NMAXNSERVERS; i++) {
	if (VlEntry->serverNumber[i] == BADSERVERID)
	    break;
	VldbEntry->serverFlags[i] = VlEntry->serverFlags[i];
	VldbEntry->serverUnique[i] = 0;
	if ((vl_HostAddress[j = VlEntry->serverNumber[i]] & 0xff000000) ==
	    0xff000000) {
	    struct extentaddr *exp;
	    int base, index;
	    afsUUID tuuid;

	    base = (vl_HostAddress[j] >> 16) & 0xff;
	    index = vl_HostAddress[j] & 0x0000ffff;
	    exp = &ex_addr[base][index];
	    tuuid = exp->ex_hostuuid;
	    afs_ntohuuid(&tuuid);
	    VldbEntry->serverFlags[i] |= VLSERVER_FLAG_UUID;
	    VldbEntry->serverNumber[i] = tuuid;
	    VldbEntry->serverUnique[i] = ntohl(exp->ex_uniquifier);
	} else {
	    VldbEntry->serverNumber[i].time_low =
		vl_HostAddress[VlEntry->serverNumber[i]];
	}
	VldbEntry->serverPartition[i] = VlEntry->serverPartition[i];

    }
    VldbEntry->nServers = i;
    for (i = 0; i < MAXTYPES; i++)
	VldbEntry->volumeId[i] = VlEntry->volumeId[i];
    VldbEntry->cloneId = VlEntry->cloneId;
    VldbEntry->flags = VlEntry->flags;
}

#define LEGALCHARS ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"


/* Verify that the volname is a valid volume name. */
static int
InvalidVolname(volname)
     char *volname;
{
    char *map;
    int slen;

    map = LEGALCHARS;
    slen = strlen(volname);
    if (slen >= VL_MAXNAMELEN)
	return 1;
    return (slen != strspn(volname, map));
}


/* Verify that the given volume type is valid. */
static int
InvalidVoltype(voltype)
     afs_int32 voltype;
{
    if (voltype != RWVOL && voltype != ROVOL && voltype != BACKVOL)
	return 1;
    return 0;
}


static int
InvalidOperation(voloper)
     afs_int32 voloper;
{
    if (voloper != VLOP_MOVE && voloper != VLOP_RELEASE
	&& voloper != VLOP_BACKUP && voloper != VLOP_DELETE
	&& voloper != VLOP_DUMP)
	return 1;
    return 0;
}

static int
InvalidReleasetype(releasetype)
     afs_int32 releasetype;
{
    if ((releasetype & LOCKREL_TIMESTAMP) || (releasetype & LOCKREL_OPCODE)
	|| (releasetype & LOCKREL_AFSID))
	return 0;
    return 1;
}

static int
IpAddrToRelAddr(ipaddr, atrans)
     struct ubik_trans *atrans;
     register afs_uint32 ipaddr;
{
    register int i, j;
    register afs_int32 code, base, index;
    struct extentaddr *exp;

    for (i = 0; i <= MAXSERVERID; i++) {
	if (vl_HostAddress[i] == ipaddr)
	    return i;
	if ((vl_HostAddress[i] & 0xff000000) == 0xff000000) {
	    base = (vl_HostAddress[i] >> 16) & 0xff;
	    index = vl_HostAddress[i] & 0x0000ffff;
	    if (base >= VL_MAX_ADDREXTBLKS) {
		VLog(0,
		     ("Internal error: Multihome extent base is too large. Base %d index %d\n",
		      base, index));
		return -1;	/* EINVAL */
	    }
	    if (index >= VL_MHSRV_PERBLK) {
		VLog(0,
		     ("Internal error: Multihome extent index is too large. Base %d index %d\n",
		      base, index));
		return -1;	/* EINVAL */
	    }
	    if (!ex_addr[base]) {
		VLog(0,
		     ("Internal error: Multihome extent does not exist. Base %d\n",
		      base));
		return -1;	/* EINVAL */
	    }
	    exp = &ex_addr[base][index];
	    for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
		if (exp->ex_addrs[j] && (ntohl(exp->ex_addrs[j]) == ipaddr)) {
		    return i;
		}
	    }
	}
    }

    /* allocate the new server a server id pronto */
    if (atrans) {
	for (i = 0; i <= MAXSERVERID; i++) {
	    if (cheader.IpMappedAddr[i] == 0) {
		cheader.IpMappedAddr[i] = htonl(ipaddr);
		code =
		    vlwrite(atrans,
			    DOFFSET(0, &cheader, &cheader.IpMappedAddr[i]),
			    (char *)&cheader.IpMappedAddr[i],
			    sizeof(afs_int32));
		vl_HostAddress[i] = ipaddr;
		if (code)
		    return -1;
		return i;
	    }
	}
    }
    return -1;
}

static int
ChangeIPAddr(ipaddr1, ipaddr2, atrans)
     struct ubik_trans *atrans;
     register afs_uint32 ipaddr1, ipaddr2;
{
    int i, j;
    afs_int32 code;
    struct extentaddr *exp;
    int base, index, mhidx;
    afsUUID tuuid;
    afs_int32 blockindex, count;
    int pollcount = 0;
    struct nvlentry tentry;

    if (!atrans)
	return VL_CREATEFAIL;

    /* Don't let addr change to 256.*.*.* : Causes internal error below */
    if ((ipaddr2 & 0xff000000) == 0xff000000)
	return (VL_BADSERVER);

    /* If we are removing an address, ip1 will be -1 and ip2 will be
     * the original address. This prevents an older revision vlserver
     * from removing the IP address (won't find server 0xfffffff in
     * the VLDB). An older revision vlserver does not have the check
     * to see if any volumes exist on the server being removed.
     */
    if (ipaddr1 == 0xffffffff) {
	ipaddr1 = ipaddr2;
	ipaddr2 = 0;
    }

    for (i = 0; i <= MAXSERVERID; i++) {
	if ((vl_HostAddress[i] & 0xff000000) == 0xff000000) {
	    base = (vl_HostAddress[i] >> 16) & 0xff;
	    index = vl_HostAddress[i] & 0x0000ffff;
	    if ((base >= VL_MAX_ADDREXTBLKS) || (index >= VL_MHSRV_PERBLK)) {
		VLog(0,
		     ("Internal error: Multihome extent addr is too large. Base %d index %d\n",
		      base, index));
		return -1;	/* EINVAL */
	    }

	    exp = &ex_addr[base][index];
	    for (mhidx = 0; mhidx < VL_MAXIPADDRS_PERMH; mhidx++) {
		if (!exp->ex_addrs[mhidx])
		    continue;
		if (ntohl(exp->ex_addrs[mhidx]) == ipaddr1)
		    break;
	    }
	    if (mhidx < VL_MAXIPADDRS_PERMH) {
		break;
	    }
	} else if (vl_HostAddress[i] == ipaddr1) {
	    exp = NULL;
	    break;
	}
    }

    if (i >= MAXSERVERID) {
	return VL_NOENT;	/* not found */
    }

    /* If we are removing a server entry, a volume cannot
     * exist on the server. If one does, don't remove the
     * server entry: return error "volume entry exists".
     */
    if (ipaddr2 == 0) {
	for (blockindex = NextEntry(atrans, 0, &tentry, &count); blockindex;
	     blockindex = NextEntry(atrans, blockindex, &tentry, &count)) {
	    if (++pollcount > 50) {
		IOMGR_Poll();
		pollcount = 0;
	    }
	    for (j = 0; j < NMAXNSERVERS; j++) {
		if (tentry.serverNumber[j] == BADSERVERID)
		    break;
		if (tentry.serverNumber[j] == i) {
		    return VL_IDEXIST;
		}
	    }
	}
    }

    /* Log a message saying we are changing/removing an IP address */
    VLog(0,
	 ("The following IP address is being %s:\n",
	  (ipaddr2 ? "changed" : "removed")));
    VLog(0, ("      entry %d: ", i));
    if (exp) {
	VLog(0, ("["));
	for (mhidx = 0; mhidx < VL_MAXIPADDRS_PERMH; mhidx++) {
	    if (!exp->ex_addrs[mhidx])
		continue;
	    if (mhidx > 0)
		VLog(0, (" "));
	    PADDR(ntohl(exp->ex_addrs[mhidx]));
	}
	VLog(0, ("]"));
    } else {
	PADDR(ipaddr1);
    }
    if (ipaddr2) {
	VLog(0, (" -> "));
	PADDR(ipaddr2);
    }
    VLog(0, ("\n"));

    /* Change the registered uuuid addresses */
    if (exp) {
	memset(&tuuid, 0, sizeof(afsUUID));
	afs_htonuuid(&tuuid);
	exp->ex_hostuuid = tuuid;
	code =
	    vlwrite(atrans,
		    DOFFSET(ntohl(ex_addr[0]->ex_contaddrs[base]),
			    (char *)ex_addr[base], (char *)exp),
		    (char *)&tuuid, sizeof(tuuid));
	if (code)
	    return VL_IO;
    }

    /* Now change the host address entry */
    cheader.IpMappedAddr[i] = htonl(ipaddr2);
    code =
	vlwrite(atrans, DOFFSET(0, &cheader, &cheader.IpMappedAddr[i]),
		(char *)
		&cheader.IpMappedAddr[i], sizeof(afs_int32));
    vl_HostAddress[i] = ipaddr2;
    if (code)
	return VL_IO;

    return 0;
}

/* see if the vlserver is back yet */
afs_int32
SVL_ProbeServer(rxcall)
     struct rx_call *rxcall;
{
    return 0;
}
