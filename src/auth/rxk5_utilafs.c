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
#include <afs/afsutil.h>
#include <auth/cellconfig.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "rxk5_utilafs.h"
#include <rx/rxk5.h>

#define START_OF_TIME	300	/* must be nz */
#define END_OF_TIME	((~0U)>>1)

int have_afs_rxk5_keytab(char *confdir_name)
{
    int r, code;
    struct stat st;
    char *keytab;

    r = 0;
    keytab = get_afs_rxk5_keytab(confdir_name);
    code = stat(keytab, &st);
    if((code == 0) && (S_ISREG(st.st_mode))) {
    	r = 1;
    }
    free(keytab);
    return r;
}

char* get_afs_rxk5_keytab(char *confdir_name)
{
	/* Format a full path to the AFS keytab, caller must free */
	int len;
	char* rxk5_keytab;

	len = 12 + strlen(confdir_name);
	rxk5_keytab = (char*) malloc(len * sizeof(char));
	memset(rxk5_keytab, 0, len);
	sprintf(rxk5_keytab, "%s/afs.keytab", confdir_name);

	return rxk5_keytab;
}

char* get_afs_krb5_localauth_svc_princ(struct afsconf_dir *confdir)
{
    /* Returns the AFS service principal that should be sent by afs-k5
	-localauth.

    The afs-k5 service principal is created as follows:

	afs-k5/cell@REALM
	cell == what afsconf considers to be the local cell
	REALM == 1st realm in krb.conf, else UPPER(cell)

    The client must free.

    */

  int code, plen;
  char* princ = 0;
  struct afsconf_cell info;
  krb5_context k5context = 0;
  char the_realm[AFS_REALM_SZ];

  code = krb5_init_context(&k5context);
  if(code) {
    com_err("rxk5_utilafs", code, "error krb5_init_context");
    goto cleanup;
  }
  code = afsconf_GetCellInfo(confdir, NULL, NULL, &info);

  if (code) {
    com_err("rxk5_utilafs", code, " --unable to resolve local cell");
    goto cleanup;
  }

  if (afs_krb_get_lrealm(the_realm, 0) != 0) {
    /* not found, so upcase the local cell */
    ucstring(the_realm, info.name, AFS_REALM_SZ);
  }

  plen = 9 + strlen(info.name) + strlen(the_realm);
  princ = (char*) malloc(plen * sizeof(char));
  if (!princ) goto cleanup;
  sprintf(princ, "afs-k5/%s@%s", info.name, the_realm);

 cleanup:
  if(k5context)
    krb5_free_context(k5context);

  return princ;
}

/*
 * get_afs_krb5_svc_princ
 *
 * Returns the AFS service principal for the chosen cell/realm,
 * as used by AFS programs (eg, pts)--what aklog does.
 *
 * The caller must free.
 */

char*
get_afs_krb5_svc_princ(struct afsconf_cell *info)
{
    int code, plen;
    char *princ = 0, **hrealms = 0;
    krb5_context k5context;

    k5context = rxk5_get_context(0);
    if (!k5context)
	goto cleanup;

    if ((code = krb5_get_host_realm(k5context, info->hostName[0], &hrealms))
	    || !hrealms || !*hrealms) {
	com_err("rxk5_utilafs", code,
	    "no realms for afsdb host <%s>", info->hostName[0]);
	goto cleanup;
    }
    plen = 9 + strlen(info->name) + strlen(*hrealms); /* afs-k5/cell@REALM */
    princ = malloc(plen);
    if (!princ)
	goto cleanup;
    snprintf(princ, plen, "afs-k5/%s@%s", info->name, *hrealms);

cleanup:
    if (hrealms) krb5_free_host_realm(k5context, hrealms);
    return princ;
}

int env_afs_rxk5_default(void)
{
    char* ev = (char*) getenv("AFS_RXK5_DEFAULT");
    if (!ev) return FORCE_RXKAD|FORCE_RXK5;
    if ((strcasecmp(ev, "YES") == 0) || (strcasecmp(ev, "1") == 0)) {
	return FORCE_RXK5;
    } else {
	return FORCE_RXKAD;
    }
}

static int
afs_rxk5_is_local_realm(struct afsconf_dir *adir, char *avrealm)
{
    int i;
    char afs_realm[AFS_REALM_SZ];

    for (i = 0; !afs_krb_get_lrealm(afs_realm, i); ++i) {
	if (!strcmp(afs_realm, avrealm))
	    return 1;
    }
    if (!i && adir && adir->cellName) {
	ucstring(afs_realm, adir->cellName, sizeof afs_realm);
	i = !strcmp(avrealm, afs_realm);
    }
    return i;
}

#ifdef USING_HEIMDAL
#define krb5_princ_size(c,p)	((p)->name.name_string.len)
#endif

int
afs_rxk5_parse_name_k5(struct afsconf_dir *adir,
    const char *princ,
    char **name,
    int downcase)
{
  /* if princ is in local realm, strip @REALM from princ and return
     as *name, else strdup princ */
    krb5_context k5context = 0;
    krb5_principal parsed_princ = 0;
    krb5_principal_data work[1];
    int ncomp, code, len;
    char *first = 0, *instance = 0, *realm = 0;
    char *cp;

    k5context = rxk5_get_context(0);
    *name = 0;
    if (!k5context)
	return EDOM;	/* XXX */

    code = krb5_parse_name(k5context, princ, &parsed_princ);
    if (code) goto Failed;

    code = EDOM;	/* XXX */
    switch(ncomp = krb5_princ_size(k5context, parsed_princ)) {
    case 2:
    case 1:
	break;
    default:
	goto Failed;
    }
    memset(work, 0, sizeof *work);
#ifdef USING_HEIMDAL
    work->name.name_string.val = parsed_princ->name.name_string.val;
    work->name.name_string.len = 1;
    work->realm = "";
#else
    work->data = krb5_princ_component(k5context, parsed_princ, 0);
    work->length = 1;
#endif
    code = krb5_unparse_name(k5context, work, &first);
    if (code) goto Failed;
    cp = first + strlen(first);
    if (cp > first && *--cp == '@') *cp = 0;
    code = EDOM;	/* XXX */
    if (cp - first > 64
	|| strchr(first, '.')
	|| strchr(first, '@')) goto Failed;
    if (ncomp > 1) {
#ifdef USING_HEIMDAL
	++ work->name.name_string.val;
#else
	work->data = krb5_princ_component(k5context, parsed_princ, 1);
#endif
	code = krb5_unparse_name(k5context, work, &instance);
	if (code) goto Failed;
	cp = instance + strlen(instance);
	if (cp > instance && *--cp == '@') *cp = 0;
	code = EDOM;	/* XXX */
	if (cp == instance || cp - instance > 64
	    || strchr(instance, '@')) goto Failed;
    }
#ifdef USING_HEIMDAL
    work->name.name_string.len = 0;
#else
    work->length = 0;
#endif
    work->realm = parsed_princ->realm;
    code = krb5_unparse_name(k5context, work, &realm);
    if (code) goto Failed;
    code = EDOM;	/* XXX */
    if (strlen(realm) > 64+1) goto Failed;
    if (adir && afs_rxk5_is_local_realm(adir, realm+1)) {
	free(realm);
	realm = 0;
    }
    len = 1+strlen(first);
    if (instance) len += 1+strlen(instance);
    if (realm) len += 1+strlen(realm);
    *name = malloc(len);
    code = ENOMEM;
    if (!*name) goto Failed;
    strcpy(*name, first);
    cp = *name + strlen(*name);
    if (instance) {
	*cp++ = '.';
	strcpy(cp, instance);
	cp += strlen(cp);
    }
    if (realm) {
	if (downcase)
	    lcstring(cp, realm, 64);
	else
	    strcpy(cp, realm);
    }
    code = 0;
Failed:
    if (first) free(first);
    if (instance) free(instance);
    if (realm) free(realm);
    if (parsed_princ) krb5_free_principal(k5context, parsed_princ);
    return code;
}

int
afs_rxk5_split_name_instance(char* k5name, char** k4name, char** k4instance)
{
  /* this is crap, but... */
  int code, k5len, nlen, ilen;
  char *inst_pos, *dot_pos;
  code = 0;
  dot_pos = strchr(k5name, '.');

  if(!dot_pos) {
    *k4name = strdup(k5name);
    *k4instance = strdup("");
    goto out;
  }

  k5len = strlen(k5name);
  nlen = dot_pos - k5name;
  inst_pos = dot_pos + 1;
  ilen = strlen(inst_pos);

  *k4name = (char*) malloc((nlen+1) * sizeof(char));
  memset(*k4name, 0, (nlen+1));
  strncpy(*k4name, k5name, nlen);

  *k4instance = (char*) malloc((ilen+1) * sizeof(char));
  memset(*k4instance, 0, (ilen+1));
  strncpy(*k4instance, inst_pos, ilen);

 out:
    return code;
}

#define K5FORGE_IGNORE_ENCTYPE 0
#define K5FORGE_IGNORE_VNO 0

int Dflag;
#define k5forge_progname "afs_rxk5_k5forge"
int exitcode;

#if USING_HEIMDAL
#define deref_keyblock_enctype(kb) \
	((kb)->keytype)

#define deref_entry_keyblock(entry) \
	entry->keyblock

#define deref_session_key(creds) \
	creds->session

#define deref_enc_tkt_addrs(tkt) \
	tkt->caddr

#define deref_enc_length(enc) \
	((enc)->cipher.length)

#define deref_enc_data(enc) \
	((enc)->cipher.data)

#define krb5_free_keytab_entry_contents	krb5_kt_free_entry

#else
#define deref_keyblock_enctype(kb) \
	((kb)->enctype)

#define deref_entry_keyblock(entry) \
	entry->key

#define deref_session_key(creds) \
	creds->keyblock

#define deref_enc_tkt_addrs(tkt) \
	tkt->caddrs

#define deref_enc_length(enc) \
	((enc)->ciphertext.length)

#define deref_enc_data(enc) \
	((enc)->ciphertext.data)

#endif

#define deref_entry_enctype(entry) \
	deref_keyblock_enctype(&deref_entry_keyblock(entry))

/* Forge a krb5 ticket from a keytab entry, return it in creds, which caller
   must free */
int afs_rxk5_k5forge(krb5_context context,
    char* keytab,
    char* service,
    char* client,
    time_t starttime,
    time_t endtime,
    int *allowed_enctypes,
    int *paddress,
    krb5_creds** out_creds /* out */ )
{
    int code;
    krb5_keytab kt = 0;
    krb5_kt_cursor cursor[1];
    krb5_keytab_entry entry[1];
    krb5_principal service_principal = 0, client_principal = 0;
    krb5_ccache cc = 0;
    krb5_creds *creds = 0;
    krb5_enctype enctype;
    krb5_kvno kvno;
    krb5_keyblock session_key[1];
#if USING_HEIMDAL
    Ticket ticket_reply[1];
    EncTicketPart enc_tkt_reply[1];
    krb5_address address[30];
    krb5_addresses faddr[1];
    int temp_vno[1];
    time_t temp_time[2];
#else
    krb5_ticket ticket_reply[1];
    krb5_enc_tkt_part enc_tkt_reply[1];
    krb5_address address[30], *faddr[30];
#endif
    krb5_data * temp;
    int i;
    static int any_enctype[] = {0};

    *out_creds = 0;
    if (!(creds = malloc(sizeof *creds))) {
	code = ENOMEM;
	goto cleanup;
    }
    if (!allowed_enctypes)
	allowed_enctypes = any_enctype;

    client_principal = service_principal = 0;
    cc = 0;
    enctype = K5FORGE_IGNORE_ENCTYPE;
    kvno = K5FORGE_IGNORE_VNO;
    memset((char*)creds, 0, sizeof *creds);
    memset((char*)entry, 0, sizeof *entry);
    memset((char*)session_key, 0, sizeof *session_key);
    memset((char*)ticket_reply, 0, sizeof *ticket_reply);
    memset((char*)enc_tkt_reply, 0, sizeof *enc_tkt_reply);
    if (service && (code = krb5_parse_name(context, service,
	    &service_principal))) {
	com_err(k5forge_progname, code, "when parsing name <%s>", service);
	goto cleanup;
    }
    if (client && (code = krb5_parse_name(context, client,
	    &client_principal))) {
	com_err(k5forge_progname, code, "when parsing name <%s>", client);
	goto cleanup;
    }
    code = krb5_kt_resolve(context, keytab, &kt);
    if (code) {
	if (keytab)
	    com_err(k5forge_progname, code, "while resolving keytab %s", keytab);
	else
	    com_err(k5forge_progname, code, "while resolving default keytab");
	goto cleanup;
    }

    if (service) {
	for (i = 0; (enctype = allowed_enctypes[i]) || !i; ++i) {
	  code = krb5_kt_get_entry(context,
				   kt,
				   service_principal,
				   kvno,
				   enctype,
				   entry);
	  if (!code) {
	    if (allowed_enctypes[i])
		deref_keyblock_enctype(session_key) = allowed_enctypes[i];
	    break;
	  }
	}
	if (code) {
	  com_err(k5forge_progname, code,"while scanning keytab entries for %s", service);
	  goto cleanup;
	}
    } else {
	krb5_keytab_entry new[1];
	int best = -1;
	memset(new, 0, sizeof *new);
	if ((code == krb5_kt_start_seq_get(context, kt, cursor))) {
	    com_err(k5forge_progname, code, "while starting keytab scan");
	    goto cleanup;
	}
	while (!(code = krb5_kt_next_entry(context, kt, new, cursor))) {
	    for (i = 0;
		    allowed_enctypes[i] && allowed_enctypes[i]
			!= deref_entry_enctype(new); ++i)
		;
	    if ((!i || allowed_enctypes[i]) &&
		    (best < 0 || best > i)) {
		krb5_free_keytab_entry_contents(context, entry);
		*entry = *new;
		memset(new, 0, sizeof *new);
	    } else krb5_free_keytab_entry_contents(context, new);
	}
	if ((i = krb5_kt_end_seq_get(context, kt, cursor))) {
	    com_err(k5forge_progname, i, "while ending keytab scan");
	    code = i;
	    goto cleanup;
	}
	if (best < 0) {
	    com_err(k5forge_progname, code, "while scanning keytab");
	    goto cleanup;
	}
	deref_keyblock_enctype(session_key) = deref_entry_enctype(entry);
    }

    /* Make Ticket */

#if USING_HEIMDAL
    if ((code = krb5_generate_random_keyblock(context,
	    deref_keyblock_enctype(session_key), session_key))) {
	com_err(k5forge_progname, code, "while making session key");
	goto cleanup;
    }
    enc_tkt_reply->flags.initial = 1;
    enc_tkt_reply->transited.tr_type = DOMAIN_X500_COMPRESS;
    enc_tkt_reply->cname = client_principal->name;
    enc_tkt_reply->crealm = client_principal->realm;
    enc_tkt_reply->key = *session_key;
    {
	static krb5_data empty_string;
	enc_tkt_reply->transited.contents = empty_string;
    }
    enc_tkt_reply->authtime = starttime;
    enc_tkt_reply->starttime = temp_time;
    *enc_tkt_reply->starttime = starttime;
#if 0
    enc_tkt_reply->renew_till = temp_time + 1;
    *enc_tkt_reply->renew_till = endtime;
#endif
    enc_tkt_reply->endtime = endtime;
#else
    if ((code = krb5_c_make_random_key(context,
	    deref_keyblock_enctype(session_key), session_key))) {
	com_err(k5forge_progname, code, "while making session key");
	goto cleanup;
    }
#if !USING_SSL
    enc_tkt_reply->magic = KV5M_ENC_TKT_PART;
#define DATACAST	(unsigned char *)
#else
#define DATACAST	/**/
#endif
    enc_tkt_reply->flags |= TKT_FLG_INITIAL;
    enc_tkt_reply->transited.tr_type = KRB5_DOMAIN_X500_COMPRESS;
    enc_tkt_reply->session = session_key;
    enc_tkt_reply->client = client_principal;
    {
	static krb5_data empty_string;
	enc_tkt_reply->transited.tr_contents = empty_string;
    }
    enc_tkt_reply->times.authtime = starttime;
    enc_tkt_reply->times.starttime = starttime;	/* krb524init needs this */
    enc_tkt_reply->times.endtime = endtime;
#endif	/* USING_HEIMDAL */

    /* NB:  We will discard address for now--rxk5 will ignore caddr field
     in any case.  MIT branch does what it always did. */

    if (paddress && *paddress) {
	deref_enc_tkt_addrs(enc_tkt_reply) = faddr;
#if USING_HEIMDAL
	faddr->len = 0;
	faddr->val = address;
#endif
	for (i = 0; paddress[i]; ++i) {
#if USING_HEIMDAL
	    address[i].addr_type = KRB5_ADDRESS_INET;
	    address[i].address.data = (void*)(paddress+i);
	    address[i].address.length = sizeof(paddress[i]);
#else
#if !USING_SSL
	    address[i].magic = KV5M_ADDRESS;
	    address[i].addrtype = ADDRTYPE_INET;
#else
	    address[i].addrtype = AF_INET;
#endif
	    address[i].contents = (void*)(paddress+i);
	    address[i].length = sizeof(int);
	    faddr[i] = address+i;
#endif
	}
#if USING_HEIMDAL
	faddr->len = i;
#else
	faddr[i] = 0;
#endif
    }

#if USING_HEIMDAL
    ticket_reply->sname = service_principal->name;
    ticket_reply->realm = service_principal->realm;

    { /* crypto block */
	krb5_crypto crypto = 0;
	unsigned char *buf = 0;
	size_t buf_size, buf_len;
	char *what;

	ASN1_MALLOC_ENCODE(EncTicketPart, buf, buf_size,
	    enc_tkt_reply, &buf_len, code);
	if(code) {
	    com_err(k5forge_progname, code, "while encoding ticket");
	    goto cleanup;
	}

	if(buf_len != buf_size) {
	    com_err(k5forge_progname, code,
"%d != %d while encoding ticket (internal ASN.1 encoder error",
		buf_len, buf_size);
	    goto cleanup;
	}
	what = "krb5_crypto_init";
	code = krb5_crypto_init(context,
	    &deref_entry_keyblock(entry),
	    deref_entry_enctype(entry),
	    &crypto);
	if(!code) {
	    what = "krb5_encrypt";
	    code = krb5_encrypt_EncryptedData(context, crypto, KRB5_KU_TICKET,
		buf, buf_len, entry->vno, &(ticket_reply->enc_part));
	}
	if (buf) free(buf);
	if (crypto) krb5_crypto_destroy(context, crypto);
	if(code) {
	    com_err(k5forge_progname, code, "while %s", what);
	    goto cleanup;
	}
    } /* crypto block */
    ticket_reply->enc_part.etype = deref_entry_enctype(entry);
    ticket_reply->enc_part.kvno = temp_vno;
    *ticket_reply->enc_part.kvno = entry->vno;
    ticket_reply->tkt_vno = 5;
#else
    ticket_reply->server = service_principal;
    ticket_reply->enc_part2 = enc_tkt_reply;
    if ((code = krb5_encrypt_tkt_part(context, &deref_entry_keyblock(entry), ticket_reply))) {
        com_err(k5forge_progname, code, "while making ticket");
	goto cleanup;
    }
    ticket_reply->enc_part.kvno = entry->vno;
#endif

    /* Construct Creds */

    if ((code = krb5_copy_principal(context, service_principal,
	    &creds->server))) {
	com_err(k5forge_progname, code, "while copying service principal");
	goto cleanup;
    }
    if ((code = krb5_copy_principal(context, client_principal,
	    &creds->client))) {
	com_err(k5forge_progname, code, "while copying client principal");
	goto cleanup;
    }
    if ((code = krb5_copy_keyblock_contents(context, session_key,
	    &deref_session_key(creds)))) {
	com_err(k5forge_progname, code, "while copying session key");
	goto cleanup;
    }

#if USING_HEIMDAL
    creds->times.authtime = enc_tkt_reply->authtime;
    creds->times.starttime = *(enc_tkt_reply->starttime);
    creds->times.endtime = enc_tkt_reply->endtime;
#if 0
    creds->times.renew_till = *(enc_tkt_reply->renew_till);
#endif
    creds->flags.b = enc_tkt_reply->flags;
#else
    creds->times = enc_tkt_reply->times;
    creds->ticket_flags = enc_tkt_reply->flags;
#endif
    if (!deref_enc_tkt_addrs(enc_tkt_reply))
	;
    else if ((code = krb5_copy_addresses(context,
	    deref_enc_tkt_addrs(enc_tkt_reply), &creds->addresses))) {
	com_err(k5forge_progname, code, "while copying addresses");
	goto cleanup;
    }

#if USING_HEIMDAL
    {
      size_t creds_tkt_len;
      ASN1_MALLOC_ENCODE(Ticket, creds->ticket.data, creds->ticket.length,
			 ticket_reply, &creds_tkt_len, code);
      if(code) {
	com_err(k5forge_progname, code, "while encoding ticket");
	goto cleanup;
      }
    }
#else
    if ((code = encode_krb5_ticket(ticket_reply, &temp))) {
      com_err(k5forge_progname, code, "while encoding ticket");
      goto cleanup;
    }
    creds->ticket = *temp;
    free(temp);
#endif
    /* return creds */
    *out_creds = creds;
    creds = 0;
cleanup:
    if (deref_enc_data(&ticket_reply->enc_part))
	free(deref_enc_data(&ticket_reply->enc_part));
    krb5_free_keytab_entry_contents(context, entry);
    if (client_principal)
	krb5_free_principal(context, client_principal);
    if (service_principal)
	krb5_free_principal(context, service_principal);
    if (cc)
	krb5_cc_close(context, cc);
    if (kt)
	krb5_kt_close(context, kt);
    if (creds) krb5_free_creds(context, creds);
    krb5_free_keyblock_contents(context, session_key);
out:
    return code;
}

int
default_afs_rxk5_forge(krb5_context context,
    struct afsconf_dir *adir,
    char* service,
    krb5_creds* in_creds)
{
    struct afsconf_dir x[1];
    int code;
    char *afs_keytab;
    krb5_creds *k5creds;
    char *confdir_name = adir->name;
    char *to_free = 0;
    int allowed_enctypes[] = {
	/* XXX needs work... */
	ENCTYPE_AES256_CTS_HMAC_SHA1_96,
	ENCTYPE_AES128_CTS_HMAC_SHA1_96,
	ENCTYPE_DES3_CBC_SHA1,
#ifndef USING_HEIMDAL
#define ENCTYPE_ARCFOUR_HMAC_MD5 ENCTYPE_ARCFOUR_HMAC
#endif
	ENCTYPE_ARCFOUR_HMAC_MD5,
	ENCTYPE_DES_CBC_CRC, 0
    };

    if(!have_afs_rxk5_keytab(confdir_name)) {
	code = EDOM;	/* XXX */
	goto out;
    }

    if (!service) {
	to_free = service = get_afs_krb5_localauth_svc_princ(adir);
    }

    afs_keytab = get_afs_rxk5_keytab(confdir_name);
    code = afs_rxk5_k5forge(context,
	afs_keytab,
	service,
	service,
	START_OF_TIME, END_OF_TIME,
	allowed_enctypes,
	0 /* paddress */,
	&k5creds /* out */);
    if (code) goto out;

    memcpy(in_creds, k5creds, sizeof(krb5_creds));
    free(k5creds);

out:
    if (to_free) free(to_free);
    return code;
}
