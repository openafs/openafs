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

RCSID
    ("$Header$");

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

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <afs/dirpath.h>
#include <errno.h>
#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/afsint.h>
#include <afs/cmd.h>
#include <rx/rxkad.h>

/*
  Get the appropriate type of ubik client structure out from the system.
*/
afs_int32
ugen_ClientInit(int noAuthFlag, char *confDir, char *cellName, afs_int32 sauth,
	       struct ubik_client **uclientp, int (*secproc) (),
	       char *funcName, afs_int32 gen_rxkad_level, 
	       afs_int32 maxservers, char *serviceid, afs_int32 deadtime,
	       afs_uint32 server, afs_uint32 port, afs_int32 usrvid)
{
    afs_int32 code, scIndex, i;
    struct afsconf_cell info;
    struct afsconf_dir *tdir;
    struct ktc_principal sname;
    struct ktc_token ttoken;
    struct rx_securityClass *sc;
    /* This must change if VLDB_MAXSERVERS becomes larger than MAXSERVERS */
    static struct rx_connection *serverconns[MAXSERVERS];
    char cellstr[64];

    code = rx_Init(0);
    if (code) {
	fprintf(stderr, "%s: could not initialize rx.\n", funcName);
	return code;
    }
    rx_SetRxDeadTime(deadtime);

    if (sauth) {		/* -localauth */
	tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
	if (!tdir) {
	    fprintf(stderr,
		    "%s: Could not process files in configuration directory (%s).\n",
		    funcName, AFSDIR_SERVER_ETC_DIRPATH);
	    return -1;
	}
	code = afsconf_ClientAuth(tdir, &sc, &scIndex);	/* sets sc,scIndex */
	if (code) {
	    afsconf_Close(tdir);
	    fprintf(stderr,
		    "%s: Could not get security object for -localAuth\n",
		    funcName);
	    return -1;
	}
	code =
	    afsconf_GetCellInfo(tdir, tdir->cellName, serviceid,
				&info);
	if (code) {
	    afsconf_Close(tdir);
	    fprintf(stderr,
		    "%s: can't find cell %s's hosts in %s/%s\n",
		    funcName, cellName, AFSDIR_SERVER_ETC_DIRPATH,
		    AFSDIR_CELLSERVDB_FILE);
	    exit(1);
	}
    } else {			/* not -localauth */
	tdir = afsconf_Open(confDir);
	if (!tdir) {
	    fprintf(stderr,
		    "%s: Could not process files in configuration directory (%s).\n",
		    funcName, confDir);
	    return -1;
	}

	if (!cellName) {
	    code = afsconf_GetLocalCell(tdir, cellstr, sizeof(cellstr));
	    if (code) {
		fprintf(stderr,
			"%s: can't get local cellname, check %s/%s\n",
			funcName, confDir, AFSDIR_THISCELL_FILE);
		exit(1);
	    }
	    cellName = cellstr;
	}

	code =
	    afsconf_GetCellInfo(tdir, cellName, serviceid, &info);
	if (code) {
	    fprintf(stderr,
		    "%s: can't find cell %s's hosts in %s/%s\n",
		    funcName, cellName, confDir, AFSDIR_CELLSERVDB_FILE);
	    exit(1);
	}
	if (noAuthFlag)		/* -noauth */
	    scIndex = 0;
	else {			/* not -noauth */
	    strcpy(sname.cell, info.name);
	    sname.instance[0] = 0;
	    strcpy(sname.name, "afs");
	    code = ktc_GetToken(&sname, &ttoken, sizeof(ttoken), NULL);
	    if (code) {		/* did not get ticket */
		fprintf(stderr,
			"%s: Could not get afs tokens, running unauthenticated.\n",
			funcName);
		scIndex = 0;
	    } else {		/* got a ticket */
		scIndex = 2;
		if ((ttoken.kvno < 0) || (ttoken.kvno > 256)) {
		    fprintf(stderr,
			    "%s: funny kvno (%d) in ticket, proceeding\n",
			    funcName, ttoken.kvno);
		}
	    }
	}

	switch (scIndex) {
	case 0:
	    sc = rxnull_NewClientSecurityObject();
	    break;
	case 2:
	    sc = rxkad_NewClientSecurityObject(gen_rxkad_level,
					       &ttoken.sessionKey,
					       ttoken.kvno, ttoken.ticketLen,
					       ttoken.ticket);
	    break;
	default:
	    fprintf(stderr, "%s: unsupported security index %d\n",
		    funcName, scIndex);
	    exit(1);
	    break;
	}
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
	    exit(1);
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


