/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/types.h>
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#include <rx/rx.h>

/* services available on incoming message port */
BC_Print(acall, acode, aflags, amessage)
     struct rx_call *acall;
     afs_int32 acode, aflags;
     char *amessage;
{
    struct rx_connection *tconn;
    struct rx_peer *tpeer;

    tconn = rx_ConnectionOf(acall);
    tpeer = rx_PeerOf(tconn);
    printf("From %08x: %s <%d>\n", tpeer->host, amessage, acode);
    return 0;
}
