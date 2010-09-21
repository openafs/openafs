/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Revision 2.1  1990/08/07  19:11:33
 * Start with clean version to sync test and dev trees.
 *
 * Revision 1.11  89/02/10  11:56:23
 * Added union to kaentry structure which allows overloading several
 *   fields depending on which type of user it is.
 *
 * Revision 1.10  89/02/02  14:24:22
 * Removed calls to gethostid() which doesn't return IP address on Suns.
 *
 * Revision 1.9  88/12/22  14:43:48
 * COUNT_REQ remembers name of last operation.
 *
 * Revision 1.8  88/12/09  14:42:00
 * Greatly improved the external key version number management code.
 * Added new entry type called kaOldKeys.
 *
 * Revision 1.7  88/11/22  09:30:40
 * Change RPC statistics to be more robust.
 *
 * Revision 1.6  88/11/18  09:21:09
 * Conversion to Rx and the new RxGen.
 * Key version number is now a long.
 * Conversion to use of <afs/auth.h> include file.
 * Changed macros for statistics code since opcode hacking was punted.
 *
 * Revision 1.5  88/10/12  16:26:34
 * Added much better (working) support of key version number maintenance.
 *
 * Revision 1.4  88/09/20  11:53:12
 * Added IBM Copyright
 *
 * Revision 1.3  88/08/29  12:46:39
 * This checks in several new modules and many updates.  The AuthServer
 *   at this point works more or less as described in the NAFS document
 *   released in at the Aug 23-24 1988 workshop.
 * Tickets are as described in the kerberos.ticket file.
 * Intergrated w/ MIT's des implementation and the Andrew one-way password
 *   encryption.  Uses bcrypt for RSECURE connections.  Uses R not Rx.
 *
 * Revision 1.2  88/07/19  16:20:33
 * Added GetEntry and ListEntry; other internal changes.
 *  */

#ifndef KADBVERSION
#define KADBVERSION	5	/* the database version */
#endif /* KADBVERSION */

#define HASHSIZE	8191	/* pick a prime for the length */

#define NULLO		0

/* all fields are stored in network (sun or rt) byte order */
struct kaheader {
    afs_int32 version;		/* database version number */
    afs_int32 headerSize;	/* bytes in header, for skipping in bad times */
    afs_int32 freePtr;		/* first (if any) free entry in freelist */
    afs_int32 eofPtr;		/* first free byte in file */
    afs_int32 kvnoPtr;		/* first special name old keys entry */
    struct kasstats stats;	/* track interesting statistics */
    afs_int32 admin_accounts;	/* total number of users w/ admin flag set */
    afs_int32 specialKeysVersion;	/* inc if special name gets new key */
    afs_int32 hashsize;		/* allocated size of nameHash */
#if (KADBVERSION > 5)
    afs_int32 spare[10];	/* allocate some spares next time */
#endif
    afs_int32 nameHash[HASHSIZE];	/* hash table for names */
    afs_int32 checkVersion;	/* database version number, same as first field */
};

#define ENTRYSIZE		200
#define KA_NPWSUMS		(ENTRYSIZE - sizeof(kaident) - sizeof(struct ktc_encryptionKey)  - 11*4)

/* all fields are stored in network byte order */
struct kaentry {
    afs_int32 flags;		/* random flags */
    afs_int32 next;		/* next block same entry (or freelist) */
    Date user_expiration;	/* user registration good till then */
    Date modification_time;	/* time of last update */
    afs_int32 modification_id;	/* identity of user doing update */
    Date change_password_time;	/* time user last changed own password */
    afs_int32 max_ticket_lifetime;	/* maximum lifetime for tickets */
    afs_int32 key_version;	/* verson number of this key */
    union {			/* overload several miscellaneous fields */
	struct {
	    afs_int32 nOldKeys;	/* number of outstanding old keys */
	    afs_int32 oldKeys;	/* block containing old keys */
	} asServer;		/* for principals that are part of the AuthServer itself */
	struct {
	    afs_int32 maxAssociates;	/* associates this user can create */
	    afs_int32 nInstances;	/* number of instances user's created */
	} assocRoot;		/* for principals at root of associate tree */
	struct {
	    afs_int32 root;	/* identity of this instance's root */
	    afs_int32 spare;
	} associate;		/* associate instance */
    } misc;
    /* put the strings last to simplify alignment calculations */
    struct kaident userID;	/* user and instance names */
    struct ktc_encryptionKey key;	/* the key to use */
    unsigned char misc_auth_bytes[4];	/* expires, spare, attempts, locktime */
    unsigned char pwsums[KA_NPWSUMS];	/* pad to 200 bytes */
};
typedef struct kaentry kaentry;

#define EXPIRES 0
#define REUSEFLAGS 1
#define ATTEMPTS 2
#define LOCKTIME 3

struct kaOldKey {		/* each old key still remembered */
    Date superseded;		/* time this key replaced, or zero */
    afs_int32 version;		/* key's version */
    struct ktc_encryptionKey key;
};
#define NOLDKEYS ((ENTRYSIZE-3*sizeof(afs_int32))/sizeof(struct kaOldKey))

struct kaOldKeys {
    afs_int32 flags;		/* random flags */
    afs_int32 next;		/* next block of old keys for all entries (or freelist) */
    afs_int32 entry;		/* corresponding user entry */
    struct kaOldKey keys[NOLDKEYS];	/* each old key */
    char padding[ENTRYSIZE -
		 ((NOLDKEYS * sizeof(struct kaOldKey)) +
		  3 * sizeof(afs_int32))];
};

#define COUNT_REQ(op) int *this_op = &dynamic_statistics.op.aborts; dynamic_statistics.op.requests++; lastOperation = # op
#define COUNT_ABO (*this_op)++

/* given the disk offset of a structure, the memory address of the structure
   and the address of an item within the structure, compute the disk address of
   this last item. */
#define	DOFFSET(abase,astr,aitem) ((abase)+(((char *)(aitem)) - ((char *)(astr))))

/* given an index for an entry return its disk address */
#define IOFFSET(idx) (sizeof(kaentry)*(idx) + sizeof(cheader))

extern struct kaheader cheader;
extern struct kadstats dynamic_statistics;
extern afs_uint32 myHost;

extern int kaux_opendb(char *path);

extern void kaux_closedb(void
    );

extern int kaux_read(afs_int32 to, unsigned int *nfailures,
		     afs_uint32 * lasttime);

extern int kaux_write(afs_int32 to, unsigned int nfailures,
		      afs_uint32 lasttime);

extern void kaux_inc(afs_int32 to, afs_uint32 locktime);

extern int kaux_islocked(afs_int32 to, u_int attempts, u_int locktime);

extern afs_int32 krb4_cross;

extern afs_int32 es_Report(char *fmt, ...)
    AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);

#define LOCKPW
