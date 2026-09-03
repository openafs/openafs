/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	_VLSERVER_
#define	_VLSERVER_

#include "vldbint.h"
#include <afs/afsutil.h>

extern struct vldstats dynamic_statistics;



#define	HASHSIZE		8191	/* Must be prime */
#define	NULLO			0
#define	VLDBALLOCCOUNT		500

/* Current upper limits limits on certain entries; increase with care! */
#define	BADSERVERID	255
#define	MAXSERVERID	254	/* permits 255 servers; was == 30 in version 1 */
#define	MAXSERVERFLAG	0x80
#define	MAXPARTITIONID	255
#define	MAXBUMPCOUNT	0x7fffffff	/* Infinite  upper bound on bumping for now */
#define	MAXLOCKTIME	0x7fffffff	/* Infinite locking for now */

/* Order of entries in the volumeid[] array */
#define	RWVOL		0
#define	ROVOL		1
#define	BACKVOL		2

/* Header struct holding stats, internal pointers and the hash tables */
struct vlheader {
    struct vital_vlheader vital_header;	/* all small critical stuff are in here */
    afs_uint32 IpMappedAddr[MAXSERVERID + 1];	/* Mapping of ip addresses to relative ones */
    afs_uint32 VolnameHash[HASHSIZE];	/* hash table for vol names */
    afs_uint32 VolidHash[MAXTYPES][HASHSIZE];	/* hash table for vol ids */
    afs_int32 SIT;		/* spare for poss future use */
};

/* Vlentry's flags state */
#define	VLFREE		1	/* If in free list */
#define	VLDELETED	2	/* Entry is soft deleted */
#define	VLLOCKED	4	/* Advisory lock on entry */
#define	VLCONTBLOCK	8	/* Special continuation block entry */

/* Valid RelaseLock types */
#define	LOCKREL_TIMESTAMP   1
#define	LOCKREL_OPCODE	    2
#define	LOCKREL_AFSID	    4

/* Per repsite flags (serverFlags) */
#define	VLREPSITE_NEW   1	/* Replication site is got new release */

/* Internal representation of vldbentry; trying to save any bytes */
struct vlentry {
    afs_uint32 volumeId[MAXTYPES];	/* Corresponding volume of each type */
    afs_int32 flags;		/* General flags */
    afs_int32 LockAfsId;	/* Person who locked entry */
    afs_int32 LockTimestamp;	/* lock time stamp */
    afs_uint32 cloneId;		/* used during cloning */
    afs_int32 spares0;		/* XXXX was AssociatedChain XXXX */
    afs_uint32 nextIdHash[MAXTYPES];	/* Next id hash table pointer (or freelist ->[0]) */
    afs_uint32 nextNameHash;	/* Next name hash table pointer */
    afs_int32 spares1[2];	/* long spares */
    char name[VL_MAXNAMELEN];	/* Volume name */
    char spares3;		/* XXX was volumeType XXXX */
    u_char serverNumber[OMAXNSERVERS];	/* Server # for each server that holds volume */
    u_char serverPartition[OMAXNSERVERS];	/* Server Partition number */
    u_char serverFlags[OMAXNSERVERS];	/* Server flags */
    char spares4;		/* XXX was RefCount XXX */
    char spares2[1];		/* for 32-bit alignment */
};

struct nvlentry {
    afs_uint32 volumeId[MAXTYPES];	/* Corresponding volume of each type */
    afs_int32 flags;		/* General flags */
    afs_int32 LockAfsId;	/* Person who locked entry */
    afs_int32 LockTimestamp;	/* lock time stamp */
    afs_uint32 cloneId;		/* used during cloning */
    afs_uint32 nextIdHash[MAXTYPES];	/* Next id hash table pointer (or freelist ->[0]) */
    afs_uint32 nextNameHash;	/* Next name hash table pointer */
    char name[VL_MAXNAMELEN];	/* Volume name */
    u_char serverNumber[NMAXNSERVERS];	/* Server # for each server that holds volume */
    u_char serverPartition[NMAXNSERVERS];	/* Server Partition number */
    u_char serverFlags[NMAXNSERVERS];	/* Server flags */
};

typedef struct vlheader vlheader;
typedef struct vlentry vlentry;
typedef struct nvlentry nvlentry;

#define	DOFFSET(abase,astr,aitem) ((abase)+(((char *)(aitem)) - ((char *)(astr))))

#define	VL_MHSRV_PERBLK		64
#define	VL_MAXIPADDRS_PERMH	15
#define VL_MAX_ADDREXTBLKS	4
#define	VL_ADDREXTBLK_SIZE	8192
struct extentaddr {
    union ex_un {
	struct {
	    afs_int32 count;	/* # of valid addresses */
	    afs_int32 spares1[2];
	    afs_int32 flags;	/* must be in the same position as the vlentry's
				   flags field */
	    afs_uint32 contaddrs[VL_MAX_ADDREXTBLKS];
	    afs_int32 spares2[24];
	} _ex_header;
	struct {
	    afsUUID hostuuid;
	    afs_int32 uniquifier;
	    afs_uint32 addrs[VL_MAXIPADDRS_PERMH];
	    afs_uint32 flags;
	    afs_int32 spares[11];
	} _ex_addrentry;
    } _ex_un;
};
#define	ex_count	_ex_un._ex_header.count
#define	ex_hdrflags	_ex_un._ex_header.flags
#define	ex_contaddrs	_ex_un._ex_header.contaddrs
#define	ex_hostuuid	_ex_un._ex_addrentry.hostuuid
#define	ex_addrs	_ex_un._ex_addrentry.addrs
#define	ex_uniquifier	_ex_un._ex_addrentry.uniquifier
#define ex_srvflags	_ex_un._ex_addrentry.flags

/* ex_srvflags bit: this mh entry also has an Endpoint Extension Entry
 * (see below) holding its complete, typed (v4+v6) address list; addrs[]
 * above still holds that list's plain-IPv4 subset, verbatim, for every
 * reader that predates this. See "Multi-homed Entry" in doc/txt/vldb.txt. */
#define EXSRV_HAS_ENDPOINTS	0x1

/* Valid only when EXSRV_HAS_ENDPOINTS is set: a packed (base<<16)|index
 * reference to the corresponding Endpoint Extension Entry - see
 * "Endpoint Reference" in doc/txt/vldb.txt. Repurposes what is
 * otherwise the first word of this entry's reserved tail. */
#define ex_epref	_ex_un._ex_addrentry.spares[0]
#define VLEPREF_PACK(base, index) \
    (((afs_int32)(base) << 16) | ((index) & 0xffff))
#define VLEPREF_BASE(ref)	(((ref) >> 16) & 0xffff)
#define VLEPREF_INDEX(ref)	((ref) & 0xffff)

/* Endpoint Extension Block: a second sub-type of the same 8192-octet
 * extension-block record used for Multi-homed Extension Blocks (struct
 * extentaddr, above) - occupies one of the (up to 3) non-root slots in
 * the same chain rooted at the vldb header's SIT field. Added in
 * version 5 to hold, per fileserver, a complete typed address list that
 * may include IPv6 addresses. See "Endpoint Extension Block" in
 * doc/txt/vldb.txt for the full byte layout this mirrors. */
#define VLCONTBLOCK_ENDPOINTS	0x10	/* combined with VLCONTBLOCK in
					   exep_hdrflags to mark this
					   sub-type, rather than a plain
					   Multi-homed Extension Block */
#define VL_ENDPOINT_HDRSIZE	256	/* size of both the header slot and
					   each entry slot */
#define VL_EPSRV_PERBLK		31	/* usable entry slots per block:
					   (VL_ADDREXTBLK_SIZE /
					   VL_ENDPOINT_HDRSIZE) - 1 for the
					   header slot */
struct extentendpoints {
    union exep_un {
	struct {
	    afs_int32 count;		/* unused, always 0 */
	    afs_int32 spares1[2];
	    afs_int32 flags;		/* VLCONTBLOCK|VLCONTBLOCK_ENDPOINTS;
					   same position as extentaddr's
					   ex_hdrflags and nvlentry's flags */
	    afs_uint32 contaddrs[VL_MAX_ADDREXTBLKS];
	    afs_int32 spares2[56];	/* pad the header slot to
					   VL_ENDPOINT_HDRSIZE */
	} _exep_header;
	struct {
	    afsUUID hostuuid;
	    afs_int32 uniquifier;
	    afs_int32 count;		/* valid entries in endpoints[] */
	    struct vlendpoint endpoints[VL_MAXENDPOINTS];
	    afs_int32 flags;		/* reserved, always 0 */
	    afs_int32 spares[9];	/* pad the entry slot to
					   VL_ENDPOINT_HDRSIZE */
	} _exep_entry;
    } _exep_un;
};
#define exep_hdrflags	_exep_un._exep_header.flags
#define exep_contaddrs	_exep_un._exep_header.contaddrs
#define exep_hostuuid	_exep_un._exep_entry.hostuuid
#define exep_uniquifier	_exep_un._exep_entry.uniquifier
#define exep_count	_exep_un._exep_entry.count
#define exep_endpoints	_exep_un._exep_entry.endpoints

#define VLog(level, str)   ViceLog(level, str)

#endif /* _VLSERVER_ */
