/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"  /* statistics */

void osi_VM_Truncate(avc, alen, acred)
    struct vcache *avc;
    int alen;
    struct AFS_UCRED *acred;
{
    return;
}

int osi_VM_FlushVCache(avc, slept)
    struct vcache *avc;
    int *slept;
{
    return 0;
}

void osi_VM_StoreAllSegments(avc)
    struct vcache *avc;
{
    return;
}

void osi_VM_TryToSmush(avc, acred, sync)
    struct vcache *avc;
    struct AFS_UCRED *acred;
    int sync;
{
    return;
}

void osi_VM_FlushPages(avc, credp)
    struct vcache *avc;
    struct AFS_UCRED *credp;
{
    return;
}
