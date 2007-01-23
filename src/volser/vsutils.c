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

#include <afs/stds.h>
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

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <errno.h>
#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <afs/nfs.h>
#include <afs/vlserver.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/afsint.h>
#include <afs/cmd.h>
#include <rx/rxkad.h>
#ifdef AFS_RXK5
#include "rxk5_utilafs.h"
#endif
#include "volser.h"
#include "volint.h"
#include "lockdata.h"

struct ubik_client *cstruct;
static rxkad_level vsu_rxkad_level = rxkad_clear;

#ifdef AFS_RXK5
static afs_int32 vsu_rxk5;
#endif

static void
ovlentry_to_nvlentry(oentryp, nentryp)
     struct vldbentry *oentryp;
     struct nvldbentry *nentryp;
{
    register int i;

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

static
nvlentry_to_ovlentry(nentryp, oentryp)
     struct nvldbentry *nentryp;
     struct vldbentry *oentryp;
{
    register int i;

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

static int newvlserver = 0;

VLDB_CreateEntry(entryp)
     struct nvldbentry *entryp;
{
    struct vldbentry oentry;
    register int code;

    if (newvlserver == 1) {
      tryold:
	code = nvlentry_to_ovlentry(entryp, &oentry);
	if (code)
	    return code;
	code = ubik_VL_CreateEntry(cstruct, 0, &oentry);
	return code;
    }
    code = ubik_VL_CreateEntryN(cstruct, 0, entryp);
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = 2;
	}
    }
    return code;
}

VLDB_GetEntryByID(volid, voltype, entryp)
     afs_int32 volid, voltype;
     struct nvldbentry *entryp;
{
    struct vldbentry oentry;
    register int code;

    if (newvlserver == 1) {
      tryold:
	code =
	    ubik_VL_GetEntryByID(cstruct, 0, volid, voltype, &oentry);
	if (!code)
	    ovlentry_to_nvlentry(&oentry, entryp);
	return code;
    }
    code = ubik_VL_GetEntryByIDN(cstruct, 0, volid, voltype, entryp);
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = 2;
	}
    }
    return code;
}

VLDB_GetEntryByName(namep, entryp)
     char *namep;
     struct nvldbentry *entryp;
{
    struct vldbentry oentry;
    register int code;

    if (newvlserver == 1) {
      tryold:
	code = ubik_VL_GetEntryByNameO(cstruct, 0, namep, &oentry);
	if (!code)
	    ovlentry_to_nvlentry(&oentry, entryp);
	return code;
    }
    code = ubik_VL_GetEntryByNameN(cstruct, 0, namep, entryp);
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = 2;
	}
    }
    return code;
}

VLDB_ReplaceEntry(volid, voltype, entryp, releasetype)
     afs_int32 volid, voltype, releasetype;
     struct nvldbentry *entryp;
{
    struct vldbentry oentry;
    register int code;

    if (newvlserver == 1) {
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
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = 2;
	}
    }
    return code;
}



VLDB_ListAttributes(attrp, entriesp, blkentriesp)
     VldbListByAttributes *attrp;
     afs_int32 *entriesp;
     nbulkentries *blkentriesp;
{
    bulkentries arrayEntries;
    register int code, i;

    if (newvlserver == 1) {
      tryold:
	memset(&arrayEntries, 0, sizeof(arrayEntries));	/*initialize to hint the stub  to alloc space */
	code =
	    ubik_VL_ListAttributes(cstruct, 0, attrp, entriesp,
		      &arrayEntries);
	if (!code) {
	    blkentriesp->nbulkentries_val =
		(nvldbentry *) malloc(*entriesp * sizeof(struct nvldbentry));
	    for (i = 0; i < *entriesp; i++) {	/* process each entry */
		ovlentry_to_nvlentry(&arrayEntries.bulkentries_val[i],
				     &blkentriesp->nbulkentries_val[i]);
	    }
	}
	if (arrayEntries.bulkentries_val)
	    free(arrayEntries.bulkentries_val);
	return code;
    }
    code =
	ubik_VL_ListAttributesN(cstruct, 0, attrp, entriesp,
		  blkentriesp);
    if (!newvlserver) {
	if (code == RXGEN_OPCODE) {
	    newvlserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvlserver = 2;
	}
    }
    return code;
}

VLDB_ListAttributesN2(attrp, name, thisindex, nentriesp, blkentriesp,
		      nextindexp)
     VldbListByAttributes *attrp;
     char *name;
     afs_int32 thisindex;
     afs_int32 *nentriesp;
     nbulkentries *blkentriesp;
     afs_int32 *nextindexp;
{
    afs_int32 code;

    code =
	ubik_VL_ListAttributesN2(cstruct, 0, attrp, (name ? name : ""),
		  thisindex, nentriesp, blkentriesp, nextindexp);
    return code;
}


static int vlserverv4 = -1;
struct cacheips {
    afs_int32 server;
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

VLDB_IsSameAddrs(serv1, serv2, errorp)
     afs_int32 serv1, serv2, *errorp;
{
    register int code;
    ListAddrByAttributes attrs;
    bulkaddrs addrs;
    afs_uint32 *addrp, nentries, unique, i, j, f1, f2;
    afsUUID uuid;
    static int initcache = 0;

    *errorp = 0;

    if (serv1 == serv2)
	return 1;
    if (vlserverv4 == 1) {
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
    if (vlserverv4 == -1) {
	if (code == RXGEN_OPCODE) {
	    vlserverv4 = 1;	/* Doesn't support new interface */
	    return 0;
	} else if (!code) {
	    vlserverv4 = 2;
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
vsu_SetCrypt(cryptflag)
     int cryptflag;
{
    if (cryptflag) {
	vsu_rxkad_level = rxkad_crypt;
#ifdef AFS_RXK5
	/* nothing special needs to be done here, because rxkad_crypt == rxk5_crypt, etc */
#endif
    } else {
	vsu_rxkad_level = rxkad_auth;
    }
}


/*
  Get the appropriate type of ubik client structure out from the system.
*/
afs_int32
vsu_ClientInit(noAuthFlag, confDir, cellName, sauth, uclientp, secproc)
     int noAuthFlag;
     int (*secproc) ();
     char *cellName;
     struct ubik_client **uclientp;
     char *confDir;
     afs_int32 sauth;
{
    afs_int32 code, scIndex, i;
    struct afsconf_cell info;
    struct afsconf_dir *tdir;
    struct ktc_principal sname;
    struct ktc_token ttoken;
    struct rx_securityClass *sc;
    static struct rx_connection *serverconns[VLDB_MAXSERVERS];
    char cellstr[64];

    return ugen_ClientInit(noAuthFlag, confDir, cellName, sauth, uclientp, 
			   secproc, "vsu_ClientInit", vsu_rxkad_level,
			   VLDB_MAXSERVERS, AFSCONF_VLDBSERVICE, 90,
			   0, 0, USER_SERVICE_ID);
}


/*extract the name of volume <name> without readonly or backup suffixes
 * and return the result as <rname>.
 */
int
vsu_ExtractName(rname, name)
     char rname[], name[];
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
vsu_GetVolumeID(astring, acstruct, errp)
     struct ubik_client *acstruct;
     afs_int32 *errp;
     char *astring;
{
    afs_uint32 value;
    char volname[VOLSER_OLDMAXVOLNAME + 1];
    struct nvldbentry entry;
    afs_int32 vcode = 0;
    int total;

    *errp = 0;

    if (isdigit(astring[0])) {
	char *end;
	afs_uint32 result;
	result = strtoul(astring, &end, 10);
	if (result != ULONG_MAX && *end == '\0')
	    return result;
    }

    /* It was not a volume number but something else */
    total = strlen(astring);
    vsu_ExtractName(volname, astring);
    vcode = VLDB_GetEntryByName(volname, &entry);
    if (!vcode) {
      if (!strcmp(&astring[total - 9], ".readonly"))
	return entry.volumeId[ROVOL];
      else if ((!strcmp(&astring[total - 7], ".backup")))
	return entry.volumeId[BACKVOL];
      else
	return (entry.volumeId[RWVOL]);
    }
    *errp = vcode;
    return 0;		/* can't find volume */
}
