/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef	__NFSCLIENT__
#define	__NFSCLIENT__

#define	NNFSCLIENTS	32	/* Hash table size for afs_nfspags table */
#define	NHash(host)	((host) & (NNFSCLIENTS-1))
#define NFSCLIENTGC     (24*3600)	/* time after which to GC nfsclientpag structs */
#define	NFSXLATOR_CRED	0xaaaa

struct nfsclientpag {
    /* From here to .... */
    struct nfsclientpag *next;	/* Next hash pointer */
    struct exporterops *nfs_ops;
    afs_int32 states;
    afs_int32 type;
    struct exporterstats nfs_stats;
    /* .... here is also an overlay to the afs_exporter structure */

    afs_int32 refCount;		/* Ref count for packages using this */
    afs_int32 uid;		/* search based on uid and ... */
    afs_int32 host;		/* ... nfs client's host ip address */
    afs_int32 pag;		/* active pag for all  (uid, host) "unpaged" conns */
    afs_int32 client_uid;       /* actual UID on client */
    char *sysname[MAXNUMSYSNAMES];/* user's "@sys" value; also kept in unixuser */
    int sysnamecount;           /*  number of sysnames */
    afs_int32 lastcall;		/*  Used for timing out nfsclientpag structs  */
};


#endif /* __NFSCLIENT__ */
