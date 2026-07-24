/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	Information Technology Center
	Carnegie-Mellon University
*/

#ifndef _ACL_
#define _ACL_


#include "afs/ptint.h"

#define ACL_VERSION "Version 1"

struct acl_accessEntry {
    int id;			/*internally-used ID of user or group */
    int rights;			/*mask */
};

/*
The above access list entry format is used in VICE
*/


#define ACL_ACLVERSION  1	/*Identifies current format of access lists */

struct acl_accessList {
    int size;			/*size of this access list in bytes, including MySize itself */
    int version;		/*to deal with upward compatibility ; <= ACL_ACLVERSION */
    int total;
    int positive;		/* number of positive entries */
    int negative;		/* number of minus entries */
    struct acl_accessEntry entries[1];	/* negative entries are stored backwards from end */
};

/*
Used in VICE. This is how acccess lists are stored on secondary storage.
*/


#define ACL_MAXENTRIES	20

/*
 * External access lists are just char *'s, with the following format:
 *
 * Begins with a decimal integer in format "%d\n%d\n" specifying the
 * number of positive entries and negative entries that follow.  This is
 * followed by the list of entries.  Each entry consists of a username or
 * groupname followed by a decimal number representing the rights mask for
 * that name.  Each entry in the list looks as if it had been produced by
 * printf() using a format list of "%s\t%d\n".
 *
 * Note that the number of entries must be less than or equal to ACL_MAXENTRIES
 */

/* This is temporary hack to get around changing the volume package for now */

typedef struct acl_accessList AL_AccessList;

extern int acl_NewACL(int nEntries, struct acl_accessList **acl);
extern int acl_FreeACL(struct acl_accessList **acl);
extern int acl_NewExternalACL(int nEntries, char **r);
extern int acl_FreeExternalACL(char **r);
extern int acl_Externalize(struct acl_accessList *acl, char **elist);
extern int acl_Internalize(char *elist, struct acl_accessList **acl);
extern int acl_Externalize_pr(int (*func)(idlist *ids, namelist *names), struct acl_accessList *acl, char **elist);
extern int acl_Internalize_pr(int (*func)(namelist *names, idlist *ids), char *elist, struct acl_accessList **acl);
extern int acl_Initialize(char *version);
#ifdef	_RXGEN_PTINT_
extern int acl_CheckRights(struct acl_accessList *acl, prlist *groups, int *rights);
extern int acl_IsAMember(afs_int32 aid, prlist *cps);
#endif

extern int acl_HtonACL(struct acl_accessList *);
extern int acl_NtohACL(struct acl_accessList *);

/*
 * The aclu_* routines and structs below ('u' for 'user' or 'utility') are for
 * parsing/printing ACL data for end-user-facing applications. This is opposed
 * to the acl_* routines above, which are for converting ACL data between
 * on-disk and wire formats.
 */

enum aclu_rights_type {
    ACLU_RTYPE_SET = 1,	/**< overwrite/set rights ('=' default behavior) */
    ACLU_RTYPE_DESTROY,	/**< remove the ACL entirely ("none") */
    ACLU_RTYPE_DENY,	/**< revoke all rights ("null" DFS specific) */
    ACLU_RTYPE_RELADD,	/**< add specific rights to existing ones ('+') */
    ACLU_RTYPE_RELDEL,	/**< remove specific rights from existing ones ('-') */
};

extern int aclu_ParseRights(const char *rights, afs_uint32 *mask,
			    enum aclu_rights_type *rtypep, char *bad_char)
			    AFS_NONNULL((1,2,3));
extern int aclu_ParseRightsDFS(const char *arights, afs_uint32 *mask,
			       enum aclu_rights_type *rtypep, char *bad_char)
			       AFS_NONNULL((1,2,3));

struct aclu_rightsbuf {
    char sbuf[16];
};

extern const char *aclu_StringifyRights(afs_uint32 arights,
					struct aclu_rightsbuf *strbuf)
					AFS_NONNULL_ALL;
extern const char *aclu_StringifyRightsDFS(afs_uint32 arights,
					   struct aclu_rightsbuf *strbuf)
					   AFS_NONNULL_ALL;


#define ACLU_MAXNAME 100

struct aclu_AclEntry {
    struct aclu_AclEntry *next;
    char name[ACLU_MAXNAME];
    afs_int32 rights;
};

struct aclu_Acl {
    int dfs;		/* Originally true if a dfs acl; now also the type
			 * of the acl (1, 2, or 3, corresponding to object,
			 * initial dir, or initial object). */
    char cell[1025];	/* DFS cell name, from DCE sec_rgy_name_t */
    int nplus;
    int nminus;
    struct aclu_AclEntry *pluslist;
    struct aclu_AclEntry *minuslist;
};

extern int aclu_ParseEmptyAcl(const char *astr, struct aclu_Acl **a_acl)
			      AFS_NONNULL_ALL;
extern int aclu_ParseAcl(const char *astr, struct aclu_Acl **a_acl)
			 AFS_NONNULL_ALL;
extern void aclu_FreeAcl(struct aclu_Acl **a_acl) AFS_NONNULL_ALL;

#endif
