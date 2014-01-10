/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

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
#define KERBEROS_APPLE_DEPRECATED(x)
#include <krb5.h>
#endif
#include <rx/rx.h>
#include <errno.h>
#include <afs/afsutil.h>
#include "cellconfig.h"
#include "keys.h"
#include "auth.h"
#if defined(USE_RXKAD_KEYTAB) && !defined(UKERNEL)
#include "akimpersonate.h"
#endif

/* return a null security object if nothing else can be done */
static afs_int32
QuickAuth(struct rx_securityClass **astr, afs_int32 *aindex)
{
    struct rx_securityClass *tc;
    tc = rxnull_NewClientSecurityObject();
    *astr = tc;
    *aindex = RX_SECIDX_NULL;
    return 0;
}

#if !defined(UKERNEL)
/* Return an appropriate security class and index */
afs_int32
afsconf_ServerAuth(void *arock,
		   struct rx_securityClass **astr,
		   afs_int32 *aindex)
{
    struct afsconf_dir *adir = (struct afsconf_dir *) arock;
    struct rx_securityClass *tclass;
#ifdef USE_RXKAD_KEYTAB
    int keytab_enable = 0;
    char *csdb_name;
    size_t csdblen;
    char *keytab_name;
    size_t ktlen;
    csdblen = strlen(adir->name) + 1 + strlen(AFSDIR_CELLSERVDB_FILE) + 1;
    csdb_name = malloc(csdblen);
    ktlen = 5 + strlen(adir->name) + 1 + strlen(AFSDIR_RXKAD_KEYTAB_FILE) + 1;
    keytab_name = malloc(ktlen);
    if (csdb_name != NULL && keytab_name != NULL) {
	strcompose(csdb_name, csdblen, adir->name, "/",
	           AFSDIR_CELLSERVDB_FILE, (char *)NULL);
	strcompose(keytab_name, ktlen, "FILE:", adir->name, "/",
		   AFSDIR_RXKAD_KEYTAB_FILE, (char *)NULL);
	if (rxkad_InitKeytabDecrypt(csdb_name, keytab_name) == 0)
	    keytab_enable = 1;
    }
    free(csdb_name);
    free(keytab_name);
#endif
    LOCK_GLOBAL_MUTEX;
    tclass = (struct rx_securityClass *)
	rxkad_NewServerSecurityObject(0, adir, afsconf_GetKey, NULL);
    if (tclass) {
	*astr = tclass;
	*aindex = RX_SECIDX_KAD;
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
	    *aindex = RX_SECIDX_KAD;
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
	    rxkad_level enclevel,
	    int noauth_fallback)
{
    char tbuffer[256];
    struct ktc_encryptionKey key, session;
    struct rx_securityClass *tclass;
    afs_int32 kvno;
    afs_int32 ticketLen;
    afs_int32 code;

#if defined(USE_RXKAD_KEYTAB) && !defined(UKERNEL)
    /* Try to do things the v5 way, before switching down to v4 */
    code = K5Auth(adir, astr, aindex, enclevel);
    if (code == 0)
	return 0;
#endif

    /* first, find the right key and kvno to use */
    code = afsconf_GetLatestKey(adir, &kvno, &key);
    if (code) {
	goto error;
    }

    /* next create random session key, using key for seed to good random */
    des_init_random_number_generator(ktc_to_cblock(&key));
    code = des_random_key(ktc_to_cblock(&session));
    if (code) {
	goto error;
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
	goto error;
    }

    /* Next, we have ticket, kvno and session key, authenticate the connection.
     * We use a magic # instead of a constant because of basic compilation
     * order when compiling the system from scratch (rx/rxkad.h isn't installed
     * yet). */
    tclass = (struct rx_securityClass *)
	rxkad_NewClientSecurityObject(enclevel, &session, kvno, ticketLen,
				      tbuffer);
    *astr = tclass;
    *aindex = RX_SECIDX_KAD;
    return 0;

 error:
    if (noauth_fallback) {
	return QuickAuth(astr, aindex);
    }
    return code;
}

/* build a fake ticket for 'afs' using keys from adir, returning an
 * appropriate security class and index
 */
afs_int32
afsconf_ClientAuth(void *arock, struct rx_securityClass ** astr,
		   afs_int32 * aindex)
{
    struct afsconf_dir * adir = (struct afsconf_dir *) arock;
    afs_int32 rc;

    LOCK_GLOBAL_MUTEX;
    rc = GenericAuth(adir, astr, aindex, rxkad_clear, 1);
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}

/* build a fake ticket for 'afs' using keys from adir, returning an
 * appropriate security class and index.  This one, unlike the above,
 * tells rxkad to encrypt the data, too.
 */
afs_int32
afsconf_ClientAuthSecure(void *arock,
			 struct rx_securityClass **astr,
			 afs_int32 *aindex)
{
    struct afsconf_dir *adir = (struct afsconf_dir *) arock;
    afs_int32 rc;

    LOCK_GLOBAL_MUTEX;
    rc = GenericAuth(adir, astr, aindex, rxkad_crypt, 1);
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}

/*!
 * Build a security class from the user's current tokens
 *
 * This function constructs an RX security class from a user's current
 * tokens.
 *
 * @param[in] info	The cell information structure
 * @param[in] flags	Security flags describing the desired mechanism
 * @param[out] sc	The selected security class
 * @param[out] scIndex  The index of the selected class
 * @parma[out] expires  The expiry time of the tokens used to build the class
 *
 * Only the AFSCONF_SECOPTS_ALWAYSENCRYPT flag will modify the behaviour of
 * this function - it determines whether a cleartext, or encrypting, security
 * class is provided.
 *
 * @return
 *     0 on success, non-zero on failure. An error code of
 *     AFSCONF_NO_SECURITY_CLASS indicates that were were unable to build a
 *     security class using the selected tokens.
 */

afs_int32
afsconf_ClientAuthToken(struct afsconf_cell *info,
			afsconf_secflags flags,
			struct rx_securityClass **sc,
			afs_int32 *scIndex,
			time_t *expires)
{
    struct ktc_principal sname;
    struct ktc_token ttoken;
    int encryptLevel;
    afs_int32 code;

    *sc = NULL;
    *scIndex = RX_SECIDX_NULL;

    strcpy(sname.cell, info->name);
    sname.instance[0] = 0;
    strcpy(sname.name, "afs");
    code = ktc_GetToken(&sname, &ttoken, sizeof(ttoken), NULL);

    if (code == 0) {
	/* XXX - We should think about how to handle this */
	if (ttoken.kvno < 0 || ttoken.kvno > 256) {
	     fprintf(stderr,
		    "funny kvno (%d) in ticket, proceeding\n",
		    ttoken.kvno);
	}
	if (flags & AFSCONF_SECOPTS_ALWAYSENCRYPT)
	    encryptLevel = rxkad_crypt;
	else
	    encryptLevel = rxkad_clear;
	*sc = rxkad_NewClientSecurityObject(encryptLevel,
					    &ttoken.sessionKey,
					    ttoken.kvno,
					    ttoken.ticketLen,
					    ttoken.ticket);
	*scIndex = RX_SECIDX_KAD;
	if (expires)
	    *expires = ttoken.endTime;
    }
    if (*sc == NULL)
	return AFSCONF_NO_SECURITY_CLASS;

    return code;
}

/*!
 * Build a set of security classes suitable for a server accepting
 * incoming connections
 */
#if !defined(UKERNEL)
void
afsconf_BuildServerSecurityObjects(struct afsconf_dir *dir,
				   afs_uint32 flags,
			           struct rx_securityClass ***classes,
			           afs_int32 *numClasses)
{
#ifdef USE_RXKAD_KEYTAB
    int keytab_enable = 0;
    char *csdb_name;
    size_t csdblen;
    char *keytab_name;
    size_t ktlen;
    csdblen = strlen(dir->name) + 1 + strlen(AFSDIR_CELLSERVDB_FILE) + 1;
    csdb_name = malloc(csdblen);
    ktlen = 5 + strlen(dir->name) + 1 + strlen(AFSDIR_RXKAD_KEYTAB_FILE) + 1;
    keytab_name = malloc(ktlen);
    if (csdb_name != NULL && keytab_name != NULL) {
	strcompose(csdb_name, csdblen, dir->name, "/",
	           AFSDIR_CELLSERVDB_FILE, (char *)NULL);
	strcompose(keytab_name, ktlen, "FILE:", dir->name, "/",
		   AFSDIR_RXKAD_KEYTAB_FILE, (char *)NULL);
	if (rxkad_InitKeytabDecrypt(csdb_name, keytab_name) == 0)
	    keytab_enable = 1;
    }
    free(csdb_name);
    free(keytab_name);
#endif
    if (flags & AFSCONF_SEC_OBJS_RXKAD_CRYPT)
	*numClasses = 4;
    else
	*numClasses = 3;

    *classes = calloc(*numClasses, sizeof(**classes));

    (*classes)[0] = rxnull_NewServerSecurityObject();
    (*classes)[1] = NULL;
    (*classes)[2] = rxkad_NewServerSecurityObject(0, dir,
						  afsconf_GetKey, NULL);
#ifdef USE_RXKAD_KEYTAB
    if (keytab_enable)
	rxkad_BindKeytabDecrypt((*classes)[2]);
#endif
    if (flags & AFSCONF_SEC_OBJS_RXKAD_CRYPT) {
	(*classes)[3] = rxkad_NewServerSecurityObject(rxkad_crypt, dir,
						      afsconf_GetKey, NULL);
#ifdef USE_RXKAD_KEYTAB
	if (keytab_enable)
	    rxkad_BindKeytabDecrypt((*classes)[3]);
#endif
    }
}
#endif

/*!
 * Pick a security class to use for an outgoing connection
 *
 * This function selects an RX security class to use for an outgoing
 * connection, based on the set of security flags provided.
 *
 * @param[in] dir
 * 	The configuration directory structure for this cell. If NULL,
 * 	no classes requiring local configuration will be returned.
 * @param[in] flags
 * 	A set of flags to determine the properties of the security class which
 * 	is selected
 * 	- AFSCONF_SECOPTS_NOAUTH - return an anonymous secirty class
 * 	- AFSCONF_SECOPTS_LOCALAUTH - use classes which have local key
 * 		material available.
 * 	- AFSCONF_SECOPTS_ALWAYSENCRYPT - use classes in encrypting, rather
 * 	 	than authentication or integrity modes.
 * 	- AFSCONF_SECOPTS_FALLBACK_NULL - if no suitable class can be found,
 * 		then fallback to the rxnull security class.
 * @param[in] info
 * 	The cell information structure for the current cell. If this is NULL,
 * 	then use a version locally obtained using the cellName.
 * @param[in] cellName
 * 	The cellName to use when obtaining cell information (may be NULL if
 * 	info is specified)
 * @param[out] sc
 * 	The selected security class
 * @param[out] scIndex
 * 	The index of the selected security class
 * @param[out] expires
 * 	The expiry time of the tokens used to construct the class. Will be
 * 	NEVER_DATE if the class has an unlimited lifetime. If NULL, the
 * 	function won't store the expiry date.
 *
 * @return
 * 	Returns 0 on success, or a com_err error code on failure.
 */
afs_int32
afsconf_PickClientSecObj(struct afsconf_dir *dir, afsconf_secflags flags,
		         struct afsconf_cell *info,
		         char *cellName, struct rx_securityClass **sc,
			 afs_int32 *scIndex, time_t *expires) {
    struct afsconf_cell localInfo;
    afs_int32 code = 0;

    *sc = NULL;
    *scIndex = RX_SECIDX_NULL;
    if (expires)
	expires = 0;

    if ( !(flags & AFSCONF_SECOPTS_NOAUTH) ) {
	if (!dir)
	    return AFSCONF_NOCELLDB;

	if (flags & AFSCONF_SECOPTS_LOCALAUTH) {
	    int fallback = 0;
	    if (flags & AFSCONF_SECOPTS_FALLBACK_NULL)
		fallback = 1;

	    LOCK_GLOBAL_MUTEX;
	    if (flags & AFSCONF_SECOPTS_ALWAYSENCRYPT)
		code = GenericAuth(dir, sc, scIndex, rxkad_crypt, fallback);
	    else
		code = GenericAuth(dir, sc, scIndex, rxkad_clear, fallback);
	    UNLOCK_GLOBAL_MUTEX;

	    if (code)
		goto out;

	    if (expires)
		*expires = NEVERDATE;
	} else {
	    if (info == NULL) {
		code = afsconf_GetCellInfo(dir, cellName, NULL, &localInfo);
		if (code)
		    goto out;
		info = &localInfo;
	    }

	    code = afsconf_ClientAuthToken(info, flags, sc, scIndex, expires);
	    if (code && !(flags & AFSCONF_SECOPTS_FALLBACK_NULL))
		goto out;

	    /* If we didn't get a token, we'll just run anonymously */
	    code = 0;
	}
    }
    if (*sc == NULL) {
	*sc = rxnull_NewClientSecurityObject();
	*scIndex = RX_SECIDX_NULL;
	if (expires)
	    *expires = NEVERDATE;
    }

out:
    return code;
}
