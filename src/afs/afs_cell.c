/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
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
#if     defined(AFS_SUN58_ENV)
#include <netinet/ip6.h>
#endif
#include <inet/ip.h>
#endif

/* Exported variables */
afs_rwlock_t afs_xcell;			/* allocation lock for cells */
struct afs_q CellLRU;
afs_int32 afs_cellindex=0;
afs_uint32 afs_nextCellNum = 0x100;


/* Local variables. */
struct cell *afs_rootcell = 0;

static char afs_AfsdbHandlerWait;
static char afs_AfsdbLookupWait;

char afs_AfsdbHandlerPresent = 0;
char afs_AfsdbHandlerInuse = 0;

char *afs_AfsdbHandler_CellName;
afs_int32 *afs_AfsdbHandler_CellHosts;
int *afs_AfsdbHandler_Timeout;

char afs_AfsdbHandler_ReqPending;
char afs_AfsdbHandler_Completed;


struct cell *afs_GetCellByName_int();

int afs_strcasecmp(s1, s2)
    register char *s1, *s2;
{
    while (*s1 && *s2) {
	register char c1, c2;

	c1 = *s1++;
	c2 = *s2++;
	if (c1 >= 'A' && c1 <= 'Z') c1 += 0x20;
	if (c2 >= 'A' && c2 <= 'Z') c2 += 0x20;
	if (c1 != c2)
	    return c1-c2;
    }

    return *s1 - *s2;
}


#ifdef AFS_AFSDB_ENV
int afs_AfsdbHandler(acellName, acellNameLen, kernelMsg)
    char *acellName;
    int acellNameLen;
    afs_int32 *kernelMsg;
{
    /* afs_syscall_call() has already grabbed the global lock */

    afs_AfsdbHandlerPresent = 1;

    if (afs_AfsdbHandler_ReqPending) {
	int i, hostCount;

	hostCount = kernelMsg[0];
	*afs_AfsdbHandler_Timeout = kernelMsg[1];
	if (*afs_AfsdbHandler_Timeout) *afs_AfsdbHandler_Timeout += osi_Time();

	for (i=0; i<MAXCELLHOSTS; i++) {
	    if (i >= hostCount)
		afs_AfsdbHandler_CellHosts[i] = 0;
	    else
		afs_AfsdbHandler_CellHosts[i] = kernelMsg[2+i];
	}

	/* Request completed, wake up the relevant thread */
	afs_AfsdbHandler_ReqPending = 0;
	afs_AfsdbHandler_Completed = 1;
	afs_osi_Wakeup(&afs_AfsdbLookupWait);
    }

    /* Wait for a request */
    while (afs_AfsdbHandler_ReqPending == 0)
	afs_osi_Sleep(&afs_AfsdbHandlerWait);

    /* Copy the requested cell name into the request buffer */
    strncpy(acellName, afs_AfsdbHandler_CellName, acellNameLen);

    /* Return the lookup request to userspace */    
    return 0;
}
#endif


int afs_GetCellHostsFromDns(acellName, acellHosts, timeout)
    char *acellName;
    afs_int32 *acellHosts;
    int *timeout;
{
#ifdef AFS_AFSDB_ENV
    char grab_glock = 0;

    if (!afs_AfsdbHandlerPresent) return ENOENT;

    if (!ISAFS_GLOCK()) {
	grab_glock = 1;
	AFS_GLOCK();
    }

    /* Wait until the AFSDB handler is available, and grab it */
    while (afs_AfsdbHandlerInuse)
	afs_osi_Sleep(&afs_AfsdbLookupWait);
    afs_AfsdbHandlerInuse = 1;

    /* Set up parameters for the handler */
    afs_AfsdbHandler_CellName = acellName;
    afs_AfsdbHandler_CellHosts = acellHosts;
    afs_AfsdbHandler_Timeout = timeout;

    /* Wake up the AFSDB handler */
    afs_AfsdbHandler_Completed = 0;
    afs_AfsdbHandler_ReqPending = 1;
    afs_osi_Wakeup(&afs_AfsdbHandlerWait);

    /* Wait for the handler to get back to us with the reply */
    while (!afs_AfsdbHandler_Completed)
	afs_osi_Sleep(&afs_AfsdbLookupWait);

    /* Release the AFSDB handler and wake up others waiting for it */
    afs_AfsdbHandlerInuse = 0;
    afs_osi_Wakeup(&afs_AfsdbLookupWait);

    if (grab_glock) AFS_GUNLOCK();

    if (*acellHosts) return 0;
    return ENOENT;
#else
    return ENOENT;
#endif
}


void afs_RefreshCell(tc)
    register struct cell *tc;
{
    afs_int32 acellHosts[MAXCELLHOSTS];
    int timeout;

    /* Don't need to do anything if no timeout or it's not expired */
    if (!tc->timeout || tc->timeout > osi_Time()) return;

    if (!afs_GetCellHostsFromDns(tc->cellName, acellHosts, &timeout)) {
	afs_NewCell(tc->cellName, acellHosts, tc->states,
		    tc->lcellp ? tc->lcellp->cellName : (char *) 0,
		    tc->fsport, tc->vlport, timeout);
    }

    /* In case of a DNS failure, keep old cell data.. */
    return;
}


struct cell *afs_GetCellByName_Dns(acellName, locktype)
    register char *acellName;
    afs_int32 locktype;
{
    afs_int32 acellHosts[MAXCELLHOSTS];
    int timeout;

    if (afs_GetCellHostsFromDns(acellName, acellHosts, &timeout))
	return (struct cell *) 0;
    if (afs_NewCell(acellName, acellHosts, CNoSUID, (char *) 0, 0, 0, timeout))
	return (struct cell *) 0;

    return afs_GetCellByName_int(acellName, locktype, 0);
}


struct cell *afs_GetCellByName_int(acellName, locktype, trydns)
    register char *acellName;
    afs_int32 locktype;
    char trydns;
{
    register struct cell *tc;
    register struct afs_q *cq, *tq;

    AFS_STATCNT(afs_GetCellByName);
    ObtainWriteLock(&afs_xcell,100);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	if (!afs_strcasecmp(tc->cellName, acellName)) {
	    QRemove(&tc->lruq);
	    QAdd(&CellLRU, &tc->lruq);
	    ReleaseWriteLock(&afs_xcell);
	    afs_RefreshCell(tc);
	    return tc;
	}
    }
    ReleaseWriteLock(&afs_xcell);
    return trydns ? afs_GetCellByName_Dns(acellName, locktype)
		  : (struct cell *) 0;

} /*afs_GetCellByName_int*/


struct cell *afs_GetCellByName(acellName, locktype)
    register char *acellName;
    afs_int32 locktype;
{
    return afs_GetCellByName_int(acellName, locktype, 1);

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
	    afs_RefreshCell(tc);
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
	    afs_RefreshCell(tc);
	    return tc;
	}
    }
    ReleaseWriteLock(&afs_xcell);
    return (struct cell *) 0;

} /*afs_GetCellByIndex*/


afs_int32 afs_NewCell(acellName, acellHosts, aflags, linkedcname, fsport, vlport, timeout)
    int aflags;
    char *acellName;
    register afs_int32 *acellHosts;
    char *linkedcname;
    u_short fsport, vlport;
    int timeout;
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
	if (afs_strcasecmp(tc->cellName, acellName) == 0) {
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
	    tc->cell = afs_nextCellNum++;
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
	    if (!afs_strcasecmp(tcl->cellName, linkedcname)) {
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
    tc->timeout = timeout;
 
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

