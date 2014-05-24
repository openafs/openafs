/* AUTORIGHTS
Copyright (C) 2003 - 2010 Chaskiel Grundman
All rights reserved

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <afsconfig.h>
#include <afs/param.h>

#include <ctype.h>
#include <afs/cellconfig.h>
#ifndef AFSCONF_CLIENTNAME
#include <afs/dirpath.h>
#define AFSCONF_CLIENTNAME AFSDIR_CLIENT_ETC_DIRPATH
#endif
#include <ubik.h>
#include <rx/rx_null.h>
#include <rx/rxkad.h>
#ifdef HAVE_KERBEROS
# define KERBEROS_APPLE_DEPRECATED(x)
# include <krb5.h>
#endif
#include "afscp.h"
#include "afscp_internal.h"

#ifdef HAVE_KRB5_CREDS_KEYBLOCK_ENCTYPE
#define Z_keydata(keyblock)     ((keyblock)->contents)
#define Z_keylen(keyblock)      ((keyblock)->length)
#define Z_credskey(creds)       (&(creds)->keyblock)
#define Z_enctype(keyblock)     ((keyblock)->enctype)
#else
#define Z_keydata(keyblock)     ((keyblock)->keyvalue.data)
#define Z_keylen(keyblock)      ((keyblock)->keyvalue.length)
#define Z_credskey(creds)       (&(creds)->session)
#define Z_enctype(keyblock)     ((keyblock)->keytype)
#endif

static int insecure = 0;
static int try_anonymous = 0;

int
afscp_Insecure(void)
{
    insecure = 1;
    return 0;
}

int
afscp_AnonymousAuth(int state)
{
    try_anonymous = state;
    return 0;
}

static struct afsconf_dir *confdir;

void
afscp_SetConfDir(char *confDir)
{
    if (confdir != NULL)
	afsconf_Close(confdir);

    confdir = afsconf_Open(confDir);
}

static int
_GetCellInfo(char *cell, struct afsconf_cell *celldata)
{
    int code;
    if (confdir == NULL)
	confdir = afsconf_Open(AFSCONF_CLIENTNAME);
    if (confdir == NULL) {
	return AFSCONF_NODB;
    }
    code = afsconf_GetCellInfo(confdir, cell, AFSCONF_VLDBSERVICE, celldata);
    return code;
}

static int
_GetNullSecurityObject(struct afscp_cell *cell)
{
    cell->security = (struct rx_securityClass *)rxnull_NewClientSecurityObject();
    cell->scindex = RX_SECIDX_NULL;
    return 0;
}

int
_GetSecurityObject(struct afscp_cell *cell)
{
    int code = ENOENT;
#ifdef HAVE_KERBEROS
    krb5_context context;
    krb5_creds match;
    krb5_creds *cred;
    krb5_ccache cc;
    char **realms, *realm;
    struct afsconf_cell celldata;
    char localcell[MAXCELLCHARS + 1];
    struct rx_securityClass *sc;
    struct ktc_encryptionKey k;
    int i;
    rxkad_level l;
    code = _GetCellInfo(cell->name, &celldata);
    if (code != 0) {
	goto try_anon;
    }

    code = krb5_init_context(&context);	/* see aklog.c main() */
    if (code != 0) {
	goto try_anon;
    }

    if (cell->realm == NULL) {
	realm = NULL;
	code = krb5_get_host_realm(context, celldata.hostName[0], &realms);

	if (code == 0) {
	    strlcpy(localcell, realms[0], sizeof(localcell));
	    krb5_free_host_realm(context, realms);
	    realm = localcell;
	}
    } else {
	realm = cell->realm;
	strlcpy(localcell, realm, MAXCELLCHARS + 1);
    }
    if (realm)
	if (realm == NULL) {
	    for (i = 0; (i < MAXCELLCHARS && cell->name[i]); i++) {
		if (isalpha(cell->name[i]))
		    localcell[i] = toupper(cell->name[i]);
		else
		    localcell[i] = cell->name[i];
	    }
	    localcell[i] = '\0';
	    realm = localcell;
	}
    cc = NULL;
    code = krb5_cc_default(context, &cc);

    memset(&match, 0, sizeof(match));
    Z_enctype(Z_credskey(&match)) = ENCTYPE_DES_CBC_CRC;

    if (code == 0)
	code = krb5_cc_get_principal(context, cc, &match.client);
    if (code == 0)
	code = krb5_build_principal(context, &match.server,
				    strlen(realm), realm,
				    "afs", cell->name, NULL);

    if (code != 0) {
	krb5_free_cred_contents(context, &match);
	if (cc)
	    krb5_cc_close(context, cc);
	krb5_free_context(context);
	goto try_anon;
    }

    code = krb5_get_credentials(context, 0, cc, &match, &cred);
    if (code != 0) {
	krb5_free_principal(context, match.server);
	match.server = NULL;

	code = krb5_build_principal(context, &match.server,
				    strlen(realm), realm, "afs", (void *)NULL);
	if (code == 0)
	    code = krb5_get_credentials(context, 0, cc, &match, &cred);
	if (code != 0) {
	    krb5_free_cred_contents(context, &match);
	    if (cc)
		krb5_cc_close(context, cc);
	    krb5_free_context(context);
	    goto try_anon;
	}
    }

    if (insecure)
	l = rxkad_clear;
    else
	l = rxkad_crypt;
    memcpy(&k.data, Z_keydata(Z_credskey(cred)), 8);
    sc = (struct rx_securityClass *)rxkad_NewClientSecurityObject
	(l, &k, RXKAD_TKT_TYPE_KERBEROS_V5,
	 cred->ticket.length, cred->ticket.data);
    krb5_free_creds(context, cred);
    krb5_free_cred_contents(context, &match);
    if (cc)
	krb5_cc_close(context, cc);
    krb5_free_context(context);
    cell->security = sc;
    cell->scindex = 2;
    return 0;

    try_anon:
#endif /* HAVE_KERBEROS */
    if (try_anonymous)
	return _GetNullSecurityObject(cell);
    else
	return code;
}

int
_GetVLservers(struct afscp_cell *cell)
{
    struct rx_connection *conns[MAXHOSTSPERCELL + 1];
    int i;
    int code;
    struct afsconf_cell celldata;

    code = _GetCellInfo(cell->name, &celldata);
    if (code != 0) {
	return code;
    }

    for (i = 0; i < celldata.numServers; i++) {
	conns[i] = rx_NewConnection(celldata.hostAddr[i].sin_addr.s_addr,
				    htons(AFSCONF_VLDBPORT),
				    USER_SERVICE_ID, cell->security,
				    cell->scindex);
    }
    conns[i] = 0;
    return ubik_ClientInit(conns, &cell->vlservers);
}
