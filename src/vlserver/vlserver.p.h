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
    afs_int32 VolnameHash[HASHSIZE];	/* hash table for vol names */
    afs_int32 VolidHash[MAXTYPES][HASHSIZE];	/* hash table for vol ids */
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
    afs_int32 cloneId;		/* used during cloning */
    afs_int32 spares0;		/* XXXX was AssociatedChain XXXX */
    afs_int32 nextIdHash[MAXTYPES];	/* Next id hash table pointer (or freelist ->[0]) */
    afs_int32 nextNameHash;	/* Next name hash table pointer */
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
    afs_int32 cloneId;		/* used during cloning */
    afs_int32 nextIdHash[MAXTYPES];	/* Next id hash table pointer (or freelist ->[0]) */
    afs_int32 nextNameHash;	/* Next name hash table pointer */
    char name[VL_MAXNAMELEN];	/* Volume name */
    u_char serverNumber[NMAXNSERVERS];	/* Server # for each server that holds volume */
    u_char serverPartition[NMAXNSERVERS];	/* Server Partition number */
    u_char serverFlags[NMAXNSERVERS];	/* Server flags */
};

typedef struct vlheader vlheader;
typedef struct vlentry vlentry;
typedef struct nvlentry nvlentry;

#define COUNT_REQ(op) static int this_op = op-VL_LOWEST_OPCODE; dynamic_statistics.requests[this_op]++
#define COUNT_ABO dynamic_statistics.aborts[this_op]++
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
	    afs_int32 flags;	/* must match vlentry's flags field XXX */
	    afs_uint32 contaddrs[VL_MAX_ADDREXTBLKS];
	    afs_int32 spares2[24];
	} _ex_header;
	struct {
	    afsUUID hostuuid;
	    afs_int32 uniquifier;
	    afs_uint32 addrs[VL_MAXIPADDRS_PERMH];
	} _ex_addrentry;
    } _ex_un;
};
#define	ex_count	_ex_un._ex_header.count
#define	ex_flags	_ex_un._ex_header.flags
#define	ex_contaddrs	_ex_un._ex_header.contaddrs
#define	ex_hostuuid	_ex_un._ex_addrentry.hostuuid
#define	ex_addrs	_ex_un._ex_addrentry.addrs
#define	ex_uniquifier	_ex_un._ex_addrentry.uniquifier

#define VLog(level, str)   ViceLog(5, str)

struct ubik_trans;
extern int FreeBlock(struct ubik_trans *trans, afs_int32 blockindex);

#endif /* _VLSERVER_ */
