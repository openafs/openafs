/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* These routines provide an interface to the token cache maintained by the
   kernel.  Principally it handles cache misses by requesting the desired token
   from the AuthServer. */

#include <afsconfig.h>
#if defined(UKERNEL)
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header$");

#if defined(UKERNEL)
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/stds.h"
#include "rx/xdr.h"
#include "afs/pthread_glock.h"
#include "afs/lock.h"
#include "ubik.h"
#include "afs/kauth.h"
#include "afs/kautils.h"
#include "afs/auth.h"
#include "afs/pthread_glock.h"
#else /* defined(UKERNEL) */
#include <afs/stds.h>
#include <sys/types.h>
#include <rx/xdr.h>
#include <afs/pthread_glock.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <string.h>
/* netinet/in.h and cellconfig.h  are needed together */
#include <afs/cellconfig.h>
    /* these are needed together */
#include <lock.h>
#include <ubik.h>

#include "kauth.h"
#include "kautils.h"
#include <afs/auth.h>
#endif /* defined(UKERNEL) */


afs_int32
ka_GetAuthToken(char *name, char *instance, char *cell,
		struct ktc_encryptionKey * key, afs_int32 lifetime,
		afs_int32 * pwexpires)
{
    afs_int32 code;
    struct ubik_client *conn;
    afs_int32 now = time(0);
    struct ktc_token token;
    char cellname[MAXKTCREALMLEN];
    char realm[MAXKTCREALMLEN];
    struct ktc_principal client, server;

    LOCK_GLOBAL_MUTEX;
    code = ka_ExpandCell(cell, cellname, 0 /*local */ );
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    cell = cellname;

    /* get an unauthenticated connection to desired cell */
    code = ka_AuthServerConn(cell, KA_AUTHENTICATION_SERVICE, 0, &conn);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    code =
	ka_Authenticate(name, instance, cell, conn,
			KA_TICKET_GRANTING_SERVICE, key, now, now + lifetime,
			&token, pwexpires);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    code = ubik_ClientDestroy(conn);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    code = ka_CellToRealm(cell, realm, 0 /*local */ );
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    strcpy(client.name, name);
    strcpy(client.instance, instance);
    strncpy(client.cell, cell, sizeof(client.cell));
    strcpy(server.name, KA_TGS_NAME);
    strcpy(server.instance, realm);
    strcpy(server.cell, cell);
    code = ktc_SetToken(&server, &token, &client, 0);
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

afs_int32
ka_GetServerToken(char *name, char *instance, char *cell, Date lifetime,
		  struct ktc_token * token, int new, int dosetpag)
{
    afs_int32 code;
    struct ubik_client *conn;
    afs_int32 now = time(0);
    struct ktc_token auth_token;
    struct ktc_token cell_token;
    struct ktc_principal server, auth_server, client;
    char *localCell = ka_LocalCell();
    char cellname[MAXKTCREALMLEN];
    char realm[MAXKTCREALMLEN];
    char authDomain[MAXKTCREALMLEN];
    int local;

    LOCK_GLOBAL_MUTEX;
    code = ka_ExpandCell(cell, cellname, 0 /*local */ );
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    cell = cellname;

    strcpy(server.name, name);
    strcpy(server.instance, instance);
    lcstring(server.cell, cell, sizeof(server.cell));
    if (!new) {
	code =
	    ktc_GetToken(&server, token, sizeof(struct ktc_token), &client);
	if (!code) {
	    UNLOCK_GLOBAL_MUTEX;
	    return 0;
	}
    }

    code = ka_CellToRealm(cell, realm, &local);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    /* get TGS ticket for proper realm */
    strcpy(auth_server.name, KA_TGS_NAME);
    strcpy(auth_server.instance, realm);
    lcstring(auth_server.cell, realm, sizeof(auth_server.cell));
    strcpy(authDomain, realm);
    code =
	ktc_GetToken(&auth_server, &auth_token, sizeof(auth_token), &client);
    if (code && !local) {	/* try for remotely authenticated ticket */
	strcpy(auth_server.cell, localCell);
	strcpy(authDomain, "");
	code =
	    ktc_GetToken(&auth_server, &auth_token, sizeof(auth_token),
			 &client);
    }

    if (code && local) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    } else if (code) {
	/* here we invoke the inter-cell mechanism */

	/* get local auth ticket */
	ucstring(auth_server.instance, localCell,
		 sizeof(auth_server.instance));
	strcpy(auth_server.cell, localCell);
	code =
	    ktc_GetToken(&auth_server, &cell_token, sizeof(cell_token),
			 &client);
	if (code) {
	    UNLOCK_GLOBAL_MUTEX;
	    return code;
	}
	/* get a connection to the local cell */
	if ((code =
	     ka_AuthServerConn(localCell, KA_TICKET_GRANTING_SERVICE, 0,
			       &conn))) {
	    UNLOCK_GLOBAL_MUTEX;
	    return code;
	}
	/* get foreign auth ticket */
	if ((code =
	     ka_GetToken(KA_TGS_NAME, realm, localCell, client.name,
			 client.instance, conn, now, now + lifetime,
			 &cell_token, "" /* local auth domain */ ,
			 &auth_token))) {
	    UNLOCK_GLOBAL_MUTEX;
	    return code;
	}
	code = ubik_ClientDestroy(conn);
	if (code) {
	    UNLOCK_GLOBAL_MUTEX;
	    return code;
	}
	conn = 0;

	/* save foreign auth ticket */
	strcpy(auth_server.instance, realm);
	lcstring(auth_server.cell, localCell, sizeof(auth_server.cell));
	ucstring(authDomain, localCell, sizeof(authDomain));
	if ((code = ktc_SetToken(&auth_server, &auth_token, &client, 0))) {
	    UNLOCK_GLOBAL_MUTEX;
	    return code;
	}
    }

    if ((code =
	 ka_AuthServerConn(cell, KA_TICKET_GRANTING_SERVICE, 0, &conn))) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    if ((code =
	 ka_GetToken(name, instance, cell, client.name, client.instance, conn,
		     now, now + lifetime, &auth_token, authDomain, token))) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    code = ubik_ClientDestroy(conn);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    if ((code =
	 ktc_SetToken(&server, token, &client,
		      dosetpag ? AFS_SETTOK_SETPAG : 0))) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

afs_int32
ka_GetAdminToken(char *name, char *instance, char *cell,
		 struct ktc_encryptionKey * key, afs_int32 lifetime,
		 struct ktc_token * token, int new)
{
    int code;
    struct ubik_client *conn;
    afs_int32 now = time(0);
    struct ktc_principal server, client;
    struct ktc_token localToken;
    char cellname[MAXKTCREALMLEN];

    LOCK_GLOBAL_MUTEX;
    code = ka_ExpandCell(cell, cellname, 0 /*local */ );
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    cell = cellname;

    if (token == 0)
	token = &localToken;	/* in case caller doesn't want token */

    strcpy(server.name, KA_ADMIN_NAME);
    strcpy(server.instance, KA_ADMIN_INST);
    strncpy(server.cell, cell, sizeof(server.cell));
    if (!new) {
	code =
	    ktc_GetToken(&server, token, sizeof(struct ktc_token), &client);
	if (code == 0) {
	    UNLOCK_GLOBAL_MUTEX;
	    return 0;
	}
    }

    if ((name == 0) || (key == 0)) {
	/* just lookup in cache don't get new one */
	UNLOCK_GLOBAL_MUTEX;
	return KANOTICKET;
    }

    /* get an unauthenticated connection to desired cell */
    code = ka_AuthServerConn(cell, KA_AUTHENTICATION_SERVICE, 0, &conn);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    code =
	ka_Authenticate(name, instance, cell, conn, KA_MAINTENANCE_SERVICE,
			key, now, now + lifetime, token, 0);
    (void)ubik_ClientDestroy(conn);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    strcpy(client.name, name);
    strcpy(client.instance, instance);
    strncpy(client.cell, cell, sizeof(client.cell));
    code = ktc_SetToken(&server, token, &client, 0);
    UNLOCK_GLOBAL_MUTEX;
    return code;
}


afs_int32
ka_VerifyUserToken(char *name, char *instance, char *cell,
		   struct ktc_encryptionKey * key)
{
    afs_int32 code;
    struct ubik_client *conn;
    afs_int32 now = time(0);
    struct ktc_token token;
    char cellname[MAXKTCREALMLEN];
    afs_int32 pwexpires;

    LOCK_GLOBAL_MUTEX;
    code = ka_ExpandCell(cell, cellname, 0 /*local */ );
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    cell = cellname;

    /* get an unauthenticated connection to desired cell */
    code = ka_AuthServerConn(cell, KA_AUTHENTICATION_SERVICE, 0, &conn);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    code =
	ka_Authenticate(name, instance, cell, conn,
			KA_TICKET_GRANTING_SERVICE, key, now,
			now + MAXKTCTICKETLIFETIME, &token, &pwexpires);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
    code = ubik_ClientDestroy(conn);
    UNLOCK_GLOBAL_MUTEX;
    return code;
}
