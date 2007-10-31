/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements the weblog binary which links with the AFS libraries and acts
 * as the server for authenticating users for apache access to AFS. Code
 * structure is based on klog.c. The communication with clients is done 
 * via pipes whose file descriptors are passed as command line arguments
 * thus making it necessary for a common parent to start this process and 
 * the processes that will communicate with it for them to inherit the 
 * pipes. Also passed as a command line argument is a Silent flag (like klog)
 * and a cache expiration flag which allows cache expiration times for
 * tokens to be set for testing purposes
 */


/* These two needed for rxgen output to work */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <rx/xdr.h>

#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#include <lock.h>
#include <ubik.h>
#include <stdio.h>
#include <pwd.h>
#include <signal.h>

#include <afs/com_err.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>

#include "weblog_errors.h"

#ifndef MAX
#define MAX(A,B)		((A)>(B)?(A):(B))
#endif /* !MAX */
#ifndef MIN
#define MIN(A,B)		((A)<(B)?(A):(B))
#endif /* !MAX */

#include "apache_afs_utils.h"

#define MAXBUFF 1024

/* For caching */
#include "apache_afs_cache.h"

/* the actual function that does all the work! */
int CommandProc();

static int zero_argc;
static char **zero_argv;

/* pipes used for communicating with web server */
/* these are passed as command line args - defaults to stdin/stdout */

static int readPipe;
static int writePipe;

/* 
 * now I know why this was necessary! - it's a hokie thing - 
 * the call to ka_UserAuthenticateGeneral doesn't compile otherwise
 */
int
osi_audit()
{
    return 0;
}


main(int argc, char **argv)
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
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif

/* 
 * we ignore SIGPIPE so that EPIPE is returned if there is no one reading
 * data being written to the pipe 
 */

#ifdef AIX
    /* TODO - for AIX? */
#else
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
#endif

    zero_argc = argc;
    zero_argv = argv;

    ts = cmd_CreateSyntax(NULL, CommandProc, NULL,
			  "obtain Kerberos authentication for web servers");

/* define the command line arguments */
#define aREADPIPE 0
#define aWRITEPIPE 1
#define aCACHEEXPIRATION 2
#define aTOKENEXPIRATION 3
#define aSILENT 4

    cmd_AddParm(ts, "-readPipe", CMD_SINGLE, CMD_OPTIONAL, "inPipefd");
    cmd_AddParm(ts, "-writePipe", CMD_SINGLE, CMD_OPTIONAL, "outPipefd");
    cmd_AddParm(ts, "-cacheExpiration", CMD_SINGLE, CMD_OPTIONAL,
		"local cache expiration times for tokens");
    cmd_AddParm(ts, "-tokenExpiration", CMD_SINGLE, CMD_OPTIONAL,
		"cache manager expiration time for tokens");
    cmd_AddParm(ts, "-silent", CMD_FLAG, CMD_OPTIONAL, "silent operation");

    code = cmd_Dispatch(argc, argv);

    WEBLOGEXIT(code);
}


/*
 * send a buffer over the pipe 
 */
static int
sendToken(int len, void *buf)
{
    if (buf == NULL) {
	WEBLOGEXIT(NULLARGSERROR);
    }
    if (write(writePipe, buf, len) != len) {
#ifdef DEBUG
	perror("weblog: write to pipe error");
#endif
	return -1;
    }
    return 0;
}


/*
 * read the incoming buffer from the pipe
 */

static int
readFromClient(char *buf)
{
    int n;
    if (buf == NULL) {
	WEBLOGEXIT(NULLARGSERROR);
    }
    n = read(readPipe, buf, MAXBUFF);
    if (n < 0) {
#ifdef DEBUG
	perror("weblog: pipe read error");
#endif
	return -1;
    }
    if (n == 0) {
#ifdef DEBUG
	perror("weblog: zero bytes read from pipe");
#endif
	return -1;
    }
    return n;
}

/* 
 * copies the string spereated by the sep into retbuf and returns the position
 * from the beginning of the string that this string ends at so you can call 
 * it againword seperated by the sep character and give that value as th start 
 * parameter - used to parse incoming buffer from clients over the pipe
 */
/* 
 * NOTE - the space seperated credentials failed for passwds with spaces, thus
 * we use newline for seperators instead
 */
static int
getNullSepWord(char *buf, char sep, char *retBuf, int start)
{
    int ret = 0;
    int len = strlen(buf) - start;

    /* error checks */
    if ((buf == NULL) || (retBuf == NULL) || (start < 0)) {
	fprintf(stderr, "weblog (getWordSep):NULL args\n");
	return -1;
    }

    while ((buf[start] != sep) && (ret <= len)) {
	retBuf[ret] = buf[start];
	ret++;
	start++;
    }
    retBuf[ret] = '\0';
    return (ret + 1);
}


/* 
 * parses the NEWLINE seperated buffer giving the username, passwd and cell
 * coming over the pipe from the clients and sets the variables accordingly
 */
static int
parseBuf(char *buf, char *user, char *pass, char *cell, char *type)
{
    char *temp;
    int start = 0, ret = 0;

    if ((buf == NULL) || (user == NULL) || (pass == NULL) || (cell == NULL)
	|| (type == NULL)) {
#ifdef DEBUG
	fprintf(stderr, "afs_Authenticator:parseBuf-an arg was NULL\n");
#endif
	return -1;
    }
    if ((ret = getNullSepWord(buf, '\n', type, start)) < 0) {
	return -1;
    }
    start += ret;
    if ((ret = getNullSepWord(buf, '\n', user, start)) < 0) {
	return -1;
    }
    start += ret;
    if ((ret = getNullSepWord(buf, '\n', cell, start)) < 0) {
	return -1;
    }
    start += ret;
    if ((ret = getNullSepWord(buf, '\n', pass, start)) < 0) {
	return -1;
    }
    return 0;
}


/*
 * Discard any authentication information held in trust by the Cache Manager
 * for the calling process and all other processes in the same PAG
 */
static int
unlog()
{
    return do_pioctl(NULL, 0, NULL, 0, VIOCUNPAG, NULL, 0);
}


/* we can obtain a PAG by calling this system call */
static int
makeNewPAG()
{
    return do_setpag();
}

#ifdef ENABLE_DCE_DLOG
/*
 * Attempt to use the dlog mechanism to get a DCE-DFS ticket into the AFS cache manager
 */

#include "adkint.h"
#include "assert.h"
#include <des.h>
#include <afs/afsutil.h>

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

static char *
makeString(char *sp)
{
    int len;
    char *new_string;

    if (sp == NULL) {
	fprintf(stderr, "weblog: makeString - NULL argument\n");
	return NULL;
    }
    len = strlen(sp);
    if (len < 0) {
	fprintf(stderr, "weblog: makeString. strlen error\n");
	return NULL;
    }
    new_string = (char *)malloc(len + 1);
    if (new_string == NULL) {
	fprintf(stderr, "weblog: Out of memory - malloc failed\n");
	return NULL;
    }
    strncpy(new_string, sp, len);
    return new_string;
}

/*
 * Store the returned credentials as an AFS "token" for the user
 * "AFS ID <user_id>".
 */
static int
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
		"weblog: DCE ticket is too long (length %d)."
		"Maximum length accepted by AFS cache manager is %d\n",
		MAXKTCTICKETLEN);
	return -1;
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


static char *
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
static int
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

static int
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
	int len;

	op = *buf++;
	len = *buf++;
	if ((op & 0x20) == 0) {
	    /* Primitive encoding */
	    if (len & 0x80) {
		return 1;	/* Forget about long unspecified lengths */
	    }
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


/*
 * Attempt to obtain a DFS ticket 
 */
static int
getDFScreds(char *name, char *realm, char *passwd, afs_uint32 lifetime,
	    char **reason)
{
    extern ADK_GetTicket();
    afs_int32 serverList[MAXSERVERS];
    struct rx_connection *serverconns[MAXSERVERS];
    struct ubik_client *ubik_handle = 0;
    struct timeval now;		/* current time */
    afs_int32 nonce;		/* Kerberos V5 "nonce" */
    adk_error_ptr error_p;	/* Error code from ktc intermediary */
    adk_reply_ptr reply_p;	/* reply from ktc intermediary */
    des_cblock passwd_key;	/* des key from user password */
    des_key_schedule schedule;	/* Key schedule from key */
    kdc_as_reply_t kdcrep;	/* Our own decoded version of 
				 * ciphertext portion of kdc reply */
    int code;
    struct afsconf_dir *cdir;	/* Open configuration structure */
    int i;
    struct afsconf_cell cellinfo;	/* Info for specified cell */

    if ((name == NULL) || (realm == NULL) || (passwd == NULL)) {
	*reason = makeString("weblog: NULL Arguments to getDCEcreds");
	return -1;
    }

    cdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH);
    if (!cdir) {
	*reason =
	    makeString("weblog: unable to read or open AFS client "
		       "configuration  file");
	return -1;
    }

    /*
     * Resolve full name of cell and get server list.
     */
    code = afsconf_GetCellInfo(cdir, realm, 0, &cellinfo);
    if (code) {
	*reason = makeString("-- unable to get cell info");
	return -1;
    }

    if (strcmp(realm, cellinfo.name)) {
	strncpy(realm, cellinfo.name, sizeof(realm) - 1);
	realm[sizeof(realm) - 1] = '\0';
    }

    for (i = 0; i < cellinfo.numServers && i < MAXSERVERS; i++) {
	serverList[i] = cellinfo.hostAddr[i].sin_addr.s_addr;
    }
    if (i < MAXSERVERS)
	serverList[i] = 0;

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
	*reason = makeString("-- failed to contact authentication service");
	return -1;
    }

    /*
     * Also check for DCE errors, which are interpreted for us by
     * the translator.
     */
    if (error_p && error_p->code) {
	*reason = (char *)makeString(error_p->data);
	fprintf(stderr, "weblog error:error_p->data:%s\n", error_p->data);
	return -1;
    }


    /*
     * Make sure the reply was filled in.
     */
    if (!reply_p) {
	*reason = (char *)
	    makeString
	    ("weblog: unexpected error in server response; aborted");
	return -1;
    }


    /*
     * Convert the password into the appropriate key block, given
     * the salt passed back from the ADK_GetTicket call, above. Destroy
     * the password.
     */
    if (strlen(passwd) + strlen(reply_p->salt) + 1 > BUFSIZ) {
	*reason = (char *)
	    makeString("weblog: unexpectedly long passwd/salt combination");
	return -1;
    }
    strcat(passwd, reply_p->salt);
    des_string_to_key(passwd, passwd_key);

    /* Destroy the password. */
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
	*reason =
	    (char *)makeString("-- unable to decrypt reply from the DCE KDC");
	return -1;
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
	*reason =
	    (char *)makeString("weblog: DCE authentication failed -- "
			       "password is probably incorrect");
	return -1;
    }


    /*
     * Make an AFS token out of the ticket and session key, and install it
     * in the cache manager.
     */
    code =
	store_afs_token(reply_p->unix_id, realm, reply_p->tktype,
			reply_p->ticket.adk_code_val,
			reply_p->ticket.adk_code_len, kdcrep.session_key,
			kdcrep.starttime ? kdcrep.starttime : kdcrep.authtime,
			kdcrep.endtime, 0);

    if (code) {
	*reason = (char *)
	    makeString("weblog -- getDCEcreds:failed to store tickets");
	return -1;
    }
    return 0;
}
#endif /* ENABLE_DCE_DLOG */


/*
 * The main procedure that waits in an infinite loop for data to
 * arrive through a pipe from the httpds, authenticates the user and
 * returns a token (or a failure message) over the pipe
 */
static int
CommandProc(struct cmd_syndesc *as, void *arock)
{
    char name[MAXKTCNAMELEN];
    char cell[MAXKTCREALMLEN];
    char passwd[BUFSIZ];
/* All the constant sizes for these arrays are taken from the code for klog */

    char cksum[SHA_HASH_BYTES];	/* for sha checksum for caching */
    afs_int32 expires = 0;	/* for cache expiration */
    afs_int32 cacheExpiration = 0;	/* configurable cmd line parameter */
    afs_int32 testExpires = 0;	/* cacheExpiration + current time */

    int authtype = 0;		/* AFS or AFS-DFS Authentication */
    int code;
    int shutdown = 0;		/* on getting shutdown from the pipe we GO */
    int gotToken = 0;		/* did we get a token from the cache manager */
    afs_int32 i = 0;		/* for getting primary token held by CM */
    int dosetpag = 1;		/* not used */
    Date lifetime;		/* requested ticket lifetime */
    char tbuffer[MAXBUFF];	/* for pioctl ops + pipe transfers */
    static char rn[] = "weblog";	/* Routine name */
    static int Silent = 0;	/* Don't want error messages */
    afs_int32 password_expires = -1;
    char *reason;		/* string describing errors */
    char type[10];		/* authentication type AFS or DFS */

    /* blow away command line arguments */
    for (i = 1; i < zero_argc; i++)
	memset(zero_argv[i], 0, strlen(zero_argv[i]));
    zero_argc = 0;

    /* first determine quiet flag based on -silent switch */
    Silent = (as->parms[aSILENT].items ? 1 : 0);

    code = ka_Init(0);
    if (code) {
	if (!Silent) {
	    fprintf(stderr, "%s:ka_Init FAILED\n", rn);
	}
	WEBLOGEXIT(KAERROR);
    }

    /* Parse our arguments. */
    if (as->parms[aREADPIPE].items)
	/* there is a file descriptor instead of stdin */
	readPipe = atoi(as->parms[aREADPIPE].items->data);
    else
	readPipe = stdin;

    if (as->parms[aWRITEPIPE].items)
	/* there is a file descriptor instead of stdout */
	writePipe = atoi(as->parms[aWRITEPIPE].items->data);
    else
	writePipe = stdout;

    if (as->parms[aCACHEEXPIRATION].items)
	/* set configurable cache expiration time */
	cacheExpiration = atoi(as->parms[aCACHEEXPIRATION].items->data);

    if (as->parms[aTOKENEXPIRATION].items)
	/* set configurable token lifetime */
	lifetime = atoi(as->parms[aTOKENEXPIRATION].items->data);

    /*
     * Initialize the cache for tokens
     */
    token_cache_init();

    /* 
     * discard any tokens held for this PAG - 
     * should we create a seperate PAG for weblog first? makeNewPAG does that
     */
#ifdef DEBUG
    fprintf(stderr, "%s:Before MAKENEWPAG\n", rn);
    printGroups();
    fprintf(stderr, "\nWEBLOG: before PAG:\t");
    printPAG();
#endif
    if (makeNewPAG()) {
	fprintf(stderr, "WEBLOG: MakeNewPAG failed\n");
    }
#ifdef DEBUG
    fprintf(stderr, "weblog:AFTER do_setpag,PAG:\t");
    printPAG();
    fprintf(stderr, "%s:After MAKENEWPAG\n", rn);
    printGroups();
#endif

    if (unlog()) {
#ifdef DEBUG
	fprintf(stderr, "WEBLOG: UNLOG FAILED\n");
#endif
    }

    while (!shutdown) {
	gotToken = 0;
	code = readFromClient(tbuffer);
	if (code > 0) {
	    tbuffer[code] = '\0';
	    code = parseBuf(tbuffer, name, passwd, cell, type);
	    if (code) {
		fprintf(stderr, "weblog: parseBuf FAILED\n");
		WEBLOGEXIT(PARSEERROR);
	    }
	} else {
#ifdef DEBUG
	    fprintf(stderr, "%s: readFromClient FAILED:%d...exiting\n", rn,
		    code);
#endif
	    WEBLOGEXIT(PIPEREADERROR);
	}

	if (strcasecmp(type, "AFS") == 0) {
	    authtype = 1;
	}
#ifdef ENABLE_DCE_DLOG
	else if (strcasecmp(type, "AFS-DFS") == 0) {
	    authtype = 2;
	}
#endif /* ENABLE_DCE_DLOG */
	else {
	    authtype = 0;
	}

	if (!authtype) {
	    reason = (char *)malloc(sizeof(tbuffer));
	    sprintf(reason,
		    "weblog: Unknown Authentication type:%s. AFS-DFS login "
		    "may not be enabled - check compile flags for ENABLE_DCE_DLOG",
		    type);
	    goto reply_failure;
	}

	memset((void *)&tbuffer, 0, sizeof(tbuffer));

	/* look up local cache */
	weblog_login_checksum(name, cell, passwd, cksum);
	code = weblog_login_lookup(name, cell, cksum, &tbuffer[0]);

	if (!code) {		/* local cache lookup failed */
	    /* authenticate user */

#ifdef DEBUG
	    fprintf(stderr, "WEBLOG GROUPCHECK BEFORE KA_AUTH\n");
	    printGroups();
#endif

	    if (authtype == 1) {
		code =
		    ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION + 0, name,
					       NULL, cell, passwd, lifetime,
					       &password_expires, 0, &reason);
	    }
#ifdef ENABLE_DCE_DLOG
	    else if (authtype == 2) {
		unlog();
		code = getDFScreds(name, cell, passwd, lifetime, &reason);
	    }
#ifdef DEBUG_DCE
	    printf("Code:%d\n", code);
	    if (code) {
		if (reason) {
		    printf("FAILURE:Reason:%s\n", reason);
		}
	    }
#endif
#endif /*  ENABLE_DCE_DLOG */

	    if (code) {
#ifdef DEBUG
		if (!Silent) {
		    fprintf(stderr,
			    "weblog:Unable to authenticate to AFS because "
			    "%s\n", reason);
		}
#endif
		goto reply_failure;
	    } else {
#ifdef DEBUG
		fprintf(stderr,
			"WEBLOG:After ka_UserAuthenticateGeneral GroupCheck\n");
		printGroups();
#endif
		/* get just the ONE token for this PAG from cache manager */
		i = 0;
		memcpy((void *)&tbuffer[0], (void *)&i, sizeof(afs_int32));
		code =
		    do_pioctl(tbuffer, sizeof(afs_int32), tbuffer,
			      sizeof(tbuffer), VIOCGETTOK, NULL, 0);
		if (code) {
		    fprintf(stderr, "weblog: failed to get token:%d\n", code);
		    strcpy(reason,
			   "FAILED TO GET TOKEN FROM CACHE MANAGER\n");
		} else {
		    gotToken = 1;

#ifdef DEBUG
		    hexDump(tbuffer, sizeof(tbuffer));
		    parseToken(tbuffer);
#endif /* _DEBUG */

		    /* put the token in local cache with the expiration date/time */
		    expires = getExpiration(tbuffer);
		    if (expires < 0) {
#ifdef DEBUG
			fprintf(stderr, "Error getting expiration time\n");
#endif
		    } else {
			weblog_login_checksum(name, cell, passwd, cksum);
			if (cacheExpiration == 0) {
			    weblog_login_store(name, cell, cksum, &tbuffer[0],
					       sizeof(tbuffer), expires);
			} else {
			    testExpires = cacheExpiration + time(NULL);
			    weblog_login_store(name, cell, cksum, &tbuffer[0],
					       sizeof(tbuffer), MIN(expires,
								    testExpires));
			}
		    }
		}
	    }
	} else {
	    /* cache lookup succesful */
#ifdef DEBUG
	    fprintf(stderr, "WEBLOG: Cache lookup succesful\n");
#endif
	    gotToken = 1;
	}

	/* prepare the reply buffer with this token */
	if (!gotToken) {

	  reply_failure:
	    /* respond with a reason why authentication failed */
	    sprintf(tbuffer, "FAILURE:%s", reason);
	}

	/* send response to client */
	code = sendToken(sizeof(tbuffer), tbuffer);
	if (code) {
#ifdef DEBUG
	    fprintf(stderr, "sendToken FAILED\n");
#endif
	    WEBLOGEXIT(PIPESENDERROR);
	}
	/* unlog after every request unconditionally */
	if (unlog()) {
#ifdef DEBUG
	    fprintf(stderr, "WEBLOG: UNLOG FAILED\n");
#endif
	}
    }				/* end - while */
    return 0;
}
