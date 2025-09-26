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

#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/afs_lock.h>

#include "ubik.h"
#include "utst_int.h"

/* main program */

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    afs_int32 code;
    struct ubik_client *cstruct = 0;
    afs_uint32 serverList[MAXSERVERS];
    struct rx_connection *serverconns[MAXSERVERS];
    struct rx_securityClass *sc;
    afs_int32 i;
    afs_int32 temp;

    if (argc == 1) {
	printf
	    ("uclient: usage is 'uclient -servers ... [-try] [-get] [-inc] [-minc] [-trunc]\n");
	exit(0);
    }
#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit() < 0)
	return -1;
#endif
    /* first parse '-servers <server-1> <server-2> ... <server-n>' from command line */
    code = ubik_ParseClientList(argc, argv, serverList);
    if (code) {
	printf("could not parse server list, code %d\n", code);
	exit(1);
    }
    rx_Init(0);
    sc = rxnull_NewClientSecurityObject();
    for (i = 0; i < MAXSERVERS; i++) {
	if (serverList[i]) {
	    serverconns[i] =
		rx_NewConnection(serverList[i], htons(3000), USER_SERVICE_ID,
				 sc, 0);
	} else {
	    serverconns[i] = (struct rx_connection *)0;
	    break;
	}
    }

    /* next, pass list of server rx_connections (in serverconns), and
     * a place to put the returned client structure that we'll use in
     * all of our rpc calls (via ubik_Calll) */
    code = ubik_ClientInit(serverconns, &cstruct);

    /* check code from init */
    if (code) {
	printf("ubik client init failed with code %d\n", code);
	exit(1);
    }

    /* parse command line for our own operations */
    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-inc")) {
	    /* use ubik_Call to do the work, finding an up server and handling
	     * the job of finding a sync site, if need be */
	    code = ubik_SAMPLE_Inc(cstruct, 0);
	    printf("return code is %d\n", code);
	} else if (!strcmp(argv[i], "-try")) {
	    code = ubik_SAMPLE_Test(cstruct, 0);
	    printf("return code is %d\n", code);
	} else if (!strcmp(argv[i], "-qget")) {
	    code = ubik_SAMPLE_QGet(cstruct, 0, &temp);
	    printf("got quick value %d (code %d)\n", temp, code);
	} else if (!strcmp(argv[i], "-get")) {
	    code = ubik_SAMPLE_Get(cstruct, 0, &temp);
	    printf("got value %d (code %d)\n", temp, code);
	} else if (!strcmp(argv[i], "-trunc")) {
	    code = ubik_SAMPLE_Trun(cstruct, 0);
	    printf("return code is %d\n", code);
	} else if (!strcmp(argv[i], "-minc")) {
	    afs_int32 temp;
	    struct timeval tv;
	    tv.tv_sec = 1;
	    tv.tv_usec = 0;
	    printf("ubik_client: Running minc...\n");

	    while (1) {
		temp = 0;
		code = ubik_SAMPLE_Get(cstruct, 0, &temp);
		if (code != 0) {
		    printf("SAMPLE_Get #1 failed with code %ld\n",
			   afs_printable_int32_ld(code));
		} else {
		    printf("SAMPLE_Get #1 succeeded, got value %ld\n",
			   afs_printable_int32_ld(temp));
		}

		temp = 0;
		code = ubik_SAMPLE_Inc(cstruct, 0);
		if (code != 0) {
		    printf("SAMPLE_Inc #1 failed with code %ld\n",
			   afs_printable_int32_ld(code));
		} else {
		    printf("SAMPLE_Inc #1 succeeded, incremented integer\n");
		}
		temp = 0;
		code = ubik_SAMPLE_Get(cstruct, 0, &temp);
		if (code != 0) {
		    printf("SAMPLE_Get #2 failed with code %ld\n",
			   afs_printable_int32_ld(code));
		} else {
		    printf("SAMPLE_Get #2 succeeded, got value %ld\n",
			   afs_printable_int32_ld(temp));
		}

		temp = 0;
		code = ubik_SAMPLE_Inc(cstruct, 0);
		if (code != 0)
		    printf("SAMPLE_Inc #2 failed with code %ld\n",
			   afs_printable_int32_ld(code));
		else
		    printf("SAMPLE_Inc #2 succeeded, incremented integer\n");

		tv.tv_sec = 1;
		tv.tv_usec = 0;
#ifdef AFS_PTHREAD_ENV
		select(0, 0, 0, 0, &tv);
#else
		IOMGR_Select(0, 0, 0, 0, &tv);
#endif
		printf("Repeating the SAMPLE operations again...\n");
	    }
	} else if (!strcmp(argv[i], "-mget")) {
	    afs_int32 temp;
	    struct timeval tv;
	    tv.tv_sec = 1;
	    tv.tv_usec = 0;
	    while (1) {
		code = ubik_SAMPLE_Get(cstruct, 0, &temp);
		printf("got value %d (code %d)\n", temp, code);

		code = ubik_SAMPLE_Inc(cstruct, 0);
		printf("update return code is %d\n", code);

		code = ubik_SAMPLE_Get(cstruct, 0, &temp);
		printf("got value %d (code %d)\n", temp, code);

		code = ubik_SAMPLE_Get(cstruct, 0, &temp);
		printf("got value %d (code %d)\n", temp, code);

		tv.tv_sec = 1;
		tv.tv_usec = 0;
#ifdef AFS_PTHREAD_ENV
		select(0, 0, 0, 0, &tv);
#else
		IOMGR_Select(0, 0, 0, 0, &tv);
#endif
	    }
	}
    }
    return 0;
}
