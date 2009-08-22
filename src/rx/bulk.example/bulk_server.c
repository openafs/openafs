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
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <rx/xdr.h>
#include "bulk.h"

#define N_SECURITY_OBJECTS 1

void Quit(char *msg);

int
main(int argc, char **argv)
{
    struct rx_securityClass *(securityObjects[N_SECURITY_OBJECTS]);
    struct rx_service *service;

    /* Initialize Rx, telling it port number this server will use for its single service */
    if (rx_Init(BULK_SERVER_PORT) < 0)
	Quit("rx_init");

    /* Create a single security object, in this case the null security object, for unauthenticated connections, which will be used to control security on connections made to this server */
    securityObjects[BULK_NULL] = rxnull_NewServerSecurityObject();
    if (securityObjects[BULK_NULL] == (struct rx_securityClass *)0)
	Quit("rxnull_NewServerSecurityObject");

    /* Instantiate a single BULK service.  The rxgen-generated procedure which is called to decode requests is passed in here (BULK_ExecuteRequest). */
    service =
	rx_NewService(0, BULK_SERVICE_ID, "BULK", securityObjects,
		      N_SECURITY_OBJECTS, BULK_ExecuteRequest);
    if (service == (struct rx_service *)0)
	Quit("rx_NewService");
    rx_SetMaxProcs(service, 2);

    rx_StartServer(1);		/* Donate this process to the server process pool */
    Quit("StartServer returned?");
}

int
BULK_FetchFile(struct rx_call *call, int verbose, char *name)
{
    int fd = -1;
    int error = 0;
    struct stat status;
    if (verbose)
	printf("Fetch file %s\n", name);
    fd = open(name, O_RDONLY, 0);
    if (fd < 0 || fstat(fd, &status) < 0) {
	if (verbose)
	    printf("Failed to open %s\n", name);
	error = BULK_ERROR;
    }
    if (!error)
	error = bulk_SendFile(fd, call, &status);
    if (fd >= 0)
	close(fd);
    return error;
}

int
BULK_StoreFile(struct rx_call *call, int verbose, char *name)
{
    int fd = -1;
    struct stat status;
    int error = 0;
    if (verbose)
	printf("Store file %s\n", name);
    fd = open(name, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd < 0 || fstat(fd, &status) < 0) {
	fprintf(stderr, "Could not create %s\n", name);
	error = BULK_ERROR;
    }
    if (!error)
	error = bulk_ReceiveFile(fd, call, &status);
    if (fd >= 0)
	close(fd);
    return error;
}

void
Quit(char *msg)
{
    fprintf(stderr, msg);
    exit(1);
}
