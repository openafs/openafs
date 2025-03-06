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

#include <lock.h>
#include <afs/afsutil.h>
#include <ubik.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <afs/keys.h>
#include <afs/cellconfig.h>

#include "vlserver.h"
#include "vlserver_internal.h"
#include "afs/audit.h"

#ifdef HAVE_POSIX_REGEX		/* use POSIX regexp library */
#include <regex.h>
#endif

extern int smallMem;
extern int restrictedQueryLevel;
extern int extent_mod;
extern struct afsconf_dir *vldb_confdir;
extern struct ubik_dbase *VL_dbase;
int maxnservers;
#define ABORT(c) do { \
    code = (c); \
    goto abort; \
} while (0)

#define VLDBALLOCLIMIT	10000
#define VLDBALLOCINCR	2048

static int put_attributeentry(struct vl_ctx *ctx,
			      struct vldbentry **, struct vldbentry **,
			      struct vldbentry **, bulkentries *,
			      struct nvlentry *, afs_int32 *, afs_int32 *);
static int put_nattributeentry(struct vl_ctx *ctx,
			       struct nvldbentry **, struct nvldbentry **,
			       struct nvldbentry **, nbulkentries *,
			       struct nvlentry *, afs_int32, afs_int32,
			       afs_int32 *, afs_int32 *);
static int RemoveEntry(struct vl_ctx *ctx, afs_int32 entryptr,
		       struct nvlentry *tentry);
static void ReleaseEntry(struct nvlentry *tentry, afs_int32 releasetype);
static int check_vldbentry(struct vldbentry *aentry);
static int check_nvldbentry(struct nvldbentry *aentry);
static int vldbentry_to_vlentry(struct vl_ctx *ctx,
				struct vldbentry *VldbEntry,
				struct nvlentry *VlEntry);
static int nvldbentry_to_vlentry(struct vl_ctx *ctx,
				 struct nvldbentry *VldbEntry,
				 struct nvlentry *VlEntry);
static int get_vldbupdateentry(struct vl_ctx *ctx, afs_int32 blockindex,
			       struct VldbUpdateEntry *updateentry,
			       struct nvlentry *VlEntry);
static int repsite_exists(struct nvlentry *VlEntry, int server, int partition);
static void repsite_compress(struct nvlentry *VlEntry, int offset);
static int vlentry_to_vldbentry(struct vl_ctx *ctx,
				struct nvlentry *VlEntry,
				struct vldbentry *VldbEntry);
static int vlentry_to_nvldbentry(struct vl_ctx *ctx,
				 struct nvlentry *VlEntry,
				 struct nvldbentry *VldbEntry);
static int vlentry_to_uvldbentry(struct vl_ctx *ctx,
				 struct nvlentry *VlEntry,
				 struct uvldbentry *VldbEntry);
static int InvalidVolname(char *volname);
static int InvalidVoltype(afs_int32 voltype);
static int InvalidOperation(afs_int32 voloper);
static int InvalidReleasetype(afs_int32 releasetype);
static int IpAddrToRelAddr(struct vl_ctx *ctx, afs_uint32 ipaddr, int create);
static int ChangeIPAddr(struct vl_ctx *ctx, afs_uint32 ipaddr1,
                        afs_uint32 ipaddr2);

static_inline void
countRequest(int opcode)
{
    if (opcode != 0) {
        dynamic_statistics.requests[opcode - VL_LOWEST_OPCODE]++;
    }
}

static_inline void
countAbort(int opcode)
{
    if (opcode != 0) {
	dynamic_statistics.aborts[opcode - VL_LOWEST_OPCODE]++;
    }
}


static_inline int
multiHomedExtentBase(struct vl_ctx *ctx, int srvidx, struct extentaddr **exp,
		     int *basePtr)
{
    int base;
    int index;

    *exp = NULL;
    *basePtr = 0;

    if ((ctx->hostaddress[srvidx] & 0xff000000) == 0xff000000) {
	base = (ctx->hostaddress[srvidx] >> 16) & 0xff;
	index = ctx->hostaddress[srvidx] & 0x0000ffff;
	if (base >= VL_MAX_ADDREXTBLKS) {
	    VLog(0, ("Internal error: Multihome extent base is too large. "
		     "Base %d index %d\n", base, index));
	    return VL_IO;
	}
	if (index >= VL_MHSRV_PERBLK) {
	    VLog(0, ("Internal error: Multihome extent index is too large. "
		     "Base %d index %d\n", base, index));
	    return VL_IO;
	}
	if (!ctx->ex_addr[base]) {
	    VLog(0, ("Internal error: Multihome extent does not exist. "
		     "Base %d\n", base));
	    return VL_IO;
	}

	*basePtr = base;
	*exp = &ctx->ex_addr[base][index];
    }

    return 0;
}

static_inline int
multiHomedExtent(struct vl_ctx *ctx, int srvidx, struct extentaddr **exp)
{
    int base;

    return multiHomedExtentBase(ctx, srvidx, exp, &base);
}

#define AFS_RXINFO_LEN 217
static char *
rxkadInfo(char *str, struct rx_connection *conn, struct in_addr hostAddr)
{
    int code;
    char tname[64] = "";
    char tinst[64] = "";
    char tcell[64] = "";

    code = rxkad_GetServerInfo(conn, NULL, NULL, tname, tinst, tcell,
			       NULL);
    if (!code)
	snprintf(str, AFS_RXINFO_LEN,
		 "%s rxkad:%s%s%s%s%s", inet_ntoa(hostAddr), tname,
		(tinst[0] == '\0') ? "" : ".",
		(tinst[0] == '\0') ? "" : tinst,
		(tcell[0] == '\0') ? "" : "@",
		(tcell[0] == '\0') ? "" : tcell);
    else
	snprintf(str, AFS_RXINFO_LEN, "%s noauth", inet_ntoa(hostAddr));
    return (str);
}

static char *
rxinfo(char *str, struct rx_call *rxcall)
{
    struct rx_connection *conn;
    struct in_addr hostAddr;
    rx_securityIndex authClass;

    conn = rx_ConnectionOf(rxcall);
    authClass = rx_SecurityClassOf(conn);
    hostAddr.s_addr = rx_HostOf(rx_PeerOf(conn));

    switch(authClass) {
    case RX_SECIDX_KAD:
	return rxkadInfo(str, conn, hostAddr);
    default:
	;
    }

    snprintf(str, AFS_RXINFO_LEN, "%s noauth", inet_ntoa(hostAddr));
    return str;
}


/* This is called to initialize the database, set the appropriate locks and make sure that the vldb header is valid */
int
Init_VLdbase(struct vl_ctx *ctx,
	     int locktype,	/* indicate read or write transaction */
	     int opcode)
{
    int code = 0, pass, wl;

    for (pass = 1; pass <= 3; pass++) {
	if (pass == 2) {	/* take write lock to rebuild the db */
	    code = ubik_BeginTrans(VL_dbase, UBIK_WRITETRANS, &ctx->trans);
	    wl = 1;
	} else if (locktype == LOCKREAD) {
#ifdef UBIK_READ_WHILE_WRITE
	    code = ubik_BeginTransReadAnyWrite(VL_dbase, UBIK_READTRANS, &ctx->trans);
#else
	    code = ubik_BeginTransReadAny(VL_dbase, UBIK_READTRANS, &ctx->trans);
#endif
	    wl = 0;
	} else {
	    code = ubik_BeginTrans(VL_dbase, UBIK_WRITETRANS, &ctx->trans);
	    wl = 1;
	}
	if (code)
	    return code;

	code = ubik_SetLock(ctx->trans, 1, 1, locktype);
	if (code) {
	    countAbort(opcode);
	    ubik_AbortTrans(ctx->trans);
	    return code;
	}

	/* check that dbase is initialized and setup cheader */
	/* 2nd pass we try to rebuild the header */
	code = CheckInit(ctx->trans, ((pass == 2) ? 1 : 0));
	if (!code && wl && extent_mod)
	    code = readExtents(ctx->trans);	/* Fix the mh extent blocks */
	if (code) {
	    countAbort(opcode);
	    ubik_AbortTrans(ctx->trans);
	    /* Only rebuld if the database is empty */
	    /* Exit if can't rebuild */
	    if ((pass == 1) && (code != VL_EMPTY))
		return code;
	    if (pass == 2)
		return code;
	} else {		/* No code */
	    if (pass == 2) {
		/* The database header was rebuilt; end the write transaction.
		 * This will call vlsynccache() to copy the write header buffers
		 * to the read header buffers, before calling vlsetache().
		 * Do a third pass to re-acquire the original lock, which
		 * may be a read lock. */
		ubik_EndTrans(ctx->trans);
	    } else {
		break;		/* didn't rebuild and successful - exit */
	    }
	}
    }
    if (code == 0) {
	code = vlsetcache(ctx, locktype);
    }
    return code;
}


/* Create a new vldb entry; both new volume id and name must be unique
 * (non-existant in vldb).
 */

static afs_int32
CreateEntry(struct rx_call *rxcall, struct vldbentry *newentry)
{
    int this_op = VLCREATEENTRY;
    struct vl_ctx ctx;
    afs_int32 code, blockindex;
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL)) {
	return VL_PERM;
    }

    /* Do some validity tests on new entry */
    if ((code = check_vldbentry(newentry))
	|| (code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    VLog(1,
	 ("OCreate Volume %d %s\n", newentry->volumeId[RWVOL],
	  rxinfo(rxstr, rxcall)));
    if (EntryIDExists(&ctx, newentry->volumeId, MAXTYPES, &code)) {
	/* at least one of the specified IDs already exists; we fail */
	code = VL_IDEXIST;
	goto abort;
    } else if (code) {
	goto abort;
    }

    /* Is this following check (by volume name) necessary?? */
    /* If entry already exists, we fail */
    if (FindByName(&ctx, newentry->name, &tentry, &code)) {
	code = VL_NAMEEXIST;
	goto abort;
    } else if (code) {
	goto abort;
    }

    blockindex = AllocBlock(&ctx, &tentry);
    if (blockindex == 0) {
	code = VL_CREATEFAIL;
	goto abort;
    }

    memset(&tentry, 0, sizeof(struct nvlentry));
    /* Convert to its internal representation; both in host byte order */
    if ((code = vldbentry_to_vlentry(&ctx, newentry, &tentry))) {
	FreeBlock(&ctx, blockindex);
	goto abort;
    }

    /* Actually insert the entry in vldb */
    code = ThreadVLentry(&ctx, blockindex, &tentry);
    if (code) {
	FreeBlock(&ctx, blockindex);
	goto abort;
    } else {
	return ubik_EndTrans(ctx.trans);
    }

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_CreateEntry(struct rx_call *rxcall, struct vldbentry *newentry)
{
    afs_int32 code;

    code = CreateEntry(rxcall, newentry);
    osi_auditU(rxcall, VLCreateEntryEvent, code, AUD_STR,
	       (newentry ? newentry->name : NULL), AUD_END);
    return code;
}

static afs_int32
CreateEntryN(struct rx_call *rxcall, struct nvldbentry *newentry)
{
    int this_op = VLCREATEENTRYN;
    struct vl_ctx ctx;
    afs_int32 code, blockindex;
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL)) {
	return VL_PERM;
    }

    /* Do some validity tests on new entry */
    if ((code = check_nvldbentry(newentry))
	|| (code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    VLog(1,
	 ("Create Volume %d %s\n", newentry->volumeId[RWVOL],
	  rxinfo(rxstr, rxcall)));
    if (EntryIDExists(&ctx, newentry->volumeId, MAXTYPES, &code)) {
	/* at least one of the specified IDs already exists; we fail */
	code = VL_IDEXIST;
	goto abort;
    } else if (code) {
	goto abort;
    }

    /* Is this following check (by volume name) necessary?? */
    /* If entry already exists, we fail */
    if (FindByName(&ctx, newentry->name, &tentry, &code)) {
	code = VL_NAMEEXIST;
	goto abort;
    } else if (code) {
	goto abort;
    }

    blockindex = AllocBlock(&ctx, &tentry);
    if (blockindex == 0) {
	code = VL_CREATEFAIL;
	goto abort;
    }

    memset(&tentry, 0, sizeof(struct nvlentry));
    /* Convert to its internal representation; both in host byte order */
    if ((code = nvldbentry_to_vlentry(&ctx, newentry, &tentry))) {
	FreeBlock(&ctx, blockindex);
	goto abort;
    }

    /* Actually insert the entry in vldb */
    code = ThreadVLentry(&ctx, blockindex, &tentry);
    if (code) {
	FreeBlock(&ctx, blockindex);
	goto abort;
    } else {
	return ubik_EndTrans(ctx.trans);
    }

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_CreateEntryN(struct rx_call *rxcall, struct nvldbentry *newentry)
{
    afs_int32 code;

    code = CreateEntryN(rxcall, newentry);
    osi_auditU(rxcall, VLCreateEntryEvent, code, AUD_STR,
	       (newentry ? newentry->name : NULL), AUD_END);
    return code;
}

static afs_int32
ChangeAddr(struct rx_call *rxcall, afs_uint32 ip1, afs_uint32 ip2)
{
    int this_op = VLCHANGEADDR;
    struct vl_ctx ctx;
    afs_int32 code;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL)) {
	return VL_PERM;
    }

    if ((code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    VLog(1, ("Change Addr %u -> %u %s\n", ip1, ip2, rxinfo(rxstr, rxcall)));
    if ((code = ChangeIPAddr(&ctx, ip1, ip2)))
	goto abort;
    else {
	code = ubik_EndTrans(ctx.trans);
	return code;
    }

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_ChangeAddr(struct rx_call *rxcall, afs_uint32 ip1, afs_uint32 ip2)
{
    afs_int32 code;

    code = ChangeAddr(rxcall, ip1, ip2);
    osi_auditU(rxcall, VLChangeAddrEvent, code, AUD_LONG, ip1, AUD_LONG,
	       ip2, AUD_END);
    return code;
}

/* Delete a vldb entry given the volume id. */
static afs_int32
DeleteEntry(struct rx_call *rxcall, afs_uint32 volid, afs_int32 voltype)
{
    int this_op = VLDELETEENTRY;
    struct vl_ctx ctx;
    afs_int32 blockindex, code;
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	return VL_PERM;

    if ((voltype != -1) && (InvalidVoltype(voltype)))
	return VL_BADVOLTYPE;

    if ((code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    VLog(1, ("Delete Volume %u %s\n", volid, rxinfo(rxstr, rxcall)));
    blockindex = FindByID(&ctx, volid, voltype, &tentry, &code);
    if (blockindex == 0) {	/* volid not found */
	if (!code)
	    code = VL_NOENT;
	goto abort;
    }

    if (tentry.flags & VLDELETED) {	/* Already deleted; return */
	ABORT(VL_ENTDELETED);
    }
    if ((code = RemoveEntry(&ctx, blockindex, &tentry))) {
	goto abort;
    }
    return ubik_EndTrans(ctx.trans);

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_DeleteEntry(struct rx_call *rxcall, afs_uint32 volid, afs_int32 voltype)
{
    afs_int32 code;

    code = DeleteEntry(rxcall, volid, voltype);
    osi_auditU(rxcall, VLDeleteEntryEvent, code, AUD_LONG, volid,
	       AUD_END);
    return code;
}


/* Get a vldb entry given its volume id; make sure it's not a deleted entry. */
static int
GetEntryByID(struct rx_call *rxcall,
	     afs_uint32 volid,
	     afs_int32 voltype,
	     char *aentry,	/* entry data copied here */
	     afs_int32 new,
	     afs_int32 this_op)
{
    struct vl_ctx ctx;
    afs_int32 blockindex, code;
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);

    if ((voltype != -1) && (InvalidVoltype(voltype)))
	return VL_BADVOLTYPE;
    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;

    VLog(5, ("GetVolumeByID %u (%d) %s\n", volid, new,
             rxinfo(rxstr, rxcall)));
    blockindex = FindByID(&ctx, volid, voltype, &tentry, &code);
    if (blockindex == 0) {	/* entry not found */
	if (!code)
	    code = VL_NOENT;
	goto abort;
    }
    if (tentry.flags & VLDELETED) {	/* Entry is deleted! */
	code = VL_ENTDELETED;
	goto abort;
    }
    /* Convert from the internal to external form */
    if (new == 1)
	code = vlentry_to_nvldbentry(&ctx, &tentry, (struct nvldbentry *)aentry);
    else if (new == 2)
	code = vlentry_to_uvldbentry(&ctx, &tentry, (struct uvldbentry *)aentry);
    else
	code = vlentry_to_vldbentry(&ctx, &tentry, (struct vldbentry *)aentry);

    if (code)
	goto abort;

    return (ubik_EndTrans(ctx.trans));

abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_GetEntryByID(struct rx_call *rxcall,
		 afs_uint32 volid,
		 afs_int32 voltype,
		 vldbentry *aentry)		/* entry data copied here */
{
    return (GetEntryByID(rxcall, volid, voltype, (char *)aentry, 0,
			 VLGETENTRYBYID));
}

afs_int32
SVL_GetEntryByIDN(struct rx_call *rxcall,
		  afs_uint32 volid,
		  afs_int32 voltype,
		  nvldbentry *aentry)	/* entry data copied here */
{
    return (GetEntryByID(rxcall, volid, voltype, (char *)aentry, 1,
			 VLGETENTRYBYIDN));
}

afs_int32
SVL_GetEntryByIDU(struct rx_call *rxcall,
		  afs_uint32 volid,
		  afs_int32 voltype,
		  uvldbentry *aentry)	/* entry data copied here */
{
    return (GetEntryByID(rxcall, volid, voltype, (char *)aentry, 2,
			 VLGETENTRYBYIDU));
}

/* returns true if the id is a decimal integer, in which case we interpret
 * it as an id.  make the cache manager much simpler */
static int
NameIsId(char *aname)
{
    int tc;
    while ((tc = *aname++)) {
	if (tc > '9' || tc < '0')
	    return 0;
    }
    return 1;
}

/* Get a vldb entry given the volume's name; of course, very similar to
 * VLGetEntryByID() above. */
static afs_int32
GetEntryByName(struct rx_call *rxcall,
	       char *volname,
	       char *aentry,		/* entry data copied here */
	       int new,
	       int this_op)
{
    struct vl_ctx ctx;
    afs_int32 blockindex, code;
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    if (NameIsId(volname)) {
	return GetEntryByID(rxcall, strtoul(volname, NULL, 10), -1, aentry, new, this_op);
    }

    countRequest(this_op);

    if (InvalidVolname(volname))
	return VL_BADNAME;
    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;
    VLog(5, ("GetVolumeByName %s (%d) %s\n", volname, new, rxinfo(rxstr, rxcall)));
    blockindex = FindByName(&ctx, volname, &tentry, &code);
    if (blockindex == 0) {	/* entry not found */
	if (!code)
	    code = VL_NOENT;
	goto abort;
    }
    if (tentry.flags & VLDELETED) {	/* Entry is deleted */
	code = VL_ENTDELETED;
	goto abort;
    }
    /* Convert to external entry representation */
    if (new == 1)
	code = vlentry_to_nvldbentry(&ctx, &tentry, (struct nvldbentry *)aentry);
    else if (new == 2)
	code = vlentry_to_uvldbentry(&ctx, &tentry, (struct uvldbentry *)aentry);
    else
	code = vlentry_to_vldbentry(&ctx, &tentry, (struct vldbentry *)aentry);

    if (code)
	goto abort;

    return (ubik_EndTrans(ctx.trans));

abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;

}

afs_int32
SVL_GetEntryByNameO(struct rx_call *rxcall,
		    char *volname,
		    struct vldbentry *aentry)	/* entry data copied here */
{
    return (GetEntryByName(rxcall, volname, (char *)aentry, 0,
			   VLGETENTRYBYNAME));
}

afs_int32
SVL_GetEntryByNameN(struct rx_call *rxcall,
		    char *volname,
		    struct nvldbentry *aentry)	/* entry data copied here */
{
    return (GetEntryByName(rxcall, volname, (char *)aentry, 1,
			   VLGETENTRYBYNAMEN));
}

afs_int32
SVL_GetEntryByNameU(struct rx_call *rxcall,
		    char *volname,
		    struct uvldbentry *aentry)	/* entry data copied here */
{
    return (GetEntryByName(rxcall, volname, (char *)aentry, 2,
			   VLGETENTRYBYNAMEU));
}

/* Get the current value of the maximum volume id and bump the volume id counter by Maxvolidbump. */
static afs_int32
getNewVolumeId(struct rx_call *rxcall, afs_uint32 Maxvolidbump,
		   afs_uint32 *newvolumeid)
{
    int this_op = VLGETNEWVOLUMEID;
    afs_int32 code;
    afs_uint32 maxvolumeid;
    struct vl_ctx ctx;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	return VL_PERM;

    if (Maxvolidbump > MAXBUMPCOUNT)
	return VL_BADVOLIDBUMP;

    if ((code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    *newvolumeid = maxvolumeid = NextUnusedID(&ctx,
	ntohl(ctx.cheader->vital_header.MaxVolumeId), Maxvolidbump, &code);
    if (code) {
	goto abort;
    }

    maxvolumeid += Maxvolidbump;
    VLog(1, ("GetNewVolid newmax=%u %s\n", maxvolumeid, rxinfo(rxstr, rxcall)));
    ctx.cheader->vital_header.MaxVolumeId = htonl(maxvolumeid);
    if (write_vital_vlheader(&ctx)) {
	ABORT(VL_IO);
    }
    return ubik_EndTrans(ctx.trans);

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_GetNewVolumeId(struct rx_call *rxcall, afs_uint32 Maxvolidbump,
		   afs_uint32 *newvolumeid)
{
    afs_int32 code;

    code = getNewVolumeId(rxcall, Maxvolidbump, newvolumeid);
    osi_auditU(rxcall, VLGetNewVolumeIdEvent, code, AUD_END);
    return code;
}


/* Simple replace the contents of the vldb entry, volid, with
 * newentry. No individual checking/updating per field (alike
 * VLUpdateEntry) is done. */

static afs_int32
ReplaceEntry(struct rx_call *rxcall, afs_uint32 volid, afs_int32 voltype,
		 struct vldbentry *newentry, afs_int32 releasetype)
{
    int this_op = VLREPLACEENTRY;
    struct vl_ctx ctx;
    afs_int32 blockindex, code, typeindex;
    int hashnewname;
    int hashVol[MAXTYPES];
    struct nvlentry tentry;
    afs_uint32 checkids[MAXTYPES];
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    for (typeindex = 0; typeindex < MAXTYPES; typeindex++)
	hashVol[typeindex] = 0;
    hashnewname = 0;
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	return VL_PERM;

    if ((code = check_vldbentry(newentry)))
	return code;

    if (voltype != -1 && InvalidVoltype(voltype))
	return VL_BADVOLTYPE;

    if (releasetype && InvalidReleasetype(releasetype))
	return VL_BADRELLOCKTYPE;
    if ((code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    VLog(1, ("OReplace Volume %u %s\n", volid, rxinfo(rxstr, rxcall)));
    /* find vlentry we're changing */
    blockindex = FindByID(&ctx, volid, voltype, &tentry, &code);
    if (blockindex == 0) {	/* entry not found */
	if (!code)
	    code = VL_NOENT;
	goto abort;
    }

    /* check that we're not trying to change the RW vol ID */
    if (newentry->volumeId[RWVOL] != tentry.volumeId[RWVOL]) {
	ABORT(VL_BADENTRY);
    }

    /* make sure none of the IDs we are changing to are already in use */
    memset(&checkids, 0, sizeof(checkids));
    for (typeindex = ROVOL; typeindex < MAXTYPES; typeindex++) {
	if (tentry.volumeId[typeindex] != newentry->volumeId[typeindex]) {
	    checkids[typeindex] = newentry->volumeId[typeindex];
	}
    }
    if (EntryIDExists(&ctx, checkids, MAXTYPES, &code)) {
	ABORT(VL_IDEXIST);
    } else if (code) {
	goto abort;
    }

    /* make sure the name we're changing to doesn't already exist */
    if (strcmp(newentry->name, tentry.name)) {
	struct nvlentry tmp_entry;
	if (FindByName(&ctx, newentry->name, &tmp_entry, &code)) {
	    ABORT(VL_NAMEEXIST);
	} else if (code) {
	    goto abort;
	}
    }

    /* unhash volid entries if they're disappearing or changing.
     * Remember if we need to hash in the new value (we don't have to
     * rehash if volid stays same */
    for (typeindex = ROVOL; typeindex <= BACKVOL; typeindex++) {
	if (tentry.volumeId[typeindex] != newentry->volumeId[typeindex]) {
	    if (tentry.volumeId[typeindex])
		if ((code =
		    UnhashVolid(&ctx, typeindex, blockindex, &tentry))) {
		    goto abort;
		}
	    /* we must rehash new id if the id is different and the ID is nonzero */
	    hashVol[typeindex] = 1;	/* must rehash this guy if he exists */
	}
    }

    /* Rehash volname if it changes */
    if (strcmp(newentry->name, tentry.name)) {	/* Name changes; redo hashing */
	if ((code = UnhashVolname(&ctx, blockindex, &tentry))) {
	    goto abort;
	}
	hashnewname = 1;
    }

    /* after this, tentry is new entry, not old one.  vldbentry_to_vlentry
     * doesn't touch hash chains */
    if ((code = vldbentry_to_vlentry(&ctx, newentry, &tentry))) {
	goto abort;
    }

    for (typeindex = ROVOL; typeindex <= BACKVOL; typeindex++) {
	if (hashVol[typeindex] && tentry.volumeId[typeindex]) {
	    if ((code = HashVolid(&ctx, typeindex, blockindex, &tentry))) {
		goto abort;
	    }
	}
    }

    if (hashnewname)
	HashVolname(&ctx, blockindex, &tentry);

    if (releasetype)
	ReleaseEntry(&tentry, releasetype);	/* Unlock entry if necessary */
    if (vlentrywrite(ctx.trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }

    return ubik_EndTrans(ctx.trans);

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_ReplaceEntry(struct rx_call *rxcall, afs_uint32 volid, afs_int32 voltype,
		 struct vldbentry *newentry, afs_int32 releasetype)
{
    afs_int32 code;

    code = ReplaceEntry(rxcall, volid, voltype, newentry, releasetype);
    osi_auditU(rxcall, VLReplaceVLEntryEvent, code, AUD_LONG, volid, AUD_END);
    return code;
}

static afs_int32
ReplaceEntryN(struct rx_call *rxcall, afs_uint32 volid, afs_int32 voltype,
		  struct nvldbentry *newentry, afs_int32 releasetype)
{
    int this_op = VLREPLACEENTRYN;
    struct vl_ctx ctx;
    afs_int32 blockindex, code, typeindex;
    int hashnewname;
    int hashVol[MAXTYPES];
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    for (typeindex = 0; typeindex < MAXTYPES; typeindex++)
	hashVol[typeindex] = 0;
    hashnewname = 0;
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	return VL_PERM;

    if ((code = check_nvldbentry(newentry)))
	return code;

    if (voltype != -1 && InvalidVoltype(voltype))
	return VL_BADVOLTYPE;

    if (releasetype && InvalidReleasetype(releasetype))
	return VL_BADRELLOCKTYPE;
    if ((code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    VLog(1, ("Replace Volume %u %s\n", volid, rxinfo(rxstr, rxcall)));
    /* find vlentry we're changing */
    blockindex = FindByID(&ctx, volid, voltype, &tentry, &code);
    if (blockindex == 0) {	/* entry not found */
	if (!code)
	    code = VL_NOENT;
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
		if ((code =
		    UnhashVolid(&ctx, typeindex, blockindex, &tentry))) {
		    goto abort;
		}
	    /* we must rehash new id if the id is different and the ID is nonzero */
	    hashVol[typeindex] = 1;	/* must rehash this guy if he exists */
	}
    }

    /* Rehash volname if it changes */
    if (strcmp(newentry->name, tentry.name)) {	/* Name changes; redo hashing */
	if ((code = UnhashVolname(&ctx, blockindex, &tentry))) {
	    goto abort;
	}
	hashnewname = 1;
    }

    /* after this, tentry is new entry, not old one.  vldbentry_to_vlentry
     * doesn't touch hash chains */
    if ((code = nvldbentry_to_vlentry(&ctx, newentry, &tentry))) {
	goto abort;
    }

    for (typeindex = ROVOL; typeindex <= BACKVOL; typeindex++) {
	if (hashVol[typeindex] && tentry.volumeId[typeindex]) {
	    if ((code = HashVolid(&ctx, typeindex, blockindex, &tentry))) {
		goto abort;
	    }
	}
    }

    if (hashnewname)
	HashVolname(&ctx, blockindex, &tentry);

    if (releasetype)
	ReleaseEntry(&tentry, releasetype);	/* Unlock entry if necessary */
    if (vlentrywrite(ctx.trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }

    return ubik_EndTrans(ctx.trans);

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_ReplaceEntryN(struct rx_call *rxcall, afs_uint32 volid, afs_int32 voltype,
		  struct nvldbentry *newentry, afs_int32 releasetype)
{
    afs_int32 code;

    code = ReplaceEntryN(rxcall, volid, voltype, newentry, releasetype);
    osi_auditU(rxcall, VLReplaceVLEntryEvent, code, AUD_LONG, volid, AUD_END);
    return code;
}


/* Update a vldb entry (accessed thru its volume id). Almost all of the
 * entry's fields can be modified in a single call by setting the
 * appropriate bits in the Mask field in VldbUpdateentry. */
/* this routine may never have been tested; use replace entry instead
 * unless you're brave */
static afs_int32
UpdateEntry(struct rx_call *rxcall,
	    afs_uint32 volid,
	    afs_int32 voltype,
	    struct VldbUpdateEntry *updateentry,	/* Update entry copied here */
	    afs_int32 releasetype)
{
    int this_op = VLUPDATEENTRY;
    struct vl_ctx ctx;
    afs_int32 blockindex, code;
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	return VL_PERM;
    if ((voltype != -1) && (InvalidVoltype(voltype)))
	return VL_BADVOLTYPE;
    if (releasetype && InvalidReleasetype(releasetype))
	return VL_BADRELLOCKTYPE;
    if ((code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    VLog(1, ("Update Volume %u %s\n", volid, rxinfo(rxstr, rxcall)));
    blockindex = FindByID(&ctx, volid, voltype, &tentry, &code);
    if (blockindex == 0) {	/* entry not found */
	if (!code)
	    code = VL_NOENT;
	goto abort;
    }

    /* Do the actual updating of the entry, tentry. */
    if ((code =
	get_vldbupdateentry(&ctx, blockindex, updateentry, &tentry))) {
	goto abort;
    }
    if (releasetype)
	ReleaseEntry(&tentry, releasetype);	/* Unlock entry if necessary */
    if (vlentrywrite(ctx.trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }
    return ubik_EndTrans(ctx.trans);

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_UpdateEntry(struct rx_call *rxcall,
		afs_uint32 volid,
		afs_int32 voltype,
		struct VldbUpdateEntry *updateentry,
		afs_int32 releasetype)
{
    afs_int32 code;

    code = UpdateEntry(rxcall, volid, voltype, updateentry, releasetype);
    osi_auditU(rxcall, VLUpdateEntryEvent, code, AUD_LONG, volid, AUD_END);
    return code;
}

static afs_int32
UpdateEntryByName(struct rx_call *rxcall,
		  char *volname,
		  struct VldbUpdateEntry *updateentry, /* Update entry copied here */
		  afs_int32 releasetype)
{
    int this_op = VLUPDATEENTRYBYNAME;
    struct vl_ctx ctx;
    afs_int32 blockindex, code;
    struct nvlentry tentry;

    countRequest(this_op);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	return VL_PERM;
    if (releasetype && InvalidReleasetype(releasetype))
	return VL_BADRELLOCKTYPE;
    if ((code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    blockindex = FindByName(&ctx, volname, &tentry, &code);
    if (blockindex == 0) {	/* entry not found */
	if (!code)
	    code = VL_NOENT;
	goto abort;
    }

    /* Do the actual updating of the entry, tentry. */
    if ((code =
	get_vldbupdateentry(&ctx, blockindex, updateentry, &tentry))) {
	goto abort;
    }
    if (releasetype)
	ReleaseEntry(&tentry, releasetype);	/* Unlock entry if necessary */
    if (vlentrywrite(ctx.trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }
    return ubik_EndTrans(ctx.trans);

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_UpdateEntryByName(struct rx_call *rxcall,
		      char *volname,
		      struct VldbUpdateEntry *updateentry, /* Update entry copied here */
		      afs_int32 releasetype)
{
    afs_int32 code;

    code = UpdateEntryByName(rxcall, volname, updateentry, releasetype);
    osi_auditU(rxcall, VLUpdateEntryEvent, code, AUD_LONG, -1, AUD_END);
    return code;
}

/* Set a lock to the vldb entry for volid (of type voltype if not -1). */
static afs_int32
SetLock(struct rx_call *rxcall, afs_uint32 volid, afs_int32 voltype,
	afs_int32 voloper)
{
    int this_op = VLSETLOCK;
    afs_int32 timestamp, blockindex, code;
    struct vl_ctx ctx;
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	return VL_PERM;
    if ((voltype != -1) && (InvalidVoltype(voltype)))
	return VL_BADVOLTYPE;
    if (InvalidOperation(voloper))
	return VL_BADVOLOPER;
    if ((code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    VLog(1, ("SetLock Volume %u %s\n", volid, rxinfo(rxstr, rxcall)));
    blockindex = FindByID(&ctx, volid, voltype, &tentry, &code);
    if (blockindex == NULLO) {
	if (!code)
	    code = VL_NOENT;
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

    if (vlentrywrite(ctx.trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }
    return ubik_EndTrans(ctx.trans);

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_SetLock(struct rx_call *rxcall, afs_uint32 volid, afs_int32 voltype,
	    afs_int32 voloper)
{
    afs_int32 code;

    code = SetLock(rxcall, volid, voltype, voloper);
    osi_auditU(rxcall, VLSetLockEvent, code, AUD_LONG, volid, AUD_END);
    return code;
}

/* Release an already locked vldb entry. Releasetype determines what
 * fields (afsid and/or volume operation) will be cleared along with
 * the lock time stamp. */

static afs_int32
ReleaseLock(struct rx_call *rxcall, afs_uint32 volid, afs_int32 voltype,
	    afs_int32 releasetype)
{
    int this_op = VLRELEASELOCK;
    afs_int32 blockindex, code;
    struct vl_ctx ctx;
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	return VL_PERM;
    if ((voltype != -1) && (InvalidVoltype(voltype)))
	return VL_BADVOLTYPE;
    if (releasetype && InvalidReleasetype(releasetype))
	return VL_BADRELLOCKTYPE;
    if ((code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
	return code;

    VLog(1, ("ReleaseLock Volume %u %s\n", volid, rxinfo(rxstr, rxcall)));
    blockindex = FindByID(&ctx, volid, voltype, &tentry, &code);
    if (blockindex == NULLO) {
	if (!code)
	    code = VL_NOENT;
	goto abort;
    }
    if (tentry.flags & VLDELETED) {
	ABORT(VL_ENTDELETED);
    }
    if (releasetype)
	ReleaseEntry(&tentry, releasetype);	/* Unlock the appropriate fields */
    if (vlentrywrite(ctx.trans, blockindex, &tentry, sizeof(tentry))) {
	ABORT(VL_IO);
    }
    return ubik_EndTrans(ctx.trans);

  abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_ReleaseLock(struct rx_call *rxcall, afs_uint32 volid, afs_int32 voltype,
		afs_int32 releasetype)
{
    afs_int32 code;

    code = ReleaseLock(rxcall, volid, voltype, releasetype);
    osi_auditU(rxcall, VLReleaseLockEvent, code, AUD_LONG, volid, AUD_END);
    return code;
}

/* ListEntry returns a single vldb entry, aentry, with offset previous_index;
 * the remaining parameters (i.e. next_index) are used so that sequential
 * calls to this routine will get the next (all) vldb entries.
 */
static afs_int32
ListEntry(struct rx_call *rxcall, afs_int32 previous_index,
	  afs_int32 *count, afs_int32 *next_index,
	  struct vldbentry *aentry)
{
    int this_op = VLLISTENTRY;
    int code;
    struct vl_ctx ctx;
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);

    if (!afsconf_CheckRestrictedQuery(vldb_confdir, rxcall,
				      restrictedQueryLevel))
	return VL_PERM;

    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;
    VLog(25, ("OListEntry index=%d %s\n", previous_index,
              rxinfo(rxstr, rxcall)));
    *next_index = NextEntry(&ctx, previous_index, &tentry, count);
    if (*next_index) {
	code = vlentry_to_vldbentry(&ctx, &tentry, aentry);
	if (code) {
	    countAbort(this_op);
	    ubik_AbortTrans(ctx.trans);
	    return code;
	}
    }
    return ubik_EndTrans(ctx.trans);
}

afs_int32
SVL_ListEntry(struct rx_call *rxcall, afs_int32 previous_index,
	      afs_int32 *count, afs_int32 *next_index,
	      struct vldbentry *aentry)
{
    afs_int32 code;

    code = ListEntry(rxcall, previous_index, count, next_index, aentry);
    osi_auditU(rxcall, VLListEntryEvent, code, AUD_LONG, previous_index, AUD_END);
    return code;
}

/* ListEntry returns a single vldb entry, aentry, with offset previous_index;
 * the remaining parameters (i.e. next_index) are used so that sequential
 * calls to this routine will get the next (all) vldb entries.
 */
static afs_int32
ListEntryN(struct rx_call *rxcall, afs_int32 previous_index,
	   afs_int32 *count, afs_int32 *next_index,
	   struct nvldbentry *aentry)
{
    int this_op = VLLISTENTRYN;
    int code;
    struct vl_ctx ctx;
    struct nvlentry tentry;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);

    if (!afsconf_CheckRestrictedQuery(vldb_confdir, rxcall,
				      restrictedQueryLevel))
	return VL_PERM;

    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;
    VLog(25, ("ListEntry index=%d %s\n", previous_index, rxinfo(rxstr, rxcall)));
    *next_index = NextEntry(&ctx, previous_index, &tentry, count);
    if (*next_index) {
	code = vlentry_to_nvldbentry(&ctx, &tentry, aentry);
	if (code) {
	    countAbort(this_op);
	    ubik_AbortTrans(ctx.trans);
	    return code;
	}
    }

    return ubik_EndTrans(ctx.trans);
}

afs_int32
SVL_ListEntryN(struct rx_call *rxcall, afs_int32 previous_index,
	       afs_int32 *count, afs_int32 *next_index,
	       struct nvldbentry *aentry)
{
    afs_int32 code;

    code = ListEntryN(rxcall, previous_index, count, next_index, aentry);
    osi_auditU(rxcall, VLListEntryEventN, code, AUD_LONG, previous_index, AUD_END);
    return code;
}

/* Retrieves in vldbentries all vldb entries that match the specified
 * attributes (by server number, partition, volume type, and flag); if volume
 * id is specified then the associated list for that entry is returned.
 * CAUTION: This could be a very expensive call since in most cases
 * sequential search of all vldb entries is performed.
 */
static afs_int32
ListAttributes(struct rx_call *rxcall,
	       struct VldbListByAttributes *attributes,
	       afs_int32 *nentries,
	       bulkentries *vldbentries)
{
    int this_op = VLLISTATTRIBUTES;
    int code, allocCount = 0;
    struct vl_ctx ctx;
    struct nvlentry tentry;
    struct vldbentry *Vldbentry = 0, *VldbentryFirst = 0, *VldbentryLast = 0;
    int pollcount = 0;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);

    if (!afsconf_CheckRestrictedQuery(vldb_confdir, rxcall,
				      restrictedQueryLevel))
	return VL_PERM;

    vldbentries->bulkentries_val = 0;
    vldbentries->bulkentries_len = *nentries = 0;
    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;
    allocCount = VLDBALLOCCOUNT;
    Vldbentry = VldbentryFirst = vldbentries->bulkentries_val =
	malloc(allocCount * sizeof(vldbentry));
    if (Vldbentry == NULL) {
	code = VL_NOMEM;
	goto abort;
    }
    VldbentryLast = VldbentryFirst + allocCount;
    /* Handle the attribute by volume id totally separate of the rest
     * (thus additional Mask values are ignored if VLLIST_VOLUMEID is set!) */
    if (attributes->Mask & VLLIST_VOLUMEID) {
	afs_int32 blockindex;

	blockindex =
	    FindByID(&ctx, attributes->volumeid, -1, &tentry, &code);
	if (blockindex == 0) {
	    if (!code)
		code = VL_NOENT;
	    goto abort;
	}

	code = put_attributeentry(&ctx, &Vldbentry, &VldbentryFirst,
				  &VldbentryLast, vldbentries, &tentry,
				  nentries, &allocCount);
	if (code)
	    goto abort;
    } else {
	afs_int32 nextblockindex = 0, count = 0, k = 0, match = 0;
	while ((nextblockindex =
	       NextEntry(&ctx, nextblockindex, &tentry, &count))) {
	    if (++pollcount > 50) {
#ifndef AFS_PTHREAD_ENV
		IOMGR_Poll();
#endif
		pollcount = 0;
	    }
	    match = 0;
	    if (attributes->Mask & VLLIST_SERVER) {
		int serverindex;
		if ((serverindex =
		     IpAddrToRelAddr(&ctx, attributes->server, 0)) == -1)
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
	    code = put_attributeentry(&ctx, &Vldbentry, &VldbentryFirst,
				      &VldbentryLast, vldbentries, &tentry,
				      nentries, &allocCount);
	    if (code)
		goto abort;
	}
    }
    if (vldbentries->bulkentries_len
	&& (allocCount > vldbentries->bulkentries_len)) {

	vldbentries->bulkentries_val =
	    realloc(vldbentries->bulkentries_val,
		    vldbentries->bulkentries_len * sizeof(vldbentry));
	if (vldbentries->bulkentries_val == NULL) {
	    code = VL_NOMEM;
	    goto abort;
	}
    }
    VLog(5,
	 ("ListAttrs nentries=%d %s\n", vldbentries->bulkentries_len,
	  rxinfo(rxstr, rxcall)));
    return ubik_EndTrans(ctx.trans);

abort:
    if (vldbentries->bulkentries_val)
	free(vldbentries->bulkentries_val);
    vldbentries->bulkentries_val = 0;
    vldbentries->bulkentries_len = 0;

    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_ListAttributes(struct rx_call *rxcall,
		   struct VldbListByAttributes *attributes,
		   afs_int32 *nentries,
		   bulkentries *vldbentries)
{
    afs_int32 code;

    code = ListAttributes(rxcall, attributes, nentries, vldbentries);
    osi_auditU(rxcall, VLListAttributesEvent, code, AUD_END);
    return code;
}

static afs_int32
ListAttributesN(struct rx_call *rxcall,
		struct VldbListByAttributes *attributes,
		afs_int32 *nentries,
		nbulkentries *vldbentries)
{
    int this_op = VLLISTATTRIBUTESN;
    int code, allocCount = 0;
    struct vl_ctx ctx;
    struct nvlentry tentry;
    struct nvldbentry *Vldbentry = 0, *VldbentryFirst = 0, *VldbentryLast = 0;
    int pollcount = 0;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);

    if (!afsconf_CheckRestrictedQuery(vldb_confdir, rxcall,
				      restrictedQueryLevel))
	return VL_PERM;

    vldbentries->nbulkentries_val = 0;
    vldbentries->nbulkentries_len = *nentries = 0;
    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;
    allocCount = VLDBALLOCCOUNT;
    Vldbentry = VldbentryFirst = vldbentries->nbulkentries_val =
	malloc(allocCount * sizeof(nvldbentry));
    if (Vldbentry == NULL) {
	code = VL_NOMEM;
	goto abort;
    }
    VldbentryLast = VldbentryFirst + allocCount;
    /* Handle the attribute by volume id totally separate of the rest
     * (thus additional Mask values are ignored if VLLIST_VOLUMEID is set!) */
    if (attributes->Mask & VLLIST_VOLUMEID) {
	afs_int32 blockindex;

	blockindex =
	    FindByID(&ctx, attributes->volumeid, -1, &tentry, &code);
	if (blockindex == 0) {
	    if (!code)
		code = VL_NOENT;
	    goto abort;
	}

	code = put_nattributeentry(&ctx, &Vldbentry, &VldbentryFirst,
				   &VldbentryLast, vldbentries, &tentry,
				   0, 0, nentries, &allocCount);
	if (code)
	    goto abort;
    } else {
	afs_int32 nextblockindex = 0, count = 0, k = 0, match = 0;
	while ((nextblockindex =
	       NextEntry(&ctx, nextblockindex, &tentry, &count))) {
	    if (++pollcount > 50) {
#ifndef AFS_PTHREAD_ENV
		IOMGR_Poll();
#endif
		pollcount = 0;
	    }

	    match = 0;
	    if (attributes->Mask & VLLIST_SERVER) {
		int serverindex;
		if ((serverindex =
		     IpAddrToRelAddr(&ctx, attributes->server, 0)) == -1)
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
	    code = put_nattributeentry(&ctx, &Vldbentry, &VldbentryFirst,
				       &VldbentryLast, vldbentries,
				       &tentry, 0, 0, nentries, &allocCount);
	    if (code)
		goto abort;
	}
    }
    if (vldbentries->nbulkentries_len
	&& (allocCount > vldbentries->nbulkentries_len)) {

	vldbentries->nbulkentries_val =
	    realloc(vldbentries->nbulkentries_val,
		    vldbentries->nbulkentries_len * sizeof(nvldbentry));
	if (vldbentries->nbulkentries_val == NULL) {
	    code = VL_NOMEM;
	    goto abort;
	}
    }
    VLog(5,
	 ("NListAttrs nentries=%d %s\n", vldbentries->nbulkentries_len,
	  rxinfo(rxstr, rxcall)));
    return ubik_EndTrans(ctx.trans);

abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    if (vldbentries->nbulkentries_val)
	free(vldbentries->nbulkentries_val);
    vldbentries->nbulkentries_val = 0;
    vldbentries->nbulkentries_len = 0;
    return code;
}

afs_int32
SVL_ListAttributesN(struct rx_call *rxcall,
		    struct VldbListByAttributes *attributes,
		    afs_int32 *nentries,
		    nbulkentries *vldbentries)
{
    afs_int32 code;

    code = ListAttributesN(rxcall, attributes, nentries, vldbentries);
    osi_auditU(rxcall, VLListAttributesNEvent, code, AUD_END);
    return code;
}

static afs_int32
ListAttributesN2(struct rx_call *rxcall,
		 struct VldbListByAttributes *attributes,
		 char *name,		/* Wildcarded volume name */
		 afs_int32 startindex,
		 afs_int32 *nentries,
		 nbulkentries *vldbentries,
		 afs_int32 *nextstartindex)
{
    int this_op = VLLISTATTRIBUTESN2;
    int code = 0, maxCount = VLDBALLOCCOUNT;
    struct vl_ctx ctx;
    struct nvlentry tentry;
    struct nvldbentry *Vldbentry = 0, *VldbentryFirst = 0, *VldbentryLast = 0;
    afs_int32 blockindex = 0, count = 0, k, match;
    afs_int32 matchindex = 0;
    int serverindex = -1;	/* no server found */
    int findserver = 0, findpartition = 0, findflag = 0, findname = 0;
    int pollcount = 0;
    int namematchRWBK, namematchRO, thismatch;
    int matchtype = 0;
    int size;
    char volumename[VL_MAXNAMELEN+3]; /* regex anchors */
    char rxstr[AFS_RXINFO_LEN];
#ifdef HAVE_POSIX_REGEX
    regex_t re;
    int need_regfree = 0;
#else
    char *t;
#endif

    countRequest(this_op);

    if (!afsconf_CheckRestrictedQuery(vldb_confdir, rxcall,
				      restrictedQueryLevel))
	return VL_PERM;

    vldbentries->nbulkentries_val = 0;
    vldbentries->nbulkentries_len = 0;
    *nentries = 0;
    *nextstartindex = -1;

    code = Init_VLdbase(&ctx, LOCKREAD, this_op);
    if (code)
	return code;

    Vldbentry = VldbentryFirst = vldbentries->nbulkentries_val =
	malloc(maxCount * sizeof(nvldbentry));
    if (Vldbentry == NULL) {
	countAbort(this_op);
	ubik_AbortTrans(ctx.trans);
	return VL_NOMEM;
    }

    VldbentryLast = VldbentryFirst + maxCount;

    /* Handle the attribute by volume id totally separate of the rest
     * (thus additional Mask values are ignored if VLLIST_VOLUMEID is set!)
     */
    if (attributes->Mask & VLLIST_VOLUMEID) {
	blockindex =
	    FindByID(&ctx, attributes->volumeid, -1, &tentry, &code);
	if (blockindex == 0) {
	    if (!code)
		code = VL_NOENT;
	} else {
	    code =
		put_nattributeentry(&ctx, &Vldbentry, &VldbentryFirst,
				    &VldbentryLast, vldbentries, &tentry, 0,
				    0, nentries, &maxCount);
	    if (code)
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
		IpAddrToRelAddr(&ctx, attributes->server, 0);
	    if (serverindex == -1)
		goto done;
	    findserver = 1;
	}
	findpartition = ((attributes->Mask & VLLIST_PARTITION) ? 1 : 0);
	findflag = ((attributes->Mask & VLLIST_FLAG) ? 1 : 0);
	if (name && (strcmp(name, ".*") != 0) && (strcmp(name, "") != 0)) {
	    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL)) {
		code = VL_PERM;
		goto done;
	    }
	    size = snprintf(volumename, sizeof(volumename), "^%s$", name);
	    if (size < 0 || size >= sizeof(volumename)) {
		code = VL_BADNAME;
		goto done;
	    }
#ifdef HAVE_POSIX_REGEX
	    if (regcomp(&re, volumename, REG_NOSUB) != 0) {
		code = VL_BADNAME;
		goto done;
	    }
	    need_regfree = 1;
#else
	    t = (char *)re_comp(volumename);
	    if (t) {
		code = VL_BADNAME;
		goto done;
	    }
#endif
	    findname = 1;
	}

	/* Read each entry and see if it is the one we want */
	blockindex = startindex;
	while ((blockindex = NextEntry(&ctx, blockindex, &tentry, &count))) {
	    if (++pollcount > 50) {
#ifndef AFS_PTHREAD_ENV
		IOMGR_Poll();
#endif
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
			    size = snprintf(volumename, sizeof(volumename),
					    "%s", tentry.name);
			    if (size < 0 || size >= sizeof(volumename)) {
				code = VL_BADNAME;
				goto done;
			    }
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
			    /* If this fails, the tentry.name is invalid */
			    size = snprintf(volumename, sizeof(volumename),
					    "%s.backup", tentry.name);
			    if (size < 0 || size >= sizeof(volumename)) {
				code = VL_BADNAME;
				goto done;
			    }
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
		    if ((tentry.serverFlags[k] & VLSF_ROVOL) != 0) {
			if (findname) {
			    if (namematchRO) {
				thismatch =
				    ((namematchRO == 1) ? VLSF_ROVOL : 0);
			    } else {
				/* If this fails, the tentry.name is invalid */
				size = snprintf(volumename, sizeof(volumename),
						"%s.readonly", tentry.name);
				if (size < 0 || size >= sizeof(volumename)) {
				    code = VL_BADNAME;
				    goto done;
				}
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
		code =
		    put_nattributeentry(&ctx, &Vldbentry, &VldbentryFirst,
					&VldbentryLast, vldbentries, &tentry,
					matchtype, matchindex, nentries,
					&maxCount);
		if (code)
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

    if (code) {
	countAbort(this_op);
	ubik_AbortTrans(ctx.trans);
	if (vldbentries->nbulkentries_val)
	    free(vldbentries->nbulkentries_val);
	vldbentries->nbulkentries_val = 0;
	vldbentries->nbulkentries_len = 0;
	*nextstartindex = -1;
    } else {
	VLog(5,
	     ("N2ListAttrs nentries=%d %s\n", vldbentries->nbulkentries_len,
	      rxinfo(rxstr, rxcall)));
	code = ubik_EndTrans(ctx.trans);
    }

    return code;
}

afs_int32
SVL_ListAttributesN2(struct rx_call *rxcall,
		     struct VldbListByAttributes *attributes,
		     char *name,		/* Wildcarded volume name */
		     afs_int32 startindex,
		     afs_int32 *nentries,
		     nbulkentries *vldbentries,
		     afs_int32 *nextstartindex)
{
    afs_int32 code;

    code = ListAttributesN2(rxcall, attributes, name, startindex,
			    nentries, vldbentries, nextstartindex);
    osi_auditU(rxcall, VLListAttributesN2Event, code, AUD_END);
    return code;
}

/* Retrieves in vldbentries all vldb entries that match the specified
 * attributes (by server number, partition, volume type, and flag); if
 * volume id is specified then the associated list for that entry is
 * returned. CAUTION: This could be a very expensive call since in most
 * cases sequential search of all vldb entries is performed.
 */
static afs_int32
LinkedList(struct rx_call *rxcall,
	   struct VldbListByAttributes *attributes,
	   afs_int32 *nentries,
	   vldb_list *vldbentries)
{
    int this_op = VLLINKEDLIST;
    int code;
    struct vl_ctx ctx;
    struct nvlentry tentry;
    vldblist vllist, *vllistptr;
    afs_int32 blockindex, count, match;
    afs_int32 k = 0;
    int serverindex;
    int pollcount = 0;

    countRequest(this_op);

    if (!afsconf_CheckRestrictedQuery(vldb_confdir, rxcall,
				      restrictedQueryLevel))
	return VL_PERM;

    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;

    *nentries = 0;
    vldbentries->node = NULL;
    vllistptr = &vldbentries->node;

    /* List by volumeid */
    if (attributes->Mask & VLLIST_VOLUMEID) {
	blockindex =
	    FindByID(&ctx, attributes->volumeid, -1, &tentry, &code);
	if (!blockindex) {
	    if (!code)
		code = VL_NOENT;
	    goto abort;
	}

	vllist = malloc(sizeof(single_vldbentry));
	if (vllist == NULL) {
	    code = VL_NOMEM;
	    goto abort;
	}
	code = vlentry_to_vldbentry(&ctx, &tentry, &vllist->VldbEntry);
	if (code)
	    goto abort;

	vllist->next_vldb = NULL;

	*vllistptr = vllist;	/* Thread onto list */
	vllistptr = &vllist->next_vldb;
	(*nentries)++;
    }

    /* Search by server, partition, and flags */
    else {
	for (blockindex = NextEntry(&ctx, 0, &tentry, &count); blockindex;
	     blockindex = NextEntry(&ctx, blockindex, &tentry, &count)) {
	    match = 0;

	    if (++pollcount > 50) {
#ifndef AFS_PTHREAD_ENV
		IOMGR_Poll();
#endif
		pollcount = 0;
	    }

	    /* Does this volume exist on the desired server */
	    if (attributes->Mask & VLLIST_SERVER) {
		serverindex =
		    IpAddrToRelAddr(&ctx, attributes->server, 0);
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

	    vllist = malloc(sizeof(single_vldbentry));
	    if (vllist == NULL) {
		code = VL_NOMEM;
		goto abort;
	    }
	    code = vlentry_to_vldbentry(&ctx, &tentry, &vllist->VldbEntry);
	    if (code)
		goto abort;

	    vllist->next_vldb = NULL;

	    *vllistptr = vllist;	/* Thread onto list */
	    vllistptr = &vllist->next_vldb;
	    (*nentries)++;
	    if (smallMem && (*nentries >= VLDBALLOCCOUNT)) {
		code = VL_SIZEEXCEEDED;
		goto abort;
	    }
	}
    }
    *vllistptr = NULL;
    return ubik_EndTrans(ctx.trans);

abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_LinkedList(struct rx_call *rxcall,
	       struct VldbListByAttributes *attributes,
	       afs_int32 *nentries,
	       vldb_list *vldbentries)
{
    afs_int32 code;

    code = LinkedList(rxcall, attributes, nentries, vldbentries);
    osi_auditU(rxcall, VLLinkedListEvent, code, AUD_END);
    return code;
}

static afs_int32
LinkedListN(struct rx_call *rxcall,
	    struct VldbListByAttributes *attributes,
	    afs_int32 *nentries,
	    nvldb_list *vldbentries)
{
    int this_op = VLLINKEDLISTN;
    int code;
    struct vl_ctx ctx;
    struct nvlentry tentry;
    nvldblist vllist, *vllistptr;
    afs_int32 blockindex, count, match;
    afs_int32 k = 0;
    int serverindex;
    int pollcount = 0;

    countRequest(this_op);

    if (!afsconf_CheckRestrictedQuery(vldb_confdir, rxcall,
				      restrictedQueryLevel))
	return VL_PERM;

    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;

    *nentries = 0;
    vldbentries->node = NULL;
    vllistptr = &vldbentries->node;

    /* List by volumeid */
    if (attributes->Mask & VLLIST_VOLUMEID) {
	blockindex =
	    FindByID(&ctx, attributes->volumeid, -1, &tentry, &code);
	if (!blockindex) {
	    if (!code)
		code = VL_NOENT;
	    goto abort;
	}

	vllist = malloc(sizeof(single_nvldbentry));
	if (vllist == NULL) {
	    code = VL_NOMEM;
	    goto abort;
	}
	code = vlentry_to_nvldbentry(&ctx, &tentry, &vllist->VldbEntry);
	if (code)
	    goto abort;

	vllist->next_vldb = NULL;

	*vllistptr = vllist;	/* Thread onto list */
	vllistptr = &vllist->next_vldb;
	(*nentries)++;
    }

    /* Search by server, partition, and flags */
    else {
	for (blockindex = NextEntry(&ctx, 0, &tentry, &count); blockindex;
	     blockindex = NextEntry(&ctx, blockindex, &tentry, &count)) {
	    match = 0;

	    if (++pollcount > 50) {
#ifndef AFS_PTHREAD_ENV
		IOMGR_Poll();
#endif
		pollcount = 0;
	    }

	    /* Does this volume exist on the desired server */
	    if (attributes->Mask & VLLIST_SERVER) {
		serverindex =
		    IpAddrToRelAddr(&ctx, attributes->server, 0);
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

	    vllist = malloc(sizeof(single_nvldbentry));
	    if (vllist == NULL) {
		code = VL_NOMEM;
		goto abort;
	    }
	    code = vlentry_to_nvldbentry(&ctx, &tentry, &vllist->VldbEntry);
	    if (code)
		goto abort;

	    vllist->next_vldb = NULL;

	    *vllistptr = vllist;	/* Thread onto list */
	    vllistptr = &vllist->next_vldb;
	    (*nentries)++;
	    if (smallMem && (*nentries >= VLDBALLOCCOUNT)) {
		code = VL_SIZEEXCEEDED;
		goto abort;
	    }
	}
    }
    *vllistptr = NULL;
    return ubik_EndTrans(ctx.trans);

abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_LinkedListN(struct rx_call *rxcall,
		struct VldbListByAttributes *attributes,
		afs_int32 *nentries,
		nvldb_list *vldbentries)
{
    afs_int32 code;

    code = LinkedListN(rxcall, attributes, nentries, vldbentries);
    osi_auditU(rxcall, VLLinkedListNEvent, code, AUD_END);
    return code;
}

/* Get back vldb header statistics (allocs, frees, maxvolumeid,
 * totalentries, etc) and dynamic statistics (number of requests and/or
 * aborts per remote procedure call, etc)
 */
static afs_int32
GetStats(struct rx_call *rxcall,
	 vldstats *stats,
	 vital_vlheader *vital_header)
{
    int this_op = VLGETSTATS;
    afs_int32 code;
    struct vl_ctx ctx;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);

    if (!afsconf_CheckRestrictedQuery(vldb_confdir, rxcall,
				      restrictedQueryLevel))
	return VL_PERM;

    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;
    VLog(5, ("GetStats %s\n", rxinfo(rxstr, rxcall)));
    memcpy((char *)vital_header, (char *)&ctx.cheader->vital_header,
	   sizeof(vital_vlheader));
    memcpy((char *)stats, (char *)&dynamic_statistics, sizeof(vldstats));
    return ubik_EndTrans(ctx.trans);
}

afs_int32
SVL_GetStats(struct rx_call *rxcall,
	     vldstats *stats,
	     vital_vlheader *vital_header)
{
    afs_int32 code;

    code = GetStats(rxcall, stats, vital_header);
    osi_auditU(rxcall, VLGetStatsEvent, code, AUD_END);
    return code;
}

/* Get the list of file server addresses from the VLDB.  Currently it's pretty
 * easy to do.  In the future, it might require a little bit of grunging
 * through the VLDB, but that's life.
 */
afs_int32
SVL_GetAddrs(struct rx_call *rxcall,
	     afs_int32 Handle,
	     afs_int32 spare2,
	     struct VLCallBack *spare3,
	     afs_int32 *nentries,
	     bulkaddrs *addrsp)
{
    int this_op = VLGETADDRS;
    afs_int32 code;
    struct vl_ctx ctx;
    int nservers, i;
    afs_uint32 *taddrp;

    countRequest(this_op);
    addrsp->bulkaddrs_len = *nentries = 0;
    addrsp->bulkaddrs_val = 0;
    memset(spare3, 0, sizeof(struct VLCallBack));

    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;

    VLog(5, ("GetAddrs\n"));
    addrsp->bulkaddrs_val = taddrp =
	malloc(sizeof(afs_uint32) * (MAXSERVERID + 1));
    nservers = *nentries = addrsp->bulkaddrs_len = 0;

    if (!taddrp) {
	code = VL_NOMEM;
	goto abort;
    }

    for (i = 0; i <= MAXSERVERID; i++) {
	if ((*taddrp = ntohl(ctx.cheader->IpMappedAddr[i]))) {
	    taddrp++;
	    nservers++;
	}
    }

    addrsp->bulkaddrs_len = *nentries = nservers;
    return (ubik_EndTrans(ctx.trans));

abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

static_inline void
append_addr(char *buffer, afs_uint32 addr, size_t buffer_size)
{
    int n = strlen(buffer);
    if (buffer_size > n) {
	snprintf(buffer + n, buffer_size - n, "%u.%u.%u.%u",
		 (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
		 addr & 0xff);
    }
}

afs_int32
SVL_RegisterAddrs(struct rx_call *rxcall, afsUUID *uuidp, afs_int32 spare1,
		  bulkaddrs *addrsp)
{
    int this_op = VLREGADDR;
    afs_int32 code;
    struct vl_ctx ctx;
    int cnt, h, i, j, k, m;
    struct extentaddr *exp = 0, *tex;
    char addrbuf[256];
    afsUUID tuuid;
    afs_uint32 addrs[VL_MAXIPADDRS_PERMH];
    int base;
    int count, willChangeEntry, foundUuidEntry, willReplaceCnt;
    int WillReplaceEntry, WillChange[MAXSERVERID + 1];
    int FoundUuid = 0;
    int ReplaceEntry = 0;
    int srvidx, mhidx;

    countRequest(this_op);
    if (!afsconf_SuperUser(vldb_confdir, rxcall, NULL))
	return (VL_PERM);
    if ((code = Init_VLdbase(&ctx, LOCKWRITE, this_op)))
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
	code = VL_INDEXERANGE;
	goto abort;
    }

    count = 0;
    willReplaceCnt = 0;
    foundUuidEntry = 0;
    /* For each server registered within the VLDB */
    for (srvidx = 0; srvidx <= MAXSERVERID; srvidx++) {
	willChangeEntry = 0;
	WillReplaceEntry = 1;
	code = multiHomedExtent(&ctx, srvidx, &exp);
	if (code)
	     continue;

	if (exp) {
	    /* See if the addresses to register will change this server entry */
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
		if (ctx.hostaddress[srvidx] == addrs[k]) {
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
	for (addrbuf[0] = '\0', k = 0; k < cnt; k++) {
	    if (k > 0)
		strlcat(addrbuf, " ", sizeof(addrbuf));
	    append_addr(addrbuf, addrs[k], sizeof(addrbuf));
	}
	VLog(0, ("      [%s]\n", addrbuf));

	if (foundUuidEntry) {
	    code = multiHomedExtent(&ctx, FoundUuid, &exp);
	    if (code == 0) {
	        VLog(0, ("   It would have replaced the existing VLDB server "
			 "entry:\n"));
	        for (addrbuf[0] = '\0', mhidx = 0; mhidx < VL_MAXIPADDRS_PERMH; mhidx++) {
		    if (!exp->ex_addrs[mhidx])
			continue;
		    if (mhidx > 0)
			strlcat(addrbuf, " ", sizeof(addrbuf));
		    append_addr(addrbuf, ntohl(exp->ex_addrs[mhidx]), sizeof(addrbuf));
		}
		VLog(0, ("      entry %d: [%s]\n", FoundUuid, addrbuf));
	    }
	}

	if (count == 1)
	    VLog(0, ("   Yet another VLDB server entry exists:\n"));
	else
	    VLog(0, ("   Yet other VLDB server entries exist:\n"));
	for (j = 0; j < count; j++) {
	    srvidx = WillChange[j];
	    VLog(0, ("      entry %d: ", srvidx));

	    code = multiHomedExtent(&ctx, srvidx, &exp);
	    if (code)
		goto abort;

	    addrbuf[0] = '\0';
	    if (exp) {
		for (mhidx = 0; mhidx < VL_MAXIPADDRS_PERMH; mhidx++) {
		    if (!exp->ex_addrs[mhidx])
			continue;
		    if (mhidx > 0)
			strlcat(addrbuf, " ", sizeof(addrbuf));
		    append_addr(addrbuf, ntohl(exp->ex_addrs[mhidx]), sizeof(addrbuf));
		}
	    } else {
		append_addr(addrbuf, ctx.hostaddress[srvidx], sizeof(addrbuf));
	    }
	    VLog(0, ("      entry %d: [%s]\n", srvidx, addrbuf));
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

	code = VL_MULTIPADDR;
	goto abort;
    }

    /* Passed the checks. Now find and update the existing mh entry, or create
     * a new mh entry.
     */
    if (foundUuidEntry) {
	/* Found the entry with same uuid. See if we need to change it */
	int change = 0;

	code = multiHomedExtentBase(&ctx, FoundUuid, &exp, &base);
	if (code)
	    goto abort;

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
	    return (ubik_EndTrans(ctx.trans));
	}
    }

    VLog(0, ("The following fileserver is being registered in the VLDB:\n"));
    for (addrbuf[0] = '\0', k = 0; k < cnt; k++) {
	if (k > 0)
	    strlcat(addrbuf, " ", sizeof(addrbuf));
	append_addr(addrbuf, addrs[k], sizeof(addrbuf));
    }
    VLog(0, ("      [%s]\n", addrbuf));

    if (foundUuidEntry) {
	VLog(0,
	    ("   It will replace the following existing entry in the VLDB (same uuid):\n"));
	for (addrbuf[0] = '\0', k = 0; k < VL_MAXIPADDRS_PERMH; k++) {
	    if (exp->ex_addrs[k] == 0)
		continue;
	    if (k > 0)
		strlcat(addrbuf, " ", sizeof(addrbuf));
	    append_addr(addrbuf, ntohl(exp->ex_addrs[k]), sizeof(addrbuf));
	}
	VLog(0, ("      entry %d: [%s]\n", FoundUuid, addrbuf));
    } else if (willReplaceCnt || (count == 1)) {
	/* If we are not replacing an entry and there is only one entry to change,
	 * then we will replace that entry.
	 */
	if (!willReplaceCnt) {
	    ReplaceEntry = WillChange[0];
	    willReplaceCnt++;
	}

	/* Have an entry that needs to be replaced */
	code = multiHomedExtentBase(&ctx, ReplaceEntry, &exp, &base);
	if (code)
	    goto abort;

	if (exp) {
	    VLog(0,
		("   It will replace the following existing entry in the VLDB (new uuid):\n"));
	    for (addrbuf[0] = '\0', k = 0; k < VL_MAXIPADDRS_PERMH; k++) {
		if (exp->ex_addrs[k] == 0)
		    continue;
		if (k > 0)
		    strlcat(addrbuf, " ", sizeof(addrbuf));
		append_addr(addrbuf, ntohl(exp->ex_addrs[k]), sizeof(addrbuf));
	    }
	    VLog(0, ("      entry %d: [%s]\n", ReplaceEntry, addrbuf));
	} else {
	    /* Not a mh entry. So we have to create a new mh entry and
	     * put it on the ReplaceEntry slot of the ctx.hostaddress array.
	     */
	    addrbuf[0] = '\0';
	    append_addr(addrbuf, ctx.hostaddress[ReplaceEntry], sizeof(addrbuf));
	    VLog(0, ("   It will replace existing entry %d, %s,"
		     " in the VLDB (new uuid):\n", ReplaceEntry, addrbuf));
	    code =
		FindExtentBlock(&ctx, uuidp, 1, ReplaceEntry, &exp, &base);
	    if (code || !exp) {
		if (!code)
		    code = VL_IO;
		goto abort;
	    }
	}
    } else {
	/* There is no entry for this server, must create a new mh entry as
	 * well as use a new slot of the ctx.hostaddress array.
	 */
	VLog(0, ("   It will create a new entry in the VLDB.\n"));
	code = FindExtentBlock(&ctx, uuidp, 1, -1, &exp, &base);
	if (code || !exp) {
	    if (!code)
		code = VL_IO;
	    goto abort;
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
	(ctx.trans,
	 DOFFSET(ntohl(ctx.ex_addr[0]->ex_contaddrs[base]),
		 (char *)ctx.ex_addr[base], (char *)exp), (char *)exp,
	 sizeof(*exp))) {
	code = VL_IO;
	goto abort;
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

	code = multiHomedExtentBase(&ctx, WillChange[i], &tex, &base);
	if (code)
	    goto abort;

	if (++m == 1)
	    VLog(0,
		("   The following existing entries in the VLDB will be updated:\n"));

	for (addrbuf[0] = '\0', h = j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
	    if (tex->ex_addrs[j]) {
		if (j > 0)
		    strlcat(addrbuf, " ", sizeof(addrbuf));
		append_addr(addrbuf, ntohl(tex->ex_addrs[j]), sizeof(addrbuf));
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
	VLog(0, ("      entry %d: [%s]\n", WillChange[i], addrbuf));

	/* Write out the modified mh entry */
	tex->ex_uniquifier = htonl(ntohl(tex->ex_uniquifier) + 1);
	doff =
	    DOFFSET(ntohl(ctx.ex_addr[0]->ex_contaddrs[base]),
		    (char *)ctx.ex_addr[base], (char *)tex);
	if (vlwrite(ctx.trans, doff, (char *)tex, sizeof(*tex))) {
	    code = VL_IO;
	    goto abort;
	}
    }

    return (ubik_EndTrans(ctx.trans));

abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

afs_int32
SVL_GetAddrsU(struct rx_call *rxcall,
	      struct ListAddrByAttributes *attributes,
	      afsUUID *uuidpo,
	      afs_int32 *uniquifier,
	      afs_int32 *nentries,
	      bulkaddrs *addrsp)
{
    int this_op = VLGETADDRSU;
    afs_int32 code, index;
    struct vl_ctx ctx;
    int nservers, i, j, base = 0;
    struct extentaddr *exp = 0;
    afsUUID tuuid;
    afs_uint32 *taddrp, taddr;
    char rxstr[AFS_RXINFO_LEN];

    countRequest(this_op);
    addrsp->bulkaddrs_len = *nentries = 0;
    addrsp->bulkaddrs_val = 0;
    VLog(5, ("GetAddrsU %s\n", rxinfo(rxstr, rxcall)));
    if ((code = Init_VLdbase(&ctx, LOCKREAD, this_op)))
	return code;

    if (attributes->Mask & VLADDR_IPADDR) {
	if (attributes->Mask & (VLADDR_INDEX | VLADDR_UUID)) {
	    code = VL_BADMASK;
	    goto abort;
	}
	/* Search for a server registered with the VLDB with this ip address. */
	for (index = 0; index <= MAXSERVERID; index++) {
	    code = multiHomedExtent(&ctx, index, &exp);
	    if (code)
		continue;

	    if (exp) {
		for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
		    if (exp->ex_addrs[j]
			&& (ntohl(exp->ex_addrs[j]) == attributes->ipaddr)) {
			break;
		    }
		}
		if (j < VL_MAXIPADDRS_PERMH)
		    break;
	    }
	}
	if (index > MAXSERVERID) {
	    code = VL_NOENT;
	    goto abort;
	}
    } else if (attributes->Mask & VLADDR_INDEX) {
	if (attributes->Mask & (VLADDR_IPADDR | VLADDR_UUID)) {
	    code = VL_BADMASK;
	    goto abort;
	}
	/* VLADDR_INDEX index is one based */
	if (attributes->index < 1 || attributes->index > MAXSERVERID) {
	    code = VL_INDEXERANGE;
	    goto abort;
	}
	index = attributes->index - 1;
	code = multiHomedExtent(&ctx, index, &exp);
	if (code) {
	    code = VL_NOENT;
	    goto abort;
	}
    } else if (attributes->Mask & VLADDR_UUID) {
	if (attributes->Mask & (VLADDR_IPADDR | VLADDR_INDEX)) {
	    code = VL_BADMASK;
	    goto abort;
	}
	if (!ctx.ex_addr[0]) {	/* mh servers probably aren't setup on this vldb */
	    code = VL_NOENT;
	    goto abort;
	}
	code = FindExtentBlock(&ctx, &attributes->uuid, 0, -1, &exp, &base);
	if (code)
	    goto abort;
    } else {
	code = VL_BADMASK;
	goto abort;
    }

    if (exp == NULL) {
	code = VL_NOENT;
	goto abort;
    }
    addrsp->bulkaddrs_val = taddrp =
	malloc(sizeof(afs_uint32) * (MAXSERVERID + 1));
    nservers = *nentries = addrsp->bulkaddrs_len = 0;
    if (!taddrp) {
	code = VL_NOMEM;
	goto abort;
    }
    tuuid = exp->ex_hostuuid;
    afs_ntohuuid(&tuuid);
    if (afs_uuid_is_nil(&tuuid)) {
	code = VL_NOENT;
	goto abort;
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
    return (ubik_EndTrans(ctx.trans));

abort:
    countAbort(this_op);
    ubik_AbortTrans(ctx.trans);
    return code;
}

/* ============> End of Exported vldb RPC functions <============= */


/* Routine that copies the given vldb entry to the output buffer, vldbentries. */
static int
put_attributeentry(struct vl_ctx *ctx,
		   struct vldbentry **Vldbentry,
		   struct vldbentry **VldbentryFirst,
		   struct vldbentry **VldbentryLast,
		   bulkentries *vldbentries,
		   struct nvlentry *entry,
		   afs_int32 *nentries,
		   afs_int32 *alloccnt)
{
    vldbentry *reall;
    afs_int32 allo;
    int code;

    if (*Vldbentry == *VldbentryLast) {
	if (smallMem)
	    return VL_SIZEEXCEEDED;	/* no growing if smallMem defined */

	/* Allocate another set of memory; each time allocate twice as
	 * many blocks as the last time. When we reach VLDBALLOCLIMIT,
	 * then grow in increments of VLDBALLOCINCR.
	 */
	allo = (*alloccnt > VLDBALLOCLIMIT) ? VLDBALLOCINCR : *alloccnt;
	reall = realloc(*VldbentryFirst,
			(*alloccnt + allo) * sizeof(vldbentry));
	if (reall == NULL)
	    return VL_NOMEM;

	*VldbentryFirst = vldbentries->bulkentries_val = reall;
	*Vldbentry = *VldbentryFirst + *alloccnt;
	*VldbentryLast = *Vldbentry + allo;
	*alloccnt += allo;
    }

    code = vlentry_to_vldbentry(ctx, entry, *Vldbentry);
    if (code)
	return code;

    (*Vldbentry)++;
    (*nentries)++;
    vldbentries->bulkentries_len++;
    return 0;
}

static int
put_nattributeentry(struct vl_ctx *ctx,
		    struct nvldbentry **Vldbentry,
		    struct nvldbentry **VldbentryFirst,
		    struct nvldbentry **VldbentryLast,
		    nbulkentries *vldbentries,
		    struct nvlentry *entry,
		    afs_int32 matchtype,
		    afs_int32 matchindex,
		    afs_int32 *nentries,
		    afs_int32 *alloccnt)
{
    nvldbentry *reall;
    afs_int32 allo;
    int code;

    if (*Vldbentry == *VldbentryLast) {
	if (smallMem)
	    return VL_SIZEEXCEEDED;	/* no growing if smallMem defined */

	/* Allocate another set of memory; each time allocate twice as
	 * many blocks as the last time. When we reach VLDBALLOCLIMIT,
	 * then grow in increments of VLDBALLOCINCR.
	 */
	allo = (*alloccnt > VLDBALLOCLIMIT) ? VLDBALLOCINCR : *alloccnt;
	reall = realloc(*VldbentryFirst,
			(*alloccnt + allo) * sizeof(nvldbentry));
	if (reall == NULL)
	    return VL_NOMEM;

	*VldbentryFirst = vldbentries->nbulkentries_val = reall;
	*Vldbentry = *VldbentryFirst + *alloccnt;
	*VldbentryLast = *Vldbentry + allo;
	*alloccnt += allo;
    }
    code = vlentry_to_nvldbentry(ctx, entry, *Vldbentry);
    if (code)
	return code;

    (*Vldbentry)->matchindex = (matchtype << 16) + matchindex;
    (*Vldbentry)++;
    (*nentries)++;
    vldbentries->nbulkentries_len++;
    return 0;
}


/* Common code to actually remove a vldb entry from the database. */
static int
RemoveEntry(struct vl_ctx *ctx, afs_int32 entryptr,
	    struct nvlentry *tentry)
{
    int code;

    if ((code = UnthreadVLentry(ctx, entryptr, tentry)))
	return code;
    if ((code = FreeBlock(ctx, entryptr)))
	return code;
    return 0;
}

static void
ReleaseEntry(struct nvlentry *tentry, afs_int32 releasetype)
{
    if (releasetype & LOCKREL_TIMESTAMP)
	tentry->LockTimestamp = 0;
    if (releasetype & LOCKREL_OPCODE)
	tentry->flags &= ~VLOP_ALLOPERS;
    if (releasetype & LOCKREL_AFSID)
	tentry->LockAfsId = 0;
}


/* Verify that the incoming vldb entry is valid; multi type of error codes
 * are returned. */
static int
check_vldbentry(struct vldbentry *aentry)
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
check_nvldbentry(struct nvldbentry *aentry)
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
vldbentry_to_vlentry(struct vl_ctx *ctx,
		     struct vldbentry *VldbEntry,
		     struct nvlentry *VlEntry)
{
    int i, serverindex;

    if (strcmp(VlEntry->name, VldbEntry->name))
	strncpy(VlEntry->name, VldbEntry->name, sizeof(VlEntry->name));
    for (i = 0; i < VldbEntry->nServers; i++) {
	serverindex = IpAddrToRelAddr(ctx, VldbEntry->serverNumber[i], 1);
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

static int
nvldbentry_to_vlentry(struct vl_ctx *ctx,
		      struct nvldbentry *VldbEntry,
		      struct nvlentry *VlEntry)
{
    int i, serverindex;

    if (strcmp(VlEntry->name, VldbEntry->name))
	strncpy(VlEntry->name, VldbEntry->name, sizeof(VlEntry->name));
    for (i = 0; i < VldbEntry->nServers; i++) {
	serverindex = IpAddrToRelAddr(ctx, VldbEntry->serverNumber[i], 1);
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


/* Update the vldb entry with the new fields as indicated by the value of
 * the Mask entry in the updateentry structure. All necessary validation
 * checks are performed.
 */
static int
get_vldbupdateentry(struct vl_ctx *ctx,
		    afs_int32 blockindex,
		    struct VldbUpdateEntry *updateentry,
		    struct nvlentry *VlEntry)
{
    int i, j, code, serverindex;
    afs_uint32 checkids[MAXTYPES];

    /* check if any specified new IDs are already present in the db. Do
     * this check before doing anything else, so we don't get a half-
     * updated entry. */
    memset(&checkids, 0, sizeof(checkids));
    if (updateentry->Mask & VLUPDATE_RWID) {
	checkids[RWVOL] = updateentry->spares3;	/* rw id */
    }
    if (updateentry->Mask & VLUPDATE_READONLYID) {
	checkids[ROVOL] = updateentry->ReadOnlyId;
    }
    if (updateentry->Mask & VLUPDATE_BACKUPID) {
	checkids[BACKVOL] = updateentry->BackupId;
    }

    if (EntryIDExists(ctx, checkids, MAXTYPES, &code)) {
	return VL_IDEXIST;
    } else if (code) {
	return code;
    }

    if (updateentry->Mask & VLUPDATE_VOLUMENAME) {
	struct nvlentry tentry;

	if (InvalidVolname(updateentry->name))
	    return VL_BADNAME;

	if (FindByName(ctx, updateentry->name, &tentry, &code)) {
	    return VL_NAMEEXIST;
	} else if (code) {
	    return code;
	}

	if ((code = UnhashVolname(ctx, blockindex, VlEntry)))
	    return code;
	strncpy(VlEntry->name, updateentry->name, sizeof(VlEntry->name));
	HashVolname(ctx, blockindex, VlEntry);
    }

    if (updateentry->Mask & VLUPDATE_VOLNAMEHASH) {
	if ((code = UnhashVolname(ctx, blockindex, VlEntry))) {
	    if (code != VL_NOENT)
		return code;
	}
	HashVolname(ctx, blockindex, VlEntry);
    }

    if (updateentry->Mask & VLUPDATE_FLAGS) {
	VlEntry->flags = updateentry->flags;
    }
    if (updateentry->Mask & VLUPDATE_CLONEID) {
	VlEntry->cloneId = updateentry->cloneId;
    }
    if (updateentry->Mask & VLUPDATE_RWID) {
	if ((code = UnhashVolid(ctx, RWVOL, blockindex, VlEntry))) {
	    if (code != VL_NOENT)
		return code;
	}
	VlEntry->volumeId[RWVOL] = updateentry->spares3;	/* rw id */
	if ((code = HashVolid(ctx, RWVOL, blockindex, VlEntry)))
	    return code;
    }
    if (updateentry->Mask & VLUPDATE_READONLYID) {
	if ((code = UnhashVolid(ctx, ROVOL, blockindex, VlEntry))) {
	    if (code != VL_NOENT)
		return code;
	}
	VlEntry->volumeId[ROVOL] = updateentry->ReadOnlyId;
	if ((code = HashVolid(ctx, ROVOL, blockindex, VlEntry)))
	    return code;
    }
    if (updateentry->Mask & VLUPDATE_BACKUPID) {
	if ((code = UnhashVolid(ctx, BACKVOL, blockindex, VlEntry))) {
	    if (code != VL_NOENT)
		return code;
	}
	VlEntry->volumeId[BACKVOL] = updateentry->BackupId;
	if ((code = HashVolid(ctx, BACKVOL, blockindex, VlEntry)))
	    return code;
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
				    IpAddrToRelAddr(ctx, updateentry->
						    RepsitesTargetServer[i],
						    1),
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
		     IpAddrToRelAddr(ctx, updateentry->RepsitesNewServer[i], 1),
		     updateentry->RepsitesNewPart[i]) != -1)
		    return VL_DUPREPSERVER;
		for (j = 0;
		     VlEntry->serverNumber[j] != BADSERVERID
		     && j < OMAXNSERVERS; j++);
		if (j >= OMAXNSERVERS)
		    return VL_REPSFULL;
		if ((serverindex =
		     IpAddrToRelAddr(ctx, updateentry->RepsitesNewServer[i],
				     1)) == -1)
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
				    IpAddrToRelAddr(ctx, updateentry->
						    RepsitesTargetServer[i],
						    1),
				    updateentry->RepsitesTargetPart[i])) !=
		    -1) {
		    VlEntry->serverNumber[j] =
			IpAddrToRelAddr(ctx, updateentry->RepsitesNewServer[i],
					1);
		} else
		    return VL_NOREPSERVER;
	    }
	    if (updateentry->RepsitesMask[i] & VLUPDATE_REPS_MODPART) {
		if (updateentry->RepsitesNewPart[i] < 0
		    || updateentry->RepsitesNewPart[i] > MAXPARTITIONID)
		    return VL_BADPARTITION;
		if ((j =
		     repsite_exists(VlEntry,
				    IpAddrToRelAddr(ctx, updateentry->
						    RepsitesTargetServer[i],
						    1),
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
				    IpAddrToRelAddr(ctx, updateentry->
						    RepsitesTargetServer[i],
						    1),
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


/* Check if the specified [server,partition] entry is found in the vldb
 * entry's repsite table; it's offset in the table is returned, if it's
 * present there. */
static int
repsite_exists(struct nvlentry *VlEntry, int server, int partition)
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



/* Repsite table compression: used when deleting a repsite entry so that
 * all active repsite entries are on the top of the table. */
static void
repsite_compress(struct nvlentry *VlEntry, int offset)
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


/* Convert from the internal (compacted) vldb entry to the external
 * representation used by the interface. */
static int
vlentry_to_vldbentry(struct vl_ctx *ctx, struct nvlentry *VlEntry,
                     struct vldbentry *VldbEntry)
{
    int i, j, code;
    struct extentaddr *exp;

    memset(VldbEntry, 0, sizeof(struct vldbentry));
    strncpy(VldbEntry->name, VlEntry->name, sizeof(VldbEntry->name));
    for (i = 0; i < OMAXNSERVERS; i++) {
	if (VlEntry->serverNumber[i] == BADSERVERID)
	    break;
	code = multiHomedExtent(ctx, VlEntry->serverNumber[i], &exp);
	if (code)
	    return code;
	if (exp) {
	    /* For now return the first ip address back */
	    for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
		if (exp->ex_addrs[j]) {
		    VldbEntry->serverNumber[i] = ntohl(exp->ex_addrs[j]);
		    break;
		}
	    }
	} else
	    VldbEntry->serverNumber[i] =
		ctx->hostaddress[VlEntry->serverNumber[i]];
	VldbEntry->serverPartition[i] = VlEntry->serverPartition[i];
	VldbEntry->serverFlags[i] = VlEntry->serverFlags[i];
    }
    VldbEntry->nServers = i;
    for (i = 0; i < MAXTYPES; i++)
	VldbEntry->volumeId[i] = VlEntry->volumeId[i];
    VldbEntry->cloneId = VlEntry->cloneId;
    VldbEntry->flags = VlEntry->flags;

    return 0;
}


/* Convert from the internal (compacted) vldb entry to the external
 * representation used by the interface. */
static int
vlentry_to_nvldbentry(struct vl_ctx *ctx, struct nvlentry *VlEntry,
                      struct nvldbentry *VldbEntry)
{
    int i, j, code;
    struct extentaddr *exp;

    memset(VldbEntry, 0, sizeof(struct nvldbentry));
    strncpy(VldbEntry->name, VlEntry->name, sizeof(VldbEntry->name));
    for (i = 0; i < NMAXNSERVERS; i++) {
	if (VlEntry->serverNumber[i] == BADSERVERID)
	    break;
	code = multiHomedExtent(ctx, VlEntry->serverNumber[i], &exp);
	if (code)
	    return code;

	if (exp) {
	    /* For now return the first ip address back */
	    for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
		if (exp->ex_addrs[j]) {
		    VldbEntry->serverNumber[i] = ntohl(exp->ex_addrs[j]);
		    break;
		}
	    }
	} else
	    VldbEntry->serverNumber[i] =
		ctx->hostaddress[VlEntry->serverNumber[i]];
	VldbEntry->serverPartition[i] = VlEntry->serverPartition[i];
	VldbEntry->serverFlags[i] = VlEntry->serverFlags[i];
    }
    VldbEntry->nServers = i;
    for (i = 0; i < MAXTYPES; i++)
	VldbEntry->volumeId[i] = VlEntry->volumeId[i];
    VldbEntry->cloneId = VlEntry->cloneId;
    VldbEntry->flags = VlEntry->flags;

    return 0;
}

static int
vlentry_to_uvldbentry(struct vl_ctx *ctx, struct nvlentry *VlEntry,
                      struct uvldbentry *VldbEntry)
{
    int i, code;
    struct extentaddr *exp;

    memset(VldbEntry, 0, sizeof(struct uvldbentry));
    strncpy(VldbEntry->name, VlEntry->name, sizeof(VldbEntry->name));
    for (i = 0; i < NMAXNSERVERS; i++) {
	if (VlEntry->serverNumber[i] == BADSERVERID)
	    break;
	VldbEntry->serverFlags[i] = VlEntry->serverFlags[i];
	VldbEntry->serverUnique[i] = 0;
	code = multiHomedExtent(ctx, VlEntry->serverNumber[i], &exp);
	if (code)
	    return code;

	if (exp) {
	    afsUUID tuuid;

	    tuuid = exp->ex_hostuuid;
	    afs_ntohuuid(&tuuid);
	    VldbEntry->serverFlags[i] |= VLSF_UUID;
	    VldbEntry->serverNumber[i] = tuuid;
	    VldbEntry->serverUnique[i] = ntohl(exp->ex_uniquifier);
	} else {
	    VldbEntry->serverNumber[i].time_low =
		ctx->hostaddress[VlEntry->serverNumber[i]];
	}
	VldbEntry->serverPartition[i] = VlEntry->serverPartition[i];

    }
    VldbEntry->nServers = i;
    for (i = 0; i < MAXTYPES; i++)
	VldbEntry->volumeId[i] = VlEntry->volumeId[i];
    VldbEntry->cloneId = VlEntry->cloneId;
    VldbEntry->flags = VlEntry->flags;

    return 0;
}

#define LEGALCHARS ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"


/* Verify that the volname is a valid volume name. */
static int
InvalidVolname(char *volname)
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
InvalidVoltype(afs_int32 voltype)
{
    if (voltype != RWVOL && voltype != ROVOL && voltype != BACKVOL)
	return 1;
    return 0;
}


static int
InvalidOperation(afs_int32 voloper)
{
    if (voloper != VLOP_MOVE && voloper != VLOP_RELEASE
	&& voloper != VLOP_BACKUP && voloper != VLOP_DELETE
	&& voloper != VLOP_DUMP)
	return 1;
    return 0;
}

static int
InvalidReleasetype(afs_int32 releasetype)
{
    if ((releasetype & LOCKREL_TIMESTAMP) || (releasetype & LOCKREL_OPCODE)
	|| (releasetype & LOCKREL_AFSID))
	return 0;
    return 1;
}

static int
IpAddrToRelAddr(struct vl_ctx *ctx, afs_uint32 ipaddr, int create)
{
    int i, j;
    afs_int32 code;
    struct extentaddr *exp;

    for (i = 0; i <= MAXSERVERID; i++) {
	if (ctx->hostaddress[i] == ipaddr)
	    return i;
	code = multiHomedExtent(ctx, i, &exp);
	if (code)
	    return -1;
	if (exp) {
	    for (j = 0; j < VL_MAXIPADDRS_PERMH; j++) {
		if (exp->ex_addrs[j] && (ntohl(exp->ex_addrs[j]) == ipaddr)) {
		    return i;
		}
	    }
	}
    }

    /* allocate the new server a server id pronto */
    if (create) {
	for (i = 0; i <= MAXSERVERID; i++) {
	    if (ctx->cheader->IpMappedAddr[i] == 0) {
		ctx->cheader->IpMappedAddr[i] = htonl(ipaddr);
		code =
		    vlwrite(ctx->trans,
			    DOFFSET(0, ctx->cheader, &ctx->cheader->IpMappedAddr[i]),
			    (char *)&ctx->cheader->IpMappedAddr[i],
			    sizeof(afs_int32));
		ctx->hostaddress[i] = ipaddr;
		if (code)
		    return -1;
		return i;
	    }
	}
    }
    return -1;
}

static int
ChangeIPAddr(struct vl_ctx *ctx, afs_uint32 ipaddr1, afs_uint32 ipaddr2)
{
    int i, j;
    afs_int32 code;
    struct extentaddr *exp = NULL;
    int base = -1;
    int mhidx;
    afsUUID tuuid;
    afs_int32 blockindex, count;
    int pollcount = 0;
    struct nvlentry tentry;
    int ipaddr1_id = -1, ipaddr2_id = -1;
    char addrbuf1[256];
    char addrbuf2[256];

    /* Don't let addr change to 255.*.*.* : Causes internal error below */
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
	struct extentaddr *texp = NULL;
	int tbase;

	code = multiHomedExtentBase(ctx, i, &texp, &tbase);
	if (code)
	    return code;

	if (texp) {
	    for (mhidx = 0; mhidx < VL_MAXIPADDRS_PERMH; mhidx++) {
		if (!texp->ex_addrs[mhidx])
		    continue;
		if (ntohl(texp->ex_addrs[mhidx]) == ipaddr1) {
		    ipaddr1_id = i;
		    exp = texp;
		    base = tbase;
		}
		if (ipaddr2 != 0 && ntohl(texp->ex_addrs[mhidx]) == ipaddr2) {
		    ipaddr2_id = i;
		}
	    }
	} else {
	    if (ctx->hostaddress[i] == ipaddr1) {
		exp = NULL;
		base = -1;
		ipaddr1_id = i;
	    }
	    if (ipaddr2 != 0 && ctx->hostaddress[i] == ipaddr2) {
		ipaddr2_id = i;
	    }
	}

	if (ipaddr1_id >= 0 && (ipaddr2 == 0 || ipaddr2_id >= 0)) {
	    /* we've either found both IPs already in the VLDB, or we found
	     * ipaddr1, and we're not going to find ipaddr2 because it's 0 */
	    break;
	}
    }

    if (ipaddr1_id < 0) {
	return VL_NOENT;	/* not found */
    }

    if (ipaddr2_id >= 0 && ipaddr2_id != ipaddr1_id) {
	char buf1[16], buf2[16];
	VLog(0, ("Cannot change IP address from %s to %s because the latter "
	         "is in use by server id %d\n",
	         afs_inet_ntoa_r(htonl(ipaddr1), buf1),
	         afs_inet_ntoa_r(htonl(ipaddr2), buf2),
	         ipaddr2_id));
	return VL_MULTIPADDR;
    }

    /* If we are removing a server entry, a volume cannot
     * exist on the server. If one does, don't remove the
     * server entry: return error "volume entry exists".
     */
    if (ipaddr2 == 0) {
	for (blockindex = NextEntry(ctx, 0, &tentry, &count); blockindex;
	     blockindex = NextEntry(ctx, blockindex, &tentry, &count)) {
	    if (++pollcount > 50) {
#ifndef AFS_PTHREAD_ENV
		IOMGR_Poll();
#endif
		pollcount = 0;
	    }
	    for (j = 0; j < NMAXNSERVERS; j++) {
		if (tentry.serverNumber[j] == BADSERVERID)
		    break;
		if (tentry.serverNumber[j] == ipaddr1_id) {
		    return VL_IDEXIST;
		}
	    }
	}
    } else if (exp) {
	/* Do not allow changing addresses in multi-homed entries.
	   Older versions of this RPC would silently "downgrade" mh entries
	   to single-homed entries and orphan the mh enties. */
	addrbuf1[0] = '\0';
	append_addr(addrbuf1, ipaddr1, sizeof(addrbuf1));
	VLog(0, ("Refusing to change address %s in multi-homed entry; "
		 "use RegisterAddrs instead.\n", addrbuf1));
	return VL_NOENT;	/* single-homed entry not found */
    }

    /* Log a message saying we are changing/removing an IP address */
    VLog(0,
	 ("The following IP address is being %s:\n",
	  (ipaddr2 ? "changed" : "removed")));
    addrbuf1[0] = addrbuf2[0] = '\0';
    if (exp) {
	for (mhidx = 0; mhidx < VL_MAXIPADDRS_PERMH; mhidx++) {
	    if (!exp->ex_addrs[mhidx])
		continue;
	    if (mhidx > 0)
		strlcat(addrbuf1, " ", sizeof(addrbuf1));
	    append_addr(addrbuf1, ntohl(exp->ex_addrs[mhidx]), sizeof(addrbuf1));
	}
    } else {
	append_addr(addrbuf1, ipaddr1, sizeof(addrbuf1));
    }
    if (ipaddr2) {
	append_addr(addrbuf2, ipaddr2, sizeof(addrbuf2));
    }
    VLog(0, ("      entry %d: [%s] -> [%s]\n", ipaddr1_id, addrbuf1, addrbuf2));

    /* Change the registered uuuid addresses */
    if (exp && base != -1) {
	memset(&tuuid, 0, sizeof(afsUUID));
	afs_htonuuid(&tuuid);
	exp->ex_hostuuid = tuuid;
	code =
	    vlwrite(ctx->trans,
		    DOFFSET(ntohl(ctx->ex_addr[0]->ex_contaddrs[base]),
			    (char *)ctx->ex_addr[base], (char *)exp),
		    (char *)&tuuid, sizeof(tuuid));
	if (code)
	    return VL_IO;
    }

    /* Now change the host address entry */
    ctx->cheader->IpMappedAddr[ipaddr1_id] = htonl(ipaddr2);
    code =
	vlwrite(ctx->trans, DOFFSET(0, ctx->cheader, &ctx->cheader->IpMappedAddr[ipaddr1_id]),
		(char *)
		&ctx->cheader->IpMappedAddr[ipaddr1_id], sizeof(afs_int32));
    ctx->hostaddress[ipaddr1_id] = ipaddr2;
    if (code)
	return VL_IO;

    return 0;
}

/* see if the vlserver is back yet */
afs_int32
SVL_ProbeServer(struct rx_call *rxcall)
{
    return 0;
}
