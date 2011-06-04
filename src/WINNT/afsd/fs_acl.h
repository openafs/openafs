/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_FS_ACL_H
#define AFS_FS_ACL_H

#define ACL_MAXNAME 100


/*
 * Character to use between name and rights in printed representation for
 * DFS ACL's.
 */
#define DFS_SEPARATOR	' '

typedef char sec_rgy_name_t[1025];	/* A DCE definition */

struct Acl {
    int dfs;	        /* Originally true if a dfs acl; now also the type
                         * of the acl (1, 2, or 3, corresponding to object,
                         * initial dir, or initial object). */
    sec_rgy_name_t cell; /* DFS cell name */
    int nplus;
    int nminus;
    struct AclEntry *pluslist;
    struct AclEntry *minuslist;
};

struct AclEntry {
    struct AclEntry *next;
    char name[ACL_MAXNAME];
    afs_int32 rights;
};

/*
 * Mods for the AFS/DFS protocol translator.
 *
 * DFS rights. It's ugly to put these definitions here, but they
 * *cannot* change, because they're part of the wire protocol.
 * In any event, the protocol translator will guarantee these
 * assignments for AFS cache managers.
 */
#define DFS_READ          0x01
#define DFS_WRITE         0x02
#define DFS_EXECUTE       0x04
#define DFS_CONTROL       0x08
#define DFS_INSERT        0x10
#define DFS_DELETE        0x20

/* the application definable ones (backwards from AFS) */
#define DFS_USR0 0x80000000      /* "A" bit */
#define DFS_USR1 0x40000000      /* "B" bit */
#define DFS_USR2 0x20000000      /* "C" bit */
#define DFS_USR3 0x10000000      /* "D" bit */
#define DFS_USR4 0x08000000      /* "E" bit */
#define DFS_USR5 0x04000000      /* "F" bit */
#define DFS_USR6 0x02000000      /* "G" bit */
#define DFS_USR7 0x01000000      /* "H" bit */
#define DFS_USRALL	(DFS_USR0 | DFS_USR1 | DFS_USR2 | DFS_USR3 |\
			 DFS_USR4 | DFS_USR5 | DFS_USR6 | DFS_USR7)

/* added relative add resp. delete    */
/* (so old add really means to set)   */
enum rtype { add, destroy, deny, reladd, reldel };

extern void ZapList(struct AclEntry *alist);

extern void ZapAcl(struct Acl *acl);

extern int PruneList(struct AclEntry **ae, int dfs);

extern int CleanAcl(struct Acl *aa, char *cellname);

extern struct Acl *EmptyAcl(char *astr);

extern struct Acl *ParseAcl (char *astr, int astr_size);

extern char *AclToString(struct Acl *acl);

extern void ChangeList (struct Acl *al, afs_int32 plus, char *aname, afs_int32 arights);

extern struct AclEntry *FindList (struct AclEntry *alist, char *aname);


#endif
