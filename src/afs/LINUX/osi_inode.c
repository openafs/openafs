/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * LINUX inode operations
 *
 * Implements:
 * afs_syscall_icreate
 * afs_syscall_iopen
 * afs_syscall_iincdec
 *
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/osi_inode.h"
#include "../afs/afs_stats.h" /* statistics stuff */

#define BAD_IGET	-1000

/*
 * SGI dependent system calls
 */
#ifndef INODESPECIAL
/*
 * `INODESPECIAL' type inodes are ones that describe volumes.
 */
#define INODESPECIAL	0xffffffff	/* ... from ../vol/viceinode.h	*/
#endif


int afs_syscall_icreate(void)
{
    return 0;
}

int afs_syscall_iopen(void)
{
    return 0;
}

int afs_syscall_iincdec(void)
{
    return 0;
}
