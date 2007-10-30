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

#include "vosutils.h"
#include "vsprocs.h"
#include "lockprocs.h"
#include <afs/afs_AdminErrors.h>
#include <string.h>

/*
 * VLDB entry conversion routines.
 * convert from one type of VLDB record to another.  There are only
 * two types "old" and "new".
 */

static int
OldVLDB_to_NewVLDB(struct vldbentry *source, struct nvldbentry *dest,
		   afs_status_p st)
{
    register int i;
    int rc = 0;
    afs_status_t tst = 0;

    memset(dest, 0, sizeof(struct nvldbentry));
    strncpy(dest->name, source->name, sizeof(dest->name));
    for (i = 0; i < source->nServers; i++) {
	dest->serverNumber[i] = source->serverNumber[i];
	dest->serverPartition[i] = source->serverPartition[i];
	dest->serverFlags[i] = source->serverFlags[i];
    }
    dest->nServers = source->nServers;
    for (i = 0; i < MAXTYPES; i++)
	dest->volumeId[i] = source->volumeId[i];
    dest->cloneId = source->cloneId;
    dest->flags = source->flags;

    rc = 1;

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * We can fail to store the new to old VLDB record if there are more
 * servers in the cell than the old format can handle.  If we fail,
 * return an error.
 */

static int
NewVLDB_to_OldVLDB(struct nvldbentry *source, struct vldbentry *dest,
		   afs_status_p st)
{
    register int i;
    afs_status_t tst = 0;
    int rc = 0;

    memset(dest, 0, sizeof(struct vldbentry));
    strncpy(dest->name, source->name, sizeof(dest->name));
    if (source->nServers <= OMAXNSERVERS) {
	for (i = 0; i < source->nServers; i++) {
	    dest->serverNumber[i] = source->serverNumber[i];
	    dest->serverPartition[i] = source->serverPartition[i];
	    dest->serverFlags[i] = source->serverFlags[i];
	}
	dest->nServers = i;
	for (i = 0; i < MAXTYPES; i++)
	    dest->volumeId[i] = source->volumeId[i];
	dest->cloneId = source->cloneId;
	dest->flags = source->flags;
	rc = 1;
    } else {
	tst = VL_BADSERVER;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

int
VLDB_CreateEntry(afs_cell_handle_p cellHandle, struct nvldbentry *entryp,
		 afs_status_p st)
{
    struct vldbentry oentry;
    afs_status_t tst = 0;
    int rc = 0;

    do {
	if (cellHandle->vos_new) {
	    tst = ubik_VL_CreateEntryN(cellHandle->vos, 0, entryp);
	    if (tst) {
		if (tst == RXGEN_OPCODE) {
		    cellHandle->vos_new = 0;
		}
	    } else {
		rc = 1;
	    }
	} else {
	    if (NewVLDB_to_OldVLDB(entryp, &oentry, &tst)) {
		tst = ubik_VL_CreateEntry(cellHandle->vos, 0, &oentry);
		if (!tst) {
		    rc = 1;
		}
	    }
	}
    } while (tst == RXGEN_OPCODE);

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

int
aVLDB_GetEntryByID(afs_cell_handle_p cellHandle, afs_int32 volid,
		  afs_int32 voltype, struct nvldbentry *entryp,
		  afs_status_p st)
{
    struct vldbentry oentry;
    afs_status_t tst = 0;
    int rc = 0;

    do {
	if (cellHandle->vos_new) {
	    tst =
		ubik_VL_GetEntryByIDN(cellHandle->vos, 0, volid,
			  voltype, entryp);
	    if (tst) {
		if (tst == RXGEN_OPCODE) {
		    cellHandle->vos_new = 0;
		}
	    } else {
		rc = 1;
	    }
	} else {
	    tst =
		ubik_VL_GetEntryByID(cellHandle->vos, 0, volid, voltype,
			  &oentry);
	    if (tst == 0) {
		rc = OldVLDB_to_NewVLDB(&oentry, entryp, &tst);
	    }
	}

    } while (tst == RXGEN_OPCODE);

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

int
aVLDB_GetEntryByName(afs_cell_handle_p cellHandle, const char *namep,
		    struct nvldbentry *entryp, afs_status_p st)
{
    struct vldbentry oentry;
    afs_status_t tst = 0;
    int rc = 0;

    do {
	if (cellHandle->vos_new) {
	    tst =
		ubik_VL_GetEntryByNameN(cellHandle->vos, 0, namep,
			  entryp);
	    if (tst) {
		if (tst == RXGEN_OPCODE) {
		    cellHandle->vos_new = 0;
		}
	    } else {
		rc = 1;
	    }
	} else {
	    tst =
		ubik_VL_GetEntryByNameO(cellHandle->vos, 0, namep,
			  &oentry);
	    if (tst == 0) {
		rc = OldVLDB_to_NewVLDB(&oentry, entryp, &tst);
	    }
	}

    } while (tst == RXGEN_OPCODE);

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

int
VLDB_ReplaceEntry(afs_cell_handle_p cellHandle, afs_int32 volid,
		  afs_int32 voltype, struct nvldbentry *entryp,
		  afs_int32 releasetype, afs_status_p st)
{
    struct vldbentry oentry;
    afs_status_t tst = 0;
    int rc = 0;

    do {
	if (cellHandle->vos_new) {
	    tst =
		ubik_VL_ReplaceEntryN(cellHandle->vos, 0, volid,
			  voltype, entryp, releasetype);
	    if (tst) {
		if (tst == RXGEN_OPCODE) {
		    cellHandle->vos_new = 0;
		}
	    } else {
		rc = 1;
	    }
	} else {
	    if (NewVLDB_to_OldVLDB(entryp, &oentry, &tst)) {
		tst =
		    ubik_VL_ReplaceEntry(cellHandle->vos, 0, volid,
			      voltype, &oentry, releasetype);
		if (!tst) {
		    rc = 1;
		}
	    }
	}
    } while (tst == RXGEN_OPCODE);

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

int
VLDB_ListAttributes(afs_cell_handle_p cellHandle,
		    VldbListByAttributes * attrp, afs_int32 * entriesp,
		    nbulkentries * blkentriesp, afs_status_p st)
{
    bulkentries arrayEntries;
    int i;
    afs_status_t tst = 0;
    int rc = 0;

    do {
	if (cellHandle->vos_new) {
	    tst =
		ubik_VL_ListAttributesN(cellHandle->vos, 0, attrp,
			  entriesp, blkentriesp);
	    if (tst) {
		if (tst == RXGEN_OPCODE) {
		    cellHandle->vos_new = 0;
		}
	    } else {
		rc = 1;
	    }
	} else {
	    memset((void *)&arrayEntries, 0, sizeof(arrayEntries));
	    tst =
		ubik_VL_ListAttributes(cellHandle->vos, 0, attrp,
			  entriesp, &arrayEntries);
	    if (tst == 0) {
		blkentriesp->nbulkentries_val =
		    (nvldbentry *) malloc(*entriesp * sizeof(*blkentriesp));
		if (blkentriesp->nbulkentries_val != NULL) {
		    for (i = 0; i < *entriesp; i++) {
			OldVLDB_to_NewVLDB((struct vldbentry *)&arrayEntries.
					   bulkentries_val[i],
					   (struct nvldbentry *)&blkentriesp->
					   nbulkentries_val[i], &tst);
		    }
		} else {
		    tst = ADMNOMEM;
		}
		if (arrayEntries.bulkentries_val) {
		    free(arrayEntries.bulkentries_val);
		}
		rc = 1;
	    }
	}

    } while (tst == RXGEN_OPCODE);

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

int
VLDB_ListAttributesN2(afs_cell_handle_p cellHandle,
		      VldbListByAttributes * attrp, char *name,
		      afs_int32 thisindex, afs_int32 * nentriesp,
		      nbulkentries * blkentriesp, afs_int32 * nextindexp,
		      afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    tst =
	ubik_VL_ListAttributesN2(cellHandle->vos, 0, attrp,
		  (name ? name : ""), thisindex, nentriesp, blkentriesp,
		  nextindexp);
    if (!tst) {
	rc = 1;
    }

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

int
VLDB_IsSameAddrs(afs_cell_handle_p cellHandle, afs_int32 serv1,
		 afs_int32 serv2, int *equal, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;

    ListAddrByAttributes attrs;
    bulkaddrs addrs;
    afs_uint32 *addrp, nentries, unique, i;
    afsUUID uuid;

    *equal = 0;

    if (serv1 == serv2) {
	*equal = 1;
	rc = 1;
	goto fail_VLDB_IsSameAddrs;
    }

    memset(&attrs, 0, sizeof(attrs));
    attrs.Mask = VLADDR_IPADDR;
    attrs.ipaddr = serv1;
    memset(&addrs, 0, sizeof(addrs));
    memset(&uuid, 0, sizeof(uuid));
    tst =
	ubik_VL_GetAddrsU(cellHandle->vos, 0, &attrs, &uuid, &unique,
		  &nentries, &addrs);
    if (tst) {
	*equal = 0;
	goto fail_VLDB_IsSameAddrs;
    }

    addrp = addrs.bulkaddrs_val;
    for (i = 0; i < nentries; i++, addrp++) {
	if (serv2 == *addrp) {
	    *equal = 1;
	    break;
	}
    }
    rc = 1;

  fail_VLDB_IsSameAddrs:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * GetVolumeInfo - retrieve information about a particular volume.
 *
 * PARAMETERS
 *
 * IN cellHandle - a handle that corresponds to the cell where the volume
 * is located.
 *
 * IN volid - the volume to be retrieved.
 *
 * OUT rentry - the vldb entry of the volume.
 *
 * OUT server - the address of the server where the volume resides in 
 * host byte order.
 *
 * OUT partition - the volume to be retrieved.
 *
 * OUT voltype - the type of volume retrieved.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int
GetVolumeInfo(afs_cell_handle_p cellHandle, unsigned int volid,
	      struct nvldbentry *rentry, afs_int32 * server,
	      afs_int32 * partition, afs_int32 * voltype, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst;
    int i, index = -1;

    if (!aVLDB_GetEntryByID(cellHandle, volid, -1, rentry, &tst)) {
	rc = 0;
	goto fail_GetVolumeInfo;
    }

    if (volid == rentry->volumeId[ROVOL]) {
	*voltype = ROVOL;
	for (i = 0; i < rentry->nServers; i++) {
	    if ((index == -1) && (rentry->serverFlags[i] & ITSROVOL)
		&& !(rentry->serverFlags[i] & RO_DONTUSE))
		index = i;
	}
	if (index == -1) {
	    tst = 1;
	    goto fail_GetVolumeInfo;
	}
	*server = rentry->serverNumber[index];
	*partition = rentry->serverPartition[index];
	rc = 1;
	goto fail_GetVolumeInfo;
    }

    if ((index = Lp_GetRwIndex(cellHandle, rentry, &tst)) < 0) {
	goto fail_GetVolumeInfo;
    }
    if (volid == rentry->volumeId[RWVOL]) {
	*voltype = RWVOL;
	*server = rentry->serverNumber[index];
	*partition = rentry->serverPartition[index];
    } else if (volid == rentry->volumeId[BACKVOL]) {
	*voltype = BACKVOL;
	*server = rentry->serverNumber[index];
	*partition = rentry->serverPartition[index];
    }
    rc = 1;

  fail_GetVolumeInfo:

    if (st != NULL) {
	*st = tst;
    }
    return rc;
}

/*
 * ValidateVolumeName - validate a potential volume name
 *
 * PARAMETERS
 *
 * IN volumeName - the volume name to be validated.
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

int
ValidateVolumeName(const char *volumeName, afs_status_p st)
{
    int rc = 0;
    afs_status_t tst = 0;
    size_t len;

    if ((volumeName == NULL) || (*volumeName == 0)) {
	tst = ADMVOSVOLUMENAMENULL;
	goto fail_ValidateVolumeName;
    }

    if (!ISNAMEVALID(volumeName)) {
	tst = ADMVOSVOLUMENAMETOOLONG;
	goto fail_ValidateVolumeName;
    }

    len = strlen(volumeName);

    if (((len > 8) && (!strcmp(&volumeName[len - 9], ".readonly")))
	|| ((len > 6) && (!strcmp(&volumeName[len - 7], ".backup")))) {
	tst = ADMVOSVOLUMENAMEINVALID;
	goto fail_ValidateVolumeName;
    }

    rc = 1;

  fail_ValidateVolumeName:

    if (st != NULL) {
	*st = tst;
    }

    return rc;
}

/*extract the name of volume <name> without readonly or backup suffixes
 * and return the result as <rname>.
 */
int
vsu_ExtractName(char *rname, char *name)
{
    char sname[32];
    size_t total;

    strncpy(sname, name, 32);
    sname[31] ='\0';
    total = strlen(sname);
    if ((total > 9) && (!strcmp(&sname[total - 9], ".readonly"))) {
	/*discard the last 8 chars */
	sname[total - 9] = '\0';
	strcpy(rname, sname);
	return 0;
    } else if ((total > 7) && (!strcmp(&sname[total - 7], ".backup"))) {
	/*discard last 6 chars */
	sname[total - 7] = '\0';
	strcpy(rname, sname);
	return 0;
    } else {
	strncpy(rname, name, VOLSER_OLDMAXVOLNAME);
	return -1;
    }
}



/*
 * AddressMatch - determines if an IP address matches a pattern
 *
 * PARAMETERS
 *
 * IN addrTest - the IP address to test, in either byte-order
 * IN addrPattern - the IP address pattern, in the same byte-order
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 if the address matches the pattern specified
 * (where 255 in any byte in the pattern indicates a wildcard).
 */

int
AddressMatch(int addrTest, int addrPattern)
{
    int bTest;
    int bPattern;

    /* Test the high byte */
    bTest = addrTest >> 24;
    bPattern = addrPattern >> 24;
    if ((bTest != bPattern) && (bPattern != 255)) {
	return FALSE;
    }

    /* Test the next-highest byte */
    bTest = (addrTest >> 16) & 255;
    bPattern = (addrPattern >> 16) & 255;
    if ((bTest != bPattern) && (bPattern != 255)) {
	return FALSE;
    }

    /* Test the next-to-lowest byte */
    bTest = (addrTest >> 8) & 255;
    bPattern = (addrPattern >> 8) & 255;
    if ((bTest != bPattern) && (bPattern != 255)) {
	return FALSE;
    }

    /* Test the low byte */
    bTest = addrTest & 255;
    bPattern = addrPattern & 255;
    if ((bTest != bPattern) && (bPattern != 255)) {
	return FALSE;
    }

    return TRUE;
}


/*
 * RemoveBadAddresses - (optionally) removes addresses that are better ignored
 *
 * PARAMETERS
 *
 * IN OUT totalp - the number of addresses in the addrsp structure
 * IN OUT addrsp - a bulk array of addresses
 *
 * LOCKS
 *
 * No locks are obtained or released by this function
 *
 * RETURN CODES
 *
 * Returns != 0 upon successful completion.
 */

static pthread_once_t badaddr_init_once = PTHREAD_ONCE_INIT;
static int addr_to_skip;

static void
badaddr_once(void)
{

#ifdef AFS_NT40_ENV

#define cszREG_IGNORE_KEY "Software\\TransarcCorporation\\AFS Control Center"
#define cszREG_IGNORE_VALUE "IgnoreBadAddrs"

    /*
     * In order for this routine to do anything, it must first validate
     * that the user of this machine wants this filtering to take place.
     * There is an undocumented registry value which signifies that it's
     * okay to filter bogus IP addresses--and, moreover, indicates
     * the range of values which may be ignored. Check that value.
     */

    HKEY hk;
    addr_to_skip = 0;		/* don't ignore any addrs unless we find otherwise */
    if (RegOpenKey(HKEY_LOCAL_MACHINE, cszREG_IGNORE_KEY, &hk) == 0) {
	DWORD dwType = REG_DWORD;
	DWORD dwSize = sizeof(addr_to_skip);
	RegQueryValueEx(hk, cszREG_IGNORE_VALUE, 0, &dwType,
			(PBYTE) & addr_to_skip, &dwSize);
	RegCloseKey(hk);
    }
#else

    /*
     * We only support this functionality (so far) on NT; on other
     * platforms, we'll never ignore IP addresses. If the feature
     * is needed in the future, here's the place to add it.
     *
     */

    addr_to_skip = 0;		/* don't skip any addresses */

#endif

}

int
RemoveBadAddresses(afs_int32 * totalp, bulkaddrs * addrsp)
{
    pthread_once(&badaddr_init_once, badaddr_once);

    /*
     * If we've been requested to skip anything, addr_to_skip will be
     * non-zero. It's actually an IP address of the form:
     *    10.0.0.255
     * A "255" means any value is acceptable, and any other value means
     * the to-be-skipped address must match that value.
     */

    if (addr_to_skip && addrsp && addrsp->bulkaddrs_val) {
	size_t iiWrite = 0;
	size_t iiRead = 0;
	for (; iiRead < addrsp->bulkaddrs_len; ++iiRead) {

	    /*
	     * Check this IP address to see if it should be skipped.
	     */

	    if (!AddressMatch(addrsp->bulkaddrs_val[iiRead], addr_to_skip)) {

		/*
		 * The address is okay; make sure it stays in the list.
		 */

		if (iiWrite != iiRead) {
		    addrsp->bulkaddrs_val[iiWrite] =
			addrsp->bulkaddrs_val[iiRead];
		}

		++iiWrite;
	    }
	}
	*totalp = (afs_int32) iiWrite;
	addrsp->bulkaddrs_len = iiWrite;
    }

    return TRUE;
}
