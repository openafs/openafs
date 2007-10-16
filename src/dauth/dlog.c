/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * dlog
 *
 * This program acquires a Kerberos V5 ticket, or variant thereof, from
 * the DCE KDC, using the services of an intermediary server (which happens
 * to be implemented in the AFS/DFS translator). The intermediary takes
 * care of most of the intricate details of the KRB5 authentication exchange,
 * since it has available to it the appropriate KRB5 utilities. This program
 * does have to decrypt the enciphered portion of the KDC reply to extract
 * the session key to use with the ticket, and needs to know just enough ASN.1
 * to decode the decrypted result. As a side-effect of using the AFS/DFS
 * translator as the intermediary, this program also does not have to access
 * any KRB5 location/configuration information--it just contacts the servers
 * listed in the CellServDB in the usual manner (via ubik_.
 *
 * This works as follows:
 * 
 * 1. dlog sends a GetTickets request to the intermediary.
 *
 * 2. The intermediary reformats the request as an KRB5 AS request(asking
 *    for a ticket made out to the specified principal, suitable for contacting
 *    the AFS/DFS translator principal. This is determined by the server, and
 *    is by default "afs".
 *
 * 3. Since the AS service is used directly, an appropriate ticket will
 *    be passed back immediately (there is no need to get a TGT first).
 *
 * 4. The translator decodes the response and, in the absense of an error,
 *    returns some in-the-clear information about the ticket, the Unix id
 *    of the user, the ticket itself, encrypted in the afs principal's key,
 *    and the session key and ticket valid times, all encrypted in a key
 *    derived from the user's password and a salt. The appropriate salt to
 *    append to the password is returned with the result. We also return
 *    a ticket type, which may indicate that the ticket is a standard
 *    Kerberos V5 ticket (RXKAD_TICKET_TYPE_KERBEROS_V5, defined in rxkad.h)
 *    or that it is in a private formats reserved by the translator (this
 *    allows the translator to strip of unecessary in-the-clear information
 *    in the ticket or even to re-encrypt the ticket in another format,
 *    if desired, to save space).
 *
 * 5. Finally, this program requests the user's password and attempts 
 *    decryption and decoding of the session key and related information.
 *    Included in the decrypted result is a "nonce" which was supplied
 *    by the client in the first place. If the nonce is retrieved undamaged,
 *    and if we are able to decode the result (with a very limited ASN.1
 *    decoder) then it is assumed the client's password must have been correct.
 *
 * 6. The user id, session key, ticket, ticket type, and expiration time are
 *    all stored in the cache manager.
 *
 * NOTE 1: this program acquires only a simple ticket (no DCE PAC information),
 * which is all that is required to hold a conversation with the AFS/DFS
 * translator. The AFS/DFS translator must obtain another ticket for use with
 * the DFS file exporter which *does* include complete user information (i.e.
 * a PAC). That is another story.
 *
 * NOTE 2: no authentication libraries are provided which match this program.
 * This program, itself, constitutes a usable authentication interface, and
 * may be called by another program (such as login), using the -pipe switch.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/dauth/Attic/dlog.c,v 1.9.2.2 2007/04/10 18:43:42 shadow Exp $");

#include <afs/stds.h>
#include <sys/types.h>
#include <rx/xdr.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <ubik.h>

#include <stdio.h>
#include <pwd.h>
#include <afs/com_err.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include "adkint.h"
#include "assert.h"
#include <des.h>
#include <afs/afsutil.h>

/*
 * The password reading routine in des/readpassword.c will not work if the
 * buffer size passed in is greater than BUFSIZ, so we pretty well have to
 * use that constant and *HOPE* that the BUFSIZ there is the same as the
 * BUFSIZ here.
 */
#define MAX_PASSWD_LEN BUFSIZ

/*
 * Read a null-terminated password from stdin, stop on \n or eof
 */
static char *
getpipepass()
{
    static char gpbuf[MAX_PASSWD_LEN];

    register int i, tc;
    memset(gpbuf, 0, sizeof(gpbuf));
    for (i = 0; i < (sizeof(gpbuf) - 1); i++) {
	tc = fgetc(stdin);
	if (tc == '\n' || tc == EOF)
	    break;
	gpbuf[i] = tc;
    }
    return gpbuf;
}

/*
 * The un-decoded version of the encrypted portion of the kdc
 * AS reply message (see Kerberos V5 spec).
 */
typedef struct kdc_as_reply {
    des_cblock session_key;
    afs_int32 nonce;
    afs_int32 authtime;
    afs_int32 starttime;
    afs_int32 endtime;
    char *realm;
} kdc_as_reply_t;

int CommandProc();

static int zero_argc;
static char **zero_argv;

/*
 * Store the returned credentials as an AFS "token" for the user
 * "AFS ID <user_id>".
 */
int
store_afs_token(unix_id, realm_p, tkt_type, ticket_p, ticket_len, session_key,
		starttime, endtime, set_pag)
     afs_int32 unix_id;
     char *realm_p;
     afs_int32 tkt_type;
     unsigned char *ticket_p;
     int ticket_len;
     des_cblock session_key;
     afs_int32 starttime;
     afs_int32 endtime;
     int set_pag;
{
    struct ktc_token token;
    struct ktc_principal client, server;

    token.startTime = starttime;
    token.endTime = endtime;
    memcpy((char *)&token.sessionKey, session_key, sizeof(token.sessionKey));
    token.kvno = tkt_type;
    token.ticketLen = ticket_len;
    if (ticket_len > MAXKTCTICKETLEN) {
	fprintf(stderr,
		"dlog: DCE ticket is too long (length %d). Maximum length accepted by AFS cache manager is %d\n",
		ticket_len, MAXKTCTICKETLEN);
	exit(1);
    }
    memcpy((char *)token.ticket, (char *)ticket_p, ticket_len);

    sprintf(client.name, "AFS ID %d", unix_id);
    strcpy(client.instance, "");
    strcpy(client.cell, realm_p);

    strcpy(server.name, "afs");
    strcpy(server.instance, "");
    strcpy(server.cell, realm_p);

    return (ktc_SetToken
	    (&server, &token, &client, set_pag ? AFS_SETTOK_SETPAG : 0));
}

char *
make_string(s_p, length)
     char *s_p;
     int length;
{
    char *new_p = (char *)malloc(length + 1);
    if (new_p == NULL) {
	fprintf(stderr, "dlog: out of memory\n");
	exit(1);
    }
    memcpy(new_p, s_p, length);
    new_p[length] = '\0';
    return new_p;
}

/*
 * Decode an asn.1 GeneralizedTime, turning it into a 32-bit Unix time.
 * Format is fixed at YYYYMMDDHHMMSS plus a terminating "Z".
 *
 * NOTE: A test for this procedure is included at the end of this file.
 */
int
decode_asn_time(buf, buflen, utime)
     char *buf;
     int buflen;
     afs_int32 *utime;
{
    int year, month, day, hour, mina, sec;
    int leapyear, days;
    static mdays[11] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30 };
    int m;

    if (buflen != 15
	|| sscanf(buf, "%4d%2d%2d%2d%2d%2dZ", &year, &month, &day, &hour,
		  &mina, &sec) != 6) {
	return 1;
    }
    leapyear = month > 2 ? (year + 1) : year;	/* Account for feb 29 if
						 * current year is a leap year */
    for (days = 0, m = 0; m < month - 1; m++)
	days += mdays[m];

    *utime =
	((((((year - 1970) * 365 + (leapyear - 1970 + 1) / 4 + days + day -
	     1) * 24) + hour) * 60 + mina) * 60) + sec;
    return 0;
}

/*
 * A quick and (very) dirty ASN.1 decode of the ciphertext portion
 * of the KDC AS reply message... good enough for a product with a short
 * expected lifetime (this will work as long as the message format doesn't
 * change much).
 *
 * Assumptions:
 * 
 * 1. The nonce is the only INTEGER with a tag of [2], at any nesting level.
 * 2. The session key is the only OCTET STRING with length 8 and tag [1].
 * 3. The authtime, starttime, and endtimes are the only Generalized Time
 *    strings with tags 5, 6, and 7, respectively.
 * 4. The realm is the only General String with tag 9.
 * 5. The tags, above, are presented in ascending order.
 */

#define ASN_INTEGER		0x2
#define ASN_OCTET_STRING	0x4
#define ASN_TIME		0x18
#define ASN_GENERAL_STRING	0x1b
#define KDC_REP			0x7a

int
decode_reply(buf, buflen, reply_p)
     unsigned char *buf;	/* encoded ASN.1 string */
     int buflen;		/* length of encoded string */
     kdc_as_reply_t *reply_p;	/* result */
{
    unsigned char *limit = buf + buflen;

    char saw_nonce = 0;
    char saw_kdc_rep = 0;
    char saw_session_key = 0;
    char saw_authtime = 0;
    char saw_starttime = 0;
    char saw_endtime = 0;
    char saw_realm = 0;

    int context = -1;		/* Initialize with invalid context */

    reply_p->starttime = 0;	/* This is optionally provided by kdc */

    while (buf < limit) {
	int op;
	unsigned int len;

	op = *buf++;
	len = *buf++;
	if (len & 0x80 && len != 0x80) {
	    unsigned int n = (len & 0x7f);
	    if (n > sizeof(len))
		return 1;	/* too long for us to handle */
	    len = 0;
	    while (n) {
		len = (len << 8) | *buf++;
		n--;
	    }
	}
	if ((op & 0x20) == 0) {
	    /* Primitive encoding */
	    /* Bounds check */
	    if (buf + len > limit)
		return 1;
	}

	switch (op) {
	case KDC_REP:
	    saw_kdc_rep++;
	    break;

	case ASN_INTEGER:
	    {
		/*
		 * Since non ANSI C doesn't recognize the "signed"
		 * type attribute for chars, we have to fiddle about
		 * to get sign extension (the sign bit is the top bit
		 * of the first byte).
		 */
#define		SHIFTSIGN ((sizeof(afs_int32) - 1) * 8)
		afs_int32 val;
		val = (afs_int32) (*buf++ << SHIFTSIGN) >> SHIFTSIGN;

		while (--len)
		    val = (val << 8) | *buf++;

		if (context == 2) {
		    reply_p->nonce = val;
		    saw_nonce++;
		}
	    }
	    break;

	case ASN_OCTET_STRING:
	    if (context == 1 && len == sizeof(reply_p->session_key)) {
		saw_session_key++;
		memcpy(reply_p->session_key, buf, len);
	    }
	    buf += len;
	    break;

	case ASN_GENERAL_STRING:
	    if (context == 9) {
		saw_realm = 1;
		reply_p->realm = make_string(buf, len);
		goto out;	/* Best to terminate now, rather than
				 * continue--we don't know if the entire
				 * request is padded with zeroes, and if
				 * not there is a danger of misinterpreting
				 * an op-code (since the request may well
				 * be padded somewhat, for encryption purposes)
				 * This would work much better if we really
				 * tracked constructed type boundaries.
				 */
	    }
	    buf += len;
	    break;

	case ASN_TIME:
	    switch (context) {
	    case 5:
		saw_authtime++;
		if (decode_asn_time(buf, len, &reply_p->authtime))
		    return 1;
		break;

	    case 6:
		saw_starttime++;
		if (decode_asn_time(buf, len, &reply_p->starttime))
		    return 1;
		break;

	    case 7:
		saw_endtime++;
		if (decode_asn_time(buf, len, &reply_p->endtime))
		    return 1;
		break;
	    }
	    buf += len;
	    break;

	default:
	    if ((op & 0xe0) == 0xa0) {
		/* Remember last context label */
		context = op & 0x1f;
	    } else if ((op & 0x20) == 0) {
		/* Skip primitive encodings we don't understand */
		buf += len;
	    }
	}
    }

  out:
    return !(saw_kdc_rep == 1 && saw_nonce == 1 && saw_session_key == 1
	     && saw_authtime == 1 && (saw_starttime == 1
				      || saw_starttime == 0)
	     && saw_endtime == 1 && saw_realm == 1);
}

main(argc, argv)
     int argc;
     char *argv[];
{
    struct cmd_syndesc *ts;
    afs_int32 code;
#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    zero_argc = argc;
    zero_argv = argv;

    initialize_U_error_table();
    initialize_KTC_error_table();
    initialize_ACFG_error_table();

    ts = cmd_CreateSyntax(NULL, CommandProc, 0,
			  "obtain Kerberos authentication");

#define aPRINCIPAL 0
#define aCELL 1
#define aPASSWORD 2
#define aSERVERS 3
#define aLIFETIME 4
#define aSETPAG 5
#define aPIPE 6
#define aTEST 7

    cmd_AddParm(ts, "-principal", CMD_SINGLE, CMD_OPTIONAL, "user name");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-password", CMD_SINGLE, CMD_OPTIONAL, "user's password");
    cmd_AddParm(ts, "-servers", CMD_LIST, CMD_OPTIONAL,
		"explicit list of servers");
    cmd_AddParm(ts, "-lifetime", CMD_SINGLE, CMD_OPTIONAL,
		"ticket lifetime in hh[:mm[:ss]]");
    cmd_AddParm(ts, "-setpag", CMD_FLAG, CMD_OPTIONAL,
		"Create a new setpag before authenticating");
    cmd_AddParm(ts, "-pipe", CMD_FLAG, CMD_OPTIONAL,
		"read password from stdin");

#ifdef DLOG_TEST
    cmd_AddParm(ts, "-test", CMD_FLAG, CMD_OPTIONAL, "self-test");
#endif
    code = cmd_Dispatch(argc, argv);
    exit(code);
}

CommandProc(as, arock)
     char *arock;
     struct cmd_syndesc *as;
{
    char name[MAXKTCNAMELEN];
    char realm[MAXKTCREALMLEN];

    extern ADK_GetTicket();
    afs_int32 serverList[MAXSERVERS];
    struct rx_connection *serverconns[MAXSERVERS];
    struct ubik_client *ubik_handle = 0;
    struct timeval now;		/* Current time */
    afs_int32 nonce;		/* Kerberos V5 "nonce" */
    adk_error_ptr error_p;	/* Error code from ktc intermediary */
    adk_reply_ptr reply_p;	/* Reply from ktc intermediary */
    des_cblock passwd_key;	/* des key from user password */
    des_key_schedule schedule;	/* Key schedule from key */
    kdc_as_reply_t kdcrep;	/* Our own decoded version of
				 * ciphertext portion of kdc reply */

    int code;
    int i, dosetpag;
    afs_uint32 lifetime;	/* requested ticket lifetime */
    char passwd[MAX_PASSWD_LEN];

    static char rn[] = "dlog";	/*Routine name */
    static int readpipe;	/* reading from a pipe */

    int explicit_cell = 0;	/* servers specified explicitly */
    int foundPassword = 0;	/*Not yet, anyway */

    struct afsconf_dir *cdir;	/* Open configuration structure */

    /*
     * Discard command line arguments, in case the password is on the
     * command line (to avoid it showing up from a ps command).
     */
    for (i = 1; i < zero_argc; i++)
	memset(zero_argv[i], 0, strlen(zero_argv[i]));
    zero_argc = 0;

#ifdef DLOG_TEST
    /*;
     * Do a small self test if asked.
     */
    if (as->parms[aTEST].items) {
	exit(self_test());
    }
#endif

    /*
     * Determine if we should also do a setpag based on -setpag switch.
     */
    dosetpag = (as->parms[aSETPAG].items ? 1 : 0);

    /*
     * If reading the password from a pipe, don't prompt for it.
     */
    readpipe = (as->parms[aPIPE].items ? 1 : 0);

    cdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (!cdir) {
	fprintf(stderr,
		"dlog: unable to read or open AFS client configuration file\n");
	exit(1);
    }

    if (as->parms[aCELL].items) {
	strncpy(realm, as->parms[aCELL].items->data, sizeof(realm) - 1);
	realm[sizeof(realm) - 1] = '\0';
	explicit_cell = 1;
    } else {
	afsconf_GetLocalCell(cdir, realm, sizeof(realm));
    }

    if (as->parms[aSERVERS].items) {
	/*
	 * Explicit server list. Note that if servers are specified,
	 * we don't bother trying to look up cell information, but just
	 * use the specified cell name, which must be fully specified
	 * *or* take it out of the ticket if not specified.
	 */
	int i;
	struct cmd_item *ip;
	char *ap[MAXSERVERS + 2];

	for (ip = as->parms[aSERVERS].items, i = 2; ip; ip = ip->next, i++)
	    ap[i] = ip->data;
	ap[0] = "";
	ap[1] = "-servers";
	code = ubik_ParseClientList(i, ap, serverList);
	if (code) {
	    afs_com_err(rn, code, "-- could not parse server list");
	    exit(1);
	}
    } else {
	int i;
	struct afsconf_cell cellinfo;	/* Info for specified cell */

	/*
	 * Resolve full name of cell and get server list.
	 */
	code = afsconf_GetCellInfo(cdir, realm, 0, &cellinfo);
	if (code) {
	    afs_com_err(rn, code, "-- unable to get cell info");
	    exit(1);
	}
	strncpy(realm, cellinfo.name, sizeof(realm) - 1);
	realm[sizeof(realm) - 1] = '\0';
	for (i = 0; i < cellinfo.numServers && i < MAXSERVERS; i++) {
	    serverList[i] = cellinfo.hostAddr[i].sin_addr.s_addr;
	}
	if (i < MAXSERVERS)
	    serverList[i] = 0;
    }

    if (as->parms[aPRINCIPAL].items) {
	strncpy(name, as->parms[aPRINCIPAL].items->data, sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
    } else {
	/* No explicit name provided: use Unix uid to get a name */
	struct passwd *pw;
	pw = getpwuid(getuid());
	if (pw == 0) {
	    fprintf(stderr, "Can't determine your name from your user id.\n");
	    fprintf(stderr, "Try providing a principal name.\n");
	    exit(1);
	}
	strncpy(name, pw->pw_name, sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
    }

    if (as->parms[aPASSWORD].items) {
	/*
	 * Current argument is the desired password string.  Remember it in
	 * our local buffer, and zero out the argument string - anyone can
	 * see it there with ps!
	 */
	foundPassword = 1;
	strncpy(passwd, as->parms[aPASSWORD].items->data, sizeof(passwd) - 1);
	passwd[sizeof(passwd) - 1] = '\0';
	memset(as->parms[aPASSWORD].items->data, 0,
	       strlen(as->parms[aPASSWORD].items->data));
    }

    if (as->parms[aLIFETIME].items) {
	char *life = as->parms[aLIFETIME].items->data;
	char *sp;		/* string ptr to rest of life */
	lifetime = 3600 * strtol(life, &sp, 0);	/* hours */
	if (sp == life) {
	  bad_lifetime:
	    fprintf(stderr, "%s: translating '%s' to lifetime\n", rn, life);
	    exit(1);
	}
	if (*sp == ':') {
	    life = sp + 1;	/* skip the colon */
	    lifetime += 60 * strtol(life, &sp, 0);	/* minutes */
	    if (sp == life)
		goto bad_lifetime;
	    if (*sp == ':') {
		life = sp + 1;
		lifetime += strtol(life, &sp, 0);	/* seconds */
		if (sp == life)
		    goto bad_lifetime;
		if (*sp)
		    goto bad_lifetime;
	    } else if (*sp)
		goto bad_lifetime;
	} else if (*sp)
	    goto bad_lifetime;
    } else
	lifetime = 0;

    /*
     * Make connections to all the servers.
     */
    rx_Init(0);
    for (i = 0; i < MAXSERVERS; i++) {
	if (!serverList[i]) {
	    serverconns[i] = 0;
	    break;
	}
	serverconns[i] =
	    rx_NewConnection(serverList[i], htons(ADK_PORT), ADK_SERVICE,
			     rxnull_NewClientSecurityObject(), 0);
    }

    /*
     * Initialize ubik client gizmo to randomize the calls for us.
     */
    ubik_ClientInit(serverconns, &ubik_handle);

    /*
     * Come up with an acceptable nonce. V5 doc says that we just need
     * time of day. Actually, for this app, I don't think anything
     * is really needed. Better safe than sorry (although I wonder if
     * it might have been better to encode the AFS ID in the nonce
     * reply field--that's the one field that the intermediate server
     * has total control over, and which can be securely transmitted
     * back to the client).
     */
    gettimeofday(&now, 0);
    nonce = now.tv_sec;

    /*
     * Ask our agent to get us a Kerberos V5 ticket.
     */
    reply_p = (adk_reply_ptr) 0;
    error_p = (adk_error_ptr) 0;
    code = ubik_ADK_GetTicket(ubik_handle, 0,	/* Ubik flags */
		     name,	/* IN:  Principal: must be exact DCE principal */
		     nonce,	/* IN:  Input nonce */
		     lifetime,	/* IN:  lifetime */
		     &error_p,	/* OUT: Error, if any */
		     &reply_p);	/* OUT: KTC reply, if no error */

    /*
     * Destroy Rx connections on the off-chance this will allow less state
     * to be preserved at the server.
     */
    ubik_ClientDestroy(ubik_handle);

    /*
     * Finalize Rx. This may allow connections at the server to wind down
     * faster.
     */
    rx_Finalize();

    /*
     * Check for simple communication failures.
     */
    if (code) {
	afs_com_err(rn, code, "-- failed to contact authentication service");
	exit(1);
    }

    /*
     * Also check for DCE errors, which are interpreted for us by
     * the translator.
     */
    if (error_p && error_p->code) {
	fprintf(stderr, "dlog: %s\n", error_p->data);
	exit(1);
    }

    /*
     * Make sure the reply was filled in.
     */
    if (!reply_p) {
	fprintf(stderr,
		"dlog: unexpected error in server response; aborted\n");
	exit(1);
    }

    /*
     * Get the password if it wasn't provided.
     */
    if (!foundPassword) {
	if (readpipe) {
	    strcpy(passwd, getpipepass());
	} else {
	    code = des_read_pw_string(passwd, sizeof(passwd), "Password:", 0);
	    if (code) {
		afs_com_err(rn, code, "-- couldn't read password");
		exit(1);
	    }
	}
    }

    /*
     * Convert the password into the appropriate key block, given
     * the salt passed back from the ADK_GetTicket call, above. Destroy
     * the password.
     */
    if (strlen(passwd) + strlen(reply_p->salt) + 1 > sizeof(passwd)) {
	fprintf(stderr, "dlog: unexpectedly long passwd/salt combination");
	exit(1);
    }
    strcat(passwd, reply_p->salt);
    des_string_to_key(passwd, passwd_key);
    memset(passwd, 0, strlen(passwd));

    /*
     * Decrypt the private data returned by the DCE KDC, and forwarded
     * to us by the translator.
     */
    code = des_key_sched(passwd_key, schedule);
    if (!code) {
	code =
	    des_cbc_encrypt(reply_p->private.adk_code_val,
			    reply_p->private.adk_code_val,
			    reply_p->private.adk_code_len, schedule,
			    passwd_key, DECRYPT);
    }
    if (code) {
	afs_com_err(rn, code, "-- unable to decrypt reply from the DCE KDC");
	exit(1);
    }

    /*
     * Destroy the key block: it's no longer needed.
     */
    memset(schedule, 0, sizeof(schedule));
    memset(passwd_key, 0, sizeof(passwd_key));

    /*
     * Do a very quick and dirty ASN.1 decode of the relevant parts
     * of the private data.
     *
     * The decrypted data contains a 12-byte header (confounder and CRC-32
     * checksum). We choose to ignore this.
     */
    code = decode_reply(reply_p->private.adk_code_val + 12,	/* Skip header */
			reply_p->private.adk_code_len - 12,	/* ditto */
			&kdcrep);

    if (code || kdcrep.nonce != nonce) {
	fprintf(stderr,
		"dlog: DCE authentication failed -- your password is probably incorrect\n");
	exit(1);
    }

    /*
     * If the cell was not explicitly specified, then we hope that the local
     * name for the cell is the same as the one in the ticket.
     * If not, we should get an error when we store it, so the user will see
     * the errant name at that time.
     */
    if (!explicit_cell)
	strcpy(realm, kdcrep.realm);

    /*
     * Make an AFS token out of the ticket and session key, and install it
     * in the cache manager.
     */
    code =
	store_afs_token(reply_p->unix_id, realm, reply_p->tktype,
			reply_p->ticket.adk_code_val,
			reply_p->ticket.adk_code_len, kdcrep.session_key,
			kdcrep.starttime ? kdcrep.starttime : kdcrep.authtime,
			kdcrep.endtime, dosetpag);

    if (code) {
	afs_com_err("dlog", code, "-- failed to store tickets");
	exit(1);
    }

    return 0;
}

#ifdef DLOG_TEST
/*
 * Check the ASN.1 generalized time conversion routine, which assumes
 * the restricted format defined in the Kerberos V5 document.
 */

/*
 * The times in this array were checked independently with the following perl
 * script:
 *
 *   ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = gmtime(shift);
 *   $mon++; $year += 1900;
 *   printf("%04d%02d%02d%02d%02d%02dZ\n", $year, $mon, $mday, $hour, $min, $sec);
 */
struct test_times {
    char *generalized_time_p;
    afs_int32 unix_time;
} test_times[] = {
    {
    "19700101000000Z", 0}, {
    "19930101000000Z", 725846400}, {
    "19940101000000Z", 757382400}, {
    "19940201000000Z", 760060800}, {
    "19940301000000Z", 762480000}, {
    "19940401000000Z", 765158400}, {
    "19950101000000Z", 788918400}, {
    "19950201000000Z", 791596800}, {
    "19950301000000Z", 794016000}, {
    "19950401000000Z", 796694400}, {
    "19950501000000Z", 799286400}, {
    "19950601000000Z", 801964800}, {
    "19950701000000Z", 804556800}, {
    "19950801000000Z", 807235200}, {
    "19950901000000Z", 809913600}, {
    "19951001000000Z", 812505600}, {
    "19951101000000Z", 815184000}, {
    "19951201000000Z", 817776000}, {
    "19951231235959Z", 820454399}, {
    "19960101000000Z", 820454400}, {
    "19960131000000Z", 823046400}, {
    "19960131235959Z", 823132799}, {
    "19960201000000Z", 823132800}, {
    "19960229000000Z", 825552000}, {
    "19960229235959Z", 825638399}, {
    "19960301000000Z", 825638400}, {
    "19960331000000Z", 828230400}, {
    "19960331235959Z", 828316799}, {
    "19970101000000Z", 852076800}, {
    "19980101000000Z", 883612800}, {
    "19990101000000Z", 915148800}, {
    "20000101000000Z", 946684800}, {
    "20010101000000Z", 978307200}, {
    "20020101000000Z", 1009843200}, {
    "20030101000000Z", 1041379200}, {
    "20040101000000Z", 1072915200}, {
    "20050101000000Z", 1104537600}, {
"20380119031407Z", 2147483647},};

self_test()
{
    int i;
    int nerrors = 0;

    for (i = 0; i < sizeof(test_times) / sizeof(test_times[0]); i++) {
	struct test_times *t_p = &test_times[i];
	afs_int32 status;
	afs_int32 unix_time;

	status =
	    decode_asn_time(t_p->generalized_time_p,
			    strlen(t_p->generalized_time_p), &unix_time);
	if (status) {
	    printf("dlog: decode of ASN.1 time %s failed\n",
		   t_p->generalized_time_p);
	    nerrors++;
	} else if (t_p->unix_time != unix_time) {
	    printf
		("dlog: ASN.1 time %s converted incorrectly to %lu (should be %lu)\n",
		 t_p->generalized_time_p, unix_time, t_p->unix_time);
	}
    }

    if (nerrors) {
	fprintf(stderr, "dlog: self test failed\n");
	return 1;
    }
    fprintf(stderr, "dlog: self test OK\n");
    return 0;
}
#endif
