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

RCSID
    ("$Header: /cvs/openafs/src/pam/afs_auth.c,v 1.12.2.1 2005/05/30 03:37:48 shadow Exp $");

#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <afs/kautils.h>
#include <signal.h>
#include <errno.h>

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include "afs_message.h"
#include "afs_util.h"


#define RET(x) { retcode = (x); goto out; }

extern int
pam_sm_authenticate(pam_handle_t * pamh, int flags, int argc,
		    const char **argv)
{
    int retcode = PAM_SUCCESS;
    int errcode = PAM_SUCCESS;
    int code;
    int origmask;
    int logmask = LOG_UPTO(LOG_INFO);
    int nowarn = 0;
    int use_first_pass = 0;
    int try_first_pass = 0;
    int ignore_uid = 0;
    uid_t ignore_uid_id = 0;
    char my_password_buf[256];
    char *cell_ptr = NULL;
    /*
     * these options are added to handle stupid apps, which won't call
     * pam_set_cred()
     */
    int refresh_token = 0;
    int set_token = 0;
    int dont_fork = 0;
    /* satisfy kdm 2.x
     */
    int use_klog = 0;
    int set_expires = 0;	/* This option is only used in pam_set_cred() */
    int got_authtok = 0;	/* got PAM_AUTHTOK upon entry */
    char *user = NULL, *password = NULL;
    long password_expires = -1;
    int torch_password = 1;
    int i;
    struct pam_conv *pam_convp = NULL;
    int auth_ok;
    struct passwd unix_pwd, *upwd = NULL;
    char upwd_buf[2048];	/* size is a guess. */
    char *reason = NULL;
    pid_t cpid, rcpid;
    int status;
    struct sigaction newAction, origAction;


#ifndef AFS_SUN56_ENV
    openlog(pam_afs_ident, LOG_CONS | LOG_PID, LOG_AUTH);
#endif
    origmask = setlogmask(logmask);

    /*
     * Parse the user options.  Log an error for any unknown options.
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
	    ignore_uid = 1;
	    ignore_uid_id = 0;
	} else if (strcasecmp(argv[i], "ignore_uid") == 0) {
	    i++;
	    if (i == argc) {
		pam_afs_syslog(LOG_ERR, PAMAFS_IGNOREUID,
			       "ignore_uid missing argument");
		ignore_uid = 0;
	    } else {
		ignore_uid = 1;
		ignore_uid_id = (uid_t) strtol(argv[i], (char **)NULL, 10);
		if ((ignore_uid_id < 0) || (ignore_uid_id > IGNORE_MAX)) {
		    ignore_uid = 0;
		    pam_afs_syslog(LOG_ERR, PAMAFS_IGNOREUID, argv[i]);
		}
	    }
	} else if (strcasecmp(argv[i], "cell") == 0) {
	    i++;
	    if (i == argc) {
		pam_afs_syslog(LOG_ERR, PAMAFS_OTHERCELL,
			       "cell missing argument");
	    } else {
		cell_ptr = argv[i];
		pam_afs_syslog(LOG_INFO, PAMAFS_OTHERCELL, cell_ptr);
	    }
	} else if (strcasecmp(argv[i], "refresh_token") == 0) {
	    refresh_token = 1;
	} else if (strcasecmp(argv[i], "set_token") == 0) {
	    set_token = 1;
	} else if (strcasecmp(argv[i], "dont_fork") == 0) {
	    if (!use_klog)
		dont_fork = 1;
	    else
		pam_afs_syslog(LOG_ERR, PAMAFS_CONFLICTOPT, "dont_fork");
	} else if (strcasecmp(argv[i], "use_klog") == 0) {
	    if (!dont_fork)
		use_klog = 1;
	    else
		pam_afs_syslog(LOG_ERR, PAMAFS_CONFLICTOPT, "use_klog");
	} else if (strcasecmp(argv[i], "setenv_password_expires") == 0) {
	    set_expires = 1;
	} else {
	    pam_afs_syslog(LOG_ERR, PAMAFS_UNKNOWNOPT, argv[i]);
	}
    }

    /* Later we use try_first_pass to see if we can try again.    */
    /* If use_first_pass is true we don't want to ever try again, */
    /* so turn that flag off right now.                           */
    if (use_first_pass)
	try_first_pass = 0;

    if (logmask && LOG_MASK(LOG_DEBUG))
	pam_afs_syslog(LOG_DEBUG, PAMAFS_OPTIONS, nowarn, use_first_pass,
		       try_first_pass, ignore_uid, ignore_uid_id,
		       refresh_token, set_token, dont_fork, use_klog);

    /* Try to get the user-interaction info, if available. */
    errcode = pam_get_item(pamh, PAM_CONV, (const void **)&pam_convp);
    if (errcode != PAM_SUCCESS) {
	pam_afs_syslog(LOG_WARNING, PAMAFS_NO_USER_INT);
	pam_convp = NULL;
    }

    /* Who are we trying to authenticate here? */
    if ((errcode =
	 pam_get_user(pamh, (const char **)&user,
		      "login: ")) != PAM_SUCCESS) {
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
    /* enhanced: use "ignore_uid <number>" to specify the largest uid
     * which should be ignored by this module
     */
#if	defined(AFS_HPUX_ENV)
#if     defined(AFS_HPUX110_ENV)
    i = getpwnam_r(user, &unix_pwd, upwd_buf, sizeof(upwd_buf), &upwd);
#else /* AFS_HPUX110_ENV */
    i = getpwnam_r(user, &unix_pwd, upwd_buf, sizeof(upwd_buf));
    if (i == 0)			/* getpwnam_r success */
	upwd = &unix_pwd;
#endif /* else AFS_HPUX110_ENV */
    if (ignore_uid && i == 0 && upwd->pw_uid <= ignore_uid_id) {
	pam_afs_syslog(LOG_INFO, PAMAFS_IGNORINGROOT, user);
	RET(PAM_AUTH_ERR);
    }
#else
#if     defined(AFS_LINUX20_ENV) || defined(AFS_FBSD_ENV) || defined(AFS_NBSD_ENV)
    upwd = getpwnam(user);
#else
    upwd = getpwnam_r(user, &unix_pwd, upwd_buf, sizeof(upwd_buf));
#endif
    if (ignore_uid && upwd != NULL && upwd->pw_uid <= ignore_uid_id) {
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

  try_auth:
    if (password == NULL) {

	torch_password = 1;

	if (use_first_pass)
	    RET(PAM_AUTH_ERR);	/* shouldn't happen */
	if (try_first_pass)
	    try_first_pass = 0;	/* we come back if try_first_pass==1 below */

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

    /* Be sure to allocate a PAG here if we should set a token,
     * All of the remaining stuff to authenticate the user and to
     * get a token is done in a child process - if not suppressed by the config,
     * see below
     * But dont get a PAG if the refresh_token option was set
     * We have to do this in such a way because some
     * apps (such as screensavers) wont call setcred but authenticate :-(
     */
    if (!refresh_token) {
	setpag();
#ifdef AFS_KERBEROS_ENV
	ktc_newpag();
#endif
	if (logmask && LOG_MASK(LOG_DEBUG))
	    syslog(LOG_DEBUG, "New PAG created in pam_authenticate()");
    }

    if (!dont_fork) {
	/* Prepare for fork(): set SIGCHLD signal handler to default */
	sigemptyset(&newAction.sa_mask);
	newAction.sa_handler = SIG_DFL;
	newAction.sa_flags = 0;
	code = sigaction(SIGCHLD, &newAction, &origAction);
	if (code) {
	    pam_afs_syslog(LOG_ERR, PAMAFS_PAMERROR, errno);
	    RET(PAM_AUTH_ERR);
	}

	/* Fork a process and let it verify authentication. So that any
	 * memory/sockets allocated will get cleaned up when the child
	 * exits: defect 11686.
	 */
	if (use_klog) {		/* used by kdm 2.x */
	    if (refresh_token || set_token) {
		i = do_klog(user, password, NULL, cell_ptr);
	    } else {
		i = do_klog(user, password, "00:00:01", cell_ptr);
		ktc_ForgetAllTokens();
	    }
	    if (logmask && LOG_MASK(LOG_DEBUG))
		syslog(LOG_DEBUG, "do_klog returned %d", i);
	    auth_ok = i ? 0 : 1;
	} else {
	    if (logmask && LOG_MASK(LOG_DEBUG))
		syslog(LOG_DEBUG, "forking ...");
	    cpid = fork();
	    if (cpid <= 0) {	/* The child process */
		if (logmask && LOG_MASK(LOG_DEBUG))
		    syslog(LOG_DEBUG, "in child");
		if ((code = rx_Init(0)) != 0) {
		    pam_afs_syslog(LOG_ERR, PAMAFS_KAERROR, code);
		    exit(0);
		}
		if (refresh_token || set_token)
		    code = ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION, user,	/* kerberos name */
						      NULL,	/* instance */
						      cell_ptr,	/* realm */
						      password,	/* password */
						      0,	/* default lifetime */
						      &password_expires, 0,	/* spare 2 */
						      &reason
						      /* error string */ );
		else
		    code = ka_VerifyUserPassword(KA_USERAUTH_VERSION, user,	/* kerberos name */
						 NULL,	/* instance */
						 cell_ptr,	/* realm */
						 password,	/* password */
						 0,	/* spare 2 */
						 &reason /* error string */ );
		if (code) {
		    pam_afs_syslog(LOG_ERR, PAMAFS_LOGIN_FAILED, user,
				   reason);
		    auth_ok = 0;
		} else {
		    auth_ok = 1;
		}
		if (logmask && LOG_MASK(LOG_DEBUG))
		    syslog(LOG_DEBUG, "child: auth_ok=%d", auth_ok);
		if (cpid == 0)
		    exit(auth_ok);
	    } else {
		do {
		    if (logmask && LOG_MASK(LOG_DEBUG))
			syslog(LOG_DEBUG, "in parent, waiting ...");
		    rcpid = waitpid(cpid, &status, 0);
		} while ((rcpid == -1) && (errno == EINTR));

		if ((rcpid == cpid) && WIFEXITED(status)) {
		    auth_ok = WEXITSTATUS(status);
		} else {
		    auth_ok = 0;
		}
		if (logmask && LOG_MASK(LOG_DEBUG))
		    syslog(LOG_DEBUG, "parent: auth_ok=%d", auth_ok);
	    }
	}
	/* Restore old signal handler */
	code = sigaction(SIGCHLD, &origAction, NULL);
	if (code) {
	    pam_afs_syslog(LOG_ERR, PAMAFS_PAMERROR, errno);
	}
    } else {			/* dont_fork, used by httpd */
	if ((code = rx_Init(0)) != 0) {
	    pam_afs_syslog(LOG_ERR, PAMAFS_KAERROR, code);
	    RET(PAM_AUTH_ERR);
	}
	if (logmask && LOG_MASK(LOG_DEBUG))
	    syslog(LOG_DEBUG, "dont_fork");
	if (refresh_token || set_token)
	    code = ka_UserAuthenticateGeneral(KA_USERAUTH_VERSION, user,	/* kerberos name */
					      NULL,	/* instance */
					      cell_ptr,	/* realm */
					      password,	/* password */
					      0,	/* default lifetime */
					      &password_expires, 0,	/* spare 2 */
					      &reason /* error string */ );
	else
	    code = ka_VerifyUserPassword(KA_USERAUTH_VERSION, user,	/* kerberos name */
					 NULL,	/* instance */
					 cell_ptr,	/* realm */
					 password,	/* password */
					 0,	/* spare 2 */
					 &reason /* error string */ );
	if (logmask && LOG_MASK(LOG_DEBUG))
	    syslog(LOG_DEBUG, "dont_fork, code = %d", code);
	if (code) {
	    pam_afs_syslog(LOG_ERR, PAMAFS_LOGIN_FAILED, user, reason);
	    auth_ok = 0;
	} else {
	    auth_ok = 1;
	}
	if (logmask && LOG_MASK(LOG_DEBUG))
	    syslog(LOG_DEBUG, "dont_fork: auth_ok=%d", auth_ok);
    }

    if (!auth_ok && try_first_pass) {
	password = NULL;
	goto try_auth;
    }

    /* We don't care if this fails; all we can do is try. */
    /* It is not reasonable to store the password only if it was correct
     * because it could satisfy another module that is called in the chain
     * after pam_afs
     */
    if (!got_authtok) {
	torch_password = 0;
	(void)pam_set_item(pamh, PAM_AUTHTOK, password);
    }

    if (logmask && LOG_MASK(LOG_DEBUG))
	syslog(LOG_DEBUG, "leaving auth: auth_ok=%d", auth_ok);
    if (code == KANOENT)
	RET(PAM_USER_UNKNOWN);
    RET(auth_ok ? PAM_SUCCESS : PAM_AUTH_ERR);

  out:
    if (password) {
	/* we store the password in the data portion */
	char *tmp = strdup(password);
	(void)pam_set_data(pamh, pam_afs_lh, tmp, lc_cleanup);
	if (torch_password)
	    memset(password, 0, strlen(password));
    }
    (void)setlogmask(origmask);
#ifndef AFS_SUN56_ENV
    closelog();
#endif
    return retcode;
}
