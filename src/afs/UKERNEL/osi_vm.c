/* Copyright (C) 1995 Transarc Corporation - All rights reserved. */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
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
