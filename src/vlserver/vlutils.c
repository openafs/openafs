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

#include <afs/afs_lock.h>
#include <rx/xdr.h>
#include <ubik.h>

#include "vlserver.h"
#include "vlserver_internal.h"

extern int maxnservers;
extern afs_uint32 rd_HostAddress[MAXSERVERID + 1];
extern afs_uint32 wr_HostAddress[MAXSERVERID + 1];
struct extentaddr *rd_ex_addr[VL_MAX_ADDREXTBLKS] = { 0, 0, 0, 0 };
struct extentaddr *wr_ex_addr[VL_MAX_ADDREXTBLKS] = { 0, 0, 0, 0 };
struct vlheader rd_cheader;	/* kept in network byte order */
struct vlheader wr_cheader;
int vldbversion = 0;

static int index_OK(struct vl_ctx *ctx, afs_int32 blockindex);

#define ERROR_EXIT(code) do { \
    error = (code); \
    goto error_exit; \
} while (0)

/* Hashing algorithm based on the volume id; HASHSIZE must be prime */
afs_int32
IDHash(afs_int32 volumeid)
{
    return ((abs(volumeid)) % HASHSIZE);
}


/* Hashing algorithm based on the volume name; name's size is implicit (64 chars) and if changed it should be reflected here. */
afs_int32
NameHash(char *volumename)
{
    unsigned int hash;
    int i;

    hash = 0;
    for (i = strlen(volumename), volumename += i - 1; i--; volumename--)
	hash = (hash * 63) + (*((unsigned char *)volumename) - 63);
    return (hash % HASHSIZE);
}


/* package up seek and write into one procedure for ease of use */
afs_int32
vlwrite(struct ubik_trans *trans, afs_int32 offset, void *buffer,
	afs_int32 length)
{
    afs_int32 errorcode;

    if ((errorcode = ubik_Seek(trans, 0, offset)))
	return errorcode;
    return (ubik_Write(trans, buffer, length));
}


/* Package up seek and read into one procedure for ease of use */
afs_int32
vlread(struct ubik_trans *trans, afs_int32 offset, char *buffer,
       afs_int32 length)
{
    afs_int32 errorcode;

    if ((errorcode = ubik_Seek(trans, 0, offset)))
	return errorcode;
    return (ubik_Read(trans, buffer, length));
}


/* take entry and convert to network order and write to disk */
afs_int32
vlentrywrite(struct ubik_trans *trans, afs_int32 offset, void *buffer,
	     afs_int32 length)
{
    struct vlentry oentry;
    struct nvlentry nentry, *nep;
    char *bufp;
    afs_int32 i;

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
vlentryread(struct ubik_trans *trans, afs_int32 offset, char *buffer,
	    afs_int32 length)
{
    struct vlentry *oep, tentry;
    struct nvlentry *nep, *nbufp;
    char *bufp = (char *)&tentry;
    afs_int32 i;

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
	memcpy(nbufp->serverNumber, oep->serverNumber, OMAXNSERVERS);
	memcpy(nbufp->serverPartition, oep->serverPartition, OMAXNSERVERS);
	memcpy(nbufp->serverFlags, oep->serverFlags, OMAXNSERVERS);
	/* initilize the last elements to BADSERVERID */
	for (i = OMAXNSERVERS; i < NMAXNSERVERS; i++) {
	    nbufp->serverNumber[i] = BADSERVERID;
	    nbufp->serverPartition[i] = BADSERVERID;
	    nbufp->serverFlags[i] = BADSERVERID;
	}
    }
    return 0;
}

/* Convenient write of small critical vldb header info to the database. */
int
write_vital_vlheader(struct vl_ctx *ctx)
{
    if (vlwrite
	(ctx->trans, 0, (char *)&ctx->cheader->vital_header, sizeof(vital_vlheader)))
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
readExtents(struct ubik_trans *trans)
{
    afs_uint32 extentAddr;
    afs_int32 error = 0, code;
    int i;

    extent_mod = 0;
    extentAddr = ntohl(rd_cheader.SIT);
    if (!extentAddr)
	return 0;

    /* Read the first extension block */
    if (!rd_ex_addr[0]) {
	rd_ex_addr[0] = malloc(VL_ADDREXTBLK_SIZE);
	if (!rd_ex_addr[0])
	    ERROR_EXIT(VL_NOMEM);
    }
    code = vlread(trans, extentAddr, (char *)rd_ex_addr[0], VL_ADDREXTBLK_SIZE);
    if (code) {
	free(rd_ex_addr[0]);	/* Not the place to create it */
	rd_ex_addr[0] = 0;
	ERROR_EXIT(VL_IO);
    }

    /* In case more that 64 mh servers are in use they're kept in these
     * continuation blocks
     */
    for (i = 1; i < VL_MAX_ADDREXTBLKS; i++) {
	if (!rd_ex_addr[0]->ex_contaddrs[i])
	    continue;

	/* Before reading it in, check to see if the address is good */
	if ((ntohl(rd_ex_addr[0]->ex_contaddrs[i]) <
	     ntohl(rd_ex_addr[0]->ex_contaddrs[i - 1]) + VL_ADDREXTBLK_SIZE)
	    || (ntohl(rd_ex_addr[0]->ex_contaddrs[i]) >
		ntohl(rd_cheader.vital_header.eofPtr) - VL_ADDREXTBLK_SIZE)) {
	    extent_mod = 1;
	    rd_ex_addr[0]->ex_contaddrs[i] = 0;
	    continue;
	}


	/* Read the continuation block */
	if (!rd_ex_addr[i]) {
	    rd_ex_addr[i] = malloc(VL_ADDREXTBLK_SIZE);
	    if (!rd_ex_addr[i])
		ERROR_EXIT(VL_NOMEM);
	}
	code =
	    vlread(trans, ntohl(rd_ex_addr[0]->ex_contaddrs[i]),
		   (char *)rd_ex_addr[i], VL_ADDREXTBLK_SIZE);
	if (code) {
	    free(rd_ex_addr[i]);	/* Not the place to create it */
	    rd_ex_addr[i] = 0;
	    ERROR_EXIT(VL_IO);
	}

	/* After reading it in, check to see if its a real continuation block */
	if (ntohl(rd_ex_addr[i]->ex_hdrflags) != VLCONTBLOCK) {
	    extent_mod = 1;
	    rd_ex_addr[0]->ex_contaddrs[i] = 0;
	    free(rd_ex_addr[i]);	/* Not the place to create it */
	    rd_ex_addr[i] = 0;
	    continue;
	}
    }

    if (extent_mod) {
	code = vlwrite(trans, extentAddr, rd_ex_addr[0], VL_ADDREXTBLK_SIZE);
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
/**
 * reads in db cache from ubik.
 *
 * @param[in] ut ubik transaction
 * @param[in] rock  opaque pointer to an int*; if 1, we should rebuild the db
 *                  if it appears empty, if 0 we should return an error if the
 *                  db appears empty
 *
 * @return operation status
 *   @retval 0 success
 */
static afs_int32
UpdateCache(struct ubik_trans *trans, void *rock)
{
    int *builddb_rock = rock;
    int builddb = *builddb_rock;
    afs_int32 error = 0, i, code, ubcode;

    /* if version changed (or first call), read the header */
    ubcode = vlread(trans, 0, (char *)&rd_cheader, sizeof(rd_cheader));
    vldbversion = ntohl(rd_cheader.vital_header.vldbversion);

    if (!ubcode && (vldbversion != 0)) {
	memcpy(rd_HostAddress, rd_cheader.IpMappedAddr, sizeof(rd_cheader.IpMappedAddr));
	for (i = 0; i < MAXSERVERID + 1; i++) {	/* cvt HostAddress to host order */
	    rd_HostAddress[i] = ntohl(rd_HostAddress[i]);
	}

	code = readExtents(trans);
	if (code)
	    ERROR_EXIT(code);
    }

    /* now, if can't read, or header is wrong, write a new header */
    if (ubcode || vldbversion == 0) {
	if (builddb) {
	    VLog(0, ("Can't read VLDB header, re-initialising...\n"));

	    /* try to write a good header */
	    /* The read cache will be sync'ed to this new header
	     * when the ubik transaction is ended by vlsynccache(). */
	    memset(&wr_cheader, 0, sizeof(wr_cheader));
	    wr_cheader.vital_header.vldbversion = htonl(VLDBVERSION);
	    wr_cheader.vital_header.headersize = htonl(sizeof(wr_cheader));
	    /* DANGER: Must get this from a master place!! */
	    wr_cheader.vital_header.MaxVolumeId = htonl(0x20000000);
	    wr_cheader.vital_header.eofPtr = htonl(sizeof(wr_cheader));
	    for (i = 0; i < MAXSERVERID + 1; i++) {
		wr_cheader.IpMappedAddr[i] = 0;
		wr_HostAddress[i] = 0;
	    }
	    code = vlwrite(trans, 0, (char *)&wr_cheader, sizeof(wr_cheader));
	    if (code) {
		VLog(0, ("Can't write VLDB header (error = %d)\n", code));
		ERROR_EXIT(VL_IO);
	    }
	    vldbversion = ntohl(wr_cheader.vital_header.vldbversion);
	} else {
	    VLog(1, ("Unable to read VLDB header.\n"));
	    ERROR_EXIT(VL_EMPTY);
	}
    }

    if ((vldbversion != VLDBVERSION) && (vldbversion != OVLDBVERSION)
        && (vldbversion != VLDBVERSION_4)) {
	VLog(0,
	    ("VLDB version %d doesn't match this software version(%d, %d or %d), quitting!\n",
	     vldbversion, VLDBVERSION_4, VLDBVERSION, OVLDBVERSION));
	ERROR_EXIT(VL_BADVERSION);
    }

    maxnservers = ((vldbversion == 3 || vldbversion == 4) ? 13 : 8);

  error_exit:
    /* all done */
    return error;
}

afs_int32
CheckInit(struct ubik_trans *trans, int builddb)
{
    afs_int32 code;

    code = ubik_CheckCache(trans, UpdateCache, &builddb);
    if (code) {
	return code;
    }

    /* these next two cases shouldn't happen (UpdateCache should either
     * rebuild the db or return an error if these cases occur), but just to
     * be on the safe side... */
    if (vldbversion == 0) {
	return VL_EMPTY;
    }
    if ((vldbversion != VLDBVERSION) && (vldbversion != OVLDBVERSION)
        && (vldbversion != VLDBVERSION_4)) {
	return VL_BADVERSION;
    }

    return 0;
}

/**
 * Grow the eofPtr in the header by 'bump' bytes.
 *
 * @param[inout] cheader    VL header
 * @param[in] bump	    How many bytes to add to eofPtr
 * @param[out] a_blockindex On success, set to the original eofPtr before we
 *			    bumped it
 * @return VL error codes
 */
static afs_int32
grow_eofPtr(struct vlheader *cheader, afs_int32 bump, afs_int32 *a_blockindex)
{
    afs_int32 blockindex = ntohl(cheader->vital_header.eofPtr);

    if (blockindex < 0 || blockindex >= MAX_AFS_INT32 - bump) {
	VLog(0, ("Error: Tried to grow the VLDB beyond the 2GiB limit. Either "
		 "find a way to trim down your VLDB, or upgrade to a release "
		 "and database format that supports a larger VLDB.\n"));
	return VL_IO;
    }

    *a_blockindex = blockindex;
    cheader->vital_header.eofPtr = htonl(blockindex + bump);
    return 0;
}

afs_int32
GetExtentBlock(struct vl_ctx *ctx, afs_int32 base)
{
    afs_int32 blockindex, code, error = 0;

    /* Base 0 must exist before any other can be created */
    if ((base != 0) && !ctx->ex_addr[0])
	ERROR_EXIT(VL_CREATEFAIL);	/* internal error */

    if (!ctx->ex_addr[0] || !ctx->ex_addr[0]->ex_contaddrs[base]) {
	/* Create a new extension block */
	if (!ctx->ex_addr[base]) {
	    ctx->ex_addr[base] = malloc(VL_ADDREXTBLK_SIZE);
	    if (!ctx->ex_addr[base])
		ERROR_EXIT(VL_NOMEM);
	}
	memset(ctx->ex_addr[base], 0, VL_ADDREXTBLK_SIZE);

	/* Write the full extension block at end of vldb */
	ctx->ex_addr[base]->ex_hdrflags = htonl(VLCONTBLOCK);
	code = grow_eofPtr(ctx->cheader, VL_ADDREXTBLK_SIZE, &blockindex);
	if (code)
	    ERROR_EXIT(VL_IO);

	code =
	    vlwrite(ctx->trans, blockindex, (char *)ctx->ex_addr[base],
		    VL_ADDREXTBLK_SIZE);
	if (code)
	    ERROR_EXIT(VL_IO);


	code = write_vital_vlheader(ctx);
	if (code)
	    ERROR_EXIT(VL_IO);

	/* Write the address of the base extension block in the vldb header */
	if (base == 0) {
	    ctx->cheader->SIT = htonl(blockindex);
	    code =
		vlwrite(ctx->trans, DOFFSET(0, ctx->cheader, &ctx->cheader->SIT),
			(char *)&ctx->cheader->SIT, sizeof(ctx->cheader->SIT));
	    if (code)
		ERROR_EXIT(VL_IO);
	}

	/* Write the address of this extension block into the base extension block */
	ctx->ex_addr[0]->ex_contaddrs[base] = htonl(blockindex);
	code =
	    vlwrite(ctx->trans, ntohl(ctx->cheader->SIT), ctx->ex_addr[0],
		    sizeof(struct extentaddr));
	if (code)
	    ERROR_EXIT(VL_IO);
    }

  error_exit:
    return error;
}


afs_int32
FindExtentBlock(struct vl_ctx *ctx, afsUUID *uuidp,
		afs_int32 createit, afs_int32 hostslot,
		struct extentaddr **expp, afs_int32 *basep)
{
    afsUUID tuuid;
    struct extentaddr *exp;
    afs_int32 i, j, code, base, index, error = 0;

    *expp = NULL;
    *basep = 0;

    /* Create the first extension block if it does not exist */
    if (!ctx->cheader->SIT) {
	code = GetExtentBlock(ctx, 0);
	if (code)
	    ERROR_EXIT(code);
    }

    for (i = 0; i < MAXSERVERID + 1; i++) {
	if ((ctx->hostaddress[i] & 0xff000000) == 0xff000000) {
	    if ((base = (ctx->hostaddress[i] >> 16) & 0xff) > VL_MAX_ADDREXTBLKS) {
		ERROR_EXIT(VL_INDEXERANGE);
	    }
	    if ((index = ctx->hostaddress[i] & 0x0000ffff) > VL_MHSRV_PERBLK) {
		ERROR_EXIT(VL_INDEXERANGE);
	    }
	    exp = &ctx->ex_addr[base][index];
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
		if (!ctx->hostaddress[i])
		    break;
	    }
	    if (i > MAXSERVERID)
		ERROR_EXIT(VL_REPSFULL);
	} else {
	    i = hostslot;
	}

	for (base = 0; base < VL_MAX_ADDREXTBLKS; base++) {
	    if (!ctx->ex_addr[0]->ex_contaddrs[base]) {
		code = GetExtentBlock(ctx, base);
		if (code)
		    ERROR_EXIT(code);
	    }
	    for (j = 1; j < VL_MHSRV_PERBLK; j++) {
		exp = &ctx->ex_addr[base][j];
		tuuid = exp->ex_hostuuid;
		afs_ntohuuid(&tuuid);
		if (afs_uuid_is_nil(&tuuid)) {
		    tuuid = *uuidp;
		    afs_htonuuid(&tuuid);
		    exp->ex_hostuuid = tuuid;
		    code =
			vlwrite(ctx->trans,
				DOFFSET(ntohl(ctx->ex_addr[0]->ex_contaddrs[base]),
					(char *)ctx->ex_addr[base], (char *)exp),
				(char *)&tuuid, sizeof(tuuid));
		    if (code)
			ERROR_EXIT(VL_IO);
		    ctx->hostaddress[i] =
			0xff000000 | ((base << 16) & 0xff0000) | (j & 0xffff);
		    *expp = exp;
		    *basep = base;
		    if (vldbversion != VLDBVERSION_4) {
			ctx->cheader->vital_header.vldbversion =
			    htonl(VLDBVERSION_4);
			code = write_vital_vlheader(ctx);
			if (code)
			    ERROR_EXIT(VL_IO);
		    }
		    ctx->cheader->IpMappedAddr[i] = htonl(ctx->hostaddress[i]);
		    code =
			vlwrite(ctx->trans,
				DOFFSET(0, ctx->cheader,
					&ctx->cheader->IpMappedAddr[i]),
				(char *)&ctx->cheader->IpMappedAddr[i],
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
AllocBlock(struct vl_ctx *ctx, struct nvlentry *tentry)
{
    afs_int32 blockindex;

    if (ctx->cheader->vital_header.freePtr) {
	/* allocate this dude */
	blockindex = ntohl(ctx->cheader->vital_header.freePtr);
	if (vlentryread(ctx->trans, blockindex, (char *)tentry, sizeof(vlentry)))
	    return 0;
	ctx->cheader->vital_header.freePtr = htonl(tentry->nextIdHash[0]);
    } else {
	afs_int32 code;
	/* hosed, nothing on free list, grow file */
	code = grow_eofPtr(ctx->cheader, sizeof(vlentry), &blockindex);
	if (code)
	    return 0;
    }
    ctx->cheader->vital_header.allocs++;
    if (write_vital_vlheader(ctx))
	return 0;
    memset(tentry, 0, sizeof(nvlentry));	/* zero new entry */
    return blockindex;
}


/* Free a block given its index.  It must already have been unthreaded. Returns zero for success or an error code on failure. */
int
FreeBlock(struct vl_ctx *ctx, afs_int32 blockindex)
{
    struct nvlentry tentry;

    /* check validity of blockindex just to be on the safe side */
    if (!index_OK(ctx, blockindex))
	return VL_BADINDEX;
    memset(&tentry, 0, sizeof(nvlentry));
    tentry.nextIdHash[0] = ctx->cheader->vital_header.freePtr;	/* already in network order */
    tentry.flags = htonl(VLFREE);
    ctx->cheader->vital_header.freePtr = htonl(blockindex);
    if (vlwrite(ctx->trans, blockindex, (char *)&tentry, sizeof(nvlentry)))
	return VL_IO;
    ctx->cheader->vital_header.frees++;
    if (write_vital_vlheader(ctx))
	return VL_IO;
    return 0;
}


/* Look for a block by volid and voltype (if not known use -1 which searches
 * all 3 volid hash lists. Note that the linked lists are read in first from
 * the database header.  If found read the block's contents into the area
 * pointed to by tentry and return the block's index.  If not found return 0.
 */
afs_int32
FindByID(struct vl_ctx *ctx, afs_uint32 volid, afs_int32 voltype,
	 struct nvlentry *tentry, afs_int32 *error)
{
    afs_int32 typeindex, hashindex, blockindex;

    *error = 0;
    hashindex = IDHash(volid);
    if (voltype == -1) {
/* Should we have one big hash table for volids as opposed to the three ones? */
	for (typeindex = 0; typeindex < MAXTYPES; typeindex++) {
	    for (blockindex = ntohl(ctx->cheader->VolidHash[typeindex][hashindex]);
		 blockindex != NULLO;
		 blockindex = tentry->nextIdHash[typeindex]) {
		if (vlentryread
		    (ctx->trans, blockindex, (char *)tentry, sizeof(nvlentry))) {
		    *error = VL_IO;
		    return 0;
		}
		if (volid == tentry->volumeId[typeindex])
		    return blockindex;
	    }
	}
    } else {
	for (blockindex = ntohl(ctx->cheader->VolidHash[voltype][hashindex]);
	     blockindex != NULLO; blockindex = tentry->nextIdHash[voltype]) {
	    if (vlentryread
		(ctx->trans, blockindex, (char *)tentry, sizeof(nvlentry))) {
		*error = VL_IO;
		return 0;
	    }
	    if (volid == tentry->volumeId[voltype])
		return blockindex;
	}
    }
    return 0;			/* no such entry */
}


/* Look for a block by volume name. If found read the block's contents into
 * the area pointed to by tentry and return the block's index.  If not
 * found return 0.
 */
afs_int32
FindByName(struct vl_ctx *ctx, char *volname, struct nvlentry *tentry,
	   afs_int32 *error)
{
    afs_int32 hashindex;
    afs_int32 blockindex;
    char tname[VL_MAXNAMELEN];

    /* remove .backup or .readonly extensions for stupid backwards
     * compatibility
     */
    hashindex = strlen(volname);	/* really string length */
    if (hashindex >= 8 && strcmp(volname + hashindex - 7, ".backup") == 0) {
	/* this is a backup volume */
	if (strlcpy(tname, volname, sizeof(tname)) >= sizeof(tname)) {
	    *error = VL_BADNAME;
	    return 0;
	}
	tname[hashindex - 7] = 0;	/* zap extension */
    } else if (hashindex >= 10
	       && strcmp(volname + hashindex - 9, ".readonly") == 0) {
	/* this is a readonly volume */
	if (strlcpy(tname, volname, sizeof(tname)) >= sizeof(tname)) {
	    *error = VL_BADNAME;
	    return 0;
	}
	tname[hashindex - 9] = 0;	/* zap extension */
    } else {
	if (strlcpy(tname, volname, sizeof(tname)) >= sizeof(tname)) {
	    *error = VL_BADNAME;
	    return 0;
	}
    }

    *error = 0;
    hashindex = NameHash(tname);
    for (blockindex = ntohl(ctx->cheader->VolnameHash[hashindex]);
	 blockindex != NULLO; blockindex = tentry->nextNameHash) {
	if (vlentryread(ctx->trans, blockindex, (char *)tentry, sizeof(nvlentry))) {
	    *error = VL_IO;
	    return 0;
	}
	if (!strcmp(tname, tentry->name))
	    return blockindex;
    }
    return 0;			/* no such entry */
}

/**
 * Returns whether or not any of the supplied volume IDs already exist
 * in the vldb.
 *
 * @param ctx      transaction context
 * @param ids      an array of volume IDs
 * @param ids_len  the number of elements in the 'ids' array
 * @param error    filled in with an error code in case of error
 *
 * @return whether any of the volume IDs are already used
 *  @retval 1  at least one of the volume IDs is already used
 *  @retval 0  none of the volume IDs are used, or an error occurred
 */
int
EntryIDExists(struct vl_ctx *ctx, const afs_uint32 *ids,
	      afs_int32 ids_len, afs_int32 *error)
{
    afs_int32 typeindex;
    struct nvlentry tentry;

    *error = 0;

    for (typeindex = 0; typeindex < ids_len; typeindex++) {
	if (ids[typeindex]
	    && FindByID(ctx, ids[typeindex], -1, &tentry, error)) {

	    return 1;
	} else if (*error) {
	    return 0;
	}
    }

    return 0;
}

/**
 * Finds the next range of unused volume IDs in the vldb.
 *
 * @param ctx       transaction context
 * @param maxvolid  the current max vol ID, and where to start looking
 *                  for an unused volume ID range
 * @param bump      how many volume IDs we need to be unused
 * @param error     filled in with an error code in case of error
 *
 * @return the next volume ID 'volid' such that the range
 *         [volid, volid+bump) of volume IDs is unused, or 0 if there's
 *         an error
 */
afs_uint32
NextUnusedID(struct vl_ctx *ctx, afs_uint32 maxvolid, afs_uint32 bump,
	     afs_int32 *error)
{
    struct nvlentry tentry;
    afs_uint32 id;
    afs_uint32 nfree;

    *error = 0;

     /* we simply start at the given maxvolid, keep a running tally of
      * how many free volume IDs we've seen in a row, and return when
      * we've seen 'bump' unused IDs in a row */
    for (id = maxvolid, nfree = 0; nfree < bump; ++id) {
	if (FindByID(ctx, id, -1, &tentry, error)) {
	    nfree = 0;
	} else if (*error) {
	    return 0;
	} else {
	    ++nfree;
	}
    }

    /* 'id' is now at the end of the [maxvolid,maxvolid+bump) range,
     * but we need to return the first unused id, so subtract the
     * number of current running free IDs to get the beginning */
    return id - nfree;
}

int
HashNDump(struct vl_ctx *ctx, int hashindex)
{
    int i = 0;
    int blockindex;
    struct nvlentry tentry;

    for (blockindex = ntohl(ctx->cheader->VolnameHash[hashindex]);
	 blockindex != NULLO; blockindex = tentry.nextNameHash) {
	if (vlentryread(ctx->trans, blockindex, (char *)&tentry, sizeof(nvlentry)))
	    return 0;
	i++;
	VLog(0,
	     ("[%d]#%d: %10d %d %d (%s)\n", hashindex, i, tentry.volumeId[0],
	      tentry.nextIdHash[0], tentry.nextNameHash, tentry.name));
    }
    return 0;
}


int
HashIdDump(struct vl_ctx *ctx, int hashindex)
{
    int i = 0;
    int blockindex;
    struct nvlentry tentry;

    for (blockindex = ntohl(ctx->cheader->VolidHash[0][hashindex]);
	 blockindex != NULLO; blockindex = tentry.nextIdHash[0]) {
	if (vlentryread(ctx->trans, blockindex, (char *)&tentry, sizeof(nvlentry)))
	    return 0;
	i++;
	VLog(0,
	     ("[%d]#%d: %10d %d %d (%s)\n", hashindex, i, tentry.volumeId[0],
	      tentry.nextIdHash[0], tentry.nextNameHash, tentry.name));
    }
    return 0;
}


/* Add a block to the hash table given a pointer to the block and its index.
 * The block is threaded onto both hash tables and written to disk.  The
 * routine returns zero if there were no errors.
 */
int
ThreadVLentry(struct vl_ctx *ctx, afs_int32 blockindex,
	      struct nvlentry *tentry)
{
    int errorcode;

    if (!index_OK(ctx, blockindex))
	return VL_BADINDEX;
    /* Insert into volid's hash linked list */
    if ((errorcode = HashVolid(ctx, RWVOL, blockindex, tentry)))
	return errorcode;

    /* For rw entries we also enter the RO and BACK volume ids (if they
     * exist) in the hash tables; note all there volids (RW, RO, BACK)
     * should not be hashed yet! */
    if (tentry->volumeId[ROVOL]) {
	if ((errorcode = HashVolid(ctx, ROVOL, blockindex, tentry)))
	    return errorcode;
    }
    if (tentry->volumeId[BACKVOL]) {
	if ((errorcode = HashVolid(ctx, BACKVOL, blockindex, tentry)))
	    return errorcode;
    }

    /* Insert into volname's hash linked list */
    HashVolname(ctx, blockindex, tentry);

    /* Update cheader entry */
    if (write_vital_vlheader(ctx))
	return VL_IO;

    /* Update hash list pointers in the entry itself */
    if (vlentrywrite(ctx->trans, blockindex, (char *)tentry, sizeof(nvlentry)))
	return VL_IO;
    return 0;
}


/* Remove a block from both the hash tables.  If success return 0, else
 * return an error code. */
int
UnthreadVLentry(struct vl_ctx *ctx, afs_int32 blockindex,
		struct nvlentry *aentry)
{
    afs_int32 errorcode, typeindex;

    if (!index_OK(ctx, blockindex))
	return VL_BADINDEX;
    if ((errorcode = UnhashVolid(ctx, RWVOL, blockindex, aentry)))
	return errorcode;

    /* Take the RO/RW entries of their respective hash linked lists. */
    for (typeindex = ROVOL; typeindex <= BACKVOL; typeindex++) {
	if ((errorcode = UnhashVolid(ctx, typeindex, blockindex, aentry))) {
	    if (errorcode == VL_NOENT) {
		VLog(0, ("Unable to unhash vlentry '%s' (address %d) from hash "
			 "chain for volid %u (type %d).\n",
			 aentry->name, blockindex, aentry->volumeId[typeindex], typeindex));
		VLog(0, ("... The VLDB may be partly corrupted; see vldb_check "
			 "for how to check for and fix errors.\n"));
		return VL_DBBAD;
	    }
	    return errorcode;
	}
    }

    /* Take it out of the Volname hash list */
    if ((errorcode = UnhashVolname(ctx, blockindex, aentry))) {
	if (errorcode == VL_NOENT) {
	    VLog(0, ("Unable to unhash vlentry '%s' (address %d) from name "
		     "hash chain.\n", aentry->name, blockindex));
	    VLog(0, ("... The VLDB may be partly corrupted; see vldb_check "
		     "for how to check for and fix errors.\n"));
	    return VL_DBBAD;
	}
	return errorcode;
    }

    /* Update cheader entry */
    write_vital_vlheader(ctx);

    return 0;
}

/* cheader must have be read before this routine is called. */
int
HashVolid(struct vl_ctx *ctx, afs_int32 voltype, afs_int32 blockindex,
          struct nvlentry *tentry)
{
    afs_int32 hashindex, errorcode;
    struct nvlentry ventry;

    if (FindByID
	(ctx, tentry->volumeId[voltype], voltype, &ventry, &errorcode))
	return VL_IDALREADYHASHED;
    else if (errorcode)
	return errorcode;
    hashindex = IDHash(tentry->volumeId[voltype]);
    tentry->nextIdHash[voltype] =
	ntohl(ctx->cheader->VolidHash[voltype][hashindex]);
    ctx->cheader->VolidHash[voltype][hashindex] = htonl(blockindex);
    if (vlwrite
	(ctx->trans, DOFFSET(0, ctx->cheader, &ctx->cheader->VolidHash[voltype][hashindex]),
	 (char *)&ctx->cheader->VolidHash[voltype][hashindex], sizeof(afs_int32)))
	return VL_IO;
    return 0;
}


/* cheader must have be read before this routine is called. */
int
UnhashVolid(struct vl_ctx *ctx, afs_int32 voltype, afs_int32 blockindex,
	    struct nvlentry *aentry)
{
    int hashindex, nextblockindex, prevblockindex;
    struct nvlentry tentry;
    afs_int32 code;
    afs_int32 temp;

    if (aentry->volumeId[voltype] == NULLO)	/* Assume no volume id */
	return 0;
    /* Take it out of the VolId[voltype] hash list */
    hashindex = IDHash(aentry->volumeId[voltype]);
    nextblockindex = ntohl(ctx->cheader->VolidHash[voltype][hashindex]);
    if (nextblockindex == blockindex) {
	/* First on the hash list; just adjust pointers */
	ctx->cheader->VolidHash[voltype][hashindex] =
	    htonl(aentry->nextIdHash[voltype]);
	code =
	    vlwrite(ctx->trans,
		    DOFFSET(0, ctx->cheader,
			    &ctx->cheader->VolidHash[voltype][hashindex]),
		    (char *)&ctx->cheader->VolidHash[voltype][hashindex],
		    sizeof(afs_int32));
	if (code)
	    return VL_IO;
    } else {
	while (nextblockindex != blockindex) {
	    prevblockindex = nextblockindex;	/* always done once */
	    if (vlentryread
		(ctx->trans, nextblockindex, (char *)&tentry, sizeof(nvlentry)))
		return VL_IO;
	    if ((nextblockindex = tentry.nextIdHash[voltype]) == NULLO)
		return VL_NOENT;
	}
	temp = tentry.nextIdHash[voltype] = aentry->nextIdHash[voltype];
	temp = htonl(temp);	/* convert to network byte order before writing */
	if (vlwrite
	    (ctx->trans,
	     DOFFSET(prevblockindex, &tentry, &tentry.nextIdHash[voltype]),
	     (char *)&temp, sizeof(afs_int32)))
	    return VL_IO;
    }
    aentry->nextIdHash[voltype] = 0;
    return 0;
}


int
HashVolname(struct vl_ctx *ctx, afs_int32 blockindex,
	    struct nvlentry *aentry)
{
    afs_int32 hashindex;
    afs_int32 code;

    /* Insert into volname's hash linked list */
    hashindex = NameHash(aentry->name);
    aentry->nextNameHash = ntohl(ctx->cheader->VolnameHash[hashindex]);
    ctx->cheader->VolnameHash[hashindex] = htonl(blockindex);
    code =
	vlwrite(ctx->trans, DOFFSET(0, ctx->cheader, &ctx->cheader->VolnameHash[hashindex]),
		(char *)&ctx->cheader->VolnameHash[hashindex], sizeof(afs_int32));
    if (code)
	return VL_IO;
    return 0;
}


int
UnhashVolname(struct vl_ctx *ctx, afs_int32 blockindex,
	      struct nvlentry *aentry)
{
    afs_int32 hashindex, nextblockindex, prevblockindex;
    struct nvlentry tentry;
    afs_int32 temp;

    /* Take it out of the Volname hash list */
    hashindex = NameHash(aentry->name);
    nextblockindex = ntohl(ctx->cheader->VolnameHash[hashindex]);
    if (nextblockindex == blockindex) {
	/* First on the hash list; just adjust pointers */
	ctx->cheader->VolnameHash[hashindex] = htonl(aentry->nextNameHash);
	if (vlwrite
	    (ctx->trans, DOFFSET(0, ctx->cheader, &ctx->cheader->VolnameHash[hashindex]),
	     (char *)&ctx->cheader->VolnameHash[hashindex], sizeof(afs_int32)))
	    return VL_IO;
    } else {
	while (nextblockindex != blockindex) {
	    prevblockindex = nextblockindex;	/* always done at least once */
	    if (vlentryread
		(ctx->trans, nextblockindex, (char *)&tentry, sizeof(nvlentry)))
		return VL_IO;
	    if ((nextblockindex = tentry.nextNameHash) == NULLO)
		return VL_NOENT;
	}
	tentry.nextNameHash = aentry->nextNameHash;
	temp = htonl(tentry.nextNameHash);
	if (vlwrite
	    (ctx->trans, DOFFSET(prevblockindex, &tentry, &tentry.nextNameHash),
	     (char *)&temp, sizeof(afs_int32)))
	    return VL_IO;
    }
    aentry->nextNameHash = 0;
    return 0;
}


/* Returns the vldb entry tentry at offset index; remaining is the number of
 * entries left; the routine also returns the index of the next sequential
 * entry in the vldb
 */

afs_int32
NextEntry(struct vl_ctx *ctx, afs_int32 blockindex,
	  struct nvlentry *tentry, afs_int32 *remaining)
{
    afs_int32 lastblockindex;

    if (blockindex == 0)	/* get first one */
	blockindex = sizeof(*ctx->cheader);
    else {
	if (!index_OK(ctx, blockindex)) {
	    *remaining = -1;	/* error */
	    return 0;
	}
	blockindex += sizeof(nvlentry);
    }
    /* now search for the first entry that isn't free */
    for (lastblockindex = ntohl(ctx->cheader->vital_header.eofPtr);
	 blockindex < lastblockindex;) {
	if (vlentryread(ctx->trans, blockindex, (char *)tentry, sizeof(nvlentry))) {
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


/* Routine to verify that index is a legal offset to a vldb entry in the
 * table
 */
static int
index_OK(struct vl_ctx *ctx, afs_int32 blockindex)
{
    if ((blockindex < sizeof(*ctx->cheader))
	|| (blockindex >= ntohl(ctx->cheader->vital_header.eofPtr)))
	return 0;
    return 1;
}

/* makes a deep copy of src_ex into dst_ex */
static int
vlexcpy(struct extentaddr **dst_ex, struct extentaddr **src_ex)
{
    int i;
    for (i = 0; i < VL_MAX_ADDREXTBLKS; i++) {
	if (src_ex[i]) {
	    if (!dst_ex[i]) {
		dst_ex[i] = malloc(VL_ADDREXTBLK_SIZE);
	    }
	    if (!dst_ex[i]) {
		return VL_NOMEM;
	    }
	    memcpy(dst_ex[i], src_ex[i], VL_ADDREXTBLK_SIZE);

	} else if (dst_ex[i]) {
	    /* we have no src, but we have a dst... meaning, this block
	     * has gone away */
	    free(dst_ex[i]);
	    dst_ex[i] = NULL;
	}
    }
    return 0;
}

int
vlsetcache(struct vl_ctx *ctx, int locktype)
{
    if (locktype == LOCKREAD) {
	ctx->hostaddress = rd_HostAddress;
	ctx->ex_addr = rd_ex_addr;
	ctx->cheader = &rd_cheader;
	return 0;
    } else {
	memcpy(wr_HostAddress, rd_HostAddress, sizeof(wr_HostAddress));
	memcpy(&wr_cheader, &rd_cheader, sizeof(wr_cheader));

	ctx->hostaddress = wr_HostAddress;
	ctx->ex_addr = wr_ex_addr;
	ctx->cheader = &wr_cheader;

	return vlexcpy(wr_ex_addr, rd_ex_addr);
    }
}

int
vlsynccache(void)
{
    memcpy(rd_HostAddress, wr_HostAddress, sizeof(rd_HostAddress));
    memcpy(&rd_cheader, &wr_cheader, sizeof(rd_cheader));
    return vlexcpy(rd_ex_addr, wr_ex_addr);
}
