/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <afsconfig.h>
#include <afs/param.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>

RCSID
    ("$Header: /cvs/openafs/src/pam/afs_password.c,v 1.10.2.1 2005/05/30 03:37:48 shadow Exp $");

#include <sys/param.h>
#include <afs/kautils.h>
#include "afs_message.h"
#include "afs_util.h"
#include "afs_pam_msg.h"
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define RET(x) { retcode = (x); goto out; }

extern int
pam_sm_chauthtok(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
    int retcode = PAM_SUCCESS;
    int errcode = PAM_SUCCESS;
    int code;
    int origmask;
    int logmask = LOG_UPTO(LOG_INFO);
    int nowarn = 0;
    int use_first_pass = 0;
    int try_first_pass = 0;
    int ignore_root = 0;
    int got_authtok = 0;	/* got PAM_AUTHTOK upon entry */
    int torch_password = 1;
    int i;
    char my_password_buf[256];
    char instance[256];
    char realm[256];
    char cell[256];
    char *localcell;
    char *user = NULL, *password = NULL;
    char *new_password = NULL, *verify_password = NULL;
    char upwd_buf[2048];	/* size is a guess. */
    char *reason = NULL;
    struct ktc_encryptionKey oldkey, newkey;
    struct ktc_token token;
    struct ubik_client *conn = 0;
    struct pam_conv *pam_convp = NULL;
    struct passwd unix_pwd, *upwd = NULL;

#ifndef AFS_SUN56_ENV
    openlog(pam_afs_ident, LOG_CONS, LOG_AUTH);
#endif
    origmask = setlogmask(logmask);

    /*
     * Parse the user options.  Log an error for any unknown options.
     *
     * Todo options: PAM_SILENT
     */
    for (i = 0; i < argc; i++) {
	if (strcasecmp(argv[i], "debug") == 0) {
	    logmask |= LOG_MASK(LOG_DEBUG);
	    (void)setlogmask(logmask);
	} else if (strcasecmp(argv[i], "nowarn") == 0) {
	    nowarn = 1;
	} else if (strcasecmp(argv[i], "use_first_pass") == 0) {
	    use_first_pass = 1;
	} else if (strcasecmp(argv[i], "try_first_pass") == 0) {
	    try_first_pass = 1;
	} else if (strcasecmp(argv[i], "ignore_root") == 0) {
	    ignore_root = 1;
	} else {
	    pam_afs_syslog(LOG_ERR, PAMAFS_UNKNOWNOPT, argv[i]);
	}
    }

    if (use_first_pass)
	try_first_pass = 0;

    if (logmask && LOG_MASK(LOG_DEBUG)) {
	pam_afs_syslog(LOG_DEBUG, PAMAFS_OPTIONS, nowarn, use_first_pass,
		       try_first_pass);
	pam_afs_syslog(LOG_DEBUG, PAMAFS_PAMERROR, flags);
    }

    /* Try to get the user-interaction info, if available. */
    errcode = pam_get_item(pamh, PAM_CONV, (const void **)&pam_convp);
    if (errcode != PAM_SUCCESS) {
	pam_afs_syslog(LOG_WARNING, PAMAFS_NO_USER_INT);
	pam_convp = NULL;
    }

    /* Who are we trying to authenticate here? */
    if ((errcode =
	 pam_get_user(pamh, (const char **)&user,
		      "AFS username: ")) != PAM_SUCCESS) {
	pam_afs_syslog(LOG_ERR, PAMAFS_NOUSER, errcode);
	RET(PAM_USER_UNKNOWN);
    }

    if (logmask && LOG_MASK(LOG_DEBUG))
	pam_afs_syslog(LOG_DEBUG, PAMAFS_USERNAMEDEBUG, user);

    /*
     * If the user has a "local" (or via nss, possibly nss_dce) pwent,
     * and its uid==0, and "ignore_root" was given in pam.conf,
     * ignore the user.
     */
#if	defined(AFS_HPUX_ENV)
#if     defined(AFS_HPUX110_ENV)
    i = getpwnam_r(user, &unix_pwd, upwd_buf, sizeof(upwd_buf), &upwd);
#else /* AFS_HPUX110_ENV */
    i = getpwnam_r(user, &unix_pwd, upwd_buf, sizeof(upwd_buf));
    if (i == 0)			/* getpwnam_r success */
	upwd = &unix_pwd;
#endif /* else AFS_HPUX110_ENV */
    if (ignore_root && i == 0 && upwd->pw_uid == 0) {
	pam_afs_syslog(LOG_INFO, PAMAFS_IGNORINGROOT, user);
	RET(PAM_AUTH_ERR);
    }
#else
#if     defined(AFS_LINUX20_ENV) || defined(AFS_FBSD_ENV) || defined(AFS_NBSD_ENV)
    upwd = getpwnam(user);
#else
    upwd = getpwnam_r(user, &unix_pwd, upwd_buf, sizeof(upwd_buf));
#endif
    if (ignore_root && upwd != NULL && upwd->pw_uid == 0) {
	pam_afs_syslog(LOG_INFO, PAMAFS_IGNORINGROOT, user);
	RET(PAM_AUTH_ERR);
    }
#endif

    errcode = pam_get_item(pamh, PAM_AUTHTOK, (const void **)&password);
    if (errcode != PAM_SUCCESS || password == NULL) {
	if (use_first_pass) {
	    pam_afs_syslog(LOG_ERR, PAMAFS_PASSWD_REQ, user);
	    RET(PAM_AUTH_ERR);
	}
	password = NULL;	/* In case it isn't already NULL */
	if (logmask && LOG_MASK(LOG_DEBUG))
	    pam_afs_syslog(LOG_DEBUG, PAMAFS_NOFIRSTPASS, user);
    } else if (password[0] == '\0') {
	/* Actually we *did* get one but it was empty. */
	torch_password = 0;
	pam_afs_syslog(LOG_INFO, PAMAFS_NILPASSWORD, user);
	RET(PAM_NEW_AUTHTOK_REQD);
    } else {
	if (logmask && LOG_MASK(LOG_DEBUG))
	    pam_afs_syslog(LOG_DEBUG, PAMAFS_GOTPASS, user);
	torch_password = 0;
	got_authtok = 1;
    }
    if (!(use_first_pass || try_first_pass)) {
	password = NULL;
    }

    if (password == NULL) {
	torch_password = 1;
	if (use_first_pass)
	    RET(PAM_AUTH_ERR);	/* shouldn't happen */
	if (try_first_pass)
	    try_first_pass = 0;
	if (pam_convp == NULL || pam_convp->conv == NULL) {
	    pam_afs_syslog(LOG_ERR, PAMAFS_CANNOT_PROMPT);
	    RET(PAM_AUTH_ERR);
	}

	errcode = pam_afs_prompt(pam_convp, &password, 0, PAMAFS_PWD_PROMPT);
	if (errcode != PAM_SUCCESS || password == NULL) {
	    pam_afs_syslog(LOG_ERR, PAMAFS_GETPASS_FAILED);
	    RET(PAM_AUTH_ERR);
	}
	if (password[0] == '\0') {
	    pam_afs_syslog(LOG_INFO, PAMAFS_NILPASSWORD, user);
	    RET(PAM_NEW_AUTHTOK_REQD);
	}

	/*
	 * We aren't going to free the password later (we will wipe it,
	 * though), because the storage for it if we get it from other
	 * paths may belong to someone else.  Since we do need to free
	 * this storage, copy it to a buffer that won't need to be freed
	 * later, and free this storage now.
	 */
	strncpy(my_password_buf, password, sizeof(my_password_buf));
	my_password_buf[sizeof(my_password_buf) - 1] = '\0';
	memset(password, 0, strlen(password));
	free(password);
	password = my_password_buf;
    }

    if ((code = ka_VerifyUserPassword(KA_USERAUTH_VERSION + KA_USERAUTH_DOSETPAG, user,	/* kerberos name */
				      NULL,	/* instance */
				      NULL,	/* realm */
				      password,	/* password */
				      0,	/* spare 2 */
				      &reason /* error string */ )) != 0) {
	pam_afs_syslog(LOG_ERR, PAMAFS_LOGIN_FAILED, user, reason);
	RET(PAM_AUTH_ERR);
    }
    torch_password = 0;
    pam_set_item(pamh, PAM_AUTHTOK, password);
    pam_set_item(pamh, PAM_OLDAUTHTOK, password);
    if (flags & PAM_PRELIM_CHECK) {
	/* only auth check was requested, so return success here */
	return (PAM_SUCCESS);
    }
    if (!(flags & PAM_UPDATE_AUTHTOK)) {
	/* these lines are never executed ... */
	/* UPDATE_AUTHTOK flag is required, return with error */
	pam_afs_syslog(LOG_ERR, PAMAFS_FLAGS, "PAM_UPDATE_AUTHTOK");
	RET(PAM_AUTH_ERR);
    }

    /* get the new passwd and verify it */
    errcode =
	pam_afs_prompt(pam_convp, &new_password, 0, PAMAFS_NEW_PWD_PROMPT);
    if (errcode != PAM_SUCCESS || new_password == NULL) {
	pam_afs_syslog(LOG_ERR, PAMAFS_GETPASS_FAILED);
	RET(PAM_AUTH_ERR);
    }
    if (new_password[0] == '\0') {
	pam_afs_syslog(LOG_INFO, PAMAFS_NILPASSWORD, user);
	RET(PAM_AUTH_ERR);
    }
    errcode =
	pam_afs_prompt(pam_convp, &verify_password, 0,
		       PAMAFS_VERIFY_PWD_PROMPT);
    if (errcode != PAM_SUCCESS || verify_password == NULL) {
	pam_afs_syslog(LOG_ERR, PAMAFS_GETPASS_FAILED);
	memset(new_password, 0, strlen(new_password));
	RET(PAM_AUTH_ERR);
    }
    if (verify_password[0] == '\0') {
	pam_afs_syslog(LOG_INFO, PAMAFS_NILPASSWORD, user);
	memset(new_password, 0, strlen(new_password));
	RET(PAM_AUTH_ERR);
    }
    if (strcmp(new_password, verify_password) != 0) {
	pam_afs_syslog(LOG_INFO, PAMAFS_NE_PASSWORD);
	memset(new_password, 0, strlen(new_password));
	memset(verify_password, 0, strlen(verify_password));
	RET(PAM_AUTH_ERR);
    }
    memset(verify_password, 0, strlen(verify_password));
    /* checking password length and quality is up to other PAM modules */

    /* set the new password */
    if ((code = ka_Init(0)) != 0) {
	pam_afs_syslog(LOG_ERR, PAMAFS_KAERROR, code);
	RET(PAM_AUTH_ERR);
    }
    if ((code = rx_Init(0)) != 0) {
	pam_afs_syslog(LOG_ERR, PAMAFS_KAERROR, code);
	RET(PAM_AUTH_ERR);
    }
    strcpy(instance, "");
    if ((localcell = ka_LocalCell()) == NULL) {
	pam_afs_syslog(LOG_ERR, PAMAFS_NOCELLNAME);
	RET(PAM_AUTH_ERR);
    }
    strcpy(realm, localcell);
    strcpy(cell, realm);
    /* oldkey is not used in ka_ChangePassword (only for ka_auth) */
    ka_StringToKey(password, realm, &oldkey);
    ka_StringToKey(new_password, realm, &newkey);
    if ((code =
	 ka_GetAdminToken(user, instance, realm, &oldkey, 20, &token,
			  0)) != 0) {
	pam_afs_syslog(LOG_ERR, PAMAFS_KAERROR, code);
	RET(PAM_AUTH_ERR);
    }
    if ((code =
	 ka_AuthServerConn(realm, KA_MAINTENANCE_SERVICE, &token,
			   &conn)) != 0) {
	pam_afs_syslog(LOG_ERR, PAMAFS_KAERROR, code);
	RET(PAM_AUTH_ERR);
    }
    if ((code = ka_ChangePassword(user,	/* kerberos name */
				  instance,	/* instance */
				  conn,	/* conn */
				  0,	/* old password unused */
				  &newkey /* new password */ )) != 0) {
	pam_afs_syslog(LOG_ERR, PAMAFS_KAPASS_FAIL);
	memset(new_password, 0, strlen(new_password));
	RET(PAM_AUTH_ERR);
    } else {
	pam_set_item(pamh, PAM_AUTHTOK, new_password);
	RET(PAM_SUCCESS);
    }

  out:
    if (password && torch_password) {
	memset(password, 0, strlen(password));
    }
    (void)setlogmask(origmask);
#ifndef AFS_SUN56_ENV
    closelog();
#endif
    return retcode;
}
