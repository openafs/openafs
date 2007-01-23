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
#ifdef AFS_RXK5
#include <rx/rxk5.h>
#include <rx/rxk5errors.h>
#include <afs/rxk5_utilafs.h>
#endif
#include "afs_token.h"

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
    struct rx_securityClass *sc;
#ifdef AFS_RXK5
    afs_int32 afs_rxk5_p = 0;
    krb5_creds *k5_creds = 0, in_creds[1];
    krb5_context k5context = 0;
    char* afs_k5_princ = 0;
    krb5_ccache cc = 0;
#endif
    /* This must change if VLDB_MAXSERVERS becomes larger than MAXSERVERS */
    static struct rx_connection *serverconns[MAXSERVERS];

#ifdef AFS_RXK5
    memset(in_creds, 0, sizeof *in_creds);
#define NOAUTH_DEFAULT	env_afs_rxk5_default()
#else
#define NOAUTH_DEFAULT	FORCE_RXKAD
#endif
    switch(noAuthFlag & (FORCE_NOAUTH|FORCE_RXK5|FORCE_RXKAD)) {
    case 0:
	noAuthFlag = NOAUTH_DEFAULT;
	break;
    default:
	break;
    }
    code = rx_Init(0);
    if (code) {
	com_err(funcName, code, "so could not initialize rx.");
	return code;
    }
    rx_SetRxDeadTime(deadtime);

    if (sauth) {		/* -localauth */
	if (!confDir
		|| !strcmp(confDir, "")
		|| !strcmp(confDir, AFSDIR_CLIENT_ETC_DIRPATH))
	    confDir = AFSDIR_SERVER_ETC_DIRPATH;
    }

    tdir = afsconf_Open(confDir);
    if (!tdir) {
	if (confDir && strcmp(confDir, ""))
	    fprintf(stderr,
		"%s: Could not process files in configuration directory (%s).\n",
		funcName, confDir);
	else
	    fprintf(stderr,
		"%s: No configuration directory specified.\n",
		funcName);
	return -1;
    }
    if (!cellName) cellName = tdir->cellName;
    code =
	afsconf_GetCellInfo(tdir, cellName, serviceid,
			    &info);
    if (code) {
	afsconf_Close(tdir);
	com_err(funcName, code, "so can't find cell %s's hosts in %s/%s",
		cellName, confDir, AFSDIR_CELLSERVDB_FILE);
	return -1;
    }

    scIndex = 1;
    sc = 0;

    if (noAuthFlag & FORCE_NOAUTH) {	/* -noauth */
	scIndex = 0;
    } else if (sauth) {		/* -localauth */
	code = afsconf_ClientAuthEx(tdir, &sc, &scIndex,
		noAuthFlag & (FORCE_RXK5|FORCE_RXKAD));	/* sets sc,scIndex */
	if (code) {
	    afsconf_Close(tdir);
	    com_err(funcName, code,
		"so can't get security object for -localauth");
	    return -1;
	}
#ifdef AFS_RXK5
    } else if (noAuthFlag & FORCE_RXK5) { /* -k5 */
	char *what;

	scIndex = 5;
	code = ENOMEM;
	what = "get_afs_krb5_svc_princ";
	afs_k5_princ = get_afs_krb5_svc_princ(&info);
	if (!afs_k5_princ) goto Failed;

	what = "krb5_init_context";
	code = krb5_init_context(&k5context);
	if(code) goto Failed;

	what = "krb5_cc_default";
	code = krb5_cc_default(k5context, &cc); /* in MIT is pointer to ctxt? */
	if(code) goto Failed;

	what = "krb5_cc_get_principal";
	code = krb5_cc_get_principal(k5context, cc, &in_creds->client);
	if(code) goto Failed;

	what = "krb5_parse_name";
	code = krb5_parse_name(k5context, afs_k5_princ,	&in_creds->server);
	if(code) goto Failed;

	/* this stays for now (eg, get from cm) */
	what = "krb5_get_credentials";
	code = krb5_get_credentials(k5context, 0, cc, in_creds, &k5_creds);
	if(code) goto Failed;

	if (!gen_rxkad_level) ++gen_rxkad_level;
	sc = rxk5_NewClientSecurityObject(gen_rxkad_level, k5_creds, 0);
    Failed:
	if (code) {
	    if (afs_k5_princ)
		com_err(funcName, code, "in %s for %s", what, afs_k5_princ);
	    else
		com_err(funcName, code, "in %s", what);
	}
#endif /* rxk5 */
    } else if (noAuthFlag & FORCE_RXKAD) { /* -k4 */
	struct ktc_token ttoken;
	struct afs_token *afstoken = 0;

	code = ktc_GetTokenEx(0, info.name, &afstoken);
	if (code) {		/* did not get ticket */
	    com_err(funcName, code,
		"so could not get afs tokens, running unauthenticated.");
	    scIndex = 0;
#ifdef AFS_RXK5
	} else if (afstoken->cu->cu_type == CU_K5) {	/* got a k5 ticket */
	    scIndex = 5;
	    code = afstoken_to_v5cred(afstoken, in_creds);
	    if (!gen_rxkad_level) ++gen_rxkad_level;
	    if (!code)
		sc=rxk5_NewClientSecurityObject(gen_rxkad_level, in_creds, 0);
#endif
	} else if (afstoken->cu->cu_type == CU_KAD) {	/* got a k5 ticket */
	    scIndex = 2;
	    code = afstoken_to_token(afstoken, &ttoken, sizeof ttoken);
	    if (code) goto SkipSc;
	    if ((ttoken.kvno < 0) || (ttoken.kvno > 256)) {
/* formerly vab */
		fprintf(stderr,
			"%s: funny kvno (%d) in ticket, proceeding\n",
			funcName, ttoken.kvno);
	    }
	    sc = rxkad_NewClientSecurityObject(gen_rxkad_level,
					   &ttoken.sessionKey,
					   ttoken.kvno,
					   ttoken.ticketLen,
					   ttoken.ticket);
	} else {		/* got a whatzits */
	    fprintf(stderr,
		"%s: unknown token type\n",
		funcName,
		afstoken->cu->cu_type);
	}
SkipSc:
	if (afstoken) free_afs_token(afstoken);
    }

    if (!sc) {
	if (scIndex)
	    fprintf(stderr,"%s: running unauthenticated.\n", funcName);
	scIndex = 0;
	sc = rxnull_NewClientSecurityObject();
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
	    code = EDOM;	/* XXX */
	    goto Done;
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
	if (code)
	    com_err(funcName, code, "ubik client init failed.");
    }
Done:
    if (secproc)
	;
    else if (code)
	rxs_Release(sc);
    else
	code = rxs_Release(sc);
#if defined(AFS_RXK5)
    if (afs_k5_princ) free(afs_k5_princ);
    if (k5context) {
	if (cc)
	    krb5_cc_close(k5context, cc);
	if (k5_creds)
	    krb5_free_creds(k5context, k5_creds);
	krb5_free_principal(k5context, in_creds->client);
	krb5_free_principal(k5context, in_creds->server);
	krb5_free_context(k5context);
    }
#endif
    return code;
}


