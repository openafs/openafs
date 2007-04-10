/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This file contains the code to handle UDP requests to the authentication
   server using the MIT Kerberos protocol for obtaining tickets.  It will only
   handle authentication and get ticket requests to provide read-only protocol
   level compatibility with the standard Kerberos servers. */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <afs/errmap_nt.h>
#define snprintf _snprintf
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <afs/afsutil.h>
#include <time.h>
#include <afs/com_err.h>
#include <lwp.h>
#include <des.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <afs/auth.h>
#include <ubik.h>

#include "kauth.h"
#include "kautils.h"
#include "kaserver.h"
#include "prot.h"		/* protocol definitions */
#include "kaport.h"
#include "afs/audit.h"
#include "kalog.h"

/* my kerberos error codes */
#define KERB_ERR_BAD_MSG_TYPE  99
#define KERB_ERR_BAD_LIFETIME  98
#define KERB_ERR_NONNULL_REALM 97
#define KERB_ERR_PKT_LENGTH    96
#define KERB_ERR_TEXT_LENGTH   95
#ifndef	KDC_GEN_ERR
#define	KDC_GEN_ERR	20
#endif


int krb_udp_debug = 0;

static int sock_kerb = -1;	/* the socket we're using */
static int sock_kerb5 = -1;	/* the socket we're using */

struct packet {
    int len;
    struct sockaddr_in from;
    int byteOrder;
    char *name;
    char *inst;
    char *realm;
    Date time;
    char *rest;			/* remaining bytes of packet */
    char data[MAX_PKT_LEN];
};

extern char *lastOperation;
extern char lrealm[];

char udpAuthPrincipal[256];
char udptgsPrincipal[256];
char udptgsServerPrincipal[256];

#define putstr(name) if ((slen = strlen(name)) >= MAXKTCNAMELEN) return -1;\
		     else strcpy (answer, name), answer += slen+1
#define putint(num) num = htonl(num), \
		    memcpy(answer, &num, sizeof(afs_int32)), \
		    answer += sizeof(afs_int32)

#define getstr(name) if ((slen = strlen(packet)) >= sizeof(name)) return -1;\
		     strcpy (name, packet), packet += slen+1
#define getint(num) memcpy(&num, packet, sizeof(afs_int32)), \
		    num = ktohl (byteOrder, num), \
		    packet += sizeof(afs_int32)

/* this is just to close the log every five minutes to rm works */

int fiveminutes = 300;

static
FiveMinuteCheckLWP()
{

    printf("start 5 min check lwp\n");

    while (1) {
	IOMGR_Sleep(fiveminutes);
	/* close the log so it can be removed */
	ReOpenLog(AFSDIR_SERVER_KALOG_FILEPATH);	/* no trunc, just append */
    }
}


static afs_int32
create_cipher(cipher, cipherLen, sessionKey, sname, sinst, start, end, kvno,
	      ticket, ticketLen, key)
     char *cipher;
     int *cipherLen;
     struct ktc_encryptionKey *sessionKey;
     char *sname;
     char *sinst;
     Date start, end;
     afs_int32 kvno;
     char *ticket;
     int ticketLen;
     struct ktc_encryptionKey *key;
{
    char *answer;
    int slen;
    unsigned char life = time_to_life(start, end);
    int len;
    des_key_schedule schedule;
    afs_int32 code;

    answer = cipher;
    len =
	sizeof(*sessionKey) + strlen(sname) + strlen(sinst) + strlen(lrealm) +
	3 /*nulls */  + 3 + ticketLen + sizeof(Date);
    if (len > *cipherLen)
	return KAANSWERTOOLONG;
    if (ticketLen > 255)
	return KAANSWERTOOLONG;
    if (kvno > 255)
	return KAANSWERTOOLONG;

    memcpy(answer, sessionKey, sizeof(*sessionKey));
    answer += sizeof(*sessionKey);
    putstr(sname);
    putstr(sinst);
    putstr(lrealm);
    *answer++ = life;
    *answer++ = (unsigned char)kvno;
    *answer++ = (unsigned char)ticketLen;
    memcpy(answer, ticket, ticketLen);
    answer += ticketLen;
    putint(start);

    if (krb_udp_debug) {
	printf("Printing ticket (%d) and date: ", ticketLen);
	ka_PrintBytes(ticket, ticketLen);
	ka_PrintBytes(answer - 4, 4);
	printf("\n");
    }

    if (code = des_key_sched(key, schedule))
	printf("In KAAuthenticate: key_sched returned %d\n", code);
    des_pcbc_encrypt(cipher, cipher, len, schedule, key, ENCRYPT);
    *cipherLen = round_up_to_ebs(len);

    if (krb_udp_debug) {
	printf("Printing cipher (%d): ", *cipherLen);
	ka_PrintBytes(cipher, *cipherLen);
	printf("\n");
    }
    return 0;
}

static afs_int32
create_reply(ans, name, inst, startTime, endTime, kvno, cipher, cipherLen)
     struct packet *ans;
     char *name;
     char *inst;
     Date startTime, endTime;
     afs_int32 kvno;
     char *cipher;
     int cipherLen;
{
    char *answer = ans->data;
    int slen;

    ans->len = 2 + strlen(name) + strlen(inst) + 3 /*nulls */  +
	sizeof(afs_int32) + 1 /*ntkts */  + sizeof(afs_int32) + 1 /*kvno */  +
	sizeof(short) + cipherLen;
    if (ans->len > sizeof(ans->data))
	return KAANSWERTOOLONG;
    if (kvno > 255)
	return KAANSWERTOOLONG;

    *answer++ = (unsigned char)KRB_PROT_VERSION;
    *answer++ = (unsigned char)AUTH_MSG_KDC_REPLY;
    /* always send claiming network byte order */
    putstr(name);
    putstr(inst);
    putstr("");
    putint(startTime);
    *answer++ = 1;		/* undocumented number of tickets! */
    putint(endTime);
    *answer++ = (unsigned char)kvno;
    {
	short w = (short)cipherLen;
	w = htons(w);
	memcpy(answer, &w, sizeof(short));
	answer += sizeof(short);
    }
    memcpy(answer, cipher, cipherLen);
    return 0;
}

static afs_int32
check_auth(pkt, auth, authLen, key, name, inst, cell)
     struct packet *pkt;
     char *auth;
     int authLen;
     struct ktc_encryptionKey *key;
     char *name;
     char *inst;
     char *cell;
{
    char *packet;
    des_key_schedule schedule;
    afs_int32 cksum;
    /* unsigned char time_msec; */
    afs_int32 time_sec;
    int byteOrder = pkt->byteOrder;

    des_key_sched(key, schedule);
    des_pcbc_encrypt(auth, auth, authLen, schedule, key, DECRYPT);
    packet = auth;
    if (strcmp(packet, name) != 0)
	return KABADTICKET;
    packet += strlen(packet) + 1;
    if (strcmp(packet, inst) != 0)
	return KABADTICKET;
    packet += strlen(packet) + 1;
    if (strcmp(packet, cell) != 0)
	return KABADTICKET;
    packet += strlen(packet) + 1;
    getint(cksum);
    /* time_msec = */ *(unsigned char *)packet++;
    getint(time_sec);
    if ((packet - auth) > authLen)
	return KABADTICKET;
    return 0;
}

afs_int32
UDP_Authenticate(ksoc, client, name, inst, startTime, endTime, sname, sinst)
     int ksoc;
     struct sockaddr_in *client;
     char *name;
     char *inst;
     Date startTime;
     Date endTime;
     char *sname;
     char *sinst;
{
    struct ubik_trans *tt;
    afs_int32 to;		/* offset of block */
    struct kaentry tentry;
    afs_int32 tgskvno;		/* key version of service key */
    struct ktc_encryptionKey tgskey;	/* service key for encrypting ticket */
    int tgt;
    Date now = time(0);
    afs_int32 code;

    char ticket[MAXKTCTICKETLEN];	/* our copy of the ticket */
    int ticketLen;
    struct ktc_encryptionKey sessionKey;	/* we have to invent a session key */

    char cipher[2 * MAXKTCTICKETLEN];	/* put encrypted part of answer here */
    int cipherLen;
    struct packet ans;

    COUNT_REQ(UAuthenticate);
    if (!name_instance_legal(name, inst))
	return KERB_ERR_NAME_EXP;	/* KABADNAME */
    if (code = InitAuthServ(&tt, LOCKREAD, this_op))
	return code;

    code = FindBlock(tt, name, inst, &to, &tentry);
    if (code)
	goto abort;
    if (to) {			/* if user exists check other stuff */
	afs_int32 sto;
	struct kaentry sentry;
	save_principal(udpAuthPrincipal, name, inst, 0);

	tgt = ((strcmp(sname, KA_TGS_NAME) == 0)
	       && (strcmp(sinst, lrealm) == 0));
	if ((ntohl(tentry.user_expiration) < now)
	    || (tgt && (ntohl(tentry.flags) & KAFNOTGS))) {
	    code = KERB_ERR_NAME_EXP;	/* KABADUSER */
	    goto abort;
	}
	code = FindBlock(tt, KA_TGS_NAME, lrealm, &sto, &sentry);
	if (code)
	    goto abort;
	if (sto == 0) {
	    code = KANOENT;
	    goto abort;
	}
	if ((ntohl(sentry.user_expiration) < now)) {
	    code = KERB_ERR_NAME_EXP;	/* XXX Could use another error code XXX */
	    goto abort;
	}
	if (abs(startTime - now) > KTC_TIME_UNCERTAINTY) {
	    code = KERB_ERR_SERVICE_EXP;	/* was KABADREQUEST */
	    goto abort;
	}

	if (tentry.misc_auth_bytes) {
	    unsigned char misc_auth_bytes[4];
	    afs_uint32 temp;	/* unsigned for safety */
	    afs_uint32 pwexpires;

	    temp = ntohl(*((afs_int32 *) (tentry.misc_auth_bytes)));
	    unpack_long(temp, misc_auth_bytes);
	    pwexpires = misc_auth_bytes[0];
	    if (pwexpires) {
		pwexpires =
		    ntohl(tentry.change_password_time) +
		    24 * 60 * 60 * pwexpires;
		if (pwexpires < now) {
		    code = KERB_ERR_AUTH_EXP;	/* was KAPWEXPIRED */
		    goto abort;
		}
	    }
	}

	/* make the ticket */
	code = des_random_key(&sessionKey);
	if (code) {
	    code = KERB_ERR_NULL_KEY;	/* was KANOKEYS */
	    goto abort;
	}
	endTime =
	    umin(endTime, startTime + ntohl(tentry.max_ticket_lifetime));
	if ((code = ka_LookupKey(tt, sname, sinst, &tgskvno, &tgskey))
	    || (code =
		tkt_MakeTicket(ticket, &ticketLen, &tgskey, name, inst,
			       lrealm, startTime, endTime, &sessionKey,
			       htonl(client->sin_addr.s_addr), sname, sinst)))
	    goto abort;

	cipherLen = sizeof(cipher);
	code =
	    create_cipher(cipher, &cipherLen, &sessionKey, sname, sinst,
			  startTime, endTime, tgskvno, ticket, ticketLen,
			  &tentry.key);
	if (code)
	    goto abort;
    } else {			/* no such user */
	cipherLen = 0;
	tentry.key_version = 0;
    }
    code = ubik_EndTrans(tt);
    if (code)
	goto fail;

    code =
	create_reply(&ans, name, inst, startTime, endTime,
		     ntohl(tentry.key_version), cipher, cipherLen);
    if (code)
	goto fail;

    if (krb_udp_debug) {
	printf("Sending %d bytes ending in: ", ans.len);
	ka_PrintBytes(ans.data + ans.len - 8, 8);
	printf("\n");
    }

    code =
	sendto(ksoc, ans.data, ans.len, 0, (struct sockaddr *)client,
	       sizeof(*client));
    if (code != ans.len) {
	perror("calling sendto");
	code = -1;
	goto fail;
    }
    KALOG(name, inst, sname, sinst, NULL, client->sin_addr.s_addr,
	  LOG_AUTHENTICATE);
    osi_audit(UDPAuthenticateEvent, 0, AUD_STR, name, AUD_STR, inst, AUD_END);
    return 0;

  abort:
    COUNT_ABO;
    ubik_AbortTrans(tt);

  fail:
    osi_audit(UDPAuthenticateEvent, code, AUD_STR, name, AUD_STR, inst,
	      AUD_END);
    return code;
}

afs_int32
UDP_GetTicket(ksoc, pkt, kvno, authDomain, ticket, ticketLen, auth, authLen)
     int ksoc;
     struct packet *pkt;
     afs_int32 kvno;
     char *authDomain;
     char *ticket;
     int ticketLen;
     char *auth;
     int authLen;
{
    afs_int32 code;
    struct ktc_encryptionKey tgskey;
    char name[MAXKTCNAMELEN];
    char inst[MAXKTCNAMELEN];
    char cell[MAXKTCREALMLEN];
    struct ktc_encryptionKey authSessionKey;
    afs_int32 host;
    Date start;
    Date authEnd;
    Date now = time(0);
    int celllen;
    int import;

    char *packet;
    int slen;
    int byteOrder = pkt->byteOrder;
    char sname[MAXKTCNAMELEN];
    char sinst[MAXKTCNAMELEN];
    afs_int32 time_ws;
    unsigned char life;

    struct ubik_trans *tt;
    afs_int32 to;
    struct kaentry caller;
    struct kaentry server;
    Date reqEnd;
    struct ktc_encryptionKey sessionKey;
    int newTicketLen;
    char newTicket[MAXKTCTICKETLEN];

    char cipher[2 * MAXKTCTICKETLEN];	/* put encrypted part of answer here */
    int cipherLen;
    struct packet ans;

    COUNT_REQ(UGetTicket);

    if (code = InitAuthServ(&tt, LOCKREAD, this_op))
	goto fail;
    code =
	ka_LookupKvno(tt, KA_TGS_NAME,
		      ((strlen(authDomain) > 0) ? authDomain : lrealm), kvno,
		      &tgskey);
    if (code)
	goto abort;

    code =
	tkt_DecodeTicket(ticket, ticketLen, &tgskey, name, inst, cell,
			 &authSessionKey, &host, &start, &authEnd);
    pkt->name = name;
    pkt->inst = inst;
    pkt->realm = cell;
    if (code) {
	code = KERB_ERR_AUTH_EXP;	/* was KANOAUTH */
	goto abort;
    }
    save_principal(udptgsPrincipal, name, inst, cell);
    code = tkt_CheckTimes(start, authEnd, now);
    if (code <= 0) {
	if (code == -1) {
	    code = KERB_ERR_SERVICE_EXP;	/* was RXKADEXPIRED */
	    goto abort;
	}
	code = KERB_ERR_AUTH_EXP;	/* was KANOAUTH */
	goto abort;
    }
    celllen = strlen(cell);
    import = 0;
    if ((strlen(authDomain) > 0) && (strcmp(authDomain, lrealm) != 0))
	import = 1;
    if (import && (celllen == 0)) {
	code = KERB_ERR_PKT_VER;	/* was KABADTICKET */
	goto abort;
    }
    if (celllen == 0) {
	strncpy(cell, lrealm, MAXKTCREALMLEN - 1);
	cell[MAXKTCREALMLEN - 1] = 0;
    };

    if (!krb4_cross && strcmp(lrealm, cell) != 0) {
	code = KERB_ERR_PRINCIPAL_UNKNOWN;
	goto abort;
    }

    if (krb_udp_debug) {
	printf("UGetTicket: got ticket from '%s'.'%s'@'%s'\n", name, inst,
	       cell);
    }

    code = check_auth(pkt, auth, authLen, &authSessionKey, name, inst, cell);
    if (code)
	goto abort;

    /* authenticator and all is OK so read actual request */
    packet = pkt->rest;
    getint(time_ws);
    life = *(unsigned char *)packet++;
    getstr(sname);
    getstr(sinst);
    start = now;
    reqEnd = life_to_time(start, life);
    if (krb_udp_debug) {
	printf("UGetTicket: request for server '%s'.'%s'\n", sname, sinst);
    }
    save_principal(udptgsServerPrincipal, sname, sinst, 0);

    if (import) {
	strcpy(caller.userID.name, name);
	strcpy(caller.userID.instance, inst);
	caller.max_ticket_lifetime = htonl(MAXKTCTICKETLIFETIME);
    } else {
	code = FindBlock(tt, name, inst, &to, &caller);
	if (code)
	    goto abort;
	if (to == 0) {
	    ka_PrintUserID("GetTicket: User ", name, inst, " unknown.\n");
	    code = KERB_ERR_PRINCIPAL_UNKNOWN;	/* KANOENT */
	    goto abort;
	}
	if (ntohl(caller.flags) & KAFNOTGS) {
	    code = KERB_ERR_AUTH_EXP;	/* was KABADUSER */
	    goto abort;
	}
    }

    code = FindBlock(tt, sname, sinst, &to, &server);	/* get server's entry */
    if (code)
	goto abort;
    if (to == 0) {		/* entry not found */
	ka_PrintUserID("GetTicket: Server ", sname, sinst, " unknown.\n");
	code = KERB_ERR_PRINCIPAL_UNKNOWN;	/* KANOENT */
	goto abort;
    }
    code = ubik_EndTrans(tt);
    if (code)
	goto fail;

    if (ntohl(server.flags) & KAFNOSEAL)
	return KABADSERVER;

    code = des_random_key(&sessionKey);
    if (code) {
	code = KERB_ERR_NULL_KEY;	/* was KANOKEYS */
	goto fail;
    }

    reqEnd =
	umin(umin(reqEnd, authEnd),
	     umin(start + ntohl(caller.max_ticket_lifetime),
		  start + ntohl(server.max_ticket_lifetime)));

    code =
	tkt_MakeTicket(newTicket, &newTicketLen, &server.key,
		       caller.userID.name, caller.userID.instance, cell,
		       start, reqEnd, &sessionKey,
		       htonl(pkt->from.sin_addr.s_addr), server.userID.name,
		       server.userID.instance);
    if (code)
	goto fail;

    cipherLen = sizeof(cipher);
    code =
	create_cipher(cipher, &cipherLen, &sessionKey, sname, sinst, start,
		      reqEnd, ntohl(server.key_version), newTicket,
		      newTicketLen, &authSessionKey);
    if (code)
	goto fail;

    code =
	create_reply(&ans, name, inst, start, reqEnd, 0, cipher, cipherLen);
    if (code)
	goto fail;

    code =
	sendto(ksoc, ans.data, ans.len, 0, (struct sockaddr *)&pkt->from,
	       sizeof(pkt->from));
    if (code != ans.len) {
	perror("calling sendto");
	code = -1;
	goto fail;
    }

    if (cipherLen != 0) {
	KALOG(name, inst, sname, sinst, NULL, host, LOG_GETTICKET);
    }
    osi_audit(UDPGetTicketEvent, 0, AUD_STR, name, AUD_STR, inst, AUD_STR,
	      cell, AUD_STR, sname, AUD_STR, sinst, AUD_END);
    return 0;

  abort:
    ubik_AbortTrans(tt);
  fail:
    osi_audit(UDPGetTicketEvent, code, AUD_STR, name, AUD_STR, inst, AUD_STR,
	      NULL, AUD_STR, NULL, AUD_STR, NULL, AUD_END);
    return code;
}

static int
err_packet(ksoc, pkt, code, reason)
     int ksoc;
     struct packet *pkt;
     afs_int32 code;
     char *reason;
{
    struct packet ans;
    char *answer = ans.data;
    int slen;
    char buf[256];

    if (reason == 0)
	reason = "";
    snprintf(buf, 255, "code = %d: %s", code, reason);

    if (krb_udp_debug) {
	printf("Sending error packet to '%s'.'%s'@'%s' containing %s\n",
	       pkt->name, pkt->inst, pkt->realm, buf);
    }

    ans.len =
	2 + strlen(pkt->name) + strlen(pkt->inst) + strlen(pkt->realm) +
	3 /* nulls */  + (2 * sizeof(afs_int32)) + strlen(buf) + 1;
    if (ans.len > sizeof(ans.data)) {
	printf("Answer packet too long\n");
	return -1;
    }

    *answer++ = (unsigned char)KRB_PROT_VERSION;
    *answer++ = (unsigned char)AUTH_MSG_ERR_REPLY;
    /* always send claiming network byte order */
    putstr(pkt->name);
    putstr(pkt->inst);
    putstr(pkt->realm);
    putint(pkt->time);
    if ((code < 0) || (code > KERB_ERR_MAXIMUM)) {
	/* It could be because of kauth errors so we should return something instead of success!! */
	code = KDC_GEN_ERR;
    }
    putint(code);
    putstr(buf);

    code =
	sendto(ksoc, ans.data, ans.len, 0, (struct sockaddr *)&pkt->from,
	       sizeof(pkt->from));
    if (code != ans.len) {
	if (code >= 0)
	    printf
		("call to sendto returned %d, sending error packet %d bytes long\n",
		 code, ans.len);
	else
	    perror("err_packet: sendto");
    }
    return 0;
}

int
process_udp_auth(ksoc, pkt)
     int ksoc;
     struct packet *pkt;
{
    char *packet = pkt->rest;
    char name[MAXKTCNAMELEN];
    char inst[MAXKTCNAMELEN];
    char realm[MAXKTCREALMLEN];
    char sname[MAXKTCNAMELEN];
    char sinst[MAXKTCNAMELEN];
    int slen;
    Date now = time(0);
    Date startTime, endTime;
    unsigned char lifetime;
    afs_int32 code;

    pkt->name = packet;
    getstr(name);
    pkt->inst = packet;
    getstr(inst);
    pkt->realm = packet;
    getstr(realm);
    if (krb_udp_debug) {
	printf("Processing KDC Request from '%s'.'%s'@'%s'\n", name, inst,
	       realm);
    }

    if ((strlen(realm) > 0) && (strcmp(realm, lrealm) != 0)) {
	err_packet(ksoc, pkt, KERB_ERR_NONNULL_REALM,
		   "null realm name not allowed");
	return -1;
    }
    memcpy(&startTime, packet, sizeof(startTime));
    packet += sizeof(startTime);
    startTime = ktohl(pkt->byteOrder, startTime);
    pkt->time = startTime;
    lifetime = *packet++;
    endTime = life_to_time(startTime, lifetime);
    code = tkt_CheckTimes(startTime, endTime, now);
    if (code < 0) {
	err_packet(ksoc, pkt, KERB_ERR_BAD_LIFETIME,
		   "requested ticket lifetime invalid");
	return -1;
    }
    getstr(sname);
    getstr(sinst);
    if ((packet - pkt->data) != pkt->len) {
	err_packet(ksoc, pkt, KERB_ERR_PKT_LENGTH,
		   "packet length inconsistent");
	return -1;
    }
    pkt->rest = packet;
    code =
	UDP_Authenticate(ksoc, &pkt->from, name, inst, startTime, endTime,
			 sname, sinst);
    if (code) {
	if (code == KANOENT) {
	    code = KERB_ERR_PRINCIPAL_UNKNOWN;
	    err_packet(ksoc, pkt, code, (char *)afs_error_message(code));
	} else if (code == KAPWEXPIRED) {
	    code = KERB_ERR_NAME_EXP;
	    err_packet(ksoc, pkt, code, "password has expired");
	} else
	    err_packet(ksoc, pkt, code, (char *)afs_error_message(code));
    }
    return 0;
}

int
process_udp_appl(ksoc, pkt)
     int ksoc;
     struct packet *pkt;
{
    char *packet = pkt->rest;
    afs_int32 kvno;
    char realm[MAXKTCREALMLEN];
    char ticket[MAXKTCTICKETLEN];
    char auth[3 * MAXKTCNAMELEN + 4 + 5];
    int slen;
    int ticketLen, authLen;
    afs_int32 code;

    if (krb_udp_debug) {
	printf("Processing APPL Request\n");
    }
    kvno = *packet++;
    getstr(realm);
    ticketLen = *(unsigned char *)packet++;
    authLen = *(unsigned char *)packet++;
    if (ticketLen > sizeof(ticket)) {
	err_packet(ksoc, pkt, KERB_ERR_TEXT_LENGTH, "ticket too long");
	return -1;
    }
    memcpy(ticket, packet, ticketLen);
    packet += ticketLen;
    if (authLen > sizeof(auth)) {
	err_packet(ksoc, pkt, KERB_ERR_TEXT_LENGTH, "authenticator too long");
	return -1;
    }
    memcpy(auth, packet, authLen);
    pkt->rest = packet + authLen;
    code =
	UDP_GetTicket(ksoc, pkt, kvno, realm, ticket, ticketLen, auth,
		      authLen);
    if (code) {
	if (code == KANOENT)
	    code = KERB_ERR_PRINCIPAL_UNKNOWN;
	err_packet(ksoc, pkt, code, (char *)afs_error_message(code));
	return -1;
    }
    return 0;
}

void
process_udp_request(ksoc, pkt)
     int ksoc;
     struct packet *pkt;
{
    char *packet = pkt->data;
    unsigned char version, auth_msg_type;

    version = *packet++;
    if (version != KRB_PROT_VERSION) {
	err_packet(ksoc, pkt, KERB_ERR_PKT_VER,
		   "packet version number unknown");
	return;
    }
    auth_msg_type = *packet++;
    pkt->byteOrder = auth_msg_type & 1;
    pkt->rest = packet;
    switch (auth_msg_type & ~1) {
    case AUTH_MSG_KDC_REQUEST:
	process_udp_auth(ksoc, pkt);
	break;
    case AUTH_MSG_APPL_REQUEST:
	process_udp_appl(ksoc, pkt);
	break;
    default:
	printf("unknown msg type 0x%x\n", auth_msg_type);
	err_packet(ksoc, pkt, KERB_ERR_BAD_MSG_TYPE,
		   "message type not supported");
	return;
    }
    return;
}

static
SocketListener()
{
    fd_set rfds;
    struct timeval tv;
    struct packet packet;
    int fromLen;
    afs_int32 code;

    printf("Starting to listen for UDP packets\n");
    while (1) {
	FD_ZERO(&rfds);
	if (sock_kerb >= 0)
	    FD_SET(sock_kerb, &rfds);
	if (sock_kerb5 >= 0)
	    FD_SET(sock_kerb5, &rfds);

	tv.tv_sec = 100;
	tv.tv_usec = 0;
	/* write and exception fd_set's are null */
	code = IOMGR_Select(32, &rfds, NULL, NULL, &tv);
	if (code == 0) {	/* timeout */
	    /* printf ("Timeout\n"); */
	    continue;
	} else if (code < 0) {
	    perror("calling IOMGR_Select");
	    break;
	}

	fromLen = sizeof(packet.from);
	if ((sock_kerb >= 0) && FD_ISSET(sock_kerb, &rfds)) {
	    code =
		recvfrom(sock_kerb, packet.data, sizeof(packet.data), 0,
			 (struct sockaddr *)&packet.from, &fromLen);
	    if (code < 0) {
		if (errno == EAGAIN || errno == ECONNREFUSED)
		    goto try_kerb5;
		perror("calling recvfrom");
		break;
	    }
	    packet.len = code;
	    if (krb_udp_debug) {
		printf("Kerb:udp: Got %d bytes from addr %s which are '",
		       code, afs_inet_ntoa(packet.from.sin_addr.s_addr));
		ka_PrintBytes(packet.data, packet.len);
		printf("'\n");
	    }
	    packet.name = packet.inst = packet.realm = "";
	    packet.time = 0;
	    process_udp_request(sock_kerb, &packet);
	}
      try_kerb5:
	if ((sock_kerb5 >= 0) && FD_ISSET(sock_kerb5, &rfds)) {
	    code =
		recvfrom(sock_kerb5, packet.data, sizeof(packet.data), 0,
			 (struct sockaddr *)&packet.from, &fromLen);
	    if (code < 0) {
		if (errno == EAGAIN || errno == ECONNREFUSED)
		    continue;
		perror("calling recvfrom");
		break;
	    }
	    packet.len = code;
	    if (krb_udp_debug) {
		printf("Kerb5:udp: Got %d bytes from addr %s which are '",
		       code, afs_inet_ntoa(packet.from.sin_addr.s_addr));
		ka_PrintBytes(packet.data, packet.len);
		printf("'\n");
	    }
	    packet.name = packet.inst = packet.realm = "";
	    packet.time = 0;
	    process_udp_request(sock_kerb5, &packet);
	}
    }
    if (sock_kerb >= 0) {
	close(sock_kerb);
	sock_kerb = -1;
    }
    if (sock_kerb5 >= 0) {
	close(sock_kerb5);
	sock_kerb5 = -1;
    }
    printf("UDP SocketListener exiting due to error\n");
}

#if MAIN

#include "AFS_component_version_number.c"

main()
#else
afs_int32
init_krb_udp()
#endif
{
    struct sockaddr_in taddr;
    static PROCESS slPid;	/* socket listener pid */
    static PROCESS checkPid;	/* fiveminute check */
    afs_int32 code;
    char *krb4name;		/* kerberos version4 service */

#if MAIN
    PROCESS junk;
#endif
    struct servent *sp;
    static int inited = 0;
    afs_int32 kerb_port;

    if (inited)
	return -1;
    inited = 1;

    memset(&taddr, 0, sizeof(taddr));
    krb4name = "kerberos4";
    sp = getservbyname(krb4name, "udp");
    taddr.sin_family = AF_INET;	/* added for NCR port */
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    taddr.sin_len = sizeof(struct sockaddr_in);
#endif
    if (!sp) {
	/* if kerberos-4 is not available, try "kerberos-iv" */
	krb4name = "kerberos-iv";
	sp = getservbyname(krb4name, "udp");
    }
    if (!sp) {
	/* if kerberos-iv is not available, try "kerberos" */
	krb4name = "kerberos";
	sp = getservbyname(krb4name, "udp");
    }
    if (!sp) {
	fprintf(stderr,
		"kerberos/udp is unknown; check /etc/services.  Using port=%d as default\n",
		KRB_PORT);
	taddr.sin_port = htons(KRB_PORT);
    } else {
	/* copy the port number */
	fprintf(stderr, "%s/udp port=%hu\n", krb4name,
		(unsigned short)sp->s_port);
	taddr.sin_port = sp->s_port;
    }
    kerb_port = taddr.sin_port;
    sock_kerb = socket(AF_INET, SOCK_DGRAM, 0);
    code = bind(sock_kerb, (struct sockaddr *)&taddr, sizeof(taddr));
    if (code < 0) {
	perror("calling bind");
	sock_kerb = -1;
    }

    sp = getservbyname("kerberos5", "udp");
    if (!sp) {
	fprintf(stderr,
		"kerberos5/udp is unknown; check /etc/services.  Using port=%d as default\n",
		KRB5_PORT);
	taddr.sin_port = htons(KRB5_PORT);
    } else {
	/* copy the port number */
	fprintf(stderr, "kerberos5/udp port=%hu\n",
		(unsigned short)sp->s_port);
	taddr.sin_port = sp->s_port;
    }
    if (taddr.sin_port != kerb_port) {	/* a different port */
	sock_kerb5 = socket(AF_INET, SOCK_DGRAM, 0);
	code = bind(sock_kerb5, (struct sockaddr *)&taddr, sizeof(taddr));
	if (code < 0) {
	    perror("calling bind");
	    sock_kerb5 = -1;
	}
    }

    /* Bail out if we can't bind with any port */
    if ((sock_kerb < 0) && (sock_kerb5 < 0))
	return -1;

#if MAIN
    /* this has already been done */
    LWP_InitializeProcessSupport(LWP_NORMAL_PRIORITY, &junk);
    IOMGR_Initialize();
#endif
    LWP_CreateProcess(SocketListener, /*stacksize */ 16000,
		      LWP_NORMAL_PRIORITY, (void *)0, "Socket Listener",
		      &slPid);

    /* just to close the log every five minutes */

    LWP_CreateProcess(FiveMinuteCheckLWP, 24 * 1024, LWP_MAX_PRIORITY - 2,
		      (void *)&fiveminutes, "FiveMinuteChecks", &checkPid);

#if MAIN
    initialize_ka_error_table();
    initialize_rxk_error_table();
    while (1)			/* don't just stand there, run it */
	IOMGR_Sleep(60);
#else
    return 0;
#endif

}

#if MAIN
char *lastOperation;		/* name of last operation */
char *lrealm = "REALMNAME";
struct kadstats dynamic_statistics;

static int
InitAuthServ(tt, lock, this_op)
     struct ubik_trans **tt;
     int lock;			/* read or write transaction */
     int *this_op;		/* op of RCP routine, for COUNT_ABO */
{
    int code = 0;

    *tt = 0;
    printf("Calling InitAuthServ\n");
    return code;
}

static int
ubik_EndTrans(tt)
     struct ubik_trans *tt;
{
    printf("Calling ubik_EndTrans\n");
    return 0;
}

static int
ubik_AbortTrans(tt)
     struct ubik_trans *tt;
{
    printf("Calling ubik_AbortTrans\n");
    return 0;
}

static int
FindBlock(at, aname, ainstance, tentry)
     struct ubik_trans *at;
     char *aname;
     char *ainstance;
     struct kaentry *tentry;
{
    printf("Calling FindBlock with '%s'.'%s'\n", aname, ainstance);
    strcpy(tentry->userID.name, aname);
    strcpy(tentry->userID.instance, ainstance);
    tentry->key_version = htonl(17);
    des_string_to_key("toa", &tentry->key);
    tentry->flags = htonl(KAFNORMAL);
    tentry->user_expiration = htonl(NEVERDATE);
    tentry->max_ticket_lifetime = htonl(MAXKTCTICKETLIFETIME);
    return 323232;
}

static int
ka_LookupKey(tt, name, inst, kvno, key)
     struct ubik_trans *tt;
     char *name;
     char *inst;
     afs_int32 *kvno;		/* returned */
     struct ktc_encryptionKey *key;	/* copied out */
{
    printf("Calling ka_LookupKey with '%s'.'%s'\n", name, inst);
    *kvno = 23;
    des_string_to_key("applexx", key);
}

static afs_int32
kvno_tgs_key(authDomain, kvno, tgskey)
     char *authDomain;
     afs_int32 kvno;
     struct ktc_encryptionKey *tgskey;
{
    if (strcmp(authDomain, lrealm) != 0)
	printf("Called with wrong %s as authDomain\n", authDomain);
    if (kvno != 23)
	printf("kvno_tgs_key: being called with wrong kvno: %d\n", kvno);
    des_string_to_key("applexx", tgskey);
    return 0;
}

save_principal()
{
}

name_instance_legal()
{
    return 1;
}
#endif
