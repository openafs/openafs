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
External access lists are just char *'s, with the following format:  Begins with a decimal integer in format "%d\n%d\n"
specifying the number of positive entries and negative entries that follow.  This is followed by the list
of entries.  Each entry consists of a
username or groupname followed by a decimal number representing the rights mask for that name.  Each
entry in the list looks as if it had been produced by printf() using a format list of "%s\t%d\n".

Note that the number of entries must be less than or equal to ACL_MAXENTRIES
*/

/* This is temporary hack to get around changing the volume package
for now */

typedef struct acl_accessList AL_AccessList;

extern int acl_NewACL(int nEntries, struct acl_accessList **acl);
extern int acl_FreeACL(struct acl_accessList **acl);
extern int acl_NewExternalACL(int nEntries, char **r);
extern int acl_FreeExternalACL(char **r);
extern int acl_Externalize(struct acl_accessList *acl, char **elist);
extern int acl_Internalize(char *elist, struct acl_accessList **acl);
extern int acl_Initialize(char *version);
#ifdef	_RXGEN_PTINT_
extern int acl_CheckRights(struct acl_accessList *acl, prlist *groups, int *rights);
extern int acl_IsAMember(afs_int32 aid, prlist *cps);
#endif

#endif
