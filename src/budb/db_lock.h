/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

struct db_lockS {
    afs_int32 type;		/* user defined - for consistency checks */
    afs_int32 lockState;	/* locked/free */
    afs_int32 lockTime;		/* when locked */
    afs_int32 expires;		/* when timeout expires */
    afs_int32 instanceId;	/* user instance id */
    int lockHost;		/* locking host, if possible */
};

typedef struct db_lockS db_lockT;
typedef db_lockT db_lockP;
