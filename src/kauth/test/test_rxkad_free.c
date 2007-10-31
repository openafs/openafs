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

#include <afs/stds.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <rx/rx.h>
#include <ubik.h>
#include <pwd.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include <afs/com_err.h>

#include "kauth.h"
#include "kautils.h"


char *whoami = "test_rxkad_free";

static void
PrintRxkadStats()
{
    printf("New Objects client %d, server %d.  Destroyed objects %d.\n",
	   rxkad_stats.clientObjects, rxkad_stats.serverObjects,
	   rxkad_stats.destroyObject);
    printf("client conns: %d %d %d, destroyed client %d.\n",
	   rxkad_stats.connections[0], rxkad_stats.connections[1],
	   rxkad_stats.connections[2], rxkad_stats.destroyClient);
    printf("server challenges %d, responses %d %d %d\n",
	   rxkad_stats.challengesSent, rxkad_stats.responses[0],
	   rxkad_stats.responses[1], rxkad_stats.responses[2]);
    printf("server conns %d %d %d unused %d, unauth %d\n",
	   rxkad_stats.destroyConn[0], rxkad_stats.destroyConn[1],
	   rxkad_stats.destroyConn[2], rxkad_stats.destroyUnused,
	   rxkad_stats.destroyUnauth);
}

static int
Main(struct cmd_syndesc *as, void *arock)
{
    int code;
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    char newCell[MAXKTCREALMLEN];
    char *cell;

    long serverList[MAXSERVERS];
    extern struct passwd *getpwuid();

    struct passwd *pw;
    struct ktc_encryptionKey key;

    char passwd[BUFSIZ];

    int cellSpecified;
    int i;
    int verbose = (as->parms[1].items != 0);
    int hostUsage = (as->parms[2].items != 0);
    int waitReap = (as->parms[4].items != 0);
    int doAuth = (as->parms[5].items != 0);
    int number;			/* number of iterations */
    int callsPerSecond;		/* to allow conn GC to run */

    unsigned long lo, hi;	/* mem usage */
    unsigned long highWater;	/* mem usage after reap period */
    unsigned long lastWater;	/* mem usage after last msg */
    int serversUse[MAXSERVERS];	/* usage of each server */
    long serversHost[MAXSERVERS];	/* host addr */
    unsigned long startTime;
    unsigned long now;

    lo = 0;
    whoami = as->a0name;
    newCell[0] = 0;

    if (as->parms[0].items)
	number = atoi(as->parms[0].items->data);
    else
	number = 100;
    if (as->parms[3].items)
	callsPerSecond = atoi(as->parms[3].items->data);
    else
	callsPerSecond = 1;
    if (doAuth && hostUsage) {
	fprintf(stderr,
		"Can't report host usage when calling UserAuthenticate\n");
	return -1;
    }

    if (as->parms[12].items) {	/* if username specified */
	code =
	    ka_ParseLoginName(as->parms[12].items->data, name, instance,
			      newCell);
	if (code) {
	    afs_com_err(whoami, code, "parsing user's name '%s'",
		    as->parms[12].items->data);
	    return code;
	}
	if (strlen(newCell) > 0)
	    cellSpecified = 1;
    } else {
	/* No explicit name provided: use Unix uid. */
	pw = getpwuid(getuid());
	if (pw == 0) {
	    printf("Can't figure out your name from your user id.\n");
	    return KABADCMD;
	}
	strncpy(name, pw->pw_name, sizeof(name));
	strcpy(instance, "");
	strcpy(newCell, "");
    }
    if (strcmp(as->parms[14].name, "-cell") == 0) {
	if (as->parms[14].items) {	/* if cell specified */
	    if (cellSpecified)
		printf("Duplicate cell specification not allowed\n");
	    else
		strncpy(newCell, as->parms[14].items->data, sizeof(newCell));
	}
    }

    code = ka_ExpandCell(newCell, newCell, 0 /*local */ );
    if (code) {
	afs_com_err(whoami, code, "Can't expand cell name");
	return code;
    }
    cell = newCell;

    if (as->parms[13].items) {	/* if password specified */
	strncpy(passwd, as->parms[13].items->data, sizeof(passwd));
	memset(as->parms[13].items->data, 0,
	       strlen(as->parms[13].items->data));
    } else {
	char msg[sizeof(name) + 15];
	if (as->parms[12].items)
	    strcpy(msg, "Admin Password: ");
	else
	    sprintf(msg, "Password for %s: ", name);
	code = read_pw_string(passwd, sizeof(passwd), msg, 0);
	if (code)
	    code = KAREADPW;
	else if (strlen(passwd) == 0)
	    code = KANULLPASSWORD;
	if (code) {
	    afs_com_err(whoami, code, "reading password");
	    return code;
	}
    }
    if (as->parms[15].items) {
	struct cmd_item *ip;
	char *ap[MAXSERVERS + 2];

	for (ip = as->parms[15].items, i = 2; ip; ip = ip->next, i++)
	    ap[i] = ip->data;
	ap[0] = "";
	ap[1] = "-servers";
	code = ubik_ParseClientList(i, ap, serverList);
	if (code) {
	    afs_com_err(whoami, code, "could not parse server list");
	    return code;
	}
	ka_ExplicitCell(cell, serverList);
    }

    if (!doAuth) {
	ka_StringToKey(passwd, cell, &key);
	memset(passwd, 0, sizeof(passwd));
    }
    if (hostUsage) {
	memset(serversUse, 0, sizeof(serversUse));
	memset(serversHost, 0, sizeof(serversHost));
    }

    startTime = time(0);
    for (i = 0; i < number; i++) {
	if (doAuth) {
	    char *reason;
	    code =
		ka_UserAuthenticateLife(0, name, instance, cell, passwd, 0,
					&reason);
	    if (code) {
		fprintf(stderr, "Unable to authenticate to AFS because %s.\n",
			reason);
		return code;
	    }
	} else {
	    struct ktc_token token;
	    struct ktc_token *pToken;
	    struct ubik_client *ubikConn;
	    struct kaentryinfo tentry;
	    int c;

	    code =
		ka_GetAdminToken(name, instance, cell, &key, 3600, &token,
				 1 /*new */ );
	    if (code) {
		afs_com_err(whoami, code, "getting admin token");
		return code;
	    }
	    pToken = &token;
	    if (token.ticketLen == 0) {
		fprintf("Can't get admin token\n");
		return -1;
	    }

	    code =
		ka_AuthServerConn(cell, KA_MAINTENANCE_SERVICE, pToken,
				  &ubikConn);
	    if (code) {
		afs_com_err(whoami, code, "Getting AuthServer ubik conn");
		return code;
	    }

	    if (verbose)
		for (c = 0; c < MAXSERVERS; c++) {
		    struct rx_connection *rxConn =
			ubik_GetRPCConn(ubikConn, c);
		    struct rx_peer *peer;

		    if (rxConn == 0)
			break;
		    peer = rx_PeerOf(rxConn);
		    printf("conn to %s:%d secObj:%x\n",
			   inet_ntoa(rx_HostOf(peer)), ntohs(rx_PortOf(peer)),
			   rxConn->securityObject);
		}

	    code =
		ubik_Call(KAM_GetEntry, ubikConn, 0, name, instance,
			  KAMAJORVERSION, &tentry);
	    if (code) {
		afs_com_err(whoami, code, "getting information for %s.%s", name,
			instance);
		return code;
	    }

	    for (c = 0; c < MAXSERVERS; c++) {
		struct rx_connection *rxConn = ubik_GetRPCConn(ubikConn, c);
		int d;
		if (rxConn == 0)
		    break;
		if (rxConn->serial > 0) {
		    long host = rx_HostOf(rx_PeerOf(rxConn));
		    for (d = 0; d < MAXSERVERS; d++) {
			if (serversHost[d] == 0)
			    serversHost[d] = host;
			if (host == serversHost[d]) {
			    serversUse[d]++;
			    break;
			}
		    }
		}
		if (verbose)
		    printf("serial is %d\n", rxConn->serial);
	    }
	    ubik_ClientDestroy(ubikConn);
	}

	now = time(0);
	if (!lo)
	    lo = sbrk(0);
	if (i && ((i & 0x3f) == 0)) {
	    unsigned long this = sbrk(0);
	    printf("  mem after %d: lo=%x, cur=%x => %d (@ %d)\n", i, lo,
		   this, this - lo, (this - lo) / i);
	    if (highWater && (lastWater != this)) {
		lastWater = this;
		printf("  core leaking (after %d) should be %x, is %x\n", i,
		       highWater, this);
	    }
	}
	if ((highWater == 0) && ((now - startTime) > 61)) {
	    highWater = sbrk(0);
	    lastWater = highWater;
	    printf("  mem highWater mark (after %d) should be %x\n", i,
		   highWater);
	}
	if (callsPerSecond) {
	    long target;
	    if (callsPerSecond > 0)
		target = i / callsPerSecond;
	    else		/* if negative interpret as seconds per call */
		target = i * (-callsPerSecond);
	    target = (startTime + target) - now;
	    if (target > 0)
		IOMGR_Sleep(target);
	}
    }
    printf("calling finalize\n");
    rx_Finalize();
    hi = sbrk(0);
    if (hostUsage) {
	int total = 0;
	for (i = 0; i < MAXSERVERS; i++) {
	    total += serversUse[i];
	    if (serversHost[i] == 0)
		break;
	    printf("host %s used %d times (%2g%%)\n",
		   inet_ntoa(serversHost[i]), serversUse[i],
		   100.0 * serversUse[i] / (double)number);
	}
	if (total > number)
	    printf("  %2g%% retries\n",
		   100.0 * (total - number) / (double)number);
    }
    PrintRxkadStats();
    printf("mem usage: lo=%x, hi=%x => %d (@ %d)\n", lo, hi, hi - lo,
	   (hi - lo) / number);
    if (waitReap) {
	unsigned long mediumHi;
	printf("Waiting 61 seconds for all connections to be reaped\n");
	IOMGR_Sleep(61);
	PrintRxkadStats();
	mediumHi = hi;
	hi = sbrk(0);
	if (mediumHi != hi)
	    printf("mem usage: lo=%x, hi=%x => %d (@ %d)\n", lo, hi, hi - lo,
		   (hi - lo) / number);
    }
    /* most of these checks are sure to fail w/o waiting for reap */
    if (waitReap
	&& ((rxkad_stats_clientObjects != rxkad_stats.destroyObject)
	    || (rxkad_stats.connections[0] + rxkad_stats.connections[1] +
		rxkad_stats.connections[2] != rxkad_stats.destroyClient)
	    || (rxkad_stats.responses[0] != rxkad_stats.destroyConn[0])
	    || (rxkad_stats.responses[1] != rxkad_stats.destroyConn[1])
	    || (rxkad_stats.responses[2] != rxkad_stats.destroyConn[2])
	    /* values of destroyUnused and destroyUnauth should be very small */
	)) {
	fprintf(stderr, "Some rxkad security storage not freed\n");
	return 1;
    }
    if ((highWater != 0) && (highWater < hi)) {
	/* We should reach steady state memory usage after 60 seconds (the Rx
	 * connection reap period).  If we are still using memory, there must
	 * be a core leak. */
	fprintf(stderr, "Core leak\n");
	return 1;
    }
    return 0;
}

int
main(argc, argv)
     IN int argc;
     IN char *argv[];
{
    register struct cmd_syndesc *ts;
    long code;

    initialize_U_error_table();
    initialize_CMD_error_table();
    initialize_RXK_error_table();
    initialize_KTC_error_table();
    initialize_ACFG_error_table();
    initialize_KA_error_table();

    ts = cmd_CreateSyntax(NULL, Main, NULL, "Main program");
    /* 0 */ cmd_AddParm(ts, "-number", CMD_SINGLE, CMD_OPTIONAL,
			"number of iterations");
    /* 1 */ cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL,
			"long form of output");
    /* 2 */ cmd_AddParm(ts, "-hostusage", CMD_FLAG, CMD_OPTIONAL,
			"report distribution of host usage");
    /* 3 */ cmd_AddParm(ts, "-rate", CMD_SINGLE, CMD_OPTIONAL,
			"calls per second");
    /* 4 */ cmd_AddParm(ts, "-waitforreap", CMD_FLAG, CMD_OPTIONAL,
			"wait one reap time before exit");
    /* 5 */ cmd_AddParm(ts, "-doauth", CMD_FLAG, CMD_OPTIONAL,
			"call UserAuthenticate instead of GetEntry");
    cmd_Seek(ts, 12);
    /* 12 */ cmd_AddParm(ts, "-admin_username", CMD_SINGLE, CMD_OPTIONAL,
			 "admin principal to use for authentication");
    /* 13 */ cmd_AddParm(ts, "-password_for_admin", CMD_SINGLE, CMD_OPTIONAL,
			 "admin password");
    /* 14 */ cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    /* 15 */ cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL,
			 "explicit list of authentication servers");
    code = cmd_Dispatch(argc, argv);
    return (code != 0);
}
