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
#if defined(KERNEL)
# include "afs/param.h"
# include "afs/sysincludes.h"
# include "afsincludes.h"
# include "afs_stats.h"
# if !defined(UKERNEL) || defined(USING_SSL)
#  include "k5ssl.h"
# else	/* UKERNEL && !USING_SSL && KERNEL */
#  undef u
#  include <krb5.h>
# endif	/* UKERNEL && !USING_SSL && KERNEL */
#else	/* !KERNEL */
#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strdup(p) strdup(p)
# include <afs/afsutil.h>
# include <auth/cellconfig.h>
# include <stdlib.h>
# include <syslog.h>
# include <stdarg.h>
# include <string.h>
# include <stdio.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <errno.h>
# if defined(USING_SSL)
#  include "k5ssl.h"
# else	/* !USING_SSL && !KERNEL */
#  include <krb5.h>
# endif	/* !USING_SSL && !KERNEL */
#endif	/* !KERNEL */
#include "rx/rx.h"
#include "rx/rxk5.h"
#include "rxk5_tkt.h"
#include "afs/afs_token.h"

static
char* expand_principal_name(
	krb5_context context, 
	krb5_principal princ, 
	int *bufsize /* out */) 
{
	char* buf;
#if !defined(USING_SHISHI)
	int code;

	code = krb5_unparse_name(context, princ, &buf);
	if(code == 0) {
		*bufsize = strlen(buf) + 1;
	} else {
		*bufsize = 0;
	}
#endif
	return buf;
}

/*
 * Free rxk5_creds structure
 */
void rxk5_free_creds(
	krb5_context k5context, 
	rxk5_creds *creds)
{
	krb5_free_creds(k5context, creds->k5creds);
	rxk5_free_str(creds->cell);
	afs_osi_Free(creds, sizeof(rxk5_creds));
}

#define MAX_RXK5_TOKEN_LEN	32000
#define MAX_RXKAD_TOKEN_LEN	12000

/*
 * Free a structure using clever xdr logic.   Most of xdrs is never initialized.  If x_op is
 * XDR_FREE, the rest of it is just ignored.
 */

#if 0
static
int free_rxk5_princ(
	rxk5_principal *princ)
{
        XDR xdrs[1];
        xdrs->x_op = XDR_FREE;
        if (!xdr_rxk5_principal(xdrs, princ)) {
	  return 1;
        }
	return 0;
}
#endif

static 
void parse_rxk5_princ(
	char *str, 
	rxk5_principal *x)
{
	int i;
	char *cp, *ep, *cep, *np;
	
	memset(x, 0, sizeof *x);

	if ((cp = strchr(str, '@'))) {
                x->realm = afs_strdup(cp+1);
                ep = cp;
        } else {
                x->realm = afs_strdup("");
                ep = str + strlen(str);
        }

	if(ep > str)
	  x->name.name_len = 1;

	/* count instances --saves one alloc */
	cep = ep;
	for(cp =  str; cp < cep; ) {
                np = memchr(cp, '/', cep-cp);
                if (!np)
                        break;
                x->name.name_len++;		
                cp = np + 1;		
	}

	x->name.name_val = afs_osi_Alloc(
		x->name.name_len * sizeof *x->name.name_val);
	
        for (i = 0, cp = str; cp < ep; ++i) {
                np = memchr(cp, '/', ep-cp);
                if (!np)
                        np = ep;
                memcpy(x->name.name_val[i] = afs_osi_Alloc(1 + np - cp), cp, np - cp);
                x->name.name_val[i][np-cp] = 0;
                cp = np + 1;
	}
}

/*
 * Format an rxk5_principal structure as a krb5 name.  The equivalent 
 * of krb_unparse_name.  Caller must free.
 */
static
int rxk5_unparse_name(
	rxk5_principal *x, 
	char** s, 
	int *sz)
{
	char *p;
	int ix, len, nlen;
	
	len = 1 /* @, nul */ + strlen(x->realm);
	for(ix = 0; ix < x->name.name_len; ++ix) {
		len += strlen(x->name.name_val[ix]) + 1 /* / */;
	}
	*sz = len + 1;
	p = *s = afs_osi_Alloc(*sz);
	for(ix = 0; ix < x->name.name_len; ++ix) {
		char* pv = x->name.name_val[ix];
		nlen = strlen(pv);
		memcpy(p, pv, nlen);
		p += nlen;
		if(ix != (x->name.name_len - 1))
			*p++ = '/';
	}
	*p++ = '@'; 
	nlen = strlen(x->realm);
	memcpy(p, x->realm, nlen);
	p += nlen;
	*p++ = 0;
	
	return 0;
}

void rxk5_principal_to_krb5_principal( 
	krb5_principal *k5_princ, 
	rxk5_principal *rxk5_princ)
{
	char *name;
	int code, sz;

	code = rxk5_unparse_name(rxk5_princ, &name, &sz);
	code = krb5_parse_name(rxk5_get_context(0), name, k5_princ);
	afs_osi_Free(name, sz);
}

#if 1 && !defined(KERNEL)

print_rxk5_princ(
	struct rxk5_principal *princ)
{
        int i;

        for (i = 0; i < princ->name.name_len; ++i)
                printf ("/%s"+!i, princ->name.name_val[i]);
        printf ("@%s", princ->realm);
}

print_rxk5_key(struct rxk5_key *key)
{
        int i;

        printf ("type=%d length=%d data=", key->keytype, key->m_key.m_key_len);
        for (i = 0; i < key->m_key.m_key_len; ++i)
                printf ("%02x", i[(unsigned char*)key->m_key.m_key_val]);
}

print_rxk5_token(
	struct rxk5_token *token)
{
        int i;

        printf (" client=");
        print_rxk5_princ(&token->client);
        printf ("\n server=");
        print_rxk5_princ(&token->server);
        printf ("\n session=");
        print_rxk5_key(&token->session);
        printf ("\n authtime=%#x starttime=%#x endtime=%#x\n",
                token->authtime, token->starttime, token->endtime);
        printf (" flags=%#x\n", token->flags);
        printf (" ticket=");
        for (i = 0; i < token->k5ticket.k5ticket_len; ++i)
                printf ("%02x", i[(unsigned char*)token->k5ticket.k5ticket_val]);
        printf ("\n");
}

#endif /* debug tokens */

/*
 * Format new-style afs_token using kerberos 5 credentials (rxk5), 
 * caller frees returned memory (of size bufsize).
 */
int
make_afs_token_rxk5(
	krb5_context context,
	char *cell,
	int viceid,
	krb5_creds *creds,
	afs_token **a_token /* out */)
{
	rxk5_token *k5_token;
	char *cp_name, *sp_name;
	int cpname_size, spname_size;

	(*a_token) = (afs_token*) afs_osi_Alloc(sizeof(afs_token));
	memset((*a_token), 0, sizeof(afs_token)); /* skip? */

	(*a_token)->nextcellnumber = 0;
	(*a_token)->cell = afs_strdup(cell);
	(*a_token)->cu->cu_type = CU_K5;

	k5_token = &((*a_token)->cu->cu_u.cu_rxk5);
	k5_token->viceid = viceid;
	cp_name = expand_principal_name(context,
					creds->client, &cpname_size);
	parse_rxk5_princ(cp_name, &k5_token->client);
	sp_name = expand_principal_name(context,
					creds->server, &spname_size);
	parse_rxk5_princ(sp_name, &k5_token->server);
	k5_token->authtime = (creds->times).authtime;
	k5_token->starttime = (creds->times).starttime;
	k5_token->endtime = (creds->times).endtime;
	k5_token->k5ticket.k5ticket_len = (creds->ticket).length;
	k5_token->k5ticket.k5ticket_val = afs_osi_Alloc(k5_token->k5ticket.k5ticket_len);
	memcpy(k5_token->k5ticket.k5ticket_val, (creds->ticket).data, 
	       k5_token->k5ticket.k5ticket_len);

#if USING_HEIMDAL
	k5_token->session.keytype = (creds->session).keytype;
	k5_token->session.m_key.m_key_len = (creds->session).keyvalue.length;
	k5_token->session.m_key.m_key_val = 
		afs_osi_Alloc(k5_token->session.m_key.m_key_len);
	memcpy(k5_token->session.m_key.m_key_val, (creds->session).keyvalue.data, 
	       k5_token->session.m_key.m_key_len);
	k5_token->flags = (creds->flags.i);
#else
	k5_token->session.keytype = (creds->keyblock).enctype;
	k5_token->session.m_key.m_key_len = (creds->keyblock).length;
	k5_token->session.m_key.m_key_val = 
		afs_osi_Alloc(k5_token->session.m_key.m_key_len);
	memcpy(k5_token->session.m_key.m_key_val, (creds->keyblock).contents, 
	       k5_token->session.m_key.m_key_len);
	k5_token->flags = (creds->ticket_flags);
#endif

	afs_osi_Free(cp_name, cpname_size);
	afs_osi_Free(sp_name, spname_size);

	return 0;
}

/* 
 * Converts afs_token structure to an rxk5_creds structure, which is returned
 * in creds.  Caller must free.
 */
int afs_token_to_rxk5_creds(
	afs_token *a_token, 
	rxk5_creds **creds)
{
	int code;
	rxk5_token *k5_token;

	switch(a_token->cu->cu_type) {
	case CU_K5:
		break;
	default:
		/* bad credential type */
		return -1;
	}

	*creds =  afs_osi_Alloc(sizeof(rxk5_creds));
	if(!*creds)
		return ENOMEM;
	code = afs_token_to_k5_creds(a_token, &((*creds)->k5creds));
	if(code)
		return code;
	k5_token = &(a_token->cu->cu_u.cu_rxk5);
	(*creds)->ViceId = k5_token->viceid;
	(*creds)->cell = afs_strdup(a_token->cell);

	return 0;
}


/* 
 * Converts afs_token structure to a native krb5_creds structure, which is returned
 * in creds.  Caller must free.
 */
int afs_token_to_k5_creds(
	afs_token *a_token, 
	krb5_creds **creds)
{
	rxk5_token *k5_token;
	krb5_creds *k5_creds;

	switch(a_token->cu->cu_type) {
	case CU_K5:
		break;
	default:
		/* bad credential type */
		return -1;
	}

	/* already asserted */
	k5_token = &(a_token->cu->cu_u.cu_rxk5);

	k5_creds = afs_osi_Alloc(sizeof(krb5_creds));
	memset(k5_creds, 0, sizeof(krb5_creds));

	rxk5_principal_to_krb5_principal(&(k5_creds->client), &k5_token->client);
	rxk5_principal_to_krb5_principal(&(k5_creds->server), &k5_token->server);
	(k5_creds->times).authtime = k5_token->authtime;
	(k5_creds->times).starttime = k5_token->starttime;
	(k5_creds->times).endtime = k5_token->endtime;
	(k5_creds->ticket).length = k5_token->k5ticket.k5ticket_len;
	(k5_creds->ticket).data = afs_osi_Alloc((k5_creds->ticket).length);
	memcpy((k5_creds->ticket).data, k5_token->k5ticket.k5ticket_val,
	       (k5_creds->ticket).length);

#if USING_HEIMDAL
	(k5_creds->session).keytype = k5_token->session.keytype;
	(k5_creds->session).keyvalue.length = k5_token->session.m_key.m_key_len;
	(k5_creds->session).keyvalue.data = 
		afs_osi_Alloc((k5_creds->session).keyvalue.length);
	memcpy((k5_creds->session).keyvalue.data, 
	       k5_token->session.m_key.m_key_val, (k5_creds->session).keyvalue.length);
	(k5_creds->flags.i) = k5_token->flags;

	/* omit addresses */
	(k5_creds->addresses).len = 0;
	(k5_creds->addresses).val = (krb5_address*) afs_osi_Alloc(sizeof(krb5_address*));
	memset((k5_creds->addresses).val, 0, sizeof(krb5_address*));
#else
	(k5_creds->keyblock).enctype = k5_token->session.keytype;
	(k5_creds->keyblock).length = k5_token->session.m_key.m_key_len;
	(k5_creds->keyblock).contents = afs_osi_Alloc((k5_creds->keyblock).length);
	memcpy((k5_creds->keyblock).contents, k5_token->session.m_key.m_key_val,
	       (k5_creds->keyblock).length);
	(k5_creds->ticket_flags) = k5_token->flags;
	
	/* omit addresses */
	(k5_creds->addresses) = afs_osi_Alloc(sizeof(krb5_address*));
	*(k5_creds->addresses) = 0;
#endif

	*creds = k5_creds;
	return 0;
}
