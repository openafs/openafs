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

#include <afs/vldbint.h>


#include <stdio.h>
#include <string.h>

#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#include <ctype.h>
#include <sys/types.h>
#include <afs/auth.h>
#include <afs/cmd.h>
#include <afs/cellconfig.h>
#include <afs/afsint.h>
#include <afs/vlserver.h>
#include <rx/rx.h>
#include <rx/xdr.h>

#include <ubik.h>
#include <afs/kauth.h>
#include <afs/afsutil.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
File servers in NW byte order.
*/

static afs_int32 server_count = 0;
static afs_int32 server_id[256];

static struct ubik_client *client;

static struct rx_securityClass *sc;
static int scindex;

/*
Obtain list of file servers as known to VLDB. These may
not actually be configured as file servers in the cell.
*/

static afs_int32
ListServers(void)
{
    afs_int32 code;
    int i;
    struct VLCallBack vlcb;
    struct VLCallBack spare3;
    bulkaddrs addrs, m_addrs;
    afs_uint32 ip;
    afs_int32 base, index;
    afsUUID m_uuid;
    afs_int32 m_uniq = 0;
    afs_int32 m_nentries;
    char hoststr[16];
    ListAddrByAttributes m_attrs;

    memset(&m_attrs, 0, sizeof(m_attrs));
    memset(&addrs, 0, sizeof(addrs));
    memset(&spare3, 0, sizeof(spare3));
    code =
	ubik_VL_GetAddrs(client, 0, 0, 0, &vlcb,
		  &server_count, &addrs);
    if (code) {
	printf("Fatal error: could not get list of file servers\n");
	return 1;
    }

    for (i = 0; i < server_count; ++i) {
	ip = addrs.bulkaddrs_val[i];

	if (((ip & 0xff000000) == 0xff000000) && (ip & 0xffff)) {
	    base = (ip >> 16) & 0xff;
	    index = ip & 0xffff;

	    /* server is a multihomed host; query the vldb for its addresses,
	     * and just pick the first one */

	    if ((base >= 0) && (base <= VL_MAX_ADDREXTBLKS) && (index >= 1)
	        && (index <= VL_MHSRV_PERBLK)) {

	        m_attrs.Mask = VLADDR_INDEX;
		m_attrs.index = (base * VL_MHSRV_PERBLK) + index;
		m_nentries = 0;
		m_addrs.bulkaddrs_val = 0;
		m_addrs.bulkaddrs_len = 0;

		code = ubik_VL_GetAddrsU(client, 0, &m_attrs, &m_uuid, &m_uniq,
		                         &m_nentries, &m_addrs);

		if (code || m_addrs.bulkaddrs_len == 0) {
		    printf("Error getting multihomed addresses for server "
		           "%s (index %ld)\n",
			   afs_inet_ntoa_r(m_addrs.bulkaddrs_val[0], hoststr),
			   afs_printable_int32_ld(m_attrs.index));
		    server_id[i] = 0;
		} else {
		    server_id[i] = htonl(m_addrs.bulkaddrs_val[0]);
		}
	    }
	} else {
	    server_id[i] = htonl(addrs.bulkaddrs_val[i]);
	}
    }

    return code;
}


static int
InvalidateCache(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code = 0;
    struct cmd_item *u;
    struct rx_connection *conn;
    int i;
    afs_int32 port = 7000;
    char hoststr[16];
    int err = 0;

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
	if (!server_id[i]) {
	    err = 1;
	    continue;
	}
	conn = rx_NewConnection(server_id[i], htons(port), 1, sc, scindex);
	if (!conn) {
	    printf("Informational: could not connect to \
file server %s\n", afs_inet_ntoa_r(server_id[i], hoststr));
	    err = 1;
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

	if (code) {
	    printf("Informational: failed to invalidate \
file server %s cache code = %ld\n", afs_inet_ntoa_r(server_id[i], hoststr),
	    afs_printable_int32_ld(code));
	    err = 1;
	}

	rx_DestroyConnection(conn);
    }
    return err;
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
    char *tcell = NULL;
    char confdir[200];
    struct afsconf_dir *tdir;
    struct afsconf_cell info;
    struct rx_connection *serverconns[MAXSERVERS];
    afs_int32 code, i;
    struct rx_securityClass *scnull;

    sprintf(confdir, "%s", AFSDIR_CLIENT_ETC_DIRPATH);
    if (as->parms[4].items) { /* -localauth */
	sprintf(confdir, "%s", AFSDIR_SERVER_ETC_DIRPATH);
    }

    /* setup to talk to servers */
    code = rx_Init(0);
    if (code) {
	printf("Warning: could not initialize network communication.\n");
	return 1;
    }

    scnull = sc = rxnull_NewClientSecurityObject();
    scindex = 0;

    tdir = afsconf_Open(confdir);
    if (!tdir) {
	printf("Warning: could not get cell configuration.\n");
	return 1;
    }

    if (as->parms[2].items)	/* if -cell specified */
	tcell = as->parms[2].items->data;
    code = afsconf_GetCellInfo(tdir, tcell, AFSCONF_VLDBSERVICE, &info);
    if (code || info.numServers > MAXSERVERS) {
	printf("Warning: could not init cell info.\n");
	return 1;
    }

    if (as->parms[4].items) { /* -localauth */
	code = afsconf_ClientAuth(tdir, &sc, &scindex);
	if (code || scindex == 0) {
	    afsconf_Close(tdir);
	    fprintf(stderr, "Could not get security object for -localauth (code: %d)\n",
	            code);
	    return -1;
	}

    } else if (!as->parms[3].items) { /* not -noauth */
	struct ktc_principal sname;
	struct ktc_token ttoken;

	strcpy(sname.cell, info.name);
	sname.instance[0] = '\0';
	strcpy(sname.name, "afs");

	code = ktc_GetToken(&sname, &ttoken, sizeof(ttoken), NULL);
	if (code) {
	    fprintf(stderr, "Could not get afs tokens, running unauthenticated\n");
	} else {
	    scindex = 2;
	    sc = rxkad_NewClientSecurityObject(rxkad_auth, &ttoken.sessionKey,
	                                       ttoken.kvno, ttoken.ticketLen,
	                                       ttoken.ticket);
	}
    }

    for (i = 0; i < info.numServers; ++i)
	serverconns[i] =
	    rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
			     info.hostAddr[i].sin_port, USER_SERVICE_ID, scnull,
			     0);
    for (; i < MAXSERVERS; ++i) {
	serverconns[i] = (struct rx_connection *)0;
    }
    code = ubik_ClientInit(serverconns, &client);
    if (code) {
	printf("Warning: could not initialize RPC interface.\n");
	return 1;
    }
    return 0;
}


int
main(int argc, char **argv)
{
    afs_int32 code = 0;
    struct cmd_syndesc *ts;

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
    cmd_AddParm(ts, "-noauth", CMD_FLAG, CMD_OPTIONAL, "don't authenticate");
    cmd_AddParm(ts, "-localauth", CMD_FLAG, CMD_OPTIONAL, "user server tickets");

    ts = cmd_CreateSyntax("listservers", GetServerList, NULL,
			  "list servers in the cell");
    cmd_CreateAlias(ts, "ls");

    code = cmd_Dispatch(argc, argv);

    rx_Finalize();

    exit(code);
}
