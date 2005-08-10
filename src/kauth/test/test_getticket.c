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
    ("$Header: /cvs/openafs/src/kauth/test/test_getticket.c,v 1.7.2.1 2005/05/30 04:57:34 shadow Exp $");

#include <afs/stds.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <rx/rx.h>
#include <ubik.h>
#include <pwd.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include <afs/com_err.h>
#include <afs/debug.h>

#include "kauth.h"
#include "kautils.h"


static char *whoami = "test_rxkad_free";
static char realm[MAXKTCREALMLEN];
static char *cell;
static char *admin_user;

static struct ktc_principal afs;
static struct ktc_token oldAFSToken;
static struct ktc_principal oldClient;

static void
Crash(line)
     IN int line;
{
    long code;
    if (oldAFSToken.endTime > 0) {
	code = ktc_SetToken(&afs, &oldAFSToken, &oldClient, 0);
	printf("%s original AFS token\n",
	       (code == 0 ? "Restoring" : "Failed to restore"));
    }
    if (line)
	printf("Crashing at line %d\n", line);
    exit(line);
}

#define CRASH() Crash (__LINE__)
#define EXIT() Crash (0)

static void
PrintRxkadStats()
{
    printf("New Objects client %d, server %d.  Destroyed objects %d.\n",
	   rxkad_stats.clientObjects, rxkad_stats.serverObjects,
	   rxkad_stats.destroyObject);
    printf("client conns: %d %d %d, destroyed client %d.\n",
	   rxkad_stats.connections[0], rxkad_stats.connections[1],
	   rxkad_stats.connections[2], rxkad_stats.destroyClient);
    printf("server challenges %d, responses %d %d %d\n",
	   rxkad_stats.challengesSent, rxkad_stats.responses[0],
	   rxkad_stats.responses[1], rxkad_stats.responses[2]);
    printf("server conns %d %d %d unused %d, unauth %d\n",
	   rxkad_stats.destroyConn[0], rxkad_stats.destroyConn[1],
	   rxkad_stats.destroyConn[2], rxkad_stats.destroyUnused,
	   rxkad_stats.destroyUnauth);
}

static void
SetFields(conn, name, flags, expiration, lifetime)
     IN struct ubik_client *conn;
     IN char *name;
     IN long flags;
     IN Date expiration;
     IN long lifetime;
{
    long code;
    char *instance;
    char buf[32];
    char *which;
    char *what;

    if (strcmp(name, KA_TGS_NAME) == 0)
	instance = realm;
    else
	instance = "";
    if (flags) {
	which = "flags";
	if (flags & KAFNOTGS)
	    if (flags & KAFNOSEAL)
		what = "NOTGS+NOSEAL";
	    else
		what = "NOTGS";
	else if (flags & KAFNOSEAL)
	    what = "NOSEAL";
	else
	    what = "normal";
	flags |= KAFNORMAL;
	if (strcmp(name, admin_user) == 0)
	    flags |= KAFADMIN;
    } else if (expiration) {
	which = "expiration";
	sprintf(buf, "now + %.1f hours",
		(double)(expiration - time(0)) / 3600.0);
	what = buf;
    } else if (lifetime) {
	which = "lifetime";
	sprintf(buf, "%.1f hours", (double)lifetime / 3600.0);
	what = buf;
    } else
	CRASH();

    printf("Setting %s of ", which);
    ka_PrintUserID("", name, instance, "");
    printf(" to %s\n", what);
    code =
	ubik_Call(KAM_SetFields, conn, 0, name, instance, flags, expiration,
		  lifetime, -1,
		  /* spares */ 0, 0);
    if (code) {
	com_err(whoami, code, "calling set fields on %s", name);
	CRASH();
    }
}

#define SetLife(c,n,l) SetFields (c,n,0,0,l);
#define SetExp(c,n,e) SetFields (c,n,0,e,0);
#define SetFlags(c,n,f) SetFields (c,n,f,0,0);

#define EXACT 0x5b
#define KERBEROS 0x2a

static void
CheckLife(tokenEnd, start, expectedLife, match)
     IN Date tokenEnd;
     IN Date start;
{
    Date expectedEnd;
    char bob[KA_TIMESTR_LEN];
    printf("Expecting %s match with lifetime of %.1f hours\n",
	   (match == EXACT ? "exact" : "kerberos"),
	   (double)expectedLife / 3600.0);
    expectedEnd = expectedLife + start;
    if (match == EXACT) {
	if (abs(expectedEnd - tokenEnd) <= 2)
	    return;
    }
    if (match == KERBEROS) {
	unsigned char kerberosV4Life;
	Date kend;
	kerberosV4Life = time_to_life(start, expectedEnd);
	kend = life_to_time(start, kerberosV4Life);
	if (abs(kend - tokenEnd) <= 1)
	    return;
	kerberosV4Life = time_to_life(start, expectedEnd - 2);
	kend = life_to_time(start, kerberosV4Life);
	if (abs(kend - tokenEnd) <= 1)
	    return;
    }
    ka_timestr(tokenEnd, bob, KA_TIMESTR_LEN);
    printf("End doesn't match: token was %s", bob);
    ka_timestr(expectedEnd, bob, KA_TIMESTR_LEN);
    printf(", but expected %s\n", bob);
    CRASH();
}

static void
GetTokenLife(name, passwd, expectedLife, match)
     IN char *name;
     IN char *passwd;
     IN long expectedLife;
     IN long match;		/* or expected error code */
{
    char *reason;
    long code;
    struct ktc_token t;

    code = ka_UserAuthenticateLife(0, name, "", cell, passwd, 0, &reason);
    if (!((match == EXACT) || (match == KERBEROS))) {	/* must be error code */
	if (code == match) {
	    printf("Received expected error code\n");
	    return;
	}
    }
    if (code) {
	fprintf(stderr, "Unable to authenticate to AFS because %s.\n",
		reason);
	CRASH();
    }
    code = ktc_GetToken(&afs, &t, sizeof(t), 0);
    if (code) {
	com_err(whoami, code, "getting afs token from ktc");
	CRASH();
    }
    CheckLife(t.endTime, t.startTime, expectedLife, match);
}

static long
Main(as, arock)
     IN struct cmd_syndesc *as;
     IN char *arock;
{
    long code;
    char name[MAXKTCNAMELEN];
    char instance[MAXKTCNAMELEN];
    char newCell[MAXKTCREALMLEN];

    long serverList[MAXSERVERS];
    extern struct passwd *getpwuid();

    struct passwd *pw;
    struct ktc_encryptionKey key;

    char passwd[BUFSIZ];

    int cellSpecified;
    int i;

    unsigned long startTime;
    unsigned long deadTime;
    unsigned long now;
    unsigned long end;

    int patient = (as->parms[0].items != 0);

    whoami = as->a0name;
    newCell[0] = 0;

    if (as->parms[12].items) {	/* if username specified */
	code =
	    ka_ParseLoginName(as->parms[12].items->data, name, instance,
			      newCell);
	if (code) {
	    com_err(whoami, code, "parsing user's name '%s'",
		    as->parms[12].items->data);
	    return code;
	}
	if (strlen(newCell) > 0)
	    cellSpecified = 1;
    } else {
	/* No explicit name provided: use Unix uid. */
	pw = getpwuid(getuid());
	if (pw == 0) {
	    printf("Can't figure out your name from your user id.\n");
	    return KABADCMD;
	}
	strncpy(name, pw->pw_name, sizeof(name));
	strcpy(instance, "");
	strcpy(newCell, "");
    }
    admin_user = name;		/* this guy should keep admin bit */

    if (strcmp(as->parms[14].name, "-cell") == 0) {
	if (as->parms[14].items) {	/* if cell specified */
	    if (cellSpecified)
		printf("Duplicate cell specification not allowed\n");
	    else
		strncpy(newCell, as->parms[14].items->data, sizeof(newCell));
	}
    }

    code = ka_ExpandCell(newCell, newCell, 0 /*local */ );
    if (code) {
	com_err(whoami, code, "Can't expand cell name");
	return code;
    }
    cell = newCell;
    code = ka_CellToRealm(cell, realm, 0);
    if (code) {
	com_err(whoami, code, "Can't get realm from cell name");
	return code;
    }

    if (as->parms[13].items) {	/* if password specified */
	strncpy(passwd, as->parms[13].items->data, sizeof(passwd));
	memset(as->parms[13].items->data, 0,
	       strlen(as->parms[13].items->data));
    } else {
	char msg[sizeof(name) + 15];
	if (as->parms[12].items)
	    strcpy(msg, "Admin Password: ");
	else
	    sprintf(msg, "Password for %s: ", name);
	code = read_pw_string(passwd, sizeof(passwd), msg, 0);
	if (code)
	    code = KAREADPW;
	else if (strlen(passwd) == 0)
	    code = KANULLPASSWORD;
	if (code) {
	    com_err(whoami, code, "reading password");
	    return code;
	}
    }
    if (as->parms[15].items) {
	struct cmd_item *ip;
	char *ap[MAXSERVERS + 2];

	for (ip = as->parms[15].items, i = 2; ip; ip = ip->next, i++)
	    ap[i] = ip->data;
	ap[0] = "";
	ap[1] = "-servers";
	code = ubik_ParseClientList(i, ap, serverList);
	if (code) {
	    com_err(whoami, code, "could not parse server list");
	    return code;
	}
	ka_ExplicitCell(cell, serverList);
    }

    ka_StringToKey(passwd, cell, &key);

    strcpy(afs.name, AUTH_SUPERUSER);
    strcpy(afs.instance, "");
    strcpy(afs.cell, cell);
    code = ktc_GetToken(&afs, &oldAFSToken, sizeof(oldAFSToken), &oldClient);
    if (code) {
	com_err(whoami, code, "saving existing afs token");
	return code;
    }

    startTime = time(0);
    {
	struct ktc_token token;
	struct ktc_token *pToken;
	struct ubik_client *ubikConn;
	struct kaentryinfo tentry;
	struct ktc_principal tgs_server;
	struct ktc_token tgs_token;
	struct ktc_principal tgs_client;
	Date start;

	code =
	    ka_GetAdminToken(name, instance, cell, &key, 3600, &token,
			     1 /*new */ );
	if (code) {
	    com_err(whoami, code, "getting admin token");
	    return code;
	}
	pToken = &token;
	if (token.ticketLen == 0) {
	    fprintf("Can't get admin token\n");
	    return -1;
	}

	code =
	    ka_AuthServerConn(cell, KA_MAINTENANCE_SERVICE, pToken,
			      &ubikConn);
	if (code) {
	    com_err(whoami, code, "Getting AuthServer ubik conn");
	    return code;
	}

	SetFlags(ubikConn, AUTH_SUPERUSER, KAFNORMAL);
	SetFlags(ubikConn, name, KAFNORMAL);
	SetLife(ubikConn, KA_TGS_NAME, MAXKTCTICKETLIFETIME);
	SetLife(ubikConn, AUTH_SUPERUSER, MAXKTCTICKETLIFETIME);
	SetLife(ubikConn, name, 3600);
	deadTime = startTime + 365 * 24 * 3600;
	SetExp(ubikConn, KA_TGS_NAME, deadTime);
	SetExp(ubikConn, AUTH_SUPERUSER, deadTime);
	SetExp(ubikConn, name, deadTime);

	GetTokenLife(name, passwd, 3600, EXACT);

	/* get TGS ticket for proper realm */
	strcpy(tgs_server.name, KA_TGS_NAME);
	strcpy(tgs_server.instance, realm);
	strcpy(tgs_server.cell, cell);
	/* save this for future use */
	code =
	    ktc_GetToken(&tgs_server, &tgs_token, sizeof(tgs_token),
			 &tgs_client);
	if (code) {
	    com_err(whoami, code, "saving tgs token");
	    return code;
	}

	SetLife(ubikConn, name, MAXKTCTICKETLIFETIME);
	now = time(0);
	GetTokenLife(name, passwd, MAXKTCTICKETLIFETIME, EXACT);

	SetLife(ubikConn, AUTH_SUPERUSER, 4000);
	now = time(0);
	GetTokenLife(name, passwd, 4000, KERBEROS);
	SetLife(ubikConn, AUTH_SUPERUSER, MAXKTCTICKETLIFETIME);

	SetLife(ubikConn, KA_TGS_NAME, 5000);
	now = time(0);
	GetTokenLife(name, passwd, 5000, KERBEROS);
	SetLife(ubikConn, KA_TGS_NAME, MAXKTCTICKETLIFETIME);

	now = time(0);
	SetExp(ubikConn, KA_TGS_NAME, now + 6000);
	GetTokenLife(name, passwd, 6000, KERBEROS);
	SetExp(ubikConn, KA_TGS_NAME, deadTime);

	now = time(0);
	SetExp(ubikConn, AUTH_SUPERUSER, now + 7000);
	GetTokenLife(name, passwd, 7000, KERBEROS);
	SetExp(ubikConn, AUTH_SUPERUSER, deadTime);

	now = time(0);
	SetExp(ubikConn, name, now + 8000);
	GetTokenLife(name, passwd, 8000, KERBEROS);

	/* since the rest should be errors, restore good AFS ticket */
	code = ktc_SetToken(&afs, &oldAFSToken, &oldClient, 0);
	if (code) {
	    com_err(whoami, code, "restoring old afs token");
	    return code;
	}

	SetExp(ubikConn, name, now - 1000);
	GetTokenLife(name, passwd, 8000, KABADUSER);
	SetExp(ubikConn, name, deadTime);

	SetExp(ubikConn, AUTH_SUPERUSER, now - 1000);
	GetTokenLife(name, passwd, 8000, KABADSERVER);
	SetExp(ubikConn, AUTH_SUPERUSER, deadTime);

	SetFlags(ubikConn, AUTH_SUPERUSER, KAFNORMAL + KAFNOSEAL);
	GetTokenLife(name, passwd, 8000, KABADSERVER);
	SetFlags(ubikConn, AUTH_SUPERUSER, KAFNORMAL);

	SetFlags(ubikConn, name, KAFNORMAL + KAFNOTGS);
	GetTokenLife(name, passwd, 8000, KABADUSER);
	/* restore old tgs, since GetTicket are prohibited too. */
	code = ktc_SetToken(&tgs_server, &tgs_token, &tgs_client, 0);
	if (code) {
	    com_err(whoami, code, "restoring old tgs token");
	    return code;
	}
	printf("Restoring TGT obtained before NOTGS set\n");
	code = ka_GetServerToken(AUTH_SUPERUSER, "", cell, 100, 0, 1);
	if (code != KABADUSER) {
	    com_err(whoami, code,
		    "expected BADUSER error, getting AFS token w/ old tgs token but with NOTGS set");
	    CRASH();
	} else
	    printf("Received expected error code\n");
	SetFlags(ubikConn, name, KAFNORMAL);

	if (patient) {		/* this requires waiting too long */
	    struct ktc_token afsToken;
	    code = ktc_SetToken(&afs, &oldAFSToken, &oldClient, 0);
	    if (code) {
		com_err(whoami, code, "restoring old afs token");
		return code;
	    }
	    fprintf(stdout, "Waiting for TGS ticket to age (about 5 min)...");
	    fflush(stdout);
	    while (((now = time(0)) - tgs_token.startTime) < 5 * 60) {
		if (((now - tgs_token.startTime) % 60) == 0) {
		    fprintf(stdout, "%d seconds to go...",
			    (now - tgs_token.startTime));
		    fflush(stdout);
		}
		IOMGR_Sleep(1);	/* with afs token restored... */
	    }
	    /* restore old tgs */
	    code = ktc_SetToken(&tgs_server, &tgs_token, &tgs_client, 0);
	    if (code) {
		com_err(whoami, code, "restoring old tgs token");
		return code;
	    }
	    code =
		ka_GetServerToken(AUTH_SUPERUSER, "", cell,
				  MAXKTCTICKETLIFETIME, &afsToken, 1);
	    if (code) {
		com_err(whoami, code, "getting AFS token w/ old tgs token");
		CRASH();
	    }
	    CheckLife(afsToken.endTime, afsToken.startTime, 3600 - (5 * 60),
		      EXACT);
	}
	ubik_ClientDestroy(ubikConn);
    }

    printf("calling finalize\n");
    rx_Finalize();
    PrintRxkadStats();
    printf("All Okay\n");
    return 0;
}

int
main(argc, argv)
     IN int argc;
     IN char *argv[];
{
    register struct cmd_syndesc *ts;
    long code;

    initialize_U_error_table();
    initialize_CMD_error_table();
    initialize_RXK_error_table();
    initialize_KTC_error_table();
    initialize_ACFG_error_table();
    initialize_KA_error_table();

    ts = cmd_CreateSyntax(0, Main, 0, "Main program");
    /* 0 */ cmd_AddParm(ts, "-patient", CMD_FLAG, CMD_OPTIONAL,
			"wait for TGS ticket to age");
    cmd_Seek(ts, 12);
    /* 12 */ cmd_AddParm(ts, "-admin_username", CMD_SINGLE, CMD_OPTIONAL,
			 "admin principal to use for authentication");
    /* 13 */ cmd_AddParm(ts, "-password_for_admin", CMD_SINGLE, CMD_OPTIONAL,
			 "admin password");
    /* 14 */ cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    /* 15 */ cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL,
			 "explicit list of authentication servers");
    code = cmd_Dispatch(argc, argv);
    if (code)
	CRASH();
    EXIT();
}
