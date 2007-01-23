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
#include <time.h>
#else
#include <sys/file.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <errno.h>
#include <lock.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_multi.h>
#include <afs/cellconfig.h>
#ifndef AFS_NT40_ENV
#include <afs/afsutil.h>
#include <afs/netutils.h>
#endif

#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

/* statics used to determine if we're the sync site */
static afs_int32 syncSiteUntil = 0;	/* valid only if amSyncSite */
int ubik_amSyncSite = 0;	/* flag telling if I'm sync site */
static nServers;		/* total number of servers */
static char amIMagic = 0;	/* is this host the magic host */
char amIClone = 0;		/* is this a clone which doesn't vote */
static char ubik_singleServer = 0;
int (*ubik_CRXSecurityProc) (void *, struct rx_securityClass **, afs_int32 *);
void *ubik_CRXSecurityRock;
afs_int32 ubikSecIndex;
struct rx_securityClass *ubikSecClass;
static verifyInterfaceAddress();


/* Module responsible for both deciding if we're currently the sync site,
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

/* procedure called from debug rpc call to get this module's state for debugging */
ubeacon_Debug(aparm)
     register struct ubik_debug *aparm;
{
    /* fill in beacon's state fields in the ubik_debug structure */
    aparm->syncSiteUntil = syncSiteUntil;
    aparm->nServers = nServers;
}

/* procedure that determines whether this site has enough current votes to remain sync site.
 *  called from higher-level modules (everything but the vote module).
 *
 * If we're the sync site, check that our guarantees, obtained by the ubeacon_Interact
 * light-weight process, haven't expired.  We're sync site as long as a majority of the
 * servers in existence have promised us unexpired guarantees.  The variable ubik_syncSiteUntil
 * contains the time at which the latest of the majority of the sync site guarantees expires
 * (if the variable ubik_amSyncSite is true)
 * This module also calls up to the recovery module if it thinks that the recovery module
 * may have to pick up a new database (which offucr sif we lose the sync site votes).
 */
ubeacon_AmSyncSite()
{
    register afs_int32 now;
    register afs_int32 rcode;

    /* special case for fast startup */
    if (nServers == 1 && !amIClone) {
	return 1;		/* one guy is always the sync site */
    }

    if (ubik_amSyncSite == 0 || amIClone)
	rcode = 0;		/* if I don't think I'm the sync site, say so */
    else {
	now = FT_ApproxTime();
	if (syncSiteUntil <= now) {	/* if my votes have expired, say so */
	    if (ubik_amSyncSite)
		ubik_dprint("Ubik: I am no longer the sync site\n");
	    ubik_amSyncSite = 0;
	    rcode = 0;
	} else {
	    rcode = 1;		/* otherwise still have the required votes */
	}
    }
    if (rcode == 0)
	urecovery_ResetState();	/* force recovery to re-execute */
    ubik_dprint("beacon: amSyncSite is %d\n", rcode);
    return rcode;
}

/* setup server list; called with two parms, first is my address, second is list of other servers
 * called only at initialization to set up the list of servers to contact for votes.  Just creates
 * the server structure.  Note that there are two connections in every server structure, one for
 * vote calls (which must always go through quickly) and one for database operations, which
 * are subject to waiting for locks.  If we used only one, the votes would sometimes get
 * held up behind database operations, and the sync site guarantees would timeout
 * even though the host would be up for communication.
 *
 * The "magic" host is the one with the lowest internet address.  It is
 * magic because its vote counts epsilon more than the others.  This acts
 * as a tie-breaker when we have an even number of hosts in the system.
 * For example, if the "magic" host is up in a 2 site system, then it
 * is sync site.  Without the magic host hack, if anyone crashed in a 2
 * site system, we'd be out of business.
 */
ubeacon_InitServerListByInfo(ame, info, clones)
     afs_int32 ame;
     struct afsconf_cell *info;
     char clones[];
{
    afs_int32 code;

    code = ubeacon_InitServerListCommon(ame, info, clones, 0);
    return code;
}

ubeacon_InitServerList(ame, aservers)
     afs_int32 ame;
     register afs_int32 aservers[];
{
    afs_int32 code;

    code =
	ubeacon_InitServerListCommon(ame, (struct afsconf_cell *)0, 0,
				     aservers);
    return code;
}

ubeacon_InitServerListCommon(ame, info, clones, aservers)
     afs_int32 ame;
     struct afsconf_cell *info;
     char clones[];
     register afs_int32 aservers[];
{
    register struct ubik_server *ts;
    afs_int32 me = -1;
    register afs_int32 servAddr;
    register afs_int32 i, code;
    afs_int32 magicHost;
    struct ubik_server *magicServer;

    /* verify that the addresses passed in are correct */
    if ((code = verifyInterfaceAddress(&ame, info, aservers)))
	return code;

    /* get the security index to use, if we can */
    if (ubik_CRXSecurityProc) {
	i = (*ubik_CRXSecurityProc) (ubik_CRXSecurityRock, &ubikSecClass,
				     &ubikSecIndex);
    } else
	i = 1;
    if (i) {
	/* don't have sec module yet */
	ubikSecIndex = 0;
	ubikSecClass = rxnull_NewClientSecurityObject();
    }
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
	    ts = (struct ubik_server *)malloc(sizeof(struct ubik_server));
	    memset(ts, 0, sizeof(struct ubik_server));
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
				 ubikSecClass, ubikSecIndex);
	    /* for disk reqs */
	    ts->disk_rxcid =
		rx_NewConnection(info->hostAddr[i].sin_addr.s_addr,
				 ubik_callPortal, DISK_SERVICE_ID,
				 ubikSecClass, ubikSecIndex);
	    ts->up = 1;
	}
    } else {
	i = 0;
	while ((servAddr = *aservers++)) {
	    if (i >= MAXSERVERS)
		return UNHOSTS;	/* too many hosts */
	    ts = (struct ubik_server *)malloc(sizeof(struct ubik_server));
	    memset(ts, 0, sizeof(struct ubik_server));
	    ts->next = ubik_servers;
	    ubik_servers = ts;
	    ts->addr[0] = servAddr;	/* primary address in  net byte order */
	    ts->vote_rxcid = rx_NewConnection(servAddr, ubik_callPortal, VOTE_SERVICE_ID, ubikSecClass, ubikSecIndex);	/* for vote reqs */
	    ts->disk_rxcid = rx_NewConnection(servAddr, ubik_callPortal, DISK_SERVICE_ID, ubikSecClass, ubikSecIndex);	/* for disk reqs */
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
    /* send addrs to all other servers */
    code = updateUbikNetworkAddress(ubik_host);
    if (code)
	return code;

/* Shoud we set some defaults for RX??
    r_retryInterval = 2;	
    r_nRetries = (RPCTIMEOUT/r_retryInterval);
*/
    if (info) {
	if (!ubik_servers)	/* special case 1 server */
	    ubik_singleServer = 1;
	if (nServers == 1 && !amIClone) {
	    ubik_amSyncSite = 1;	/* let's start as sync site */
	    syncSiteUntil = 0x7fffffff;	/* and be it quite a while */
	}
    } else {
	if (nServers == 1)	/* special case 1 server */
	    ubik_singleServer = 1;
    }

    if (ubik_singleServer) {
	if (!ubik_amSyncSite)
	    ubik_dprint("Ubik: I am the sync site - 1 server\n");
	ubik_amSyncSite = 1;
	syncSiteUntil = 0x7fffffff;	/* quite a while */
    }
    return 0;
}

/* main lwp loop for code that sends out beacons.  This code only runs while
 * we're sync site or we want to be the sync site.  It runs in its very own light-weight
 * process.
 */
ubeacon_Interact()
{
    register afs_int32 code;
    struct timeval tt;
    struct rx_connection *connections[MAXSERVERS];
    struct ubik_server *servers[MAXSERVERS];
    register afs_int32 i;
    register struct ubik_server *ts;
    afs_int32 temp, yesVotes, lastWakeupTime, oldestYesVote, syncsite;
    struct ubik_tid ttid;
    afs_int32 startTime;

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
	    code = IOMGR_Select(0, 0, 0, 0, &tt);
	} else
	    code = 0;

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
	for (ts = ubik_servers; ts; ts = ts->next) {
	    if (ts->up && ts->addr[0] != ubik_host[0]) {
		servers[i] = ts;
		connections[i++] = ts->vote_rxcid;
	    }
	}
	servers[i] = (struct ubik_server *)0;	/* end of list */
	/* note that we assume in the vote module that we'll always get at least BIGTIME 
	 * seconds of vote from anyone who votes for us, which means we can conservatively
	 * assume we'll be fine until SMALLTIME seconds after we start collecting votes */
	/* this next is essentially an expansion of rgen's ServBeacon routine */

	ttid.epoch = ubik_epochTime;
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
#if defined(UBIK_PAUSE)
	ubik_dbase->flags |= DBVOTING;
#endif /* UBIK_PAUSE */

	/* now analyze return codes, counting up our votes */
	yesVotes = 0;		/* count how many to ensure we have quorum */
	oldestYesVote = 0x7fffffff;	/* time quorum expires */
	syncsite = ubeacon_AmSyncSite();
	startTime = FT_ApproxTime();
	/*
	 * Don't waste time using mult Rx calls if there are no connections out there
	 */
	if (i > 0) {
	    multi_Rx(connections, i) {
		multi_VOTE_Beacon(syncsite, startTime, &ubik_dbase->version,
				  &ttid);
		temp = FT_ApproxTime();	/* now, more or less */
		ts = servers[multi_i];
		ts->lastBeaconSent = temp;
		code = multi_error;
		/* note that the vote time (the return code) represents the time
		 * the vote was computed, *not* the time the vote expires.  We compute
		 * the latter down below if we got enough votes to go with */
		if (code > 0) {
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
				afs_inet_ntoa(ts->addr[0]));
		} else if (code == 0) {
		    ts->lastVoteTime = temp;
		    ts->lastVote = 0;
		    ts->beaconSinceDown = 1;
		    ubik_dprint("no vote from %s\n",
				afs_inet_ntoa(ts->addr[0]));
		} else if (code < 0) {
		    ts->up = 0;
		    ts->beaconSinceDown = 0;
		    urecovery_LostServer();
		    ubik_dprint("time out from %s\n",
				afs_inet_ntoa(ts->addr[0]));
		}
	    }
	    multi_End;
	}
	/* now call our own voter module to see if we'll vote for ourself.  Note that
	 * the same restrictions apply for our voting for ourself as for our voting
	 * for anyone else. */
	i = SVOTE_Beacon((struct rx_call *)0, ubeacon_AmSyncSite(), startTime,
			 &ubik_dbase->version, &ttid);
	if (i) {
	    yesVotes += 2;
	    if (amIMagic)
		yesVotes++;	/* extra epsilon */
	    if (i < oldestYesVote)
		oldestYesVote = i;
	}
#if defined(UBIK_PAUSE)
	ubik_dbase->flags &= ~DBVOTING;
#endif /* UBIK_PAUSE */

	/* now decide if we have enough votes to become sync site.
	 * Note that we can still get enough votes even if we didn't for ourself. */
	if (yesVotes > nServers) {	/* yesVotes is bumped by 2 or 3 for each site */
	    if (!ubik_amSyncSite)
		ubik_dprint("Ubik: I am the sync site\n");
	    ubik_amSyncSite = 1;
	    syncSiteUntil = oldestYesVote + SMALLTIME;
	    LWP_NoYieldSignal(&ubik_amSyncSite);
	} else {
	    if (ubik_amSyncSite)
		ubik_dprint("Ubik: I am no longer the sync site\n");
	    ubik_amSyncSite = 0;
	    urecovery_ResetState();	/* tell recovery we're no longer the sync site */
	}

    }				/* while loop */
}

/* 
* Input Param   : ame is the pointer to my IP address specified in the
*                 CellServDB file. aservers is an array containing IP 
*                 addresses of remote ubik servers. The array is 
*                 terminated by a zero address.
*
* Algorithm     : Verify that my IP addresses 'ame' does actually exist
*                 on this machine.  If any of my IP addresses are there 
*                 in the remote server list 'aserver', remove them from 
*                 this list.  Update global variable ubik_host[] with 
*                 my IP addresses.
*
* Return Values : 0 on success, non-zero on failure
*/
static
verifyInterfaceAddress(ame, info, aservers)
     afs_uint32 *ame;		/* one of my interface addr in net byte order */
     struct afsconf_cell *info;
     afs_uint32 aservers[];	/* list of all possible server addresses */
{
    afs_uint32 myAddr[UBIK_MAX_INTERFACE_ADDR], *servList, tmpAddr;
    afs_uint32 myAddr2[UBIK_MAX_INTERFACE_ADDR];
    int tcount, count, found, i, j, totalServers, start, end, usednetfiles =
	0;

    if (info)
	totalServers = info->numServers;
    else {			/* count the number of servers */
	for (totalServers = 0, servList = aservers; *servList; servList++)
	    totalServers++;
    }

#ifdef AFS_NT40_ENV
    /* get all my interface addresses in net byte order */
    count = rx_getAllAddr(myAddr, UBIK_MAX_INTERFACE_ADDR);
#else
    if (AFSDIR_SERVER_NETRESTRICT_FILEPATH || AFSDIR_SERVER_NETINFO_FILEPATH) {
	/*
	 * Find addresses we are supposed to register as per the netrestrict file
	 * if it exists, else just register all the addresses we find on this 
	 * host as returned by rx_getAllAddr (in NBO)
	 */
	char reason[1024];
	count =
	    parseNetFiles(myAddr, NULL, NULL, UBIK_MAX_INTERFACE_ADDR, reason,
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
#endif

    if (count <= 0) {		/* no address found */
	ubik_print("ubik: No network addresses found, aborting..");
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
		   afs_inet_ntoa(*ame));
	/* if we had the result of rx_getAllAddr already, avoid subverting
	 * the "is gethostbyname(gethostname()) us" check. If we're
	 * using NetInfo/NetRestrict, we assume they have enough clue
	 * to avoid that big hole in their foot from the loaded gun. */
	if (usednetfiles) {
	    /* take the address we did get, then see if ame was masked */
	    *ame = myAddr[0];
	    tcount = rx_getAllAddr(myAddr2, UBIK_MAX_INTERFACE_ADDR);
	    if (tcount <= 0) {	/* no address found */
		ubik_print("ubik: No network addresses found, aborting..");
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
	ubik_print("Using %s as my primary address\n", afs_inet_ntoa(*ame));

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


/* 
* Input Param   : ubik_host is an array containing all my IP addresses.
*
* Algorithm     : Do an RPC to all remote ubik servers infroming them 
*                 about my IP addresses. Get their IP addresses and
*                 update my linked list of ubik servers 'ubik_servers'
*
* Return Values : 0 on success, non-zero on failure
*/
int
updateUbikNetworkAddress(ubik_host)
     afs_uint32 ubik_host[UBIK_MAX_INTERFACE_ADDR];
{
    int j, count, code = 0;
    UbikInterfaceAddr inAddr, outAddr;
    struct rx_connection *conns[MAXSERVERS];
    struct ubik_server *ts, *server[MAXSERVERS];
    char buffer[32];

    for (count = 0, ts = ubik_servers; ts; count++, ts = ts->next) {
	conns[count] = ts->disk_rxcid;
	server[count] = ts;
    }


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
		if (ts->addr[0] != htonl(outAddr.hostAddr[0])) {
		    code = UBADHOST;
		    strcpy(buffer, (char *)afs_inet_ntoa(ts->addr[0]));
		    ubik_print("ubik:Two primary addresses for same server \
                    %s %s\n", buffer, afs_inet_ntoa(htonl(outAddr.hostAddr[0])));
		} else {
		    for (j = 1; j < UBIK_MAX_INTERFACE_ADDR; j++)
			ts->addr[j] = htonl(outAddr.hostAddr[j]);
		}
	    } else if (multi_error == RXGEN_OPCODE) {	/* pre 3.5 remote server */
		ubik_print
		    ("ubik server %s does not support UpdateInterfaceAddr RPC\n",
		     afs_inet_ntoa(ts->addr[0]));
	    } else if (multi_error == UBADHOST) {
		code = UBADHOST;	/* remote CellServDB inconsistency */
		ubik_print("Inconsistent Cell Info on server: ");
		for (j = 0; j < UBIK_MAX_INTERFACE_ADDR && ts->addr[j]; j++)
		    ubik_print("%s ", afs_inet_ntoa(ts->addr[j]));
		ubik_print("\n");
	    } else {
		ts->up = 0;	/* mark the remote server as down */
	    }
	}
	multi_End;
    }
    return code;
}
