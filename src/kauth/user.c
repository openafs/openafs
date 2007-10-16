/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This file provides the easiest, turn-key interface to the authication
 * package. */

#include <afsconfig.h>
#if defined(UKERNEL)
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header: /cvs/openafs/src/kauth/user.c,v 1.11.2.2 2007/04/10 18:43:43 shadow Exp $");

#if defined(UKERNEL)
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/stds.h"
#include "afs/com_err.h"
#include "afs/cellconfig.h"
#include "afs/auth.h"
#include "afs/ptint.h"
#include "afs/pterror.h"
#include "afs/ptserver.h"
#include "rx/rx.h"
#include "rx/rx_globals.h"
#include "rx/rxkad.h"
#include "afs/kauth.h"
#include "afs/kautils.h"
#include "afs/afsutil.h"
#else /* defined(UKERNEL) */
#include <afs/stds.h>
#include <signal.h>
#include <afs/com_err.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <afs/cellconfig.h>
#include <afs/auth.h>
#include <afs/ptint.h>
#include <afs/pterror.h>
#include <afs/ptserver.h>
#include <afs/afsutil.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <rx/rxkad.h>		/* max ticket lifetime */
#include "kauth.h"
#include "kautils.h"
#endif /* defined(UKERNEL) */


afs_int32
GetTickets(char *name, char *instance, char *realm,
	   struct ktc_encryptionKey * key, Date lifetime,
	   afs_int32 * pwexpires, afs_int32 flags)
{
    afs_int32 code;
    struct ktc_token token;

    code = ka_GetAuthToken(name, instance, realm, key, lifetime, pwexpires);
    memset(key, 0, sizeof(*key));
    if (code)
	return code;
    code = ka_GetAFSTicket(name, instance, realm, lifetime, flags);
    return code;
}

/* 
 * Requires that you already possess a TGT.
 */
afs_int32
ka_GetAFSTicket(char *name, char *instance, char *realm, Date lifetime,
		afs_int32 flags)
{
    afs_int32 code;
    struct ktc_token token;
    struct ktc_principal server, client;

    code = ka_GetServerToken("afs", "", realm, lifetime, &token, /*new */ 1,
			     /*dosetpag */ flags);
    if (code)
	return code;
    if (ktc_OldPioctl()) {
	int local;
	char username[MAXKTCNAMELEN];
	afs_int32 viceId;
	int len;
	char *whoami = "UserAuthenticate: ptserver";

	strcpy(server.name, "afs");
	strcpy(server.instance, "");
	code = ka_ExpandCell(realm, server.cell, &local);
	if (code)
	    return code;
	code = pr_Initialize(0, AFSDIR_CLIENT_ETC_DIRPATH, server.cell);
	if (code) {
	    afs_com_err(whoami, code, "initializing ptserver in cell '%s'",
		    server.cell);
	    return 0;
	}
	len = strlen(name);
	if (instance[0])
	    len += strlen(instance) + 1;
	if (len >= sizeof(username)) {
	    fprintf(stderr, "user's name '%s'.'%s' would be too large\n",
		    name, instance);
	    return 0;
	}
	strcpy(username, name);
	if (instance[0]) {
	    strcat(username, ".");
	    strcat(username, instance);
	}
	code = pr_SNameToId(username, &viceId);
	/* Before going further, shutdown the pr ubik connection */
	pr_End();
	if ((code == 0) && (viceId == ANONYMOUSID))
	    code = PRNOENT;
	if (code) {
	    afs_com_err(whoami, code, "translating %s to id", username);
	    return 0;
	}

	sprintf(client.name, "AFS ID %d", viceId);
	strcpy(client.instance, "");
	strcpy(client.cell, server.cell);
	code = ktc_SetToken(&server, &token, &client, /*dosetpag */ 0);
	if (code)
	    return code;
    }
    return code;
}

#ifdef ka_UserAuthenticate
#undef ka_UserAuthenticate
#endif

afs_int32
ka_UserAuthenticateGeneral(afs_int32 flags, char *name, char *instance, char *realm, char *password, Date lifetime, afs_int32 * password_expires,	/* days 'til, or don't change if not set */
			   afs_int32 spare2, char **reasonP)
{
    int remainingTime = 0;
    struct ktc_encryptionKey key;
    afs_int32 code, dosetpag = 0;
    int (*old) ();

    if (reasonP)
	*reasonP = "";
    if ((flags & KA_USERAUTH_VERSION_MASK) != KA_USERAUTH_VERSION)
	return KAOLDINTERFACE;
    if ((strcmp(name, "root") == 0) && (instance == 0)) {
	if (reasonP)
	    *reasonP = "root is only authenticated locally";
	return KANOENT;
    }
    code = ka_Init(0);
    if (code)
	return code;

    ka_StringToKey(password, realm, &key);

/* 
 * alarm is set by klogin and kpasswd only so ignore for
 * NT
 */

#ifndef AFS_NT40_ENV
    {				/* Rx uses timers, save to be safe */
	if (rx_socket) {
	    /* don't reset alarms, rx already running */
	    remainingTime = 0;
	} else
	    remainingTime = alarm(0);
    }
#endif

#if !defined(AFS_NT40_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_USR_LINUX20_ENV) && !defined(AFS_XBSD_ENV)
    /* handle smoothly the case where no AFS system calls exists (yet) */
    old = (int (*)())signal(SIGSYS, SIG_IGN);
#endif
#ifdef	AFS_DECOSF_ENV
    (void)signal(SIGTRAP, SIG_IGN);
#endif /* AFS_DECOSF_ENV */
    if (instance == 0)
	instance = "";
    if (flags & KA_USERAUTH_ONLY_VERIFY) {
	code = ka_VerifyUserToken(name, instance, realm, &key);
	if (code == KABADREQUEST) {
	    des_string_to_key(password, &key);
	    code = ka_VerifyUserToken(name, instance, realm, &key);
	}
    } else {
#ifdef AFS_DUX40_ENV
	if (flags & KA_USERAUTH_DOSETPAG)
	    afs_setpag();
#else
#if !defined(UKERNEL) && !defined(AFS_NT40_ENV)
	if (flags & KA_USERAUTH_DOSETPAG)
	    setpag();
#endif
#endif
	if (flags & KA_USERAUTH_DOSETPAG2)
	    dosetpag = 1;
#ifdef AFS_KERBEROS_ENV
	if ((flags & KA_USERAUTH_DOSETPAG) || dosetpag)
	    ktc_newpag();
#endif
	if (lifetime == 0)
	    lifetime = MAXKTCTICKETLIFETIME;
	code =
	    GetTickets(name, instance, realm, &key, lifetime,
		       password_expires, dosetpag);
	if (code == KABADREQUEST) {
	    des_string_to_key(password, &key);
	    code =
		GetTickets(name, instance, realm, &key, lifetime,
			   password_expires, dosetpag);
	}

/* By the time 3.3 comes out, these "old-style" passwd programs should be 
 * well and truly obsolete.  Any passwords set with such a program
 * OUGHT to have been changed years ago.  Having 2 -or- 3
 * authentication RPCs generated for every klog plays hob with the
 * "failed login limits" code in the kaserver, and it's hard to
 * explain to admins just how to set the limit properly.  By removing 
 * this function, we can just double it internally in the kaserver, and 
 * not document anything.  kpasswd had the TRUNCATEPASSWORD "feature"
 * disabled on 10/02/90.
 */
#ifdef OLDCRUFT
	if ((code == KABADREQUEST) && (strlen(password) > 8)) {
	    /* try with only the first 8 characters incase they set their password
	     * with an old style passwd program. */
	    char pass8[9];
	    strncpy(pass8, password, 8);
	    pass8[8] = 0;
	    ka_StringToKey(pass8, realm, &key);
	    memset(pass8, 0, sizeof(pass8));
	    code =
		GetTickets(name, instance, realm, &key, lifetime,
			   password_expires, dosetpag);
	    if (code == 0) {
		fprintf(stderr, "%s %s\n%s %s\n%s\n",
			"Warning: you have typed a password longer than 8",
			"characters, but only the",
			"first 8 characters were actually significant.  If",
			"you change your password",
			"again this warning message will go away.\n");
	    }
	}
#endif /* OLDCRUFT */
    }

#ifndef AFS_NT40_ENV
    if (remainingTime) {
	pr_End();
	rx_Finalize();
	alarm(remainingTime);	/* restore timer, if any */
    }
#endif

    if (code && reasonP)
	switch (code) {
	case KABADREQUEST:
	    *reasonP = "password was incorrect";
	    break;
	case KAUBIKCALL:
	    *reasonP = "Authentication Server was unavailable";
	    break;
	default:
	    *reasonP = (char *)afs_error_message(code);
	}
    return code;
}

/* For backward compatibility */
afs_int32
ka_UserAuthenticate(char *name, char *instance, char *realm, char *password,
		    int doSetPAG, char **reasonP)
{
    return ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION +
				      ((doSetPAG) ? KA_USERAUTH_DOSETPAG : 0),
				      name, instance, realm, password,
				      /*lifetime */ 0, /*spare1,2 */ 0, 0,
				      reasonP);
}

#if !defined(UKERNEL)
afs_int32
ka_UserReadPassword(char *prompt, char *password, int plen, char **reasonP)
{
    afs_int32 code = 0;

    if (reasonP)
	*reasonP = "";
    code = ka_Init(0);
    if (code)
	return code;
    code = read_pw_string(password, plen, prompt, 0);
    if (code)
	code = KAREADPW;
    else if (strlen(password) == 0)
	code = KANULLPASSWORD;
    else
	return 0;

    if (reasonP) {
	*reasonP = (char *)afs_error_message(code);
    }
    return code;
}
#endif /* !defined(UKERNEL) */

afs_int32
ka_VerifyUserPassword(afs_int32 version, char *name, char *instance,
		      char *realm, char *password, int spare, char **reasonP)
{
    afs_int32 pwexpires;

    version &= KA_USERAUTH_VERSION_MASK;
    return ka_UserAuthenticateGeneral(version | KA_USERAUTH_ONLY_VERIFY, name,
				      instance, realm, password, 0,
				      &pwexpires, spare, reasonP);
}
