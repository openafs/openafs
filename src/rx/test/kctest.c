/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"


#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <signal.h>
#include "xdr.h"
#include "rx.h"
#include "rx_globals.h"
#include "rx_null.h"
#if RX_VAB_EXISTS
#include "rx_vab.h"
#endif

static long host;
static short port;
static short count;
static short secLevel = 0;
static short stats = 0;

#if RX_VAB_EXISTS
static
MakeVTest(akey, aticket, asession)
     struct rxvab_EncryptionKey *akey, *asession;
     struct rxvab_Ticket *aticket;
{
    aticket->ViceId = htonl(71);
    memcpy(&aticket->HandShakeKey, "testkeyx", 8);
    memcpy(asession, "testkeyx", 8);
    bcrypt_encrypt(aticket, aticket, sizeof(struct rxvab_Ticket), akey);
    return 0;
}
#else
#define MakeVTest(a,b,c) (printf ("rx_vab support removed\n"), exit (-1))
#endif

void
SigInt(int ignore)
{
    if (rx_debugFile) {
	rx_PrintStats(rx_debugFile);
	fflush(rx_debugFile);
    }
    if (stats)
	rx_PrintStats(stdout);
    rx_Finalize();
    exit(1);
}

static
ParseCmd(argc, argv)
     int argc;
     char **argv;
{
    int i;
    struct hostent *th;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-port")) {
	    port = atoi(argv[i + 1]);
	    i++;
	} else if (!strcmp(argv[i], "-host")) {
	    th = gethostbyname(argv[i + 1]);
	    if (!th) {
		printf("could not find host '%s' in host table\n",
		       argv[i + 1]);
		return -1;
	    }
	    memcpy(&host, th->h_addr, sizeof(long));
	    i++;
	} else if (!strcmp(argv[i], "-count")) {
	    count = atoi(argv[i + 1]);
	    i++;
	} else if (!strcmp(argv[i], "-security")) {
	    secLevel = atoi(argv[i + 1]);
	    i++;
	} else if (!strcmp(argv[i], "-log")) {
	    rx_debugFile = fopen("kctest.log", "w");
	    if (rx_debugFile == NULL)
		printf("Couldn't open rx_stest.db");
	    signal(SIGINT, SigInt);
	} else if (!strcmp(argv[i], "-stats"))
	    stats = 1;
	else {
	    printf("unrecognized switch '%s'\n", argv[i]);
	    return -1;
	}
    }
    return 0;
}

nowms()
{
    struct timeval tv;
    long temp;

    gettimeofday(&tv, 0);
    temp = ((tv.tv_sec & 0xffff) * 1000) + (tv.tv_usec / 1000);
    return temp;
}

main(argc, argv)
     int argc;
     char **argv;
{
    struct rx_securityClass *so;
    struct rx_connection *tconn;
    struct rx_call *tcall;
    XDR xdr;
    int i, startms, endms;
    long temp;
#if RX_VAB_EXISTS
    struct rxvab_Ticket ticket;
    struct rxvab_EncryptionKey session;
#endif

    host = htonl(0x7f000001);
    port = htons(10000);
    count = 1;
    if (ParseCmd(argc, argv) != 0) {
	printf("error parsing commands\n");
	exit(1);
    }
    rx_Init(0);
    if (secLevel == 0)
	so = rxnull_NewClientSecurityObject();
    else if (secLevel == 1) {
	MakeVTest((struct rxvab_EncryptionKey *)"applexxx", &ticket,
		  &session);
#if RX_VAB_EXISTS
	so = rxvab_NewClientSecurityObject(&session, &ticket, 0);
#endif
    } else if (secLevel == 2) {
	MakeVTest((struct rxvab_EncryptionKey *)"applexxx", &ticket,
		  &session);
#if RX_VAB_EXISTS
	so = rxvab_NewClientSecurityObject(&session, &ticket, 1);
#endif
    } else {
	printf("bad security index\n");
	exit(1);
    }
    if (!so) {
	printf("failed to create security obj\n");
	exit(1);
    }
    tconn = rx_NewConnection(host, port, 1, so, secLevel);
    printf("conn is %x\n", tconn);

    startms = nowms();
    for (i = 0; i < count; i++) {
	tcall = rx_NewCall(tconn);
	/* fill in data */
	xdrrx_create(&xdr, tcall, XDR_ENCODE);
	temp = 1988;
	xdr_long(&xdr, &temp);
	xdr.x_op = XDR_DECODE;
	xdr_long(&xdr, &temp);
	if (temp != 1989)
	    printf("wrong value returned (%d)\n", temp);
	rx_EndCall(tcall, 0);
    }
    endms = nowms();
    printf("That was %d ms per call.\n", (endms - startms) / count);
    printf("Done.\n");
#ifdef RXDEBUG
    if (stats)
	rx_PrintStats(stdout);
#endif
    SigInt(0);
}
