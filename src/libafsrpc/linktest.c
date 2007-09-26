/*
 * Copyright (c) 2006
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <afsconfig.h>
#include <afs/stds.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <rx/rx.h>
#include <stdio.h>
#include <errno.h>
#include <afs/afsint.h>
#include <rx/rxstat.h>
#include <des/des.h>
#ifdef AFS_RXK5

#include <rx/rxk5.h>
#include <rx/rxk5errors.h>

#ifdef USING_K5SSL   
#include "k5ssl.h"   
#else
#if USING_SHISHI
#include <shishi.h>
#else 
#ifdef private
#undef private
#endif
#include <krb5.h>
#endif
#endif

#endif

afs_int32 RXSTATS_ExecuteRequest(struct rx_call *);
extern const char *RXAFSCB_function_names[];

	/* just enough smarts to exercise library linkage a bit. */

int
main(int ac, char **av)
{
    struct rx_connection *conn, **conns;
    struct rx_securityClass *sc;
    struct rx_securityClass *ssc[1];
    struct rx_service *service;
    char *name = "";
    int code;
    XDR xdrs[1];
    CBS cbs[1];
    struct timeval tv[1];
    afs_uint32 a, b;
#ifdef AFS_RXK5
    krb5_creds k5_creds[1];
#endif
    des_cblock key[1];

    initialize_rx_error_table();
    rx_Init(0);
#ifdef AFS_RXK5
    memset(k5_creds, 0, sizeof *k5_creds);
    sc = rxk5_NewClientSecurityObject(rxk5_auth, k5_creds, 0);
#endif
    sc = rxnull_NewClientSecurityObject();
    if (!sc) {
	afs_com_err("linktest", errno, "Can't make null security object.");
	exit(1);
    }
    conn = rx_NewConnection(htonl(0x7f000001), htons(7003), 52, sc, 0);
    if (!conn) {
#ifdef AFS_DARWIN_ENV
	/* without ranlib -c: these come up undefined:
	 *	rxkad_random_mutex des_random_mutex des_init_mutex
	 * So, this is just a hack...
	 */
	des_init_random_number_generator();
	rxkad_GetServerInfo();
#endif
	afs_com_err("linktest", errno, "Can't make rx connection.");
	exit(1);
    }
    code = RXAFS_GetRootVolume(conn, &name);
    code = RXAFSCB_GetLocalCell(conn, &name);
    code = RXSTATS_ClearProcessRPCStats(conn, 0U);
#ifdef AFS_RXK5
    code = rxk5_GetServerInfo2(conn, 0, 0, 0, 0, 0, 0);
    *ssc = rxk5_NewServerSecurityObject(rxk5_auth, name, rxk5_default_get_key, 0, 0);
#else
    *ssc = 0;
#endif
    service = rx_NewService(0,
	RX_STATS_SERVICE_ID,
	"rpcstats",
	ssc, 1,
	RXSTATS_ExecuteRequest);
    ucstring(name, name, 0);
    lcstring(name, name, 0);
    xdrs->x_op = XDR_FREE;
    memset(cbs, 0, sizeof *cbs);
    xdr_CBS(xdrs, cbs);
    TM_GetTimeOfDay(tv, 0);
    conns = &conn;
    multi_Rx(conns, 1) {
	multi_RXAFS_GetTime(&a, &b);
    } multi_End;
    if (code == 666) {
	puts(RXAFSCB_function_names[0]);
	puts(afs_error_table_name(code));
	des_string_to_key(name, key);
	des_cblock_print_file(key, stdout);
	des_quad_cksum(0, 0, 0, 0, 0);
	crypt(name, name);
    }
    rx_Finalize();
    exit(0);
}
