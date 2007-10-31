/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/vldbint.h>

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <string.h>

#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#include <ctype.h>
#include <sys/types.h>
#include <afs/cmd.h>
#include <afs/cellconfig.h>
#include <rx/rx.h>
#include <rx/xdr.h>

#include <ubik.h>
#include <afs/kauth.h>
#include <afs/afsutil.h>

/*
File servers in NW byte order.
*/

int server_count = 0;
afs_int32 server_id[256];

struct ubik_client *client;

struct ViceIds {
    int ViceIds_len;
    afs_int32 *ViceIds_val;
};

struct IPAddrs {
    int IPAddrs_len;
    afs_int32 *IPAddrs_val;
};

struct ubik_dbase *VL_dbase;
struct afsconf_dir *vldb_confdir;
struct kadstats dynamic_statistics;
struct rx_securityClass *junk;

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern int VL_GetAddrs();

static int
InvalidateCache(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code = 0;
    struct cmd_item *u;
    struct rx_connection *conn;
    int i;
    afs_int32 port = 7000;

    afs_int32 spare1 = 0;
    afs_int32 spare2, spare3;

    afs_int32 id[256];
    afs_int32 ip[256];

    struct ViceIds vid;
    struct IPAddrs ipa;

    code = ListServers();
    if (code)
	return code;


    /* make sure something there */

    if (!as->parms[0].items && !as->parms[1].items) {
	printf("Use -help flag for list of optional argmuments\n");
	return 1;
    }

    /* get user ids */

    for (i = 0, u = as->parms[0].items; i < 255 && u; ++i, u = u->next) {
	code = util_GetInt32(u->data, &id[i]);
	if (code) {
	    printf("Fatal error: bad conversion to long for %s\n", u->data);
	    return code;
	}
    }

    id[i] = 0;
    vid.ViceIds_len = i;
    vid.ViceIds_val = id;

    /* get IP addresses, convert to NW byte order */

    for (i = 0, u = as->parms[1].items; i < 255 && u; ++i, u = u->next)
	ip[i] = inet_addr(u->data);

    ip[i] = 0;
    ipa.IPAddrs_len = i;
    ipa.IPAddrs_val = ip;

    for (i = 0; i < server_count; ++i) {
	conn = rx_NewConnection(server_id[i], htonl(port), 1, junk, 0);
	if (!conn) {
	    printf("Informational: could not connect to \
file server %lx\n", server_id[i]);
	    continue;
	}

	/* invalidate the cache */

	code = RXAFS_FlushCPS(conn, &vid, &ipa, spare1, &spare2, &spare3);

	/*
	 * May get spurious error codes in case server is
	 * down or is reported by VLDB as a file server
	 * even though it is not configured as such in the
	 * cell.
	 */

	if (code)
	    printf("Informational: failed to invalidate \
file server %lx cache code = %ld\n", server_id[i], code);

	rx_DestroyConnection(conn);
    }
    return 0;
}

/*
Obtain list of file servers as known to VLDB. These may
not actually be configured as file servers in the cell.
*/

afs_int32
ListServers()
{
    afs_int32 code;
    struct rx_connection *conn;
    struct rx_call *call;
    int i;
    int byte_count;
    int nentries;
    afs_int32 base, index;

    afs_int32 Handle = 0;
    afs_int32 spare2 = 0;
    struct VLCallBack spare3;

    bulkaddrs addrs, m_addrs;
    ListAddrByAttributes m_attrs;
    afs_int32 m_unique, m_nentries;
    afs_uint32 *p;

    /* get list of file servers in NW byte order */
    memset(&addrs, 0, sizeof(addrs));
    memset(&spare3, 0, sizeof(spare3));
    code =
	ubik_Call(VL_GetAddrs, client, 0, Handle, spare2, &spare3,
		  &server_count, &addrs);
    if (code) {
	printf("Fatal error: could not get list of file servers\n");
	return 1;
    }

    for (i = 0, p = addrs.bulkaddrs_val; i < server_count; ++i, ++p) {
	if (((*p & 0xff000000) == 0xff000000) && ((*p) & 0xffff)) {
	    if ((base >= 0) && (base <= VL_MAX_ADDREXTBLKS) && (index >= 1)
		&& (index <= VL_MHSRV_PERBLK)) {
		m_attrs.Mask = VLADDR_INDEX;
		m_attrs.index = (base * VL_MHSRV_PERBLK) + index;
		m_nentries = 0;
		m_addrs.bulkaddrs_val = 0;
		m_addrs.bulkaddrs_len = 0;
		code =
		    ubik_Call(VL_GetAddrsU, client, 0, &m_attrs, &m_uuid,
			      &m_unique, &m_nentries, &m_addrs);
		if (vcode)
		    return code;

		m_addrp = (afs_int32 *) m_addrs.bulkaddrs_val;
		for (j = 0; j < m_nentries; j++, m_addrp++) {
		    server_id[i] = *m_addrp;
		    *m_addrp = htonl(*m_addrp);
		    printf("host %s\n", hostutil_GetNameByINet(*p));
		}
	    }
	} else {
	    server_id[i] = *p;
	    *p = htonl(*p);
	    printf("host %s\n", hostutil_GetNameByINet(*p));
	}
    }
    return code;
}

static int
GetServerList(struct cmd_syndesc *as, void *arock)
{
    int code;
    int i;

    code = ListServers();
    if (code)
	return (code);

    printf("There are %d file servers in the cell\n\n", server_count);
    fflush(stdout);
    for (i = 0; i < server_count; ++i)
	printf("%s\n", hostutil_GetNameByINet(server_id[i]));
    fflush(stdout);

    return code;
}

/*
User enters lists of:

	1. AFS user ids - say from "pts exam username".
	2. IP addresses - say from /etc/hosts (no wildcards).

Command is executed in user's cell.
*/

static int
MyBeforeProc(struct cmd_syndesc *as, void *arock)
{
    register char *tcell = NULL;
    char confdir[200];
    struct afsconf_dir *tdir;
    struct afsconf_cell info;
    struct rx_connection *serverconns[MAXSERVERS];
    register afs_int32 code, i;
    register afs_int32 sauth;

    sprintf(confdir, "%s", AFSDIR_CLIENT_ETC_DIRPATH);
    /* setup to talk to servers */
    code = rx_Init(0);
    if (code)
	printf("Warning: could not initialize network communication.\n");

    junk = rxnull_NewClientSecurityObject();
    tdir = afsconf_Open(confdir);
    if (!tdir)
	printf("Warning: could not get cell configuration.\n");

    if (as->parms[2].items)	/* if -cell specified */
	tcell = as->parms[2].items->data;
    code = afsconf_GetCellInfo(tdir, tcell, AFSCONF_VLDBSERVICE, &info);
    if (info.numServers > MAXSERVERS)
	printf("Warning: could not init cell info.\n");

    for (i = 0; i < info.numServers; ++i)
	serverconns[i] =
	    rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
			     info.hostAddr[i].sin_port, USER_SERVICE_ID, junk,
			     0);
    for (; i < MAXSERVERS; ++i) {
	serverconns[i] = (struct rx_connection *)0;
    }
    code = ubik_ClientInit(serverconns, &client);
    if (code)
	printf("Warning: could not initialize RPC interface.\n");
}


int
main(argc, argv)
     int argc;
     char **argv;
{
    afs_int32 code = 0;
    struct cmd_syndesc *ts;
    int i;

#ifdef	AFS_AIX32_ENV
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif

    /*
     * Look in /usr/vice/etc (client side database).
     */
    cmd_SetBeforeProc(MyBeforeProc, NULL);

    ts = cmd_CreateSyntax("initcmd" /*"invalidatecache" */ , InvalidateCache,
			  NULL, "invalidate server ACL cache");
    cmd_AddParm(ts, "-id", CMD_LIST, CMD_OPTIONAL, "user identifier");
    cmd_AddParm(ts, "-ip", CMD_LIST, CMD_OPTIONAL, "IP address");
    cmd_CreateAlias(ts, "ic");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");

    ts = cmd_CreateSyntax("listservers", GetServerList, NULL,
			  "list servers in the cell");
    cmd_CreateAlias(ts, "ls");

    code = cmd_Dispatch(argc, argv);

    rx_Finalize();

    exit(code);
}
