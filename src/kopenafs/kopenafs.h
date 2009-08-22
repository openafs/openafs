/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file defines the interface to the libkopenafs library, which provides
 * a reduced set of functions for compatibility with the Heimdal/KTH libkafs
 * library.  This interface and the corresponding library are the best way to
 * get a completely standalone setpag() function from OpenAFS (in the form of
 * the k_setpag() interface defined here).
 *
 * The calls here only work on systems with native AFS clients and *will not*
 * work through the NFS translator.
 */

#ifndef KOPENAFS_H
#define KOPENAFS_H 1

/* Get the VIOC* constants and struct ViceIoctl. */
#include <afs/vioc.h>

/*
 * Initialization function.  Returns true if AFS is available on the system
 * and false otherwise.  Should be called before any of the other functions,
 * and if it returns false, the other functions should not be called.
 */
int k_hasafs(void);

/*
 * Create a new PAG and put the current process in it.  Returns 0 on success,
 * non-zero on system call failure.  Equivalent to lsetpag().
 */
int k_setpag(void);

/*
 * Remove the tokens in the current PAG.  Returns 0 on success, non-zero on
 * system call failure.
 */
int k_unlog(void);

/*
 * Perform an arbitrary pioctl system call with the specified arguments.
 * Returns 0 on success, non-zero on system call failure.  Equivalent to
 * lpioctl().
 */
int k_pioctl(char *path, int cmd, struct ViceIoctl *cmarg, int follow);

#endif /* KOPENAFS_H */
