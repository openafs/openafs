/* Copyright (C) 1995, 1989, 1998 Transarc Corporation - All rights reserved */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*
 * afs_cell.c
 *
 * Implements:
 */
#include "../afs/param.h"	/* Should be always first */
#include "../afs/stds.h"
#include "../afs/sysincludes.h"	/* Standard vendor system headers */

#if !defined(UKERNEL)
#include <net/if.h>
#include <netinet/in.h>

#ifdef AFS_SGI62_ENV
#include "../h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX20_ENV)
#include <netinet/in_var.h>
#endif /* ! ASF_HPUX110_ENV */
#endif /* !defined(UKERNEL) */

#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* afs statistics */

#if	defined(AFS_SUN56_ENV)
#include <inet/led.h>
#include <inet/common.h>
#include <inet/ip.h>
#endif

/* Exported variables */
afs_rwlock_t afs_xcell;			/* allocation lock for cells */
struct afs_q CellLRU;
afs_int32 afs_cellindex=0;


/* Local variables. */
struct cell *afs_rootcell = 0;

struct cell *afs_GetCellByName(acellName, locktype)
    register char *acellName;
    afs_int32 locktype;
{
    register struct cell *tc;
    register struct afs_q *cq, *tq;

    AFS_STATCNT(afs_GetCellByName);
    ObtainWriteLock(&afs_xcell,100);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	if (!strcmp(tc->cellName, acellName)) {
	    QRemove(&tc->lruq);
	    QAdd(&CellLRU, &tc->lruq);
	    ReleaseWriteLock(&afs_xcell);
	    return tc;
	}
    }
    ReleaseWriteLock(&afs_xcell);
    return (struct cell *) 0;

} /*afs_GetCellByName*/


struct cell *afs_GetCell(acell, locktype)
    register afs_int32 acell;
    afs_int32 locktype;
{
    register struct cell *tc;
    register struct afs_q *cq, *tq;

    AFS_STATCNT(afs_GetCell);
    if (acell == 1 && afs_rootcell) return afs_rootcell;
    ObtainWriteLock(&afs_xcell,101);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	if (tc->cell == acell) {
	    QRemove(&tc->lruq);
	    QAdd(&CellLRU, &tc->lruq);
	    ReleaseWriteLock(&afs_xcell);
	    return tc;
	}
    }
    ReleaseWriteLock(&afs_xcell);
    return (struct cell *) 0;

} /*afs_GetCell*/


struct cell *afs_GetCellByIndex(cellindex, locktype)
    register afs_int32 cellindex;
    afs_int32 locktype;
{
    register struct cell *tc;
    register struct afs_q *cq, *tq;

    AFS_STATCNT(afs_GetCellByIndex);
    ObtainWriteLock(&afs_xcell,102);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	if (tc->cellIndex == cellindex) {
	    QRemove(&tc->lruq);
	    QAdd(&CellLRU, &tc->lruq);
	    ReleaseWriteLock(&afs_xcell);
	    return tc;
	}
    }
    ReleaseWriteLock(&afs_xcell);
    return (struct cell *) 0;

} /*afs_GetCellByIndex*/


afs_int32 afs_NewCell(acellName, acellHosts, aflags, linkedcname, fsport, vlport)
    int aflags;
    char *acellName;
    register afs_int32 *acellHosts;
    char *linkedcname;
    u_short fsport, vlport;
{
    register struct cell *tc, *tcl=0;
    register afs_int32 i, newc=0, code=0;
    register struct afs_q *cq, *tq;

    AFS_STATCNT(afs_NewCell);
    if (*acellHosts == 0)
	/* need >= one host to gen cell # */
	return EINVAL;

    ObtainWriteLock(&afs_xcell,103);

    /* Find the cell and mark its servers as not down but gone */
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	if (strcmp(tc->cellName, acellName) == 0) {
	    /* we don't want to keep pinging old vlservers which were down,
	     * since they don't matter any more.  It's easier to do this than
	     * to remove the server from its various hash tables. */
	    for (i=0; i<MAXCELLHOSTS; i++) {
	        if (!tc->cellHosts[i]) break;
		tc->cellHosts[i]->flags &= ~SRVR_ISDOWN;
		tc->cellHosts[i]->flags |= SRVR_ISGONE;
	    }
	    break;
	}
    }

    if (cq != &CellLRU) {
	aflags &= ~CNoSUID;
    }
    else {
	tc = (struct cell *) afs_osi_Alloc(sizeof(struct cell));
	QAdd(&CellLRU, &tc->lruq);		       	/* put in lruq */
	tc->cellName = (char *) afs_osi_Alloc(strlen(acellName)+1);
	strcpy(tc->cellName, acellName);
	tc->cellIndex = afs_cellindex++;
	if (aflags & CPrimary) {
	    extern int afs_rootCellIndex;
	    tc->cell = 1;	/* primary cell is always 1 */
	    afs_rootcell = tc;
	    afs_rootCellIndex = tc->cellIndex;
	} else {
	    tc->cell = *acellHosts; /* won't be reused by another cell */
	}
	tc->states = 0;
	tc->lcellp = (struct cell *)0;
	tc->fsport = (fsport ? fsport : AFS_FSPORT);
	tc->vlport = (vlport ? vlport : AFS_VLPORT);
	afs_stats_cmperf.numCellsVisible++;
	newc++;
    }

    if (aflags & CLinkedCell) {
	if (!linkedcname) {
	    code = EINVAL;
	    goto bad;
	}
	for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	    tcl = QTOC(cq); tq = QNext(cq);
	    if (!strcmp(tcl->cellName, linkedcname)) {
		break;
	    }
	    tcl = 0;
	}
	if (!tcl) {
	    code = ENOENT;
	    goto bad;
	}
	if (tcl->lcellp) {	/* XXX Overwriting if one existed before! XXX */
	    tcl->lcellp->lcellp = (struct cell *)0;
	    tcl->lcellp->states &= ~CLinkedCell;
	}
	tc->lcellp = tcl;
	tcl->lcellp = tc;
    }
    tc->states |= aflags;
 
    bzero((char *)tc->cellHosts, sizeof(tc->cellHosts));
    for (i=0; i<MAXCELLHOSTS; i++) {
        struct server *ts;
	afs_uint32 temp = acellHosts[i];
	if (!temp) break;
	ts = afs_GetServer(&temp, 1, 0, tc->vlport, WRITE_LOCK, (afsUUID *)0, 0);
        ts->cell = tc;
	ts->flags &= ~SRVR_ISGONE;
	tc->cellHosts[i] = ts;
	afs_PutServer(ts, WRITE_LOCK);
    }
    afs_SortServers(tc->cellHosts, MAXCELLHOSTS);	/* randomize servers */
    ReleaseWriteLock(&afs_xcell);
    return 0;
bad:
    if (newc) {
	QRemove(&tc->lruq);
	afs_osi_Free(tc->cellName, strlen(tc->cellName)+1);
	afs_osi_Free((char *)tc, sizeof(struct cell));
    }
    ReleaseWriteLock(&afs_xcell);
    return code;

} /*afs_NewCell*/

afs_RemoveCellEntry(struct server *srvp)
{
  struct cell *tc;
  afs_int32 j, k;

  tc = srvp->cell;
  if (!tc) return;

  /* Remove the server structure from the cell list - if there */
  ObtainWriteLock(&afs_xcell,200);
  for (j=k=0; j<MAXCELLHOSTS; j++) {
     if (!tc->cellHosts[j]) break;
     if (tc->cellHosts[j] != srvp) {
        tc->cellHosts[k++] = tc->cellHosts[j];
     }
  }
  if (k == 0) {
     /* What do we do if we remove the last one? */
  }
  for (; k<MAXCELLHOSTS; k++) {
     tc->cellHosts[k] = 0;
  }
  ReleaseWriteLock(&afs_xcell);
}

