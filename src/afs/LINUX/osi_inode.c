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

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/osi_inode.h"
#include "afs/afs_stats.h"	/* statistics stuff */

int
afs_syscall_icreate(long a, long b, long c, long d, long e, long f)
{
    return 0;
}

int
afs_syscall_iopen(int a, int b, int c)
{
    return 0;
}

int
afs_syscall_iincdec(int a, int v, int c, int d)
{
    return 0;
}
