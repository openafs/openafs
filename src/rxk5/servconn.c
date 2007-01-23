/*
 * Copyright (c) 2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

/*
 * application connect logic.
 */

#include "afsconfig.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <afs/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#ifdef CONFIG_K4
#ifdef HAVE_KERBEROSIV_DES_H
#include <kerberosIV/des.h>
#endif
#ifdef HAVE_DES_H
#include <des.h>
#endif
#ifdef HAVE_KERBEROSIV_KRB_H
#include <kerberosIV/krb.h>
#endif
#ifdef HAVE_KRB_H
#include <krb.h>
#endif
#endif
#ifdef CONFIG_K5
#ifdef USING_SHISHI
#include <shishi.h>
#define ERROR_TABLE_BASE_SHI5 (1283619328L)
#endif
#ifdef USING_SSL
#include "k5ssl.h"
#endif
#if defined(USING_MIT) || defined(USING_HEIMDAL)
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#include <krb5.h>
#endif
#include <assert.h>
#endif
#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#ifdef CONFIG_RXKAD
#include <rx/rxkad.h>
#endif
#ifdef CONFIG_RXK5
#include "rxk5.h"
#include "rxk5errors.h"
#endif
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if ibm032
#define NO_VFPRINTF 1
#endif
#ifndef NO_VFPRINTF
#if defined(__STDC__) || defined(_IBMR2)
#define USE_STDARG 1
#else
#define USE_VARARGS 1
#endif
#endif

#ifdef USE_VARARGS
#include <varargs.h>
#endif
#ifdef USE_STDARG
#include <stdarg.h>
#endif
#ifdef ibm032
extern int errno;
#endif

/* #include "spsubr.h" */

#include "servconn.h"

int general_debug;
#if 0
int utest_mallocseed;
#endif

#include "str.h"

#if NO_VFPRINTF
lcprintf(fmt, args)
	char *fmt, *args;
{
	_doprnt(fmt, &args, stderr);
}
elcprintf(fmt, args)
	char *fmt, *args;
{
	_doprnt(fmt, &args, stderr);
}
#endif

#if USE_VARARGS
int lcprintf(fmt, va_alist)
	va_dcl
	char *fmt;
{
	va_list args;

	va_start(args);
	vfprintf(stderr, fmt, args);
	va_end (args);
	return 0;
}
int elcprintf(fmt, va_alist)
	va_dcl
	char *fmt;
{
	va_list args;

	va_start(args);
	vfprintf(stderr, fmt, args);
	va_end (args);
	return 0;
}
#endif

#ifdef USE_STDARG
int lcprintf(char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end (args);
	return 0;
}
int elcprintf(char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end (args);
	return 0;
}
#endif

#ifdef CONFIG_K4
#if 0
/* The maximum sizes for aname, realm, sname, and instance +1 */
#define 	ANAME_SZ	40
#define		REALM_SZ	40
#define		SNAME_SZ	40
#define		INST_SZ		40
/* include space for '.' and '@' */

/* Definition of text structure used to pass text around */
#define		MAX_KTXT_LEN	1250

typedef unsigned char des_cblock[8];	/* crypto-block size */
#define C_Block des_cblock

struct ktext {
    int     length;		/* Length of the text */
    unsigned char dat[MAX_KTXT_LEN];	/* The data itself */
    unsigned long mbz;		/* zero to catch runaway strings */
};

typedef struct ktext *KTEXT;
typedef struct ktext KTEXT_ST;

#ifndef MAXKTCTICKETLEN
#define MAXKTCTICKETLEN 344	/* should be 12000 */
#endif


/* Parameters for rd_ap_req */
/* Maximum alloable clock skew in seconds */

/* Structure definition for credentials returned by get_cred */

struct credentials {
    char    service[ANAME_SZ];	/* Service name */
    char    instance[INST_SZ];	/* Instance */
    char    realm[REALM_SZ];	/* Auth domain */
    C_Block session;		/* Session key */
    int     lifetime;		/* Lifetime */
    int     kvno;		/* Key version number */
    KTEXT_ST ticket_st;		/* The ticket itself */
    long    issue_date;		/* The issue time */
    char    pname[ANAME_SZ];	/* Principal's name */
    char    pinst[INST_SZ];	/* Principal's instance */
};

typedef struct credentials CREDENTIALS;
#endif
#endif

/*
 * rx_krb_cred
 * generic structure combining rx-relevant information for k4 and k5.
 */
struct rx_krb_cred {
	void *session_key;
	int kvno;
	int ticket_length;
	void *ticket_data;
};

union k45_cred {
#ifdef CONFIG_K4
	CREDENTIALS k4;
#endif
#ifdef CONFIG_K5
#ifdef USING_SHISHI
	Shishi_tkt *k5;
#else
	krb5_creds *k5;
#endif
#endif
	char dummy[12000];
};

#if defined(CONFIG_RXKAD) && defined(CONFIG_K4)
static int
k4_get_cred(char *service, char *instance, char *realm,
            union k45_cred *cred, struct rx_krb_cred *rkcred)
{
	CREDENTIALS *cr = &cred->k4;
	int code;
	char realm[MAXKTCREALMLEN];
	char instance[MAXKTCNAMELEN];
	char service[MAXKTCNAMELEN];
	memset(realm, 0, sizeof realm);
	memset(instance, 0, sizeof instance);
	memset(service, 0, sizeof service);
	code = my_ParseLoginName (sps->krbname, service, instance, realm);
	if (code) return code;


	code = krb_get_cred(service, instance, realm, cr);
	if (code)
	{
		code = get_ad_tkt(service, instance, realm, 255);
		if (code) {
			printf ("Can't get ad ticket for %s.%s@%s krberr=%d unixerr=%d\n",
				service, instance, realm, code, errno);
			return code + 39525376L;
		}
		code = krb_get_cred(service, instance, realm, cr);
		if (code) {
			printf ("Can't get credentials for %s.%s@%s krberr=%d unixerr=%d\n",
				service, instance, realm, code, errno);
			return code + 39525376L;
		}
	}
	rkcred->session_key = &cr->session;
	rkcred->kvno = cr->kvno;
	rkcred->ticket_length = cr->ticket_st.length;
	rkcred->ticket_data = cr->ticket_st.dat;
	return 0;
}
#endif

#ifdef CONFIG_K5
#if defined(CONFIG_RXKAD) && (defined(USING_SHISHI) || !defined(USING_HEIMDAL))
#define SEQUENCE 16
#define CONSTRUCTED 32
#define APPLICATION 64
#define CONTEXT_SPECIFIC 128
static int skip_get_number(pp, lp, np)
	char **pp;
	size_t *lp;
	int *np;
{
	int l, r, n, i;
	char *p;

	l = *lp;
	if (l < 1)
	{
printf ("skip_bad_number: missing number\n");
		return -1;
	}
	p = *pp;
	r = (unsigned char)*p;
	++p; --l;
	if (r & 0x80)
	{
		n = (r&0x7f);
		if (l < n)
		{
printf ("skip_bad_number: truncated number\n");
			return -1;
		}
		r = 0;
		for (i = n; --i >= 0; )
		{
			r <<= 8;
			r += (unsigned char)*p;
			++p; --l;
		}
	}
	*np = r;
	*pp = p;
	*lp = l;
	return 0;
}

static int
skip_ticket_wrapper(tix, tixlen, enc, enclen)
	char *tix, **enc;
	size_t tixlen, *enclen;
{
	char *p = tix;
	size_t l = tixlen;
	int code;
	int num;

	if (l < 1) return -1;
	if (*p != (char) (CONSTRUCTED+APPLICATION+1)) return -1;
	++p; --l;
	if ((code = skip_get_number(&p, &l, &num))) return code;
	if (l != num) return -1;
	if (l < 1) return -1;
	if (*p != (char)(CONSTRUCTED+SEQUENCE)) return -1;
	++p; --l;
	if ((code = skip_get_number(&p, &l, &num))) return code;
	if (l != num) return -1;
	if (l < 1) return -1;
	if (*p != (char)(CONSTRUCTED+CONTEXT_SPECIFIC+0)) return -1;
	++p; --l;
	if ((code = skip_get_number(&p, &l, &num))) return code;
	if (l < num) return -1;
	l -= num; p += num;
	if (l < 1) return -1;
	if (*p != (char)(CONSTRUCTED+CONTEXT_SPECIFIC+1)) return -1;
	++p; --l;
	if ((code = skip_get_number(&p, &l, &num))) return code;
	if (l < num) return -1;
	l -= num; p += num;
	if (l < 1) return -1;
	if (*p != (char)(CONSTRUCTED+CONTEXT_SPECIFIC+2)) return -1;
	++p; --l;
	if ((code = skip_get_number(&p, &l, &num))) return code;
	if (l < num) return -1;
	l -= num; p += num;
	if (l < 1) return -1;
	if (*p != (char)(CONSTRUCTED+CONTEXT_SPECIFIC+3)) return -1;
	++p; --l;
	if ((code = skip_get_number(&p, &l, &num))) return code;
	if (l != num) return -1;
	*enc = p;
	*enclen = l;
	return 0;
}
#endif

static int
k5_get_cred(char *service,
            union k45_cred *cred, struct rx_krb_cred *rkcred)
{
#ifdef USING_SHISHI
	Shishi *ctx = 0;
	Shishi_tkt *tkt;
	Shishi_tkts_hint hint[1];
	char *cname;
	int code;
	size_t clen;
#ifdef CONFIG_RXKAD
	Shishi_key *sk;
	Shishi_asn1 ticket;
	struct {
	    char *data; size_t length;
	} enc_part[1], temp[1];
#endif
#else
	krb5_context ctx = 0;
	krb5_ccache cc;
	krb5_creds in_creds[1];
	char *p;
	char msgbuf[512];
#ifdef CONFIG_RXKAD
	krb5_creds *cr;
#ifdef USING_HEIMDAL
	Ticket ticket[1];
	size_t len;
#else
	krb5_ticket *ticket = NULL;
#endif
	krb5_data enc_part[1];
#endif
	krb5_error_code code;
#endif
	char *when;

#ifdef USING_SHISHI
	memset(hint, 0, sizeof(*hint));
#else
	/* Initialization */
	memset(&cc, 0, sizeof(cc));
	memset(in_creds, 0, sizeof(*in_creds));
#if defined(CONFIG_RXKAD) && defined(USING_HEIMDAL)
	memset(ticket, 0, sizeof *ticket);
#endif
#endif
#ifdef CONFIG_RXKAD
	memset(enc_part, 0, sizeof *enc_part);
#endif

	when = "initializing context";
#ifdef USING_SHISHI
	code = shishi_init (&ctx);
	if (code) { code += ERROR_TABLE_BASE_SHI5; goto Out; }
#else
	code = krb5_init_context(&ctx);
	if (code) goto Out;
#endif

#ifndef USING_SHISHI
	when = "determining default ccache";
	code = krb5_cc_default(ctx, &cc);
	if (code) goto Out;
#endif

	when = "determining client principal from ccache";
#ifdef USING_SHISHI
	tkt = shishi_tkts_nth(shishi_tkts_default(ctx), 0);
	if (tkt)
	{
		code = shishi_tkt_clientrealm(tkt, &cname, &clen);
	}
	else
	cname = (char *) shishi_principal_default(ctx);	/* XXX dubious... */
#else
	code = krb5_cc_get_principal(ctx, cc, &in_creds->client);
	if (code) goto Out;
#endif

	/*
	* Build k5 service principal
	*/
	when = "parsing service principal";
#ifdef USING_SHISHI
	code = shishi_parse_name(ctx, service, &hint->server, &hint->serverrealm);
	if (code) { code += ERROR_TABLE_BASE_SHI5; goto Out; }
#else
	code = krb5_parse_name(ctx, service, &in_creds->server);
	if (code) goto Out;
#endif

#ifdef USING_SHISHI
	when  = "getting credentials";
	cred->k5 = shishi_tkts_get(shishi_tkts_default(ctx), hint);
	if (!cred->k5) { code = EDOM /* XXX */; goto Out; }
#else
	when  = "unparsing service";
	code = krb5_unparse_name(ctx, in_creds->server, &p);
	if (code) goto Out;
	when  = msgbuf;
	sprintf(msgbuf,"getting credentials for %s", p);
	free(p);
	code = krb5_get_credentials(ctx, 0 /* cc flags */,
		cc, in_creds, &cred->k5);
	if (code) goto Out;
#endif
	if (!rkcred) goto Out;
	code = ENXIO;
#ifdef CONFIG_RXKAD
	/*
	 * For information on rxkad 2b extension, see
	 *     http://grand.central.org/dl/doc/protocol/rx/rxkad-2b.html
	 */
#ifdef USING_SHISHI

	sk = shishi_tkt_key(cred->k5);
	if (!sk) { code = EDOM /* XXX */; goto Out; }

	if (shishi_key_length(sk) != 8)
	{
		fprintf(stderr,"ERROR: got type %d which is not des!\n",
			shishi_key_type(sk));
		code = ENXIO;
		goto Out;
	}
	rkcred->session_key = shishi_key_value(sk);
	/* should probably copy... */
#else
	cr = cred->k5;
#ifdef USING_HEIMDAL
	rkcred->session_key = cr->session.keyvalue.data;
	if (cr->session.keyvalue.length != 8)
	{
		fprintf(stderr,"ERROR: got type %d which is not des!\n",
			cr->session.keytype);
		code = ENXIO;
		goto Out;
	}
#else
	rkcred->session_key = cr->keyblock.contents;
	if (cr->keyblock.length != 8)
	{
		fprintf(stderr,"ERROR: got type %d which is not des!\n",
			cr->keyblock.enctype);
		code = ENXIO;
		goto Out;
	}
#endif
#endif
	rkcred->kvno = 0x100 - 0x2b; /* 2b-specific version number */

	when = "decoding ticket";
#ifdef USING_SHISHI
	ticket = shishi_tkt_ticket(cred->k5);
	if (!ticket) { code = EDOM /* XXX */; goto Out; }
	code = shishi_asn1_to_der(ctx, ticket, &enc_part->data, &enc_part->length);
	if (code) { code += ERROR_TABLE_BASE_SHI5; goto Out; }
	if (skip_ticket_wrapper(enc_part->data, enc_part->length,
		&temp->data, &temp->length))
	{
fprintf(stderr,"Problem skipping ticket wrapper\n");
		code = EDOM;
		goto Out;
	}
	memmove(enc_part->data, temp->data, temp->length);
	enc_part->length = temp->length;
#else
#ifdef USING_HEIMDAL
	code = decode_Ticket(cr->ticket.data, cr->ticket.length, ticket, NULL);
	if (code) goto Out;
	when = "encoding enc_part of ticket";
	enc_part->length = length_EncryptedData(&ticket->enc_part);
	if (!(enc_part->data = malloc(enc_part->length)))
		goto Out;
	code = encode_EncryptedData(enc_part->data+enc_part->length-1,
		enc_part->length,
		&ticket->enc_part, &len);
#else
	if (skip_ticket_wrapper(cr->ticket.data, cr->ticket.length,
		&enc_part->data, &enc_part->length))
	{
fprintf(stderr,"Problem skipping ticket wrapper\n");
		code = EDOM;
		goto Out;
	}
	code = 0;
#endif
	if (code) goto Out;
#endif

#ifdef KRB524_ENCFULL
	code = KRB524_ENCFULL;
	when = "encoded enc_part is too large";
	if (enc_part->length >= MAXKTCTICKETLEN) goto Out;
#endif

	code = 0;
	rkcred->ticket_length = enc_part->length;
	rkcred->ticket_data = enc_part->data;
	memset(enc_part, 0, sizeof *enc_part);	/* since we stole the data */
#endif

Out:
	if (code) com_err(__FUNCTION__, code, when);
#ifdef USING_SHISHI
	if (hint->server)
		free(hint->server);
	if (hint->serverrealm)
		free(hint->serverrealm);
#ifdef CONFIG_RXKAD
	if (enc_part->data) free(enc_part->data);
#endif
	if (ctx) shishi_done(ctx);
#else
#ifdef USING_HEIMDAL
#ifdef CONFIG_RXKAD
        krb5_data_free(enc_part);
	if (ticket->realm)
		free_Ticket(ticket);
#endif
#else
#ifdef CONFIG_RXKAD
	if (ticket) krb5_free_ticket(ctx, ticket);
#endif
#endif
	krb5_free_cred_contents(ctx, in_creds);
	if (ctx) krb5_free_context(ctx);
#endif

	return code;
#endif
}

int getservconn(sps, stash, connp)
	struct servparm *sps;
	struct servstash *stash;
	struct rx_connection **connp;
{
	struct rx_connection *conn, *new;
	long hostipaddr;
	struct rx_securityClass *sc;
	int si;
	struct hostent *hp;
	int code;
	union k45_cred kcred[1];
	struct rx_krb_cred rkcred[1];

	if ((conn = stash->savedconn))
	{
		if (stash->change_count != sps->changed)
		{
			stash->savedconn = 0;
			rx_DestroyConnection(conn);
			conn = 0;
		}
		else if (rx_ConnError(conn))
		{
			new = rx_NewConnection(rx_HostOf(rx_PeerOf(conn)),
				rx_PortOf(rx_PeerOf(conn)),
				rx_ServiceIdOf(conn),
				rx_SecurityObjectOf(conn),
				rx_SecurityClassOf(conn));
			rx_DestroyConnection(conn);
			stash->savedconn = conn = new;
		}
		if (conn)
		{
			*connp = conn;
			return 0;
		}
	}

	if (isdigit(*sps->host))
		hostipaddr = inet_addr(sps->host);
	else if (!(hp = gethostbyname(sps->host)))
	{
		elcprintf ("getservconn: can't find host <%s>\n", sps->host);
		return -1;
	} else {
		memcpy((char*)&hostipaddr, hp->h_addr, sizeof hostipaddr);
	}
	memset(rkcred, 0, sizeof *rkcred);
	si = -1;
	if (!*sps->krbname) {
		sc = (struct rx_securityClass *) rxnull_NewClientSecurityObject();
		si = 0;
		code = 0;
	} else {
		code = ENXIO;
#if defined(CONFIG_RXKAD) && defined(CONFIG_K4)
		if (sps->securityindex == 2)
		  code = k4_get_cred(sps->krbname, kcred, rkcred);
		if (code)
#endif
#ifdef CONFIG_K5
		code = k5_get_cred(sps->krbname, kcred,
			sps->securityindex == 2 ? rkcred : 0);
		if (code)
#endif
		return code;
	}
#ifdef CONFIG_RXKAD
	if (sps->securityindex == 2) {
		sc = (struct rx_securityClass *) rxkad_NewClientSecurityObject(
			rxkad_crypt,
			rkcred->session_key,
			rkcred->kvno,
			rkcred->ticket_length,
			rkcred->ticket_data);
		si = 2;
	}
#endif
#ifdef CONFIG_RXK5
	if (sps->securityindex == 5) {
		sc = (struct rx_securityClass *) rxk5_NewClientSecurityObject(
			sps->level,
			kcred->k5, 0);
		si = 5;
	}
#endif
	if (si == -1)
	{
		elcprintf("getservconn: no authentication support si=%d <%s>\n",
			sps->securityindex, sps->krbname);
		return ENXIO;
	}
	if (!sc)
	{
		elcprintf ("getservconn: something went wrong making security object\n");
		return -1;
	}
	conn = rx_NewConnection(hostipaddr,
		htons(sps->port),
		sps->serviceid,
		sc,
		si);
	if (!conn)
	{
		elcprintf ("getservconn: can't connect to host <%s>\n", sps->host);
		return -1;
	}
	stash->savedconn = conn;
	*connp = conn;
#ifdef CONFIG_RXKAD
#ifdef init_RXK_err_tbl
initialize_RXK_error_table();
#else
#ifdef VOID
initialize_rxk_error_table();
#endif
#endif
#endif
#ifdef CONFIG_RXK5
#ifdef init_RXK5_err_tbl
initialize_RXK5_error_table();
/* printf ("initialize_RXK5_error_table\n"); */
#else
#ifdef VOID
#ifndef AFS_DARWIN_ENV	/* Hmmm... */
initialize_rxk5_error_table();
#endif
/* printf ("initialize_rxk5_error_table\n"); */
#endif
#endif
#endif
	return 0;
}

int setservhost(sps, cp, word)
	struct servparm *sps;
	char *cp, *word;
{
	int f;
	char *t;

	f = 0;
	++sps->changed;
	cp = skipspace(cp);
	while (*cp)
	{
		cp = getword(cp, word);
		t = 0;
		if (!f && (t = strchr(word, ':')))
		{
			*t++ = 0;
			sps->port = atoi(t);
		}
		switch(f++)
		{
		case 0:
			if (*word && strcmp(word, "-"))
				sps->host = strdup(word);
			break;
		case 1:
			sps->port = atoi(word);
			break;
		default:
			fprintf(stderr,"Extra/bad host parameter: <%s>\n", word);
			return 1;
		}
		f += !!t;
	}
	return 0;
}

int setservkrbname(sps, cp, word)
	struct servparm *sps;
	char *cp, *word;
{
	int f;
	char *t, *w;

	++sps->changed;
	f = 0;
	cp = skipspace(cp);
	while (*cp)
	{
		cp = getword(cp, word);
		for (w = word;w;w = t)
		{
			t = strchr(w, ':');
			if (t) *t++ = 0;
			switch(f++)
			{
			case 0:
				if (!strcmp(w, "-")) *w = 0;
				sps->krbname = strdup(w);
				break;
			case 1:
				if (strcmp(w, "-"))
					sps->securityindex = atoi(w);
				break;
			case 2:
				if (strcmp(w, "-"))
					sps->level = atoi(w);
				break;
			default:
				fprintf(stderr,
"Extra/bad krbname parameter: <%s>\n",
					w);
				return 1;
			}
			
		}
	}
	return 0;
}
