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
#include <string.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <lock.h>
#include <rx/xdr.h>
#include <ubik.h>
#include "vlserver.h"

extern struct vlheader cheader;
struct vlheader xheader;
extern afs_uint32 HostAddress[];
extern int maxnservers;
struct extentaddr extentaddr;
extern struct extentaddr *ex_addr[];
int vldbversion = 0;

static int index_OK();

#define ERROR_EXIT(code) {error=(code); goto error_exit;}

/* Hashing algorithm based on the volume id; HASHSIZE must be prime */
afs_int32
IDHash(volumeid)
     afs_int32 volumeid;
{
    return ((abs(volumeid)) % HASHSIZE);
}


/* Hashing algorithm based on the volume name; name's size is implicit (64 chars) and if changed it should be reflected here. */
afs_int32
NameHash(volumename)
     register char *volumename;
{
    register unsigned int hash;
    register int i;

    hash = 0;
    for (i = strlen(volumename), volumename += i - 1; i--; volumename--)
	hash = (hash * 63) + (*((unsigned char *)volumename) - 63);
    return (hash % HASHSIZE);
}


/* package up seek and write into one procedure for ease of use */
afs_int32
vlwrite(trans, offset, buffer, length)
     struct ubik_trans *trans;
     afs_int32 offset;
     char *buffer;
     afs_int32 length;
{
    afs_int32 errorcode;

    if (errorcode = ubik_Seek(trans, 0, offset))
	return errorcode;
    return (ubik_Write(trans, buffer, length));
}


/* Package up seek and read into one procedure for ease of use */
afs_int32
vlread(trans, offset, buffer, length)
     struct ubik_trans *trans;
     afs_int32 offset;
     char *buffer;
     afs_int32 length;
{
    afs_int32 errorcode;

    if (errorcode = ubik_Seek(trans, 0, offset))
	return errorcode;
    return (ubik_Read(trans, buffer, length));
}


/* take entry and convert to network order and write to disk */
afs_int32
vlentrywrite(trans, offset, buffer, length)
     struct ubik_trans *trans;
     afs_int32 offset;
     char *buffer;
     afs_int32 length;
{
    struct vlentry oentry;
    struct nvlentry nentry, *nep;
    char *bufp;
    register afs_int32 i;

    if (length != sizeof(oentry))
	return -1;
    if (maxnservers == 13) {
	nep = (struct nvlentry *)buffer;
	for (i = 0; i < MAXTYPES; i++)
	    nentry.volumeId[i] = htonl(nep->volumeId[i]);
	nentry.flags = htonl(nep->flags);
	nentry.LockAfsId = htonl(nep->LockAfsId);
	nentry.LockTimestamp = htonl(nep->LockTimestamp);
	nentry.cloneId = htonl(nep->cloneId);
	for (i = 0; i < MAXTYPES; i++)
	    nentry.nextIdHash[i] = htonl(nep->nextIdHash[i]);
	nentry.nextNameHash = htonl(nep->nextNameHash);
	memcpy(nentry.name, nep->name, VL_MAXNAMELEN);
	memcpy(nentry.serverNumber, nep->serverNumber, NMAXNSERVERS);
	memcpy(nentry.serverPartition, nep->serverPartition, NMAXNSERVERS);
	memcpy(nentry.serverFlags, nep->serverFlags, NMAXNSERVERS);
	bufp = (char *)&nentry;
    } else {
	memset(&oentry, 0, sizeof(struct vlentry));
	nep = (struct nvlentry *)buffer;
	for (i = 0; i < MAXTYPES; i++)
	    oentry.volumeId[i] = htonl(nep->volumeId[i]);
	oentry.flags = htonl(nep->flags);
	oentry.LockAfsId = htonl(nep->LockAfsId);
	oentry.LockTimestamp = htonl(nep->LockTimestamp);
	oentry.cloneId = htonl(nep->cloneId);
	for (i = 0; i < MAXTYPES; i++)
	    oentry.nextIdHash[i] = htonl(nep->nextIdHash[i]);
	oentry.nextNameHash = htonl(nep->nextNameHash);
	memcpy(oentry.name, nep->name, VL_MAXNAMELEN);
	memcpy(oentry.serverNumber, nep->serverNumber, OMAXNSERVERS);
	memcpy(oentry.serverPartition, nep->serverPartition, OMAXNSERVERS);
	memcpy(oentry.serverFlags, nep->serverFlags, OMAXNSERVERS);
	bufp = (char *)&oentry;
    }
    return vlwrite(trans, offset, bufp, length);
}

/* read entry and convert to host order and write to disk */
afs_int32
vlentryread(trans, offset, buffer, length)
     struct ubik_trans *trans;
     afs_int32 offset;
     char *buffer;
     afs_int32 length;
{
    struct vlentry *oep, tentry;
    struct nvlentry *nep, *nbufp;
    char *bufp = (char *)&tentry;
    register afs_int32 i;

    if (length != sizeof(vlentry))
	return -1;
    i = vlread(trans, offset, bufp, length);
    if (i)
	return i;
    if (maxnservers == 13) {
	nep = (struct nvlentry *)bufp;
	nbufp = (struct nvlentry *)buffer;
	for (i = 0; i < MAXTYPES; i++)
	    nbufp->volumeId[i] = ntohl(nep->volumeId[i]);
	nbufp->flags = ntohl(nep->flags);
	nbufp->LockAfsId = ntohl(nep->LockAfsId);
	nbufp->LockTimestamp = ntohl(nep->LockTimestamp);
	nbufp->cloneId = ntohl(nep->cloneId);
	for (i = 0; i < MAXTYPES; i++)
	    nbufp->nextIdHash[i] = ntohl(nep->nextIdHash[i]);
	nbufp->nextNameHash = ntohl(nep->nextNameHash);
	memcpy(nbufp->name, nep->name, VL_MAXNAMELEN);
	memcpy(nbufp->serverNumber, nep->serverNumber, NMAXNSERVERS);
	memcpy(nbufp->serverPartition, nep->serverPartition, NMAXNSERVERS);
	memcpy(nbufp->serverFlags, nep->serverFlags, NMAXNSERVERS);
    } else {
	oep = (struct vlentry *)bufp;
	nbufp = (struct nvlentry *)buffer;
	memset(nbufp, 0, sizeof(struct nvlentry));
	for (i = 0; i < MAXTYPES; i++)
	    nbufp->volumeId[i] = ntohl(oep->volumeId[i]);
	nbufp->flags = ntohl(oep->flags);
	nbufp->LockAfsId = ntohl(oep->LockAfsId);
	nbufp->LockTimestamp = ntohl(oep->LockTimestamp);
	nbufp->cloneId = ntohl(oep->cloneId);
	for (i = 0; i < MAXTYPES; i++)
	    nbufp->nextIdHash[i] = ntohl(oep->nextIdHash[i]);
	nbufp->nextNameHash = ntohl(oep->nextNameHash);
	memcpy(nbufp->name, oep->name, VL_MAXNAMELEN);
	memcpy(nbufp->serverNumber, oep->serverNumber, NMAXNSERVERS);
	memcpy(nbufp->serverPartition, oep->serverPartition, NMAXNSERVERS);
	memcpy(nbufp->serverFlags, oep->serverFlags, NMAXNSERVERS);
    }
    return 0;
}

/* Convenient write of small critical vldb header info to the database. */
int
write_vital_vlheader(trans)
     register struct ubik_trans *trans;
{
    if (vlwrite
	(trans, 0, (char *)&cheader.vital_header, sizeof(vital_vlheader)))
	return VL_IO;
    return 0;
}


int extent_mod = 0;

/* This routine reads in the extent blocks for multi-homed servers.
 * There used to be an initialization bug that would cause the contaddrs
 * pointers in the first extent block to be bad. Here we will check the
 * pointers and zero them in the in-memory copy if we find them bad. We
 * also try to write the extent blocks back out. If we can't, then we
 * will wait until the next write transaction to write them out
 * (extent_mod tells us the on-disk copy is bad).
 */
afs_int32
readExtents(trans)
     struct ubik_trans *trans;
{
    afs_uint32 extentAddr;
    afs_int32 error = 0, code;
    int i;

    extent_mod = 0;
    extentAddr = ntohl(cheader.SIT);
    if (!extentAddr)
	return 0;

    /* Read the first extension block */
    if (!ex_addr[0]) {
	ex_addr[0] = (struct extentaddr *)malloc(VL_ADDREXTBLK_SIZE);
	if (!ex_addr[0])
	    ERROR_EXIT(VL_NOMEM);
    }
    code = vlread(trans, extentAddr, (char *)ex_addr[0], VL_ADDREXTBLK_SIZE);
    if (code) {
	free(ex_addr[0]);	/* Not the place to create it */
	ex_addr[0] = 0;
	ERROR_EXIT(VL_IO);
    }

    /* In case more that 64 mh servers are in use they're kept in these
     * continuation blocks
     */
    for (i = 1; i < VL_MAX_ADDREXTBLKS; i++) {
	if (!ex_addr[0]->ex_contaddrs[i])
	    continue;

	/* Before reading it in, check to see if the address is good */
	if ((ntohl(ex_addr[0]->ex_contaddrs[i]) <
	     ntohl(ex_addr[0]->ex_contaddrs[i - 1]) + VL_ADDREXTBLK_SIZE)
	    || (ntohl(ex_addr[0]->ex_contaddrs[i]) >
		ntohl(cheader.vital_header.eofPtr) - VL_ADDREXTBLK_SIZE)) {
	    extent_mod = 1;
	    ex_addr[0]->ex_contaddrs[i] = 0;
	    continue;
	}


	/* Read the continuation block */
	if (!ex_addr[i]) {
	    ex_addr[i] = (struct extentaddr *)malloc(VL_ADDREXTBLK_SIZE);
	    if (!ex_addr[i])
		ERROR_EXIT(VL_NOMEM);
	}
	code =
	    vlread(trans, ntohl(ex_addr[0]->ex_contaddrs[i]),
		   (char *)ex_addr[i], VL_ADDREXTBLK_SIZE);
	if (code) {
	    free(ex_addr[i]);	/* Not the place to create it */
	    ex_addr[i] = 0;
	    ERROR_EXIT(VL_IO);
	}

	/* After reading it in, check to see if its a real continuation block */
	if (ntohl(ex_addr[i]->ex_flags) != VLCONTBLOCK) {
	    extent_mod = 1;
	    ex_addr[0]->ex_contaddrs[i] = 0;
	    free(ex_addr[i]);	/* Not the place to create it */
	    ex_addr[i] = 0;
	    continue;
	}
    }

    if (extent_mod) {
	code = vlwrite(trans, extentAddr, ex_addr[0], VL_ADDREXTBLK_SIZE);
	if (!code) {
	    VLog(0, ("Multihome server support modification\n"));
	}
	/* Keep extent_mod true in-case the transaction aborts */
	/* Don't return error so we don't abort transaction */
    }

  error_exit:
    return error;
}

/* Check that the database has been initialized.  Be careful to fail in a safe
   manner, to avoid bogusly reinitializing the db.  */
afs_int32
CheckInit(trans, builddb)
     struct ubik_trans *trans;
     int builddb;
{
    afs_int32 error = 0, i, code, ubcode = 0;

    /* ubik_CacheUpdate must be called on every transaction.  It returns 0 if the
     * previous transaction would have left the cache fine, and non-zero otherwise.
     * Thus, a local abort or a remote commit will cause this to return non-zero
     * and force a header re-read.  Necessary for a local abort because we may
     * have damaged cheader during the operation.  Necessary for a remote commit
     * since it may have changed cheader. 
     */
    if (ubik_CacheUpdate(trans) != 0) {
	/* if version changed (or first call), read the header */
	ubcode = vlread(trans, 0, (char *)&cheader, sizeof(cheader));
	vldbversion = ntohl(cheader.vital_header.vldbversion);

	if (!ubcode && (vldbversion != 0)) {
	    memcpy(HostAddress, cheader.IpMappedAddr,
		   sizeof(cheader.IpMappedAddr));
	    for (i = 0; i < MAXSERVERID + 1; i++) {	/* cvt HostAddress to host order */
		HostAddress[i] = ntohl(HostAddress[i]);
	    }

	    code = readExtents(trans);
	    if (code)
		ERROR_EXIT(code);
	}
    }

    vldbversion = ntohl(cheader.vital_header.vldbversion);

    /* now, if can't read, or header is wrong, write a new header */
    if (ubcode || vldbversion == 0) {
	if (builddb) {
	    printf("Can't read VLDB header, re-initialising...\n");

	    /* try to write a good header */
	    memset(&cheader, 0, sizeof(cheader));
	    cheader.vital_header.vldbversion = htonl(VLDBVERSION);
	    cheader.vital_header.headersize = htonl(sizeof(cheader));
	    /* DANGER: Must get this from a master place!! */
	    cheader.vital_header.MaxVolumeId = htonl(0x20000000);
	    cheader.vital_header.eofPtr = htonl(sizeof(cheader));
	    for (i = 0; i < MAXSERVERID + 1; i++) {
		cheader.IpMappedAddr[i] = 0;
		HostAddress[i] = 0;
	    }
	    code = vlwrite(trans, 0, (char *)&cheader, sizeof(cheader));
	    if (code) {
		printf("Can't write VLDB header (error = %d)\n", code);
		ERROR_EXIT(VL_IO);
	    }
	} else
	    ERROR_EXIT(VL_EMPTY);
    } else if ((vldbversion != VLDBVERSION) && (vldbversion != OVLDBVERSION)
	       && (vldbversion != VLDBVERSION_4)) {
	printf
	    ("VLDB version %d doesn't match this software version(%d, %d or %d), quitting!\n",
	     vldbversion, VLDBVERSION_4, VLDBVERSION, OVLDBVERSION);
	ERROR_EXIT(VL_BADVERSION);
    }

    maxnservers = ((vldbversion == 3 || vldbversion == 4) ? 13 : 8);

  error_exit:
    /* all done */
    return error;
}


afs_int32
GetExtentBlock(trans, base)
     register struct ubik_trans *trans;
     register afs_int32 base;
{
    afs_int32 blockindex, code, error = 0;

    /* Base 0 must exist before any other can be created */
    if ((base != 0) && !ex_addr[0])
	ERROR_EXIT(VL_CREATEFAIL);	/* internal error */

    if (!ex_addr[0] || !ex_addr[0]->ex_contaddrs[base]) {
	/* Create a new extension block */
	if (!ex_addr[base]) {
	    ex_addr[base] = (struct extentaddr *)malloc(VL_ADDREXTBLK_SIZE);
	    if (!ex_addr[base])
		ERROR_EXIT(VL_NOMEM);
	}
	memset((char *)ex_addr[base], 0, VL_ADDREXTBLK_SIZE);

	/* Write the full extension block at end of vldb */
	ex_addr[base]->ex_flags = htonl(VLCONTBLOCK);
	blockindex = ntohl(cheader.vital_header.eofPtr);
	code =
	    vlwrite(trans, blockindex, (char *)ex_addr[base],
		    VL_ADDREXTBLK_SIZE);
	if (code)
	    ERROR_EXIT(VL_IO);

	/* Update the cheader.vitalheader structure on disk */
	cheader.vital_header.eofPtr = blockindex + VL_ADDREXTBLK_SIZE;
	cheader.vital_header.eofPtr = htonl(cheader.vital_header.eofPtr);
	code = write_vital_vlheader(trans);
	if (code)
	    ERROR_EXIT(VL_IO);

	/* Write the address of the base extension block in the vldb header */
	if (base == 0) {
	    cheader.SIT = htonl(blockindex);
	    code =
		vlwrite(trans, DOFFSET(0, &cheader, &cheader.SIT),
			(char *)&cheader.SIT, sizeof(cheader.SIT));
	    if (code)
		ERROR_EXIT(VL_IO);
	}

	/* Write the address of this extension block into the base extension block */
	ex_addr[0]->ex_contaddrs[base] = htonl(blockindex);
	code =
	    vlwrite(trans, ntohl(cheader.SIT), ex_addr[0],
		    sizeof(struct extentaddr));
	if (code)
	    ERROR_EXIT(VL_IO);
    }

  error_exit:
    return error;
}


afs_int32
FindExtentBlock(trans, uuidp, createit, hostslot, expp, basep)
     register struct ubik_trans *trans;
     afsUUID *uuidp;
     afs_int32 createit, hostslot, *basep;
     struct extentaddr **expp;
{
    afsUUID tuuid;
    struct extentaddr *exp;
    register afs_int32 i, j, code, base, index, error = 0;

    *expp = NULL;
    *basep = 0;

    /* Create the first extension block if it does not exist */
    if (!cheader.SIT) {
	code = GetExtentBlock(trans, 0);
	if (code)
	    ERROR_EXIT(code);
    }

    for (i = 0; i < MAXSERVERID + 1; i++) {
	if ((HostAddress[i] & 0xff000000) == 0xff000000) {
	    if ((base = (HostAddress[i] >> 16) & 0xff) > VL_MAX_ADDREXTBLKS) {
		ERROR_EXIT(VL_INDEXERANGE);
	    }
	    if ((index = HostAddress[i] & 0x0000ffff) > VL_MHSRV_PERBLK) {
		ERROR_EXIT(VL_INDEXERANGE);
	    }
	    exp = &ex_addr[base][index];
	    tuuid = exp->ex_hostuuid;
	    afs_ntohuuid(&tuuid);
	    if (afs_uuid_equal(uuidp, &tuuid)) {
		*expp = exp;
		*basep = base;
		ERROR_EXIT(0);
	    }
	}
    }

    if (createit) {
	if (hostslot == -1) {
	    for (i = 0; i < MAXSERVERID + 1; i++) {
		if (!HostAddress[i])
		    break;
	    }
	    if (i > MAXSERVERID)
		ERROR_EXIT(VL_REPSFULL);
	} else {
	    i = hostslot;
	}

	for (base = 0; base < VL_MAX_ADDREXTBLKS; base++) {
	    if (!ex_addr[0]->ex_contaddrs[base]) {
		code = GetExtentBlock(trans, base);
		if (code)
		    ERROR_EXIT(code);
	    }
	    for (j = 1; j < VL_MHSRV_PERBLK; j++) {
		exp = &ex_addr[base][j];
		tuuid = exp->ex_hostuuid;
		afs_ntohuuid(&tuuid);
		if (afs_uuid_is_nil(&tuuid)) {
		    tuuid = *uuidp;
		    afs_htonuuid(&tuuid);
		    exp->ex_hostuuid = tuuid;
		    code =
			vlwrite(trans,
				DOFFSET(ntohl(ex_addr[0]->ex_contaddrs[base]),
					(char *)ex_addr[base], (char *)exp),
				(char *)&tuuid, sizeof(tuuid));
		    if (code)
			ERROR_EXIT(VL_IO);
		    HostAddress[i] =
			0xff000000 | ((base << 16) & 0xff0000) | (j & 0xffff);
		    *expp = exp;
		    *basep = base;
		    if (vldbversion != VLDBVERSION_4) {
			cheader.vital_header.vldbversion =
			    htonl(VLDBVERSION_4);
			code = write_vital_vlheader(trans);
			if (code)
			    ERROR_EXIT(VL_IO);
		    }
		    cheader.IpMappedAddr[i] = htonl(HostAddress[i]);
		    code =
			vlwrite(trans,
				DOFFSET(0, &cheader,
					&cheader.IpMappedAddr[i]),
				(char *)&cheader.IpMappedAddr[i],
				sizeof(afs_int32));
		    if (code)
			ERROR_EXIT(VL_IO);
		    ERROR_EXIT(0);
		}
	    }
	}
	ERROR_EXIT(VL_REPSFULL);	/* No reason to utilize a new error code */
    }

  error_exit:
    return error;
}

/* Allocate a free block of storage for entry, returning address of a new
   zeroed entry (or zero if something is wrong).  */
afs_int32
AllocBlock(trans, tentry)
     register struct ubik_trans *trans;
     struct nvlentry *tentry;
{
    register afs_int32 blockindex;

    if (cheader.vital_header.freePtr) {
	/* allocate this dude */
	blockindex = ntohl(cheader.vital_header.freePtr);
	if (vlentryread(trans, blockindex, (char *)tentry, sizeof(vlentry)))
	    return 0;
	cheader.vital_header.freePtr = htonl(tentry->nextIdHash[0]);
    } else {
	/* hosed, nothing on free list, grow file */
	blockindex = ntohl(cheader.vital_header.eofPtr);	/* remember this guy */
	cheader.vital_header.eofPtr = htonl(blockindex + sizeof(vlentry));
    }
    cheader.vital_header.allocs++;
    if (write_vital_vlheader(trans))
	return 0;
    memset(tentry, 0, sizeof(nvlentry));	/* zero new entry */
    return blockindex;
}


/* Free a block given its index.  It must already have been unthreaded. Returns zero for success or an error code on failure. */
int
FreeBlock(trans, blockindex)
     struct ubik_trans *trans;
     afs_int32 blockindex;
{
    struct nvlentry tentry;

    /* check validity of blockindex just to be on the safe side */
    if (!index_OK(trans, blockindex))
	return VL_BADINDEX;
    memset(&tentry, 0, sizeof(nvlentry));
    tentry.nextIdHash[0] = cheader.vital_header.freePtr;	/* already in network order */
    tentry.flags = htonl(VLFREE);
    cheader.vital_header.freePtr = htonl(blockindex);
    if (vlwrite(trans, blockindex, (char *)&tentry, sizeof(nvlentry)))
	return VL_IO;
    cheader.vital_header.frees++;
    if (write_vital_vlheader(trans))
	return VL_IO;
    return 0;
}


/* Look for a block by volid and voltype (if not known use -1 which searches all 3 volid hash lists. Note that the linked lists are read in first from the database header.  If found read the block's contents into the area pointed to by tentry and return the block's index.  If not found return 0. */
afs_int32
FindByID(trans, volid, voltype, tentry, error)
     struct ubik_trans *trans;
     afs_int32 volid;
     afs_int32 voltype;
     struct nvlentry *tentry;
     afs_int32 *error;
{
    register afs_int32 typeindex, hashindex, blockindex;

    *error = 0;
    hashindex = IDHash(volid);
    if (voltype == -1) {
/* Should we have one big hash table for volids as opposed to the three ones? */
	for (typeindex = 0; typeindex < MAXTYPES; typeindex++) {
	    for (blockindex = ntohl(cheader.VolidHash[typeindex][hashindex]);
		 blockindex != NULLO;
		 blockindex = tentry->nextIdHash[typeindex]) {
		if (vlentryread
		    (trans, blockindex, (char *)tentry, sizeof(nvlentry))) {
		    *error = VL_IO;
		    return 0;
		}
		if (volid == tentry->volumeId[typeindex])
		    return blockindex;
	    }
	}
    } else {
	for (blockindex = ntohl(cheader.VolidHash[voltype][hashindex]);
	     blockindex != NULLO; blockindex = tentry->nextIdHash[voltype]) {
	    if (vlentryread
		(trans, blockindex, (char *)tentry, sizeof(nvlentry))) {
		*error = VL_IO;
		return 0;
	    }
	    if (volid == tentry->volumeId[voltype])
		return blockindex;
	}
    }
    return 0;			/* no such entry */
}


/* Look for a block by volume name. If found read the block's contents into the area pointed to by tentry and return the block's index.  If not found return 0. */
afs_int32
FindByName(trans, volname, tentry, error)
     struct ubik_trans *trans;
     char *volname;
     struct nvlentry *tentry;
     afs_int32 *error;
{
    register afs_int32 hashindex;
    register afs_int32 blockindex;
    char tname[VL_MAXNAMELEN];

    /* remove .backup or .readonly extensions for stupid backwards compatibility */
    hashindex = strlen(volname);	/* really string length */
    if (hashindex >= 8 && strcmp(volname + hashindex - 7, ".backup") == 0) {
	/* this is a backup volume */
	strcpy(tname, volname);
	tname[hashindex - 7] = 0;	/* zap extension */
    } else if (hashindex >= 10
	       && strcmp(volname + hashindex - 9, ".readonly") == 0) {
	/* this is a readonly volume */
	strcpy(tname, volname);
	tname[hashindex - 9] = 0;	/* zap extension */
    } else
	strcpy(tname, volname);

    *error = 0;
    hashindex = NameHash(tname);
    for (blockindex = ntohl(cheader.VolnameHash[hashindex]);
	 blockindex != NULLO; blockindex = tentry->nextNameHash) {
	if (vlentryread(trans, blockindex, (char *)tentry, sizeof(nvlentry))) {
	    *error = VL_IO;
	    return 0;
	}
	if (!strcmp(tname, tentry->name))
	    return blockindex;
    }
    return 0;			/* no such entry */
}

int
HashNDump(trans, hashindex)
     struct ubik_trans *trans;
     int hashindex;

{
    register int i = 0;
    register int blockindex;
    struct nvlentry tentry;

    for (blockindex = ntohl(cheader.VolnameHash[hashindex]);
	 blockindex != NULLO; blockindex = tentry.nextNameHash) {
	if (vlentryread(trans, blockindex, (char *)&tentry, sizeof(nvlentry)))
	    return 0;
	i++;
	VLog(0,
	     ("[%d]#%d: %10d %d %d (%s)\n", hashindex, i, tentry.volumeId[0],
	      tentry.nextIdHash[0], tentry.nextNameHash, tentry.name));
    }
    return 0;
}


int
HashIdDump(trans, hashindex)
     struct ubik_trans *trans;
     int hashindex;

{
    register int i = 0;
    register int blockindex;
    struct nvlentry tentry;

    for (blockindex = ntohl(cheader.VolidHash[0][hashindex]);
	 blockindex != NULLO; blockindex = tentry.nextIdHash[0]) {
	if (vlentryread(trans, blockindex, (char *)&tentry, sizeof(nvlentry)))
	    return 0;
	i++;
	VLog(0,
	     ("[%d]#%d: %10d %d %d (%s)\n", hashindex, i, tentry.volumeId[0],
	      tentry.nextIdHash[0], tentry.nextNameHash, tentry.name));
    }
    return 0;
}


/* Add a block to the hash table given a pointer to the block and its index. The block is threaded onto both hash tables and written to disk.  The routine returns zero if there were no errors. */
int
ThreadVLentry(trans, blockindex, tentry)
     struct ubik_trans *trans;
     afs_int32 blockindex;
     struct nvlentry *tentry;
{
    int errorcode;

    if (!index_OK(trans, blockindex))
	return VL_BADINDEX;
    /* Insert into volid's hash linked list */
    if (errorcode = HashVolid(trans, RWVOL, blockindex, tentry))
	return errorcode;

    /* For rw entries we also enter the RO and BACK volume ids (if they exist) in the hash tables; note all there volids (RW, RO, BACK) should not be hashed yet! */
    if (tentry->volumeId[ROVOL]) {
	if (errorcode = HashVolid(trans, ROVOL, blockindex, tentry))
	    return errorcode;
    }
    if (tentry->volumeId[BACKVOL]) {
	if (errorcode = HashVolid(trans, BACKVOL, blockindex, tentry))
	    return errorcode;
    }

    /* Insert into volname's hash linked list */
    HashVolname(trans, blockindex, tentry);

    /* Update cheader entry */
    if (write_vital_vlheader(trans))
	return VL_IO;

    /* Update hash list pointers in the entry itself */
    if (vlentrywrite(trans, blockindex, (char *)tentry, sizeof(nvlentry)))
	return VL_IO;
    return 0;
}


/* Remove a block from both the hash tables.  If success return 0, else return an error code. */
int
UnthreadVLentry(trans, blockindex, aentry)
     struct ubik_trans *trans;
     afs_int32 blockindex;
     struct nvlentry *aentry;
{
    register afs_int32 errorcode, typeindex;

    if (!index_OK(trans, blockindex))
	return VL_BADINDEX;
    if (errorcode = UnhashVolid(trans, RWVOL, blockindex, aentry))
	return errorcode;

    /* Take the RO/RW entries of their respective hash linked lists. */
    for (typeindex = ROVOL; typeindex <= BACKVOL; typeindex++) {
	if (errorcode = UnhashVolid(trans, typeindex, blockindex, aentry))
	    return errorcode;
    }

    /* Take it out of the Volname hash list */
    if (errorcode = UnhashVolname(trans, blockindex, aentry))
	return errorcode;

    /* Update cheader entry */
    write_vital_vlheader(trans);

    return 0;
}

/* cheader must have be read before this routine is called. */
int
HashVolid(trans, voltype, blockindex, tentry)
     struct ubik_trans *trans;
     afs_int32 voltype;
     afs_int32 blockindex;
     struct nvlentry *tentry;
{
    afs_int32 hashindex, errorcode;
    struct vlentry ventry;

    if (FindByID
	(trans, tentry->volumeId[voltype], voltype, &ventry, &errorcode))
	return VL_IDALREADYHASHED;
    else if (errorcode)
	return errorcode;
    hashindex = IDHash(tentry->volumeId[voltype]);
    tentry->nextIdHash[voltype] =
	ntohl(cheader.VolidHash[voltype][hashindex]);
    cheader.VolidHash[voltype][hashindex] = htonl(blockindex);
    if (vlwrite
	(trans, DOFFSET(0, &cheader, &cheader.VolidHash[voltype][hashindex]),
	 (char *)&cheader.VolidHash[voltype][hashindex], sizeof(afs_int32)))
	return VL_IO;
    return 0;
}


/* cheader must have be read before this routine is called. */
int
UnhashVolid(trans, voltype, blockindex, aentry)
     struct ubik_trans *trans;
     afs_int32 voltype;
     afs_int32 blockindex;
     struct nvlentry *aentry;
{
    int hashindex, nextblockindex, prevblockindex;
    struct nvlentry tentry;
    afs_int32 code;
    afs_int32 temp;

    if (aentry->volumeId[voltype] == NULLO)	/* Assume no volume id */
	return 0;
    /* Take it out of the VolId[voltype] hash list */
    hashindex = IDHash(aentry->volumeId[voltype]);
    nextblockindex = ntohl(cheader.VolidHash[voltype][hashindex]);
    if (nextblockindex == blockindex) {
	/* First on the hash list; just adjust pointers */
	cheader.VolidHash[voltype][hashindex] =
	    htonl(aentry->nextIdHash[voltype]);
	code =
	    vlwrite(trans,
		    DOFFSET(0, &cheader,
			    &cheader.VolidHash[voltype][hashindex]),
		    (char *)&cheader.VolidHash[voltype][hashindex],
		    sizeof(afs_int32));
	if (code)
	    return VL_IO;
    } else {
	while (nextblockindex != blockindex) {
	    prevblockindex = nextblockindex;	/* always done once */
	    if (vlentryread
		(trans, nextblockindex, (char *)&tentry, sizeof(nvlentry)))
		return VL_IO;
	    if ((nextblockindex = tentry.nextIdHash[voltype]) == NULLO)
		return VL_NOENT;
	}
	temp = tentry.nextIdHash[voltype] = aentry->nextIdHash[voltype];
	temp = htonl(temp);	/* convert to network byte order before writing */
	if (vlwrite
	    (trans,
	     DOFFSET(prevblockindex, &tentry, &tentry.nextIdHash[voltype]),
	     (char *)&temp, sizeof(afs_int32)))
	    return VL_IO;
    }
    aentry->nextIdHash[voltype] = 0;
    return 0;
}


int
HashVolname(trans, blockindex, aentry)
     struct ubik_trans *trans;
     afs_int32 blockindex;
     struct nvlentry *aentry;
{
    register afs_int32 hashindex;
    register afs_int32 code;

    /* Insert into volname's hash linked list */
    hashindex = NameHash(aentry->name);
    aentry->nextNameHash = ntohl(cheader.VolnameHash[hashindex]);
    cheader.VolnameHash[hashindex] = htonl(blockindex);
    code =
	vlwrite(trans, DOFFSET(0, &cheader, &cheader.VolnameHash[hashindex]),
		(char *)&cheader.VolnameHash[hashindex], sizeof(afs_int32));
    if (code)
	return VL_IO;
    return 0;
}


int
UnhashVolname(trans, blockindex, aentry)
     struct ubik_trans *trans;
     afs_int32 blockindex;
     struct nvlentry *aentry;
{
    register afs_int32 hashindex, nextblockindex, prevblockindex;
    struct nvlentry tentry;
    afs_int32 temp;

    /* Take it out of the Volname hash list */
    hashindex = NameHash(aentry->name);
    nextblockindex = ntohl(cheader.VolnameHash[hashindex]);
    if (nextblockindex == blockindex) {
	/* First on the hash list; just adjust pointers */
	cheader.VolnameHash[hashindex] = htonl(aentry->nextNameHash);
	if (vlwrite
	    (trans, DOFFSET(0, &cheader, &cheader.VolnameHash[hashindex]),
	     (char *)&cheader.VolnameHash[hashindex], sizeof(afs_int32)))
	    return VL_IO;
    } else {
	while (nextblockindex != blockindex) {
	    prevblockindex = nextblockindex;	/* always done at least once */
	    if (vlentryread
		(trans, nextblockindex, (char *)&tentry, sizeof(nvlentry)))
		return VL_IO;
	    if ((nextblockindex = tentry.nextNameHash) == NULLO)
		return VL_NOENT;
	}
	tentry.nextNameHash = aentry->nextNameHash;
	temp = htonl(tentry.nextNameHash);
	if (vlwrite
	    (trans, DOFFSET(prevblockindex, &tentry, &tentry.nextNameHash),
	     (char *)&temp, sizeof(afs_int32)))
	    return VL_IO;
    }
    aentry->nextNameHash = 0;
    return 0;
}


/* Returns the vldb entry tentry at offset index; remaining is the number of entries left; the routine also returns the index of the next sequential entry in the vldb */

afs_int32
NextEntry(trans, blockindex, tentry, remaining)
     struct ubik_trans *trans;
     afs_int32 blockindex;
     struct nvlentry *tentry;
     afs_int32 *remaining;
{
    register afs_int32 lastblockindex;

    if (blockindex == 0)	/* get first one */
	blockindex = sizeof(cheader);
    else {
	if (!index_OK(trans, blockindex)) {
	    *remaining = -1;	/* error */
	    return 0;
	}
	blockindex += sizeof(nvlentry);
    }
    /* now search for the first entry that isn't free */
    for (lastblockindex = ntohl(cheader.vital_header.eofPtr);
	 blockindex < lastblockindex;) {
	if (vlentryread(trans, blockindex, (char *)tentry, sizeof(nvlentry))) {
	    *remaining = -1;
	    return 0;
	}
	if (tentry->flags == VLCONTBLOCK) {
	    /*
	     * This is a special mh extension block just simply skip over it
	     */
	    blockindex += VL_ADDREXTBLK_SIZE;
	} else {
	    if (tentry->flags != VLFREE) {
		/* estimate remaining number of entries, not including this one */
		*remaining =
		    (lastblockindex - blockindex) / sizeof(nvlentry) - 1;
		return blockindex;
	    }
	    blockindex += sizeof(nvlentry);
	}
    }
    *remaining = 0;		/* no more entries */
    return 0;
}


/* Routine to verify that index is a legal offset to a vldb entry in the table */
static int
index_OK(trans, blockindex)
     struct ubik_trans *trans;
     afs_int32 blockindex;
{
    if ((blockindex < sizeof(cheader))
	|| (blockindex >= ntohl(cheader.vital_header.eofPtr)))
	return 0;
    return 1;
}
