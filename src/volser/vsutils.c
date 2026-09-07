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

#include <roken.h>

#include <ctype.h>
#ifdef AFS_AIX_ENV
#include <sys/statfs.h>
#endif

#include <afs/afs_lock.h>
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
    memset(entryp, 0, sizeof(*entryp));	    /* ensure padding is cleared */
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
    memset(entryp, 0, sizeof(*entryp));	    /* ensure padding is cleared */
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
    unsigned int i;

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
    afs_uint32 count;
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
    afs_uint32 *addrp, j, f1, f2;
    afs_int32 unique, nentries, i;
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
    if (addrs.bulkaddrs_len < nentries) {
	nentries = addrs.bulkaddrs_len;
    }
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
 * Family-agnostic siblings of VLDB_IsSameAddrs(), for callers that now
 * hold a struct rx_sockaddr (v4 or v6) rather than a raw IPv4 address -
 * e.g. src/volser/vos.c's GetServer()/vsprocs.c's UV_* functions, since
 * their own IPv6 conversion.
 *
 * VLDB_IsSameAddrs() itself stays IPv4-only: the multi-homed-equivalence
 * lookup it performs (ubik_VL_GetAddrsU(), keyed by a plain afs_uint32)
 * has no v6-capable sibling yet (that would need a UUID- or
 * VL_GetEndpoints-based equivalent - out of scope here, see the longer
 * comment in vsprocs.c's UV_CreateVolume3()). So when either side of the
 * comparison is a genuinely non-v4-mappable address, these fall back to
 * plain address equality, which is correct except for the narrow case of
 * two different addresses of the same v6-only multi-homed server - a
 * real but small gap, not a silent wrong answer (a v6-only server can
 * never spuriously match an unrelated VLDB v4 site this way).
 */
int
VLDB_IsSameServer(const struct rx_sockaddr *sa1, const struct rx_sockaddr *sa2,
		  afs_int32 *errorp)
{
    afs_uint32 ip1, ip2;

    *errorp = 0;
    if (rx_try_sockaddr_to_ipv4(sa1, &ip1) && rx_try_sockaddr_to_ipv4(sa2, &ip2))
	return VLDB_IsSameAddrs(ip1, ip2, errorp);
    return rx_compare_sockaddr(sa1, sa2, RXA_ADDR);
}

/* Same as VLDB_IsSameServer(), but the second address is a raw IPv4
 * address already in hand (typically a VLDB entry's serverNumber[]). */
int
VLDB_SockaddrMatchesIP(const struct rx_sockaddr *sa, afs_uint32 ip, afs_int32 *errorp)
{
    struct rx_sockaddr ipsa;

    rx_ipv4_to_sockaddr(ip, 0, 0, &ipsa);
    return VLDB_IsSameServer(sa, &ipsa, errorp);
}

/*
 * Resolve a fileserver's rx address to its VLDB-registered uuid, for a
 * caller (UV_CreateVolume3()/UV_AddSite2(), vsprocs.c) that needs to name
 * a VLDB site by uuid because the server has no IPv4 address for the
 * classic representation.
 *
 * There is no VLDB lookup RPC keyed by a raw IPv6 address - GetAddrsU's
 * and GetEndpoints' own address-mask lookup (VLADDR_IPADDR) only ever
 * took a plain afs_uint32. So instead this walks every relative server-id
 * slot (VLADDR_INDEX 1..MAXSERVERID, the same range GetEndpoints itself
 * validates against) and compares aserver against each slot's endpoint
 * list - the address-independent analogue of what IpAddrToRelAddr() does
 * server-side by scanning ctx->hostaddress[]/the Multi-homed Entry chain
 * directly. The table is small (<=254 entries) and this is only ever
 * called once per site being added, not on any hot path.
 *
 * Returns 0 and fills *uuidp on a match. VL_NOENT means no registered
 * server has aserver among its endpoints - most likely because it has
 * not called RegisterAddrs/RegisterEndpoints yet (a fileserver does this
 * automatically at startup, before serving any volume, so in practice
 * this means the address is simply wrong). Any other nonzero return is a
 * genuine RPC failure, including RXGEN_OPCODE against a vlserver that
 * predates GetEndpoints - the caller's cue to fall back to the classic,
 * IPv4-only entry-creation path's hard failure.
 */
int
VLDB_GetUuidByAddr(const struct rx_sockaddr *aserver, afsUUID *uuidp)
{
    ListAddrByAttributes attrs;
    afsUUID tuuid;
    afs_int32 unique, index, code, i;
    vlendpoints endpoints;
    int found;

    for (index = 1; index <= MAXSERVERID; index++) {
	memset(&attrs, 0, sizeof(attrs));
	attrs.Mask = VLADDR_INDEX;
	attrs.index = index;
	memset(&tuuid, 0, sizeof(tuuid));
	memset(&endpoints, 0, sizeof(endpoints));
	unique = 0;

	code = ubik_VL_GetEndpoints(cstruct, 0, &attrs, &tuuid, &unique,
				     &endpoints);
	if (code == VL_NOENT || code == VL_INDEXERANGE) {
	    /* Unused relative-id slot; keep scanning. */
	    continue;
	}
	if (code) {
	    /* Includes RXGEN_OPCODE (old vlserver) - stop and let the
	     * caller decide how to report it. */
	    if (endpoints.vlendpoints_val)
		xdr_free((xdrproc_t) xdr_vlendpoints, &endpoints);
	    return code;
	}

	found = 0;
	for (i = 0; i < endpoints.vlendpoints_len; i++) {
	    struct vlendpoint *ep = &endpoints.vlendpoints_val[i];
	    struct rx_sockaddr sa;
	    unsigned char v6[16];
	    int j;
	    afs_uint32 w;

	    memset(&sa, 0, sizeof(sa));
	    if (ep->type == VL_ENDPOINT_IPV4 && ep->length == 4) {
		rx_ipv4_to_sockaddr(htonl(ep->value[0]), 0, 0, &sa);
#ifdef HAVE_IPV6
	    } else if (ep->type == VL_ENDPOINT_IPV6 && ep->length == 16) {
		for (j = 0; j < 4; j++) {
		    w = htonl(ep->value[j]);
		    memcpy(&v6[j * 4], &w, 4);
		}
		rx_ipv6_to_sockaddr(v6, 0, 0, &sa);
#endif
	    } else {
		continue;
	    }
	    if (rx_compare_sockaddr(aserver, &sa, RXA_ADDR)) {
		found = 1;
		break;
	    }
	}
	xdr_free((xdrproc_t) xdr_vlendpoints, &endpoints);
	if (found) {
	    *uuidp = tuuid;
	    return 0;
	}
    }
    return VL_NOENT;
}

/*
 * Build a uvldbentry from an nvldbentry that is already in the "ready to
 * send" state VLDB_CreateEntry()/VLDB_ReplaceEntry() themselves expect
 * (i.e. right after MapNetworkToHost(), the same state UV_CreateVolume3()/
 * UV_AddSite2() (vsprocs.c) already build for the classic path) -
 * preserving every site's address using the classic IPv4-in-time_low
 * representation (no VLSF_UUID), matching vlentry_to_uvldbentry()'s
 * output convention server-side.
 *
 * This exists so a caller that needs to name exactly one site (the one
 * with no IPv4 address) by uuid doesn't have to rebuild every other,
 * perfectly ordinary site from scratch: it builds the classic entry as
 * always, converts it wholesale with this, then overwrites just the one
 * site's serverNumber/serverFlags with the uuid and VLSF_UUID.
 */
void
VLDB_NvldbentryToUvldbentry(struct nvldbentry *entryp, struct uvldbentry *uentryp)
{
    int i, count;

    memset(uentryp, 0, sizeof(*uentryp));
    strncpy(uentryp->name, entryp->name, sizeof(uentryp->name));
    uentryp->nServers = entryp->nServers;
    count = entryp->nServers;
    if (count < NMAXNSERVERS)
	count++;
    for (i = 0; i < count; i++) {
	memset(&uentryp->serverNumber[i], 0, sizeof(uentryp->serverNumber[i]));
	uentryp->serverNumber[i].time_low = entryp->serverNumber[i];
	uentryp->serverPartition[i] = entryp->serverPartition[i];
	uentryp->serverFlags[i] = entryp->serverFlags[i];
    }
    for (i = 0; i < MAXTYPES; i++)
	uentryp->volumeId[i] = entryp->volumeId[i];
    uentryp->cloneId = entryp->cloneId;
    uentryp->flags = entryp->flags;
}

/*
 * CreateEntryU/ReplaceEntryU's thin client wrappers, mirroring
 * VLDB_CreateEntry()/VLDB_ReplaceEntry() but with no old-server fallback
 * of their own - unlike the N-vs-classic negotiation those perform
 * internally, a caller only ever reaches for the U-family RPCs when it
 * specifically needs to name a site by uuid (no IPv4 address exists to
 * fall back to within the classic wire format), so the caller itself
 * must decide what to do if the peer is too old to support them
 * (RXGEN_OPCODE) - there is no less-capable representation to retry
 * with here.
 */
int
VLDB_CreateEntryU(struct uvldbentry *entryp)
{
    return ubik_VL_CreateEntryU(cstruct, 0, entryp);
}

int
VLDB_ReplaceEntryU(afs_uint32 volid, afs_int32 voltype,
		   struct uvldbentry *entryp, afs_int32 releasetype)
{
    return ubik_VL_ReplaceEntryU(cstruct, 0, volid, voltype, entryp,
				 releasetype);
}

/*
  Get the appropriate type of ubik client structure out from the system.
*/
int
vsu_ClientInit(const char *confDir, char *cellName, int secFlags,
	       int (*secproc)(struct rx_securityClass *, afs_int32),
	       struct ubik_client **uclientp)
{
    return ugen_ClientInitFlags(confDir, cellName, secFlags, uclientp,
				secproc, VLDB_MAXSERVERS, AFSCONF_VLDBSERVICE,
				90);
}

/*extract the name of volume <name> without readonly or backup suffixes
 * and return the result as <rname>.
 */
int
vsu_ExtractName(char rname[], char name[])
{
    char sname[VOLSER_OLDMAXVOLNAME + 1];
    size_t total;

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
    size_t total;

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
