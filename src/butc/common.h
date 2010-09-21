/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* random constants */
#define TC_MAXNAMELEN  64	/* name length */

/* This specifies the interface to the tape coordinator */

/* describes the information that should be dumped to dump a single volume */
struct tc_dumpDesc {
    afs_int32 vid;		/* volume to dump */
    afs_int32 partition;	/* partition at which to find the volume */
    afs_int32 date;		/* date from which to do the dump */
    opaque hostID[16];		/* opaque netaddress, really a sockaddr_in */
};

/* define how to restore a volume */
struct tc_restoreDesc {
    afs_int32 origVid;		/* original volume id */
    afs_int32 vid;		/* 0 means allocate new volid */
    afs_int32 partition;	/* where to restore the volume */
    afs_int32 flags;		/* flags */
    opaque hostID[16];		/* which file server to restore the volume to */
    opaque newName[TC_MAXNAMELEN];	/* new name suffix */
};
/* describes the current status of a dump */
struct tc_dumpStat {
    afs_int32 dumpID;		/* dump id we're returning */
    afs_int32 bytesDumped;	/* bytes dumped so far */
    afs_int32 volumeBeingDumped;	/* guess */
    afs_int32 flags;		/* true if the dump is done */
};
