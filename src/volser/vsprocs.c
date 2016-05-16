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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
#endif
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif

#include <lock.h>
#include <afs/voldefs.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/vlserver.h>
#include <afs/nfs.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/afsint.h>
#include "volser.h"
#include "volint.h"
#include "lockdata.h"
#include <afs/com_err.h>
#include <rx/rxkad.h>
#include <afs/kautils.h>
#include <afs/cmd.h>
#include <afs/ihandle.h>
#ifdef AFS_NT40_ENV
#include <afs/ntops.h>
#endif
#include <afs/vnode.h>
#include <afs/volume.h>
#include <errno.h>
#define ERRCODE_RANGE 8		/* from error_table.h */
#define	CLOCKSKEW   2		/* not really skew, but resolution */
#define CLOCKADJ(x) (((x) < CLOCKSKEW) ? 0 : (x) - CLOCKSKEW)

/* for UV_MoveVolume() recovery */

#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */
#include <setjmp.h>

#include "volser_internal.h"
#include "volser_prototypes.h"
#include "vsutils_prototypes.h"
#include "lockprocs_prototypes.h"

struct ubik_client *cstruct;
int verbose = 0, noresolve = 0;

struct release {
    afs_uint32 crtime;
    afs_uint32 uptime;
    afs_int32 vldbEntryIndex;
};

/* Utility macros used by rest of this source file */
#define EPRINT(ec, es) \
do { \
	fprintf(STDERR, "\n"); \
	fprintf(STDERR, (es)); \
	PrintError("   ",ec); \
} while (0)

#define EPRINT1(ec, es, ep1) \
do { \
	fprintf(STDERR, "\n"); \
	fprintf(STDERR, (es), (ep1)); \
	PrintError("   ",ec); \
} while (0)

#define EPRINT2(ec, es, ep1, ep2) \
do { \
	fprintf(STDERR, "\n"); \
	fprintf(STDERR, (es), (ep1), (ep2)); \
	PrintError("   ",ec); \
} while (0)

#define EPRINT3(ec, es, ep1, ep2, ep3) \
do { \
	fprintf(STDERR, "\n"); \
	fprintf(STDERR, (es), (ep1), (ep2), (ep3)); \
	PrintError("   ",ec); \
} while (0)

#define EGOTO(where, ec, es) \
do { \
	if (ec) { \
		EPRINT((ec),(es)); \
		error = (ec); \
		goto where; \
	} \
} while (0)

#define EGOTO1(where, ec, es, ep1) \
do { \
	if (ec) { \
		EPRINT1((ec),(es),(ep1)); \
		error = (ec); \
		goto where; \
	} \
} while (0)

#define EGOTO2(where, ec, es, ep1, ep2) \
do { \
	if (ec) { \
		EPRINT2((ec),(es),(ep1),(ep2)); \
		error = (ec); \
		goto where; \
	} \
} while (0)

#define EGOTO3(where, ec, es, ep1, ep2, ep3) \
do { \
	if (ec) { \
		EPRINT3((ec),(es),(ep1),(ep2),(ep3)); \
		error = (ec); \
		goto where; \
	} \
} while (0)

#define VPRINT(es) \
	{ if (verbose) { fprintf(STDOUT, (es)); fflush(STDOUT); } }
#define VPRINT1(es, p) \
	{ if (verbose) { fprintf(STDOUT, (es), (p)); fflush(STDOUT); } }
#define VPRINT2(es, p1, p2) \
	{ if (verbose) { fprintf(STDOUT, (es), (p1), (p2)); fflush(STDOUT); } }
#define VPRINT3(es, p1, p2, p3) \
	{ if (verbose) { fprintf(STDOUT, (es), (p1), (p2), (p3)); fflush(STDOUT); } }
#define VDONE \
	{ if (verbose) { fprintf(STDOUT, " done\n"); fflush(STDOUT); } }
#define VEPRINT(es) \
	{ if (verbose) { fprintf(STDERR, (es)); fflush(STDERR); } }
#define VEPRINT1(es, p) \
	{ if (verbose) { fprintf(STDERR, (es), (p)); fflush(STDERR); } }
#define VEPRINT2(es, p1, p2) \
	{ if (verbose) { fprintf(STDERR, (es), (p1), (p2)); fflush(STDERR); } }
#define VEPRINT3(es, p1, p2, p3) \
	{ if (verbose) { fprintf(STDERR, (es), (p1), (p2), (p3)); fflush(STDERR); } }
#define VEDONE \
	{ if (verbose) { fprintf(STDERR, " done\n"); fflush(STDERR); } }



/* getting rid of this */
#define ERROR_EXIT(code) do { \
    error = (code); \
    goto error_exit; \
} while (0)


/* Protos for static routines */
#if 0
static afs_int32 CheckAndDeleteVolume(struct rx_connection *aconn,
				      afs_int32 apart, afs_uint32 okvol,
				      afs_uint32 delvol);
#endif
static int GetTrans(struct nvldbentry *vldbEntryPtr, afs_int32 index,
		    struct rx_connection **connPtr, afs_int32 * transPtr,
		    afs_uint32 * crtimePtr, afs_uint32 * uptimePtr,
		    afs_int32 *origflags, afs_uint32 tmpVolId);
static int SimulateForwardMultiple(struct rx_connection *fromconn,
				   afs_int32 fromtid, afs_int32 fromdate,
				   manyDests * tr, afs_int32 flags,
				   void *cookie, manyResults * results);
static int DoVolOnline(struct nvldbentry *vldbEntryPtr, afs_uint32 avolid,
		       int index, char *vname, struct rx_connection *connPtr);
static int DoVolClone(struct rx_connection *aconn, afs_uint32 avolid,
		      afs_int32 apart, int type, afs_uint32 cloneid,
		      char *typestring, char *pname, char *vname, char *suffix,
		      struct volser_status *volstatus, afs_int32 *transPtr);
static int DoVolDelete(struct rx_connection *aconn, afs_uint32 avolid,
		       afs_int32 apart, char *typestring, afs_uint32 atoserver,
		       struct volser_status *volstatus, char *pprefix);
static afs_int32 CheckVolume(volintInfo * volumeinfo, afs_uint32 aserver,
			     afs_int32 apart, afs_int32 * modentry,
			     afs_uint32 * maxvolid, struct nvldbentry *aentry);


/*map the partition <partId> into partition name <partName>*/
void
MapPartIdIntoName(afs_int32 partId, char *partName)
{
    if (partId < 26) {		/* what if partId > = 26 ? */
	strcpy(partName, "/vicep");
	partName[6] = partId + 'a';
	partName[7] = '\0';
	return;
    } else if (partId < VOLMAXPARTS) {
	strcpy(partName, "/vicep");
	partId -= 26;
	partName[6] = 'a' + (partId / 26);
	partName[7] = 'a' + (partId % 26);
	partName[8] = '\0';
	return;
    }
}

int
yesprompt(char *str)
{
    int response, c;
    int code;

    fprintf(STDERR, "Do you want to %s? [yn](n): ", str);
    response = c = getchar();
    while (!(c == EOF || c == '\n'))
	c = getchar();		/*skip to end of line */
    code = (response == 'y' || response == 'Y');
    return code;
}


int
PrintError(char *msg, afs_int32 errcode)
{
    fprintf(STDERR, "%s", msg);
    /*replace by a big switch statement */
    switch (errcode) {
    case 0:
	break;
    case -1:
	fprintf(STDERR, "Possible communication failure\n");
	break;
    case VSALVAGE:
	fprintf(STDERR, "Volume needs to be salvaged\n");
	break;
    case VNOVNODE:
	fprintf(STDERR, "Bad vnode number quoted\n");
	break;
    case VNOVOL:
	fprintf(STDERR,
		"Volume not attached, does not exist, or not on line\n");
	break;
    case VVOLEXISTS:
	fprintf(STDERR, "Volume already exists\n");
	break;
    case VNOSERVICE:
	fprintf(STDERR, "Volume is not in service\n");
	break;
    case VOFFLINE:
	fprintf(STDERR, "Volume is off line\n");
	break;
    case VONLINE:
	fprintf(STDERR, "Volume is already on line\n");
	break;
    case VDISKFULL:
	fprintf(STDERR, "Partition is full\n");
	break;
    case VOVERQUOTA:
	fprintf(STDERR, "Volume max quota exceeded\n");
	break;
    case VBUSY:
	fprintf(STDERR, "Volume temporarily unavailable\n");
	break;
    case VMOVED:
	fprintf(STDERR, "Volume has moved to another server\n");
	break;
    case VL_IDEXIST:
	fprintf(STDERR, "VLDB: volume Id exists in the vldb\n");
	break;
    case VL_IO:
	fprintf(STDERR, "VLDB: a read terminated too early\n");
	break;
    case VL_NAMEEXIST:
	fprintf(STDERR, "VLDB: volume entry exists in the vldb\n");
	break;
    case VL_CREATEFAIL:
	fprintf(STDERR, "VLDB: internal creation failure\n");
	break;
    case VL_NOENT:
	fprintf(STDERR, "VLDB: no such entry\n");
	break;
    case VL_EMPTY:
	fprintf(STDERR, "VLDB: vldb database is empty\n");
	break;
    case VL_ENTDELETED:
	fprintf(STDERR, "VLDB: entry is deleted (soft delete)\n");
	break;
    case VL_BADNAME:
	fprintf(STDERR, "VLDB: volume name is illegal\n");
	break;
    case VL_BADINDEX:
	fprintf(STDERR, "VLDB: index was out of range\n");
	break;
    case VL_BADVOLTYPE:
	fprintf(STDERR, "VLDB: bad volume type\n");
	break;
    case VL_BADSERVER:
	fprintf(STDERR, "VLDB: illegal server number (not within limits)\n");
	break;
    case VL_BADPARTITION:
	fprintf(STDERR, "VLDB: bad partition number\n");
	break;
    case VL_REPSFULL:
	fprintf(STDERR, "VLDB: run out of space for replication sites\n");
	break;
    case VL_NOREPSERVER:
	fprintf(STDERR, "VLDB: no such repsite server exists\n");
	break;
    case VL_DUPREPSERVER:
	fprintf(STDERR, "VLDB: replication site server already exists\n");
	break;
    case VL_RWNOTFOUND:
	fprintf(STDERR, "VLDB: parent r/w entry not found\n");
	break;
    case VL_BADREFCOUNT:
	fprintf(STDERR, "VLDB: illegal reference count number\n");
	break;
    case VL_SIZEEXCEEDED:
	fprintf(STDERR, "VLDB: vldb size for attributes exceeded\n");
	break;
    case VL_BADENTRY:
	fprintf(STDERR, "VLDB: bad incoming vldb entry\n");
	break;
    case VL_BADVOLIDBUMP:
	fprintf(STDERR, "VLDB: illegal max volid increment\n");
	break;
    case VL_IDALREADYHASHED:
	fprintf(STDERR, "VLDB: (RO/BACK) Id already hashed\n");
	break;
    case VL_ENTRYLOCKED:
	fprintf(STDERR, "VLDB: vldb entry is already locked\n");
	break;
    case VL_BADVOLOPER:
	fprintf(STDERR, "VLDB: bad volume operation code\n");
	break;
    case VL_BADRELLOCKTYPE:
	fprintf(STDERR, "VLDB: bad release lock type\n");
	break;
    case VL_RERELEASE:
	fprintf(STDERR, "VLDB: status report: last release was aborted\n");
	break;
    case VL_BADSERVERFLAG:
	fprintf(STDERR, "VLDB: invalid replication site server flag\n");
	break;
    case VL_PERM:
	fprintf(STDERR, "VLDB: no permission access for call\n");
	break;
    case VOLSERREAD_DUMPERROR:
	fprintf(STDERR,
		"VOLSER:  Problems encountered in reading the dump file !\n");
	break;
    case VOLSERDUMPERROR:
	fprintf(STDERR, "VOLSER: Problems encountered in doing the dump !\n");
	break;
    case VOLSERATTACH_ERROR:
	fprintf(STDERR, "VOLSER: Could not attach the volume\n");
	break;
    case VOLSERDETACH_ERROR:
	fprintf(STDERR, "VOLSER: Could not detach the volume\n");
	break;
    case VOLSERILLEGAL_PARTITION:
	fprintf(STDERR, "VOLSER: encountered illegal partition number\n");
	break;
    case VOLSERBAD_ACCESS:
	fprintf(STDERR, "VOLSER: permission denied, not a super user\n");
	break;
    case VOLSERVLDB_ERROR:
	fprintf(STDERR, "VOLSER: error detected in the VLDB\n");
	break;
    case VOLSERBADNAME:
	fprintf(STDERR, "VOLSER: error in volume name\n");
	break;
    case VOLSERVOLMOVED:
	fprintf(STDERR, "VOLSER: volume has moved\n");
	break;
    case VOLSERBADOP:
	fprintf(STDERR, "VOLSER: illegal operation\n");
	break;
    case VOLSERBADRELEASE:
	fprintf(STDERR, "VOLSER: release could not be completed\n");
	break;
    case VOLSERVOLBUSY:
	fprintf(STDERR, "VOLSER: volume is busy\n");
	break;
    case VOLSERNO_MEMORY:
	fprintf(STDERR, "VOLSER: volume server is out of memory\n");
	break;
    case VOLSERNOVOL:
	fprintf(STDERR,
		"VOLSER: no such volume - location specified incorrectly or volume does not exist\n");
	break;
    case VOLSERMULTIRWVOL:
	fprintf(STDERR,
		"VOLSER: multiple RW volumes with same ID, one of which should be deleted\n");
	break;
    case VOLSERFAILEDOP:
	fprintf(STDERR,
		"VOLSER: not all entries were successfully processed\n");
	break;
    default:
	{
	    initialize_KA_error_table();
	    initialize_RXK_error_table();
	    initialize_KTC_error_table();
	    initialize_ACFG_error_table();
	    initialize_CMD_error_table();
	    initialize_VL_error_table();

	    fprintf(STDERR, "%s: %s\n", afs_error_table_name(errcode),
		    afs_error_message(errcode));
	    break;
	}
    }
    return 0;
}

void init_volintInfo(struct volintInfo *vinfo) {
    memset(vinfo, 0, sizeof(struct volintInfo));

    vinfo->maxquota = -1;
    vinfo->dayUse = -1;
    vinfo->creationDate = -1;
    vinfo->updateDate = -1;
    vinfo->flags = -1;
    vinfo->spare0 = -1;
    vinfo->spare1 = -1;
    vinfo->spare2 = -1;
    vinfo->spare3 = -1;
}

static struct rx_securityClass *uvclass = 0;
static int uvindex = -1;
/* called by VLDBClient_Init to set the security module to be used in the RPC */
int
UV_SetSecurity(struct rx_securityClass *as, afs_int32 aindex)
{
    uvindex = aindex;
    uvclass = as;
    return 0;
}

/* bind to volser on <port> <aserver> */
/* takes server address in network order, port in host order.  dumb */
struct rx_connection *
UV_Bind(afs_uint32 aserver, afs_int32 port)
{
    struct rx_connection *tc;

    tc = rx_NewConnection(aserver, htons(port), VOLSERVICE_ID, uvclass,
			  uvindex);
    return tc;
}

static int
AFSVolCreateVolume_retry(struct rx_connection *z_conn,
		       afs_int32 partition, char *name, afs_int32 type,
		       afs_int32 parent, afs_uint32 *volid, afs_int32 *trans)
{
    afs_int32 code;
    int retries = 3;
    while (retries) {
	code = AFSVolCreateVolume(z_conn, partition, name, type, parent,
				  volid, trans);
        if (code != VOLSERVOLBUSY)
            break;
        retries--;
#ifdef AFS_PTHREAD_ENV
        sleep(3-retries);
#else
        IOMGR_Sleep(3-retries);
#endif
    }
    return code;
}

static int
AFSVolTransCreate_retry(struct rx_connection *z_conn,
			afs_int32 volume, afs_int32 partition,
			afs_int32 flags, afs_int32 * trans)
{
    afs_int32 code;
    int retries = 3;
    while (retries) {
	code = AFSVolTransCreate(z_conn, volume, partition, flags, trans);
        if (code != VOLSERVOLBUSY)
            break;
        retries--;
#ifdef AFS_PTHREAD_ENV
        sleep(3-retries);
#else
        IOMGR_Sleep(3-retries);
#endif
    }
    return code;
}

#if 0
/* if <okvol> is allright(indicated by beibg able to
 * start a transaction, delete the <delvol> */
static afs_int32
CheckAndDeleteVolume(struct rx_connection *aconn, afs_int32 apart,
		     afs_uint32 okvol, afs_uint32 delvol)
{
    afs_int32 error, code, tid, rcode;
    error = 0;
    code = 0;

    if (okvol == 0) {
	code = AFSVolTransCreate_retry(aconn, delvol, apart, ITOffline, &tid);
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
	code = AFSVolTransCreate_retry(aconn, okvol, apart, ITOffline, &tid);
	if (!code) {
	    code = AFSVolEndTrans(aconn, tid, &rcode);
	    if (!code)
		code = rcode;
	    if (!error && code)
		error = code;
	    code = AFSVolTransCreate_retry(aconn, delvol, apart, ITOffline, &tid);
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

#endif

/* called by EmuerateEntry, show vldb entry in a reasonable format */
void
SubEnumerateEntry(struct nvldbentry *entry)
{
    int i;
    char pname[10];
    int isMixed = 0;
    char hoststr[16];

#ifdef notdef
    fprintf(STDOUT, "	readWriteID %-10u ", entry->volumeId[RWVOL]);
    if (entry->flags & RW_EXISTS)
	fprintf(STDOUT, " valid \n");
    else
	fprintf(STDOUT, " invalid \n");
    fprintf(STDOUT, "	readOnlyID  %-10u ", entry->volumeId[ROVOL]);
    if (entry->flags & RO_EXISTS)
	fprintf(STDOUT, " valid \n");
    else
	fprintf(STDOUT, " invalid \n");
    fprintf(STDOUT, "	backUpID    %-10u ", entry->volumeId[BACKVOL]);
    if (entry->flags & BACK_EXISTS)
	fprintf(STDOUT, " valid \n");
    else
	fprintf(STDOUT, " invalid \n");
    if ((entry->cloneId != 0) && (entry->flags & RO_EXISTS))
	fprintf(STDOUT, "    releaseClone %-10u \n", entry->cloneId);
#else
    if (entry->flags & RW_EXISTS)
	fprintf(STDOUT, "    RWrite: %-10u", entry->volumeId[RWVOL]);
    if (entry->flags & RO_EXISTS)
	fprintf(STDOUT, "    ROnly: %-10u", entry->volumeId[ROVOL]);
    if (entry->flags & BACK_EXISTS)
	fprintf(STDOUT, "    Backup: %-10u", entry->volumeId[BACKVOL]);
    if ((entry->cloneId != 0) && (entry->flags & RO_EXISTS))
	fprintf(STDOUT, "    RClone: %-10lu", (unsigned long)entry->cloneId);
    fprintf(STDOUT, "\n");
#endif
    fprintf(STDOUT, "    number of sites -> %lu\n",
	    (unsigned long)entry->nServers);
    for (i = 0; i < entry->nServers; i++) {
	if (entry->serverFlags[i] & NEW_REPSITE)
	    isMixed = 1;
    }
    for (i = 0; i < entry->nServers; i++) {
	MapPartIdIntoName(entry->serverPartition[i], pname);
	fprintf(STDOUT, "       server %s partition %s ",
		noresolve ? afs_inet_ntoa_r(entry->serverNumber[i], hoststr) :
                hostutil_GetNameByINet(entry->serverNumber[i]), pname);
	if (entry->serverFlags[i] & ITSRWVOL)
	    fprintf(STDOUT, "RW Site ");
	else
	    fprintf(STDOUT, "RO Site ");
	if (isMixed) {
	    if (entry->serverFlags[i] & NEW_REPSITE)
		fprintf(STDOUT," -- New release");
	    else
		if (!(entry->serverFlags[i] & ITSRWVOL))
		    fprintf(STDOUT," -- Old release");
	} else {
	    if (entry->serverFlags[i] & RO_DONTUSE)
		fprintf(STDOUT, " -- Not released");
	}
	fprintf(STDOUT, "\n");
    }

    return;

}

/*enumerate the vldb entry corresponding to <entry> */
void
EnumerateEntry(struct nvldbentry *entry)
{

    fprintf(STDOUT, "\n");
    fprintf(STDOUT, "%s \n", entry->name);
    SubEnumerateEntry(entry);
    return;
}

/* forcibly remove a volume.  Very dangerous call */
int
UV_NukeVolume(afs_uint32 server, afs_int32 partid, afs_uint32 volid)
{
    struct rx_connection *tconn;
    afs_int32 code;

    tconn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    if (tconn) {
	code = AFSVolNukeVolume(tconn, partid, volid);
	rx_DestroyConnection(tconn);
    } else
	code = 0;
    return code;
}

/* like df. Return usage of <pname> on <server> in <partition> */
int
UV_PartitionInfo64(afs_uint32 server, char *pname,
		   struct diskPartition64 *partition)
{
    struct rx_connection *aconn;
    afs_int32 code = 0;

    aconn = (struct rx_connection *)0;
    aconn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    code = AFSVolPartitionInfo64(aconn, pname, partition);
    if (code == RXGEN_OPCODE) {
	struct diskPartition *dpp =
	    (struct diskPartition *)malloc(sizeof(struct diskPartition));
	code = AFSVolPartitionInfo(aconn, pname, dpp);
	if (!code) {
	    strncpy(partition->name, dpp->name, 32);
	    strncpy(partition->devName, dpp->devName, 32);
	    partition->lock_fd = dpp->lock_fd;
	    partition->free = dpp->free;
	    partition->minFree = dpp->minFree;
	}
	free(dpp);
    }
    if (code) {
	fprintf(STDERR, "Could not get information on partition %s\n", pname);
	PrintError("", code);
    }
    if (aconn)
	rx_DestroyConnection(aconn);
    return code;
}

/* old interface to create volumes */
int
UV_CreateVolume(afs_uint32 aserver, afs_int32 apart, char *aname,
		afs_uint32 * anewid)
{
    afs_int32 code;
    *anewid = 0;
    code = UV_CreateVolume2(aserver, apart, aname, 5000, 0, 0, 0, 0, anewid);
    return code;
}

/* less old interface to create volumes */
int
UV_CreateVolume2(afs_uint32 aserver, afs_int32 apart, char *aname,
		 afs_int32 aquota, afs_int32 aspare1, afs_int32 aspare2,
		 afs_int32 aspare3, afs_int32 aspare4, afs_uint32 * anewid)
{
    afs_uint32 roid = 0, bkid = 0;
    return UV_CreateVolume3(aserver, apart, aname, aquota, aspare1, aspare2,
	aspare3, aspare4, anewid, &roid, &bkid);
}

/**
 * Create a volume on the given server and partition
 *
 * @param aserver  server to create volume on
 * @param spart  partition to create volume on
 * @param aname  name of new volume
 * @param aquota  quota for new volume
 * @param anewid  contains the desired volume id for the new volume. If
 *                *anewid == 0, a new id will be chosen, and will be placed
 *                in *anewid when UV_CreateVolume3 returns.
 * @param aroid  contains the desired RO volume id. If NULL, the RO id entry
 *               will be unset. If *aroid == 0, an id will be chosen, and
 *               will be placed in *anewid when UV_CreateVolume3 returns.
 * @param abkid  same as aroid, except for the BK volume id instead of the
 *               RO volume id.
 * @return 0 on success, error code otherwise.
 */
int
UV_CreateVolume3(afs_uint32 aserver, afs_int32 apart, char *aname,
		 afs_int32 aquota, afs_int32 aspare1, afs_int32 aspare2,
		 afs_int32 aspare3, afs_int32 aspare4, afs_uint32 * anewid,
		 afs_uint32 * aroid, afs_uint32 * abkid)
{
    struct rx_connection *aconn;
    afs_int32 tid;
    afs_int32 code;
    afs_int32 error;
    afs_int32 rcode, vcode;
    afs_int32 lastid;
    struct nvldbentry entry, storeEntry;	/*the new vldb entry */
    struct volintInfo tstatus;

    tid = 0;
    aconn = (struct rx_connection *)0;
    error = 0;

    memset(&storeEntry, 0, sizeof(struct nvldbentry));

    init_volintInfo(&tstatus);
    tstatus.maxquota = aquota;

    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);

    if (aroid && *aroid) {
	VPRINT1("Using RO volume ID %d.\n", *aroid);
    }
    if (abkid && *abkid) {
	VPRINT1("Using BK volume ID %d.\n", *abkid);
    }

    if (*anewid) {
        vcode = VLDB_GetEntryByID(*anewid, -1, &entry);
	if (!vcode) {
	    fprintf(STDERR, "Volume ID %d already exists\n", *anewid);
	    return VVOLEXISTS;
	}
	VPRINT1("Using volume ID %d.\n", *anewid);
    } else {
	vcode = ubik_VL_GetNewVolumeId(cstruct, 0, 1, anewid);
	EGOTO1(cfail, vcode, "Could not get an Id for volume %s\n", aname);

	if (aroid && *aroid == 0) {
	    vcode = ubik_VL_GetNewVolumeId(cstruct, 0, 1, aroid);
	    EGOTO1(cfail, vcode, "Could not get an RO Id for volume %s\n", aname);
	}

	if (abkid && *abkid == 0) {
	    vcode = ubik_VL_GetNewVolumeId(cstruct, 0, 1, abkid);
	    EGOTO1(cfail, vcode, "Could not get a BK Id for volume %s\n", aname);
	}
    }

    /* rw,ro, bk id are related in the default case */
    /* If caller specified RW id, but not RO/BK ids, have them be RW+1 and RW+2 */
    lastid = *anewid;
    if (aroid && *aroid != 0) {
	lastid = MAX(lastid, *aroid);
    }
    if (abkid && *abkid != 0) {
	lastid = MAX(lastid, *abkid);
    }
    if (aroid && *aroid == 0) {
	*aroid = ++lastid;
    }
    if (abkid && *abkid == 0) {
	*abkid = ++lastid;
    }

    code =
	AFSVolCreateVolume_retry(aconn, apart, aname, volser_RW, 0, anewid, &tid);
    EGOTO2(cfail, code, "Failed to create the volume %s %u \n", aname,
	   *anewid);

    code = AFSVolSetInfo(aconn, tid, &tstatus);
    if (code)
	EPRINT(code, "Could not change quota, continuing...\n");

    code = AFSVolSetFlags(aconn, tid, 0);	/* bring it online (mark it InService */
    EGOTO2(cfail, code, "Could not bring the volume %s %u online \n", aname,
	   *anewid);

    VPRINT2("Volume %s %u created and brought online\n", aname, *anewid);

    /* set up the vldb entry for this volume */
    strncpy(entry.name, aname, VOLSER_OLDMAXVOLNAME);
    entry.nServers = 1;
    entry.serverNumber[0] = aserver;	/* this should have another
					 * level of indirection later */
    entry.serverPartition[0] = apart;	/* this should also have
					 * another indirection level */
    entry.flags = RW_EXISTS;	/* this records that rw volume exists */
    entry.serverFlags[0] = ITSRWVOL;	/*this rep site has rw  vol */
    entry.volumeId[RWVOL] = *anewid;
    entry.volumeId[ROVOL] = aroid ? *aroid : 0;
    entry.volumeId[BACKVOL] = abkid ? *abkid : 0;
    entry.cloneId = 0;
    /*map into right byte order, before passing to xdr, the stuff has to be in host
     * byte order. Xdr converts it into network order */
    MapNetworkToHost(&entry, &storeEntry);
    /* create the vldb entry */
    vcode = VLDB_CreateEntry(&storeEntry);
    if (vcode) {
	fprintf(STDERR,
		"Could not create a VLDB entry for the volume %s %lu\n",
		aname, (unsigned long)*anewid);
	/*destroy the created volume */
	VPRINT1("Deleting the newly created volume %u\n", *anewid);
	AFSVolDeleteVolume(aconn, tid);
	error = vcode;
	goto cfail;
    }
    VPRINT2("Created the VLDB entry for the volume %s %u\n", aname, *anewid);
    /* volume created, now terminate the transaction and release the connection */
    code = AFSVolEndTrans(aconn, tid, &rcode);	/*if it crashes before this
						 * the volume will come online anyway when transaction timesout , so if
						 * vldb entry exists then the volume is guaranteed to exist too wrt create */
    tid = 0;
    if (code) {
	fprintf(STDERR,
		"Failed to end the transaction on the volume %s %lu\n", aname,
		(unsigned long)*anewid);
	error = code;
	goto cfail;
    }

  cfail:
    if (tid) {
	code = AFSVolEndTrans(aconn, tid, &rcode);
	if (code)
	    fprintf(STDERR, "WARNING: could not end transaction\n");
    }
    if (aconn)
	rx_DestroyConnection(aconn);
    PrintError("", error);
    return error;
}

/* create a volume, given a server, partition number, volume name --> sends
* back new vol id in <anewid>*/
int
UV_AddVLDBEntry(afs_uint32 aserver, afs_int32 apart, char *aname,
		afs_uint32 aid)
{
    struct rx_connection *aconn;
    afs_int32 error;
    afs_int32 vcode;
    struct nvldbentry entry, storeEntry;	/*the new vldb entry */

    memset(&storeEntry, 0, sizeof(struct nvldbentry));

    aconn = (struct rx_connection *)0;
    error = 0;

    /* set up the vldb entry for this volume */
    strncpy(entry.name, aname, VOLSER_OLDMAXVOLNAME);
    entry.nServers = 1;
    entry.serverNumber[0] = aserver;	/* this should have another
					 * level of indirection later */
    entry.serverPartition[0] = apart;	/* this should also have
					 * another indirection level */
    entry.flags = RW_EXISTS;	/* this records that rw volume exists */
    entry.serverFlags[0] = ITSRWVOL;	/*this rep site has rw  vol */
    entry.volumeId[RWVOL] = aid;
#ifdef notdef
    entry.volumeId[ROVOL] = anewid + 1;	/* rw,ro, bk id are related in the default case */
    entry.volumeId[BACKVOL] = *anewid + 2;
#else
    entry.volumeId[ROVOL] = 0;
    entry.volumeId[BACKVOL] = 0;
#endif
    entry.cloneId = 0;
    /*map into right byte order, before passing to xdr, the stuff has to be in host
     * byte order. Xdr converts it into network order */
    MapNetworkToHost(&entry, &storeEntry);
    /* create the vldb entry */
    vcode = VLDB_CreateEntry(&storeEntry);
    if (vcode) {
	fprintf(STDERR,
		"Could not create a VLDB entry for the  volume %s %lu\n",
		aname, (unsigned long)aid);
	error = vcode;
	goto cfail;
    }
    VPRINT2("Created the VLDB entry for the volume %s %u\n", aname, aid);

  cfail:
    if (aconn)
	rx_DestroyConnection(aconn);
    PrintError("", error);
    return error;
}

/* Delete the volume <volid>on <aserver> <apart>
 * the physical entry gets removed from the vldb only if the ref count
 * becomes zero
 */
int
UV_DeleteVolume(afs_uint32 aserver, afs_int32 apart, afs_uint32 avolid)
{
    struct rx_connection *aconn = (struct rx_connection *)0;
    afs_int32 ttid = 0;
    afs_int32 code, rcode;
    afs_int32 error = 0;
    struct nvldbentry entry, storeEntry;
    int islocked = 0;
    afs_int32 avoltype = -1, vtype;
    int notondisk = 0, notinvldb = 0;

    memset(&storeEntry, 0, sizeof(struct nvldbentry));

    /* Find and read bhe VLDB entry for this volume */
    code = ubik_VL_SetLock(cstruct, 0, avolid, avoltype, VLOP_DELETE);
    if (code) {
	if (code != VL_NOENT) {
	    EGOTO1(error_exit, code,
		   "Could not lock VLDB entry for the volume %u\n", avolid);
	}
	notinvldb = 1;
    } else {
	islocked = 1;

	code = VLDB_GetEntryByID(avolid, avoltype, &entry);
	EGOTO1(error_exit, code, "Could not fetch VLDB entry for volume %u\n",
	       avolid);
	MapHostToNetwork(&entry);

	if (verbose)
	    EnumerateEntry(&entry);
    }

    /* Whether volume is in the VLDB or not. Delete the volume on disk */
    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);

    code = DoVolDelete(aconn, avolid, apart, "the", 0, NULL, NULL);
    if (code) {
	if (code == VNOVOL)
	    notondisk = 1;
	else {
	    error = code;
	    goto error_exit;
	}
    }

    /* Now update the VLDB entry.
     * But first, verify we have a VLDB entry.
     * Whether volume is on disk or not. Delete the volume in VLDB.
     */
    if (notinvldb)
	ERROR_EXIT(0);

    if (avolid == entry.volumeId[BACKVOL]) {
	/* Its a backup volume, modify the VLDB entry. Check that the
	 * backup volume is on the server/partition we asked to delete.
	 */
	if (!(entry.flags & BACK_EXISTS) || !Lp_Match(aserver, apart, &entry)) {
	    notinvldb = 2;	/* Not on this server and partition */
	    ERROR_EXIT(0);
	}

	VPRINT1("Marking the backup volume %u deleted in the VLDB\n", avolid);

	entry.flags &= ~BACK_EXISTS;
	vtype = BACKVOL;
    }

    else if (avolid == entry.volumeId[ROVOL]) {
	/* Its a read-only volume, modify the VLDB entry. Check that the
	 * readonly volume is on the server/partition we asked to delete.
	 * If flags does not have RO_EIXSTS set, then this may mean the RO
	 * hasn't been released (and could exist in VLDB).
	 */
	if (!Lp_ROMatch(aserver, apart, &entry)) {
	    notinvldb = 2;	/* Not found on this server and partition */
	    ERROR_EXIT(0);
	}

	if (verbose)
	    fprintf(STDOUT,
		    "Marking the readonly volume %lu deleted in the VLDB\n",
		    (unsigned long)avolid);

	Lp_SetROValue(&entry, aserver, apart, 0, 0);	/* delete the site */
	entry.nServers--;
	if (!Lp_ROMatch(0, 0, &entry))
	    entry.flags &= ~RO_EXISTS;	/* This was the last ro volume */
	vtype = ROVOL;
    }

    else if (avolid == entry.volumeId[RWVOL]) {
	/* It's a rw volume, delete the backup volume, modify the VLDB entry.
	 * Check that the readwrite volumes is on the server/partition we
	 * asked to delete.
	 */
	if (!(entry.flags & RW_EXISTS) || !Lp_Match(aserver, apart, &entry)) {
	    notinvldb = 2;	/* Not found on this server and partition */
	    ERROR_EXIT(0);
	}

	if (entry.volumeId[BACKVOL]) {
	    /* Delete backup if it exists */
	    code = DoVolDelete(aconn, entry.volumeId[BACKVOL], apart,
	                       "the backup", 0, NULL, NULL);
	    if (code && code != VNOVOL) {
		error = code;
		goto error_exit;
	    }
	}

	if (verbose)
	    fprintf(STDOUT,
		    "Marking the readwrite volume %lu%s deleted in the VLDB\n",
		    (unsigned long)avolid,
		    ((entry.
		      flags & BACK_EXISTS) ? ", and its backup volume," :
		     ""));

	Lp_SetRWValue(&entry, aserver, apart, 0L, 0L);
	entry.nServers--;
	entry.flags &= ~(BACK_EXISTS | RW_EXISTS);
	vtype = RWVOL;

	if (entry.flags & RO_EXISTS)
	    fprintf(STDERR, "WARNING: ReadOnly copy(s) may still exist\n");
    }

    else {
	notinvldb = 2;		/* Not found on this server and partition */
	ERROR_EXIT(0);
    }

    /* Either delete or replace the VLDB entry */
    if ((entry.nServers <= 0) || !(entry.flags & (RO_EXISTS | RW_EXISTS))) {
	if (verbose)
	    fprintf(STDOUT,
		    "Last reference to the VLDB entry for %lu - deleting entry\n",
		    (unsigned long)avolid);
	code = ubik_VL_DeleteEntry(cstruct, 0, avolid, vtype);
	EGOTO1(error_exit, code,
	       "Could not delete the VLDB entry for the volume %u \n",
	       avolid);
    } else {
	MapNetworkToHost(&entry, &storeEntry);
	code =
	    VLDB_ReplaceEntry(avolid, vtype, &storeEntry,
			      (LOCKREL_OPCODE | LOCKREL_AFSID |
			       LOCKREL_TIMESTAMP));
	EGOTO1(error_exit, code,
	       "Could not update the VLDB entry for the volume %u \n",
	       avolid);
    }
    islocked = 0;

  error_exit:
    if (error)
	EPRINT(error, "\n");

    if (notondisk && notinvldb) {
	EPRINT2(VOLSERNOVOL, "Volume %u does not exist %s\n", avolid,
		((notinvldb == 2) ? "on server and partition" : ""));
	if (!error)
	    error = VOLSERNOVOL;
    } else if (notondisk) {
	fprintf(STDERR,
		"WARNING: Volume %lu did not exist on the partition\n",
		(unsigned long)avolid);
    } else if (notinvldb) {
	fprintf(STDERR, "WARNING: Volume %lu does not exist in VLDB %s\n",
		(unsigned long)avolid,
		((notinvldb == 2) ? "on server and partition" : ""));
    }

    if (ttid) {
	code = AFSVolEndTrans(aconn, ttid, &rcode);
	code = (code ? code : rcode);
	if (code) {
	    fprintf(STDERR, "Could not end transaction on the volume %lu\n",
		    (unsigned long)avolid);
	    PrintError("", code);
	    if (!error)
		error = code;
	}
    }

    if (islocked) {
	code =
	    ubik_VL_ReleaseLock(cstruct, 0, avolid, -1,
				(LOCKREL_OPCODE | LOCKREL_AFSID |
				 LOCKREL_TIMESTAMP));
	if (code) {
	    EPRINT1(code,
		    "Could not release the lock on the VLDB entry for the volume %u \n",
		    avolid);
	    if (!error)
		error = code;
	}
    }

    if (aconn)
	rx_DestroyConnection(aconn);
    return error;
}

/* add recovery to UV_MoveVolume */

#define TESTC	0		/* set to test recovery code, clear for production */

jmp_buf env;
int interrupt = 0;

static void *
do_interrupt(void * unused)
{
    if (interrupt) {
#if !defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
	/* Avoid UNIX LWP from getting confused that our stack has suddenly
	 * changed. This will avoid some sanity checks, but until a better way
	 * is found, the only alternative is always crashing and burning on at
	 * least the stack-overflow check. */
	lwp_cpptr->stack = NULL;
#endif
	longjmp(env, 0);
    }

    fprintf(STDOUT, "\nSIGINT handler: vos move operation in progress\n");
    fprintf(STDOUT,
	    "WARNING: may leave AFS storage and metadata in indeterminate state\n");
    fprintf(STDOUT, "enter second control-c to exit\n");
    fflush(STDOUT);

    interrupt = 1;
    return NULL;
}

static void
sigint_handler(int x)
{
#ifdef AFS_PTHREAD_ENV
    do_interrupt(NULL);
#else
    IOMGR_SoftSig(do_interrupt, 0);
#endif
    (void)signal(SIGINT, sigint_handler);
}

static int
DoVolDelete(struct rx_connection *aconn, afs_uint32 avolid,
	    afs_int32 apart, char *ptypestring, afs_uint32 atoserver,
	    struct volser_status *volstatus, char *pprefix)
{
    afs_int32 ttid = 0, code, rcode, error = 0;
    char *prefix, *typestring;
    int beverbose = 0;

    if (pprefix)
	prefix = pprefix;
    else
	prefix = "";

    if (ptypestring) {
	typestring = ptypestring;
	beverbose = 1;
    } else
	typestring = "the";

    if (beverbose)
	VPRINT3("%sDeleting %s volume %u ...", prefix, typestring, avolid);

    code =
	AFSVolTransCreate_retry(aconn, avolid, apart, ITOffline, &ttid);

    /* return early and quietly for VNOVOL; don't continue the attempt to delete. */
    if (code == VNOVOL) {
	error = code;
	goto dfail;
    }

    EGOTO2(dfail, code, "%sFailed to start transaction on %u\n",
	   prefix, avolid);

    if (volstatus) {
	code = AFSVolGetStatus(aconn, ttid, volstatus);
	EGOTO2(dfail, code, "%sCould not get timestamp from volume %u\n",
	       prefix, avolid);
    }

    code =
	AFSVolSetFlags(aconn, ttid,
		       VTDeleteOnSalvage | VTOutOfService);

    EGOTO2(dfail, code, "%sCould not set flags on volume %u \n",
	   prefix, avolid);

    if (atoserver) {
	VPRINT1("%sSetting volume forwarding pointer ...", prefix);
	AFSVolSetForwarding(aconn, ttid, atoserver);
	VDONE;
    }

    code = AFSVolDeleteVolume(aconn, ttid);
    EGOTO2(dfail, code, "%sCould not delete volume %u\n", prefix, avolid);

dfail:
    if (ttid) {
        code = AFSVolEndTrans(aconn, ttid, &rcode);
	ttid = 0;
        if (!code)
            code = rcode;
        if (code) {
            fprintf(STDERR, "%sCould not end transaction on %s volume %lu \n",
                    prefix, typestring, (unsigned long)avolid);
            if (!error)
                error = code;
        }
    }

    if (beverbose && !error)
	VDONE;
    return error;
}

static int
DoVolClone(struct rx_connection *aconn, afs_uint32 avolid,
	   afs_int32 apart, int type, afs_uint32 cloneid,
	   char *typestring, char *pname, char *vname, char *suffix,
	   struct volser_status *volstatus, afs_int32 *transPtr)
{
    char cname[64];
    afs_int32 ttid = 0, btid = 0;
    afs_int32 code = 0, rcode = 0;
    afs_int32 error = 0;
    int cloneexists = 1;

    /* Test to see if the clone volume exists by trying to create
     * a transaction on the clone volume. We've assumed the clone exists.
     */
    code = AFSVolTransCreate_retry(aconn, cloneid, apart, ITOffline, &btid);
    if (code) {
        if (code != VNOVOL) {
	    EPRINT2(code, "Could not reach the %s volume %lu\n",
                    typestring, (unsigned long)cloneid);
            error = code;
            goto cfail;
        }
        cloneexists = 0;         /* clone volume does not exist */
    }
    if (btid) {
        code = AFSVolEndTrans(aconn, btid, &rcode);
        btid = 0;
        if (code || rcode) {
            fprintf(STDERR,
                    "Could not end transaction on the previous %s volume %lu\n",
                    typestring, (unsigned long)cloneid);
            error = (code ? code : rcode);
            goto cfail;
        }
    }

    /* Now go ahead and try to clone the RW volume.
     * First start a transaction on the RW volume
     */
    code = AFSVolTransCreate_retry(aconn, avolid, apart, ITBusy, &ttid);
    if (code) {
        fprintf(STDERR, "Could not start a transaction on the volume %lu\n",
                (unsigned long)avolid);
        error = code;
        goto cfail;
    }

    /* Clone or reclone the volume, depending on whether the clone
     * volume exists or not
     */
    if (cloneexists) {
        VPRINT2("Re-cloning %s volume %u ...", typestring, cloneid);

        code = AFSVolReClone(aconn, ttid, cloneid);
        if (code) {
            EPRINT2(code, "Could not re-clone %s volume %lu\n",
                    typestring, (unsigned long)cloneid);
            error = code;
            goto cfail;
        }
    } else {
        VPRINT2("Creating a new %s clone %u ...", typestring, cloneid);

	if (!vname) {
	    strcpy(cname, pname);
	    strcat(cname, suffix);
	}

        code = AFSVolClone(aconn, ttid, 0, type, vname?vname:cname,
			   &cloneid);
        if (code) {
            fprintf(STDERR, "Failed to clone the volume %lu\n",
                    (unsigned long)avolid);
            error = code;
            goto cfail;
        }
    }

    VDONE;

    if (volstatus) {
	VPRINT1("Getting status of parent volume %u...", avolid);
	code = AFSVolGetStatus(aconn, ttid, volstatus);
	if (code) {
	    fprintf(STDERR, "Failed to get the status of the parent volume %lu\n",
		    (unsigned long)avolid);
	    error = code;
            goto cfail;
	}
	VDONE;
    }

cfail:
    if (ttid) {
        code = AFSVolEndTrans(aconn, ttid, &rcode);
        if (code || rcode) {
            fprintf(STDERR, "Could not end transaction on the volume %lu\n",
                    (unsigned long)avolid);
            if (!error)
                error = (code ? code : rcode);
        }
    }

    if (btid) {
        code = AFSVolEndTrans(aconn, btid, &rcode);
        if (code || rcode) {
            fprintf(STDERR,
                    "Could not end transaction on the %s volume %lu\n",
                    typestring, (unsigned long)cloneid);
            if (!error)
                error = (code ? code : rcode);
        }
    }
    return error;
}

/* Move volume <afromvol> on <afromserver> <afrompart> to <atoserver>
 * <atopart>.  The operation is almost idempotent.  The following
 * flags are recognized:
 *
 *     RV_NOCLONE - don't use a copy clone
 */

int
UV_MoveVolume2(afs_uint32 afromvol, afs_uint32 afromserver, afs_int32 afrompart,
	       afs_uint32 atoserver, afs_int32 atopart, int flags)
{
    /* declare stuff 'volatile' that may be used from setjmp/longjmp and may
     * be changing during the move */
    struct rx_connection * volatile toconn;
    struct rx_connection * volatile fromconn;
    afs_int32 volatile fromtid;
    afs_int32 volatile totid;
    afs_int32 volatile clonetid;
    afs_uint32 volatile newVol;
    afs_uint32 volatile volid;
    afs_uint32 volatile backupId;
    int volatile islocked;
    int volatile pntg;

    char vname[64];
    char *volName = 0;
    char tmpName[VOLSER_MAXVOLNAME + 1];
    afs_int32 rcode;
    afs_int32 fromDate;
    afs_int32 tmp;
    afs_uint32 tmpVol;
    struct restoreCookie cookie;
    afs_int32 vcode, code;
    struct volser_status tstatus;
    struct destServer destination;

    struct nvldbentry entry, storeEntry;
    int i;
    afs_int32 error;
    char in, lf;		/* for test code */
    int same;
    char hoststr[16];

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

    /* support control-c processing */
    if (setjmp(env))
	goto mfail;
    (void)signal(SIGINT, sigint_handler);

    if (TESTC) {
	fprintf(STDOUT,
		"\nThere are three tests points - verifies all code paths through recovery.\n");
	fprintf(STDOUT, "First test point - operation not started.\n");
	fprintf(STDOUT, "...test here (y, n)? ");
	fflush(STDOUT);
	if (fscanf(stdin, "%c", &in) < 1)
	    in = 0;
	if (fscanf(stdin, "%c", &lf) < 0)	/* toss away */
	    ; /* don't care */
	if (in == 'y') {
	    fprintf(STDOUT, "type control-c\n");
	    while (1) {
		fprintf(stdout, ".");
		fflush(stdout);
		sleep(1);
	    }
	}
	/* or drop through */
    }

    vcode = VLDB_GetEntryByID(afromvol, -1, &entry);
    EGOTO1(mfail, vcode,
	   "Could not fetch the entry for the volume  %u from the VLDB \n",
	   afromvol);

    if (entry.volumeId[RWVOL] != afromvol) {
	fprintf(STDERR, "Only RW volume can be moved\n");
	exit(1);
    }

    vcode = ubik_VL_SetLock(cstruct, 0, afromvol, RWVOL, VLOP_MOVE);
    EGOTO1(mfail, vcode, "Could not lock entry for volume %u \n", afromvol);
    islocked = 1;

    vcode = VLDB_GetEntryByID(afromvol, RWVOL, &entry);
    EGOTO1(mfail, vcode,
	   "Could not fetch the entry for the volume  %u from the VLDB \n",
	   afromvol);

    backupId = entry.volumeId[BACKVOL];
    MapHostToNetwork(&entry);

    if (!Lp_Match(afromserver, afrompart, &entry)) {
	/* the from server and partition do not exist in the vldb entry corresponding to volid */
	if (!Lp_Match(atoserver, atopart, &entry)) {
	    /* the to server and partition do not exist in the vldb entry corresponding to volid */
	    fprintf(STDERR, "The volume %lu is not on the specified site. \n",
		    (unsigned long)afromvol);
	    fprintf(STDERR, "The current site is :");
	    for (i = 0; i < entry.nServers; i++) {
		if (entry.serverFlags[i] == ITSRWVOL) {
		    char pname[10];
		    MapPartIdIntoName(entry.serverPartition[i], pname);
		    fprintf(STDERR, " server %s partition %s \n",
			    noresolve ? afs_inet_ntoa_r(entry.serverNumber[i], hoststr) :
                            hostutil_GetNameByINet(entry.serverNumber[i]),
			    pname);
		}
	    }
	    vcode =
		ubik_VL_ReleaseLock(cstruct, 0, afromvol, -1,
			  (LOCKREL_OPCODE | LOCKREL_AFSID |
			   LOCKREL_TIMESTAMP));
	    EGOTO1(mfail, vcode,
		   " Could not release lock on the VLDB entry for the volume %u \n",
		   afromvol);

	    return VOLSERVOLMOVED;
	}

	/* delete the volume afromvol on src_server */
	/* from-info does not exist but to-info does =>
	 * we have already done the move, but the volume
	 * may still be existing physically on from fileserver
	 */
	fromconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);
	pntg = 1;

	code = DoVolDelete(fromconn, afromvol, afrompart,
			   "leftover", 0, NULL, NULL);
	if (code && code != VNOVOL) {
	    error = code;
	    goto mfail;
	}

	code = DoVolDelete(fromconn, backupId, afrompart,
			   "leftover backup", 0, NULL, NULL);
	if (code && code != VNOVOL) {
	    error = code;
	    goto mfail;
	}

	fromtid = 0;
	error = 0;
	goto mfail;
    }

    /* From-info matches the vldb info about volid,
     * its ok start the move operation, the backup volume
     * on the old site is deleted in the process
     */
    if (afrompart == atopart) {
	same = VLDB_IsSameAddrs(afromserver, atoserver, &error);
	EGOTO2(mfail, error,
	       "Failed to get info about server's %d address(es) from vlserver (err=%d); aborting call!\n",
	       afromserver, error);

	if (same) {
	    EGOTO1(mfail, VOLSERVOLMOVED,
		   "Warning: Moving volume %u to its home partition ignored!\n",
		   afromvol);
	}
    }

    pntg = 1;
    toconn = UV_Bind(atoserver, AFSCONF_VOLUMEPORT);	/* get connections to the servers */
    fromconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);
    fromtid = totid = 0;	/* initialize to uncreated */

    /* ***
     * clone the read/write volume locally.
     * ***/

    VPRINT1("Starting transaction on source volume %u ...", afromvol);
    code = AFSVolTransCreate_retry(fromconn, afromvol, afrompart, ITBusy, &tmp);
    fromtid = tmp;
    EGOTO1(mfail, code, "Failed to create transaction on the volume %u\n",
	   afromvol);
    VDONE;

    if (!(flags & RV_NOCLONE)) {
	/* Get a clone id */
	VPRINT1("Allocating new volume id for clone of volume %u ...",
		afromvol);
	newVol = tmpVol = 0;
	vcode = ubik_VL_GetNewVolumeId(cstruct, 0, 1, &tmpVol);
	newVol = tmpVol;
	EGOTO1(mfail, vcode,
	       "Could not get an ID for the clone of volume %u from the VLDB\n",
	       afromvol);
	VDONE;

	/* Do the clone. Default flags on clone are set to delete on salvage and out of service */
	VPRINT1("Cloning source volume %u ...", afromvol);
	strcpy(vname, "move-clone-temp");
	code =
	    AFSVolClone(fromconn, fromtid, 0, readonlyVolume, vname, &tmpVol);
	newVol = tmpVol;
	EGOTO1(mfail, code, "Failed to clone the source volume %u\n",
	       afromvol);
	VDONE;
    }

    /* lookup the name of the volume we just cloned */
    volid = afromvol;
    code = AFSVolGetName(fromconn, fromtid, &volName);
    EGOTO1(mfail, code, "Failed to get the name of the volume %u\n",
	   afromvol);

    VPRINT1("Ending the transaction on the source volume %u ...", afromvol);
    rcode = 0;
    code = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;
    if (!code)
	code = rcode;
    EGOTO1(mfail, code,
	   "Failed to end the transaction on the source volume %u\n",
	   afromvol);
    VDONE;

    /* ***
     * Create the destination volume
     * ***/

    if (!(flags & RV_NOCLONE)) {
	/* All of this is to get the fromDate */
	VPRINT1("Starting transaction on the cloned volume %u ...", newVol);
	tmp = clonetid;
	code =
	    AFSVolTransCreate_retry(fromconn, newVol, afrompart, ITOffline,
			      &tmp);
	clonetid = tmp;
	EGOTO1(mfail, code,
	       "Failed to start a transaction on the cloned volume%u\n",
	       newVol);
	VDONE;

	VPRINT1("Setting flags on cloned volume %u ...", newVol);
	code =
	    AFSVolSetFlags(fromconn, clonetid,
			   VTDeleteOnSalvage | VTOutOfService);	/*redundant */
	EGOTO1(mfail, code, "Could not set flags on the cloned volume %u\n",
	       newVol);
	VDONE;

	/* remember time from which we've dumped the volume */
	VPRINT1("Getting status of cloned volume %u ...", newVol);
	code = AFSVolGetStatus(fromconn, clonetid, &tstatus);
	EGOTO1(mfail, code,
	       "Failed to get the status of the cloned volume %u\n",
	       newVol);
	VDONE;

	fromDate = CLOCKADJ(tstatus.creationDate);
    } else {
	/* With RV_NOCLONE, just do a full copy from the source */
	fromDate = 0;
    }


#ifdef	ENABLE_BUGFIX_1165
    /*
     * Get the internal volume state from the source volume. We'll use such info (i.e. dayUse)
     * to copy it to the new volume (via AFSSetInfo later on) so that when we move volumes we
     * don't use this information...
     */
    volumeInfo.volEntries_val = (volintInfo *) 0;	/*this hints the stub to allocate space */
    volumeInfo.volEntries_len = 0;
    code = AFSVolListOneVolume(fromconn, afrompart, afromvol, &volumeInfo);
    EGOTO1(mfail, code,
	   "Failed to get the volint Info of the cloned volume %u\n",
	   afromvol);

    infop = (volintInfo *) volumeInfo.volEntries_val;
    infop->maxquota = -1;	/* Else it will replace the default quota */
    infop->creationDate = -1;	/* Else it will use the source creation date */
    infop->updateDate = -1;	/* Else it will use the source update date */
#endif

    /* create a volume on the target machine */
    volid = afromvol;
    code = DoVolDelete(toconn, volid, atopart,
		       "pre-existing destination", 0, NULL, NULL);
    if (code && code != VNOVOL) {
	error = code;
	goto mfail;
    }

    VPRINT1("Creating the destination volume %u ...", volid);
    tmp = totid;
    tmpVol = volid;
    code =
	AFSVolCreateVolume(toconn, atopart, volName, volser_RW, volid, &tmpVol,
			   &tmp);
    totid = tmp;
    volid = tmpVol;
    EGOTO1(mfail, code, "Failed to create the destination volume %u\n",
	   volid);
    VDONE;

    strncpy(tmpName, volName, VOLSER_OLDMAXVOLNAME);
    free(volName);
    volName = NULL;

    VPRINT1("Setting volume flags on destination volume %u ...", volid);
    code =
	AFSVolSetFlags(toconn, totid, (VTDeleteOnSalvage | VTOutOfService));
    EGOTO1(mfail, code,
	   "Failed to set the flags on the destination volume %u\n", volid);
    VDONE;

    /***
     * Now dump the clone to the new volume
     ***/

    destination.destHost = ntohl(atoserver);
    destination.destPort = AFSCONF_VOLUMEPORT;
    destination.destSSID = 1;

    strncpy(cookie.name, tmpName, VOLSER_OLDMAXVOLNAME);
    cookie.type = RWVOL;
    cookie.parent = entry.volumeId[RWVOL];
    cookie.clone = 0;

    if (!(flags & RV_NOCLONE)) {
	/* Copy the clone to the new volume */
	VPRINT2("Dumping from clone %u on source to volume %u on destination ...",
		newVol, afromvol);
	code =
	    AFSVolForward(fromconn, clonetid, 0, &destination, totid,
			  &cookie);
	EGOTO1(mfail, code, "Failed to move data for the volume %u\n", volid);
	VDONE;

	VPRINT1("Ending transaction on cloned volume %u ...", newVol);
	code = AFSVolEndTrans(fromconn, clonetid, &rcode);
	if (!code)
	    code = rcode;
	clonetid = 0;
	EGOTO1(mfail, code,
	       "Failed to end the transaction on the cloned volume %u\n",
	       newVol);
	VDONE;
    }

    /* ***
     * reattach to the main-line volume, and incrementally dump it.
     * ***/

    VPRINT1("Starting transaction on source volume %u ...", afromvol);
    tmp = fromtid;
    code = AFSVolTransCreate_retry(fromconn, afromvol, afrompart, ITBusy, &tmp);
    fromtid = tmp;
    EGOTO1(mfail, code,
	   "Failed to create a transaction on the source volume %u\n",
	   afromvol);
    VDONE;

    /* now do the incremental */
    VPRINT2
	("Doing the%s dump from source to destination for volume %u ... ",
	 (flags & RV_NOCLONE) ? "" : " incremental",
	 afromvol);
    code =
	AFSVolForward(fromconn, fromtid, fromDate, &destination, totid,
		      &cookie);
    EGOTO1(mfail, code,
	   "Failed to do the%s dump from rw volume on old site to rw volume on newsite\n",
	  (flags & RV_NOCLONE) ? "" : " incremental");
    VDONE;

    /* now adjust the flags so that the new volume becomes official */
    VPRINT1("Setting volume flags on old source volume %u ...", afromvol);
    code = AFSVolSetFlags(fromconn, fromtid, VTOutOfService);
    EGOTO(mfail, code,
	  "Failed to set the flags to make old source volume offline\n");
    VDONE;

    VPRINT1("Setting volume flags on new source volume %u ...", afromvol);
    code = AFSVolSetFlags(toconn, totid, 0);
    EGOTO(mfail, code,
	  "Failed to set the flags to make new source volume online\n");
    VDONE;

#ifdef	ENABLE_BUGFIX_1165
    VPRINT1("Setting volume status on destination volume %u ...", volid);
    code = AFSVolSetInfo(toconn, totid, infop);
    EGOTO1(mfail, code,
	   "Failed to set volume status on the destination volume %u\n",
	   volid);
    VDONE;
#endif

    /* put new volume online */
    VPRINT1("Ending transaction on destination volume %u ...", afromvol);
    code = AFSVolEndTrans(toconn, totid, &rcode);
    totid = 0;
    if (!code)
	code = rcode;
    EGOTO1(mfail, code,
	   "Failed to end the transaction on the volume %u on the new site\n",
	   afromvol);
    VDONE;

    Lp_SetRWValue(&entry, afromserver, afrompart, atoserver, atopart);
    MapNetworkToHost(&entry, &storeEntry);
    storeEntry.flags &= ~BACK_EXISTS;

    if (TESTC) {
	fprintf(STDOUT,
		"Second test point - operation in progress but not complete.\n");
	fprintf(STDOUT, "...test here (y, n)? ");
	fflush(STDOUT);
	if (fscanf(stdin, "%c", &in) < 1)
	    in = 0;
	if (fscanf(stdin, "%c", &lf) < 0)	/* toss away */
	    ; /* don't care */
	if (in == 'y') {
	    fprintf(STDOUT, "type control-c\n");
	    while (1) {
		fprintf(stdout, ".");
		fflush(stdout);
		sleep(1);
	    }
	}
	/* or drop through */
    }

    VPRINT1("Releasing lock on VLDB entry for volume %u ...", afromvol);
    vcode =
	VLDB_ReplaceEntry(afromvol, -1, &storeEntry,
			  (LOCKREL_OPCODE | LOCKREL_AFSID |
			   LOCKREL_TIMESTAMP));
    if (vcode) {
	fprintf(STDERR,
		" Could not release the lock on the VLDB entry for the volume %s %lu \n",
		storeEntry.name, (unsigned long)afromvol);
	error = vcode;
	goto mfail;
    }
    islocked = 0;
    VDONE;

    if (TESTC) {
	fprintf(STDOUT,
		"Third test point - operation complete but no cleanup.\n");
	fprintf(STDOUT, "...test here (y, n)? ");
	fflush(STDOUT);
	if (fscanf(stdin, "%c", &in) < 1)
	    in = 0;
	if (fscanf(stdin, "%c", &lf) < 0)	/* toss away */
	    ; /* don't care */
	if (in == 'y') {
	    fprintf(STDOUT, "type control-c\n");
	    while (1) {
		fprintf(stdout, ".");
		fflush(stdout);
		sleep(1);
	    }
	}
	/* or drop through */
    }
#ifdef notdef
    /* This is tricky.  File server is very stupid, and if you mark the volume
     * as VTOutOfService, it may mark the *good* instance (if you're moving
     * between partitions on the same machine) as out of service.  Since
     * we're cleaning this code up in DEcorum, we're just going to kludge around
     * it for now by removing this call. */
    /* already out of service, just zap it now */
    code =
	AFSVolSetFlags(fromconn, fromtid, VTDeleteOnSalvage | VTOutOfService);
    if (code) {
	fprintf(STDERR,
		"Failed to set the flags to make the old source volume offline\n");
	goto mfail;
    }
#endif
    if (atoserver != afromserver) {
	/* set forwarding pointer for moved volumes */
	VPRINT1("Setting forwarding pointer for volume %u ...", afromvol);
	code = AFSVolSetForwarding(fromconn, fromtid, atoserver);
	EGOTO1(mfail, code,
	       "Failed to set the forwarding pointer for the volume %u\n",
	       afromvol);
	VDONE;
    }

    VPRINT1("Deleting old volume %u on source ...", afromvol);
    code = AFSVolDeleteVolume(fromconn, fromtid);	/* zap original volume */
    EGOTO1(mfail, code, "Failed to delete the old volume %u on source\n",
	   afromvol);
    VDONE;

    VPRINT1("Ending transaction on old volume %u on the source ...",
	    afromvol);
    code = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;
    if (!code)
	code = rcode;
    EGOTO1(mfail, code,
	   "Failed to end the transaction on the old volume %u on the source\n",
	   afromvol);
    VDONE;

    code = DoVolDelete(fromconn, backupId, afrompart,
		       "source backup", 0, NULL, NULL);
    if (code && code != VNOVOL) {
	error = code;
	goto mfail;
    }

    code = 0;		/* no backup volume? that's okay */

    fromtid = 0;
    if (!(flags & RV_NOCLONE)) {
	code = DoVolDelete(fromconn, newVol, afrompart,
			   "cloned", 0, NULL, NULL);
	if (code) {
	    if (code == VNOVOL) {
		EPRINT1(code, "Failed to start transaction on %u\n", newVol);
	    }
	    error = code;
	    goto mfail;
	}
    }

    /* fall through */
    /* END OF MOVE */

    if (TESTC) {
	fprintf(STDOUT, "Fourth test point - operation complete.\n");
	fprintf(STDOUT, "...test here (y, n)? ");
	fflush(STDOUT);
	if (fscanf(stdin, "%c", &in) < 1)
	    in = 0;
	if (fscanf(stdin, "%c", &lf) < 0)	/* toss away */
	    ; /* don't care */
	if (in == 'y') {
	    fprintf(STDOUT, "type control-c\n");
	    while (1) {
		fprintf(stdout, ".");
		fflush(stdout);
		sleep(1);
	    }
	}
	/* or drop through */
    }

    /* normal cleanup code */

    if (entry.flags & RO_EXISTS)
	fprintf(STDERR, "WARNING : readOnly copies still exist \n");

    if (islocked) {
	VPRINT1("Cleanup: Releasing VLDB lock on volume %u ...", afromvol);
	vcode =
	    ubik_VL_ReleaseLock(cstruct, 0, afromvol, -1,
		      (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	if (vcode) {
	    VPRINT("\n");
	    fprintf(STDERR,
		    " Could not release the lock on the VLDB entry for the volume %lu \n",
		    (unsigned long)afromvol);
	    if (!error)
		error = vcode;
	}
	VDONE;
    }

    if (fromtid) {
	VPRINT1("Cleanup: Ending transaction on source volume %u ...",
		afromvol);
	code = AFSVolEndTrans(fromconn, fromtid, &rcode);
	if (code || rcode) {
	    VPRINT("\n");
	    fprintf(STDERR,
		    "Could not end transaction on the source volume %lu\n",
		    (unsigned long)afromvol);
	    if (!error)
		error = (code ? code : rcode);
	}
	VDONE;
    }

    if (clonetid) {
	VPRINT1("Cleanup: Ending transaction on clone volume %u ...", newVol);
	code = AFSVolEndTrans(fromconn, clonetid, &rcode);
	if (code || rcode) {
	    VPRINT("\n");
	    fprintf(STDERR,
		    "Could not end transaction on the source's clone volume %lu\n",
		    (unsigned long)newVol);
	    if (!error)
		error = (code ? code : rcode);
	}
	VDONE;
    }

    if (totid) {
	VPRINT1("Cleanup: Ending transaction on destination volume %u ...",
		afromvol);
	code = AFSVolEndTrans(toconn, totid, &rcode);
	if (code) {
	    VPRINT("\n");
	    fprintf(STDERR,
		    "Could not end transaction on destination volume %lu\n",
		    (unsigned long)afromvol);
	    if (!error)
		error = (code ? code : rcode);
	}
	VDONE;
    }
    if (volName)
	free(volName);
#ifdef	ENABLE_BUGFIX_1165
    if (infop)
	free(infop);
#endif
    if (fromconn)
	rx_DestroyConnection(fromconn);
    if (toconn)
	rx_DestroyConnection(toconn);
    PrintError("", error);
    return error;

    /* come here only when the sky falls */
  mfail:

    if (pntg) {
	fprintf(STDOUT,
		"vos move: operation interrupted, cleanup in progress...\n");
	fprintf(STDOUT, "clear transaction contexts\n");
	fflush(STDOUT);
    }

    /* unlock VLDB entry */
    if (islocked) {
	VPRINT1("Recovery: Releasing VLDB lock on volume %u ...", afromvol);
	ubik_VL_ReleaseLock(cstruct, 0, afromvol, -1,
		  (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	VDONE;
	islocked = 0;
    }

    if (clonetid) {
	VPRINT("Recovery: Ending transaction on clone volume ...");
	AFSVolEndTrans(fromconn, clonetid, &rcode);
	VDONE;
    }
    if (totid) {
	VPRINT("Recovery: Ending transaction on destination volume ...");
	AFSVolEndTrans(toconn, totid, &rcode);
	VDONE;
    }
    if (fromtid) {		/* put it on-line */
	VPRINT("Recovery: Setting volume flags on source volume ...");
	AFSVolSetFlags(fromconn, fromtid, 0);
	VDONE;

	VPRINT("Recovery: Ending transaction on source volume ...");
	AFSVolEndTrans(fromconn, fromtid, &rcode);
	VDONE;
    }

    VPRINT("Recovery: Accessing VLDB.\n");
    vcode = VLDB_GetEntryByID(afromvol, -1, &entry);
    if (vcode) {
	fprintf(STDOUT, "FATAL: VLDB access error: abort cleanup\n");
	fflush(STDOUT);
	goto done;
    }
    MapHostToNetwork(&entry);

    /* Delete either the volume on the source location or the target location.
     * If the vldb entry still points to the source location, then we know the
     * volume move didn't finish so we remove the volume from the target
     * location. Otherwise, we remove the volume from the source location.
     */
    if (Lp_Match(afromserver, afrompart, &entry)) {	/* didn't move - delete target volume */
	if (pntg) {
	    fprintf(STDOUT,
		    "move incomplete - attempt cleanup of target partition - no guarantee\n");
	    fflush(STDOUT);
	}

	if (volid && toconn) {
	    code = DoVolDelete(toconn, volid, atopart,
			       "destination", 0, NULL, "Recovery:");
	    if (code == VNOVOL) {
		EPRINT1(code, "Recovery: Failed to start transaction on %u\n", volid);
	    }
        }

	/* put source volume on-line */
	if (fromconn) {
	    VPRINT1("Recovery: Creating transaction on source volume %u ...",
		    afromvol);
	    tmp = fromtid;
	    code =
		AFSVolTransCreate_retry(fromconn, afromvol, afrompart, ITBusy,
				  &tmp);
	    fromtid = tmp;
	    if (!code) {
		VDONE;

		VPRINT1("Recovery: Setting flags on source volume %u ...",
			afromvol);
		AFSVolSetFlags(fromconn, fromtid, 0);
		VDONE;

		VPRINT1
		    ("Recovery: Ending transaction on source volume %u ...",
		     afromvol);
		AFSVolEndTrans(fromconn, fromtid, &rcode);
		VDONE;
	    } else {
		VPRINT1
		    ("\nRecovery: Unable to start transaction on source volume %u.\n",
		     afromvol);
	    }
	}
    } else {			/* yep, move complete */
	if (pntg) {
	    fprintf(STDOUT,
		    "move complete - attempt cleanup of source partition - no guarantee\n");
	    fflush(STDOUT);
	}

	/* delete backup volume */
	if (fromconn) {
	    code = DoVolDelete(fromconn, backupId, afrompart,
			       "backup", 0, NULL, "Recovery:");
	    if (code == VNOVOL) {
		EPRINT1(code, "Recovery: Failed to start transaction on %u\n", backupId);
	    }

	    code = DoVolDelete(fromconn, afromvol, afrompart, "source",
			       (atoserver != afromserver)?atoserver:0,
			       NULL, NULL);
	    if (code == VNOVOL) {
		EPRINT1(code, "Failed to start transaction on %u\n", afromvol);
	    }
	}
    }

    /* common cleanup - delete local clone */
    if (newVol) {
	code = DoVolDelete(fromconn, newVol, afrompart,
			   "clone", 0, NULL, "Recovery:");
	if (code == VNOVOL) {
	    EPRINT1(code, "Recovery: Failed to start transaction on %u\n", newVol);
	}
    }

    /* unlock VLDB entry */
    if (islocked) {
	VPRINT1("Recovery: Releasing lock on VLDB entry for volume %u ...",
		afromvol);
	ubik_VL_ReleaseLock(cstruct, 0, afromvol, -1,
			    (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	VDONE;
	islocked = 0;
    }
  done:			/* routine cleanup */
    if (volName)
	free(volName);
#ifdef	ENABLE_BUGFIX_1165
    if (infop)
	free(infop);
#endif
    if (fromconn)
	rx_DestroyConnection(fromconn);
    if (toconn)
	rx_DestroyConnection(toconn);

    if (pntg) {
	fprintf(STDOUT, "cleanup complete - user verify desired result\n");
	fflush(STDOUT);
    }
    exit(1);
}


int
UV_MoveVolume(afs_uint32 afromvol, afs_uint32 afromserver, afs_int32 afrompart,
	      afs_uint32 atoserver, afs_int32 atopart)
{
    return UV_MoveVolume2(afromvol, afromserver, afrompart,
			  atoserver, atopart, 0);
}


/* Copy volume <afromvol> from <afromserver> <afrompart> to <atoserver>
 * <atopart>.  The new volume is named by <atovolname>.  The new volume
 * has ID <atovolid> if that is nonzero; otherwise a new ID is allocated
 * from the VLDB.  the following flags are supported:
 *
 *     RV_RDONLY  - target volume is RO
 *     RV_OFFLINE - leave target volume offline
 *     RV_CPINCR  - do incremental dump if target exists
 *     RV_NOVLDB  - don't create/update VLDB entry
 *     RV_NOCLONE - don't use a copy clone
 */
int
UV_CopyVolume2(afs_uint32 afromvol, afs_uint32 afromserver, afs_int32 afrompart,
	       char *atovolname, afs_uint32 atoserver, afs_int32 atopart,
	       afs_uint32 atovolid, int flags)
{
    /* declare stuff 'volatile' that may be used from setjmp/longjmp and may
     * be changing during the copy */
    int volatile pntg;
    afs_int32 volatile clonetid;
    afs_int32 volatile totid;
    afs_int32 volatile fromtid;
    struct rx_connection * volatile fromconn;
    struct rx_connection * volatile toconn;
    afs_uint32 volatile cloneVol;

    char vname[64];
    afs_int32 rcode;
    afs_int32 fromDate, cloneFromDate;
    struct restoreCookie cookie;
    afs_int32 vcode, code;
    afs_uint32 newVol;
    afs_int32 volflag;
    struct volser_status tstatus;
    struct destServer destination;
    struct nvldbentry entry, newentry, storeEntry;
    afs_int32 error;
    afs_int32 tmp;
    afs_uint32 tmpVol;

    fromconn = (struct rx_connection *)0;
    toconn = (struct rx_connection *)0;
    fromtid = 0;
    totid = 0;
    clonetid = 0;
    error = 0;
    pntg = 0;
    newVol = 0;

    /* support control-c processing */
    if (setjmp(env))
	goto mfail;
    (void)signal(SIGINT, sigint_handler);

    vcode = VLDB_GetEntryByID(afromvol, -1, &entry);
    EGOTO1(mfail, vcode,
	   "Could not fetch the entry for the volume  %u from the VLDB \n",
	   afromvol);
    MapHostToNetwork(&entry);

    pntg = 1;
    toconn = UV_Bind(atoserver, AFSCONF_VOLUMEPORT);	/* get connections to the servers */
    fromconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);
    fromtid = totid = 0;	/* initialize to uncreated */

    /* ***
     * clone the read/write volume locally.
     * ***/

    cloneVol = 0;
    if (!(flags & RV_NOCLONE)) {
	VPRINT1("Starting transaction on source volume %u ...", afromvol);
	tmp = fromtid;
	code = AFSVolTransCreate_retry(fromconn, afromvol, afrompart, ITBusy,
				 &tmp);
	fromtid = tmp;
	EGOTO1(mfail, code, "Failed to create transaction on the volume %u\n",
	       afromvol);
	VDONE;

	/* Get a clone id */
	VPRINT1("Allocating new volume id for clone of volume %u ...",
		afromvol);
	cloneVol = 0;
	tmpVol = cloneVol;
	vcode = ubik_VL_GetNewVolumeId(cstruct, 0, 1, &tmpVol);
	cloneVol = tmpVol;
	EGOTO1(mfail, vcode,
	   "Could not get an ID for the clone of volume %u from the VLDB\n",
	   afromvol);
	VDONE;
    }

    if (atovolid) {
	newVol = atovolid;
    } else {
	/* Get a new volume id */
	VPRINT1("Allocating new volume id for copy of volume %u ...", afromvol);
	newVol = 0;
	vcode = ubik_VL_GetNewVolumeId(cstruct, 0, 1, &newVol);
	EGOTO1(mfail, vcode,
	       "Could not get an ID for the copy of volume %u from the VLDB\n",
	       afromvol);
	VDONE;
    }

    if (!(flags & RV_NOCLONE)) {
	/* Do the clone. Default flags on clone are set to delete on salvage and out of service */
	VPRINT1("Cloning source volume %u ...", afromvol);
	strcpy(vname, "copy-clone-temp");
	tmpVol = cloneVol;
	code =
	    AFSVolClone(fromconn, fromtid, 0, readonlyVolume, vname,
			&tmpVol);
	cloneVol = tmpVol;
	EGOTO1(mfail, code, "Failed to clone the source volume %u\n",
	       afromvol);
	VDONE;

	VPRINT1("Ending the transaction on the source volume %u ...", afromvol);
	rcode = 0;
	code = AFSVolEndTrans(fromconn, fromtid, &rcode);
	fromtid = 0;
	if (!code)
	    code = rcode;
	EGOTO1(mfail, code,
	       "Failed to end the transaction on the source volume %u\n",
	       afromvol);
	VDONE;
    }

    /* ***
     * Create the destination volume
     * ***/

    if (!(flags & RV_NOCLONE)) {
	VPRINT1("Starting transaction on the cloned volume %u ...", cloneVol);
	tmp = clonetid;
	code =
	    AFSVolTransCreate_retry(fromconn, cloneVol, afrompart, ITOffline,
			  &tmp);
	clonetid = tmp;
	EGOTO1(mfail, code,
	       "Failed to start a transaction on the cloned volume%u\n",
	       cloneVol);
	VDONE;

	VPRINT1("Setting flags on cloned volume %u ...", cloneVol);
	code =
	    AFSVolSetFlags(fromconn, clonetid,
			   VTDeleteOnSalvage | VTOutOfService);	/*redundant */
	EGOTO1(mfail, code, "Could not set flags on the cloned volume %u\n",
	       cloneVol);
	VDONE;

	/* remember time from which we've dumped the volume */
	VPRINT1("Getting status of cloned volume %u ...", cloneVol);
	code = AFSVolGetStatus(fromconn, clonetid, &tstatus);
	EGOTO1(mfail, code,
	       "Failed to get the status of the cloned volume %u\n",
	       cloneVol);
	VDONE;

	fromDate = CLOCKADJ(tstatus.creationDate);
    } else {
	fromDate = 0;
    }

    /* create a volume on the target machine */
    cloneFromDate = 0;
    tmp = totid;
    code = AFSVolTransCreate_retry(toconn, newVol, atopart, ITOffline, &tmp);
    totid = tmp;
    if (!code) {
	if ((flags & RV_CPINCR)) {
	    VPRINT1("Getting status of pre-existing volume %u ...", newVol);
	    code = AFSVolGetStatus(toconn, totid, &tstatus);
	    EGOTO1(mfail, code,
		   "Failed to get the status of the pre-existing volume %u\n",
		   newVol);
	    VDONE;

	    /* Using the update date should be OK here, but add some fudge */
	    cloneFromDate = CLOCKADJ(tstatus.updateDate);
	    if ((flags & RV_NOCLONE))
		fromDate = cloneFromDate;

	    /* XXX We should check that the source volume's creationDate is
	     * XXX not newer than the existing target volume, and if not,
	     * XXX throw away the existing target and do a full dump. */

	    goto cpincr;
	}

	/* Delete the existing volume.
	 * While we are deleting the volume in these steps, the transaction
	 * we started against the cloned volume (clonetid above) will be
	 * sitting idle. It will get cleaned up after 600 seconds
	 */
	VPRINT1("Deleting pre-existing volume %u on destination ...", newVol);
	code = AFSVolDeleteVolume(toconn, totid);
	EGOTO1(mfail, code,
	       "Could not delete the pre-existing volume %u on destination\n",
	       newVol);
	VDONE;

	VPRINT1
	    ("Ending transaction on pre-existing volume %u on destination ...",
	     newVol);
	code = AFSVolEndTrans(toconn, totid, &rcode);
	totid = 0;
	if (!code)
	    code = rcode;
	EGOTO1(mfail, code,
	       "Could not end the transaction on pre-existing volume %u on destination\n",
	       newVol);
	VDONE;
    }

    VPRINT1("Creating the destination volume %u ...", newVol);
    tmp = totid;
    code =
	AFSVolCreateVolume(toconn, atopart, atovolname,
			   (flags & RV_RDONLY) ? volser_RO : volser_RW,
			   newVol, &newVol, &tmp);
    totid = tmp;
    EGOTO1(mfail, code, "Failed to create the destination volume %u\n",
	   newVol);
    VDONE;

    VPRINT1("Setting volume flags on destination volume %u ...", newVol);
    code =
	AFSVolSetFlags(toconn, totid, (VTDeleteOnSalvage | VTOutOfService));
    EGOTO1(mfail, code,
	   "Failed to set the flags on the destination volume %u\n", newVol);
    VDONE;

cpincr:

    destination.destHost = ntohl(atoserver);
    destination.destPort = AFSCONF_VOLUMEPORT;
    destination.destSSID = 1;

    strncpy(cookie.name, atovolname, VOLSER_OLDMAXVOLNAME);
    cookie.type = (flags & RV_RDONLY) ? ROVOL : RWVOL;
    cookie.parent = 0;
    cookie.clone = 0;

    /***
     * Now dump the clone to the new volume
     ***/

    if (!(flags & RV_NOCLONE)) {
	/* XXX probably should have some code here that checks to see if
	 * XXX we are copying to same server and partition - if so, just
	 * XXX use a clone to save disk space */

	/* Copy the clone to the new volume */
	VPRINT2("Dumping from clone %u on source to volume %u on destination ...",
	    cloneVol, newVol);
	code =
	    AFSVolForward(fromconn, clonetid, cloneFromDate, &destination,
			  totid, &cookie);
	EGOTO1(mfail, code, "Failed to move data for the volume %u\n",
	       newVol);
	VDONE;

	VPRINT1("Ending transaction on cloned volume %u ...", cloneVol);
	code = AFSVolEndTrans(fromconn, clonetid, &rcode);
	if (!code)
	    code = rcode;
	clonetid = 0;
	EGOTO1(mfail, code,
	       "Failed to end the transaction on the cloned volume %u\n",
	       cloneVol);
	VDONE;
    }

    /* ***
     * reattach to the main-line volume, and incrementally dump it.
     * ***/

    VPRINT1("Starting transaction on source volume %u ...", afromvol);
    tmp = fromtid;
    code = AFSVolTransCreate_retry(fromconn, afromvol, afrompart, ITBusy, &tmp);
    fromtid = tmp;
    EGOTO1(mfail, code,
	   "Failed to create a transaction on the source volume %u\n",
	   afromvol);
    VDONE;

    /* now do the incremental */
    VPRINT2
	("Doing the%s dump from source to destination for volume %u ... ",
	 (flags & RV_NOCLONE) ? "" : " incremental",
	 afromvol);
    code =
	AFSVolForward(fromconn, fromtid, fromDate, &destination, totid,
		      &cookie);
    EGOTO1(mfail, code,
	   "Failed to do the%s dump from old site to new site\n",
	   (flags & RV_NOCLONE) ? "" : " incremental");
    VDONE;

    VPRINT1("Setting volume flags on destination volume %u ...", newVol);
    volflag = ((flags & RV_OFFLINE) ? VTOutOfService : 0);	/* off or on-line */
    code = AFSVolSetFlags(toconn, totid, volflag);
    EGOTO(mfail, code,
	  "Failed to set the flags to make destination volume online\n");
    VDONE;

    /* put new volume online */
    VPRINT1("Ending transaction on destination volume %u ...", newVol);
    code = AFSVolEndTrans(toconn, totid, &rcode);
    totid = 0;
    if (!code)
	code = rcode;
    EGOTO1(mfail, code,
	   "Failed to end the transaction on the destination volume %u\n",
	   newVol);
    VDONE;

    VPRINT1("Ending transaction on source volume %u ...", afromvol);
    code = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;
    if (!code)
	code = rcode;
    EGOTO1(mfail, code,
	   "Failed to end the transaction on the source volume %u\n",
	   afromvol);
    VDONE;

    fromtid = 0;

    if (!(flags & RV_NOCLONE)) {
	code = DoVolDelete(fromconn, cloneVol, afrompart,
			   "cloned", 0, NULL, NULL);
	if (code) {
	    if (code == VNOVOL) {
		EPRINT1(code, "Failed to start transaction on %u\n", cloneVol);
	    }
	    error = code;
	    goto mfail;
	}
    }

    if (!(flags & RV_NOVLDB)) {
	/* create the vldb entry for the copied volume */
	strncpy(newentry.name, atovolname, VOLSER_OLDMAXVOLNAME);
	newentry.nServers = 1;
	newentry.serverNumber[0] = atoserver;
	newentry.serverPartition[0] = atopart;
	newentry.flags = (flags & RV_RDONLY) ? RO_EXISTS : RW_EXISTS;
	newentry.serverFlags[0] = (flags & RV_RDONLY) ? ITSROVOL : ITSRWVOL;
	newentry.volumeId[RWVOL] = newVol;
	newentry.volumeId[ROVOL] = (flags & RV_RDONLY) ? newVol : 0;
	newentry.volumeId[BACKVOL] = 0;
	newentry.cloneId = 0;
	/*map into right byte order, before passing to xdr, the stuff has to be in host
	 * byte order. Xdr converts it into network order */
	MapNetworkToHost(&newentry, &storeEntry);
	/* create the vldb entry */
	vcode = VLDB_CreateEntry(&storeEntry);
	if (vcode) {
	    fprintf(STDERR,
		    "Could not create a VLDB entry for the volume %s %lu\n",
		    atovolname, (unsigned long)newVol);
	    /*destroy the created volume */
	    VPRINT1("Deleting the newly created volume %u\n", newVol);
	    AFSVolDeleteVolume(toconn, totid);
	    error = vcode;
	    goto mfail;
	}
	VPRINT2("Created the VLDB entry for the volume %s %u\n", atovolname,
		newVol);
    }

    /* normal cleanup code */

    if (fromtid) {
	VPRINT1("Cleanup: Ending transaction on source volume %u ...",
		afromvol);
	code = AFSVolEndTrans(fromconn, fromtid, &rcode);
	if (code || rcode) {
	    VPRINT("\n");
	    fprintf(STDERR,
		    "Could not end transaction on the source volume %lu\n",
		    (unsigned long)afromvol);
	    if (!error)
		error = (code ? code : rcode);
	}
	VDONE;
    }

    if (clonetid) {
	VPRINT1("Cleanup: Ending transaction on clone volume %u ...",
		cloneVol);
	code = AFSVolEndTrans(fromconn, clonetid, &rcode);
	if (code || rcode) {
	    VPRINT("\n");
	    fprintf(STDERR,
		    "Could not end transaction on the source's clone volume %lu\n",
		    (unsigned long)cloneVol);
	    if (!error)
		error = (code ? code : rcode);
	}
	VDONE;
    }

    if (totid) {
	VPRINT1("Cleanup: Ending transaction on destination volume %u ...",
		newVol);
	code = AFSVolEndTrans(toconn, totid, &rcode);
	if (code) {
	    VPRINT("\n");
	    fprintf(STDERR,
		    "Could not end transaction on destination volume %lu\n",
		    (unsigned long)newVol);
	    if (!error)
		error = (code ? code : rcode);
	}
	VDONE;
    }
    if (fromconn)
	rx_DestroyConnection(fromconn);
    if (toconn)
	rx_DestroyConnection(toconn);
    PrintError("", error);
    return error;

    /* come here only when the sky falls */
  mfail:

    if (pntg) {
	fprintf(STDOUT,
		"vos copy: operation interrupted, cleanup in progress...\n");
	fprintf(STDOUT, "clear transaction contexts\n");
	fflush(STDOUT);
    }

    if (clonetid) {
	VPRINT("Recovery: Ending transaction on clone volume ...");
	AFSVolEndTrans(fromconn, clonetid, &rcode);
	VDONE;
    }
    if (totid) {
	VPRINT("Recovery: Ending transaction on destination volume ...");
	AFSVolEndTrans(toconn, totid, &rcode);
	VDONE;
    }
    if (fromtid) {		/* put it on-line */
	VPRINT("Recovery: Ending transaction on source volume ...");
	AFSVolEndTrans(fromconn, fromtid, &rcode);
	VDONE;
    }

    VPRINT("Recovery: Accessing VLDB.\n");
    vcode = VLDB_GetEntryByID(afromvol, -1, &entry);
    if (vcode) {
	fprintf(STDOUT, "FATAL: VLDB access error: abort cleanup\n");
	fflush(STDOUT);
	goto done;
    }
    MapHostToNetwork(&entry);

    /* common cleanup - delete local clone */
    if (cloneVol) {
	code = DoVolDelete(fromconn, cloneVol, afrompart,
			   "clone", 0, NULL, "Recovery:");
	if (code == VNOVOL) {
	    EPRINT1(code, "Recovery: Failed to start transaction on %u\n", cloneVol);
	}
    }

  done:			/* routine cleanup */
    if (fromconn)
	rx_DestroyConnection(fromconn);
    if (toconn)
	rx_DestroyConnection(toconn);

    if (pntg) {
	fprintf(STDOUT, "cleanup complete - user verify desired result\n");
	fflush(STDOUT);
    }
    exit(1);
}


int
UV_CopyVolume(afs_uint32 afromvol, afs_uint32 afromserver, afs_int32 afrompart,
	      char *atovolname, afs_uint32 atoserver, afs_int32 atopart)
{
    return UV_CopyVolume2(afromvol, afromserver, afrompart,
                          atovolname, atoserver, atopart, 0, 0);
}



/* Make a new backup of volume <avolid> on <aserver> and <apart>
 * if one already exists, update it
 */

int
UV_BackupVolume(afs_uint32 aserver, afs_int32 apart, afs_uint32 avolid)
{
    struct rx_connection *aconn = (struct rx_connection *)0;
    afs_int32 ttid = 0, btid = 0;
    afs_uint32 backupID;
    afs_int32 code = 0, rcode = 0;
    struct nvldbentry entry, storeEntry;
    afs_int32 error = 0;
    int vldblocked = 0, vldbmod = 0;

    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);

    /* the calls to VLDB will succeed only if avolid is a RW volume,
     * since we are following the RW hash chain for searching */
    code = VLDB_GetEntryByID(avolid, RWVOL, &entry);
    if (code) {
	fprintf(STDERR,
		"Could not fetch the entry for the volume %lu from the VLDB \n",
		(unsigned long)avolid);
	error = code;
	goto bfail;
    }
    MapHostToNetwork(&entry);

    /* These operations require the VLDB be locked since it means the VLDB
     * will change or the vldb is already locked.
     */
    if (!(entry.flags & BACK_EXISTS) ||	/* backup volume doesnt exist */
	(entry.flags & VLOP_ALLOPERS) ||	/* vldb lock already held */
	(entry.volumeId[BACKVOL] == INVALID_BID)) {	/* no assigned backup volume id */

	code = ubik_VL_SetLock(cstruct, 0, avolid, RWVOL, VLOP_BACKUP);
	if (code) {
	    fprintf(STDERR,
		    "Could not lock the VLDB entry for the volume %lu\n",
		    (unsigned long)avolid);
	    error = code;
	    goto bfail;
	}
	vldblocked = 1;

	/* Reread the vldb entry */
	code = VLDB_GetEntryByID(avolid, RWVOL, &entry);
	if (code) {
	    fprintf(STDERR,
		    "Could not fetch the entry for the volume %lu from the VLDB \n",
		    (unsigned long)avolid);
	    error = code;
	    goto bfail;
	}
	MapHostToNetwork(&entry);
    }

    if (!ISNAMEVALID(entry.name)) {
	fprintf(STDERR, "Name of the volume %s exceeds the size limit\n",
		entry.name);
	error = VOLSERBADNAME;
	goto bfail;
    }

    backupID = entry.volumeId[BACKVOL];
    if (backupID == INVALID_BID) {
	/* Get a backup volume id from the VLDB and update the vldb
	 * entry with it.
	 */
	code = ubik_VL_GetNewVolumeId(cstruct, 0, 1, &backupID);
	if (code) {
	    fprintf(STDERR,
		    "Could not allocate ID for the backup volume of  %lu from the VLDB\n",
		    (unsigned long)avolid);
	    error = code;
	    goto bfail;
	}
	entry.volumeId[BACKVOL] = backupID;
	vldbmod = 1;
    }

    code = DoVolClone(aconn, avolid, apart, backupVolume, backupID, "backup",
		      entry.name, NULL, ".backup", NULL, NULL);
    if (code) {
	error = code;
	goto bfail;
    }

    /* Mark vldb as backup exists */
    if (!(entry.flags & BACK_EXISTS)) {
	entry.flags |= BACK_EXISTS;
	vldbmod = 1;
    }

    /* Now go back to the backup volume and bring it on line */
    code = AFSVolTransCreate_retry(aconn, backupID, apart, ITOffline, &btid);
    if (code) {
	fprintf(STDERR,
		"Failed to start a transaction on the backup volume %lu\n",
		(unsigned long)backupID);
	error = code;
	goto bfail;
    }

    code = AFSVolSetFlags(aconn, btid, 0);
    if (code) {
	fprintf(STDERR, "Could not mark the backup volume %lu on line \n",
		(unsigned long)backupID);
	error = code;
	goto bfail;
    }

    code = AFSVolEndTrans(aconn, btid, &rcode);
    btid = 0;
    if (code || rcode) {
	fprintf(STDERR,
		"Failed to end the transaction on the backup volume %lu\n",
		(unsigned long)backupID);
	error = (code ? code : rcode);
	goto bfail;
    }

    VDONE;

    /* Will update the vldb below */

  bfail:
    if (ttid) {
	code = AFSVolEndTrans(aconn, ttid, &rcode);
	if (code || rcode) {
	    fprintf(STDERR, "Could not end transaction on the volume %lu\n",
		    (unsigned long)avolid);
	    if (!error)
		error = (code ? code : rcode);
	}
    }

    if (btid) {
	code = AFSVolEndTrans(aconn, btid, &rcode);
	if (code || rcode) {
	    fprintf(STDERR,
		    "Could not end transaction the backup volume %lu\n",
		    (unsigned long)backupID);
	    if (!error)
		error = (code ? code : rcode);
	}
    }

    /* Now update the vldb - if modified */
    if (vldblocked) {
	if (vldbmod) {
	    MapNetworkToHost(&entry, &storeEntry);
	    code =
		VLDB_ReplaceEntry(avolid, RWVOL, &storeEntry,
				  (LOCKREL_OPCODE | LOCKREL_AFSID |
				   LOCKREL_TIMESTAMP));
	    if (code) {
		fprintf(STDERR,
			"Could not update the VLDB entry for the volume %lu \n",
			(unsigned long)avolid);
		if (!error)
		    error = code;
	    }
	} else {
	    code =
		ubik_VL_ReleaseLock(cstruct, 0, avolid, RWVOL,
			  (LOCKREL_OPCODE | LOCKREL_AFSID |
			   LOCKREL_TIMESTAMP));
	    if (code) {
		fprintf(STDERR,
			"Could not unlock the VLDB entry for the volume %lu \n",
			(unsigned long)avolid);
		if (!error)
		    error = code;
	    }
	}
    }

    if (aconn)
	rx_DestroyConnection(aconn);

    PrintError("", error);
    return error;
}

/* Make a new clone of volume <avolid> on <aserver> and <apart>
 * using volume ID <acloneid>, or a new ID allocated from the VLDB.
 * The new volume is named by <aname>, or by appending ".clone" to
 * the existing name if <aname> is NULL.  The following flags are
 * supported:
 *
 *     RV_RDONLY  - target volume is RO
 *     RV_OFFLINE - leave target volume offline
 */

int
UV_CloneVolume(afs_uint32 aserver, afs_int32 apart, afs_uint32 avolid,
	       afs_uint32 acloneid, char *aname, int flags)
{
    struct rx_connection *aconn = (struct rx_connection *)0;
    afs_int32 ttid = 0, btid = 0;
    afs_int32 code = 0, rcode = 0;
    char vname[VOLSER_MAXVOLNAME + 1];
    afs_int32 error = 0;
    volEntries volumeInfo;
    int type = 0;

    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);

    if (!aname) {
	volumeInfo.volEntries_val = (volintInfo *) 0;
	volumeInfo.volEntries_len = 0;
	code = AFSVolListOneVolume(aconn, apart, avolid, &volumeInfo);
	if (code) {
	    fprintf(stderr, "Could not get info for volume %lu\n",
		    (unsigned long)avolid);
	    error = code;
	    goto bfail;
	}
	strncpy(vname, volumeInfo.volEntries_val[0].name,
		VOLSER_OLDMAXVOLNAME - 7);
	vname[VOLSER_OLDMAXVOLNAME - 7] = 0;
	strcat(vname, ".clone");
	aname = vname;
	if (volumeInfo.volEntries_val)
	    free(volumeInfo.volEntries_val);
    }

    if (!acloneid) {
	/* Get a clone id */
	VPRINT1("Allocating new volume id for clone of volume %u ...",
		avolid);
	code = ubik_VL_GetNewVolumeId(cstruct, 0, 1, &acloneid);
	EGOTO1(bfail, code,
	   "Could not get an ID for the clone of volume %u from the VLDB\n",
	   avolid);
	VDONE;
    }

    if (flags & RV_RWONLY)
	type = readwriteVolume;
    else if (flags & RV_RDONLY)
	type = readonlyVolume;
    else
	type = backupVolume;

    code = DoVolClone(aconn, avolid, apart, type, acloneid, "clone",
		      NULL, aname, NULL, NULL, NULL);
    if (code) {
	error = code;
	goto bfail;
    }

    /* Now go back to the backup volume and bring it on line */
    if (!(flags & RV_OFFLINE)) {
	code = AFSVolTransCreate_retry(aconn, acloneid, apart, ITOffline, &btid);
	if (code) {
	    fprintf(STDERR,
		    "Failed to start a transaction on the clone volume %lu\n",
		    (unsigned long)acloneid);
	    error = code;
	    goto bfail;
	}

	code = AFSVolSetFlags(aconn, btid, 0);
	if (code) {
	    fprintf(STDERR, "Could not mark the clone volume %lu on line \n",
		    (unsigned long)acloneid);
	    error = code;
	    goto bfail;
	}

	code = AFSVolEndTrans(aconn, btid, &rcode);
	btid = 0;
	if (code || rcode) {
	    fprintf(STDERR,
		    "Failed to end the transaction on the clone volume %lu\n",
		    (unsigned long)acloneid);
	    error = (code ? code : rcode);
	    goto bfail;
	}
    }

    VDONE;

  bfail:
    if (ttid) {
	code = AFSVolEndTrans(aconn, ttid, &rcode);
	if (code || rcode) {
	    fprintf(STDERR, "Could not end transaction on the volume %lu\n",
		    (unsigned long)avolid);
	    if (!error)
		error = (code ? code : rcode);
	}
    }

    if (btid) {
	code = AFSVolEndTrans(aconn, btid, &rcode);
	if (code || rcode) {
	    fprintf(STDERR,
		    "Could not end transaction on the clone volume %lu\n",
		    (unsigned long)acloneid);
	    if (!error)
		error = (code ? code : rcode);
	}
    }

    if (aconn)
	rx_DestroyConnection(aconn);

    PrintError("", error);
    return error;
}

#define ONERROR(ec, ep, es) do { \
    if (ec) { \
        fprintf(STDERR, (es), (ep)); \
        error = (ec); \
        goto rfail; \
    } \
} while (0)
#define ONERROR0(ec, es) do { \
    if (ec) { \
        fprintf(STDERR, (es)); \
        error = (ec); \
        goto rfail; \
    } \
} while (0)
#define ERROREXIT(ec) do { \
    error = (ec); \
    goto rfail; \
} while (0)

/* Get a "transaction" on this replica.  Create the volume
 * if necessary.  Return the time from which a dump should
 * be made (0 if it's a new volume)
 */
static int
GetTrans(struct nvldbentry *vldbEntryPtr, afs_int32 index,
	 struct rx_connection **connPtr, afs_int32 * transPtr,
	 afs_uint32 * crtimePtr, afs_uint32 * uptimePtr,
	 afs_int32 *origflags, afs_uint32 tmpVolId)
{
    afs_uint32 volid;
    struct volser_status tstatus;
    int code = 0;
    int rcode, tcode;
    char hoststr[16];

    *connPtr = (struct rx_connection *)0;
    *transPtr = 0;
    *crtimePtr = 0;
    *uptimePtr = 0;

    /* get connection to the replication site */
    *connPtr = UV_Bind(vldbEntryPtr->serverNumber[index], AFSCONF_VOLUMEPORT);
    if (!*connPtr)
	goto fail;		/* server is down */

    volid = vldbEntryPtr->volumeId[ROVOL];

    if (volid) {
	code =
	    AFSVolTransCreate_retry(*connPtr, volid,
			      vldbEntryPtr->serverPartition[index], ITOffline,
			      transPtr);

	if (!code && (origflags[index] & RO_DONTUSE)) {
	    /* If RO_DONTUSE is set, this is supposed to be an entirely new
	     * site. Don't trust any data on it, since it is possible we
	     * have encountered some temporary volume from some other
	     * incomplete volume operation. It is difficult to detect if
	     * that has happened vs if this is a legit volume, so just
	     * delete it to be safe. */

	    VPRINT1("Deleting extant RO_DONTUSE site on %s...",
                    noresolve ? afs_inet_ntoa_r(vldbEntryPtr->
                                                serverNumber[index], hoststr) :
                    hostutil_GetNameByINet(vldbEntryPtr->
					   serverNumber[index]));

	    code = AFSVolDeleteVolume(*connPtr, *transPtr);
	    if (code) {
		PrintError("Failed to delete RO_DONTUSE site: ", code);
		goto fail;
	    }

	    tcode = AFSVolEndTrans(*connPtr, *transPtr, &rcode);
	    *transPtr = 0;
	    if (!tcode) {
		tcode = rcode;
	    }
	    if (tcode) {
		PrintError("Failed to end transaction on RO_DONTUSE site: ",
		           tcode);
		goto fail;
	    }

	    VDONE;

	    /* emulate what TransCreate would have returned, so we try to
	     * create the volume below */
	    code = VNOVOL;
	}
    }

    /* If the volume does not exist, create it */
    if (!volid || code) {
	char volname[VL_MAXNAMELEN];
        char hoststr[16];

	if (volid && (code != VNOVOL)) {
	    PrintError("Failed to start a transaction on the RO volume.\n",
		       code);
	    goto fail;
	}

	strlcpy(volname, vldbEntryPtr->name, sizeof(volname));

	if (strlcat(volname,
		    tmpVolId?".roclone":".readonly",
		    sizeof(volname)) >= sizeof(volname)) {
	    code = ENOMEM;
	    PrintError("Volume name is too long\n", code);
	    goto fail;
	}

	if (verbose) {
	    fprintf(STDOUT,
		    "Creating new volume %lu on replication site %s: ",
		    tmpVolId?(unsigned long)tmpVolId:(unsigned long)volid,
                    noresolve ? afs_inet_ntoa_r(vldbEntryPtr->
                                                serverNumber[index], hoststr) :
                    hostutil_GetNameByINet(vldbEntryPtr->
					   serverNumber[index]));
	    fflush(STDOUT);
	}

	code =
	  AFSVolCreateVolume(*connPtr, vldbEntryPtr->serverPartition[index],
			     volname, volser_RO,
			     vldbEntryPtr->volumeId[RWVOL],
			     tmpVolId?&tmpVolId:&volid,
			     transPtr);
	if (code) {
	    PrintError("Failed to create the ro volume: ", code);
	    goto fail;
	}
	vldbEntryPtr->volumeId[ROVOL] = volid;

	VDONE;

	/* The following is a bit redundant, since create sets these flags by default */
	code =
	    AFSVolSetFlags(*connPtr, *transPtr,
			   VTDeleteOnSalvage | VTOutOfService);
	if (code) {
	    PrintError("Failed to set flags on the ro volume: ", code);
	    goto fail;
	}
    }

    /* Otherwise, the transaction did succeed, so get the creation date of the
     * latest RO volume on the replication site
     */
    else {
	VPRINT2("Updating existing ro volume %u on %s ...\n", volid,
                noresolve ? afs_inet_ntoa_r(vldbEntryPtr->
                                            serverNumber[index], hoststr) :
                hostutil_GetNameByINet(vldbEntryPtr->serverNumber[index]));

	code = AFSVolGetStatus(*connPtr, *transPtr, &tstatus);
	if (code) {
	    PrintError("Failed to get status of volume on destination: ",
		       code);
	    goto fail;
	}
	if (tmpVolId) {
	    code = AFSVolEndTrans(*connPtr, *transPtr, &rcode);
	    *transPtr = 0;
	    if (!code)
		code = rcode;
	    if (!code)
		code = DoVolClone(*connPtr, volid,
				  vldbEntryPtr->serverPartition[index],
				  readonlyVolume, tmpVolId, "temporary",
				  vldbEntryPtr->name, NULL, ".roclone", NULL,
				  transPtr);
	    if (code)
		goto fail;
	}
	*crtimePtr = CLOCKADJ(tstatus.creationDate);
	*uptimePtr = CLOCKADJ(tstatus.updateDate);
    }

    return 0;

  fail:
    if (*transPtr) {
	tcode = AFSVolEndTrans(*connPtr, *transPtr, &rcode);
	*transPtr = 0;
	if (!tcode)
	    tcode = rcode;
	if (tcode && tcode != ENOENT)
	    PrintError("Could not end transaction on a ro volume: ", tcode);
    }

    return code;
}

static int
SimulateForwardMultiple(struct rx_connection *fromconn, afs_int32 fromtid,
			afs_int32 fromdate, manyDests * tr, afs_int32 flags,
			void *cookie, manyResults * results)
{
    unsigned int i;

    for (i = 0; i < tr->manyDests_len; i++) {
	results->manyResults_val[i] =
	    AFSVolForward(fromconn, fromtid, fromdate,
			  &(tr->manyDests_val[i].server),
			  tr->manyDests_val[i].trans, cookie);
    }
    return 0;
}

/**
 * Check if a trans has timed out, and recreate it if necessary.
 *
 * @param[in] aconn  RX connection to the relevant server
 * @param[inout] atid  Transaction ID to check; if we recreated the trans,
 *                     contains the new trans ID on success
 * @param[in] apart  Partition for the transaction
 * @param[in] astat  The status of the original transaction
 *
 * @return operation status
 *  @retval 0 existing transaction is still valid, or we managed to recreate
 *            the trans successfully
 *  @retval nonzero Fatal error; bail out
 */
static int
CheckTrans(struct rx_connection *aconn, afs_int32 *atid, afs_int32 apart,
           struct volser_status *astat)
{
    struct volser_status new_status;
    afs_int32 code;

    memset(&new_status, 0, sizeof(new_status));
    code = AFSVolGetStatus(aconn, *atid, &new_status);
    if (code) {
	if (code == ENOENT) {
	    *atid = 0;
	    VPRINT1("Old transaction on cloned volume %lu timed out, "
	            "restarting transaction\n", (long unsigned) astat->volID);
	    code = AFSVolTransCreate_retry(aconn, astat->volID, apart,
	                                   ITBusy, atid);
	    if (code) {
		PrintError("Failed to recreate cloned RO volume transaction\n",
		           code);
		return 1;
	    }

	    memset(&new_status, 0, sizeof(new_status));
	    code = AFSVolGetStatus(aconn, *atid, &new_status);
	    if (code) {
		PrintError("Failed to get status on recreated transaction\n",
		           code);
		return 1;
	    }

	    if (memcmp(&new_status, astat, sizeof(new_status)) != 0) {
		PrintError("Recreated transaction on cloned RO volume, but "
		           "the volume has changed!\n", 0);
		return 1;
	    }
	} else {
	    PrintError("Unable to get status of current cloned RO transaction\n",
	               code);
	    return 1;
	}
    } else {
	if (memcmp(&new_status, astat, sizeof(new_status)) != 0) {
	    /* sanity check */
	    PrintError("Internal error: current GetStatus does not match "
	               "original GetStatus?\n", 0);
	    return 1;
	}
    }

    return 0;
}

static void
PutTrans(afs_int32 *vldbindex, struct replica *replicas,
	 struct rx_connection **toconns, struct release *times,
	 afs_int32 volcount)
{
    afs_int32 s, code = 0, rcode = 0;
    /* End the transactions and destroy the connections */
    for (s = 0; s < volcount; s++) {
	if (replicas[s].trans) {
	    code = AFSVolEndTrans(toconns[s], replicas[s].trans, &rcode);

	    replicas[s].trans = 0;
	    if (!code)
		code = rcode;
	    if (code) {
		if ((s == 0) || (code != ENOENT)) {
		    PrintError("Could not end transaction on a ro volume: ",
			       code);
		} else {
		    PrintError
			("Transaction timed out on a ro volume. Will retry.\n",
			 0);
		    if (times[s].vldbEntryIndex < *vldbindex)
			*vldbindex = times[s].vldbEntryIndex;
		}
	    }
	}
	if (toconns[s])
	    rx_DestroyConnection(toconns[s]);
	toconns[s] = 0;
    }
}

static int
DoVolOnline(struct nvldbentry *vldbEntryPtr, afs_uint32 avolid, int index,
	    char *vname, struct rx_connection *connPtr)
{
    afs_int32 code = 0, rcode = 0, onlinetid = 0;

    code =
	AFSVolTransCreate_retry(connPtr, avolid,
				vldbEntryPtr->serverPartition[index],
				ITOffline,
				&onlinetid);
    if (code)
      EPRINT(code, "Could not create transaction on readonly...\n");

    else {
	code = AFSVolSetFlags(connPtr, onlinetid, 0);
	if (code)
	    EPRINT(code, "Could not set flags on readonly...\n");
    }

    if (!code) {
	code =
	    AFSVolSetIdsTypes(connPtr, onlinetid, vname,
			      ROVOL, vldbEntryPtr->volumeId[RWVOL],
			      0, 0);
	if (code)
	    EPRINT(code, "Could not set ids on readonly...\n");
    }
    if (!code)
	code = AFSVolEndTrans(connPtr, onlinetid, &rcode);
    if (!code)
	code = rcode;
    return code;
}

/* UV_ReleaseVolume()
 *    Release volume <afromvol> on <afromserver> <afrompart> to all
 *    its RO sites (full release). Unless the previous release was
 *    incomplete: in which case we bring the remaining incomplete
 *    volumes up to date with the volumes that were released
 *    successfully.
 *    forceflag: Performs a full release.
 *
 *    Will create a clone from the RW, then dump the clone out to
 *    the remaining replicas. If there is more than 1 RO sites,
 *    ensure that the VLDB says at least one RO is available all
 *    the time: Influences when we write back the VLDB entry.
 */

int
UV_ReleaseVolume(afs_uint32 afromvol, afs_uint32 afromserver,
		 afs_int32 afrompart, int forceflag, int stayUp)
{
    char vname[64];
    afs_int32 code = 0;
    afs_int32 vcode, rcode, tcode;
    afs_uint32 cloneVolId = 0, roVolId;
    struct replica *replicas = 0;
    struct nvldbentry entry, storeEntry;
    int i, volcount = 0, m, fullrelease, vldbindex;
    int failure;
    struct restoreCookie cookie;
    struct rx_connection **toconns = 0;
    struct release *times = 0;
    int nservers = 0;
    struct rx_connection *fromconn = (struct rx_connection *)0;
    afs_int32 error = 0;
    int islocked = 0;
    afs_int32 clonetid = 0, onlinetid;
    afs_int32 fromtid = 0;
    afs_uint32 fromdate = 0;
    afs_uint32 thisdate;
    time_t tmv;
    int s;
    manyDests tr;
    manyResults results;
    int rwindex, roindex, roclone, roexists;
    afs_uint32 rwcrdate = 0, rwupdate = 0;
    afs_uint32 clcrdate;
    struct rtime {
	int validtime;
	afs_uint32 uptime;
    } remembertime[NMAXNSERVERS];
    int releasecount = 0;
    struct volser_status volstatus;
    char hoststr[16];
    afs_int32 origflags[NMAXNSERVERS];
    struct volser_status orig_status;
    int notreleased = 0;
    int tried_justnewsites = 0;
    int justnewsites = 0; /* are we just trying to release to new RO sites? */

    memset(remembertime, 0, sizeof(remembertime));
    memset(&results, 0, sizeof(results));
    memset(origflags, 0, sizeof(origflags));

    vcode = ubik_VL_SetLock(cstruct, 0, afromvol, RWVOL, VLOP_RELEASE);
    if (vcode != VL_RERELEASE)
	ONERROR(vcode, afromvol,
		"Could not lock the VLDB entry for the volume %u.\n");
    islocked = 1;

    /* Get the vldb entry in readable format */
    vcode = VLDB_GetEntryByID(afromvol, RWVOL, &entry);
    ONERROR(vcode, afromvol,
	    "Could not fetch the entry for the volume %u from the VLDB.\n");
    MapHostToNetwork(&entry);

    if (verbose)
	EnumerateEntry(&entry);

    if (!ISNAMEVALID(entry.name))
	ONERROR(VOLSERBADOP, entry.name,
		"Volume name %s is too long, rename before releasing.\n");
    if (entry.volumeId[RWVOL] != afromvol)
	ONERROR(VOLSERBADOP, afromvol,
		"The volume %u being released is not a read-write volume.\n");
    if (entry.nServers <= 1)
	ONERROR(VOLSERBADOP, afromvol,
		"Volume %u has no replicas - release operation is meaningless!\n");
    if (strlen(entry.name) > (VOLSER_OLDMAXVOLNAME - 10))
	ONERROR(VOLSERBADOP, entry.name,
		"RO volume name %s exceeds (VOLSER_OLDMAXVOLNAME - 10) character limit\n");

    /* roclone is true if one of the RO volumes is on the same
     * partition as the RW volume. In this case, we make the RO volume
     * on the same partition a clone instead of a complete copy.
     */

    roindex = Lp_ROMatch(afromserver, afrompart, &entry) - 1;
    roclone = ((roindex == -1) ? 0 : 1);
    rwindex = Lp_GetRwIndex(&entry);
    if (rwindex < 0)
	ONERROR0(VOLSERNOVOL, "There is no RW volume \n");

    /* Make sure we have a RO volume id to work with */
    if (entry.volumeId[ROVOL] == INVALID_BID) {
	/* need to get a new RO volume id */
	vcode = ubik_VL_GetNewVolumeId(cstruct, 0, 1, &roVolId);
	ONERROR(vcode, entry.name, "Cant allocate ID for RO volume of %s\n");

	entry.volumeId[ROVOL] = roVolId;
	MapNetworkToHost(&entry, &storeEntry);
	vcode = VLDB_ReplaceEntry(afromvol, RWVOL, &storeEntry, 0);
	ONERROR(vcode, entry.name, "Could not update vldb entry for %s.\n");
    }

    /* Will we be completing a previously unfinished release. -force overrides */
    for (s = 0, m = 0, fullrelease=0, i=0; (i<entry.nServers); i++) {
	if (entry.serverFlags[i] & ITSROVOL) {
	    m++;
	    if (entry.serverFlags[i] & NEW_REPSITE) s++;
	    if (entry.serverFlags[i] & RO_DONTUSE) notreleased++;
	}
	origflags[i] = entry.serverFlags[i];
    }
    if ((forceflag && !fullrelease) || (s == m) || (s == 0))
	fullrelease = 1;

    if (!forceflag && (s == m || s == 0)) {
	if (notreleased && notreleased != m) {
	    /* we have some new unreleased sites. try to just release to those,
	     * if the RW has not changed */
	    justnewsites = 1;
	}
    }

    /* Determine which volume id to use and see if it exists */
    cloneVolId =
	((fullrelease
	  || (entry.cloneId == 0)) ? entry.volumeId[ROVOL] : entry.cloneId);
    code = VolumeExists(afromserver, afrompart, cloneVolId);
    roexists = ((code == ENODEV) ? 0 : 1);

    /* For stayUp case, if roclone is the only site, bypass special handling */
    if (stayUp && roclone) {
	int e;
	error = 0;

	for (e = 0; (e < entry.nServers) && !error; e++) {
	    if ((entry.serverFlags[e] & ITSROVOL)) {
		if (!(VLDB_IsSameAddrs(entry.serverNumber[e], afromserver,
				       &error)))
		    break;
	    }
	}
	if (e >= entry.nServers)
	    stayUp = 0;
    }

    /* If we had a previous release to complete, do so, else: */
    if (stayUp && (cloneVolId == entry.volumeId[ROVOL])) {
	code = ubik_VL_GetNewVolumeId(cstruct, 0, 1, &cloneVolId);
	ONERROR(code, afromvol,
		"Cannot get temporary clone id for volume %u\n");
    }

    fromconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);
    if (!fromconn)
	ONERROR(-1, afromserver,
		"Cannot establish connection with server 0x%x\n");

    if (!fullrelease) {
	if (!roexists)
	    fullrelease = 1;	/* Do a full release if RO clone does not exist */
	else {
	    /* Begin transaction on RW and mark it busy while we query it */
	    code = AFSVolTransCreate_retry(
			fromconn, afromvol, afrompart, ITBusy, &fromtid
		   );
	    ONERROR(code, afromvol,
		    "Failed to start transaction on RW volume %u\n");

	    /* Query the creation date for the RW */
	    code = AFSVolGetStatus(fromconn, fromtid, &volstatus);
	    ONERROR(code, afromvol,
		    "Failed to get the status of RW volume %u\n");
	    rwcrdate = volstatus.creationDate;

	    /* End transaction on RW */
	    code = AFSVolEndTrans(fromconn, fromtid, &rcode);
	    fromtid = 0;
	    ONERROR((code ? code : rcode), afromvol,
		    "Failed to end transaction on RW volume %u\n");

	    /* Begin transaction on clone and mark it busy while we query it */
	    code = AFSVolTransCreate_retry(
			fromconn, cloneVolId, afrompart, ITBusy, &clonetid
		   );
	    ONERROR(code, cloneVolId,
		    "Failed to start transaction on RW clone %u\n");

	    /* Query the creation date for the clone */
	    code = AFSVolGetStatus(fromconn, clonetid, &volstatus);
	    ONERROR(code, cloneVolId,
		    "Failed to get the status of RW clone %u\n");
	    clcrdate = volstatus.creationDate;

	    /* End transaction on clone */
	    code = AFSVolEndTrans(fromconn, clonetid, &rcode);
	    clonetid = 0;
	    ONERROR((code ? code : rcode), cloneVolId,
		    "Failed to end transaction on RW clone %u\n");

	    if (rwcrdate > clcrdate)
		fullrelease = 2;/* Do a full release if RO clone older than RW */
	}
    }

    if (fullrelease != 1) {
	/* in case the RW has changed, and just to be safe */
	justnewsites = 0;
    }

    if (verbose) {
	switch (fullrelease) {
	    case 2:
		fprintf(STDOUT, "RW %lu changed, doing a complete release\n",
			(unsigned long)afromvol);
		break;
	    case 1:
		fprintf(STDOUT, "This is a complete release of volume %lu\n",
			(unsigned long)afromvol);
		if (justnewsites) {
		    tried_justnewsites = 1;
		    fprintf(STDOUT, "There are new RO sites; we will try to "
		                    "only release to new sites\n");
		}
		break;
	    case 0:
		fprintf(STDOUT, "This is a completion of a previous release\n");
		break;
	}
    }

    if (fullrelease) {
	afs_int32 oldest = 0;
	/* If the RO clone exists, then if the clone is a temporary
	 * clone, delete it. Or if the RO clone is marked RO_DONTUSE
	 * (it was recently added), then also delete it. We do not
	 * want to "reclone" a temporary RO clone.
	 */
	if (stayUp) {
	    code = VolumeExists(afromserver, afrompart, cloneVolId);
	    if (!code) {
		code = DoVolDelete(fromconn, cloneVolId, afrompart, "previous clone", 0,
				   NULL, NULL);
		if (code && (code != VNOVOL))
		    ERROREXIT(code);
		VDONE;
	    }
	}
	/* clean up any previous tmp clone before starting if staying up */
	if (roexists
	    && (!roclone || (entry.serverFlags[roindex] & RO_DONTUSE))) {
	    code = DoVolDelete(fromconn,
			       stayUp ? entry.volumeId[ROVOL] : cloneVolId,
			       afrompart, "the", 0, NULL, NULL);
	    if (code && (code != VNOVOL))
		ERROREXIT(code);
	    roexists = 0;
	}

	if (justnewsites) {
	    VPRINT("Querying old RO sites for update times...");
	    for (vldbindex = 0; vldbindex < entry.nServers; vldbindex++) {
		volEntries volumeInfo;
		struct rx_connection *conn;
		afs_int32 crdate;

		if (!(entry.serverFlags[vldbindex] & ITSROVOL)) {
		    continue;
		}
		if ((entry.serverFlags[vldbindex] & RO_DONTUSE)) {
		    continue;
		}
		conn = UV_Bind(entry.serverNumber[vldbindex], AFSCONF_VOLUMEPORT);
		if (!conn) {
		    fprintf(STDERR, "Cannot establish connection to server %s\n",
		                    hostutil_GetNameByINet(entry.serverNumber[vldbindex]));
		    justnewsites = 0;
		    break;
		}
		volumeInfo.volEntries_val = NULL;
		volumeInfo.volEntries_len = 0;
		code = AFSVolListOneVolume(conn, entry.serverPartition[vldbindex],
		                           entry.volumeId[ROVOL],
		                           &volumeInfo);
		if (code) {
		    fprintf(STDERR, "Could not fetch information about RO vol %lu from server %s\n",
		                    (unsigned long)entry.volumeId[ROVOL],
		                    hostutil_GetNameByINet(entry.serverNumber[vldbindex]));
		    PrintError("", code);
		    justnewsites = 0;
		    rx_DestroyConnection(conn);
		    break;
		}

		crdate = CLOCKADJ(volumeInfo.volEntries_val[0].creationDate);

		if (oldest == 0 || crdate < oldest) {
		    oldest = crdate;
		}

		rx_DestroyConnection(conn);
		free(volumeInfo.volEntries_val);
		volumeInfo.volEntries_val = NULL;
		volumeInfo.volEntries_len = 0;
	    }
	    VDONE;
	}
	if (justnewsites) {
	    volEntries volumeInfo;
	    volumeInfo.volEntries_val = NULL;
	    volumeInfo.volEntries_len = 0;
	    code = AFSVolListOneVolume(fromconn, afrompart, afromvol,
	                               &volumeInfo);
	    if (code) {
		fprintf(STDERR, "Could not fetch information about RW vol %lu from server %s\n",
		                (unsigned long)afromvol,
		                hostutil_GetNameByINet(afromserver));
		PrintError("", code);
		justnewsites = 0;
	    } else {
		rwupdate = volumeInfo.volEntries_val[0].updateDate;

		free(volumeInfo.volEntries_val);
		volumeInfo.volEntries_val = NULL;
		volumeInfo.volEntries_len = 0;
	    }
	}
	if (justnewsites && oldest <= rwupdate) {
	    /* RW has changed */
	    justnewsites = 0;
	}

	/* Mark all the ROs in the VLDB entry as RO_DONTUSE. We don't
	 * write this entry out to the vlserver until after the first
	 * RO volume is released (temp RO clones don't count).
	 *
	 * If 'justnewsites' is set, we're only updating sites that have
	 * RO_DONTUSE set, so set NEW_REPSITE for all of the others.
	 */
	for (i = 0; i < entry.nServers; i++) {
	    if (justnewsites) {
		if ((entry.serverFlags[i] & RO_DONTUSE)) {
		    entry.serverFlags[i] &= ~NEW_REPSITE;
		} else {
		    entry.serverFlags[i] |= NEW_REPSITE;
		}
	    } else {
		entry.serverFlags[i] &= ~NEW_REPSITE;
		entry.serverFlags[i] |= RO_DONTUSE;
	    }
	}
	entry.serverFlags[rwindex] |= NEW_REPSITE;
	entry.serverFlags[rwindex] &= ~RO_DONTUSE;
    }

    if (justnewsites && roexists) {
	/* if 'justnewsites' and 'roexists' are set, we don't need to do
	 * anything with the RO clone, so skip the reclone */
	/* noop */

    } else if (fullrelease) {

	if (roclone) {
	    strcpy(vname, entry.name);
	    if (stayUp)
		strcat(vname, ".roclone");
	    else
		strcat(vname, ".readonly");
	} else {
	    strcpy(vname, "readonly-clone-temp");
	}

	code = DoVolClone(fromconn, afromvol, afrompart, readonlyVolume,
			  cloneVolId, (roclone && !stayUp)?"permanent RO":
			  "temporary RO", NULL, vname, NULL, &volstatus, NULL);
	if (code) {
	    error = code;
	    goto rfail;
	}

	if (justnewsites && rwupdate != volstatus.updateDate) {
	    justnewsites = 0;
	    /* reset the serverFlags as if 'justnewsites' had never been set */
	    for (i = 0; i < entry.nServers; i++) {
		entry.serverFlags[i] &= ~NEW_REPSITE;
		entry.serverFlags[i] |= RO_DONTUSE;
	    }
	    entry.serverFlags[rwindex] |= NEW_REPSITE;
	    entry.serverFlags[rwindex] &= ~RO_DONTUSE;
	}

	rwcrdate = volstatus.creationDate;

	/* Remember clone volume ID in case we fail or are interrupted */
	entry.cloneId = cloneVolId;

	if (roclone && !stayUp) {
	    /* Bring the RO clone online - though not if it's a temporary clone */
	    VPRINT1("Starting transaction on RO clone volume %u...",
		    cloneVolId);
	    code =
		AFSVolTransCreate_retry(fromconn, cloneVolId, afrompart, ITOffline,
				  &onlinetid);
	    ONERROR(code, cloneVolId,
		    "Failed to start transaction on volume %u\n");
	    VDONE;

	    VPRINT1("Setting volume flags for volume %u...", cloneVolId);
	    tcode = AFSVolSetFlags(fromconn, onlinetid, 0);
	    VDONE;

	    VPRINT1("Ending transaction on volume %u...", cloneVolId);
	    code = AFSVolEndTrans(fromconn, onlinetid, &rcode);
	    ONERROR((code ? code : rcode), cloneVolId,
		    "Failed to end transaction on RO clone %u\n");
	    VDONE;

	    ONERROR(tcode, cloneVolId, "Could not bring volume %u on line\n");

	    /* Sleep so that a client searching for an online volume won't
	     * find the clone offline and then the next RO offline while the
	     * release brings the clone online and the next RO offline (race).
	     * There is a fix in the 3.4 client that does not need this sleep
	     * anymore, but we don't know what clients we have.
	     */
	    if (entry.nServers > 2 && !justnewsites)
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
	    MapNetworkToHost(&entry, &storeEntry);
	    VPRINT1("Replacing VLDB entry for %s...", entry.name);
	    vcode = VLDB_ReplaceEntry(afromvol, RWVOL, &storeEntry, 0);
	    ONERROR(vcode, entry.name,
		    "Could not update vldb entry for %s.\n");
	    VDONE;
	}
    }

    if (justnewsites) {
	VPRINT("RW vol has not changed; only releasing to new RO sites\n");
	/* act like this is a completion of a previous release */
	fullrelease = 0;
    } else if (tried_justnewsites) {
	VPRINT("RW vol has changed; releasing to all sites\n");
    }

    /* Now we will release from the clone to the remaining RO replicas.
     * The first 2 ROs (counting the non-temporary RO clone) are released
     * individually: releasecount. This is to reduce the race condition
     * of clients trying to find an on-line RO volume. The remaining ROs
     * are released in parallel but no more than half the number of ROs
     * (rounded up) at a time: nservers.
     */

    strcpy(vname, entry.name);
    if (stayUp)
	strcat(vname, ".roclone");
    else
	strcat(vname, ".readonly");
    memset(&cookie, 0, sizeof(cookie));
    strncpy(cookie.name, vname, VOLSER_OLDMAXVOLNAME);
    cookie.type = ROVOL;
    cookie.parent = entry.volumeId[RWVOL];
    cookie.clone = 0;

    /* how many to do at once, excluding clone */
    if (stayUp || justnewsites)
	nservers = entry.nServers; /* can do all, none offline */
    else
	nservers = entry.nServers / 2;
    replicas =
	(struct replica *)malloc(sizeof(struct replica) * nservers + 1);
    times = (struct release *)malloc(sizeof(struct release) * nservers + 1);
    toconns =
	(struct rx_connection **)malloc(sizeof(struct rx_connection *) *
					nservers + 1);
    results.manyResults_val =
	(afs_int32 *) malloc(sizeof(afs_int32) * nservers + 1);
    if (!replicas || !times || !results.manyResults_val || !toconns)
	ONERROR0(ENOMEM,
		"Failed to create transaction on the release clone\n");

    memset(replicas, 0, (sizeof(struct replica) * nservers + 1));
    memset(times, 0, (sizeof(struct release) * nservers + 1));
    memset(toconns, 0, (sizeof(struct rx_connection *) * nservers + 1));
    memset(results.manyResults_val, 0, (sizeof(afs_int32) * nservers + 1));

    /* Create a transaction on the cloned volume */
    VPRINT1("Starting transaction on cloned volume %u...", cloneVolId);
    code =
	AFSVolTransCreate_retry(fromconn, cloneVolId, afrompart, ITBusy, &fromtid);
    if (!code) {
	memset(&orig_status, 0, sizeof(orig_status));
	code = AFSVolGetStatus(fromconn, fromtid, &orig_status);
    }
    if (!fullrelease && code)
	ONERROR(VOLSERNOVOL, afromvol,
		"Old clone is inaccessible. Try vos release -f %u.\n");
    ONERROR0(code, "Failed to create transaction on the release clone\n");
    VDONE;

    /* if we have a clone, treat this as done, for now */
    if (stayUp && !fullrelease) {
	entry.serverFlags[roindex] |= NEW_REPSITE;
	entry.serverFlags[roindex] &= ~RO_DONTUSE;
	entry.flags |= RO_EXISTS;

	releasecount++;
    }

    /* For each index in the VLDB */
    for (vldbindex = 0; vldbindex < entry.nServers;) {
	/* Get a transaction on the replicas. Pick replicas which have an old release. */
	for (volcount = 0;
	     ((volcount < nservers) && (vldbindex < entry.nServers));
	     vldbindex++) {
	    if (!stayUp && !justnewsites) {
		/* The first two RO volumes will be released individually.
		 * The rest are then released in parallel. This is a hack
		 * for clients not recognizing right away when a RO volume
		 * comes back on-line.
		 */
		if ((volcount == 1) && (releasecount < 2))
		    break;
	    }

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
		ntohl(entry.serverNumber[vldbindex]);
	    replicas[volcount].server.destPort = AFSCONF_VOLUMEPORT;
	    replicas[volcount].server.destSSID = 1;
	    times[volcount].vldbEntryIndex = vldbindex;

	    code =
		GetTrans(&entry, vldbindex, &(toconns[volcount]),
			 &(replicas[volcount].trans),
			 &(times[volcount].crtime),
			 &(times[volcount].uptime),
			 origflags, stayUp?cloneVolId:0);
	    if (code)
		continue;

	    /* Thisdate is the date from which we want to pick up all changes */
	    if (forceflag || !fullrelease
		|| (rwcrdate > times[volcount].crtime)) {
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
		    (remembertime[vldbindex].uptime < times[volcount].uptime)
			? remembertime[vldbindex].uptime
			: times[volcount].uptime;
	    } else {
		thisdate = times[volcount].uptime;
	    }
	    remembertime[vldbindex].validtime = 1;
	    remembertime[vldbindex].uptime = thisdate;

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

	code = CheckTrans(fromconn, &fromtid, afrompart, &orig_status);
	if (code) {
	    error = ENOENT;
	    goto rfail;
	}

	if (verbose) {
	    fprintf(STDOUT, "Starting ForwardMulti from %lu to %u on %s",
		    (unsigned long)cloneVolId, stayUp?
		    cloneVolId:entry.volumeId[ROVOL],
                    noresolve ? afs_inet_ntoa_r(entry.serverNumber[times[0].
                                                vldbEntryIndex], hoststr) :
                    hostutil_GetNameByINet(entry.
					   serverNumber[times[0].
							vldbEntryIndex]));

	    for (s = 1; s < volcount; s++) {
		fprintf(STDOUT, " and %s",
                        noresolve ? afs_inet_ntoa_r(entry.serverNumber[times[s].
                                                    vldbEntryIndex], hoststr) :
                        hostutil_GetNameByINet(entry.
					       serverNumber[times[s].
							    vldbEntryIndex]));
	    }

	    if (fromdate == 0)
		fprintf(STDOUT, " (full release)");
	    else {
		tmv = fromdate;
		fprintf(STDOUT, " (as of %.24s)", ctime(&tmv));
	    }
	    fprintf(STDOUT, ".\n");
	    fflush(STDOUT);
	}

	/* Release the ones we have collected */
	tr.manyDests_val = &(replicas[0]);
	tr.manyDests_len = results.manyResults_len = volcount;
	code =
	    AFSVolForwardMultiple(fromconn, fromtid, fromdate, &tr,
				  0 /*spare */ , &cookie, &results);
	if (code == RXGEN_OPCODE) {	/* RPC Interface Mismatch */
	    code =
		SimulateForwardMultiple(fromconn, fromtid, fromdate, &tr,
					0 /*spare */ , &cookie, &results);
	    nservers = 1;
	}

	if (code) {
	    PrintError("Release failed: ", code);
	} else {
	    for (m = 0; m < volcount; m++) {
		if (results.manyResults_val[m]) {
		    if ((m == 0) || (results.manyResults_val[m] != ENOENT)) {
			/* we retry timed out transaction. When it is
			 * not the first volume and the transaction wasn't found
			 * (assume it timed out and was garbage collected by volser).
			 */
			PrintError
			    ("Failed to dump volume from clone to a ro site: ",
			     results.manyResults_val[m]);
		    }
		    continue;
		}

		code =
		    AFSVolSetIdsTypes(toconns[m], replicas[m].trans, vname,
				      ROVOL, entry.volumeId[RWVOL], 0, 0);
		if (code) {
		    if ((m == 0) || (code != ENOENT)) {
			PrintError("Failed to set correct names and ids: ",
				   code);
		    }
		    continue;
		}

		/* have to clear dest. flags to ensure new vol goes online:
		 * because the restore (forwarded) operation copied
		 * the V_inService(=0) flag over to the destination.
		 */
		code = AFSVolSetFlags(toconns[m], replicas[m].trans, 0);
		if (code) {
		    if ((m == 0) || (code != ENOENT)) {
			PrintError("Failed to set flags on ro volume: ",
				   code);
		    }
		    continue;
		}

		entry.serverFlags[times[m].vldbEntryIndex] |= NEW_REPSITE;
		entry.serverFlags[times[m].vldbEntryIndex] &= ~RO_DONTUSE;
		entry.flags |= RO_EXISTS;
		releasecount++;
	    }
	}

	if (!stayUp) {
	    PutTrans(&vldbindex, replicas, toconns, times, volcount);
	    MapNetworkToHost(&entry, &storeEntry);
	    vcode = VLDB_ReplaceEntry(afromvol, RWVOL, &storeEntry, 0);
	    ONERROR(vcode, afromvol,
		    " Could not update VLDB entry for volume %u\n");
	}
    }				/* for each index in the vldb */

    /* for the stayup case, put back at the end */
    if (stayUp) {
	afs_uint32 tmpVol = entry.volumeId[ROVOL];
	strcpy(vname, entry.name);
	strcat(vname, ".readonly");

	if (roclone) {
	    /* have to clear flags to ensure new vol goes online
	     */
	    code = AFSVolSetFlags(fromconn, fromtid, 0);
	    if (code && (code != ENOENT)) {
		PrintError("Failed to set flags on ro volume: ",
			   code);
	    }

	    VPRINT3("%sloning to permanent RO %u on %s...", roexists?"Re-c":"C", tmpVol,
		    noresolve ?
		    afs_inet_ntoa_r(entry.serverNumber[roindex],
				    hoststr) :
		    hostutil_GetNameByINet(entry.serverNumber[roindex]));

	    code = AFSVolClone(fromconn, fromtid, roexists?tmpVol:0,
			       readonlyVolume, vname, &tmpVol);

	    if (!code) {
		VDONE;
		VPRINT("Bringing readonly online...");
		code = DoVolOnline(&entry, tmpVol, roindex, vname,
				   fromconn);
	    }
	    if (code) {
		EPRINT(code, "Failed: ");
		entry.serverFlags[roindex] &= ~NEW_REPSITE;
		entry.serverFlags[roindex] |= RO_DONTUSE;
	    } else {
		entry.serverFlags[roindex] |= NEW_REPSITE;
		entry.serverFlags[roindex] &= ~RO_DONTUSE;
		entry.flags |= RO_EXISTS;
		VDONE;
	    }

	}
	for (s = 0; s < volcount; s++) {
	    if (replicas[s].trans) {
		vldbindex = times[s].vldbEntryIndex;

		/* ok, so now we have to end the previous transaction */
		code = AFSVolEndTrans(toconns[s], replicas[s].trans, &rcode);
		if (!code)
		    code = rcode;

		if (!code) {
		    code = AFSVolTransCreate_retry(toconns[s],
						   cloneVolId,
						   entry.serverPartition[vldbindex],
						   ITBusy,
						   &(replicas[s].trans));
		    if (code) {
			PrintError("Unable to begin transaction on temporary clone: ", code);
		    }
		} else {
		    PrintError("Unable to end transaction on temporary clone: ", code);
		}

		VPRINT3("%sloning to permanent RO %u on %s...", times[s].crtime?"Re-c":"C",
			tmpVol, noresolve ?
			afs_inet_ntoa_r(htonl(replicas[s].server.destHost),
					hoststr) :
			hostutil_GetNameByINet(htonl(replicas[s].server.destHost)));
		if (times[s].crtime)
		    code = AFSVolClone(toconns[s], replicas[s].trans, tmpVol,
				       readonlyVolume, vname, &tmpVol);
		else
		    code = AFSVolClone(toconns[s], replicas[s].trans, 0,
				       readonlyVolume, vname, &tmpVol);

		if (code) {
		    if (!times[s].crtime) {
			entry.serverFlags[vldbindex] |= RO_DONTUSE;
		    }
		    entry.serverFlags[vldbindex] &= ~NEW_REPSITE;
		    PrintError("Failed: ",
			       code);
		} else
		    VDONE;

		if (entry.serverFlags[vldbindex] != RO_DONTUSE) {
		    /* bring it online (mark it InService) */
		    VPRINT1("Bringing readonly online on %s...",
			    noresolve ?
			    afs_inet_ntoa_r(
				htonl(replicas[s].server.destHost),
				hoststr) :
			    hostutil_GetNameByINet(
				htonl(replicas[s].server.destHost)));

		    code = DoVolOnline(&entry, tmpVol, vldbindex, vname,
				       toconns[s]);
		    /* needed to come online for cloning */
		    if (code) {
			/* technically it's still new, just not online */
			entry.serverFlags[s] &= ~NEW_REPSITE;
			entry.serverFlags[s] |= RO_DONTUSE;
			if (code != ENOENT) {
			    PrintError("Failed to set correct names and ids: ",
				       code);
			}
		    } else
			VDONE;
		}

		VPRINT("Marking temporary clone for deletion...\n");
		code = AFSVolSetFlags(toconns[s],
				      replicas[s].trans,
				      VTDeleteOnSalvage |
				      VTOutOfService);
		if (code)
		  EPRINT(code, "Failed: ");
		else
		  VDONE;

		VPRINT("Ending transaction on temporary clone...\n");
		code = AFSVolEndTrans(toconns[s], replicas[s].trans, &rcode);
		if (!code)
		    rcode = code;
		if (code)
		    PrintError("Failed: ", code);
		else {
		    VDONE;
		    /* ended successfully */
		    replicas[s].trans = 0;

		    VPRINT2("Deleting temporary clone %u on %s...", cloneVolId,
			    noresolve ?
			    afs_inet_ntoa_r(htonl(replicas[s].server.destHost),
					    hoststr) :
			    hostutil_GetNameByINet(htonl(replicas[s].server.destHost)));
		    code = DoVolDelete(toconns[s], cloneVolId,
				       entry.serverPartition[vldbindex],
				       NULL, 0, NULL, NULL);
		    if (code) {
			EPRINT(code, "Failed: ");
		    } else
			VDONE;
		}
	    }
	}

	/* done. put the vldb entry in the success tail case*/
	PutTrans(&vldbindex, replicas, toconns, times, volcount);
    }

    /* End the transaction on the cloned volume */
    code = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;
    if (!code)
	code = rcode;
    if (code)
	PrintError("Failed to end transaction on rw volume: ", code);

    /* Figure out if any volume were not released and say so */
    for (failure = 0, i = 0; i < entry.nServers; i++) {
	if (!(entry.serverFlags[i] & NEW_REPSITE))
	    failure++;
    }
    if (failure) {
	char pname[10];
	fprintf(STDERR,
		"The volume %lu could not be released to the following %d sites:\n",
		(unsigned long)afromvol, failure);
	for (i = 0; i < entry.nServers; i++) {
	    if (!(entry.serverFlags[i] & NEW_REPSITE)) {
		MapPartIdIntoName(entry.serverPartition[i], pname);
		fprintf(STDERR, "\t%35s %s\n",
                        noresolve ? afs_inet_ntoa_r(entry.serverNumber[i], hoststr) :
                        hostutil_GetNameByINet(entry.serverNumber[i]), pname);
	    }
	}
	MapNetworkToHost(&entry, &storeEntry);
	vcode =
	    VLDB_ReplaceEntry(afromvol, RWVOL, &storeEntry,
			      LOCKREL_TIMESTAMP);
	ONERROR(vcode, afromvol,
		" Could not update VLDB entry for volume %u\n");

	ERROREXIT(VOLSERBADRELEASE);
    }

    entry.cloneId = 0;
    /* All the ROs were release successfully. Remove the temporary clone */
    if (!roclone || stayUp) {
	if (verbose) {
	    fprintf(STDOUT, "Deleting the releaseClone %lu ...",
		    (unsigned long)cloneVolId);
	    fflush(STDOUT);
	}
	code = DoVolDelete(fromconn, cloneVolId, afrompart, NULL, 0, NULL,
			   NULL);
	ONERROR(code, cloneVolId, "Failed to delete volume %u.\n");
	VDONE;
    }

    for (i = 0; i < entry.nServers; i++)
	entry.serverFlags[i] &= ~NEW_REPSITE;

    /* Update the VLDB */
    VPRINT("updating VLDB ...");

    MapNetworkToHost(&entry, &storeEntry);
    vcode =
	VLDB_ReplaceEntry(afromvol, RWVOL, &storeEntry,
			  LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
    ONERROR(vcode, afromvol, " Could not update VLDB entry for volume %u\n");
    VDONE;

  rfail:
    if (clonetid) {
	code = AFSVolEndTrans(fromconn, clonetid, &rcode);
	clonetid = 0;
	if (code) {
	    fprintf(STDERR,
		    "Failed to end cloning transaction on the RW volume %lu\n",
		    (unsigned long)afromvol);
	    if (!error)
		error = code;
	}
    }
    if (fromtid) {
	code = AFSVolEndTrans(fromconn, fromtid, &rcode);
	fromtid = 0;
	if (code) {
	    fprintf(STDERR,
		    "Failed to end transaction on the release clone %lu\n",
		    (unsigned long)cloneVolId);
	    if (!error)
		error = code;
	}
    }
    for (i = 0; i < nservers; i++) {
	if (replicas && replicas[i].trans) {
	    code = AFSVolEndTrans(toconns[i], replicas[i].trans, &rcode);
	    replicas[i].trans = 0;
	    if (code) {
		fprintf(STDERR,
			"Failed to end transaction on ro volume %u at server %s\n",
			entry.volumeId[ROVOL],
                        noresolve ? afs_inet_ntoa_r(htonl(replicas[i].server.
                                                        destHost), hoststr) :
                        hostutil_GetNameByINet(htonl
					       (replicas[i].server.destHost)));
		if (!error)
		    error = code;
	    }
	}
	if (toconns && toconns[i]) {
	    rx_DestroyConnection(toconns[i]);
	    toconns[i] = 0;
	}
    }
    if (islocked) {
	vcode =
	    ubik_VL_ReleaseLock(cstruct, 0, afromvol, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (vcode) {
	    fprintf(STDERR,
		    "Could not release lock on the VLDB entry for volume %lu\n",
		    (unsigned long)afromvol);
	    if (!error)
		error = vcode;
	}
    }

    PrintError("", error);

    if (fromconn)
	rx_DestroyConnection(fromconn);
    if (results.manyResults_val)
	free(results.manyResults_val);
    if (replicas)
	free(replicas);
    if (toconns)
	free(toconns);
    if (times)
	free(times);
    return error;
}


void
dump_sig_handler(int x)
{
    fprintf(STDERR, "\nSignal handler: vos dump operation\n");
    longjmp(env, 0);
}

/* Dump the volume <afromvol> on <afromserver> and
 * <afrompart> to <afilename> starting from <fromdate>.
 * DumpFunction does the real work behind the scenes after
 * extracting parameters from the rock
 */
int
UV_DumpVolume(afs_uint32 afromvol, afs_uint32 afromserver, afs_int32 afrompart,
	      afs_int32 fromdate,
	      afs_int32(*DumpFunction) (struct rx_call *, void *), void *rock,
	      afs_int32 flags)
{
    /* declare stuff 'volatile' that may be used from setjmp/longjmp and may
     * be changing during the dump */
    struct rx_call * volatile fromcall = NULL;
    struct rx_connection * volatile fromconn = NULL;
    afs_int32 volatile fromtid = 0;

    afs_int32 rxError = 0, rcode = 0;
    afs_int32 code, error = 0;
    afs_int32 tmp;
    time_t tmv = fromdate;

    if (setjmp(env))
	ERROR_EXIT(EPIPE);
#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, dump_sig_handler);
#endif
    (void)signal(SIGINT, dump_sig_handler);

    if (!fromdate) {
	VEPRINT("Full Dump ...\n");
    } else {
	VEPRINT1("Incremental Dump (as of %.24s)...\n",
		ctime(&tmv));
    }

    /* get connections to the servers */
    fromconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);

    VEPRINT1("Starting transaction on volume %u...", afromvol);
    tmp = fromtid;
    code = AFSVolTransCreate_retry(fromconn, afromvol, afrompart, ITBusy, &tmp);
    fromtid = tmp;
    EGOTO1(error_exit, code,
	   "Could not start transaction on the volume %u to be dumped\n",
	   afromvol);
    VEDONE;

    fromcall = rx_NewCall(fromconn);

    VEPRINT1("Starting volume dump on volume %u...", afromvol);
    if (flags & VOLDUMPV2_OMITDIRS)
	code = StartAFSVolDumpV2(fromcall, fromtid, fromdate, flags);
    else
	code = StartAFSVolDump(fromcall, fromtid, fromdate);
    EGOTO(error_exit, code, "Could not start the dump process \n");
    VEDONE;

    VEPRINT1("Dumping volume %u...", afromvol);
    code = DumpFunction(fromcall, rock);
    if (code == RXGEN_OPCODE)
	goto error_exit;
    EGOTO(error_exit, code, "Error while dumping volume \n");
    VEDONE;

  error_exit:
    if (fromcall) {
	code = rx_EndCall(fromcall, rxError);
	if (code && code != RXGEN_OPCODE)
	    fprintf(STDERR, "Error in rx_EndCall\n");
	if (code && !error)
	    error = code;
    }
    if (fromtid) {
	VEPRINT1("Ending transaction on volume %u...", afromvol);
	code = AFSVolEndTrans(fromconn, fromtid, &rcode);
	if (code || rcode) {
	    fprintf(STDERR, "Could not end transaction on the volume %lu\n",
		    (unsigned long)afromvol);
	    if (!error)
		error = (code ? code : rcode);
	}
	VEDONE;
    }
    if (fromconn)
	rx_DestroyConnection(fromconn);

    if (error != RXGEN_OPCODE)
	PrintError("", error);
    return (error);
}

/* Clone the volume <afromvol> on <afromserver> and
 * <afrompart>, and then dump the clone volume to
 * <afilename> starting from <fromdate>.
 * DumpFunction does the real work behind the scenes after
 * extracting parameters from the rock
 */
int
UV_DumpClonedVolume(afs_uint32 afromvol, afs_uint32 afromserver,
		    afs_int32 afrompart, afs_int32 fromdate,
		    afs_int32(*DumpFunction) (struct rx_call *, void *),
		    void *rock, afs_int32 flags)
{
    /* declare stuff 'volatile' that may be used from setjmp/longjmp and may
     * be changing during the dump */
    struct rx_connection * volatile fromconn = NULL;
    struct rx_call * volatile fromcall = NULL;
    afs_int32 volatile clonetid = 0;
    afs_uint32 volatile clonevol = 0;

    afs_int32 tmp;
    afs_int32 fromtid = 0, rxError = 0, rcode = 0;
    afs_int32 code = 0, error = 0;
    afs_uint32 tmpVol;
    char vname[64];
    time_t tmv = fromdate;

    if (setjmp(env))
	ERROR_EXIT(EPIPE);
#ifndef AFS_NT40_ENV
    (void)signal(SIGPIPE, dump_sig_handler);
#endif
    (void)signal(SIGINT, dump_sig_handler);

    if (!fromdate) {
	VEPRINT("Full Dump ...\n");
    } else {
	VEPRINT1("Incremental Dump (as of %.24s)...\n",
		ctime(&tmv));
    }

    /* get connections to the servers */
    fromconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);

    VEPRINT1("Starting transaction on volume %u...", afromvol);
    code = AFSVolTransCreate_retry(fromconn, afromvol, afrompart, ITBusy, &fromtid);
    EGOTO1(error_exit, code,
	   "Could not start transaction on the volume %u to be dumped\n",
	   afromvol);
    VEDONE;

    /* Get a clone id */
    VEPRINT1("Allocating new volume id for clone of volume %u ...", afromvol);
    tmpVol = clonevol;
    code = ubik_VL_GetNewVolumeId(cstruct, 0, 1, &tmpVol);
    clonevol = tmpVol;
    EGOTO1(error_exit, code,
	   "Could not get an ID for the clone of volume %u from the VLDB\n",
	   afromvol);
    VEDONE;

    /* Do the clone. Default flags on clone are set to delete on salvage and out of service */
    VEPRINT2("Cloning source volume %u to clone volume %u...", afromvol,
	    clonevol);
    strcpy(vname, "dump-clone-temp");
    tmpVol = clonevol;
    code =
	AFSVolClone(fromconn, fromtid, 0, readonlyVolume, vname, &tmpVol);
    clonevol = tmpVol;
    EGOTO1(error_exit, code, "Failed to clone the source volume %u\n",
	   afromvol);
    VEDONE;

    VEPRINT1("Ending the transaction on the volume %u ...", afromvol);
    rcode = 0;
    code = AFSVolEndTrans(fromconn, fromtid, &rcode);
    fromtid = 0;
    if (!code)
	code = rcode;
    EGOTO1(error_exit, code,
	   "Failed to end the transaction on the volume %u\n", afromvol);
    VEDONE;


    VEPRINT1("Starting transaction on the cloned volume %u ...", clonevol);
    tmp = clonetid;
    code =
	AFSVolTransCreate_retry(fromconn, clonevol, afrompart, ITOffline,
			  &tmp);
    clonetid = tmp;
    EGOTO1(error_exit, code,
	   "Failed to start a transaction on the cloned volume%u\n",
	   clonevol);
    VEDONE;

    VEPRINT1("Setting flags on cloned volume %u ...", clonevol);
    code = AFSVolSetFlags(fromconn, clonetid, VTDeleteOnSalvage | VTOutOfService);	/*redundant */
    EGOTO1(error_exit, code, "Could not set falgs on the cloned volume %u\n",
	   clonevol);
    VEDONE;


    fromcall = rx_NewCall(fromconn);

    VEPRINT1("Starting volume dump from cloned volume %u...", clonevol);
    if (flags & VOLDUMPV2_OMITDIRS)
	code = StartAFSVolDumpV2(fromcall, clonetid, fromdate, flags);
    else
	code = StartAFSVolDump(fromcall, clonetid, fromdate);
    EGOTO(error_exit, code, "Could not start the dump process \n");
    VEDONE;

    VEPRINT1("Dumping volume %u...", afromvol);
    code = DumpFunction(fromcall, rock);
    EGOTO(error_exit, code, "Error while dumping volume \n");
    VEDONE;

  error_exit:
    /* now delete the clone */
    VEPRINT1("Deleting the cloned volume %u ...", clonevol);
    code = AFSVolDeleteVolume(fromconn, clonetid);
    if (code) {
	fprintf(STDERR, "Failed to delete the cloned volume %lu\n",
		(unsigned long)clonevol);
    } else {
	VEDONE;
    }

    if (fromcall) {
	code = rx_EndCall(fromcall, rxError);
	if (code) {
	    fprintf(STDERR, "Error in rx_EndCall\n");
	    if (!error)
		error = code;
	}
    }
    if (clonetid) {
	VEPRINT1("Ending transaction on cloned volume %u...", clonevol);
	code = AFSVolEndTrans(fromconn, clonetid, &rcode);
	if (code || rcode) {
	    fprintf(STDERR,
		    "Could not end transaction on the cloned volume %lu\n",
		    (unsigned long)clonevol);
	    if (!error)
		error = (code ? code : rcode);
	}
	VEDONE;
    }
    if (fromconn)
	rx_DestroyConnection(fromconn);

    PrintError("", error);
    return (error);
}



/*
 * Restore a volume <tovolid> <tovolname> on <toserver> <topart> from
 * the dump file <afilename>. WriteData does all the real work
 * after extracting params from the rock
 */
int
UV_RestoreVolume2(afs_uint32 toserver, afs_int32 topart, afs_uint32 tovolid,
		  afs_uint32 toparentid, char tovolname[], int flags,
		  afs_int32(*WriteData) (struct rx_call *, void *),
		  void *rock)
{
    struct rx_connection *toconn, *tempconn;
    struct rx_call *tocall;
    afs_int32 totid, code, rcode, vcode, terror = 0;
    afs_int32 rxError = 0;
    struct volser_status tstatus;
    struct volintInfo vinfo;
    char partName[10];
    char tovolreal[VOLSER_OLDMAXVOLNAME];
    afs_uint32 pvolid;
    afs_int32 temptid, pparentid;
    int success;
    struct nvldbentry entry, storeEntry;
    afs_int32 error;
    int islocked;
    struct restoreCookie cookie;
    int reuseID;
    afs_int32 volflag, voltype, volsertype;
    afs_int32 oldCreateDate, oldUpdateDate, newCreateDate, newUpdateDate;
    VolumeId oldCloneId = 0;
    VolumeId oldBackupId = 0;
    int index, same, errcode;
    char apartName[10];
    char hoststr[16];

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

    if (flags & RV_RDONLY) {
	voltype = ROVOL;
	volsertype = volser_RO;
    } else {
	voltype = RWVOL;
	volsertype = volser_RW;
    }

    pvolid = tovolid;
    pparentid = toparentid;
    toconn = UV_Bind(toserver, AFSCONF_VOLUMEPORT);
    if (pvolid == 0) {		/*alot a new id if needed */
	vcode = VLDB_GetEntryByName(tovolname, &entry);
	if (vcode == VL_NOENT) {
	    vcode = ubik_VL_GetNewVolumeId(cstruct, 0, 1, &pvolid);
	    if (vcode) {
		fprintf(STDERR, "Could not get an Id for the volume %s\n",
			tovolname);
		error = vcode;
		goto refail;
	    }
	    reuseID = 0;
	} else if (flags & RV_RDONLY) {
	    if (entry.flags & RW_EXISTS) {
		fprintf(STDERR,
			"Entry for ReadWrite volume %s already exists!\n",
			entry.name);
		error = VOLSERBADOP;
		goto refail;
	    }
	    if (!entry.volumeId[ROVOL]) {
		fprintf(STDERR,
			"Existing entry for volume %s has no ReadOnly ID\n",
			tovolname);
		error = VOLSERBADOP;
		goto refail;
	    }
	    pvolid = entry.volumeId[ROVOL];
	    pparentid = entry.volumeId[RWVOL];
	} else {
	    pvolid = entry.volumeId[RWVOL];
	    pparentid = entry.volumeId[RWVOL];
	}
    }
    if (!pparentid) pparentid = pvolid;
    /* at this point we have a volume id to use/reuse for the volume to be restored */
    strncpy(tovolreal, tovolname, VOLSER_OLDMAXVOLNAME);

    if (strlen(tovolname) > (VOLSER_OLDMAXVOLNAME - 1)) {
	EGOTO1(refail, VOLSERBADOP,
	       "The volume name %s exceeds the maximum limit of (VOLSER_OLDMAXVOLNAME -1 ) bytes\n",
	       tovolname);
    } else {
	if ((pparentid != pvolid) && (flags & RV_RDONLY)) {
	    if (strlen(tovolname) > (VOLSER_OLDMAXVOLNAME - 10)) {
		EGOTO1(refail, VOLSERBADOP,
		       "The volume name %s exceeds the maximum limit of (VOLSER_OLDMAXVOLNAME -1 ) bytes\n", tovolname);
	    }
	    snprintf(tovolreal, VOLSER_OLDMAXVOLNAME, "%s.readonly", tovolname);
	}
    }
    MapPartIdIntoName(topart, partName);
    fprintf(STDOUT, "Restoring volume %s Id %lu on server %s partition %s ..",
	    tovolreal, (unsigned long)pvolid,
            noresolve ? afs_inet_ntoa_r(toserver, hoststr) :
	    hostutil_GetNameByINet(toserver), partName);
    fflush(STDOUT);
    code =
	AFSVolCreateVolume(toconn, topart, tovolreal, volsertype, pparentid, &pvolid,
			   &totid);
    if (code) {
	if (flags & RV_FULLRST) {	/* full restore: delete then create anew */
	    code = DoVolDelete(toconn, pvolid, topart, "the previous", 0,
			       &tstatus, NULL);
	    if (code && code != VNOVOL) {
		error = code;
		goto refail;
	    }

	    code =
		AFSVolCreateVolume(toconn, topart, tovolreal, volsertype, pparentid,
				   &pvolid, &totid);
	    EGOTO1(refail, code, "Could not create new volume %u\n", pvolid);
	} else {
	    code =
		AFSVolTransCreate_retry(toconn, pvolid, topart, ITOffline, &totid);
	    EGOTO1(refail, code, "Failed to start transaction on %u\n",
		   pvolid);

	    code = AFSVolGetStatus(toconn, totid, &tstatus);
	    EGOTO1(refail, code, "Could not get timestamp from volume %u\n",
		   pvolid);

	}
	oldCreateDate = tstatus.creationDate;
	oldUpdateDate = tstatus.updateDate;
	oldCloneId = tstatus.cloneID;
	oldBackupId = tstatus.backupID;
    } else {
	oldCreateDate = 0;
	oldUpdateDate = 0;
    }

    cookie.parent = pparentid;
    cookie.type = voltype;
    cookie.clone = 0;
    strncpy(cookie.name, tovolreal, VOLSER_OLDMAXVOLNAME);

    tocall = rx_NewCall(toconn);
    terror = StartAFSVolRestore(tocall, totid, 1, &cookie);
    if (terror) {
	fprintf(STDERR, "Volume restore Failed \n");
	error = terror;
	goto refail;
    }
    code = WriteData(tocall, rock);
    if (code) {
	fprintf(STDERR, "Could not transmit data\n");
	error = code;
	goto refail;
    }
    terror = rx_EndCall(tocall, rxError);
    tocall = (struct rx_call *)0;
    if (terror) {
	fprintf(STDERR, "rx_EndCall Failed \n");
	error = terror;
	goto refail;
    }
    code = AFSVolGetStatus(toconn, totid, &tstatus);
    if (code) {
	fprintf(STDERR,
		"Could not get status information about the volume %lu\n",
		(unsigned long)pvolid);
	error = code;
	goto refail;
    }
    code = AFSVolSetIdsTypes(toconn, totid, tovolreal, voltype, pparentid,
		                oldCloneId, oldBackupId);
    if (code) {
	fprintf(STDERR, "Could not set the right type and IDs on %lu\n",
		(unsigned long)pvolid);
	error = code;
	goto refail;
    }

    if (flags & RV_CRDUMP)
	newCreateDate = tstatus.creationDate;
    else if (flags & RV_CRKEEP && oldCreateDate != 0)
	newCreateDate = oldCreateDate;
    else
	newCreateDate = time(0);
    if (flags & RV_LUDUMP)
	newUpdateDate = tstatus.updateDate;
    else if (flags & RV_LUKEEP)
	newUpdateDate = oldUpdateDate;
    else
	newUpdateDate = time(0);
    code = AFSVolSetDate(toconn,totid, newCreateDate);
    if (code) {
	fprintf(STDERR, "Could not set the 'creation' date on %u\n", pvolid);
	error = code;
	goto refail;
    }

    init_volintInfo(&vinfo);
    vinfo.creationDate = newCreateDate;
    vinfo.updateDate = newUpdateDate;
    code = AFSVolSetInfo(toconn, totid, &vinfo);
    if (code) {
	fprintf(STDERR, "Could not set the 'last updated' date on %u\n",
		pvolid);
	error = code;
	goto refail;
    }

    volflag = ((flags & RV_OFFLINE) ? VTOutOfService : 0);	/* off or on-line */
    code = AFSVolSetFlags(toconn, totid, volflag);
    if (code) {
	fprintf(STDERR, "Could not mark %lu online\n", (unsigned long)pvolid);
	error = code;
	goto refail;
    }

/* It isn't handled right in refail */
    code = AFSVolEndTrans(toconn, totid, &rcode);
    totid = 0;
    if (!code)
	code = rcode;
    if (code) {
	fprintf(STDERR, "Could not end transaction on %lu\n",
		(unsigned long)pvolid);
	error = code;
	goto refail;
    }

    success = 1;
    fprintf(STDOUT, " done\n");
    fflush(STDOUT);
    if (success && (!reuseID || (flags & RV_FULLRST))) {
	/* Volume was restored on the file server, update the
	 * VLDB to reflect the change.
	 */
	vcode = VLDB_GetEntryByID(pvolid, voltype, &entry);
	if (vcode && vcode != VL_NOENT && vcode != VL_ENTDELETED) {
	    fprintf(STDERR,
		    "Could not fetch the entry for volume number %lu from VLDB \n",
		    (unsigned long)pvolid);
	    error = vcode;
	    goto refail;
	}
	if (!vcode)
	    MapHostToNetwork(&entry);
	if (vcode == VL_NOENT) {	/* it doesnot exist already */
	    /*make the vldb return this indication specifically */
	    VPRINT("------- Creating a new VLDB entry ------- \n");
	    strcpy(entry.name, tovolname);
	    entry.nServers = 1;
	    entry.serverNumber[0] = toserver;	/*should be indirect */
	    entry.serverPartition[0] = topart;
	    entry.serverFlags[0] = (flags & RV_RDONLY) ? ITSROVOL : ITSRWVOL;
	    entry.flags = (flags & RV_RDONLY) ? RO_EXISTS : RW_EXISTS;
	    if (flags & RV_RDONLY)
		entry.volumeId[ROVOL] = pvolid;
	    else if (tstatus.cloneID != 0) {
		entry.volumeId[ROVOL] = tstatus.cloneID;	/*this should come from status info on the volume if non zero */
	    } else
		entry.volumeId[ROVOL] = INVALID_BID;
	    entry.volumeId[RWVOL] = pparentid;
	    entry.cloneId = 0;
	    if (tstatus.backupID != 0) {
		entry.volumeId[BACKVOL] = tstatus.backupID;
		/*this should come from status info on the volume if non zero */
	    } else
		entry.volumeId[BACKVOL] = INVALID_BID;
	    MapNetworkToHost(&entry, &storeEntry);
	    vcode = VLDB_CreateEntry(&storeEntry);
	    if (vcode) {
		fprintf(STDERR,
			"Could not create the VLDB entry for volume number %lu  \n",
			(unsigned long)pvolid);
		error = vcode;
		goto refail;
	    }
	    islocked = 0;
	    if (verbose)
		EnumerateEntry(&entry);
	} else {		/*update the existing entry */
	    if (verbose) {
		fprintf(STDOUT, "Updating the existing VLDB entry\n");
		fprintf(STDOUT, "------- Old entry -------\n");
		EnumerateEntry(&entry);
		fprintf(STDOUT, "------- New entry -------\n");
	    }
	    vcode =
		ubik_VL_SetLock(cstruct, 0, pvolid, voltype,
			  VLOP_RESTORE);
	    if (vcode) {
		fprintf(STDERR,
			"Could not lock the entry for volume number %lu \n",
			(unsigned long)pvolid);
		error = vcode;
		goto refail;
	    }
	    islocked = 1;
	    strcpy(entry.name, tovolname);

	    /* Update the vlentry with the new information */
	    if (flags & RV_RDONLY)
		index = Lp_ROMatch(toserver, topart, &entry) - 1;
	    else
		index = Lp_GetRwIndex(&entry);
	    if (index == -1) {
		/* Add the new site for the volume being restored */
		entry.serverNumber[entry.nServers] = toserver;
		entry.serverPartition[entry.nServers] = topart;
		entry.serverFlags[entry.nServers] =
		    (flags & RV_RDONLY) ? ITSROVOL : ITSRWVOL;
		entry.nServers++;
	    } else {
		/* This volume should be deleted on the old site
		 * if its different from new site.
		 */
		same =
		    VLDB_IsSameAddrs(toserver, entry.serverNumber[index],
				     &errcode);
		if (errcode)
		    EPRINT2(errcode,
			    "Failed to get info about server's %d address(es) from vlserver (err=%d)\n",
			    toserver, errcode);
		if ((!errcode && !same)
		    || (entry.serverPartition[index] != topart)) {
		    if (flags & RV_NODEL) {
			VPRINT2
			    ("Not deleting the previous volume %u on server %s, ...",
			     pvolid,
                             noresolve ? afs_inet_ntoa_r(entry.serverNumber[index], hoststr) :
			     hostutil_GetNameByINet(entry.serverNumber[index]));
		    } else {
			tempconn =
			    UV_Bind(entry.serverNumber[index],
				    AFSCONF_VOLUMEPORT);

			MapPartIdIntoName(entry.serverPartition[index],
					  apartName);
			VPRINT3
			    ("Deleting the previous volume %u on server %s, partition %s ...",
			     pvolid,
                             noresolve ? afs_inet_ntoa_r(entry.serverNumber[index], hoststr) :
			     hostutil_GetNameByINet(entry.serverNumber[index]),
			     apartName);
			code = DoVolDelete(tempconn, pvolid,
					   entry.serverPartition[index],
					   "the", 0, NULL, NULL);
			if (code && code != VNOVOL) {
			    error = code;
			    goto refail;
			}
			MapPartIdIntoName(entry.serverPartition[index],
					  partName);
		    }
		}
		entry.serverNumber[index] = toserver;
		entry.serverPartition[index] = topart;
	    }

	    entry.flags |= (flags & RV_RDONLY) ? RO_EXISTS : RW_EXISTS;
	    MapNetworkToHost(&entry, &storeEntry);
	    vcode =
		VLDB_ReplaceEntry(pvolid, voltype, &storeEntry,
				  LOCKREL_OPCODE | LOCKREL_AFSID |
				  LOCKREL_TIMESTAMP);
	    if (vcode) {
		fprintf(STDERR,
			"Could not update the entry for volume number %lu  \n",
			(unsigned long)pvolid);
		error = vcode;
		goto refail;
	    }
	    islocked = 0;
	    if (verbose)
		EnumerateEntry(&entry);
	}


    }
  refail:
    if (tocall) {
	code = rx_EndCall(tocall, rxError);
	if (!error)
	    error = code;
    }
    if (islocked) {
	vcode =
	    ubik_VL_ReleaseLock(cstruct, 0, pvolid, voltype,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (vcode) {
	    fprintf(STDERR,
		    "Could not release lock on the VLDB entry for the volume %lu\n",
		    (unsigned long)pvolid);
	    if (!error)
		error = vcode;
	}
    }
    if (totid) {
	code = AFSVolEndTrans(toconn, totid, &rcode);
	if (!code)
	    code = rcode;
	if (code) {
	    fprintf(STDERR, "Could not end transaction on the volume %lu \n",
		    (unsigned long)pvolid);
	    if (!error)
		error = code;
	}
    }
    if (temptid) {
	code = AFSVolEndTrans(toconn, temptid, &rcode);
	if (!code)
	    code = rcode;
	if (code) {
	    fprintf(STDERR, "Could not end transaction on the volume %lu \n",
		    (unsigned long)pvolid);
	    if (!error)
		error = code;
	}
    }
    if (tempconn)
	rx_DestroyConnection(tempconn);
    if (toconn)
	rx_DestroyConnection(toconn);
    PrintError("", error);
    return error;
}

int
UV_RestoreVolume(afs_uint32 toserver, afs_int32 topart, afs_uint32 tovolid,
		 char tovolname[], int flags,
		 afs_int32(*WriteData) (struct rx_call *, void *),
		 void *rock)
{
    return UV_RestoreVolume2(toserver, topart, tovolid, 0, tovolname, flags,
			     WriteData, rock);
}


/*unlocks the vldb entry associated with <volid> */
int
UV_LockRelease(afs_uint32 volid)
{
    afs_int32 vcode;

    VPRINT("Binding to the VLDB server\n");
    vcode =
	ubik_VL_ReleaseLock(cstruct, 0, volid, -1,
		  LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
    if (vcode) {
	fprintf(STDERR,
		"Could not unlock the entry for volume number %lu in VLDB \n",
		(unsigned long)volid);
	PrintError("", vcode);
	return (vcode);
    }
    VPRINT("VLDB updated\n");
    return 0;

}

/* old interface to add rosites */
int
UV_AddSite(afs_uint32 server, afs_int32 part, afs_uint32 volid,
	   afs_int32 valid)
{
    return UV_AddSite2(server, part, volid, 0, valid);
}

/*adds <server> and <part> as a readonly replication site for <volid>
*in vldb */
int
UV_AddSite2(afs_uint32 server, afs_int32 part, afs_uint32 volid,
	    afs_uint32 rovolid, afs_int32 valid)
{
    int j, nro = 0, islocked = 0;
    struct nvldbentry entry, storeEntry, entry2;
    afs_int32 vcode, error = 0;
    char apartName[10];

    error = ubik_VL_SetLock(cstruct, 0, volid, RWVOL, VLOP_ADDSITE);
    if (error) {
	fprintf(STDERR,
		" Could not lock the VLDB entry for the volume %lu \n",
		(unsigned long)volid);
	goto asfail;
    }
    islocked = 1;

    error = VLDB_GetEntryByID(volid, RWVOL, &entry);
    if (error) {
	fprintf(STDERR,
		"Could not fetch the VLDB entry for volume number %lu  \n",
		(unsigned long)volid);
	goto asfail;

    }
    if (!ISNAMEVALID(entry.name)) {
	fprintf(STDERR,
		"Volume name %s is too long, rename before adding site\n",
		entry.name);
	error = VOLSERBADOP;
	goto asfail;
    }
    MapHostToNetwork(&entry);

    /* See if it's too many entries */
    if (entry.nServers >= NMAXNSERVERS) {
	fprintf(STDERR, "Total number of entries will exceed %u\n",
		NMAXNSERVERS);
	error = VOLSERBADOP;
	goto asfail;
    }

    /* See if it's on the same server */
    for (j = 0; j < entry.nServers; j++) {
	if (entry.serverFlags[j] & ITSROVOL) {
	    nro++;
	    if (VLDB_IsSameAddrs(server, entry.serverNumber[j], &error)) {
		if (error) {
		    fprintf(STDERR,
			    "Failed to get info about server's %d address(es) from vlserver (err=%d); aborting call!\n",
			    server, error);
		} else {
		    MapPartIdIntoName(entry.serverPartition[j], apartName);
		    fprintf(STDERR,
			    "RO already exists on partition %s. Multiple ROs on a single server aren't allowed\n",
			    apartName);
		    error = VOLSERBADOP;
		}
		goto asfail;
	    }
	}
    }

    /* See if it's too many RO sites - leave one for the RW */
    if (nro >= NMAXNSERVERS - 1) {
	fprintf(STDERR, "Total number of sites will exceed %u\n",
		NMAXNSERVERS - 1);
	error = VOLSERBADOP;
	goto asfail;
    }

    /* if rovolid == 0, we leave the RO volume id alone. If the volume doesn't
     * have an RO volid at this point (i.e. entry.volumeId[ROVOL] ==
     * INVALID_BID) and we leave it alone, it gets an RO volid at release-time.
     */
    if (rovolid) {
	if (entry.volumeId[ROVOL] == INVALID_BID) {
	    vcode = VLDB_GetEntryByID(rovolid, -1, &entry2);
	    if (!vcode) {
		fprintf(STDERR, "Volume ID %d already exists\n", rovolid);
		return VVOLEXISTS;
	    }
	    VPRINT1("Using RO volume id %d.\n", rovolid);
	    entry.volumeId[ROVOL] = rovolid;
	} else {
	    fprintf(STDERR, "Ignoring given RO id %d, since volume already has RO id %d\n",
		rovolid, entry.volumeId[ROVOL]);
	}
    }

    VPRINT("Adding a new site ...");
    entry.serverNumber[entry.nServers] = server;
    entry.serverPartition[entry.nServers] = part;
    if (!valid) {
	entry.serverFlags[entry.nServers] = (ITSROVOL | RO_DONTUSE);
    } else {
	entry.serverFlags[entry.nServers] = (ITSROVOL);
    }
    entry.nServers++;

    MapNetworkToHost(&entry, &storeEntry);
    error =
	VLDB_ReplaceEntry(volid, RWVOL, &storeEntry,
			  LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
    if (error) {
	fprintf(STDERR, "Could not update entry for volume %lu \n",
		(unsigned long)volid);
	goto asfail;
    }
    islocked = 0;
    VDONE;

  asfail:
    if (islocked) {
	vcode =
	    ubik_VL_ReleaseLock(cstruct, 0, volid, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (vcode) {
	    fprintf(STDERR,
		    "Could not release lock on volume entry for %lu \n",
		    (unsigned long)volid);
	    PrintError("", vcode);
	}
    }

    PrintError("", error);
    return error;
}

/*removes <server> <part> as read only site for <volid> from the vldb */
int
UV_RemoveSite(afs_uint32 server, afs_int32 part, afs_uint32 volid)
{
    afs_int32 vcode;
    struct nvldbentry entry, storeEntry;

    vcode = ubik_VL_SetLock(cstruct, 0, volid, RWVOL, VLOP_ADDSITE);
    if (vcode) {
	fprintf(STDERR, " Could not lock the VLDB entry for volume %lu \n",
		(unsigned long)volid);
	PrintError("", vcode);
	return (vcode);
    }
    vcode = VLDB_GetEntryByID(volid, RWVOL, &entry);
    if (vcode) {
	fprintf(STDERR,
		"Could not fetch the entry for volume number %lu from VLDB \n",
		(unsigned long)volid);
	PrintError("", vcode);
	return (vcode);
    }
    MapHostToNetwork(&entry);
    if (!Lp_ROMatch(server, part, &entry)) {
	/*this site doesnot exist  */
	fprintf(STDERR, "This site is not a replication site \n");
	vcode =
	    ubik_VL_ReleaseLock(cstruct, 0, volid, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (vcode) {
	    fprintf(STDERR, "Could not update entry for volume %lu \n",
		    (unsigned long)volid);
	    PrintError("", vcode);
	    ubik_VL_ReleaseLock(cstruct, 0, volid, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	    return (vcode);
	}
	return VOLSERBADOP;
    } else {			/*remove the rep site */
	Lp_SetROValue(&entry, server, part, 0, 0);
	entry.nServers--;
	if ((entry.nServers == 1) && (entry.flags & RW_EXISTS))
	    entry.flags &= ~RO_EXISTS;
	if (entry.nServers < 1) {	/*this is the last ref */
	    VPRINT1("Deleting the VLDB entry for %u ...", volid);
	    fflush(STDOUT);
	    vcode = ubik_VL_DeleteEntry(cstruct, 0, volid, ROVOL);
	    if (vcode) {
		fprintf(STDERR,
			"Could not delete VLDB entry for volume %lu \n",
			(unsigned long)volid);
		PrintError("", vcode);
		return (vcode);
	    }
	    VDONE;
	}
	MapNetworkToHost(&entry, &storeEntry);
	fprintf(STDOUT, "Deleting the replication site for volume %lu ...",
		(unsigned long)volid);
	fflush(STDOUT);
	vcode =
	    VLDB_ReplaceEntry(volid, RWVOL, &storeEntry,
			      LOCKREL_OPCODE | LOCKREL_AFSID |
			      LOCKREL_TIMESTAMP);
	if (vcode) {
	    fprintf(STDERR,
		    "Could not release lock on volume entry for %lu \n",
		    (unsigned long)volid);
	    PrintError("", vcode);
	    ubik_VL_ReleaseLock(cstruct, 0, volid, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	    return (vcode);
	}
	VDONE;
    }
    return 0;
}

/*sets <server> <part> as read/write site for <volid> in the vldb */
int
UV_ChangeLocation(afs_uint32 server, afs_int32 part, afs_uint32 volid)
{
    afs_int32 vcode;
    struct nvldbentry entry, storeEntry;
    int index;

    vcode = ubik_VL_SetLock(cstruct, 0, volid, RWVOL, VLOP_ADDSITE);
    if (vcode) {
	fprintf(STDERR, " Could not lock the VLDB entry for volume %lu \n",
		(unsigned long)volid);
	PrintError("", vcode);
	return (vcode);
    }
    vcode = VLDB_GetEntryByID(volid, RWVOL, &entry);
    if (vcode) {
	fprintf(STDERR,
		"Could not fetch the entry for volume number %lu from VLDB \n",
		(unsigned long)volid);
	PrintError("", vcode);
	return (vcode);
    }
    MapHostToNetwork(&entry);
    index = Lp_GetRwIndex(&entry);
    if (index < 0) {
	/* no RW site exists  */
	fprintf(STDERR, "No existing RW site for volume %lu",
		(unsigned long)volid);
	vcode =
	    ubik_VL_ReleaseLock(cstruct, 0, volid, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (vcode) {
	    fprintf(STDERR,
		    "Could not release lock on entry for volume %lu \n",
		    (unsigned long)volid);
	    PrintError("", vcode);
	    return (vcode);
	}
	return VOLSERBADOP;
    } else {			/* change the RW site */
	entry.serverNumber[index] = server;
	entry.serverPartition[index] = part;
	MapNetworkToHost(&entry, &storeEntry);
	vcode =
	    VLDB_ReplaceEntry(volid, RWVOL, &storeEntry,
			      LOCKREL_OPCODE | LOCKREL_AFSID |
			      LOCKREL_TIMESTAMP);
	if (vcode) {
	    fprintf(STDERR, "Could not update entry for volume %lu \n",
		    (unsigned long)volid);
	    PrintError("", vcode);
	    ubik_VL_ReleaseLock(cstruct, 0, volid, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	    return (vcode);
	}
	VDONE;
    }
    return 0;
}

/*list all the partitions on <aserver> */
int
UV_ListPartitions(afs_uint32 aserver, struct partList *ptrPartList,
		  afs_int32 * cntp)
{
    struct rx_connection *aconn;
    struct pIDs partIds;
    struct partEntries partEnts;
    int i, j = 0, code;

    *cntp = 0;
    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);

    partEnts.partEntries_len = 0;
    partEnts.partEntries_val = NULL;
    code = AFSVolXListPartitions(aconn, &partEnts);	/* this is available only on new servers */
    if (code == RXGEN_OPCODE) {
	for (i = 0; i < 26; i++)	/* try old interface */
	    partIds.partIds[i] = -1;
	code = AFSVolListPartitions(aconn, &partIds);
	if (!code) {
	    for (i = 0; i < 26; i++) {
		if ((partIds.partIds[i]) != -1) {
		    ptrPartList->partId[j] = partIds.partIds[i];
		    ptrPartList->partFlags[j] = PARTVALID;
		    j++;
		} else
		    ptrPartList->partFlags[i] = 0;
	    }
	    *cntp = j;
	}
    } else if (!code) {
	*cntp = partEnts.partEntries_len;
	if (*cntp > VOLMAXPARTS) {
	    fprintf(STDERR,
		    "Warning: number of partitions on the server too high %d (process only %d)\n",
		    *cntp, VOLMAXPARTS);
	    *cntp = VOLMAXPARTS;
	}
	for (i = 0; i < *cntp; i++) {
	    ptrPartList->partId[i] = partEnts.partEntries_val[i];
	    ptrPartList->partFlags[i] = PARTVALID;
	}
	free(partEnts.partEntries_val);
    }

   /* out: */
    if (code)
	fprintf(STDERR,
		"Could not fetch the list of partitions from the server\n");
    PrintError("", code);
    if (aconn)
	rx_DestroyConnection(aconn);
    return code;
}


/*zap the list of volumes specified by volPtrArray (the volCloneId field).
 This is used by the backup system */
int
UV_ZapVolumeClones(afs_uint32 aserver, afs_int32 apart,
		   struct volDescription *volPtr, afs_int32 arraySize)
{
    struct rx_connection *aconn;
    struct volDescription *curPtr;
    int curPos;
    afs_int32 code = 0;
    afs_int32 success = 1;

    aconn = (struct rx_connection *)0;
    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);
    curPos = 0;
    for (curPtr = volPtr; curPos < arraySize; curPtr++) {
	if (curPtr->volFlags & CLONEVALID) {
	    curPtr->volFlags &= ~CLONEZAPPED;
	    success = 1;

	    code = DoVolDelete(aconn, curPtr->volCloneId, apart,
			       "clone", 0, NULL, NULL);
	    if (code)
		success = 0;

	    if (success)
		curPtr->volFlags |= CLONEZAPPED;
	    if (!success)
		fprintf(STDERR, "Could not zap volume %lu\n",
			(unsigned long)curPtr->volCloneId);
	    if (success)
		VPRINT2("Clone of %s %u deleted\n", curPtr->volName,
			curPtr->volCloneId);
	    curPos++;
	}
    }
    if (aconn)
	rx_DestroyConnection(aconn);
    return 0;
}

/*return a list of clones of the volumes specified by volPtrArray. Used by the
 backup system */
int
UV_GenerateVolumeClones(afs_uint32 aserver, afs_int32 apart,
			struct volDescription *volPtr, afs_int32 arraySize)
{
    struct rx_connection *aconn;
    struct volDescription *curPtr;
    int curPos;
    afs_int32 code = 0;
    afs_int32 rcode = 0;
    afs_int32 tid;
    int reuseCloneId = 0;
    afs_uint32 curCloneId = 0;
    char cloneName[256];	/*max vol name */

    aconn = (struct rx_connection *)0;
    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);
    curPos = 0;
    if ((volPtr->volFlags & REUSECLONEID) && (volPtr->volFlags & ENTRYVALID))
	reuseCloneId = 1;
    else {			/*get a bunch of id's from vldb */
	code =
	    ubik_VL_GetNewVolumeId(cstruct, 0, arraySize, &curCloneId);
	if (code) {
	    fprintf(STDERR, "Could not get ID's for the clone from VLDB\n");
	    PrintError("", code);
	    return code;
	}
    }

    for (curPtr = volPtr; curPos < arraySize; curPtr++) {
	if (curPtr->volFlags & ENTRYVALID) {

	    curPtr->volFlags |= CLONEVALID;
	    /*make a clone of curParentId and record as curPtr->volCloneId */
	    code =
		AFSVolTransCreate_retry(aconn, curPtr->volId, apart, ITOffline,
				  &tid);
	    if (code)
		VPRINT2("Clone for volume %s %u failed \n", curPtr->volName,
			curPtr->volId);
	    if (code) {
		curPtr->volFlags &= ~CLONEVALID;	/*cant clone */
		curPos++;
		continue;
	    }
	    if (strlen(curPtr->volName) < (VOLSER_OLDMAXVOLNAME - 9)) {
		strcpy(cloneName, curPtr->volName);
		strcat(cloneName, "-tmpClone-");
	    } else
		strcpy(cloneName, "-tmpClone");
	    if (reuseCloneId) {
		curPtr->volCloneId = curCloneId;
		curCloneId++;
	    }

	    code =
		AFSVolClone(aconn, tid, 0, readonlyVolume, cloneName,
			    &(curPtr->volCloneId));
	    if (code) {
		curPtr->volFlags &= ~CLONEVALID;
		curPos++;
		fprintf(STDERR, "Could not clone %s due to error %lu\n",
			curPtr->volName, (unsigned long)code);
		code = AFSVolEndTrans(aconn, tid, &rcode);
		if (code)
		    fprintf(STDERR, "WARNING: could not end transaction\n");
		continue;
	    }
	    VPRINT2("********** Cloned %s temporary %u\n", cloneName,
		    curPtr->volCloneId);
	    code = AFSVolEndTrans(aconn, tid, &rcode);
	    if (code || rcode) {
		curPtr->volFlags &= ~CLONEVALID;
		curPos++;
		continue;
	    }

	    curPos++;
	}
    }
    if (aconn)
	rx_DestroyConnection(aconn);
    return 0;
}


/*list all the volumes on <aserver> and <apart>. If all = 1, then all the
* relevant fields of the volume are also returned. This is a heavy weight operation.*/
int
UV_ListVolumes(afs_uint32 aserver, afs_int32 apart, int all,
	       struct volintInfo **resultPtr, afs_int32 * size)
{
    struct rx_connection *aconn;
    afs_int32 code = 0;
    volEntries volumeInfo;

    code = 0;
    *size = 0;
    *resultPtr = (volintInfo *) 0;
    volumeInfo.volEntries_val = (volintInfo *) 0;	/*this hints the stub to allocate space */
    volumeInfo.volEntries_len = 0;

    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);
    code = AFSVolListVolumes(aconn, apart, all, &volumeInfo);
    if (code) {
	fprintf(STDERR,
		"Could not fetch the list of volumes from the server\n");
    } else {
	*resultPtr = volumeInfo.volEntries_val;
	*size = volumeInfo.volEntries_len;
    }

    if (aconn)
	rx_DestroyConnection(aconn);
    PrintError("", code);
    return code;
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
UV_XListVolumes(afs_uint32 a_serverID, afs_int32 a_partID, int a_all,
		struct volintXInfo **a_resultPP,
		afs_int32 * a_numEntsInResultP)
{
    struct rx_connection *rxConnP;	/*Ptr to the Rx connection involved */
    afs_int32 code;		/*Error code to return */
    volXEntries volumeXInfo;	/*Area for returned extended vol info */

    /*
     * Set up our error code and the area for returned extended volume info.
     * We set the val field to a null pointer as a hint for the stub to
     * allocate space.
     */
    code = 0;
    *a_numEntsInResultP = 0;
    *a_resultPP = (volintXInfo *) 0;
    volumeXInfo.volXEntries_val = (volintXInfo *) 0;
    volumeXInfo.volXEntries_len = 0;

    /*
     * Bind to the Volume Server port on the File Server machine in question,
     * then go for it.
     */
    rxConnP = UV_Bind(a_serverID, AFSCONF_VOLUMEPORT);
    code = AFSVolXListVolumes(rxConnP, a_partID, a_all, &volumeXInfo);
    if (code)
	fprintf(STDERR, "[UV_XListVolumes] Couldn't fetch volume list\n");
    else {
	/*
	 * We got the info; pull out the pointer to where the results lie
	 * and how many entries are there.
	 */
	*a_resultPP = volumeXInfo.volXEntries_val;
	*a_numEntsInResultP = volumeXInfo.volXEntries_len;
    }

    /*
     * If we got an Rx connection, throw it away.
     */
    if (rxConnP)
	rx_DestroyConnection(rxConnP);

    PrintError("", code);
    return (code);
}				/*UV_XListVolumes */

/* get all the information about volume <volid> on <aserver> and <apart> */
int
UV_ListOneVolume(afs_uint32 aserver, afs_int32 apart, afs_uint32 volid,
		 struct volintInfo **resultPtr)
{
    struct rx_connection *aconn;
    afs_int32 code = 0;
    volEntries volumeInfo;

    code = 0;

    *resultPtr = (volintInfo *) 0;
    volumeInfo.volEntries_val = (volintInfo *) 0;	/*this hints the stub to allocate space */
    volumeInfo.volEntries_len = 0;

    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);
    code = AFSVolListOneVolume(aconn, apart, volid, &volumeInfo);
    if (code) {
	fprintf(STDERR,
		"Could not fetch the information about volume %lu from the server\n",
		(unsigned long)volid);
    } else {
	*resultPtr = volumeInfo.volEntries_val;

    }

    if (aconn)
	rx_DestroyConnection(aconn);
    PrintError("", code);
    return code;
}

/*------------------------------------------------------------------------
 * EXPORTED UV_XListOneVolume
 *
 * Description:
 *	List the extended information for a volume on a particular File
 *	Server and partition.
 *
 * Arguments:
 *	a_serverID	   : Address of the File Server for which we want
 *				extended volume info.
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
UV_XListOneVolume(afs_uint32 a_serverID, afs_int32 a_partID, afs_uint32 a_volID,
		  struct volintXInfo **a_resultPP)
{
    struct rx_connection *rxConnP;	/*Rx connection to Volume Server */
    afs_int32 code;		/*Error code */
    volXEntries volumeXInfo;	/*Area for returned info */

    /*
     * Set up our error code, and the area we're in which we are returning
     * the info.  Setting the val field to a null pointer tells the stub
     * to allocate space for us.
     */
    code = 0;
    *a_resultPP = (volintXInfo *) 0;
    volumeXInfo.volXEntries_val = (volintXInfo *) 0;
    volumeXInfo.volXEntries_len = 0;

    /*
     * Bind to the Volume Server port on the File Server machine in question,
     * then go for it.
     */
    rxConnP = UV_Bind(a_serverID, AFSCONF_VOLUMEPORT);
    code = AFSVolXListOneVolume(rxConnP, a_partID, a_volID, &volumeXInfo);
    if (code)
	fprintf(STDERR,
		"[UV_XListOneVolume] Couldn't fetch the volume information\n");
    else
	/*
	 * We got the info; pull out the pointer to where the results lie.
	 */
	*a_resultPP = volumeXInfo.volXEntries_val;

    /*
     * If we got an Rx connection, throw it away.
     */
    if (rxConnP)
	rx_DestroyConnection(rxConnP);

    PrintError("", code);
    return code;
}

/* CheckVolume()
 *    Given a volume we read from a partition, check if it is
 *    represented in the VLDB correctly.
 *
 *    The VLDB is looked up by the RW volume id (not its name).
 *    The RW contains the true name of the volume (BK and RO set
 *       the name in the VLDB only on creation of the VLDB entry).
 *    We want rules strict enough that when we check all volumes
 *       on one partition, it does not need to be done again. IE:
 *       two volumes on different partitions won't constantly
 *       change a VLDB entry away from what the other set.
 *    For RW and BK volumes, we will always check the VLDB to see
 *       if the two exist on the server/partition. May seem redundant,
 *       but this is an easy check of the VLDB. IE: if the VLDB entry
 *       says the BK exists but no BK volume is there, we will detect
 *       this when we check the RW volume.
 *    VLDB entries are locked only when a change needs to be done.
 *    Output changed to look a lot like the "vos syncserv" otuput.
 */
static afs_int32
CheckVolume(volintInfo * volumeinfo, afs_uint32 aserver, afs_int32 apart,
	    afs_int32 * modentry, afs_uint32 * maxvolid,
            struct nvldbentry *aentry)
{
    int idx = 0;
    int j;
    afs_int32 code, error = 0;
    struct nvldbentry entry, storeEntry;
    char pname[10];
    int pass = 0, createentry, addvolume, modified, mod, doit = 1;
    afs_uint32 rwvolid;
    char hoststr[16];

    if (modentry) {
	if (*modentry == 1)
	    doit = 0;
	*modentry = 0;
    }
    rwvolid =
	((volumeinfo->type ==
	  RWVOL) ? volumeinfo->volid : volumeinfo->parentID);

  retry:
    /* Check to see if the VLDB is ok without locking it (pass 1).
     * If it will change, then lock the VLDB entry, read it again,
     * then make the changes to it (pass 2).
     */
    if (++pass == 2) {
	code = ubik_VL_SetLock(cstruct, 0, rwvolid, RWVOL, VLOP_DELETE);
	if (code) {
	    fprintf(STDERR, "Could not lock VLDB entry for %lu\n",
		    (unsigned long)rwvolid);
	    ERROR_EXIT(code);
	}
    }

    createentry = 0;		/* Do we need to create a VLDB entry */
    addvolume = 0;		/* Add this volume to the VLDB entry */
    modified = 0;		/* The VLDB entry was modified */

    if (aentry) {
	memcpy(&entry, aentry, sizeof(entry));
    } else {
	/* Read the entry from VLDB by its RW volume id */
	code = VLDB_GetEntryByID(rwvolid, RWVOL, &entry);
	if (code) {
	    if (code != VL_NOENT) {
		fprintf(STDOUT,
		        "Could not retrieve the VLDB entry for volume %lu \n",
		        (unsigned long)rwvolid);
		ERROR_EXIT(code);
	    }

	    memset(&entry, 0, sizeof(entry));
	    vsu_ExtractName(entry.name, volumeinfo->name);	/* Store name of RW */

	    createentry = 1;
	} else {
	    MapHostToNetwork(&entry);
	}
    }

    if (verbose && (pass == 1)) {
	fprintf(STDOUT, "_______________________________\n");
	fprintf(STDOUT, "\n-- status before -- \n");
	if (createentry) {
	    fprintf(STDOUT, "\n**does not exist**\n");
	} else {
	    if ((entry.flags & RW_EXISTS) || (entry.flags & RO_EXISTS)
		|| (entry.flags & BACK_EXISTS))
		EnumerateEntry(&entry);
	}
	fprintf(STDOUT, "\n");
    }

    if (volumeinfo->type == RWVOL) {	/* RW volume exists */
	if (createentry) {
	    idx = 0;
	    entry.nServers = 1;
	    addvolume++;
	} else {
	    /* Check existence of RW and BK volumes */
	    code = CheckVldbRWBK(&entry, &mod);
	    if (code)
		ERROR_EXIT(code);
	    if (mod)
		modified++;

	    idx = Lp_GetRwIndex(&entry);
	    if (idx == -1) {	/* RW index not found in the VLDB entry */
		idx = entry.nServers;	/* put it into next index */
		entry.nServers++;
		addvolume++;
	    } else {		/* RW index found in the VLDB entry. */
		/* Verify if this volume's location matches where the VLDB says it is */
		if (!Lp_Match(aserver, apart, &entry)) {
		    if (entry.flags & RW_EXISTS) {
			/* The RW volume exists elsewhere - report this one a duplicate */
			if (pass == 1) {
			    MapPartIdIntoName(apart, pname);
			    fprintf(STDERR,
				    "*** Warning: Orphaned RW volume %lu exists on %s %s\n",
				    (unsigned long)rwvolid,
                                    noresolve ?
                                    afs_inet_ntoa_r(aserver, hoststr) :
				    hostutil_GetNameByINet(aserver), pname);
			    MapPartIdIntoName(entry.serverPartition[idx],
					      pname);
			    fprintf(STDERR,
				    "    VLDB reports RW volume %lu exists on %s %s\n",
				    (unsigned long)rwvolid,
                                    noresolve ?
                                    afs_inet_ntoa_r(entry.serverNumber[idx], hoststr) :
				    hostutil_GetNameByINet(entry.
							   serverNumber[idx]),
				    pname);
			}
		    } else {
			/* The RW volume does not exist - have VLDB point to this one */
			addvolume++;

			/* Check for orphaned BK volume on old partition */
			if (entry.flags & BACK_EXISTS) {
			    if (pass == 1) {
				MapPartIdIntoName(entry.serverPartition[idx],
						  pname);
				fprintf(STDERR,
					"*** Warning: Orphaned BK volume %u exists on %s %s\n",
					entry.volumeId[BACKVOL],
                                        noresolve ?
                                        afs_inet_ntoa_r(entry.serverNumber[idx], hoststr) :
					hostutil_GetNameByINet(entry.
							       serverNumber
							       [idx]), pname);
				MapPartIdIntoName(apart, pname);
				fprintf(STDERR,
					"    VLDB reports its RW volume %lu exists on %s %s\n",
					(unsigned long)rwvolid,
                                        noresolve ?
                                        afs_inet_ntoa_r(aserver, hoststr) :
					hostutil_GetNameByINet(aserver),
					pname);
			    }
			}
		    }
		} else {
		    /* Volume location matches the VLDB location */
		    if ((volumeinfo->backupID && !entry.volumeId[BACKVOL])
			|| (volumeinfo->cloneID && !entry.volumeId[ROVOL])
			||
			(strncmp
			 (entry.name, volumeinfo->name,
			  VOLSER_OLDMAXVOLNAME) != 0)) {
			addvolume++;
		    }
		}
	    }
	}

	if (addvolume) {
	    entry.flags |= RW_EXISTS;
	    entry.volumeId[RWVOL] = rwvolid;
	    if (!entry.volumeId[BACKVOL])
		entry.volumeId[BACKVOL] = volumeinfo->backupID;
	    if (!entry.volumeId[ROVOL])
		entry.volumeId[ROVOL] = volumeinfo->cloneID;

	    entry.serverFlags[idx] = ITSRWVOL;
	    entry.serverNumber[idx] = aserver;
	    entry.serverPartition[idx] = apart;
	    strncpy(entry.name, volumeinfo->name, VOLSER_OLDMAXVOLNAME);

	    modified++;

	    /* One last check - to update BK if need to */
	    code = CheckVldbRWBK(&entry, &mod);
	    if (code)
		ERROR_EXIT(code);
	    if (mod)
		modified++;
	}
    }

    else if (volumeinfo->type == BACKVOL) {	/* A BK volume */
	if (createentry) {
	    idx = 0;
	    entry.nServers = 1;
	    addvolume++;
	} else {
	    /* Check existence of RW and BK volumes */
	    code = CheckVldbRWBK(&entry, &mod);
	    if (code)
		ERROR_EXIT(code);
	    if (mod)
		modified++;

	    idx = Lp_GetRwIndex(&entry);
	    if (idx == -1) {	/* RW index not found in the VLDB entry */
		idx = entry.nServers;	/* Put it into next index */
		entry.nServers++;
		addvolume++;
	    } else {		/* RW index found in the VLDB entry */
		/* Verify if this volume's location matches where the VLDB says it is */
		if (!Lp_Match(aserver, apart, &entry)) {
		    /* VLDB says RW and/or BK is elsewhere - report this BK volume orphaned */
		    if (pass == 1) {
			MapPartIdIntoName(apart, pname);
			fprintf(STDERR,
				"*** Warning: Orphaned BK volume %lu exists on %s %s\n",
				(unsigned long)volumeinfo->volid,
                                noresolve ?
                                afs_inet_ntoa_r(aserver, hoststr) :
				hostutil_GetNameByINet(aserver), pname);
			MapPartIdIntoName(entry.serverPartition[idx], pname);
			fprintf(STDERR,
				"    VLDB reports its RW/BK volume %lu exists on %s %s\n",
				(unsigned long)rwvolid,
                                noresolve ?
                                afs_inet_ntoa_r(entry.serverNumber[idx], hoststr) :
				hostutil_GetNameByINet(entry.
						       serverNumber[idx]),
				pname);
		    }
		} else {
		    if (volumeinfo->volid != entry.volumeId[BACKVOL]) {
			if (!(entry.flags & BACK_EXISTS)) {
			    addvolume++;
			} else if (volumeinfo->volid >
				   entry.volumeId[BACKVOL]) {
			    addvolume++;

			    if (pass == 1) {
				MapPartIdIntoName(entry.serverPartition[idx],
						  pname);
				fprintf(STDERR,
					"*** Warning: Orphaned BK volume %u exists on %s %s\n",
					entry.volumeId[BACKVOL],
                                        noresolve ?
                                        afs_inet_ntoa_r(aserver, hoststr) :
					hostutil_GetNameByINet(aserver),
					pname);
				fprintf(STDERR,
					"    VLDB reports its BK volume ID is %lu\n",
					(unsigned long)volumeinfo->volid);
			    }
			} else {
			    if (pass == 1) {
				MapPartIdIntoName(entry.serverPartition[idx],
						  pname);
				fprintf(STDERR,
					"*** Warning: Orphaned BK volume %lu exists on %s %s\n",
                                        (unsigned long)volumeinfo->volid,
                                        noresolve ?
                                        afs_inet_ntoa_r(aserver, hoststr) :
                                        hostutil_GetNameByINet(aserver),
					pname);
				fprintf(STDERR,
					"    VLDB reports its BK volume ID is %u\n",
					entry.volumeId[BACKVOL]);
			    }
			}
		    } else if (!entry.volumeId[BACKVOL]) {
			addvolume++;
		    }
		}
	    }
	}
	if (addvolume) {
	    entry.flags |= BACK_EXISTS;
	    entry.volumeId[RWVOL] = rwvolid;
	    entry.volumeId[BACKVOL] = volumeinfo->volid;

	    entry.serverNumber[idx] = aserver;
	    entry.serverPartition[idx] = apart;
	    entry.serverFlags[idx] = ITSBACKVOL;

	    modified++;
	}
    }

    else if (volumeinfo->type == ROVOL) {	/* A RO volume */
	if (volumeinfo->volid == entry.volumeId[ROVOL]) {
	    /* This is a quick check to see if the RO entry exists in the
	     * VLDB so we avoid the CheckVldbRO() call (which checks if each
	     * RO volume listed in the VLDB exists).
	     */
	    idx = Lp_ROMatch(aserver, apart, &entry) - 1;
	    if (idx == -1) {
		idx = entry.nServers;
		entry.nServers++;
		addvolume++;
	    } else {
		if (!(entry.flags & RO_EXISTS)) {
		    addvolume++;
		}
	    }
	} else {
	    /* Before we correct the VLDB entry, make sure all the
	     * ROs listed in the VLDB exist.
	     */
	    code = CheckVldbRO(&entry, &mod);
	    if (code)
		ERROR_EXIT(code);
	    if (mod)
		modified++;

	    if (!(entry.flags & RO_EXISTS)) {
		/* No RO exists in the VLDB entry - add this one */
		idx = entry.nServers;
		entry.nServers++;
		addvolume++;
	    } else if (volumeinfo->volid > entry.volumeId[ROVOL]) {
		/* The volume headers's RO ID does not match that in the VLDB entry,
		 * and the vol hdr's ID is greater (implies more recent). So delete
		 * all the RO volumes listed in VLDB entry and add this volume.
		 */
		for (j = 0; j < entry.nServers; j++) {
		    if (entry.serverFlags[j] & ITSROVOL) {
			/* Verify this volume exists and print message we are orphaning it */
			if (pass == 1) {
			    MapPartIdIntoName(apart, pname);
			    fprintf(STDERR,
				    "*** Warning: Orphaned RO volume %u exists on %s %s\n",
				    entry.volumeId[ROVOL],
                                    noresolve ?
                                    afs_inet_ntoa_r(entry.serverNumber[j], hoststr) :
                                    hostutil_GetNameByINet(entry.
							   serverNumber[j]),
				    pname);
			    fprintf(STDERR,
				    "    VLDB reports its RO volume ID is %lu\n",
				    (unsigned long)volumeinfo->volid);
			}

			Lp_SetRWValue(&entry, entry.serverNumber[idx],
				      entry.serverPartition[idx], 0L, 0L);
			entry.nServers--;
			modified++;
			j--;
		    }
		}

		idx = entry.nServers;
		entry.nServers++;
		addvolume++;
	    } else if (volumeinfo->volid < entry.volumeId[ROVOL]) {
		/* The volume headers's RO ID does not match that in the VLDB entry,
		 * and the vol hdr's ID is lower (implies its older). So orphan it.
		 */
		if (pass == 1) {
		    MapPartIdIntoName(apart, pname);
		    fprintf(STDERR,
			    "*** Warning: Orphaned RO volume %lu exists on %s %s\n",
                            (unsigned long)volumeinfo->volid,
                            noresolve ?
                            afs_inet_ntoa_r(aserver, hoststr) :
                            hostutil_GetNameByINet(aserver), pname);
		    fprintf(STDERR,
			    "    VLDB reports its RO volume ID is %u\n",
			    entry.volumeId[ROVOL]);
		}
	    } else {
		/* The RO volume ID in the volume header match that in the VLDB entry,
		 * and there exist RO volumes in the VLDB entry. See if any of them
		 * are this one. If not, then we add it.
		 */
		idx = Lp_ROMatch(aserver, apart, &entry) - 1;
		if (idx == -1) {
		    idx = entry.nServers;
		    entry.nServers++;
		    addvolume++;
		}
	    }
	}

	if (addvolume) {
	    entry.flags |= RO_EXISTS;
	    entry.volumeId[RWVOL] = rwvolid;
	    entry.volumeId[ROVOL] = volumeinfo->volid;

	    entry.serverNumber[idx] = aserver;
	    entry.serverPartition[idx] = apart;
	    entry.serverFlags[idx] = ITSROVOL;

	    modified++;
	}
    }

    /* Remember largest volume id */
    if (entry.volumeId[ROVOL] > *maxvolid)
	*maxvolid = entry.volumeId[ROVOL];
    if (entry.volumeId[BACKVOL] > *maxvolid)
	*maxvolid = entry.volumeId[BACKVOL];
    if (entry.volumeId[RWVOL] > *maxvolid)
	*maxvolid = entry.volumeId[RWVOL];

    if (modified && doit) {
	MapNetworkToHost(&entry, &storeEntry);

	if (createentry) {
	    code = VLDB_CreateEntry(&storeEntry);
	    if (code) {
		fprintf(STDOUT,
			"Could not create a VLDB entry for the volume %lu\n",
			(unsigned long)rwvolid);
		ERROR_EXIT(code);
	    }
	} else {
	    if (pass == 1)
		goto retry;
	    code =
		VLDB_ReplaceEntry(rwvolid, RWVOL, &storeEntry,
				  LOCKREL_OPCODE | LOCKREL_AFSID |
				  LOCKREL_TIMESTAMP);
	    if (code) {
		fprintf(STDERR, "Could not update entry for %lu\n",
			(unsigned long)rwvolid);
		ERROR_EXIT(code);
	    }
	}
    } else if (pass == 2) {
	code =
	    ubik_VL_ReleaseLock(cstruct, 0, rwvolid, RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (code) {
	    PrintError("Could not unlock VLDB entry ", code);
	}
    }

    if (modified && modentry) {
	*modentry = 1;
    }

    if (aentry) {
	memcpy(aentry, &entry, sizeof(entry));
    }

    if (verbose) {
	fprintf(STDOUT, "-- status after --\n");
	if (modified)
	    EnumerateEntry(&entry);
	else
	    fprintf(STDOUT, "\n**no change**\n");
    }

  error_exit:
    VPRINT("\n_______________________________\n");
    return (error);
}

int
sortVolumes(const void *a, const void *b)
{
    volintInfo *v1 = (volintInfo *) a;
    volintInfo *v2 = (volintInfo *) b;
    afs_uint32 rwvolid1, rwvolid2;

    rwvolid1 = ((v1->type == RWVOL) ? v1->volid : v1->parentID);
    rwvolid2 = ((v2->type == RWVOL) ? v2->volid : v2->parentID);

    if (rwvolid1 > rwvolid2)
	return -1;		/* lower RW id goes first */
    if (rwvolid1 < rwvolid2)
	return 1;

    if (v1->type == RWVOL)
	return -1;		/* RW vols go first */
    if (v2->type == RWVOL)
	return 1;

    if ((v1->type == BACKVOL) && (v2->type == ROVOL))
	return -1;		/* BK vols next */
    if ((v1->type == ROVOL) && (v2->type == BACKVOL))
	return 1;

    if (v1->volid < v2->volid)
	return 1;		/* larger volids first */
    if (v1->volid > v2->volid)
	return -1;
    return 0;
}

/* UV_SyncVolume()
 *      Synchronise <aserver> <apart>(if flags = 1) <avolid>.
 *      Synchronize an individual volume against a sever and partition.
 *      Checks the VLDB entry (similar to syncserv) as well as checks
 *      if the volume exists on specified servers (similar to syncvldb).
 */
int
UV_SyncVolume(afs_uint32 aserver, afs_int32 apart, char *avolname, int flags)
{
    struct rx_connection *aconn = 0;
    afs_int32 j, k, code, vcode, error = 0;
    afs_int32 tverbose;
    afs_int32 mod, modified = 0, deleted = 0;
    struct nvldbentry vldbentry;
    afs_uint32 volumeid = 0;
    volEntries volumeInfo;
    struct partList PartList;
    afs_int32 pcnt;
    afs_uint32 maxvolid = 0;

    volumeInfo.volEntries_val = (volintInfo *) 0;
    volumeInfo.volEntries_len = 0;

    /* Turn verbose logging off and do our own verbose logging */
    /* tverbose must be set before we call ERROR_EXIT() */

    tverbose = verbose;
    if (flags & 2)
	tverbose = 1;
    verbose = 0;

    if (!aserver && (flags & 1)) {
	/* fprintf(STDERR,"Partition option requires a server option\n"); */
	ERROR_EXIT(EINVAL);
    }

    /* Read the VLDB entry */
    vcode = VLDB_GetEntryByName(avolname, &vldbentry);
    if (vcode && (vcode != VL_NOENT)) {
	fprintf(STDERR, "Could not access the VLDB for volume %s\n",
		avolname);
	ERROR_EXIT(vcode);
    } else if (!vcode) {
	MapHostToNetwork(&vldbentry);
    }

    if (tverbose) {
	fprintf(STDOUT, "Processing VLDB entry %s ...\n", avolname);
	fprintf(STDOUT, "_______________________________\n");
	fprintf(STDOUT, "\n-- status before -- \n");
	if (vcode) {
	    fprintf(STDOUT, "\n**does not exist**\n");
	} else {
	    if ((vldbentry.flags & RW_EXISTS) || (vldbentry.flags & RO_EXISTS)
		|| (vldbentry.flags & BACK_EXISTS))
		EnumerateEntry(&vldbentry);
	}
	fprintf(STDOUT, "\n");
    }

    /* Verify that all of the VLDB entries exist on the repective servers
     * and partitions (this does not require that avolname be a volume ID).
     * Equivalent to a syncserv.
     */
    if (!vcode) {
	/* Tell CheckVldb not to update if appropriate */
	if (flags & 2)
	    mod = 1;
	else
	    mod = 0;
	code = CheckVldb(&vldbentry, &mod, &deleted);
	if (code) {
	    fprintf(STDERR, "Could not process VLDB entry for volume %s\n",
		    vldbentry.name);
	    ERROR_EXIT(code);
	}
	if (mod)
	    modified++;
    }

    /* If aserver is given, we will search for the desired volume on it */
    if (aserver) {
	/* Generate array of partitions on the server that we will check */
	if (!(flags & 1)) {
	    code = UV_ListPartitions(aserver, &PartList, &pcnt);
	    if (code) {
		fprintf(STDERR,
			"Could not fetch the list of partitions from the server\n");
		ERROR_EXIT(code);
	    }
	} else {
	    PartList.partId[0] = apart;
	    pcnt = 1;
	}

	aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);

	/* If a volume ID were given, search for it on each partition */
	if ((volumeid = atol(avolname))) {
	    for (j = 0; j < pcnt; j++) {
		code =
		    AFSVolListOneVolume(aconn, PartList.partId[j], volumeid,
					&volumeInfo);
		if (code) {
		    if (code != ENODEV) {
			fprintf(STDERR, "Could not query server\n");
			ERROR_EXIT(code);
		    }
		} else {
		    if (flags & 2)
			mod = 1;
		    else
			mod = 0;
		    /* Found one, sync it with VLDB entry */
		    code =
			CheckVolume(volumeInfo.volEntries_val, aserver,
				    PartList.partId[j], &mod, &maxvolid, &vldbentry);
		    if (code)
			ERROR_EXIT(code);
		    if (mod)
			modified++;
		}

		if (volumeInfo.volEntries_val)
		    free(volumeInfo.volEntries_val);
		volumeInfo.volEntries_val = (volintInfo *) 0;
		volumeInfo.volEntries_len = 0;
	    }
	}

	/* Check to see if the RW, BK, and RO IDs exist on any
	 * partitions. We get the volume IDs from the VLDB.
	 */
	for (j = 0; j < MAXTYPES; j++) {	/* for RW, RO, and BK IDs */
	    if (vldbentry.volumeId[j] == 0)
		continue;

	    for (k = 0; k < pcnt; k++) {	/* For each partition */
		volumeInfo.volEntries_val = (volintInfo *) 0;
		volumeInfo.volEntries_len = 0;
		code =
		    AFSVolListOneVolume(aconn, PartList.partId[k],
					vldbentry.volumeId[j], &volumeInfo);
		if (code) {
		    if (code != ENODEV) {
			fprintf(STDERR, "Could not query server\n");
			ERROR_EXIT(code);
		    }
		} else {
		    if (flags & 2)
			mod = 1;
		    else
			mod = 0;
		    /* Found one, sync it with VLDB entry */
		    code =
			CheckVolume(volumeInfo.volEntries_val, aserver,
				    PartList.partId[k], &mod, &maxvolid, &vldbentry);
		    if (code)
			ERROR_EXIT(code);
		    if (mod)
			modified++;
		}

		if (volumeInfo.volEntries_val)
		    free(volumeInfo.volEntries_val);
		volumeInfo.volEntries_val = (volintInfo *) 0;
		volumeInfo.volEntries_len = 0;
	    }
	}
    }

    /* if (aserver) */
    /* If verbose output, print a summary of what changed */
    if (tverbose) {
	fprintf(STDOUT, "-- status after --\n");
	if (deleted) {
	    fprintf(STDOUT, "\n**entry deleted**\n");
	} else if (modified) {
	    EnumerateEntry(&vldbentry);
	} else {
	    fprintf(STDOUT, "\n**no change**\n");
	}
	fprintf(STDOUT, "\n_______________________________\n");
    }

  error_exit:
    /* Now check if the maxvolid is larger than that stored in the VLDB */
    if (maxvolid) {
	afs_uint32 maxvldbid = 0;
	code = ubik_VL_GetNewVolumeId(cstruct, 0, 0, &maxvldbid);
	if (code) {
	    fprintf(STDERR,
		    "Could not get the highest allocated volume id from the VLDB\n");
	    if (!error)
		error = code;
	} else if (maxvolid > maxvldbid) {
	    afs_uint32 id, nid;
	    id = maxvolid - maxvldbid + 1;
	    code = ubik_VL_GetNewVolumeId(cstruct, 0, id, &nid);
	    if (code) {
		fprintf(STDERR,
			"Error in increasing highest allocated volume id in VLDB\n");
		if (!error)
		    error = code;
	    }
	}
    }

    verbose = tverbose;
    if (verbose) {
	if (error)
	    fprintf(STDOUT, "...error encountered");
	else
	    fprintf(STDOUT, "...done entry\n");
    }
    if (aconn)
	rx_DestroyConnection(aconn);
    if (volumeInfo.volEntries_val)
	free(volumeInfo.volEntries_val);

    PrintError("", error);
    return error;
}

/* UV_SyncVldb()
 *      Synchronise vldb with the file server <aserver> and,
 *      optionally, <apart>.
 */
int
UV_SyncVldb(afs_uint32 aserver, afs_int32 apart, int flags, int force)
{
    struct rx_connection *aconn;
    afs_int32 code, error = 0;
    int i, pfail;
    unsigned int j;
    volEntries volumeInfo;
    struct partList PartList;
    afs_int32 pcnt;
    char pname[10];
    volintInfo *vi;
    afs_int32 failures = 0, modifications = 0, tentries = 0;
    afs_int32 modified;
    afs_uint32 maxvolid = 0;
    char hoststr[16];

    volumeInfo.volEntries_val = (volintInfo *) 0;
    volumeInfo.volEntries_len = 0;

    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);

    /* Generate array of partitions to check */
    if (!(flags & 1)) {
	code = UV_ListPartitions(aserver, &PartList, &pcnt);
	if (code) {
	    fprintf(STDERR,
		    "Could not fetch the list of partitions from the server\n");
	    ERROR_EXIT(code);
	}
    } else {
	PartList.partId[0] = apart;
	pcnt = 1;
    }

    VPRINT("Processing volume entries ...\n");

    /* Step through the array of partitions */
    for (i = 0; i < pcnt; i++) {
	apart = PartList.partId[i];
	MapPartIdIntoName(apart, pname);

	volumeInfo.volEntries_val = (volintInfo *) 0;
	volumeInfo.volEntries_len = 0;
	code = AFSVolListVolumes(aconn, apart, 1, &volumeInfo);
	if (code) {
	    fprintf(STDERR,
		    "Could not fetch the list of volumes from the server\n");
	    ERROR_EXIT(code);
	}

	/* May want to sort the entries: RW, BK (high to low), RO (high to low) */
	qsort((char *)volumeInfo.volEntries_val, volumeInfo.volEntries_len,
	      sizeof(volintInfo), sortVolumes);

	pfail = 0;
	for (vi = volumeInfo.volEntries_val, j = 0;
	     j < volumeInfo.volEntries_len; j++, vi++) {
	    if (!vi->status)
		continue;

	    tentries++;

	    if (verbose) {
		fprintf(STDOUT,
			"Processing volume entry %d: %s (%lu) on server %s %s...\n",
			j + 1, vi->name, (unsigned long)vi->volid,
                        noresolve ?
                        afs_inet_ntoa_r(aserver, hoststr) :
                        hostutil_GetNameByINet(aserver), pname);
		fflush(STDOUT);
	    }

	    if (flags & 2)
		modified = 1;
	    else
		modified = 0;
	    code = CheckVolume(vi, aserver, apart, &modified, &maxvolid, NULL);
	    if (code) {
		PrintError("", code);
		failures++;
		pfail++;
	    } else if (modified) {
		modifications++;
	    }

	    if (verbose) {
		if (code) {
		    fprintf(STDOUT, "...error encountered\n\n");
		} else {
		    fprintf(STDOUT, "...done entry %d\n\n", j + 1);
		}
	    }
	}

	if (pfail) {
	    fprintf(STDERR,
		    "Could not process entries on server %s partition %s\n",
                    noresolve ?
                    afs_inet_ntoa_r(aserver, hoststr) :
                    hostutil_GetNameByINet(aserver), pname);
	}
	if (volumeInfo.volEntries_val) {
	    free(volumeInfo.volEntries_val);
	    volumeInfo.volEntries_val = 0;
	}

    }				/* thru all partitions */

    if (flags & 2) {
	VPRINT3("Total entries: %u, Failed to process %d, Would change %d\n",
		tentries, failures, modifications);
    } else {
	VPRINT3("Total entries: %u, Failed to process %d, Changed %d\n",
		tentries, failures, modifications);
    }

  error_exit:
    /* Now check if the maxvolid is larger than that stored in the VLDB */
    if (maxvolid) {
	afs_uint32 maxvldbid = 0;
	code = ubik_VL_GetNewVolumeId(cstruct, 0, 0, &maxvldbid);
	if (code) {
	    fprintf(STDERR,
		    "Could not get the highest allocated volume id from the VLDB\n");
	    if (!error)
		error = code;
	} else if (maxvolid > maxvldbid) {
	    afs_uint32 id, nid;
	    id = maxvolid - maxvldbid + 1;
	    code = ubik_VL_GetNewVolumeId(cstruct, 0, id, &nid);
	    if (code) {
		fprintf(STDERR,
			"Error in increasing highest allocated volume id in VLDB\n");
		if (!error)
		    error = code;
	    }
	}
    }

    if (aconn)
	rx_DestroyConnection(aconn);
    if (volumeInfo.volEntries_val)
	free(volumeInfo.volEntries_val);
    PrintError("", error);
    return (error);
}

/* VolumeExists()
 *      Determine if a volume exists on a server and partition.
 *      Try creating a transaction on the volume. If we can,
 *      the volume exists, if not, then return the error code.
 *      Some error codes mean the volume is unavailable but
 *      still exists - so we catch these error codes.
 */
afs_int32
VolumeExists(afs_uint32 server, afs_int32 partition, afs_uint32 volumeid)
{
    struct rx_connection *conn = (struct rx_connection *)0;
    afs_int32 code = -1;
    volEntries volumeInfo;

    conn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    if (conn) {
	volumeInfo.volEntries_val = (volintInfo *) 0;
	volumeInfo.volEntries_len = 0;
	code = AFSVolListOneVolume(conn, partition, volumeid, &volumeInfo);
	if (volumeInfo.volEntries_val)
	    free(volumeInfo.volEntries_val);
	if (code == VOLSERILLEGAL_PARTITION)
	    code = ENODEV;
	rx_DestroyConnection(conn);
    }
    return code;
}

/* CheckVldbRWBK()
 *
 */
afs_int32
CheckVldbRWBK(struct nvldbentry * entry, afs_int32 * modified)
{
    int modentry = 0;
    int idx;
    afs_int32 code, error = 0;
    char pname[10];
    char hoststr[16];

    if (modified)
	*modified = 0;
    idx = Lp_GetRwIndex(entry);

    /* Check to see if the RW volume exists and set the RW_EXISTS
     * flag accordingly.
     */
    if (idx == -1) {		/* Did not find a RW entry */
	if (entry->flags & RW_EXISTS) {	/* ... yet entry says RW exists */
	    entry->flags &= ~RW_EXISTS;	/* ... so say RW does not exist */
	    modentry++;
	}
    } else {
	code =
	    VolumeExists(entry->serverNumber[idx],
			 entry->serverPartition[idx], entry->volumeId[RWVOL]);
	if (code == 0) {	/* RW volume exists */
	    if (!(entry->flags & RW_EXISTS)) {	/* ... yet entry says RW does not exist */
		entry->flags |= RW_EXISTS;	/* ... so say RW does exist */
		modentry++;
	    }
	} else if (code == ENODEV) {	/* RW volume does not exist */
	    if (entry->flags & RW_EXISTS) {	/* ... yet entry says RW exists */
		entry->flags &= ~RW_EXISTS;	/* ... so say RW does not exist */
		modentry++;
	    }
	} else {
	    /* If VLDB says it didn't exist, then ignore error */
	    if (entry->flags & RW_EXISTS) {
		MapPartIdIntoName(entry->serverPartition[idx], pname);
		fprintf(STDERR,
			"Transaction call failed for RW volume %u on server %s %s\n",
			entry->volumeId[RWVOL],
                        noresolve ?
                        afs_inet_ntoa_r(entry->serverNumber[idx], hoststr) :
                        hostutil_GetNameByINet(entry->serverNumber[idx]),
			pname);
		ERROR_EXIT(code);
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
	code =
	    VolumeExists(entry->serverNumber[idx],
			 entry->serverPartition[idx],
			 entry->volumeId[BACKVOL]);
	if (code == 0) {	/* BK volume exists */
	    if (!(entry->flags & BACK_EXISTS)) {	/* ... yet entry says BK does not exist */
		entry->flags |= BACK_EXISTS;	/* ... so say BK does exist */
		modentry++;
	    }
	} else if (code == ENODEV) {	/* BK volume does not exist */
	    if (entry->flags & BACK_EXISTS) {	/* ... yet entry says BK exists */
		entry->flags &= ~BACK_EXISTS;	/* ... so say BK does not exist */
		modentry++;
	    }
	} else {
	    /* If VLDB says it didn't exist, then ignore error */
	    if (entry->flags & BACK_EXISTS) {
		MapPartIdIntoName(entry->serverPartition[idx], pname);
		fprintf(STDERR,
			"Transaction call failed for BK volume %u on server %s %s\n",
			entry->volumeId[BACKVOL],
                        noresolve ?
                        afs_inet_ntoa_r(entry->serverNumber[idx], hoststr) :
                        hostutil_GetNameByINet(entry->serverNumber[idx]),
			pname);
		ERROR_EXIT(code);
	    }
	}
    }

    /* If there is an idx but the BK and RW volumes no
     * longer exist, then remove the RW entry.
     */
    if ((idx != -1) && !(entry->flags & RW_EXISTS)
	&& !(entry->flags & BACK_EXISTS)) {
	Lp_SetRWValue(entry, entry->serverNumber[idx],
		      entry->serverPartition[idx], 0L, 0L);
	entry->nServers--;
	modentry++;
    }

  error_exit:
    if (modified)
	*modified = modentry;
    return (error);
}

int
CheckVldbRO(struct nvldbentry *entry, afs_int32 * modified)
{
    int idx;
    int foundro = 0, modentry = 0;
    afs_int32 code, error = 0;
    char pname[10];
    char hoststr[16];

    if (modified)
	*modified = 0;

    /* Check to see if the RO volumes exist and set the RO_EXISTS
     * flag accordingly.
     */
    for (idx = 0; idx < entry->nServers; idx++) {
	if (!(entry->serverFlags[idx] & ITSROVOL)) {
	    continue;		/* not a RO */
	}

	code =
	    VolumeExists(entry->serverNumber[idx],
			 entry->serverPartition[idx], entry->volumeId[ROVOL]);
	if (code == 0) {	/* RO volume exists */
	    foundro++;
	} else if (code == ENODEV) {	/* RW volume does not exist */
	    Lp_SetROValue(entry, entry->serverNumber[idx],
			  entry->serverPartition[idx], 0L, 0L);
	    entry->nServers--;
	    idx--;
	    modentry++;
	} else {
	    MapPartIdIntoName(entry->serverPartition[idx], pname);
	    fprintf(STDERR,
		    "Transaction call failed for RO %u on server %s %s\n",
		    entry->volumeId[ROVOL],
                    noresolve ?
                    afs_inet_ntoa_r(entry->serverNumber[idx], hoststr) :
                    hostutil_GetNameByINet(entry->serverNumber[idx]), pname);
	    ERROR_EXIT(code);
	}
    }

    if (foundro) {		/* A RO volume exists */
	if (!(entry->flags & RO_EXISTS)) {	/* ... yet entry says RW does not exist */
	    entry->flags |= RO_EXISTS;	/* ... so say RW does exist */
	    modentry++;
	}
    } else {			/* A RO volume does not exist */
	if (entry->flags & RO_EXISTS) {	/* ... yet entry says RO exists */
	    entry->flags &= ~RO_EXISTS;	/* ... so say RO does not exist */
	    modentry++;
	}
    }

  error_exit:
    if (modified)
	*modified = modentry;
    return (error);
}

/* CheckVldb()
 *      Ensure that <entry> matches with the info on file servers
 */
afs_int32
CheckVldb(struct nvldbentry * entry, afs_int32 * modified, afs_int32 * deleted)
{
    afs_int32 code, error = 0;
    struct nvldbentry storeEntry;
    int islocked = 0, mod, modentry, delentry = 0;
    int pass = 0, doit=1;

    if (modified) {
	if (*modified == 1)
	    doit = 0;
	*modified = 0;
    }
    if (verbose) {
	fprintf(STDOUT, "_______________________________\n");
	fprintf(STDOUT, "\n-- status before -- \n");
	if ((entry->flags & RW_EXISTS) || (entry->flags & RO_EXISTS)
	    || (entry->flags & BACK_EXISTS))
	    EnumerateEntry(entry);
	fprintf(STDOUT, "\n");
    }

    if (strlen(entry->name) > (VOLSER_OLDMAXVOLNAME - 10)) {
	fprintf(STDERR, "Volume name %s exceeds limit of %d characters\n",
		entry->name, VOLSER_OLDMAXVOLNAME - 10);
    }

  retry:
    /* Check to see if the VLDB is ok without locking it (pass 1).
     * If it will change, then lock the VLDB entry, read it again,
     * then make the changes to it (pass 2).
     */
    if (++pass == 2) {
	code =
	    ubik_VL_SetLock(cstruct, 0, entry->volumeId[RWVOL], RWVOL,
		      VLOP_DELETE);
	if (code) {
	    fprintf(STDERR, "Could not lock VLDB entry for %u \n",
		    entry->volumeId[RWVOL]);
	    ERROR_EXIT(code);
	}
	islocked = 1;

	code = VLDB_GetEntryByID(entry->volumeId[RWVOL], RWVOL, entry);
	if (code) {
	    fprintf(STDERR, "Could not read VLDB entry for volume %s\n",
		    entry->name);
	    ERROR_EXIT(code);
	} else {
	    MapHostToNetwork(entry);
	}
    }

    modentry = 0;

    /* Check if the RW and BK entries are ok */
    code = CheckVldbRWBK(entry, &mod);
    if (code)
	ERROR_EXIT(code);
    if (mod && (pass == 1) && doit)
	goto retry;
    if (mod)
	modentry++;

    /* Check if the RO volumes entries are ok */
    code = CheckVldbRO(entry, &mod);
    if (code)
	ERROR_EXIT(code);
    if (mod && (pass == 1) && doit)
	goto retry;
    if (mod)
	modentry++;

    /* The VLDB entry has been updated. If it as been modified, then
     * write the entry back out the the VLDB.
     */
    if (modentry && doit) {
	if (pass == 1)
	    goto retry;

	if (!(entry->flags & RW_EXISTS) && !(entry->flags & BACK_EXISTS)
	    && !(entry->flags & RO_EXISTS) && doit) {
	    /* The RW, BK, nor RO volumes do not exist. Delete the VLDB entry */
	    code =
		ubik_VL_DeleteEntry(cstruct, 0, entry->volumeId[RWVOL],
			  RWVOL);
	    if (code) {
		fprintf(STDERR,
			"Could not delete VLDB entry for volume %u \n",
			entry->volumeId[RWVOL]);
		ERROR_EXIT(code);
	    }
	    delentry = 1;
	} else {
	    /* Replace old entry with our new one */
	    MapNetworkToHost(entry, &storeEntry);
	    code =
		VLDB_ReplaceEntry(entry->volumeId[RWVOL], RWVOL, &storeEntry,
				  (LOCKREL_OPCODE | LOCKREL_AFSID |
				   LOCKREL_TIMESTAMP));
	    if (code) {
		fprintf(STDERR, "Could not update VLDB entry for volume %u\n",
			entry->volumeId[RWVOL]);
		ERROR_EXIT(code);
	    }
	}
	islocked = 0;
    }

    if (modified && modentry) {
	*modified = 1;
    }
    if (deleted && delentry) {
	*deleted = 1;
    }

    if (verbose) {
	fprintf(STDOUT, "-- status after --\n");
	if (delentry)
	    fprintf(STDOUT, "\n**entry deleted**\n");
	else if (modentry)
	    EnumerateEntry(entry);
	else
	    fprintf(STDOUT, "\n**no change**\n");
    }

  error_exit:
    VPRINT("\n_______________________________\n");

    if (islocked) {
	code =
	    ubik_VL_ReleaseLock(cstruct, 0, entry->volumeId[RWVOL],
		      RWVOL,
		      (LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP));
	if (code) {
	    fprintf(STDERR,
		    "Could not release lock on VLDB entry for volume %u\n",
		    entry->volumeId[RWVOL]);
	    if (!error)
		error = code;
	}
    }
    return error;
}

/* UV_SyncServer()
 *      Synchronise <aserver> <apart>(if flags = 1) with the VLDB.
 */
int
UV_SyncServer(afs_uint32 aserver, afs_int32 apart, int flags, int force)
{
    struct rx_connection *aconn;
    afs_int32 code, error = 0;
    afs_int32 nentries, tentries = 0;
    struct VldbListByAttributes attributes;
    nbulkentries arrayEntries;
    afs_int32 failures = 0, modified, modifications = 0;
    struct nvldbentry *vlentry;
    afs_int32 si, nsi, j;

    if (flags & 2)
	verbose = 1;

    aconn = UV_Bind(aserver, AFSCONF_VOLUMEPORT);

    /* Set up attributes to search VLDB  */
    memset(&attributes, 0, sizeof(attributes));
    attributes.server = ntohl(aserver);
    attributes.Mask = VLLIST_SERVER;
    if ((flags & 1)) {
	attributes.partition = apart;
	attributes.Mask |= VLLIST_PARTITION;
    }

    VPRINT("Processing VLDB entries ...\n");

    /* While we need to collect more VLDB entries */
    for (si = 0; si != -1; si = nsi) {
	memset(&arrayEntries, 0, sizeof(arrayEntries));

	/* Collect set of VLDB entries */
	code =
	    VLDB_ListAttributesN2(&attributes, 0, si, &nentries,
				  &arrayEntries, &nsi);
	if (code == RXGEN_OPCODE) {
	    code = VLDB_ListAttributes(&attributes, &nentries, &arrayEntries);
	    nsi = -1;
	}
	if (code) {
	    fprintf(STDERR, "Could not access the VLDB for attributes\n");
	    ERROR_EXIT(code);
	}
	tentries += nentries;

	for (j = 0; j < nentries; j++) {
	    vlentry = &arrayEntries.nbulkentries_val[j];
	    MapHostToNetwork(vlentry);

	    VPRINT1("Processing VLDB entry %d ...\n", j + 1);

	    /* Tell CheckVldb not to update if appropriate */
	    if (flags & 2)
		modified = 1;
	    else
		modified = 0;
	    code = CheckVldb(vlentry, &modified, NULL);
	    if (code) {
		PrintError("", code);
		fprintf(STDERR,
			"Could not process VLDB entry for volume %s\n",
			vlentry->name);
		failures++;
	    } else if (modified) {
		modifications++;
	    }

	    if (verbose) {
		if (code) {
		    fprintf(STDOUT, "...error encountered\n\n");
		} else {
		    fprintf(STDOUT, "...done entry %d\n\n", j + 1);
		}
	    }
	}

	if (arrayEntries.nbulkentries_val) {
	    free(arrayEntries.nbulkentries_val);
	    arrayEntries.nbulkentries_val = 0;
	}
    }

    if (flags & 2) {
	VPRINT3("Total entries: %u, Failed to process %d, Would change %d\n",
		tentries, failures, modifications);
    } else {
	VPRINT3("Total entries: %u, Failed to process %d, Changed %d\n",
		tentries, failures, modifications);
    }

  error_exit:
    if (aconn)
	rx_DestroyConnection(aconn);
    if (arrayEntries.nbulkentries_val)
	free(arrayEntries.nbulkentries_val);

    if (failures)
	error = VOLSERFAILEDOP;
    return error;
}

/*rename volume <oldname> to <newname>, changing the names of the related
 *readonly and backup volumes. This operation is also idempotent.
 *salvager is capable of recovering from rename operation stopping halfway.
 *to recover run syncserver on the affected machines,it will force renaming to completion. name clashes should have been detected before calling this proc */
int
UV_RenameVolume(struct nvldbentry *entry, char oldname[], char newname[])
{
    struct nvldbentry storeEntry;
    afs_int32 vcode, code, rcode, error;
    int i, index;
    char nameBuffer[256];
    afs_int32 tid;
    struct rx_connection *aconn;
    int islocked;
    char hoststr[16];

    error = 0;
    aconn = (struct rx_connection *)0;
    tid = 0;
    islocked = 0;

    vcode = ubik_VL_SetLock(cstruct, 0, entry->volumeId[RWVOL], RWVOL, VLOP_ADDSITE);	/*last param is dummy */
    if (vcode) {
	fprintf(STDERR,
		" Could not lock the VLDB entry for the  volume %u \n",
		entry->volumeId[RWVOL]);
	error = vcode;
	goto rvfail;
    }
    islocked = 1;
    strncpy(entry->name, newname, VOLSER_OLDMAXVOLNAME);
    MapNetworkToHost(entry, &storeEntry);
    vcode = VLDB_ReplaceEntry(entry->volumeId[RWVOL], RWVOL, &storeEntry, 0);
    if (vcode) {
	fprintf(STDERR, "Could not update VLDB entry for %u\n",
		entry->volumeId[RWVOL]);
	error = vcode;
	goto rvfail;
    }
    VPRINT1("Recorded the new name %s in VLDB\n", newname);
    /*at this stage the intent to rename is recorded in the vldb, as far as the vldb
     * is concerned, oldname is lost */
    if (entry->flags & RW_EXISTS) {
	index = Lp_GetRwIndex(entry);
	if (index == -1) {	/* there is a serious discrepancy */
	    fprintf(STDERR,
		    "There is a serious discrepancy in VLDB entry for volume %u\n",
		    entry->volumeId[RWVOL]);
	    fprintf(STDERR, "try building VLDB from scratch\n");
	    error = VOLSERVLDB_ERROR;
	    goto rvfail;
	}
	aconn = UV_Bind(entry->serverNumber[index], AFSCONF_VOLUMEPORT);
	code =
	    AFSVolTransCreate_retry(aconn, entry->volumeId[RWVOL],
			      entry->serverPartition[index], ITOffline, &tid);
	if (code) {		/*volume doesnot exist */
	    fprintf(STDERR,
		    "Could not start transaction on the rw volume %u\n",
		    entry->volumeId[RWVOL]);
	    error = code;
	    goto rvfail;
	} else {		/*volume exists, process it */

	    code =
		AFSVolSetIdsTypes(aconn, tid, newname, RWVOL,
				  entry->volumeId[RWVOL],
				  entry->volumeId[ROVOL],
				  entry->volumeId[BACKVOL]);
	    if (!code) {
		VPRINT2("Renamed rw volume %s to %s\n", oldname, newname);
		code = AFSVolEndTrans(aconn, tid, &rcode);
		tid = 0;
		if (code) {
		    fprintf(STDERR,
			    "Could not  end transaction on volume %s %u\n",
			    entry->name, entry->volumeId[RWVOL]);
		    error = code;
		    goto rvfail;
		}
	    } else {
		fprintf(STDERR, "Could not  set parameters on volume %s %u\n",
			entry->name, entry->volumeId[RWVOL]);
		error = code;
		goto rvfail;
	    }
	}
	if (aconn)
	    rx_DestroyConnection(aconn);
	aconn = (struct rx_connection *)0;
    }
    /*end rw volume processing */
    if (entry->flags & BACK_EXISTS) {	/*process the backup volume */
	index = Lp_GetRwIndex(entry);
	if (index == -1) {	/* there is a serious discrepancy */
	    fprintf(STDERR,
		    "There is a serious discrepancy in the VLDB entry for the backup volume %u\n",
		    entry->volumeId[BACKVOL]);
	    fprintf(STDERR, "try building VLDB from scratch\n");
	    error = VOLSERVLDB_ERROR;
	    goto rvfail;
	}
	aconn = UV_Bind(entry->serverNumber[index], AFSCONF_VOLUMEPORT);
	code =
	    AFSVolTransCreate_retry(aconn, entry->volumeId[BACKVOL],
			      entry->serverPartition[index], ITOffline, &tid);
	if (code) {		/*volume doesnot exist */
	    fprintf(STDERR,
		    "Could not start transaction on the backup volume  %u\n",
		    entry->volumeId[BACKVOL]);
	    error = code;
	    goto rvfail;
	} else {		/*volume exists, process it */
	    if (strlen(newname) > (VOLSER_OLDMAXVOLNAME - 8)) {
		fprintf(STDERR,
			"Volume name %s.backup exceeds the limit of %u characters\n",
			newname, VOLSER_OLDMAXVOLNAME);
		error = code;
		goto rvfail;
	    }
	    strcpy(nameBuffer, newname);
	    strcat(nameBuffer, ".backup");

	    code =
		AFSVolSetIdsTypes(aconn, tid, nameBuffer, BACKVOL,
				  entry->volumeId[RWVOL], 0, 0);
	    if (!code) {
		VPRINT1("Renamed backup volume to %s \n", nameBuffer);
		code = AFSVolEndTrans(aconn, tid, &rcode);
		tid = 0;
		if (code) {
		    fprintf(STDERR,
			    "Could not  end transaction on the backup volume %u\n",
			    entry->volumeId[BACKVOL]);
		    error = code;
		    goto rvfail;
		}
	    } else {
		fprintf(STDERR,
			"Could not  set parameters on the backup volume %u\n",
			entry->volumeId[BACKVOL]);
		error = code;
		goto rvfail;
	    }
	}
    }				/* end backup processing */
    if (aconn)
	rx_DestroyConnection(aconn);
    aconn = (struct rx_connection *)0;
    if (entry->flags & RO_EXISTS) {	/*process the ro volumes */
	for (i = 0; i < entry->nServers; i++) {
	    if (entry->serverFlags[i] & ITSROVOL) {
		aconn = UV_Bind(entry->serverNumber[i], AFSCONF_VOLUMEPORT);
		code =
		    AFSVolTransCreate_retry(aconn, entry->volumeId[ROVOL],
				      entry->serverPartition[i], ITOffline,
				      &tid);
		if (code) {	/*volume doesnot exist */
		    fprintf(STDERR,
			    "Could not start transaction on the ro volume %u\n",
			    entry->volumeId[ROVOL]);
		    error = code;
		    goto rvfail;
		} else {	/*volume exists, process it */
		    strcpy(nameBuffer, newname);
		    strcat(nameBuffer, ".readonly");
		    if (strlen(nameBuffer) > (VOLSER_OLDMAXVOLNAME - 1)) {
			fprintf(STDERR,
				"Volume name %s exceeds the limit of %u characters\n",
				nameBuffer, VOLSER_OLDMAXVOLNAME);
			error = code;
			goto rvfail;
		    }
		    code =
			AFSVolSetIdsTypes(aconn, tid, nameBuffer, ROVOL,
					  entry->volumeId[RWVOL], 0, 0);
		    if (!code) {
			VPRINT2("Renamed RO volume %s on host %s\n",
				nameBuffer,
                                noresolve ?
                                afs_inet_ntoa_r(entry->serverNumber[i], hoststr) :
                                hostutil_GetNameByINet(entry->
						       serverNumber[i]));
			code = AFSVolEndTrans(aconn, tid, &rcode);
			tid = 0;
			if (code) {
			    fprintf(STDERR,
				    "Could not  end transaction on volume %u\n",
				    entry->volumeId[ROVOL]);
			    error = code;
			    goto rvfail;
			}
		    } else {
			fprintf(STDERR,
				"Could not  set parameters on the ro volume %u\n",
				entry->volumeId[ROVOL]);
			error = code;
			goto rvfail;
		    }
		}
		if (aconn)
		    rx_DestroyConnection(aconn);
		aconn = (struct rx_connection *)0;
	    }
	}
    }
  rvfail:
    if (islocked) {
	vcode =
	    ubik_VL_ReleaseLock(cstruct, 0, entry->volumeId[RWVOL],
		      RWVOL,
		      LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
	if (vcode) {
	    fprintf(STDERR,
		    "Could not unlock the VLDB entry for the volume %s %u\n",
		    entry->name, entry->volumeId[RWVOL]);
	    if (!error)
		error = vcode;
	}
    }
    if (tid) {
	code = AFSVolEndTrans(aconn, tid, &rcode);
	if (!code)
	    code = rcode;
	if (code) {
	    fprintf(STDERR, "Failed to end transaction on a volume \n");
	    if (!error)
		error = code;
	}
    }
    if (aconn)
	rx_DestroyConnection(aconn);
    PrintError("", error);
    return error;

}

/*report on all the active transactions on volser */
int
UV_VolserStatus(afs_uint32 server, transDebugInfo ** rpntr, afs_int32 * rcount)
{
    struct rx_connection *aconn;
    transDebugEntries transInfo;
    afs_int32 code = 0;

    aconn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    transInfo.transDebugEntries_val = (transDebugInfo *) 0;
    transInfo.transDebugEntries_len = 0;
    code = AFSVolMonitor(aconn, &transInfo);
    if (code) {
	fprintf(STDERR,
		"Could not access status information about the server\n");
	PrintError("", code);
	if (transInfo.transDebugEntries_val)
	    free(transInfo.transDebugEntries_val);
	if (aconn)
	    rx_DestroyConnection(aconn);
	return code;
    } else {
	*rcount = transInfo.transDebugEntries_len;
	*rpntr = transInfo.transDebugEntries_val;
	if (aconn)
	    rx_DestroyConnection(aconn);
	return 0;
    }


}

/*delete the volume without interacting with the vldb */
int
UV_VolumeZap(afs_uint32 server, afs_int32 part, afs_uint32 volid)
{
    afs_int32 error;
    struct rx_connection *aconn;

    aconn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    error = DoVolDelete(aconn, volid, part,
			"the", 0, NULL, NULL);
    if (error == VNOVOL) {
	EPRINT1(error, "Failed to start transaction on %u\n", volid);
    }

    PrintError("", error);
    if (aconn)
	rx_DestroyConnection(aconn);
    return error;
}

int
UV_SetVolume(afs_uint32 server, afs_int32 partition, afs_uint32 volid,
	     afs_int32 transflag, afs_int32 setflag, int sleeptime)
{
    struct rx_connection *conn = 0;
    afs_int32 tid = 0;
    afs_int32 code, error = 0, rcode;

    conn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    if (!conn) {
	fprintf(STDERR, "SetVolumeStatus: Bind Failed");
	ERROR_EXIT(-1);
    }

    code = AFSVolTransCreate_retry(conn, volid, partition, transflag, &tid);
    if (code) {
	fprintf(STDERR, "SetVolumeStatus: TransCreate Failed\n");
	ERROR_EXIT(code);
    }

    code = AFSVolSetFlags(conn, tid, setflag);
    if (code) {
	fprintf(STDERR, "SetVolumeStatus: SetFlags Failed\n");
	ERROR_EXIT(code);
    }

    if (sleeptime) {
#ifdef AFS_PTHREAD_ENV
	sleep(sleeptime);
#else
	IOMGR_Sleep(sleeptime);
#endif
    }

  error_exit:
    if (tid) {
	rcode = 0;
	code = AFSVolEndTrans(conn, tid, &rcode);
	if (code || rcode) {
	    fprintf(STDERR, "SetVolumeStatus: EndTrans Failed\n");
	    if (!error)
		error = (code ? code : rcode);
	}
    }

    if (conn)
	rx_DestroyConnection(conn);
    return (error);
}

int
UV_SetVolumeInfo(afs_uint32 server, afs_int32 partition, afs_uint32 volid,
		 volintInfo * infop)
{
    struct rx_connection *conn = 0;
    afs_int32 tid = 0;
    afs_int32 code, error = 0, rcode;

    conn = UV_Bind(server, AFSCONF_VOLUMEPORT);
    if (!conn) {
	fprintf(STDERR, "SetVolumeInfo: Bind Failed");
	ERROR_EXIT(-1);
    }

    code = AFSVolTransCreate_retry(conn, volid, partition, ITOffline, &tid);
    if (code) {
	fprintf(STDERR, "SetVolumeInfo: TransCreate Failed\n");
	ERROR_EXIT(code);
    }

    code = AFSVolSetInfo(conn, tid, infop);
    if (code) {
	fprintf(STDERR, "SetVolumeInfo: SetInfo Failed\n");
	ERROR_EXIT(code);
    }

  error_exit:
    if (tid) {
	rcode = 0;
	code = AFSVolEndTrans(conn, tid, &rcode);
	if (code || rcode) {
	    fprintf(STDERR, "SetVolumeInfo: EndTrans Failed\n");
	    if (!error)
		error = (code ? code : rcode);
	}
    }

    if (conn)
	rx_DestroyConnection(conn);
    return (error);
}

int
UV_GetSize(afs_uint32 afromvol, afs_uint32 afromserver, afs_int32 afrompart,
	   afs_int32 fromdate, struct volintSize *vol_size)
{
    struct rx_connection *aconn = (struct rx_connection *)0;
    afs_int32 tid = 0, rcode = 0;
    afs_int32 code, error = 0;


    /* get connections to the servers */
    aconn = UV_Bind(afromserver, AFSCONF_VOLUMEPORT);

    VPRINT1("Starting transaction on volume %u...", afromvol);
    code = AFSVolTransCreate_retry(aconn, afromvol, afrompart, ITBusy, &tid);
    EGOTO1(error_exit, code,
	   "Could not start transaction on the volume %u to be measured\n",
	   afromvol);
    VDONE;

    VPRINT1("Getting size of volume on volume %u...", afromvol);
    code = AFSVolGetSize(aconn, tid, fromdate, vol_size);
    EGOTO(error_exit, code, "Could not start the measurement process \n");
    VDONE;

  error_exit:
    if (tid) {
	VPRINT1("Ending transaction on volume %u...", afromvol);
	code = AFSVolEndTrans(aconn, tid, &rcode);
	if (code || rcode) {
	    fprintf(STDERR, "Could not end transaction on the volume %u\n",
		    afromvol);
	    fprintf(STDERR, "error codes: %d and %d\n", code, rcode);
	    if (!error)
		error = (code ? code : rcode);
	}
	VDONE;
    }
    if (aconn)
	rx_DestroyConnection(aconn);

    PrintError("", error);
    return (error);
}

/*maps the host addresses in <old > (present in network byte order) to
 that in< new> (present in host byte order )*/
void
MapNetworkToHost(struct nvldbentry *old, struct nvldbentry *new)
{
    int i, count;

    memset(new, 0, sizeof(struct nvldbentry));

    /*copy all the fields */
    strcpy(new->name, old->name);
/*    new->volumeType = old->volumeType;*/
    new->nServers = old->nServers;
    count = old->nServers;
    if (count < NMAXNSERVERS)
	count++;
    for (i = 0; i < count; i++) {
	new->serverNumber[i] = ntohl(old->serverNumber[i]);
	new->serverPartition[i] = old->serverPartition[i];
	new->serverFlags[i] = old->serverFlags[i];
    }
    new->volumeId[RWVOL] = old->volumeId[RWVOL];
    new->volumeId[ROVOL] = old->volumeId[ROVOL];
    new->volumeId[BACKVOL] = old->volumeId[BACKVOL];
    new->cloneId = old->cloneId;
    new->flags = old->flags;
}

/*maps the host entries in <entry> which are present in host byte order to network byte order */
void
MapHostToNetwork(struct nvldbentry *entry)
{
    int i, count;

    count = entry->nServers;
    if (count < NMAXNSERVERS)
	count++;
    for (i = 0; i < count; i++) {
	entry->serverNumber[i] = htonl(entry->serverNumber[i]);
    }
}
