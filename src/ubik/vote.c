/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif
#include <afs/afsutil.h>
#include <lock.h>
#include <string.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/afsutil.h>
#include <time.h>

#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

/*
    General Ubik Goal:
    The goal is to provide reliable operation among N servers, such that any
    server can crash with the remaining servers continuing operation within a
    short period of time.  While a *short* outage is acceptable, this time
    should be order of 3 minutes or less.
    
    Theory of operation:
    
    Note: SMALLTIME and BIGTIME are essentially the same time value, separated
    only by the clock skew, MAXSKEW.  In general, if you are making guarantees
    for someone else, promise them no more than SMALLTIME seconds of whatever
    invariant you provide.  If you are waiting to be sure some invariant is now
    *false*, wait at least BIGTIME seconds to be sure that SMALLTIME seconds
    has passed at the other site.

    Now, back to the design:
    One site in the collection is a special site, designated the *sync* site.
    The sync site sends periodic messages, which can be thought of as
    keep-alive messages.  When a non-sync site hears from the sync site, it
    knows that it is getting updates for the next SMALLTIME seconds from that
    sync site.
    
    If a server does not hear from the sync site in SMALLTIME seconds, it
    determines that it no longer is getting updates, and thus refuses to give
    out potentially out-of-date data.  If a sync site can not muster a majority
    of servers to agree that it is the sync site, then there is a possibility
    that a network partition has occurred, allowing another server to claim to
    be the sync site.  Thus, any time that the sync site has not heard from a
    majority of the servers in the last SMALLTIME seconds, it voluntarily
    relinquishes its role as sync site.
    
    While attempting to nominate a new sync site, certain rules apply.  First,
    a server can not reply "ok" (return 1 from ServBeacon) to two different
    hosts in less than BIGTIME seconds; this allows a server that has heard
    affirmative replies from a majority of the servers to know that no other
    server in the network has heard enough affirmative replies in the last
    BIGTIME seconds to become sync site, too.  The variables ubik_lastYesTime
    and lastYesHost are used by all servers to keep track of which host they
    have last replied affirmatively to, when queried by a potential new sync
    site.
    
    Once a sync site has become a sync site, it periodically sends beacon
    messages with a parameter of 1, indicating that it already has determined
    it is supposed to be the sync site.  The servers treat such a message as a
    guarantee that no other site will become sync site for the next SMALLTIME
    seconds.  In the interim, these servers can answer a query concerning which
    site is the sync site without any communication with any server.  The
    variables lastBeaconArrival and lastBeaconHost are used by all servers to
    keep track of which sync site has last contacted them.
    
    One complication occurs while nominating a new sync site: each site may be
    trying to nominate a different site (based on the value of lastYesHost),
    yet we must nominate the smallest host (under some order), to prevent this
    process from looping.  The process could loop by having each server give
    one vote to another server, but with no server getting a majority of the
    votes.  To avoid this, we try to withhold our votes for the server with the
    lowest internet address (an easy-to-generate order).  To this effect, we
    keep track (in lowestTime and lowestHost) of the lowest server trying to
    become a sync site.  We wait for this server unless there is already a sync
    site (indicated by ServBeacon's parameter being 1).
*/

afs_int32 ubik_debugFlag = 0;	/* print out debugging messages? */

/* these statics are used by all sites in nominating new sync sites */
afs_int32 ubik_lastYesTime = 0;	/* time we sent the last *yes* vote */
static afs_uint32 lastYesHost = 0xffffffff;	/* host to which we sent *yes* vote */
/* Next is time sync site began this vote: guarantees sync site until this + SMALLTIME */
static afs_int32 lastYesClaim = 0;
static int lastYesState = 0;	/* did last site we voted for claim to be sync site? */

/* used to guarantee that nomination process doesn't loop */
static afs_int32 lowestTime = 0;
static afs_uint32 lowestHost = 0xffffffff;
static afs_int32 syncTime = 0;
static afs_int32 syncHost = 0;

/* used to remember which dbase version is the one at the sync site (for non-sync sites */
struct ubik_version ubik_dbVersion;	/* sync site's dbase version */
struct ubik_tid ubik_dbTid;	/* sync site's tid, or 0 if none */

/* decide if we should try to become sync site.  The basic rule is that we
 * don't run if there is a valid sync site and it ain't us (we have to run if
 * it is us, in order to keep our votes).  If there is no sync site, then we
 * want to run if we're the lowest numbered host running, otherwise we defer to
 * the lowest host.  However, if the lowest host hasn't been heard from for a
 * while, then we start running again, in case he crashed.
 *
 * This function returns true if we should run, and false otherwise.
 */
int
uvote_ShouldIRun(void)
{
    register afs_int32 now;

    now = FT_ApproxTime();
    if (BIGTIME + ubik_lastYesTime < now)
	return 1;		/* no valid guy even trying */
    if (lastYesState && lastYesHost != ubik_host[0])
	return 0;		/* other guy is sync site, leave him alone */
    if (ntohl((afs_uint32) lastYesHost) < ntohl((afs_uint32) ubik_host[0]))
	return 0;		/* if someone is valid and better than us, don't run */
    /* otherwise we should run */
    return 1;
}

/* Return the current synchronization site, if any.  Simple approach: if the
 * last guy we voted yes for claims to be the sync site, then we we're happy to
 * use that guy for a sync site until the time his mandate expires.  If the guy
 * does not claim to be sync site, then, of course, there's none.
 *
 * In addition, if we lost the sync, we set urecovery_syncSite to an invalid
 * value, indicating that we no longer know which version of the dbase is the
 * one we should have.  We'll get a new one when we next hear from the sync
 * site.
 *
 * This function returns 0 or currently valid sync site.  It can return our own
 * address, if we're the sync site.
 */
afs_int32
uvote_GetSyncSite(void)
{
    register afs_int32 now;
    register afs_int32 code;

    if (!lastYesState)
	code = 0;
    else {
	now = FT_ApproxTime();
	if (SMALLTIME + lastYesClaim < now)
	    code = 0;		/* last guy timed out */
	else
	    code = lastYesHost;
    }
    return code;
}

/* called by the sync site to handle vote beacons; if aconn is null, this is a
 * local call; returns 0 or time whe the vote was sent.  It returns 0 if we are
 * not voting for this sync site, or the time we actually voted yes, if
 * non-zero.
 */
afs_int32
SVOTE_Beacon(register struct rx_call * rxcall, afs_int32 astate,
	     afs_int32 astart, struct ubik_version * avers,
	     struct ubik_tid * atid)
{
    register afs_int32 otherHost;
    register afs_int32 now;
    afs_int32 vote;
    struct rx_connection *aconn;
    struct rx_peer *rxp;
    struct ubik_server *ts;
    int isClone = 0;

    now = FT_ApproxTime();	/* close to current time */
    if (rxcall) {		/* caller's host */
	aconn = rx_ConnectionOf(rxcall);
	rxp = rx_PeerOf(aconn);
	otherHost = rx_HostOf(rxp);

	/* get the primary interface address for this host.  */
	/* This is the identifier that ubik uses. */
	otherHost = ubikGetPrimaryInterfaceAddr(otherHost);
	if (!otherHost) {
	    ubik_dprint("Received beacon from unknown host %s\n",
			afs_inet_ntoa(rx_HostOf(rxp)));
	    return 0;		/* I don't know about you: vote no */
	}
	for (ts = ubik_servers; ts; ts = ts->next) {
	    if (ts->addr[0] == otherHost)
		break;
	}
	if (!ts)
	    ubik_dprint("Unknown host %x has sent a beacon\n", otherHost);
	if (ts && ts->isClone)
	    isClone = 1;
    } else {
	otherHost = ubik_host[0];	/* this host */
	isClone = amIClone;
    }

    ubik_dprint("Received beacon type %d from host %s\n", astate,
		afs_inet_ntoa(otherHost));

    /* compute the lowest server we've heard from.  We'll try to only vote for
     * this dude if we don't already have a synchronization site.  Also, don't
     * let a very old lowestHost confusing things forever.  We pick a new
     * lowestHost after BIGTIME seconds to limit the damage if this host
     * actually crashes.  Finally, we also count in this computation: don't
     * pick someone else if we're even better!
     * 
     * Note that the test below must be <=, not <, so that we keep refreshing
     * lowestTime.  Otherwise it will look like we haven't heard from
     * lowestHost in a while and another host could slip in.  */


    /* First compute the lowest host we've heard from, whether we want them
     * for a sync site or not.  If we haven't heard from a site in BIGTIME
     * seconds, we ignore its presence in lowestHost: it may have crashed.
     * Note that we don't ever let anyone appear in our lowestHost if we're
     * lower than them, 'cause we know we're up. */
    /* But do not consider clones for lowesHost since they never may become
     * sync site */
    if (!isClone
	&& (ntohl((afs_uint32) otherHost) <= ntohl((afs_uint32) lowestHost)
	    || lowestTime + BIGTIME < now)) {
	lowestTime = now;
	lowestHost = otherHost;
    }
    /* why do we need this next check?  Consider the case where each of two
     * servers decides the other is lowestHost.  Each stops sending beacons
     * 'cause the other is there.  Not obvious that this process terminates:
     * i.e. each guy could restart procedure and again think other side is
     * lowest.  Need to prove: if one guy in the system is lowest and knows
     * he's lowest, these loops don't occur.  because if someone knows he's
     * lowest, he will send out beacons telling others to vote for him. */
    if (!amIClone
	&& (ntohl((afs_uint32) ubik_host[0]) <= ntohl((afs_uint32) lowestHost)
	    || lowestTime + BIGTIME < now)) {
	lowestTime = now;
	lowestHost = ubik_host[0];
    }

    /* tell if we've heard from a sync site recently (even if we're not voting
     * for this dude yet).  After a while, time the guy out. */
    if (astate) {		/* this guy is a sync site */
	syncHost = otherHost;
	syncTime = now;
    } else if (syncTime + BIGTIME < now) {
	if (syncHost) {
	    ubik_dprint
		("Ubik: Lost contact with sync-site %d.%d.%d.%d (NOT in quorum)\n",
		 ((syncHost >> 24) & 0xff), ((syncHost >> 16) & 0xff),
		 ((syncHost >> 8) & 0xff), (syncHost & 0xff));
	}
	syncHost = 0;
    }

    /* decide how to vote */
    vote = 0;			/* start off voting no */

    /* if we this guy isn't a sync site, we don't really have to vote for him.
     * We get to apply some heuristics to try to avoid weird oscillation sates
     * in the voting procedure. */
    if (astate == 0) {
	/* in here only if this guy doesn't claim to be a sync site */

	/* lowestHost is also trying for our votes, then just say no. */
	if (ntohl(lowestHost) != ntohl(otherHost)) {
	    return 0;
	}

	/* someone else *is* a sync site, just say no */
	if (syncHost && syncHost != otherHost)
	    return 0;
    } else /* fast startup if this is the only non-clone */ if (lastYesHost ==
								0xffffffff
								&& otherHost
								==
								ubik_host[0])
    {
	int i = 0;
	for (ts = ubik_servers; ts; ts = ts->next) {
	    if (ts->addr[0] == otherHost)
		continue;
	    if (!ts->isClone)
		i++;
	}
	if (!i)
	    lastYesHost = otherHost;
    }


    if (isClone)
	return 0;		/* clone never can become sync site */

    /* Don't promise sync site support to more than one host every BIGTIME
     * seconds.  This is the heart of our invariants in this system. */
    if (ubik_lastYesTime + BIGTIME < now || otherHost == lastYesHost) {
	if ((ubik_lastYesTime + BIGTIME < now) || (otherHost != lastYesHost)
	    || (lastYesState != astate)) {
	    /* A new vote or a change in the vote or changed quorum */
	    ubik_dprint("Ubik: vote 'yes' for %s %s\n",
			afs_inet_ntoa(otherHost),
			(astate ? "(in quorum)" : "(NOT in quorum)"));
	}

	vote = now;		/* vote yes */
	ubik_lastYesTime = now;	/* remember when we voted yes */
	lastYesClaim = astart;	/* remember for computing when sync site expires */
	lastYesHost = otherHost;	/* and who for */
	lastYesState = astate;	/* remember if site is a sync site */
	ubik_dbVersion = *avers;	/* resync value */
	ubik_dbTid = *atid;	/* transaction id, if any, of active trans */
	urecovery_CheckTid(atid);	/* check if current write trans needs aborted */
    }
    return vote;
}

/* handle per-server debug command, where 0 is the first server.  Basic network
   debugging hooks. */
afs_int32
SVOTE_SDebug(struct rx_call * rxcall, afs_int32 awhich,
	     register struct ubik_sdebug * aparm)
{
    afs_int32 code, isClone;
    code = SVOTE_XSDebug(rxcall, awhich, aparm, &isClone);
    return code;
}

afs_int32
SVOTE_XSDebug(struct rx_call * rxcall, afs_int32 awhich,
	      register struct ubik_sdebug * aparm, afs_int32 * isclone)
{
    register struct ubik_server *ts;
    register int i;
    for (ts = ubik_servers; ts; ts = ts->next) {
	if (awhich-- == 0) {
	    /* we're done */
	    aparm->addr = ntohl(ts->addr[0]);	/* primary interface */
	    for (i = 0; i < UBIK_MAX_INTERFACE_ADDR - 1; i++)
		aparm->altAddr[i] = ntohl(ts->addr[i + 1]);
	    aparm->lastVoteTime = ts->lastVoteTime;
	    aparm->lastBeaconSent = ts->lastBeaconSent;
	    memcpy(&aparm->remoteVersion, &ts->version,
		   sizeof(struct ubik_version));
	    aparm->lastVote = ts->lastVote;
	    aparm->up = ts->up;
	    aparm->beaconSinceDown = ts->beaconSinceDown;
	    aparm->currentDB = ts->currentDB;
	    *isclone = ts->isClone;
	    return 0;
	}
    }
    return 2;
}

afs_int32
SVOTE_XDebug(struct rx_call * rxcall, register struct ubik_debug * aparm,
	     afs_int32 * isclone)
{
    afs_int32 code;

    code = SVOTE_Debug(rxcall, aparm);
    *isclone = amIClone;
    return code;
}

/* handle basic network debug command.  This is the global state dumper */
afs_int32
SVOTE_Debug(struct rx_call * rxcall, register struct ubik_debug * aparm)
{
    int i;
    /* fill in the basic debug structure.  Note the the RPC protocol transfers,
     * integers in host order. */

    aparm->now = FT_ApproxTime();
    aparm->lastYesTime = ubik_lastYesTime;
    aparm->lastYesHost = ntohl(lastYesHost);
    aparm->lastYesState = lastYesState;
    aparm->lastYesClaim = lastYesClaim;
    aparm->lowestHost = ntohl(lowestHost);
    aparm->lowestTime = lowestTime;
    aparm->syncHost = ntohl(syncHost);
    aparm->syncTime = syncTime;

    /* fill in all interface addresses of myself in hostbyte order */
    for (i = 0; i < UBIK_MAX_INTERFACE_ADDR; i++)
	aparm->interfaceAddr[i] = ntohl(ubik_host[i]);

    aparm->amSyncSite = ubik_amSyncSite;
    ubeacon_Debug(aparm);

    udisk_Debug(aparm);

    ulock_Debug(aparm);

    /* Get the recovery state. The label of the database may not have 
     * been written yet but set the flag so udebug behavior remains.
     * Defect 9477.
     */
    aparm->recoveryState = urecovery_state;
    if ((urecovery_state & UBIK_RECSYNCSITE)
	&& (urecovery_state & UBIK_RECFOUNDDB)
	&& (urecovery_state & UBIK_RECHAVEDB)) {
	aparm->recoveryState |= UBIK_RECLABELDB;
    }
    memcpy(&aparm->syncVersion, &ubik_dbVersion, sizeof(struct ubik_version));
    memcpy(&aparm->syncTid, &ubik_dbTid, sizeof(struct ubik_tid));
    aparm->activeWrite = (ubik_dbase->flags & DBWRITING);
    aparm->tidCounter = ubik_dbase->tidCounter;

    if (ubik_currentTrans) {
	aparm->currentTrans = 1;
	if (ubik_currentTrans->type == UBIK_WRITETRANS)
	    aparm->writeTrans = 1;
	else
	    aparm->writeTrans = 0;
    } else {
	aparm->currentTrans = 0;
    }

    aparm->epochTime = ubik_epochTime;

    return 0;
}

afs_int32
SVOTE_SDebugOld(struct rx_call * rxcall, afs_int32 awhich,
		register struct ubik_sdebug_old * aparm)
{
    register struct ubik_server *ts;

    for (ts = ubik_servers; ts; ts = ts->next) {
	if (awhich-- == 0) {
	    /* we're done */
	    aparm->addr = ntohl(ts->addr[0]);	/* primary interface */
	    aparm->lastVoteTime = ts->lastVoteTime;
	    aparm->lastBeaconSent = ts->lastBeaconSent;
	    memcpy(&aparm->remoteVersion, &ts->version,
		   sizeof(struct ubik_version));
	    aparm->lastVote = ts->lastVote;
	    aparm->up = ts->up;
	    aparm->beaconSinceDown = ts->beaconSinceDown;
	    aparm->currentDB = ts->currentDB;
	    return 0;
	}
    }
    return 2;
}


/* handle basic network debug command.  This is the global state dumper */
afs_int32
SVOTE_DebugOld(struct rx_call * rxcall,
	       register struct ubik_debug_old * aparm)
{

    /* fill in the basic debug structure.  Note the the RPC protocol transfers,
     * integers in host order. */

    aparm->now = FT_ApproxTime();
    aparm->lastYesTime = ubik_lastYesTime;
    aparm->lastYesHost = ntohl(lastYesHost);
    aparm->lastYesState = lastYesState;
    aparm->lastYesClaim = lastYesClaim;
    aparm->lowestHost = ntohl(lowestHost);
    aparm->lowestTime = lowestTime;
    aparm->syncHost = ntohl(syncHost);
    aparm->syncTime = syncTime;

    aparm->amSyncSite = ubik_amSyncSite;
    ubeacon_Debug(aparm);

    udisk_Debug(aparm);

    ulock_Debug(aparm);

    /* Get the recovery state. The label of the database may not have 
     * been written yet but set the flag so udebug behavior remains.
     * Defect 9477.
     */
    aparm->recoveryState = urecovery_state;
    if ((urecovery_state & UBIK_RECSYNCSITE)
	&& (urecovery_state & UBIK_RECFOUNDDB)
	&& (urecovery_state & UBIK_RECHAVEDB)) {
	aparm->recoveryState |= UBIK_RECLABELDB;
    }
    memcpy(&aparm->syncVersion, &ubik_dbVersion, sizeof(struct ubik_version));
    memcpy(&aparm->syncTid, &ubik_dbTid, sizeof(struct ubik_tid));
    aparm->activeWrite = (ubik_dbase->flags & DBWRITING);
    aparm->tidCounter = ubik_dbase->tidCounter;

    if (ubik_currentTrans) {
	aparm->currentTrans = 1;
	if (ubik_currentTrans->type == UBIK_WRITETRANS)
	    aparm->writeTrans = 1;
	else
	    aparm->writeTrans = 0;
    } else {
	aparm->currentTrans = 0;
    }

    aparm->epochTime = ubik_epochTime;

    return 0;
}


/* get the sync site; called by remote servers to find where they should go */
afs_int32
SVOTE_GetSyncSite(register struct rx_call * rxcall,
		  register afs_int32 * ahost)
{
    register afs_int32 temp;

    temp = uvote_GetSyncSite();
    *ahost = ntohl(temp);
    return 0;
}

int
ubik_dprint(char *a, char *b, char *c, char *d, char *e, char *f, char *g,
	    char *h)
{
    ViceLog(5, (a, b, c, d, e, f, g, h));
    return 0;
}

int
ubik_print(char *a, char *b, char *c, char *d, char *e, char *f, char *g,
	   char *h)
{
    ViceLog(0, (a, b, c, d, e, f, g, h));
    return 0;
}

/* called once/run to init the vote module */
int
uvote_Init(void)
{
    /* pretend we just voted for someone else, since we just restarted */
    ubik_lastYesTime = FT_ApproxTime();
    return 0;
}
