/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <lwp.h>
#include <rx/rx.h>
#include <afs/bubasics.h>
#include "bc.h"

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    struct rx_securityClass *rxsc;
    struct rx_connection *tconn;
    afs_int32 code;

    rx_Init(0);
    rxsc = rxnull_NewClientSecurityObject();
    tconn =
	rx_NewConnection(htonl(0x7f000001), htons(BC_MESSAGEPORT), 1, rxsc,
			 0);
    code = BC_Print(tconn, 1, 2, argv[1]);
    printf("Done, code %d\n", code);
    exit(0);
}
