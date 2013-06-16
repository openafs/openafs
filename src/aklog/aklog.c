/*
 * $Id$
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology
 * For distribution and copying rights, see the file "mit-copyright.h"
 */
/*
 * Copyright (c) 2005, 2006
 * The Linux Box Corporation
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the Linux Box
 * Corporation is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * Linux Box Corporation is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the Linux Box Corporation as to its fitness for any
 * purpose, and without warranty by the Linux Box Corporation
 * of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the Linux Box Corporation shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <afsconfig.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <sys/param.h>
#include <sys/errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>

#include <afs/stds.h>
#define KERBEROS_APPLE_DEPRECATED(x)
#include <krb5.h>
#ifdef HAVE_COM_ERR_H
# include <com_err.h>
#elif HAVE_ET_COM_ERR_H
# include <et/com_err.h>
#elif HAVE_KRB5_COM_ERR_H
# include <krb5/com_err.h>
#else
# error No com_err.h? We need some kind of com_err.h
#endif

#ifndef HAVE_KERBEROSV_HEIM_ERR_H
#include <afs/com_err.h>
#endif

#include <afs/param.h>
#ifdef AFS_SUN5_ENV
#include <sys/ioccom.h>
#endif

/* Prevent inclusion of des.h to avoid conflicts with des types */
#define NO_DES_H_INCLUDE

#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/vice.h>
#include <afs/venus.h>
#include <afs/ptserver.h>
#include <afs/ptuser.h>
#include <afs/pterror.h>
#include <afs/dirpath.h>
#include <afs/afsutil.h>
#include <afs/akimpersonate.h>

#include "aklog.h"
#include "linked_list.h"

#ifdef HAVE_KRB5_CREDS_KEYBLOCK
#define USING_MIT 1
#endif
#ifdef HAVE_KRB5_CREDS_SESSION
#define USING_HEIMDAL 1
#endif

#define AFSKEY "afs"
#define AFSINST ""

#ifndef AFS_TRY_FULL_PRINC
#define AFS_TRY_FULL_PRINC 1
#endif /* AFS_TRY_FULL_PRINC */

#define AKLOG_TRYAGAIN -1
#define AKLOG_SUCCESS 0
#define AKLOG_USAGE 1
#define AKLOG_SOMETHINGSWRONG 2
#define AKLOG_AFS 3
#define AKLOG_KERBEROS 4
#define AKLOG_TOKEN 5
#define AKLOG_BADPATH 6
#define AKLOG_MISC 7

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MAXSYMLINKS
/* RedHat 4.x doesn't seem to define this */
#define MAXSYMLINKS	5
#endif

#define DIR '/'			/* Character that divides directories */
#define DIRSTRING "/"		/* String form of above */
#define VOLMARKER ':'		/* Character separating cellname from mntpt */
#define VOLMARKERSTRING ":"	/* String form of above */

typedef struct {
    char cell[BUFSIZ];
    char realm[REALM_SZ];
} cellinfo_t;

static krb5_ccache  _krb425_ccache = NULL;

/*
 * Why doesn't AFS provide these prototypes?
 */

extern int pioctl(char *, afs_int32, struct ViceIoctl *, afs_int32);

/*
 * Other prototypes
 */

extern char *afs_realm_of_cell(krb5_context, struct afsconf_cell *, int);
static int isdir(char *, unsigned char *);
static krb5_error_code get_credv5(krb5_context context, char *, char *,
				  char *, krb5_creds **);
static int get_user_realm(krb5_context, char **);

#define TRYAGAIN(x) (x == AKLOG_TRYAGAIN || \
		     x == KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN || \
		     x == KRB5KRB_ERR_GENERIC)

#if defined(HAVE_KRB5_PRINC_SIZE) || defined(krb5_princ_size)

#define get_princ_str(c, p, n) krb5_princ_component(c, p, n)->data
#define get_princ_len(c, p, n) krb5_princ_component(c, p, n)->length
#define second_comp(c, p) (krb5_princ_size(c, p) > 1)
#define realm_data(c, p) krb5_princ_realm(c, p)->data
#define realm_len(c, p) krb5_princ_realm(c, p)->length

#elif defined(HAVE_KRB5_PRINCIPAL_GET_COMP_STRING)

#define get_princ_str(c, p, n) krb5_principal_get_comp_string(c, p, n)
#define get_princ_len(c, p, n) strlen(krb5_principal_get_comp_string(c, p, n))
#define second_comp(c, p) (krb5_principal_get_comp_string(c, p, 1) != NULL)
#define realm_data(c, p) krb5_realm_data(krb5_principal_get_realm(c, p))
#define realm_len(c, p) krb5_realm_length(krb5_principal_get_realm(c, p))

#else
#error "Must have either krb5_princ_size or krb5_principal_get_comp_string"
#endif

/* MITKerberosShim logs but returns success */
#if !defined(HAVE_KRB5_524_CONV_PRINCIPAL) || defined(AFS_DARWIN110_ENV) || (!defined(HAVE_KRB5_524_CONVERT_CREDS) && !defined(HAVE_KRB524_CONVERT_CREDS_KDC))
#define HAVE_NO_KRB5_524
#elif !defined(HAVE_KRB5_524_CONVERT_CREDS) && defined(HAVE_KRB524_CONVERT_CREDS_KDC)
#define krb5_524_convert_creds krb524_convert_creds_kdc
#endif

#if USING_HEIMDAL
#define deref_keyblock_enctype(kb)		\
    ((kb)->keytype)

#define deref_entry_keyblock(entry)		\
    entry->keyblock

#define deref_session_key(creds)		\
    creds->session

#define deref_enc_tkt_addrs(tkt)		\
    tkt->caddr

#define deref_enc_length(enc)			\
    ((enc)->cipher.length)

#define deref_enc_data(enc)			\
    ((enc)->cipher.data)

#define krb5_free_keytab_entry_contents krb5_kt_free_entry

#else
#define deref_keyblock_enctype(kb)		\
    ((kb)->enctype)

#define deref_entry_keyblock(entry)		\
    entry->key

#define deref_session_key(creds)		\
    creds->keyblock

#define deref_enc_tkt_addrs(tkt)		\
    tkt->caddrs

#define deref_enc_length(enc)			\
    ((enc)->ciphertext.length)

#define deref_enc_data(enc)			\
    ((enc)->ciphertext.data)

#endif

#define deref_entry_enctype(entry)			\
    deref_keyblock_enctype(&deref_entry_keyblock(entry))

/*
 * Provide a replacement for strerror if we don't have it
 */

#ifndef HAVE_STRERROR
extern char *sys_errlist[];
#define strerror(x) sys_errlist[x]
#endif /* HAVE_STRERROR */

static char *progname = NULL;	/* Name of this program */
static int dflag = FALSE;	/* Give debugging information */
static int noauth = FALSE;	/* If true, don't try to get tokens */
static int zsubs = FALSE;	/* Are we keeping track of zephyr subs? */
static int hosts = FALSE;	/* Are we keeping track of hosts? */
static int noprdb = FALSE;	/* Skip resolving name to id? */
static int linked = FALSE;      /* try for both AFS nodes */
static int afssetpag = FALSE;   /* setpag for AFS */
static int force = FALSE;	/* Bash identical tokens? */
static int do524 = FALSE;	/* Should we do 524 instead of rxkad2b? */
static char *keytab = NULL;     /* keytab for akimpersonate */
static char *client = NULL;     /* client principal for akimpersonate */
static linked_list zsublist;	/* List of zephyr subscriptions */
static linked_list hostlist;	/* List of host addresses */
static linked_list authedcells;	/* List of cells already logged to */

/* A com_error bodge. The idea here is that this routine lets us lookup
 * things in the system com_err, if the AFS one just tells us the error
 * is unknown
 */

void
redirect_errors(const char *who, afs_int32 code, const char *fmt, va_list ap)
{
    if (who) {
	fputs(who, stderr);
	fputs(": ", stderr);
    }
    if (code) {
	const char *str = afs_error_message(code);
	if (strncmp(str, "unknown", strlen("unknown")) == 0) {
#ifdef HAVE_KRB5_SVC_GET_MSG
	    krb5_svc_get_msg(code,&str);
#elif defined(HAVE_ERROR_MESSAGE)
	    str = error_message(code);
#else
	    ; /* IRIX apparently has neither: use the string we have */
#endif
	}
	fputs(str, stderr);
	fputs(" ", stderr);
#ifdef HAVE_KRB5_SVC_GET_MSG
	krb5_free_string(str);
#endif
    }
    if (fmt) {
	vfprintf(stderr, fmt, ap);
    }
    putc('\n', stderr);
    fflush(stderr);
}

static void
afs_dprintf(char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    if (dflag)
	vprintf(fmt, ap);
    va_end(ap);
}

static char *
copy_cellinfo(cellinfo_t *cellinfo)
{
    cellinfo_t *new_cellinfo;

    if ((new_cellinfo = (cellinfo_t *)malloc(sizeof(cellinfo_t))))
	memcpy(new_cellinfo, cellinfo, sizeof(cellinfo_t));

    return ((char *)new_cellinfo);
}


static int
get_cellconfig(char *cell, struct afsconf_cell *cellconfig, char **local_cell)
{
    int status = AKLOG_SUCCESS;
    struct afsconf_dir *configdir;

    memset(cellconfig, 0, sizeof(*cellconfig));

    *local_cell = malloc(MAXCELLCHARS);
    if (*local_cell == NULL) {
	fprintf(stderr, "%s: can't allocate memory for local cell name\n",
		progname);
	exit(AKLOG_AFS);
    }

    if (!(configdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH))) {
	fprintf(stderr,
		"%s: can't get afs configuration (afsconf_Open(%s))\n",
		progname, AFSDIR_CLIENT_ETC_DIRPATH);
	exit(AKLOG_AFS);
    }

    if (afsconf_GetLocalCell(configdir, *local_cell, MAXCELLCHARS)) {
	fprintf(stderr, "%s: can't determine local cell.\n", progname);
	exit(AKLOG_AFS);
    }

    if ((cell == NULL) || (cell[0] == 0))
	cell = *local_cell;

    /* XXX - This function modifies 'cell' by passing it through lcstring */
    if (afsconf_GetCellInfo(configdir, cell, NULL, cellconfig)) {
	fprintf(stderr, "%s: Can't get information about cell %s.\n",
		progname, cell);
	status = AKLOG_AFS;
    }

    afsconf_Close(configdir);

    return(status);
}

static char *
extract_realm(krb5_context context, krb5_principal princ) {
    int len;
    char *realm;

    len = realm_len(context, princ);
    if (len > REALM_SZ-1)
	len = REALM_SZ-1;

    realm = malloc(sizeof(char) * (len+1));
    if (realm == NULL)
	return NULL;

    strncpy(realm, realm_data(context, princ), len);
    realm[len] = '\0';

    return realm;
}

static int
get_realm_from_cred(krb5_context context, krb5_creds *v5cred, char **realm) {
#if !defined(HEIMDAL) && defined(HAVE_KRB5_DECODE_TICKET)
    krb5_error_code code;
    krb5_ticket *ticket;

    *realm = NULL;

    code = krb5_decode_ticket(&v5cred->ticket, &ticket);
    if (code)
	return code;

    *realm = extract_realm(context, ticket->server);
    if (*realm == NULL)
	code = ENOMEM;

    krb5_free_ticket(context, ticket);

    return code;
#else
    *realm = NULL;
    return 0;
#endif
}

/*!
 * Get a Kerberos service ticket to use as the base of an rxkad token for
 * a given AFS cell.
 *
 * @param[in] context
 * 	An initialized Kerberos v5 context
 * @param[in] realm
 * 	The realm to look in for the service principal. If NULL, then the
 * 	realm is determined from the cell name or the user's credentials
 * 	(see below for the heuristics used)
 * @param[in] cell
 * 	The cell information for the cell to obtain a ticket for
 * @param[out] v5cred
 * 	A Kerberos credentials structure containing the ticket acquired
 * 	for the cell. This is a dynamically allocated structure, which
 * 	should be freed by using the appropriate Kerberos API function.
 * @param[out] realmUsed
 * 	The realm in which the cell's service principal was located. If
 * 	unset, then the principal was located in the same realm as the
 * 	current user. This is a malloc'd string which should be freed
 * 	by the caller.
 *
 * @returns
 * 	0 on success, an error value upon failure
 *
 * @notes
 *	This code tries principals in the following, much debated,
 *	order:
 *
 *	If the realm is specified on the command line we do
 *	   - afs/cell@COMMAND-LINE-REALM
 *	   - afs@COMMAND-LINE-REALM
 *
 * 	Otherwise, we do
 *	   - afs/cell@REALM-FROM-USERS-PRINCIPAL
 *	   - afs/cell@krb5_get_host_realm(db-server)
 *	  Then, if krb5_get_host_realm(db-server) is non-empty
 *	     - afs@ krb5_get_host_realm(db-server)
 *	  Otherwise
 *	     - afs/cell@ upper-case-domain-of-db-server
 *	     - afs@ upper-case-domain-of-db-server
 *
 *	In all cases, the 'afs@' variant is only tried where the
 *	cell and the realm match case-insensitively.
 */

static int
rxkad_get_ticket(krb5_context context, char *realm,
		 struct afsconf_cell *cell,
		 krb5_creds **v5cred, char **realmUsed) {
    char *realm_of_cell = NULL;
    char *realm_of_user = NULL;
    char *realm_from_princ = NULL;
    int status;
    int retry;

    *realmUsed = NULL;

    if ((status = get_user_realm(context, &realm_of_user))) {
	fprintf(stderr, "%s: Couldn't determine realm of user:", progname);
	afs_com_err(progname, status, " while getting realm");
	status = AKLOG_KERBEROS;
	goto out;
    }

    retry = 1;

    while(retry) {
	/* Cell on command line - use that one */
	if (realm && realm[0]) {
	    realm_of_cell = realm;
	    status = AKLOG_TRYAGAIN;
	    afs_dprintf("We were told to authenticate to realm %s.\n", realm);
	} else {
	    /* Initially, try using afs/cell@USERREALM */
	    afs_dprintf("Trying to authenticate to user's realm %s.\n",
		    realm_of_user);
	    realm_of_cell = realm_of_user;
	    status = get_credv5(context, AFSKEY, cell->name, realm_of_cell,
			        v5cred);

	    /* If that failed, try to determine the realm from the name of
	     * one of the DB servers */
	    if (TRYAGAIN(status)) {
		realm_of_cell = afs_realm_of_cell(context, cell, FALSE);
		if (!realm_of_cell) {
		    fprintf(stderr, "%s: Couldn't figure out realm for cell "
			    "%s.\n", progname, cell->name);
		    exit(AKLOG_MISC);
		}

		if (realm_of_cell[0])
		    afs_dprintf("We've deduced that we need to authenticate"
			    " to realm %s.\n", realm_of_cell);
		    else
		    afs_dprintf("We've deduced that we need to authenticate "
			    "using referrals.\n");
	    }
	}

	if (TRYAGAIN(status)) {
	    /* If we've got the full-princ-first option, or we're in a
	     * different realm from the cell - use the cell name as the
	     * instance */
	    if (AFS_TRY_FULL_PRINC ||
	        strcasecmp(cell->name, realm_of_cell)!=0) {
		status = get_credv5(context, AFSKEY, cell->name,
				    realm_of_cell, v5cred);

		/* If we failed & we've got an empty realm, then try
		 * calling afs_realm_for_cell again. */
		if (TRYAGAIN(status) && !realm_of_cell[0]) {
		    /* This time, get the realm by taking the domain
		     * component of the db server and make it upper case */
		    realm_of_cell = afs_realm_of_cell(context, cell, TRUE);
		    if (!realm_of_cell) {
			fprintf(stderr,
				"%s: Couldn't figure out realm for cell %s.\n",
				progname, cell->name);
			exit(AKLOG_MISC);
		    }
		    afs_dprintf("We've deduced that we need to authenticate"
			    " to realm %s.\n", realm_of_cell);
		    status = get_credv5(context, AFSKEY, cell->name,
					realm_of_cell, v5cred);
		}
	    }

	    /* If the realm and cell name match, then try without an
	     * instance, but only if realm is non-empty */

	    if (TRYAGAIN(status) &&
		strcasecmp(cell->name, realm_of_cell) == 0) {
	        status = get_credv5(context, AFSKEY, NULL, realm_of_cell,
				    v5cred);
		if (!AFS_TRY_FULL_PRINC && TRYAGAIN(status)) {
		    status = get_credv5(context, AFSKEY, cell->name,
					realm_of_cell, v5cred);
		}
	    }
	}

	/* Try to find a service principal for this cell.
	 * Some broken MIT libraries return KRB5KRB_AP_ERR_MSG_TYPE upon
	 * the first attempt, so we try twice to be sure */

	if (status == KRB5KRB_AP_ERR_MSG_TYPE && retry == 1)
	    retry++;
	else
	    retry = 0;
    }

    if (status != 0) {
	afs_dprintf("Kerberos error code returned by get_cred : %d\n", status);
	fprintf(stderr, "%s: Couldn't get %s AFS tickets:\n",
		progname, cell->name);
	afs_com_err(progname, status, "while getting AFS tickets");
#ifdef KRB5_CC_NOT_KTYPE
	if (status == KRB5_CC_NOT_KTYPE) {
	    fprintf(stderr, "allow_weak_crypto may be required in the Kerberos configuration\n");
	}
#endif
	status = AKLOG_KERBEROS;
	goto out;
    }

    /* If we've got a valid ticket, and we still don't know the realm name
     * try to figure it out from the contents of the ticket
     */
    if (strcmp(realm_of_cell, "") == 0) {
	status = get_realm_from_cred(context, *v5cred, &realm_from_princ);
	if (status) {
	    fprintf(stderr,
		    "%s: Couldn't decode ticket to determine realm for "
		    "cell %s.\n",
		    progname, cell->name);
	} else {
	    if (realm_from_princ)
		realm_of_cell = realm_from_princ;
	}
    }

    /* If the realm of the user and cell differ, then we need to use the
     * realm when we later construct the user's principal */
    if (realm_of_cell != NULL && strcmp(realm_of_user, realm_of_cell) != 0)
	*realmUsed = realm_of_user;

out:
    if (realm_from_princ)
	free(realm_from_princ);
    if (realm_of_user && *realmUsed == NULL)
	free(realm_of_user);

    return status;
}

/*!
 * Build an rxkad token from a Kerberos ticket, using only local tools (that
 * is, without using a 524 conversion service)
 *
 * @param[in] context
 *	An initialised Kerberos 5 context
 * @param[in] v5cred
 * 	A Kerberos credentials structure containing a suitable service ticket
 * @param[out] tokenPtr
 * 	An AFS token structure containing an rxkad token. This is a malloc'd
 * 	structure which should be freed by the caller.
 * @param[out[ userPtr
 * 	A string containing the principal of the user to whom the token was
 * 	issued. This is a malloc'd block which should be freed by the caller,
 *      if set.
 *
 * @returns
 * 	0 on success, an error value upon failure
 */
static int
rxkad_build_native_token(krb5_context context, krb5_creds *v5cred,
			 struct ktc_token **tokenPtr, char **userPtr) {
    char username[BUFSIZ]="";
    struct ktc_token *token;
#ifdef HAVE_NO_KRB5_524
    char *p;
    int len;
#else
    int status;
    char k4name[ANAME_SZ];
    char k4inst[INST_SZ];
    char k4realm[REALM_SZ];
#endif
    void *inkey = get_cred_keydata(v5cred);
    size_t inkey_sz = get_cred_keylen(v5cred);

    afs_dprintf("Using Kerberos V5 ticket natively\n");

    *tokenPtr = NULL;
    *userPtr = NULL;

#ifndef HAVE_NO_KRB5_524
    status = krb5_524_conv_principal (context, v5cred->client,
				      (char *) &k4name,
				      (char *) &k4inst,
				      (char *) &k4realm);
    if (status) {
	if (!noprdb)
	    afs_com_err(progname, status,
			"while converting principal to Kerberos V4 format");
    } else {
	strcpy (username, k4name);
	if (k4inst[0]) {
	    strcat (username, ".");
	    strcat (username, k4inst);
	}
    }
#else
    len = min(get_princ_len(context, v5cred->client, 0),
	      second_comp(context, v5cred->client) ?
	      MAXKTCNAMELEN - 2 : MAXKTCNAMELEN - 1);
    strncpy(username, get_princ_str(context, v5cred->client, 0), len);
    username[len] = '\0';

    if (second_comp(context, v5cred->client)) {
	strcat(username, ".");
	p = username + strlen(username);
	len = min(get_princ_len(context, v5cred->client, 1),
		  MAXKTCNAMELEN - strlen(username) - 1);
	strncpy(p, get_princ_str(context, v5cred->client, 1), len);
	p[len] = '\0';
    }
#endif

    token = malloc(sizeof(struct ktc_token));
    if (token == NULL)
	return ENOMEM;

    memset(token, 0, sizeof(struct ktc_token));

    token->kvno = RXKAD_TKT_TYPE_KERBEROS_V5;
    token->startTime = v5cred->times.starttime;;
    token->endTime = v5cred->times.endtime;
    if (tkt_DeriveDesKey(get_creds_enctype(v5cred), inkey, inkey_sz,
			 &token->sessionKey) != 0) {
	free(token);
	return RXKADBADKEY;
    }
    token->ticketLen = v5cred->ticket.length;
    memcpy(token->ticket, v5cred->ticket.data, token->ticketLen);

    *tokenPtr = token;
    if (username[0] != '\0')
	*userPtr = strdup(username);
    return 0;
}

/*!
 * Convert a Keberos ticket to an rxkad token, using information obtained
 * from an external Kerberos 5->4 conversion service. If the code is built
 * with HAVE_NO_KRB5_524 then this is a stub function which will always
 * return success without a token.
 *
 * @param[in] context
 *	An initialised Kerberos 5 context
 * @param[in] v5cred
 * 	A Kerberos credentials structure containing a suitable service ticket
 * @param[out] tokenPtr
 * 	An AFS token structure containing an rxkad token. This is a malloc'd
 * 	structure which should be freed by the caller.
 * @param[out[ userPtr
 * 	A string containing the principal of the user to whom the token was
 * 	issued. This is a malloc'd block which should be freed by the caller.
 *
 * @returns
 * 	0 on success, an error value upon failure
 */

#ifdef HAVE_NO_KRB5_524
static int
rxkad_get_converted_token(krb5_context context, krb5_creds *v5cred,
			  struct ktc_token **tokenPtr, char **userPtr) {
    *tokenPtr = NULL;
    *userPtr = NULL;

    return 0;
}
#else
static int
rxkad_get_converted_token(krb5_context context, krb5_creds *v5cred,
			  struct ktc_token **tokenPtr, char **userPtr) {
    CREDENTIALS cred;
    char username[BUFSIZ];
    struct ktc_token *token;
    int status;

    *tokenPtr = NULL;
    *userPtr = NULL;

    afs_dprintf("Using Kerberos 524 translator service\n");

    status = krb5_524_convert_creds(context, v5cred, &cred);

    if (status) {
	afs_com_err(progname, status, "while converting tickets "
		"to Kerberos V4 format");
	return AKLOG_KERBEROS;
    }

    strcpy (username, cred.pname);
    if (cred.pinst[0]) {
	strcat (username, ".");
	strcat (username, cred.pinst);
    }

    token = malloc(sizeof(struct ktc_token));
    memset(token, 0, sizeof(struct ktc_token));

    token->kvno = cred.kvno;
    token->startTime = cred.issue_date;
    /*
     * It seems silly to go through a bunch of contortions to
     * extract the expiration time, when the v5 credentials already
     * has the exact time!  Let's use that instead.
     *
     * Note that this isn't a security hole, as the expiration time
     * is also contained in the encrypted token
     */
    token->endTime = v5cred->times.endtime;
    memcpy(&token->sessionKey, cred.session, 8);
    token->ticketLen = cred.ticket_st.length;
    memcpy(token->ticket, cred.ticket_st.dat, token->ticketLen);

    *tokenPtr = token;
    *userPtr = strdup(username);

    return 0;
}
#endif

/*!
 * This function gets an rxkad token for a given cell.
 *
 * @param[in] context
 * 	An initialized Kerberos v5 context
 * @param[in] cell
 * 	The cell information for the cell which we're obtaining a token for
 * @param[in] realm
 * 	The realm to look in for the service principal. If NULL, then the
 * 	realm is determined from the cell name or the user's credentials
 * 	(see the documentation for rxkad_get_ticket)
 * @param[out] token
 * 	The rxkad token produced. This is a malloc'd structure which should
 * 	be freed by the caller.
 * @parma[out] authuser
 * 	A string containing the principal of the user to whom the token was
 * 	issued. This is a malloc'd block which should be freed by the caller,
 *	if set.
 * @param[out] foreign
 * 	Whether the user is considered as 'foreign' to the realm of the cell.
 *
 * @returns
 * 	0 on success, an error value upon failuer
 */
static int
rxkad_get_token(krb5_context context, struct afsconf_cell *cell, char *realm,
		struct ktc_token **token, char **authuser, int *foreign) {
    krb5_creds *v5cred;
    char *realmUsed = NULL;
    char *username = NULL;
    int status;
    size_t len;

    *token = NULL;
    *authuser = NULL;
    *foreign = 0;

    status = rxkad_get_ticket(context, realm, cell, &v5cred, &realmUsed);
    if (status)
	return status;

    if (do524)
	status = rxkad_get_converted_token(context, v5cred, token, &username);
    else
	status = rxkad_build_native_token(context, v5cred, token, &username);

    if (status)
	goto out;

    /* We now have the username, plus the realm name, so stitch them together
     * to give us the name that the ptserver will know the user by */
    if (realmUsed == NULL || username == NULL) {
	*authuser = username;
	username = NULL;
	*foreign = 0;
    } else {
	len = strlen(username)+strlen(realmUsed)+2;
	*authuser = malloc(len);
	afs_snprintf(*authuser, len, "%s@%s", username, realmUsed);
	*foreign = 1;
    }

out:
    if (realmUsed)
	free(realmUsed);
    if (username)
	free(username);

    return status;
}

static int
get_kernel_token(struct afsconf_cell *cell, struct ktc_token **tokenPtr) {
    struct ktc_principal client, server;
    struct ktc_token *token;
    int ret;

    *tokenPtr = NULL;

    strncpy(server.name, AFSKEY, MAXKTCNAMELEN - 1);
    strncpy(server.instance, AFSINST, MAXKTCNAMELEN - 1);
    strncpy(server.cell, cell->name, MAXKTCREALMLEN - 1);

    token = malloc(sizeof(struct ktc_token));
    if (token == NULL)
	return ENOMEM;

    memset(token, 0, sizeof(struct ktc_token));

    ret = ktc_GetToken(&server, token, sizeof(struct ktc_token), &client);
    if (ret) {
	free(token);
	return ret;
    }

    *tokenPtr = token;
    return 0;
}

static int
set_kernel_token(struct afsconf_cell *cell, char *username,
		 struct ktc_token *token, int setpag)
{
    struct ktc_principal client, server;

    strncpy(client.name,
	    username ? username : "",
	    MAXKTCNAMELEN - 1);
    strcpy(client.instance, "");
    strncpy(client.cell, cell->name, MAXKTCREALMLEN - 1);

    strncpy(server.name, AFSKEY, MAXKTCNAMELEN - 1);
    strncpy(server.instance, AFSINST, MAXKTCNAMELEN - 1);
    strncpy(server.cell, cell->name, MAXKTCREALMLEN - 1);

    return ktc_SetToken(&server, token, &client, setpag);
}

static int
tokens_equal(struct ktc_token *tokenA, struct ktc_token *tokenB) {
    return (tokenA != NULL && tokenB != NULL &&
	    tokenA->kvno == tokenB->kvno &&
	    tokenA->ticketLen == tokenB->ticketLen &&
	    !memcmp(&tokenA->sessionKey, &tokenB->sessionKey,
		    sizeof(tokenA->sessionKey)) &&
	    !memcmp(tokenA->ticket, tokenB->ticket, tokenA->ticketLen));
}

/*
 * Log to a cell.  If the cell has already been logged to, return without
 * doing anything.  Otherwise, log to it and mark that it has been logged
 * to.
 */
static int
auth_to_cell(krb5_context context, char *cell, char *realm, char **linkedcell)
{
    int status = AKLOG_SUCCESS;
    int isForeign = 0;
    char *username = NULL;	/* To hold client username structure */
    afs_int32 viceId;		/* AFS uid of user */

    char *local_cell = NULL;
    struct ktc_token *token;
    struct ktc_token *btoken;
    struct afsconf_cell cellconf;

    /* NULL or empty cell returns information on local cell */
    if ((status = get_cellconfig(cell, &cellconf, &local_cell)))
	return(status);

    if (linkedcell != NULL) {
	if (cellconf.linkedCell != NULL) {
	    *linkedcell = strdup(cellconf.linkedCell);
	    if (*linkedcell == NULL) {
		status = ENOMEM;
		goto out;
	    }
	} else {
	    *linkedcell = NULL;
	}
    }

    if (ll_string(&authedcells, ll_s_check, cellconf.name)) {
	afs_dprintf("Already authenticated to %s (or tried to)\n", cellconf.name);
	status = AKLOG_SUCCESS;
	goto out;
    }

    /*
     * Record that we have attempted to log to this cell.  We do this
     * before we try rather than after so that we will not try
     * and fail repeatedly for one cell.
     */
    ll_string(&authedcells, ll_s_add, cellconf.name);

    /*
     * Record this cell in the list of zephyr subscriptions.  We may
     * want zephyr subscriptions even if authentication fails.
     * If this is done after we attempt to get tokens, aklog -zsubs
     * can return something different depending on whether or not we
     * are in -noauth mode.
     */
    if (ll_string(&zsublist, ll_s_add, cellconf.name) == LL_FAILURE) {
	fprintf(stderr,
		"%s: failure adding cell %s to zephyr subscriptions list.\n",
		progname, cellconf.name);
	exit(AKLOG_MISC);
    }
    if (ll_string(&zsublist, ll_s_add, local_cell) == LL_FAILURE) {
	fprintf(stderr,
		"%s: failure adding cell %s to zephyr subscriptions list.\n",
		progname, local_cell);
	exit(AKLOG_MISC);
    }

    if (!noauth) {
	afs_dprintf("Authenticating to cell %s (server %s).\n", cellconf.name,
		cellconf.hostName[0]);

	status = rxkad_get_token(context, &cellconf, realm, &token,
				 &username, &isForeign);
	if (status)
	    return status;


	if (!force &&
	    !get_kernel_token(&cellconf, &btoken) &&
	    tokens_equal(token, btoken)) {
	    afs_dprintf("Identical tokens already exist; skipping.\n");
	    status = AKLOG_SUCCESS;
	    goto out;
	}

#ifdef FORCE_NOPRDB
	noprdb = 1;
#endif

	if (username == NULL) {
	    afs_dprintf("Not resolving name to id\n");
	}
	else if (noprdb) {
	    afs_dprintf("Not resolving name %s to id (-noprdb set)\n", username);
	}
	else {
	    afs_dprintf("About to resolve name %s to id in cell %s.\n", username,
		    cellconf.name);

	    if (!pr_Initialize (0,  AFSDIR_CLIENT_ETC_DIRPATH, cellconf.name))
		status = pr_SNameToId (username, &viceId);

	    if (status)
		afs_dprintf("Error %d\n", status);
	    else
		afs_dprintf("Id %d\n", (int) viceId);


	    /*
	     * This code is taken from cklog -- it lets people
	     * automatically register with the ptserver in foreign cells
	     */

#ifdef ALLOW_REGISTER
	    if ((status == 0) && (viceId == ANONYMOUSID) && isForeign) {
		afs_dprintf("doing first-time registration of %s at %s\n",
			username, cellconf.name);
		viceId = 0;

		status = set_kernel_token(&cellconf, username, token, 0);
		if (status) {
		    afs_com_err(progname, status,
				"while obtaining tokens for cell %s",
		                cellconf.name);
		    status = AKLOG_TOKEN;
		}

		/*
		 * In case you're wondering, we don't need to change the
		 * filename here because we're still connecting to the
		 * same cell -- we're just using a different authentication
		 * level
		 */

		if ((status = pr_Initialize(1L,  AFSDIR_CLIENT_ETC_DIRPATH,
					    cellconf.name))) {
		    printf("Error %d\n", status);
		}

		if ((status = pr_CreateUser(username, &viceId))) {
		    fprintf(stderr, "%s: %s so unable to create remote PTS "
			    "user %s in cell %s (status: %d).\n", progname,
			    afs_error_message(status), username, cellconf.name,
			    status);
		    viceId = ANONYMOUSID;
		} else {
		    printf("created cross-cell entry for %s (Id %d) at %s\n",
			   username, viceId, cellconf.name);
		}
	    }
#endif /* ALLOW_REGISTER */

	    /*
	     * This is a crock, but it is Transarc's crock, so we have to play
	     * along in order to get the functionality.  The way the afs id is
	     * stored is as a string in the username field of the token.
	     * Contrary to what you may think by looking at the code for
	     * tokens, this hack (AFS ID %d) will not work if you change %d
	     * to something else.
	     */

	    if ((status == 0) && (viceId != ANONYMOUSID)) {
		free(username);
		if (afs_asprintf(&username, "AFS ID %d", (int) viceId) < 0) {
		    status = ENOMEM;
		    username = NULL;
		    goto out;
		}
	    }
	}

	if (username) {
	    afs_dprintf("Set username to %s\n", username);

	    afs_dprintf("Setting tokens. %s @ %s\n",
			username, cellconf.name);
	} else {
	    afs_dprintf("Setting tokens for cell %s\n", cellconf.name);
	}

#ifndef AFS_AIX51_ENV
	/* on AIX 4.1.4 with AFS 3.4a+ if a write is not done before
	 * this routine, it will not add the token. It is not clear what
	 * is going on here! So we will do the following operation.
	 * On AIX 5, it causes the parent program to die, so we won't.
	 * We don't care about the return value, but need to collect it
	 * to avoid compiler warnings.
	 */
	if (write(2,"",0) < 0) /* dummy write */
	    ; /* don't care */
#endif
	status = set_kernel_token(&cellconf, username, token, afssetpag);
	if (status) {
	    afs_com_err(progname, status, "while obtaining tokens for cell %s",
			cellconf.name);
	    status = AKLOG_TOKEN;
	}
    }
    else
	afs_dprintf("Noauth mode; not authenticating.\n");

out:
    if (local_cell)
	free(local_cell);
    if (username)
	free(username);

    return(status);
}

static int
get_afs_mountpoint(char *file, char *mountpoint, int size)
{
#ifdef AFS_SUN_ENV
    char V ='V'; /* AFS has problem on Sun with pioctl */
#endif
    char our_file[MAXPATHLEN + 1];
    char *parent_dir;
    char *last_component;
    struct ViceIoctl vio;
    char cellname[BUFSIZ];

    strlcpy(our_file, file, sizeof(our_file));

    if ((last_component = strrchr(our_file, DIR))) {
	*last_component++ = 0;
	parent_dir = our_file;
    }
    else {
	last_component = our_file;
	parent_dir = ".";
    }

    memset(cellname, 0, sizeof(cellname));

    vio.in = last_component;
    vio.in_size = strlen(last_component)+1;
    vio.out_size = size;
    vio.out = mountpoint;

    if (!pioctl(parent_dir, VIOC_AFS_STAT_MT_PT, &vio, 0)) {
	if (strchr(mountpoint, VOLMARKER) == NULL) {
	    vio.in = file;
	    vio.in_size = strlen(file) + 1;
	    vio.out_size = sizeof(cellname);
	    vio.out = cellname;

	    if (!pioctl(file, VIOC_FILE_CELL_NAME, &vio, 1)) {
		strlcat(cellname, VOLMARKERSTRING, sizeof(cellname));
		strlcat(cellname, mountpoint + 1, sizeof(cellname));
		memset(mountpoint + 1, 0, size - 1);
		strcpy(mountpoint + 1, cellname);
	    }
	}
	return(TRUE);
    }
    else
	return(FALSE);
}

/*
 * This routine each time it is called returns the next directory
 * down a pathname.  It resolves all symbolic links.  The first time
 * it is called, it should be called with the name of the path
 * to be descended.  After that, it should be called with the arguemnt
 * NULL.
 */
static char *
next_path(char *origpath)
{
    static char path[MAXPATHLEN + 1];
    static char pathtocheck[MAXPATHLEN + 1];

    ssize_t link;		/* Return value from readlink */
    char linkbuf[MAXPATHLEN + 1];
    char tmpbuf[MAXPATHLEN + 1];

    static char *last_comp;	/* last component of directory name */
    static char *elast_comp;	/* End of last component */
    char *t;
    int len;

    static int symlinkcount = 0; /* We can't exceed MAXSYMLINKS */

    /* If we are given something for origpath, we are initializing only. */
    if (origpath) {
	memset(path, 0, sizeof(path));
	memset(pathtocheck, 0, sizeof(pathtocheck));
	strlcpy(path, origpath, sizeof(path));
	last_comp = path;
	symlinkcount = 0;
	return(NULL);
    }

    /* We were not given origpath; find then next path to check */

    /* If we've gotten all the way through already, return NULL */
    if (last_comp == NULL)
	return(NULL);

    do {
	while (*last_comp == DIR)
	    strncat(pathtocheck, last_comp++, 1);
	len = (elast_comp = strchr(last_comp, DIR))
	    ? elast_comp - last_comp : strlen(last_comp);
	strncat(pathtocheck, last_comp, len);
	memset(linkbuf, 0, sizeof(linkbuf));
	link = readlink(pathtocheck, linkbuf, sizeof(linkbuf)-1);

	if (link > 0) {
	    linkbuf[link] = '\0'; /* NUL terminate string */

	    if (++symlinkcount > MAXSYMLINKS) {
		fprintf(stderr, "%s: %s\n", progname, strerror(ELOOP));
		exit(AKLOG_BADPATH);
	    }

	    memset(tmpbuf, 0, sizeof(tmpbuf));
	    if (elast_comp)
		strlcpy(tmpbuf, elast_comp, sizeof(tmpbuf));
	    if (linkbuf[0] == DIR) {
		/*
		 * If this is a symbolic link to an absolute path,
		 * replace what we have by the absolute path.
		 */
		memset(path, 0, strlen(path));
		memcpy(path, linkbuf, sizeof(linkbuf));
		strcat(path, tmpbuf);
		last_comp = path;
		elast_comp = NULL;
		memset(pathtocheck, 0, sizeof(pathtocheck));
	    }
	    else {
		/*
		 * If this is a symbolic link to a relative path,
		 * replace only the last component with the link name.
		 */
		strncpy(last_comp, linkbuf, strlen(linkbuf) + 1);
		strcat(path, tmpbuf);
		elast_comp = NULL;
		if ((t = strrchr(pathtocheck, DIR))) {
		    t++;
		    memset(t, 0, strlen(t));
		}
		else
		    memset(pathtocheck, 0, sizeof(pathtocheck));
	    }
	}
	else
	    last_comp = elast_comp;
    }
    while(link > 0);

    return(pathtocheck);
}

static void
add_hosts(char *file)
{
#ifdef AFS_SUN_ENV
    char V = 'V'; /* AFS has problem on SunOS */
#endif
    struct ViceIoctl vio;
    char outbuf[BUFSIZ];
    long *phosts;
    int i;
    struct hostent *hp;
    struct in_addr in;

    memset(outbuf, 0, sizeof(outbuf));

    vio.out_size = sizeof(outbuf);
    vio.in_size = 0;
    vio.out = outbuf;

    afs_dprintf("Getting list of hosts for %s\n", file);

    /* Don't worry about errors. */
    if (!pioctl(file, VIOCWHEREIS, &vio, 1)) {
	phosts = (long *) outbuf;

	/*
	 * Lists hosts that we care about.  If ALLHOSTS is defined,
	 * then all hosts that you ever may possible go through are
	 * included in this list.  If not, then only hosts that are
	 * the only ones appear.  That is, if a volume you must use
	 * is replaced on only one server, that server is included.
	 * If it is replicated on many servers, then none are included.
	 * This is not perfect, but the result is that people don't
	 * get subscribed to a lot of instances of FILSRV that they
	 * probably won't need which reduces the instances of
	 * people getting messages that don't apply to them.
	 */
#ifndef ALLHOSTS
	if (phosts[1] != '\0')
	    return;
#endif
	for (i = 0; phosts[i]; i++) {
	    if (hosts) {
		in.s_addr = phosts[i];
		afs_dprintf("Got host %s\n", inet_ntoa(in));
		ll_string(&hostlist, ll_s_add, (char *)inet_ntoa(in));
	    }
	    if (zsubs && (hp=gethostbyaddr((char *) &phosts[i],sizeof(long),AF_INET))) {
		afs_dprintf("Got host %s\n", hp->h_name);
		ll_string(&zsublist, ll_s_add, hp->h_name);
	    }
	}
    }
}

/*
 * This routine descends through a path to a directory, logging to
 * every cell it encounters along the way.
 */
static int
auth_to_path(krb5_context context, char *path)
{
    int status = AKLOG_SUCCESS;
    int auth_status = AKLOG_SUCCESS;

    char *nextpath;
    char pathtocheck[MAXPATHLEN + 1];
    char mountpoint[MAXPATHLEN + 1];

    char *cell;
    char *endofcell;

    u_char isdirectory;

    /* Initialize */
    if (path[0] == DIR)
	strlcpy(pathtocheck, path, sizeof(pathtocheck));
    else {
	if (getcwd(pathtocheck, sizeof(pathtocheck)) == NULL) {
	    fprintf(stderr, "Unable to find current working directory:\n");
	    fprintf(stderr, "%s\n", pathtocheck);
	    fprintf(stderr, "Try an absolute pathname.\n");
	    exit(AKLOG_BADPATH);
	}
	else {
	    strlcat(pathtocheck, DIRSTRING, sizeof(pathtocheck));
	    strlcat(pathtocheck, path, sizeof(pathtocheck));
	}
    }
    next_path(pathtocheck);

    /* Go on to the next level down the path */
    while ((nextpath = next_path(NULL))) {
	strlcpy(pathtocheck, nextpath, sizeof(pathtocheck));
	afs_dprintf("Checking directory %s\n", pathtocheck);
	/*
	 * If this is an afs mountpoint, determine what cell from
	 * the mountpoint name which is of the form
	 * #cellname:volumename or %cellname:volumename.
	 */
	if (get_afs_mountpoint(pathtocheck, mountpoint, sizeof(mountpoint))) {
	    /* skip over the '#' or '%' */
	    cell = mountpoint + 1;
	    /* Add this (cell:volumename) to the list of zsubs */
	    if (zsubs)
		ll_string(&zsublist, ll_s_add, cell);
	    if (zsubs || hosts)
		add_hosts(pathtocheck);
	    if ((endofcell = strchr(mountpoint, VOLMARKER))) {
		*endofcell = '\0';
		if ((auth_status = auth_to_cell(context, cell, NULL, NULL))) {
		    if (status == AKLOG_SUCCESS)
			status = auth_status;
		    else if (status != auth_status)
			status = AKLOG_SOMETHINGSWRONG;
		}
	    }
	}
	else {
	    if (isdir(pathtocheck, &isdirectory) < 0) {
		/*
		 * If we've logged and still can't stat, there's
		 * a problem...
		 */
		fprintf(stderr, "%s: stat(%s): %s\n", progname,
			pathtocheck, strerror(errno));
		return(AKLOG_BADPATH);
	    }
	    else if (! isdirectory) {
		/* Allow only directories */
		fprintf(stderr, "%s: %s: %s\n", progname, pathtocheck,
			strerror(ENOTDIR));
		return(AKLOG_BADPATH);
	    }
	}
    }


    return(status);
}


/* Print usage message and exit */
static void
usage(void)
{
    fprintf(stderr, "\nUsage: %s %s%s%s\n", progname,
	    "[-d] [[-cell | -c] cell [-k krb_realm]] ",
	    "[[-p | -path] pathname]\n",
	    "    [-zsubs] [-hosts] [-noauth] [-noprdb] [-force] [-setpag] \n"
		"    [-linked]"
#ifndef HAVE_NO_KRB5_524
		" [-524]"
#endif
		"\n");
    fprintf(stderr, "    -d gives debugging information.\n");
    fprintf(stderr, "    krb_realm is the kerberos realm of a cell.\n");
    fprintf(stderr, "    pathname is the name of a directory to which ");
    fprintf(stderr, "you wish to authenticate.\n");
    fprintf(stderr, "    -zsubs gives zephyr subscription information.\n");
    fprintf(stderr, "    -hosts gives host address information.\n");
    fprintf(stderr, "    -noauth does not attempt to get tokens.\n");
    fprintf(stderr, "    -noprdb means don't try to determine AFS ID.\n");
    fprintf(stderr, "    -force means replace identical tickets. \n");
    fprintf(stderr, "    -linked means if AFS node is linked, try both. \n");
    fprintf(stderr, "    -setpag set the AFS process authentication group.\n");
#ifndef HAVE_NO_KRB5_524
    fprintf(stderr, "    -524 means use the 524 converter instead of V5 directly\n");
#endif
    fprintf(stderr, "    No commandline arguments means ");
    fprintf(stderr, "authenticate to the local cell.\n");
    fprintf(stderr, "\n");
    exit(AKLOG_USAGE);
}

int
main(int argc, char *argv[])
{
    krb5_context context;
    int status = AKLOG_SUCCESS;
    int i;
    int somethingswrong = FALSE;

    cellinfo_t cellinfo;

    extern char *progname;	/* Name of this program */

    int cmode = FALSE;		/* Cellname mode */
    int pmode = FALSE;		/* Path name mode */

    char realm[REALM_SZ];	/* Kerberos realm of afs server */
    char cell[BUFSIZ];		/* Cell to which we are authenticating */
    char path[MAXPATHLEN + 1];		/* Path length for path mode */

    linked_list cells;		/* List of cells to log to */
    linked_list paths;		/* List of paths to log to */
    ll_node *cur_node;
    char *linkedcell;

    memset(&cellinfo, 0, sizeof(cellinfo));

    memset(realm, 0, sizeof(realm));
    memset(cell, 0, sizeof(cell));
    memset(path, 0, sizeof(path));

    ll_init(&cells);
    ll_init(&paths);

    ll_init(&zsublist);
    ll_init(&hostlist);

    /* Store the program name here for error messages */
    if ((progname = strrchr(argv[0], DIR)))
	progname++;
    else
	progname = argv[0];

#if defined(KRB5_PROG_ETYPE_NOSUPP) && !(defined(HAVE_KRB5_ENCTYPE_ENABLE) || defined(HAVE_KRB5_ALLOW_WEAK_CRYPTO))
    {
	char *filepath = NULL, *newpath = NULL;
#ifndef AFS_DARWIN_ENV
	char *defaultpath = "/etc/krb5.conf:/etc/krb5/krb5.conf";
#else
	char *defaultpath = "~/Library/Preferences/edu.mit.Kerberos:/Library/Preferences/edu.mit.Kerberos";
#endif
	filepath = getenv("KRB5_CONFIG");

	/* only fiddle with KRB5_CONFIG if krb5-weak.conf actually exists */
	afs_asprintf(&newpath, "%s/krb5-weak.conf", AFSDIR_CLIENT_ETC_DIRPATH);
	if (access(newpath, R_OK) == 0) {
	    free(newpath);
	    newpath = NULL;
	    afs_asprintf(&newpath, "%s:%s/krb5-weak.conf",
	                 filepath ? filepath : defaultpath,
	                 AFSDIR_CLIENT_ETC_DIRPATH);
	    setenv("KRB5_CONFIG", newpath, 1);
	}
#endif
	krb5_init_context(&context);

#if defined(KRB5_PROG_ETYPE_NOSUPP) && !(defined(HAVE_KRB5_ENCTYPE_ENABLE) || defined(HAVE_KRB5_ALLOW_WEAK_CRYPTO))
	free(newpath);
	if (filepath)
	    setenv("KRB5_CONFIG", filepath, 1);
	else
	    unsetenv("KRB5_CONFIG");
    }
#endif
    initialize_KTC_error_table ();
    initialize_U_error_table();
    initialize_RXK_error_table();
    initialize_ACFG_error_table();
    initialize_PT_error_table();
    afs_set_com_err_hook(redirect_errors);

    /*
     * Enable DES enctypes, which are currently still required for AFS.
     * krb5_allow_weak_crypto is MIT Kerberos 1.8.  krb5_enctype_enable is
     * Heimdal.
     */
#if defined(HAVE_KRB5_ENCTYPE_ENABLE)
    i = krb5_enctype_valid(context, ETYPE_DES_CBC_CRC);
    if (i)
        krb5_enctype_enable(context, ETYPE_DES_CBC_CRC);
#elif defined(HAVE_KRB5_ALLOW_WEAK_CRYPTO)
    krb5_allow_weak_crypto(context, 1);
#endif

    /* Initialize list of cells to which we have authenticated */
    ll_init(&authedcells);

    /* Parse commandline arguments and make list of what to do. */
    for (i = 1; i < argc; i++) {
	if (strcmp(argv[i], "-d") == 0)
	    dflag++;
	else if (strcmp(argv[i], "-noauth") == 0)
	    noauth++;
	else if (strcmp(argv[i], "-zsubs") == 0)
	    zsubs++;
	else if (strcmp(argv[i], "-hosts") == 0)
	    hosts++;
	else if (strcmp(argv[i], "-noprdb") == 0)
	    noprdb++;
	else if (strcmp(argv[i], "-linked") == 0)
		linked++;
	else if (strcmp(argv[i], "-force") == 0)
	    force++;
#ifndef HAVE_NO_KRB5_524
	else if (strcmp(argv[i], "-524") == 0)
	    do524++;
#endif
    else if (strcmp(argv[i], "-setpag") == 0)
	    afssetpag++;
	else if (((strcmp(argv[i], "-cell") == 0) ||
		  (strcmp(argv[i], "-c") == 0)) && !pmode)
	    if (++i < argc) {
		cmode++;
		strlcpy(cell, argv[i], sizeof(cell));
	    }
	    else
		usage();
	else if ((strcmp(argv[i], "-keytab") == 0))
	    if (++i < argc) {
		keytab = argv[i];
	    }
	    else
		usage();
	else if ((strcmp(argv[i], "-principal") == 0))
	    if (++i < argc) {
		client = argv[i];
	    }
	    else
		usage();
	else if (((strcmp(argv[i], "-path") == 0) ||
		  (strcmp(argv[i], "-p") == 0)) && !cmode)
	    if (++i < argc) {
		pmode++;
		strlcpy(path, argv[i], sizeof(path));
	    }
	    else
		usage();

	else if (argv[i][0] == '-')
	    usage();
	else if (!pmode && !cmode) {
	    if (strchr(argv[i], DIR) || (strcmp(argv[i], ".") == 0) ||
		(strcmp(argv[i], "..") == 0)) {
		pmode++;
		strlcpy(path, argv[i], sizeof(path));
	    }
	    else {
		cmode++;
		strlcpy(cell, argv[i], sizeof(cell));
	    }
	}
	else
	    usage();

	if (cmode) {
	    if (((i + 1) < argc) && (strcmp(argv[i + 1], "-k") == 0)) {
		i+=2;
		if (i < argc)
		    strlcpy(realm, argv[i], sizeof(realm));
		else
		    usage();
	    }
	    /* Add this cell to list of cells */
	    strcpy(cellinfo.cell, cell);
	    strcpy(cellinfo.realm, realm);
	    if ((cur_node = ll_add_node(&cells, ll_tail))) {
		char *new_cellinfo;
		if ((new_cellinfo = copy_cellinfo(&cellinfo)))
		    ll_add_data(cur_node, new_cellinfo);
		else {
		    fprintf(stderr,
			    "%s: failure copying cellinfo.\n", progname);
		    exit(AKLOG_MISC);
		}
	    }
	    else {
		fprintf(stderr, "%s: failure adding cell to cells list.\n",
			progname);
		exit(AKLOG_MISC);
	    }
	    memset(&cellinfo, 0, sizeof(cellinfo));
	    cmode = FALSE;
	    memset(cell, 0, sizeof(cell));
	    memset(realm, 0, sizeof(realm));
	}
	else if (pmode) {
	    /* Add this path to list of paths */
	    if ((cur_node = ll_add_node(&paths, ll_tail))) {
		char *new_path;
		if ((new_path = strdup(path)))
		    ll_add_data(cur_node, new_path);
		else {
		    fprintf(stderr, "%s: failure copying path name.\n",
			    progname);
		    exit(AKLOG_MISC);
		}
	    }
	    else {
		fprintf(stderr, "%s: failure adding path to paths list.\n",
			progname);
		exit(AKLOG_MISC);
	    }
	    pmode = FALSE;
	    memset(path, 0, sizeof(path));
	}
    }

    /* If nothing was given, log to the local cell. */
    if ((cells.nelements + paths.nelements) == 0) {
	struct passwd *pwd;

	status = auth_to_cell(context, NULL, NULL, &linkedcell);

	/* If this cell is linked to a DCE cell, and user requested -linked,
	 * get tokens for both. This is very useful when the AFS cell is
	 * linked to a DFS cell and this system does not also have DFS.
	 */

	if (!status && linked && linkedcell != NULL) {
	    afs_dprintf("Linked cell: %s\n", linkedcell);
	    status = auth_to_cell(context, linkedcell, NULL, NULL);
	}
	if (linkedcell) {
	    free(linkedcell);
	    linkedcell = NULL;
	}

	/*
	 * Local hack - if the person has a file in their home
	 * directory called ".xlog", read that for a list of
	 * extra cells to authenticate to
	 */

	if ((pwd = getpwuid(getuid())) != NULL) {
	    struct stat sbuf;
	    FILE *f;
	    char fcell[100], xlog_path[512];

	    strlcpy(xlog_path, pwd->pw_dir, sizeof(xlog_path));
	    strlcat(xlog_path, "/.xlog", sizeof(xlog_path));

	    if ((stat(xlog_path, &sbuf) == 0) &&
		((f = fopen(xlog_path, "r")) != NULL)) {

		afs_dprintf("Reading %s for cells to authenticate to.\n",
			xlog_path);

		while (fgets(fcell, 100, f) != NULL) {
		    int auth_status;

		    fcell[strlen(fcell) - 1] = '\0';

		    afs_dprintf("Found cell %s in %s.\n", fcell, xlog_path);

		    auth_status = auth_to_cell(context, fcell, NULL, NULL);
		    if (status == AKLOG_SUCCESS)
			status = auth_status;
		    else
			status = AKLOG_SOMETHINGSWRONG;
		}
	    }
	}
    }
    else {
	/* Log to all cells in the cells list first */
	for (cur_node = cells.first; cur_node; cur_node = cur_node->next) {
	    memcpy((char *)&cellinfo, cur_node->data, sizeof(cellinfo));
	    if ((status = auth_to_cell(context, cellinfo.cell, cellinfo.realm,
				       &linkedcell)))
		somethingswrong++;
	    else {
		if (linked && linkedcell != NULL) {
		    afs_dprintf("Linked cell: %s\n", linkedcell);
		    if ((status = auth_to_cell(context, linkedcell,
					       cellinfo.realm, NULL)))
			somethingswrong++;
		}
		if (linkedcell != NULL) {
		    free(linkedcell);
		    linkedcell = NULL;
		}
	    }
	}

	/* Then, log to all paths in the paths list */
	for (cur_node = paths.first; cur_node; cur_node = cur_node->next) {
	    if ((status = auth_to_path(context, cur_node->data)))
		somethingswrong++;
	}

	/*
	 * If only one thing was logged to, we'll return the status
	 * of the single call.  Otherwise, we'll return a generic
	 * something failed status.
	 */
	if (somethingswrong && ((cells.nelements + paths.nelements) > 1))
	    status = AKLOG_SOMETHINGSWRONG;
    }

    /* If we are keeping track of zephyr subscriptions, print them. */
    if (zsubs)
	for (cur_node = zsublist.first; cur_node; cur_node = cur_node->next) {
	    printf("zsub: %s\n", cur_node->data);
	}

    /* If we are keeping track of host information, print it. */
    if (hosts)
	for (cur_node = hostlist.first; cur_node; cur_node = cur_node->next) {
	    printf("host: %s\n", cur_node->data);
	}

    exit(status);
}

static int
isdir(char *path, unsigned char *val)
{
    struct stat statbuf;

    if (lstat(path, &statbuf) < 0)
	return (-1);
    else {
	if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
	    *val = TRUE;
	else
	    *val = FALSE;
	return (0);
    }
}

static krb5_error_code
get_credv5(krb5_context context, char *name, char *inst, char *realm,
	   krb5_creds **creds)
{
    krb5_creds increds;
    krb5_error_code r;
    static krb5_principal client_principal = 0;

    afs_dprintf("Getting tickets: %s%s%s@%s\n", name,
	    (inst && inst[0]) ? "/" : "", inst ? inst : "", realm);

    memset(&increds, 0, sizeof(increds));
/* ANL - instance may be ptr to a null string. Pass null then */
    if ((r = krb5_build_principal(context, &increds.server,
				  strlen(realm), realm,
				  name,
				  (inst && strlen(inst)) ? inst : (void *) NULL,
				  (void *) NULL))) {
        return r;
    }


    if (!_krb425_ccache) {
        r = krb5_cc_default(context, &_krb425_ccache);
	if (r)
	    return r;
    }
    if (!client_principal) {
	if (client) {
	    r = krb5_parse_name(context, client,  &client_principal);
	} else {
	    r = krb5_cc_get_principal(context, _krb425_ccache, &client_principal);
	}
	if (r)
	    return r;
    }

    increds.client = client_principal;
    increds.times.endtime = 0;
    if (do524)
	/* Ask for DES since that is what V4 understands */
	get_creds_enctype((&increds)) = ENCTYPE_DES_CBC_CRC;

    if (keytab) {
	r = get_credv5_akimpersonate(context,
				     keytab,
				     increds.server,
				     increds.client,
				     0, 0x7fffffff,
				     NULL,
				     creds /* out */);
    } else {
	r = krb5_get_credentials(context, 0, _krb425_ccache, &increds, creds);
    }
    return r;
}


static int
get_user_realm(krb5_context context, char **realm)
{
    static krb5_principal client_principal = 0;
    krb5_error_code r = 0;

    *realm = NULL;

    if (!_krb425_ccache) {
	r = krb5_cc_default(context, &_krb425_ccache);
	if (r)
	    return r;
    }
    if (!client_principal) {
	if (client) {
	    r = krb5_parse_name(context, client,  &client_principal);
	} else {
	    r = krb5_cc_get_principal(context, _krb425_ccache, &client_principal);
	}
	if (r)
	    return r;
    }

    *realm = extract_realm(context, client_principal);
    if (*realm == NULL)
	return ENOMEM;

    return(r);
}
