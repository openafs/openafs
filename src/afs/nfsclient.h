/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#ifndef	__NFSCLIENT__
#define	__NFSCLIENT__

#define	NNFSCLIENTS	32	/* Hash table size for afs_nfspags table */
#define	NHash(host)	((host) & (NNFSCLIENTS-1))
#define NFSCLIENTGC     (24*3600) /* time after which to GC nfsclientpag structs */
#define	NFSXLATOR_CRED	0xaaaa

struct nfsclientpag {
    /* From here to .... */
    struct nfsclientpag	*next;	/* Next hash pointer */
    struct exporterops	*nfs_ops;
    afs_int32 states;
    afs_int32 type;
    struct exporterstats nfs_stats;
    /* .... here is also an overlay to the afs_exporter structure */

    afs_int32 refCount;		/* Ref count for packages using this */
    afs_int32 uid;			/* search based on uid and ... */
    afs_int32 host;			/* ... nfs client's host ip address */
    afs_int32 pag;			/* active pag for all  (uid, host) "unpaged" conns */
    char *sysname;		/* user's "@sys" value; also kept in unixuser */
    afs_int32 lastcall;		/*  Used for timing out nfsclientpag structs  */
};


#endif /* __NFSCLIENT__ */

