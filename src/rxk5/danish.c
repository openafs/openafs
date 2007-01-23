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

#include "afsconfig.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "afs/stds.h"
#ifdef USING_SSL
#include "k5ssl.h"
#else
#include <krb5.h>
#define krb5i_check_transited_list(a,b,c,d) krb5_check_transited_list(a,&b->tr_contents,c,d)
#endif

/*
 * compare with:
 * krb5_rd_req_decoded		in krb5/src/lib/krb5/krb/rd_req_dec.c
 * differences:
 * /1/ key may not come from keytab
 *	(krb5_rd_req_decoded: can override with auth_context->keyblock? )
 * /2/ no authenticator
 *	-> can't do krb5_principal_compare & err with KRB5KRB_AP_ERR_BADMATCH
 *	-> can't do rcache stuff; it needs tktauthent.authenticator.
 *		(krb5_rd_req_decoded: optional anyways;
 *		can not set auth_context->rcache)
 *	-> can't check authentp->ctime & err KRB5KRB_AP_ERR_SKEW
 * /3/ don't do krb5_address_search & err KRB5KRB_AP_ERR_BADADDR
 *	(krb5_rd_req_decoded: can not set auth_context->remote_addr)
 * /4/ don't do krb5_validate_times
 *	have to do this elsewhere in rxk5 anyways; must handle per-packet
 *	to age connection.
 * /5/ don't do permitted enctype & err KRB5_NOPERM_ETYPE
 *	(krb5_rd_req_decoded: could use auth_context->auth_context_flags
 *	& KRB5_AUTH_CONTEXT_PERMIT_ALL, could then check auth_context->keyblock
 *	or ticket->enc_part2->session later)
 * /6/ does not free ticket->enc_part2 on error.
 * /7/ only does "_SINGLE_HOP_ONLY" case.
 *	(this looks like a mess anyways.)
 *
 * ignored here (but should pass out):
 * ticket->enc_part2->caddrs
 * ticket->enc_part2->session->enctype	(app can check this later)
 * ticket->enc_part2->server
 *
 * check here:
 * ticket->enc_part2->transited
 * ticket->enc_part2->flags	& TKT_FLG_INVALID
 *
 * Products needed later (enc_part2):
 * ticket->enc_part2->session;
 * ticket->enc_part2->client;
 * ticket->enc_part2->times.startTime, endTime
 */

krb5_error_code
krb5_danish_surprise(krb5_context context,
    krb5_keyblock *key,
    krb5_ticket *ticket)
{
    krb5_error_code code;
    krb5_data *realm;
    krb5_transited *trans;

    code = krb5_decrypt_tkt_part(context, key, ticket);
    if (code) goto Done;

    trans = &ticket->enc_part2->transited;
    realm = &ticket->enc_part2->client->realm;
    if (trans->tr_contents.data && *trans->tr_contents.data) {
#if 0
	/* not single-hop at most */
	code = KRB5KRB_AP_ERR_ILL_CR_TKT;
#else
	/* the general case */
	code = krb5i_check_transited_list(context, trans,
	    realm, &ticket->server->realm);
#endif
	goto Done;
    }

    if (ticket->enc_part2->flags & TKT_FLG_INVALID) {	/* ie, KDC_OPT_POSTDATED */
	code = KRB5KRB_AP_ERR_TKT_INVALID;
	goto Done;
    }
Done:
    return code;
}
