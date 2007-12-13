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

RCSID
    ("$Header$");



#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#ifndef AFS_FBSD_ENV

#ifdef AFS_AIX41_ENV
#include "sys/lockl.h"
#include "sys/sleep.h"
#include "sys/syspest.h"
#include "sys/lock_def.h"
/*lock_t osi_fsplock = LOCK_AVAIL;*/
#endif

afs_lock_t osi_fsplock;



static struct osi_packet {
    struct osi_packet *next;
} *freePacketList = NULL, *freeSmallList;
afs_lock_t osi_flplock;

static char memZero;		/* address of 0 bytes for kmem_alloc */

struct osimem {
    struct osimem *next;
};


void *
afs_osi_Alloc(size_t x)
{
#if !defined(AFS_LINUX20_ENV) && !defined(AFS_FBSD_ENV)
    register struct osimem *tm = NULL;
    register int size;
#endif

    AFS_STATCNT(osi_Alloc);
    /* 0-length allocs may return NULL ptr from AFS_KALLOC, so we special-case
     * things so that NULL returned iff an error occurred */
    if (x == 0)
	return &memZero;

    AFS_STATS(afs_stats_cmperf.OutStandingAllocs++);
    AFS_STATS(afs_stats_cmperf.OutStandingMemUsage += x);
#ifdef AFS_LINUX20_ENV
    return osi_linux_alloc(x, 1);
#elif defined(AFS_FBSD_ENV)
    return osi_fbsd_alloc(x, 1);
#else
    size = x;
    tm = (struct osimem *)AFS_KALLOC(size);
#ifdef	AFS_SUN5_ENV
    if (!tm)
	osi_Panic("osi_Alloc: Couldn't allocate %d bytes; out of memory!\n",
		  size);
#endif
    return (void *)tm;
#endif
}

#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)

void *
afs_osi_Alloc_NoSleep(size_t x)
{
    register struct osimem *tm;
    register int size;

    AFS_STATCNT(osi_Alloc);
    /* 0-length allocs may return NULL ptr from AFS_KALLOC, so we special-case
     * things so that NULL returned iff an error occurred */
    if (x == 0)
	return &memZero;

    size = x;
    AFS_STATS(afs_stats_cmperf.OutStandingAllocs++);
    AFS_STATS(afs_stats_cmperf.OutStandingMemUsage += x);
    tm = (struct osimem *)AFS_KALLOC_NOSLEEP(size);
    return (void *)tm;
}

#endif /* SUN || SGI */

void
afs_osi_Free(void *x, size_t asize)
{
    AFS_STATCNT(osi_Free);
    if (x == &memZero)
	return;			/* check for putting memZero back */

    AFS_STATS(afs_stats_cmperf.OutStandingAllocs--);
    AFS_STATS(afs_stats_cmperf.OutStandingMemUsage -= asize);
#if defined(AFS_LINUX20_ENV)
    osi_linux_free(x);
#elif defined(AFS_FBSD_ENV)
    osi_fbsd_free(x);
#else
    AFS_KFREE((struct osimem *)x, asize);
#endif
}

void
afs_osi_FreeStr(char *x)
{
    afs_osi_Free(x, strlen(x) + 1);
}



/* free space allocated by AllocLargeSpace.  Also called by mclput when freeing
 * a packet allocated by osi_NetReceive. */

void
osi_FreeLargeSpace(void *adata)
{

    AFS_ASSERT_GLOCK();

    AFS_STATCNT(osi_FreeLargeSpace);
    afs_stats_cmperf.LargeBlocksActive--;
    MObtainWriteLock(&osi_flplock, 322);
    ((struct osi_packet *)adata)->next = freePacketList;
    freePacketList = adata;
    MReleaseWriteLock(&osi_flplock);
}

void
osi_FreeSmallSpace(void *adata)
{

    AFS_ASSERT_GLOCK();

    AFS_STATCNT(osi_FreeSmallSpace);
    afs_stats_cmperf.SmallBlocksActive--;
    MObtainWriteLock(&osi_fsplock, 323);
    ((struct osi_packet *)adata)->next = freeSmallList;
    freeSmallList = adata;
    MReleaseWriteLock(&osi_fsplock);
}


/* allocate space for sender */
void *
osi_AllocLargeSpace(size_t size)
{
    register struct osi_packet *tp;

    AFS_ASSERT_GLOCK();

    AFS_STATCNT(osi_AllocLargeSpace);
    if (size > AFS_LRALLOCSIZ)
	osi_Panic("osi_AllocLargeSpace: size=%d\n", size);
    afs_stats_cmperf.LargeBlocksActive++;
    if (!freePacketList) {
	char *p;

	afs_stats_cmperf.LargeBlocksAlloced++;
	p = (char *)afs_osi_Alloc(AFS_LRALLOCSIZ);
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
    MObtainWriteLock(&osi_flplock, 324);
    tp = freePacketList;
    if (tp)
	freePacketList = tp->next;
    MReleaseWriteLock(&osi_flplock);
    return (char *)tp;
}


/* allocate space for sender */
void *
osi_AllocSmallSpace(size_t size)
{
    register struct osi_packet *tp;

    AFS_STATCNT(osi_AllocSmallSpace);
    if (size > AFS_SMALLOCSIZ)
	osi_Panic("osi_AllocSmallS: size=%d\n", size);

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
    MObtainWriteLock(&osi_fsplock, 327);
    tp = freeSmallList;
    if (tp)
	freeSmallList = tp->next;
    MReleaseWriteLock(&osi_fsplock);
    return (char *)tp;
}



void
shutdown_osinet(void)
{
    AFS_STATCNT(shutdown_osinet);
    if (afs_cold_shutdown) {
	struct osi_packet *tp;

	while ((tp = freePacketList)) {
	    freePacketList = tp->next;
#ifdef  KERNEL_HAVE_PIN
	    unpin(tp, AFS_LRALLOCSIZ);
#endif
	    afs_osi_Free(tp, AFS_LRALLOCSIZ);
	}

	while ((tp = freeSmallList)) {
	    freeSmallList = tp->next;
#ifdef  KERNEL_HAVE_PIN
	    unpin(tp, AFS_SMALLOCSIZ);
#endif
	    afs_osi_Free(tp, AFS_SMALLOCSIZ);
	}
	LOCK_INIT(&osi_fsplock, "osi_fsplock");
	LOCK_INIT(&osi_flplock, "osi_flplock");
    }
    if (afs_stats_cmperf.LargeBlocksActive || 
	afs_stats_cmperf.SmallBlocksActive) 
    {
	afs_warn("WARNING: not all blocks freed: large %d small %d\n", 
		 afs_stats_cmperf.LargeBlocksActive, 
		 afs_stats_cmperf.SmallBlocksActive);
    }
}
#endif
