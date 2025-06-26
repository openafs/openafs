/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* These routines provide a convenient interface to the AuthServer. */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <roken.h>
#include <afs/opr.h>

#ifdef IGNORE_SOME_GCC_WARNINGS
# pragma GCC diagnostic warning "-Wimplicit-function-declaration"
#endif

#include <hcrypto/des.h>

#define UBIK_LEGACY_CALLITER 1

#include <afs/pthread_glock.h>
#include <rx/rxkad.h>
#include <rx/rxkad_convert.h>
#include <afs/cellconfig.h>
#include <ubik.h>
#include <afs/auth.h>
#include <afs/afsutil.h>

#include "kauth.h"
#include "kautils.h"

static struct afsconf_dir *conf = 0;
static struct afsconf_cell explicit_cell_server_list;
static struct afsconf_cell debug_cell_server_list;
static int explicit = 0;
static int debug = 0;

#ifdef ENCRYPTIONBLOCKSIZE
#undef ENCRYPTIONBLOCKSIZE
#endif
#define ENCRYPTIONBLOCKSIZE (sizeof(DES_cblock))

/* Copy the specified list of servers into a specially know cell named
   "explicit".  The cell can then be used to debug experimental servers. */

void
ka_ExplicitCell(char *cell, afs_uint32 serverList[])
{
    int i;

    LOCK_GLOBAL_MUTEX;
    ka_ExpandCell(cell, explicit_cell_server_list.name, 0);
    for (i = 0; i < MAXHOSTSPERCELL; i++)
	if (serverList[i]) {
	    explicit_cell_server_list.numServers = i + 1;
	    explicit_cell_server_list.hostAddr[i].sin_family = AF_INET;
	    explicit_cell_server_list.hostAddr[i].sin_addr.s_addr =
		serverList[i];
	    explicit_cell_server_list.hostName[i][0] = 0;
	    explicit_cell_server_list.hostAddr[i].sin_port =
		htons(AFSCONF_KAUTHPORT);
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	    explicit_cell_server_list.hostAddr[i].sin_len =
		sizeof(struct sockaddr_in);
#endif
	    explicit = 1;
	} else
	    break;
    UNLOCK_GLOBAL_MUTEX;
}

static int
myCellLookup(struct afsconf_dir *conf, char *cell, char *service,
	     struct afsconf_cell *cellinfo)
{
    if (debug) {
	*cellinfo = debug_cell_server_list;
	return 0;
    } else if (explicit
	       && (strcmp(cell, explicit_cell_server_list.name) == 0)) {
	*cellinfo = explicit_cell_server_list;
	return 0;
    }
    /* call the real one */
    else
	return afsconf_GetCellInfo(conf, cell, service, cellinfo);
}

afs_int32
ka_GetServers(char *cell, struct afsconf_cell * cellinfo)
{
    afs_int32 code;
    char cellname[MAXKTCREALMLEN];

    LOCK_GLOBAL_MUTEX;
    if (cell == NULL || strlen(cell) == 0)
	cell = NULL;
    else
	cell = lcstring(cellname, cell, sizeof(cellname));

    if (!conf) {
	conf = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
	if (!conf) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KANOCELLS;
	}
    }
    code = myCellLookup(conf, cell, AFSCONF_KAUTHSERVICE, cellinfo);
    UNLOCK_GLOBAL_MUTEX;
    return code;
}

afs_int32
ka_GetSecurity(int service, struct ktc_token * token,
	       struct rx_securityClass ** scP, int *siP)
{				/* security class index */
    LOCK_GLOBAL_MUTEX;
    *scP = 0;
    switch (service) {
    case KA_AUTHENTICATION_SERVICE:
    case KA_TICKET_GRANTING_SERVICE:
      no_security:
	*scP = rxnull_NewClientSecurityObject();
	*siP = RX_SECIDX_NULL;
	break;
    case KA_MAINTENANCE_SERVICE:
	if (!token)
	    goto no_security;
	*scP =
	    rxkad_NewClientSecurityObject(rxkad_crypt, &token->sessionKey,
					  token->kvno, token->ticketLen,
					  token->ticket);
	*siP = RX_SECIDX_KAD;
	break;
    default:
	UNLOCK_GLOBAL_MUTEX;
	return KABADARGUMENT;
    }
    if (*scP == 0) {
	printf("Failed gettting security object\n");
	UNLOCK_GLOBAL_MUTEX;
	return KARXFAIL;
    }
    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

afs_int32
ka_SingleServerConn(char *cell, char *server,	/* name of server to contact */
		    int service, struct ktc_token * token,
		    struct ubik_client ** conn)
{
    afs_int32 code;
    struct rx_connection *serverconns[2];
    struct rx_securityClass *sc;
    int si;			/* security class index */
    struct afsconf_cell cellinfo;	/* for cell auth server list */
    int i;
    int match;
    char sname[MAXHOSTCHARS];
    int snamel;

    LOCK_GLOBAL_MUTEX;
    code = ka_GetServers(cell, &cellinfo);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    lcstring(sname, server, sizeof(sname));
    snamel = strlen(sname);
    match = -1;
    for (i = 0; i < cellinfo.numServers; i++) {
	if (strncmp(cellinfo.hostName[i], sname, snamel) == 0) {
	    if (match >= 0) {
		UNLOCK_GLOBAL_MUTEX;
		return KANOCELLS;
	    } else
		match = i;
	}
    }
    if (match < 0) {
	UNLOCK_GLOBAL_MUTEX;
	return KANOCELLS;
    }

    code = rx_Init(0);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    code = ka_GetSecurity(service, token, &sc, &si);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }
#ifdef AFS_PTHREAD_ENV
    serverconns[0] =
	rx_GetCachedConnection(cellinfo.hostAddr[match].sin_addr.s_addr,
			       cellinfo.hostAddr[match].sin_port, service, sc,
			       si);
#else
    serverconns[0] =
	rx_NewConnection(cellinfo.hostAddr[match].sin_addr.s_addr,
			 cellinfo.hostAddr[match].sin_port, service, sc, si);
#endif
    serverconns[1] = 0;		/* terminate list */

    /* next, pass list of server rx_connections (in serverconns), and a place
     * to put the returned client structure that we'll use in all of our rpc
     * calls (via ubik_Call) */
    *conn = 0;
    code = ubik_ClientInit(serverconns, conn);
    rxs_Release(sc);
    UNLOCK_GLOBAL_MUTEX;
    if (code)
	return KAUBIKINIT;
    return 0;
}

afs_int32
ka_AuthSpecificServersConn(int service, struct ktc_token * token,
			   struct afsconf_cell * cellinfo,
			   struct ubik_client ** conn)
{
    afs_int32 code;
    struct rx_connection *serverconns[MAXSERVERS];
    struct rx_securityClass *sc;
    int si;			/* security class index */
    int i;

    LOCK_GLOBAL_MUTEX;
    code = rx_Init(0);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    code = ka_GetSecurity(service, token, &sc, &si);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    for (i = 0; i < cellinfo->numServers; i++)
#ifdef AFS_PTHREAD_ENV
	serverconns[i] =
	    rx_GetCachedConnection(cellinfo->hostAddr[i].sin_addr.s_addr,
				   cellinfo->hostAddr[i].sin_port, service,
				   sc, si);
#else
	serverconns[i] =
	    rx_NewConnection(cellinfo->hostAddr[i].sin_addr.s_addr,
			     cellinfo->hostAddr[i].sin_port, service, sc, si);
#endif
    serverconns[cellinfo->numServers] = 0;	/* terminate list */

    /* next, pass list of server rx_connections (in serverconns), and a place
     * to put the returned client structure that we'll use in all of our rpc
     * calls (via ubik_Call) */
    *conn = 0;
    code = ubik_ClientInit(serverconns, conn);
    rxs_Release(sc);
    UNLOCK_GLOBAL_MUTEX;
    if (code)
	return KAUBIKINIT;
    return 0;
}

afs_int32
ka_AuthServerConn(char *cell, int service, struct ktc_token * token,
		  struct ubik_client ** conn)
{
    afs_int32 code;
    struct rx_connection *serverconns[MAXSERVERS];
    struct rx_securityClass *sc;
    int si;			/* security class index */
    int i;
    struct afsconf_cell cellinfo;	/* for cell auth server list */

    LOCK_GLOBAL_MUTEX;
    code = ka_GetServers(cell, &cellinfo);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    code = rx_Init(0);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    code = ka_GetSecurity(service, token, &sc, &si);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return code;
    }

    for (i = 0; i < cellinfo.numServers; i++)
#ifdef AFS_PTHREAD_ENV
	serverconns[i] =
	    rx_GetCachedConnection(cellinfo.hostAddr[i].sin_addr.s_addr,
				   cellinfo.hostAddr[i].sin_port, service, sc,
				   si);
#else
	serverconns[i] =
	    rx_NewConnection(cellinfo.hostAddr[i].sin_addr.s_addr,
			     cellinfo.hostAddr[i].sin_port, service, sc, si);
#endif
    serverconns[cellinfo.numServers] = 0;	/* terminate list */

    /* next, pass list of server rx_connections (in serverconns), and a place
     * to put the returned client structure that we'll use in all of our rpc
     * calls (via ubik_Call) */
    *conn = 0;
    code = ubik_ClientInit(serverconns, conn);
    rxs_Release(sc);
    UNLOCK_GLOBAL_MUTEX;
    if (code)
	return KAUBIKINIT;
    return 0;
}

static afs_int32
CheckTicketAnswer(ka_BBS * oanswer, afs_int32 challenge,
		  struct ktc_token *token, struct ktc_principal *caller,
		  struct ktc_principal *server, char *label,
		  afs_int32 * pwexpires)
{
    struct ka_ticketAnswer *answer;
    unsigned char tempc;

    answer = (struct ka_ticketAnswer *)oanswer->SeqBody;

    if (challenge != ntohl(answer->challenge))
	return KABADPROTOCOL;
    memcpy(&token->sessionKey, &answer->sessionKey,
	   sizeof(token->sessionKey));
    token->startTime = ntohl(answer->startTime);
    token->endTime = ntohl(answer->endTime);
    token->kvno = (short)ntohl(answer->kvno);
    token->ticketLen = ntohl(answer->ticketLen);

    if (tkt_CheckTimes(token->startTime, token->endTime, time(0)) < 0)
	return KABADPROTOCOL;
    if ((token->ticketLen < MINKTCTICKETLEN)
	|| (token->ticketLen > MAXKTCTICKETLEN))
	return KABADPROTOCOL;

    {
	char *strings = answer->name;
	int len;
#undef chkstr
#define chkstr(field) \
	len = strlen (strings); \
	if (len > MAXKTCNAMELEN) return KABADPROTOCOL;\
	if ((field) && strcmp (field, strings)) return KABADPROTOCOL;\
	strings += len+1

#define chknostr() \
	len = strlen(strings); \
	if (len > MAXKTCNAMELEN) return KABADPROTOCOL; \
	strings += len+1

	if (caller) {
	    chkstr(caller->name);
	    chkstr(caller->instance);
	    chkstr(caller->cell);
	} else {
	    chknostr();
	    chknostr();
	    chknostr();
	}
	if (server) {
	    chkstr(server->name);
	    chkstr(server->instance);
	} else {
	    chknostr();
	    chknostr();
	}

	if (oanswer->SeqLen -
	    ((strings - oanswer->SeqBody) + token->ticketLen + KA_LABELSIZE)
	    >= (ENCRYPTIONBLOCKSIZE + 12)
	    )
	    return KABADPROTOCOL;

	memcpy(token->ticket, strings, token->ticketLen);
	strings += token->ticketLen;
	if (memcmp(strings, label, KA_LABELSIZE) != 0)
	    return KABADPROTOCOL;

	if (pwexpires) {
	    afs_int32 temp;
	    strings += KA_LABELSIZE;
	    temp = round_up_to_ebs((strings - oanswer->SeqBody));

	    if (oanswer->SeqLen > temp) {
		strings = oanswer->SeqBody + temp;
		memcpy(&temp, strings, sizeof(afs_int32));
		tempc = ntohl(temp) >> 24;
		/* don't forget this if you add any more fields!
		 * strings += sizeof(afs_int32);
		 */
	    } else {
		tempc = 255;
	    }
	    *pwexpires = tempc;
	}

    }
    return 0;
}

/* call this instead of stub and we'll guarantee to find a host that's up.
 * this doesn't handle UNOTSYNC very well, should use ubik_Call if you care
 */
static afs_int32
kawrap_ubik_Call(int (*aproc) (), struct ubik_client *aclient,
                 afs_int32 aflags, void *p1, void *p2, void *p3, void *p4,
                 void *p5, void *p6, void *p7, void *p8)
{
    afs_int32 code, lcode;
    int count;
    int pass;

    /* First pass only checks servers known running. Second checks all.
     * Once we've cycled through all the servers and still nothing, return
     * error code from the last server tried.
     */
    for (pass = 0, aflags |= UPUBIKONLY; pass < 2;
	 pass++, aflags &= ~UPUBIKONLY) {
	code = 0;
	count = 0;
	do {			/* Cycle through the servers */
	    lcode = code;
	    code =
		ubik_CallIter(aproc, aclient, aflags, &count, (long) p1,
			      (long) p2, (long) p3, (long) p4,
			      (long) p5, (long) p6, (long) p7,
			      (long) p8, 0, 0, 0, 0, 0, 0, 0, 0);
	} while ((code == UNOQUORUM) || (code == UNOTSYNC)
		 || (code == KALOCKED) || (code == -1));

	if (code != UNOSERVERS)
	    break;
    }

    /* If cycled through all the servers, return the last error code */
    if ((code == UNOSERVERS) && lcode) {
	code = lcode;
    }
    return code;
}

/* This is the interface to the AuthServer RPC routine Authenticate.  It
   formats the request packet, calls the encryption routine on the answer,
   calls Authenticate, and decrypts the response.  The response is checked for
   correctness and its contents are copied into the token. */

/* Errors:
     KABADKEY = the key failed when converted to a key schedule.  Probably bad
       parity.
     KAUBIKCALL - the call to Ubik returned an error, probably a communication
       failure such as timeout.
     KABADPROTOCOL - the returned packet was in error.  Since a packet was
       returned it can be presumed that the AuthServer correctly interpreted
       the response.  This may indicate an inauthentic AuthServer.
     <other> - errors generated by the server process are returned directly.
 */

afs_int32
ka_Authenticate(char *name, char *instance, char *cell, struct ubik_client * conn,	/* Ubik connection to the AuthServer in
											 * the desired cell */
		int service,	/* ticket granting or admin service */
		struct ktc_encryptionKey * key, Date start, Date end,	/* ticket lifetime */
		struct ktc_token * token, afs_int32 * pwexpires)
{				/* days until it expires */
    afs_int32 code;
    DES_key_schedule schedule;
    Date request_time;
    struct ka_gettgtRequest request;
    struct ka_gettgtAnswer answer_old;
    struct ka_ticketAnswer answer;
    ka_CBS arequest;
    ka_BBS oanswer;
    char *req_label;
    char *ans_label;
    int version;

    LOCK_GLOBAL_MUTEX;
    if ((code = DES_key_sched(ktc_to_cblock(key), &schedule))) {
	UNLOCK_GLOBAL_MUTEX;
	return KABADKEY;
    }

    if (service == KA_MAINTENANCE_SERVICE) {
	req_label = KA_GETADM_REQ_LABEL;
	ans_label = KA_GETADM_ANS_LABEL;
    } else if (service == KA_TICKET_GRANTING_SERVICE) {
	req_label = KA_GETTGT_REQ_LABEL;
	ans_label = KA_GETTGT_ANS_LABEL;
    } else {
	UNLOCK_GLOBAL_MUTEX;
	return KABADARGUMENT;
    }

    request_time = time(0);
    request.time = htonl(request_time);
    memcpy(request.label, req_label, sizeof(request.label));
    arequest.SeqLen = sizeof(request);
    arequest.SeqBody = (char *)&request;
    DES_pcbc_encrypt(arequest.SeqBody, arequest.SeqBody, arequest.SeqLen,
		     &schedule, ktc_to_cblockptr(key), ENCRYPT);

    oanswer.MaxSeqLen = sizeof(answer);
    oanswer.SeqLen = 0;
    oanswer.SeqBody = (char *)&answer;
    memset(&answer, 0, sizeof(answer));
    memset(&answer_old, 0, sizeof(answer_old));

    version = 2;
    code =
	kawrap_ubik_Call(KAA_AuthenticateV2, conn, 0, name, instance,
			 (void*)(uintptr_t)start, (void*)(uintptr_t)end, &arequest, &oanswer, 0, 0);
    if (code == RXGEN_OPCODE) {
	oanswer.SeqLen = 0;
	oanswer.MaxSeqLen = sizeof(answer);
	oanswer.SeqBody = (char *)&answer;
	version = 1;
	code =
	    ubik_KAA_Authenticate(conn, 0, name, instance, start, end,
				  &arequest, &oanswer);
	if (code == RXGEN_OPCODE) {
	    oanswer.SeqLen = 0;
	    oanswer.MaxSeqLen = sizeof(answer_old);
	    oanswer.SeqBody = (char *)&answer_old;
	    version = 0;
	    code =
		ubik_KAA_Authenticate_old(conn, 0, name, instance,
					  start, end, &arequest, &oanswer);
	}
	if (code == RXGEN_OPCODE) {
	    code = KAOLDINTERFACE;
	}
    }
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	if ((code >= KAMINERROR) && (code <= KAMAXERROR))
	    return code;
	return KAUBIKCALL;
    }
    DES_pcbc_encrypt(oanswer.SeqBody, oanswer.SeqBody, oanswer.SeqLen,
		     &schedule, ktc_to_cblockptr(key), DECRYPT);

    switch (version) {
    case 1:
    case 2:
	{
	    struct ktc_principal caller;
	    strcpy(caller.name, name);
	    strcpy(caller.instance, instance);
	    strcpy(caller.cell, "");
	    code =
		CheckTicketAnswer(&oanswer, request_time + 1, token, &caller,
				  0, ans_label, pwexpires);
	    if (code) {
		UNLOCK_GLOBAL_MUTEX;
		return code;
	    }
	}
	break;
    case 0:
	answer_old.time = ntohl(answer_old.time);
	answer_old.ticket_len = ntohl(answer_old.ticket_len);
	if ((answer_old.time != request_time + 1)
	    || (answer_old.ticket_len < MINKTCTICKETLEN)
	    || (answer_old.ticket_len > MAXKTCTICKETLEN)) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KABADPROTOCOL;
	}
	{
	    char *label = ((char *)answer_old.ticket) + answer_old.ticket_len;

	    if (strncmp(label, ans_label, sizeof(answer_old.label))) {
		UNLOCK_GLOBAL_MUTEX;
		return KABADPROTOCOL;
	    }
	    token->startTime = start;
	    token->endTime = end;
	    token->kvno = ntohl(answer_old.kvno);
	    token->ticketLen = answer_old.ticket_len;
	    memcpy(token->ticket, answer_old.ticket, sizeof(token->ticket));
	    memcpy(&token->sessionKey, &answer_old.sessionkey,
		   sizeof(struct ktc_encryptionKey));
	}
	break;
    default:
	UNLOCK_GLOBAL_MUTEX;
	return KAINTERNALERROR;
    }

    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

afs_int32
ka_GetToken(char *name, char *instance, char *cell, char *cname, char *cinst, struct ubik_client * conn,	/* Ubik conn to cell's AuthServer */
	    Date start, Date end,	/* desired ticket lifetime */
	    struct ktc_token * auth_token, char *auth_domain,
	    struct ktc_token * token)
{
    struct ka_getTicketTimes times;
    struct ka_getTicketAnswer answer_old;
    struct ka_ticketAnswer answer;
    afs_int32 code;
    ka_CBS aticket;
    ka_CBS atimes;
    ka_BBS oanswer;
    char *strings;
    int len;
    DES_key_schedule schedule;
    int version;
    afs_int32 pwexpires;

    LOCK_GLOBAL_MUTEX;
    aticket.SeqLen = auth_token->ticketLen;
    aticket.SeqBody = auth_token->ticket;

    code = DES_key_sched(ktc_to_cblock(&auth_token->sessionKey), &schedule);
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	return KABADKEY;
    }

    times.start = htonl(start);
    times.end = htonl(end);
    DES_ecb_encrypt((DES_cblock *)&times, (DES_cblock *)&times, &schedule,
		    ENCRYPT);

    atimes.SeqLen = sizeof(times);
    atimes.SeqBody = (char *)&times;

    oanswer.SeqLen = 0;
    oanswer.MaxSeqLen = sizeof(answer);
    oanswer.SeqBody = (char *)&answer;

    version = 1;
    code =
	ubik_KAT_GetTicket(conn, 0, auth_token->kvno, auth_domain,
		  &aticket, name, instance, &atimes, &oanswer);
    if (code == RXGEN_OPCODE) {
	oanswer.SeqLen = 0;	/* this may be set by first call */
	oanswer.MaxSeqLen = sizeof(answer_old);
	oanswer.SeqBody = (char *)&answer_old;
	version = 0;
	code =
	    ubik_KAT_GetTicket_old(conn, 0, auth_token->kvno,
		      auth_domain, &aticket, name, instance, &atimes,
		      &oanswer);
	if (code == RXGEN_OPCODE) {
	    code = KAOLDINTERFACE;
	}
    }
    if (code) {
	UNLOCK_GLOBAL_MUTEX;
	if ((code >= KAMINERROR) && (code <= KAMAXERROR))
	    return code;
	return KAUBIKCALL;
    }

    DES_pcbc_encrypt(oanswer.SeqBody, oanswer.SeqBody, oanswer.SeqLen,
		     &schedule, ktc_to_cblockptr(&auth_token->sessionKey),
		     DECRYPT);

    switch (version) {
    case 1:
	{
	    struct ktc_principal server;
	    strcpy(server.name, name);
	    strcpy(server.instance, instance);
	    code =
		CheckTicketAnswer(&oanswer, 0, token, 0, &server,
				  KA_GETTICKET_ANS_LABEL, &pwexpires);
	    if (code) {
		UNLOCK_GLOBAL_MUTEX;
		return code;
	    }
	}
	break;
    case 0:
	token->startTime = ntohl(answer_old.startTime);
	token->endTime = ntohl(answer_old.endTime);
	token->ticketLen = ntohl(answer_old.ticketLen);
	token->kvno = ntohl(answer_old.kvno);
	memcpy(&token->sessionKey, &answer_old.sessionKey,
	       sizeof(token->sessionKey));

	if (tkt_CheckTimes(token->startTime, token->endTime, time(0)) < 0) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KABADPROTOCOL;
	}
	if ((token->ticketLen < MINKTCTICKETLEN)
	    || (token->ticketLen > MAXKTCTICKETLEN)) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KABADPROTOCOL;
	}
	strings = answer_old.name;
	len = strlen(strings);	/* check client name */
	if ((len < 1) || (len > MAXKTCNAMELEN)) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KABADPROTOCOL;
	}
	strings += len + 1;	/* check client instance */
	len = strlen(strings);
	if ((len < 0) || (len > MAXKTCNAMELEN)) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KABADPROTOCOL;
	}
	strings += len + 1;
	len = strlen(strings);	/* check client cell */
	if ((len < 0) || (len > MAXKTCNAMELEN)) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KABADPROTOCOL;
	}
	strings += len + 1;
	len = strlen(strings);	/* check server name */
	if ((len < 1) || (len > MAXKTCNAMELEN) || strcmp(name, strings)) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KABADPROTOCOL;
	}
	strings += len + 1;
	len = strlen(strings);	/* check server instance */
	if ((len < 0) || (len > MAXKTCNAMELEN) || strcmp(instance, strings)) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KABADPROTOCOL;
	}
	strings += len + 1;

	if ((strings - oanswer.SeqBody + token->ticketLen) - oanswer.SeqLen >=
	    ENCRYPTIONBLOCKSIZE) {
	    UNLOCK_GLOBAL_MUTEX;
	    return KABADPROTOCOL;
	}
	memcpy(token->ticket, strings, token->ticketLen);

	break;
    default:
	UNLOCK_GLOBAL_MUTEX;
	return KAINTERNALERROR;
    }

    UNLOCK_GLOBAL_MUTEX;
    return 0;
}

afs_int32
ka_ChangePassword(char *name, char *instance, struct ubik_client * conn,	/* Ubik connection to the AuthServer in
										 * the desired cell */
		  struct ktc_encryptionKey * oldkey,
		  struct ktc_encryptionKey * newkey)
{
    afs_int32 code;

    LOCK_GLOBAL_MUTEX;
    code =
	ubik_KAM_SetPassword(conn, UBIK_CALL_NEW, name, instance, 0, *(EncryptionKey *)newkey);
    UNLOCK_GLOBAL_MUTEX;
    return code;
}
