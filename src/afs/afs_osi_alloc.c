/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"




#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */



#ifdef AFS_AIX41_ENV
#include "sys/lockl.h"
#include "sys/sleep.h"
#include "sys/syspest.h"
#include "sys/lock_def.h"
#endif

#ifndef AFS_PRIVATE_OSI_ALLOCSPACES

afs_lock_t osi_fsplock;
afs_lock_t osi_flplock;

static struct osi_packet {
    struct osi_packet *next;
} *freePacketList = NULL, *freeSmallList = NULL;

#endif /* AFS_PRIVATE_OSI_ALLOCSPACES */

static char memZero;		/* address of 0 bytes for kmem_alloc */

void *
afs_osi_Alloc(size_t size)
{
    AFS_STATCNT(osi_Alloc);
    /* 0-length allocs may return NULL ptr from AFS_KALLOC, so we special-case
     * things so that NULL returned iff an error occurred */
    if (size == 0)
	return &memZero;

    AFS_STATS(afs_stats_cmperf.OutStandingAllocs++);
    AFS_STATS(afs_stats_cmperf.OutStandingMemUsage += size);
#ifdef AFS_LINUX_ENV
    return osi_linux_alloc(size, 1);
#else
    return AFS_KALLOC(size);
#endif
}

void
afs_osi_Free(void *x, size_t asize)
{
    AFS_STATCNT(osi_Free);
    if (x == &memZero || x == NULL)
	return;			/* check for putting memZero back */

    AFS_STATS(afs_stats_cmperf.OutStandingAllocs--);
    AFS_STATS(afs_stats_cmperf.OutStandingMemUsage -= asize);
#if defined(AFS_LINUX_ENV)
    osi_linux_free(x);
#else
    AFS_KFREE(x, asize);
#endif
}

void
afs_osi_FreeStr(char *x)
{
    afs_osi_Free(x, strlen(x) + 1);
}

#ifndef AFS_PRIVATE_OSI_ALLOCSPACES

/* free space allocated by AllocLargeSpace.  Also called by mclput when freeing
 * a packet allocated by osi_NetReceive. */

void
osi_FreeLargeSpace(void *adata)
{

    AFS_ASSERT_GLOCK();

    AFS_STATCNT(osi_FreeLargeSpace);
    afs_stats_cmperf.LargeBlocksActive--;
    ObtainWriteLock(&osi_flplock, 322);
    ((struct osi_packet *)adata)->next = freePacketList;
    freePacketList = adata;
    ReleaseWriteLock(&osi_flplock);
}

void
osi_FreeSmallSpace(void *adata)
{

    AFS_ASSERT_GLOCK();

    AFS_STATCNT(osi_FreeSmallSpace);
    afs_stats_cmperf.SmallBlocksActive--;
    ObtainWriteLock(&osi_fsplock, 323);
    ((struct osi_packet *)adata)->next = freeSmallList;
    freeSmallList = adata;
    ReleaseWriteLock(&osi_fsplock);
}


/* allocate space for sender */
void *
osi_AllocLargeSpace(size_t size)
{
    struct osi_packet *tp;

    AFS_ASSERT_GLOCK();

    AFS_STATCNT(osi_AllocLargeSpace);
    if (size > AFS_LRALLOCSIZ)
	osi_Panic("osi_AllocLargeSpace: size=%d\n", (int)size);
    afs_stats_cmperf.LargeBlocksActive++;
    if (!freePacketList) {
	char *p;

	afs_stats_cmperf.LargeBlocksAlloced++;
	p = afs_osi_Alloc(AFS_LRALLOCSIZ);
#ifdef  KERNEL_HAVE_PIN
	/*
	 * Need to pin this memory since under heavy conditions this memory
	 * could be swapped out; the problem is that we could inside rx where
	 * interrupts are disabled and thus we would panic if we don't pin it.
	 */
	pin(p, AFS_LRALLOCSIZ);
#endif
	return p;
    }
    ObtainWriteLock(&osi_flplock, 324);
    tp = freePacketList;
    if (tp)
	freePacketList = tp->next;
    ReleaseWriteLock(&osi_flplock);
    return (char *)tp;
}


/* allocate space for sender */
void *
osi_AllocSmallSpace(size_t size)
{
    struct osi_packet *tp;

    AFS_STATCNT(osi_AllocSmallSpace);
    if (size > AFS_SMALLOCSIZ)
	osi_Panic("osi_AllocSmallS: size=%d\n", (int)size);

    if (!freeSmallList) {
	afs_stats_cmperf.SmallBlocksAlloced++;
	afs_stats_cmperf.SmallBlocksActive++;
	tp = afs_osi_Alloc(AFS_SMALLOCSIZ);
#ifdef KERNEL_HAVE_PIN
        pin((char *)tp, AFS_SMALLOCSIZ);
#endif
        return (char *)tp;
    }
    afs_stats_cmperf.SmallBlocksActive++;
    ObtainWriteLock(&osi_fsplock, 327);
    tp = freeSmallList;
    if (tp)
	freeSmallList = tp->next;
    ReleaseWriteLock(&osi_fsplock);
    return (char *)tp;
}
#endif /* AFS_PRIVATE_OSI_ALLOCSPACES */

void
shutdown_osinet(void)
{
#ifndef AFS_PRIVATE_OSI_ALLOCSPACES
    struct osi_packet *tp;
#endif

    AFS_STATCNT(shutdown_osinet);

#ifndef AFS_PRIVATE_OSI_ALLOCSPACES
    while ((tp = freePacketList)) {
	freePacketList = tp->next;
	afs_osi_Free(tp, AFS_LRALLOCSIZ);
#ifdef  KERNEL_HAVE_PIN
	unpin(tp, AFS_LRALLOCSIZ);
#endif
    }

    while ((tp = freeSmallList)) {
	freeSmallList = tp->next;
	afs_osi_Free(tp, AFS_SMALLOCSIZ);
#ifdef  KERNEL_HAVE_PIN
	unpin(tp, AFS_SMALLOCSIZ);
#endif
    }
    if (afs_cold_shutdown) {
	LOCK_INIT(&osi_fsplock, "osi_fsplock");
	LOCK_INIT(&osi_flplock, "osi_flplock");
    }
#endif /* AFS_PRIVATE_OSI_ALLOCSPACES */
    if (afs_stats_cmperf.LargeBlocksActive ||
	afs_stats_cmperf.SmallBlocksActive)
    {
	afs_warn("WARNING: not all blocks freed: large %d small %d\n",
		 afs_stats_cmperf.LargeBlocksActive,
		 afs_stats_cmperf.SmallBlocksActive);
    }
}

