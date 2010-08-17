/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* VLDB structures for VLDB version 1. */
struct vital_vlheader_1 {
    afs_int32 vldbversion;
    afs_int32 headersize;
    afs_int32 freePtr;
    afs_int32 eofPtr;
    afs_int32 allocs;
    afs_int32 frees;
    afs_int32 MaxVolumeId;
    afs_int32 totalEntries[3];
};

typedef struct vital_vlheader_1 vital_vlheader1;

struct vlheader_1 {
    vital_vlheader1 vital_header;
    afs_uint32 IpMappedAddr[31];
    afs_int32 VolnameHash[8191];
    afs_int32 VolidHash[3][8191];
};
struct vlentry_1 {
    afs_uint32 volumeId[3];
    afs_int32 flags;
    afs_int32 LockAfsId;
    afs_int32 LockTimestamp;
    afs_uint32 cloneId;
    afs_int32 spares0;
    afs_int32 nextIdHash[3];
    afs_int32 nextNameHash;
    afs_int32 spares1[2];
    char name[65];
    char spares3;
    unsigned char serverNumber[8];
    unsigned char serverPartition[8];
    unsigned char serverFlags[8];
    char spares4;
    char spares2[1];
};

/* VLDB structures for VLDB version 2. */
typedef struct vital_vlheader_1 vital_vlheader2;

struct vlheader_2 {
    vital_vlheader2 vital_header;
    afs_uint32 IpMappedAddr[255];	/* == 0..254 */
    afs_int32 VolnameHash[8191];
    afs_int32 VolidHash[3][8191];
    afs_int32 SIT;
};

struct vlentry_2 {
    afs_uint32 volumeId[3];
    afs_int32 flags;
    afs_int32 LockAfsId;
    afs_int32 LockTimestamp;
    afs_uint32 cloneId;
    afs_int32 spares0;
    afs_int32 nextIdHash[3];
    afs_int32 nextNameHash;
    afs_int32 spares1[2];
    char name[65];
    char spares3;
    unsigned char serverNumber[8];
    unsigned char serverPartition[8];
    unsigned char serverFlags[8];
    char spares4;
    char spares2[1];
};

typedef struct vital_vlheader_1 vital_vlheader3;

struct vlheader_3 {
    vital_vlheader3 vital_header;
    afs_uint32 IpMappedAddr[255];	/* == 0..254 */
    afs_int32 VolnameHash[8191];
    afs_int32 VolidHash[3][8191];
    afs_int32 SIT;
};


struct vlentry_3 {
    afs_uint32 volumeId[3];
    afs_int32 flags;
    afs_int32 LockAfsId;
    afs_int32 LockTimestamp;
    afs_uint32 cloneId;
    afs_int32 nextIdHash[3];
    afs_int32 nextNameHash;
    char name[65];
    unsigned char serverNumber[MAXSERVERS];
    unsigned char serverPartition[MAXSERVERS];
    unsigned char serverFlags[MAXSERVERS];

#ifdef	obsolete_vldb_fields
    afs_int32 spares0;		/* AssociatedChain */
    afs_int32 spares1[0];
    afs_int32 spares1[1];
    char spares3;		/* volumeType */
    char spares4;		/* RefCount */
    char spares2[1];
#endif
};
