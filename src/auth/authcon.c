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

#include <roken.h>

#ifdef IGNORE_SOME_GCC_WARNINGS
# pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif

#define HC_DEPRECATED
#include <hcrypto/des.h>
#include <hcrypto/rand.h>

#include <rx/rxkad.h>
#include <rx/rx.h>

#include <afs/pthread_glock.h>

#include "cellconfig.h"
#include "keys.h"
#include "ktc.h"
#include "auth.h"
#include "authcon.h"

#ifdef AFS_RXGK_ENV
# include <rx/rxgk.h>
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

static int _afsconf_GetRxkadKrb5Key(void *arock, int kvno, int enctype, void *outkey,
				    size_t *keylen)
{
    struct afsconf_dir *adir = arock;
    struct afsconf_typedKey *kobj;
    struct rx_opaque *keymat;
    afsconf_keyType tktype;
    int tkvno, tenctype;
    int code;

    code = afsconf_GetKeyByTypes(adir, afsconf_rxkad_krb5, kvno, enctype, &kobj);
    if (code != 0)
	return code;
    afsconf_typedKey_values(kobj, &tktype, &tkvno, &tenctype, &keymat);
    if (*keylen < keymat->len) {
	afsconf_typedKey_put(&kobj);
	return AFSCONF_BADKEY;
    }
    memcpy(outkey, keymat->val, keymat->len);
    *keylen = keymat->len;
    afsconf_typedKey_put(&kobj);
    return 0;
}


/* Return an appropriate security class and index */
afs_int32
afsconf_ServerAuth(void *arock,
		   struct rx_securityClass **astr,
		   afs_int32 *aindex)
{
    struct afsconf_dir *adir = (struct afsconf_dir *) arock;
    struct rx_securityClass *tclass;

    LOCK_GLOBAL_MUTEX;
    tclass = (struct rx_securityClass *)
	rxkad_NewKrb5ServerSecurityObject(0, adir, afsconf_GetKey,
					  _afsconf_GetRxkadKrb5Key, NULL);
    if (tclass) {
	*astr = tclass;
	*aindex = RX_SECIDX_KAD;
	UNLOCK_GLOBAL_MUTEX;
	return 0;
    } else {
	UNLOCK_GLOBAL_MUTEX;
	return 2;
    }
}

static afs_int32
GenericAuth(struct afsconf_dir *adir,
	    struct rx_securityClass **astr,
	    afs_int32 *aindex,
	    rxkad_level enclevel)
{
    int enctype_preflist[]={18, 17, 23, 16, 0};
    char tbuffer[512];
    struct ktc_encryptionKey key, session;
    struct rx_securityClass *tclass;
    afs_int32 kvno;
    afs_int32 ticketLen;
    afs_int32 code;
    int use_krb5=0;
    struct afsconf_typedKey *kobj;
    struct rx_opaque *keymat;
    int *et;

    /* first, find the right key and kvno to use */

    et = enctype_preflist;
    while(*et != 0) {
	code = afsconf_GetLatestKeyByTypes(adir, afsconf_rxkad_krb5, *et,
					   &kobj);
	if (code == 0) {
	    afsconf_keyType tktype;
	    int tenctype;
	    afsconf_typedKey_values(kobj, &tktype, &kvno, &tenctype, &keymat);
	    RAND_add(keymat->val, keymat->len, 0.0);
	    use_krb5 = 1;
	    break;
	}
	et++;
    }

    if (use_krb5 == 0) {
	code = afsconf_GetLatestKey(adir, &kvno, &key);
	if (code) {
	    return QuickAuth(astr, aindex);
	}
	/* next create random session key, using key for seed to good random */
	DES_init_random_number_generator((DES_cblock *) &key);
    }
    code = DES_new_random_key((DES_cblock *) &session);
    if (code) {
	if (use_krb5)
	    afsconf_typedKey_put(&kobj);
	return QuickAuth(astr, aindex);
    }

    if (use_krb5) {
	ticketLen = sizeof(tbuffer);
	memset(tbuffer, '\0', sizeof(tbuffer));
	code =
	    tkt_MakeTicket5(tbuffer, &ticketLen, *et, &kvno, keymat->val,
			    keymat->len, AUTH_SUPERUSER, "", "", 0, 0x7fffffff,
			    &session, "afs", "");
	afsconf_typedKey_put(&kobj);
    } else {
	/* now create the actual ticket */
	ticketLen = sizeof(tbuffer);
	memset(tbuffer, '\0', sizeof(tbuffer));
	code =
	    tkt_MakeTicket(tbuffer, &ticketLen, &key, AUTH_SUPERUSER, "", "", 0,
			   0xffffffff, &session, 0, "afs", "");
	/* parms were buffer, ticketlen, key to seal ticket with, principal
	 * name, instance and cell, start time, end time, session key to seal
	 * in ticket, inet host, server name and server instance */
    }
    if (code) {
	return QuickAuth(astr, aindex);
    }

    /* Next, we have ticket, kvno and session key, authenticate the connection.*/
    tclass = (struct rx_securityClass *)
	rxkad_NewClientSecurityObject(enclevel, &session, kvno, ticketLen,
				      tbuffer);
    *astr = tclass;
    *aindex = RX_SECIDX_KAD;
    return 0;
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
    rc = GenericAuth(adir, astr, aindex, rxkad_clear);
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
    rc = GenericAuth(adir, astr, aindex, rxkad_crypt);
    UNLOCK_GLOBAL_MUTEX;
    return rc;
}

/**
 * Print an rxgk token and make a security class with it
 *
 * Print an rxgk token and build a security class from it, returning the
 * correct index along with the class.
 *
 * As with the other ClientAuth variants, fall back to rxnull on errors.
 * The caller can check the returned aindex if necessary.
 *
 * If 'crypt' is nonzero, use the _CRYPT security level. Otherwise, if 'auth'
 * is nonzero, use the _AUTH security level. Otherwise, use _CLEAR.
 */
static afs_int32
_ClientAuthRXGK(void *arock, struct rx_securityClass **aclass,
		afs_int32 *aindex, int crypt, int auth)
{
#ifdef AFS_RXGK_ENV
    struct rx_securityClass *tclass;
    struct rx_opaque token = RX_EMPTY_OPAQUE;
    RXGK_Level level;
    RXGK_TokenInfo tokeninfo;
    rxgk_key cell_key = NULL, k0 = NULL;
    afs_int32 code, kvno, cell_enctype, k0_enctype;

    memset(&tokeninfo, 0, sizeof(tokeninfo));

    if (crypt)
	level = RXGK_LEVEL_CRYPT;
    else if (auth)
	level = RXGK_LEVEL_AUTH;
    else
	level = RXGK_LEVEL_CLEAR;

    code = afsconf_GetLatestRXGKKey(arock, &kvno, &cell_enctype, &cell_key);
    if (code != 0)
	goto done;

    /* assume that cell_key's enctype works for the our token's k0, too */
    k0_enctype = cell_enctype;

    tokeninfo.enctype = k0_enctype;
    tokeninfo.level = level;
    code = rxgk_print_token_and_key(&token, &tokeninfo, cell_key, kvno,
				    cell_enctype, &k0);
    if (code != 0)
	goto done;
    tclass = rxgk_NewClientSecurityObject(level, k0_enctype, k0, &token);
    if (tclass == NULL)
	code = RXGK_INCONSISTENCY;
 done:
    rxgk_release_key(&cell_key);
    rxgk_release_key(&k0);
    rx_opaque_freeContents(&token);
    if (code != 0)
	return QuickAuth(aclass, aindex);
    *aclass = tclass;
    *aindex = RX_SECIDX_GK;
    return code;
#else	/* AFS_RXGK_ENV */
    return QuickAuth(aclass, aindex);
#endif	/* !AFS_RXGK_ENV */
}

afs_int32
afsconf_ClientAuthRXGKClear(void *arock, struct rx_securityClass **aclass,
			    afs_int32 *aindex)
{
    return _ClientAuthRXGK(arock, aclass, aindex, 0, 0);
}

afs_int32
afsconf_ClientAuthRXGKAuth(void *arock, struct rx_securityClass **aclass,
			    afs_int32 *aindex)
{
    return _ClientAuthRXGK(arock, aclass, aindex, 0, 1);
}

afs_int32
afsconf_ClientAuthRXGKCrypt(void *arock, struct rx_securityClass **aclass,
			    afs_int32 *aindex)
{
    return _ClientAuthRXGK(arock, aclass, aindex, 1, 0);
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
    struct ktc_setTokenData *tokenSet = NULL;
    struct ktc_token ttoken;
    int encryptLevel;
    afs_int32 code;

    *sc = NULL;
    *scIndex = RX_SECIDX_NULL;

    if ((flags & AFSCONF_SECOPTS_RXGK)) {
	/* We don't support non-printed rxgk tokens yet */
	code = AFSCONF_NO_SECURITY_CLASS;
	goto out;
    }

    code = ktc_GetTokenEx(info->name, &tokenSet);
    if (code)
	goto out;

    code = token_extractRxkad(tokenSet, &ttoken, NULL, NULL);
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

out:
    token_FreeSet(&tokenSet);

    if (*sc == NULL)
	return AFSCONF_NO_SECURITY_CLASS;

    return code;
}

/*!
 * Set the security flags to be used for a particular configuration
 */
void
afsconf_SetSecurityFlags(struct afsconf_dir *dir,
			 afsconf_secflags flags)
{
    dir->securityFlags = flags;
}

static void
LogDesWarning(struct afsconf_bsso_info *info)
{
    if (info->logger == NULL) {
	return;
    }
    /* The blank newlines help this stand out a bit more in the log. */
    (*info->logger)("\n");
    (*info->logger)("WARNING: You are using single-DES keys in a KeyFile. Using "
		    "single-DES\n");
    (*info->logger)("WARNING: long-term keys is considered insecure, and it is "
		    "strongly\n");
    (*info->logger)("WARNING: recommended that you migrate to stronger "
		    "encryption. See\n");
    (*info->logger)("WARNING: OPENAFS-SA-2013-003 on "
		    "http://www.openafs.org/security/\n");
    (*info->logger)("WARNING: for details.\n");
    (*info->logger)("\n");
}

static void
LogNoKeysWarning(struct afsconf_bsso_info *info)
{
    if (info->logger == NULL) {
	return;
    }
    (*info->logger)("WARNING: No encryption keys found! "
		    "All authenticated accesses will fail. "
		    "Run akeyconvert or asetkey to import encryption keys.\n");
}

/* Older version of afsconf_BuildServerSecurityObjects_int. In-tree callers
 * should use afsconf_BuildServerSecurityObjects_int where possible. */
void
afsconf_BuildServerSecurityObjects(void *rock,
				   struct rx_securityClass ***classes,
				   afs_int32 *numClasses)
{
    struct afsconf_bsso_info info;
    memset(&info, 0, sizeof(info));
    info.dir = rock;
    afsconf_BuildServerSecurityObjects_int(&info, classes, numClasses);
}

/*!
 * Build a set of security classes suitable for a server accepting
 * incoming connections
 */
void
afsconf_BuildServerSecurityObjects_int(struct afsconf_bsso_info *info,
				       struct rx_securityClass ***classes,
				       afs_int32 *numClasses)
{
    struct afsconf_dir *dir = info->dir;

    if (afsconf_GetLatestKey(dir, NULL, NULL) == 0) {
	LogDesWarning(info);
    }
    if (afsconf_CountKeys(dir) == 0) {
	LogNoKeysWarning(info);
    }

    *numClasses = RX_SECIDX_GK+1;

    *classes = calloc(*numClasses, sizeof(**classes));

    (*classes)[RX_SECIDX_NULL] = rxnull_NewServerSecurityObject();
    (*classes)[RX_SECIDX_KAD] =
	rxkad_NewKrb5ServerSecurityObject(0, dir, afsconf_GetKey,
					  _afsconf_GetRxkadKrb5Key, NULL);

    if (dir->securityFlags & AFSCONF_SECOPTS_ALWAYSENCRYPT)
	(*classes)[RX_SECIDX_KAE] =
	    rxkad_NewKrb5ServerSecurityObject(rxkad_crypt, dir, afsconf_GetKey,
					      _afsconf_GetRxkadKrb5Key, NULL);
#ifdef AFS_RXGK_ENV
    (*classes)[RX_SECIDX_GK] =
	rxgk_NewServerSecurityObject(dir, afsconf_GetRXGKKey);
#endif
}

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
 * 	- AFSCONF_SECOPTS_NEVERENCRYPT - avoid encrypting classes (currently
 * 		only valid with _RXGK)
 * 	- AFSCONF_SECOPTS_ALWAYSCLEAR - avoid encrypting or authenticating
 * 		classes (always use "clear" security classes) (currently
 * 		only valid with _RXGK)
 * 	- AFSCONF_SECOPTS_RXGK - only use rxgk security classes (currently
 * 		only valid with _LOCALAUTH)
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
	*expires = 0;

    if ( !(flags & AFSCONF_SECOPTS_NOAUTH) ) {
	if (!dir)
	    return AFSCONF_NOCELLDB;

	if (flags & AFSCONF_SECOPTS_LOCALAUTH) {
	    if ((flags & AFSCONF_SECOPTS_RXGK)) {
		if ((flags & AFSCONF_SECOPTS_ALWAYSCLEAR))
		    code = afsconf_ClientAuthRXGKClear(dir, sc, scIndex);
		else if ((flags & AFSCONF_SECOPTS_NEVERENCRYPT))
		    code = afsconf_ClientAuthRXGKAuth(dir, sc, scIndex);
		else
		    code = afsconf_ClientAuthRXGKCrypt(dir, sc, scIndex);

	    } else if (flags & AFSCONF_SECOPTS_ALWAYSENCRYPT)
		code = afsconf_ClientAuthSecure(dir, sc, scIndex);
	    else
		code = afsconf_ClientAuth(dir, sc, scIndex);

	    if (code)
		goto out;

	    /* The afsconf_ClientAuth functions will fall back to giving
	     * a rxnull object, which we don't want if localauth has been
	     * explicitly requested. Check for this, and bail out if we
	     * get one. Note that this leaks a security object at present
	     */
	    if (!(flags & AFSCONF_SECOPTS_FALLBACK_NULL) &&
		*scIndex == RX_SECIDX_NULL) {
		sc = NULL;
		code = AFSCONF_NOTFOUND;
		goto out;
	    }

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
