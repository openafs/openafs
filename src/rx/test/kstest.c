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

static int port;
static short stats = 0;

void
SigInt(int ignore)
{
    if (rx_debugFile) {
	rx_PrintStats(rx_debugFile);
	fflush(rx_debugFile);
    }
    if (stats)
	rx_PrintStats(stdout);
    exit(1);
}


static
ParseCmd(argc, argv)
     int argc;
     char **argv;
{
    int i;
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-port")) {
	    port = atoi(argv[i + 1]);
	    port = htons(port);
	    i++;
	} else if (!strcmp(argv[i], "-log")) {
	    rx_debugFile = fopen("kstest.log", "w");
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

/* er loop */
static
rxk_erproc(acall)
     struct rx_call *acall;
{
    XDR xdr;
    long temp;

    xdrrx_create(&xdr, acall, XDR_DECODE);
    xdr_long(&xdr, &temp);
    temp++;
    xdr.x_op = XDR_ENCODE;
    xdr_long(&xdr, &temp);
    return 0;
}

main(argc, argv)
     int argc;
     char **argv;
{
    long code;
    static struct rx_securityClass *sc[3];	/* so other kernel procs can reference it */
    struct rx_service *tservice;

    port = htons(10000);
    if (ParseCmd(argc, argv) != 0) {
	printf("erorr parsing commands\n");
	exit(1);
    }
    code = rx_Init(port);
    if (code) {
	printf("init failed code %d\n", code);
	exit(1);
    }
    signal(SIGINT, SigInt);
    printf("back from rx server init\n");
    sc[0] = rxnull_NewServerSecurityObject();
    sc[1] = sc[2] = 0;
    printf("new secobj created\n");
    tservice = rx_NewService(0, 1, "test", sc, 1 /* 3 */ , rxk_erproc);
    printf("service is %x\n", tservice);
    if (!tservice) {
	printf("failed to create service\n");
	exit(1);
    }
    rx_StartServer(1);		/* donate self */
}
