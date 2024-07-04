/* AUTORIGHTS
Copyright (C) 2003 - 2010 Chaskiel Grundman
Copyright (c) 2011 Your Filesystem Inc.
Copyright (c) 2012 Sine Nomine Associates
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

#include <roken.h>

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

#include <hcrypto/des.h>

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
static char authas_name[256];
static char authas_inst[256];

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

/**
 * Connect to all servers using authenticated connections, using the local
 * KeyFile to appear as an arbitrary user.
 *
 * @param[in] aname  The pts username to impersonate
 *
 * @note aname is krb4-based name, not a krb5 principal. So for example, you
 *       probably want to give "user.admin" instead of "user/admin".
 *
 * @return operation status
 *  @retval 0 success
 */
int
afscp_LocalAuthAs(const char *aname)
{
    const char *ainst = strchr(aname, '.');
    size_t namelen, instlen;

    if (ainst) {
	namelen = ainst - aname;
	ainst++;
	instlen = strlen(ainst);
    } else {
	namelen = strlen(aname);
	ainst = "";
	instlen = 0;
    }

    if (namelen+1 > sizeof(authas_name) || instlen+1 > sizeof(authas_inst)) {
	return EINVAL;
    }
    strncpy(authas_name, aname, namelen);
    strncpy(authas_inst, ainst, instlen);
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

static int
_GetLocalSecurityObject(struct afscp_cell *cell,
                        char *aname, char *ainst)
{
    int code = 0;
    char tbuffer[256];
    struct ktc_encryptionKey key, session;
    struct rx_securityClass *tc;
    afs_int32 kvno;
    afs_int32 ticketLen;
    rxkad_level lev;
    struct afsconf_dir *tdir;

    tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!tdir) {
	code = AFSCONF_FAILURE;
	goto done;
    }

    code = afsconf_GetLatestKey(tdir, &kvno, &key);
    if (code) {
	goto done;
    }

    DES_init_random_number_generator((DES_cblock *)&key);
    code = DES_new_random_key((DES_cblock *)&session);
    if (code) {
	goto done;
    }

    ticketLen = sizeof(tbuffer);
    memset(tbuffer, 0, sizeof(tbuffer));
    code = tkt_MakeTicket(tbuffer, &ticketLen, &key, aname, ainst, "", 0,
                          0xffffffff, &session, 0, "afs", "");
    if (code) {
	goto done;
    }

    if (insecure) {
	lev = rxkad_clear;
    } else {
	lev = rxkad_crypt;
    }

    tc = (struct rx_securityClass *)
        rxkad_NewClientSecurityObject(lev, &session, kvno, ticketLen,
	                              tbuffer);
    if (!tc) {
	code = RXKADBADKEY;
	goto done;
    }

    cell->security = tc;
    cell->scindex = 2;

 done:
    if (tdir) {
	afsconf_Close(tdir);
    }
    return code;
}

int
_GetSecurityObject(struct afscp_cell *cell)
{
    int code = ENOENT;
#ifdef HAVE_KERBEROS
    krb5_context context = NULL;
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

    if (authas_name[0]) {
	code = _GetLocalSecurityObject(cell, authas_name, authas_inst);
	if (code == 0) {
	    return 0;
	}
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
	} else
	    goto try_anon;
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
	goto try_anon;
    }

    code = krb5_get_credentials(context, 0, cc, &match, &cred);
    if (code != 0) {
	krb5_free_principal(context, match.server);
	match.server = NULL;

	code = krb5_build_principal(context, &match.server,
				    strlen(realm), realm, "afs", NULL);
	if (code == 0)
	    code = krb5_get_credentials(context, 0, cc, &match, &cred);
	if (code != 0) {
	    krb5_free_cred_contents(context, &match);
	    if (cc)
		krb5_cc_close(context, cc);
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
    if (context != NULL) {
        krb5_free_context(context);
    }
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
