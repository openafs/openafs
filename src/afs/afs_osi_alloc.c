/* Copyright (C) 1995 Transarc Corporation - All rights reserved. */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include "../afs/param.h"	/* Should be always first */


#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* afs statistics */

#ifdef AFS_AIX41_ENV
#include "sys/lockl.h"
#include "sys/sleep.h"
#include "sys/syspest.h"
#include "sys/lock_def.h"
/*lock_t osi_fsplock = LOCK_AVAIL;*/
#else
afs_lock_t osi_fsplock;
#endif



static struct osi_packet {
    struct osi_packet *next;
} *freePacketList = 0, *freeSmallList, *freeMediumList;
afs_lock_t osi_flplock;


afs_int32 afs_preallocs = 512;	/* Reserve space for all small allocs! */
void osi_AllocMoreSSpace(preallocs)
    register afs_int32 preallocs;
{
    register int i;
    char *p;

    p = (char *) afs_osi_Alloc(AFS_SMALLOCSIZ * preallocs);
#ifdef	AFS_AIX32_ENV
    pin(p, AFS_SMALLOCSIZ * preallocs);	/* XXXX */
#endif
    for (i=0; i < preallocs; i++, p += AFS_SMALLOCSIZ) {
#ifdef AFS_AIX32_ENV
	*p = '\0'; /* page fault it in. */
#endif
	osi_FreeSmallSpace((char *) p);
    }
    afs_stats_cmperf.SmallBlocksAlloced += preallocs;
    afs_stats_cmperf.SmallBlocksActive  += preallocs;
}



/* free space allocated by AllocLargeSpace.  Also called by mclput when freeing
 * a packet allocated by osi_NetReceive. */

void
osi_FreeLargeSpace(void *adata)
{

    AFS_ASSERT_GLOCK();

    AFS_STATCNT(osi_FreeLargeSpace);
    afs_stats_cmperf.LargeBlocksActive--;
    MObtainWriteLock(&osi_flplock,322);
    ((struct osi_packet *)adata)->next = freePacketList;
    freePacketList = adata;
    MReleaseWriteLock(&osi_flplock);
}

void
osi_FreeSmallSpace(void *adata)
{

#if	defined(AFS_AIX32_ENV)
    int x;
#endif
#if	defined(AFS_HPUX_ENV)
    ulong_t x;
#endif

    AFS_ASSERT_GLOCK();

    AFS_STATCNT(osi_FreeSmallSpace);
    afs_stats_cmperf.SmallBlocksActive--;
#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    x = splnet();	/*lockl(&osi_fsplock, LOCK_SHORT);*/
#else
    MObtainWriteLock(&osi_fsplock,323);
#endif
    ((struct osi_packet *)adata)->next = freeSmallList;
    freeSmallList = adata;
#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    splx(x);		/*unlockl(&osi_fsplock);*/
#else
    MReleaseWriteLock(&osi_fsplock);
#endif
}

#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
osi_AllocMoreMSpace(preallocs)
    register afs_int32 preallocs;
{
    register int i;
    char *p;

    p = (char *) afs_osi_Alloc(AFS_MDALLOCSIZ * preallocs);
#ifdef	AFS_AIX32_ENV
    pin(p, AFS_MDALLOCSIZ * preallocs);	/* XXXX */
#endif
    for (i=0; i < preallocs; i++, p += AFS_MDALLOCSIZ) {
#ifdef AFS_AIX32_ENV
	*p = '\0'; /* page fault it in. */
#endif
	osi_FreeMediumSpace((char *) p);
    }
    afs_stats_cmperf.MediumBlocksAlloced += preallocs;
    afs_stats_cmperf.MediumBlocksActive  += preallocs;
}


void *osi_AllocMediumSpace(size_t size) 
{
    register struct osi_packet *tp;
#if	defined(AFS_AIX32_ENV)
    int x;
#endif
#if	defined(AFS_HPUX_ENV)
    ulong_t x;
#endif

    afs_stats_cmperf.MediumBlocksActive++;
 retry:
    x = splnet();
    tp = freeMediumList;
    if ( tp ) freeMediumList = tp->next;
    splx(x);
    if (!tp) {
	osi_AllocMoreMSpace(AFS_MALLOC_LOW_WATER); 
	goto retry;
    }
    return tp;
}

void
osi_FreeMediumSpace(void *adata)
{

#if	defined(AFS_AIX32_ENV)
    int x;
#endif
#if	defined(AFS_HPUX_ENV)
    ulong_t x;
#endif

    afs_stats_cmperf.MediumBlocksActive--;
    x = splnet();
    ((struct osi_packet *)adata)->next = freeMediumList;
    freeMediumList = adata;
    splx(x);
}
#endif /* defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV) */


/* allocate space for sender */
void *osi_AllocLargeSpace(size_t size) 
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
	p = (char *) afs_osi_Alloc(AFS_LRALLOCSIZ);
#ifdef	AFS_AIX32_ENV
	/*
	 * Need to pin this memory since under heavy conditions this memory
         * could be swapped out; the problem is that we could inside rx where
         * interrupts are disabled and thus we would panic if we don't pin it.
         */
	pin(p, AFS_LRALLOCSIZ);	
#endif
	return p;
    }
    MObtainWriteLock(&osi_flplock,324);
    tp = freePacketList;
    if ( tp ) freePacketList = tp->next;
    MReleaseWriteLock(&osi_flplock);
    return (char *) tp;
}

#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
/*
 * XXX We could have used a macro around osi_AllocSmallSpace but it's
 * probably better like this so that we can remove this at some point.
 */
char *osi_AllocSmall(size, morespace) 
    register afs_int32 morespace;		/* 1 - means we called at splnet level */
    register afs_int32 size;
{
    register struct osi_packet *tp;
#if	defined(AFS_AIX32_ENV)
    int x;
#endif
#if	defined(AFS_HPUX_ENV)
    ulong_t x;
#endif

    AFS_ASSERT_GLOCK();

    AFS_STATCNT(osi_AllocSmallSpace);
    if (size > AFS_SMALLOCSIZ) osi_Panic("osi_AllocSmall, size=%d", size);
    if ((!morespace && 
	 ((afs_stats_cmperf.SmallBlocksAlloced - afs_stats_cmperf.SmallBlocksActive)
	  <= AFS_SALLOC_LOW_WATER)) 
	|| !freeSmallList) {
	osi_AllocMoreSSpace(AFS_SALLOC_LOW_WATER * 2);
    }
    afs_stats_cmperf.SmallBlocksActive++;
#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    x = splnet();	/*lockl(&osi_fsplock, LOCK_SHORT);*/
#else
    MObtainWriteLock(&osi_fsplock,325);
#endif
    tp = freeSmallList;
    if ( tp ) freeSmallList = tp->next;
#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    splx(x);		/*unlockl(&osi_fsplock);*/
#else
    MReleaseWriteLock(&osi_fsplock);
#endif

    return (char *) tp;
}

osi_FreeSmall(adata)
    register struct osi_packet *adata; {
#if	defined(AFS_AIX32_ENV)
    int x;
#endif
#if	defined(AFS_HPUX_ENV)
    ulong_t x;
#endif

    AFS_STATCNT(osi_FreeSmallSpace);
    afs_stats_cmperf.SmallBlocksActive--;
#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    x = splnet();	/*lockl(&osi_fsplock, LOCK_SHORT);*/
#else
    MObtainWriteLock(&osi_fsplock,326);
#endif
    adata->next = freeSmallList;
    freeSmallList = adata;
#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    splx(x);		/*unlockl(&osi_fsplock);*/
#else
    MReleaseWriteLock(&osi_fsplock);
#endif
    return 0;
}
#endif /* AFS_AIX32_ENV || AFS_HPUX_ENV */

/* allocate space for sender */
void *osi_AllocSmallSpace(size_t size) 
{
    register struct osi_packet *tp;
#if	defined(AFS_AIX32_ENV)
    int x;
#endif
#if	defined(AFS_HPUX_ENV)
    ulong_t x;
#endif

    AFS_STATCNT(osi_AllocSmallSpace);
    if (size > AFS_SMALLOCSIZ) osi_Panic("osi_AllocSmallS: size=%d\n", size);

#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    /* 
     * We're running out of free blocks (< 50); get some more ourselves so that
     * when we don't run out of them when called under splnet() (from rx);
     */
    if (((afs_stats_cmperf.SmallBlocksAlloced - afs_stats_cmperf.SmallBlocksActive)
	  <= AFS_SALLOC_LOW_WATER) || !freeSmallList) {
	osi_AllocMoreSSpace(AFS_SALLOC_LOW_WATER * 2);
    }
#else
    if (!freeSmallList) {
	afs_stats_cmperf.SmallBlocksAlloced++;
	afs_stats_cmperf.SmallBlocksActive++;
	return afs_osi_Alloc(AFS_SMALLOCSIZ);
    }
#endif
    afs_stats_cmperf.SmallBlocksActive++;
#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    x = splnet();	/*lockl(&osi_fsplock, LOCK_SHORT);*/
#else
    MObtainWriteLock(&osi_fsplock,327);
#endif
    tp = freeSmallList;
    if ( tp ) freeSmallList = tp->next;
#if	defined(AFS_AIX32_ENV) || defined(AFS_HPUX_ENV)
    splx(x);		/*unlockl(&osi_fsplock);*/
#else
    MReleaseWriteLock(&osi_fsplock);
#endif
    return (char *) tp;
}



void shutdown_osinet()
{   
  extern int afs_cold_shutdown;

  AFS_STATCNT(shutdown_osinet);
  if (afs_cold_shutdown) {
    struct osi_packet *tp;

    while (tp = freePacketList) {
      freePacketList = tp->next;
      afs_osi_Free(tp, AFS_LRALLOCSIZ);
#ifdef	AFS_AIX32_ENV
      unpin(tp, AFS_LRALLOCSIZ);	
#endif
    }

    while (tp = freeSmallList) {
      freeSmallList = tp->next;
      afs_osi_Free(tp, AFS_SMALLOCSIZ);
#ifdef	AFS_AIX32_ENV
      unpin(tp, AFS_SMALLOCSIZ);	
#endif
    }
    afs_preallocs = 512;
#ifndef	AFS_AIX32_ENV
    LOCK_INIT(&osi_fsplock, "osi_fsplock");
#endif
    LOCK_INIT(&osi_flplock, "osi_flplock");
  }
}

