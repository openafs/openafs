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
#include <afsconfig.h>
#include "../afs/param.h"

RCSID("$Header: /tmp/cvstemp/openafs/src/afs/afs_cell.c,v 1.1.1.10 2002/08/02 04:28:38 hartmans Exp $");

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
afs_int32 afs_realcellindex=0;
afs_uint32 afs_nextCellNum = 0x100;

/* Local variables. */
struct cell *afs_rootcell = 0;

/* Handler waiting for request from client */
static char afs_AfsdbHandlerWait;
/* Client waiting for handler to become available or finish request */
static char afs_AfsdbLookupWait;

/* Set to 1 when we've seen the userspace AFSDB process at least once */
char afs_AfsdbHandlerPresent = 0;
/* Set to 1 when there is a client interacting with the AFSDB handler.
 * Protects the in and out variables below.  Protected by GLOCK. */
char afs_AfsdbHandlerInuse = 0;
/* Set to 1 when AFSDB has been shut down */
char afs_AfsdbHandlerShutdown = 0;

/* Input to handler from the client: cell name to look up */
char *afs_AfsdbHandler_CellName;
/* Outputs from handler to client: cell hosts, TTL, and real cell name */
afs_int32 *afs_AfsdbHandler_CellHosts;
int *afs_AfsdbHandler_Timeout;
char **afs_AfsdbHandler_RealName;

/* Client sets ReqPending to 1 whenever it queues a request for it */
char afs_AfsdbHandler_ReqPending = 0;
/* Handler sets Completed to 1 when it completes the client request */
char afs_AfsdbHandler_Completed = 0;


struct cell *afs_GetCellByName2();

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
void afs_StopAfsdb()
{
    if (afs_AfsdbHandlerPresent) {
	afs_osi_Wakeup(&afs_AfsdbHandlerWait);
    } else {
	afs_AfsdbHandlerShutdown = 1;
	afs_termState = AFSOP_STOP_RXEVENT;
    }
}

int afs_AfsdbHandler(acellName, acellNameLen, kernelMsg)
    char *acellName;
    int acellNameLen;
    afs_int32 *kernelMsg;
{
    /* afs_syscall_call() has already grabbed the global lock */

    if (afs_AfsdbHandlerShutdown) return -2;
    afs_AfsdbHandlerPresent = 1;

    if (afs_AfsdbHandler_ReqPending) {
	int i, hostCount;

	hostCount = kernelMsg[0];
	*afs_AfsdbHandler_Timeout = kernelMsg[1];
	if (*afs_AfsdbHandler_Timeout) *afs_AfsdbHandler_Timeout += osi_Time();

	*afs_AfsdbHandler_RealName = afs_osi_Alloc(strlen(acellName) + 1);
	strcpy(*afs_AfsdbHandler_RealName, acellName);

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
    while (afs_AfsdbHandler_ReqPending == 0 && afs_termState != AFSOP_STOP_AFSDB)
	afs_osi_Sleep(&afs_AfsdbHandlerWait);

    /* Check if we're shutting down */
    if (afs_termState == AFSOP_STOP_AFSDB) {
	/* Inform anyone waiting for us that we're going away */
	afs_AfsdbHandlerShutdown = 1;
	afs_AfsdbHandlerPresent = 0;
	afs_osi_Wakeup(&afs_AfsdbLookupWait);

	afs_termState = AFSOP_STOP_RXEVENT;
	afs_osi_Wakeup(&afs_termState);
	return -2;
    }

    /* Copy the requested cell name into the request buffer */
    strncpy(acellName, afs_AfsdbHandler_CellName, acellNameLen);

    /* Return the lookup request to userspace */    
    return 0;
}
#endif


int afs_GetCellHostsFromDns(acellName, acellHosts, timeout, realName)
    char *acellName;
    afs_int32 *acellHosts;
    int *timeout;
    char **realName;
{
#ifdef AFS_AFSDB_ENV
    char grab_glock = 0;

    if (!afs_AfsdbHandlerPresent) return ENOENT;

    /* Initialize host list to empty in case the handler is gone */
    *acellHosts = 0;

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
    afs_AfsdbHandler_RealName = realName;

    /* Wake up the AFSDB handler */
    afs_AfsdbHandler_Completed = 0;
    afs_AfsdbHandler_ReqPending = 1;
    afs_osi_Wakeup(&afs_AfsdbHandlerWait);

    /* Wait for the handler to get back to us with the reply */
    while (afs_AfsdbHandlerPresent && !afs_AfsdbHandler_Completed)
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


void afs_RefreshCell(ac)
    register struct cell *ac;
{
    afs_int32 cellHosts[MAXCELLHOSTS];
    char *realName = NULL;
    struct cell *tc;
    int timeout;

    /* Don't need to do anything if no timeout or it's not expired */
    if (!ac->timeout || ac->timeout > osi_Time()) return;

    if (afs_GetCellHostsFromDns(ac->cellName, cellHosts, &timeout, &realName))
	/* In case of lookup failure, keep old data */
	goto done;

    /* Refresh the DB servers for the real cell; other values stay the same. */
    afs_NewCell(realName, cellHosts, 0, (char *) 0, 0, 0, timeout, (char *) 0);

    /* If this is an alias, update the alias entry too */
    if (afs_strcasecmp(ac->cellName, realName)) {
	/*
	 * Look up the entry we just updated, to compensate for
	 * uppercase-vs-lowercase lossage with DNS.
	 */
	tc = afs_GetCellByName2(realName, READ_LOCK, 0 /* no AFSDB */);

	if (tc) {
	    afs_NewCell(ac->cellName, 0, CAlias, (char *) 0, 0, 0,
			timeout, tc->cellName);
	    afs_PutCell(tc, READ_LOCK);
	}
    }

done:
    if (realName)
	afs_osi_Free(realName, strlen(realName) + 1);
}


struct cell *afs_GetCellByName_Dns(acellName, locktype)
    register char *acellName;
    afs_int32 locktype;
{
    afs_int32 cellHosts[MAXCELLHOSTS];
    char *realName = NULL;
    struct cell *tc;
    int timeout;

    if (afs_GetCellHostsFromDns(acellName, cellHosts, &timeout, &realName))
	goto bad;
    if (afs_NewCell(realName, cellHosts, CNoSUID, (char *) 0, 0, 0,
		    timeout, (char *) 0))
	goto bad;

    /* If this is an alias, create an entry for it too */
    if (afs_strcasecmp(acellName, realName)) {
	/*
	 * Look up the entry we just updated, to compensate for
	 * uppercase-vs-lowercase lossage with DNS.
	 */
	tc = afs_GetCellByName2(realName, READ_LOCK, 0 /* no AFSDB */);
	if (!tc)
	    goto bad;

	if (afs_NewCell(acellName, 0, CAlias, (char *) 0, 0, 0,
			timeout, tc->cellName)) {
	    afs_PutCell(tc, READ_LOCK);
	    goto bad;
	}

	afs_PutCell(tc, READ_LOCK);
    }

    if (realName)
	afs_osi_Free(realName, strlen(realName) + 1);
    return afs_GetCellByName2(acellName, locktype, 0);

bad:
    if (realName)
	afs_osi_Free(realName, strlen(realName) + 1);
    return (struct cell *) 0;
}


struct cell *afs_GetCellByName2(acellName, locktype, trydns)
    register char *acellName;
    afs_int32 locktype;
    char trydns;
{
    register struct cell *tc;
    register struct afs_q *cq, *tq;
    int didAlias = 0;

    AFS_STATCNT(afs_GetCellByName);
retry:
    ObtainWriteLock(&afs_xcell,100);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	if (!afs_strcasecmp(tc->cellName, acellName)) {
	    QRemove(&tc->lruq);
	    QAdd(&CellLRU, &tc->lruq);
	    ReleaseWriteLock(&afs_xcell);
	    afs_RefreshCell(tc);
	    if ((tc->states & CAlias) && (didAlias == 0)) {
		acellName = tc->realName;
		if (!acellName) return (struct cell *) 0;
		didAlias = 1;
		goto retry;
	    }
	    return tc;
	}
    }
    ReleaseWriteLock(&afs_xcell);
    return trydns ? afs_GetCellByName_Dns(acellName, locktype)
		  : (struct cell *) 0;

} /*afs_GetCellByName2*/


struct cell *afs_GetCellByName(acellName, locktype)
    register char *acellName;
    afs_int32 locktype;
{
    return afs_GetCellByName2(acellName, locktype, 1);

} /*afs_GetCellByName*/

static struct cell *afs_GetCellInternal(acell, locktype, holdxcell)
    register afs_int32 acell;
    afs_int32 locktype;
    int holdxcell;
{
    register struct cell *tc;
    register struct afs_q *cq, *tq;

    AFS_STATCNT(afs_GetCell);
    if (acell == 1 && afs_rootcell) return afs_rootcell;
    if (holdxcell)
	ObtainWriteLock(&afs_xcell,101);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	if (tc->cell == acell) {
	    QRemove(&tc->lruq);
	    QAdd(&CellLRU, &tc->lruq);
	    if (holdxcell)
		ReleaseWriteLock(&afs_xcell);
	    afs_RefreshCell(tc);
	    return tc;
	}
    }
    if (holdxcell)
	ReleaseWriteLock(&afs_xcell);
    return (struct cell *) 0;

} /*afs_GetCell*/

struct cell *afs_GetCell(acell, locktype)
    register afs_int32 acell;
    afs_int32 locktype;
{
    return afs_GetCellInternal(acell, locktype, 1);
}

/* This is only to be called if the caller is already holding afs_xcell */
struct cell *afs_GetCellNoLock(acell, locktype)
    register afs_int32 acell;
    afs_int32 locktype;
{
    return afs_GetCellInternal(acell, locktype, 0);
}

struct cell *afs_GetCellByIndex(cellindex, locktype, refresh)
    register afs_int32 cellindex;
    afs_int32 locktype;
    afs_int32 refresh;
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
	    if (refresh) afs_RefreshCell(tc);
	    return tc;
	}
    }
    ReleaseWriteLock(&afs_xcell);
    return (struct cell *) 0;

} /*afs_GetCellByIndex*/


struct cell *afs_GetRealCellByIndex(cellindex, locktype, refresh)
    register afs_int32 cellindex;
    afs_int32 locktype;
    afs_int32 refresh;
{
    register struct cell *tc;
    register struct afs_q *cq, *tq;

    AFS_STATCNT(afs_GetCellByIndex);
    ObtainWriteLock(&afs_xcell,102);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	if (tc->realcellIndex == cellindex) {
	    QRemove(&tc->lruq);
	    QAdd(&CellLRU, &tc->lruq);
	    ReleaseWriteLock(&afs_xcell);
	    if (refresh) afs_RefreshCell(tc);
	    return tc;
	}
    }
    ReleaseWriteLock(&afs_xcell);
    return (struct cell *) 0;
} /*afs_GetRealCellByIndex*/


afs_int32 afs_NewCell(acellName, acellHosts, aflags, linkedcname, fsport, vlport, timeout, aliasFor)
    int aflags;
    char *acellName;
    register afs_int32 *acellHosts;
    char *linkedcname;
    u_short fsport, vlport;
    int timeout;
    char *aliasFor;
{
    register struct cell *tc, *tcl=0;
    register afs_int32 i, newc=0, code=0;
    register struct afs_q *cq, *tq;

    AFS_STATCNT(afs_NewCell);
    if (!(aflags & CAlias) && *acellHosts == 0)
	/* need >= one host to gen cell # */
	return EINVAL;

    ObtainWriteLock(&afs_xcell,103);

    /* Find the cell and mark its servers as not down but gone */
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	if (afs_strcasecmp(tc->cellName, acellName) == 0) {
	    /* if the cell we've found has the correct name but no timeout,
	     * and we're called with a non-zero timeout, bail out:  never
	     * override static configuration entries with AFSDB ones. */
	    if (timeout && !tc->timeout) {
		ReleaseWriteLock(&afs_xcell);
		return 0;
	    }
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
	memset((char *)tc, 0, sizeof(*tc));
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
	if (!aflags & CAlias) {
	    tc->realcellIndex = afs_realcellindex++;
	} else {
	    tc->realcellIndex = -1;
	}
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

    /* Allow converting an alias into a real cell */
    if (!(aflags & CAlias)) {
	tc->states &= ~CAlias;
	tc->realcellIndex = afs_realcellindex++;
    }
 
    memset((char *)tc->cellHosts, 0, sizeof(tc->cellHosts));
    if (aflags & CAlias) {
	if (!aliasFor) {
	    code = EINVAL;
	    goto bad;
	}
	if (tc->realName) afs_osi_Free(tc->realName, strlen(tc->realName)+1);
	tc->realName = (char *) afs_osi_Alloc(strlen(aliasFor)+1);
	strcpy(tc->realName, aliasFor);
	goto done;
    }

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
done:
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

