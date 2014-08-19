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
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#endif /* AFS_NT40_ENV */
#include <sys/stat.h>
#ifdef AFS_AIX_ENV
#include <sys/statfs.h>
#endif

#include <string.h>

#include <afs/dirpath.h>
#include <errno.h>
#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include "ubik.h"
#include <afs/afsint.h>
#include <afs/cmd.h>

/*!
 * \brief Get the appropriate type of ubik client structure out from the system.
 */
afs_int32
ugen_ClientInit(int noAuthFlag, const char *confDir, char *cellName, afs_int32 sauth,
	       struct ubik_client **uclientp,
	       int (*secproc) (struct rx_securityClass *, afs_int32),
	       char *funcName, afs_int32 gen_rxkad_level,
	       afs_int32 maxservers, char *serviceid, afs_int32 deadtime,
	       afs_uint32 server, afs_uint32 port, afs_int32 usrvid)
{
    afs_int32 code, secFlags, i;
    afs_int32 scIndex;
    struct afsconf_cell info;
    struct afsconf_dir *tdir;
    struct rx_securityClass *sc;
    /* This must change if VLDB_MAXSERVERS becomes larger than MAXSERVERS */
    static struct rx_connection *serverconns[MAXSERVERS];
    const char *confdir;

    code = rx_Init(0);
    if (code) {
	fprintf(stderr, "%s: could not initialize rx.\n", funcName);
	return code;
    }
    rx_SetRxDeadTime(deadtime);

    secFlags = AFSCONF_SECOPTS_FALLBACK_NULL;
    if (sauth) {
	secFlags |= AFSCONF_SECOPTS_LOCALAUTH;
	confdir = AFSDIR_SERVER_ETC_DIRPATH;
    } else {
	confdir = AFSDIR_CLIENT_ETC_DIRPATH;
    }

    if (noAuthFlag) {
	secFlags |= AFSCONF_SECOPTS_NOAUTH;
    }

    if (gen_rxkad_level == rxkad_crypt)
	secFlags |= AFSCONF_SECOPTS_ALWAYSENCRYPT;

    tdir = afsconf_Open(confdir);
    if (!tdir) {
	fprintf(stderr,
		"%s: Could not process files in configuration directory (%s).\n",
		funcName, confdir);
	return -1;
    }

    if (sauth)
	cellName = tdir->cellName;

    code = afsconf_GetCellInfo(tdir, cellName, serviceid, &info);
    if (code) {
	afsconf_Close(tdir);
	fprintf(stderr, "%s: can't find cell %s's hosts in %s/%s\n",
		funcName, cellName, confdir, AFSDIR_CELLSERVDB_FILE);
	return -1;
    }
    code = afsconf_PickClientSecObj(tdir, secFlags, &info, cellName, &sc,
				    &scIndex, NULL);
    if (code) {
	fprintf(stderr, "%s: can't create client security object", funcName);
	return -1;
    }
    if (scIndex == RX_SECIDX_NULL && !noAuthFlag) {
	fprintf(stderr,
		"%s: Could not get afs tokens, running unauthenticated.\n",
		funcName);
    }

    afsconf_Close(tdir);

    if (secproc)	/* tell UV module about default authentication */
	(*secproc) (sc, scIndex);
    if (server) {
	serverconns[0] = rx_NewConnection(server, port,
					  usrvid, sc, scIndex);
    } else {
	if (info.numServers > maxservers) {
	    fprintf(stderr,
		    "%s: info.numServers=%d (> maxservers=%d)\n",
		    funcName, info.numServers, maxservers);
	    return -1;
	}
	for (i = 0; i < info.numServers; i++) {
	    serverconns[i] =
		rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
				 info.hostAddr[i].sin_port, usrvid,
				 sc, scIndex);
	}
    }
    /* Are we just setting up connections, or is this really ubik stuff? */
    if (uclientp) {
	*uclientp = 0;
	code = ubik_ClientInit(serverconns, uclientp);
	if (code) {
	    fprintf(stderr, "%s: ubik client init failed.\n", funcName);
	    return code;
	}
    }
    return 0;
}


