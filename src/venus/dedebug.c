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

RCSID("$Header$");


#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <afs/afscbint.h>
#include <afs/cmd.h>
#include <afs/com_err.h>
#include <rx/rx.h>
#include <lock.h>

extern struct rx_securityClass *rxnull_NewServerSecurityObject();
extern struct hostent *hostutil_GetHostByName();

static PrintCacheEntries(struct rx_connection *aconn, int aint32)
{
    register int i;
    register afs_int32 code, addr, inode, flags, time;
    char *fileName;

    for(i=0;i<100000;i++) {
	code = RXAFSCB_GetDE(aconn, i, &addr, &inode, &flags, &time, &fileName);
	if (code) {
	    if (code == 1) break;
	    printf("cmdebug: failed to get cache entry %d (%s)\n", i,
		   afs_error_message(code));
	    return code;
	}

	/* otherwise print this entry */
	printf("%d: ** dentry %d %08x %d %d %s\n",
	       i, addr, inode, flags, time, fileName);

	printf("\n");
    }
    printf("Returned %d entries.\n", i);
    return 0;
}

static int
CommandProc(struct cmd_syndesc *as, void *arock)
{
    struct rx_connection *conn;
    register char *hostName;
    register struct hostent *thp;
    afs_int32 port;
    struct rx_securityClass *secobj;
    int int32p;
    afs_int32 addr;

    hostName = as->parms[0].items->data;
    if (as->parms[1].items)
	port = atoi(as->parms[1].items->data);
    else
	port = 7001;
    thp = hostutil_GetHostByName(hostName);
    if (!thp) {
	printf("cmdebug: can't resolve address for host %s.\n", hostName);
	exit(1);
    }
    memcpy(&addr, thp->h_addr, sizeof(afs_int32));
    secobj = rxnull_NewServerSecurityObject();
    conn = rx_NewConnection(addr, htons(port), 1, secobj, 0);
    if (!conn) {
	printf("cmdebug: failed to create connection for host %s\n", hostName);
	exit(1);
    }
    if (as->parms[2].items) int32p = 1;
    else int32p = 0;
    PrintCacheEntries(conn, int32p);
    return 0;
}

#include "AFS_component_version_number.c"

main(argc, argv)
int argc;
char **argv; {
    register struct cmd_syndesc *ts;

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
    rx_Init(0);

    ts = cmd_CreateSyntax(NULL, CommandProc, NULL, "probe unik server");
    cmd_AddParm(ts, "-servers", CMD_SINGLE, CMD_REQUIRED, "server machine");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "IP port");
    cmd_AddParm(ts, "-long", CMD_FLAG, CMD_OPTIONAL, "print all info");

    cmd_Dispatch(argc, argv);
    exit(0);
}
