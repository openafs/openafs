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

#ifdef AFS_AIX_ENV
# include <sys/statfs.h>
#endif

#include <afs/dirpath.h>
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

static int
internal_client_init(struct afsconf_dir *dir, struct afsconf_cell *info,
		     int secFlags, struct ubik_client **uclientp,
		     ugen_secproc_func secproc,
		     int maxservers, const char *serviceid, int deadtime,
		     afs_uint32 server, afs_uint32 port, afs_int32 usrvid)
{
    int code, i;
    afs_int32 scIndex;
    struct rx_securityClass *sc;
    /* This must change if VLDB_MAXSERVERS becomes larger than MAXSERVERS */
    static struct rx_connection *serverconns[MAXSERVERS];
    const char *progname;

    progname = getprogname();
    if (progname == NULL)
	progname = "<unknown>";

    code = rx_Init(0);
    if (code) {
	fprintf(stderr, "%s: could not initialize rx.\n", progname);
	return code;
    }
    rx_SetRxDeadTime(deadtime);

    code = afsconf_PickClientSecObj(dir, secFlags, info, NULL, &sc,
				    &scIndex, NULL);
    if (code) {
	fprintf(stderr, "%s: can't create client security object", progname);
	return code;
    }

    if (scIndex == RX_SECIDX_NULL && !(secFlags & AFSCONF_SECOPTS_NOAUTH)) {
	fprintf(stderr,
		"%s: Could not get afs tokens, running unauthenticated.\n",
		progname);
    }

    if (secproc)	/* tell UV module about default authentication */
	(*secproc) (sc, scIndex);

    if (server) {
	serverconns[0] = rx_NewConnection(server, port,
					  usrvid, sc, scIndex);
    } else {
	if (info->numServers > maxservers) {
	    fprintf(stderr,
		    "%s: info.numServers=%d (> maxservers=%d)\n",
		    progname, info->numServers, maxservers);
	    return -1;
	}
	for (i = 0; i < info->numServers; i++) {
	    if (!info->hostAddr[i].sin_port && port)
		info->hostAddr[i].sin_port = port;
	    serverconns[i] =
		rx_NewConnection(info->hostAddr[i].sin_addr.s_addr,
				 info->hostAddr[i].sin_port, usrvid,
				 sc, scIndex);
	}
    }
    /* Are we just setting up connections, or is this really ubik stuff? */
    if (uclientp) {
	*uclientp = 0;
	code = ubik_ClientInit(serverconns, uclientp);
	if (code) {
	    fprintf(stderr, "%s: ubik client init failed.\n", progname);
	    return code;
	}
    }

    return 0;
}

int
ugen_ClientInitCell(struct afsconf_dir *dir, struct afsconf_cell *info,
		    int secFlags, struct ubik_client **uclientp,
		    int maxservers, const char *serviceid, int deadtime)
{
    return internal_client_init(dir, info, secFlags, uclientp, NULL,
			        maxservers, serviceid, deadtime, 0, 0,
				USER_SERVICE_ID);
}

static int
internal_client_init_dir(const char *confDir, char *cellName, int secFlags,
		      struct ubik_client **uclientp,
		      ugen_secproc_func secproc,
		      afs_int32 maxservers, char *serviceid, afs_int32 deadtime,
		      afs_uint32 server, afs_uint32 port, afs_int32 usrvid)
{
    int code;
    const char *progname;
    struct afsconf_dir *dir;
    struct afsconf_cell info;

    progname = getprogname();
    if (progname == NULL)
	progname = "<unknown>";

    if (confDir == NULL)
	confDir = AFSDIR_CLIENT_ETC_DIRPATH;

    dir = afsconf_Open(confDir);
    if (!dir) {
	fprintf(stderr,
		"%s: Could not process files in configuration directory (%s).\n",
		progname, confDir);
	return EIO;
    }

    if (cellName == NULL)
	cellName = dir->cellName;

    code = afsconf_GetCellInfo(dir, cellName, serviceid, &info);
    if (code) {
	fprintf(stderr, "%s: can't find cell %s's hosts in %s\n",
		progname?progname:"<unknown>", cellName, dir->cellservDB);
	afsconf_Close(dir);
	return code;
    }

    code = internal_client_init(dir, &info, secFlags, uclientp, secproc,
			        maxservers, serviceid, deadtime, server,
				port, usrvid);

    afsconf_Close(dir);

    return code;
}

int
ugen_ClientInitServer(const char *confDir, char *cellName, int secFlags,
		      struct ubik_client **uclientp, int maxservers,
		      char *serviceid, int deadtime, afs_uint32 server,
		      afs_uint32 port)
{

    return internal_client_init_dir(confDir, cellName, secFlags, uclientp,
				    NULL, maxservers, serviceid, deadtime,
				    server, port, USER_SERVICE_ID);
}

int
ugen_ClientInitFlags(const char *confDir, char *cellName, int secFlags,
		     struct ubik_client **uclientp,
	             ugen_secproc_func secproc,
		     int maxservers, char *serviceid, int deadtime)
{
    return internal_client_init_dir(confDir, cellName, secFlags, uclientp,
				    secproc, maxservers, serviceid, deadtime,
				    0, 0, USER_SERVICE_ID);
}

/*!
 * \brief Get the appropriate type of ubik client structure out from the system.
 */
afs_int32
ugen_ClientInit(int noAuthFlag, const char *confDir, char *cellName, afs_int32 sauth,
	       struct ubik_client **uclientp,
	       ugen_secproc_func secproc,
	       char *funcName, afs_int32 gen_rxkad_level,
	       afs_int32 maxservers, char *serviceid, afs_int32 deadtime,
	       afs_uint32 server, afs_uint32 port, afs_int32 usrvid)
{
    int secFlags;

    secFlags = AFSCONF_SECOPTS_FALLBACK_NULL;
    if (sauth) {
	secFlags |= AFSCONF_SECOPTS_LOCALAUTH;
	confDir = AFSDIR_SERVER_ETC_DIRPATH;
    }

    secFlags |= AFSCONF_SECOPTS_ALWAYSENCRYPT;

    if (noAuthFlag)
	secFlags |= AFSCONF_SECOPTS_NOAUTH;

    return internal_client_init_dir(confDir, cellName, secFlags, uclientp,
				    secproc, maxservers, serviceid, deadtime,
				    server, port, usrvid);
}

