/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#if defined(UKERNEL)
#include "afs/ptint.h"
#else /* defined(UKERNEL) */
#include "ptint.h"
#endif /* defined(UKERNEL) */

#define	PRSRV		73

#define	ENTRYSIZE	192
#define	HASHSIZE	8191

#define	PRBADID		0x80000000

#define SYSVIEWERID     -203
#define	SYSADMINID	-204
#define SYSBACKUPID     -205
#define	ANYUSERID	-101
#define	AUTHUSERID	-102
#define	ANONYMOUSID	32766

#define PRDBVERSION	0

struct prheader {
    afs_int32 version;		/* database version number */
    afs_int32 headerSize;	/* bytes in header (almost version#) */
    afs_int32 freePtr;		/* first free entry in freelist */
    afs_int32 eofPtr;		/* first free byte in file */
    afs_int32 maxGroup;		/* most negative group id */
    afs_int32 maxID;		/* largest user id allocated */
    afs_int32 maxForeign;	/* largest foreign id allocated */
    afs_int32 maxInst;		/* largest sub/super id allocated */
    afs_int32 orphan;		/* groups owned by deleted users */
    afs_int32 usercount;	/* num users in system */
    afs_int32 groupcount;	/* num groups in system */
    afs_int32 foreigncount;	/* num registered foreign users NYI */
    afs_int32 instcount;	/* number of sub and super users NYI */
#if defined(PRDB_EXTENSIONS)
    afs_int32 reserved[4];	/* just in case */
    afs_int32 ext_hashtables;	/* chain of extended hash tables */
#else
    afs_int32 reserved[5];	/* just in case */
#endif
    afs_int32 nameHash[HASHSIZE];	/* hash table for names */
    afs_int32 idHash[HASHSIZE];	/* hash table for ids */
};

extern struct prheader cheader;

#define set_header_word(tt,field,value) \
  pr_Write ((tt), 0, (afs_int32)((char *)&(cheader.field) - (char *)&cheader),   \
	    ((cheader.field = (value)), (char *)&(cheader.field)),    \
	    sizeof(afs_int32))

#define inc_header_word(tt,field,inc) \
  pr_Write ((tt), 0, (afs_int32)((char *)&(cheader.field) - (char *)&cheader), \
	    ((cheader.field = (htonl(ntohl(cheader.field)+(inc)))),	    \
	     (char *)&(cheader.field)),				    \
	    sizeof(afs_int32))

#define	PRFREE		1	/* 1 if in free list */
#define	PRGRP		2	/* 1 if a group entry */
#define	PRCONT		4	/* 1 if an extension block */
#define	PRCELL		8	/* 1 if cell entry */
#define	PRFOREIGN	16	/* 1 if foreign user */
#define	PRINST		32	/* 1 if sub/super instance */
#define PRHASHTBL       0x100   /* 1 if extended hash table */
#define PRAUTHNAME      0x200   /* 1 if auth name entry */

#if defined(PRDB_EXTENSIONS)
#define PRTYPE		0x33f	/* type bits: only one should be set */
#else
#define PRTYPE		0x3f	/* type bits: only one should be set */
#endif
#define PRUSER		0	/* all type bits 0 => user entry */

#define PRACCESS	(1<<6)	/* access checking enabled */
#define PRQUOTA		(1<<7)	/* group creation quota checking on */

/* extended hash table types */
#define PRTABLE_AUTHNAME 0

/* define the access bits for entries, they are stored in the left half of the
 * entry's flags.  The SetFields interface takes them in the right half.  There
 * are eight bits altogether defining access rights for status, owned, member,
 * add, and remove operations.  For rights with two bits the values are defined
 * to be o=00, m=01, a=10, with 11 reserved.  As implemented, however, it is
 * o=00, m=01, a=1x. */
#define PRIVATE_SHIFT    16	/* move privacy bits to left half of flags */
#define PRP_STATUS_ANY (0x80 << PRIVATE_SHIFT)
#define PRP_STATUS_MEM (0x40 << PRIVATE_SHIFT)
#define PRP_OWNED_ANY  (0x20 << PRIVATE_SHIFT)
#define PRP_MEMBER_ANY (0x10 << PRIVATE_SHIFT)
#define PRP_MEMBER_MEM (0x08 << PRIVATE_SHIFT)
#define PRP_ADD_ANY    (0x04 << PRIVATE_SHIFT)
#define PRP_ADD_MEM    (0x02 << PRIVATE_SHIFT)
#define PRP_REMOVE_MEM (0x01 << PRIVATE_SHIFT)

#define PRP_GROUP_DEFAULT (PRP_STATUS_ANY | PRP_MEMBER_ANY)
#define PRP_USER_DEFAULT (PRP_STATUS_ANY)

#define PR_REMEMBER_TIMES 1

struct prentry {
    afs_int32 flags;		/* random flags */
    afs_int32 id;		/* user or group id */
    afs_int32 cellid;		/* A foreign users's repsenting group */
    afs_int32 next;		/* next block same entry (or freelist) */
#ifdef PR_REMEMBER_TIMES
    afs_uint32 createTime, addTime, removeTime, changeTime;
#ifdef PRDB_EXTENSIONS
    afs_int32 authnames;	/* chain of authentication names for this entry */
#else
    afs_int32 reserved[1];
#endif
#else
#ifdef PRDB_EXTENSIONS
    afs_int32 reserved[4];
    afs_int32 authnames;	/* chain of authentication names for this entry */
#else
    afs_int32 reserved[5];
#endif
#endif
    afs_int32 entries[PRSIZE];	/* groups a user is a member of (or list of members */
    afs_int32 nextID;		/* id hash table next pointer */
    afs_int32 nextName;		/* name has table next ptr */
    afs_int32 owner;		/* id of owner of entry */
    afs_int32 creator;		/* may differ from owner */
    afs_int32 ngroups;		/* number of groups this user has created
				 * -- 0 for reg group entries
				 * -- number of foreign users if foreign group */
    afs_int32 nusers;		/* Users added to foreign group */
    afs_int32 count;		/* number of members/groups for this group/user */
    afs_int32 instance;		/* number of sub/super instances for this user NYI */
    afs_int32 owned;		/* chain of groups owned by this entry */
    afs_int32 nextOwned;	/* chain of groups for owner of this entry */
    afs_int32 parent;		/* ptr to super instance  NYI */
    afs_int32 sibling;		/* ptr to sibling instance  NYI */
    afs_int32 child;		/* ptr to first child  NYI */
    char name[PR_MAXNAMELEN];	/* user or group name */
};

#if defined(SUPERGROUPS)

struct prentryg {
    afs_int32 flags;		/* random flags */
    afs_int32 id;		/* user or group id */
    afs_int32 cellid;		/* reserved for cellID */
    afs_int32 next;		/* next block same entry (or freelist) */
#ifdef PR_REMEMBER_TIMES
    afs_uint32 createTime, addTime, removeTime, changeTime;
    afs_int32 reserved[1];
#else
    afs_int32 reserved[5];
#endif
    afs_int32 entries[PRSIZE];	/* groups a user is a member of (or list of members */
    afs_int32 nextID;		/* id hash table next pointer */
    afs_int32 nextName;		/* name has table next ptr */
    afs_int32 owner;		/* id of owner of entry */
    afs_int32 creator;		/* may differ from owner */
    afs_int32 ngroups;		/* number of groups this user has created - 0 for group entries */
    afs_int32 nusers;		/* number of foreign user entries this user has created - 0 for group entries NYI */
    afs_int32 count;		/* number of members/groups for this group/user */
    afs_int32 countsg;		/* number of supergroups for this group */
    afs_int32 owned;		/* chain of groups owned by this entry */
    afs_int32 nextOwned;	/* chain of groups for owner of this entry */
    afs_int32 nextsg;		/* next block same entry for supergroups */
#define SGSIZE 2		/* number of supergroup entries */
    afs_int32 supergroup[SGSIZE];	/* supergroups this group belongs to */
    char name[PR_MAXNAMELEN];	/* user or group name */
};

#endif /* SUPERGROUPS */

struct contentry {		/* continuation of entry */
    afs_int32 flags;
    afs_int32 id;
    afs_int32 cellid;
    afs_int32 next;
    afs_int32 reserved[5];
    afs_int32 entries[COSIZE];
};


/* prdb entry containing a portion of an extended hash table */
/* note that 40 buckets would fit, but only 32 are used. */
struct hashentry {
    afs_int32 flags;		/* random flags */
    afs_int32 id;		/* unused */
    afs_int32 cellid;		/* unused */
    afs_int32 next;		/* next entry this table */
    afs_int32 nextTable;	/* next table */
    afs_int32 tableid;		/* table type */
    afs_int32 offset;		/* first bucket # in this entry */
    afs_int32 reserved;
    afs_int32 buckets[32];
};


#define ANSIZE 160

struct authentry {
    afs_int32 flags;		/* random flags */
    afs_int32 id;		/* id of owning entry */
    afs_int32 cellid;		/* unused */
    afs_int32 next;		/* next block for this authname */
    afs_int32 nextHash;		/* next authname in this bucket */
    afs_int32 nextAuthName;	/* next authname for this entry */
    afs_int32 kind;		/* PRAUTHTYPE_* */
    afs_int32 length;		/* total length of auth data */
    unsigned char data[ANSIZE];
};

/* The following are flags for PR_ListEntries() */
#define PRUSERS  0x1
#define PRGROUPS 0x2

#if defined(PRDB_EXTENSIONS)
struct pr_xht {
    char *name;			/* descriptive name of this table */
    afs_int32 tableid;		/* table ID in database file */
    int nbuckets;		/* number of buckets in table */

    /* compute the hash bucket number for a key on this table */
    afs_int32 (*hash)(void *name, int namelen);

    /*
     * check whether the entry at a specified location has the
     * desired name.  Returns 0 on success, error code on failure.
     * On success, *next is set to the location of the next entry,
     * and *found is set nonzero iff the name matches.
     */
    afs_int32 (*ckname)(struct ubik_trans *tt, afs_int32 loc,
			void *name, int namelen, afs_int32 *next, int *found);

    /*
     * Set the next-entry pointer in the entry at the specified location.
     * Returns 0 on success, error code on failure.
     */
    afs_int32 (*setnxt)(struct ubik_trans *tt, afs_int32 loc, afs_int32 next);

    int flags;			/* random flags */
    int nparts;  	 	/* number of entries containing table */
    afs_int32 *partids;		/* array of entry locations */
    struct hashentry *parts;	/* array of parts */

};

#define XHT_INITED	1	/* hashtable has been inited */
#endif
/* vi:set cin noet sw=4 tw=70: */
