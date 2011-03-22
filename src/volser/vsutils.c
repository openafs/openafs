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


#include <afs/stds.h>
#include <string.h>
#include <ctype.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#endif /* AFS_NT40_ENV */
#include <sys/stat.h>
#ifdef AFS_AIX_ENV
#include <sys/statfs.h>
#endif

#include <errno.h>
#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <afs/nfs.h>
#include <afs/vlserver.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/afsint.h>
#include <afs/cmd.h>
#include <rx/rxkad.h>
#include "volser.h"
#include "volint.h"
#include "lockdata.h"

#include "vsutils_prototypes.h"

struct ubik_client *cstruct;
static rxkad_level vsu_rxkad_level = rxkad_clear;

static void
ovlentry_to_nvlentry(struct vldbentry *oentryp,
                     struct nvldbentry *nentryp)
{
    int i;

    memset(nentryp, 0, sizeof(struct nvldbentry));
    strncpy(nentryp->name, oentryp->name, sizeof(nentryp->name));
    for (i = 0; i < oentryp->nServers; i++) {
	nentryp->serverNumber[i] = oentryp->serverNumber[i];
	nentryp->serverPartition[i] = oentryp->serverPartition[i];
	nentryp->serverFlags[i] = oentryp->serverFlags[i];
    }
    nentryp->nServers = oentryp->nServers;
    for (i = 0; i < MAXTYPES; i++)
	nentryp->volumeId[i] = oentryp->volumeId[i];
    nentryp->cloneId = oentryp->cloneId;
    nentryp->flags = oentryp->flags;
}

static int
nvlentry_to_ovlentry(struct nvldbentry *nentryp,
                     struct vldbentry *oentryp)
{
    int i;

    memset(oentryp, 0, sizeof(struct vldbentry));
    strncpy(oentryp->name, nentryp->name, sizeof(oentryp->name));
    if (nentryp->nServers > OMAXNSERVERS) {
	/*
	 * The alternative is to store OMAXSERVERS but it's always better
	 * to know what's going on...
	 */
	return VL_BADSERVER;
    }
    for (i = 0; i < nentryp->nServers; i++) {
	oentryp->serverNumber[i] = nentryp->serverNumber[i];
	oentryp->serverPartition[i] = nentryp->serverPartition[i];
	oentryp->serverFlags[i] = nentryp->serverFlags[i];
    }
    oentryp->nServers = i;
    for (i = 0; i < MAXTYPES; i++)
	oentryp->volumeId[i] = nentryp->volumeId[i];
    oentryp->cloneId = nentryp->cloneId;
    oentryp->flags = nentryp->flags;
    return 0;
}

enum _vlserver_type {
    vltype_unknown = 0,
    vltype_old = 1,
    vltype_new = 2,
    vltype_uuid = 3
};

static enum _vlserver_type newvlserver = vltype_unknown;

int
VLDB_CreateEntry(struct nvldbentry *entryp)
{
    struct vldbentry oentry;
    int code;

    if (newvlserver == vltype_old) {
      tryold:
	code = nvlentry_to_ovlentry(entryp, &oentry);
	if (code)
	    return code;
	code = ubik_VL_CreateEntry(cstruct, 0, &oentry);
	return code;
    }
    code = ubik_VL_CreateEntryN(cstruct, 0, entryp);
    if (newvlserver == vltype_unknown) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = vltype_old;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = vltype_new;
	}
    }
    return code;
}

int
VLDB_GetEntryByID(afs_uint32 volid, afs_int32 voltype, struct nvldbentry *entryp)
{
    struct vldbentry oentry;
    int code;

    if (newvlserver == vltype_old) {
      tryold:
	code =
	    ubik_VL_GetEntryByID(cstruct, 0, volid, voltype, &oentry);
	if (!code)
	    ovlentry_to_nvlentry(&oentry, entryp);
	return code;
    }
    code = ubik_VL_GetEntryByIDN(cstruct, 0, volid, voltype, entryp);
    if (newvlserver == vltype_unknown) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = vltype_old;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = vltype_new;
	}
    }
    return code;
}

int
VLDB_GetEntryByName(char *namep, struct nvldbentry *entryp)
{
    struct vldbentry oentry;
    int code;

    if (newvlserver == vltype_old) {
      tryold:
	code = ubik_VL_GetEntryByNameO(cstruct, 0, namep, &oentry);
	if (!code)
	    ovlentry_to_nvlentry(&oentry, entryp);
	return code;
    }
    code = ubik_VL_GetEntryByNameN(cstruct, 0, namep, entryp);
    if (newvlserver == vltype_unknown) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = vltype_old;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = vltype_new;
	}
    }
    return code;
}

int
VLDB_ReplaceEntry(afs_uint32 volid, afs_int32 voltype, struct nvldbentry *entryp, afs_int32 releasetype)
{
    struct vldbentry oentry;
    int code;

    if (newvlserver == vltype_old) {
      tryold:
	code = nvlentry_to_ovlentry(entryp, &oentry);
	if (code)
	    return code;
	code =
	    ubik_VL_ReplaceEntry(cstruct, 0, volid, voltype, &oentry,
		      releasetype);
	return code;
    }
    code =
	ubik_VL_ReplaceEntryN(cstruct, 0, volid, voltype, entryp,
		  releasetype);
    if (newvlserver == vltype_unknown) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = vltype_old;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = vltype_new;
	}
    }
    return code;
}

static void
convertBulkToNBulk(bulkentries *bulk, nbulkentries *nbulk) {
    int i;

    if (bulk->bulkentries_len == 0)
	return;

    nbulk->nbulkentries_len = bulk->bulkentries_len;
    nbulk->nbulkentries_val =
	xdr_alloc(bulk->bulkentries_len * sizeof(struct nvldbentry));

    for (i = 0; i < bulk->bulkentries_len; i++) {
	ovlentry_to_nvlentry(&bulk->bulkentries_val[i],
			     &nbulk->nbulkentries_val[i]);
    }
}

int
VLDB_ListAttributes(VldbListByAttributes *attrp,
                    afs_int32 *entriesp,
                    nbulkentries *blkentriesp)
{
    bulkentries arrayEntries;
    int code;

    if (newvlserver == vltype_old) {
      tryold:
	memset(&arrayEntries, 0, sizeof(arrayEntries));
	code =
	    ubik_VL_ListAttributes(cstruct, 0, attrp, entriesp,
		      &arrayEntries);

	if (code)
	    return code;

	/* Ensure the number of entries claimed matches the no. returned */
	if (*entriesp < 0)
	    *entriesp = 0;
	if (*entriesp > arrayEntries.bulkentries_len)
	    *entriesp = arrayEntries.bulkentries_len;

	convertBulkToNBulk(&arrayEntries, blkentriesp);

	xdr_free((xdrproc_t) xdr_bulkentries, &arrayEntries);
	return code;
    }
    code =
        ubik_VL_ListAttributesN(cstruct, 0, attrp, entriesp, blkentriesp);
    if (newvlserver == vltype_unknown) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = vltype_old;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = vltype_new;
	}
    }

    if (code)
	return code;

    /* Ensure the number of entries claimed matches the no. returned */
    if (*entriesp < 0)
	*entriesp = 0;
    if (*entriesp > blkentriesp->nbulkentries_len)
	*entriesp = blkentriesp->nbulkentries_len;

    return code;
}

int
VLDB_ListAttributesN2(VldbListByAttributes *attrp,
                      char *name,
                      afs_int32 thisindex,
                      afs_int32 *nentriesp,
                      nbulkentries *blkentriesp,
                      afs_int32 *nextindexp)
{
    afs_int32 code = RXGEN_OPCODE;

    if (newvlserver != vltype_old) {
        code =
            ubik_VL_ListAttributesN2(cstruct, 0, attrp, (name ? name : ""),
                                     thisindex, nentriesp, blkentriesp, nextindexp);
	if (code)
	    return code;

	/* Ensure the number of entries claimed matches the no. returned */
	if (*nentriesp < 0)
	    *nentriesp = 0;
	if (*nentriesp > blkentriesp->nbulkentries_len)
	    *nentriesp = blkentriesp->nbulkentries_len;
    }
    return code;
}

struct cacheips {
    afs_uint32 server;
    afs_int32 count;
    afs_uint32 addrs[16];
};
/*
 * Increase cache size.  This avoids high CPU usage by the vlserver
 * in environments where there are more than 16 fileservers in the
 * cell.
 */
#define GETADDRUCACHESIZE             64
struct cacheips cacheips[GETADDRUCACHESIZE];
int cacheip_index = 0;

int
VLDB_IsSameAddrs(afs_uint32 serv1, afs_uint32 serv2, afs_int32 *errorp)
{
    int code;
    ListAddrByAttributes attrs;
    bulkaddrs addrs;
    afs_uint32 *addrp, i, j, f1, f2;
    afs_int32 unique, nentries;
    afsUUID uuid;
    static int initcache = 0;

    *errorp = 0;

    if (serv1 == serv2)
	return 1;
    if (newvlserver == vltype_old || newvlserver == vltype_new) {
	return 0;
    }
    if (!initcache) {
	for (i = 0; i < GETADDRUCACHESIZE; i++) {
	    cacheips[i].server = cacheips[i].count = 0;
	}
	initcache = 1;
    }

    /* See if it's cached */
    for (i = 0; i < GETADDRUCACHESIZE; i++) {
	f1 = f2 = 0;
	for (j = 0; j < cacheips[i].count; j++) {
	    if (serv1 == cacheips[i].addrs[j])
		f1 = 1;
	    else if (serv2 == cacheips[i].addrs[j])
		f2 = 1;

	    if (f1 && f2)
		return 1;
	}
	if (f1 || f2)
	    return 0;
	if (cacheips[i].server == serv1)
	    return 0;
    }

    memset(&attrs, 0, sizeof(attrs));
    attrs.Mask = VLADDR_IPADDR;
    attrs.ipaddr = serv1;
    memset(&addrs, 0, sizeof(addrs));
    memset(&uuid, 0, sizeof(uuid));
    code =
	ubik_VL_GetAddrsU(cstruct, 0, &attrs, &uuid, &unique, &nentries,
		  &addrs);
    if (newvlserver == vltype_unknown) {
	if (code == RXGEN_OPCODE) {
	    return 0;
	} else if (!code) {
	    newvlserver = vltype_uuid;
	}
    }
    if (code == VL_NOENT)
	return 0;
    if (code) {
	*errorp = code;
	return 0;
    }

    code = 0;
    if (nentries > GETADDRUCACHESIZE)
	nentries = GETADDRUCACHESIZE;	/* safety check; should not happen */
    if (++cacheip_index >= GETADDRUCACHESIZE)
	cacheip_index = 0;
    cacheips[cacheip_index].server = serv1;
    cacheips[cacheip_index].count = nentries;
    addrp = addrs.bulkaddrs_val;
    for (i = 0; i < nentries; i++, addrp++) {
	cacheips[cacheip_index].addrs[i] = *addrp;
	if (serv2 == *addrp) {
	    code = 1;
	}
    }
    return code;
}


/*
  Set encryption.  If 'cryptflag' is nonzero, encrpytion is turned on
  for authenticated connections; if zero, encryption is turned off.
  Calling this function always results in a level of at least rxkad_auth;
  to get a rxkad_clear connection, simply don't call this.
*/
void
vsu_SetCrypt(int cryptflag)
{
    if (cryptflag) {
	vsu_rxkad_level = rxkad_crypt;
    } else {
	vsu_rxkad_level = rxkad_auth;
    }
}


/*
  Get the appropriate type of ubik client structure out from the system.
*/
afs_int32
vsu_ClientInit(int noAuthFlag, const char *confDir, char *cellName, afs_int32 sauth,
               struct ubik_client **uclientp,
	       int (*secproc)(struct rx_securityClass *, afs_int32))
{
    return ugen_ClientInit(noAuthFlag, confDir, cellName, sauth, uclientp,
			   secproc, "vsu_ClientInit", vsu_rxkad_level,
			   VLDB_MAXSERVERS, AFSCONF_VLDBSERVICE, 90,
			   0, 0, USER_SERVICE_ID);
}


/*extract the name of volume <name> without readonly or backup suffixes
 * and return the result as <rname>.
 */
int
vsu_ExtractName(char rname[], char name[])
{
    char sname[VOLSER_OLDMAXVOLNAME + 1];
    int total;

    strncpy(sname, name, sizeof(sname));
    sname[sizeof(sname) - 1] = '\0';
    total = strlen(sname);
    if (!strcmp(&sname[total - 9], ".readonly")) {
	/*discard the last 8 chars */
	sname[total - 9] = '\0';
	strcpy(rname, sname);
	return 0;
    } else if (!strcmp(&sname[total - 7], ".backup")) {
	/*discard last 6 chars */
	sname[total - 7] = '\0';
	strcpy(rname, sname);
	return 0;
    } else {
	strncpy(rname, name, VOLSER_OLDMAXVOLNAME);
	rname[VOLSER_OLDMAXVOLNAME] = '\0';
	return -1;
    }
}

/* returns 0 if failed */
afs_uint32
vsu_GetVolumeID(char *astring, struct ubik_client *acstruct, afs_int32 *errp)
{
    char volname[VOLSER_OLDMAXVOLNAME + 1];
    struct nvldbentry entry;
    afs_int32 vcode = 0;
    int total;

    *errp = 0;

    if (isdigit(astring[0])) {
	char *end;
	afs_uint32 result;
	result = strtoul(astring, &end, 10);
	if (result != UINT_MAX && *end == '\0')
	    return result;
    }

    /* It was not a volume number but something else */
    total = strlen(astring);
    vsu_ExtractName(volname, astring);
    vcode = VLDB_GetEntryByName(volname, &entry);
    if (!vcode) {
      if ((total >= 9) && (!strcmp(&astring[total - 9], ".readonly")))
	return entry.volumeId[ROVOL];
      else if ((total >= 7) && (!strcmp(&astring[total - 7], ".backup")))
	return entry.volumeId[BACKVOL];
      else
	return (entry.volumeId[RWVOL]);
    }
    *errp = vcode;
    return 0;		/* can't find volume */
}
