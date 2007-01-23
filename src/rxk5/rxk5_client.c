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

#include <afs/stds.h>
#include <afsconfig.h>
#if defined(KERNEL) && !defined(UKERNEL)
#include "afs/param.h"
#include "afs/sysincludes.h"    /*Standard vendor system headers */
#include "afsincludes.h"        /*AFS-based standard headers */
#include "afs_stats.h"
#include <rx/rxk5.h>
#include <rx/rxk5errors.h>
#include <rx/rxk5c.h>
#include <k5ssl.h>
#include <rx/rxk5imp.h>
#else
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
#undef u
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
#endif /* !kernel */

#ifdef AFS_PTHREAD_ENV
extern pthread_mutex_t rxk5_cuid_mutex[1];
#define LOCK_CUID	assert(!pthread_mutex_lock(rxk5_cuid_mutex))
#define UNLOCK_CUID	assert(!pthread_mutex_unlock(rxk5_cuid_mutex))
#else
#if defined(KERNEL) && !defined(UKERNEL)
extern afs_rwlock_t rxk5i_context_mutex;
extern afs_rwlock_t rxk5_cuid_mutex;
#define LOCK_CUID ObtainWriteLock(&rxk5_cuid_mutex, 752);
#define UNLOCK_CUID ReleaseWriteLock(&rxk5_cuid_mutex);
#else /* kernel */
#define LOCK_CUID	0
#define UNLOCK_CUID	0
#endif
#endif

int
rxk5_c_Close (so)
    struct  rx_securityClass *so;
{
    struct rxk5_cprivate *tcp = (struct rxk5_cprivate*) so->privateData;

    if (--so->refCount > 0)
	return 0;
    krb5_free_keyblock_contents(tcp->k5_context, tcp->session);
    rxi_Free(tcp, sizeof *tcp + tcp->ticketlen);
    rxi_Free(so, sizeof *so);
    INCSTAT(destroyObject);
    return 0;
}

int
rxk5_c_GetResponse(so, conn, p)
    struct rx_securityClass *so;
    struct rx_connection *conn;
    struct rx_packet *p;
{
    struct rxk5_cprivate *tcp = (struct rxk5_cprivate *) so->privateData;
    struct rxk5_cconn * cconn = (struct rxk5_cconn*)conn->securityData;
    char *cp;
    int cs, c;
    int code;
    struct rxk5c_challenge hc[1];
    struct rxk5c_response hr[1];
    struct rxk5c_response_encrypted he[1];
    char *work = 0;
#define WSIZE (RXK5_MAXKTCTICKETLEN * 3)
    XDR xdrs[1];
    krb5_data plain[1];
#ifdef USING_SHISHI
    krb5_data cipher[1];
#else
#ifdef USING_HEIMDAL
    krb5_crypto crypto = 0;
    krb5_data cipher[1];
#else
    krb5_enc_data cipher[1];
    size_t enclen;
#endif
#endif
    int i;
#if 0
    int j;
#endif
    if (!tcp || !cconn)
	return RXK5NOSECDATA;

    memset((void*)hc, 0, sizeof *hc);
    memset((void*)plain, 0, sizeof *plain);
    memset((void*)cipher, 0, sizeof *cipher);
    memset((void*)hr, 0, sizeof *hr);
    work = osi_Alloc(WSIZE);
    if (!work) {
	return RXK5INCONSISTENCY;
    }
    rx_Pullup(p, sizeof *hc);
    cp = rx_data(p, 0, cs);
    xdrmem_create(xdrs, cp, cs, XDR_DECODE);
    if (!xdr_rxk5c_challenge(xdrs, hc)) {
	code = RXK5PACKETSHORT;
	goto Done;
    }
    if (hc->vers) {
	code = RXK5WRONGVERS;
	goto Done;
    }
    if (hc->level > tcp->level) {
	code = RXK5LEVELFAIL;
	goto Done;
    }
    /* if ((code = rxk5i_generate_keys(tcp, cconn))) goto Done; */
    xdrmem_create(xdrs, work, WSIZE, XDR_ENCODE);
    cp = work;
    hr->vers = 0;
    hr->skeyidx = conn->serial;
    he->endpoint.cuid[0] = conn->epoch;
    he->endpoint.cuid[1] = conn->cid & RX_CIDMASK;
    he->endpoint.securityindex = conn->securityIndex;
    he->incchallengeid = hc->challengeid + 1;
    he->level = tcp->level;
    he->cktype = tcp->cktype;
    he->dktype = DK_CUSTOM;
    rxi_GetCallNumberVector(conn, he->callnumbers);
    for (i = 0; i < RX_MAXCALLS; ++i) if (he->callnumbers[i] < 0) {
/* printf ("negative callnumber: [%d] = %d\n", i, he->callnumbers[i]); */
	code = RXK5INCONSISTENCY;
	goto Done;
    }
#if 0
    for (i = 0; i < NUMALT; ++i) {
	/* invert keys on server if server's seen current key */
	j = i ^ !!cconn->altkeys[0].serial;
	he->keys[j].keyidx = cconn->mykeyidx;
    }
#endif
#ifdef USING_SHISHI
    he->rxk5c_key.rxk5c_key_val = shishi_key_value(cconn->thekey->sk);
    he->rxk5c_key.rxk5c_key_len = shishi_key_length(cconn->thekey->sk);
#else
#ifdef USING_HEIMDAL
    he->rxk5c_key.rxk5c_key_val = cconn->thekey->keyvalue.data;
    he->rxk5c_key.rxk5c_key_len = cconn->thekey->keyvalue.length;
#else
    he->rxk5c_key.rxk5c_key_val = cconn->thekey->contents;
    he->rxk5c_key.rxk5c_key_len = cconn->thekey->length;
#endif
#endif
    if (!xdr_rxk5c_response_encrypted(xdrs, he)) {
/* printf ("Can't serialize he to xdr\n"); */
	code = RXK5INCONSISTENCY;
	goto Done;
    }
    cs = xdr_getpos(xdrs);
    plain->data = cp;
    plain->length = cs;
#ifdef USING_SHISHI
    cp += cs;
    LOCK_RXK5_CONTEXT;
    code = shishi_encrypt(tcp->k5_context,
	tcp->session->sk, hr->skeyidx + USAGE_RESPONSE,
	plain->data, plain->length,
	&cipher->data, &cipher->length);
    UNLOCK_RXK5_CONTEXT;
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    if (cs + cipher->length >= WSIZE) {
/* printf ("offset %d max %d\n", cs + cipher->length, WSIZE); */
	code = RXK5INCONSISTENCY;
	goto Done;
    }
    memcpy(cp, cipher->data, cipher->length);
    cp += cipher->length;
    cs += cipher->length;
#else
#ifdef USING_HEIMDAL
    cp += cs;
    code = krb5_crypto_init(tcp->k5_context, tcp->session, tcp->session->keytype, &crypto);
    if (code) goto Done;
    LOCK_RXK5_CONTEXT;
    code = krb5_encrypt(tcp->k5_context, crypto, hr->skeyidx + USAGE_RESPONSE,
	plain->data, plain->length,
	cipher);
    UNLOCK_RXK5_CONTEXT;
    if (code) goto Done;
    if (cs + cipher->length >= WSIZE) {
/* printf ("offset %d max %d\n", cs + cipher->length, WSIZE); */
	code = RXK5INCONSISTENCY;
	goto Done;
    }
    memcpy(cp, cipher->data, cipher->length);
    cp += cipher->length;
    cs += cipher->length;
#else
    code = krb5_c_encrypt_length(tcp->k5_context, tcp->session->enctype, plain->length, &enclen);
    if (code) goto Done;
    cp += cs;
    cipher->ciphertext.length = enclen;
    cipher->ciphertext.data = cp;
    cp += enclen;
    cs += enclen;
    if (cs >= WSIZE) {
/* printf ("offset %d max %d\n", cs, WSIZE); */
	code = RXK5INCONSISTENCY;
	goto Done;
    }
    LOCK_RXK5_CONTEXT;
    code = krb5_c_encrypt(tcp->k5_context, tcp->session,
	hr->skeyidx + USAGE_RESPONSE, 0, plain, cipher);
    UNLOCK_RXK5_CONTEXT;
    if (code) goto Done;
#endif
#endif
    xdr_setpos(xdrs, cs);
#if defined(USING_HEIMDAL) || defined(USING_SHISHI)
    hr->rxk5c_encrypted.rxk5c_encrypted_len = cipher->length;
    hr->rxk5c_encrypted.rxk5c_encrypted_val = cipher->data;
#else
    hr->rxk5c_encrypted.rxk5c_encrypted_len = cipher->ciphertext.length;
    hr->rxk5c_encrypted.rxk5c_encrypted_val = cipher->ciphertext.data;
#endif
    hr->rxk5c_ticket.rxk5c_ticket_len = tcp->ticketlen;
    hr->rxk5c_ticket.rxk5c_ticket_val = tcp->ticket;
    if (!xdr_rxk5c_response(xdrs, hr)) {
/* printf ("Can't serialize response to xdr\n"); */
	code = RXK5INCONSISTENCY;
	goto Done;
    }
    c = xdr_getpos(xdrs) - cs;
    rx_packetwrite(p, 0, c, cp);
    rx_SetDataSize(p, c);
#if 0
XXX do something to clear secrets from memory...
#endif
Done:
#ifdef USING_SHISHI
    if (cipher->data) osi_Free(cipher->data, cipher->length);
#endif
#ifdef USING_HEIMDAL
    if (crypto) krb5_crypto_destroy(tcp->k5_context, crypto);
    if (cipher->data) osi_Free(cipher->data, cipher->length);
#endif
    if (work)
	osi_Free(work, WSIZE);
    return code;
}

static int rxk5_cuid[2];
int rxk5_EpochWasSet;

int
rxk5_AllocCID(conn, context)
    struct rx_connection *conn;
    krb5_context context;
{
    int code;
#if !defined(USING_HEIMDAL) && !defined(USING_SHISHI)
    krb5_data rd[1];
#endif

    code = 0;
    LOCK_CUID;
    if (!*rxk5_cuid) {
	LOCK_RXK5_CONTEXT;
#ifdef USING_SHISHI
	code = shishi_randomize(context, 0,
	    (void*) rxk5_cuid, sizeof rxk5_cuid);
	if (code) code += ERROR_TABLE_BASE_SHI5;
#else
#ifdef USING_HEIMDAL
	krb5_generate_random_block((void*) rxk5_cuid, sizeof rxk5_cuid);
#else
	rd->data = (void*) rxk5_cuid;
	rd->length = sizeof rxk5_cuid;
	code = krb5_c_random_make_octets(context, rd);
#endif
#endif
	UNLOCK_RXK5_CONTEXT;
	rxk5_cuid[0] &= ~0x40000000;
	rxk5_cuid[0] |= 0x80000000;
	rxk5_cuid[1] &= RX_CIDMASK;
	if (!code) {
	    rx_SetEpoch(rxk5_cuid[0]);
	    rxk5_EpochWasSet = 1;
	}
    }
    if (conn) {
	rxk5_cuid[1] += 1<<RX_CIDSHIFT;
	conn->epoch = rxk5_cuid[0];
	conn->cid = rxk5_cuid[1];
    }
    UNLOCK_CUID;
    return code;
}

/* decision: client tells server which cktype to use */
int
rxk5i_select_ctype(context, enctype, cktypep)
	krb5_context context;
	int *cktypep;
{
#if 1 || defined(USING_HEIMDAL) || defined(USING_SHISHI)
    switch(enctype) {
    case ENCTYPE_DES_CBC_CRC:
    case ENCTYPE_DES_CBC_MD4:
    case ENCTYPE_DES_CBC_MD5:
	*cktypep = CKSUMTYPE_RSA_MD5_DES;
	break;
#if 1
    default:
	*cktypep = CKSUMTYPE_HMAC_SHA1_DES3;
#else
#ifdef ENCTYPE_ARCFOUR_HMAC_MD5
    case ENCTYPE_ARCFOUR_HMAC_MD5:
#ifdef CKSUMTYPE_HMAC_MD5_ARCFOUR
	*cktypep = CKSUMTYPE_HMAC_MD5_ARCFOUR;
#else
	*cktypep = CKSUMTYPE_HMAC_MD5;
#endif
	break;
#endif
#ifdef ENCTYPE_DES3_CBC_MD5
    case ENCTYPE_DES3_CBC_MD5:
#endif
    case ENCTYPE_DES3_CBC_SHA1:
    /* case ENCTYPE_OLD_DES3_CBC_SHA1: */
	*cktypep = CKSUMTYPE_HMAC_SHA1_DES3;
	break;
#ifdef ENCTYPE_AES128_CTS_HMAC_SHA1_96
    case ENCTYPE_AES128_CTS_HMAC_SHA1_96:
	*cktypep = CKSUMTYPE_HMAC_SHA1_96_AES_128;
	break;
#endif
#ifdef ENCTYPE_AES256_CTS_HMAC_SHA1_96
    case ENCTYPE_AES256_CTS_HMAC_SHA1_96:
	*cktypep = CKSUMTYPE_HMAC_SHA1_96_AES_256;
	break;
#endif
    default:
printf ("rxk5i_select_ctype: No match for enctype=%d\n", enctype);
	return RXK5INCONSISTENCY;	/* XXX */
#endif
    }
    return 0;
#else
    int code;
    unsigned int ntypes;
    krb5_cksumtype *sumtypes;

    code = krb5_c_keyed_checksum_types(context, enctype, &ntypes, &sumtypes);
    if (code) return code;
    if (!ntypes) return RXK5INCONSISTENCY;
    *cktypep = *sumtypes;
    krb5_free_cksumtypes(context, sumtypes);
    if (!krb5_c_valid_cksumtype(*cktypep)
	    || !krb5_c_is_coll_proof_cksum(*cktypep)
	    || !krb5_c_is_keyed_cksum(*cktypep)) {
	return RXK5INCONSISTENCY;
    }
    return 0;
#endif
}

int
rxk5i_generate_keys(tcp, cconn)
    struct rxk5_cconn * cconn;
    struct rxk5_cprivate *tcp;
{
    int code;
    krb5_data plain[1];
    int usage;

#if 0
    if (cconn->altkeys[0].serial < cconn->altkeys[1].serial) {
printf ("server has seen new key\n");
	cconn->altkeys[0] = cconn->altkeys[1];
	cconn->altkeys[1] =0s;
    }
else printf ("server has NOT seen new key\n");
#endif
#ifdef USING_SHISHI
    if (cconn->thekey->sk)
#else
#ifdef USING_HEIMDAL
    if (cconn->thekey->keyvalue.data)
#else
    if (cconn->thekey->contents)
#endif
#endif
	return 0;
#ifdef USING_SHISHI
    LOCK_RXK5_CONTEXT;
    code = shishi_key_random(tcp->k5_context,
	shishi_key_type(tcp->session->sk), &cconn->thekey->sk);
    UNLOCK_RXK5_CONTEXT;
    if (code) { code += ERROR_TABLE_BASE_SHI5; return code; }
#else
    LOCK_RXK5_CONTEXT;
#ifdef USING_HEIMDAL
    code = krb5_generate_random_keyblock(tcp->k5_context,
	    tcp->session->keytype, cconn->thekey);
#else
    code = krb5_c_make_random_key(tcp->k5_context,
	    tcp->session->enctype, cconn->thekey);
#endif
    UNLOCK_RXK5_CONTEXT;
    if (code)
	return code;
#endif
    plain->data = (void *) &usage;
    plain->length = sizeof usage;
    usage = htonl(USAGE_CLIENT);
    if ((code = rxk5i_derive_key(tcp->k5_context, cconn->thekey, plain, cconn->mykey)))
	return code;
    usage = htonl(USAGE_SERVER);
    if ((code = rxk5i_derive_key(tcp->k5_context, cconn->thekey, plain, cconn->hiskey)))
	return code;
    return 0;
}

int
rxk5_c_NewConnection(so, conn)
    struct rx_securityClass *so;
    struct rx_connection *conn;
{
    struct rxk5_cprivate *tcp;
    struct rxk5_cconn *cconn;
    int code;

    if (conn->securityData) {
	code = RXK5INCONSISTENCY;
	goto Out;
    }
    tcp = (struct rxk5_cprivate*)so->privateData;
    if ((code = rxk5i_select_ctype(tcp->k5_context,
#ifdef USING_SHISHI
	    shishi_key_type(tcp->session->sk),
#else
#ifdef USING_HEIMDAL
	    tcp->session->keytype,
#else
	    tcp->session->enctype,
#endif
#endif
	    &tcp->cktype))) {
printf ("rxk5_c_NewConnection: rxk5i_select_ctype failed: code=%d\n", code);
	goto Out;
    }
    rxk5_AllocCID(conn, tcp->k5_context);
    INCSTAT(connections[tcp->level])
    code = rxk5i_NewConnection(so, conn, sizeof(struct rxk5_cconn));
    if (!code) {
	cconn = (struct rxk5_cconn*)conn->securityData;
	code = rxk5i_SetLevel(conn, tcp->level,
#ifdef USING_SHISHI
	    shishi_key_type(tcp->session->sk),
#else
#ifdef USING_HEIMDAL
	    tcp->session->keytype,
#else
	    tcp->session->enctype,
#endif
#endif
	    tcp->cktype,
	    tcp->k5_context, cconn->pad);
    }
if (code)
printf ("rxk5_c_NewConnection: code=%d conn=%#x secdata=%#x\n",
code, (int)conn, (int)conn->securityData);
Out:
    if (code)
	rxi_ConnectionError(conn, code);
    return code;
}

int
rxk5_c_DestroyConnection(so, conn)
    struct  rx_securityClass *so;
    struct  rx_connection *conn;
{
    struct rxk5_cconn *cconn;
    struct rxk5_cprivate *tcp = (struct rxk5_cprivate *) so->privateData;

    if ((cconn = (struct rxk5_cconn*)conn->securityData)) {
	conn->securityData = 0;
	krb5_free_keyblock_contents(tcp->k5_context, cconn->hiskey);
	krb5_free_keyblock_contents(tcp->k5_context, cconn->mykey);
	krb5_free_keyblock_contents(tcp->k5_context, cconn->thekey);
	rxi_Free(cconn, sizeof *cconn);
    }
    INCSTAT(destroyClient)
    return rxk5_c_Close(so);
}

int
rxk5_c_PreparePacket(so, call, p)
    struct rx_securityClass *so;
    struct rx_call *call;
    struct rx_packet *p;
{
    struct rx_connection *conn = rx_ConnectionOf(call);
    int level;
    struct rxk5_cconn * cconn = (struct rxk5_cconn*)conn->securityData;
    struct rxk5_cprivate *tcp = (struct rxk5_cprivate *) so->privateData;
    int code;

    /* these can happen if rxk5_c_NewConnection fails */
    if ((code = rx_ConnError(conn))) goto Out;
    code = RXK5NOSECDATA; if (!cconn || !tcp) goto Out;

    level = tcp->level;
    if ((code = rxk5i_generate_keys(tcp, cconn)))
	goto Out;
    INCSTAT(preparePackets[rxk5_StatIndex(rxk5_client, level)])
    switch(level) {
    default:
	code = RXK5ILLEGALLEVEL;
	break;
    case rxk5_crypt:
	code = rxk5i_PrepareEncrypt(so, call, p,
	    cconn->mykey, cconn->pad,
	    &cconn->stats, tcp->k5_context);
	break;
    case rxk5_auth:
	code = rxk5i_PrepareSum(so, call, p,
	    cconn->mykey, tcp->cktype,
	    &cconn->stats, tcp->k5_context);
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
rxk5_c_CheckPacket(so, call, p)
    struct rx_securityClass *so;
    struct rx_call *call;
    struct rx_packet *p;
{
    struct rx_connection *conn = rx_ConnectionOf(call);
    int level;
    struct rxk5_cconn * cconn = (struct rxk5_cconn*)conn->securityData;
    struct rxk5_cprivate *tcp = (struct rxk5_cprivate *) so->privateData;

    if (!cconn || !tcp) return RXK5NOSECDATA;
    level = tcp->level;
    INCSTAT(preparePackets[rxk5_StatIndex(rxk5_client, level)])
    switch(level) {
    default:
	return RXK5ILLEGALLEVEL;
    case rxk5_crypt:
	return rxk5i_CheckEncrypt(so, call, p,
	    cconn->hiskey, cconn->pad,
	    &cconn->stats, tcp->k5_context);
    case rxk5_auth:
	return rxk5i_CheckSum(so, call, p,
	    cconn->hiskey, tcp->cktype,
	    &cconn->stats, tcp->k5_context);
#if CONFIG_CLEAR
    case rxk5_clear:
	return 0;
#endif
    }
}

int
rxk5_c_GetStats(so, conn, stats)
    struct rx_securityClass *so;
    struct rx_connection *conn;
    struct rx_securityObjectStats *stats;
{
    struct rxk5_cconn *cconn = (struct rxk5_cconn*)conn->securityData;

    stats->type = 3;
    stats->level = ((struct rxk5_cprivate*)so->privateData)->level;
    if (!cconn) {
	stats->flags |= 1;
	return 0;
    }
    return rxk5i_GetStats(so, conn, stats, &cconn->stats);
}

static struct rx_securityOps rxk5_client_ops[] = {{
    rxk5_c_Close,
    rxk5_c_NewConnection,
    rxk5_c_PreparePacket,
    0,	/* B SendPacket */
    0,	/* S CheckAuthentication */
    0,	/* S CreateChallenge */
    0,	/* S GetChallenge */
    rxk5_c_GetResponse,
    0,	/* S CheckResponse */
    rxk5_c_CheckPacket,
    rxk5_c_DestroyConnection,
    rxk5_c_GetStats,
}};

struct rx_securityClass *
rxk5_NewClientSecurityObject(level, creds, context)
    int level;
#ifdef USING_SHISHI
    Shishi_tkt *creds;
#else
    krb5_creds *creds;
#endif
    krb5_context context;
{
    struct rx_securityClass *result;
    struct rxk5_cprivate *tcp;
    int code;
#ifdef USING_SHISHI
    Shishi_asn1 ticket;
    char *der;
    size_t derlen;
    Shishi_key *key;
#endif

    result = (struct rx_securityClass *) osi_Alloc (sizeof *result);
    if (!result) return 0;
    result->refCount = 1;
    result->ops = rxk5_client_ops;
#ifdef USING_SHISHI
    ticket = shishi_tkt_ticket(creds);
    code = shishi_asn1_to_der(context, ticket, &der, &derlen);
    if (code) {
	osi_Free(result, sizeof *result);
	return 0;
    }
    tcp = (struct rxk5_cprivate *) osi_Alloc(sizeof *tcp + derlen);
#else
    tcp = (struct rxk5_cprivate *) osi_Alloc(sizeof *tcp + creds->ticket.length);
#endif
    if (!tcp) {
#ifdef USING_SHISHI
	osi_Free(der, derlen);
#endif
	osi_Free(result, sizeof *result);
	return 0;
    }
    memset(tcp, 0, sizeof *tcp);
#ifdef USING_SHISHI
    memcpy(tcp->ticket, der, derlen);
    osi_Free(der, derlen);
    tcp->ticketlen = derlen;
#else
    memcpy(tcp->ticket, creds->ticket.data, creds->ticket.length);
    tcp->ticketlen = creds->ticket.length;
#endif
    tcp->level = level;
    tcp->k5_context = rxk5_get_context(context);
#ifdef USING_SHISHI
    if ((code = shishi_key(context, &tcp->session->sk)))
	;
    else if (!(key = shishi_tkt_key(creds)))
	code = 666;	/* bogus, but invisible anyways */
    else
	shishi_key_copy(tcp->session->sk, key);
#else
    code = krb5_copy_keyblock_contents(context,
#ifdef USING_HEIMDAL
	    &creds->session,
#else
	    &creds->keyblock,
#endif
	    tcp->session);
#endif
    if (code) {
	osi_Free(tcp, sizeof *tcp + creds->ticket.length);
	osi_Free(result, sizeof *result);
	return NULL;
    }
    result->privateData = (char *) tcp;
    INCSTAT(clientObjects);
    return result;
}
