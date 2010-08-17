/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _LOCKDATA_
#define _LOCKDATA_ 1

#define	ZERO 2147617029		/*to be replaced by 0L */
#define N_SECURITY_OBJECTS 1


struct aqueue {
    char name[VOLSER_MAXVOLNAME];	/* related to max volname allowed in the vldb */
    afs_int32 ids[3];
    afs_int32 copyDate[3];
    int isValid[3];
    struct aqueue *next;
};

struct qHead {
    int count;
    struct aqueue *next;
};

#endif /* _LOCKDATA_ */
