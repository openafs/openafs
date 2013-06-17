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


#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/file.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/cmd.h>
#include <afs/afsutil.h>

#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

static_inline int afs_cast_time_t(time_t d) { return (int) d; }

static short
PortNumber(char *aport)
{
    int tc;
    afs_int32 total;

    total = 0;
    while ((tc = *aport++)) {
	if (tc < '0' || tc > '9')
	    return -1;		/* bad port number */
	total *= 10;
	total += tc - (int)'0';
    }
    return (total);
}

static short
PortName(char *aname)
{
    struct servent *ts;
    int len;

    ts = getservbyname(aname, NULL);

    if (ts)
	return ntohs(ts->s_port);	/* returns it in host byte order */

    len = strlen(aname);
    if (strncmp(aname, "vlserver", len) == 0) {
	return 7003;
    } else if (strncmp(aname, "ptserver", len) == 0) {
	return 7002;
    } else if (strncmp(aname, "kaserver", len) == 0) {
	return 7004;
    } else if (strncmp(aname, "buserver", len) == 0) {
	return 7021;
    }
    return (-1);
}

static int
CommandProc(struct cmd_syndesc *as, void *arock)
{
    char *hostName, *portName, *times;
    afs_int32 hostAddr;
    afs_int32 i, j, code;
    short port;
    int int32p;
    time_t now, then, diff, newtime;
    struct hostent *th;
    struct rx_connection *tconn;
    struct rx_securityClass *sc;
    struct ubik_debug udebug;
    struct ubik_sdebug usdebug;
    int oldServer = 0;		/* are we talking to a pre 3.5 server? */
    afs_int32 isClone = 0;
    char hoststr[16];

    int32p = (as->parms[2].items ? 1 : 0);

    if (as->parms[0].items)
	hostName = as->parms[0].items->data;
    else
	hostName = NULL;

    if (as->parms[1].items)
	portName = as->parms[1].items->data;
    else
	portName = NULL;

    /* lookup host */
    if (hostName) {
	th = hostutil_GetHostByName(hostName);
	if (!th) {
	    printf("udebug: host %s not found in host table\n", hostName);
	    exit(1);
	}
	memcpy(&hostAddr, th->h_addr, sizeof(afs_int32));
    } else
	hostAddr = htonl(0x7f000001);	/* IP localhost */

    if (!portName)
	port = htons(3000);	/* default */
    else {
	port = PortNumber(portName);
	if (port < 0)
	    port = PortName(portName);
	if (port < 0) {
	    printf("udebug: can't resolve port name %s\n", portName);
	    exit(1);
	}
	port = htons(port);
    }

    rx_Init(0);
    sc = rxnull_NewClientSecurityObject();
    tconn = rx_NewConnection(hostAddr, port, VOTE_SERVICE_ID, sc, 0);

    /* now do the main call */
    code = VOTE_XDebug(tconn, &udebug, &isClone);
    if (code)
	code = VOTE_Debug(tconn, &udebug);
    if (code == RXGEN_OPCODE) {
	oldServer = 1;		/* talking to a pre 3.5 server */
	memset(&udebug, 0, sizeof(udebug));
	code = VOTE_DebugOld(tconn, (ubik_debug_old *)&udebug);
    }

    if (code) {
	printf("return code %d from VOTE_Debug\n", code);
	exit(0);
    }
    now = time(0);
    then = udebug.now;

    /* now print the main info */
    times = ctime(&then);
    times[24] = 0;
    if (!oldServer) {
	printf("Host's addresses are: ");
	for (j = 0; (j < UBIK_MAX_INTERFACE_ADDR) && udebug.interfaceAddr[j];
	     j++)
	    printf("%s ", afs_inet_ntoa_r(htonl(udebug.interfaceAddr[j]), hoststr));
	printf("\n");
    }
    printf("Host's %s time is %s\n", afs_inet_ntoa_r(hostAddr, hoststr), times);

    times = ctime(&now);
    times[24] = 0;
    diff = now - udebug.now;
    printf("Local time is %s (time differential %d secs)\n", times, afs_cast_time_t(diff));
    if (abs((int)diff) >= MAXSKEW)
	printf("****clock may be bad\n");

    /* UBIK skips the voting if 1 server - so we fudge it here */
    if (udebug.amSyncSite && (udebug.nServers == 1)) {
	udebug.lastYesHost = ntohl(hostAddr);
	udebug.lastYesTime = udebug.now;
	udebug.lastYesState = 1;
	udebug.lastYesClaim = udebug.now;
	udebug.syncVersion.epoch = udebug.localVersion.epoch;
	udebug.syncVersion.counter = udebug.localVersion.counter;
    }

    /* XDR converts addresses for us, so all addresses are in HBO */
    if (udebug.lastYesHost == 0xffffffff) {
	printf("Last yes vote not cast yet \n");
    } else {
	diff = udebug.now - udebug.lastYesTime;
	printf("Last yes vote for %s was %d secs ago (%ssync site); \n",
	       afs_inet_ntoa_r(htonl(udebug.lastYesHost), hoststr),
	       afs_cast_time_t(diff),
	       ((udebug.lastYesState) ? "" : "not "));

	diff = udebug.now - udebug.lastYesClaim;
	newtime = now - diff;
	times = ctime(&newtime);
	times[24] = 0;
	printf("Last vote started %d secs ago (at %s)\n",
	       afs_cast_time_t(diff), times);
    }

    printf("Local db version is %d.%d\n", udebug.localVersion.epoch,
	   udebug.localVersion.counter);

    if (udebug.amSyncSite) {
	if (udebug.syncSiteUntil == 0x7fffffff) {
	    printf("I am sync site forever (%d server%s)\n", udebug.nServers,
		   ((udebug.nServers > 1) ? "s" : ""));
	} else {
	    diff = udebug.syncSiteUntil - udebug.now;
	    newtime = now + diff;
	    times = ctime(&newtime);
	    times[24] = 0;
	    printf
		("I am sync site until %d secs from now (at %s) (%d server%s)\n",
		 afs_cast_time_t(diff), times, udebug.nServers,
		 ((udebug.nServers > 1) ? "s" : ""));
	}
	printf("Recovery state %x\n", udebug.recoveryState);
    } else {
	if (isClone)
	    printf("I am a clone and never can become sync site\n");
	else
	    printf("I am not sync site\n");
	diff = udebug.now - udebug.lowestTime;
	printf("Lowest host %s was set %d secs ago\n",
	       afs_inet_ntoa_r(htonl(udebug.lowestHost), hoststr),
	       afs_cast_time_t(diff));

	diff = udebug.now - udebug.syncTime;
	printf("Sync host %s was set %d secs ago\n",
	       afs_inet_ntoa_r(htonl(udebug.syncHost), hoststr),
	       afs_cast_time_t(diff));
    }

    if (udebug.activeWrite) {
	printf("I am currently managing write trans %d.%d\n",
	       udebug.epochTime, udebug.tidCounter);
    } else {
	printf("The last trans I handled was %d.%d\n",
	       udebug.epochTime, udebug.tidCounter);
    }

    printf("Sync site's db version is %d.%d\n", udebug.syncVersion.epoch,
	   udebug.syncVersion.counter);
    printf("%d locked pages, %d of them for write\n", udebug.lockedPages,
	   udebug.writeLockedPages);

    if (udebug.anyReadLocks)
	printf("There are read locks held\n");
    if (udebug.anyWriteLocks)
	printf("There are write locks held\n");

    if (udebug.currentTrans) {
	if (udebug.writeTrans)
	    printf("There is an active write transaction\n");
	else
	    printf("There is at least one active read transaction\n");
	printf("Transaction tid is %d.%d\n", udebug.syncTid.epoch,
	       udebug.syncTid.counter);
    }
    if (udebug.epochTime) {
	diff = udebug.now - udebug.epochTime;
	newtime = now - diff;
	times = ctime(&newtime);
	times[24] = 0;
	printf
	    ("Last time a new db version was labelled was:\n\t %d secs ago (at %s)\n",
	     afs_cast_time_t(diff), times);
    }

    if (int32p || udebug.amSyncSite) {
	/* now do the subcalls */
	for (i = 0;; i++) {
	    isClone = 0;
	    code = VOTE_XSDebug(tconn, i, &usdebug, &isClone);
	    if (code < 0) {
		if (oldServer) {	/* pre 3.5 server */
		    memset(&usdebug, 0, sizeof(usdebug));
		    code = VOTE_SDebugOld(tconn, i, (ubik_sdebug_old *)&usdebug);
		} else
		    code = VOTE_SDebug(tconn, i, &usdebug);
	    }
	    if (code > 0)
		break;		/* done */
	    if (code < 0) {
		printf("error code %d from VOTE_SDebug\n", code);
		break;
	    }
	    /* otherwise print the structure */
	    printf("\nServer (%s", afs_inet_ntoa_r(htonl(usdebug.addr), hoststr));
	    for (j = 0;
		 ((j < UBIK_MAX_INTERFACE_ADDR - 1) && (usdebug.altAddr[j]));
		 j++)
		printf(" %s", afs_inet_ntoa_r(htonl(usdebug.altAddr[j]), hoststr));
	    printf("): (db %d.%d)", usdebug.remoteVersion.epoch,
		   usdebug.remoteVersion.counter);
	    if (isClone)
		printf("    is only a clone!");
	    printf("\n");

	    if (usdebug.lastVoteTime == 0) {
		printf("    last vote never rcvd \n");
	    } else {
		diff = udebug.now - usdebug.lastVoteTime;
		newtime = now - diff;
		times = ctime(&newtime);
		times[24] = 0;
		printf("    last vote rcvd %d secs ago (at %s),\n",
		       afs_cast_time_t(diff),
		       times);
	    }

	    if (usdebug.lastBeaconSent == 0) {
		printf("    last beacon never sent \n");
	    } else {
		diff = udebug.now - usdebug.lastBeaconSent;
		newtime = now - diff;
		times = ctime(&newtime);
		times[24] = 0;
		printf
		    ("    last beacon sent %d secs ago (at %s), last vote was %s\n",
		     afs_cast_time_t(diff), times, ((usdebug.lastVote) ? "yes" : "no"));
	    }

	    printf("    dbcurrent=%d, up=%d beaconSince=%d\n",
		   usdebug.currentDB, usdebug.up, usdebug.beaconSinceDown);
	}
    }
    return (0);
}

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a
     * crash (i.e. core is generated) we can include the user's data section
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    ts = cmd_CreateSyntax(NULL, CommandProc, NULL, "probe ubik server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server machine");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "IP port");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL, "print all info");

    cmd_Dispatch(argc, argv);
    exit(0);
}
