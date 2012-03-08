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

#include <windows.h>
#include <rpc.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>		/* sprintf */
#include <malloc.h>
#include <afs/kautils.h>
#include <afs/cm.h>
#include <afs/cm_config.h>
#include <afs/krb.h>
#include <afs/krb_prot.h>
#include <rx/rxkad.h>
#include <crypt.h>
#include <des.h>

int krb_add_host(struct sockaddr_in *server_list_p);
static void krb_set_port(long port);

static long
ka_AddHostProc(void *rockp, struct sockaddr_in *addrp, char *namep)
{
    return krb_add_host(addrp);
}

static char bogusReason[100];

static char *
ka_MapKerberosError(int code)
{
    switch (code) {
    case INTK_BADPW:
	return "password was incorrect";
    case KERB_ERR_PRINCIPAL_UNKNOWN:
	return "user doesn't exist";
    case KERB_ERR_SERVICE_EXP:
	return "server and client clocks are badly skewed";
    case SKDC_RETRY:
	return "Authentication Server was unavailable";
    case RD_AP_TIME:
	return "server and client clocks are badly skewed";
    default:
	sprintf(bogusReason, "unknown authentication error %d", code);
	return bogusReason;
    }
}

static int krb_get_in_tkt_ext(char *user, char *instance, char *realm,
			      char *service, char *sinstance, int life,
			      struct ktc_encryptionKey *key1,
			      struct ktc_encryptionKey *key2, char **ticketpp,
			      long *ticketLenp,
			      struct ktc_encryptionKey *outKeyp, long *kvnop,
			      long *expp, char **reasonp);


afs_int32
ka_UserAuthenticateGeneral(afs_int32 flags, char *name, char *instance,
			   char *realm, char *password, Date lifetime,
			   afs_int32 * password_expiresP, afs_int32 spare,
			   char **reasonP)
{
    return ka_UserAuthenticateGeneral2(flags, name, instance, realm, password,
				       NULL, lifetime, password_expiresP,
				       spare, reasonP);
}

afs_int32
ka_UserAuthenticateGeneral2(afs_int32 flags, char *name, char *instance,
			    char *realm, char *password, char *smbname,
			    Date lifetime, afs_int32 * password_expiresP,
			    afs_int32 spare, char **reasonP)
{
    int code;
    struct ktc_encryptionKey key1, key2;
    char *ticket = NULL;
    int ticketLen;
    struct ktc_encryptionKey sessionKey;
    long kvno;
    long expirationTime;
    char fullRealm[256];
    char upperRealm[256];
    struct servent *sp;
    int ttl;

    struct ktc_principal server;
    struct ktc_principal client;
    struct ktc_token token;

    if (instance == NULL)
	instance = "";
    if (lifetime == 0)
	lifetime = MAXKTCTICKETLIFETIME;

    code = cm_SearchCellRegistry(1, realm, fullRealm, NULL, ka_AddHostProc, NULL);
    if (code && code != CM_ERROR_FORCE_DNS_LOOKUP)
        code = cm_SearchCellFile(realm, fullRealm, ka_AddHostProc, NULL);
    if (code) {
	code =
	    cm_SearchCellByDNS(realm, fullRealm, &ttl, ka_AddHostProc, NULL);
    }
    if (code) {
	*reasonP = "specified realm is unknown";
	return (code);
    }

    strcpy(upperRealm, fullRealm);
    _strupr(upperRealm);

    /* encrypt password, both ways */
    ka_StringToKey(password, upperRealm, &key1);
    des_string_to_key(password, &key2);

    /* set port number */
    sp = getservbyname("kerberos4", "udp");
    if (!sp)
    sp = getservbyname("kerberos-iv", "udp");
    if (!sp)
        sp = getservbyname("kerberos", "udp");
    if (sp)
	krb_set_port(ntohs(sp->s_port));

    *reasonP = NULL;
    code =
	krb_get_in_tkt_ext(name, instance, upperRealm, "afs", "", lifetime,
			   &key1, &key2, &ticket, &ticketLen, &sessionKey,
			   &kvno, &expirationTime, reasonP);

    if (code && *reasonP == NULL)
	*reasonP = ka_MapKerberosError(code);

    if (code)
	return code;

    strcpy(server.name, "afs");
    strcpy(server.instance, "");
    strcpy(server.cell, fullRealm);

    /* Would like to use Vice ID's; using raw names for now. */
    strcpy(client.name, name);
    strcpy(client.instance, instance);
    strcpy(client.cell, upperRealm);
    if (smbname)
	strcpy(client.smbname, smbname);

    token.startTime = 0;	/* XXX */
    token.endTime = expirationTime;
    token.sessionKey = sessionKey;
    token.kvno = (short)kvno;
    token.ticketLen = ticketLen;
    memcpy(token.ticket, ticket, ticketLen);

    code =
	ktc_SetToken(&server, &token, &client,
		     (flags & KA_USERAUTH_AUTHENT_LOGON) ? AFS_SETTOK_LOGON :
		     0);
    if (code) {
	if (code == KTC_NOCM || code == KTC_NOCMRPC)
	    *reasonP = "AFS service may not have started";
	else if (code == KTC_RPC)
	    *reasonP = "RPC failure in AFS gateway";
	else if (code == KTC_NOCELL)
	    *reasonP = "unknown cell";
	else
	    *reasonP = "unknown error";
    }

    return code;
}

/*
 * krb_get_in_tkt()
 *
 * This code is descended from kerberos files krb_get_in_tkt.c and
 * send_to_kdc.c, and one.c.
 */

/*
 * definition of variable set to 1.
 * used in krb_conf.h to determine host byte order.
 */
static int krbONE = 1;

#define HOST_BYTE_ORDER (* (char *) &krbONE)
#define MSB_FIRST		0
#define LSB_FIRST		1

/*
 * Copyright 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 */

#include <string.h>
#include <time.h>

#include <des.h>
#include "krb.h"

#include <sys/types.h>
#include <winsock2.h>

static int swap_bytes;

/*
 * The kaserver defines these error codes *privately*. So we redefine them
 * here, with a slight name change to show that they really are kaserver
 * errors.
 */
#define KERB_KA_ERR_BAD_MSG_TYPE  99
#define KERB_KA_ERR_BAD_LIFETIME  98
#define KERB_KA_ERR_NONNULL_REALM 97
#define KERB_KA_ERR_PKT_LENGTH    96
#define KERB_KA_ERR_TEXT_LENGTH   95

static void
swap_u_int32(afs_uint32 * u)
{
    *u = *u >> 24 | (*u & 0x00ff0000) >> 8 | (*u & 0x0000ff00) << 8 | *u <<
	24;
}

static void
swap_u_int16(afs_uint16 * u)
{
    *u = *u >> 8 | *u << 8;
}

int pkt_clen(KTEXT pkt);
KTEXT pkt_cipher(KTEXT packet);

/*
 * The following routine has been hacked to make it work for two different
 * possible string-to-key algorithms. This is a minimal displacement
 * of the code.
 */

/*
 * krb_get_in_tkt() gets a ticket for a given principal to use a given
 * service and stores the returned ticket and session key for future
 * use.
 *
 * The "user", "instance", and "realm" arguments give the identity of
 * the client who will use the ticket.  The "service" and "sinstance"
 * arguments give the identity of the server that the client wishes
 * to use.  (The realm of the server is the same as the Kerberos server
 * to whom the request is sent.)  The "life" argument indicates the
 * desired lifetime of the ticket; the "key_proc" argument is a pointer
 * to the routine used for getting the client's private key to decrypt
 * the reply from Kerberos.  The "decrypt_proc" argument is a pointer
 * to the routine used to decrypt the reply from Kerberos; and "arg"
 * is an argument to be passed on to the "key_proc" routine.
 *
 * If all goes well, krb_get_in_tkt() returns INTK_OK, otherwise it
 * returns an error code:  If an AUTH_MSG_ERR_REPLY packet is returned
 * by Kerberos, then the error code it contains is returned.  Other
 * error codes returned by this routine include INTK_PROT to indicate
 * wrong protocol version, INTK_BADPW to indicate bad password (if
 * decrypted ticket didn't make sense), INTK_ERR if the ticket was for
 * the wrong server or the ticket store couldn't be initialized.
 *
 * The format of the message sent to Kerberos is as follows:
 *
 * Size			Variable		Field
 * ----			--------		-----
 *
 * 1 byte		KRB_PROT_VERSION	protocol version number
 * 1 byte		AUTH_MSG_KDC_REQUEST |	message type
 *			HOST_BYTE_ORDER		local byte order in lsb
 * string		user			client's name
 * string		instance		client's instance
 * string		realm			client's realm
 * 4 bytes		tlocal.tv_sec		timestamp in seconds
 * 1 byte		life			desired lifetime
 * string		service			service's name
 * string		sinstance		service's instance
 */

/*
 * Check_response is a support routine for krb_get_in_tkt.
 *
 * Check the response with the supplied key. If the key is apparently
 * wrong, return INTK_BADPW, otherwise zero.
 */
static
check_response(KTEXT rpkt, KTEXT cip, char *service, char *instance,
	       char *realm, struct ktc_encryptionKey *key)
{
    Key_schedule key_s;
    char *ptr;
    char s_service[SNAME_SZ];
    char s_instance[INST_SZ];
    char s_realm[REALM_SZ];
    int ticket_len;

    if (!key)
	return -1;

    /* copy information from return packet into "cip" */
    cip->length = pkt_clen(rpkt);
    memcpy((char *)(cip->dat), (char *)pkt_cipher(rpkt), cip->length);

    /* decrypt ticket */
    key_sched((char *)key, key_s);
    pcbc_encrypt((C_Block *) cip->dat, (C_Block *) cip->dat,
		 (long)cip->length, key_s, (des_cblock *) key, 0);

    /* Skip session key */
    ptr = (char *)cip->dat + 8;

    /* Check and extract server's name */
    if ((strlen(ptr) + (ptr - (char *)cip->dat)) > cip->length) {
	return (INTK_BADPW);
    }

    (void)strncpy(s_service, ptr, sizeof(s_service) - 1);
    s_service[sizeof(s_service) - 1] = '\0';
    ptr += strlen(s_service) + 1;

    /* Check and extract server's instance */
    if ((strlen(ptr) + (ptr - (char *)cip->dat)) > cip->length) {
	return (INTK_BADPW);
    }

    (void)strncpy(s_instance, ptr, sizeof(s_instance) - 1);
    s_instance[sizeof(s_instance) - 1] = '\0';
    ptr += strlen(s_instance) + 1;

    /* Check and extract server's realm */
    if ((strlen(ptr) + (ptr - (char *)cip->dat)) > cip->length) {
	return (INTK_BADPW);
    }

    (void)strncpy(s_realm, ptr, sizeof(s_realm));
    s_realm[sizeof(s_realm) - 1] = '\0';
    ptr += strlen(s_realm) + 1;

    /* Ignore ticket lifetime, server key version */
    ptr += 2;

    /* Extract and check ticket length */
    ticket_len = (unsigned char)*ptr++;

    if ((ticket_len < 0)
	|| ((ticket_len + (ptr - (char *)cip->dat)) > (int)cip->length)) {
	return (INTK_BADPW);
    }

    /* Check returned server name, instance, and realm fields */
    /*
     * 7/23/98 - Deleting realm check.  This allows cell name to differ
     * from realm name.
     */
#ifdef REALMCHECK
    if (strcmp(s_service, service) || strcmp(s_instance, instance)
	|| strcmp(s_realm, realm)) {
#else
    if (strcmp(s_service, service) || strcmp(s_instance, instance)) {
#endif
	/* not what we asked for: assume decryption failed */
	return (INTK_BADPW);
    }

    return 0;
}

/*
 * The old kaserver (pre 3.4) returned zero error codes sometimes, leaving
 * the kaserver error code in a string in the text of the error message.
 * The new one does the same, but returns KDC_GEN_ERR rather than zero.
 * We try to extract the actual error code.
 */
static char bogus_kaerror[100];
static int
kaserver_map_error_code(int code, char *etext, char **reasonP)
{
    if (code == 0 || code == KDC_GEN_ERR) {
	int mapcode;
	if (sscanf(etext, "code =%u: ", &mapcode) == 1) {
	    code = mapcode;
	    strcpy(bogus_kaerror, etext);
	    *reasonP = bogus_kaerror;
	}
    }

    if (code == 0) {
	code = KDC_GEN_ERR;
    }

    return code;
}

static int
krb_get_in_tkt_ext(user, instance, realm, service, sinstance, life, key1,
		   key2, ticketpp, ticketLenp, outKeyp, kvnop, expp, reasonp)
     char *user;
     char *instance;
     char *realm;
     char *service;
     char *sinstance;
     int life;
     struct ktc_encryptionKey *key1, *key2;
     char **ticketpp;
     long *ticketLenp;
     struct ktc_encryptionKey *outKeyp;
     long *kvnop;
     long *expp;
     char **reasonp;
{
    KTEXT_ST pkt_st;
    KTEXT pkt = &pkt_st;	/* Packet to KDC */
    KTEXT_ST rpkt_st;
    KTEXT rpkt = &rpkt_st;	/* Returned packet */
    KTEXT_ST cip_st;
    KTEXT cip = &cip_st;	/* Returned Ciphertext */
    KTEXT_ST tkt_st;
    KTEXT tkt = &tkt_st;	/* Current ticket */
    C_Block ses;		/* Session key for tkt */
    int kvno;			/* Kvno for session key */
    unsigned char *v = pkt->dat;	/* Prot vers no */
    unsigned char *t = (pkt->dat + 1);	/* Prot msg type */

    char s_name[SNAME_SZ];
    char s_instance[INST_SZ];
    char rlm[REALM_SZ];
    int lifetime;
    char kerberos_life;
    int msg_byte_order;
    int kerror;
    char *ptr;

    unsigned long t_local;

    afs_uint32 rep_err_code;
    afs_uint32 exp_date;
    afs_uint32 kdc_time;

    /* BUILD REQUEST PACKET */

    /* Set up the fixed part of the packet */
    *v = (unsigned char)KRB_PROT_VERSION;
    *t = (unsigned char)AUTH_MSG_KDC_REQUEST;
    *t |= HOST_BYTE_ORDER;

    /* Now for the variable info */
    (void)strcpy((char *)(pkt->dat + 2), user);	/* aname */
    pkt->length = 3 + strlen(user);
    (void)strcpy((char *)(pkt->dat + pkt->length), instance);	/* instance */
    pkt->length += 1 + strlen(instance);
    (void)strcpy((char *)(pkt->dat + pkt->length), realm);	/* realm */
    pkt->length += 1 + strlen(realm);

#ifndef WIN32
    (void)gettimeofday(&t_local, NULL);
#else /* WIN32 */
    t_local = time((void *)0);
#endif /* WIN32 */
    /* timestamp */
    memcpy((char *)(pkt->dat + pkt->length), (char *)&(t_local), 4);
    pkt->length += 4;

    if (life == 0) {
	kerberos_life = DEFAULT_TKT_LIFE;
    } else {
	kerberos_life = time_to_life(0, life);
	if (kerberos_life == 0) {
	    kerberos_life = DEFAULT_TKT_LIFE;
	}
    }

    *(pkt->dat + (pkt->length)++) = kerberos_life;
    (void)strcpy((char *)(pkt->dat + pkt->length), service);
    pkt->length += 1 + strlen(service);
    (void)strcpy((char *)(pkt->dat + pkt->length), sinstance);

    pkt->length += 1 + strlen(sinstance);

    rpkt->length = 0;

    /* SEND THE REQUEST AND RECEIVE THE RETURN PACKET */

    if (kerror = send_to_kdc(pkt, rpkt)) {
	return (kerror);
    }

    /* check packet version of the returned packet */
    if (pkt_version(rpkt) != KRB_PROT_VERSION)
	return (INTK_PROT);

    /* Check byte order */
    msg_byte_order = pkt_msg_type(rpkt) & 1;
    swap_bytes = 0;
    if (msg_byte_order != HOST_BYTE_ORDER) {
	swap_bytes++;
    }

    switch (pkt_msg_type(rpkt) & ~1) {
    case AUTH_MSG_KDC_REPLY:
	break;
    case AUTH_MSG_ERR_REPLY:
	memcpy((char *)&rep_err_code, pkt_err_code(rpkt), 4);
	if (swap_bytes)
	    swap_u_int32(&rep_err_code);
	/* kaservers return bogus error codes in different ways, so map it
	 * from the error text if this is the case */
	return kaserver_map_error_code(rep_err_code, pkt_err_text(rpkt),
				       reasonp);

    default:
	return (INTK_PROT);
    }

    /* get the principal's expiration date */
    memcpy((char *)&exp_date, pkt_x_date(rpkt), sizeof(exp_date));
    if (swap_bytes)
	swap_u_int32(&exp_date);

    /* Extract length. This will be re-extracted in check_response, below */
    cip->length = pkt_clen(rpkt);

    /* Length of zero seems to correspond to no principal (with kaserver) */
    if (cip->length == 0) {
	return (KERB_ERR_PRINCIPAL_UNKNOWN);
    }

    if ((cip->length < 0) || (cip->length > sizeof(cip->dat))) {
	return (INTK_ERR);	/* no appropriate error code
				 * currently defined for INTK_ */
    }

    /*
     * Check the response against both possible keys, and use the one
     * that works.
     */
    if (check_response(rpkt, cip, service, sinstance, realm, key1)
	&& check_response(rpkt, cip, service, sinstance, realm, key2)) {
	return INTK_BADPW;
    }

    /*
     * EXTRACT INFORMATION FROM RETURN PACKET
     *
     * Some of the fields, below are already checked for integrity by
     * check_response.
     */
    ptr = (char *)cip->dat;

    /* extract session key */
    memcpy((char *)ses, ptr, 8);
    ptr += 8;

    /* extract server's name */
    (void)strncpy(s_name, ptr, sizeof(s_name) - 1);
    s_name[sizeof(s_name) - 1] = '\0';
    ptr += strlen(s_name) + 1;

    /* extract server's instance */
    (void)strncpy(s_instance, ptr, sizeof(s_instance) - 1);
    s_instance[sizeof(s_instance) - 1] = '\0';
    ptr += strlen(s_instance) + 1;

    /* extract server's realm */
    (void)strncpy(rlm, ptr, sizeof(rlm));
    rlm[sizeof(rlm) - 1] = '\0';
    ptr += strlen(rlm) + 1;

    /* extract ticket lifetime, server key version, ticket length */
    /* be sure to avoid sign extension on lifetime! */
    lifetime = (unsigned char)ptr[0];
    kvno = (unsigned char)ptr[1];
    tkt->length = (unsigned char)ptr[2];
    ptr += 3;

    /* extract ticket itself */
    memcpy((char *)(tkt->dat), ptr, tkt->length);
    ptr += tkt->length;

    /* check KDC time stamp */
    memcpy((char *)&kdc_time, ptr, 4);	/* Time (coarse) */
    if (swap_bytes)
	swap_u_int32(&kdc_time);

    ptr += 4;

    t_local = time((void *)0);
    if (abs((int)(t_local - kdc_time)) > CLOCK_SKEW) {
	return (RD_AP_TIME);	/* XXX should probably be better
				 * code */
    }

    /* copy out results; if *ticketpp is non-null, the caller has already
     * allocated the buffer for us.
     */
    memcpy(outKeyp, ses, sizeof(struct ktc_encryptionKey));
    if (*ticketpp == NULL) {
	*ticketpp = malloc(tkt->length);
    } else if (tkt->length > (unsigned long)*ticketLenp)
	return -1;
    *ticketLenp = tkt->length;
    memcpy(*ticketpp, tkt->dat, tkt->length);
    *kvnop = kvno;
    if (expp)
	*expp = life_to_time(kdc_time, (char)lifetime);

    return (INTK_OK);		/* this is zero */
}

/*
 *
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 */

#define S_AD_SZ sizeof(struct sockaddr_in)

static int krb_debug;

/* CLIENT_KRB_TIMEOUT indicates the time to wait before
 * retrying a server.  It's defined in "krb.h".
 */
static struct timeval timeout = { CLIENT_KRB_TIMEOUT, 0 };
static char *prog = "dm";
static send_recv();

/*
 * This file contains two routines, send_to_kdc() and send_recv().
 * send_recv() is a static routine used by send_to_kdc().
 */

/*
 * send_to_kdc() sends a message to the Kerberos authentication
 * server(s) in the given realm and returns the reply message.
 * The "pkt" argument points to the message to be sent to Kerberos;
 * the "rpkt" argument will be filled in with Kerberos' reply.
 * The "realm" argument indicates the realm of the Kerberos server(s)
 * to transact with.  If the realm is null, the local realm is used.
 *
 * If more than one Kerberos server is known for a given realm,
 * different servers will be queried until one of them replies.
 * Several attempts (retries) are made for each server before
 * giving up entirely.
 *
 * If an answer was received from a Kerberos host, KSUCCESS is
 * returned.  The following errors can be returned:
 *
 * SKDC_CANT    - can't get local realm
 *              - can't find "kerberos" in /etc/services database
 *              - can't open socket
 *              - can't bind socket
 *              - all ports in use
 *              - couldn't find any Kerberos host
 *
 * SKDC_RETRY   - couldn't get an answer from any Kerberos server,
 *		  after several retries
 */

typedef struct krb_server {
    struct krb_server *nextp;
    struct sockaddr_in addr;
} krb_server_t;

static long krb_udp_port = KRB_PORT;	/* In host byte order */
static krb_server_t *krb_hosts_p = NULL;
static int krb_nhosts = 0;

static void
krb_set_port(long port)
{
    krb_udp_port = port;
}

int
krb_add_host(struct sockaddr_in *server_list_p)
{
    krb_server_t *krb_host_p;

    krb_host_p = malloc(sizeof(krb_server_t));

    /* add host to list */
    krb_host_p->nextp = krb_hosts_p;
    krb_hosts_p = krb_host_p;
    krb_nhosts++;

    /* copy in the data */
    memcpy(&krb_host_p->addr, server_list_p, sizeof(struct sockaddr_in));

    return 0;
}

static
send_to_kdc(pkt, rpkt)
     KTEXT pkt;
     KTEXT rpkt;
{
    SOCKET f;
    int retry;
    int retval;
    krb_server_t *tsp;
    struct sockaddr_in to;
    int timeAvail, timePerIter, numIters;

    memset(&to, 0, sizeof(to));
    if ((f = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	if (krb_debug)
	    fprintf(stderr, "%s: Can't open socket\n", prog);
	return (SKDC_CANT);
    }
    /* from now on, exit through rtn label for cleanup */

    /* compute # of retries */
    /* The SMB client seems to time out after 60 seconds. */
    timeAvail = 60;
    /* Leave ourselves some margin for fooling around
     * timeAvail -= 10;
     * /* How long does one iteration take? */
    timePerIter = krb_nhosts * CLIENT_KRB_TIMEOUT;
    /* How many iters? */
    numIters = timeAvail / timePerIter;
    /* No more than max */
    if (numIters > CLIENT_KRB_RETRY)
	numIters = CLIENT_KRB_RETRY;
    /* At least one */
    if (numIters < 1)
	numIters = 1;

    /* retry each host in sequence */
    for (retry = 0; retry < numIters; ++retry) {
	for (tsp = krb_hosts_p; tsp; tsp = tsp->nextp) {
	    to = tsp->addr;
	    to.sin_family = AF_INET;
	    to.sin_port = htons(((unsigned short)krb_udp_port));
	    if (send_recv(pkt, rpkt, f, &to)) {
		retval = KSUCCESS;
		goto rtn;
	    }
	}
    }

    retval = SKDC_RETRY;

  rtn:
    (void)closesocket(f);

    return (retval);
}

/*
 * try to send out and receive message.
 * return 1 on success, 0 on failure
 */

static
send_recv(pkt, rpkt, f, _to)
     KTEXT pkt;
     KTEXT rpkt;
     SOCKET f;
     struct sockaddr_in *_to;
{
    fd_set readfds;
    struct sockaddr_in from;
    int sin_size;
    int numsent;
    int code;

    if (krb_debug) {
	if (_to->sin_family == AF_INET)
	    printf("Sending message to %s...", inet_ntoa(_to->sin_addr));
	else
	    printf("Sending message...");
	(void)fflush(stdout);
    }
    if ((numsent =
	 sendto(f, (char *)(pkt->dat), pkt->length, 0, (struct sockaddr *)_to,
		S_AD_SZ)) != (int)pkt->length) {
	if (krb_debug)
	    printf("sent only %d/%d\n", numsent, pkt->length);
	return 0;
    }
    if (krb_debug) {
	printf("Sent\nWaiting for reply...");
	(void)fflush(stdout);
    }
    FD_ZERO(&readfds);
    FD_SET(f, &readfds);
    errno = 0;
    /* select - either recv is ready, or timeout */
    /* see if timeout or error or wrong descriptor */
    if (select(f + 1, &readfds, (fd_set *) 0, (fd_set *) 0, &timeout) < 1
	|| !FD_ISSET(f, &readfds)) {
	if (krb_debug) {
	    fprintf(stderr, "select failed: readfds=%p", readfds);
	    perror("");
	}
	return 0;
    }
    sin_size = sizeof(from);
    if ((code =
	 recvfrom(f, (char *)(rpkt->dat), sizeof(rpkt->dat), 0,
		  (struct sockaddr *)&from, &sin_size))
	< 0) {
	if (krb_debug)
	    perror("recvfrom");
	return 0;
    }
    if (krb_debug) {
	printf("received packet from %s\n", inet_ntoa(from.sin_addr));
	fflush(stdout);
    }
    return 1;
}

/*
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

/*
 * This routine takes a reply packet from the Kerberos ticket-granting
 * service and returns a pointer to the beginning of the ciphertext in it.
 *
 * See "krb_prot.h" for packet format.
 */

static KTEXT
pkt_cipher(KTEXT packet)
{
    unsigned char *ptr =
	pkt_a_realm(packet) + 6 + strlen((char *)pkt_a_realm(packet));
    /* Skip a few more fields */
    ptr += 3 + 4;		/* add 4 for exp_date */

    /* And return the pointer */
    return ((KTEXT) ptr);
}

/*
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

/*
 * Given a pointer to an AUTH_MSG_KDC_REPLY packet, return the length of
 * its ciphertext portion.  The external variable "swap_bytes" is assumed
 * to have been set to indicate whether or not the packet is in local
 * byte order.  pkt_clen() takes this into account when reading the
 * ciphertext length out of the packet.
 */

static int
pkt_clen(KTEXT pkt)
{
    afs_uint16 temp;

    /* Start of ticket list */
    unsigned char *ptr =
	pkt_a_realm(pkt) + 10 + strlen((char *)pkt_a_realm(pkt));

    /* Finally the length */
    memcpy((char *)&temp, (char *)(++ptr), 2);	/* alignment */
    if (swap_bytes) {
	swap_u_int16(&temp);
    }

    if (krb_debug) {
	printf("Clen is %d\n", temp);
    }

    return (temp);
}

/* This defines the Andrew string_to_key function.  It accepts a password
   string as input and converts its via a one-way encryption algorithm to a DES
   encryption key.  It is compatible with the original Andrew authentication
   service password database. */

static void
Andrew_StringToKey(str, cell, key)
     char *str;
     char *cell;		/* cell for password */
     struct ktc_encryptionKey *key;
{
    char password[8 + 1];	/* crypt's limit is 8 chars anyway */
    int i;
    int passlen;

    memset(key, 0, sizeof(struct ktc_encryptionKey));

    strncpy(password, cell, 8);
    passlen = strlen(str);
    if (passlen > 8)
	passlen = 8;

    for (i = 0; i < passlen; i++)
	password[i] ^= str[i];

    for (i = 0; i < 8; i++)
	if (password[i] == '\0')
	    password[i] = 'X';

    /* crypt only considers the first 8 characters of password but for some
     * reason returns eleven characters of result (plus the two salt chars). */
    strncpy((char *)key, (char *)crypt(password, "p1") + 2,
	    sizeof(struct ktc_encryptionKey));

    /* parity is inserted into the LSB so leftshift each byte up one bit.  This
     * allows ascii characters with a zero MSB to retain as much significance
     * as possible. */
    {
	char *keybytes = (char *)key;
	unsigned int temp;

	for (i = 0; i < 8; i++) {
	    temp = (unsigned int)keybytes[i];
	    keybytes[i] = (unsigned char)(temp << 1);
	}
    }
    des_fixup_key_parity((unsigned char *)key);
}


static void
StringToKey(str, cell, key)
     char *str;
     char *cell;		/* cell for password */
     struct ktc_encryptionKey *key;
{
    des_key_schedule schedule;
    char temp_key[8];
    char ivec[8];
    char password[BUFSIZ];
    int passlen;

    strncpy(password, str, sizeof(password));
    if ((passlen = strlen(password)) < sizeof(password) - 1)
	strncat(password, cell, sizeof(password) - passlen);
    if ((passlen = strlen(password)) > sizeof(password))
	passlen = sizeof(password);

    memcpy(ivec, "kerberos", 8);
    memcpy(temp_key, "kerberos", 8);
    des_fixup_key_parity(temp_key);
    des_key_sched(temp_key, schedule);
    des_cbc_cksum(password, ivec, passlen, schedule, ivec);

    memcpy(temp_key, ivec, 8);
    des_fixup_key_parity(temp_key);
    des_key_sched(temp_key, schedule);
    des_cbc_cksum(password, (char *)key, passlen, schedule, ivec);

    des_fixup_key_parity((char *)key);
}
