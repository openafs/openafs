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
#include <afs/stds.h>

#include <roken.h>

#include <afs/afsint.h>
#include <rx/rx_globals.h>
#include <ubik.h>

struct ubik_client *cstruct;
struct rx_connection *serverconns[MAXSERVERS];
char *(args[50]);

afs_int32
pxclient_Initialize(int auth, afs_int32 serverAddr)
{
    afs_int32 code;
    rx_securityIndex scIndex;
    struct rx_securityClass *sc;

    code = rx_Init(htons(2115) /*0 */ );
    if (code) {
	fprintf(stderr, "pxclient_Initialize:  Could not initialize rx.\n");
	return code;
    }
    scIndex = RX_SECIDX_NULL;
    rx_SetRxDeadTime(50);
    sc = rxnull_NewClientSecurityObject();
    serverconns[0] =
	rx_NewConnection(serverAddr, htons(7000), 1, sc, scIndex);

    code = ubik_ClientInit(serverconns, &cstruct);

    if (code) {
	fprintf(stderr, "pxclient_Initialize: ubik client init failed.\n");
	return code;
    }
    return 0;
}

/* main program */

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    char **av = argv;
    struct sockaddr_in host;
    afs_int32 code;
    struct hostent *hp;
    struct timeval tv;
    int noAuth = 1;		/* Default is authenticated connections */

    argc--, av++;
    if (argc < 1) {
	printf("usage: fsprobe <serverHost>\n");
	exit(1);
    }
    memset(&host, 0, sizeof(struct sockaddr_in));
    host.sin_family = AF_INET;
    host.sin_addr.s_addr = inet_addr(av[0]);
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    host.sin_len = sizeof(struct sockaddr_in);
#endif
    if (host.sin_addr.s_addr == -1) {
	hp = gethostbyname(av[0]);
	if (hp) {
	    host.sin_family = hp->h_addrtype;
	    memcpy((caddr_t) & host.sin_addr, hp->h_addr, hp->h_length);
	} else {
	    printf("unknown server host %s\n", av[0]);
	    exit(1);
	}
    }
    if ((code = pxclient_Initialize(noAuth, host.sin_addr.s_addr)) != 0) {
	printf("Couldn't initialize fs library (code=%d).\n", code);
	exit(1);
    }

    code = RXAFS_GetTime(cstruct->conns[0], (afs_uint32 *)&tv.tv_sec, (afs_uint32 *)&tv.tv_usec);
    if (!code)
	printf("AFS_GetTime on %s sec=%ld, usec=%ld\n", av[0], (long)tv.tv_sec,
	       (long)tv.tv_usec);
    else
	printf("return code is %d\n", code);

    return 0;
}


void
GetArgs(char *line, char **args, int *nargs)
{
    *nargs = 0;
    while (*line) {
	char *last = line;
	while (*line == ' ')
	    line++;
	if (*last == ' ')
	    *last = 0;
	if (!*line)
	    break;
	*args++ = line, (*nargs)++;
	while (*line && *line != ' ')
	    line++;
    }
}
