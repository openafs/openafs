/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* RX Authentication Stress test: server side code. */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#endif
#include <stdio.h>
#include <lwp.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/com_err.h>
#include <afs/cmd.h>

#include <rx/rxkad.h>
#include "stress.h"
#include "stress_internal.h"
#include "rx/rx_globals.h"
#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#endif

int maxSkew = 5;


static char *whoami;

static int
StringToAuth(authname)
     IN char *authname;
{
    int nonoauth = 0;
    if (strcmp(authname, "rxkad") == 0)
	return rxkad_clear;
    if (strcmp(authname, "rxkad_")) {
	if (strncmp(authname, "rxkad_", 6) == 0)
	    nonoauth++, authname += 6;
	if (strcmp(authname, "clear") == 0)
	    return rxkad_clear;
	if (strcmp(authname, "auth") == 0)
	    return rxkad_auth;
	if (strcmp(authname, "crypt") == 0)
	    return rxkad_crypt;
	if (strcmp(authname, "null") == 0)
	    return -1;
	if (strcmp(authname, "none") == 0)
	    return -1;
	if (strncmp(authname, "noauth", 6) == 0)
	    return -1;
	if (strncmp(authname, "unauth", 6) == 0)
	    return -1;
	/* error */
    }
    fprintf(stderr,
	    "Unknown authentication name %s, using rxkad_clear by default\n",
	    authname);
    return rxkad_clear;
}

#define aSERVER		0
#define aCLIENT		1
#define aSENDLEN	2
#define aRECVLEN	3
#define aFASTCALLS	4
#define aSLOWCALLS	5
#define aCOPIOUSCALLS	6
#define aPRINTSTATS	7
#define aPRINTTIMING	8
#define aNOEXIT		9
#define aSTHREADS      10
#define aCTHREADS      11
#define aCALLTEST      12
#define aHIJACKTEST    13
#define aAUTHENTICATION 14
#define aMINSERVERAUTH 15
#define aREPEATINTERVAL 16
#define aREPEATCOUNT   17
#define aSTOPSERVER    18
#define aTRACE         19
#define aMELT1b    20
#define aRECLAIM     21
#define a2DCHOICE     22
#define aMAXSKEW	23
#define aUSETOKENS 24
#define aCELL 25
#define aKEYFILE 26

static int
CommandProc(as, arock)
     char *arock;
     struct cmd_syndesc *as;
{
    long code;
    int startServer = (as->parms[aSERVER].items != 0);
    int startClient = (as->parms[aCLIENT].items != 0);
#ifndef AFS_PTHREAD_ENV
    PROCESS pid;
#endif
    struct serverParms *sParms;
    struct clientParms *cParms;

    sParms = (struct serverParms *)osi_Alloc(sizeof(*sParms));
    cParms = (struct clientParms *)osi_Alloc(sizeof(*cParms));
    memset(sParms, 0, sizeof(*sParms));
    memset(cParms, 0, sizeof(*cParms));
    sParms->whoami = cParms->whoami = whoami;

    if (!(startServer || startClient)) {
	/* Default to "-server -client <localhost>" */
	gethostname(cParms->server, sizeof(cParms->server));
	startServer = startClient = 1;
    }

    /* get booleans */
    cParms->printStats = (as->parms[aPRINTSTATS].items != 0);
    cParms->printTiming = (as->parms[aPRINTTIMING].items != 0);
    cParms->noExit = (as->parms[aNOEXIT].items != 0);
    cParms->callTest = (as->parms[aCALLTEST].items != 0);
    cParms->hijackTest = (as->parms[aHIJACKTEST].items != 0);
    cParms->stopServer = (as->parms[aSTOPSERVER].items != 0);
    cParms->useTokens = (as->parms[aUSETOKENS].items != 0);

    if (as->parms[aMELT1b].items)
	meltdown_1pkt = 0;
    if (as->parms[aRECLAIM].items)
	rxi_doreclaim = 0;
    if (as->parms[a2DCHOICE].items)
	rxi_2dchoice = 0;

    if (startServer) {
	if (as->parms[aTRACE].items) {
	    extern char rxi_tracename[];
	    strcpy(rxi_tracename, as->parms[aTRACE].items->data);
	}

	/* These options not compatible with -server */
	if (cParms->stopServer) {
	    code = RXKST_BADARGS;
	    com_err(whoami, code, "stop server not compatible with -client");
	    return code;
	}

	/* process -server options */
	sParms->threads = 3;	/* one less than channels/conn */
	if (as->parms[aSTHREADS].items)
	    sParms->threads = atoi(as->parms[aSTHREADS].items->data);
	sParms->authentication = 0;
	if (as->parms[aMINSERVERAUTH].items)
	    sParms->authentication =
		StringToAuth(as->parms[aMINSERVERAUTH].items->data);
	if (as->parms[aKEYFILE].items)
	    sParms->keyfile = as->parms[aKEYFILE].items->data;

#ifdef AFS_PTHREAD_ENV
	{
	    pthread_t serverID;
	    pthread_attr_t tattr;

	    code = pthread_attr_init(&tattr);
	    if (code) {
		com_err(whoami, code,
			"can't pthread_attr_init server process");
		return code;
	    }

	    code =
		pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	    if (code) {
		com_err(whoami, code,
			"can't pthread_attr_setdetachstate server process");
		return code;
	    }

	    code =
		pthread_create(&serverID, &tattr, rxkst_StartServer,
			       (void *)sParms);
	}
#else
	code =
	    LWP_CreateProcess(rxkst_StartServer, 16000, LWP_NORMAL_PRIORITY,
			      (opaque) sParms, "Server Process", &pid);
#endif
	if (code) {
	    com_err(whoami, code, "can't create server process");
	    return code;
	}
    } else {
	/* check for server-only args */
    }

    if (startClient) {
	u_long calls;		/* default number of calls */

	/* process -client options */
	if (as->parms[aCLIENT].items)
	    lcstring(cParms->server, as->parms[aCLIENT].items->data,
		     sizeof(cParms->server));
	cParms->threads = 6;	/* 150% of channels/conn */
	if (as->parms[aCTHREADS].items)
	    cParms->threads = atoi(as->parms[aCTHREADS].items->data);
	if (cParms->callTest || cParms->hijackTest || cParms->printTiming
	    || cParms->stopServer)
	    calls = 0;
	else
	    calls = 1;
	cParms->fastCalls = cParms->slowCalls = cParms->copiousCalls = calls;

	cParms->sendLen = cParms->recvLen = 10000;
	if (as->parms[aSENDLEN].items)
	    cParms->sendLen = atoi(as->parms[aSENDLEN].items->data);
	if (as->parms[aRECVLEN].items)
	    cParms->recvLen = atoi(as->parms[aRECVLEN].items->data);
	if (as->parms[aFASTCALLS].items)
	    cParms->fastCalls = atoi(as->parms[aFASTCALLS].items->data);
	if (as->parms[aSLOWCALLS].items)
	    cParms->slowCalls = atoi(as->parms[aSLOWCALLS].items->data);
	if (as->parms[aCOPIOUSCALLS].items)
	    cParms->copiousCalls = atoi(as->parms[aCOPIOUSCALLS].items->data);
	cParms->repeatInterval = cParms->repeatCount = 0;
	if (as->parms[aREPEATINTERVAL].items)
	    cParms->repeatInterval =
		atoi(as->parms[aREPEATINTERVAL].items->data);
	if (as->parms[aREPEATCOUNT].items)
	    cParms->repeatCount = atoi(as->parms[aREPEATCOUNT].items->data);
	if (as->parms[aMAXSKEW].items) {
	    maxSkew = atoi(as->parms[aMAXSKEW].items->data);
	    if (maxSkew < 1) {
		printf("Minimum allowed maxSkew is 1, resetting.\n");
		maxSkew = 1;
	    }
	}

	cParms->authentication = 0;
	if (as->parms[aAUTHENTICATION].items)
	    cParms->authentication =
		StringToAuth(as->parms[aAUTHENTICATION].items->data);
	cParms->cell = RXKST_CLIENT_CELL;
	if (as->parms[aCELL].items)
	    cParms->cell = as->parms[aCELL].items->data;

	code = rxkst_StartClient(cParms);
	if (code) {
	    com_err(whoami, code, "StartClient returned");
	    return code;
	}
    } else {
	if (as->parms[aSENDLEN].items || as->parms[aRECVLEN].items
	    || as->parms[aFASTCALLS].items || as->parms[aSLOWCALLS].items
	    || as->parms[aCOPIOUSCALLS].items) {
	    code = RXKST_BADARGS;
	    com_err(whoami, code,
		    "send/recv len and # calls are client options");
	    return code;
	}

	/* donate this LWP to server-side */
	rx_ServerProc();
    }

    return 0;
}

void
main(argc, argv)
     IN int argc;
     IN char *argv[];
{
    long code;
#ifndef AFS_PTHREAD_ENV
    PROCESS initialProcess;
#endif
    struct cmd_syndesc *ts;

    whoami = argv[0];

    initialize_RXK_error_table();
    initialize_RKS_error_table();
    initialize_CMD_error_table();
    initialize_KTC_error_table();
    initialize_rx_error_table();

    code = rx_Init(0);
    rx_SetRxDeadTime(120);
    if (code < 0) {
	com_err(whoami, code, "can't init Rx");
	exit(1);
    }
#ifndef AFS_PTHREAD_ENV
    initialProcess = 0;
    code = LWP_CurrentProcess(&initialProcess);
    if (code) {
	com_err(whoami, code, "LWP initialization failed");
	exit(1);
    }
#endif
    ts = cmd_CreateSyntax(NULL, CommandProc, 0,
			  "run Rx authentication stress test");
    cmd_AddParm(ts, "-server", CMD_FLAG, CMD_OPTIONAL, "start server");
    cmd_AddParm(ts, "-client", CMD_SINGLE, CMD_OPTIONAL, "start client");
    cmd_AddParm(ts, "-sendlen", CMD_SINGLE, CMD_OPTIONAL,
		"bytes to send to server in Copious call");
    cmd_AddParm(ts, "-recvlen", CMD_SINGLE, CMD_OPTIONAL,
		"bytes to request from server in Copious call");
    cmd_AddParm(ts, "-fastcalls", CMD_SINGLE, CMD_OPTIONAL,
		"number of fast calls to make");
    cmd_AddParm(ts, "-slowcalls", CMD_SINGLE, CMD_OPTIONAL,
		"number of slow calls to make (one second)");
    cmd_AddParm(ts, "-copiouscalls", CMD_SINGLE, CMD_OPTIONAL,
		"number of fast calls to make");
    cmd_AddParm(ts, "-printstatistics", CMD_FLAG, CMD_OPTIONAL,
		"print statistics before exiting");
    cmd_AddParm(ts, "-printtimings", CMD_FLAG, CMD_OPTIONAL,
		"print timing information for calls");
    cmd_AddParm(ts, "-noexit", CMD_FLAG, CMD_OPTIONAL,
		"don't exit after successful finish");
    cmd_AddParm(ts, "-sthreads", CMD_SINGLE, CMD_OPTIONAL,
		"number server threads");
    cmd_AddParm(ts, "-cthreads", CMD_SINGLE, CMD_OPTIONAL,
		"number client threads");
    cmd_AddParm(ts, "-calltest", CMD_FLAG, CMD_OPTIONAL,
		"check server's call number verification (this takes about 30 seconds)");
    cmd_AddParm(ts, "-hijacktest", CMD_FLAG, CMD_OPTIONAL,
		"check hijack prevention measures by making various modifications to incoming/outgoing packets");
    cmd_AddParm(ts, "-authentication", CMD_SINGLE, CMD_OPTIONAL,
		"type of authentication to use; one of: none, clear, auth, crypt");
    cmd_AddParm(ts, "-minserverauth", CMD_SINGLE, CMD_OPTIONAL,
		"minimum level of authentication permitted by server");
    cmd_AddParm(ts, "-repeatinterval", CMD_SINGLE, CMD_OPTIONAL,
		"seconds between load test activity");
    cmd_AddParm(ts, "-repeatcount", CMD_SINGLE, CMD_OPTIONAL,
		"repetitions of load test activity");
    cmd_AddParm(ts, "-stopserver", CMD_FLAG, CMD_OPTIONAL,
		"send RPC to cause server to exit");
    cmd_AddParm(ts, "-trace", CMD_SINGLE, CMD_OPTIONAL,
		"file for per-call trace info");
    cmd_AddParm(ts, "-nomd1pkt", CMD_FLAG, CMD_OPTIONAL,
		"dont prefer one-packet calls");
    cmd_AddParm(ts, "-noreclaim", CMD_FLAG, CMD_OPTIONAL,
		"dont aggressively reclaim packets");
    cmd_AddParm(ts, "-no2dchoice", CMD_FLAG, CMD_OPTIONAL,
		"disable rx_getcall 2d choice code");
    cmd_AddParm(ts, "-maxskew", CMD_SINGLE, CMD_OPTIONAL,
		"max client server skew in seconds");
    cmd_AddParm(ts, "-usetokens", CMD_FLAG, CMD_OPTIONAL, "use ktc tokens");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "name of test cell");
    cmd_AddParm(ts, "-keyfile", CMD_SINGLE, CMD_OPTIONAL,
		"read server key from file");

    code = cmd_Dispatch(argc, argv);
    exit(code != 0);
}
