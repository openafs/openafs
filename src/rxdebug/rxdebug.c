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
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/file.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <sys/stat.h>
#include <afs/stds.h>
#include <afs/cmd.h>

#include <stdio.h>

#include "rx_user.h"
#include "rx_clock.h"
#include "rx_queue.h"
#include "rx.h"
#include "rx_globals.h"


#define	TIMEOUT	    20

extern struct hostent *hostutil_GetHostByName();

static short
PortNumber(aport)
     register char *aport;
{
    register int tc;
    register short total;

    total = 0;
    while (tc = *aport++) {
	if (tc < '0' || tc > '9')
	    return -1;		/* bad port number */
	total *= 10;
	total += tc - (int)'0';
    }
    return htons(total);
}

static short
PortName(aname)
     register char *aname;
{
    register struct servent *ts;
    ts = getservbyname(aname, NULL);
    if (!ts)
	return -1;
    return ts->s_port;		/* returns it in network byte order */
}

int
MainCommand(as, arock)
     char *arock;
     struct cmd_syndesc *as;
{
    register int i;
    osi_socket s;
    int j;
    struct sockaddr_in taddr;
    afs_int32 host;
    struct in_addr hostAddr;
    short port;
    struct hostent *th;
    register afs_int32 code;
    int nodally;
    int allconns;
    int rxstats;
    int onlyClient, onlyServer;
    afs_int32 onlyHost;
    short onlyPort;
    int onlyAuth;
    int flag;
    int dallyCounter;
    int withSecStats;
    int withAllConn;
    int withRxStats;
    int withWaiters;
    int withIdleThreads;
    int withWaited;
    int withPeers;
    struct rx_debugStats tstats;
    char *portName, *hostName;
    char hoststr[20];
    struct rx_debugConn tconn;
    short noConns;
    short showPeers;
    short showLong;
    int version_flag;
    char version[64];
    afs_int32 length = 64;

    afs_uint32 supportedDebugValues = 0;
    afs_uint32 supportedStatValues = 0;
    afs_uint32 supportedConnValues = 0;
    afs_uint32 supportedPeerValues = 0;
    afs_int32 nextconn = 0;
    afs_int32 nextpeer = 0;

    nodally = (as->parms[2].items ? 1 : 0);
    allconns = (as->parms[3].items ? 1 : 0);
    rxstats = (as->parms[4].items ? 1 : 0);
    onlyServer = (as->parms[5].items ? 1 : 0);
    onlyClient = (as->parms[6].items ? 1 : 0);
    version_flag = (as->parms[10].items ? 1 : 0);
    noConns = (as->parms[11].items ? 1 : 0);
    showPeers = (as->parms[12].items ? 1 : 0);
    showLong = (as->parms[13].items ? 1 : 0);

    if (as->parms[0].items)
	hostName = as->parms[0].items->data;
    else
	hostName = NULL;

    if (as->parms[1].items)
	portName = as->parms[1].items->data;
    else
	portName = NULL;

    if (as->parms[7].items) {
	char *name = as->parms[7].items->data;
	if ((onlyPort = PortNumber(name)) == -1)
	    onlyPort = PortName(name);
	if (onlyPort == -1) {
	    printf("rxdebug: can't resolve port name %s\n", name);
	    exit(1);
	}
    } else
	onlyPort = -1;

    if (as->parms[8].items) {
	char *name = as->parms[8].items->data;
	struct hostent *th;
	th = hostutil_GetHostByName(name);
	if (!th) {
	    printf("rxdebug: host %s not found in host table\n", name);
	    exit(1);
	}
	memcpy(&onlyHost, th->h_addr, sizeof(afs_int32));
    } else
	onlyHost = -1;

    if (as->parms[9].items) {
	char *name = as->parms[9].items->data;
	if (strcmp(name, "clear") == 0)
	    onlyAuth = 0;
	else if (strcmp(name, "auth") == 0)
	    onlyAuth = 1;
	else if (strcmp(name, "crypt") == 0)
	    onlyAuth = 2;
	else if ((strcmp(name, "null") == 0) || (strcmp(name, "none") == 0)
		 || (strncmp(name, "noauth", 6) == 0)
		 || (strncmp(name, "unauth", 6) == 0))
	    onlyAuth = -1;
	else {
	    fprintf(stderr, "Unknown authentication level: %s\n", name);
	    exit(1);
	}
    } else
	onlyAuth = 999;

    /* lookup host */
    if (hostName) {
	th = hostutil_GetHostByName(hostName);
	if (!th) {
	    printf("rxdebug: host %s not found in host table\n", hostName);
	    exit(1);
	}
	memcpy(&host, th->h_addr, sizeof(afs_int32));
    } else
	host = htonl(0x7f000001);	/* IP localhost */

    if (!portName)
	port = htons(7000);	/* default is fileserver */
    else {
	if ((port = PortNumber(portName)) == -1)
	    port = PortName(portName);
	if (port == -1) {
	    printf("rxdebug: can't resolve port name %s\n", portName);
	    exit(1);
	}
    }

    dallyCounter = 0;

    hostAddr.s_addr = host;
    afs_inet_ntoa_r(hostAddr.s_addr, hoststr);
    printf("Trying %s (port %d):\n", hoststr, ntohs(port));
    s = socket(AF_INET, SOCK_DGRAM, 0);
    taddr.sin_family = AF_INET;
    taddr.sin_port = 0;
    taddr.sin_addr.s_addr = 0;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    taddr.sin_len = sizeof(struct sockaddr_in);
#endif
    code = bind(s, (struct sockaddr *)&taddr, sizeof(struct sockaddr_in));
    if (code) {
	perror("bind");
	exit(1);
    }

    if (version_flag) {

	code = rx_GetServerVersion(s, host, port, length, version);
	if (code < 0) {
	    printf("get version call failed with code %d, errno %d\n", code,
		   errno);
	    exit(1);
	}
	printf("AFS version: %s\n", version);
	fflush(stdout);

	exit(0);

    }


    code = rx_GetServerDebug(s, host, port, &tstats, &supportedDebugValues);
    if (code < 0) {
	printf("getstats call failed with code %d\n", code);
	exit(1);
    }

    withSecStats = (supportedDebugValues & RX_SERVER_DEBUG_SEC_STATS);
    withAllConn = (supportedDebugValues & RX_SERVER_DEBUG_ALL_CONN);
    withRxStats = (supportedDebugValues & RX_SERVER_DEBUG_RX_STATS);
    withWaiters = (supportedDebugValues & RX_SERVER_DEBUG_WAITER_CNT);
    withIdleThreads = (supportedDebugValues & RX_SERVER_DEBUG_IDLE_THREADS);
    withWaited = (supportedDebugValues & RX_SERVER_DEBUG_WAITED_CNT);
    withPeers = (supportedDebugValues & RX_SERVER_DEBUG_ALL_PEER);

    printf("Free packets: %d, packet reclaims: %d, calls: %d, used FDs: %d\n",
	   tstats.nFreePackets, tstats.packetReclaims, tstats.callsExecuted,
	   tstats.usedFDs);
    if (!tstats.waitingForPackets)
	printf("not ");
    printf("waiting for packets.\n");
    if (withWaiters)
	printf("%d calls waiting for a thread\n", tstats.nWaiting);
    if (withIdleThreads)
	printf("%d threads are idle\n", tstats.idleThreads);
    if (withWaited)
	printf("%d calls have waited for a thread\n", tstats.nWaited);

    if (rxstats) {
	if (!withRxStats) {
	  noRxStats:
	    withRxStats = 0;
	    fprintf(stderr,
		    "WARNING: Server doesn't support retrieval of Rx statistics\n");
	} else {
	    struct rx_stats rxstats;

	    /* should gracefully handle the case where rx_stats grows */
	    code =
		rx_GetServerStats(s, host, port, &rxstats,
				  &supportedStatValues);
	    if (code < 0) {
		printf("rxstats call failed with code %d\n", code);
		exit(1);
	    }
	    if (code != sizeof(rxstats)) {
		if ((((struct rx_debugIn *)(&rxstats))->type ==
		     RX_DEBUGI_BADTYPE))
		    goto noRxStats;
		printf
		    ("WARNING: returned Rx statistics of unexpected size (got %d)\n",
		     code);
		/* handle other versions?... */
	    }

	    rx_PrintTheseStats(stdout, &rxstats, sizeof(rxstats),
			       tstats.nFreePackets, tstats.version);
	}
    }

    if (!noConns) {
	if (allconns) {
	    if (!withAllConn)
		fprintf(stderr,
			"WARNING: Server doesn't support retrieval of all connections,\n         getting only interesting instead.\n");
	}

	if (onlyServer)
	    printf("Showing only server connections\n");
	if (onlyClient)
	    printf("Showing only client connections\n");
	if (onlyAuth != 999) {
	    static char *name[] =
		{ "unauthenticated", "rxkad_clear", "rxkad_auth",
		"rxkad_crypt"
	    };
	    printf("Showing only %s connections\n", name[onlyAuth + 1]);
	}
	if (onlyHost != -1) {
	    hostAddr.s_addr = onlyHost;
	    afs_inet_ntoa_r(hostAddr.s_addr, hoststr);
	    printf("Showing only connections from host %s\n",
		   hoststr);
	}
	if (onlyPort != -1)
	    printf("Showing only connections on port %u\n", ntohs(onlyPort));

	for (i = 0;; i++) {
	    code =
		rx_GetServerConnections(s, host, port, &nextconn, allconns,
					supportedDebugValues, &tconn,
					&supportedConnValues);
	    if (code < 0) {
		printf("getconn call failed with code %d\n", code);
		break;
	    }
	    if (tconn.cid == (afs_int32) 0xffffffff) {
		printf("Done.\n");
		break;
	    }

	    /* see if we're in nodally mode and all calls are dallying */
	    if (nodally) {
		flag = 0;
		for (j = 0; j < RX_MAXCALLS; j++) {
		    if (tconn.callState[j] != RX_STATE_NOTINIT
			&& tconn.callState[j] != RX_STATE_DALLY) {
			flag = 1;
			break;
		    }
		}
		if (flag == 0) {
		    /* this call looks too ordinary, bump skipped count and go
		     * around again */
		    dallyCounter++;
		    continue;
		}
	    }
	    if ((onlyHost != -1) && (onlyHost != tconn.host))
		continue;
	    if ((onlyPort != -1) && (onlyPort != tconn.port))
		continue;
	    if (onlyServer && (tconn.type != RX_SERVER_CONNECTION))
		continue;
	    if (onlyClient && (tconn.type != RX_CLIENT_CONNECTION))
		continue;
	    if (onlyAuth != 999) {
		if (onlyAuth == -1) {
		    if (tconn.securityIndex != 0)
			continue;
		} else {
		    if (tconn.securityIndex != 2)
			continue;
		    if (withSecStats && (tconn.secStats.type == 3)
			&& (tconn.secStats.level != onlyAuth))
			continue;
		}
	    }

	    /* now display the connection */
	    hostAddr.s_addr = tconn.host;
	    afs_inet_ntoa_r(hostAddr.s_addr, hoststr);
	    printf("Connection from host %s, port %hu, ", hoststr,
		   ntohs(tconn.port));
	    if (tconn.epoch)
		printf("Cuid %x/%x", tconn.epoch, tconn.cid);
	    else
		printf("cid %x", tconn.cid);
	    if (tconn.error)
		printf(", error %d", tconn.error);
	    printf("\n  serial %d, ", tconn.serial);
	    printf(" natMTU %d, ", tconn.natMTU);

	    if (tconn.flags) {
		printf("flags");
		if (tconn.flags & RX_CONN_MAKECALL_WAITING)
		    printf(" MAKECALL_WAITING");
		if (tconn.flags & RX_CONN_DESTROY_ME)
		    printf(" DESTROYED");
		if (tconn.flags & RX_CONN_USING_PACKET_CKSUM)
		    printf(" pktCksum");
		printf(", ");
	    }
	    printf("security index %d, ", tconn.securityIndex);
	    if (tconn.type == RX_CLIENT_CONNECTION)
		printf("client conn\n");
	    else
		printf("server conn\n");

	    if (withSecStats) {
		switch ((int)tconn.secStats.type) {
		case 0:
		    if (tconn.securityIndex == 2)
			printf
			    ("  no GetStats procedure for security object\n");
		    break;
		case 1:
		    printf("  rxnull level=%d, flags=%d\n",
			   tconn.secStats.level, tconn.secStats.flags);
		    break;
		case 2:
		    printf("  rxvab level=%d, flags=%d\n",
			   tconn.secStats.level, tconn.secStats.flags);
		    break;
		case 3:{
			char *level;
			char flags = tconn.secStats.flags;
			if (tconn.secStats.level == 0)
			    level = "clear";
			else if (tconn.secStats.level == 1)
			    level = "auth";
			else if (tconn.secStats.level == 2)
			    level = "crypt";
			else
			    level = "unknown";
			printf("  rxkad: level %s", level);
			if (flags)
			    printf(", flags");
			if (flags & 1)
			    printf(" unalloc");
			if (flags & 2)
			    printf(" authenticated");
			if (flags & 4)
			    printf(" expired");
			if (flags & 8)
			    printf(" pktCksum");
			if (tconn.secStats.expires)
			    /* Apparently due to a bug in the RT compiler that
			     * prevents (afs_uint32)0xffffffff => (double) from working,
			     * this code produces negative lifetimes when run on the
			     * RT. */
			    printf(", expires in %.1f hours",
				   ((afs_uint32) tconn.secStats.expires -
				    time(0)) / 3600.0);
			if (!(flags & 1)) {
			    printf("\n  Received %u bytes in %u packets\n",
				   tconn.secStats.bytesReceived,
				   tconn.secStats.packetsReceived);
			    printf("  Sent %u bytes in %u packets\n",
				   tconn.secStats.bytesSent,
				   tconn.secStats.packetsSent);
			} else
			    printf("\n");
			break;
		    }

		default:
		    printf("  unknown\n");
		}
	    }

	    for (j = 0; j < RX_MAXCALLS; j++) {
		printf("    call %d: # %d, state ", j, tconn.callNumber[j]);
		if (tconn.callState[j] == RX_STATE_NOTINIT) {
		    printf("not initialized\n");
		    continue;
		} else if (tconn.callState[j] == RX_STATE_PRECALL)
		    printf("precall, ");
		else if (tconn.callState[j] == RX_STATE_ACTIVE)
		    printf("active, ");
		else if (tconn.callState[j] == RX_STATE_DALLY)
		    printf("dally, ");
		else if (tconn.callState[j] == RX_STATE_HOLD)
		    printf("hold, ");
		printf("mode: ");
		if (tconn.callMode[j] == RX_MODE_SENDING)
		    printf("sending");
		else if (tconn.callMode[j] == RX_MODE_RECEIVING)
		    printf("receiving");
		else if (tconn.callMode[j] == RX_MODE_ERROR)
		    printf("error");
		else if (tconn.callMode[j] == RX_MODE_EOF)
		    printf("eof");
		else
		    printf("unknown");
		if (tconn.callFlags[j]) {
		    printf(", flags:");
		    if (tconn.callFlags[j] & RX_CALL_READER_WAIT)
			printf(" reader_wait");
		    if (tconn.callFlags[j] & RX_CALL_WAIT_WINDOW_ALLOC)
			printf(" window_alloc");
		    if (tconn.callFlags[j] & RX_CALL_WAIT_WINDOW_SEND)
			printf(" window_send");
		    if (tconn.callFlags[j] & RX_CALL_WAIT_PACKETS)
			printf(" wait_packets");
		    if (tconn.callFlags[j] & RX_CALL_WAIT_PROC)
			printf(" waiting_for_process");
		    if (tconn.callFlags[j] & RX_CALL_RECEIVE_DONE)
			printf(" receive_done");
		    if (tconn.callFlags[j] & RX_CALL_CLEARED)
			printf(" call_cleared");
		}
		if (tconn.callOther[j] & RX_OTHER_IN)
		    printf(", has_input_packets");
		if (tconn.callOther[j] & RX_OTHER_OUT)
		    printf(", has_output_packets");
		printf("\n");
	    }
	}
	if (nodally)
	    printf("Skipped %d dallying connections.\n", dallyCounter);
    }
    if (showPeers && withPeers) {
	for (i = 0;; i++) {
	    struct rx_debugPeer tpeer;
	    code =
		rx_GetServerPeers(s, host, port, &nextpeer, allconns, &tpeer,
				  &supportedPeerValues);
	    if (code < 0) {
		printf("getpeer call failed with code %d\n", code);
		break;
	    }
	    if (tpeer.host == 0xffffffff) {
		printf("Done.\n");
		break;
	    }

	    if ((onlyHost != -1) && (onlyHost != tpeer.host))
		continue;
	    if ((onlyPort != -1) && (onlyPort != tpeer.port))
		continue;

	    /* now display the peer */
	    hostAddr.s_addr = tpeer.host;
	    afs_inet_ntoa_r(hostAddr.s_addr, hoststr);
	    printf("Peer at host %s, port %hu\n", hoststr, 
		   ntohs(tpeer.port));
	    printf("\tifMTU %hu\tnatMTU %hu\tmaxMTU %hu\n", tpeer.ifMTU,
		   tpeer.natMTU, tpeer.maxMTU);
	    printf("\tpackets sent %u\tpacket resends %u\n", tpeer.nSent,
		   tpeer.reSends);
	    printf("\tbytes sent high %u low %u\n", tpeer.bytesSent.high,
		   tpeer.bytesSent.low);
	    printf("\tbytes received high %u low %u\n",
		   tpeer.bytesReceived.high, tpeer.bytesReceived.low);
	    printf("\trtt %u msec, rtt_dev %u msec\n", tpeer.rtt >> 3,
		   tpeer.rtt_dev >> 2);
	    printf("\ttimeout %u.%03u sec\n", tpeer.timeout.sec,
		   tpeer.timeout.usec / 1000);
	    if (!showLong)
		continue;

	    printf("\tin/out packet skew: %d/%d\n", tpeer.inPacketSkew,
		    tpeer.outPacketSkew);
	    printf("\tcongestion window %d, MTU %d\n", tpeer.cwind,
		   tpeer.MTU);
	    printf("\tcurrent/if/max jumbogram size: %d/%d/%d\n",
		   tpeer.nDgramPackets, tpeer.ifDgramPackets,
		   tpeer.maxDgramPackets);
	}
    }
    exit(0);
}

/* simple main program */
#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif
int
main(argc, argv)
     int argc;
     char **argv;
{
    struct cmd_syndesc *ts;

#ifdef RXDEBUG
    rxi_DebugInit();
#endif
#ifdef AFS_NT40_ENV
    if (afs_winsockInit() < 0) {
	printf("%s: Couldn't initialize winsock. Exiting...\n", argv[0]);
	return 1;
    }
#endif

    ts = cmd_CreateSyntax(NULL, MainCommand, 0, "probe RX server");
    cmd_AddParm(ts, "-servers", CMD_SINGLE, CMD_REQUIRED, "server machine");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "IP port");
    cmd_AddParm(ts, "-nodally", CMD_FLAG, CMD_OPTIONAL,
		"don't show dallying conns");
    cmd_AddParm(ts, "-allconnections", CMD_FLAG, CMD_OPTIONAL,
		"don't filter out uninteresting connections on server");
    cmd_AddParm(ts, "-rxstats", CMD_FLAG, CMD_OPTIONAL, "show Rx statistics");
    cmd_AddParm(ts, "-onlyserver", CMD_FLAG, CMD_OPTIONAL,
		"only show server conns");
    cmd_AddParm(ts, "-onlyclient", CMD_FLAG, CMD_OPTIONAL,
		"only show client conns");
    cmd_AddParm(ts, "-onlyport", CMD_SINGLE, CMD_OPTIONAL,
		"show only <port>");
    cmd_AddParm(ts, "-onlyhost", CMD_SINGLE, CMD_OPTIONAL,
		"show only <host>");
    cmd_AddParm(ts, "-onlyauth", CMD_SINGLE, CMD_OPTIONAL,
		"show only <auth level>");

    cmd_AddParm(ts, "-version", CMD_FLAG, CMD_OPTIONAL,
		"show AFS version id");
    cmd_AddParm(ts, "-noconns", CMD_FLAG, CMD_OPTIONAL,
		"show no connections");
    cmd_AddParm(ts, "-peers", CMD_FLAG, CMD_OPTIONAL, "show peers");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL, "detailed output");

    cmd_Dispatch(argc, argv);
    exit(0);
}
