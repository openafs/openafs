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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <afs/stds.h>
#include <afsconfig.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#ifdef USING_SHISHI
#include <shishi.h>
#else
#ifdef USING_SSL
#include "k5ssl.h"
#else
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#include <krb5.h>
#endif
#endif
#include <assert.h>
#include <errno.h>
#include <com_err.h>
#include <string.h>

#include "rxk5imp.h"
#include "rxk5c.h"
#include "rxk5errors.h"

#if defined(USING_MIT) || defined(USING_SSL)
krb5_error_code krb5_danish_surprise(krb5_context,
krb5_keyblock *, krb5_ticket *);
#endif

int
rxk5_s_Close (so)
    struct  rx_securityClass *so;
{
    struct rxk5_sprivate *tsp = (struct rxk5_sprivate*) so->privateData;

    if (--so->refCount > 0)
	return 0;
    rxi_Free(tsp, sizeof *tsp);
    rxi_Free(so, sizeof *so);
    INCSTAT(destroyObject);
    return 0;
}

int
rxk5_s_NewConnection(so, conn)
    struct  rx_securityClass *so;
    struct  rx_connection *conn;
{
    int code;
    if (conn->securityData) code = RXK5INCONSISTENCY;
    else code = rxk5i_NewConnection(so, conn, sizeof(struct rxk5_sconn));
    if (code) rxi_ConnectionError(conn, code);
    return code;
}

int
rxk5_s_DestroyConnection(so, conn)
    struct  rx_securityClass *so;
    struct  rx_connection *conn;
{
    struct rxk5_sconn *sconn;
    struct rxk5_sprivate *tsp = (struct rxk5_sprivate*)so->privateData;

    if ((sconn = (struct rxk5_sconn*)conn->securityData)) {
	conn->securityData = 0;
	INCSTAT(destroyConn[conn->level])
	if (sconn->client)
	    free(sconn->client);
	if (sconn->server)
	    free(sconn->server);
	krb5_free_keyblock_contents(tsp->k5_context, sconn->hiskey);
	krb5_free_keyblock_contents(tsp->k5_context, sconn->mykey);
	rxi_Free(sconn, sizeof *sconn);
    } else {
	INCSTAT(destroyUnused)
    }
    return rxk5_s_Close(so);
}

int
rxk5_s_CheckAuthentication(so, conn)
    struct rx_securityClass *so;
    struct rx_connection *conn;
{
    struct rxk5_sconn *sconn;
    if (!(sconn = (struct rxk5_sconn*)conn->securityData))
	return RXK5NOSECDATA;
    return !(sconn->flags & AUTHENTICATED);
}

int
rxk5_s_CreateChallenge(so, conn)
    struct rx_securityClass *so;
    struct rx_connection *conn;
{
    struct rxk5_sconn *sconn = (struct rxk5_sconn*)conn->securityData;
    struct rxk5_sprivate *tsp = (struct rxk5_sprivate*)so->privateData;

    if (!sconn || !tsp) return RXK5NOSECDATA;
    sconn->challengeid = rxk5i_random_integer(tsp->k5_context);
    sconn->flags = 0;
    sconn->level = tsp->minlevel;
    return 0;
}

int
rxk5_s_GetChallenge(so, conn, p)
    struct rx_securityClass *so;
    struct rx_connection *conn;
    struct rx_packet *p;
{
    struct rxk5_sconn *sconn = (struct rxk5_sconn*)conn->securityData;
    struct rxk5c_challenge hc[1];
    XDR xdrs[1];
    int cs;
    char *cp;

    if (!sconn) return RXK5NOSECDATA;
    hc->vers = 0;
    hc->challengeid = sconn->challengeid;
    hc->level = sconn->level;
    rx_Pullup(p, sizeof *hc);
    cp = rx_data(p, 0, cs);
    xdrmem_create(xdrs, cp, cs, XDR_ENCODE);
    if (!xdr_rxk5c_challenge(xdrs, hc))
{
printf ("rxk5_s_GetChallenge: xdr_rxk5c_challenge failed\n");
	return RXK5INCONSISTENCY;
}
    cs = xdr_getpos(xdrs);
    rx_SetDataSize(p, cs);
    sconn->flags |= TRIED;
    INCSTAT(challengesSent);
    return 0;
}

#ifdef USING_HEIMDAL
#define _krb5_principalname2krb5_principal my_krb5_principalname2krb5_principal
static int
my_krb5_principalname2krb5_principal(krb5_principal *pp,
    const PrincipalName from,
    const Realm realm)
{
    krb5_principal p;
    int code = ENOMEM;

    *pp = 0;
    if (!(p = malloc(sizeof *p))) goto Done;
    memset(p, 0, sizeof *p);
    if (!(p->realm = strdup(realm))) goto Done;
    if ((code = copy_PrincipalName(&from, &p->name))) goto Done;
    *pp = p; p = 0;
    /* code = 0; */
Done:
    if (p) {
	free_Principal(p);
	free(p);
    }
    return code;
}
#endif

int
rxk5_s_CheckResponse(so, conn, p)
    struct rx_securityClass *so;
    struct rx_connection *conn;
    struct rx_packet *p;
{
#define WSIZE (RXK5_MAXKTCTICKETLEN * 3)
    struct rxk5_sconn *sconn = (struct rxk5_sconn*)conn->securityData;
    struct rxk5_sprivate *tsp = (struct rxk5_sprivate*)so->privateData;
    struct rxk5c_response hr[1];
    struct rxk5c_response_encrypted he[1];
    XDR xdrs[1];
    int cs;
    int i;
    char *cp;
    int code = 0;
    krb5_keyblock key[1];
    krb5_data plain[1];
#ifdef USING_SHISHI
    Shishi_asn1 ticket;
    Shishi_asn1 encticketpart;
    Shishi_tkt *tkt;
    Shishi_key *session;
    unsigned int tkt_vno;
    unsigned int epkvno;
    char *realm = 0;
    char *sname = 0;
    char *server = 0;
    char *cname = 0;
    char *crealm = 0;
    char *trdata = 0;
    int etype;
    size_t buflen;
    unsigned int flags;
    krb5_data plaintext[1];
#else
    krb5_keyblock *session;
    krb5_principal server = 0;
#ifdef USING_HEIMDAL
    krb5_data plaintext[1];
    Ticket enctkt[1];
    krb5_ticket ticket[1];
    krb5_crypto crypto = 0;
    size_t len;
    krb5_principal client = 0;
#else
    krb5_ticket *ticket = 0;
    krb5_data tixdata[1];
    krb5_enc_data cipher[1];
    char *work = 0;
#endif
#endif
    int usage;

    if (!sconn || !tsp) return RXK5NOSECDATA;
    memset((void*) hr, 0, sizeof *hr);
    memset((void*) he, 0, sizeof *he);
    memset((void*) key, 0, sizeof *key);
#ifdef USING_SHISHI
    code = shishi_tkt(tsp->k5_context, &tkt);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    memset((void*) plaintext, 0, sizeof *plaintext);
#else
#ifdef USING_HEIMDAL
    memset((void*) enctkt, 0, sizeof *enctkt);
    memset((void*) plaintext, 0, sizeof *plaintext);
    memset((void*) ticket, 0, sizeof *ticket);
#else
    work = osi_Alloc(WSIZE);
    if (!work) {
printf ("rxk5_s_CheckResponse: osi_Alloc(%d) failed\n", WSIZE);
	code = RXK5INCONSISTENCY;
	goto Done;
    }
#endif
#endif
    cs = rx_GetDataSize(p);
    rx_Pullup(p, cs);		/* XXX unconvincing */
    cp = rx_data(p, 0, cs);
    xdrmem_create(xdrs, cp, cs, XDR_DECODE);
    if (!xdr_rxk5c_response(xdrs, hr)) {
printf ("rxk5_s_CheckResponse: xdr_rxk5c_response failed\n");
	code = RXK5TICKETLEN;
	goto Done;
    }
    if (hr->vers) {
	code = RXK5WRONGVERS;
	goto Done;
    }
    sconn->flags |= TRIED;
#ifdef USING_SHISHI
    ticket = shishi_der2asn1_ticket(tsp->k5_context, 
	hr->rxk5c_ticket.rxk5c_ticket_val,
	hr->rxk5c_ticket.rxk5c_ticket_len);
    if (!ticket) {
printf ("rxk5_s_CheckResponse: shishi_der2asn1_ticket failed\n");
	code = RXK5BADTICKET;	/* XXX */
	goto Done;
    }
    shishi_tkt_ticket_set(tkt, ticket);
#if 0
    /* Apparently I need to set kdcreppart for shishi_tkt_clientrealm... */
	/* but shishi_kdcrep_set_ticket is broken... */
    code = shishi_kdcrep_set_ticket(tsp->k5_context,
	shishi_tkt_enckdcreppart(tkt), ticket);
    if (code) {
printf ("PROBLEM: shishi_kdcrep_set_ticket failed %d\n", code);
code += ERROR_TABLE_BASE_SHI5; goto Done; }
#endif
    code = shishi_asn1_read_int32(tsp->k5_context, ticket, "tkt-vno",
	&tkt_vno);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    if (tkt_vno != 5) {
printf ("rxk5_s_CheckResponse: tkt_vno is %d not 5\n", tkt_vno);
#if 0
	code = RXK5BADTICKET;	/* XXX */
	goto Done;
#endif
    }
    code = shishi_asn1_read_int32(tsp->k5_context, ticket, "enc-part.kvno",
	&epkvno);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    code = shishi_ticket_realm_get(tsp->k5_context, ticket, &realm, &buflen);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    code = shishi_ticket_server(tsp->k5_context, ticket, &sname, &buflen);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    code = shishi_ticket_get_enc_part_etype(tsp->k5_context, ticket, &etype);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    server = malloc(strlen(sname) + strlen(realm) + 2);
    if (!server) {
printf ("rxk5_s_CheckResponse: can't allocate space for %s @ %s\n", sname, realm);
code = RXK5INCONSISTENCY; goto Done; }
    sprintf (server, "%s@%s", sname, realm);
#if 0
    code = shishi_ticket_decrypt(tsp->k5_context, ticket, key->sk, &encticketpart);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
#endif
    code = tsp->get_key(tsp->get_key_arg,
	tsp->k5_context,
	server,
	etype,
	epkvno,
	key);
    if (code) goto Done;
    code = shishi_tkt_decrypt(tkt, key->sk);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    /* Apparently I need to set enckdcreppart for shishi_tkt_flags etc. */
    encticketpart = shishi_tkt_encticketpart(tkt);
    code = shishi_enckdcreppart_populate_encticketpart(tsp->k5_context,
	shishi_tkt_enckdcreppart(tkt),
	encticketpart);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
#else
#ifdef USING_HEIMDAL
    if ((code = decode_Ticket((void*)hr->rxk5c_ticket.rxk5c_ticket_val,
	    hr->rxk5c_ticket.rxk5c_ticket_len,
	    enctkt, &len)))
	goto Done;
    /* tkt_vno is the KIND of ticket; *.enc_part.kvno is the KEY kvno */
    if (!enctkt->enc_part.kvno || enctkt->tkt_vno != 5) {
	code = RXK5BADTICKET /* KRB5KRB_AP_ERR_TKT_INVALID */;
	goto Done;
    }
    _krb5_principalname2krb5_principal(&server, enctkt->sname, enctkt->realm);
    code = tsp->get_key(tsp->get_key_arg,
	tsp->k5_context,
	server,
	enctkt->enc_part.etype,
	*enctkt->enc_part.kvno,
	key);
    if (code) goto Done;
    code = krb5_decrypt_ticket(tsp->k5_context, enctkt, key, &ticket->ticket, 0);
    if (code) goto Done;
#else
    tixdata->data = hr->rxk5c_ticket.rxk5c_ticket_val;
    tixdata->length = hr->rxk5c_ticket.rxk5c_ticket_len;
    if ((code = krb5_decode_ticket(tixdata, &ticket))) {
	goto Done;
    }
    server = ticket->server;	/* steal storage */
    ticket->server = 0;
    code = tsp->get_key(tsp->get_key_arg,
	tsp->k5_context,
	server,
	ticket->enc_part.enctype,
	(int) ticket->enc_part.kvno,
	key);
    if (code) goto Done;
    code = krb5_danish_surprise(tsp->k5_context,
	key, ticket);
    if (code) goto Done;
#endif
#endif
    krb5_free_keyblock_contents(tsp->k5_context, key);
#ifdef USING_SHISHI
    code = shishi_asn1_read(tsp->k5_context,
	encticketpart, "transited.contents",
	&trdata, &buflen);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    if (buflen) {
	code = ERROR_TABLE_BASE_SHI5 + SHISHI_INVALID_TICKET;
	goto Done;
    }
    /* shishi_tkt_invalid_p doesn't check result from shishi_tkt_flags */
    code = shishi_tkt_flags(tkt, &flags);
    if (code) {
printf ("can't get flags\n");
code += ERROR_TABLE_BASE_SHI5; goto Done; }
    if (flags & SHISHI_TICKETFLAGS_INVALID) {
printf ("rxk5_s_CheckResponse: %#x has invalid flag set\n", flags);
	code = ERROR_TABLE_BASE_SHI5 + SHISHI_INVALID_TICKET;
	goto Done;
    }
    code = shishi_encticketpart_get_key(tsp->k5_context, encticketpart, &session);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    if (!session) {
printf ("rxk5_s_CheckResponse: NO session key???\n");
shishi_tkt_pretty_print(tkt, stdout);
	code = ERROR_TABLE_BASE_SHI5 + SHISHI_INVALID_TICKET;
	goto Done;
    }
#else
#ifdef USING_HEIMDAL
/* krb5_decrypt_ticket checked:
 *	t.starttime,now,t.endtime (fwiw)
 *	t.flags.invalid
 *	transited realm stuff
 */
    session = &ticket->ticket.key;
#else
    /* krb5_danish_surprise checked:
     *	ticket->enc_part2->transited
     *	ticket-enc_part2->flags & TKT_FLG_INVALID
     */
    session = ticket->enc_part2->session;
#endif
#endif
#ifdef USING_SHISHI
    code = shishi_decrypt(tsp->k5_context, session, hr->skeyidx + USAGE_RESPONSE,
	hr->rxk5c_encrypted.rxk5c_encrypted_val,
	hr->rxk5c_encrypted.rxk5c_encrypted_len,
	&plaintext->data, &plaintext->length);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    xdrmem_create(xdrs, plaintext->data, plaintext->length, XDR_DECODE);
#else
#ifdef USING_HEIMDAL
    code = krb5_crypto_init(tsp->k5_context, session, session->keytype, &crypto);
    if (code) goto Done;
    code = krb5_decrypt(tsp->k5_context, crypto, hr->skeyidx + USAGE_RESPONSE,
	hr->rxk5c_encrypted.rxk5c_encrypted_val,
	hr->rxk5c_encrypted.rxk5c_encrypted_len,
	plaintext);
    if (code) goto Done;
    xdrmem_create(xdrs, plaintext->data, plaintext->length, XDR_DECODE);
#else
    cipher->ciphertext.length = hr->rxk5c_encrypted.rxk5c_encrypted_len;
    cipher->ciphertext.data = hr->rxk5c_encrypted.rxk5c_encrypted_val;
    cipher->enctype = session->enctype;
    cipher->kvno = ticket->enc_part.kvno;
    plain->data = work;
    plain->length = WSIZE;
    code = krb5_c_decrypt(tsp->k5_context,
	session, hr->skeyidx + USAGE_RESPONSE, 0, cipher, plain);
    if (code) goto Done;
    xdrmem_create(xdrs, plain->data, plain->length, XDR_DECODE);
#endif
#endif
    if (!xdr_rxk5c_response_encrypted(xdrs, he)) {
	code = RXK5PACKETSHORT;
	goto Done;
    }
    if (he->endpoint.cuid[0] != conn->epoch
	    || he->endpoint.cuid[1] != (conn->cid & RX_CIDMASK)
	    || he->endpoint.securityindex != conn->securityIndex) {
	code = RXK5SEALEDINCON;
	goto Done;
    }
    if (he->incchallengeid != sconn->challengeid + 1) {
	code = RXK5OUTOFSEQUENCE;
	goto Done;
    }
    for (i = 0; i < RX_MAXCALLS; ++i) if (he->callnumbers[i] < 0) {
	code = RXK5SEALEDINCON;
	goto Done;
    }
    if (sconn->client)
	free(sconn->client);
#ifdef USING_SHISHI
    code = shishi_encticketpart_crealm(tsp->k5_context, encticketpart, &crealm, &buflen);
    if (code) {
printf ("rxk5_s_CheckResponse: shishi_encticketpart_crealm failed code=%d\n", code);
code += ERROR_TABLE_BASE_SHI5; goto Done; }
    code = shishi_encticketpart_client(tsp->k5_context, encticketpart, &cname, &buflen);
    if (code) {
printf ("rxk5_s_CheckResponse: shishi_encticketpart_client failed code=%d\n", code);
code += ERROR_TABLE_BASE_SHI5; goto Done; }
    sconn->server = server;
    server = 0;
    sconn->client = malloc(strlen(crealm) + strlen(cname) + 2);
    sprintf(sconn->client, "%s@%s", cname, crealm);
#else
    code = krb5_unparse_name(tsp->k5_context,
	server,
	&sconn->server);
    if (code) goto Done;
#ifdef USING_HEIMDAL
    _krb5_principalname2krb5_principal(&client,
	ticket->ticket.cname,
	ticket->ticket.crealm);
    code = krb5_unparse_name(tsp->k5_context, client, &sconn->client);
    if (code) goto Done;
    key->keytype = session->keytype;
    key->keyvalue.length = session->keyvalue.length;
    if (!(key->keyvalue.data
		= osi_Alloc(session->keyvalue.length))) {
	code = RXK5INCONSISTENCY;
	goto Done;
    }
#else
    code = krb5_unparse_name(tsp->k5_context,
	ticket->enc_part2->client,
	&sconn->client);
    if (code) goto Done;
    key->enctype = session->enctype;
    key->length = session->length;
    if (!(key->contents
		= osi_Alloc(session->length))) {
printf ("rxk5_s_CheckResponse: osi_Alloc(%d) failed\n", session->length);
	code = RXK5INCONSISTENCY;
	goto Done;
    }
#endif
#endif
#ifdef USING_SHISHI
    code = shishi_key_from_value(tsp->k5_context,
	shishi_key_type(session), he->rxk5c_key.rxk5c_key_val,
	&key->sk);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
#else
#ifdef USING_HEIMDAL
#define deref_keyblock_contents(kb)	((kb)->keyvalue.data)
#else
#define deref_keyblock_contents(kb)	((kb)->contents)
#endif
    memcpy(
	deref_keyblock_contents(key),
	he->rxk5c_key.rxk5c_key_val,
	he->rxk5c_key.rxk5c_key_len);
#endif
    krb5_free_keyblock_contents(tsp->k5_context, sconn->hiskey);
    krb5_free_keyblock_contents(tsp->k5_context, sconn->mykey);
    plain->data = (void *) &usage;
    plain->length = sizeof usage;
    usage = htonl(USAGE_CLIENT);
    code = rxk5i_derive_key(tsp->k5_context, key, plain, sconn->hiskey);
    if (code) goto Done;
    usage = htonl(USAGE_SERVER);
    code = rxk5i_derive_key(tsp->k5_context, key, plain, sconn->mykey);
    if (code) goto Done;
    (void) rxi_SetCallNumberVector(conn, he->callnumbers);
#ifdef USING_SHISHI
    sconn->kvno = epkvno,
#else
#ifdef USING_HEIMDAL
    sconn->kvno = enctkt->tkt_vno;
#else
    sconn->kvno = ticket->enc_part.kvno;
#endif
#endif
#ifdef USING_SHISHI
    sconn->start = shishi_tkt_startctime(tkt);
    if (sconn->start == (time_t) -1)
	sconn->start = shishi_tkt_authctime(tkt);
    sconn->expires = shishi_tkt_endctime(tkt);
#else
#ifdef USING_HEIMDAL
    if (ticket->ticket.starttime)
	sconn->start = *ticket->ticket.starttime;
    else
	sconn->start = ticket->ticket.authtime;
    sconn->expires = ticket->ticket.endtime;
#else
    sconn->start = ticket->enc_part2->times.starttime;
    sconn->expires = ticket->enc_part2->times.endtime;
#endif
#endif
    switch(he->level) {
#if CONFIG_CLEAR
    case rxk5_clear:
#endif
    case rxk5_auth:
    case rxk5_crypt:
	if (he->level >= sconn->level)
	    break;
	code = RXK5LEVELFAIL;
	goto Done;
    default:
	code = RXK5ILLEGALLEVEL;
	goto Done;
    }
    sconn->level = he->level;
    sconn->flags |= AUTHENTICATED;
    sconn->cktype = he->cktype;
    rxk5i_SetLevel(conn, he->level,
#ifdef USING_SHISHI
	shishi_key_type(session),
#else
#ifdef USING_HEIMDAL
	session->keytype,
#else
	session->enctype,
#endif
#endif
	sconn->cktype,
	tsp->k5_context, sconn->pad);
    INCSTAT(responses[sconn->level ...]);
Done:
#if 0
	XXX do something to clear secrets from memory.
#endif
#ifdef USING_SHISHI
    if (tkt)
	shishi_tkt_done(tkt);	/* XXX almost certainly leaks memory */
    if (cname) free(cname);
    if (crealm) free(crealm);
    if (sname) free(sname);
    if (realm) free(realm);
    if (server) free(server);
    if (plaintext->data) osi_Free(plaintext->data, plaintext->length);
    if (trdata) free(trdata);
#else
    krb5_free_principal(tsp->k5_context, server);
#ifdef USING_HEIMDAL
    if (enctkt->realm) free_Ticket(enctkt);
    if (crypto) krb5_crypto_destroy(tsp->k5_context, crypto);
    if (plaintext->data) osi_Free(plaintext->data, plaintext->length);
    if (ticket->ticket.key.keyvalue.data)
	free_EncTicketPart(&ticket->ticket);
    if (client)
	krb5_free_principal(tsp->k5_context, client);
#else
    if (work)
	osi_free(work, WSIZE);
    if (ticket)
	krb5_free_ticket(tsp->k5_context, ticket);
#endif
#endif
    krb5_free_keyblock_contents(tsp->k5_context, key);
    xdrs->x_op = XDR_FREE;
    xdr_rxk5c_response_encrypted(xdrs, he);
    xdr_rxk5c_response(xdrs, hr);
    return code;
}

int
rxk5_s_PreparePacket(so, call, p)
    struct rx_securityClass *so;
    struct rx_call *call;
    struct rx_packet *p;
{
    struct rx_connection *conn = rx_ConnectionOf(call);
    int level;
    struct rxk5_sprivate *tsp = (struct rxk5_sprivate*)so->privateData;
    struct rxk5_sconn *sconn;
    time_t now;
    int code;

    code = RXK5NOSECDATA; if (!tsp) goto Out;
    now = osi_Time(); code = RXK5EXPIRED;
    if (!(sconn = (struct rxk5_sconn*)conn->securityData)
	    || !(sconn->flags & AUTHENTICATED)
	    || now < sconn->start
	    || now >= sconn->expires) {
	INCSTAT(expired)
	goto Out;
    }
    level = sconn->level;
    INCSTAT(preparePackets[rxk5_StatIndex(rxk5_server, level)])
    switch(level) {
    default:
	code = RXK5ILLEGALLEVEL;
	break;
    case rxk5_crypt:
	code = rxk5i_PrepareEncrypt(so, call, p,
	    sconn->mykey, sconn->pad,
	    &sconn->stats, tsp->k5_context);
	break;
    case rxk5_auth:
	code = rxk5i_PrepareSum(so, call, p,
	    sconn->mykey, sconn->cktype,
	    &sconn->stats, tsp->k5_context);
	break;
#if CONFIG_CLEAR
    case rxk5_clear:
	code = 0;
	break;
#endif
    }
Out:
    if (code)
	rxi_CallError(call, code);
    return 0;
}

int
rxk5_s_CheckPacket(so, call, p)
    struct rx_securityClass *so;
    struct rx_call *call;
    struct rx_packet *p;
{
    struct rx_connection *conn = rx_ConnectionOf(call);
    int level;
    struct rxk5_sprivate *tsp = (struct rxk5_sprivate*)so->privateData;
    struct rxk5_sconn *sconn;
    time_t now;

    if (!tsp) return RXK5NOSECDATA;
    now = osi_Time();
    if (!(sconn = (struct rxk5_sconn*)conn->securityData)
	    || !(sconn->flags & AUTHENTICATED)
	    || now < sconn->start
	    || now >= sconn->expires) {
	INCSTAT(expired)
	return RXK5EXPIRED;
    }
    level = sconn->level;
    INCSTAT(checkPackets[rxk5_StatIndex(rxk5_server, level)])
    switch(level) {
    default:
	return RXK5ILLEGALLEVEL;
    case rxk5_crypt:
	return rxk5i_CheckEncrypt(so, call, p,
	    sconn->hiskey, sconn->pad,
	    &sconn->stats, tsp->k5_context);
    case rxk5_auth:
	return rxk5i_CheckSum(so, call, p,
	    sconn->hiskey, sconn->cktype,
	    &sconn->stats, tsp->k5_context);
#if CONFIG_CLEAR
    case rxk5_clear:
	return 0;
#endif
    }
}

int
rxk5_s_GetStats(so, conn, stats)
    struct rx_securityClass *so;
    struct rx_connection *conn;
    struct rx_securityObjectStats *stats;
{
    struct rxk5_sconn *sconn = (struct rxk5_sconn*)conn->securityData;

    stats->type = 3;
    if (!sconn) {
	stats->flags |= 1;
	return 0;
    }
    stats->level = sconn->level;
    if (sconn->flags & AUTHENTICATED)
	    stats->flags |= 2;
    stats->expires = sconn->expires;
    return rxk5i_GetStats(so, conn, stats, &sconn->stats);
}

static struct rx_securityOps rxk5_server_ops[] = {{
    rxk5_s_Close,
    rxk5_s_NewConnection,
    rxk5_s_PreparePacket,
    0,	/* B SendPacket */
    rxk5_s_CheckAuthentication,
    rxk5_s_CreateChallenge,
    rxk5_s_GetChallenge,
    0,	/* C GetResponse */
    rxk5_s_CheckResponse,
    rxk5_s_CheckPacket,
    rxk5_s_DestroyConnection,
    rxk5_s_GetStats,
}};

struct rx_securityClass *
rxk5_NewServerSecurityObject(level, get_key_arg, get_key, user_ok, k5_context)
    int level;
    char *get_key_arg;
    int (*get_key)(), (*user_ok)();
    krb5_context k5_context;
{
    struct rx_securityClass *result;
    struct rxk5_sprivate *tsp;

    result = (struct rx_securityClass *) osi_Alloc (sizeof *result);
    if (!result) return 0;
    result->refCount = 1;
    result->ops = rxk5_server_ops;
    tsp = (struct rxk5_sprivate *) osi_Alloc(sizeof *tsp);
    if (!tsp) {
	osi_Free(result, sizeof *result);
	return 0;
    }
    tsp->minlevel = level;
    tsp->get_key_arg = get_key_arg;
    tsp->get_key = get_key;
    tsp->user_ok = user_ok;
    tsp->k5_context = rxk5_get_context(k5_context);

    result->privateData = (char *) tsp;
    return result;
}
