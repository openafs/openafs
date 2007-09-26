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
#if defined(USING_K5SSL)
#  include "k5ssl.h"
# else	/* USING_K5SSL */
#  undef u
#  include <krb5.h>
# endif	/* USING_K5SSL */
#else	/* !KERNEL */
#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strdup(p) strdup(p)
# include <afs/afsutil.h>
# ifdef AFS_NT40_ENV
#  include <afs/cellconfig.h>
# else
#  include <auth/cellconfig.h>
# endif
# include <stdlib.h>
# include <stdarg.h>
# include <string.h>
# include <stdio.h>
# include <sys/types.h>
# include <sys/stat.h>
# ifndef AFS_NT40_ENV
#  include <unistd.h>
#  include <syslog.h>
# endif
# include <errno.h>
#include "rxk5_utilafs.h"
#endif	/* !KERNEL */
#include "rx/rx.h"
#include "rxk5_tkt.h"
#include "afs_token_protos.h"
#include "rx/rxk5.h"
#include "afs/afs_token.h"

/*
 * Free rxk5_creds structure
 */
void
rxk5_free_creds(krb5_context k5context, 
	rxk5_creds *creds)
{
	krb5_free_cred_contents(k5context, creds->k5creds);
	afs_osi_Free(creds, sizeof(rxk5_creds));
}

void
rxk5_principal_to_krb5_principal(krb5_principal *k5_princ, 
    k5_principal *rxk5_princ)
{
    int code;
    krb5_principal princ;
    krb5_context context = 0;
    int i, l;
    char *cp;
#if defined(USING_HEIMDAL)
    heim_general_string *comp;
    heim_general_string realm = NULL;
#endif

    *k5_princ = 0;
    princ = afs_osi_Alloc(sizeof *princ);
    if (!princ) goto Failed;
    memset(princ, 0, sizeof *princ);
    l = rxk5_princ->name.name_len;
#if defined(USING_HEIMDAL)
    comp = afs_osi_Alloc(l * sizeof(*comp));
    if (!(realm = afs_strdup(rxk5_princ->k5realm))) goto Failed;
    for (i = 0; i < l; ++i) {
      comp[i] = afs_strdup(rxk5_princ->name.name_val[i]);
    }
    (princ)->name.name_type = KRB5_NT_PRINCIPAL;
    (princ)->name.name_string.val = comp;
    (princ)->name.name_string.len = l;
    (princ)->realm = realm;
#else
    krb5_princ_type(context, princ) = KRB5_NT_PRINCIPAL;
    if (!(cp = afs_strdup(rxk5_princ->k5realm))) goto Failed;
    krb5_princ_realm(context, princ)->data = cp;
    krb5_princ_realm(context, princ)->length = strlen(cp);
    krb5_princ_name(context, princ) = afs_osi_Alloc(l * sizeof(krb5_data));
    if (!krb5_princ_name(context, princ)) goto Failed;
    memset(krb5_princ_name(context, princ), 0, l * sizeof(krb5_data));
    krb5_princ_size(context, princ) = l;
    for (i = 0; i < l; ++i) {
	cp = afs_strdup(rxk5_princ->name.name_val[i]);
	if (!cp) goto Failed;
	krb5_princ_component(context, princ, i)->data = cp;
	krb5_princ_component(context, princ, i)->length = strlen(cp);
    }
#endif
    *k5_princ = princ;
    princ = 0;
Failed:
    krb5_free_principal(context, princ);
}

int
krb5_principal_to_rxk5_principal(krb5_principal princ,
    k5_principal *k5_princ)
{
    krb5_context context = rxk5_get_context(0);
    int code;
    char *cp;
    int i, sl, nl;
    XDR xdrs[1];

    code = ENOMEM;
    memset(k5_princ, 0, sizeof *k5_princ);
#if defined(USING_HEIMDAL)
    k5_princ->k5realm = afs_strdup((princ)->realm);
    if(!k5_princ->k5realm) goto Done;
    nl = (princ)->name.name_string.len;
    k5_princ->name.name_len = nl;
    k5_princ->name.name_val = afs_osi_Alloc(nl * sizeof(k5component));
    if (!k5_princ->name.name_val) goto Done;
    for (i = 0; i < nl; ++i) {
	k5_princ->name.name_val[i] = 
	  afs_strdup((princ)->name.name_string.val[i]);
	if (!k5_princ->name.name_val[i])
	    goto Done;      
    }   
#else
    cp = krb5_princ_realm(context, princ)->data;
    sl = krb5_princ_realm(context, princ)->length;
    k5_princ->k5realm = afs_osi_Alloc(sl + 1);
    if (!k5_princ->k5realm) goto Done;
    memcpy(k5_princ->k5realm, cp, sl);
    sl[k5_princ->k5realm] = 0;
    nl = krb5_princ_size(context, princ);
    k5_princ->name.name_len = nl;
    k5_princ->name.name_val = afs_osi_Alloc(nl * sizeof(k5component));
    if (!k5_princ->name.name_val) goto Done;
    memset(k5_princ->name.name_val, 0, nl * sizeof (k5component));
    for (i = 0; i < nl; ++i) {
	cp = krb5_princ_component(context, princ, i)->data;
	sl = krb5_princ_component(context, princ, i)->length;
	k5_princ->name.name_val[i] = afs_osi_Alloc(sl + 1);
	if (!k5_princ->name.name_val[i])
	    goto Done;
	memcpy(k5_princ->name.name_val[i], cp, sl);
	sl[k5_princ->name.name_val[i]] = 0;
    }
#endif
    code = 0;
Done:
    if (code) {
	xdrs->x_op = XDR_FREE;
	xdr_k5_principal(xdrs, k5_princ);
	memset(k5_princ, 0, sizeof *k5_princ);
    }
    return code;
}

#if 1 && !defined(KERNEL)

print_rxk5_princ(
	struct k5_principal *princ)
{
        int i;

        for (i = 0; i < princ->name.name_len; ++i)
                printf ("/%s"+!i, princ->name.name_val[i]);
        printf ("@%s", princ->k5realm);
}

print_rxk5_key(struct token_tagged_data *key)
{
        int i;

        printf ("type=%d length=%d data=", key->tag, key->tag_data.tag_data_len);
        for (i = 0; i < key->tag_data.tag_data_len; ++i)
                printf ("%02x", i[(unsigned char*)key->tag_data.tag_data_val]);
}

print_rxk5_token(
	token_rxk5 *token)
{
        int i;

        printf (" client=");
        print_rxk5_princ(&token->k5_client);
        printf ("\n server=");
        print_rxk5_princ(&token->k5_server);
        printf ("\n session=");
        print_rxk5_key(&token->k5_session);
        printf ("\n authtime=%#x starttime=%#x endtime=%#x\n",
                Int64ToInt32(token->k5_authtime),
		Int64ToInt32(token->k5_starttime),
		Int64ToInt32(token->k5_endtime));
        printf (" flags=%#x\n", token->k5_flags);
        printf (" ticket=");
        for (i = 0; i < token->k5_ticket.k5_ticket_len; ++i)
                printf ("%02x", i[(unsigned char*)token->k5_ticket.k5_ticket_val]);
        printf ("\n");
}

#endif /* debug tokens */

/*
 * Format new-style afs_token using kerberos 5 credentials (rxk5), 
 * caller frees returned memory (of size bufsize).
 */
int
add_afs_token_rxk5(krb5_context context,
    krb5_creds *creds,
    pioctl_set_token *a_token)
{
    afstoken_soliton at[1];
    token_rxk5 *k5_token = &at->afstoken_soliton_u.at_rxk5;
    XDR xdrs[1];
    int code;

    memset(at, 0, sizeof *at);
    at->at_type = AFSTOKEN_UNION_K5;

    code = krb5_principal_to_rxk5_principal(creds->client, &k5_token->k5_client);
    if (code) goto Done;
    code = krb5_principal_to_rxk5_principal(creds->server, &k5_token->k5_server);
    if (code) goto Done;
    code = ENOMEM;
    FillInt64(k5_token->k5_authtime, 0, creds->times.authtime)
    FillInt64(k5_token->k5_starttime, 0, creds->times.starttime)
    FillInt64(k5_token->k5_endtime, 0, creds->times.endtime)
    FillInt64(k5_token->k5_renew_till, 0, creds->times.renew_till)
    k5_token->k5_ticket.k5_ticket_len = creds->ticket.length;
    k5_token->k5_ticket.k5_ticket_val = creds->ticket.data;

#if USING_HEIMDAL
    k5_token->k5_session.tag = creds->session.keytype;
    k5_token->k5_session.tag_data.tag_data_len = creds->session.keyvalue.length;
    k5_token->k5_session.tag_data.tag_data_val = creds->session.keyvalue.data;
    k5_token->k5_flags = creds->flags.i;
#else
    k5_token->k5_session.tag = creds->keyblock.enctype;
    k5_token->k5_session.tag_data.tag_data_len = creds->keyblock.length;
    k5_token->k5_session.tag_data.tag_data_val = creds->keyblock.contents;
    k5_token->k5_flags = creds->ticket_flags;
#endif
    code = add_afs_token_soliton(a_token, at);
Done:
    k5_token->k5_ticket.k5_ticket_val = 0;
    k5_token->k5_session.tag_data.tag_data_val = 0;
    xdrs->x_op = XDR_FREE;
    xdr_afstoken_soliton(xdrs, at);
    return code;
}

/* 
 * Converts afs_token structure to an rxk5_creds structure, which is returned
 * in creds.  Caller must free.
 */
int afs_token_to_rxk5_creds(pioctl_set_token *a_token, 
    rxk5_creds **creds)
{
    int code;
    rxk5_creds *temp;
    krb5_context context = rxk5_get_context(0);

    temp = afs_osi_Alloc(sizeof(rxk5_creds));
    if (!temp)
	return ENOMEM;
    memset(temp, 0, sizeof *temp);
    code = afstoken_to_v5cred(a_token, temp->k5creds);
    if(!code)
	*creds = temp;
    else
	rxk5_free_creds(context, temp);

    return 0;
}

/* copy bits of an rxkad token into a k5 credential */
int
afstoken_to_v5cred(pioctl_set_token *a_token, krb5_creds *v5cred)
{
    afstoken_soliton at[1];
    XDR xdrs[1];
    int code;

    memset(v5cred, 0, sizeof *v5cred);
    code = afstoken_to_soliton(a_token, AFSTOKEN_UNION_K5, at);
    if (code) goto Failed;

    rxk5_principal_to_krb5_principal(&(v5cred->client), &at->afstoken_soliton_u.at_rxk5.k5_client);
    rxk5_principal_to_krb5_principal(&(v5cred->server), &at->afstoken_soliton_u.at_rxk5.k5_server);
    v5cred->times.authtime = Int64ToInt32(at->afstoken_soliton_u.at_rxk5.k5_authtime);
    v5cred->times.starttime = Int64ToInt32(at->afstoken_soliton_u.at_rxk5.k5_starttime);
    v5cred->times.endtime = Int64ToInt32(at->afstoken_soliton_u.at_rxk5.k5_endtime);
    v5cred->times.renew_till = Int64ToInt32(at->afstoken_soliton_u.at_rxk5.k5_renew_till);
#if defined(USING_HEIMDAL)
#else
    v5cred->is_skey = at->afstoken_soliton_u.at_rxk5.k5_is_skey;
#endif
    if (!(v5cred->ticket.length = at->afstoken_soliton_u.at_rxk5.k5_ticket.k5_ticket_len))
	;
    else if (!(v5cred->ticket.data = afs_osi_Alloc((v5cred->ticket).length))) {
	code = ENOMEM;
	goto Failed;
    } else
    memcpy(v5cred->ticket.data, at->afstoken_soliton_u.at_rxk5.k5_ticket.k5_ticket_val,
       v5cred->ticket.length);

    if (!(v5cred->second_ticket.length = at->afstoken_soliton_u.at_rxk5.k5_ticket2.k5_ticket_len))
	;
    else if (!(v5cred->second_ticket.data = afs_osi_Alloc((v5cred->second_ticket).length))) {
	code = ENOMEM;
	goto Failed;
    } else
    memcpy(v5cred->second_ticket.data, at->afstoken_soliton_u.at_rxk5.k5_ticket2.k5_ticket_val,
	v5cred->second_ticket.length);
#if USING_HEIMDAL
    v5cred->session.keytype = at->afstoken_soliton_u.at_rxk5.k5_session.tag;
    v5cred->session.keyvalue.length = at->afstoken_soliton_u.at_rxk5.k5_session.tag_data.tag_data_len;
    v5cred->session.keyvalue.data   = at->afstoken_soliton_u.at_rxk5.k5_session.tag_data.tag_data_val;
    v5cred->flags.i = at->afstoken_soliton_u.at_rxk5.k5_flags;

    /* omit addresses */
    v5cred->addresses.len = 0;
    v5cred->addresses.val = (krb5_address*) afs_osi_Alloc(sizeof(krb5_address*));
    memset(v5cred->addresses.val, 0, sizeof(krb5_address*));
#else
    v5cred->keyblock.enctype = at->afstoken_soliton_u.at_rxk5.k5_session.tag;
    v5cred->keyblock.length   = at->afstoken_soliton_u.at_rxk5.k5_session.tag_data.tag_data_len;
    v5cred->keyblock.contents = (void *) at->afstoken_soliton_u.at_rxk5.k5_session.tag_data.tag_data_val;
    v5cred->ticket_flags = at->afstoken_soliton_u.at_rxk5.k5_flags;
    
    /* omit addresses */
    v5cred->addresses = afs_osi_Alloc(sizeof(krb5_address*));
    *(v5cred->addresses) = 0;
#endif
    v5cred->ticket.length = at->afstoken_soliton_u.at_rxk5.k5_ticket.k5_ticket_len;
    v5cred->ticket.data = at->afstoken_soliton_u.at_rxk5.k5_ticket.k5_ticket_val;
    at->afstoken_soliton_u.at_rxk5.k5_ticket.k5_ticket_val = 0;
    at->afstoken_soliton_u.at_rxk5.k5_session.tag_data.tag_data_val = 0;
Failed:
    xdrs->x_op = XDR_FREE;
    xdr_afstoken_soliton(xdrs, at);
    return code;
}
