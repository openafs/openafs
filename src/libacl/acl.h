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

Note that the number of entries must be less than ACL_MAXENTRIES
*/

/* This is temporary hack to get around changing the volume package
for now */

typedef struct acl_accessList AL_AccessList;

#endif
