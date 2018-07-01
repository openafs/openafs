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

#include <roken.h>

#include <afs/opr.h>
#ifdef AFS_PTHREAD_ENV
# include <opr/lock.h>
#else
# include <opr/lockstub.h>
#endif
#include <lock.h>
#include <rx/rx.h>
#include <afs/afsutil.h>

#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

/*! \file
 * General Ubik Goal:
 * The goal is to provide reliable operation among N servers, such that any
 * server can crash with the remaining servers continuing operation within a
 * short period of time.  While a \b short outage is acceptable, this time
 * should be order of 3 minutes or less.
 *
 * Theory of operation:
 *
 * Note: #SMALLTIME and #BIGTIME are essentially the same time value, separated
 * only by the clock skew, #MAXSKEW.  In general, if you are making guarantees
 * for someone else, promise them no more than #SMALLTIME seconds of whatever
 * invariant you provide.  If you are waiting to be sure some invariant is now
 * \b false, wait at least #BIGTIME seconds to be sure that #SMALLTIME seconds
 * has passed at the other site.
 *
 * Now, back to the design:
 * One site in the collection is a special site, designated the \b sync site.
 * The sync site sends periodic messages, which can be thought of as
 * keep-alive messages.  When a non-sync site hears from the sync site, it
 * knows that it is getting updates for the next #SMALLTIME seconds from that
 * sync site.
 *
 * If a server does not hear from the sync site in #SMALLTIME seconds, it
 * determines that it no longer is getting updates, and thus refuses to give
 * out potentially out-of-date data.  If a sync site can not muster a majority
 * of servers to agree that it is the sync site, then there is a possibility
 * that a network partition has occurred, allowing another server to claim to
 * be the sync site.  Thus, any time that the sync site has not heard from a
 * majority of the servers in the last #SMALLTIME seconds, it voluntarily
 * relinquishes its role as sync site.
 *
 * While attempting to nominate a new sync site, certain rules apply.  First,
 * a server can not reply "ok" (return 1 from ServBeacon) to two different
 * hosts in less than #BIGTIME seconds; this allows a server that has heard
 * affirmative replies from a majority of the servers to know that no other
 * server in the network has heard enough affirmative replies in the last
 * #BIGTIME seconds to become sync site, too.  The variables #ubik_lastYesTime
 * and #lastYesHost are used by all servers to keep track of which host they
 * have last replied affirmatively to, when queried by a potential new sync
 * site.
 *
 * Once a sync site has become a sync site, it periodically sends beacon
 * messages with a parameter of 1, indicating that it already has determined
 * it is supposed to be the sync site.  The servers treat such a message as a
 * guarantee that no other site will become sync site for the next #SMALLTIME
 * seconds.  In the interim, these servers can answer a query concerning which
 * site is the sync site without any communication with any server.  The
 * variables #lastBeaconArrival and #lastBeaconHost are used by all servers to
 * keep track of which sync site has last contacted them.
 *
 * One complication occurs while nominating a new sync site: each site may be
 * trying to nominate a different site (based on the value of #lastYesHost),
 * yet we must nominate the smallest host (under some order), to prevent this
 * process from looping.  The process could loop by having each server give
 * one vote to another server, but with no server getting a majority of the
 * votes.  To avoid this, we try to withhold our votes for the server with the
 * lowest internet address (an easy-to-generate order).  To this effect, we
 * keep track (in #lowestTime and #lowestHost) of the lowest server trying to
 * become a sync site.  We wait for this server unless there is already a sync
 * site (indicated by ServBeacon's parameter being 1).
 */

afs_int32 ubik_debugFlag = 0;	/*!< print out debugging messages? */

struct vote_data vote_globals;


/*!
 * \brief Decide if we should try to become sync site.
 *
 * The basic rule is that we
 * don't run if there is a valid sync site and it ain't us (we have to run if
 * it is us, in order to keep our votes).  If there is no sync site, then we
 * want to run if we're the lowest numbered host running, otherwise we defer to
 * the lowest host.  However, if the lowest host hasn't been heard from for a
 * while, then we start running again, in case he crashed.
 *
 * \return true if we should run, and false otherwise.
 */
int
uvote_ShouldIRun(void)
{
    afs_int32 now;
    int code = 1; /* default to yes */

    if (amIClone) {
	return 0;		/* if we cannot be the sync-site, do not ask for votes */
    }

    UBIK_VOTE_LOCK;
    now = FT_ApproxTime();
    if (BIGTIME + vote_globals.ubik_lastYesTime < now)
	goto done;
    if (vote_globals.lastYesState && vote_globals.lastYesHost != ubik_host[0]) {
	code = 0;		/* other guy is sync site, leave him alone */
	goto done;
    }
    if (ntohl((afs_uint32)vote_globals.lastYesHost) < ntohl((afs_uint32)ubik_host[0])) {
	code = 0;		/* if someone is valid and better than us, don't run */
	goto done;
    }

done:
    UBIK_VOTE_UNLOCK;
    return code;
}

/*!
 * \brief Return the current synchronization site, if any.
 *
 * Simple approach: if the
 * last guy we voted yes for claims to be the sync site, then we we're happy to
 * use that guy for a sync site until the time his mandate expires.  If the guy
 * does not claim to be sync site, then, of course, there's none.
 *
 * In addition, if we lost the sync, we set #urecovery_syncSite to an invalid
 * value, indicating that we no longer know which version of the dbase is the
 * one we should have.  We'll get a new one when we next hear from the sync
 * site.
 *
 * \return 0 or currently valid sync site.  It can return our own
 * address, if we're the sync site.
 */
afs_int32
uvote_GetSyncSite(void)
{
    afs_int32 now;
    afs_int32 code;

    UBIK_VOTE_LOCK;
    if (!vote_globals.lastYesState)
	code = 0;
    else {
	now = FT_ApproxTime();
	if (SMALLTIME + vote_globals.lastYesClaim < now)
	    code = 0;		/* last guy timed out */
	else
	    code = vote_globals.lastYesHost;
    }
    UBIK_VOTE_UNLOCK;
    return code;
}

/*!
 * \brief called by the sync site to handle vote beacons; if aconn is null, this is a
 * local call
 *
 * \returns 0 or time when the vote was sent.  It returns 0 if we are
 * not voting for this sync site, or the time we actually voted yes, if
 * non-zero.
 */
afs_int32
SVOTE_Beacon(struct rx_call * rxcall, afs_int32 astate,
	     afs_int32 astart, struct ubik_version * avers,
	     struct ubik_tid * atid)
{
    afs_int32 otherHost;
    afs_int32 now;
    afs_int32 vote;
    struct rx_connection *aconn;
    struct rx_peer *rxp;
    struct ubik_server *ts;
    int isClone = 0;
    char hoststr[16];

    if (rxcall) {		/* caller's host */
	aconn = rx_ConnectionOf(rxcall);
	rxp = rx_PeerOf(aconn);
	otherHost = rx_HostOf(rxp);

	/* get the primary interface address for this host.  */
	/* This is the identifier that ubik uses. */
	otherHost = ubikGetPrimaryInterfaceAddr(otherHost);
	if (!otherHost) {
	    ubik_dprint("Received beacon from unknown host %s\n",
			afs_inet_ntoa_r(rx_HostOf(rxp), hoststr));
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
		afs_inet_ntoa_r(otherHost, hoststr));

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
    UBIK_VOTE_LOCK;
    now = FT_ApproxTime();	/* close to current time */
    if (!isClone
	&& (ntohl((afs_uint32)otherHost) <= ntohl((afs_uint32)vote_globals.lowestHost)
	    || vote_globals.lowestTime + BIGTIME < now)) {
	vote_globals.lowestTime = now;
	vote_globals.lowestHost = otherHost;
    }
    /* why do we need this next check?  Consider the case where each of two
     * servers decides the other is lowestHost.  Each stops sending beacons
     * 'cause the other is there.  Not obvious that this process terminates:
     * i.e. each guy could restart procedure and again think other side is
     * lowest.  Need to prove: if one guy in the system is lowest and knows
     * he's lowest, these loops don't occur.  because if someone knows he's
     * lowest, he will send out beacons telling others to vote for him. */
    if (!amIClone
	&& (ntohl((afs_uint32) ubik_host[0]) <= ntohl((afs_uint32)vote_globals.lowestHost)
	    || vote_globals.lowestTime + BIGTIME < now)) {
	vote_globals.lowestTime = now;
	vote_globals.lowestHost = ubik_host[0];
    }

    /* tell if we've heard from a sync site recently (even if we're not voting
     * for this dude yet).  After a while, time the guy out. */
    if (astate) {		/* this guy is a sync site */
	vote_globals.syncHost = otherHost;
	vote_globals.syncTime = now;
    } else if (vote_globals.syncTime + BIGTIME < now) {
	if (vote_globals.syncHost) {
	    ubik_dprint
		("Ubik: Lost contact with sync-site %s (NOT in quorum)\n",
		 afs_inet_ntoa_r(vote_globals.syncHost, hoststr));
	}
	vote_globals.syncHost = 0;
    }

    /* decide how to vote */
    vote = 0;			/* start off voting no */

    /* if we this guy isn't a sync site, we don't really have to vote for him.
     * We get to apply some heuristics to try to avoid weird oscillation sates
     * in the voting procedure. */
    if (astate == 0) {
	/* in here only if this guy doesn't claim to be a sync site */

	/* lowestHost is also trying for our votes, then just say no. */
	if (ntohl(vote_globals.lowestHost) != ntohl(otherHost)) {
	    goto done_zero;
	}

	/* someone else *is* a sync site, just say no */
	if (vote_globals.syncHost && vote_globals.syncHost != otherHost)
	    goto done_zero;
    } else if (vote_globals.lastYesHost == 0xffffffff && otherHost == ubik_host[0]) {
	/* fast startup if this is the only non-clone */
	int i = 0;
	for (ts = ubik_servers; ts; ts = ts->next) {
	    if (ts->addr[0] == otherHost)
		continue;
	    if (!ts->isClone)
		i++;
	}
	if (!i)
	    vote_globals.lastYesHost = otherHost;
    }


    if (isClone)
	goto done_zero;		/* clone never can become sync site */

    /* Don't promise sync site support to more than one host every BIGTIME
     * seconds.  This is the heart of our invariants in this system. */
    if (vote_globals.ubik_lastYesTime + BIGTIME < now || otherHost == vote_globals.lastYesHost) {
	if ((vote_globals.ubik_lastYesTime + BIGTIME < now) || (otherHost != vote_globals.lastYesHost)
	    || (vote_globals.lastYesState != astate)) {
	    /* A new vote or a change in the vote or changed quorum */
	    ubik_dprint("Ubik: vote 'yes' for %s %s\n",
			afs_inet_ntoa_r(otherHost, hoststr),
			(astate ? "(in quorum)" : "(NOT in quorum)"));
	}

	vote = now;		/* vote yes */
	vote_globals.ubik_lastYesTime = now;	/* remember when we voted yes */
	vote_globals.lastYesClaim = astart;	/* remember for computing when sync site expires */
	vote_globals.lastYesHost = otherHost;	/* and who for */
	vote_globals.lastYesState = astate;	/* remember if site is a sync site */
	vote_globals.ubik_dbVersion = *avers;	/* resync value */
	vote_globals.ubik_dbTid = *atid;	/* transaction id, if any, of active trans */
	UBIK_VOTE_UNLOCK;
	DBHOLD(ubik_dbase);
	urecovery_CheckTid(atid, 0);	/* check if current write trans needs aborted */
	DBRELE(ubik_dbase);
    } else {
	UBIK_VOTE_UNLOCK;
    }
    return vote;
done_zero:
    UBIK_VOTE_UNLOCK;
    return 0;
}

/*!
 * \brief Handle per-server debug command, where 0 is the first server.
 *
 * Basic network debugging hooks.
 */
afs_int32
SVOTE_SDebug(struct rx_call * rxcall, afs_int32 awhich,
	     struct ubik_sdebug * aparm)
{
    afs_int32 code, isClone;
    code = SVOTE_XSDebug(rxcall, awhich, aparm, &isClone);
    return code;
}

afs_int32
SVOTE_XSDebug(struct rx_call * rxcall, afs_int32 awhich,
	      struct ubik_sdebug * aparm, afs_int32 * isclone)
{
    struct ubik_server *ts;
    int i;
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
SVOTE_XDebug(struct rx_call * rxcall, struct ubik_debug * aparm,
	     afs_int32 * isclone)
{
    afs_int32 code;

    code = SVOTE_Debug(rxcall, aparm);
    *isclone = amIClone;
    return code;
}

/*!
 * \brief Handle basic network debug command.  This is the global state dumper.
 */
afs_int32
SVOTE_Debug(struct rx_call * rxcall, struct ubik_debug * aparm)
{
    int i;
    /* fill in the basic debug structure.  Note the the RPC protocol transfers,
     * integers in host order. */

    aparm->now = FT_ApproxTime();
    aparm->lastYesTime = vote_globals.ubik_lastYesTime;
    aparm->lastYesHost = ntohl(vote_globals.lastYesHost);
    aparm->lastYesState = vote_globals.lastYesState;
    aparm->lastYesClaim = vote_globals.lastYesClaim;
    aparm->lowestHost = ntohl(vote_globals.lowestHost);
    aparm->lowestTime = vote_globals.lowestTime;
    aparm->syncHost = ntohl(vote_globals.syncHost);
    aparm->syncTime = vote_globals.syncTime;
    memcpy(&aparm->syncVersion, &vote_globals.ubik_dbVersion, sizeof(struct ubik_version));
    memcpy(&aparm->syncTid, &vote_globals.ubik_dbTid, sizeof(struct ubik_tid));

    /* fill in all interface addresses of myself in hostbyte order */
    for (i = 0; i < UBIK_MAX_INTERFACE_ADDR; i++)
	aparm->interfaceAddr[i] = ntohl(ubik_host[i]);

    aparm->amSyncSite = beacon_globals.ubik_amSyncSite;
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

    aparm->epochTime = version_globals.ubik_epochTime;

    return 0;
}

afs_int32
SVOTE_SDebugOld(struct rx_call * rxcall, afs_int32 awhich,
		struct ubik_sdebug_old * aparm)
{
    struct ubik_server *ts;

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


/*!
 * \brief Handle basic network debug command.  This is the global state dumper.
 */
afs_int32
SVOTE_DebugOld(struct rx_call * rxcall,
	       struct ubik_debug_old * aparm)
{

    /* fill in the basic debug structure.  Note the the RPC protocol transfers,
     * integers in host order. */

    aparm->now = FT_ApproxTime();
    aparm->lastYesTime = vote_globals.ubik_lastYesTime;
    aparm->lastYesHost = ntohl(vote_globals.lastYesHost);
    aparm->lastYesState = vote_globals.lastYesState;
    aparm->lastYesClaim = vote_globals.lastYesClaim;
    aparm->lowestHost = ntohl(vote_globals.lowestHost);
    aparm->lowestTime = vote_globals.lowestTime;
    aparm->syncHost = ntohl(vote_globals.syncHost);
    aparm->syncTime = vote_globals.syncTime;
    memcpy(&aparm->syncVersion, &vote_globals.ubik_dbVersion, sizeof(struct ubik_version));
    memcpy(&aparm->syncTid, &vote_globals.ubik_dbTid, sizeof(struct ubik_tid));

    aparm->amSyncSite = beacon_globals.ubik_amSyncSite;
    ubeacon_Debug((ubik_debug *)aparm);

    udisk_Debug((ubik_debug *)aparm);

    ulock_Debug((ubik_debug *)aparm);

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

    aparm->epochTime = version_globals.ubik_epochTime;

    return 0;
}


/*!
 * \brief Get the sync site; called by remote servers to find where they should go.
 */
afs_int32
SVOTE_GetSyncSite(struct rx_call * rxcall,
		  afs_int32 * ahost)
{
    afs_int32 temp;

    temp = uvote_GetSyncSite();
    *ahost = ntohl(temp);
    return 0;
}

void
ubik_dprint_25(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vViceLog(25, (format, ap));
    va_end(ap);
}

void
ubik_dprint(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    vViceLog(5, (format, ap));
    va_end(ap);
}

void
ubik_vprint(const char *format, va_list ap)
{
    vViceLog(0, (format, ap));
}

void
ubik_print(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    ubik_vprint(format, ap);
    va_end(ap);
}

/*!
 * \brief Called once/run to init the vote module
 */
int
uvote_Init(void)
{
    UBIK_VOTE_LOCK;
    /* pretend we just voted for someone else, since we just restarted */
    vote_globals.ubik_lastYesTime = FT_ApproxTime();

    /* Initialize globals */
    vote_globals.lastYesHost = 0xffffffff;
    vote_globals.lastYesClaim = 0;
    vote_globals.lastYesState = 0;
    vote_globals.lowestTime = 0;
    vote_globals.lowestHost = 0xffffffff;
    vote_globals.syncTime = 0;
    vote_globals.syncHost = 0;
    UBIK_VOTE_UNLOCK;

    return 0;
}

void
uvote_set_dbVersion(struct ubik_version version) {
    UBIK_VOTE_LOCK;
    vote_globals.ubik_dbVersion = version;
    UBIK_VOTE_UNLOCK;
}

/* Compare given version to current DB version.  Return true if equal. */
int
uvote_eq_dbVersion(struct ubik_version version) {
    int ret = 0;

    UBIK_VOTE_LOCK;
    if (vote_globals.ubik_dbVersion.epoch == version.epoch && vote_globals.ubik_dbVersion.counter == version.counter) {
	ret = 1;
    }
    UBIK_VOTE_UNLOCK;
    return ret;
}

/*!
 * \brief Check if there is a sync site and whether we have a given db version
 *
 * \return 1 if there is a valid sync site, and the given db version matches the sync site's
 */

int
uvote_HaveSyncAndVersion(struct ubik_version version)
{
    afs_int32 now;
    int code;

    UBIK_VOTE_LOCK;
    now = FT_ApproxTime();
    if (!vote_globals.lastYesState || (SMALLTIME + vote_globals.lastYesClaim < now) ||
			vote_globals.ubik_dbVersion.epoch != version.epoch ||
			vote_globals.ubik_dbVersion.counter != version.counter)
	code = 0;
    else
	code = 1;
    UBIK_VOTE_UNLOCK;
    return code;
}
