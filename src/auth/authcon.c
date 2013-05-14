/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#if defined(UKERNEL)
#include "afs/param.h"
#else
#include <afs/param.h>
#endif


#if defined(UKERNEL)
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/stds.h"
#include "afs/pthread_glock.h"
#include "des/des.h"
#include "des/des_prototypes.h"
#include "rx/rxkad.h"
#include "rx/rx.h"
#include "afs/cellconfig.h"
#include "afs/keys.h"
#include "afs/auth.h"
#include "afs/pthread_glock.h"
#else /* defined(UKERNEL) */
#include <afs/stds.h>
#include <afs/pthread_glock.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/file.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <string.h>
#include <stdio.h>
#include <des.h>
#include <des_prototypes.h>
#include <rx/rxkad.h>
#if defined(USE_RXKAD_KEYTAB) && !defined(UKERNEL)
#include <afs/dirpath.h>
#include <krb5.h>
#endif
#include <rx/rx.h>
#include <errno.h>
#include "cellconfig.h"
#include "keys.h"
#include "auth.h"
#if defined(USE_RXKAD_KEYTAB) && !defined(UKERNEL)
#include "akimpersonate.h"
#endif
#endif /* defined(UKERNEL) */

/* return a null security object if nothing else can be done */
static afs_int32
QuickAuth(struct rx_securityClass **astr, afs_int32 *aindex)
{
    register struct rx_securityClass *tc;
    tc = rxnull_NewClientSecurityObject();
    *astr = tc;
    *aindex = 0;
    return 0;
}

#if !defined(UKERNEL)
/* Return an appropriate security class and index */
afs_int32
afsconf_ServerAuth(register struct afsconf_dir *adir, 
		   struct rx_securityClass **astr, 
		   afs_int32 *aindex)
{
    register struct rx_securityClass *tclass;

#ifdef USE_RXKAD_KEYTAB
    int keytab_enable = 0;
    char *keytab_name;
    size_t ktlen;
    ktlen = 5 + strlen(adir->name) + 1 + strlen(AFSDIR_RXKAD_KEYTAB_FILE) + 1;
    keytab_name = malloc(ktlen);
    if (keytab_name != NULL) {
	strcompose(keytab_name, ktlen, "FILE:", adir->name, "/",
		   AFSDIR_RXKAD_KEYTAB_FILE, (char *)NULL);
	if (rxkad_InitKeytabDecrypt(keytab_name) == 0)
	    keytab_enable = 1;
	free(keytab_name);
    }
#endif
    LOCK_GLOBAL_MUTEX;
    tclass = (struct rx_securityClass *)
	rxkad_NewServerSecurityObject(0, adir, afsconf_GetKey, NULL);
    if (tclass) {
	*astr = tclass;
	*aindex = 2;		/* kerberos security index */
#ifdef USE_RXKAD_KEYTAB
	if (keytab_enable)
	    rxkad_BindKeytabDecrypt(tclass);
#endif
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    } else {
	UNLOCK_GLOBAL_MUTEX;
	return 2;
    }
}
#endif /* !defined(UKERNEL) */

#if defined(USE_RXKAD_KEYTAB) && !defined(UKERNEL)
static afs_int32
K5Auth(struct afsconf_dir *adir,
       struct rx_securityClass **astr,
       afs_int32 *aindex,
       rxkad_level enclevel)
{
    struct rx_securityClass *tclass;
    krb5_context context = NULL;
    krb5_creds* fake_princ = NULL;
    krb5_principal service_princ = NULL;
    krb5_principal client_princ = NULL;
    krb5_error_code r = 0;
    struct ktc_encryptionKey session;
    char *keytab_name = NULL;
    size_t ktlen;

    ktlen = 5 + strlen(adir->name) + 1 + strlen(AFSDIR_RXKAD_KEYTAB_FILE) + 1;
    keytab_name = malloc(ktlen);
    if (!keytab_name) {
	return errno;
    }
    strcompose(keytab_name, ktlen, "FILE:", adir->name, "/",
	       AFSDIR_RXKAD_KEYTAB_FILE, (char *)NULL);

    r = krb5_init_context(&context);
    if (r)
	goto cleanup;

    r = krb5_build_principal(context, &client_princ, 1, "\0", "afs", NULL);
    if (r)
	goto cleanup;

    r = get_credv5_akimpersonate(context, keytab_name,
				 NULL, client_princ,
				 0, 0x7fffffff,
				 NULL,
				 &fake_princ);

    if (r == 0) {
	if (tkt_DeriveDesKey(get_creds_enctype(fake_princ),
			     get_cred_keydata(fake_princ),
			     get_cred_keylen(fake_princ),
			     &session) != 0) {
	    r = RXKADBADKEY;
	    goto cleanup;
	}
	tclass = (struct rx_securityClass *)
            rxkad_NewClientSecurityObject(enclevel, &session,
					  RXKAD_TKT_TYPE_KERBEROS_V5,
					  fake_princ->ticket.length,
					  fake_princ->ticket.data);
	if (tclass != NULL) {
	    *astr = tclass;
	    *aindex = 2;
	    r = 0;
	    goto cleanup;
	}
	r = 1;
    }

cleanup:
    free(keytab_name);
    if (fake_princ != NULL)
	krb5_free_creds(context, fake_princ);
    if (context != NULL)
	krb5_free_context(context);
    return r;
}
#endif

static afs_int32
GenericAuth(struct afsconf_dir *adir, 
	    struct rx_securityClass **astr, 
	    afs_int32 *aindex, 
	    rxkad_level enclevel)
{
    char tbuffer[256];
    struct ktc_encryptionKey key, session;
    struct rx_securityClass *tclass;
    afs_int32 kvno;
    afs_int32 ticketLen;
    register afs_int32 code;

#if defined(USE_RXKAD_KEYTAB) && !defined(UKERNEL)
    /* Try to do things the v5 way, before switching down to v4 */
    code = K5Auth(adir, astr, aindex, enclevel);
    if (code == 0)
	return 0;
#endif

    /* first, find the right key and kvno to use */
    code = afsconf_GetLatestKey(adir, &kvno, &key);
    if (code) {
	return QuickAuth(astr, aindex);
    }

    /* next create random session key, using key for seed to good random */
    des_init_random_number_generator(&key);
    code = des_random_key(&session);
    if (code) {
	return QuickAuth(astr, aindex);
    }

    /* now create the actual ticket */
    ticketLen = sizeof(tbuffer);
    memset(tbuffer, '\0', sizeof(tbuffer));
    code =
	tkt_MakeTicket(tbuffer, &ticketLen, &key, AUTH_SUPERUSER, "", "", 0,
		       0xffffffff, &session, 0, "afs", "");
    /* parms were buffer, ticketlen, key to seal ticket with, principal
     * name, instance and cell, start time, end time, session key to seal
     * in ticket, inet host, server name and server instance */
    if (code) {
	return QuickAuth(astr, aindex);
    }

    /* Next, we have ticket, kvno and session key, authenticate the connection.
     * We use a magic # instead of a constant because of basic compilation
     * order when compiling the system from scratch (rx/rxkad.h isn't installed
     * yet). */
    tclass = (struct rx_securityClass *)
	rxkad_NewClientSecurityObject(enclevel, &session, kvno, ticketLen,
				      tbuffer);
    *astr = tclass;
    *aindex = 2;		/* kerberos security index */
    return 0;
}

/* build a fake ticket for 'afs' using keys from adir, returning an
 * appropriate security class and index
 */
afs_int32
afsconf_ClientAuth(struct afsconf_dir * adir, struct rx_securityClass ** astr,
		   afs_int32 * aindex)
{
    afs_int32 rc;

    LOCK_GLOBAL_MUTEX;
    rc = GenericAuth(adir, astr, aindex, rxkad_clear);
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}

/* build a fake ticket for 'afs' using keys from adir, returning an
 * appropriate security class and index.  This one, unlike the above,
 * tells rxkad to encrypt the data, too.
 */
afs_int32
afsconf_ClientAuthSecure(struct afsconf_dir *adir, 
			 struct rx_securityClass **astr, 
			 afs_int32 *aindex)
{
    afs_int32 rc;

    LOCK_GLOBAL_MUTEX;
    rc = GenericAuth(adir, astr, aindex, rxkad_crypt);
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}
