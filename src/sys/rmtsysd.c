/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Daemon that implements remote procedure call service for non-vendor system
 * calls (currently setpag and pioctl). The AFS cache manager daemon, afsd,
 * currently fires up this module, when the "-rmtsys" flag is given.
 * This is the main routine for rmtsysd, which can be used separately from
 * afsd.
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/types.h>
#include <sys/ioctl.h>
#include <afs/vice.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <stdio.h>
#include <rx/xdr.h>
#include "rmtsys.h"


extern RMTSYS_ExecuteRequest();

#define N_SECURITY_OBJECTS 1	/* No real security yet */

#include "AFS_component_version_number.c"

int
main(int argc, char *argv[])
{
    struct rx_securityClass *(securityObjects[N_SECURITY_OBJECTS]);
    struct rx_service *service;

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
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    /* Initialize the rx-based RMTSYS server */
    if (rx_Init(htons(AFSCONF_RMTSYSPORT)) < 0)
	rmt_Quit("rx_init");
    securityObjects[0] = rxnull_NewServerSecurityObject();
    if (securityObjects[0] == (struct rx_securityClass *)0)
	rmt_Quit("rxnull_NewServerSecurityObject");
    service =
	rx_NewService(0, RMTSYS_SERVICEID, AFSCONF_RMTSYSSERVICE,
		      securityObjects, N_SECURITY_OBJECTS,
		      RMTSYS_ExecuteRequest);
    if (service == (struct rx_service *)0)
	rmt_Quit("rx_NewService");
    /* One may wish to tune some default RX params for better performance
     * at some point... */
    rx_SetMaxProcs(service, 2);
    rx_StartServer(1);		/* Donate this process to the server process pool */
    return 0;
}
