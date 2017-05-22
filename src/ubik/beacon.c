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
#include <rx/rxkad.h>
#include <rx/rx_multi.h>
#include <afs/cellconfig.h>
#ifndef AFS_NT40_ENV
#include <afs/afsutil.h>
#endif

#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

/* These global variables were used to set the function to use to initialise
 * the client security layer. They are retained for backwards compatiblity with
 * legacy callers - the ubik_SetClientSecurityProcs() interface should be used
 * instead
 */
int (*ubik_CRXSecurityProc) (void *rock, struct rx_securityClass **,
                             afs_int32 *);
void *ubik_CRXSecurityRock;

/*! \name statics used to determine if we're the sync site */
static int nServers;		/*!< total number of servers */
static char amIMagic = 0;	/*!< is this host the magic host */
char amIClone = 0;		/*!< is this a clone which doesn't vote */
static char ubik_singleServer = 0;
/*\}*/
static int (*secLayerProc) (void *rock, struct rx_securityClass **,
			    afs_int32 *) = NULL;
static int (*tokenCheckProc) (void *rock) = NULL;
static void * securityRock = NULL;

afs_int32 ubikSecIndex;
struct rx_securityClass *ubikSecClass;

/* Values protected by the address lock */
struct addr_data addr_globals;

/* Values protected by the beacon lock */
struct beacon_data beacon_globals;

static int ubeacon_InitServerListCommon(afs_uint32 ame,
					struct afsconf_cell *info,
					char clones[],
					afs_uint32 aservers[]);
static int verifyInterfaceAddress(afs_uint32 *ame, struct afsconf_cell *info,
				  afs_uint32 aservers[]);

/*! \file
 * Module responsible for both deciding if we're currently the sync site,
 * and keeping collecting votes so as to stay sync site.
 *
 * The basic module contacts all of the servers it can, trying to get them to vote
 * for this server for sync site.  The vote request message (called a beacon message)
 * also specifies until which time this site claims to be the sync site, if at all, thus enabling
 * receiving sites to know how long the sync site guarantee is made for.
 *
 * Each  of these beacon messages is thus both a declaration of how long this site will
 * remain sync site, and an attempt to extend that time by collecting votes for a later
 * sync site extension.
 *
 * The voting module is responsible for choosing a reasonable time until which it promises
 * not to vote for someone else.  This parameter (BIG seconds) is not actually passed in
 * the interface (perhaps it should be?) but is instead a compile time constant that both
 * sides know about.

 * The beacon and vote modules work intimately together; the vote module decides how long
 * it should promise the beacon module its vote, and the beacon module takes all of these
 * votes and decides for how long it is the synchronization site.
 */

/*! \brief procedure called from debug rpc call to get this module's state for debugging */
void
ubeacon_Debug(struct ubik_debug *aparm)
{
    /* fill in beacon's state fields in the ubik_debug structure */
    aparm->syncSiteUntil = beacon_globals.syncSiteUntil;
    aparm->nServers = nServers;
}

static int
amSyncSite(void)
{
    afs_int32 now;
    afs_int32 rcode;

    /* special case for fast startup */
    if (nServers == 1 && !amIClone)
	return 1;		/* one guy is always the sync site */

    UBIK_BEACON_LOCK;
    if (beacon_globals.ubik_amSyncSite == 0 || amIClone)
	rcode = 0;		/* if I don't think I'm the sync site, say so */
    else {
	now = FT_ApproxTime();
	if (beacon_globals.syncSiteUntil <= now) {	/* if my votes have expired, say so */
	    if (beacon_globals.ubik_amSyncSite)
		ubik_dprint("Ubik: I am no longer the sync site\n");
	    beacon_globals.ubik_amSyncSite = 0;
	    beacon_globals.ubik_syncSiteAdvertised = 0;
	    rcode = 0;
	} else {
	    rcode = 1;		/* otherwise still have the required votes */
	}
    }
    UBIK_BEACON_UNLOCK;
    ubik_dprint("beacon: amSyncSite is %d\n", rcode);
    return rcode;
}

/*!
 * \brief Procedure that determines whether this site has enough current votes to remain sync site.
 *
 * Called from higher-level modules (everything but the vote module).
 *
 * If we're the sync site, check that our guarantees, obtained by the ubeacon_Interact()
 * light-weight process, haven't expired.  We're sync site as long as a majority of the
 * servers in existence have promised us unexpired guarantees.  The variable #ubik_syncSiteUntil
 * contains the time at which the latest of the majority of the sync site guarantees expires
 * (if the variable #ubik_amSyncSite is true)
 * This module also calls up to the recovery module if it thinks that the recovery module
 * may have to pick up a new database (which offucr sif [sic] we lose the sync site votes).
 *
 * \return 1 if local site is the sync site
 * \return 0 if sync site is elsewhere
 */
int
ubeacon_AmSyncSite(void)
{
    afs_int32 rcode;

    rcode = amSyncSite();

    if (!rcode)
	urecovery_ResetState();

    return rcode;
}

/*!
 * \brief Determine whether at least quorum are aware we have a sync-site.
 *
 * Called from higher-level modules.
 *
 * There is a gap between the time when a new sync-site is elected and the time
 * when the remotes are aware of that. Therefore, any write transaction between
 * this gap will fail. This will force a new re-election which might be time
 * consuming. This procedure determines whether the remotes (quorum) are aware
 * we have a sync-site.
 *
 * \return 1 if remotes are aware we have a sync-site
 * \return 0 if remotes are not aware we have a sync-site
 */
int
ubeacon_SyncSiteAdvertised(void)
{
    afs_int32 rcode;

    UBIK_BEACON_LOCK;
    rcode = beacon_globals.ubik_syncSiteAdvertised;
    UBIK_BEACON_UNLOCK;

    return rcode;
}

/*!
 * \see ubeacon_InitServerListCommon()
 */
int
ubeacon_InitServerListByInfo(afs_uint32 ame, struct afsconf_cell *info,
			     char clones[])
{
    afs_int32 code;

    code = ubeacon_InitServerListCommon(ame, info, clones, 0);
    return code;
}

/*!
 * \param ame "address of me"
 * \param aservers list of other servers
 *
 * \see ubeacon_InitServerListCommon()
 */
int
ubeacon_InitServerList(afs_uint32 ame, afs_uint32 aservers[])
{
    afs_int32 code;

    code =
	ubeacon_InitServerListCommon(ame, (struct afsconf_cell *)0, 0,
				     aservers);
    return code;
}

/* Must be called with address lock held */
void
ubeacon_InitSecurityClass(void)
{
    int i;
    /* get the security index to use, if we can */
    if (secLayerProc) {
	i = (*secLayerProc) (securityRock, &addr_globals.ubikSecClass, &addr_globals.ubikSecIndex);
    } else if (ubik_CRXSecurityProc) {
	i = (*ubik_CRXSecurityProc) (ubik_CRXSecurityRock, &addr_globals.ubikSecClass,
				     &addr_globals.ubikSecIndex);
    } else
	i = 1;
    if (i) {
	/* don't have sec module yet */
	addr_globals.ubikSecIndex = 0;
	addr_globals.ubikSecClass = rxnull_NewClientSecurityObject();
    }
}

void
ubeacon_ReinitServer(struct ubik_server *ts)
{
    if (tokenCheckProc && !(*tokenCheckProc) (securityRock)) {
	struct rx_connection *disk_rxcid;
	struct rx_connection *vote_rxcid;
	struct rx_connection *tmp;
	UBIK_ADDR_LOCK;
	ubeacon_InitSecurityClass();
	disk_rxcid =
	    rx_NewConnection(rx_HostOf(rx_PeerOf(ts->disk_rxcid)),
			     ubik_callPortal, DISK_SERVICE_ID,
			     addr_globals.ubikSecClass, addr_globals.ubikSecIndex);
	if (disk_rxcid) {
	    tmp = ts->disk_rxcid;
	    ts->disk_rxcid = disk_rxcid;
	    rx_PutConnection(tmp);
	}
	vote_rxcid =
	    rx_NewConnection(rx_HostOf(rx_PeerOf(ts->vote_rxcid)),
			     ubik_callPortal, VOTE_SERVICE_ID,
			     addr_globals.ubikSecClass, addr_globals.ubikSecIndex);
	if (vote_rxcid) {
	    tmp = ts->vote_rxcid;
	    ts->vote_rxcid = vote_rxcid;
	    rx_PutConnection(tmp);
	}
	UBIK_ADDR_UNLOCK;
    }
}

/*!
 * \brief setup server list
 *
 * \param ame "address of me"
 * \param aservers list of other servers
 *
 * called only at initialization to set up the list of servers to
 * contact for votes.  Just creates the server structure.
 *
 * The "magic" host is the one with the lowest internet address.  It is
 * magic because its vote counts epsilon more than the others.  This acts
 * as a tie-breaker when we have an even number of hosts in the system.
 * For example, if the "magic" host is up in a 2 site system, then it
 * is sync site.  Without the magic host hack, if anyone crashed in a 2
 * site system, we'd be out of business.
 *
 * \note There are two connections in every server structure, one for
 * vote calls (which must always go through quickly) and one for database
 * operations, which are subject to waiting for locks.  If we used only
 * one, the votes would sometimes get held up behind database operations,
 * and the sync site guarantees would timeout even though the host would be
 * up for communication.
 *
 * \see ubeacon_InitServerList(), ubeacon_InitServerListByInfo()
 */
int
ubeacon_InitServerListCommon(afs_uint32 ame, struct afsconf_cell *info,
			     char clones[], afs_uint32 aservers[])
{
    struct ubik_server *ts;
    afs_int32 me = -1;
    afs_int32 servAddr;
    afs_int32 i, code;
    afs_int32 magicHost;
    struct ubik_server *magicServer;

    /* verify that the addresses passed in are correct */
    if ((code = verifyInterfaceAddress(&ame, info, aservers)))
	return code;

    ubeacon_InitSecurityClass();

    magicHost = ntohl(ame);	/* do comparisons in host order */
    magicServer = (struct ubik_server *)0;

    if (info) {
	for (i = 0; i < info->numServers; i++) {
	    if (ntohl((afs_uint32) info->hostAddr[i].sin_addr.s_addr) ==
		ntohl((afs_uint32) ame)) {
		me = i;
		if (clones[i]) {
		    amIClone = 1;
		    magicHost = 0;
		}
	    }
	}
	nServers = 0;
	for (i = 0; i < info->numServers; i++) {
	    if (i == me)
		continue;
	    ts = calloc(1, sizeof(struct ubik_server));
	    ts->next = ubik_servers;
	    ubik_servers = ts;
	    ts->addr[0] = info->hostAddr[i].sin_addr.s_addr;
	    if (clones[i]) {
		ts->isClone = 1;
	    } else {
		if (!magicHost
		    || ntohl((afs_uint32) ts->addr[0]) <
		    (afs_uint32) magicHost) {
		    magicHost = ntohl(ts->addr[0]);
		    magicServer = ts;
		}
		++nServers;
	    }
	    /* for vote reqs */
	    ts->vote_rxcid =
		rx_NewConnection(info->hostAddr[i].sin_addr.s_addr,
				 ubik_callPortal, VOTE_SERVICE_ID,
				 addr_globals.ubikSecClass, addr_globals.ubikSecIndex);
	    /* for disk reqs */
	    ts->disk_rxcid =
		rx_NewConnection(info->hostAddr[i].sin_addr.s_addr,
				 ubik_callPortal, DISK_SERVICE_ID,
				 addr_globals.ubikSecClass, addr_globals.ubikSecIndex);
	    ts->up = 1;
	}
    } else {
	i = 0;
	while ((servAddr = *aservers++)) {
	    if (i >= MAXSERVERS)
		return UNHOSTS;	/* too many hosts */
	    ts = calloc(1, sizeof(struct ubik_server));
	    ts->next = ubik_servers;
	    ubik_servers = ts;
	    ts->addr[0] = servAddr;	/* primary address in  net byte order */
	    ts->vote_rxcid = rx_NewConnection(servAddr, ubik_callPortal, VOTE_SERVICE_ID,
			addr_globals.ubikSecClass, addr_globals.ubikSecIndex);	/* for vote reqs */
	    ts->disk_rxcid = rx_NewConnection(servAddr, ubik_callPortal, DISK_SERVICE_ID,
			addr_globals.ubikSecClass, addr_globals.ubikSecIndex);	/* for disk reqs */
	    ts->isClone = 0;	/* don't know about clones */
	    ts->up = 1;
	    if (ntohl((afs_uint32) servAddr) < (afs_uint32) magicHost) {
		magicHost = ntohl(servAddr);
		magicServer = ts;
	    }
	    i++;
	}
    }
    if (magicServer)
	magicServer->magic = 1;	/* remember for when counting votes */

    if (!amIClone && !magicServer)
	amIMagic = 1;
    if (info) {
	if (!amIClone)
	    ++nServers;		/* count this server as well as the remotes */
    } else
	nServers = i + 1;	/* count this server as well as the remotes */

    ubik_quorum = (nServers >> 1) + 1;	/* compute the majority figure */

/* Shoud we set some defaults for RX??
    r_retryInterval = 2;
    r_nRetries = (RPCTIMEOUT/r_retryInterval);
*/
    if (info) {
	if (!ubik_servers)	/* special case 1 server */
	    ubik_singleServer = 1;
	if (nServers == 1 && !amIClone) {
	    beacon_globals.ubik_amSyncSite = 1;	/* let's start as sync site */
	    beacon_globals.syncSiteUntil = 0x7fffffff;	/* and be it quite a while */
	    beacon_globals.ubik_syncSiteAdvertised = 1;
	}
    } else {
	if (nServers == 1)	/* special case 1 server */
	    ubik_singleServer = 1;
    }

    if (ubik_singleServer) {
	if (!beacon_globals.ubik_amSyncSite)
	    ubik_dprint("Ubik: I am the sync site - 1 server\n");
	beacon_globals.ubik_amSyncSite = 1;
	beacon_globals.syncSiteUntil = 0x7fffffff;	/* quite a while */
	beacon_globals.ubik_syncSiteAdvertised = 1;
    }
    return 0;
}

/*!
 * \brief main lwp loop for code that sends out beacons.
 *
 * This code only runs while we're sync site or we want to be the sync site.
 * It runs in its very own light-weight process.
 */
void *
ubeacon_Interact(void *dummy)
{
    afs_int32 code;
    struct timeval tt;
    struct rx_connection *connections[MAXSERVERS];
    struct ubik_server *servers[MAXSERVERS];
    afs_int32 i;
    struct ubik_server *ts;
    afs_int32 temp, yesVotes, lastWakeupTime, oldestYesVote, syncsite;
    struct ubik_tid ttid;
    struct ubik_version tversion;
    afs_int32 startTime;

    afs_pthread_setname_self("beacon");

    /* loop forever getting votes */
    lastWakeupTime = 0;		/* keep track of time we last started a vote collection */
    while (1) {

	/* don't wakeup more than every POLLTIME seconds */
	temp = (lastWakeupTime + POLLTIME) - FT_ApproxTime();
	/* don't sleep if last collection phase took too long (probably timed someone out ) */
	if (temp > 0) {
	    if (temp > POLLTIME)
		temp = POLLTIME;
	    tt.tv_sec = temp;
	    tt.tv_usec = 0;
#ifdef AFS_PTHREAD_ENV
	    select(0, 0, 0, 0, &tt);
#else
	    IOMGR_Select(0, 0, 0, 0, &tt);
#endif
	}

	lastWakeupTime = FT_ApproxTime();	/* started a new collection phase */

	if (ubik_singleServer)
	    continue;		/* special-case 1 server for speedy startup */

	if (!uvote_ShouldIRun())
	    continue;		/* if voter has heard from a better candidate than us, don't bother running */

	/* otherwise we should run for election, or we're the sync site (and have already won);
	 * send out the beacon packets */
	/* build list of all up hosts (noticing dead hosts are running again
	 * is a task for the recovery module, not the beacon module), and
	 * prepare to send them an r multi-call containing the beacon message */
	i = 0;			/* collect connections */
	UBIK_BEACON_LOCK;
	UBIK_ADDR_LOCK;
	for (ts = ubik_servers; ts; ts = ts->next) {
	    if (ts->up && ts->addr[0] != ubik_host[0]) {
		servers[i] = ts;
		connections[i++] = ts->vote_rxcid;
	    }
	}
	UBIK_ADDR_UNLOCK;
	UBIK_BEACON_UNLOCK;
	servers[i] = (struct ubik_server *)0;	/* end of list */
	/* note that we assume in the vote module that we'll always get at least BIGTIME
	 * seconds of vote from anyone who votes for us, which means we can conservatively
	 * assume we'll be fine until SMALLTIME seconds after we start collecting votes */
	/* this next is essentially an expansion of rgen's ServBeacon routine */

	UBIK_VERSION_LOCK;
	ttid.epoch = version_globals.ubik_epochTime;
	if (ubik_dbase->flags & DBWRITING) {
	    /*
	     * if a write is in progress, we have to send the writeTidCounter
	     * which holds the tid counter of the write transaction , and not
	     * send the tidCounter value which holds the tid counter of the
	     * last transaction.
	     */
	    ttid.counter = ubik_dbase->writeTidCounter;
	} else
	    ttid.counter = ubik_dbase->tidCounter + 1;
	tversion.epoch = ubik_dbase->version.epoch;
	tversion.counter = ubik_dbase->version.counter;
	UBIK_VERSION_UNLOCK;

	/* now analyze return codes, counting up our votes */
	yesVotes = 0;		/* count how many to ensure we have quorum */
	oldestYesVote = 0x7fffffff;	/* time quorum expires */
	syncsite = amSyncSite();
	if (!syncsite) {
	    /* Ok to use the DB lock here since we aren't sync site */
	    DBHOLD(ubik_dbase);
	    urecovery_ResetState();
	    DBRELE(ubik_dbase);
	}
	startTime = FT_ApproxTime();
	/*
	 * Don't waste time using mult Rx calls if there are no connections out there
	 */
	if (i > 0) {
	    char hoststr[16];
	    multi_Rx(connections, i) {
		multi_VOTE_Beacon(syncsite, startTime, &tversion,
				  &ttid);
		temp = FT_ApproxTime();	/* now, more or less */
		ts = servers[multi_i];
		UBIK_BEACON_LOCK;
		ts->lastBeaconSent = temp;
		code = multi_error;

		if (code > 0 && ((code < temp && code < temp - 3600) ||
		                 (code > temp && code > temp + 3600))) {
		    /* if we reached here, supposedly the remote host voted
		     * for us based on a computation from over an hour ago in
		     * the past, or over an hour in the future. this is
		     * unlikely; what actually probably happened is that the
		     * call generated some error and was aborted. this can
		     * happen due to errors with the rx security class in play
		     * (rxkad, rxgk, etc). treat the host as if we got a
		     * timeout, since this is not a valid vote. */
		    ubik_print("assuming distant vote time %d from %s is an error; marking host down\n",
		               (int)code, afs_inet_ntoa_r(ts->addr[0], hoststr));
		    code = -1;
		}
		if (code > 0 && rx_ConnError(connections[multi_i])) {
		    ubik_print("assuming vote from %s is invalid due to conn error %d; marking host down\n",
		               afs_inet_ntoa_r(ts->addr[0], hoststr),
		               (int)rx_ConnError(connections[multi_i]));
		    code = -1;
		}

		/* note that the vote time (the return code) represents the time
		 * the vote was computed, *not* the time the vote expires.  We compute
		 * the latter down below if we got enough votes to go with */
		if (code > 0) {
		    if ((code & ~0xff) == ERROR_TABLE_BASE_RXK) {
			ubik_dprint("token error %d from host %s\n",
				    code, afs_inet_ntoa_r(ts->addr[0], hoststr));
			ts->up = 0;
			ts->beaconSinceDown = 0;
			urecovery_LostServer(ts);
		    } else {
			ts->lastVoteTime = code;
			if (code < oldestYesVote)
			    oldestYesVote = code;
			ts->lastVote = 1;
			if (!ts->isClone)
			    yesVotes += 2;
			if (ts->magic)
			    yesVotes++;	/* the extra epsilon */
			ts->up = 1;	/* server is up (not really necessary: recovery does this for real) */
			ts->beaconSinceDown = 1;
			ubik_dprint("yes vote from host %s\n",
				    afs_inet_ntoa_r(ts->addr[0], hoststr));
		    }
		} else if (code == 0) {
		    ts->lastVoteTime = temp;
		    ts->lastVote = 0;
		    ts->beaconSinceDown = 1;
		    ubik_dprint("no vote from %s\n",
				afs_inet_ntoa_r(ts->addr[0], hoststr));
		} else if (code < 0) {
		    ts->up = 0;
		    ts->beaconSinceDown = 0;
		    urecovery_LostServer(ts);
		    ubik_dprint("time out from %s\n",
				afs_inet_ntoa_r(ts->addr[0], hoststr));
		}
		UBIK_BEACON_UNLOCK;
	    }
	    multi_End;
	}
	/* now call our own voter module to see if we'll vote for ourself.  Note that
	 * the same restrictions apply for our voting for ourself as for our voting
	 * for anyone else. */
	i = SVOTE_Beacon((struct rx_call *)0, ubeacon_AmSyncSite(), startTime,
			 &tversion, &ttid);
	if (i) {
	    yesVotes += 2;
	    if (amIMagic)
		yesVotes++;	/* extra epsilon */
	    if (i < oldestYesVote)
		oldestYesVote = i;
	}

	/* now decide if we have enough votes to become sync site.
	 * Note that we can still get enough votes even if we didn't for ourself. */
	if (yesVotes > nServers) {	/* yesVotes is bumped by 2 or 3 for each site */
	    UBIK_BEACON_LOCK;
	    if (!beacon_globals.ubik_amSyncSite)
		ubik_dprint("Ubik: I am the sync site\n");
	    else {
		/* at this point, we have the guarantee that at least quorum
		 * received a beacon packet informing we have a sync-site. */
		beacon_globals.ubik_syncSiteAdvertised = 1;
	    }
	    beacon_globals.ubik_amSyncSite = 1;
	    beacon_globals.syncSiteUntil = oldestYesVote + SMALLTIME;
#ifndef AFS_PTHREAD_ENV
		/* I did not find a corresponding LWP_WaitProcess(&ubik_amSyncSite) --
		   this may be a spurious signal call -- sjenkins */
		LWP_NoYieldSignal(&beacon_globals.ubik_amSyncSite);
#endif
	    UBIK_BEACON_UNLOCK;
	} else {
	    UBIK_BEACON_LOCK;
	    if (beacon_globals.ubik_amSyncSite)
		ubik_dprint("Ubik: I am no longer the sync site\n");
	    beacon_globals.ubik_amSyncSite = 0;
	    beacon_globals.ubik_syncSiteAdvertised = 0;
	    UBIK_BEACON_UNLOCK;
	    DBHOLD(ubik_dbase);
	    urecovery_ResetState();	/* tell recovery we're no longer the sync site */
	    DBRELE(ubik_dbase);
	}

    }				/* while loop */
    return NULL;
}

/*!
 * \brief Verify that a given IP addresses does actually exist on this machine.
 *
 * \param ame      the pointer to my IP address specified in the
 *                 CellServDB file.
 * \param aservers an array containing IP
 *                 addresses of remote ubik servers. The array is
 *                 terminated by a zero address.
 *
 * Algorithm     : Verify that my IP addresses \p ame does actually exist
 *                 on this machine.  If any of my IP addresses are there
 *                 in the remote server list \p aserver, remove them from
 *                 this list.  Update global variable \p ubik_host[] with
 *                 my IP addresses.
 *
 * \return 0 on success, non-zero on failure
 */
static int
verifyInterfaceAddress(afs_uint32 *ame, struct afsconf_cell *info,
		       afs_uint32 aservers[]) {
    afs_uint32 myAddr[UBIK_MAX_INTERFACE_ADDR], *servList, tmpAddr;
    afs_uint32 myAddr2[UBIK_MAX_INTERFACE_ADDR];
    char hoststr[16];
    int tcount, count, found, i, j, totalServers, start, end, usednetfiles =
	0;

    if (info)
	totalServers = info->numServers;
    else {			/* count the number of servers */
	for (totalServers = 0, servList = aservers; *servList; servList++)
	    totalServers++;
    }

    if (AFSDIR_SERVER_NETRESTRICT_FILEPATH || AFSDIR_SERVER_NETINFO_FILEPATH) {
	/*
	 * Find addresses we are supposed to register as per the netrestrict file
	 * if it exists, else just register all the addresses we find on this
	 * host as returned by rx_getAllAddr (in NBO)
	 */
	char reason[1024];
	count = afsconf_ParseNetFiles(myAddr, NULL, NULL,
				      UBIK_MAX_INTERFACE_ADDR, reason,
				      AFSDIR_SERVER_NETINFO_FILEPATH,
				      AFSDIR_SERVER_NETRESTRICT_FILEPATH);
	if (count < 0) {
	    ubik_print("ubik: Can't register any valid addresses:%s\n",
		       reason);
	    ubik_print("Aborting..\n");
	    return UBADHOST;
	}
	usednetfiles++;
    } else {
	/* get all my interface addresses in net byte order */
	count = rx_getAllAddr(myAddr, UBIK_MAX_INTERFACE_ADDR);
    }

    if (count <= 0) {		/* no address found */
	ubik_print("ubik: No network addresses found, aborting..\n");
	return UBADHOST;
    }

    /* verify that the My-address passed in by ubik is correct */
    for (j = 0, found = 0; j < count; j++) {
	if (*ame == myAddr[j]) {	/* both in net byte order */
	    found = 1;
	    break;
	}
    }

    if (!found) {
	ubik_print("ubik: primary address %s does not exist\n",
		   afs_inet_ntoa_r(*ame, hoststr));
	/* if we had the result of rx_getAllAddr already, avoid subverting
	 * the "is gethostbyname(gethostname()) us" check. If we're
	 * using NetInfo/NetRestrict, we assume they have enough clue
	 * to avoid that big hole in their foot from the loaded gun. */
	if (usednetfiles) {
	    /* take the address we did get, then see if ame was masked */
	    *ame = myAddr[0];
	    tcount = rx_getAllAddr(myAddr2, UBIK_MAX_INTERFACE_ADDR);
	    if (tcount <= 0) {	/* no address found */
		ubik_print("ubik: No network addresses found, aborting..\n");
		return UBADHOST;
	    }

	    /* verify that the My-address passed in by ubik is correct */
	    for (j = 0, found = 0; j < tcount; j++) {
		if (*ame == myAddr2[j]) {	/* both in net byte order */
		    found = 1;
		    break;
		}
	    }
	}
	if (!found)
	    return UBADHOST;
    }

    /* if any of my addresses are there in serverList, then
     ** use that as my primary addresses : the higher level
     ** application screwed up in dealing with multihomed concepts
     */
    for (j = 0, found = 0; j < count; j++) {
	for (i = 0; i < totalServers; i++) {
	    if (info)
		tmpAddr = (afs_uint32) info->hostAddr[i].sin_addr.s_addr;
	    else
		tmpAddr = aservers[i];
	    if (myAddr[j] == tmpAddr) {
		*ame = tmpAddr;
		if (!info)
		    aservers[i] = 0;
		found = 1;
	    }
	}
    }
    if (found)
	ubik_print("Using %s as my primary address\n", afs_inet_ntoa_r(*ame, hoststr));

    if (!info) {
	/* get rid of servers which were purged because all
	 ** those interface addresses are myself
	 */
	for (start = 0, end = totalServers - 1; (start < end); start++, end--) {
	    /* find the first zero entry from the beginning */
	    for (; (start < end) && (aservers[start]); start++);

	    /* find the last non-zero entry from the end */
	    for (; (end >= 0) && (!aservers[end]); end--);

	    /* if there is nothing more to purge, exit from loop */
	    if (start >= end)
		break;

	    /* move the entry */
	    aservers[start] = aservers[end];
	    aservers[end] = 0;	/* this entry was moved */
	}
    }

    /* update all my addresses in ubik_host in such a way
     ** that ubik_host[0] has the primary address
     */
    ubik_host[0] = *ame;
    for (j = 0, i = 1; j < count; j++)
	if (*ame != myAddr[j])
	    ubik_host[i++] = myAddr[j];

    return 0;			/* return success */
}


/*!
 * \brief Exchange IP address information with remote servers.
 *
 * \param ubik_host an array containing all my IP addresses.
 *
 * Algorithm     : Do an RPC to all remote ubik servers informing them
 *                 about my IP addresses. Get their IP addresses and
 *                 update my linked list of ubik servers \p ubik_servers
 *
 * \return 0 on success, non-zero on failure
 */
int
ubeacon_updateUbikNetworkAddress(afs_uint32 ubik_host[UBIK_MAX_INTERFACE_ADDR])
{
    int j, count, code = 0;
    UbikInterfaceAddr inAddr, outAddr;
    struct rx_connection *conns[MAXSERVERS];
    struct ubik_server *ts, *server[MAXSERVERS];
    char buffer[32];
    char hoststr[16];

    UBIK_ADDR_LOCK;
    for (count = 0, ts = ubik_servers; ts; count++, ts = ts->next) {
	conns[count] = ts->disk_rxcid;
	server[count] = ts;
    }
    UBIK_ADDR_UNLOCK;


    /* inform all other servers only if there are more than one
     * database servers in the cell */

    if (count > 0) {

	for (j = 0; j < UBIK_MAX_INTERFACE_ADDR; j++)
	    inAddr.hostAddr[j] = ntohl(ubik_host[j]);


	/* do the multi-RX RPC to all other servers */
	multi_Rx(conns, count) {
	    multi_DISK_UpdateInterfaceAddr(&inAddr, &outAddr);
	    ts = server[multi_i];	/* reply received from this server */
	    if (!multi_error) {
		UBIK_ADDR_LOCK;
		if (ts->addr[0] != htonl(outAddr.hostAddr[0])) {
		    code = UBADHOST;
		    strcpy(buffer, afs_inet_ntoa_r(ts->addr[0], hoststr));
		    ubik_print("ubik:Two primary addresses for same server \
                    %s %s\n", buffer,
		    afs_inet_ntoa_r(htonl(outAddr.hostAddr[0]), hoststr));
		} else {
		    for (j = 1; j < UBIK_MAX_INTERFACE_ADDR; j++)
			ts->addr[j] = htonl(outAddr.hostAddr[j]);
		}
		UBIK_ADDR_UNLOCK;
	    } else if (multi_error == RXGEN_OPCODE) {	/* pre 3.5 remote server */
		UBIK_ADDR_LOCK;
		ubik_print
		    ("ubik server %s does not support UpdateInterfaceAddr RPC\n",
		     afs_inet_ntoa_r(ts->addr[0], hoststr));
		UBIK_ADDR_UNLOCK;
	    } else if (multi_error == UBADHOST) {
		code = UBADHOST;	/* remote CellServDB inconsistency */
		ubik_print("Inconsistent Cell Info on server:\n");
		UBIK_ADDR_LOCK;
		for (j = 0; j < UBIK_MAX_INTERFACE_ADDR && ts->addr[j]; j++)
		    ubik_print("... %s\n", afs_inet_ntoa_r(ts->addr[j], hoststr));
		UBIK_ADDR_UNLOCK;
	    } else {
		UBIK_BEACON_LOCK;
		ts->up = 0;	/* mark the remote server as down */
		UBIK_BEACON_UNLOCK;
	    }
	}
	multi_End;
    }
    return code;
}

void
ubik_SetClientSecurityProcs(int (*secproc) (void *,
					    struct rx_securityClass **,
					    afs_int32 *),
			    int (*checkproc) (void *),
			    void *rock)
{
    secLayerProc = secproc;
    tokenCheckProc = checkproc;
    securityRock = rock;
}
